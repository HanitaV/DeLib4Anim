# Build & Setup Guide

## 1. Windows x64 (Primary Target)

### Verified Host Configuration
- **OS**: Windows 11 x64 (`10.0.26300.0`)
- **GPU**: NVIDIA GeForce RTX 3060 Laptop GPU (Driver `621.40`, CUDA Driver `13.5`, NVENC H.264)
- **Primary C++ Compiler (Skia + `nvcc`)**: Portable **MSVC `14.44.35207`** (`cl.exe 19.44.35230` x64) + **Windows 11 SDK `10.0.26100.0`** (`.stem_cache/toolchains/msvc/`)
- **Secondary C++ Compiler**: WinLibs MinGW-W64 UCRT64 GCC `16.2.0` (`x86_64-ucrt-posix-seh`)
- **CUDA & Profiling Toolchain**: NVIDIA CUDA `13.5.0` (`nvcc V13.5.37`, `compute-sanitizer 13.5.25`, `nsys 2026.5.2.323`, `ncu 2026.4.0.0`) in `.stem_cache/toolchains/cuda/`
- **Skia Ganesh SDK**: `JetBrains/skia-pack` `m144-e2e6623374-4` (Windows x64 Release, SHA256 `50869b098a4f72f5843687093a1a8fdd94e5d61d98397024bb4e521227fedcba`) in `.stem_cache/toolchains/skia/`
- **Build & Python Tools**: `CMake 4.4.4`, `Ninja 1.13.2`, `uv 0.13.9`, `Python 3.12.16`, `Node.js v26.13.0`, `ruff`, `ty`, `pytest`, `pytest-cov`

### Pinned Dependencies & Provenance
Managed via [`tools/setup_toolchains.py`](../tools/setup_toolchains.py), [`cmake/Dependencies.cmake`](../cmake/Dependencies.cmake) + [`tools/fetch_deps.py`](../tools/fetch_deps.py) (SHA256-verified and cached under `.stem_cache/`), and [`uv.lock`](../uv.lock):
- **GLFW**: `3.4` (`refs/tags/3.4`, SHA256 `c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01`)
- **FFmpeg nv-codec-headers**: `n13.1.15.0` (commit `0a6fba9a2820628b8103464f4c8753ee05838baa`, SHA256 `1d2070546de622fd6074a99d4b283e727988b7c3624ef85f97b88962264314d2`)
- **FFmpeg Win64 Shared SDK**: `ffmpeg-n8.1.3-41-ga8029b6528-win64-lgpl-shared-8.1.zip` (tag `autobuild-2026-11-26-12-56`, SHA256 `372016b1248544b4ffc290ae41c9c57e6441bd0da32068ea18032a25b7b36e08`)
- **stb_truetype**: `v1.26` (`nothings/stb` commit `f58f558c120e9b32c217290b80bad1a0729fbb2c`, SHA256 `ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab`)
- **CUDA Kernel PTX Compiler**: `nvcc V13.5.37` (or `nvidia-cuda-nvrtc==13.5.37` fallback) via [`tools/compile_cuda_ptx.py`](../tools/compile_cuda_ptx.py)
- **Skia Ganesh/OpenGL**: `JetBrains/skia-pack` `m144-e2e6623374-4` (enabled by default in `pyproject.toml` and `windows-msvc-skia` preset)
- **MathJax Offline Formula Compiler**: `mathjax-full@3.2.2` bootstrapped automatically via `npm` into `.stem_cache/mathjax_tool/` on first formula compile (no TeX Live required).

### Build & Test Commands (Windows)

```powershell
# 1. Sync Python build & dev environment and install/activate user-space MSVC + CUDA 13.5 + Skia toolchains
uv sync --all-groups --no-build-isolation
.venv\Scripts\python.exe tools\setup_toolchains.py
. .\scripts\activate_toolchains.ps1

# 2. Build & test native C++20 CLI + tests with MSVC + Skia Ganesh + nvcc (Release)
cmake --preset windows-msvc-skia
cmake --build --preset windows-msvc-skia
ctest --preset windows-msvc-skia --output-on-failure

# (Optional) Build & test MinGW-W64 GCC + nvcc non-Skia target
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure

# 3. Build & install pybind11 Python extension (_stem_native with MSVC + Skia + nvcc)
$env:CMAKE_GENERATOR = "Ninja"
uv sync --all-groups --no-build-isolation --reinstall-package stem-engine

# 4. Verify Python tests, type checks, linting & environment doctor
uv run --no-sync ruff check src examples tests tools
uv run --no-sync ty check src
uv run --no-sync pytest -v
uv run --no-sync stem-engine doctor
```

### Hybrid / Optimus Laptop GPU Selection
On dual-GPU Windows laptops (e.g. AMD/Intel iGPU + NVIDIA dGPU):
- Native `.exe` targets export `NvOptimusEnablement = 0x00000001` and `AmdPowerXpressRequestHighPerformance = 1`.
- Inside `python.exe`, [`src/stem_engine/__init__.py`](../src/stem_engine/__init__.py) and [`native/render/gl_context.cpp`](../native/render/gl_context.cpp) set `SHIM_MCCOMPAT=0x800000001` **before** `opengl32.dll` / `glfwInit()` loads so OpenGL binds to the NVIDIA GPU and `cuGLGetDevices` succeeds in-process.

### Compute Sanitizer WDDM Prerequisite
`nvcc`, `compute-sanitizer`, `nsys`, and `ncu` are installed under `.stem_cache/toolchains/cuda/`. On Windows WDDM, `compute-sanitizer` kernel instrumentation requires running `.stem_cache\toolchains\cuda\compute-sanitizer\EnableDebuggerInterface.bat` once from an Administrator prompt (`HKLM\SOFTWARE\NVIDIA Corporation\GPUDebugger\EnableInterface = 1`).

## 2. Linux Support Status
[`CMakeLists.txt`](../CMakeLists.txt) and [`native/interop/cuda_drv.cpp`](../native/interop/cuda_drv.cpp) include Linux support paths (`pkg-config` for `libavutil/libavcodec/libavformat/libswresample`, `OpenGL::GL`, `libcuda.so.1` via `dlopen`). **Note**: Linux builds and EGL/GLX headless contexts have **not** been executed or verified in this Windows session.
