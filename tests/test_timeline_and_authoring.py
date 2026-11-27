"""Tests for Python authoring API, seek determinism, MathJax SVG cache, layout warnings, and GPU doctor."""

from __future__ import annotations

import json
import pathlib

from stem_engine import SceneBuilder, _stem_native, compile_tex_to_svg, load_excalidraw_anchors


def test_python_doctor_colocated_nvidia() -> None:
    rep = json.loads(_stem_native.doctor_json())
    assert rep["ok"] is True, f"Doctor failed inside python.exe: {rep}"
    assert "NVIDIA" in rep["gl_vendor"]
    assert rep["gl_cuda_colocated"] is True
    assert rep["nvenc_ok"] is True


def test_seek_determinism_through_python_binding() -> None:
    sb = SceneBuilder(width=1280, height=720, duration_sec=6.0)
    b = sb.add_box("block", (-200.0, 0.0), (80.0, 50.0))
    b.move_to(0.0, 4.0, (-200.0, 0.0), (200.0, 120.0))
    sc = sb.build()

    s0 = sc.evaluate(0.0)
    s3_a = sc.evaluate(3.0)
    s1 = sc.evaluate(1.0)
    s3_b = sc.evaluate(3.0)
    assert s0.state_fingerprint() != s3_a.state_fingerprint()
    assert s1.state_fingerprint() != s3_a.state_fingerprint()
    assert s3_a.state_fingerprint() == s3_b.state_fingerprint()


def test_mathjax_svg_formula_compile_and_cache() -> None:
    k1, svg1 = compile_tex_to_svg(r"m a = m g \sin\theta - \mu_k m g \cos\theta")
    k2, svg2 = compile_tex_to_svg(r"m a = m g \sin\theta - \mu_k m g \cos\theta")
    assert k1 == k2
    assert "<svg" in svg1
    assert svg1 == svg2

    asset = _stem_native.parse_svg_to_path_asset(svg1, k1, 48.0)
    assert asset.bounds_max.x > asset.bounds_min.x
    assert asset.bounds_max.y > asset.bounds_min.y


def test_layout_overlap_warnings_and_excalidraw_anchors(tmp_path: pathlib.Path) -> None:
    sb = SceneBuilder(width=1920, height=1080, duration_sec=5.0)
    t1 = sb.add_text("t1", "Force Balance", (0.0, 200.0), font_size_px=40.0, active_window=(0.0, 4.0))
    _t2 = sb.add_text("t2", "Overlapping Label", (10.0, 205.0), font_size_px=40.0, active_window=(1.0, 3.0))
    warnings = sb.check_overlaps()
    assert any(w.a_id == "t1" and w.b_id == "t2" for w in warnings)

    # Place safely using next_to
    sb2 = SceneBuilder(width=1920, height=1080, duration_sec=5.0)
    a = sb2.add_text("a", "Header", (0.0, 200.0), font_size_px=36.0, active_window=(0.0, 4.0))
    b_box = a.layout_box().next_to(a.layout_box(), "down", gap=32.0)
    sb2.add_text("b", "Subheader", b_box, font_size_px=32.0, active_window=(0.0, 4.0))
    assert sb2.check_overlaps() == []

    # Excalidraw anchor loading
    excalidraw_file = tmp_path / "board.excalidraw"
    excalidraw_file.write_text(
        json.dumps(
            {
                "type": "excalidraw",
                "elements": [
                    {"id": "ramp", "x": 100, "y": 300, "width": 600, "height": 300, "text": "Ramp"},
                    {"id": "eq", "x": 800, "y": 100, "width": 400, "height": 120, "text": "Equation"},
                ],
            }
        ),
        encoding="utf-8",
    )
    anchors = load_excalidraw_anchors(excalidraw_file)
    assert "Ramp" in anchors and "Equation" in anchors
    assert anchors["Equation"].cy > anchors["Ramp"].cy  # y-up conversion verified
    assert t1.layout_box().width > 0.0
