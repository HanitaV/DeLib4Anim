"""STEM Chemistry Showcase: Electron Thực Sự Trông Như Thế Nào? (28s @ 1080p60).

Visual language inspired by Median's quantum mechanics & atomic orbitals series:
- Shot 1 (0.0-5.2s): 3D shaded nucleon cluster + glowing classical orbiting electrons
  on perspective floor grid -> dissolves into Heisenberg uncertainty relation.
- Shot 2 (5.2-10.6s): Live Gaussian wave packet psi(x,t) -> shaded probability density
  |psi(x,t)|^2 curve with slice probe & dimmed Schrodinger equation.
- Shot 3 (10.6-16.4s): 24,000-dot 3D Hydrogen 1s -> 2p orbital cloud + quantized n ladder.
- Shot 4 (16.4-23.2s): Angular momentum & probability current flow (2p m=1 -> 3d m=2 -> 4f m=3)
  with Bohmian azimuthal rotation and sequential numbered callout badges.
- Shot 5 (23.2-28.0s): Full-stage 3d m=2 synthesis inside viewfinder brackets with
  measurement collapse into a single localized electron point.
"""

from __future__ import annotations

import math

from stem_engine import POP_HERO, POP_SNAPPY, POP_SOFT, AudioCue, SceneBuilder
from stem_engine import _stem_native as _nat


def build_scene(
    width: int = 1920,
    height: int = 1080,
    voice_wav_path: str = "",
) -> SceneBuilder:
    duration_sec = 28.0
    dt = 1.0 / 240.0

    # Neutral Median dark-studio backdrop gradient (pure void upper, subtle charcoal bounce lower)
    sb = SceneBuilder(width=width, height=height, duration_sec=duration_sec)
    sb.set_background(
        top_color=(0.008, 0.009, 0.011, 1.0),
        bottom_color=(0.042, 0.044, 0.050, 1.0),
        show_grid=False,
        starfield=0.0,
        vignette=0.28,
        grain=0.004,
    )

    c_white = (0.945, 0.953, 0.969, 1.0)
    c_cloud = (0.860, 0.878, 0.908, 1.0)
    c_muted = (0.580, 0.600, 0.640, 1.0)
    c_proton = (0.800, 0.820, 0.855, 1.0)
    c_neutron = (0.370, 0.390, 0.430, 1.0)

    # ------------------------------------------------------------------
    # Shot 1 (0.0-5.2s): Layered 2.5D Classical Planetary Atom -> Heisenberg
    # (3D is reserved strictly for the irreducible quantum orbitals in Shots 3-5)
    # ------------------------------------------------------------------
    atom_c = (0.0, 28.0)
    for r_idx, ring_r in enumerate((195.0, 325.0, 465.0)):
        ring_pts = [
            (
                atom_c[0] + ring_r * math.cos(2.0 * math.pi * k / 96.0),
                atom_c[1] + ring_r * math.sin(2.0 * math.pi * k / 96.0),
            )
            for k in range(97)
        ]
        bg_ring = sb.add_graph(
            f"bg_ring_25d_{r_idx}",
            ring_pts,
            color=(0.30, 0.32, 0.37, 0.44 - r_idx * 0.08),
            stroke_width=1.4,
            dashed=(7.0, 9.0),
            parallax=0.55,
            z_index=2,
            visible=1.0,
        )
        bg_ring.hide(2.9, 3.5)

    # Layered 2.5D nucleon cluster (protons + neutrons as crisp elevated discs)
    nucleons = [
        (-24.0, 12.0, -18.0, False),
        (22.0, -14.0, -15.0, True),
        (4.0, 26.0, -12.0, False),
        (-14.0, -22.0, -8.0, True),
        (-28.0, -6.0, 4.0, True),
        (26.0, 10.0, 6.0, False),
        (0.0, 28.0, 12.0, True),
        (16.0, 18.0, 16.0, True),
        (-16.0, 16.0, 18.0, False),
        (14.0, -18.0, 20.0, False),
        (-10.0, -16.0, 22.0, True),
        (2.0, 2.0, 26.0, True),
    ]
    nucleons.sort(key=lambda item: item[2])
    for idx, (nx, ny, nz, is_p) in enumerate(nucleons):
        elev_n = 4.0 + ((nz + 18.0) / 44.0) * 8.0
        nuc = sb.add_circle(
            f"nucleon_{idx}",
            center=(atom_c[0] + nx, atom_c[1] + ny),
            radius=23.5,
            fill_color=c_proton if is_p else c_neutron,
            stroke_color=(0.12, 0.13, 0.16, 0.65),
            stroke_width=1.4,
            material="disc25d",
            elevation=elev_n,
            z_index=10 + idx,
        )
        nuc.hide(2.85, 3.45)

    # Three tilted orbital rings + two glowing classical electrons with luminous wakes
    ring_specs = [
        (275.0, 108.0, math.radians(-26.0), 3.1, 0.0),
        (275.0, 108.0, math.radians(32.0), -2.8, 2.1),
        (275.0, 108.0, math.radians(84.0), 2.5, 4.2),
    ]
    n_s1 = round(4.0 / dt) + 2
    for ridx, (rx, ry, rot_a, ang_vel, phi0) in enumerate(ring_specs):
        ca, sa = math.cos(rot_a), math.sin(rot_a)
        pts = []
        for k in range(97):
            u = 2.0 * math.pi * (k / 96.0)
            lx, ly = rx * math.cos(u), ry * math.sin(u)
            pts.append((atom_c[0] + ca * lx - sa * ly, atom_c[1] + sa * lx + ca * ly))
        ring = sb.add_graph(
            f"class_ring_{ridx}",
            pts,
            color=(0.48, 0.50, 0.54, 0.72),
            stroke_width=2.0,
            z_index=8,
        )
        ring.hide(2.85, 3.45)

        if ridx < 2:
            e_samples = []
            for i in range(n_s1):
                u = phi0 + ang_vel * (i * dt)
                lx, ly = rx * math.cos(u), ry * math.sin(u)
                e_samples.append((atom_c[0] + ca * lx - sa * ly, atom_c[1] + sa * lx + ca * ly))
            tr_id = sb.add_trajectory(e_samples, t_start=0.0, dt=dt)

            e_wake = _nat.GraphItem()
            e_wake.z_index = 24
            e_wake.trail_trajectory_id = tr_id
            e_wake.trail_duration_sec = 0.36
            e_wake.stroke_width.set_constant(4.5)
            e_wake.color.set_constant(_nat.ColorRGBA(0.95, 0.96, 0.99, 0.65))
            e_wake.visible.add_segment(2.85, 3.35, 1.0, 0.0, _nat.EasingType.QuinticInOut)
            sb.native.add_graph(e_wake)

            e_dot = sb.add_circle(
                f"class_electron_{ridx}",
                center=(0.0, 0.0),
                radius=11.0,
                fill_color=c_white,
                material="glow",
                trajectory_id=tr_id,
                z_index=25,
            )
            e_dot.hide(2.85, 3.35)

    # Center Heisenberg Uncertainty Principle
    eq_heis = sb.add_formula(
        "eq_heisenberg",
        r"\Delta x\,\Delta p \ge \frac{\hbar}{2}",
        (0.0, 28.0),
        target_height_px=72.0,
        color=c_white,
        z_index=32,
        visible=0.0,
        active_window=(3.4, 5.15),
    )
    eq_heis.pop_in(3.4, style=POP_HERO)
    eq_heis.hide(4.85, 5.20)

    lbl_heis = sb.add_text(
        "lbl_heis",
        "nguyên lý bất định Heisenberg",
        (0.0, -72.0),
        font_size_px=26.0,
        color=c_muted,
        z_index=32,
        visible=0.0,
        active_window=(3.4, 5.15),
    )
    lbl_heis.pop_in(3.55, style=POP_SOFT)
    lbl_heis.hide(4.85, 5.20)

    # ------------------------------------------------------------------
    # Shot 2 (5.2-10.6s): Wave Packet psi(x,t) -> Shaded Probability |psi(x,t)|^2
    # ------------------------------------------------------------------
    wave_oy = 12.0
    x_axis = sb.add_graph(
        "wave_x_axis",
        [(-560.0, wave_oy), (560.0, wave_oy)],
        color=(0.68, 0.70, 0.74, 0.85),
        stroke_width=2.0,
        screen_space=True,
        z_index=20,
        visible=0.0,
        reveal=0.0,
    )
    x_axis.reveal(5.20, 5.65).hide(10.25, 10.60)

    x_axis_lbl = sb.add_text(
        "wave_x_lbl",
        "x",
        (586.0, wave_oy),
        font_size_px=26.0,
        font_variant="italic",
        color=c_muted,
        z_index=25,
        visible=0.0,
        active_window=(5.2, 10.6),
    )
    x_axis_lbl.reveal(5.3, 5.65).hide(10.25, 10.60)

    # Phase 2A (5.2-7.6s): Oscillating Gaussian wave packet
    hdr_psi = sb.add_text(
        "hdr_psi",
        "hàm sóng ψ(x, t)",
        (0.0, 325.0),
        font_size_px=40.0,
        font_variant="semibold",
        color=c_white,
        z_index=32,
        visible=0.0,
        active_window=(5.2, 7.6),
    )
    hdr_psi.pop_in(5.25, style=POP_SOFT)
    hdr_psi.hide(7.25, 7.65)

    sigma_wp = 148.0
    xs_wp = [-500.0 + k * 4.0 for k in range(251)]
    env_top = sb.add_graph(
        "wp_env_top",
        [(x, 96.0 * math.exp(-0.5 * (x / sigma_wp) ** 2)) for x in xs_wp],
        origin=(0.0, wave_oy),
        color=(0.35, 0.37, 0.41, 0.65),
        stroke_width=1.6,
        dashed=(6.0, 5.0),
        screen_space=True,
        z_index=21,
        visible=0.0,
    )
    env_top.reveal(5.3, 5.8).hide(7.25, 7.65)

    env_bot = sb.add_graph(
        "wp_env_bot",
        [(x, -96.0 * math.exp(-0.5 * (x / sigma_wp) ** 2)) for x in xs_wp],
        origin=(0.0, wave_oy),
        color=(0.35, 0.37, 0.41, 0.65),
        stroke_width=1.6,
        dashed=(6.0, 5.0),
        screen_space=True,
        z_index=21,
        visible=0.0,
    )
    env_bot.reveal(5.3, 5.8).hide(7.25, 7.65)

    wp_wave = sb.add_graph(
        "wp_carrier",
        [(x, 0.0) for x in xs_wp],
        origin=(0.0, wave_oy),
        color=c_white,
        stroke_width=3.0,
        screen_space=True,
        envelope_sigma=sigma_wp,
        z_index=24,
        visible=0.0,
    )
    wp_wave.native.deform_freq = 0.068
    wp_wave.native.deform_speed = 5.2
    wp_wave.native.deform_amplitude.add_segment(5.3, 5.8, 0.0, 96.0, _nat.EasingType.QuinticOut)
    wp_wave.reveal(5.3, 5.7).hide(7.25, 7.65)

    # Phase 2B (7.6-10.6s): Shaded Probability Density |psi(x, t)|^2 (c_08_0306s style)
    hdr_prob = sb.add_text(
        "hdr_prob",
        "sóng xác suất |ψ(x, t)|²",
        (0.0, 325.0),
        font_size_px=40.0,
        font_variant="semibold",
        color=c_white,
        z_index=32,
        visible=0.0,
        active_window=(7.6, 10.6),
    )
    hdr_prob.pop_in(7.6, style=POP_SOFT)
    hdr_prob.hide(10.25, 10.60)

    sigma_p = 115.0
    amp_p = 232.0
    prob_pts = [(x, amp_p * math.exp(-0.5 * (x / sigma_p) ** 2)) for x in xs_wp]
    prob_curve = sb.add_graph(
        "prob_density_curve",
        prob_pts,
        origin=(0.0, wave_oy),
        color=c_white,
        stroke_width=3.0,
        screen_space=True,
        fill_under=True,
        fill_baseline_y=0.0,
        fill_top_color=(0.30, 0.32, 0.36, 0.78),
        fill_bottom_color=(0.02, 0.02, 0.03, 0.05),
        z_index=24,
        visible=0.0,
        reveal=0.0,
    )
    prob_curve.reveal(7.65, 8.35).hide(10.25, 10.60)

    # Slice measurement probe at x = +96 px
    probe_x = 96.0
    probe_h = amp_p * math.exp(-0.5 * (probe_x / sigma_p) ** 2)
    probe_line = sb.add_graph(
        "prob_probe",
        [(probe_x, wave_oy), (probe_x, wave_oy + probe_h)],
        color=c_white,
        stroke_width=3.4,
        screen_space=True,
        z_index=26,
        visible=0.0,
        reveal=0.0,
    )
    probe_line.reveal(8.25, 8.65).hide(10.25, 10.60)

    for tag, py in (("bot", wave_oy), ("top", wave_oy + probe_h)):
        dot = sb.add_circle(
            f"probe_dot_{tag}",
            center=(probe_x, py),
            radius=5.2,
            fill_color=c_white,
            screen_space=True,
            z_index=27,
            visible=0.0,
        )
        dot.reveal(8.25, 8.55).hide(10.25, 10.60)

    # Small measurement bracket [___] beneath axis + caption
    bw = 26.0
    bracket = sb.add_graph(
        "prob_bracket",
        [
            (probe_x - bw, wave_oy - 10.0),
            (probe_x - bw, wave_oy - 22.0),
            (probe_x + bw, wave_oy - 22.0),
            (probe_x + bw, wave_oy - 10.0),
        ],
        color=(0.86, 0.88, 0.92, 0.90),
        stroke_width=2.0,
        screen_space=True,
        z_index=26,
        visible=0.0,
    )
    bracket.reveal(8.45, 8.80).hide(10.25, 10.60)

    lbl_psi_probe = sb.add_text(
        "lbl_psi_probe",
        "|ψ(x)|²",
        (probe_x - 56.0, wave_oy + 76.0),
        font_size_px=26.0,
        font_variant="semibold",
        color=c_white,
        z_index=28,
        visible=0.0,
        active_window=(8.3, 10.6),
    )
    lbl_psi_probe.reveal(8.35, 8.70).hide(10.25, 10.60)

    lbl_find = sb.add_text(
        "lbl_find",
        "xác suất tìm thấy electron ở đây",
        (probe_x, wave_oy - 58.0),
        font_size_px=24.0,
        color=c_white,
        z_index=28,
        visible=0.0,
        active_window=(8.5, 10.6),
    )
    lbl_find.reveal(8.55, 8.95).hide(10.25, 10.60)

    # Dimmed Schrodinger equation below axis (c_08_0306s bottom zone)
    eq_sch_dim = sb.add_formula(
        "eq_sch_dim",
        r"i\hbar\frac{\partial\psi}{\partial t} = \hat{H}\psi",
        (0.0, -238.0),
        target_height_px=62.0,
        color=(0.56, 0.58, 0.62, 0.90),
        z_index=28,
        visible=0.0,
        active_window=(8.1, 10.6),
    )
    eq_sch_dim.reveal(8.1, 8.55).hide(10.25, 10.60)

    lbl_sch_dim = sb.add_text(
        "lbl_sch_dim",
        "phương trình Schrödinger",
        (0.0, -304.0),
        font_size_px=23.0,
        color=(0.45, 0.47, 0.51, 0.90),
        z_index=28,
        visible=0.0,
        active_window=(8.2, 10.6),
    )
    lbl_sch_dim.reveal(8.2, 8.65).hide(10.25, 10.60)

    # ------------------------------------------------------------------
    # Shot 3 (10.6-16.4s): 3D Hydrogen 1s -> 2p + Quantized Energy Ladder
    # ------------------------------------------------------------------
    orb3_pos = (-260.0, -10.0)
    cloud3 = sb.add_orbital_cloud(
        "orbital_1s_2p",
        center=orb3_pos,
        n_a=1,
        l_a=0,
        m_a=0,
        bohr_a=44.0,
        n_b=2,
        l_b=1,
        m_b=0,
        bohr_b=27.5,
        particle_count=24000,
        dot_radius_px=2.1,
        pitch_rad=0.22,
        color=c_cloud,
        z_index=10,
        visible=0.0,
        reveal=0.0,
        seed=201,
    )
    cloud3.reveal(10.60, 11.20).morph(13.20, 14.30, 0.0, 1.0).hide(16.00, 16.45)

    nuc3 = sb.add_circle(
        "nuc_3",
        center=orb3_pos,
        radius=5.2,
        fill_color=c_white,
        material="glow",
        z_index=14,
        visible=0.0,
    )
    nuc3.reveal(10.60, 10.90).hide(16.00, 16.45)

    # Top-left orbital tags
    tag_1s = sb.add_text(
        "tag_1s",
        "1s   (l = 0, m = 0)",
        (-850.0, 460.0),
        font_size_px=34.0,
        font_variant="semibold",
        align_h=-1,
        color=c_white,
        z_index=32,
        visible=0.0,
        active_window=(10.6, 13.45),
    )
    tag_1s.reveal(10.65, 11.05).hide(13.15, 13.45)

    tag_2p0 = sb.add_text(
        "tag_2p0",
        "2p   (l = 1, m = 0)",
        (-850.0, 460.0),
        font_size_px=34.0,
        font_variant="semibold",
        align_h=-1,
        color=c_white,
        z_index=32,
        visible=0.0,
        active_window=(13.5, 16.4),
    )
    tag_2p0.reveal(13.50, 13.90).hide(16.05, 16.45)

    # Right-stage Energy Ladder (c_09_0344s style)
    ladder_ax_x = 180.0
    sb.add_vector(
        "energy_axis",
        origin=(ladder_ax_x, -260.0),
        delta=(0.0, 540.0),
        color=(0.74, 0.76, 0.80, 0.90),
        shaft_width=2.0,
        head_length=14.0,
        head_width=10.5,
        screen_space=True,
        z_index=28,
        visible=0.0,
    ).reveal(10.65, 11.10).hide(16.00, 16.45)

    sb.add_text(
        "energy_lbl",
        "năng lượng",
        (ladder_ax_x, 316.0),
        font_size_px=26.0,
        color=c_white,
        z_index=30,
        visible=0.0,
        active_window=(10.6, 16.4),
    ).reveal(10.70, 11.10).hide(16.00, 16.45)

    levels = [(1, -185.0), (2, 85.0), (3, 185.0), (4, 238.0)]
    for n_val, ly in levels:
        lvl_line = sb.add_graph(
            f"lvl_{n_val}",
            [(235.0, ly), (515.0, ly)],
            color=c_white if n_val <= 2 else c_muted,
            stroke_width=3.2 if n_val <= 2 else 1.8,
            screen_space=True,
            z_index=28,
            visible=0.0,
        )
        lvl_line.reveal(10.70, 11.10).hide(16.00, 16.45)
        lvl_txt = sb.add_text(
            f"lvl_txt_{n_val}",
            f"n = {n_val}",
            (545.0, ly),
            font_size_px=26.0 if n_val <= 2 else 22.0,
            align_h=-1,
            color=c_white if n_val <= 2 else c_muted,
            z_index=30,
            visible=0.0,
            active_window=(10.6, 16.4),
        )
        lvl_txt.reveal(10.75, 11.15).hide(16.00, 16.45)

    e_jump_dot = sb.add_circle(
        "energy_jump_dot",
        center=(375.0, -185.0),
        radius=9.5,
        fill_color=c_white,
        material="glow",
        screen_space=True,
        z_index=32,
        visible=0.0,
    )
    e_jump_dot.pop_in(10.80, style=POP_SNAPPY, rise_px=0.0)
    e_jump_dot.move_to(13.10, 14.15, (375.0, -185.0), (375.0, 85.0)).hide(16.00, 16.45)

    # ------------------------------------------------------------------
    # Shot 4 (16.4-23.2s): Angular Momentum Flow (2p m=1 -> 3d m=2 -> 4f m=3) & Callouts
    # ------------------------------------------------------------------
    orb4_pos = (-325.0, -10.0)
    cloud4a = sb.add_orbital_cloud(
        "orbital_2pm1_3dm2",
        center=orb4_pos,
        n_a=2,
        l_a=1,
        m_a=1,
        bohr_a=29.0,
        n_b=3,
        l_b=2,
        m_b=2,
        bohr_b=15.5,
        particle_count=24000,
        dot_radius_px=2.05,
        pitch_rad=0.26,
        flow_speed=1.15,
        color=c_cloud,
        z_index=10,
        visible=0.0,
        reveal=0.0,
        seed=307,
    )
    cloud4a.reveal(16.35, 16.85)
    cloud4a.morph(17.70, 18.80, 0.0, 1.0)
    cloud4a.orbit_camera(17.70, 19.65, 0.0, 1.35, 0.26, 0.56)
    cloud4a.hide(19.62, 19.65)

    cloud4b = sb.add_orbital_cloud(
        "orbital_3dm2_4fm3",
        center=orb4_pos,
        n_a=3,
        l_a=2,
        m_a=2,
        bohr_a=15.5,
        n_b=4,
        l_b=3,
        m_b=3,
        bohr_b=9.8,
        particle_count=24000,
        dot_radius_px=2.05,
        yaw_rad=1.35,
        pitch_rad=0.56,
        flow_speed=1.25,
        color=c_cloud,
        z_index=10,
        visible=0.0,
        reveal=0.0,
        seed=409,
    )
    cloud4b.reveal(19.65, 19.68)
    cloud4b.morph(20.15, 21.35, 0.0, 1.0)
    cloud4b.orbit_camera(20.15, 22.80, 1.35, 2.50, 0.56, 0.34)
    cloud4b.hide(22.80, 23.25)

    nuc4 = sb.add_circle(
        "nuc_4",
        center=orb4_pos,
        radius=5.0,
        fill_color=c_white,
        material="glow",
        z_index=14,
        visible=0.0,
    )
    nuc4.reveal(16.40, 16.70).hide(22.80, 23.25)

    for tag_id, txt, t0, t1 in (
        ("tag_2pm1", "2p, m = 1", 16.40, 18.00),
        ("tag_3dm2", "3d, m = 2", 18.05, 20.90),
        ("tag_4fm3", "4f, m = 3", 20.95, 23.20),
    ):
        t_lbl = sb.add_text(
            tag_id,
            txt,
            (-850.0, 460.0),
            font_size_px=34.0,
            font_variant="semibold",
            align_h=-1,
            color=c_white,
            z_index=32,
            visible=0.0,
            active_window=(t0, t1),
        )
        t_lbl.reveal(t0, min(t1, t0 + 0.35)).hide(max(t0, t1 - 0.30), t1)

    b1 = sb.add_numbered_callout("co_1", "1", "chấm sáng = khả năng", (175.0, 118.0), active_window=(16.7, 23.2))
    b2 = sb.add_numbered_callout("co_2", "2", "kích thước = năng lượng", (175.0, 0.0), active_window=(18.7, 23.2))
    b3 = sb.add_numbered_callout("co_3", "3", "chuyển động = mômen động lượng", (175.0, -118.0), active_window=(20.7, 23.2))

    b1.pop_in(16.70).dim(18.70, 19.05).hide(22.80, 23.20)
    b2.pop_in(18.70).dim(20.70, 21.05).hide(22.80, 23.20)
    b3.pop_in(20.70).hide(22.80, 23.20)

    # ------------------------------------------------------------------
    # Shot 5 (23.2-28.0s): Full-Stage 3d m=2 Framing & Measurement Collapse
    # ------------------------------------------------------------------
    collapse_pt = (86.0, -14.0)
    cloud5 = sb.add_orbital_cloud(
        "orbital_final_collapse",
        center=(0.0, -25.0),
        n_a=3,
        l_a=2,
        m_a=2,
        bohr_a=15.5,
        n_b=3,
        l_b=2,
        m_b=2,
        bohr_b=15.5,
        particle_count=26000,
        dot_radius_px=2.05,
        pitch_rad=0.42,
        flow_speed=1.35,
        collapse_target=collapse_pt,
        color=c_cloud,
        z_index=10,
        visible=0.0,
        reveal=0.0,
        seed=503,
    )
    cloud5.reveal(23.15, 23.70)
    cloud5.orbit_camera(23.20, 25.30, 0.0, 1.4, 0.42, 0.56)
    cloud5.collapse(25.35, 25.95, 0.0, 1.0)
    cloud5.collapse(26.55, 27.35, 1.0, 0.0)

    nuc5 = sb.add_circle(
        "nuc_5",
        center=(0.0, -25.0),
        radius=5.2,
        fill_color=c_white,
        material="glow",
        z_index=14,
        visible=0.0,
    )
    nuc5.reveal(23.20, 23.50)

    # Bright collapsed single-electron flash at measurement site
    e_measured = sb.add_circle(
        "measured_electron",
        center=(collapse_pt[0], collapse_pt[1] - 25.0),
        radius=12.0,
        fill_color=c_white,
        material="glow",
        z_index=18,
        visible=0.0,
    )
    e_measured.pop_in(25.80, style=POP_SNAPPY, duration=0.22, rise_px=0.0)
    e_measured.hide(26.55, 26.90)

    vf = sb.add_viewfinder_brackets(
        "viewfinder_final",
        center=(0.0, -25.0),
        size=(660.0, 440.0),
        arm_len=32.0,
        z_index=30,
        visible=0.0,
    )
    vf.pop_in(23.30, duration=0.45, offset_px=18.0)

    sb.add_text(
        "tag_5_3d",
        "3d, m = 2",
        (-850.0, 460.0),
        font_size_px=34.0,
        font_variant="semibold",
        align_h=-1,
        color=c_white,
        z_index=32,
        visible=0.0,
        active_window=(23.2, 28.0),
    ).reveal(23.25, 23.65)

    sb.add_formula(
        "eq_sch_final",
        r"i\hbar\frac{\partial\psi}{\partial t} = \hat{H}\psi",
        (0.0, 355.0),
        target_height_px=64.0,
        color=c_white,
        z_index=32,
        visible=0.0,
        active_window=(23.3, 28.0),
    ).reveal(23.30, 23.80)

    sb.add_text(
        "lbl_collapse_note",
        "đo vị trí → hàm sóng co sụp về một điểm",
        (0.0, -345.0),
        font_size_px=26.0,
        color=c_white,
        z_index=32,
        visible=0.0,
        active_window=(25.5, 28.0),
    ).reveal(25.55, 25.95)

    # ------------------------------------------------------------------
    # Timeline Cues & Minimalist Audio Design
    # ------------------------------------------------------------------
    sb.add_cue("cue_classical", 0.4, "Mo hinh hanh tinh co dien & Heisenberg")
    sb.add_cue("cue_wavepacket", 5.2, "Ham song psi(x,t) va mat do xac suat")
    sb.add_cue("cue_1s_2p", 10.6, "Dam may 3D Hydrogen 1s nhay len 2p")
    sb.add_cue("cue_angular_flow", 16.4, "Dong chay xac suat 2p m=1 -> 3d m=2 -> 4f m=3")
    sb.add_cue("cue_collapse", 23.2, "Goc nhin toan canh & co sup phep do")

    is_test_audio = not bool(voice_wav_path)
    if voice_wav_path:
        sb.add_audio_event(AudioCue("voice_track", 0.0, duration_sec=duration_sec, gain=0.85, wav_path=voice_wav_path, is_test_tone=False))

    sb.add_audio_event(AudioCue("sfx_heis", 3.4, duration_sec=0.32, gain=0.14, pitch_hz=440.0, sfx_kind=_nat.ProceduralSfxKind.Chime, is_test_tone=is_test_audio))
    sb.add_audio_event(AudioCue("sfx_prob", 7.65, duration_sec=0.16, gain=0.14, pitch_hz=523.2, sfx_kind=_nat.ProceduralSfxKind.Click, is_test_tone=is_test_audio))
    sb.add_audio_event(AudioCue("sfx_jump", 13.2, duration_sec=0.35, gain=0.16, pitch_hz=659.2, sfx_kind=_nat.ProceduralSfxKind.Chime, is_test_tone=is_test_audio))
    sb.add_audio_event(AudioCue("sfx_3dm2", 17.7, duration_sec=0.14, gain=0.14, pitch_hz=587.3, sfx_kind=_nat.ProceduralSfxKind.Click, is_test_tone=is_test_audio))
    sb.add_audio_event(AudioCue("sfx_4fm3", 20.2, duration_sec=0.14, gain=0.14, pitch_hz=784.0, sfx_kind=_nat.ProceduralSfxKind.Click, is_test_tone=is_test_audio))
    sb.add_audio_event(AudioCue("sfx_collapse", 25.85, duration_sec=0.26, gain=0.20, pitch_hz=330.0, sfx_kind=_nat.ProceduralSfxKind.Contact, is_test_tone=is_test_audio))

    overlaps = sb.check_overlaps()
    if overlaps:
        msgs = "; ".join(w.message for w in overlaps)
        raise RuntimeError(f"Layout overlap/safe-area violation in chemistry demo: {msgs}")

    return sb
