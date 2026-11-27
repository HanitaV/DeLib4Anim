#include "native/benchmark.hpp"

#include "native/text/svg_path.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <numbers>

namespace stem {

namespace {

constexpr float kPiF = std::numbers::pi_v<float>;

std::string json_esc(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

// Sample SVG formula geometry with interior holes (e.g. '0', 'O', ring) for Workload 1
const char* kBenchSvgFormula = R"SVG(
<svg viewBox="0 0 400 120">
  <g fill="#f2f6ff">
    <path d="M 20 20 L 100 20 L 100 100 L 20 100 Z M 40 40 L 40 80 L 80 80 L 80 40 Z"/>
    <path d="M 130 20 C 175 20 190 45 190 60 C 190 75 175 100 130 100 L 130 20 Z M 148 38 L 148 82 C 166 82 172 70 172 60 C 172 50 166 38 148 38 Z"/>
    <path d="M 220 55 L 360 55 L 360 65 L 220 65 Z"/>
    <path d="M 220 20 L 360 20 L 360 35 L 220 35 Z"/>
    <path d="M 220 85 L 360 85 L 360 100 L 220 100 Z"/>
  </g>
</svg>
)SVG";

}  // namespace

Scene make_benchmark_workload_static_assets(int width, int height, double dur) {
    Scene sc(width, height, dur);
    sc.background().top_color.set_constant({0.055f, 0.080f, 0.130f, 1.0f});
    sc.background().bottom_color.set_constant({0.020f, 0.028f, 0.050f, 1.0f});

    auto asset_res = text::parse_svg_to_path_asset(kBenchSvgFormula, "bench_svg", 56.0f);
    ObjectId aid = kInvalidId;
    if (asset_res) {
        aid = sc.register_path_asset(*asset_res);
    }

    for (int i = 0; i < 24; ++i) {
        const float col = static_cast<float>(i % 6) - 2.5f;
        const float row = static_cast<float>(i / 6) - 1.5f;
        if (aid != kInvalidId) {
            PathInstanceItem pi;
            pi.asset_id = aid;
            pi.z_index = 10 + i;
            pi.position.add_segment(
                0.0,
                dur,
                Vec2{col * 250.0f, row * 190.0f},
                Vec2{col * 250.0f + 45.0f, row * 190.0f + 24.0f},
                EasingType::QuinticInOut
            );
            pi.scale.add_segment(0.0, dur, 0.85f, 1.15f, EasingType::Smoothstep);
            pi.rotation_rad.add_segment(0.0, dur, -0.06f, 0.06f, EasingType::Linear);
            sc.add_path_instance(std::move(pi));
        }

        TextItem ti;
        ti.screen_space = false;
        ti.z_index = 40 + i;
        ti.text = std::format("F_{} = {:.2f} N", i + 1, 9.81 * (i + 1));
        ti.font_size_px = 28.0f;
        ti.position.add_segment(
            0.0,
            dur,
            Vec2{col * 250.0f, row * 190.0f - 52.0f},
            Vec2{col * 250.0f + 45.0f, row * 190.0f - 28.0f},
            EasingType::QuinticInOut
        );
        sc.add_text(std::move(ti));
    }
    return sc;
}

Scene make_benchmark_workload_dynamic_curves(int width, int height, double dur) {
    Scene sc(width, height, dur);
    sc.background().show_grid = true;
    sc.background().grid_spacing = 100.0f;

    for (int c = 0; c < 6; ++c) {
        GraphItem g;
        g.z_index = 5 + c;
        g.stroke_width.set_constant(4.0f);
        g.deform_amplitude.set_constant(26.0f + c * 6.0f);
        g.deform_freq = 0.015f + c * 0.004f;
        g.deform_speed = 3.5f + c * 0.7f;
        g.origin.set_constant({0.0f, (static_cast<float>(c) - 2.5f) * 130.0f});
        g.color.set_constant({0.25f + 0.12f * c, 0.78f - 0.06f * c, 0.96f, 0.92f});
        for (int p = 0; p < 160; ++p) {
            const float x = -720.0f + static_cast<float>(p) * (1440.0f / 159.0f);
            g.points.push_back(Vec2{x, 0.0f});
        }
        sc.add_graph(std::move(g));
    }

    for (int s = 0; s < 4; ++s) {
        StickmanItem sm;
        sm.z_index = 20 + s;
        sm.hip_pos.add_segment(
            0.0,
            dur,
            Vec2{-550.0f + s * 360.0f, -220.0f},
            Vec2{-490.0f + s * 360.0f, -200.0f},
            EasingType::Smoothstep
        );
        sm.arm_r_upper_rad.add_segment(0.0, dur, -1.0f, 0.45f, EasingType::Spring, 14.0, 0.55);
        sm.arm_r_lower_rad.add_segment(0.0, dur, -0.5f, 0.65f, EasingType::Spring, 14.0, 0.55);
        sm.torso_lean_rad.add_segment(0.0, dur, -0.1f, 0.18f, EasingType::QuinticInOut);
        sc.add_stickman(std::move(sm));
    }
    return sc;
}

Scene make_benchmark_workload_particles(int width, int height, double dur) {
    Scene sc(width, height, dur);
    sc.background().top_color.set_constant({0.05f, 0.06f, 0.11f, 1.0f});
    sc.background().bottom_color.set_constant({0.015f, 0.02f, 0.04f, 1.0f});

    for (int i = 0; i < 16; ++i) {
        RectItem card;
        card.z_index = i;
        card.center.add_segment(
            0.0,
            dur,
            Vec2{static_cast<float>((i % 4) - 1.5) * 320.0f, static_cast<float>((i / 4) - 1.5) * 200.0f},
            Vec2{static_cast<float>((i % 4) - 1.5) * 320.0f + 40.0f, static_cast<float>((i / 4) - 1.5) * 200.0f},
            EasingType::Smoothstep
        );
        card.size.set_constant({260.0f, 150.0f});
        card.corner_radius.set_constant(18.0f);
        card.stroke_width.set_constant(2.5f);
        card.fill_color.set_constant({0.14f, 0.25f + 0.04f * (i % 4), 0.48f, 0.42f});
        card.stroke_color.set_constant({0.55f, 0.80f, 1.0f, 0.45f});
        sc.add_rect(std::move(card));
    }

    ParticlePoolItem p1;
    p1.z_index = 20;
    p1.seed = 4242u;
    p1.count = 512u;
    p1.spread = Vec2{1600.0f, 900.0f};
    p1.base_velocity = Vec2{60.0f, 35.0f};
    p1.min_radius = 3.0f;
    p1.max_radius = 14.0f;
    p1.color.set_constant({0.35f, 0.78f, 1.0f, 0.48f});
    sc.add_particle_pool(std::move(p1));

    ParticlePoolItem p2;
    p2.z_index = 21;
    p2.seed = 9001u;
    p2.count = 256u;
    p2.spread = Vec2{1400.0f, 800.0f};
    p2.base_velocity = Vec2{-40.0f, 50.0f};
    p2.min_radius = 4.0f;
    p2.max_radius = 18.0f;
    p2.color.set_constant({1.0f, 0.65f, 0.25f, 0.35f});
    sc.add_particle_pool(std::move(p2));

    return sc;
}

namespace {

Result<WorkloadResult> run_one_workload(
    const Scene& scene,
    const std::string& name,
    int width,
    int height,
    bool render_only,
    int runs,
    const std::string& out_dir,
    int interop_batch_size
) {
    ExportOptions opts;
    opts.width = width;
    opts.height = height;
    opts.fps = RationalFPS{60, 1};
    opts.start_sec = 0.0;
    opts.end_sec = scene.duration();
    opts.render_only = render_only;
    opts.interop_batch_size = interop_batch_size;
    opts.ring_size = std::max(3, interop_batch_size);
    opts.enable_audio = !render_only;
    opts.output_path = std::format("{}/{}_{}x{}.mp4", out_dir, name, width, height);

    // Warmup run (not recorded in timed runs)
    if (auto w = export_scene_range(scene, opts); !w) {
        return fail(w.error());
    }

    std::vector<ExportMetrics> samples;
    samples.reserve(static_cast<size_t>(runs));
    for (int r = 0; r < runs; ++r) {
        auto m = export_scene_range(scene, opts);
        if (!m) return fail(m.error());
        samples.push_back(*m);
    }

    std::ranges::sort(samples, [](const ExportMetrics& a, const ExportMetrics& b) {
        return a.total_wall_ms < b.total_wall_ms;
    });
    const ExportMetrics& med = samples[samples.size() / 2u];
    return WorkloadResult{
        .name = name,
        .mode = render_only ? "render_only" : "full_export",
        .width = width,
        .height = height,
        .target_fps = 60.0,
        .runs = runs,
        .median_metrics = med,
        .min_wall_ms = samples.front().total_wall_ms,
        .max_wall_ms = samples.back().total_wall_ms,
        .median_export_fps = med.export_fps,
    };
}

}  // namespace

Result<BenchmarkSuiteReport> run_benchmark_suite(
    const Scene* custom_demo_scene,
    const std::string& suite_name,
    const std::string& label,
    int runs,
    bool include_4k,
    const std::string& out_dir,
    int interop_batch_size
) {
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);

    const Scene demo = custom_demo_scene ? *custom_demo_scene : Scene::make_milestone1_showcase(1920, 1080, 10.0);
    const Scene w1 = make_benchmark_workload_static_assets(1920, 1080, 10.0);
    const Scene w2 = make_benchmark_workload_dynamic_curves(1920, 1080, 10.0);
    const Scene w3 = make_benchmark_workload_particles(1920, 1080, 10.0);

    BenchmarkSuiteReport rep;
    rep.suite_name = suite_name;
    rep.label = label;
#if defined(_WIN32)
    rep.host_os = "Windows x86_64";
#else
    rep.host_os = "Linux x86_64";
#endif
#if defined(__GNUC__)
    rep.cpu_compiler = std::format("GCC {}.{}.{}", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    rep.cpu_compiler = std::format("MSVC {}", _MSC_VER);
#else
    rep.cpu_compiler = "Unknown";
#endif

    struct Spec {
        std::string name;
        const Scene* sc;
        int w;
        int h;
        bool render_only;
    };
    std::vector<Spec> specs = {
        {"1_static_glyph_path_1080p60", &w1, 1920, 1080, false},
        {"2_dynamic_curves_stickman_1080p60", &w2, 1920, 1080, false},
        {"3_particles_compositing_1080p60", &w3, 1920, 1080, false},
        {"4_demo_render_only_1080p60", &demo, 1920, 1080, true},
        {"5_demo_full_export_1080p60", &demo, 1920, 1080, false},
    };
    if (include_4k) {
        specs.push_back({"6_demo_full_export_4k60", &demo, 3840, 2160, false});
    }

    for (const auto& sp : specs) {
        auto wr = run_one_workload(*sp.sc, sp.name, sp.w, sp.h, sp.render_only, std::max(1, runs), out_dir, interop_batch_size);
        if (!wr) return fail(wr.error());
        if (rep.gpu_name.empty() && !wr->median_metrics.cuda_device_name.empty()) {
            rep.gpu_name = wr->median_metrics.cuda_device_name;
            rep.cuda_driver_version = wr->median_metrics.cuda_driver_version;
        }
        if (rep.gl_version.empty()) {
            rep.gl_version = wr->median_metrics.gl_version;
            rep.build_type = wr->median_metrics.build_type;
        }
        rep.workloads.push_back(std::move(*wr));
    }

    return rep;
}

std::string benchmark_report_to_json(const BenchmarkSuiteReport& rep) {
    std::string out = std::format(
        "{{\n"
        "  \"suite\": \"{}\",\n"
        "  \"label\": \"{}\",\n"
        "  \"host_os\": \"{}\",\n"
        "  \"compiler\": \"{}\",\n"
        "  \"build_type\": \"{}\",\n"
        "  \"gpu\": \"{}\",\n"
        "  \"gl_version\": \"{}\",\n"
        "  \"cuda_driver\": {},\n"
        "  \"codec\": \"{}\",\n"
        "  \"nvenc_preset\": \"{}\",\n"
        "  \"rc_lookahead\": {},\n"
        "  \"workloads\": [\n",
        json_esc(rep.suite_name),
        json_esc(rep.label),
        json_esc(rep.host_os),
        json_esc(rep.cpu_compiler),
        json_esc(rep.build_type),
        json_esc(rep.gpu_name),
        json_esc(rep.gl_version),
        rep.cuda_driver_version,
        json_esc(rep.codec),
        json_esc(rep.nvenc_preset),
        rep.rc_lookahead
    );

    for (size_t i = 0; i < rep.workloads.size(); ++i) {
        const auto& w = rep.workloads[i];
        const auto& m = w.median_metrics;
        out += std::format(
            "    {{\n"
            "      \"name\": \"{}\",\n"
            "      \"mode\": \"{}\",\n"
            "      \"width\": {},\n"
            "      \"height\": {},\n"
            "      \"target_fps\": {:.1f},\n"
            "      \"frame_count\": {},\n"
            "      \"scene_hash\": \"{:016x}\",\n"
            "      \"runs\": {},\n"
            "      \"cold_startup_ms\": {:.3f},\n"
            "      \"asset_compile_ms\": {:.3f},\n"
            "      \"cpu_eval_avg_ms\": {:.3f},\n"
            "      \"cpu_eval_p95_ms\": {:.3f},\n"
            "      \"gl_gpu_avg_ms\": {:.3f},\n"
            "      \"gl_gpu_p95_ms\": {:.3f},\n"
            "      \"cuda_convert_avg_ms\": {:.3f},\n"
            "      \"cuda_convert_p95_ms\": {:.3f},\n"
            "      \"encode_submit_avg_ms\": {:.3f},\n"
            "      \"encode_submit_p95_ms\": {:.3f},\n"
            "      \"frame_wall_avg_ms\": {:.3f},\n"
            "      \"frame_wall_p95_ms\": {:.3f},\n"
            "      \"finalize_flush_ms\": {:.3f},\n"
            "      \"total_wall_ms\": {:.3f},\n"
            "      \"min_wall_ms\": {:.3f},\n"
            "      \"max_wall_ms\": {:.3f},\n"
            "      \"export_fps\": {:.2f},\n"
            "      \"realtime_x\": {:.2f},\n"
            "      \"peak_vram_used_mb\": {:.1f},\n"
            "      \"raw_frame_readbacks\": {},\n"
            "      \"output_file_bytes\": {}\n"
            "    }}{}\n",
            json_esc(w.name),
            json_esc(w.mode),
            w.width,
            w.height,
            w.target_fps,
            m.frame_count,
            m.scene_hash,
            w.runs,
            m.cold_startup_ms,
            m.asset_compile_ms,
            m.cpu_eval_avg_ms,
            m.cpu_eval_p95_ms,
            m.gl_gpu_avg_ms,
            m.gl_gpu_p95_ms,
            m.cuda_convert_avg_ms,
            m.cuda_convert_p95_ms,
            m.encode_submit_avg_ms,
            m.encode_submit_p95_ms,
            m.frame_wall_avg_ms,
            m.frame_wall_p95_ms,
            m.finalize_flush_ms,
            m.total_wall_ms,
            w.min_wall_ms,
            w.max_wall_ms,
            m.export_fps,
            m.export_fps / w.target_fps,
            static_cast<double>(m.peak_vram_used_bytes) / (1024.0 * 1024.0),
            m.raw_frame_readbacks,
            m.output_file_bytes,
            (i + 1 < rep.workloads.size()) ? "," : ""
        );
    }
    out += "  ]\n}\n";
    return out;
}

std::string benchmark_report_to_csv(const BenchmarkSuiteReport& rep) {
    std::string out =
        "label,workload,mode,resolution,frames,cold_ms,asset_ms,cpu_eval_avg_ms,gl_gpu_avg_ms,"
        "gl_gpu_p95_ms,cuda_avg_ms,cuda_p95_ms,encode_submit_avg_ms,frame_wall_avg_ms,"
        "frame_wall_p95_ms,finalize_ms,total_wall_ms,export_fps,realtime_x,peak_vram_mb,raw_readbacks\n";
    for (const auto& w : rep.workloads) {
        const auto& m = w.median_metrics;
        out += std::format(
            "{},{},{},{}x{},{},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{:.2f},{:.2f},{:.1f},{}\n",
            rep.label,
            w.name,
            w.mode,
            w.width,
            w.height,
            m.frame_count,
            m.cold_startup_ms,
            m.asset_compile_ms,
            m.cpu_eval_avg_ms,
            m.gl_gpu_avg_ms,
            m.gl_gpu_p95_ms,
            m.cuda_convert_avg_ms,
            m.cuda_convert_p95_ms,
            m.encode_submit_avg_ms,
            m.frame_wall_avg_ms,
            m.frame_wall_p95_ms,
            m.finalize_flush_ms,
            m.total_wall_ms,
            m.export_fps,
            m.export_fps / w.target_fps,
            static_cast<double>(m.peak_vram_used_bytes) / (1024.0 * 1024.0),
            m.raw_frame_readbacks
        );
    }
    return out;
}

}  // namespace stem
