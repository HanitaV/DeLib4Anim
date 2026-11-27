#pragma once

#include "native/core/scene.hpp"
#include "native/export_pipeline.hpp"

#include <string>
#include <vector>

namespace stem {

struct WorkloadResult {
    std::string name;
    std::string mode;  // "render_only" or "full_export"
    int width = 1920;
    int height = 1080;
    double target_fps = 60.0;
    int runs = 3;
    ExportMetrics median_metrics{};
    double min_wall_ms = 0.0;
    double max_wall_ms = 0.0;
    double median_export_fps = 0.0;
};

struct BenchmarkSuiteReport {
    std::string suite_name;
    std::string label;
    std::string host_os;
    std::string cpu_compiler;
    std::string gpu_name;
    std::string gl_version;
    int cuda_driver_version = 0;
    std::string build_type;
    std::string codec = "h264_nvenc (NV12 BT.709)";
    std::string nvenc_preset = "p4";
    int rc_lookahead = 0;
    std::vector<WorkloadResult> workloads;
};

[[nodiscard]] Scene make_benchmark_workload_static_assets(int width = 1920, int height = 1080, double dur = 10.0);
[[nodiscard]] Scene make_benchmark_workload_dynamic_curves(int width = 1920, int height = 1080, double dur = 10.0);
[[nodiscard]] Scene make_benchmark_workload_particles(int width = 1920, int height = 1080, double dur = 10.0);

[[nodiscard]] Result<BenchmarkSuiteReport> run_benchmark_suite(
    const Scene* custom_demo_scene,
    const std::string& suite_name = "baseline",
    const std::string& label = "baseline",
    int runs = 3,
    bool include_4k = false,
    const std::string& out_dir = "outputs/bench_tmp",
    int interop_batch_size = 8
);

[[nodiscard]] std::string benchmark_report_to_json(const BenchmarkSuiteReport& rep);
[[nodiscard]] std::string benchmark_report_to_csv(const BenchmarkSuiteReport& rep);

}  // namespace stem
