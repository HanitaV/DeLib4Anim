"""Reusable presentation motion physics presets and track helpers for STEM acting."""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Any

from stem_engine import _stem_native as _nat


@dataclass(frozen=True)
class PopSpec:
    """Calibrated parameters for harmonious pop-in / pop-out entrances."""

    duration: float = 0.42
    initial_scale: float = 0.84
    rise_px: float = 14.0
    omega: float = 16.5
    zeta: float = 0.62
    alpha_lead_ratio: float = 0.45


@dataclass(frozen=True)
class InertiaSpec:
    """Calibrated parameters for stopping inertia, COM pitch, and contact squash."""

    duration: float = 0.44
    distance_px: float = 8.0
    squash: float = 0.075
    tilt_rad: float = -0.032
    omega: float = 14.0
    zeta: float = 0.56


POP_HERO = PopSpec(duration=0.42, initial_scale=0.84, rise_px=14.0, omega=16.5, zeta=0.62, alpha_lead_ratio=0.45)
POP_SNAPPY = PopSpec(duration=0.34, initial_scale=0.45, rise_px=6.0, omega=19.0, zeta=0.56, alpha_lead_ratio=0.38)
POP_SOFT = PopSpec(duration=0.46, initial_scale=0.92, rise_px=10.0, omega=14.0, zeta=0.74, alpha_lead_ratio=0.52)
INERTIA_DEFAULT = InertiaSpec()


def _normalize_dir(direction: tuple[float, float]) -> tuple[float, float]:
    dx, dy = float(direction[0]), float(direction[1])
    mag = math.hypot(dx, dy)
    if mag <= 1e-6:
        return (1.0, 0.0)
    return (dx / mag, dy / mag)


def compute_stop_inertia_from_trajectory(
    traj: _nat.Trajectory2D,
    t_stop: float,
    *,
    max_distance_px: float = 9.0,
    px_per_speed: float = 0.012,
) -> tuple[tuple[float, float], float]:
    """Infers unit travel direction and speed-proportional inertia distance before `t_stop`."""
    probe_t = max(float(traj.t_start), float(t_stop) - 0.025)
    vel = traj.evaluate_vel(probe_t)
    speed = math.hypot(vel.x, vel.y)
    if speed <= 1e-4:
        return ((1.0, 0.0), 0.0)
    dist = min(float(max_distance_px), speed * float(px_per_speed))
    return ((vel.x / speed, vel.y / speed), dist)


def apply_pop_in(
    item: Any,
    t0: float,
    *,
    spec: PopSpec = POP_HERO,
    duration: float | None = None,
    initial_scale: float | None = None,
    rise_px: float | None = None,
    target_elevation: float | None = None,
) -> None:
    """Applies a harmonious pop-in (fast alpha lead + upward settle + spring scale + optional shadow lift)."""
    dur = max(0.08, float(spec.duration if duration is None else duration))
    s0 = float(spec.initial_scale if initial_scale is None else initial_scale)
    rise = float(spec.rise_px if rise_px is None else rise_px)
    t_end = t0 + dur
    t_alpha = t0 + dur * max(0.20, min(1.0, spec.alpha_lead_ratio))

    item.visible.add_segment(t0, t_alpha, 0.0, 1.0, _nat.EasingType.QuinticOut)

    if abs(rise) > 1e-4 and hasattr(item, "presentation_offset"):
        item.presentation_offset.add_segment(
            t0, t_end, _nat.Vec2(0.0, -rise), _nat.Vec2(0.0, 0.0), _nat.EasingType.QuinticOut
        )

    if hasattr(item, "presentation_scale"):
        if isinstance(item, _nat.RectItem):
            item.presentation_scale.add_segment(
                t0,
                t_end,
                _nat.Vec2(s0, s0),
                _nat.Vec2(1.0, 1.0),
                _nat.EasingType.Spring,
                spec.omega,
                spec.zeta,
            )
        else:
            item.presentation_scale.add_segment(
                t0, t_end, s0, 1.0, _nat.EasingType.Spring, spec.omega, spec.zeta
            )

    if hasattr(item, "elevation"):
        elev_goal = float(item.elevation.default_value if target_elevation is None else target_elevation)
        if elev_goal > 0.05:
            item.elevation.add_segment(t0, t_end, 0.0, elev_goal, _nat.EasingType.QuinticOut)


def apply_pop_out(
    item: Any,
    t0: float,
    *,
    duration: float = 0.28,
    end_scale: float = 0.86,
    drop_px: float = 8.0,
) -> None:
    """Applies a crisp anticipation-tuck pop-out."""
    dur = max(0.06, float(duration))
    t_end = t0 + dur
    item.visible.add_segment(t0 + dur * 0.15, t_end, 1.0, 0.0, _nat.EasingType.QuinticInOut)

    if abs(drop_px) > 1e-4 and hasattr(item, "presentation_offset"):
        item.presentation_offset.add_segment(
            t0, t_end, _nat.Vec2(0.0, 0.0), _nat.Vec2(0.0, -float(drop_px)), _nat.EasingType.BackIn, 14.0, 0.52
        )

    if hasattr(item, "presentation_scale"):
        if isinstance(item, _nat.RectItem):
            item.presentation_scale.add_segment(
                t0,
                t_end,
                _nat.Vec2(1.0, 1.0),
                _nat.Vec2(float(end_scale), float(end_scale)),
                _nat.EasingType.BackIn,
                14.0,
                0.55,
            )
        else:
            item.presentation_scale.add_segment(
                t0, t_end, 1.0, float(end_scale), _nat.EasingType.BackIn, 14.0, 0.55
            )


def apply_inertia_stop(
    item: Any,
    t_stop: float,
    direction: tuple[float, float] = (1.0, 0.0),
    *,
    spec: InertiaSpec = INERTIA_DEFAULT,
    distance_px: float | None = None,
    squash: float | None = None,
    tilt_rad: float | None = None,
    duration: float | None = None,
) -> None:
    """Applies stateless stopping inertia (translation impulse + optional COM pitch + volume squash)."""
    ux, uy = _normalize_dir(direction)
    dur = max(0.10, float(spec.duration if duration is None else duration))
    dist = float(spec.distance_px if distance_px is None else distance_px)
    sq = max(0.0, min(0.35, float(spec.squash if squash is None else squash)))
    tilt = float(spec.tilt_rad if tilt_rad is None else tilt_rad)
    t_end = t_stop + dur

    if abs(dist) > 1e-4 and hasattr(item, "presentation_offset"):
        item.presentation_offset.add_segment(
            t_stop,
            t_end,
            _nat.Vec2(0.0, 0.0),
            _nat.Vec2(ux * dist, uy * dist),
            _nat.EasingType.Impulse,
            spec.omega,
            spec.zeta,
        )

    if isinstance(item, _nat.RectItem):
        if sq > 1e-4:
            # Volume/area-preserving compression along impact axis (ux^2 + uy^2 == 1)
            s_comp = 1.0 - sq
            s_exp = 1.0 / s_comp
            wx = ux * ux
            wy = uy * uy
            sx = s_comp * wx + s_exp * wy
            sy = s_exp * wx + s_comp * wy
            item.presentation_scale.add_segment(
                t_stop,
                t_end,
                _nat.Vec2(1.0, 1.0),
                _nat.Vec2(sx, sy),
                _nat.EasingType.Impulse,
                spec.omega * 1.15,
                spec.zeta,
            )
        if abs(tilt) > 1e-5:
            sign_x = -1.0 if ux >= 0.0 else 1.0
            peak_tilt = abs(tilt) * sign_x if tilt < 0.0 else tilt
            item.presentation_rotation_rad.add_segment(
                t_stop,
                t_end,
                0.0,
                peak_tilt,
                _nat.EasingType.Impulse,
                spec.omega * 0.92,
                spec.zeta,
            )
    elif sq > 1e-4 and hasattr(item, "presentation_scale"):
        item.presentation_scale.add_segment(
            t_stop,
            t_end,
            1.0,
            1.0 + sq * 0.75,
            _nat.EasingType.Impulse,
            spec.omega * 1.15,
            spec.zeta,
        )
