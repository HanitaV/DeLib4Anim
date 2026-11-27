"""STEM Demo: Chuyển Động Trên Mặt Phẳng Nghiêng Có Ma Sát (24s @ 1080p60).

Median-inspired visual narrative (no stickman, clean obsidian starfield space):
- Shot 1 (0.0-5.2s): Equilibrium P = N on horizontal beam -> smooth tilt 0 deg -> 30 deg.
- Shot 2 (5.2-10.5s): Weight decomposition (P_perp, P_parallel), kinetic friction F_mst,
  multi-color Newton's 2nd law card, and focus-dimmed scalar derivation.
- Shot 3 (10.5-17.2s): Accelerated slide (a = 3.21 m/s^2) with dashed s=0 start marker,
  quadratic stroboscopic markers, live v & a vectors, and gold displacement s = 3.80 m.
- Shot 4 (17.2-24.0s): Split composition pairing incline system with v(t) & s(t) plots,
  animated tracer dots, and synchronized dashed projection lines.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

from stem_engine import POP_HERO, POP_SNAPPY, POP_SOFT, AudioCue, SceneBuilder
from stem_engine import _stem_native as _nat


@dataclass(frozen=True)
class InclinePhysicsParams:
    mass_kg: float = 2.0
    theta_deg: float = 30.0
    mu_k: float = 0.20
    g_mps2: float = 9.81
    ramp_length_m: float = 3.80
    t_release_sec: float = 10.50

    @property
    def theta_rad(self) -> float:
        return math.radians(self.theta_deg)

    @property
    def weight_n(self) -> float:
        return self.mass_kg * self.g_mps2

    @property
    def normal_n(self) -> float:
        return self.weight_n * math.cos(self.theta_rad)

    @property
    def parallel_gravity_n(self) -> float:
        return self.weight_n * math.sin(self.theta_rad)

    @property
    def friction_n(self) -> float:
        return self.mu_k * self.normal_n

    @property
    def net_force_n(self) -> float:
        return self.parallel_gravity_n - self.friction_n

    @property
    def accel_mps2(self) -> float:
        return self.net_force_n / self.mass_kg

    @property
    def slide_duration_sec(self) -> float:
        return math.sqrt(2.0 * self.ramp_length_m / self.accel_mps2)

    @property
    def t_contact_sec(self) -> float:
        return self.t_release_sec + self.slide_duration_sec


def _quintic_in_out(u: float) -> float:
    if u <= 0.0:
        return 0.0
    if u >= 1.0:
        return 1.0
    if u < 0.5:
        return 16.0 * (u**5)
    f = -2.0 * u + 2.0
    return 1.0 - 0.5 * (f**5)


def simulate_incline_trajectory(
    params: InclinePhysicsParams,
    duration_sec: float = 24.0,
    dt: float = 1.0 / 240.0,
) -> tuple[list[tuple[float, float, float]], float]:
    """Fixed-timestep Velocity-Verlet simulation returning `(t, s_m, v_mps)` samples and `t_contact`."""
    steps = round(duration_sec / dt) + 4
    a_const = params.accel_mps2
    s = 0.0
    v = 0.0
    t_contact = params.t_contact_sec
    recorded_contact: float | None = None
    out: list[tuple[float, float, float]] = []

    for i in range(steps):
        t = i * dt
        out.append((t, s, v))
        if t < params.t_release_sec:
            continue
        if s >= params.ramp_length_m:
            s = params.ramp_length_m
            v = 0.0
            if recorded_contact is None:
                recorded_contact = t
            continue
        s_next = s + v * dt + 0.5 * a_const * dt * dt
        v_next = v + a_const * dt
        if s_next >= params.ramp_length_m:
            s = params.ramp_length_m
            v = 0.0
            if recorded_contact is None:
                recorded_contact = t + dt
        else:
            s = s_next
            v = v_next

    return out, (recorded_contact if recorded_contact is not None else t_contact)


def build_scene(
    width: int = 1920,
    height: int = 1080,
    voice_wav_path: str = "",
) -> SceneBuilder:
    duration_sec = 24.0
    params = InclinePhysicsParams()
    sim_samples, t_contact = simulate_incline_trajectory(params, duration_sec=duration_sec)
    theta_final = params.theta_rad
    cos_f = math.cos(theta_final)
    sin_f = math.sin(theta_final)

    # Color palette matching Median's Newtonian Mechanics series
    c_emerald = (0.349, 0.788, 0.459, 1.0)    # #59C975 gravity / acceleration
    c_white = (0.953, 0.961, 0.973, 1.0)      # #F3F5F8 normal force / velocity / primary ink
    c_coral = (0.922, 0.369, 0.333, 1.0)      # #EB5E55 kinetic friction
    c_gold = (0.949, 0.788, 0.298, 1.0)       # #F2C94C displacement / angle highlight
    c_muted = (0.600, 0.624, 0.671, 1.0)      # #999FAB Vietnamese subtitles & secondary guides

    px_per_m = 195.0
    start_s_px = 78.0
    ramp_len_px = params.ramp_length_m * px_per_m + 190.0
    beam_thick = 18.0
    block_w, block_h = 114.0, 46.0

    # Beam pivots smoothly around its top-surface center from 0 deg (t <= 2.4s) to 30 deg (t >= 3.8s)
    t_tilt0 = 2.40
    t_tilt1 = 3.80
    pivot_pt = (10.0, -56.0)

    def tilt_angle_at(t: float) -> float:
        if t <= t_tilt0:
            return 0.0
        if t >= t_tilt1:
            return theta_final
        return theta_final * _quintic_in_out((t - t_tilt0) / (t_tilt1 - t_tilt0))

    ramp_traj_samples: list[tuple[float, float]] = []
    block_traj_samples: list[tuple[float, float]] = []
    contact_traj_samples: list[tuple[float, float]] = []

    for t, s_m, _ in sim_samples:
        ang = tilt_angle_at(t)
        ca = math.cos(ang)
        sa = math.sin(ang)
        down = (ca, -sa)
        norm = (sa, ca)
        top_t = (pivot_pt[0] - down[0] * (ramp_len_px * 0.5), pivot_pt[1] - down[1] * (ramp_len_px * 0.5))
        s_px = start_s_px + s_m * px_per_m
        contact_pt = (top_t[0] + down[0] * s_px, top_t[1] + down[1] * s_px)
        block_pt = (contact_pt[0] + norm[0] * (block_h * 0.5), contact_pt[1] + norm[1] * (block_h * 0.5))
        beam_pt = (pivot_pt[0] - norm[0] * (beam_thick * 0.5), pivot_pt[1] - norm[1] * (beam_thick * 0.5))
        ramp_traj_samples.append(beam_pt)
        block_traj_samples.append(block_pt)
        contact_traj_samples.append(contact_pt)

    # Final inclined pose (t >= 3.80s)
    down_f = (cos_f, -sin_f)
    norm_f = (sin_f, cos_f)
    top_f = (pivot_pt[0] - down_f[0] * (ramp_len_px * 0.5), pivot_pt[1] - down_f[1] * (ramp_len_px * 0.5))
    bot_f = (top_f[0] + down_f[0] * ramp_len_px, top_f[1] + down_f[1] * ramp_len_px)
    block_rest_contact = (top_f[0] + down_f[0] * start_s_px, top_f[1] + down_f[1] * start_s_px)

    # Flat horizontal book center in Shot 1 (t < 2.4s)
    block_flat_contact = (pivot_pt[0] - ramp_len_px * 0.5 + start_s_px, pivot_pt[1])
    block_flat_center = (block_flat_contact[0], block_flat_contact[1] + block_h * 0.5)

    # Pure obsidian starfield space (no noisy floor slab, no banners, no stickman)
    sb = SceneBuilder(width=width, height=height, duration_sec=duration_sec)
    sb.set_background(
        top_color=(0.014, 0.015, 0.018, 1.0),
        bottom_color=(0.008, 0.009, 0.011, 1.0),
        show_grid=False,
        starfield=1.50,
        vignette=0.22,
        grain=0.004,
    )

    dt = 1.0 / 240.0
    ramp_traj_id = sb.add_trajectory(ramp_traj_samples, t_start=0.0, dt=dt)
    block_traj_id = sb.add_trajectory(block_traj_samples, t_start=0.0, dt=dt)
    contact_traj_id = sb.add_trajectory(contact_traj_samples, t_start=0.0, dt=dt)

    # ------------------------------------------------------------------
    # Camera choreography across 4 clean Median shots
    # ------------------------------------------------------------------
    cam_shot1 = (block_flat_center[0] + 150.0, block_flat_center[1] - 10.0)
    cam_shot2 = (-125.0, 28.0)
    cam_shot3 = (32.0, -52.0)
    cam_shot4 = (525.0, -28.0)  # Shifts entire ramp cleanly into left screen column
    sb.camera.set_pose(pos=cam_shot1, zoom=1.24)
    sb.camera.move_with_settle(2.35, 3.95, cam_shot1, cam_shot2, zoom0=1.24, zoom1=1.12, settle_duration=0.24, overshoot_frac=0.015)
    sb.camera.move_with_settle(9.95, 10.65, cam_shot2, cam_shot3, zoom0=1.12, zoom1=0.96, settle_duration=0.22, overshoot_frac=0.015)
    sb.camera.move_with_settle(16.85, 17.85, cam_shot3, cam_shot4, zoom0=0.96, zoom1=0.78, settle_duration=0.24, overshoot_frac=0.015)

    # Subtle 2.5D orthographic backdrop reference ticks (parallax=0.45)
    for gx in range(-600, 901, 300):
        sb.add_graph(
            f"bg_parallax_tick_{gx}",
            [(float(gx), -420.0), (float(gx), 420.0)],
            color=(0.24, 0.26, 0.31, 0.28),
            stroke_width=1.2,
            dashed=(5.0, 14.0),
            parallax=0.45,
            z_index=2,
            visible=1.0,
        )

    # ------------------------------------------------------------------
    # Floating Incline Beam, Hardcover Book & Soft Contact Pressure Halo
    # ------------------------------------------------------------------
    beam = sb.add_box(
        "incline_beam",
        center=(0.0, 0.0),
        size=(ramp_len_px, beam_thick),
        rotation_rad=0.0,
        fill_color=(0.228, 0.232, 0.245, 1.0),
        stroke_color=(0.35, 0.36, 0.39, 0.45),
        stroke_width=1.0,
        corner_radius=4.5,
        z_index=4,
        trajectory_id=ramp_traj_id,
        material="matte",
        elevation=6.0,
    )
    beam.native.rotation_rad.add_segment(t_tilt0, t_tilt1, 0.0, -theta_final, _nat.EasingType.QuinticInOut)

    # Tight contact glow right at the sliding book-beam seam
    glow_item = _nat.RectItem()
    glow_item.z_index = 9
    glow_item.material_mode = 3
    glow_item.trajectory_id = contact_traj_id
    glow_item.center.set_constant(_nat.Vec2(0.0, 0.0))
    glow_item.size.set_constant(_nat.Vec2(block_w * 1.48, 22.0))
    glow_item.rotation_rad.set_constant(0.0)
    glow_item.rotation_rad.add_segment(t_tilt0, t_tilt1, 0.0, -theta_final, _nat.EasingType.QuinticInOut)
    glow_item.corner_radius.set_constant(0.0)
    glow_item.fill_color.set_constant(_nat.ColorRGBA(0.95, 0.96, 0.99, 0.72))
    sb.native.add_rect(glow_item)

    # Side-view hardcover textbook ("book" shader + 2.5D elevation + contact inertia_stop)
    book = sb.add_box(
        "physics_book",
        center=(0.0, 0.0),
        size=(block_w, block_h),
        rotation_rad=0.0,
        fill_color=(0.245, 0.250, 0.268, 1.0),
        stroke_color=(0.44, 0.46, 0.50, 0.55),
        stroke_width=1.2,
        corner_radius=4.5,
        z_index=11,
        trajectory_id=block_traj_id,
        material="book",
        elevation=10.0,
    )
    book.native.rotation_rad.add_segment(t_tilt0, t_tilt1, 0.0, -theta_final, _nat.EasingType.QuinticInOut)
    book.inertia_stop(
        t_contact,
        direction=down_f,
        distance_px=7.5,
        squash=0.075,
        tilt_rad=-0.032,
        duration=0.44,
    )

    # Crisp white COM / contact origin dot
    com_dot = sb.add_circle(
        "origin_dot",
        center=(0.0, 0.0),
        radius=5.0,
        fill_color=c_white,
        z_index=22,
        trajectory_id=contact_traj_id,
        visible=0.0,
    )
    com_dot.pop_in(0.45, style=POP_SNAPPY)
    com_dot.hide(10.1, 10.4)

    # Subtle ramp angle theta = 30 deg geometry (horizontal reference dash + gold arc)
    horiz_end = (bot_f[0] - 260.0, bot_f[1])
    ang_ref_horiz = sb.add_graph(
        "theta_horiz_dash",
        [bot_f, horiz_end],
        color=(0.55, 0.58, 0.64, 0.65),
        stroke_width=2.0,
        dashed=(8.0, 6.0),
        z_index=5,
        visible=0.0,
        reveal=0.0,
    )
    ang_ref_horiz.reveal(3.5, 4.1)

    arc_r = 104.0
    theta_arc_pts = [
        (bot_f[0] - arc_r * math.cos(a), bot_f[1] + arc_r * math.sin(a))
        for a in [theta_final * (k / 24.0) for k in range(25)]
    ]
    theta_arc = sb.add_graph(
        "theta_arc",
        theta_arc_pts,
        color=c_gold,
        stroke_width=2.6,
        z_index=7,
        visible=0.0,
        reveal=0.0,
    )
    theta_arc.reveal(3.55, 4.15)

    theta_lbl = sb.add_text(
        "theta_lbl",
        "θ = 30°",
        (bot_f[0] - 158.0, bot_f[1] + 29.0),
        font_size_px=24.0,
        font_variant="semibold_italic",
        color=c_gold,
        screen_space=False,
        z_index=12,
        visible=0.0,
    )
    theta_lbl.reveal(3.7, 4.2)

    # ------------------------------------------------------------------
    # Shot 1 & 2 Vectors: Gravity P, Rotating Normal N, Right-Angle, Friction F_mst
    # ------------------------------------------------------------------
    px_per_n = 7.4
    p_len = params.weight_n * px_per_n
    n_len_final = params.normal_n * px_per_n

    vec_p = sb.add_vector(
        "vec_p",
        origin=(0.0, 0.0),
        delta=(0.0, -p_len),
        color=c_emerald,
        shaft_width=3.2,
        head_length=15.0,
        head_width=10.5,
        z_index=19,
        attach_trajectory_id=contact_traj_id,
        visible=0.0,
        reveal=0.0,
    )
    vec_p.reveal(0.50, 1.10).hide(10.1, 10.4)

    # Normal vector rotates & shrinks smoothly with tilt angle 0 -> 30 deg over [2.40, 3.80]
    vec_n = sb.add_vector(
        "vec_n",
        origin=(0.0, 0.0),
        delta=(0.0, p_len),
        color=c_white,
        shaft_width=3.2,
        head_length=15.0,
        head_width=10.5,
        z_index=19,
        attach_trajectory_id=contact_traj_id,
        visible=0.0,
        reveal=0.0,
    )
    vec_n.reveal(1.05, 1.65).hide(10.1, 10.4)
    tilt_steps = 16
    for k in range(tilt_steps):
        ta = t_tilt0 + (t_tilt1 - t_tilt0) * (k / tilt_steps)
        tb = t_tilt0 + (t_tilt1 - t_tilt0) * ((k + 1) / tilt_steps)
        aa = tilt_angle_at(ta)
        ab = tilt_angle_at(tb)
        la = p_len * math.cos(aa)
        lb = p_len * math.cos(ab)
        vec_n.native.delta.add_segment(
            ta,
            tb,
            _nat.Vec2(math.sin(aa) * la, math.cos(aa) * la),
            _nat.Vec2(math.sin(ab) * lb, math.cos(ab) * lb),
            _nat.EasingType.Linear,
        )

    # Equality double-ticks on P and N during horizontal equilibrium (m_1275s style)
    for idx, y_off in enumerate((-p_len * 0.44, -p_len * 0.51)):
        eq_p = sb.add_graph(
            f"eq_tick_p_{idx}",
            [(block_flat_contact[0] - 8.5, block_flat_contact[1] + y_off), (block_flat_contact[0] + 8.5, block_flat_contact[1] + y_off)],
            color=c_emerald,
            stroke_width=2.3,
            z_index=21,
            visible=0.0,
        )
        eq_p.reveal(1.55, 1.80).hide(2.35, 2.55)
        eq_n = sb.add_graph(
            f"eq_tick_n_{idx}",
            [(block_flat_contact[0] - 8.5, block_flat_contact[1] - y_off), (block_flat_contact[0] + 8.5, block_flat_contact[1] - y_off)],
            color=c_white,
            stroke_width=2.3,
            z_index=21,
            visible=0.0,
        )
        eq_n.reveal(1.55, 1.80).hide(2.35, 2.55)

    # Right-angle perpendicular square (∟) between N and tilted ramp
    ra_s = 16.0
    ra_pts = [
        (block_rest_contact[0] + norm_f[0] * ra_s, block_rest_contact[1] + norm_f[1] * ra_s),
        (
            block_rest_contact[0] + norm_f[0] * ra_s + down_f[0] * ra_s,
            block_rest_contact[1] + norm_f[1] * ra_s + down_f[1] * ra_s,
        ),
        (block_rest_contact[0] + down_f[0] * ra_s, block_rest_contact[1] + down_f[1] * ra_s),
    ]
    right_angle = sb.add_graph(
        "right_angle_n",
        ra_pts,
        color=(0.92, 0.94, 0.97, 0.88),
        stroke_width=2.0,
        z_index=18,
        visible=0.0,
    )
    right_angle.reveal(3.45, 3.85).hide(10.1, 10.4)

    # Median paired vector labels: bold-italic symbol + arrow + cool gray Vietnamese name
    lbl_p = sb.add_vector_label(
        "badge_p",
        "P",
        (18.0, -p_len * 0.64),
        subtitle="trọng lực",
        symbol_size_px=31.0,
        symbol_color=c_emerald,
        subtitle_color=c_muted,
        screen_space=False,
        trajectory_id=contact_traj_id,
        z_index=25,
        visible=0.0,
    )
    lbl_p.pop_in(0.65)
    lbl_p.hide(10.1, 10.4)

    lbl_n = sb.add_vector_label(
        "badge_n",
        "N",
        (18.0, p_len * 0.72),
        subtitle="lực pháp tuyến",
        symbol_size_px=31.0,
        symbol_color=c_white,
        subtitle_color=c_muted,
        screen_space=False,
        trajectory_id=contact_traj_id,
        z_index=25,
        visible=0.0,
    )
    lbl_n.pop_in(1.20)
    lbl_n.hide(10.1, 10.4)
    lbl_n.move_by(
        t_tilt0,
        t_tilt1,
        (0.0, 0.0),
        (norm_f[0] * n_len_final * 0.85, norm_f[1] * n_len_final * 0.85 - p_len * 0.72 + 12.0),
    )

    # ------------------------------------------------------------------
    # Shot 2 (5.2-10.5s): Gravity Decomposition & Kinetic Friction F_mst
    # ------------------------------------------------------------------
    p_tip_w = (block_rest_contact[0], block_rest_contact[1] - p_len)
    p_perp_w = (
        block_rest_contact[0] - norm_f[0] * n_len_final,
        block_rest_contact[1] - norm_f[1] * n_len_final,
    )
    p_para_len = params.parallel_gravity_n * px_per_n
    p_para_w = (
        block_rest_contact[0] + down_f[0] * p_para_len,
        block_rest_contact[1] + down_f[1] * p_para_len,
    )

    decomp_perp = sb.add_graph(
        "decomp_perp_dash",
        [p_tip_w, p_perp_w, block_rest_contact],
        color=(0.349, 0.788, 0.459, 0.58),
        stroke_width=2.0,
        dashed=(7.0, 6.0),
        z_index=15,
        visible=0.0,
        reveal=0.0,
    )
    decomp_perp.reveal(5.15, 5.75).hide(10.1, 10.4)

    decomp_para_guide = sb.add_graph(
        "decomp_para_dash",
        [p_tip_w, p_para_w],
        color=(0.349, 0.788, 0.459, 0.58),
        stroke_width=2.0,
        dashed=(7.0, 6.0),
        z_index=15,
        visible=0.0,
        reveal=0.0,
    )
    decomp_para_guide.reveal(5.35, 5.95).hide(10.1, 10.4)

    # Down-slope component P_parallel
    vec_p_para = sb.add_vector(
        "vec_p_para",
        origin=(0.0, 0.0),
        delta=(down_f[0] * p_para_len, down_f[1] * p_para_len),
        color=c_emerald,
        shaft_width=3.2,
        head_length=14.0,
        head_width=10.0,
        z_index=20,
        attach_trajectory_id=contact_traj_id,
        visible=0.0,
        reveal=0.0,
    )
    vec_p_para.reveal(5.55, 6.15).hide(10.1, 10.4)

    # Kinetic friction F_mst opposing down-slope motion at the interface
    f_mst_len = params.friction_n * px_per_n * 2.2
    vec_fk = sb.add_vector(
        "vec_fk",
        origin=(0.0, 0.0),
        delta=(-down_f[0] * f_mst_len, -down_f[1] * f_mst_len),
        color=c_coral,
        shaft_width=3.2,
        head_length=14.0,
        head_width=10.0,
        z_index=21,
        attach_trajectory_id=contact_traj_id,
        visible=0.0,
        reveal=0.0,
    )
    vec_fk.reveal(6.20, 6.80).hide(10.1, 10.4)

    lbl_fk = sb.add_vector_label(
        "badge_fk",
        "F",
        (-288.0, 56.0),
        subscript="mst",
        subtitle="ma sát trượt",
        symbol_size_px=29.0,
        symbol_color=c_coral,
        subtitle_color=c_muted,
        screen_space=False,
        trajectory_id=contact_traj_id,
        z_index=25,
        visible=0.0,
    )
    lbl_fk.pop_in(6.40)
    lbl_fk.hide(10.1, 10.4)

    # Top-center Median Formula Card: multi-color Newton's 2nd law along ramp
    eq_newton_card = sb.add_formula_card(
        "eq_newton_card",
        r"\textcolor{#59C975}{\vec{P}_\parallel} + \textcolor{#EB5E55}{\vec{F}_{\text{mst}}} = m\textcolor{#59C975}{\vec{a}}",
        (0.0, 398.0),
        target_height_px=46.0,
        stroke_color=(0.82, 0.85, 0.90, 0.88),
        stroke_width=1.6,
        pad_x=30.0,
        pad_y=18.0,
        elevation=16.0,
        z_index=32,
        visible=0.0,
        active_window=(5.6, 10.4),
    )
    eq_newton_card.pop_in(5.6, style=POP_HERO)
    eq_newton_card.hide(10.0, 10.4)

    # Clean spacious right-side quantitative derivation with Median focus dimming
    eq_n_val = sb.add_formula(
        "eq_n_val",
        r"N = mg\cos\theta = 16{,}99\,\text{N}",
        (455.0, 225.0),
        target_height_px=42.0,
        color=c_white,
        z_index=32,
        visible=0.0,
        active_window=(7.1, 10.4),
    )
    eq_n_val.pop_in(7.1, style=POP_SOFT)
    eq_n_val.dim_to(8.0, 8.35, 0.34, 1.0)
    eq_n_val.dim_to(10.0, 10.4, 0.0, 0.34)

    eq_fk_val = sb.add_formula(
        "eq_fk_val",
        r"\textcolor{#EB5E55}{F_{\text{mst}}} = \mu_k N = 3{,}40\,\text{N}",
        (455.0, 115.0),
        target_height_px=42.0,
        color=c_white,
        z_index=32,
        visible=0.0,
        active_window=(8.0, 10.4),
    )
    eq_fk_val.pop_in(8.0, style=POP_SOFT)
    eq_fk_val.dim_to(8.9, 9.25, 0.36, 1.0)
    eq_fk_val.dim_to(10.0, 10.4, 0.0, 0.36)

    eq_a_val = sb.add_formula(
        "eq_a_val",
        r"\textcolor{#59C975}{a} = g(\sin\theta - \mu_k\cos\theta) = \textcolor{#59C975}{3{,}21\,\text{m/s}^2}",
        (455.0, 0.0),
        target_height_px=42.0,
        color=c_white,
        z_index=32,
        visible=0.0,
        active_window=(8.9, 10.4),
    )
    eq_a_val.pop_in(8.9, style=POP_SOFT)
    eq_a_val.hide(10.0, 10.4)

    # ------------------------------------------------------------------
    # Shot 3 (10.5-17.2s): Release, Dynamic Vectors, Strobe Markers & Gold Displacement
    # ------------------------------------------------------------------
    # Dashed s=0 start marker perpendicular to ramp
    s0_bot = (block_rest_contact[0] - norm_f[0] * 34.0, block_rest_contact[1] - norm_f[1] * 34.0)
    s0_top = (block_rest_contact[0] + norm_f[0] * 130.0, block_rest_contact[1] + norm_f[1] * 130.0)
    start_dash = sb.add_graph(
        "start_dash_s0",
        [s0_bot, s0_top],
        color=(0.76, 0.79, 0.84, 0.72),
        stroke_width=2.0,
        dashed=(8.0, 6.0),
        z_index=8,
        visible=0.0,
    )
    start_dash.reveal(10.25, 10.55).hide(16.7, 17.2)

    # Motion trail behind book
    trail = _nat.GraphItem()
    trail.z_index = 9
    trail.trail_trajectory_id = block_traj_id
    trail.trail_duration_sec = 0.72
    trail.stroke_width.set_constant(3.8)
    trail.color.set_constant(_nat.ColorRGBA(0.95, 0.96, 0.99, 0.55))
    trail.visible.set_constant(0.0)
    trail.visible.add_segment(params.t_release_sec, params.t_release_sec + 0.06, 0.0, 1.0, _nat.EasingType.Linear)
    trail.visible.add_segment(t_contact + 0.8, t_contact + 1.4, 1.0, 0.0, _nat.EasingType.QuinticInOut)
    sb.native.add_graph(trail)

    # Equal-time stroboscopic markers along contact plane showing s ~ t^2 spacing
    for k in range(1, 5):
        dt_mark = k * 0.34
        t_mark = params.t_release_sec + dt_mark
        s_mark_m = 0.5 * params.accel_mps2 * dt_mark * dt_mark
        if s_mark_m < params.ramp_length_m:
            s_px = s_mark_m * px_per_m
            dot_pos = (block_rest_contact[0] + down_f[0] * s_px, block_rest_contact[1] + down_f[1] * s_px)
            strobe = sb.add_circle(
                f"strobe_{k}",
                center=dot_pos,
                radius=4.5,
                fill_color=c_gold,
                material="disc25d",
                z_index=13,
                visible=0.0,
            )
            strobe.pop_in(t_mark, style=POP_SNAPPY, duration=0.22, rise_px=0.0)
            strobe.hide(16.6, 17.2)

    # Live velocity vector v (white) ahead of block & constant acceleration a (emerald) above
    v_max_len = 148.0
    vec_v_live = sb.add_vector(
        "vec_v_live",
        origin=(0.0, 0.0),
        delta=(down_f[0] * 12.0, down_f[1] * 12.0),
        color=c_white,
        shaft_width=3.2,
        head_length=14.0,
        head_width=10.5,
        z_index=22,
        attach_trajectory_id=block_traj_id,
        attach_offset=(down_f[0] * (block_w * 0.5 + 10.0), down_f[1] * (block_w * 0.5 + 10.0)),
        visible=0.0,
    )
    vec_v_live.native.visible.add_segment(params.t_release_sec, params.t_release_sec + 0.12, 0.0, 1.0, _nat.EasingType.Linear)
    vec_v_live.native.visible.add_segment(t_contact, t_contact + 0.18, 1.0, 0.0, _nat.EasingType.QuinticInOut)
    vec_v_live.native.delta.add_segment(
        params.t_release_sec,
        t_contact,
        _nat.Vec2(down_f[0] * 8.0, down_f[1] * 8.0),
        _nat.Vec2(down_f[0] * v_max_len, down_f[1] * v_max_len),
        _nat.EasingType.Linear,
    )

    a_vec_len = 84.0
    vec_a_live = sb.add_vector(
        "vec_a_live",
        origin=(0.0, 0.0),
        delta=(down_f[0] * a_vec_len, down_f[1] * a_vec_len),
        color=c_emerald,
        shaft_width=3.2,
        head_length=14.0,
        head_width=10.0,
        z_index=22,
        attach_trajectory_id=block_traj_id,
        attach_offset=(norm_f[0] * 38.0 - down_f[0] * 18.0, norm_f[1] * 38.0 - down_f[1] * 18.0),
        visible=0.0,
    )
    vec_a_live.native.visible.add_segment(params.t_release_sec, params.t_release_sec + 0.12, 0.0, 1.0, _nat.EasingType.Linear)
    vec_a_live.native.visible.add_segment(t_contact + 0.4, t_contact + 0.8, 1.0, 0.0, _nat.EasingType.QuinticInOut)

    # Warm-gold displacement dimension arrow parallel above ramp (clear of theta=30 deg & vectors)
    dim_off_n = 118.0
    dim_origin = (block_rest_contact[0] + norm_f[0] * dim_off_n, block_rest_contact[1] + norm_f[1] * dim_off_n)
    s_px_total = params.ramp_length_m * px_per_m
    vec_s_gold = sb.add_vector(
        "vec_s_gold",
        origin=dim_origin,
        delta=(down_f[0] * s_px_total, down_f[1] * s_px_total),
        color=c_gold,
        shaft_width=2.8,
        head_length=14.0,
        head_width=10.0,
        z_index=14,
        visible=0.0,
        reveal=0.0,
    )
    vec_s_gold.native.visible.add_segment(params.t_release_sec + 0.05, params.t_release_sec + 0.15, 0.0, 1.0, _nat.EasingType.Linear)
    vec_s_gold.native.visible.add_segment(16.6, 17.1, 1.0, 0.0, _nat.EasingType.QuinticInOut)
    # Match quadratic s(t) growth of the gold displacement arrow in 16 steps
    disp_steps = 16
    slide_span = t_contact - params.t_release_sec
    for k in range(disp_steps):
        ua = k / disp_steps
        ub = (k + 1) / disp_steps
        vec_s_gold.native.reveal.add_segment(
            params.t_release_sec + ua * slide_span,
            params.t_release_sec + ub * slide_span,
            ua * ua,
            ub * ub,
            _nat.EasingType.Linear,
        )

    dim_mid = (
        dim_origin[0] + down_f[0] * (s_px_total * 0.45) + norm_f[0] * 34.0,
        dim_origin[1] + down_f[1] * (s_px_total * 0.45) + norm_f[1] * 34.0,
    )
    lbl_s_gold = sb.add_text(
        "lbl_s_gold",
        "s = 3,80 m  (quãng đường trượt)",
        dim_mid,
        font_size_px=22.0,
        font_variant="semibold",
        align_h=-1,
        color=c_gold,
        screen_space=False,
        z_index=15,
        visible=0.0,
    )
    lbl_s_gold.reveal(11.3, 11.9).hide(16.6, 17.1)

    # Top-center Shot 3 boxed kinematics formulas (m_0360s / m_0460s style)
    card_v = sb.add_formula_card(
        "card_v",
        r"\vec{v} = \vec{v}_0 + \textcolor{#59C975}{\vec{a}}t",
        (-215.0, 398.0),
        target_height_px=45.0,
        stroke_color=(0.82, 0.85, 0.90, 0.88),
        stroke_width=1.6,
        pad_x=28.0,
        pad_y=18.0,
        elevation=16.0,
        z_index=32,
        visible=0.0,
        active_window=(10.5, 17.1),
    )
    card_v.pop_in(10.55, style=POP_HERO)
    card_v.hide(16.6, 17.1)

    card_s = sb.add_formula_card(
        "card_s",
        r"s = \frac{1}{2}\textcolor{#59C975}{a}t^2",
        (215.0, 398.0),
        target_height_px=64.0,
        stroke_color=(0.82, 0.85, 0.90, 0.88),
        stroke_width=1.6,
        pad_x=28.0,
        pad_y=12.0,
        elevation=16.0,
        z_index=32,
        visible=0.0,
        active_window=(10.5, 17.1),
    )
    card_s.pop_in(11.0, style=POP_HERO)
    card_s.hide(16.6, 17.1)

    # ------------------------------------------------------------------
    # Shot 4 (17.2-24.0s): Median Split Screen — Ramp (Left) + Kinematics Plots (Right)
    # ------------------------------------------------------------------
    divider = sb.add_graph(
        "split_divider",
        [(0.0, -350.0), (0.0, 350.0)],
        color=(0.40, 0.42, 0.46, 0.55),
        stroke_width=1.6,
        screen_space=True,
        z_index=28,
        visible=0.0,
        reveal=0.0,
    )
    divider.reveal(17.5, 18.1)

    plot_x = 155.0
    plot_w = 590.0
    plot_h = 185.0
    v_y = 95.0
    s_y = -245.0
    slide_dur = params.slide_duration_sec
    v_max = params.accel_mps2 * slide_dur

    # Upper Plot: v(t)
    sb.add_vector(
        "v_axis_x",
        origin=(plot_x, v_y),
        delta=(plot_w + 26.0, 0.0),
        color=(0.68, 0.71, 0.77, 0.90),
        shaft_width=2.0,
        head_length=12.0,
        head_width=8.5,
        screen_space=True,
        z_index=30,
        visible=0.0,
    ).reveal(17.6, 18.1)
    sb.add_vector(
        "v_axis_y",
        origin=(plot_x, v_y),
        delta=(0.0, plot_h + 22.0),
        color=(0.68, 0.71, 0.77, 0.90),
        shaft_width=2.0,
        head_length=12.0,
        head_width=8.5,
        screen_space=True,
        z_index=30,
        visible=0.0,
    ).reveal(17.6, 18.1)

    v_pts = [
        (
            plot_x + (k / 48.0) * plot_w,
            v_y + ((params.accel_mps2 * (slide_dur * k / 48.0)) / v_max) * plot_h,
        )
        for k in range(49)
    ]
    v_curve = sb.add_graph(
        "v_curve",
        v_pts,
        color=c_white,
        stroke_width=3.2,
        screen_space=True,
        z_index=33,
        visible=0.0,
        reveal=0.0,
    )
    v_curve.reveal(18.0, 21.4)

    # Lower Plot: s(t)
    sb.add_vector(
        "s_axis_x",
        origin=(plot_x, s_y),
        delta=(plot_w + 26.0, 0.0),
        color=(0.68, 0.71, 0.77, 0.90),
        shaft_width=2.0,
        head_length=12.0,
        head_width=8.5,
        screen_space=True,
        z_index=30,
        visible=0.0,
    ).reveal(17.6, 18.1)
    sb.add_vector(
        "s_axis_y",
        origin=(plot_x, s_y),
        delta=(0.0, plot_h + 22.0),
        color=(0.68, 0.71, 0.77, 0.90),
        shaft_width=2.0,
        head_length=12.0,
        head_width=8.5,
        screen_space=True,
        z_index=30,
        visible=0.0,
    ).reveal(17.6, 18.1)

    s_pts = [
        (
            plot_x + (k / 48.0) * plot_w,
            s_y + ((0.5 * params.accel_mps2 * (slide_dur * k / 48.0) ** 2) / params.ramp_length_m) * plot_h,
        )
        for k in range(49)
    ]
    s_curve = sb.add_graph(
        "s_curve",
        s_pts,
        color=c_emerald,
        stroke_width=3.2,
        screen_space=True,
        z_index=33,
        visible=0.0,
        reveal=0.0,
    )
    s_curve.reveal(18.0, 21.4)

    # Synchronized white tracer dots & dashed projection drop-lines (m_0565s style)
    tracer_v_samples: list[tuple[float, float]] = []
    tracer_s_samples: list[tuple[float, float]] = []
    t_trace0, t_trace1 = 18.0, 21.4
    for i in range(round((t_trace1 - t_trace0) / dt) + 2):
        u = min(1.0, max(0.0, (i * dt) / (t_trace1 - t_trace0)))
        tx = plot_x + u * plot_w
        vy_pos = v_y + u * plot_h
        sy_pos = s_y + (u * u) * plot_h
        tracer_v_samples.append((tx, vy_pos))
        tracer_s_samples.append((tx, sy_pos))

    v_dot_traj = sb.add_trajectory(tracer_v_samples, t_start=t_trace0, dt=dt)
    s_dot_traj = sb.add_trajectory(tracer_s_samples, t_start=t_trace0, dt=dt)

    v_tracer = sb.add_circle(
        "v_tracer_dot",
        center=(0.0, 0.0),
        radius=5.5,
        fill_color=c_white,
        material="disc25d",
        screen_space=True,
        trajectory_id=v_dot_traj,
        z_index=35,
        visible=0.0,
    )
    v_tracer.pop_in(18.0, style=POP_SNAPPY, duration=0.22, rise_px=0.0)

    s_tracer = sb.add_circle(
        "s_tracer_dot",
        center=(0.0, 0.0),
        radius=5.5,
        fill_color=c_emerald,
        material="disc25d",
        screen_space=True,
        trajectory_id=s_dot_traj,
        z_index=35,
        visible=0.0,
    )
    s_tracer.pop_in(18.0, style=POP_SNAPPY, duration=0.22, rise_px=0.0)

    # End dashed projection guides at t = 1.54 s (revealed crisply right as trace completes)
    end_x = plot_x + plot_w
    v_drop = sb.add_graph(
        "v_proj_drop",
        [(plot_x, v_y + plot_h), (end_x, v_y + plot_h), (end_x, v_y)],
        color=(0.78, 0.81, 0.87, 0.65),
        stroke_width=1.8,
        dashed=(6.0, 5.0),
        screen_space=True,
        z_index=31,
        visible=0.0,
        reveal=0.0,
    )
    v_drop.reveal(21.20, 21.55)

    s_drop = sb.add_graph(
        "s_proj_drop",
        [(plot_x, s_y + plot_h), (end_x, s_y + plot_h), (end_x, s_y)],
        color=(0.349, 0.788, 0.459, 0.65),
        stroke_width=1.8,
        dashed=(6.0, 5.0),
        screen_space=True,
        z_index=31,
        visible=0.0,
        reveal=0.0,
    )
    s_drop.reveal(21.20, 21.55)

    # Clean plot labels (O origin, axis ticks, and clean prose) — verified overlap-free
    v_title = sb.add_text(
        "v_plot_lbl",
        "v(t) = a t   ·   vận tốc cực đại 4,94 m/s",
        (plot_x + 28.0, v_y + plot_h + 44.0),
        font_size_px=25.0,
        font_variant="semibold",
        align_h=-1,
        color=c_white,
        z_index=33,
        visible=0.0,
        active_window=(17.6, 24.0),
    )
    v_title.reveal(17.7, 18.2)

    s_title = sb.add_text(
        "s_plot_lbl",
        "s(t) = ½ a t²   ·   quãng đường 3,80 m",
        (plot_x + 28.0, s_y + plot_h + 44.0),
        font_size_px=25.0,
        font_variant="semibold",
        align_h=-1,
        color=c_emerald,
        z_index=33,
        visible=0.0,
        active_window=(17.6, 24.0),
    )
    s_title.reveal(17.7, 18.2)

    v_peak_lbl = sb.add_text(
        "v_peak_tick",
        "4,94",
        (plot_x - 16.0, v_y + plot_h - 2.0),
        font_size_px=19.0,
        align_h=1,
        color=c_muted,
        z_index=32,
        visible=0.0,
        active_window=(17.6, 24.0),
    )
    v_peak_lbl.reveal(17.7, 18.2)

    s_peak_lbl = sb.add_text(
        "s_peak_tick",
        "3,80",
        (plot_x - 16.0, s_y + plot_h - 2.0),
        font_size_px=19.0,
        align_h=1,
        color=c_emerald,
        z_index=32,
        visible=0.0,
        active_window=(17.6, 24.0),
    )
    s_peak_lbl.reveal(17.7, 18.2)

    for tag, ox, oy in (("v", plot_x, v_y), ("s", plot_x, s_y)):
        o_lbl = sb.add_text(
            f"{tag}_origin_o",
            "O",
            (ox - 24.0, oy - 22.0),
            font_size_px=22.0,
            color=c_muted,
            z_index=31,
            visible=0.0,
            active_window=(17.6, 24.0),
        )
        o_lbl.reveal(17.6, 18.1)
        end_t_lbl = sb.add_text(
            f"{tag}_end_t",
            "1,54",
            (ox + plot_w, oy - 24.0),
            font_size_px=19.0,
            color=c_muted,
            z_index=31,
            visible=0.0,
            active_window=(17.6, 24.0),
        )
        end_t_lbl.reveal(21.2, 21.6)
        t_lbl = sb.add_text(
            f"{tag}_axis_t",
            "t (s)",
            (ox + plot_w + 56.0, oy - 4.0),
            font_size_px=21.0,
            font_variant="italic",
            color=c_muted,
            z_index=31,
            visible=0.0,
            active_window=(17.6, 24.0),
        )
        t_lbl.reveal(17.6, 18.1)

    # ------------------------------------------------------------------
    # Shot Cues & Subtle Procedural Sound Design (0.12 - 0.24 gain)
    # ------------------------------------------------------------------
    sb.add_cue("cue_equilibrium", 0.50, "Can bang P = N tren mat ngang")
    sb.add_cue("cue_tilt", t_tilt0, "Mat phang nghieng dan 0 -> 30 deg")
    sb.add_cue("cue_fbd_decomp", 5.20, "Phan tich P va luc ma sat truot")
    sb.add_cue("cue_release", params.t_release_sec, "Vat truot nhanh dan deu")
    sb.add_cue("cue_stop", t_contact, "Vat dat cuoi doan duong s = 3.80 m")
    sb.add_cue("cue_kinematics", 17.50, "Do thi dong hoc v(t) va s(t)")

    is_test_audio = not bool(voice_wav_path)
    if voice_wav_path:
        sb.add_audio_event(AudioCue("voice_track", 0.0, duration_sec=duration_sec, gain=0.85, wav_path=voice_wav_path, is_test_tone=False))

    sb.add_audio_event(AudioCue("sfx_p", 0.50, duration_sec=0.12, gain=0.14, pitch_hz=440.0, sfx_kind=_nat.ProceduralSfxKind.Click, is_test_tone=is_test_audio))
    sb.add_audio_event(AudioCue("sfx_n", 1.05, duration_sec=0.12, gain=0.14, pitch_hz=523.2, sfx_kind=_nat.ProceduralSfxKind.Click, is_test_tone=is_test_audio))
    sb.add_audio_event(AudioCue("sfx_decomp", 5.55, duration_sec=0.12, gain=0.14, pitch_hz=587.3, sfx_kind=_nat.ProceduralSfxKind.Click, is_test_tone=is_test_audio))
    sb.add_audio_event(AudioCue("sfx_fk", 6.20, duration_sec=0.12, gain=0.15, pitch_hz=659.2, sfx_kind=_nat.ProceduralSfxKind.Click, is_test_tone=is_test_audio))
    sb.add_audio_event(AudioCue("sfx_release", params.t_release_sec, duration_sec=0.22, gain=0.22, pitch_hz=392.0, sfx_kind=_nat.ProceduralSfxKind.Release, is_test_tone=is_test_audio))
    sb.add_audio_event(AudioCue("sfx_slide", params.t_release_sec + 0.12, duration_sec=max(0.2, slide_dur - 0.12), gain=0.14, pitch_hz=185.0, sfx_kind=_nat.ProceduralSfxKind.Slide, is_test_tone=is_test_audio))
    sb.add_audio_event(AudioCue("sfx_contact", t_contact, duration_sec=0.20, gain=0.18, pitch_hz=140.0, sfx_kind=_nat.ProceduralSfxKind.Contact, is_test_tone=is_test_audio))
    sb.add_audio_event(AudioCue("sfx_kinematics", 18.0, duration_sec=0.45, gain=0.16, pitch_hz=523.2, sfx_kind=_nat.ProceduralSfxKind.Chime, is_test_tone=is_test_audio))

    overlaps = sb.check_overlaps()
    if overlaps:
        msgs = "; ".join(w.message for w in overlaps)
        raise RuntimeError(f"Layout overlap/safe-area violation detected in incline demo: {msgs}")

    return sb
