# Roadmap

## Completed (Milestones 1–4)
- **Milestone 1**: CMake/Ninja C++20 native runtime, OpenGL 4.5 Core loader & offscreen 4x MSAA renderer, CUDA Driver API GL interop (`cuGLGetDevices` + BT.709 Limited-Range `NV12` kernel), `AV_PIX_FMT_CUDA` + `h264_nvenc` + AAC MP4 muxer, orientation/color calibration tests (`max_abs_diff = 0`).
- **Milestone 2**: `pybind11` + `scikit-build-core` extension (`_stem_native`), typed Python authoring API (`SceneBuilder`, `Camera`, `Body`, `Vector`, `Graph`, `Stickman`, `Text`, `Formula`, `AudioCue`), offline `mathjax-full@3.2.2` SVG cache, Non-Zero Winding Stencil-then-Cover path rendering, TrueType glyph atlas, Excalidraw anchor loader, SafeArea/overlap checks, native VSync preview, and `python -m stem_engine` CLI.
- **Milestone 3**: 24-second 1080p60 Inclined Plane STEM demo (`examples/incline.py`) with Median-inspired starfield/matte/book/contact-glow shaders, solid depth-shaded stickman, dashed projection/reference guides, proportional FBD vectors, MathJax focus dimming, kinematics curves, and procedural SFX track.
- **Milestone 4**: 5-workload + 4K benchmark suite, WDDM interop bottleneck diagnosis, batch-amortized GL–CUDA interop optimization, and measured before/after reports.

## Next Milestones
1. **MSVC + Skia Ganesh CI Target**: Validate `STEM_ENABLE_SKIA=ON` against `JetBrains/skia-pack` on an MSVC toolchain host and add variable-stroke/brush path effects.
2. **Controlled Token-Group Equation Morphing**: Group MathJax SVG `<g data-mml-node>` subtrees into declared token IDs for smooth crossfade/reposition transitions across algebraic derivations.
3. **Voiceover / TTS Cue Re-timing**: Auto-recompute shot cue timestamps when user-supplied voice WAV clips or cue JSON durations change.
4. **Domain Extensions (Post-MVP)**:
   - Chemistry diagrams via RDKit SVG/path bridge.
   - Vector/scalar field visualization via CUDA/Warp/Taichi GPU simulation buffers mapped directly into instanced OpenGL arrow/streamline draws.
   - Linux headless EGL + CUDA verification.
