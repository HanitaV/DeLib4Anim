"""Tests for presentation motion physics (pop, stopping inertia), scientific invariance, and 2.5D parallax."""

from __future__ import annotations

import math

from examples.physics_motion_25d import build_scene

from stem_engine import POP_HERO, SceneBuilder, compute_stop_inertia_from_trajectory
from stem_engine import _stem_native as _nat


def test_new_easing_curves_boundaries_and_overshoot() -> None:
    # Impulse: 0 at endpoints, peak near 1.0, both underdamped and critically damped
    for zeta in (0.56, 1.15):
        assert math.isclose(_nat.eval_easing(_nat.EasingType.Impulse, 0.0, 14.0, zeta), 0.0, abs_tol=1e-9)
        assert math.isclose(_nat.eval_easing(_nat.EasingType.Impulse, 1.0, 14.0, zeta), 0.0, abs_tol=1e-9)
        peak = max(_nat.eval_easing(_nat.EasingType.Impulse, i / 100.0, 14.0, zeta) for i in range(1, 100))
        assert 0.85 <= peak <= 1.05

    # BackOut overshoots > 1.0, BackIn dips < 0.0, ExpoOut reaches 1.0 cleanly
    assert _nat.eval_easing(_nat.EasingType.BackOut, 0.65, 14.0, 0.65) > 1.0
    assert math.isclose(_nat.eval_easing(_nat.EasingType.BackOut, 1.0, 14.0, 0.65), 1.0, abs_tol=1e-9)
    assert _nat.eval_easing(_nat.EasingType.BackIn, 0.25, 14.0, 0.65) < 0.0
    assert math.isclose(_nat.eval_easing(_nat.EasingType.ExpoOut, 0.0, 14.0, 0.65), 0.0, abs_tol=1e-9)
    assert math.isclose(_nat.eval_easing(_nat.EasingType.ExpoOut, 1.0, 14.0, 0.65), 1.0, abs_tol=1e-9)


def test_scientific_invariance_under_pop_and_inertia_stop() -> None:
    sb = SceneBuilder(width=1920, height=1080, duration_sec=6.0)

    # 1. Formula layout_box must remain invariant under pop_in
    f = sb.add_formula("eq", r"F = ma", (120.0, 220.0), target_height_px=48.0, active_window=(0.5, 5.5))
    box_before = f.layout_box(0.6)
    f.pop_in(0.5, style=POP_HERO)
    box_after = f.layout_box(0.6)
    assert box_before == box_after

    # 2. Trajectory2D velocity inference & inertia_stop settles cleanly to resting fingerprint
    dt = 1.0 / 240.0
    samples = [(min(1.0, i * dt / 1.5) * 400.0, 0.0) for i in range(round(6.0 / dt) + 2)]
    tr_id = sb.add_trajectory(samples, t_start=0.0, dt=dt)

    tr_obj = _nat.Trajectory2D()
    tr_obj.t_start = 0.0
    tr_obj.dt = dt
    tr_obj.samples = [_nat.Vec2(p[0], p[1]) for p in samples]
    direction, dist = compute_stop_inertia_from_trajectory(tr_obj, 1.5)
    assert direction[0] > 0.99 and dist > 0.0

    blk = sb.add_box("blk", (0.0, 0.0), (100.0, 50.0), trajectory_id=tr_id, material="card25d", elevation=12.0)
    blk.inertia_stop(1.5, direction=direction, distance_px=dist, squash=0.08, tilt_rad=-0.03, duration=0.44)

    # Reference scene without inertia_stop to verify exact resting state after settle
    sb_ref = SceneBuilder(width=1920, height=1080, duration_sec=6.0)
    f_ref = sb_ref.add_formula("eq", r"F = ma", (120.0, 220.0), target_height_px=48.0, active_window=(0.5, 5.5))
    f_ref.pop_in(0.5, style=POP_HERO)
    tr_ref_id = sb_ref.add_trajectory(samples, t_start=0.0, dt=dt)
    sb_ref.add_box("blk", (0.0, 0.0), (100.0, 50.0), trajectory_id=tr_ref_id, material="card25d", elevation=12.0)

    sc = sb.build()
    sc_ref = sb_ref.build()
    # During inertia window, presentation tracks actively deform scene state
    assert sc.evaluate(1.54).state_fingerprint() != sc_ref.evaluate(1.54).state_fingerprint()
    # After inertia settles (t > 1.94), scene returns identically to resting reference state
    assert sc.evaluate(2.20).state_fingerprint() == sc_ref.evaluate(2.20).state_fingerprint()
    assert sc.evaluate(4.50).state_fingerprint() == sc_ref.evaluate(4.50).state_fingerprint()


def test_25d_parallax_and_showcase_scene_layout() -> None:
    # Parallax layer reacts differently to camera pan than parallax=1.0 layer
    sb_p = SceneBuilder(width=1280, height=720, duration_sec=4.0)
    sb_p.camera.set_pose(pos=(0.0, 0.0), zoom=1.0)
    sb_p.camera.move_to(0.0, 2.0, (0.0, 0.0), (200.0, 0.0))
    sb_p.add_box("bg", (0.0, 0.0), (80.0, 80.0), parallax=0.4, material="card25d", elevation=8.0)
    sc_p = sb_p.build()
    assert sc_p.evaluate(0.0).state_fingerprint() != sc_p.evaluate(2.0).state_fingerprint()

    # Dedicated physics_motion_25d showcase passes overlap/safe-area checks and seek determinism
    sb = build_scene(1920, 1080)
    assert sb.check_overlaps() == []
    sc = sb.build()
    assert sc.stickman_count == 0
    assert abs(sc.duration - 16.0) < 1e-9
    fp_a = sc.evaluate(7.45).state_fingerprint()
    _ = sc.evaluate(2.10)
    fp_b = sc.evaluate(7.45).state_fingerprint()
    assert fp_a == fp_b
