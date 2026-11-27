# Engine Architecture

## Dataflow Overview

See also: [Animation Physics & Layered 2D / 2.5D Design Specification](animation_physics_and_25d_design.md).

```text
Python Authoring (stem_engine.authoring / formula)
  |-- MathJax offline SVG compiler (.stem_cache/formulas/<sha256>.svg)
  |-- LayoutBox / SafeArea / overlap warnings / Excalidraw anchor import
  v (single batch Scene transfer; GIL released for entire render/export loop)
C++20 Retained Scene & Timeline (native/core/scene.hpp/.cpp)
  |-- RationalFPS exact timestamp: t = (frame_index * den) / num
  |-- Stateless Track<T> evaluation (Step, Linear, Smoothstep, QuinticOut, QuinticInOut, Spring)
  |-- Fixed-timestep Trajectory2D (dt = 1/240s) + stateless camera follow & trail lookup
  v
OpenGL 4.5 Core Renderer (native/render/renderer.hpp/.cpp)
  |-- Shared 4x MSAA offscreen FBO (GL_RGBA8 + GL_DEPTH24_STENCIL8)
  |-- Procedural background (starfield parallax, vignette, dither/grain, optional grid)
  |-- Instanced SDF shaders (Rects with solid/matte/book/glow materials, Circles, Capsules)
  |-- Retained Graph strip VBOs (arc-length reveal + shader dashing; re-uploaded only on DirtyGeometry)
  |-- Retained SVG/MathJax PathAssets via Non-Zero Winding Stencil-then-Cover (handles compound holes)
  |-- Retained 2048x2048 GL_R8 TrueType glyph atlas (stb_truetype) + cached TextRun VBOs
  |-- glBlitNamedFramebuffer -> per-slot GL_RGBA8 resolve texture + async GL_TIME_ELAPSED query
  v
CUDA Driver API Interop (native/interop/cuda_drv.hpp/.cpp, color_convert.cu)
  |-- Single unified CUcontext via cuGLGetDevices -> cuDevicePrimaryCtxRetain
  |-- One-time cuGraphicsGLRegisterImage per slot
  |-- Batch-amortized cuGraphicsMapResources(B) -> CU_TRSF_READ_AS_INTEGER CUtexObject -> BT.709 NV12 kernel -> cuGraphicsUnmapResources(B)
  |-- Async CUevent conversion timing
  v
FFmpeg Hardware Encode & Mux (native/encode/nvenc_encoder.hpp/.cpp)
  |-- Shared AVCUDADeviceContext (same CUcontext & CUstream)
  |-- AVHWFramesContext (AV_PIX_FMT_CUDA, sw_format = AV_PIX_FMT_NV12)
  |-- h264_nvenc (BT.709 limited range) + 48 kHz stereo AAC audio -> MP4 (+faststart)
```

## Key Invariants
1. **Zero Raw-Frame CPU Readback**: Production exports and benchmarks (`raw_frame_readbacks == 0`) keep pixels in VRAM from OpenGL FBO through NVENC compressed packet emission. `debug_readback_slot_rgba8` is strictly separated for single-frame verification tests.
2. **Stateless Deterministic Seeking**: `Scene::evaluate(time)` depends purely on `time_sec` and immutable/baked tracks. Evaluating `0 -> 5 -> 2 -> 5` produces an identical `SceneSnapshot::state_fingerprint()`.
3. **Scientific vs. Presentation Separation**: Physical trajectory (`s(t), v(t)`), FBD vector magnitudes/angles, and equations derive from `InclinePhysicsParams` + fixed-dt integration; contact squash/stretch acts strictly on `RectItem::presentation_scale`.
4. **Native Preview Parity**: `preview_scene_native` uses the exact same `Renderer` instance and blits the resolved texture to the window backbuffer with VSync enabled (`glfwSwapInterval(1)`), whereas batch export disables VSync and never swaps buffers.
