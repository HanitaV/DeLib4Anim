# Performance & Benchmark Report

## 1. Test Environment
- **GPU**: NVIDIA GeForce RTX 3060 Laptop GPU (6144 MiB GDDR6, Ampere SM 8.6, 1x 7th-gen NVENC)
- **Driver / CUDA**: NVIDIA WDDM Driver `621.40` (`32.0.16.2140`), CUDA Driver `13.5` (`13050`)
- **OpenGL**: `4.5.0 NVIDIA 621.40` (`GL_VENDOR = NVIDIA Corporation`, 4x MSAA offscreen FBO)
- **OS / Compiler**: Windows 11 x64 (`10.0.26300.0`), GCC `16.2.0` (`Release`, `-O3 -DNDEBUG`)
- **Codec Settings**: `h264_nvenc`, `AV_PIX_FMT_CUDA` (`NV12`), ITU-R BT.709 Limited Range, `preset=p4`, `tune=hq`, `profile=high`, `cq=19`, `max_b_frames=2`, `rc_lookahead=0`, 48 kHz stereo AAC audio
- **Methodology**: 1 warmup run + 3 timed runs per workload (`--runs 3`); async `GL_TIME_ELAPSED` queries and async `CUevent` timers (zero `glFinish()` / `cudaDeviceSynchronize()` per frame solely for timing); `raw_frame_readbacks == 0` verified across all runs.

## 2. Measured Results: Baseline (`batch=1`) vs. Batch-Amortized Interop (`batch=8`)

Command used:
```powershell
.venv\Scripts\python.exe -m stem_engine benchmark --suite baseline --label before_batch1 --interop-batch 1 --runs 3 --include-4k --demo-script examples/incline.py --output outputs/benchmark_before.json --csv outputs/benchmark_before.csv
.venv\Scripts\python.exe -m stem_engine benchmark --suite baseline --label after_batch8 --interop-batch 8 --runs 3 --include-4k --demo-script examples/incline.py --output outputs/benchmark.json --csv outputs/benchmark.csv
```

| Workload | Resolution | Frames | Before (`batch=1`) FPS | After (`batch=8`) FPS | Realtime Factor | GL GPU Avg (ms) | CUDA NV12 Avg (ms) | Frame Wall Avg / p95 (ms) | Raw Readbacks |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `1_static_glyph_path_1080p60` | 1920x1080 | 600 | 283.96 | 281.10 | 4.69x | 0.181 -> **0.123** | 0.039 -> **0.036** | 2.585 / 2.659 | 0 |
| `2_dynamic_curves_stickman_1080p60` | 1920x1080 | 600 | 292.13 | 291.54 | 4.86x | 0.179 -> **0.124** | 0.038 -> **0.037** | 2.479 / 2.540 | 0 |
| `3_particles_compositing_1080p60` | 1920x1080 | 600 | 289.68 | 289.62 | 4.83x | 0.259 -> **0.130** | 0.037 -> **0.038** | 2.490 / 2.545 | 0 |
| `4_demo_render_only_1080p60` | 1920x1080 | 1440 | 2520.00 | **2576.58** | **42.94x** | 0.149 -> 0.151 | 0.000 | **0.023** / 0.012 | 0 |
| `5_demo_full_export_1080p60` | 1920x1080 | 1440 | 329.86 | **332.29** | **5.54x** | 0.201 -> **0.178** | 0.037 -> **0.037** | 2.517 / **2.572** | 0 |
| `6_demo_full_export_4k60` | 3840x2160 | 1440 | 96.26 | **97.24** | **1.62x** | 0.787 -> **0.536** | 0.098 -> **0.098** | 9.676 / **9.882** | 0 |

Additional measured metrics on `5_demo_full_export_1080p60` (`after_batch8`):
- **Cold startup**: `436.74 ms` (GLFW/OpenGL 4.5 init + shader compile + CUDA context + NVENC session + MP4 header)
- **Scene asset compile** (MathJax SVG paths + TrueType glyph atlas + static graph VBOs): `5.39 ms`
- **Warm CPU scene evaluation**: `0.003 ms` avg / `0.010 ms` p95
- **Finalize & MP4 `+faststart` flush**: `242.11 ms`
- **Peak VRAM delta**: `1339.9 MiB` (`1080p60`), `2334.5 MiB` (`4K60`)
- **Output file size**: `9,535,974 bytes` (24s 1080p60, identical bit-exact video payload across `batch=1` and `batch=8`)

## 3. Bottleneck Diagnosis & Optimization Findings
1. **Renderer & Color Conversion Are Fast (< 10% of frame budget)**:
   - OpenGL 4.5 4x MSAA rendering + procedural starfield/matte/book/contact-glow shaders + Non-Zero Winding MathJax formulas runs at **`2,576.58 fps`** (`0.151 ms` GPU time per 1080p frame).
   - CUDA `RGBA8 -> BT.709 NV12` kernel executes in **`0.037 ms`** at `1080p` and **`0.098 ms`** at `4K`.
2. **Negative Result — Cross-Thread WDDM Interop Contention**:
   - An intermediate experiment moving `avcodec_send_frame` to a second `std::jthread` with `cuCtxPushCurrent` while pre-submitting `render(f+1)` before `cuGraphicsMapResources(f)` **degraded** performance (`CUDA` interop latency jumped from `0.037 ms` to `0.45–3.03 ms` and demo export dropped to `243.9 fps`) due to Windows WDDM CUDA context lock contention and `cuGraphicsMapResources` fencing all pending GL draws. That approach was reverted.
3. **Batch-Amortized GL–CUDA Interop (`batch=8`)**:
   - Keeping a single host thread on the unified CUDA primary context while mapping/converting/unmapping `8` GL textures in a single `cuGraphicsMapResources(8)` / `cuGraphicsUnmapResources(8)` pair reduced GL GPU frame time (`0.201 -> 0.178 ms` at 1080p; `0.787 -> 0.536 ms` at 4K) and lowered p95 frame wall latency (`2.732 -> 2.572 ms` at 1080p; `10.443 -> 9.882 ms` at 4K).
4. **True Ceiling Identified — Single-Chip NVENC (`preset=p4 / tune=hq`)**:
   - Frame wall time scales **3.84x** from `1080p` (`2.517 ms`) to `4K` (`9.676 ms`)—matching the `4.0x` pixel-count ratio of the RTX 3060 Laptop's single NVENC hardware encoder ASIC at `preset=p4 / tune=hq / b_frames=2`.
   - Even at `p4/hq`, the engine exports **5.54x realtime at 1080p60** (`332.29 fps`) and **1.62x realtime at 4K60** (`97.24 fps`). Users needing even faster draft exports can select `nvenc_preset="p1"` via `ExportOptions`.

## 4. Profiler & Sanitizer Tooling Status
- Installed under `.stem_cache/toolchains/cuda/`: `nvcc` (`V13.5.37`), `compute-sanitizer` (`13.5.25`), Nsight Systems (`nsys 2026.5.2.323`), and Nsight Compute (`ncu 2026.4.0.0`).
- Verification run on this host (`IsAdmin = False`):
  - `nsys --version` and `ncu --version` execute cleanly.
  - `compute-sanitizer` (`memcheck`, `initcheck`, `synccheck`) against `.\build\msvc-skia\stem_native_tests.exe` executed all native tests cleanly (`0 failure(s)`), but kernel-level WDDM debugger attachment requires running `.stem_cache\toolchains\cuda\compute-sanitizer\EnableDebuggerInterface.bat` once from an Administrator shell (`HKLM\SOFTWARE\NVIDIA Corporation\GPUDebugger\EnableInterface = 1`).
- Once enabled as Administrator, run correctness checks and profiling with:
  ```powershell
  . .\scripts\activate_toolchains.ps1
  compute-sanitizer --tool memcheck --error-exitcode 1 .\build\msvc-skia\stem_native_tests.exe
  compute-sanitizer --tool initcheck --error-exitcode 1 .\build\msvc-skia\stem_native_tests.exe
  compute-sanitizer --tool synccheck --error-exitcode 1 .\build\msvc-skia\stem_native_tests.exe
  nsys profile -o outputs/trace_incline .\build\msvc-skia\stem_engine_cli.exe milestone1
  ```
