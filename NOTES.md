# Engineering Notes & Environment Decisions (`NOTES.md`)

## 1. Environment Survey Summary (2026-11-27)
- **OS**: Windows 11 Native (`Microsoft Windows NT 10.0.26300.0`, x86_64).
- **GPUs**:
  - Primary dGPU: **NVIDIA GeForce RTX 3060 Laptop GPU** (6144 MiB VRAM, Ampere SM 8.6, WDDM Driver `621.40` / `32.0.16.2140`, CUDA Driver API `13.5` / `13050`).
  - Secondary iGPU: **AMD Radeon(TM) Graphics** + **MuMuPlayer Virtual Display Adapter** (Hybrid/Optimus configuration).
- **OpenGL & CUDA Co-location (Critical Hybrid-GPU Finding)**:
  - A standalone `.exe` exporting `NvOptimusEnablement = 0x00000001` and `AmdPowerXpressRequestHighPerformance = 1` binds OpenGL `4.6.0 NVIDIA 621.40` directly to `NVIDIA GeForce RTX 3060 Laptop GPU/PCIe/SSE2` — matching CUDA `dev[0]`.
  - By contrast, `python.exe` does **not** export `NvOptimusEnablement`, so loading `opengl32.dll` inside `python.exe` defaults to `AMD Radeon(TM) Graphics`, which would break OpenGL–CUDA graphics interop!
  - **Solution verified**: Setting `SHIM_MCCOMPAT=0x800000001` in process environment *before* `opengl32.dll` loads routes `python.exe` OpenGL contexts onto `NVIDIA GeForce RTX 3060 Laptop GPU/PCIe/SSE2`, enabling zero-IPC in-process `pybind11` batch rendering on the NVIDIA GPU. Both `stem_engine/__init__.py` and native entrypoints enforce this and verify `GL_VENDOR` + `cuGLGetDevices` at runtime.
- **Compilers & Build Tools**:
  - `CMake 4.4.4`, `Ninja 1.13.2`, `uv 0.13.9`, `Node.js v26.13.0`, `Python 3.12.16` (and `3.11.9`, `3.15.2`).
  - **MSVC + Windows 11 SDK (User-Space Toolchain)**: Installed via `tools/setup_toolchains.py` into `.stem_cache/toolchains/msvc/` (`MSVC 14.44.35207` / `cl.exe 19.44.35230` x64 + `Windows 11 SDK 10.0.26100.0` + `vcvars*.bat` wrappers for `nvcc`).
  - **MinGW-W64 Host Compiler**: WinLibs **MinGW-W64 UCRT64 GCC 16.2.0** (`x86_64-ucrt-posix-seh`), pinned in `CMakePresets.json` (`windows-release`, `windows-relwithdebinfo`).
- **CUDA Toolkit & Sanitizers / Profilers**:
  - `nvcuda.dll` (CUDA 13.5 Driver API) and `nvEncodeAPI64.dll` (NVENC) are installed in `System32` and operational.
  - **CUDA 13.5.0 Redistributable Toolchain**: Installed via `tools/setup_toolchains.py` into `.stem_cache/toolchains/cuda/` (`nvcc V13.5.37`, `cuda_crt`, `cuda_cudart`, `cccl`, `libnvvm`, `compute-sanitizer 13.5.25`, `nsys 2026.5.2.323`, `ncu 2026.4.0.0`).
  - **CUDA Kernel Compilation Strategy**:
    1. `tools/compile_cuda_ptx.py` auto-discovers `nvcc` (from `PATH`, `CUDA_PATH`, or `.stem_cache/toolchains/cuda/bin/`) and compiles `native/interop/color_convert.cu` via `nvcc -ptx -lineinfo` (passing `-ccbin` + `-allow-unsupported-compiler` with `msvc_env.json` on Windows).
    2. When `nvcc` is absent on another host, it falls back cleanly to NVIDIA's official `nvidia-cuda-nvrtc==13.5.37` library, loading the PTX via `cuModuleLoadDataEx` on the unified `CUcontext`.
    3. Compute Sanitizer (`memcheck`/`initcheck`/`synccheck`) is installed; on Windows WDDM it requires running `.stem_cache\toolchains\cuda\compute-sanitizer\EnableDebuggerInterface.bat` once as Administrator (`HKLM\SOFTWARE\NVIDIA Corporation\GPUDebugger\EnableInterface = 1`) before kernel-level WDDM instrumentation attaches.
- **FFmpeg & Skia Provenance**:
  - System PATH has `Gyan.FFmpeg 9.0.2` CLI (`bin/` only, no headers/import libs).
  - CMake fetches pinned LGPL shared FFmpeg `8.1.3` (`FFmpeg-Builds` immutable tag `autobuild-2026-11-26-12-56`, SHA256 `372016b1248544b4ffc290ae41c9c57e6441bd0da32068ea18032a25b7b36e08`) when `FFMPEG_ROOT` is not externally supplied.
  - **Skia Ganesh/OpenGL (`JetBrains/skia-pack` `m144-e2e6623374-4`)**: Installed into `.stem_cache/toolchains/skia/` (SHA256 `50869b098a4f72f5843687093a1a8fdd94e5d61d98397024bb4e521227fedcba`). Because `skia.lib` uses MSVC static CRT (`MT_StaticRelease` `/MT`), `CMakeLists.txt` sets `CMAKE_MSVC_RUNTIME_LIBRARY` to `MultiThreaded` when `MSVC AND STEM_ENABLE_SKIA`, links `d3d12`/`d3dcompiler`/`dxgi` + Win32 system libs, and `native/text/skia_backend.cpp` initializes Ganesh via `GrGLInterfaces::MakeWin()` on `_WIN32`.
  - Both `pyproject.toml` (`_stem_native` Python extension) and CMake preset `windows-msvc-skia` enable `STEM_ENABLE_SKIA=ON` with MSVC + `nvcc`; `windows-release` keeps the MinGW-W64 non-Skia native GPU SVG/TrueType build working as a secondary target.

## 2. Key Architectural Invariants
- **Single Unified CUDA Context**: Created after GLFW OpenGL 4.5 context initialization via `cuGLGetDevices` -> `cuDevicePrimaryCtxRetain`, then injected into FFmpeg `AVHWDeviceContext` (`AVCUDADeviceContext::cuda_ctx` and `stream`).
- **Zero Raw-Frame CPU Readback in Production**: OpenGL renders into MSAA FBO -> resolves via `glBlitNamedFramebuffer` into `GL_RGBA8` texture -> mapped via `cuGraphicsMapResources` / `cuGraphicsSubResourceGetMappedArray` -> `CUsurfObject` -> CUDA kernel converts vertically-flipped OpenGL `RGBA8` to ITU-R BT.709 Limited-Range `NV12` directly into `AVHWFramesContext` (`AV_PIX_FMT_CUDA`) pitched device buffers -> `avcodec_send_frame` (`h264_nvenc`) -> MP4 muxer.
- **Surface Object Lifetime Safety**: `cuSurfObjectDestroy` is not stream-ordered; surface descriptors are retained per ring-buffer slot until that slot's `CUevent` signals completion before reuse or teardown.
- **WDDM Interop Threading & Batching**: Cross-thread `cuCtxPushCurrent` between GL/CUDA producer and NVENC worker contended on WDDM locks (`CUDA` latency spiked from `0.037 ms` to `0.45–3.03 ms`). Keeping a single unified CUDA thread with batch-amortized `cuGraphicsMapResources(8)` / `cuGraphicsUnmapResources(8)` preserves `0.037 ms` CUDA kernel time and achieves `332.29 fps` (1080p60) / `97.24 fps` (4K60) with `0` raw readbacks.
