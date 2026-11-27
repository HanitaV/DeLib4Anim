"""Deterministic linter and runtime estimator for Median-style STEM Markdown scripts."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys
from dataclasses import asdict, dataclass

PAUSE_WEIGHTS_SEC: dict[str, float] = {
    "[.]": 0.4,
    "[..]": 0.8,
    "[...]": 1.5,
    "[BEAT]": 2.5,
    "[PAUSE]": 2.5,
}

PAUSE_TAG_RE = re.compile(r"\[(?:\.{1,3}|BEAT|PAUSE)\]")
HEADING_RE = re.compile(r"^###\s+(.+?)\s*$")
TAG_SUFFIX_RE = re.compile(r"\s+\[(CORE|CUTTABLE)\]\s*$")
WORD_RE = re.compile(r"[^\W_]+(?:['\u2019-][^\W_]+)*", re.UNICODE)

RAW_LATEX_PATTERNS: tuple[re.Pattern[str], ...] = (
    re.compile(r"\$[^$\n]+\$"),
    re.compile(r"\\(?:frac|vec|sqrt|partial|Delta|hbar|textcolor|hat|sin|cos|alpha|beta|theta|psi)\b"),
    re.compile(r"\b[A-Za-z]\^[0-9{(]"),
)

EM_DASH_RE = re.compile(r"\u2014|\s--\s")

AI_SLOP_PATTERNS: tuple[tuple[str, re.Pattern[str]], ...] = (
    ("khong chi ... ma con", re.compile(r"\bkh\u00f4ng\s+ch\u1ec9\b.{1,80}\bm\u00e0\s+c\u00f2n\b", re.IGNORECASE)),
    ("hay cung kham pha/tim hieu", re.compile(r"\bh\u00e3y\s+c\u00f9ng\s+(?:nhau\s+)?(?:kh\u00e1m\s+ph\u00e1|t\u00ecm\s+hi\u1ec3u|\u0111i\s+s\u00e2u)\b", re.IGNORECASE)),
    ("buc tranh toan canh", re.compile(r"\bb\u1ee9c\s+tranh\s+to\u00e0n\s+c\u1ea3nh\b", re.IGNORECASE)),
    ("dong vai tro then chot", re.compile(r"\b\u0111\u00f3ng\s+vai\s+tr\u00f2\s+(?:then\s+ch\u1ed1t|c\u1ef1c\s+k\u1ef3\s+quan\s+tr\u1ecdng)\b", re.IGNORECASE)),
    ("minh chung song dong", re.compile(r"\bminh\s+ch\u1ee9ng\s+(?:s\u1ed1ng\s+\u0111\u1ed9ng|r\u00f5\s+n\u00e9t)\b", re.IGNORECASE)),
    ("let's dive in", re.compile(r"\blet['\u2019]?s\s+dive\s+in(?:to)?\b", re.IGNORECASE)),
    ("it's not just ... it's", re.compile(r"\bit['\u2019]?s\s+not\s+(?:just|merely|only)\b.{1,80}\bit['\u2019]?s\b", re.IGNORECASE)),
    ("at its core", re.compile(r"\bat\s+its\s+core\b", re.IGNORECASE)),
)

EMPTY_DEICTIC_PATTERNS: tuple[re.Pattern[str], ...] = (
    re.compile(r"\bnh\u01b0\s+(?:c\u00e1c\s+)?b\u1ea1n\s+(?:c\u00f3\s+th\u1ec3\s+)?th\u1ea5y\s+(?:\u1edf\s+\u0111\u00e2y|tr\u00ean\s+m\u00e0n\s+h\u00ecnh)\b", re.IGNORECASE),
    re.compile(r"\bnh\u00ecn\s+v\u00e0o\s+\u0111\u00e2y\b", re.IGNORECASE),
    re.compile(r"\bas\s+you\s+can\s+see\s+here\b", re.IGNORECASE),
    re.compile(r"\bclick\s+here\b", re.IGNORECASE),
)

SENTENCE_SPLIT_RE = re.compile(r"[.!?]+(?:\s+|$)")


@dataclass(frozen=True)
class LintIssue:
    severity: str  # "ERROR" or "WARNING"
    code: str
    chapter: str
    line_num: int
    message: str


@dataclass(frozen=True)
class ChapterStats:
    title: str
    tag: str
    word_count: int
    pause_sec: float
    spoken_sec: float
    total_sec: float
    start_sec: float
    has_visual_intent: bool


@dataclass(frozen=True)
class LintReport:
    path: str
    wpm: float
    total_words: int
    total_pause_sec: float
    total_sec: float
    chapters: list[ChapterStats]
    youtube_chapters: list[str]
    issues: list[LintIssue]

    @property
    def error_count(self) -> int:
        return sum(1 for item in self.issues if item.severity == "ERROR")

    @property
    def warning_count(self) -> int:
        return sum(1 for item in self.issues if item.severity == "WARNING")


def _format_mmss(seconds: float) -> str:
    sec_int = max(0, round(seconds))
    mins, secs = divmod(sec_int, 60)
    return f"{mins:02d}:{secs:02d}"


def _clean_chapter_title(raw_title: str) -> tuple[str, str]:
    match = TAG_SUFFIX_RE.search(raw_title)
    if match:
        tag = match.group(1)
        clean = raw_title[: match.start()].strip()
        return clean, tag
    return raw_title.strip(), "CORE"


def lint_script_text(
    text: str,
    *,
    path_label: str = "<memory>",
    wpm: float = 140.0,
    target_sec: float | None = None,
    tolerance_pct: float = 15.0,
    max_sentence_words: int = 38,
) -> LintReport:
    """Parses a Markdown STEM script, computes runtime & chapters, and checks quality gates."""
    lines = text.splitlines()
    raw_sections: list[tuple[str, str, int, list[tuple[int, str]], bool]] = []

    current_title: str | None = None
    current_tag = "CORE"
    current_line = 0
    vo_lines: list[tuple[int, str]] = []
    has_visual_intent = False

    def flush_section() -> None:
        if current_title is not None and (vo_lines or has_visual_intent):
            raw_sections.append(
                (current_title, current_tag, current_line, list(vo_lines), has_visual_intent)
            )

    for idx, line in enumerate(lines, start=1):
        heading = HEADING_RE.match(line)
        if heading:
            flush_section()
            current_title, current_tag = _clean_chapter_title(heading.group(1))
            current_line = idx
            vo_lines = []
            has_visual_intent = False
            continue

        if current_title is None:
            continue

        stripped = line.lstrip()
        if stripped.startswith(">"):
            body = stripped[1:].strip()
            if not body:
                continue
            lower_body = body.lower()
            if lower_body.startswith(("visual intent:", "sync:", "sfx:")):
                has_visual_intent = True
                continue
            vo_lines.append((idx, body))

    flush_section()

    issues: list[LintIssue] = []
    if not raw_sections:
        issues.append(
            LintIssue(
                severity="ERROR",
                code="NO_VO_CHAPTERS",
                chapter="<document>",
                line_num=1,
                message="No '### Chapter' headings with '>' spoken narration lines were found.",
            )
        )

    chapters: list[ChapterStats] = []
    yt_chapters: list[str] = []
    cursor_sec = 0.0
    total_words = 0
    total_pause_sec = 0.0
    words_per_sec = max(wpm, 60.0) / 60.0

    for title, tag, ch_line, ch_vo_lines, ch_has_vi in raw_sections:
        ch_words = 0
        ch_pause = 0.0

        if ch_vo_lines and not ch_has_vi:
            issues.append(
                LintIssue(
                    severity="WARNING",
                    code="MISSING_VISUAL_INTENT",
                    chapter=title,
                    line_num=ch_line,
                    message=f"Chapter '{title}' has spoken VO but no 'Visual intent:' note.",
                )
            )

        for ln_num, line_text in ch_vo_lines:
            # 1. Check raw LaTeX in spoken VO
            for pat in RAW_LATEX_PATTERNS:
                if pat.search(line_text):
                    issues.append(
                        LintIssue(
                            severity="ERROR",
                            code="RAW_LATEX_IN_VO",
                            chapter=title,
                            line_num=ln_num,
                            message=f"Raw LaTeX or math syntax in spoken VO: '{line_text[:72]}'",
                        )
                    )
                    break

            # 2. Check em-dashes in spoken VO
            if EM_DASH_RE.search(line_text):
                issues.append(
                    LintIssue(
                        severity="WARNING",
                        code="EM_DASH_IN_VO",
                        chapter=title,
                        line_num=ln_num,
                        message="Em-dash or '--' found in spoken VO; replace with comma, period, or pause tag.",
                    )
                )

            # 3. Check AI-slop phrases
            for label, slop_re in AI_SLOP_PATTERNS:
                if slop_re.search(line_text):
                    issues.append(
                        LintIssue(
                            severity="WARNING",
                            code="AI_SLOP_PHRASE",
                            chapter=title,
                            line_num=ln_num,
                            message=f"AI cliché '{label}' detected in spoken VO.",
                        )
                    )

            # 4. Check empty deictics
            for d_re in EMPTY_DEICTIC_PATTERNS:
                if d_re.search(line_text):
                    issues.append(
                        LintIssue(
                            severity="WARNING",
                            code="EMPTY_DEICTIC",
                            chapter=title,
                            line_num=ln_num,
                            message="Unanchored deictic phrase detected; replace with integrated visual description.",
                        )
                    )
                    break

            # 5. Count pauses and words
            for tag_match in PAUSE_TAG_RE.finditer(line_text):
                ch_pause += PAUSE_WEIGHTS_SEC.get(tag_match.group(0), 0.5)

            plain = PAUSE_TAG_RE.sub(" ", line_text)
            plain_no_md = plain.replace("**", "").replace("*", "")
            line_words = WORD_RE.findall(plain_no_md)
            ch_words += len(line_words)

            # 6. Check per-sentence breath length
            for seg in SENTENCE_SPLIT_RE.split(plain_no_md):
                seg_words = WORD_RE.findall(seg)
                if len(seg_words) > max_sentence_words:
                    issues.append(
                        LintIssue(
                            severity="WARNING",
                            code="BREATH_LENGTH",
                            chapter=title,
                            line_num=ln_num,
                            message=f"Spoken sentence has {len(seg_words)} words (> {max_sentence_words} breath cap).",
                        )
                    )

        spoken_sec = ch_words / words_per_sec
        ch_total_sec = spoken_sec + ch_pause
        chapters.append(
            ChapterStats(
                title=title,
                tag=tag,
                word_count=ch_words,
                pause_sec=round(ch_pause, 2),
                spoken_sec=round(spoken_sec, 2),
                total_sec=round(ch_total_sec, 2),
                start_sec=round(cursor_sec, 2),
                has_visual_intent=ch_has_vi,
            )
        )
        yt_chapters.append(f"{_format_mmss(cursor_sec)} {title}")

        if ch_words > 0 and ch_total_sec < 10.0:
            issues.append(
                LintIssue(
                    severity="WARNING",
                    code="CHAPTER_TOO_SHORT",
                    chapter=title,
                    line_num=ch_line,
                    message=f"Chapter '{title}' is {ch_total_sec:.1f}s (< 10s YouTube chapter minimum).",
                )
            )

        cursor_sec += ch_total_sec
        total_words += ch_words
        total_pause_sec += ch_pause

    if cursor_sec >= 120.0 and len(chapters) < 3:
        issues.append(
            LintIssue(
                severity="WARNING",
                code="FEW_CHAPTERS",
                chapter="<document>",
                line_num=1,
                message=f"Video >= 120s ({cursor_sec:.1f}s) should have at least 3 chapters (found {len(chapters)}).",
            )
        )

    if target_sec is not None and target_sec > 0.0:
        diff_pct = abs(cursor_sec - target_sec) / target_sec * 100.0
        if diff_pct > tolerance_pct:
            issues.append(
                LintIssue(
                    severity="WARNING",
                    code="RUNTIME_BUDGET",
                    chapter="<document>",
                    line_num=1,
                    message=(
                        f"Estimated runtime {cursor_sec:.1f}s differs from target "
                        f"{target_sec:.1f}s by {diff_pct:.1f}% (> {tolerance_pct:.0f}%)."
                    ),
                )
            )

    return LintReport(
        path=path_label,
        wpm=wpm,
        total_words=total_words,
        total_pause_sec=round(total_pause_sec, 2),
        total_sec=round(cursor_sec, 2),
        chapters=chapters,
        youtube_chapters=yt_chapters,
        issues=issues,
    )


def main(argv: list[str] | None = None) -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")

    parser = argparse.ArgumentParser(description="Lint Median-style STEM Markdown scripts and budget runtime.")
    parser.add_argument("script", type=pathlib.Path, help="Path to Markdown script file")
    parser.add_argument("--wpm", type=float, default=140.0, help="Spoken cadence in words per minute (default: 140)")
    parser.add_argument("--target-sec", type=float, default=None, help="Optional target runtime in seconds")
    parser.add_argument("--tolerance-pct", type=float, default=15.0, help="Allowed runtime deviation %% (default: 15)")
    parser.add_argument("--max-sentence-words", type=int, default=38, help="Max words per sentence (default: 38)")
    parser.add_argument("--strict", action="store_true", help="Exit non-zero on warnings as well as errors")
    parser.add_argument("--json", action="store_true", help="Print JSON report")
    args = parser.parse_args(argv)

    text = args.script.read_text(encoding="utf-8")
    report = lint_script_text(
        text,
        path_label=str(args.script),
        wpm=args.wpm,
        target_sec=args.target_sec,
        tolerance_pct=args.tolerance_pct,
        max_sentence_words=args.max_sentence_words,
    )

    if args.json:
        payload = asdict(report)
        payload["error_count"] = report.error_count
        payload["warning_count"] = report.warning_count
        print(json.dumps(payload, ensure_ascii=False, indent=2))
    else:
        print(
            f"Script: {report.path} | Words: {report.total_words} | "
            f"Pauses: {report.total_pause_sec:.1f}s | "
            f"Est. Runtime: {report.total_sec:.1f}s ({_format_mmss(report.total_sec)}) @ {report.wpm:.0f} WPM"
        )
        print("Chapters:")
        for ch_line, ch in zip(report.youtube_chapters, report.chapters, strict=True):
            print(f"  {ch_line} ({ch.word_count}w, {ch.total_sec:.1f}s, [{ch.tag}])")
        if report.issues:
            print(f"Issues ({report.error_count} errors, {report.warning_count} warnings):")
            for iss in report.issues:
                print(f"  [{iss.severity}] {iss.code} ({iss.chapter}:{iss.line_num}): {iss.message}")
        else:
            print("Issues: 0 errors, 0 warnings (PASS)")

    if report.error_count > 0:
        return 1
    if args.strict and report.warning_count > 0:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
