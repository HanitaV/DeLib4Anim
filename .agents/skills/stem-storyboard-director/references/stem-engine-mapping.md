# `stem_engine` Primitive Mapping Reference

Exact API contract verified against `src/stem_engine/authoring.py` and `src/stem_engine/cli.py`. Use this when converting Tier 2 storyboards into runnable `examples/*.py` scenes.

---

## 1. Scene Setup & Camera

```python
from stem_engine import AudioCue, SceneBuilder
from stem_engine import _stem_native as _nat

sb = SceneBuilder(width=1920, height=1080, duration_sec=26.0)
sb.set_background(
    top_color=(0.014, 0.015, 0.018, 1.0),
    bottom_color=(0.008, 0.009, 0.011, 1.0),
    show_grid=False,
    starfield=1.50,
    vignette=0.22,
    grain=0.004,
)
```

- **Static / Pan / Zoom Camera:**
  - `sb.camera.set_pose(pos=(0.0, 0.0), zoom=1.0, rotation_rad=0.0)`
  - `sb.camera.move_to(t0, t1, pos0, pos1, zoom0=1.0, zoom1=1.0, easing=_nat.EasingType.QuinticInOut)`
  - `sb.camera.follow_trajectory(traj_id, t0, t1, offset=(0.0, 0.0), window_sec=0.16)`

---

## 2. Storyboard Element -> `SceneBuilder` Primitive Map

| Storyboard Visual Element | `SceneBuilder` Method | Key Parameters & Animation Methods |
| :--- | :--- | :--- |
| **Physics Block / Book / Beam** | `sb.add_box(obj_id, center, size, ...)` | `material`: `"solid"`, `"matte"`, `"book"`, `"glow"`; `trajectory_id`, `corner_radius`, `rotation_rad`; `.reveal(t0, t1)`, `.hide(t0, t1)`, `.move_to(...)`, `.squash_contact(t_contact)` |
| **Bob / Nucleon / Dot / Electron** | `sb.add_circle(obj_id, center, radius, ...)` | `material`: `"solid"`, `"sphere3d"`, `"glow"`; `trajectory_id`; `.reveal(t0, t1)`, `.hide(t0, t1)`, `.move_to(...)` |
| **Force / Velocity / Axis Arrow** | `sb.add_vector(obj_id, origin, delta, ...)` | `color`, `shaft_width`, `head_length`, `head_width`, `attach_trajectory_id`, `attach_offset`; `.reveal(t0, t1)`, `.hide(t0, t1)` |
| **Median Vector Badge** | `sb.add_vector_label(obj_id, symbol, pos, ...)` | `subscript`, `subtitle`, `symbol_color`, `subtitle_color`, `trajectory_id`, `active_window`; `.reveal(t0, t1)`, `.move_by(t0, t1, d0, d1)`, `.hide(t0, t1)` |
| **Curves, Axes, Dashed Guides, Waves** | `sb.add_graph(obj_id, points, ...)` | `dashed=(7.0, 6.0)`, `fill_under=True`, `fill_top_color`, `fill_bottom_color`, `envelope_sigma`, `trail_trajectory_id`, `trail_duration_sec`; `.reveal(t0, t1)` (arc-length draw-on), `.hide(t0, t1)` |
| **Animated Wave Carrier** | `sb.add_graph(...)` + `.native.deform_*` | Set `g.native.deform_freq`, `g.native.deform_speed`, `g.native.deform_amplitude.add_segment(...)` |
| **3D Hydrogen Orbital Cloud** | `sb.add_orbital_cloud(obj_id, ...)` | `n_a, l_a, m_a, bohr_a, n_b, l_b, m_b, bohr_b, particle_count, flow_speed, collapse_target`; `.reveal()`, `.morph(t0, t1, 0, 1)`, `.orbit_camera()`, `.collapse()`, `.hide()` |
| **LaTeX Equation** | `sb.add_formula(obj_id, tex, pos, ...)` | `target_height_px`, `color`, `screen_space=True`, `active_window=(t0, t1)`; `.reveal()`, `.dim_to(t0, t1, 0.35, 1.0)`, `.hide()` |
| **Boxed Multi-Color Formula** | `sb.add_formula_card(obj_id, tex, pos, ...)` | Supports `\textcolor{#59C975}{...}`, `stroke_color`, `pad_x`, `pad_y`, `active_window`; `.reveal()`, `.dim_to()`, `.hide()` |
| **Labels & Titles** | `sb.add_text(obj_id, text, pos, ...)` | `font_size_px`, `font_variant` (`regular`/`semibold`/`italic`/`semibold_italic`/`bold_italic`), `align_h` (`-1` left, `0` center, `1` right), `active_window` |
| **Numbered Rule Callouts (1, 2, 3)** | `sb.add_numbered_callout(obj_id, num, text, pos, ...)` | `.activate(t0, t1)`, `.dim(t0, t1)`, `.hide(t0, t1)` |
| **4-Corner Framing Brackets** | `sb.add_viewfinder_brackets(obj_id, center, size, ...)` | `.reveal(t0, t1)`, `.hide(t0, t1)` |
| **Stickman Actor** | `sb.add_stickman(obj_id, hip_pos, ...)` | `.point_at(t0, t1, target_world)`, `.release_action(t_release)`, `.follow_through_react(t_contact)` |
| **Precomputed Physics Path** | `sb.add_trajectory(samples, t_start=0.0, dt=1/240.0)` | Returns `traj_id` attached to `Body`, `Vector`, `Graph` trail, or `Camera.follow_trajectory` |

---

## 3. Cues, Audio Events & Overlap Verification

```python
sb.add_cue("cue_shot_1", 0.0, "Equilibrium and release")
sb.add_audio_event(
    AudioCue(
        "sfx_release",
        time_sec=2.40,
        duration_sec=0.16,
        gain=0.15,
        pitch_hz=523.2,
        sfx_kind=_nat.ProceduralSfxKind.Click,  # Chime | Click | Contact
        is_test_tone=not bool(voice_wav_path),
    )
)

overlaps = sb.check_overlaps()
if overlaps:
    raise RuntimeError("; ".join(w.message for w in overlaps))
```

- **Excalidraw Layout Import:** `anchors = load_excalidraw_anchors("storyboard.excalidraw")` maps Excalidraw boxes into engine-centered `LayoutBox` coordinates.
- **Preview & Render Commands:**
  - Preview: `.venv\Scripts\python.exe -m stem_engine preview <scene.py> --fps 60`
  - Export MP4: `.venv\Scripts\python.exe -m stem_engine render <scene.py> --output outputs/<name>.mp4 --width 1920 --height 1080 --fps 60`
