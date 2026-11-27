#include "native/doctor.hpp"

#include "color_convert_ptx.hpp"
#include "native/encode/nvenc_encoder.hpp"
#include "native/interop/cuda_drv.hpp"
#include "native/render/gl_context.hpp"
#include "native/text/font_atlas.hpp"
#include "native/text/skia_backend.hpp"

#include <format>

namespace stem {

namespace {

std::string json_escape(const std::string& s) {
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

}  // namespace

DoctorReport run_doctor() {
    DoctorReport rep{};
#if defined(_WIN32)
    rep.os = "Windows x86_64";
#else
    rep.os = "Linux x86_64";
#endif
#ifdef NDEBUG
    rep.build_type = "Release";
#else
    rep.build_type = "Debug/RelWithDebInfo";
#endif
    rep.ptx_compiler = std::string(interop::kColorConvertPtxCompiler);
    rep.skia_compiled = text::SkiaGlBridge::is_compiled_in();
    rep.default_font = text::FontAtlasCache::resolve_default_font_path();

    render::GlContext gl_ctx;
    auto gi = gl_ctx.initialize(256, 256, false, "STEM Doctor");
    if (!gi) {
        rep.notes = std::format("OpenGL init failed: {}", gi.error());
        return rep;
    }
    rep.gl_vendor = gi->vendor;
    rep.gl_renderer = gi->renderer;
    rep.gl_version = gi->version;
    rep.gl_loader_version = gi->loader_version;

    interop::CudaInteropContext cuda_ctx;
    auto ci = cuda_ctx.initialize_for_current_gl_context();
    if (!ci) {
        auto fallback = interop::CudaInteropContext::probe_driver(false);
        if (fallback) {
            rep.cuda_available = true;
            rep.cuda_device_name = fallback->name;
            rep.cuda_driver_version = fallback->driver_version;
            rep.cuda_cc_major = fallback->cc_major;
            rep.cuda_cc_minor = fallback->cc_minor;
            rep.vram_total_mb = fallback->total_vram_bytes / (1024u * 1024u);
            rep.vram_free_mb = fallback->free_vram_bytes / (1024u * 1024u);
        }
        rep.gl_cuda_colocated = false;
        rep.notes = ci.error();
        gl_ctx.shutdown();
        return rep;
    }

    rep.cuda_available = true;
    rep.gl_cuda_colocated = ci->gl_interop_colocated;
    rep.cuda_device_name = ci->name;
    rep.cuda_driver_version = ci->driver_version;
    rep.cuda_cc_major = ci->cc_major;
    rep.cuda_cc_minor = ci->cc_minor;
    rep.vram_total_mb = ci->total_vram_bytes / (1024u * 1024u);
    rep.vram_free_mb = ci->free_vram_bytes / (1024u * 1024u);

    auto nv = encode::NvencMp4Exporter::probe_nvenc_support(cuda_ctx);
    if (nv) {
        rep.nvenc_ok = true;
        rep.nvenc_detail = *nv;
    } else {
        rep.nvenc_ok = false;
        rep.nvenc_detail = nv.error();
        rep.notes = nv.error();
    }

    cuda_ctx.shutdown();
    gl_ctx.shutdown();

    rep.ok = rep.cuda_available && rep.gl_cuda_colocated && rep.nvenc_ok;
    return rep;
}

std::string doctor_report_to_json(const DoctorReport& rep) {
    return std::format(
        "{{\n"
        "  \"ok\": {},\n"
        "  \"os\": \"{}\",\n"
        "  \"build_type\": \"{}\",\n"
        "  \"gl_vendor\": \"{}\",\n"
        "  \"gl_renderer\": \"{}\",\n"
        "  \"gl_version\": \"{}\",\n"
        "  \"gl_loader_version\": \"{}\",\n"
        "  \"cuda_available\": {},\n"
        "  \"gl_cuda_colocated\": {},\n"
        "  \"cuda_device_name\": \"{}\",\n"
        "  \"cuda_driver_version\": {},\n"
        "  \"cuda_compute_capability\": \"{}.{}\",\n"
        "  \"vram_total_mb\": {},\n"
        "  \"vram_free_mb\": {},\n"
        "  \"ptx_compiler\": \"{}\",\n"
        "  \"nvenc_ok\": {},\n"
        "  \"nvenc_detail\": \"{}\",\n"
        "  \"skia_compiled\": {},\n"
        "  \"default_font\": \"{}\",\n"
        "  \"notes\": \"{}\"\n"
        "}}",
        rep.ok ? "true" : "false",
        json_escape(rep.os),
        json_escape(rep.build_type),
        json_escape(rep.gl_vendor),
        json_escape(rep.gl_renderer),
        json_escape(rep.gl_version),
        json_escape(rep.gl_loader_version),
        rep.cuda_available ? "true" : "false",
        rep.gl_cuda_colocated ? "true" : "false",
        json_escape(rep.cuda_device_name),
        rep.cuda_driver_version,
        rep.cuda_cc_major,
        rep.cuda_cc_minor,
        rep.vram_total_mb,
        rep.vram_free_mb,
        json_escape(rep.ptx_compiler),
        rep.nvenc_ok ? "true" : "false",
        json_escape(rep.nvenc_detail),
        rep.skia_compiled ? "true" : "false",
        json_escape(rep.default_font),
        json_escape(rep.notes)
    );
}

std::string doctor_report_to_text(const DoctorReport& rep) {
    return std::format(
        "=== STEM Engine Environment Doctor ===\n"
        "Status          : {}\n"
        "OS / Build      : {} ({})\n"
        "OpenGL Vendor   : {}\n"
        "OpenGL Renderer : {}\n"
        "OpenGL Version  : {} (loader: {})\n"
        "CUDA GPU        : {} (SM {}.{}, driver {}, VRAM {}/{} MiB free)\n"
        "GL-CUDA Interop : {}\n"
        "PTX Compiler    : {}\n"
        "NVENC / FFmpeg  : {} ({})\n"
        "Skia Ganesh     : {}\n"
        "Default Font    : {}\n"
        "Notes           : {}\n",
        rep.ok ? "READY" : "BLOCKED",
        rep.os,
        rep.build_type,
        rep.gl_vendor,
        rep.gl_renderer,
        rep.gl_version,
        rep.gl_loader_version,
        rep.cuda_device_name,
        rep.cuda_cc_major,
        rep.cuda_cc_minor,
        rep.cuda_driver_version,
        rep.vram_free_mb,
        rep.vram_total_mb,
        rep.gl_cuda_colocated ? "COLOCATED (same NVIDIA GPU)" : "MISMATCH / UNAVAILABLE",
        rep.ptx_compiler,
        rep.nvenc_ok ? "OK" : "FAILED",
        rep.nvenc_detail,
        rep.skia_compiled ? "enabled" : "native GLSL+SVG+TrueType path active (Skia requires MSVC ABI)",
        rep.default_font.empty() ? "none" : rep.default_font,
        rep.notes.empty() ? "none" : rep.notes
    );
}

}  // namespace stem
