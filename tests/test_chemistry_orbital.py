"""Quantum orbital sampling invariants & chemistry scene checks."""

from __future__ import annotations

import math

from examples.chemistry_electron import build_scene

from stem_engine import _stem_native as _nat


def test_hydrogen_orbital_sampler_quantum_invariants() -> None:
    s1 = _nat.sample_hydrogen_orbital_3d(_nat.OrbitalSpec(1, 0, 0, 20.0), 6000, 101)
    p2_0 = _nat.sample_hydrogen_orbital_3d(_nat.OrbitalSpec(2, 1, 0, 20.0), 6000, 102)
    d3_2 = _nat.sample_hydrogen_orbital_3d(_nat.OrbitalSpec(3, 2, 2, 20.0), 6000, 103)
    f4_3 = _nat.sample_hydrogen_orbital_3d(_nat.OrbitalSpec(4, 3, 3, 20.0), 6000, 104)

    # m=0 stationary orbitals must have zero azimuthal probability flow
    assert all(pt.omega_rad_per_sec == 0.0 for pt in s1)
    assert all(pt.omega_rad_per_sec == 0.0 for pt in p2_0)

    # m>0 states carry positive Bohmian azimuthal angular velocity
    assert all(pt.omega_rad_per_sec > 0.0 for pt in d3_2)
    assert all(pt.omega_rad_per_sec > 0.0 for pt in f4_3)

    # Mean radius scales monotonically with principal quantum number n
    def mean_r(samples: list[_nat.OrbitalParticleSample]) -> float:
        return sum(math.hypot(p.r_perp_px, p.y_px) for p in samples) / len(samples)

    assert mean_r(s1) < mean_r(p2_0) < mean_r(d3_2) < mean_r(f4_3)

    # 2p(m=0) aligns along z/y pole (|y| > r_perp on average), while 3d(m=2) aligns near equator (r_perp > |y|)
    p2_polar_ratio = sum(abs(p.y_px) for p in p2_0) / sum(p.r_perp_px for p in p2_0)
    d3_polar_ratio = sum(abs(p.y_px) for p in d3_2) / sum(p.r_perp_px for p in d3_2)
    assert p2_polar_ratio > 1.25
    assert d3_polar_ratio < 0.55


def test_chemistry_scene_layout_and_determinism() -> None:
    sb = build_scene(1920, 1080)
    assert sb.check_overlaps() == []
    sc = sb.build()
    assert sc.stickman_count == 0
    assert abs(sc.duration - 28.0) < 1e-9
    # Deterministic virtual clock across out-of-order seeks
    a = sc.evaluate(19.5).state_fingerprint()
    _ = sc.evaluate(7.8)
    b = sc.evaluate(19.5).state_fingerprint()
    assert a == b
