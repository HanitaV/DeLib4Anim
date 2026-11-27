#pragma once

#include <string>

namespace stem {

struct DoctorReport {
    bool ok = false;
    std::string os;
    std::string build_type;
    std::string gl_vendor;
    std::string gl_renderer;
    std::string gl_version;
    std::string gl_loader_version;
    bool cuda_available = false;
    bool gl_cuda_colocated = false;
    std::string cuda_device_name;
    int cuda_driver_version = 0;
    int cuda_cc_major = 0;
    int cuda_cc_minor = 0;
    size_t vram_total_mb = 0;
    size_t vram_free_mb = 0;
    std::string ptx_compiler;
    bool nvenc_ok = false;
    std::string nvenc_detail;
    bool skia_compiled = false;
    std::string default_font;
    std::string notes;
};

[[nodiscard]] DoctorReport run_doctor();
[[nodiscard]] std::string doctor_report_to_json(const DoctorReport& rep);
[[nodiscard]] std::string doctor_report_to_text(const DoctorReport& rep);

}  // namespace stem
