"""Tests for stem-video-scriptwriter and stem-storyboard-director skills and CLI tools."""

from __future__ import annotations

import importlib.util
import json
import pathlib
import re
from types import ModuleType

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
SCRIPTWRITER_DIR = REPO_ROOT / ".agents" / "skills" / "stem-video-scriptwriter"
STORYBOARD_DIR = REPO_ROOT / ".agents" / "skills" / "stem-storyboard-director"


def _load_module(name: str, path: pathlib.Path) -> ModuleType:
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    import sys

    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


def test_skill_frontmatter_and_relative_links_resolve() -> None:
    link_re = re.compile(r"\[[^\]]+\]\((\./[^)#]+|../../../[^)#]+)\)")
    for skill_dir, expected_name in (
        (SCRIPTWRITER_DIR, "stem-video-scriptwriter"),
        (STORYBOARD_DIR, "stem-storyboard-director"),
    ):
        skill_md = skill_dir / "SKILL.md"
        assert skill_md.is_file(), f"Missing {skill_md}"
        content = skill_md.read_text(encoding="utf-8")
        assert content.startswith("---\n"), f"{skill_md} missing YAML frontmatter"
        fm_end = content.find("\n---", 4)
        assert fm_end > 0, f"{skill_md} frontmatter not closed"
        fm = content[4:fm_end]
        assert f"name: {expected_name}" in fm
        assert "description:" in fm

        links = link_re.findall(content)
        assert links, f"Expected relative reference links in {skill_md}"
        for rel in links:
            target = (skill_dir / rel).resolve()
            assert target.exists(), f"Broken relative link '{rel}' in {skill_md} -> {target}"


def test_sample_script_passes_linter_strict() -> None:
    linter = _load_module("lint_stem_script", SCRIPTWRITER_DIR / "scripts" / "lint_stem_script.py")
    sample_path = SCRIPTWRITER_DIR / "examples" / "sample-script-con-lac-don.md"
    report = linter.lint_script_text(
        sample_path.read_text(encoding="utf-8"),
        path_label=str(sample_path),
        wpm=140.0,
    )
    assert report.error_count == 0, report.issues
    assert report.warning_count == 0, report.issues
    assert len(report.chapters) == 5
    assert report.youtube_chapters[0].startswith("00:00 ")
    assert report.total_pause_sec > 5.0
    assert 300.0 <= report.total_sec <= 450.0

    rc = linter.main([str(sample_path), "--strict"])
    assert rc == 0


def test_linter_catches_latex_slop_deictic_emdash_and_breath() -> None:
    linter = _load_module("lint_stem_script", SCRIPTWRITER_DIR / "scripts" / "lint_stem_script.py")
    long_sentence = " ".join(["tu"] * 42) + "."
    bad_script = f"""# Bad Script

### Opening: Test
> H\u00e3y c\u00f9ng kh\u00e1m ph\u00e1 c\u00f4ng th\u1ee9c $F = ma$ ngay b\u00e2y gi\u1edd.
> Nh\u01b0 c\u00e1c b\u1ea1n c\u00f3 th\u1ec3 th\u1ea5y \u1edf \u0111\u00e2y \u2014 n\u00f3 kh\u00f4ng ch\u1ec9 \u0111\u01a1n gi\u1ea3n m\u00e0 c\u00f2n ph\u1ee9c t\u1ea1p.
> {long_sentence}
"""
    report = linter.lint_script_text(bad_script, target_sec=120.0)
    codes = {iss.code for iss in report.issues}
    assert "RAW_LATEX_IN_VO" in codes
    assert "EM_DASH_IN_VO" in codes
    assert "AI_SLOP_PHRASE" in codes
    assert "EMPTY_DEICTIC" in codes
    assert "BREATH_LENGTH" in codes
    assert "MISSING_VISUAL_INTENT" in codes
    assert "RUNTIME_BUDGET" in codes
    assert report.error_count >= 1


def test_sample_storyboard_passes_validator_and_exports(tmp_path: pathlib.Path) -> None:
    validator = _load_module("validate_storyboard", STORYBOARD_DIR / "scripts" / "validate_storyboard.py")
    sample_path = STORYBOARD_DIR / "examples" / "sample-storyboard-con-lac.md"
    srt_out = tmp_path / "con_lac.srt"
    cues_out = tmp_path / "cues.json"

    rc = validator.main(
        [
            str(sample_path),
            "--mode",
            "shot",
            "--strict",
            "--export-srt",
            str(srt_out),
            "--export-cues",
            str(cues_out),
        ]
    )
    assert rc == 0
    assert srt_out.is_file()
    srt_text = srt_out.read_text(encoding="utf-8")
    assert "00:00:00,000 -->" in srt_text
    assert "1\n" in srt_text

    cues_data = json.loads(cues_out.read_text(encoding="utf-8"))
    assert cues_data["duration_sec"] == 26.0
    assert len(cues_data["cues"]) == 4
    assert len(cues_data["sfx_events"]) == 5


def test_storyboard_validator_detects_broken_timeline_and_dense_vo() -> None:
    validator = _load_module("validate_storyboard", STORYBOARD_DIR / "scripts" / "validate_storyboard.py")
    dense_vo = " ".join(["chu"] * 18)
    broken_md = f"""# Broken Storyboard

## Semantic Color Lock
- `#F3F5F8` Primary

## Shot & Beat Table

| Shot | Time (s) | Visual & Layout (1920x1080) | Motion & Camera | Spoken VO | Sync & SFX |
| :--- | :--- | :--- | :--- | :--- | :--- |
| Shot 1 | 0.0-3.0 | Box `#123456` | Move left | "{dense_vo}" | - |
| Shot 2 | 2.0-5.0 | Circle | Fade in | "Short line" | - |
| Shot 3 | 8.0-8.0 | Triangle | Static | "Zero duration" | - |
| Shot 4 | 8.0-10.0 | Square | Pan | "After gap" | - |
"""
    report = validator.validate_storyboard_text(broken_md, mode="shot", max_wps=3.2)
    codes = {iss.code for iss in report.issues}
    assert "UNDECLARED_COLOR" in codes
    assert "VO_TOO_DENSE" in codes
    assert "TIMELINE_OVERLAP" in codes
    assert "NON_POSITIVE_DURATION" in codes
    assert "TIMELINE_GAP" in codes
    assert report.error_count >= 2
