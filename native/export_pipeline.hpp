#pragma once

#include "native/core/scene.hpp"
#include "native/encode/nvenc_encoder.hpp"
#include "native/interop/cuda_drv.hpp"
#include "native/render/gl_context.hpp"
#include "native/render/renderer.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace stem {

struct ExportOptions {
    std::string output_path = "outputs/out.mp4";
    int width = 1920;
    int height = 1080;
    RationalFPS fps{60, 1};
    double start_sec = 0.0;
    double end_sec = -1.0;  // < 0 uses scene.duration()
    int ring_size = 8;
    int interop_batch_size = 8;  // 1 = per-frame interop; >1 amortizes WDDM GL-CUDA map/unmap fence
    int msaa_samples = 4;
    int64_t bitrate_bps = 14'000'000;
    std::string nvenc_preset = "p4";
    int rc_lookahead = 0;
    bool enable_audio = true;
    bool render_only = false;  // Benchmark mode: OpenGL render + MSAA resolve only (skip CUDA/NVENC)
    bool record_per_frame_timings = true;
};

struct ExportMetrics {
    std::string build_type;
    std::string gl_vendor;
    std::string gl_renderer;
    std::string gl_version;
    std::string cuda_device_name;
    int cuda_driver_version = 0;
    int width = 0;
    int height = 0;
    double fps = 60.0;
    int64_t frame_count = 0;
    uint64_t scene_hash = 0;

    double cold_startup_ms = 0.0;
    double asset_compile_ms = 0.0;
    double cpu_eval_avg_ms = 0.0;
    double cpu_eval_p95_ms = 0.0;
    double gl_gpu_avg_ms = 0.0;
    double gl_gpu_p95_ms = 0.0;
    double cuda_convert_avg_ms = 0.0;
    double cuda_convert_p95_ms = 0.0;
    double encode_submit_avg_ms = 0.0;
    double encode_submit_p95_ms = 0.0;
    double frame_wall_avg_ms = 0.0;
    double frame_wall_p95_ms = 0.0;
    double finalize_flush_ms = 0.0;
    double total_wall_ms = 0.0;
    double export_fps = 0.0;

    size_t vram_total_bytes = 0;
    size_t vram_free_before_bytes = 0;
    size_t vram_free_min_bytes = 0;
    size_t peak_vram_used_bytes = 0;
    uint64_t raw_frame_readbacks = 0;
    uint64_t output_file_bytes = 0;
};

// Production batch exporter: Python/C++ scene -> OpenGL 4.5 MSAA -> CUDA NV12 -> NVENC H.264 -> MP4.
[[nodiscard]] Result<ExportMetrics> export_scene_range(
    const Scene& scene,
    const ExportOptions& opts
);

// Interactive native preview using the exact same OpenGL 4.5 Renderer + VSync.
[[nodiscard]] Result<void> preview_scene_native(
    const Scene& scene,
    RationalFPS fps = {60, 1},
    double max_seconds = -1.0
);

// Debug-only single-frame RGBA8 snapshot (explicitly separated from production export/benchmark).
[[nodiscard]] Result<std::vector<uint8_t>> render_single_frame_rgba8_debug(
    const Scene& scene,
    double time_sec,
    int width,
    int height,
    int msaa_samples = 4
);

// Runs GPU RGBA8 -> NV12 CUDA kernel on `scene` at `t=0` and compares against CPU IEEE-754 BT.709 reference.
struct ColorVerificationReport {
    int width = 0;
    int height = 0;
    int max_abs_diff_y = 0;
    int max_abs_diff_uv = 0;
    double mean_abs_diff_y = 0.0;
    double mean_abs_diff_uv = 0.0;
    uint8_t top_left_y = 0;
    uint8_t top_left_cb = 0;
    uint8_t top_left_cr = 0;
    uint8_t top_right_y = 0;
    uint8_t bottom_left_y = 0;
    uint8_t bottom_right_y = 0;
};

[[nodiscard]] Result<ColorVerificationReport> verify_gpu_nv12_against_reference(
    const Scene& scene,
    int width = 640,
    int height = 360
);

}  // namespace stem
