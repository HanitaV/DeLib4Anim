"""Scientific & FBD consistency tests for the Milestone 3 inclined plane demo."""

from __future__ import annotations

import math

from examples.incline import InclinePhysicsParams, build_scene, simulate_incline_trajectory


def test_incline_fbd_consistency() -> None:
    p = InclinePhysicsParams()
    assert math.isclose(p.weight_n, 19.62, rel_tol=1e-4)
    assert math.isclose(p.normal_n, 19.62 * math.cos(math.radians(30.0)), rel_tol=1e-6)
    assert math.isclose(p.friction_n, 0.20 * p.normal_n, rel_tol=1e-6)
    expected_a = p.g_mps2 * (math.sin(p.theta_rad) - p.mu_k * math.cos(p.theta_rad))
    assert math.isclose(p.accel_mps2, expected_a, rel_tol=1e-6)
    assert 3.20 < p.accel_mps2 < 3.21


def test_incline_fixed_timestep_trajectory_and_layout() -> None:
    p = InclinePhysicsParams()
    samples, t_contact = simulate_incline_trajectory(p, duration_sec=24.0, dt=1.0 / 240.0)
    assert abs(t_contact - p.t_contact_sec) < 0.02
    # Before release, block must remain strictly stationary at s = 0
    for t, s, v in samples:
        if t <= p.t_release_sec:
            assert s == 0.0 and v == 0.0
    # After contact, block rests at ramp_length_m
    assert samples[-1][1] == p.ramp_length_m
    assert samples[-1][2] == 0.0

    # Scene builder verifies zero text/formula overlap, SafeArea compliance & zero stickmen
    sb = build_scene(1920, 1080)
    assert sb.check_overlaps() == []
    sc = sb.build()
    assert sc.stickman_count == 0
    assert abs(sc.duration - 24.0) < 1e-9
    # Determinism check across seeks
    assert sc.evaluate(11.5).state_fingerprint() == sc.evaluate(11.5).state_fingerprint()
