---
name: stem-storyboard-director
description: >-
  Transforms an approved STEM video script, chapter, or short concept into a Median
  dark-studio audio-visual storyboard, shot list, SRT/cue timing manifest, and stem_engine
  (SceneBuilder) shot plan. Use when asked to storyboard a STEM video, design visual shots
  and camera moves from a script, map narration to 1920x1080 scenes, generate .srt or
  cues.json manifests, or plan a physics/chemistry animation for DeLib4Anim/stem_engine
  ("thiết kế storyboard", "phân cảnh video STEM", "chia shot theo kịch bản", "dựng cảnh
  stem_engine"). Do NOT use for drafting pure storytelling prose or rewriting spoken hooks
  — route narrative scriptwriting to `stem-video-scriptwriter`.
---

# STEM Storyboard Director (Median Dark-Studio & `stem_engine`)

Translate spoken STEM scripts into uncluttered, dark-studio visual choreography where every camera move, vector, curve, and equation earns its screen time.

**Phase Boundary:** Narrative prose and open loops belong in `stem-video-scriptwriter`. This skill owns **visual state transitions**, **shot timing**, **spatial layout on 1920×1080**, **audio/SFX sync**, and **`stem_engine` (`SceneBuilder`) primitive mapping**.

## Select Output Tier

Choose the tier matching the deliverable:

- **Tier 1 — Long-Form Audio-Visual Beat Sheet** (for 5–30 min YouTube scripts):
  - Break chapters into `10–25s` visual beats.
  - Specify backdrop mode, foreground subject, visual state change, camera framing, key on-screen text (`<= 5` words) or LaTeX card, sync phrase, and music/SFX cue.
- **Tier 2 — `stem_engine` Production Shot Spec** (for `15–90s` rendered MP4s or chapter showcases like [examples/incline.py](../../../examples/incline.py) and [examples/chemistry_electron.py](../../../examples/chemistry_electron.py)):
  - Break into `3–8s` shots with exact `[t0, t1]` timestamps, `1920×1080` centered coordinates inside `SafeArea`, exact `SceneBuilder` primitive calls, precomputed `dt = 1/240s` physics/trajectory notes, `active_window` overlap guards, and `AudioCue` SFX events.

## Core Direction Principles

Read [./references/median-visual-grammar.md](./references/median-visual-grammar.md) for visual tokens and [./references/stem-engine-mapping.md](./references/stem-engine-mapping.md) for exact `SceneBuilder` APIs before authoring a storyboard.

1. **One Visual Claim Per Shot (No Slide Clutter):** Never dump 4 unrelated diagrams on screen at once. Each shot answers one physical question or performs one transformation.
2. **Phenomenon & Geometry Before Algebra:** Animate the physical object, vector, or wave `0.4–1.0s` *before* revealing its label or formula card.
3. **Transform, Don't Replace:** Maintain spatial continuity. Pan/zoom (`sb.camera.move_to`), morph point clouds (`OrbitalCloud.morph`), or dim older derivation lines (`Formula.dim_to(0.35)`) instead of cutting abruptly to blank screens.
4. **Separate Narration from On-Screen Text:** Never copy full spoken sentences onto the canvas. Keep on-screen text to crisp labels (`<= 5` words) or compact MathJax cards; let subtitles carry the spoken stream.
5. **Scientific vs. Presentation State Separation:** Derive trajectories, vector magnitudes, and plot curves from deterministic fixed-timestep physics (`dt = 1/240s`); apply squash, glow, and camera easing strictly to presentation items.
6. **Mandatory Breathing Room & Purposeful SFX:** Hold major visual reveals (`FormulaCard`, graph traces, measurement collapse) for `0.8–2.0s` before transitioning. Anchor state changes with subtle `AudioCue` SFX (`Chime`, `Click`, `Contact` at `gain = 0.12–0.20`).
7. **Layout & Overlap Discipline:** Keep every `screen_space=True` text, formula, and badge inside `SafeArea` (`x in [-888, 888]`, `y in [-486, 486]`) and always declare `active_window=(t0, t1)` so `sb.check_overlaps()` catches collisions before rendering.

## 5-Step Storyboard Workflow

1. **Lock Visual Palette & Backdrop:** Pick either **Classical Starfield** (Newtonian mechanics, astronomy, kinematics) or **Quantum Void Studio** (atomic/wave/micro-scale with optional 3D perspective floor grid). Lock semantic colors (`#F3F5F8` primary/normal, `#59C975` gravity/accel, `#EB5E55` friction/resist, `#F2C94C` angle/displacement/highlight, `#999FAB` muted context).
2. **Segment Script into Shots/Beats:** Align shot boundaries to natural clause/pause boundaries (`[.]`, `[..]`, `[...]`, `[BEAT]`). Estimate duration from VO word count (`~2.3–2.4 words/sec`) plus `0.5–1.5s` visual absorption holds.
3. **Design Spatial Zones & Transitions:** Use [./templates/storyboard-template.md](./templates/storyboard-template.md). Map each shot to one of Median's 8 signature compositions (Center Stage, Top Formula Card + Lower Diagram, Right-Column Derivation Stack, Split-Screen System + Live Plot, Viewfinder Framing, Sequential Numbered Callouts, Stroboscopic Motion Trail, Perspective Floor Grid).
4. **Map to `stem_engine` Primitives & Audio Cues (Tier 2):** Annotate `SceneBuilder` calls (`add_box`, `add_vector`, `add_vector_label`, `add_graph`, `add_formula_card`, `add_orbital_cloud`, `add_numbered_callout`, `add_viewfinder_brackets`, `add_stickman`), trajectory sampling, and `AudioCue` timestamps.
5. **Validate Timing & Export Manifests:** Run the deterministic validator:

```powershell
.venv\Scripts\python.exe .agents/skills/stem-storyboard-director/scripts/validate_storyboard.py <storyboard.md> --mode shot --export-srt <out.srt> --export-cues <cues.json>
```

For Tier 1 long-form beat sheets (`10–45s` beats), pass `--mode beat`. Fix any timeline gaps/overlaps, negative durations, or excessive VO density (`> 3.2 words/sec`) before handing off to Python scene coding.

## Pass-Threshold Gates

1. Timeline is contiguous and strictly monotonic (`t1 > t0`, no unintended gaps or overlaps > `0.15s`).
2. Every shot/beat states both **Visual & Layout** and **Motion & Camera** (zero static PowerPoint slides).
3. Spoken VO density stays breathable (`<= 3.2 words/sec` per shot) with post-reveal hold windows.
4. Semantic colors are locked consistently across all shots; on-screen labels are `<= 5` words.
5. `validate_storyboard.py` passes with `0` errors; any generated `stem_engine` Python file passes `sb.check_overlaps() == []` and renders cleanly via `python -m stem_engine render ...`.
6. Reference worked example consulted when needed: [./examples/sample-storyboard-con-lac.md](./examples/sample-storyboard-con-lac.md).
