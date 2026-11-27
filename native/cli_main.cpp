#include "native/benchmark.hpp"
#include "native/doctor.hpp"
#include "native/export_pipeline.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#if defined(_WIN32)
#include <windows.h>
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main(int argc, char** argv) {
    const std::string_view cmd = argc >= 2 ? std::string_view(argv[1]) : "doctor";

    if (cmd == "doctor") {
        const auto rep = stem::run_doctor();
        if (argc >= 3 && std::string_view(argv[2]) == "--json") {
            std::cout << stem::doctor_report_to_json(rep) << '\n';
        } else {
            std::cout << stem::doctor_report_to_text(rep);
        }
        return rep.ok ? 0 : 1;
    }

    if (cmd == "milestone1") {
        std::string out_path = "outputs/milestone1_10s.mp4";
        int width = 1920;
        int height = 1080;
        int fps = 60;
        double dur = 10.0;
        for (int i = 2; i < argc; ++i) {
            const std::string_view arg(argv[i]);
            if (arg == "--output" && i + 1 < argc) out_path = argv[++i];
            else if (arg == "--width" && i + 1 < argc) width = std::atoi(argv[++i]);
            else if (arg == "--height" && i + 1 < argc) height = std::atoi(argv[++i]);
            else if (arg == "--fps" && i + 1 < argc) fps = std::atoi(argv[++i]);
            else if (arg == "--duration" && i + 1 < argc) dur = std::atof(argv[++i]);
        }

        const stem::Scene sc = stem::Scene::make_milestone1_showcase(width, height, dur);
        stem::ExportOptions opts;
        opts.output_path = out_path;
        opts.width = width;
        opts.height = height;
        opts.fps = stem::RationalFPS{fps, 1};
        opts.end_sec = dur;

        auto res = stem::export_scene_range(sc, opts);
        if (!res) {
            std::cerr << "Milestone 1 export failed: " << res.error() << '\n';
            return 1;
        }
        std::cout << "Exported: " << out_path << '\n'
                  << "Frames: " << res->frame_count
                  << " | Wall: " << res->total_wall_ms << " ms"
                  << " | Throughput: " << res->export_fps << " fps"
                  << " | RawReadbacks: " << res->raw_frame_readbacks
                  << " | GL GPU avg: " << res->gl_gpu_avg_ms << " ms"
                  << " | CUDA avg: " << res->cuda_convert_avg_ms << " ms"
                  << " | Size: " << res->output_file_bytes << " bytes\n";
        return 0;
    }

    if (cmd == "preview-milestone1") {
        const stem::Scene sc = stem::Scene::make_milestone1_showcase(1280, 720, 10.0);
        auto res = stem::preview_scene_native(sc, {60, 1}, 6.0);
        if (!res) {
            std::cerr << "Preview failed: " << res.error() << '\n';
            return 1;
        }
        return 0;
    }

    if (cmd == "test-calibration") {
        const stem::Scene cal = stem::Scene::make_color_calibration_scene(640, 360);
        auto rep = stem::verify_gpu_nv12_against_reference(cal, 640, 360);
        if (!rep) {
            std::cerr << "Calibration failed: " << rep.error() << '\n';
            return 1;
        }
        std::cout << "NV12 BT.709 Verification (" << rep->width << "x" << rep->height << "):\n"
                  << "  max_abs_diff_y=" << rep->max_abs_diff_y
                  << " mean_abs_diff_y=" << rep->mean_abs_diff_y << '\n'
                  << "  max_abs_diff_uv=" << rep->max_abs_diff_uv
                  << " mean_abs_diff_uv=" << rep->mean_abs_diff_uv << '\n'
                  << "  TL(Red) Y=" << static_cast<int>(rep->top_left_y)
                  << " Cb=" << static_cast<int>(rep->top_left_cb)
                  << " Cr=" << static_cast<int>(rep->top_left_cr) << '\n'
                  << "  TR(Green) Y=" << static_cast<int>(rep->top_right_y)
                  << " | BL(Blue) Y=" << static_cast<int>(rep->bottom_left_y)
                  << " | BR(White) Y=" << static_cast<int>(rep->bottom_right_y) << '\n';
        return (rep->max_abs_diff_y <= 2 && rep->max_abs_diff_uv <= 2) ? 0 : 1;
    }

    if (cmd == "benchmark") {
        std::string suite = "baseline";
        std::string label = "cli";
        std::string out_json = "outputs/benchmark_cli.json";
        int runs = 3;
        bool include_4k = false;
        for (int i = 2; i < argc; ++i) {
            const std::string_view arg(argv[i]);
            if (arg == "--suite" && i + 1 < argc) suite = argv[++i];
            else if (arg == "--label" && i + 1 < argc) label = argv[++i];
            else if (arg == "--output" && i + 1 < argc) out_json = argv[++i];
            else if (arg == "--runs" && i + 1 < argc) runs = std::atoi(argv[++i]);
            else if (arg == "--include-4k") include_4k = true;
        }
        auto rep = stem::run_benchmark_suite(nullptr, suite, label, runs, include_4k);
        if (!rep) {
            std::cerr << "Benchmark failed: " << rep.error() << '\n';
            return 1;
        }
        const std::string js = stem::benchmark_report_to_json(*rep);
        std::filesystem::path jp(out_json);
        if (jp.has_parent_path()) std::filesystem::create_directories(jp.parent_path());
        std::ofstream(jp, std::ios::binary) << js;
        std::cout << js << '\n';
        return 0;
    }

    std::cerr << "Usage: stem_engine_cli <doctor|milestone1|preview-milestone1|test-calibration|benchmark>\n";
    return 2;
}
