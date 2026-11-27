"""Deterministic validator and SRT/cue manifest exporter for STEM storyboards."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys
from dataclasses import asdict, dataclass

TABLE_ROW_RE = re.compile(r"^\|(.+)\|\s*$")
TIME_SPAN_RE = re.compile(
    r"^(?P<t0>(?:\d{1,2}:)?\d+(?:\.\d+)?)\s*(?:-|to|\u2013|\u2014)\s*(?P<t1>(?:\d{1,2}:)?\d+(?:\.\d+)?)\s*s?$"
)
WORD_RE = re.compile(r"[^\W_]+(?:['\u2019-][^\W_]+)*", re.UNICODE)
HEX_COLOR_RE = re.compile(r"#[0-9A-Fa-f]{6}\b")
SFX_SECTION_RE = re.compile(r"SFX:\s*(.+)$", re.IGNORECASE)
SFX_ITEM_RE = re.compile(r"`?([A-Za-z0-9_]+)`?\s*@\s*`?(\d+(?:\.\d+)?)s?`?", re.IGNORECASE)


@dataclass(frozen=True)
class StoryboardShot:
    shot_id: str
    t0: float
    t1: float
    duration_sec: float
    visual: str
    motion: str
    vo_text: str
    vo_words: int
    words_per_sec: float
    sync_sfx: str


@dataclass(frozen=True)
class StoryboardIssue:
    severity: str  # "ERROR" or "WARNING"
    code: str
    shot_id: str
    line_num: int
    message: str


@dataclass(frozen=True)
class StoryboardReport:
    path: str
    mode: str
    shot_count: int
    total_duration_sec: float
    total_vo_words: int
    shots: list[StoryboardShot]
    issues: list[StoryboardIssue]

    @property
    def error_count(self) -> int:
        return sum(1 for item in self.issues if item.severity == "ERROR")

    @property
    def warning_count(self) -> int:
        return sum(1 for item in self.issues if item.severity == "WARNING")


def _parse_timestamp_sec(raw: str) -> float:
    token = raw.strip().rstrip("s")
    if ":" in token:
        parts = token.split(":")
        if len(parts) == 2:
            return int(parts[0]) * 60.0 + float(parts[1])
        if len(parts) == 3:
            return int(parts[0]) * 3600.0 + int(parts[1]) * 60.0 + float(parts[2])
    return float(token)


def _format_srt_time(seconds: float) -> str:
    ms_total = max(0, round(seconds * 1000.0))
    hours, rem = divmod(ms_total, 3_600_000)
    minutes, rem = divmod(rem, 60_000)
    secs, millis = divmod(rem, 1000)
    return f"{hours:02d}:{minutes:02d}:{secs:02d},{millis:03d}"


def _wrap_subtitle_lines(text: str, max_chars: int = 42) -> str:
    clean = text.strip().strip('"').strip("'")
    words = clean.split()
    if not words:
        return ""
    lines: list[str] = []
    current: list[str] = []
    for w in words:
        candidate = " ".join([*current, w])
        if len(candidate) <= max_chars or not current:
            current.append(w)
        else:
            lines.append(" ".join(current))
            current = [w]
            if len(lines) == 2:
                break
    if current and len(lines) < 2:
        lines.append(" ".join(current))
    return "\n".join(lines)


def validate_storyboard_text(
    text: str,
    *,
    path_label: str = "<memory>",
    mode: str = "shot",
    max_wps: float = 3.2,
    gap_tolerance_sec: float = 0.15,
) -> StoryboardReport:
    """Validates a Markdown storyboard table for timeline continuity, VO pacing, and visual specs."""
    lines = text.splitlines()
    issues: list[StoryboardIssue] = []
    shots: list[StoryboardShot] = []

    max_shot_sec = 20.0 if mode == "shot" else 45.0
    min_shot_sec = 1.0 if mode == "shot" else 4.0

    # Collect locked palette colors if declared under 'Semantic Color Lock'
    declared_colors: set[str] = set()
    in_palette = False
    for line in lines:
        if line.strip().lower().startswith("## semantic color lock"):
            in_palette = True
            continue
        if in_palette and line.strip().startswith("## "):
            in_palette = False
        if in_palette:
            for hx in HEX_COLOR_RE.findall(line):
                declared_colors.add(hx.upper())

    in_shot_table = False
    for idx, line in enumerate(lines, start=1):
        match = TABLE_ROW_RE.match(line.strip())
        if not match:
            if in_shot_table and line.strip() == "":
                in_shot_table = False
            continue

        cells = [c.strip() for c in match.group(1).split("|")]
        if len(cells) < 5:
            continue

        if cells[0].lower() in {"shot", "beat", "#"}:
            in_shot_table = True
            continue
        if all(set(c) <= {"-", ":", " "} for c in cells):
            continue
        if not in_shot_table:
            continue

        shot_id = cells[0]
        time_cell = cells[1]
        visual = cells[2]
        motion = cells[3]
        vo_raw = cells[4].strip().strip('"').strip("'")
        sync_sfx = cells[5] if len(cells) >= 6 else ""

        ts_match = TIME_SPAN_RE.match(time_cell)
        if not ts_match:
            issues.append(
                StoryboardIssue(
                    severity="ERROR",
                    code="BAD_TIME_RANGE",
                    shot_id=shot_id,
                    line_num=idx,
                    message=f"Invalid time span '{time_cell}'; expected e.g. '0.0-5.2' or '00:00-00:12'.",
                )
            )
            continue

        t0 = _parse_timestamp_sec(ts_match.group("t0"))
        t1 = _parse_timestamp_sec(ts_match.group("t1"))
        dur = t1 - t0
        if dur <= 0.0:
            issues.append(
                StoryboardIssue(
                    severity="ERROR",
                    code="NON_POSITIVE_DURATION",
                    shot_id=shot_id,
                    line_num=idx,
                    message=f"Shot '{shot_id}' has non-positive duration ({t0:.2f}s -> {t1:.2f}s).",
                )
            )
            continue

        if not visual or visual == "-":
            issues.append(
                StoryboardIssue(
                    severity="WARNING",
                    code="MISSING_VISUAL",
                    shot_id=shot_id,
                    line_num=idx,
                    message=f"Shot '{shot_id}' has an empty Visual & Layout cell.",
                )
            )
        if not motion or motion == "-":
            issues.append(
                StoryboardIssue(
                    severity="WARNING",
                    code="MISSING_MOTION",
                    shot_id=shot_id,
                    line_num=idx,
                    message=f"Shot '{shot_id}' has an empty Motion & Camera cell.",
                )
            )

        if dur < min_shot_sec:
            issues.append(
                StoryboardIssue(
                    severity="WARNING",
                    code="SHOT_TOO_SHORT",
                    shot_id=shot_id,
                    line_num=idx,
                    message=f"Shot '{shot_id}' duration {dur:.2f}s is below {min_shot_sec:.1f}s ({mode} mode).",
                )
            )
        elif dur > max_shot_sec:
            issues.append(
                StoryboardIssue(
                    severity="WARNING",
                    code="SHOT_TOO_LONG",
                    shot_id=shot_id,
                    line_num=idx,
                    message=f"Shot '{shot_id}' duration {dur:.2f}s exceeds {max_shot_sec:.1f}s ({mode} mode).",
                )
            )

        if shots:
            prev = shots[-1]
            delta = t0 - prev.t1
            if delta < -gap_tolerance_sec:
                issues.append(
                    StoryboardIssue(
                        severity="ERROR",
                        code="TIMELINE_OVERLAP",
                        shot_id=shot_id,
                        line_num=idx,
                        message=f"Shot '{shot_id}' starts at {t0:.2f}s before '{prev.shot_id}' ends ({prev.t1:.2f}s).",
                    )
                )
            elif delta > gap_tolerance_sec:
                issues.append(
                    StoryboardIssue(
                        severity="WARNING",
                        code="TIMELINE_GAP",
                        shot_id=shot_id,
                        line_num=idx,
                        message=f"Gap of {delta:.2f}s between '{prev.shot_id}' ({prev.t1:.2f}s) and '{shot_id}' ({t0:.2f}s).",
                    )
                )

        if declared_colors:
            for hx in HEX_COLOR_RE.findall(visual):
                if hx.upper() not in declared_colors:
                    issues.append(
                        StoryboardIssue(
                            severity="WARNING",
                            code="UNDECLARED_COLOR",
                            shot_id=shot_id,
                            line_num=idx,
                            message=f"Color {hx.upper()} in '{shot_id}' is not in the Semantic Color Lock.",
                        )
                    )

        vo_words = len(WORD_RE.findall(vo_raw))
        wps = vo_words / dur
        if wps > max_wps:
            issues.append(
                StoryboardIssue(
                    severity="WARNING",
                    code="VO_TOO_DENSE",
                    shot_id=shot_id,
                    line_num=idx,
                    message=f"Shot '{shot_id}' VO rate is {wps:.2f} w/s (> {max_wps:.1f} w/s); extend shot or trim words.",
                )
            )

        shots.append(
            StoryboardShot(
                shot_id=shot_id,
                t0=round(t0, 3),
                t1=round(t1, 3),
                duration_sec=round(dur, 3),
                visual=visual,
                motion=motion,
                vo_text=vo_raw,
                vo_words=vo_words,
                words_per_sec=round(wps, 2),
                sync_sfx=sync_sfx,
            )
        )

    if not shots:
        issues.append(
            StoryboardIssue(
                severity="ERROR",
                code="NO_SHOTS_FOUND",
                shot_id="<document>",
                line_num=1,
                message="No valid shot/beat table rows found.",
            )
        )

    total_dur = shots[-1].t1 if shots else 0.0
    total_words = sum(s.vo_words for s in shots)
    return StoryboardReport(
        path=path_label,
        mode=mode,
        shot_count=len(shots),
        total_duration_sec=round(total_dur, 3),
        total_vo_words=total_words,
        shots=shots,
        issues=issues,
    )


def export_srt(shots: list[StoryboardShot], out_path: pathlib.Path) -> None:
    blocks: list[str] = []
    sub_idx = 1
    for s in shots:
        if not s.vo_text:
            continue
        start_str = _format_srt_time(s.t0)
        end_str = _format_srt_time(max(s.t0 + 0.5, s.t1 - 0.1))
        wrapped = _wrap_subtitle_lines(s.vo_text)
        blocks.append(f"{sub_idx}\n{start_str} --> {end_str}\n{wrapped}\n")
        sub_idx += 1
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(blocks), encoding="utf-8")


def export_cues_json(report: StoryboardReport, out_path: pathlib.Path) -> None:
    cues: list[dict[str, object]] = []
    sfx_events: list[dict[str, object]] = []
    for s in report.shots:
        cues.append(
            {
                "cue_id": s.shot_id.lower().replace(" ", "_"),
                "time_sec": s.t0,
                "end_sec": s.t1,
                "description": s.visual[:100],
            }
        )
        sfx_sec_match = SFX_SECTION_RE.search(s.sync_sfx)
        if sfx_sec_match:
            for m in SFX_ITEM_RE.finditer(sfx_sec_match.group(1)):
                sfx_events.append(
                    {
                        "shot_id": s.shot_id,
                        "sfx_kind": m.group(1),
                        "time_sec": float(m.group(2)),
                    }
                )
    payload = {
        "source": report.path,
        "mode": report.mode,
        "duration_sec": report.total_duration_sec,
        "cues": cues,
        "sfx_events": sfx_events,
    }
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")

    parser = argparse.ArgumentParser(description="Validate STEM storyboards and export SRT/cues manifests.")
    parser.add_argument("storyboard", type=pathlib.Path, help="Path to Markdown storyboard file")
    parser.add_argument("--mode", choices=("shot", "beat"), default="shot", help="Validation mode (default: shot)")
    parser.add_argument("--max-wps", type=float, default=3.2, help="Max VO words/sec per shot (default: 3.2)")
    parser.add_argument("--export-srt", type=pathlib.Path, default=None, help="Optional output .srt path")
    parser.add_argument("--export-cues", type=pathlib.Path, default=None, help="Optional output cues.json path")
    parser.add_argument("--strict", action="store_true", help="Exit non-zero on warnings as well as errors")
    parser.add_argument("--json", action="store_true", help="Print JSON report")
    args = parser.parse_args(argv)

    text = args.storyboard.read_text(encoding="utf-8")
    report = validate_storyboard_text(
        text,
        path_label=str(args.storyboard),
        mode=args.mode,
        max_wps=args.max_wps,
    )

    if args.export_srt is not None and report.shots:
        export_srt(report.shots, args.export_srt)
    if args.export_cues is not None and report.shots:
        export_cues_json(report, args.export_cues)

    if args.json:
        payload = asdict(report)
        payload["error_count"] = report.error_count
        payload["warning_count"] = report.warning_count
        print(json.dumps(payload, ensure_ascii=False, indent=2))
    else:
        print(
            f"Storyboard: {report.path} | Mode: {report.mode} | "
            f"Shots: {report.shot_count} | Duration: {report.total_duration_sec:.2f}s | "
            f"VO Words: {report.total_vo_words}"
        )
        for s in report.shots:
            print(
                f"  {s.shot_id:8s} [{s.t0:6.2f}s -> {s.t1:6.2f}s] "
                f"({s.duration_sec:5.2f}s, {s.vo_words:2d}w @ {s.words_per_sec:.2f} w/s)"
            )
        if report.issues:
            print(f"Issues ({report.error_count} errors, {report.warning_count} warnings):")
            for iss in report.issues:
                print(f"  [{iss.severity}] {iss.code} ({iss.shot_id}:{iss.line_num}): {iss.message}")
        else:
            print("Issues: 0 errors, 0 warnings (PASS)")

    if report.error_count > 0:
        return 1
    if args.strict and report.warning_count > 0:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
