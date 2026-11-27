#include "native/core/scene.hpp"
#include "native/doctor.hpp"
#include "native/export_pipeline.hpp"
#include "native/render/gl_context.hpp"
#include "native/text/skia_backend.hpp"
#include "native/text/svg_path.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace {

int g_failures = 0;

void check(bool cond, const std::string& msg) {
    if (!cond) {
        std::cerr << "[FAIL] " << msg << '\n';
        ++g_failures;
    } else {
        std::cout << "[PASS] " << msg << '\n';
    }
}

void test_rational_clock_and_seek_determinism() {
    stem::RationalFPS fps{60, 1};
    check(fps.frame_count_for_range(0.0, 10.0) == 600, "10s @ 60fps produces exactly 600 frames");
    check(std::abs(fps.time_at_frame(300) - 5.0) < 1e-12, "Frame 300 timestamp is exact 5.0s");

    const stem::Scene sc = stem::Scene::make_milestone1_showcase(1280, 720, 10.0);
    // Evaluate out-of-order: 0 -> 5 -> 2 -> 5
    const auto s0 = sc.evaluate(0.0);
    const auto s5a = sc.evaluate(5.0);
    const auto s2 = sc.evaluate(2.0);
    const auto s5b = sc.evaluate(5.0);
    check(s0.state_fingerprint() != s5a.state_fingerprint(), "Scene state changes between t=0 and t=5");
    check(s2.state_fingerprint() != s5a.state_fingerprint(), "Scene state changes between t=2 and t=5");
    check(
        s5a.state_fingerprint() == s5b.state_fingerprint(),
        "Stateless seek order (0 -> 5 -> 2 -> 5) restores exact scene state at t=5"
    );
}

void test_svg_path_parser() {
    const std::string svg = R"SVG(
        <svg viewBox="0 0 100 100">
            <defs><path id="box" d="M 10 10 L 90 10 L 90 90 L 10 90 Z M 30 30 L 30 70 L 70 70 L 70 30 Z"/></defs>
            <use xlink:href="#box" fill="#ffffff"/>
        </svg>
    )SVG";
    auto pa = stem::text::parse_svg_to_path_asset(svg, "test_box_hole", 48.0f);
    check(pa.has_value(), "SVG parser handles <defs> + <use> + compound hole path");
    if (pa) {
        check(!pa->vertices.empty() && !pa->indices.empty(), "SVG path asset produced triangles");
    }
}

void test_gpu_nv12_calibration_and_repeated_export() {
    const auto doc = stem::run_doctor();
    check(doc.ok, "Doctor confirms NVIDIA OpenGL + CUDA colocation + NVENC");
    if (!doc.ok) {
        std::cerr << stem::doctor_report_to_text(doc) << '\n';
        return;
    }

    const stem::Scene cal = stem::Scene::make_color_calibration_scene(640, 360);
    auto rep = stem::verify_gpu_nv12_against_reference(cal, 640, 360);
    check(rep.has_value(), "GPU CUDA RGBA8->NV12 BT.709 calibration ran cleanly");
    if (rep) {
        check(
            rep->max_abs_diff_y <= 2 && rep->max_abs_diff_uv <= 2,
            "CUDA NV12 BT.709 matches CPU IEEE-754 reference within +/-2 LSB"
        );
        // Orientation check: Top-Left=Red(Y~63, Cr~240), Top-Right=Green(Y~173), Bottom-Left=Blue(Y~32), Bottom-Right=White(Y~235)
        check(
            rep->top_left_y < 80 && rep->top_left_cr > 225 &&
            rep->top_right_y > 160 && rep->top_right_y < 190 &&
            rep->bottom_left_y < 45 &&
            rep->bottom_right_y >= 233,
            "Orientation & BT.709 primaries (TL=Red, TR=Green, BL=Blue, BR=White) verified"
        );
    }

    // Repeated short export & clean shutdown test (resource lifetime)
    const stem::Scene short_sc = stem::Scene::make_milestone1_showcase(640, 360, 0.5);
    const std::string p1 = "outputs/test_repeat_1.mp4";
    const std::string p2 = "outputs/test_repeat_2.mp4";
    stem::ExportOptions opts;
    opts.width = 640;
    opts.height = 360;
    opts.fps = {30, 1};
    opts.end_sec = 0.5;

    opts.output_path = p1;
    auto r1 = stem::export_scene_range(short_sc, opts);
    check(r1.has_value() && r1->frame_count == 15 && r1->raw_frame_readbacks == 0, "Repeated export #1 succeeded with 0 CPU readbacks");

    opts.output_path = p2;
    auto r2 = stem::export_scene_range(short_sc, opts);
    check(r2.has_value() && r2->frame_count == 15 && r2->raw_frame_readbacks == 0, "Repeated export #2 succeeded cleanly after shutdown");

    std::error_code ec;
    std::filesystem::remove(p1, ec);
    std::filesystem::remove(p2, ec);
}

void test_skia_ganesh_bridge() {
#ifdef STEM_HAS_SKIA
    check(stem::text::SkiaGlBridge::is_compiled_in(), "SkiaGlBridge reports compiled-in");
    stem::render::GlContext gl;
    auto gi = gl.initialize(256, 256, false, "SkiaTest");
    check(gi.has_value(), "GL context initialized for Skia Ganesh bridge test");
    if (gi) {
        stem::text::SkiaGlBridge bridge;
        auto att = bridge.attach_to_current_gl(256, 256, 0);
        check(att.has_value(), "SkiaGlBridge::attach_to_current_gl succeeded on OpenGL 4.5 context");
        bridge.reset_gl_state_before_skia();
        bridge.flush_and_submit();
        bridge.shutdown();
        gl.shutdown();
    }
#else
    check(!stem::text::SkiaGlBridge::is_compiled_in(), "SkiaGlBridge reports disabled in non-Skia build");
#endif
}

}  // namespace

void test_physics_easing_and_presentation_invariance() {
    // 1. Impulse easing starts at 0, peaks around 1.0, and returns cleanly to 0.
    check(std::abs(stem::eval_easing(stem::EasingType::Impulse, 0.0, 14.0, 0.56)) < 1e-9, "Impulse(0) == 0");
    check(std::abs(stem::eval_easing(stem::EasingType::Impulse, 1.0, 14.0, 0.56)) < 1e-9, "Impulse(1) == 0");
    double peak_val = 0.0;
    for (int i = 1; i < 100; ++i) {
        peak_val = std::max(peak_val, stem::eval_easing(stem::EasingType::Impulse, i / 100.0, 14.0, 0.56));
    }
    check(peak_val > 0.85 && peak_val <= 1.05, "Impulse peaks near 1.0");

    // 2. BackOut overshoots above 1.0 cleanly and settles to 1.0.
    check(stem::eval_easing(stem::EasingType::BackOut, 0.65, 14.0, 0.65) > 1.0, "BackOut overshoots > 1.0");
    check(std::abs(stem::eval_easing(stem::EasingType::BackOut, 1.0, 14.0, 0.65) - 1.0) < 1e-9, "BackOut(1) == 1");

    // 3. Track<Vec2> with Impulse returns to v0 after t1.
    stem::Vec2Track trk{stem::Vec2{0.0f, 0.0f}};
    trk.add_segment(1.0, 1.5, stem::Vec2{0.0f, 0.0f}, stem::Vec2{12.0f, -4.0f}, stem::EasingType::Impulse, 14.0, 0.56);
    const stem::Vec2 mid = trk.evaluate(1.045);
    const stem::Vec2 settled = trk.evaluate(1.80);
    check(mid.x > 9.0f && mid.y < -3.0f, "Impulse track displaces near peak during active window");
    check(std::abs(settled.x) < 1e-5f && std::abs(settled.y) < 1e-5f, "Impulse track settles back to v0 after t1");

    // 4. Card25D + elevation + presentation invariance determinism.
    stem::Scene sc(1280, 720, 4.0);
    stem::RectItem card;
    card.material_mode = 4;
    card.elevation.add_segment(0.2, 0.6, 0.0f, 16.0f, stem::EasingType::QuinticOut);
    card.presentation_offset.add_segment(0.2, 0.6, stem::Vec2{0.0f, -14.0f}, stem::Vec2{0.0f, 0.0f}, stem::EasingType::QuinticOut);
    sc.add_rect(std::move(card));
    const auto a = sc.evaluate(0.35);
    const auto b = sc.evaluate(0.35);
    check(a.state_fingerprint() == b.state_fingerprint(), "Card25D elevation snapshot fingerprint is deterministic");
    check(a.rects.size() == 1 && a.rects[0].elevation > 0.0f, "Card25D elevation evaluates > 0 during active reveal");
}

int main() {
    test_rational_clock_and_seek_determinism();
    test_svg_path_parser();
    test_physics_easing_and_presentation_invariance();
    test_gpu_nv12_calibration_and_repeated_export();
    test_skia_ganesh_bridge();
    std::cout << "Native test summary: " << g_failures << " failure(s)\n";
    return g_failures == 0 ? 0 : 1;
}
