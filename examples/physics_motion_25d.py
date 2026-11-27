"""STEM Showcase: Presentation Motion Physics & Layered 2.5D Design (16s @ 1080p60).

Demonstrates DeLib4Anim's physics-driven animation language and orthographic 2.5D aesthetic:
- Shot 1 (0.0-5.2s): Harmonious Entrance (`pop_in` + micro-stagger + 2.5D elevation) vs flat alpha-only reveal.
- Shot 2 (5.2-10.8s): Stopping Inertia & Contact Follow-Through (forward damped impulse, COM pitch,
  area-preserving squash-stretch, secondary badge follow-through) vs abrupt rigid stop + impulse graph.
- Shot 3 (10.8-16.0s): Layered 2.5D Multi-Plane Orthographic Parallax (`0.42 / 0.72 / 1.00 / 1.14`)
  with `Camera.move_with_settle`.
"""

from __future__ import annotations

from stem_engine import POP_HERO, POP_SNAPPY, POP_SOFT, AudioCue, SceneBuilder
from stem_engine import _stem_native as _nat


def _eval_impulse(u: float, omega: float = 14.0, zeta: float = 0.56) -> float:
    return float(_nat.eval_easing(_nat.EasingType.Impulse, u, omega, zeta))


def build_scene(
    width: int = 1920,
    height: int = 1080,
    voice_wav_path: str = "",
) -> SceneBuilder:
    duration_sec = 16.0

    c_white = (0.945, 0.953, 0.972, 1.0)
    c_muted = (0.600, 0.628, 0.675, 1.0)
    c_emerald = (0.349, 0.788, 0.459, 1.0)
    c_coral = (0.922, 0.369, 0.333, 1.0)
    c_gold = (0.949, 0.788, 0.298, 1.0)
    c_cyan = (0.320, 0.760, 0.960, 1.0)

    sb = SceneBuilder(width=width, height=height, duration_sec=duration_sec)
    sb.set_background(
        top_color=(0.015, 0.017, 0.023, 1.0),
        bottom_color=(0.008, 0.010, 0.014, 1.0),
        show_grid=False,
        starfield=1.15,
        vignette=0.22,
        grain=0.004,
    )
    sb.camera.set_pose(pos=(0.0, 0.0), zoom=1.0)

    # ------------------------------------------------------------------
    # Shot 1 (0.0-5.2s): Flat Alpha Reveal (Left) vs Harmonious Pop + 2.5D (Right)
    # ------------------------------------------------------------------
    s1_title = sb.add_text(
        "s1_title",
        "1. Xuất hiện hài hòa: Pop + Stagger + Độ cao 2.5D",
        (0.0, 445.0),
        font_size_px=34.0,
        font_variant="semibold",
        color=c_white,
        z_index=34,
        visible=0.0,
        active_window=(0.1, 5.15),
    )
    s1_title.pop_in(0.15, style=POP_SOFT)
    s1_title.pop_out(4.85)

    s1_div = sb.add_graph(
        "s1_divider",
        [(0.0, -340.0), (0.0, 360.0)],
        color=(0.38, 0.41, 0.46, 0.45),
        stroke_width=1.6,
        dashed=(8.0, 6.0),
        screen_space=True,
        z_index=20,
        visible=0.0,
    )
    s1_div.reveal(0.25, 0.75).hide(4.85, 5.20)

    # Left: Flat alpha-only reveal (no scale pop, no float, no 2.5D shadow)
    lbl_flat = sb.add_text(
        "lbl_flat",
        "Fade phẳng (không lực đàn hồi)",
        (-450.0, 325.0),
        font_size_px=24.0,
        color=c_muted,
        z_index=32,
        visible=0.0,
        active_window=(0.4, 5.15),
    )
    lbl_flat.reveal(0.45, 0.85).hide(4.85, 5.20)

    flat_card = sb.add_formula_card(
        "flat_card",
        r"F = -k x - c \dot{x}",
        (-450.0, 145.0),
        target_height_px=48.0,
        material="solid",
        elevation=0.0,
        visible=0.0,
        active_window=(0.8, 5.15),
    )
    flat_card.reveal(0.85, 1.25).hide(4.85, 5.20)

    flat_dot = sb.add_circle(
        "flat_dot",
        center=(-450.0, -65.0),
        radius=34.0,
        fill_color=c_cyan,
        material="solid",
        elevation=0.0,
        screen_space=True,
        z_index=28,
        visible=0.0,
    )
    flat_dot.reveal(1.55, 1.95).hide(4.85, 5.20)

    # Right: Harmonious Pop-In + Stagger + Layered 2.5D Elevation
    lbl_pop = sb.add_text(
        "lbl_pop",
        "Pop-in đàn hồi + Độ cao 2.5D + Trễ nhịp 40ms",
        (450.0, 325.0),
        font_size_px=24.0,
        font_variant="semibold",
        color=c_emerald,
        z_index=32,
        visible=0.0,
        active_window=(0.4, 5.15),
    )
    lbl_pop.pop_in(0.45, style=POP_SOFT)
    lbl_pop.pop_out(4.85)

    pop_card = sb.add_formula_card(
        "pop_card",
        r"\textcolor{#59C975}{F} = -k\textcolor{#F2C94C}{x} - c\dot{x}",
        (450.0, 145.0),
        target_height_px=48.0,
        material="card25d",
        elevation=18.0,
        visible=0.0,
        active_window=(0.8, 5.15),
    )
    pop_card.pop_in(0.85, style=POP_HERO, stagger=0.04)
    pop_card.pop_out(4.85)

    pop_disc = sb.add_circle(
        "pop_disc",
        center=(450.0, -65.0),
        radius=34.0,
        fill_color=c_emerald,
        stroke_color=(0.94, 0.97, 0.96, 0.55),
        stroke_width=1.6,
        material="disc25d",
        elevation=14.0,
        screen_space=True,
        z_index=28,
        visible=0.0,
    )
    pop_disc.pop_in(1.55, style=POP_SNAPPY)
    pop_disc.pop_out(4.85)

    co_pop1 = sb.add_numbered_callout(
        "co_pop1", "1", "scale 0.84 -> 1.04 -> 1.0", (220.0, -210.0), active_window=(2.1, 5.15)
    )
    co_pop2 = sb.add_numbered_callout(
        "co_pop2", "2", "trượt nổi +14px + bóng SDF 2.5D", (220.0, -285.0), active_window=(2.6, 5.15)
    )
    co_pop1.pop_in(2.15).dim(2.65, 2.95).hide(4.85, 5.20)
    co_pop2.pop_in(2.65).hide(4.85, 5.20)

    # ------------------------------------------------------------------
    # Shot 2 (5.2-10.8s): Rigid Stop (Top) vs Inertia Stop + Follow-Through (Bottom)
    # ------------------------------------------------------------------
    s2_title = sb.add_text(
        "s2_title",
        "2. Quán tính dừng & Độ nảy trọng tâm khi hãm",
        (0.0, 445.0),
        font_size_px=34.0,
        font_variant="semibold",
        color=c_white,
        z_index=34,
        visible=0.0,
        active_window=(5.2, 10.75),
    )
    s2_title.pop_in(5.25, style=POP_SOFT)
    s2_title.pop_out(10.45)

    t_slide0, t_slide1 = 6.00, 7.25
    x_start, x_stop = -520.0, 320.0
    y_top, y_bot = 215.0, -15.0

    # Reference rails & wall stoppers
    for tag, ry in (("top", y_top), ("bot", y_bot)):
        rail = sb.add_graph(
            f"rail_{tag}",
            [(x_start - 70.0, ry - 32.0), (x_stop + 75.0, ry - 32.0)],
            color=(0.45, 0.48, 0.54, 0.55),
            stroke_width=2.0,
            z_index=5,
            visible=0.0,
        )
        rail.reveal(5.35, 5.75).hide(10.45, 10.80)

        stopper = sb.add_box(
            f"stopper_{tag}",
            center=(x_stop + 56.0, ry),
            size=(14.0, 74.0),
            fill_color=(0.30, 0.32, 0.38, 1.0),
            stroke_color=(0.56, 0.59, 0.65, 0.65),
            stroke_width=1.2,
            corner_radius=3.0,
            material="card25d",
            elevation=6.0,
            z_index=9,
            visible=0.0,
        )
        stopper.pop_in(5.40, style=POP_SNAPPY)
        stopper.pop_out(10.45)

    # Top block: Rigid stop (abrupt clamp at t_slide1)
    lbl_rigid = sb.add_text(
        "lbl_rigid",
        "Dừng cứng (về 0 tức thời)",
        (-460.0, y_top + 78.0),
        font_size_px=23.0,
        align_h=-1,
        color=c_coral,
        z_index=30,
        visible=0.0,
        active_window=(5.4, 10.75),
    )
    lbl_rigid.pop_in(5.45, style=POP_SOFT)
    lbl_rigid.pop_out(10.45)

    rigid_blk = sb.add_box(
        "rigid_blk",
        center=(x_start, y_top),
        size=(96.0, 52.0),
        fill_color=c_coral,
        stroke_color=(1.0, 0.78, 0.75, 0.65),
        stroke_width=1.2,
        corner_radius=7.0,
        material="card25d",
        elevation=10.0,
        z_index=12,
        visible=0.0,
    )
    rigid_blk.pop_in(5.45, style=POP_SNAPPY)
    rigid_blk.move_to(t_slide0, t_slide1, (x_start, y_top), (x_stop, y_top), easing=_nat.EasingType.Linear)
    rigid_blk.pop_out(10.45)

    # Bottom block: Physics-informed Inertia Stop (translational overshoot + COM pitch + squash + badge lag)
    lbl_inertia = sb.add_text(
        "lbl_inertia",
        "Quán tính dừng (Impulse + nghiêng trọng tâm + nén bảo toàn diện tích)",
        (-460.0, y_bot + 78.0),
        font_size_px=23.0,
        font_variant="semibold",
        align_h=-1,
        color=c_emerald,
        z_index=30,
        visible=0.0,
        active_window=(5.4, 10.75),
    )
    lbl_inertia.pop_in(5.50, style=POP_SOFT)
    lbl_inertia.pop_out(10.45)

    inertia_blk = sb.add_box(
        "inertia_blk",
        center=(x_start, y_bot),
        size=(96.0, 52.0),
        fill_color=c_emerald,
        stroke_color=(0.82, 0.98, 0.86, 0.70),
        stroke_width=1.2,
        corner_radius=7.0,
        material="card25d",
        elevation=12.0,
        z_index=12,
        visible=0.0,
    )
    inertia_blk.pop_in(5.50, style=POP_SNAPPY)
    inertia_blk.move_to(t_slide0, t_slide1, (x_start, y_bot), (x_stop, y_bot), easing=_nat.EasingType.Linear)
    inertia_blk.inertia_stop(
        t_slide1,
        direction=(1.0, 0.0),
        distance_px=10.0,
        squash=0.10,
        tilt_rad=-0.048,
        duration=0.48,
    )
    inertia_blk.pop_out(10.45)

    # Secondary overlapping tag following inertia_blk with 35ms lag
    inertia_tag = sb.add_text(
        "inertia_tag",
        "m",
        (x_start, y_bot + 46.0),
        font_size_px=22.0,
        font_variant="bold_italic",
        color=c_gold,
        screen_space=False,
        z_index=24,
        visible=0.0,
    )
    inertia_tag.pop_in(5.55, style=POP_SNAPPY)
    inertia_tag.native.position.add_segment(
        t_slide0, t_slide1, _nat.Vec2(x_start, y_bot + 46.0), _nat.Vec2(x_stop, y_bot + 46.0), _nat.EasingType.Linear
    )
    inertia_tag.inertia_stop(t_slide1 + 0.035, direction=(1.0, 0.0), distance_px=13.0, duration=0.50)
    inertia_tag.pop_out(10.45)

    # Magnified Impulse Waveform Plot below bottom rail
    plot_ox, plot_oy = -460.0, -325.0
    plot_w, plot_h = 920.0, 145.0
    sb.add_vector(
        "imp_ax_x",
        origin=(plot_ox, plot_oy),
        delta=(plot_w + 25.0, 0.0),
        color=(0.66, 0.69, 0.75, 0.85),
        shaft_width=2.0,
        head_length=12.0,
        head_width=8.5,
        screen_space=True,
        z_index=22,
        visible=0.0,
    ).reveal(6.2, 6.6).hide(10.45, 10.80)
    sb.add_vector(
        "imp_ax_y",
        origin=(plot_ox, plot_oy - 45.0),
        delta=(0.0, plot_h + 65.0),
        color=(0.66, 0.69, 0.75, 0.85),
        shaft_width=2.0,
        head_length=12.0,
        head_width=8.5,
        screen_space=True,
        z_index=22,
        visible=0.0,
    ).reveal(6.2, 6.6).hide(10.45, 10.80)

    imp_pts = [
        (plot_ox + (k / 120.0) * plot_w, plot_oy + _eval_impulse(k / 120.0, 14.0, 0.56) * plot_h)
        for k in range(121)
    ]
    imp_curve = sb.add_graph(
        "imp_curve",
        imp_pts,
        color=c_gold,
        stroke_width=3.2,
        screen_space=True,
        z_index=25,
        visible=0.0,
        reveal=0.0,
    )
    imp_curve.reveal(t_slide1, t_slide1 + 0.75).hide(10.45, 10.80)

    imp_lbl = sb.add_text(
        "imp_lbl",
        "Đáp ứng xung tắt dần: x_pres(t)  (w(0)=0, w(1)=0)",
        (plot_ox + 26.0, plot_oy + plot_h + 22.0),
        font_size_px=21.0,
        align_h=-1,
        color=c_gold,
        z_index=28,
        visible=0.0,
        active_window=(6.5, 10.75),
    )
    imp_lbl.pop_in(6.6, style=POP_SOFT)
    imp_lbl.pop_out(10.45)

    # ------------------------------------------------------------------
    # Shot 3 (10.8-16.0s): Layered 2.5D Multi-Plane Orthographic Parallax
    # ------------------------------------------------------------------
    s3_title = sb.add_text(
        "s3_title",
        "3. Trọng tâm 2D / 2.5D đa lớp (Orthographic Parallax)",
        (0.0, 445.0),
        font_size_px=34.0,
        font_variant="semibold",
        color=c_white,
        z_index=34,
        visible=0.0,
        active_window=(10.85, 16.0),
    )
    s3_title.pop_in(10.90, style=POP_SOFT)

    # Background parallax grid ticks (parallax=0.42)
    for gx in range(-900, 901, 225):
        tick = sb.add_graph(
            f"bg_tick_{gx}",
            [(float(gx), -380.0), (float(gx), 380.0)],
            color=(0.24, 0.27, 0.34, 0.42),
            stroke_width=1.4,
            dashed=(6.0, 12.0),
            parallax=0.42,
            z_index=3,
            visible=0.0,
        )
        tick.reveal(10.90, 11.35)

    # Midground 2.5D backdrop layer (parallax=0.72)
    mid_panel = sb.add_box(
        "mid_panel",
        center=(-160.0, -10.0),
        size=(520.0, 320.0),
        fill_color=(0.075, 0.088, 0.118, 0.94),
        stroke_color=(0.35, 0.42, 0.56, 0.55),
        stroke_width=1.5,
        corner_radius=14.0,
        material="card25d",
        elevation=8.0,
        parallax=0.72,
        z_index=8,
        visible=0.0,
    )
    mid_panel.pop_in(10.95, style=POP_HERO)

    # Primary plane 2.5D focal card & discs (parallax=1.00)
    fg_card = sb.add_formula_card(
        "fg_25d_card",
        r"\vec{p}_{\text{2.5D}} = \vec{p}_{\text{world}} + (1 - \lambda)\,\vec{c}_{\text{cam}}",
        (40.0, 30.0),
        target_height_px=48.0,
        screen_space=False,
        material="card25d",
        elevation=18.0,
        parallax=1.0,
        z_index=18,
        visible=0.0,
    )
    fg_card.pop_in(11.20, style=POP_HERO, stagger=0.04)

    for idx, (cx, cy, col, rad) in enumerate(
        (
            (-210.0, -145.0, c_emerald, 28.0),
            (0.0, -145.0, c_cyan, 28.0),
            (210.0, -145.0, c_gold, 28.0),
        )
    ):
        d = sb.add_circle(
            f"layer_disc_{idx}",
            center=(cx, cy),
            radius=rad,
            fill_color=col,
            stroke_color=(0.95, 0.97, 1.0, 0.55),
            stroke_width=1.4,
            material="disc25d",
            elevation=12.0 + idx * 3.0,
            parallax=1.0 + idx * 0.06,
            z_index=20 + idx,
            visible=0.0,
        )
        d.pop_in(11.45 + idx * 0.08, style=POP_SNAPPY)

    # Subtle foreground framing accent (parallax=1.14)
    fg_note = sb.add_text(
        "fg_note",
        "2D / 2.5D mặc định  ·  Chỉ dùng 3D khi không gian 3D là bất khả giản (orbital, v x B)",
        (0.0, -395.0),
        font_size_px=24.0,
        color=c_muted,
        z_index=34,
        visible=0.0,
        active_window=(11.6, 16.0),
    )
    fg_note.pop_in(11.7, style=POP_SOFT)

    # Smooth orthographic camera sweep across Shot 3 layers with settle cushion
    sb.camera.move_with_settle(11.80, 13.70, (0.0, 0.0), (140.0, 16.0), zoom0=1.0, zoom1=1.06)
    sb.camera.move_with_settle(13.85, 15.50, (140.0, 16.0), (-90.0, -10.0), zoom0=1.06, zoom1=1.0)

    # Cues & subtle SFX
    sb.add_cue("cue_pop", 0.45, "Harmonious pop_in & 2.5D shadow elevation")
    sb.add_cue("cue_inertia", t_slide1, "Stopping inertia, COM pitch & squash")
    sb.add_cue("cue_parallax", 11.80, "Orthographic 2.5D multi-plane parallax")

    is_test_audio = not bool(voice_wav_path)
    if voice_wav_path:
        sb.add_audio_event(
            AudioCue("voice_track", 0.0, duration_sec=duration_sec, gain=0.85, wav_path=voice_wav_path, is_test_tone=False)
        )

    sb.add_audio_event(AudioCue("sfx_pop_card", 0.85, duration_sec=0.12, gain=0.14, pitch_hz=523.2, sfx_kind=_nat.ProceduralSfxKind.Click, is_test_tone=is_test_audio))
    sb.add_audio_event(AudioCue("sfx_pop_disc", 1.55, duration_sec=0.12, gain=0.14, pitch_hz=659.2, sfx_kind=_nat.ProceduralSfxKind.Click, is_test_tone=is_test_audio))
    sb.add_audio_event(AudioCue("sfx_stop_contact", t_slide1, duration_sec=0.22, gain=0.20, pitch_hz=150.0, sfx_kind=_nat.ProceduralSfxKind.Contact, is_test_tone=is_test_audio))
    sb.add_audio_event(AudioCue("sfx_parallax_sweep", 11.80, duration_sec=0.40, gain=0.14, pitch_hz=440.0, sfx_kind=_nat.ProceduralSfxKind.Chime, is_test_tone=is_test_audio))

    overlaps = sb.check_overlaps()
    if overlaps:
        msgs = "; ".join(w.message for w in overlaps)
        raise RuntimeError(f"Layout overlap/safe-area violation in physics_motion_25d demo: {msgs}")

    return sb
