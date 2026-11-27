# Antigravity: triển khai native GPU STEM animation engine

Bản thiết kế này ưu tiên tốc độ tạo khung hình và xuất video trên NVIDIA.
Dùng phần PROMPT TRIỂN KHAI bên dưới làm yêu cầu giao việc cho Antigravity.
Đây là dự án mới; không coi bất kỳ tính năng hay số liệu hiệu năng nào đã tồn tại.

## 1. Cài skills trong thư mục gốc dự án

Cần Node.js đáp ứng yêu cầu của skills CLI hiện tại và Git. Tài liệu CLI được kiểm tra ngày 27/11/2026 ghi Node.js 22.20 trở lên.

    npx skills@latest add trailofbits/skills --skill modern-cpp modern-python --agent antigravity --copy --yes
    npx skills@latest add NVIDIA/skills --skill compute-sanitizer-diagnose-cuda-bugs --agent antigravity --copy --yes
    npx skills@latest add gamedev-skills/awesome-gamedev-agent-skills --skill shader-programming --agent antigravity --copy --yes
    npx skills@latest list --agent antigravity

Dùng scope dự án, không cài toàn bộ catalog. --copy tránh phụ thuộc symlink.
CLI hiện tại nhắm .agents/skills; Antigravity còn hỗ trợ .agent/skills cho tương thích cũ.
Sau khi cài, mở conversation mới; CLI có thể dùng /skills reload.

Skills được chọn:
- modern-cpp: RAII, ownership, resource lifetime và mã C++ hiện đại.
- modern-python: môi trường và công cụ Python; không dùng để thay thế native runtime.
- shader-programming: nền tảng GLSL, coordinate spaces và effect math.
- compute-sanitizer-diagnose-cuda-bugs: chẩn đoán lỗi CUDA bằng công cụ NVIDIA.

Giới hạn:
- Skill GLSL không phải tài liệu OpenGL–CUDA/NVENC interop.
- Compute Sanitizer không thay thế Nsight profiling và không chứng minh mọi lớp lỗi đều sạch.
- modern-cpp không chuyên về CMake.
- Các skill không tự cài CUDA Toolkit, compiler, FFmpeg libraries hay Nsight.
- Không cần nsight-port-cpu-to-gpu cho lượt đầu: skill đó tập trung port HPC codebase; không phải skill tối ưu một kernel/renderer mới.
- Khi skill khuyến nghị lựa chọn khác, ràng buộc dự án dưới đây được ưu tiên: C++20 tương thích nvcc, scikit-build-core cho extension, native render.

## 2. PROMPT TRIỂN KHAI — sao chép từ đây

Bạn là Principal Graphics Engineer và senior C++/CUDA developer. Hãy trực tiếp triển khai một native GPU STEM animation engine trong workspace hiện tại.

Ưu tiên số một là tốc độ TẠO KHUNG HÌNH và tốc độ EXPORT thực tế. Không chỉ tăng tốc ghép ảnh hoặc encode.

Không dừng ở bản thiết kế, skeleton, pseudocode hoặc danh sách TODO. Sau khi khảo sát ngắn, bắt đầu sửa mã, build và kiểm tra một pipeline chạy xuyên suốt. Phân biệt rõ phần đã chạy thật, phần chỉ build được và phần đang bị chặn bởi môi trường.

### A. Bối cảnh và quyết định đã chốt

Tôi muốn chất lượng diễn xuất kiểu video STEM của Median/Clockwork: storyboard có chủ đích, stickman tương tác với toán học, camera dẫn mắt, micro-interactions và SFX gắn với hành động.

Bài học cần giữ:
- Storyboard/Excalidraw + kịch bản dẫn dắt bố cục.
- Virtual clock để scrub và render theo timestamp.
- Thư viện action/pose/effect được tích lũy qua video.
- Con người kiểm soát pacing.
- Không dùng glow/blur/shake để thay cho diễn xuất.

Kiến trúc triển khai:
Python authoring API
→ scene và animation tracks có kiểu
→ C++ native runtime
→ OpenGL GPU rendering
→ CUDA chuyển màu trong VRAM
→ FFmpeg CUDA hardware frames / NVENC
→ mux audio và MP4.

Browser, Electron, Remotion, HTML/DOM và screenshot automation không thuộc production renderer.
Không đổi sang browser vì dễ làm hơn.
Không tự viết engine đa backend hoặc compiler ngôn ngữ mới.

Python chỉ tạo cảnh, asset, tracks và precompute. Vòng lặp render/export và đánh giá tracks chạy trong C++. Không gọi Python cho từng object trên mỗi frame. Có thể render một frame qua Python API cho debug, nhưng production render gọi native batch entrypoint.

### B. Khảo sát workspace và môi trường

Đọc AGENTS.md và mã hiện có. Nếu đã có project, sửa tại chỗ và giữ thay đổi của người dùng.
Nếu có docs/, chỉ đọc tài liệu nguồn liên quan. Không ghi đè PDF, LaTeX, audio hoặc storyboard gốc.

Kiểm tra:
- OS thật; Windows native hay WSL/container.
- NVIDIA GPU, VRAM, driver, CUDA device.
- OpenGL vendor/renderer/version và có chạy trên cùng NVIDIA GPU với CUDA không.
- Compiler host tương thích CUDA Toolkit.
- CMake, Ninja, Python, nvcc.
- FFmpeg development libraries, headers và NVENC availability.
- Nsight Systems, Nsight Compute, Compute Sanitizer nếu đã có.
- Nguồn Skia/prebuilt artifacts có provenance và version rõ ràng.

Tạo environment report ngắn, không in secrets. GPU/CUDA/display có thể không khả dụng trong remote sandbox dù máy người dùng có NVIDIA; ghi đúng giới hạn.

Nếu thiếu quyền/GPU/dependency, tiếp tục phần build, CPU-side timeline tests và tài liệu chạy trên máy đích. Không thay đường native bằng browser, không lấy software renderer làm bằng chứng NVIDIA render, không bịa benchmark.

Chỉ hỏi khi bị chặn bởi một quyết định thật sự không suy ra được. Không hỏi lại xác nhận kế hoạch hay lựa chọn kỹ thuật thông thường đã được giao.

### C. Dùng skills đúng phạm vi

Phát hiện vị trí skill thực tế và đọc SKILL.md trước khi áp dụng:
- modern-cpp cho ownership và thiết kế C++.
- modern-python cho authoring package/tooling.
- shader-programming cho GLSL.
- compute-sanitizer-diagnose-cuda-bugs khi chẩn đoán kernel CUDA.

Không giả vờ đã dùng skill không có trên đĩa. Không cài thêm cả catalog.
Skill là hướng dẫn hỗ trợ, không được mở rộng scope sang web app, game engine lớn hoặc security audit không liên quan.

Đọc tài liệu/API gốc cho OpenGL, CUDA, Skia và FFmpeg. Không suy API từ tên hàm.
Duy trì NOTES.md ngắn cho các quyết định hoặc bất tương thích quan trọng.

### D. Stack và cấu trúc tối thiểu

- C++20 tương thích host compiler và nvcc; không ép C++26.
- CMake presets + Ninja khi phù hợp.
- Python >=3.11 nếu toolchain cho phép.
- pybind11 và scikit-build-core cho Python extension; uv quản lý môi trường, ruff kiểm tra Python.
- OpenGL 4.5 core, GLFW context/window, GL loader có version rõ.
- Native offscreen framebuffer là render target chính; preview dùng cùng renderer.
- Skia Ganesh/OpenGL cho text, glyphs và arbitrary paths.
- GLSL primitives/meshes/instancing cho geometry động.
- CUDA runtime hoặc driver API với context ownership nhất quán.
- FFmpeg libraries: libavutil, libavcodec, libavformat; libswresample nếu audio cần.
- Encoder đầu tiên: H.264 NVENC, NV12, SDR Rec.709, kích thước chẵn.

Pin dependencies theo tag/commit/version đã kiểm tra. Chọn một phương án quản lý dependency đơn giản cho Windows, không triển khai nhiều package managers song song.
Không tải binary không rõ provenance. Không redistrib proprietary SDK trái điều khoản; tài liệu hóa các prerequisite cần lấy riêng.

Module gợi ý, chỉ tạo khi cần:
- native/core: scene, tracks, clock, handles.
- native/render: context, retained geometry, render targets, shaders.
- native/interop: GL–CUDA mapping, device/context management.
- native/encode: hardware frames, encoder, muxer.
- native/text: Skia/glyph integration.
- python/: authoring, asset preparation, CLI.
- examples/: cảnh chạy thật.
- tests/: kiểm tra có ý nghĩa.
- tools/: environment doctor và benchmark.
- docs/: build, architecture ngắn, performance, roadmap.

Không xây ECS/plugin framework/scene compiler tổng quát trước khi có video chạy thật.

### E. Retained rendering — yêu cầu hiệu năng cốt lõi

Phân biệt transform, style, visibility và geometry dirty flags.
Giữ GPU buffers, glyph/path assets, render textures và allocation pools qua nhiều frame.

Cụ thể:
- Text/công thức: compile trước, không shape/typeset hoặc raster lại mỗi frame nếu nội dung không đổi.
- Đồ thị cố định: sample/build một lần, reveal bằng tham số độ dài cung/visibility.
- Vector/mũi tên/capsule: parametric geometry hoặc instancing.
- Stickman: cập nhật joints/rig và geometry nhỏ; không dựng lại toàn bộ scene.
- Deforming curves: ưu tiên topology ổn định, cập nhật buffer cần thiết hoặc shader evaluation.
- Particles: fixed pools, seed ổn định, GPU instancing; GPU simulation chỉ khi cần.
- Static decoration: cache có giới hạn; không raster cache mọi thứ bất kể zoom và VRAM.

Không tăng tốc bằng cách giảm chất lượng hình hoặc bỏ object mà không ghi rõ.
Không ép CPU work nhỏ sang GPU nếu launch/synchronization đắt hơn; dùng profiling để quyết định.

Skia được tích hợp trên context/target engine sở hữu. Quản lý state cache khi xen kẽ Skia và raw GL. Khóa đúng context/thread, flush/submit đúng thời điểm, resolve MSAA trước khi chia sẻ texture nếu cần.

Nếu build Skia làm chặn vertical slice đầu, hoàn thành interop/encode bằng primitives GLSL trước, rồi tích hợp text/path. Không coi engine hoàn chỉnh nếu phần text/path vẫn là stub.

### F. GPU → encoder không tải ảnh thô về CPU

Thực hiện đường chính:
1. Native renderer ghi vào offscreen color texture.
2. CUDA đăng ký graphics resource một lần cho mỗi allocation.
3. Map texture và truy cập đúng CUDA array; không cast GL texture ID thành CUdeviceptr.
4. CUDA kernel đọc texture và ghi NV12 vào encoder-compatible GPU frame.
5. NVENC đọc hardware frame.
6. Nhận compressed packets và mux MP4.

FFmpeg implementation ưu tiên:
- Một CUDA device/context thống nhất với interop.
- AVHWFramesContext có hardware format AV_PIX_FMT_CUDA, sw_format NV12.
- Dùng frame pool phù hợp, tôn trọng linesize/pitch và plane offsets.
- Ghi CUDA output vào GPU frame do pool cấp.
- Gửi frame qua avcodec, nhận packet, rescale timestamp và mux.
- Giữ frame/resource đến khi encoder không còn dùng; không tái sử dụng sớm.

Thiết kế render-texture pool và encoder-surface pool riêng. Bắt đầu đơn giản rồi tăng queue depth theo trace; không hardcode số buffer là tối ưu.

Không dùng glReadPixels/getImageData/PNG sequence làm production export.
Readback cho snapshot, debug hoặc kiểm tra màu được phép, nhưng tách khỏi benchmark production.
Compressed bitstream đi qua CPU để mux là hợp lệ.

Tránh register/unregister, malloc/free, shader compilation và global synchronization không cần thiết trong hot loop.
Không bỏ synchronization cần thiết để đạt FPS giả.
OpenGL không được truy cập resource đang map cho CUDA.
Không yêu cầu NVENC OpenGL device trực tiếp trên Windows.

“Không raw-frame readback” không đồng nghĩa không có GPU-side copy hay driver-internal conversion. Báo chính xác đường dữ liệu đã chứng minh được.

Color conversion:
- Kiểm tra orientation, RGBA/BGRA, transfer function, Rec.709 matrix và limited/full range.
- Composite alpha lên nền trước khi xuất H.264 opaque.
- Dùng test patterns để đối chiếu CPU reference với GPU conversion.
- Không gắn metadata để che một phép chuyển màu sai.

Export phải flush encoder/muxer, giữ frame cuối và đóng resources đúng thứ tự.

### G. Timeline, diễn xuất và audio

Hợp đồng:
scene.evaluate(time)
renderer.render(scene)
exporter.render_range(start, end, fps)

Frame time được suy từ chỉ số frame và rational fps, không phụ thuộc wall clock.
Native export chạy nhanh nhất có thể và tôn trọng backpressure, không sleep theo fps, không drop frame để đuổi kịp thời gian thực.
VSync chỉ dành cho preview; tắt giới hạn pacing khi benchmark export.

Deterministic trong môi trường đã xác định:
- Analytic hoặc baked tracks.
- Seed ổn định.
- Physics fixed timestep và cache khi cần.
- Camera follow không dựa vào số lần update trước đó.
- Trail lấy từ quỹ đạo tại thời điểm xác định.
- Evaluate theo thứ tự 0, 5, 2, 5 phải khôi phục cùng scene state tại giây 5.
- Không hứa bitwise raster identity xuyên GPU/driver.

Actions tối thiểu:
- reveal
- move/point
- release/contact
- follow-through

Camera tối thiểu:
- position, zoom, rotation
- frame target
- follow từ trajectory đã xác định

Dùng một spring hoặc quintic easing có tham số rõ; chưa cần planner robotics.
Tách scientific state khỏi presentation state; squash/stretch không được sửa số liệu vật lý.

Audio:
- Voice/SFX là event manifest theo shot-local time và stable cue ID.
- Không trigger audio từ render callbacks.
- Hỗ trợ voice WAV cùng cue JSON do người dùng/TTS cung cấp.
- Khi audio đổi, tính lại những tracks phụ thuộc cue.
- Không bắt buộc tích hợp model TTS trong milestone đầu.
- Nếu chưa có voice thật, dùng audio test được ghi rõ là test, không giả làm bài giảng đã lồng tiếng.
- Mix audio và mux với video, giữ PTS/duration đúng; không encode lại video không cần thiết.

### H. Authoring API, storyboard và STEM

Python API cần nhỏ, có type hints và stable IDs:
Scene, Camera, Text/Formula, Graph, Vector, Body, Stickman, Cue, AudioEvent.

Scene/track data được chuyển sang native theo lô. Không xây Python callback updater mỗi frame.
Có thể dùng JSON asset/scene dump để debug, nhưng không tạo full language/compiler.

Storyboard:
- Nhận ảnh và dữ liệu Excalidraw khi có.
- Giữ authored anchors/composition.
- Bản đầu chỉ cần attach, next_to, safe area, text bounds và overlap warnings.
- Không xây solver layout toàn cục hay Vision auto-repair loop trong milestone này.
- Agent được phép viết scene code cụ thể theo storyboard.

STEM:
- MathJax SVG compile/cache trước render; Node chỉ là asset tool ngoại tuyến nếu cần.
- Không yêu cầu TeX Live cho subset công thức MVP.
- Cache key gồm source, macros, font và compiler version.
- Không dùng bitmap độ phân giải thấp rồi zoom mạnh.
- Chưa làm arbitrary contour morph: animate token groups đã khai báo hoặc crossfade/reposition có kiểm soát.
- FBD lấy hướng/độ lớn/đơn vị từ model rõ ràng, không từ layout.
- LaTeX/PDF gốc trong docs là tài liệu nguồn; không sửa để chiều theo renderer.

Hóa học/RDKit, trường số/Warp-Taichi và simulation nâng cao nằm ở roadmap sau. Không thêm dependency đó khi demo chưa cần.

### I. Các milestone triển khai liên tiếp

Milestone 1 — Native end-to-end:
- CMake build được native CLI.
- Doctor xác nhận graphics/CUDA/encoder capabilities.
- Render primitives GLSL vào GPU texture.
- CUDA chuyển sang NV12.
- NVENC + MP4 xuất một clip 10 giây.
- Có timestamp, frame count, orientation và color tests.

Milestone 2 — Authoring và timeline:
- Python extension/package hoạt động.
- Chuyển scene/tracks sang C++.
- Seek và render-range hoạt động.
- Cache/dirty flags/resource pools.
- Glyph/text/path rendering có thật.
- Preview native tối thiểu cùng renderer; chưa cần editor đầy đủ.

Milestone 3 — Demo STEM:
- Một video 20–30 giây về vật trên mặt phẳng nghiêng.
- Chọn mô hình vật lý đơn giản, ghi assumptions rõ.
- Stickman chỉ/nhả vật, camera theo chuyển động.
- FBD và phương trình nhất quán.
- Gradient nền, contact shadow, một trail/effect có chủ đích.
- Có audio event track; voice thật nếu nguồn đã có.
- Có script tái tạo video bằng một lệnh.

Milestone 4 — Đo và sửa bottleneck:
- Benchmark các workload bên dưới.
- Tối ưu bottleneck lớn nhất đã đo, giữ chất lượng so sánh.
- Ghi before/after trên cùng máy, cùng workload, cùng build.
- Cập nhật hướng dẫn build và hạn chế còn lại.

Hoàn thành theo thứ tự; không xây toàn bộ API rồi mới thử encode.
Tiếp tục công việc đã được giao mà không chờ duyệt kế hoạch giữa các milestone.
Nếu gặp blocker thật, báo rõ lỗi và tiếp tục phần độc lập có thể làm.

### J. Benchmark và tiêu chí hoàn thành

Mục tiêu ban đầu: 1920×1080, 60 fps, export ít nhất realtime cho demo đã định nghĩa.
Đây là mục tiêu, không phải cam kết mọi GPU hoặc mọi cảnh đều đạt.
Cho phép benchmark thêm 4K, nhưng không đổi scope vì 4K chưa đạt.

Workloads:
1. Static glyph/path assets có transform.
2. Dynamic/deforming curves và stickman.
3. Particles + blur/compositing.
4. Render-only.
5. Render + conversion + NVENC + mux.

Đo:
- Cold startup, shader/font/asset compile.
- Warm scene evaluation và geometry update.
- GPU render time bằng GPU timestamp queries.
- CUDA conversion bằng CUDA events.
- Interop/synchronization stalls.
- Encode queue/backpressure và completion.
- Tổng wall time đến khi flush/mux xong.
- Peak VRAM, resource counts và raw-frame readback.
- Average/p95 frame latency khi có ý nghĩa và total export throughput.
- Phân biệt latency và throughput có overlap.

Không lấy CPU submit time làm GPU completion time.
Không bật glFinish/cudaDeviceSynchronize mỗi frame chỉ để đo cho dễ.
Dùng async queries/events hoặc profiling run riêng.
Không lấy số liệu dưới sanitizer làm performance result.
Báo profiler/tooling nào có hoặc thiếu.

Xuất benchmark JSON/CSV và một báo cáo ngắn kèm hardware, driver, resolution, fps, codec, settings, build type, scene hash và số lần chạy.
Không bịa số liệu nếu không có GPU.

Kiểm tra có ý nghĩa:
- Track evaluation và seek order.
- Frame count và timestamp/duration.
- Encoder flush và frame cuối.
- GPU conversion so với reference.
- Resource lifetime, repeated export, shutdown.
- Scientific assumptions/FBD consistency.
- Compute Sanitizer theo đúng lớp lỗi cần chẩn đoán.
- Hình ảnh vài keyframes được xem trực tiếp để bắt sai màu, clipping và chữ đè hình.

Một test sạch không chứng minh mọi lớp CUDA race/synchronization đều đúng.

### K. Đầu ra phải bàn giao

- Source và dependency/build configuration.
- Python authoring API cùng example.
- Native preview/export CLI.
- Video demo thực tế nếu môi trường cho phép.
- Benchmark command và dữ liệu đã đo.
- Hướng dẫn Windows đầu tiên; ghi đúng mức hỗ trợ Linux.
- Danh sách tính năng hoàn thành, giới hạn và blocker.
- Lệnh chính xác để người dùng build/render/benchmark lại.

CLI interface mong muốn, có thể điều chỉnh tên package nhưng phải nhất quán:

    python -m stem_engine doctor
    python -m stem_engine render examples/incline.py --output outputs/incline.mp4 --width 1920 --height 1080 --fps 60
    python -m stem_engine benchmark --suite baseline --output outputs/benchmark.json

Bắt đầu bằng khảo sát workspace và một kế hoạch ngắn, sau đó triển khai Milestone 1 ngay.
Không kết thúc bằng “tôi có thể triển khai tiếp” khi vẫn còn công việc đã được giao và làm được.
Không gọi stub, mock encoder hoặc CPU fallback là native GPU pipeline đã hoàn thành.

## 3. Nguồn đã kiểm tra

Skills và cài đặt:
- https://github.com/trailofbits/skills/blob/main/plugins/modern-cpp/skills/modern-cpp/SKILL.md
- https://github.com/trailofbits/skills/blob/main/plugins/modern-python/skills/modern-python/SKILL.md
- https://github.com/NVIDIA/skills/blob/main/skills/compute-sanitizer-diagnose-cuda-bugs/SKILL.md
- https://github.com/gamedev-skills/awesome-gamedev-agent-skills/blob/main/skills/disciplines/shader-programming/SKILL.md
- https://github.com/vercel-labs/skills/blob/main/docs/commands.md
- https://github.com/vercel-labs/skills/blob/main/docs/agents.md
- https://antigravity.google/docs/skills

Tài liệu kỹ thuật cần đối chiếu khi triển khai:
- https://docs.nvidia.com/cuda/cuda-programming-guide/04-special-topics/graphics-interop.html
- https://docs.nvidia.com/video-technologies/video-codec-sdk/13.1/nvenc-video-encoder-api-prog-guide/index.html
- https://ffmpeg.org/ffmpeg-codecs.html
- https://skia.org/docs/user/api/skcanvas_creation/
- https://docs.mathjax.org/en/stable/options/output/svg.html

Ngày kiểm tra nguồn: 27/11/2026. Xác minh lại API và dependency theo môi trường thực tế; không mặc định bản mới nhất luôn tương thích.

