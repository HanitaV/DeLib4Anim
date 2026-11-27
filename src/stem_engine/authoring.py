"""Typed Python authoring API, storyboard anchors, layout checks, and STEM acting primitives."""

from __future__ import annotations

import json
import math
import pathlib
from collections.abc import Sequence
from dataclasses import dataclass
from typing import Any

from stem_engine import _stem_native as _nat
from stem_engine.formula import compile_tex_to_svg
from stem_engine.motion import (
    INERTIA_DEFAULT,
    POP_HERO,
    POP_SNAPPY,
    POP_SOFT,
    InertiaSpec,
    PopSpec,
    apply_inertia_stop,
    apply_pop_in,
    apply_pop_out,
)


def resolve_font(variant: str = "regular") -> str:
    """Resolve an OS TrueType font path for the given typographic weight/style."""
    return str(_nat.resolve_font_variant(variant))


def _to_vec2(v: tuple[float, float] | _nat.Vec2) -> _nat.Vec2:
    if isinstance(v, _nat.Vec2):
        return v
    return _nat.Vec2(float(v[0]), float(v[1]))


def _to_color(c: tuple[float, float, float, float] | tuple[float, float, float] | _nat.ColorRGBA) -> _nat.ColorRGBA:
    if isinstance(c, _nat.ColorRGBA):
        return c
    if len(c) == 3:
        return _nat.ColorRGBA(float(c[0]), float(c[1]), float(c[2]), 1.0)
    return _nat.ColorRGBA(float(c[0]), float(c[1]), float(c[2]), float(c[3]))


@dataclass(frozen=True)
class LayoutBox:
    """Axis-aligned screen/world layout bounding box for anchor & overlap checks."""

    obj_id: str
    cx: float
    cy: float
    width: float
    height: float
    screen_space: bool = True

    @property
    def left(self) -> float:
        return self.cx - self.width * 0.5

    @property
    def right(self) -> float:
        return self.cx + self.width * 0.5

    @property
    def bottom(self) -> float:
        return self.cy - self.height * 0.5

    @property
    def top(self) -> float:
        return self.cy + self.height * 0.5

    def next_to(
        self,
        other: LayoutBox,
        direction: str = "right",
        gap: float = 24.0,
    ) -> tuple[float, float]:
        if direction == "right":
            return (other.right + gap + self.width * 0.5, other.cy)
        if direction == "left":
            return (other.left - gap - self.width * 0.5, other.cy)
        if direction == "up":
            return (other.cx, other.top + gap + self.height * 0.5)
        if direction == "down":
            return (other.cx, other.bottom - gap - self.height * 0.5)
        return (other.cx, other.cy)


@dataclass(frozen=True)
class OverlapWarning:
    a_id: str
    b_id: str
    overlap_area: float
    message: str


@dataclass(frozen=True)
class SafeArea:
    width: int = 1920
    height: int = 1080
    margin_x: float = 72.0
    margin_y: float = 54.0

    def contains(self, box: LayoutBox) -> bool:
        hw = self.width * 0.5 - self.margin_x
        hh = self.height * 0.5 - self.margin_y
        return box.left >= -hw and box.right <= hw and box.bottom >= -hh and box.top <= hh


def load_excalidraw_anchors(path: str | pathlib.Path) -> dict[str, LayoutBox]:
    """Reads storyboard anchors from an `.excalidraw` JSON file into engine-centered coordinates."""
    data = json.loads(pathlib.Path(path).read_text(encoding="utf-8"))
    elements = data.get("elements", [])
    if not elements:
        return {}

    min_x = min(e["x"] for e in elements)
    max_x = max(e["x"] + e.get("width", 0.0) for e in elements)
    min_y = min(e["y"] for e in elements)
    max_y = max(e["y"] + e.get("height", 0.0) for e in elements)
    mid_x = 0.5 * (min_x + max_x)
    mid_y = 0.5 * (min_y + max_y)

    anchors: dict[str, LayoutBox] = {}
    for idx, el in enumerate(elements):
        label = str(el.get("id") or f"el_{idx}")
        if el.get("text"):
            label = str(el["text"]).splitlines()[0][:32]
        w = float(el.get("width", 0.0))
        h = float(el.get("height", 0.0))
        cx = (float(el["x"]) + w * 0.5) - mid_x
        cy = -(float(el["y"]) + h * 0.5 - mid_y)  # Excalidraw is y-down; engine is y-up
        anchors[label] = LayoutBox(obj_id=label, cx=cx, cy=cy, width=w, height=h)
    return anchors


class Cue:
    def __init__(self, cue_id: str, time_sec: float, description: str = "") -> None:
        self.cue_id = cue_id
        self.time_sec = float(time_sec)
        self.description = description


_SFX_NONE: _nat.ProceduralSfxKind = getattr(_nat.ProceduralSfxKind, "None")


class AudioCue:
    def __init__(
        self,
        cue_id: str,
        time_sec: float,
        *,
        duration_sec: float = 0.25,
        gain: float = 0.45,
        pitch_hz: float = 440.0,
        sfx_kind: _nat.ProceduralSfxKind = _SFX_NONE,
        wav_path: str = "",
        is_test_tone: bool = False,
    ) -> None:
        self.native = _nat.AudioEvent()
        self.native.cue_id = cue_id
        self.native.time_sec = float(time_sec)
        self.native.duration_sec = float(duration_sec)
        self.native.gain = float(gain)
        self.native.pitch_hz = float(pitch_hz)
        self.native.sfx_kind = sfx_kind
        self.native.wav_path = wav_path
        self.native.is_test_tone = bool(is_test_tone)


class Camera:
    def __init__(self, native_cam: _nat.CameraSpec) -> None:
        self.native = native_cam

    def set_pose(
        self,
        pos: tuple[float, float] = (0.0, 0.0),
        zoom: float = 1.0,
        rotation_rad: float = 0.0,
    ) -> Camera:
        self.native.position.set_constant(_to_vec2(pos))
        self.native.zoom.set_constant(float(zoom))
        self.native.rotation_rad.set_constant(float(rotation_rad))
        return self

    def move_to(
        self,
        t0: float,
        t1: float,
        pos0: tuple[float, float],
        pos1: tuple[float, float],
        zoom0: float = 1.0,
        zoom1: float = 1.0,
        easing: _nat.EasingType = _nat.EasingType.QuinticInOut,
    ) -> Camera:
        self.native.position.add_segment(t0, t1, _to_vec2(pos0), _to_vec2(pos1), easing)
        self.native.zoom.add_segment(t0, t1, float(zoom0), float(zoom1), easing)
        return self

    def move_with_settle(
        self,
        t0: float,
        t1: float,
        pos0: tuple[float, float],
        pos1: tuple[float, float],
        zoom0: float = 1.0,
        zoom1: float = 1.0,
        *,
        settle_duration: float = 0.28,
        overshoot_frac: float = 0.025,
        easing: _nat.EasingType = _nat.EasingType.QuinticInOut,
    ) -> Camera:
        self.move_to(t0, t1, pos0, pos1, zoom0, zoom1, easing=easing)
        dx = float(pos1[0]) - float(pos0[0])
        dy = float(pos1[1]) - float(pos0[1])
        if abs(dx) > 1e-4 or abs(dy) > 1e-4:
            self.native.position.add_segment(
                t1,
                t1 + max(0.08, float(settle_duration)),
                _to_vec2(pos1),
                _to_vec2((float(pos1[0]) + dx * overshoot_frac, float(pos1[1]) + dy * overshoot_frac)),
                _nat.EasingType.Impulse,
                12.0,
                0.72,
            )
        return self

    def follow_trajectory(
        self,
        traj_id: int,
        t0: float,
        t1: float,
        offset: tuple[float, float] = (0.0, 0.0),
        window_sec: float = 0.16,
    ) -> Camera:
        self.native.follow_trajectory_id = traj_id
        self.native.follow_offset = _to_vec2(offset)
        self.native.follow_smoothing_window_sec = float(window_sec)
        self.native.follow_weight.add_segment(t0, t0 + 0.35, 0.0, 1.0, _nat.EasingType.QuinticOut)
        self.native.follow_weight.add_segment(t1 - 0.45, t1, 1.0, 0.0, _nat.EasingType.QuinticInOut)
        return self


class Body:
    def __init__(self, obj_id: str, native_item: _nat.RectItem | _nat.CircleItem) -> None:
        self.obj_id = obj_id
        self.native = native_item

    def reveal(self, t0: float, t1: float) -> Body:
        self.native.visible.add_segment(t0, t1, 0.0, 1.0, _nat.EasingType.QuinticOut)
        return self

    def hide(self, t0: float, t1: float) -> Body:
        self.native.visible.add_segment(t0, t1, 1.0, 0.0, _nat.EasingType.QuinticInOut)
        return self

    def pop_in(
        self,
        t0: float,
        *,
        style: PopSpec | None = None,
        duration: float | None = None,
        initial_scale: float | None = None,
        rise_px: float | None = None,
        target_elevation: float | None = None,
    ) -> Body:
        default_spec = POP_SNAPPY if isinstance(self.native, _nat.CircleItem) else POP_HERO
        apply_pop_in(
            self.native,
            t0,
            spec=style or default_spec,
            duration=duration,
            initial_scale=initial_scale,
            rise_px=rise_px,
            target_elevation=target_elevation,
        )
        return self

    def pop_out(
        self,
        t0: float,
        *,
        duration: float = 0.28,
        end_scale: float = 0.86,
        drop_px: float = 8.0,
    ) -> Body:
        apply_pop_out(self.native, t0, duration=duration, end_scale=end_scale, drop_px=drop_px)
        return self

    def inertia_stop(
        self,
        t_stop: float,
        direction: tuple[float, float] = (1.0, 0.0),
        *,
        spec: InertiaSpec = INERTIA_DEFAULT,
        distance_px: float | None = None,
        squash: float | None = None,
        tilt_rad: float | None = None,
        duration: float | None = None,
    ) -> Body:
        apply_inertia_stop(
            self.native,
            t_stop,
            direction=direction,
            spec=spec,
            distance_px=distance_px,
            squash=squash,
            tilt_rad=tilt_rad,
            duration=duration,
        )
        return self

    def move_to(
        self,
        t0: float,
        t1: float,
        p0: tuple[float, float],
        p1: tuple[float, float],
        easing: _nat.EasingType = _nat.EasingType.QuinticInOut,
    ) -> Body:
        self.native.center.add_segment(t0, t1, _to_vec2(p0), _to_vec2(p1), easing)
        return self

    def squash_contact(self, t_contact: float, duration: float = 0.24) -> Body:
        if isinstance(self.native, _nat.RectItem):
            mid = t_contact + duration * 0.25
            end = t_contact + duration
            self.native.presentation_scale.add_segment(
                t_contact, mid, _to_vec2((1.0, 1.0)), _to_vec2((1.12, 0.88)), _nat.EasingType.QuinticOut
            )
            self.native.presentation_scale.add_segment(
                mid, end, _to_vec2((1.12, 0.88)), _to_vec2((1.0, 1.0)), _nat.EasingType.Spring, 18.0, 0.55
            )
        return self


class Vector:
    def __init__(self, obj_id: str, native_item: _nat.VectorItem) -> None:
        self.obj_id = obj_id
        self.native = native_item

    def reveal(
        self,
        t0: float,
        t1: float,
        easing: _nat.EasingType = _nat.EasingType.QuinticOut,
    ) -> Vector:
        self.native.visible.add_segment(t0, min(t1, t0 + 0.08), 0.0, 1.0, _nat.EasingType.Linear)
        self.native.reveal.add_segment(t0, t1, 0.0, 1.0, easing)
        return self

    def hide(self, t0: float, t1: float) -> Vector:
        self.native.visible.add_segment(t0, t1, 1.0, 0.0, _nat.EasingType.QuinticInOut)
        return self

    def pop_in(
        self,
        t0: float,
        *,
        style: PopSpec = POP_SNAPPY,
        duration: float | None = None,
        initial_scale: float | None = None,
        rise_px: float | None = None,
    ) -> Vector:
        apply_pop_in(self.native, t0, spec=style, duration=duration, initial_scale=initial_scale, rise_px=rise_px)
        return self

    def inertia_stop(
        self,
        t_stop: float,
        direction: tuple[float, float] = (1.0, 0.0),
        *,
        distance_px: float = 5.0,
        duration: float = 0.36,
    ) -> Vector:
        apply_inertia_stop(self.native, t_stop, direction=direction, distance_px=distance_px, squash=0.0, tilt_rad=0.0, duration=duration)
        return self


class Graph:
    def __init__(self, obj_id: str, native_item: _nat.GraphItem) -> None:
        self.obj_id = obj_id
        self.native = native_item

    def reveal(
        self,
        t0: float,
        t1: float,
        easing: _nat.EasingType = _nat.EasingType.Linear,
    ) -> Graph:
        self.native.visible.add_segment(t0, min(t1, t0 + 0.05), 0.0, 1.0, _nat.EasingType.Linear)
        self.native.reveal.add_segment(t0, t1, 0.0, 1.0, easing)
        return self

    def hide(self, t0: float, t1: float) -> Graph:
        self.native.visible.add_segment(t0, t1, 1.0, 0.0, _nat.EasingType.QuinticInOut)
        return self


class Text:
    def __init__(
        self,
        obj_id: str,
        native_item: _nat.TextItem,
        bounds: _nat.TextBounds,
    ) -> None:
        self.obj_id = obj_id
        self.native = native_item
        self.bounds = bounds

    def layout_box(self, t: float = 0.0) -> LayoutBox:
        p = self.native.position.evaluate(t)
        s = self.native.scale.evaluate(t)
        w = self.bounds.width * s
        h = self.bounds.height * s
        cx = p.x
        if self.native.align_h < 0:
            cx = p.x + w * 0.5
        elif self.native.align_h > 0:
            cx = p.x - w * 0.5
        return LayoutBox(
            obj_id=self.obj_id,
            cx=cx,
            cy=p.y,
            width=w,
            height=h,
            screen_space=self.native.screen_space,
        )

    def reveal(self, t0: float, t1: float) -> Text:
        self.native.visible.add_segment(t0, t1, 0.0, 1.0, _nat.EasingType.QuinticOut)
        return self

    def hide(self, t0: float, t1: float) -> Text:
        self.native.visible.add_segment(t0, t1, 1.0, 0.0, _nat.EasingType.QuinticInOut)
        return self

    def pop_in(
        self,
        t0: float,
        *,
        style: PopSpec = POP_SOFT,
        duration: float | None = None,
        initial_scale: float | None = None,
        rise_px: float | None = None,
    ) -> Text:
        apply_pop_in(self.native, t0, spec=style, duration=duration, initial_scale=initial_scale, rise_px=rise_px)
        return self

    def pop_out(self, t0: float, *, duration: float = 0.28, end_scale: float = 0.88, drop_px: float = 6.0) -> Text:
        apply_pop_out(self.native, t0, duration=duration, end_scale=end_scale, drop_px=drop_px)
        return self

    def inertia_stop(
        self,
        t_stop: float,
        direction: tuple[float, float] = (1.0, 0.0),
        *,
        distance_px: float = 5.0,
        duration: float = 0.36,
    ) -> Text:
        apply_inertia_stop(self.native, t_stop, direction=direction, distance_px=distance_px, squash=0.0, tilt_rad=0.0, duration=duration)
        return self


class Formula:
    def __init__(
        self,
        obj_id: str,
        native_item: _nat.PathInstanceItem,
        asset: _nat.PathAsset,
    ) -> None:
        self.obj_id = obj_id
        self.native = native_item
        self.asset = asset

    def layout_box(self, t: float = 0.0) -> LayoutBox:
        p = self.native.position.evaluate(t)
        s = self.native.scale.evaluate(t)
        w = (self.asset.bounds_max.x - self.asset.bounds_min.x) * s
        h = (self.asset.bounds_max.y - self.asset.bounds_min.y) * s
        return LayoutBox(
            obj_id=self.obj_id,
            cx=p.x,
            cy=p.y,
            width=w,
            height=h,
            screen_space=self.native.screen_space,
        )

    def reveal(self, t0: float, t1: float) -> Formula:
        self.native.visible.add_segment(t0, t1, 0.0, 1.0, _nat.EasingType.QuinticOut)
        return self

    def hide(self, t0: float, t1: float) -> Formula:
        self.native.visible.add_segment(t0, t1, 1.0, 0.0, _nat.EasingType.QuinticInOut)
        return self

    def dim_to(self, t0: float, t1: float, target_alpha: float = 0.35, from_alpha: float = 1.0) -> Formula:
        self.native.visible.add_segment(t0, t1, float(from_alpha), float(target_alpha), _nat.EasingType.QuinticInOut)
        return self

    def pop_in(
        self,
        t0: float,
        *,
        style: PopSpec = POP_HERO,
        duration: float | None = None,
        initial_scale: float | None = None,
        rise_px: float | None = None,
    ) -> Formula:
        apply_pop_in(self.native, t0, spec=style, duration=duration, initial_scale=initial_scale, rise_px=rise_px)
        return self

    def pop_out(self, t0: float, *, duration: float = 0.28, end_scale: float = 0.88, drop_px: float = 8.0) -> Formula:
        apply_pop_out(self.native, t0, duration=duration, end_scale=end_scale, drop_px=drop_px)
        return self


class FormulaCard:
    """Thin-bordered rounded dark box enclosing a MathJax Formula (Median 2.5D card aesthetic)."""

    def __init__(self, obj_id: str, formula: Formula, card: Body) -> None:
        self.obj_id = obj_id
        self.formula = formula
        self.card = card

    def reveal(self, t0: float, t1: float) -> FormulaCard:
        self.card.reveal(t0, t1)
        self.formula.reveal(t0, t1)
        return self

    def hide(self, t0: float, t1: float) -> FormulaCard:
        self.card.hide(t0, t1)
        self.formula.hide(t0, t1)
        return self

    def pop_in(
        self,
        t0: float,
        *,
        style: PopSpec = POP_HERO,
        stagger: float = 0.04,
        duration: float | None = None,
    ) -> FormulaCard:
        self.card.pop_in(t0, style=style, duration=duration)
        self.formula.pop_in(t0 + stagger, style=style, duration=duration, initial_scale=max(0.90, style.initial_scale))
        return self

    def pop_out(self, t0: float, *, duration: float = 0.28) -> FormulaCard:
        self.formula.pop_out(t0, duration=duration)
        self.card.pop_out(t0 + 0.025, duration=duration)
        return self

    def dim_to(self, t0: float, t1: float, target_alpha: float = 0.35, from_alpha: float = 1.0) -> FormulaCard:
        self.card.native.visible.add_segment(t0, t1, float(from_alpha), float(target_alpha), _nat.EasingType.QuinticInOut)
        self.formula.dim_to(t0, t1, target_alpha, from_alpha)
        return self


class VectorLabelGroup:
    """Median-style vector badge: bold-italic symbol + over-arrow + optional subscript + muted subtitle."""

    def __init__(
        self,
        obj_id: str,
        texts: list[Text],
        arrow: Vector,
    ) -> None:
        self.obj_id = obj_id
        self.texts = texts
        self.arrow = arrow

    def reveal(self, t0: float, t1: float) -> VectorLabelGroup:
        for txt in self.texts:
            txt.reveal(t0, t1)
        self.arrow.native.visible.add_segment(t0, t1, 0.0, 1.0, _nat.EasingType.QuinticOut)
        return self

    def hide(self, t0: float, t1: float) -> VectorLabelGroup:
        for txt in self.texts:
            txt.hide(t0, t1)
        self.arrow.hide(t0, t1)
        return self

    def pop_in(self, t0: float, *, stagger: float = 0.035, style: PopSpec = POP_SNAPPY) -> VectorLabelGroup:
        if self.texts:
            self.texts[0].pop_in(t0, style=style)
        apply_pop_in(self.arrow.native, t0, spec=style)
        for idx, txt in enumerate(self.texts[1:], start=1):
            txt.pop_in(t0 + stagger * idx, style=POP_SOFT)
        return self

    def inertia_stop(
        self,
        t_stop: float,
        direction: tuple[float, float] = (1.0, 0.0),
        *,
        distance_px: float = 6.0,
        duration: float = 0.40,
    ) -> VectorLabelGroup:
        for txt in self.texts:
            txt.inertia_stop(t_stop, direction, distance_px=distance_px, duration=duration)
        self.arrow.inertia_stop(t_stop, direction, distance_px=distance_px, duration=duration)
        return self

    def move_by(
        self,
        t0: float,
        t1: float,
        delta0: tuple[float, float],
        delta1: tuple[float, float],
        easing: _nat.EasingType = _nat.EasingType.QuinticInOut,
    ) -> VectorLabelGroup:
        for txt in self.texts:
            base = txt.native.position.evaluate(t0)
            p0 = (base.x + delta0[0], base.y + delta0[1])
            p1 = (base.x + delta1[0], base.y + delta1[1])
            txt.native.position.add_segment(t0, t1, _to_vec2(p0), _to_vec2(p1), easing)
        a_base = self.arrow.native.origin.evaluate(t0)
        a0 = (a_base.x + delta0[0], a_base.y + delta0[1])
        a1 = (a_base.x + delta1[0], a_base.y + delta1[1])
        self.arrow.native.origin.add_segment(t0, t1, _to_vec2(a0), _to_vec2(a1), easing)
        return self


class OrbitalCloud:
    """Wrapper around native GPU 3D quantum probability cloud (`OrbitalCloud3DItem`)."""

    def __init__(self, obj_id: str, native_item: _nat.OrbitalCloud3DItem) -> None:
        self.obj_id = obj_id
        self.native = native_item

    def reveal(self, t0: float, t1: float, easing: _nat.EasingType = _nat.EasingType.QuinticOut) -> OrbitalCloud:
        self.native.visible.add_segment(t0, min(t1, t0 + 0.05), 0.0, 1.0, _nat.EasingType.Linear)
        self.native.reveal.add_segment(t0, t1, 0.0, 1.0, easing)
        return self

    def hide(self, t0: float, t1: float, easing: _nat.EasingType = _nat.EasingType.QuinticInOut) -> OrbitalCloud:
        self.native.reveal.add_segment(t0, t1, 1.0, 0.0, easing)
        self.native.visible.add_segment(max(t0, t1 - 0.05), t1, 1.0, 0.0, _nat.EasingType.Linear)
        return self

    def morph(
        self,
        t0: float,
        t1: float,
        v0: float = 0.0,
        v1: float = 1.0,
        easing: _nat.EasingType = _nat.EasingType.QuinticInOut,
    ) -> OrbitalCloud:
        self.native.morph.add_segment(t0, t1, float(v0), float(v1), easing)
        return self

    def orbit_camera(
        self,
        t0: float,
        t1: float,
        yaw0: float,
        yaw1: float,
        pitch0: float,
        pitch1: float,
        easing: _nat.EasingType = _nat.EasingType.QuinticInOut,
    ) -> OrbitalCloud:
        self.native.yaw_rad.add_segment(t0, t1, float(yaw0), float(yaw1), easing)
        self.native.pitch_rad.add_segment(t0, t1, float(pitch0), float(pitch1), easing)
        return self

    def collapse(
        self,
        t0: float,
        t1: float,
        v0: float = 0.0,
        v1: float = 1.0,
        easing: _nat.EasingType = _nat.EasingType.QuinticInOut,
    ) -> OrbitalCloud:
        self.native.collapse.add_segment(t0, t1, float(v0), float(v1), easing)
        return self


class CalloutBadge:
    """Median-style numbered circular callout (`1`, `2`, `3`) with focus dimming."""

    def __init__(self, obj_id: str, circle: Body, num_text: Text, label_text: Text) -> None:
        self.obj_id = obj_id
        self.circle = circle
        self.num_text = num_text
        self.label_text = label_text

    def activate(self, t0: float, t1: float, *, pop: bool = False, stagger: float = 0.035) -> CalloutBadge:
        if pop:
            dur = max(0.28, t1 - t0)
            self.circle.pop_in(t0, style=POP_SNAPPY, duration=dur)
            self.num_text.pop_in(t0 + stagger * 0.7, style=POP_SNAPPY, duration=dur)
            self.label_text.pop_in(t0 + stagger, style=POP_SOFT, duration=dur)
        else:
            self.circle.native.visible.add_segment(t0, t1, 0.0, 1.0, _nat.EasingType.QuinticOut)
            self.num_text.native.visible.add_segment(t0, t1, 0.0, 1.0, _nat.EasingType.QuinticOut)
            self.label_text.native.visible.add_segment(t0, t1, 0.0, 1.0, _nat.EasingType.QuinticOut)
        self.circle.native.fill_color.add_segment(
            t0, t1, _to_color((0.02, 0.02, 0.03, 0.0)), _to_color((0.92, 0.94, 0.97, 1.0)), _nat.EasingType.QuinticOut
        )
        self.num_text.native.color.add_segment(
            t0, t1, _to_color((0.88, 0.90, 0.94, 1.0)), _to_color((0.05, 0.05, 0.07, 1.0)), _nat.EasingType.QuinticOut
        )
        self.label_text.native.color.add_segment(
            t0, t1, _to_color((0.46, 0.48, 0.53, 1.0)), _to_color((0.95, 0.96, 0.98, 1.0)), _nat.EasingType.QuinticOut
        )
        return self

    def pop_in(self, t0: float, *, duration: float = 0.38, stagger: float = 0.035) -> CalloutBadge:
        return self.activate(t0, t0 + duration, pop=True, stagger=stagger)

    def dim(self, t0: float, t1: float) -> CalloutBadge:
        self.circle.native.fill_color.add_segment(
            t0, t1, _to_color((0.92, 0.94, 0.97, 1.0)), _to_color((0.02, 0.02, 0.03, 0.0)), _nat.EasingType.QuinticInOut
        )
        self.num_text.native.color.add_segment(
            t0, t1, _to_color((0.05, 0.05, 0.07, 1.0)), _to_color((0.68, 0.70, 0.75, 1.0)), _nat.EasingType.QuinticInOut
        )
        self.label_text.native.color.add_segment(
            t0, t1, _to_color((0.95, 0.96, 0.98, 1.0)), _to_color((0.42, 0.44, 0.49, 1.0)), _nat.EasingType.QuinticInOut
        )
        return self

    def hide(self, t0: float, t1: float) -> CalloutBadge:
        self.circle.hide(t0, t1)
        self.num_text.hide(t0, t1)
        self.label_text.hide(t0, t1)
        return self


class ViewfinderBrackets:
    """Four crisp L-corner brackets framing a centerpiece orbital or subject."""

    def __init__(self, obj_id: str, corners: list[Graph]) -> None:
        self.obj_id = obj_id
        self.corners = corners

    def reveal(self, t0: float, t1: float) -> ViewfinderBrackets:
        for g in self.corners:
            g.reveal(t0, t1)
        return self

    def pop_in(self, t0: float, *, duration: float = 0.44, offset_px: float = 18.0) -> ViewfinderBrackets:
        dirs = [(-1.0, 1.0), (1.0, 1.0), (-1.0, -1.0), (1.0, -1.0)]
        t_end = t0 + max(0.10, float(duration))
        for idx, g in enumerate(self.corners):
            sx, sy = dirs[idx % 4]
            g.reveal(t0, t_end, easing=_nat.EasingType.ExpoOut)
            g.native.presentation_offset.add_segment(
                t0,
                t_end,
                _to_vec2((sx * offset_px, sy * offset_px)),
                _to_vec2((0.0, 0.0)),
                _nat.EasingType.Spring,
                16.0,
                0.64,
            )
        return self

    def hide(self, t0: float, t1: float) -> ViewfinderBrackets:
        for g in self.corners:
            g.hide(t0, t1)
        return self


class Stickman:
    def __init__(self, obj_id: str, native_item: _nat.StickmanItem) -> None:
        self.obj_id = obj_id
        self.native = native_item

    def point_at(
        self,
        t0: float,
        t1: float,
        target_world: tuple[float, float],
        *,
        from_angles: tuple[float, float] = (-0.9, -0.4),
        lean_rad: float = 0.10,
    ) -> Stickman:
        hip = self.native.hip_pos.evaluate(t0)
        sc = self.native.scale.evaluate(t0)
        sh = (hip.x + math.sin(lean_rad) * 68.0 * sc, hip.y + math.cos(lean_rad) * 68.0 * sc)
        ang = math.atan2(target_world[1] - sh[1], target_world[0] - sh[0])
        self.native.torso_lean_rad.add_segment(t0, t1, 0.0, lean_rad, _nat.EasingType.QuinticInOut)
        self.native.arm_r_upper_rad.add_segment(
            t0, t1, from_angles[0], ang - 0.08, _nat.EasingType.Spring, 15.0, 0.62
        )
        self.native.arm_r_lower_rad.add_segment(
            t0, t1, from_angles[1], ang + 0.02, _nat.EasingType.Spring, 15.0, 0.62
        )
        return self

    def release_action(self, t_release: float) -> Stickman:
        # Anticipation -> release recoil -> settle follow-through
        self.native.arm_r_upper_rad.add_segment(
            t_release - 0.18, t_release, 0.15, 0.35, _nat.EasingType.QuinticInOut
        )
        self.native.arm_r_upper_rad.add_segment(
            t_release, t_release + 0.32, 0.35, -0.45, _nat.EasingType.Spring, 18.0, 0.55
        )
        self.native.arm_r_lower_rad.add_segment(
            t_release, t_release + 0.35, 0.25, -0.25, _nat.EasingType.Spring, 18.0, 0.55
        )
        self.native.torso_lean_rad.add_segment(
            t_release, t_release + 0.35, 0.10, -0.04, _nat.EasingType.Spring, 14.0, 0.62
        )
        return self

    def follow_through_react(self, t_contact: float) -> Stickman:
        self.native.head_tilt_rad.add_segment(
            t_contact, t_contact + 0.28, 0.0, 0.16, _nat.EasingType.Spring, 18.0, 0.52
        )
        self.native.arm_l_upper_rad.add_segment(
            t_contact, t_contact + 0.28, -2.1, -1.55, _nat.EasingType.Spring, 16.0, 0.58
        )
        return self


class SceneBuilder:
    """Ergonomic Python authoring wrapper producing a batched native `stem_engine._stem_native.Scene`."""

    def __init__(self, width: int = 1920, height: int = 1080, duration_sec: float = 10.0) -> None:
        self.native = _nat.Scene(width, height, float(duration_sec))
        self.camera = Camera(self.native.camera())
        self._registered_boxes: list[tuple[str, float, float, LayoutBox]] = []
        self._cues: list[Cue] = []
        self._formula_cache: dict[str, int] = {}

    def set_background(
        self,
        top_color: tuple[float, float, float, float] | tuple[float, float, float],
        bottom_color: tuple[float, float, float, float] | tuple[float, float, float],
        *,
        show_grid: bool = False,
        grid_spacing: float = 120.0,
        starfield: float = 0.0,
        vignette: float = 0.22,
        grain: float = 0.006,
        perspective_grid: float = 0.0,
    ) -> None:
        bg = self.native.background()
        bg.top_color.set_constant(_to_color(top_color))
        bg.bottom_color.set_constant(_to_color(bottom_color))
        bg.show_grid = show_grid
        bg.grid_spacing = float(grid_spacing)
        bg.starfield_intensity = float(starfield)
        bg.vignette_strength = float(vignette)
        bg.grain_amount = float(grain)
        bg.perspective_grid_opacity = float(perspective_grid)

    def add_trajectory(
        self,
        samples: Sequence[tuple[float, float]],
        *,
        t_start: float = 0.0,
        dt: float = 1.0 / 240.0,
    ) -> int:
        tr = _nat.Trajectory2D()
        tr.t_start = float(t_start)
        tr.dt = float(dt)
        tr.samples = [_to_vec2(p) for p in samples]
        return self.native.add_trajectory(tr)

    def add_box(
        self,
        obj_id: str,
        center: tuple[float, float],
        size: tuple[float, float],
        *,
        fill_color: tuple[float, float, float, float] | tuple[float, float, float] = (0.2, 0.5, 0.9, 1.0),
        stroke_color: tuple[float, float, float, float] = (1.0, 1.0, 1.0, 0.0),
        stroke_width: float = 0.0,
        corner_radius: float = 6.0,
        rotation_rad: float = 0.0,
        z_index: int = 0,
        screen_space: bool = False,
        trajectory_id: int = 0,
        visible: float = 1.0,
        material: str = "solid",
        elevation: float = 0.0,
        parallax: float = 1.0,
    ) -> Body:
        mat_map = {"solid": 0, "matte": 1, "book": 2, "glow": 3, "card25d": 4}
        r = _nat.RectItem()
        r.z_index = z_index
        r.screen_space = screen_space
        r.material_mode = mat_map.get(material, 0)
        r.center.set_constant(_to_vec2(center))
        r.size.set_constant(_to_vec2(size))
        r.fill_color.set_constant(_to_color(fill_color))
        r.stroke_color.set_constant(_to_color(stroke_color))
        r.stroke_width.set_constant(float(stroke_width))
        r.corner_radius.set_constant(float(corner_radius))
        r.rotation_rad.set_constant(float(rotation_rad))
        r.elevation.set_constant(float(elevation))
        r.parallax = float(parallax)
        r.trajectory_id = trajectory_id
        r.visible.set_constant(float(visible))
        stored = self.native.add_rect(r)
        return Body(obj_id, stored)

    def add_rect(
        self,
        obj_id: str,
        center: tuple[float, float],
        size: tuple[float, float],
        **kwargs: Any,
    ) -> Body:
        return self.add_box(obj_id, center, size, **kwargs)

    def add_circle(
        self,
        obj_id: str,
        center: tuple[float, float],
        radius: float,
        *,
        fill_color: tuple[float, float, float, float] | tuple[float, float, float] = (0.95, 0.55, 0.2, 1.0),
        stroke_color: tuple[float, float, float, float] = (1.0, 1.0, 1.0, 0.0),
        stroke_width: float = 0.0,
        z_index: int = 0,
        screen_space: bool = False,
        trajectory_id: int = 0,
        visible: float = 1.0,
        material: str = "solid",
        elevation: float = 0.0,
        parallax: float = 1.0,
    ) -> Body:
        mat_map = {"solid": 0, "sphere3d": 1, "glow": 2, "disc25d": 3}
        c = _nat.CircleItem()
        c.z_index = z_index
        c.screen_space = screen_space
        c.material_mode = mat_map.get(material, 0)
        c.center.set_constant(_to_vec2(center))
        c.radius.set_constant(float(radius))
        c.fill_color.set_constant(_to_color(fill_color))
        c.stroke_color.set_constant(_to_color(stroke_color))
        c.stroke_width.set_constant(float(stroke_width))
        c.elevation.set_constant(float(elevation))
        c.parallax = float(parallax)
        c.trajectory_id = trajectory_id
        c.visible.set_constant(float(visible))
        stored = self.native.add_circle(c)
        return Body(obj_id, stored)

    def add_vector(
        self,
        obj_id: str,
        origin: tuple[float, float],
        delta: tuple[float, float],
        *,
        color: tuple[float, float, float, float] | tuple[float, float, float] = (0.3, 0.9, 0.6, 1.0),
        shaft_width: float = 5.0,
        head_length: float = 20.0,
        head_width: float = 18.0,
        z_index: int = 12,
        screen_space: bool = False,
        attach_trajectory_id: int = 0,
        attach_offset: tuple[float, float] = (0.0, 0.0),
        visible: float = 1.0,
        reveal: float = 1.0,
        parallax: float = 1.0,
    ) -> Vector:
        v = _nat.VectorItem()
        v.z_index = z_index
        v.screen_space = screen_space
        v.origin.set_constant(_to_vec2(origin))
        v.delta.set_constant(_to_vec2(delta))
        v.color.set_constant(_to_color(color))
        v.shaft_width.set_constant(float(shaft_width))
        v.head_length.set_constant(float(head_length))
        v.head_width.set_constant(float(head_width))
        v.parallax = float(parallax)
        v.attach_trajectory_id = attach_trajectory_id
        v.attach_offset = _to_vec2(attach_offset)
        v.visible.set_constant(float(visible))
        v.reveal.set_constant(float(reveal))
        stored = self.native.add_vector(v)
        return Vector(obj_id, stored)

    def add_graph(
        self,
        obj_id: str,
        points: Sequence[tuple[float, float]],
        *,
        origin: tuple[float, float] = (0.0, 0.0),
        color: tuple[float, float, float, float] | tuple[float, float, float] = (0.35, 0.78, 1.0, 1.0),
        stroke_width: float = 4.0,
        z_index: int = 6,
        screen_space: bool = False,
        visible: float = 1.0,
        reveal: float = 1.0,
        dashed: tuple[float, float] | None = None,
        envelope_sigma: float = 0.0,
        fill_under: bool = False,
        fill_baseline_y: float = 0.0,
        fill_top_color: tuple[float, float, float, float] = (0.24, 0.26, 0.30, 0.72),
        fill_bottom_color: tuple[float, float, float, float] = (0.03, 0.03, 0.04, 0.04),
        trail_trajectory_id: int = 0,
        trail_duration_sec: float = 1.5,
        parallax: float = 1.0,
    ) -> Graph:
        g = _nat.GraphItem()
        g.z_index = z_index
        g.screen_space = screen_space
        g.origin.set_constant(_to_vec2(origin))
        g.parallax = float(parallax)
        g.points = [_to_vec2(p) for p in points]
        g.color.set_constant(_to_color(color))
        g.stroke_width.set_constant(float(stroke_width))
        if dashed is not None:
            g.dash_length = float(dashed[0])
            g.gap_length = float(dashed[1])
        g.deform_envelope_sigma = float(envelope_sigma)
        g.fill_under = bool(fill_under)
        g.fill_baseline_y = float(fill_baseline_y)
        g.fill_top_color.set_constant(_to_color(fill_top_color))
        g.fill_bottom_color.set_constant(_to_color(fill_bottom_color))
        g.visible.set_constant(float(visible))
        g.reveal.set_constant(float(reveal))
        g.trail_trajectory_id = trail_trajectory_id
        g.trail_duration_sec = float(trail_duration_sec)
        stored = self.native.add_graph(g)
        return Graph(obj_id, stored)

    def add_orbital_cloud(
        self,
        obj_id: str,
        *,
        center: tuple[float, float] = (0.0, 0.0),
        n_a: int = 1,
        l_a: int = 0,
        m_a: int = 0,
        bohr_a: float = 34.0,
        n_b: int = 2,
        l_b: int = 1,
        m_b: int = 0,
        bohr_b: float = 22.0,
        particle_count: int = 24000,
        dot_radius_px: float = 2.05,
        yaw_rad: float = 0.0,
        pitch_rad: float = 0.28,
        fov_persp: float = 0.0011,
        flow_speed: float = 1.0,
        collapse_target: tuple[float, float] = (48.0, -18.0),
        color: tuple[float, float, float, float] | tuple[float, float, float] = (0.86, 0.88, 0.91, 1.0),
        screen_space: bool = False,
        z_index: int = 10,
        visible: float = 1.0,
        reveal: float = 1.0,
        morph: float = 0.0,
        seed: int = 108,
    ) -> OrbitalCloud:
        oc = _nat.OrbitalCloud3DItem()
        oc.z_index = z_index
        oc.screen_space = screen_space
        oc.seed = int(seed)
        oc.particle_count = int(particle_count)
        oc.state_a = _nat.OrbitalSpec(n_a, l_a, m_a, float(bohr_a))
        oc.state_b = _nat.OrbitalSpec(n_b, l_b, m_b, float(bohr_b))
        oc.morph.set_constant(float(morph))
        oc.visible.set_constant(float(visible))
        oc.reveal.set_constant(float(reveal))
        oc.opacity.set_constant(1.0)
        oc.center.set_constant(_to_vec2(center))
        oc.scale.set_constant(1.0)
        oc.yaw_rad.set_constant(float(yaw_rad))
        oc.pitch_rad.set_constant(float(pitch_rad))
        oc.fov_persp.set_constant(float(fov_persp))
        oc.flow_speed.set_constant(float(flow_speed))
        oc.collapse.set_constant(0.0)
        oc.collapse_target = _to_vec2(collapse_target)
        oc.dot_radius_px = float(dot_radius_px)
        oc.color.set_constant(_to_color(color))
        stored = self.native.add_orbital_cloud(oc)
        return OrbitalCloud(obj_id, stored)

    def add_stickman(
        self,
        obj_id: str,
        hip_pos: tuple[float, float],
        *,
        scale: float = 1.0,
        color: tuple[float, float, float, float] = (0.196, 0.459, 0.651, 1.0),
        stroke_width: float = 6.5,
        z_index: int = 15,
        visible: float = 1.0,
        solid_head: bool = True,
        depth_shade_back_limbs: bool = True,
    ) -> Stickman:
        sm = _nat.StickmanItem()
        sm.z_index = z_index
        sm.solid_head = solid_head
        sm.depth_shade_back_limbs = depth_shade_back_limbs
        sm.hip_pos.set_constant(_to_vec2(hip_pos))
        sm.scale.set_constant(float(scale))
        sm.color.set_constant(_to_color(color))
        sm.stroke_width.set_constant(float(stroke_width))
        sm.visible.set_constant(float(visible))
        stored = self.native.add_stickman(sm)
        return Stickman(obj_id, stored)

    def add_text(
        self,
        obj_id: str,
        text: str,
        position: tuple[float, float],
        *,
        font_size_px: float = 32.0,
        font_path: str = "",
        font_variant: str = "regular",
        align_h: int = 0,
        color: tuple[float, float, float, float] | tuple[float, float, float] = (0.96, 0.97, 0.99, 1.0),
        z_index: int = 30,
        screen_space: bool = True,
        trajectory_id: int = 0,
        visible: float = 1.0,
        parallax: float = 1.0,
        active_window: tuple[float, float] | None = None,
    ) -> Text:
        resolved_font = font_path if font_path else resolve_font(font_variant)
        ti = _nat.TextItem()
        ti.z_index = z_index
        ti.screen_space = screen_space
        ti.text = text
        ti.font_path = resolved_font
        ti.font_size_px = float(font_size_px)
        ti.align_h = int(align_h)
        ti.position.set_constant(_to_vec2(position))
        ti.color.set_constant(_to_color(color))
        ti.parallax = float(parallax)
        ti.trajectory_id = trajectory_id
        ti.visible.set_constant(float(visible))
        stored = self.native.add_text(ti)
        bounds = _nat.measure_text_bounds(text, resolved_font, float(font_size_px))
        obj = Text(obj_id, stored, bounds)
        if active_window is not None:
            self._registered_boxes.append((obj_id, active_window[0], active_window[1], obj.layout_box(active_window[0])))
        return obj

    def add_vector_label(
        self,
        obj_id: str,
        symbol: str,
        position: tuple[float, float],
        *,
        subscript: str = "",
        subtitle: str = "",
        symbol_size_px: float = 32.0,
        symbol_color: tuple[float, float, float, float] | tuple[float, float, float] = (0.95, 0.96, 0.98, 1.0),
        subtitle_color: tuple[float, float, float, float] | tuple[float, float, float] = (0.62, 0.65, 0.70, 1.0),
        screen_space: bool = False,
        trajectory_id: int = 0,
        z_index: int = 24,
        visible: float = 1.0,
        active_window: tuple[float, float] | None = None,
    ) -> VectorLabelGroup:
        """Creates a Median-style vector label: bold-italic symbol + over-arrow + optional subscript + subtitle."""
        x0, y0 = float(position[0]), float(position[1])
        texts: list[Text] = []
        sym_txt = self.add_text(
            f"{obj_id}_sym",
            symbol,
            (x0, y0),
            font_size_px=symbol_size_px,
            font_variant="bold_italic",
            align_h=-1,
            color=symbol_color,
            z_index=z_index,
            screen_space=screen_space,
            trajectory_id=trajectory_id,
            visible=visible,
        )
        texts.append(sym_txt)
        sym_w = max(12.0, sym_txt.bounds.width)
        arrow_y = y0 + symbol_size_px * 0.48
        arrow_pad = symbol_size_px * 0.08
        arrow = self.add_vector(
            f"{obj_id}_arrow",
            origin=(x0 + arrow_pad, arrow_y),
            delta=(sym_w + symbol_size_px * 0.14, 0.0),
            color=symbol_color,
            shaft_width=max(1.8, symbol_size_px * 0.064),
            head_length=symbol_size_px * 0.25,
            head_width=symbol_size_px * 0.19,
            z_index=z_index,
            screen_space=screen_space,
            attach_trajectory_id=trajectory_id,
            visible=visible,
            reveal=1.0,
        )

        cursor_x = x0 + sym_w + symbol_size_px * 0.10
        if subscript:
            sub_txt = self.add_text(
                f"{obj_id}_sub",
                subscript,
                (cursor_x, y0 - symbol_size_px * 0.24),
                font_size_px=symbol_size_px * 0.56,
                font_variant="semibold_italic",
                align_h=-1,
                color=symbol_color,
                z_index=z_index,
                screen_space=screen_space,
                trajectory_id=trajectory_id,
                visible=visible,
            )
            texts.append(sub_txt)
            cursor_x += sub_txt.bounds.width + symbol_size_px * 0.16
        else:
            cursor_x += symbol_size_px * 0.26

        if subtitle:
            lbl_txt = self.add_text(
                f"{obj_id}_lbl",
                subtitle,
                (cursor_x, y0 - symbol_size_px * 0.03),
                font_size_px=symbol_size_px * 0.72,
                font_variant="regular",
                align_h=-1,
                color=subtitle_color,
                z_index=z_index,
                screen_space=screen_space,
                trajectory_id=trajectory_id,
                visible=visible,
            )
            texts.append(lbl_txt)
            cursor_x += lbl_txt.bounds.width

        total_w = max(20.0, cursor_x - x0)
        if active_window is not None:
            self._registered_boxes.append(
                (
                    obj_id,
                    active_window[0],
                    active_window[1],
                    LayoutBox(
                        obj_id=obj_id,
                        cx=x0 + total_w * 0.5,
                        cy=y0,
                        width=total_w,
                        height=symbol_size_px * 1.25,
                        screen_space=screen_space,
                    ),
                )
            )
        return VectorLabelGroup(obj_id, texts, arrow)

    def add_formula(
        self,
        obj_id: str,
        tex: str,
        position: tuple[float, float],
        *,
        target_height_px: float = 46.0,
        color: tuple[float, float, float, float] | tuple[float, float, float] = (0.96, 0.97, 0.99, 1.0),
        z_index: int = 32,
        screen_space: bool = True,
        trajectory_id: int = 0,
        visible: float = 1.0,
        parallax: float = 1.0,
        macros: dict[str, str] | None = None,
        active_window: tuple[float, float] | None = None,
    ) -> Formula:
        key, svg_xml = compile_tex_to_svg(tex, macros=macros)
        cache_tag = f"{key}:{target_height_px:.1f}"
        asset = _nat.parse_svg_to_path_asset(svg_xml, cache_tag, float(target_height_px), _to_color((1.0, 1.0, 1.0, 1.0)))
        if cache_tag in self._formula_cache:
            asset.asset_id = self._formula_cache[cache_tag]
        else:
            asset.asset_id = self.native.register_path_asset(asset)
            self._formula_cache[cache_tag] = asset.asset_id

        pi = _nat.PathInstanceItem()
        pi.asset_id = asset.asset_id
        pi.z_index = z_index
        pi.screen_space = screen_space
        pi.trajectory_id = trajectory_id
        pi.parallax = float(parallax)
        pi.position.set_constant(_to_vec2(position))
        pi.tint.set_constant(_to_color(color))
        pi.visible.set_constant(float(visible))
        stored = self.native.add_path_instance(pi)
        obj = Formula(obj_id, stored, asset)
        if active_window is not None:
            self._registered_boxes.append((obj_id, active_window[0], active_window[1], obj.layout_box(active_window[0])))
        return obj

    def add_formula_card(
        self,
        obj_id: str,
        tex: str,
        position: tuple[float, float],
        *,
        target_height_px: float = 46.0,
        color: tuple[float, float, float, float] | tuple[float, float, float] = (0.96, 0.97, 0.99, 1.0),
        fill_color: tuple[float, float, float, float] = (0.035, 0.038, 0.048, 0.94),
        stroke_color: tuple[float, float, float, float] = (0.82, 0.84, 0.89, 0.88),
        stroke_width: float = 1.6,
        corner_radius: float = 8.0,
        pad_x: float = 28.0,
        pad_y: float = 18.0,
        z_index: int = 32,
        screen_space: bool = True,
        visible: float = 1.0,
        material: str = "card25d",
        elevation: float = 14.0,
        parallax: float = 1.0,
        macros: dict[str, str] | None = None,
        active_window: tuple[float, float] | None = None,
    ) -> FormulaCard:
        formula = self.add_formula(
            obj_id,
            tex,
            position,
            target_height_px=target_height_px,
            color=color,
            z_index=z_index,
            screen_space=screen_space,
            visible=visible,
            parallax=parallax,
            macros=macros,
            active_window=None,
        )
        box = formula.layout_box(0.0)
        card_w = box.width + pad_x * 2.0
        card_h = box.height + pad_y * 2.0
        card = self.add_box(
            f"{obj_id}_card",
            position,
            (card_w, card_h),
            fill_color=fill_color,
            stroke_color=stroke_color,
            stroke_width=stroke_width,
            corner_radius=corner_radius,
            z_index=z_index - 1,
            screen_space=screen_space,
            visible=visible,
            material=material,
            elevation=elevation,
            parallax=parallax,
        )
        if active_window is not None:
            self._registered_boxes.append(
                (
                    obj_id,
                    active_window[0],
                    active_window[1],
                    LayoutBox(
                        obj_id=obj_id,
                        cx=position[0],
                        cy=position[1],
                        width=card_w,
                        height=card_h,
                        screen_space=screen_space,
                    ),
                )
            )
        return FormulaCard(obj_id, formula, card)

    def add_numbered_callout(
        self,
        obj_id: str,
        number_str: str,
        text: str,
        position: tuple[float, float],
        *,
        radius: float = 19.0,
        font_size_px: float = 25.0,
        z_index: int = 32,
        visible: float = 0.0,
        active_window: tuple[float, float] | None = None,
    ) -> CalloutBadge:
        cx, cy = float(position[0]), float(position[1])
        circle = self.add_circle(
            f"{obj_id}_circle",
            center=(cx, cy),
            radius=radius,
            fill_color=(0.02, 0.02, 0.03, 0.0),
            stroke_color=(0.76, 0.78, 0.84, 0.90),
            stroke_width=1.8,
            screen_space=True,
            z_index=z_index,
            visible=visible,
            material="disc25d",
            elevation=8.0,
        )
        num_txt = self.add_text(
            f"{obj_id}_num",
            number_str,
            (cx, cy),
            font_size_px=font_size_px * 0.84,
            font_variant="semibold",
            align_h=0,
            color=(0.82, 0.84, 0.88, 1.0),
            z_index=z_index + 1,
            screen_space=True,
            visible=visible,
        )
        lbl_txt = self.add_text(
            f"{obj_id}_lbl",
            text,
            (cx + radius + 22.0, cy),
            font_size_px=font_size_px,
            font_variant="regular",
            align_h=-1,
            color=(0.72, 0.74, 0.78, 1.0),
            z_index=z_index + 1,
            screen_space=True,
            visible=visible,
        )
        if active_window is not None:
            total_w = radius * 2.0 + 22.0 + lbl_txt.bounds.width
            self._registered_boxes.append(
                (
                    obj_id,
                    active_window[0],
                    active_window[1],
                    LayoutBox(
                        obj_id=obj_id,
                        cx=cx - radius + total_w * 0.5,
                        cy=cy,
                        width=total_w,
                        height=max(radius * 2.0, lbl_txt.bounds.height),
                        screen_space=True,
                    ),
                )
            )
        return CalloutBadge(obj_id, circle, num_txt, lbl_txt)

    def add_viewfinder_brackets(
        self,
        obj_id: str,
        center: tuple[float, float],
        size: tuple[float, float],
        *,
        arm_len: float = 30.0,
        stroke_width: float = 2.6,
        color: tuple[float, float, float, float] = (0.88, 0.90, 0.94, 0.88),
        screen_space: bool = True,
        z_index: int = 32,
        visible: float = 0.0,
    ) -> ViewfinderBrackets:
        cx, cy = float(center[0]), float(center[1])
        hw, hh = float(size[0]) * 0.5, float(size[1]) * 0.5
        defs = [
            ((cx - hw + arm_len, cy + hh), (cx - hw, cy + hh), (cx - hw, cy + hh - arm_len)),
            ((cx + hw - arm_len, cy + hh), (cx + hw, cy + hh), (cx + hw, cy + hh - arm_len)),
            ((cx - hw + arm_len, cy - hh), (cx - hw, cy - hh), (cx - hw, cy - hh + arm_len)),
            ((cx + hw - arm_len, cy - hh), (cx + hw, cy - hh), (cx + hw, cy - hh + arm_len)),
        ]
        corners = [
            self.add_graph(
                f"{obj_id}_c{idx}",
                pts,
                color=color,
                stroke_width=stroke_width,
                screen_space=screen_space,
                z_index=z_index,
                visible=visible,
                reveal=1.0,
            )
            for idx, pts in enumerate(defs)
        ]
        return ViewfinderBrackets(obj_id, corners)

    def add_cue(self, cue_id: str, time_sec: float, description: str = "") -> Cue:
        c = _nat.Cue()
        c.cue_id = cue_id
        c.time_sec = float(time_sec)
        c.description = description
        self.native.add_cue(c)
        cue = Cue(cue_id, time_sec, description)
        self._cues.append(cue)
        return cue

    def add_audio_event(self, ev: AudioCue) -> None:
        self.native.add_audio_event(ev.native)

    def check_overlaps(self, safe_area: SafeArea | None = None) -> list[OverlapWarning]:
        """Detects overlapping text/formula boxes within active time windows and safe-area violations."""
        warnings: list[OverlapWarning] = []
        sa = safe_area or SafeArea(self.native.width, self.native.height)
        for obj_id, _, _, box in self._registered_boxes:
            if box.screen_space and not sa.contains(box):
                warnings.append(
                    OverlapWarning(
                        a_id=obj_id,
                        b_id="safe_area",
                        overlap_area=0.0,
                        message=f"'{obj_id}' exceeds SafeArea bounds",
                    )
                )
        n = len(self._registered_boxes)
        for i in range(n):
            id_a, a0, a1, ba = self._registered_boxes[i]
            for j in range(i + 1, n):
                id_b, b0, b1, bb = self._registered_boxes[j]
                if ba.screen_space != bb.screen_space:
                    continue
                if min(a1, b1) <= max(a0, b0):
                    continue
                ox = max(0.0, min(ba.right, bb.right) - max(ba.left, bb.left))
                oy = max(0.0, min(ba.top, bb.top) - max(ba.bottom, bb.bottom))
                area = ox * oy
                if area > 1.0:
                    warnings.append(
                        OverlapWarning(
                            a_id=id_a,
                            b_id=id_b,
                            overlap_area=area,
                            message=f"Overlap ({area:.0f} px^2) between '{id_a}' and '{id_b}'",
                        )
                    )
        return warnings

    def build(self) -> _nat.Scene:
        return self.native
