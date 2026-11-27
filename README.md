# DeLib4Anim / `stem_engine`

Engine dựng và xuất video hoạt họa khoa học kỹ thuật (STEM) tăng tốc phần cứng GPU trên **NVIDIA + Windows x64**, kết hợp quy trình **AI Agent Skills** viết kịch bản & phân cảnh theo phong cách **Median / 3Blue1Brown**.

Production pipeline **không** dùng trình duyệt, HTML/DOM hay chụp màn hình (`raw_frame_readbacks == 0`). Toàn bộ khung hình đi thẳng từ OpenGL VRAM qua CUDA interop sang bộ mã hóa phần cứng NVENC.

## Cài đặt nhanh bản Pre-Release (`v0.1.0-alpha.1`)

> **Khuyên dùng nếu bạn muốn chạy ngay mà không cần cài CMake, MSVC, CUDA Toolkit hay Skia.**  
> **Yêu cầu tối thiểu:** Windows 10/11 x64 + Driver NVIDIA hỗ trợ CUDA 13.x/NVENC + Python 3.12 (`uv` hoặc `pip`).

### 1. Cài gói Python `stem-engine` dựng sẵn (Wheel kèm `_stem_native`, Skia Ganesh, CUDA PTX & FFmpeg DLLs)

```powershell
# Cách A — Dùng GitHub CLI (chạy được cho cả repo Private và Public):
gh release download v0.1.0-alpha.1 --repo HanitaV/DeLib4Anim --pattern "*.whl" --clobber
uv venv --python 3.12
uv pip install .\stem_engine-0.1.0a1-cp312-cp312-win_amd64.whl

# Cách B — Nếu dùng pip chuẩn hoặc repo ở chế độ Public:
# pip install .\stem_engine-0.1.0a1-cp312-cp312-win_amd64.whl
# uv pip install https://github.com/HanitaV/DeLib4Anim/releases/download/v0.1.0-alpha.1/stem_engine-0.1.0a1-cp312-cp312-win_amd64.whl

# Kiểm tra OpenGL NVIDIA + CUDA Interop + NVENC ngay sau khi cài:
.venv\Scripts\python.exe -m stem_engine doctor
```

### 2. Cài nhanh bộ Agent Skills (`stem-video-scriptwriter`, `stem-storyboard-director` & 4 technical skills)

```powershell
# Tải và giải nén toàn bộ .agents/skills/ từ bản Pre-Release vào thư mục dự án:
gh release download v0.1.0-alpha.1 --repo HanitaV/DeLib4Anim --pattern "delib4anim-skills-*.zip" --clobber
Expand-Archive -Path .\delib4anim-skills-v0.1.0-alpha.1.zip -DestinationPath . -Force

# Hoặc cài trực tiếp 2 skill viết kịch bản & storyboard qua skills CLI:
npx skills@latest add https://github.com/HanitaV/DeLib4Anim.git --skill stem-video-scriptwriter stem-storyboard-director --agent antigravity --copy --yes
```

*(Sau khi giải nén hoặc cài bằng `npx skills`, mở cuộc hội thoại mới hoặc gõ `/skills reload` trong Antigravity CLI).*

### 3. Bản Native C++20 CLI độc lập (Không cần Python)

- Tải `stem-engine-cli-v0.1.0-alpha.1-win-x64.zip` tại [GitHub Releases `v0.1.0-alpha.1`](https://github.com/HanitaV/DeLib4Anim/releases/tag/v0.1.0-alpha.1) (hoặc chạy `gh release download v0.1.0-alpha.1 --repo HanitaV/DeLib4Anim --pattern "stem-engine-cli-*.zip"`).
- Giải nén và kiểm tra phần cứng: `.\stem_engine_cli.exe doctor`.

---

## Kiến trúc tổng thể

```text
[1] Quy trình Kịch bản & Phân cảnh (Agent Skills)
  Ý tưởng / Tài liệu nguồn
    -> stem-video-scriptwriter (script.md + lint_stem_script.py)
    -> stem-storyboard-director (storyboard.md + .srt + cues.json)
    -> Python SceneBuilder (examples/*.py)

[2] Native GPU Render & Export Pipeline
  Python authoring API (stem_engine)
    -> Retained C++20 scene & deterministic timeline (native/core)
    -> OpenGL 4.5 Core 4x MSAA + Skia Ganesh + Stencil-then-Cover MathJax/SVG + TrueType atlas (native/render, native/text)
    -> Unified CUDA Driver API RGBA8 -> BT.709 NV12 trực tiếp trong VRAM (native/interop)
    -> FFmpeg AV_PIX_FMT_CUDA + h264_nvenc + 48 kHz AAC MP4 mux (native/encode)
```

---

## 1. Yêu cầu hệ thống & Công cụ nền (Prerequisites)

### Phần cứng & Hệ điều hành
- **Hệ điều hành**: Windows 10 / 11 x64 *(hỗ trợ chính; Linux đã có nhánh trong CMake/CUDA nhưng chưa kiểm chứng chạy thực tế)*.
- **GPU**: NVIDIA GeForce / RTX hỗ trợ **OpenGL 4.5+**, **CUDA Driver API** (driver tương thích CUDA 13.x) và **NVENC H.264**.

### Công cụ nền cần cài sẵn
Cài qua `winget` trong PowerShell nếu máy chưa có:

```powershell
winget install Git.Git
winget install Python.Python.3.12
winget install astral-sh.uv
winget install Kitware.CMake
winget install Ninja-build.Ninja
winget install OpenJS.NodeJS.LTS
```

- **Python**: `>= 3.11` (khuyến nghị `3.12`).
- **CMake**: `>= 3.25` + **Ninja**.
- **Node.js**: `>= 22.20` — dùng cho `npx skills` CLI và tự động cài đặt `mathjax-full@3.2.2` cục bộ vào `.stem_cache/mathjax_tool/` ở lần biên dịch công thức toán đầu tiên (**không cần cài TeX Live**).
- **Không cần cài thủ công Visual Studio, CUDA Toolkit hay Skia**: script [`tools/setup_toolchains.py`](tools/setup_toolchains.py) sẽ tự động tải, kiểm tra SHA256 và giải nén bộ **Portable MSVC x64 + Windows 11 SDK + CUDA 13.5 (`nvcc`, `compute-sanitizer`, `nsys`, `ncu`) + Skia Ganesh (`m144`)** vào `.stem_cache/toolchains/` ở mức user-space (không cần quyền Administrator).

---

## 2. Hướng dẫn Setup từ A tới Z (Windows x64)

### Bước 2.1 — Clone dự án & Tải Toolchain tự động (MSVC + CUDA 13.5 + Skia)

```powershell
git clone https://github.com/HanitaV/DeLib4Anim.git DeLib4Anim
cd DeLib4Anim

# Tạo virtual environment trước (chưa build extension C++ ngay)
uv venv --python 3.12

# Tải và xác thực SHA256 bộ portable MSVC, Windows 11 SDK, CUDA 13.5 và Skia
.venv\Scripts\python.exe tools\setup_toolchains.py
```

*(Tùy chọn: dùng `--only msvc|cuda|skia` để tải riêng từng thành phần, hoặc `--force` nếu cần tải lại).*

### Bước 2.2 — Kích hoạt Toolchain & Build gói Python `_stem_native`

Mỗi phiên PowerShell mới trước khi build C++/CUDA cần nạp biến môi trường toolchain:

```powershell
# Nạp cl.exe, Windows SDK, nvcc, SKIA_ROOT vào phiên PowerShell hiện tại
. .\scripts\activate_toolchains.ps1
$env:CMAKE_GENERATOR = "Ninja"

# Cài toàn bộ dependency (dev + build) và biên dịch extension C++20/CUDA (_stem_native)
uv sync --all-groups --no-build-isolation
```

*(Khi chỉnh sửa mã C++/CUDA trong `native/`, chạy lại lệnh sau để build lại extension:)*

```powershell
uv sync --all-groups --no-build-isolation --reinstall-package stem-engine
```

### Bước 2.3 — Kiểm tra phần cứng (`doctor`), Lint & Unit Tests

```powershell
# 1. Xác nhận OpenGL NVIDIA, CUDA-GL interop (cuGLGetDevices) và NVENC
.venv\Scripts\python.exe -m stem_engine doctor

# 2. Chạy lint, type-check và toàn bộ Python test suite
uv run --no-sync ruff check src examples tests tools
uv run --no-sync ty check src
uv run --no-sync pytest -v
```

### Bước 2.4 — (Tùy chọn) Build Native C++20 CLI & Chạy CTest

Dành cho khi muốn chạy trực tiếp binary C++ (`stem_engine_cli.exe`, `stem_native_tests.exe`) hoặc profiling với `compute-sanitizer` / `nsys`:

```powershell
. .\scripts\activate_toolchains.ps1

# Preset chính: MSVC + Skia Ganesh + nvcc (Release)
cmake --preset windows-msvc-skia
cmake --build --preset windows-msvc-skia
ctest --preset windows-msvc-skia --output-on-failure
```

*(Preset phụ `windows-release` / `windows-relwithdebinfo` dùng MinGW-W64 GCC cho bản không kèm Skia — xem thêm [`CMakePresets.json`](CMakePresets.json) và [`docs/build.md`](docs/build.md)).*

---

## 3. Chạy Preview, Render Video Demo & Benchmark

### Xem Preview tương tác (Native VSync OpenGL)

```powershell
.venv\Scripts\python.exe -m stem_engine preview examples/incline.py --fps 60
```

Phím tắt trong cửa sổ Preview:
- `Space`: Phát / Tạm dừng
- `Left` / `Right`: Tua nhanh
- `,` / `.`: Tiến / lùi từng frame
- `1`–`9`: Nhảy tới Cue đã đánh dấu
- `R`: Quay lại đầu | `M`: Tắt/bật tiếng | `Esc`: Thoát

### Render MP4 1080p60 (GPU Pipeline)

```powershell
# Demo 1: Vật trượt trên mặt phẳng nghiêng có ma sát (24s @ 1080p60)
.venv\Scripts\python.exe -m stem_engine render examples/incline.py --output outputs/incline.mp4 --width 1920 --height 1080 --fps 60

# Hoặc chạy script tái tạo nhanh 1 lệnh:
.\scripts\reproduce_incline.ps1

# Demo 2: Electron thực sự trông như thế nào? - Đám mây xác suất 3D 24.000 hạt (28s @ 1080p60)
.venv\Scripts\python.exe -m stem_engine render examples/chemistry_electron.py --output outputs/chemistry_electron.mp4 --width 1920 --height 1080 --fps 60
```

### Chạy Benchmark GPU (1080p60 & 4K60)

```powershell
.venv\Scripts\python.exe -m stem_engine benchmark --suite baseline --include-4k --demo-script examples/incline.py --output outputs/benchmark.json
```

---

## 4. Cài đặt & Sử dụng Agent Skills (A tới Z)

Antigravity tự động phát hiện các skill đặt tại `.agents/skills/<ten-skill>/SKILL.md` (áp dụng cơ chế Progressive Disclosure: chỉ đọc metadata trước và nạp nội dung chi tiết khi kích hoạt). Các skill cài từ GitHub/registry được theo dõi phiên bản trong [`skills-lock.json`](skills-lock.json).

### 4.1. Danh sách 6 Skills trong Workspace

#### Nhóm A — Skills Kịch bản & Phân cảnh STEM (Nội bộ dự án, có sẵn khi clone)

| Skill | Vai trò chính | Tài liệu & Công cụ đi kèm |
| :--- | :--- | :--- |
| [`stem-video-scriptwriter`](.agents/skills/stem-video-scriptwriter/SKILL.md) | Viết kịch bản, chuyển thể tài liệu, hoặc review/sửa kịch bản STEM kiểu **Median / 3Blue1Brown** (`mình – bạn`); chấm 3 hook, ước lượng thời lượng, bắt lỗi LaTeX thô & văn phong AI. | [Style DNA](.agents/skills/stem-video-scriptwriter/references/median-style-dna.md) · [Hooks & Review](.agents/skills/stem-video-scriptwriter/references/narrative-archetypes-and-hooks.md) · [Humanizer VN](.agents/skills/stem-video-scriptwriter/references/vietnamese-voice-and-humanizer.md) · [Linter CLI](.agents/skills/stem-video-scriptwriter/scripts/lint_stem_script.py) |
| [`stem-storyboard-director`](.agents/skills/stem-storyboard-director/SKILL.md) | Chuyển kịch bản thành Beat Sheet dài hoặc Shot Spec `1920x1080` chuẩn Dark-Studio, ánh xạ sang `SceneBuilder`, kiểm tra mật độ lời thoại và xuất `.srt` + `cues.json`. | [Visual Grammar](.agents/skills/stem-storyboard-director/references/median-visual-grammar.md) · [Engine Mapping](.agents/skills/stem-storyboard-director/references/stem-engine-mapping.md) · [Validator CLI](.agents/skills/stem-storyboard-director/scripts/validate_storyboard.py) |

#### Nhóm B — Skills Kỹ thuật C++ / Python / Shader / CUDA (Từ `skills.sh`)

| Skill | Nguồn | Phạm vi áp dụng trong repo |
| :--- | :--- | :--- |
| [`modern-cpp`](.agents/skills/modern-cpp/SKILL.md) | `trailofbits/skills` | RAII, ownership, lifetime tài nguyên trong `native/` |
| [`modern-python`](.agents/skills/modern-python/SKILL.md) | `trailofbits/skills` | Tooling `uv`, `ruff`, `ty`, type-safe Python cho `src/stem_engine/` |
| [`compute-sanitizer-diagnose-cuda-bugs`](.agents/skills/compute-sanitizer-diagnose-cuda-bugs/SKILL.md) | `NVIDIA/skills` | Chẩn đoán lỗi bộ nhớ, race, uninit, sync trên CUDA interop (`memcheck`, `racecheck`, `initcheck`, `synccheck`) |
| [`shader-programming`](.agents/skills/shader-programming/SKILL.md) | `gamedev-skills/awesome-gamedev-agent-skills` | Toán học GLSL, UV, coordinate spaces cho renderer |

### 4.2. Cài đặt, Khôi phục & Cập nhật Skills qua `npx skills`

Cài đặt yêu cầu **Node.js `>= 22.20`** và **Git**. Luôn dùng `--copy` để chép tệp thực vào `.agents/skills/` (tránh phụ thuộc symlink trên Windows):

```powershell
# 1. Cài (hoặc khôi phục) 4 technical skills vào .agents/skills/ của dự án
npx skills@latest add trailofbits/skills --skill modern-cpp modern-python --agent antigravity --copy --yes
npx skills@latest add NVIDIA/skills --skill compute-sanitizer-diagnose-cuda-bugs --agent antigravity --copy --yes
npx skills@latest add gamedev-skills/awesome-gamedev-agent-skills --skill shader-programming --agent antigravity --copy --yes

# 2. Liệt kê các skills đang hoạt động và cập nhật phiên bản mới
npx skills@latest list --agent antigravity
npx skills@latest update

# 3. Tìm kiếm thêm skills trên registry skills.sh (ví dụ: humanizer)
npx skills@latest find "humanizer"

# (Tùy chọn) Cài skill ở mức Global cho mọi dự án trên máy: thêm cờ -g
npx skills@latest add <owner/repo@skill> -g -y
```

Sau khi cài hoặc sửa `SKILL.md`:
- Mở **cuộc hội thoại mới** trong Antigravity IDE, hoặc gõ `/skills reload` nếu dùng Antigravity CLI (`agy`).

### 4.3. Quy trình sản xuất 1 Video STEM từ Ý tưởng -> MP4 với bộ Skills

1. **Bước 1 — Viết & Kiểm định kịch bản (`stem-video-scriptwriter`)**:
   - Yêu cầu Agent: *"Dùng skill `stem-video-scriptwriter` viết kịch bản kiểu Median dài 4 phút về chủ đề ..."*
   - Kiểm tra tự động thời lượng, YouTube chapters, LaTeX thô trong lời thoại và văn phong AI:
     ```powershell
     .venv\Scripts\python.exe .agents\skills\stem-video-scriptwriter\scripts\lint_stem_script.py path\to\script.md --target-sec 240 --strict
     ```
2. **Bước 2 — Thiết kế Storyboard & Xuất Manifest (`stem-storyboard-director`)**:
   - Yêu cầu Agent: *"Dùng skill `stem-storyboard-director` chia shot `stem_engine` theo kịch bản này và xuất `.srt` + `cues.json`."*
   - Kiểm tra timeline liên tục, tốc độ đọc từng shot (`words/sec`) và xuất phụ đề/cues:
     ```powershell
     .venv\Scripts\python.exe .agents\skills\stem-storyboard-director\scripts\validate_storyboard.py path\to\storyboard.md --mode shot --export-srt outputs\subtitles.srt --export-cues outputs\cues.json --strict
     ```
3. **Bước 3 — Dựng cảnh `SceneBuilder` & Xuất MP4**:
   - Viết script Python định nghĩa `build_scene(width=1920, height=1080)` (tham khảo [`examples/incline.py`](examples/incline.py) và [`examples/chemistry_electron.py`](examples/chemistry_electron.py)), kiểm tra `sb.check_overlaps() == []`, xem nhanh bằng `preview`, rồi `render` ra MP4 60fps.

*(Có thể chạy test kiểm tra cấu trúc và validator của cả 2 skill bằng `uv run --no-sync pytest tests/test_scriptwriting_skills.py -v`).*

---

## 5. Xử lý sự cố thường gặp (Troubleshooting)

1. **Laptop Hybrid GPU (AMD/Intel iGPU + NVIDIA dGPU) không nhận OpenGL NVIDIA trong `python.exe`**:
   - Nguyên nhân: `python.exe` không export `NvOptimusEnablement`, nên mặc định Windows có thể gắn OpenGL cho iGPU làm lỗi `cuGLGetDevices`.
   - Khắc phục: [`src/stem_engine/__init__.py`](src/stem_engine/__init__.py) đã tự động đặt `SHIM_MCCOMPAT=0x800000001` trước khi nạp `opengl32.dll`. Hãy chạy `.venv\Scripts\python.exe -m stem_engine doctor` để xác nhận `GL_VENDOR = NVIDIA Corporation`. Nếu hệ thống bị khóa cấu hình đồ họa, mở **Windows Settings -> System -> Display -> Graphics** và đặt `.venv\Scripts\python.exe` chạy **High performance (NVIDIA)**.
2. **Lỗi không tìm thấy `cl.exe`, `nvcc.exe`, hoặc `SKIA_ROOT` khi chạy `cmake` / `uv sync`**:
   - Hãy đảm bảo bạn đã **dot-source** script kích hoạt trong chính cửa sổ PowerShell đang dùng (có dấu `. ` ở đầu):
     ```powershell
     . .\scripts\activate_toolchains.ps1
     $env:CMAKE_GENERATOR = "Ninja"
     ```
3. **`compute-sanitizer` không attach được trên Windows WDDM**:
   - Mở PowerShell bằng quyền **Administrator** và chạy 1 lần duy nhất:
     ```powershell
     .stem_cache\toolchains\cuda\compute-sanitizer\EnableDebuggerInterface.bat
     ```
   - Sau đó chạy lại từ terminal thường:
     ```powershell
     . .\scripts\activate_toolchains.ps1
     compute-sanitizer --tool memcheck --error-exitcode 1 .\build\msvc-skia\stem_native_tests.exe
     ```
4. **Lỗi biên dịch công thức LaTeX (`add_formula` / `add_formula_card`) ở lần chạy đầu**:
   - Lần đầu tiên chạy cảnh có công thức toán, [`src/stem_engine/formula.py`](src/stem_engine/formula.py) cần `npm` và kết nối mạng để cài `mathjax-full@3.2.2` vào `.stem_cache/mathjax_tool/`. Các lần sau dùng hoàn toàn offline từ cache SVG.

---

## 6. Cấu trúc thư mục & Tài liệu chuyên sâu

```text
DeLib4Anim/
├── .agents/skills/       # Agent skills (scriptwriter, storyboard-director, modern-cpp/python, cuda, shader)
├── native/               # C++20 core timeline, OpenGL 4.5 + Skia renderer, CUDA interop, NVENC muxer
├── src/stem_engine/      # Python authoring API (SceneBuilder, Formula, CLI)
├── examples/             # Các cảnh STEM mẫu (incline.py, chemistry_electron.py)
├── tools/                # setup_toolchains.py, fetch_deps.py, compile_cuda_ptx.py
├── scripts/              # activate_toolchains.ps1, reproduce_incline.ps1
├── tests/                # Python & C++ unit tests
└── docs/                 # Tài liệu thiết kế, build, benchmark & video tham chiếu
```

### Tài liệu tham khảo thêm
- [Build & Environment Guide](docs/build.md)
- [Engine Architecture](docs/architecture.md)
- [Animation Physics & 2.5D Design Spec](docs/animation_physics_and_25d_design.md)
- [Performance & Benchmark Report](docs/performance.md)
- [Roadmap](docs/roadmap.md)
- [Engineering Notes](NOTES.md)
