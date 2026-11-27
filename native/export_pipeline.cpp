#include "native/export_pipeline.hpp"

#include <GLFW/glfw3.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <numeric>

namespace stem {

namespace {

std::pair<double, double> avg_and_p95(std::vector<double>& vals) {
    if (vals.empty()) return {0.0, 0.0};
    const double sum = std::accumulate(vals.begin(), vals.end(), 0.0);
    const double avg = sum / static_cast<double>(vals.size());
    std::ranges::sort(vals);
    const size_t idx = std::min(vals.size() - 1, static_cast<size_t>(std::ceil(0.95 * vals.size()) - 1));
    return {avg, vals[idx]};
}

constexpr const char* kBuildTypeStr =
#ifdef NDEBUG
    "Release";
#else
    "Debug/RelWithDebInfo";
#endif

#if defined(_WIN32)
class WinMmPreviewAudio {
public:
    ~WinMmPreviewAudio() { shutdown(); }

    void initialize(const Scene& scene) {
        if (scene.audio_events().empty() || scene.duration() <= 0.0) return;
        const auto f32 = mix_scene_audio_f32_stereo(scene, 0.0, scene.duration(), 48000);
        if (f32.empty()) return;
        pcm_.resize(f32.size());
        for (size_t i = 0; i < f32.size(); ++i) {
            const float clamped = std::clamp(f32[i], -1.0f, 1.0f);
            pcm_[i] = static_cast<int16_t>(std::lround(clamped * 32767.0f));
        }

        WAVEFORMATEX wf{};
        wf.wFormatTag = WAVE_FORMAT_PCM;
        wf.nChannels = 2;
        wf.nSamplesPerSec = 48000;
        wf.wBitsPerSample = 16;
        wf.nBlockAlign = 4;
        wf.nAvgBytesPerSec = 48000 * 4;
        if (waveOutOpen(&hwo_, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            hwo_ = nullptr;
            pcm_.clear();
        }
    }

    void sync_play(double playhead_sec, bool playing, bool muted) {
        if (!hwo_ || pcm_.empty()) return;
        if (!playing || muted) {
            if (is_active_) stop();
            return;
        }
        if (!is_active_) {
            start_at(playhead_sec);
        }
    }

    void restart_if_playing(double playhead_sec, bool playing, bool muted) {
        if (!hwo_ || pcm_.empty()) return;
        stop();
        if (playing && !muted) {
            start_at(playhead_sec);
        }
    }

    void shutdown() noexcept {
        stop();
        if (hwo_) {
            waveOutClose(hwo_);
            hwo_ = nullptr;
        }
        pcm_.clear();
    }

private:
    void start_at(double playhead_sec) {
        if (!hwo_ || pcm_.empty()) return;
        const size_t total_frames = pcm_.size() / 2u;
        const size_t start_frame = std::min(
            total_frames,
            static_cast<size_t>(std::max(0.0, playhead_sec) * 48000.0)
        );
        if (start_frame >= total_frames) return;
        hdr_ = WAVEHDR{};
        hdr_.lpData = reinterpret_cast<LPSTR>(pcm_.data() + start_frame * 2u);
        hdr_.dwBufferLength = static_cast<DWORD>((total_frames - start_frame) * 4u);
        if (waveOutPrepareHeader(hwo_, &hdr_, sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
            prepared_ = true;
            if (waveOutWrite(hwo_, &hdr_, sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
                is_active_ = true;
            }
        }
    }

    void stop() noexcept {
        if (!hwo_) return;
        if (is_active_ || prepared_) {
            waveOutReset(hwo_);
        }
        if (prepared_) {
            waveOutUnprepareHeader(hwo_, &hdr_, sizeof(WAVEHDR));
            prepared_ = false;
        }
        is_active_ = false;
    }

    HWAVEOUT hwo_ = nullptr;
    WAVEHDR hdr_{};
    bool prepared_ = false;
    bool is_active_ = false;
    std::vector<int16_t> pcm_;
};
#endif

}  // namespace

Result<ExportMetrics> export_scene_range(const Scene& scene, const ExportOptions& opts) {
    const auto wall_start = std::chrono::steady_clock::now();

    const int width = opts.width > 0 ? opts.width : scene.width();
    const int height = opts.height > 0 ? opts.height : scene.height();
    const double start_sec = std::max(0.0, opts.start_sec);
    const double end_sec = opts.end_sec > 0.0 ? opts.end_sec : scene.duration();
    const int64_t frame_count = opts.fps.frame_count_for_range(start_sec, end_sec);
    if (frame_count <= 0) {
        return fail("export_scene_range: frame_count <= 0");
    }

    // 1. OpenGL 4.5 offscreen context
    render::GlContext gl_ctx;
    auto gl_info = gl_ctx.initialize(width, height, false, "STEM Export");
    if (!gl_info) return fail(gl_info.error());

    // 2. Renderer with ring pool sized to cover batch-amortized GL-CUDA interop
    const int batch_size = opts.render_only ? 1 : std::max(1, opts.interop_batch_size);
    const int ring_size = opts.render_only ? std::max(3, opts.ring_size) : std::max(opts.ring_size, batch_size);
    render::Renderer renderer;
    if (auto rr = renderer.initialize(gl_ctx.gl(), width, height, ring_size, opts.msaa_samples); !rr) {
        return fail(rr.error());
    }
    if (auto ar = renderer.prepare_scene_assets(scene); !ar) {
        return fail(ar.error());
    }

    // 3. CUDA Driver API context bound to the same NVIDIA GPU + register GL textures once per slot
    interop::CudaInteropContext cuda_ctx;
    std::vector<interop::RegisteredGlTexture> cuda_slots(static_cast<size_t>(ring_size));
    encode::NvencMp4Exporter exporter;
    interop::CudaDeviceInfo cuda_info{};

    if (!opts.render_only) {
        auto ci = cuda_ctx.initialize_for_current_gl_context();
        if (!ci) return fail(ci.error());
        cuda_info = *ci;

        for (int i = 0; i < ring_size; ++i) {
            auto reg = cuda_ctx.register_gl_texture(
                renderer.slot(static_cast<size_t>(i)).color_tex,
                width,
                height,
                cuda_slots[static_cast<size_t>(i)]
            );
            if (!reg) return fail(reg.error());
        }

        encode::EncoderConfig ecfg{
            .output_path = opts.output_path,
            .width = width,
            .height = height,
            .fps = opts.fps,
            .bitrate_bps = opts.bitrate_bps,
            .preset = opts.nvenc_preset,
            .rc_lookahead = opts.rc_lookahead,
            .hw_pool_size = std::max(6, batch_size + 4),
            .enable_audio = opts.enable_audio,
            .audio_sample_rate = 48000,
        };
        if (auto eo = exporter.open(cuda_ctx, ecfg); !eo) {
            return fail(eo.error());
        }
    }

    const auto init_done = std::chrono::steady_clock::now();

    size_t vram_free_before = cuda_info.free_vram_bytes;
    size_t vram_total = cuda_info.total_vram_bytes;
    size_t vram_free_min = vram_free_before;
    if (!opts.render_only) {
        if (auto vb = cuda_ctx.query_vram_bytes()) {
            vram_free_before = vb->first;
            vram_total = vb->second;
            vram_free_min = vram_free_before;
        }
    }

    std::vector<double> cpu_eval_ms;
    std::vector<double> gl_gpu_ms;
    std::vector<double> cuda_conv_ms;
    std::vector<double> enc_submit_ms;
    std::vector<double> frame_wall_ms;
    if (opts.record_per_frame_timings) {
        cpu_eval_ms.reserve(static_cast<size_t>(frame_count));
        gl_gpu_ms.reserve(static_cast<size_t>(frame_count));
        cuda_conv_ms.reserve(static_cast<size_t>(frame_count));
        enc_submit_ms.reserve(static_cast<size_t>(frame_count));
        frame_wall_ms.reserve(static_cast<size_t>(frame_count));
    }

    SceneSnapshot snap;
    if (opts.render_only) {
        for (int64_t f = 0; f < frame_count; ++f) {
            const auto f_t0 = std::chrono::steady_clock::now();
            const size_t slot_idx = static_cast<size_t>(f % ring_size);
            if (opts.record_per_frame_timings) {
                const float prev_gl = renderer.poll_slot_gpu_time_ms(slot_idx);
                if (prev_gl >= 0.0f) gl_gpu_ms.push_back(static_cast<double>(prev_gl));
            }
            const double t_sec = start_sec + opts.fps.time_at_frame(f);
            const auto e0 = std::chrono::steady_clock::now();
            scene.evaluate(t_sec, snap);
            const auto e1 = std::chrono::steady_clock::now();
            if (auto rr = renderer.render(snap, slot_idx); !rr) return fail(rr.error());
            const auto f_t1 = std::chrono::steady_clock::now();
            if (opts.record_per_frame_timings) {
                cpu_eval_ms.push_back(std::chrono::duration<double, std::milli>(e1 - e0).count());
                frame_wall_ms.push_back(std::chrono::duration<double, std::milli>(f_t1 - f_t0).count());
            }
        }
    } else {
        std::vector<encode::GpuVideoFrame> batch_frames;
        std::vector<interop::CudaInteropContext::BatchNv12Target> batch_targets;
        std::vector<float> batch_cuda_ms;
        batch_frames.reserve(static_cast<size_t>(batch_size));
        batch_targets.reserve(static_cast<size_t>(batch_size));

        for (int64_t base_f = 0; base_f < frame_count; base_f += batch_size) {
            const auto b_t0 = std::chrono::steady_clock::now();
            const int64_t cur_n = std::min<int64_t>(batch_size, frame_count - base_f);
            batch_frames.clear();
            batch_targets.clear();

            // 1. Render cur_n frames in OpenGL into slots[0..cur_n-1]
            for (int64_t k = 0; k < cur_n; ++k) {
                const int64_t f = base_f + k;
                const size_t slot_idx = static_cast<size_t>(k);
                if (opts.record_per_frame_timings) {
                    const float prev_gl = renderer.poll_slot_gpu_time_ms(slot_idx);
                    if (prev_gl >= 0.0f) gl_gpu_ms.push_back(static_cast<double>(prev_gl));
                }
                const double t_sec = start_sec + opts.fps.time_at_frame(f);
                const auto e0 = std::chrono::steady_clock::now();
                scene.evaluate(t_sec, snap);
                const auto e1 = std::chrono::steady_clock::now();
                if (auto rr = renderer.render(snap, slot_idx); !rr) return fail(rr.error());
                if (opts.record_per_frame_timings) {
                    cpu_eval_ms.push_back(std::chrono::duration<double, std::milli>(e1 - e0).count());
                }

                auto frame_res = exporter.acquire_gpu_frame();
                if (!frame_res) return fail(frame_res.error());
                batch_frames.push_back(*frame_res);
                batch_targets.push_back(interop::CudaInteropContext::BatchNv12Target{
                    .slot = &cuda_slots[slot_idx],
                    .dst_y = frame_res->y_ptr,
                    .pitch_y = frame_res->y_pitch,
                    .dst_uv = frame_res->uv_ptr,
                    .pitch_uv = frame_res->uv_pitch,
                    .flip_y = true,
                    .bg_color = snap.bg_bottom,
                });
            }

            // 2. Single-fence batch CUDA map -> N color conversion kernels -> single unmap + sync
            if (auto cv = cuda_ctx.convert_gl_textures_batch_to_nv12(
                    batch_targets, opts.record_per_frame_timings, batch_cuda_ms
                );
                !cv) {
                return fail(cv.error());
            }
            for (float ms : batch_cuda_ms) {
                cuda_conv_ms.push_back(static_cast<double>(ms));
            }
            if (auto sr = cuda_ctx.synchronize_stream(); !sr) {
                return fail(sr.error());
            }

            // 3. Submit cur_n ready NV12 GPU frames to h264_nvenc + MP4 muxer
            for (int64_t k = 0; k < cur_n; ++k) {
                const auto s0 = std::chrono::steady_clock::now();
                if (auto sub = exporter.submit_gpu_frame(batch_frames[static_cast<size_t>(k)], base_f + k); !sub) {
                    return fail(sub.error());
                }
                const auto s1 = std::chrono::steady_clock::now();
                if (opts.record_per_frame_timings) {
                    enc_submit_ms.push_back(std::chrono::duration<double, std::milli>(s1 - s0).count());
                }
            }

            if ((base_f % 64) == 0) {
                if (auto vq = cuda_ctx.query_vram_bytes()) {
                    vram_free_min = std::min(vram_free_min, vq->first);
                }
            }

            const auto b_t1 = std::chrono::steady_clock::now();
            if (opts.record_per_frame_timings) {
                const double per_f_ms =
                    std::chrono::duration<double, std::milli>(b_t1 - b_t0).count() / static_cast<double>(cur_n);
                for (int64_t k = 0; k < cur_n; ++k) {
                    frame_wall_ms.push_back(per_f_ms);
                }
            }
        }
    }

    // Drain remaining async GL timer queries after loop
    if (opts.record_per_frame_timings) {
        gl_ctx.gl().Finish();
        for (int i = 0; i < ring_size; ++i) {
            const float gm = renderer.poll_slot_gpu_time_ms(static_cast<size_t>(i));
            if (gm >= 0.0f) gl_gpu_ms.push_back(static_cast<double>(gm));
        }
    }

    const auto fin0 = std::chrono::steady_clock::now();
    if (!opts.render_only) {
        if (opts.enable_audio && !scene.audio_events().empty()) {
            const auto pcm = mix_scene_audio_f32_stereo(scene, start_sec, end_sec, 48000);
            if (auto aw = exporter.write_audio_f32_stereo(pcm); !aw) {
                return fail(aw.error());
            }
        }
        if (auto fin = exporter.finalize(); !fin) {
            return fail(fin.error());
        }
        for (auto& cs : cuda_slots) {
            cuda_ctx.unregister_gl_texture(cs);
        }
        if (auto vq = cuda_ctx.query_vram_bytes()) {
            vram_free_min = std::min(vram_free_min, vq->first);
        }
        cuda_ctx.shutdown();
    }
    const auto fin1 = std::chrono::steady_clock::now();

    const uint64_t readbacks = renderer.debug_readback_count();
    const double asset_ms = renderer.last_asset_compile_ms();
    renderer.shutdown();
    gl_ctx.shutdown();

    const auto wall_end = std::chrono::steady_clock::now();
    const double total_wall = std::chrono::duration<double, std::milli>(wall_end - wall_start).count();

    auto [eval_avg, eval_p95] = avg_and_p95(cpu_eval_ms);
    auto [gl_avg, gl_p95] = avg_and_p95(gl_gpu_ms);
    auto [cuda_avg, cuda_p95] = avg_and_p95(cuda_conv_ms);
    auto [enc_avg, enc_p95] = avg_and_p95(enc_submit_ms);
    auto [fw_avg, fw_p95] = avg_and_p95(frame_wall_ms);

    uint64_t out_bytes = 0;
    if (!opts.render_only) {
        std::error_code ec;
        out_bytes = std::filesystem::file_size(opts.output_path, ec);
    }

    return ExportMetrics{
        .build_type = kBuildTypeStr,
        .gl_vendor = gl_info->vendor,
        .gl_renderer = gl_info->renderer,
        .gl_version = gl_info->version,
        .cuda_device_name = cuda_info.name,
        .cuda_driver_version = cuda_info.driver_version,
        .width = width,
        .height = height,
        .fps = opts.fps.to_double(),
        .frame_count = frame_count,
        .scene_hash = scene.scene_hash(),
        .cold_startup_ms = std::chrono::duration<double, std::milli>(init_done - wall_start).count(),
        .asset_compile_ms = asset_ms,
        .cpu_eval_avg_ms = eval_avg,
        .cpu_eval_p95_ms = eval_p95,
        .gl_gpu_avg_ms = gl_avg,
        .gl_gpu_p95_ms = gl_p95,
        .cuda_convert_avg_ms = cuda_avg,
        .cuda_convert_p95_ms = cuda_p95,
        .encode_submit_avg_ms = enc_avg,
        .encode_submit_p95_ms = enc_p95,
        .frame_wall_avg_ms = fw_avg,
        .frame_wall_p95_ms = fw_p95,
        .finalize_flush_ms = std::chrono::duration<double, std::milli>(fin1 - fin0).count(),
        .total_wall_ms = total_wall,
        .export_fps = total_wall > 1e-3 ? (static_cast<double>(frame_count) / (total_wall * 1e-3)) : 0.0,
        .vram_total_bytes = vram_total,
        .vram_free_before_bytes = vram_free_before,
        .vram_free_min_bytes = vram_free_min,
        .peak_vram_used_bytes = vram_total > vram_free_min ? (vram_total - vram_free_min) : 0,
        .raw_frame_readbacks = readbacks,
        .output_file_bytes = out_bytes,
    };
}

Result<void> preview_scene_native(const Scene& scene, RationalFPS fps, double max_seconds) {
    render::GlContext gl_ctx;
    auto gl_info = gl_ctx.initialize(scene.width(), scene.height(), true, "STEM Engine Native Preview");
    if (!gl_info) return fail(gl_info.error());

    render::Renderer renderer;
    if (auto rr = renderer.initialize(gl_ctx.gl(), scene.width(), scene.height(), 2, 4); !rr) {
        return fail(rr.error());
    }
    if (auto ar = renderer.prepare_scene_assets(scene); !ar) {
        return fail(ar.error());
    }

    gl_ctx.set_vsync(true);
    const double duration = std::max(0.01, scene.duration());
    const double fps_d = std::max(1.0, fps.to_double());
    const double frame_dt = 1.0 / fps_d;

#if defined(_WIN32)
    WinMmPreviewAudio audio;
    if (max_seconds <= 0.0) {
        audio.initialize(scene);
    }
#endif

    const auto t_start = std::chrono::steady_clock::now();
    auto last_tick = t_start;
    auto last_hud = t_start - std::chrono::seconds(1);
    double playhead_sec = 0.0;
    bool playing = true;
    bool muted = false;
    double fps_ema = fps_d;
    SceneSnapshot snap;
    size_t slot = 0;

#if defined(_WIN32)
    if (max_seconds <= 0.0) {
        audio.sync_play(playhead_sec, playing, muted);
    }
#endif

    while (!gl_ctx.should_close() && !gl_ctx.is_key_escape_pressed()) {
        gl_ctx.poll_events();
        const auto now = std::chrono::steady_clock::now();
        const double total_wall = std::chrono::duration<double>(now - t_start).count();
        if (max_seconds > 0.0 && total_wall >= max_seconds) break;

        const double wall_dt = std::clamp(std::chrono::duration<double>(now - last_tick).count(), 0.0001, 0.10);
        last_tick = now;
        fps_ema = 0.90 * fps_ema + 0.10 * (1.0 / wall_dt);

        bool seeked = false;
        if (gl_ctx.consume_key_press(GLFW_KEY_SPACE)) {
            playing = !playing;
#if defined(_WIN32)
            audio.sync_play(playhead_sec, playing, muted);
#endif
        }
        if (gl_ctx.consume_key_press(GLFW_KEY_M)) {
            muted = !muted;
#if defined(_WIN32)
            audio.sync_play(playhead_sec, playing, muted);
#endif
        }
        if (gl_ctx.consume_key_press(GLFW_KEY_R) || gl_ctx.consume_key_press(GLFW_KEY_HOME)) {
            playhead_sec = 0.0;
            seeked = true;
        }
        const double scrub_step = playing ? 2.0 : 0.5;
        if (gl_ctx.consume_key_press(GLFW_KEY_LEFT)) {
            playhead_sec = std::clamp(playhead_sec - scrub_step, 0.0, duration);
            seeked = true;
        }
        if (gl_ctx.consume_key_press(GLFW_KEY_RIGHT)) {
            playhead_sec = std::clamp(playhead_sec + scrub_step, 0.0, duration);
            seeked = true;
        }
        if (gl_ctx.consume_key_press(GLFW_KEY_COMMA)) {
            playing = false;
            playhead_sec = std::clamp(playhead_sec - frame_dt, 0.0, duration);
            seeked = true;
        }
        if (gl_ctx.consume_key_press(GLFW_KEY_PERIOD)) {
            playing = false;
            playhead_sec = std::clamp(playhead_sec + frame_dt, 0.0, duration);
            seeked = true;
        }
        const auto& cues = scene.cues();
        for (int k = 0; k < 9 && static_cast<size_t>(k) < cues.size(); ++k) {
            if (gl_ctx.consume_key_press(GLFW_KEY_1 + k)) {
                playhead_sec = std::clamp(cues[static_cast<size_t>(k)].time_sec, 0.0, duration);
                seeked = true;
            }
        }

        if (playing) {
            playhead_sec += wall_dt;
            if (playhead_sec >= duration) {
                playhead_sec = std::fmod(playhead_sec, duration);
                seeked = true;
            }
        }
#if defined(_WIN32)
        if (seeked && max_seconds <= 0.0) {
            audio.restart_if_playing(playhead_sec, playing, muted);
        }
#endif

        const int64_t q_idx = static_cast<int64_t>(std::floor(playhead_sec * fps_d));
        scene.evaluate(fps.time_at_frame(q_idx), snap);
        if (auto rr = renderer.render(snap, slot); !rr) {
            return fail(rr.error());
        }
        int win_w = 0;
        int win_h = 0;
        gl_ctx.get_framebuffer_size(win_w, win_h);
        renderer.blit_slot_to_default_framebuffer(slot, win_w, win_h);
        gl_ctx.swap_buffers();
        slot = (slot + 1) % renderer.ring_size();

        if (std::chrono::duration<double>(now - last_hud).count() >= 0.10) {
            last_hud = now;
            std::string active_cue;
            for (const auto& c : cues) {
                if (c.time_sec <= playhead_sec + 1e-6) {
                    active_cue = c.cue_id;
                }
            }
            const std::string title = std::format(
                "STEM Native Preview — {:05.2f}s / {:.2f}s [{}]{}{} | {:.1f} fps",
                playhead_sec,
                duration,
                playing ? "PLAY" : "PAUSE",
                muted ? " [MUTE]" : "",
                active_cue.empty() ? "" : (" | " + active_cue),
                fps_ema
            );
            gl_ctx.set_window_title(title.c_str());
        }
    }

#if defined(_WIN32)
    audio.shutdown();
#endif
    renderer.shutdown();
    gl_ctx.shutdown();
    return {};
}

Result<std::vector<uint8_t>> render_single_frame_rgba8_debug(
    const Scene& scene,
    double time_sec,
    int width,
    int height,
    int msaa_samples
) {
    render::GlContext gl_ctx;
    auto gi = gl_ctx.initialize(width, height, false, "STEM Debug Frame");
    if (!gi) return fail(gi.error());

    render::Renderer renderer;
    if (auto rr = renderer.initialize(gl_ctx.gl(), width, height, 1, msaa_samples); !rr) {
        return fail(rr.error());
    }
    if (auto ar = renderer.prepare_scene_assets(scene); !ar) {
        return fail(ar.error());
    }

    SceneSnapshot snap = scene.evaluate(time_sec);
    if (auto rr = renderer.render(snap, 0); !rr) {
        return fail(rr.error());
    }

    std::vector<uint8_t> rgba;
    if (auto rb = renderer.debug_readback_slot_rgba8(0, rgba); !rb) {
        return fail(rb.error());
    }
    renderer.shutdown();
    gl_ctx.shutdown();
    return rgba;
}

Result<ColorVerificationReport> verify_gpu_nv12_against_reference(
    const Scene& scene,
    int width,
    int height
) {
    render::GlContext gl_ctx;
    auto gi = gl_ctx.initialize(width, height, false, "STEM Color Verify");
    if (!gi) return fail(gi.error());

    render::Renderer renderer;
    if (auto rr = renderer.initialize(gl_ctx.gl(), width, height, 1, 1); !rr) {
        return fail(rr.error());
    }
    if (auto ar = renderer.prepare_scene_assets(scene); !ar) {
        return fail(ar.error());
    }

    interop::CudaInteropContext cuda_ctx;
    if (auto ci = cuda_ctx.initialize_for_current_gl_context(); !ci) {
        return fail(ci.error());
    }

    interop::RegisteredGlTexture slot{};
    if (auto reg = cuda_ctx.register_gl_texture(renderer.slot(0).color_tex, width, height, slot); !reg) {
        return fail(reg.error());
    }

    // Allocate a pitched NV12 buffer via FFmpeg AVHWFramesContext to match production layout
    encode::NvencMp4Exporter dummy;
    AVBufferRef* dev_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA);
    auto* hw_dev = reinterpret_cast<AVHWDeviceContext*>(dev_ref->data);
    auto* cuda_dev = reinterpret_cast<AVCUDADeviceContext*>(hw_dev->hwctx);
    cuda_dev->cuda_ctx = cuda_ctx.cu_context();
    cuda_dev->stream = cuda_ctx.cu_stream();
    if (av_hwdevice_ctx_init(dev_ref) < 0) {
        av_buffer_unref(&dev_ref);
        return fail("av_hwdevice_ctx_init failed in color verifier");
    }
    AVBufferRef* frames_ref = av_hwframe_ctx_alloc(dev_ref);
    auto* fc = reinterpret_cast<AVHWFramesContext*>(frames_ref->data);
    fc->format = AV_PIX_FMT_CUDA;
    fc->sw_format = AV_PIX_FMT_NV12;
    fc->width = width;
    fc->height = height;
    fc->initial_pool_size = 1;
    if (av_hwframe_ctx_init(frames_ref) < 0) {
        av_buffer_unref(&frames_ref);
        av_buffer_unref(&dev_ref);
        return fail("av_hwframe_ctx_init failed in color verifier");
    }
    AVFrame* hw_f = av_frame_alloc();
    av_hwframe_get_buffer(frames_ref, hw_f, 0);

    const SceneSnapshot snap = scene.evaluate(0.0);
    if (auto rr = renderer.render(snap, 0); !rr) {
        av_frame_free(&hw_f);
        av_buffer_unref(&frames_ref);
        av_buffer_unref(&dev_ref);
        return fail(rr.error());
    }

    // Readback RGBA8 for CPU IEEE-754 reference comparison
    std::vector<uint8_t> rgba;
    if (auto rb = renderer.debug_readback_slot_rgba8(0, rgba); !rb) {
        av_frame_free(&hw_f);
        av_buffer_unref(&frames_ref);
        av_buffer_unref(&dev_ref);
        return fail(rb.error());
    }

    // Run GPU CUDA conversion kernel on the exact same OpenGL texture
    auto cv = cuda_ctx.convert_gl_texture_to_nv12(
        slot,
        reinterpret_cast<CUdeviceptr>(hw_f->data[0]),
        hw_f->linesize[0],
        reinterpret_cast<CUdeviceptr>(hw_f->data[1]),
        hw_f->linesize[1],
        true,
        snap.bg_bottom,
        false
    );
    if (!cv) {
        av_frame_free(&hw_f);
        av_buffer_unref(&frames_ref);
        av_buffer_unref(&dev_ref);
        return fail(cv.error());
    }

    std::vector<uint8_t> gpu_y;
    std::vector<uint8_t> gpu_uv;
    auto dl = cuda_ctx.download_nv12_for_verification(
        reinterpret_cast<CUdeviceptr>(hw_f->data[0]),
        hw_f->linesize[0],
        reinterpret_cast<CUdeviceptr>(hw_f->data[1]),
        hw_f->linesize[1],
        width,
        height,
        gpu_y,
        gpu_uv
    );
    av_frame_free(&hw_f);
    av_buffer_unref(&frames_ref);
    av_buffer_unref(&dev_ref);
    cuda_ctx.unregister_gl_texture(slot);
    cuda_ctx.shutdown();
    renderer.shutdown();
    gl_ctx.shutdown();
    if (!dl) return fail(dl.error());

    std::vector<uint8_t> ref_y;
    std::vector<uint8_t> ref_uv;
    cpu_rgba8_to_nv12_bt709_reference(rgba.data(), width, height, true, snap.bg_bottom, ref_y, ref_uv);

    ColorVerificationReport rep{};
    rep.width = width;
    rep.height = height;
    int64_t sum_y = 0;
    int64_t sum_uv = 0;
    for (size_t i = 0; i < ref_y.size(); ++i) {
        const int d = std::abs(static_cast<int>(gpu_y[i]) - static_cast<int>(ref_y[i]));
        rep.max_abs_diff_y = std::max(rep.max_abs_diff_y, d);
        sum_y += d;
    }
    for (size_t i = 0; i < ref_uv.size(); ++i) {
        const int d = std::abs(static_cast<int>(gpu_uv[i]) - static_cast<int>(ref_uv[i]));
        rep.max_abs_diff_uv = std::max(rep.max_abs_diff_uv, d);
        sum_uv += d;
    }
    rep.mean_abs_diff_y = static_cast<double>(sum_y) / static_cast<double>(ref_y.size());
    rep.mean_abs_diff_uv = static_cast<double>(sum_uv) / static_cast<double>(ref_uv.size());

    // Sample quadrant centers (top-left, top-right, bottom-left, bottom-right)
    auto sample_y = [&](int x, int y) { return gpu_y[static_cast<size_t>(y) * width + x]; };
    rep.top_left_y = sample_y(width / 4, height / 4);
    rep.top_left_cb = gpu_uv[static_cast<size_t>(height / 8) * width + (width / 8) * 2 + 0];
    rep.top_left_cr = gpu_uv[static_cast<size_t>(height / 8) * width + (width / 8) * 2 + 1];
    rep.top_right_y = sample_y((width * 3) / 4, height / 4);
    rep.bottom_left_y = sample_y(width / 4, (height * 3) / 4);
    rep.bottom_right_y = sample_y((width * 3) / 4, (height * 3) / 4);

    return rep;
}

}  // namespace stem
