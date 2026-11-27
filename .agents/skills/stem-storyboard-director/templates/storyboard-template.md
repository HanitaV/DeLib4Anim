# Storyboard: [Tiêu đề video / demo]

## Metadata
- **Tier:** [Tier 1: Long-Form Beat Sheet / Tier 2: stem_engine Production Shot Spec]
- **Backdrop Mode:** [Classical Starfield / Quantum Void Studio]
- **Resolution & FPS:** 1920x1080 @ 60fps
- **Source Script:** [path/to/script.md]

## Semantic Color Lock
- `#F3F5F8` Primary Ink / Velocity / Normal Force / Axes
- `#59C975` Gravity / Acceleration / Active Cause
- `#EB5E55` Friction / Resistance / Limit
- `#F2C94C` Displacement / Angle / Highlight
- `#999FAB` Muted Subtitles / Inactive Context

---

## Shot & Beat Table

| Shot | Time (s) | Visual & Layout (1920x1080) | Motion & Camera | Spoken VO | Sync & SFX |
| :--- | :--- | :--- | :--- | :--- | :--- |
| Shot 1 | 0.0-5.0 | [Backdrop + hero elements + screen labels <=5 words] | [Camera pose/move, reveal/morph timing, dt=1/240s physics note] | "[Exact spoken segment]" | Sync: "[cue phrase]" -> [action]; SFX: [Chime/Click/Contact @ t] |
| Shot 2 | 5.0-11.0 | [Layout zone + FormulaCard + dimmed context] | [Camera pan/zoom, easing, breathing hold] | "[Exact spoken segment]" | Sync: "[cue phrase]" -> [action]; SFX: [kind @ t] |

---

## `stem_engine` Implementation Notes (Tier 2)
- **Physics / Trajectory Precompute:** [ODE / Verlet / RK4 params at `dt = 1/240.0`]
- **Active Windows & SafeArea:** All `add_text` / `add_formula` / `add_formula_card` / `add_numbered_callout` pass `active_window=(t0, t1)` inside `[-888..888] x [-486..486]`.
- **Verification:** Run `validate_storyboard.py`, then confirm `sb.check_overlaps() == []` and render via `python -m stem_engine render`.
