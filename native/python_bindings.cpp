#include "native/benchmark.hpp"
#include "native/core/scene.hpp"
#include "native/doctor.hpp"
#include "native/export_pipeline.hpp"
#include "native/text/font_atlas.hpp"
#include "native/text/svg_path.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace py = pybind11;

namespace {

template <typename T>
T unwrap_or_throw(stem::Result<T> res) {
    if (!res) {
        throw std::runtime_error(res.error());
    }
    return std::move(*res);
}

void unwrap_or_throw_void(stem::Result<void> res) {
    if (!res) {
        throw std::runtime_error(res.error());
    }
}

template <typename T>
void bind_track(py::module_& m, const char* name) {
    using TrackT = stem::Track<T>;
    py::class_<TrackT>(m, name)
        .def(py::init<>())
        .def(py::init<T>())
        .def("set_constant", &TrackT::set_constant)
        .def(
            "add_segment",
            &TrackT::add_segment,
            py::arg("t0"),
            py::arg("t1"),
            py::arg("v0"),
            py::arg("v1"),
            py::arg("easing") = stem::EasingType::QuinticInOut,
            py::arg("spring_omega") = 14.0,
            py::arg("spring_zeta") = 0.65
        )
        .def("evaluate", &TrackT::evaluate)
        .def_property_readonly("is_animated", &TrackT::is_animated)
        .def_property_readonly("default_value", &TrackT::default_value);
}

}  // namespace

PYBIND11_MODULE(_stem_native, m) {
    m.doc() = "Native C++20 / OpenGL 4.5 / CUDA / NVENC STEM animation runtime";

    py::class_<stem::Vec2>(m, "Vec2")
        .def(py::init<>())
        .def(py::init<float, float>(), py::arg("x") = 0.0f, py::arg("y") = 0.0f)
        .def_readwrite("x", &stem::Vec2::x)
        .def_readwrite("y", &stem::Vec2::y)
        .def("length", &stem::Vec2::length)
        .def("__repr__", [](const stem::Vec2& v) { return "Vec2(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")"; });

    py::class_<stem::ColorRGBA>(m, "ColorRGBA")
        .def(py::init<>())
        .def(
            py::init<float, float, float, float>(),
            py::arg("r") = 1.0f,
            py::arg("g") = 1.0f,
            py::arg("b") = 1.0f,
            py::arg("a") = 1.0f
        )
        .def_readwrite("r", &stem::ColorRGBA::r)
        .def_readwrite("g", &stem::ColorRGBA::g)
        .def_readwrite("b", &stem::ColorRGBA::b)
        .def_readwrite("a", &stem::ColorRGBA::a);

    py::class_<stem::RationalFPS>(m, "RationalFPS")
        .def(py::init<>())
        .def(py::init<int64_t, int64_t>(), py::arg("num") = 60, py::arg("den") = 1)
        .def_readwrite("num", &stem::RationalFPS::num)
        .def_readwrite("den", &stem::RationalFPS::den)
        .def("time_at_frame", &stem::RationalFPS::time_at_frame)
        .def("frame_count_for_range", &stem::RationalFPS::frame_count_for_range)
        .def("to_double", &stem::RationalFPS::to_double);

    py::enum_<stem::EasingType>(m, "EasingType")
        .value("Step", stem::EasingType::Step)
        .value("Linear", stem::EasingType::Linear)
        .value("Smoothstep", stem::EasingType::Smoothstep)
        .value("QuinticOut", stem::EasingType::QuinticOut)
        .value("QuinticInOut", stem::EasingType::QuinticInOut)
        .value("Spring", stem::EasingType::Spring)
        .value("BackOut", stem::EasingType::BackOut)
        .value("BackIn", stem::EasingType::BackIn)
        .value("ExpoOut", stem::EasingType::ExpoOut)
        .value("Impulse", stem::EasingType::Impulse);

    py::enum_<stem::ProceduralSfxKind>(m, "ProceduralSfxKind")
        .value("None", stem::ProceduralSfxKind::None)
        .value("Click", stem::ProceduralSfxKind::Click)
        .value("Release", stem::ProceduralSfxKind::Release)
        .value("Slide", stem::ProceduralSfxKind::Slide)
        .value("Contact", stem::ProceduralSfxKind::Contact)
        .value("Chime", stem::ProceduralSfxKind::Chime);

    bind_track<float>(m, "ScalarTrack");
    bind_track<stem::Vec2>(m, "Vec2Track");
    bind_track<stem::ColorRGBA>(m, "ColorTrack");

    py::class_<stem::Trajectory2D>(m, "Trajectory2D")
        .def(py::init<>())
        .def_readwrite("id", &stem::Trajectory2D::id)
        .def_readwrite("t_start", &stem::Trajectory2D::t_start)
        .def_readwrite("dt", &stem::Trajectory2D::dt)
        .def_readwrite("samples", &stem::Trajectory2D::samples)
        .def("evaluate_pos", &stem::Trajectory2D::evaluate_pos)
        .def("evaluate_vel", &stem::Trajectory2D::evaluate_vel);

    py::class_<stem::CameraSpec>(m, "CameraSpec")
        .def(py::init<>())
        .def_readwrite("position", &stem::CameraSpec::position)
        .def_readwrite("zoom", &stem::CameraSpec::zoom)
        .def_readwrite("rotation_rad", &stem::CameraSpec::rotation_rad)
        .def_readwrite("follow_trajectory_id", &stem::CameraSpec::follow_trajectory_id)
        .def_readwrite("follow_offset", &stem::CameraSpec::follow_offset)
        .def_readwrite("follow_weight", &stem::CameraSpec::follow_weight)
        .def_readwrite("follow_smoothing_window_sec", &stem::CameraSpec::follow_smoothing_window_sec);

    py::class_<stem::BackgroundSpec>(m, "BackgroundSpec")
        .def(py::init<>())
        .def_readwrite("top_color", &stem::BackgroundSpec::top_color)
        .def_readwrite("bottom_color", &stem::BackgroundSpec::bottom_color)
        .def_readwrite("show_grid", &stem::BackgroundSpec::show_grid)
        .def_readwrite("grid_spacing", &stem::BackgroundSpec::grid_spacing)
        .def_readwrite("grid_color", &stem::BackgroundSpec::grid_color)
        .def_readwrite("starfield_intensity", &stem::BackgroundSpec::starfield_intensity)
        .def_readwrite("vignette_strength", &stem::BackgroundSpec::vignette_strength)
        .def_readwrite("grain_amount", &stem::BackgroundSpec::grain_amount)
        .def_readwrite("perspective_grid_opacity", &stem::BackgroundSpec::perspective_grid_opacity);

    py::class_<stem::RectItem>(m, "RectItem")
        .def(py::init<>())
        .def_readwrite("id", &stem::RectItem::id)
        .def_readwrite("z_index", &stem::RectItem::z_index)
        .def_readwrite("screen_space", &stem::RectItem::screen_space)
        .def_readwrite("material_mode", &stem::RectItem::material_mode)
        .def_readwrite("visible", &stem::RectItem::visible)
        .def_readwrite("center", &stem::RectItem::center)
        .def_readwrite("size", &stem::RectItem::size)
        .def_readwrite("rotation_rad", &stem::RectItem::rotation_rad)
        .def_readwrite("corner_radius", &stem::RectItem::corner_radius)
        .def_readwrite("stroke_width", &stem::RectItem::stroke_width)
        .def_readwrite("fill_color", &stem::RectItem::fill_color)
        .def_readwrite("stroke_color", &stem::RectItem::stroke_color)
        .def_readwrite("presentation_offset", &stem::RectItem::presentation_offset)
        .def_readwrite("presentation_scale", &stem::RectItem::presentation_scale)
        .def_readwrite("presentation_rotation_rad", &stem::RectItem::presentation_rotation_rad)
        .def_readwrite("elevation", &stem::RectItem::elevation)
        .def_readwrite("parallax", &stem::RectItem::parallax)
        .def_readwrite("trajectory_id", &stem::RectItem::trajectory_id);

    py::class_<stem::CircleItem>(m, "CircleItem")
        .def(py::init<>())
        .def_readwrite("id", &stem::CircleItem::id)
        .def_readwrite("z_index", &stem::CircleItem::z_index)
        .def_readwrite("screen_space", &stem::CircleItem::screen_space)
        .def_readwrite("material_mode", &stem::CircleItem::material_mode)
        .def_readwrite("visible", &stem::CircleItem::visible)
        .def_readwrite("center", &stem::CircleItem::center)
        .def_readwrite("radius", &stem::CircleItem::radius)
        .def_readwrite("stroke_width", &stem::CircleItem::stroke_width)
        .def_readwrite("fill_color", &stem::CircleItem::fill_color)
        .def_readwrite("stroke_color", &stem::CircleItem::stroke_color)
        .def_readwrite("presentation_offset", &stem::CircleItem::presentation_offset)
        .def_readwrite("presentation_scale", &stem::CircleItem::presentation_scale)
        .def_readwrite("elevation", &stem::CircleItem::elevation)
        .def_readwrite("parallax", &stem::CircleItem::parallax)
        .def_readwrite("trajectory_id", &stem::CircleItem::trajectory_id);

    py::class_<stem::VectorItem>(m, "VectorItem")
        .def(py::init<>())
        .def_readwrite("id", &stem::VectorItem::id)
        .def_readwrite("z_index", &stem::VectorItem::z_index)
        .def_readwrite("screen_space", &stem::VectorItem::screen_space)
        .def_readwrite("visible", &stem::VectorItem::visible)
        .def_readwrite("origin", &stem::VectorItem::origin)
        .def_readwrite("delta", &stem::VectorItem::delta)
        .def_readwrite("reveal", &stem::VectorItem::reveal)
        .def_readwrite("shaft_width", &stem::VectorItem::shaft_width)
        .def_readwrite("head_length", &stem::VectorItem::head_length)
        .def_readwrite("head_width", &stem::VectorItem::head_width)
        .def_readwrite("color", &stem::VectorItem::color)
        .def_readwrite("presentation_offset", &stem::VectorItem::presentation_offset)
        .def_readwrite("presentation_scale", &stem::VectorItem::presentation_scale)
        .def_readwrite("parallax", &stem::VectorItem::parallax)
        .def_readwrite("attach_trajectory_id", &stem::VectorItem::attach_trajectory_id)
        .def_readwrite("attach_offset", &stem::VectorItem::attach_offset);

    py::class_<stem::GraphItem>(m, "GraphItem")
        .def(py::init<>())
        .def_readwrite("id", &stem::GraphItem::id)
        .def_readwrite("z_index", &stem::GraphItem::z_index)
        .def_readwrite("screen_space", &stem::GraphItem::screen_space)
        .def_readwrite("visible", &stem::GraphItem::visible)
        .def_readwrite("origin", &stem::GraphItem::origin)
        .def_readwrite("presentation_offset", &stem::GraphItem::presentation_offset)
        .def_readwrite("parallax", &stem::GraphItem::parallax)
        .def_readwrite("points", &stem::GraphItem::points)
        .def_readwrite("reveal", &stem::GraphItem::reveal)
        .def_readwrite("stroke_width", &stem::GraphItem::stroke_width)
        .def_readwrite("color", &stem::GraphItem::color)
        .def_readwrite("dash_length", &stem::GraphItem::dash_length)
        .def_readwrite("gap_length", &stem::GraphItem::gap_length)
        .def_readwrite("deform_amplitude", &stem::GraphItem::deform_amplitude)
        .def_readwrite("deform_freq", &stem::GraphItem::deform_freq)
        .def_readwrite("deform_speed", &stem::GraphItem::deform_speed)
        .def_readwrite("deform_envelope_sigma", &stem::GraphItem::deform_envelope_sigma)
        .def_readwrite("fill_under", &stem::GraphItem::fill_under)
        .def_readwrite("fill_baseline_y", &stem::GraphItem::fill_baseline_y)
        .def_readwrite("fill_top_color", &stem::GraphItem::fill_top_color)
        .def_readwrite("fill_bottom_color", &stem::GraphItem::fill_bottom_color)
        .def_readwrite("trail_trajectory_id", &stem::GraphItem::trail_trajectory_id)
        .def_readwrite("trail_duration_sec", &stem::GraphItem::trail_duration_sec);

    py::class_<stem::OrbitalSpec>(m, "OrbitalSpec")
        .def(py::init<>())
        .def(py::init<int, int, int, float>(), py::arg("n") = 1, py::arg("l") = 0, py::arg("m") = 0, py::arg("bohr_radius_px") = 32.0f)
        .def_readwrite("n", &stem::OrbitalSpec::n)
        .def_readwrite("l", &stem::OrbitalSpec::l)
        .def_readwrite("m", &stem::OrbitalSpec::m)
        .def_readwrite("bohr_radius_px", &stem::OrbitalSpec::bohr_radius_px);

    py::class_<stem::OrbitalParticleSample>(m, "OrbitalParticleSample")
        .def_readonly("r_perp_px", &stem::OrbitalParticleSample::r_perp_px)
        .def_readonly("y_px", &stem::OrbitalParticleSample::y_px)
        .def_readonly("phi0_rad", &stem::OrbitalParticleSample::phi0_rad)
        .def_readonly("omega_rad_per_sec", &stem::OrbitalParticleSample::omega_rad_per_sec);

    py::class_<stem::OrbitalCloud3DItem>(m, "OrbitalCloud3DItem")
        .def(py::init<>())
        .def_readwrite("id", &stem::OrbitalCloud3DItem::id)
        .def_readwrite("z_index", &stem::OrbitalCloud3DItem::z_index)
        .def_readwrite("screen_space", &stem::OrbitalCloud3DItem::screen_space)
        .def_readwrite("seed", &stem::OrbitalCloud3DItem::seed)
        .def_readwrite("particle_count", &stem::OrbitalCloud3DItem::particle_count)
        .def_readwrite("state_a", &stem::OrbitalCloud3DItem::state_a)
        .def_readwrite("state_b", &stem::OrbitalCloud3DItem::state_b)
        .def_readwrite("morph", &stem::OrbitalCloud3DItem::morph)
        .def_readwrite("visible", &stem::OrbitalCloud3DItem::visible)
        .def_readwrite("reveal", &stem::OrbitalCloud3DItem::reveal)
        .def_readwrite("opacity", &stem::OrbitalCloud3DItem::opacity)
        .def_readwrite("center", &stem::OrbitalCloud3DItem::center)
        .def_readwrite("scale", &stem::OrbitalCloud3DItem::scale)
        .def_readwrite("yaw_rad", &stem::OrbitalCloud3DItem::yaw_rad)
        .def_readwrite("pitch_rad", &stem::OrbitalCloud3DItem::pitch_rad)
        .def_readwrite("fov_persp", &stem::OrbitalCloud3DItem::fov_persp)
        .def_readwrite("flow_speed", &stem::OrbitalCloud3DItem::flow_speed)
        .def_readwrite("collapse", &stem::OrbitalCloud3DItem::collapse)
        .def_readwrite("collapse_target", &stem::OrbitalCloud3DItem::collapse_target)
        .def_readwrite("dot_radius_px", &stem::OrbitalCloud3DItem::dot_radius_px)
        .def_readwrite("color", &stem::OrbitalCloud3DItem::color);

    py::class_<stem::StickmanItem>(m, "StickmanItem")
        .def(py::init<>())
        .def_readwrite("id", &stem::StickmanItem::id)
        .def_readwrite("z_index", &stem::StickmanItem::z_index)
        .def_readwrite("screen_space", &stem::StickmanItem::screen_space)
        .def_readwrite("solid_head", &stem::StickmanItem::solid_head)
        .def_readwrite("depth_shade_back_limbs", &stem::StickmanItem::depth_shade_back_limbs)
        .def_readwrite("visible", &stem::StickmanItem::visible)
        .def_readwrite("hip_pos", &stem::StickmanItem::hip_pos)
        .def_readwrite("scale", &stem::StickmanItem::scale)
        .def_readwrite("torso_lean_rad", &stem::StickmanItem::torso_lean_rad)
        .def_readwrite("head_tilt_rad", &stem::StickmanItem::head_tilt_rad)
        .def_readwrite("arm_l_upper_rad", &stem::StickmanItem::arm_l_upper_rad)
        .def_readwrite("arm_l_lower_rad", &stem::StickmanItem::arm_l_lower_rad)
        .def_readwrite("arm_r_upper_rad", &stem::StickmanItem::arm_r_upper_rad)
        .def_readwrite("arm_r_lower_rad", &stem::StickmanItem::arm_r_lower_rad)
        .def_readwrite("leg_l_upper_rad", &stem::StickmanItem::leg_l_upper_rad)
        .def_readwrite("leg_l_lower_rad", &stem::StickmanItem::leg_l_lower_rad)
        .def_readwrite("leg_r_upper_rad", &stem::StickmanItem::leg_r_upper_rad)
        .def_readwrite("leg_r_lower_rad", &stem::StickmanItem::leg_r_lower_rad)
        .def_readwrite("stroke_width", &stem::StickmanItem::stroke_width)
        .def_readwrite("color", &stem::StickmanItem::color);

    py::class_<stem::ParticlePoolItem>(m, "ParticlePoolItem")
        .def(py::init<>())
        .def_readwrite("id", &stem::ParticlePoolItem::id)
        .def_readwrite("z_index", &stem::ParticlePoolItem::z_index)
        .def_readwrite("screen_space", &stem::ParticlePoolItem::screen_space)
        .def_readwrite("seed", &stem::ParticlePoolItem::seed)
        .def_readwrite("count", &stem::ParticlePoolItem::count)
        .def_readwrite("visible", &stem::ParticlePoolItem::visible)
        .def_readwrite("emitter_pos", &stem::ParticlePoolItem::emitter_pos)
        .def_readwrite("spread", &stem::ParticlePoolItem::spread)
        .def_readwrite("base_velocity", &stem::ParticlePoolItem::base_velocity)
        .def_readwrite("lifetime_sec", &stem::ParticlePoolItem::lifetime_sec)
        .def_readwrite("min_radius", &stem::ParticlePoolItem::min_radius)
        .def_readwrite("max_radius", &stem::ParticlePoolItem::max_radius)
        .def_readwrite("color", &stem::ParticlePoolItem::color)
        .def_readwrite("active_start_sec", &stem::ParticlePoolItem::active_start_sec)
        .def_readwrite("active_end_sec", &stem::ParticlePoolItem::active_end_sec);

    py::class_<stem::PathAsset>(m, "PathAsset")
        .def(py::init<>())
        .def_readwrite("asset_id", &stem::PathAsset::asset_id)
        .def_readwrite("cache_key", &stem::PathAsset::cache_key)
        .def_readonly("bounds_min", &stem::PathAsset::bounds_min)
        .def_readonly("bounds_max", &stem::PathAsset::bounds_max);

    py::class_<stem::PathInstanceItem>(m, "PathInstanceItem")
        .def(py::init<>())
        .def_readwrite("id", &stem::PathInstanceItem::id)
        .def_readwrite("asset_id", &stem::PathInstanceItem::asset_id)
        .def_readwrite("z_index", &stem::PathInstanceItem::z_index)
        .def_readwrite("screen_space", &stem::PathInstanceItem::screen_space)
        .def_readwrite("visible", &stem::PathInstanceItem::visible)
        .def_readwrite("position", &stem::PathInstanceItem::position)
        .def_readwrite("scale", &stem::PathInstanceItem::scale)
        .def_readwrite("rotation_rad", &stem::PathInstanceItem::rotation_rad)
        .def_readwrite("tint", &stem::PathInstanceItem::tint)
        .def_readwrite("presentation_offset", &stem::PathInstanceItem::presentation_offset)
        .def_readwrite("presentation_scale", &stem::PathInstanceItem::presentation_scale)
        .def_readwrite("presentation_rotation_rad", &stem::PathInstanceItem::presentation_rotation_rad)
        .def_readwrite("parallax", &stem::PathInstanceItem::parallax)
        .def_readwrite("trajectory_id", &stem::PathInstanceItem::trajectory_id);

    py::class_<stem::TextItem>(m, "TextItem")
        .def(py::init<>())
        .def_readwrite("id", &stem::TextItem::id)
        .def_readwrite("z_index", &stem::TextItem::z_index)
        .def_readwrite("screen_space", &stem::TextItem::screen_space)
        .def_readwrite("text", &stem::TextItem::text)
        .def_readwrite("font_path", &stem::TextItem::font_path)
        .def_readwrite("font_size_px", &stem::TextItem::font_size_px)
        .def_readwrite("align_h", &stem::TextItem::align_h)
        .def_readwrite("visible", &stem::TextItem::visible)
        .def_readwrite("position", &stem::TextItem::position)
        .def_readwrite("scale", &stem::TextItem::scale)
        .def_readwrite("rotation_rad", &stem::TextItem::rotation_rad)
        .def_readwrite("color", &stem::TextItem::color)
        .def_readwrite("presentation_offset", &stem::TextItem::presentation_offset)
        .def_readwrite("presentation_scale", &stem::TextItem::presentation_scale)
        .def_readwrite("presentation_rotation_rad", &stem::TextItem::presentation_rotation_rad)
        .def_readwrite("parallax", &stem::TextItem::parallax)
        .def_readwrite("trajectory_id", &stem::TextItem::trajectory_id);

    py::class_<stem::Cue>(m, "Cue")
        .def(py::init<>())
        .def_readwrite("cue_id", &stem::Cue::cue_id)
        .def_readwrite("time_sec", &stem::Cue::time_sec)
        .def_readwrite("description", &stem::Cue::description);

    py::class_<stem::AudioEvent>(m, "AudioEvent")
        .def(py::init<>())
        .def_readwrite("cue_id", &stem::AudioEvent::cue_id)
        .def_readwrite("time_sec", &stem::AudioEvent::time_sec)
        .def_readwrite("duration_sec", &stem::AudioEvent::duration_sec)
        .def_readwrite("gain", &stem::AudioEvent::gain)
        .def_readwrite("pitch_hz", &stem::AudioEvent::pitch_hz)
        .def_readwrite("sfx_kind", &stem::AudioEvent::sfx_kind)
        .def_readwrite("wav_path", &stem::AudioEvent::wav_path)
        .def_readwrite("is_test_tone", &stem::AudioEvent::is_test_tone);

    py::class_<stem::EvaluatedCamera>(m, "EvaluatedCamera")
        .def_readonly("position", &stem::EvaluatedCamera::position)
        .def_readonly("zoom", &stem::EvaluatedCamera::zoom)
        .def_readonly("rotation_rad", &stem::EvaluatedCamera::rotation_rad);

    py::class_<stem::SceneSnapshot>(m, "SceneSnapshot")
        .def_readonly("time_sec", &stem::SceneSnapshot::time_sec)
        .def_readonly("camera", &stem::SceneSnapshot::camera)
        .def("state_fingerprint", &stem::SceneSnapshot::state_fingerprint);

    py::class_<stem::Scene>(m, "Scene")
        .def(py::init<int, int, double>(), py::arg("width") = 1920, py::arg("height") = 1080, py::arg("duration_sec") = 10.0)
        .def_property("width", &stem::Scene::width, [](stem::Scene& s, int w) { s.set_resolution(w, s.height()); })
        .def_property("height", &stem::Scene::height, [](stem::Scene& s, int h) { s.set_resolution(s.width(), h); })
        .def_property("duration", &stem::Scene::duration, &stem::Scene::set_duration)
        .def("set_resolution", &stem::Scene::set_resolution)
        .def("background", py::overload_cast<>(&stem::Scene::background), py::return_value_policy::reference_internal)
        .def("camera", py::overload_cast<>(&stem::Scene::camera), py::return_value_policy::reference_internal)
        .def("add_trajectory", &stem::Scene::add_trajectory)
        .def("add_rect", &stem::Scene::add_rect_ref, py::return_value_policy::reference_internal)
        .def("add_circle", &stem::Scene::add_circle_ref, py::return_value_policy::reference_internal)
        .def("add_vector", &stem::Scene::add_vector_ref, py::return_value_policy::reference_internal)
        .def("add_graph", &stem::Scene::add_graph_ref, py::return_value_policy::reference_internal)
        .def("add_orbital_cloud", &stem::Scene::add_orbital_cloud_ref, py::return_value_policy::reference_internal)
        .def("add_stickman", &stem::Scene::add_stickman_ref, py::return_value_policy::reference_internal)
        .def("add_particle_pool", &stem::Scene::add_particle_pool_ref, py::return_value_policy::reference_internal)
        .def("register_path_asset", &stem::Scene::register_path_asset)
        .def("add_path_instance", &stem::Scene::add_path_instance_ref, py::return_value_policy::reference_internal)
        .def("add_text", &stem::Scene::add_text_ref, py::return_value_policy::reference_internal)
        .def("add_cue", &stem::Scene::add_cue)
        .def("add_audio_event", &stem::Scene::add_audio_event)
        .def("evaluate", [](const stem::Scene& sc, double t) { return sc.evaluate(t); })
        .def("scene_hash", &stem::Scene::scene_hash)
        .def_property_readonly("stickman_count", &stem::Scene::stickman_count)
        .def_static("make_milestone1_showcase", &stem::Scene::make_milestone1_showcase, py::arg("width") = 1920, py::arg("height") = 1080, py::arg("duration_sec") = 10.0)
        .def_static("make_color_calibration_scene", &stem::Scene::make_color_calibration_scene, py::arg("width") = 1920, py::arg("height") = 1080);

    py::class_<stem::ExportOptions>(m, "ExportOptions")
        .def(py::init<>())
        .def_readwrite("output_path", &stem::ExportOptions::output_path)
        .def_readwrite("width", &stem::ExportOptions::width)
        .def_readwrite("height", &stem::ExportOptions::height)
        .def_readwrite("fps", &stem::ExportOptions::fps)
        .def_readwrite("start_sec", &stem::ExportOptions::start_sec)
        .def_readwrite("end_sec", &stem::ExportOptions::end_sec)
        .def_readwrite("ring_size", &stem::ExportOptions::ring_size)
        .def_readwrite("interop_batch_size", &stem::ExportOptions::interop_batch_size)
        .def_readwrite("msaa_samples", &stem::ExportOptions::msaa_samples)
        .def_readwrite("bitrate_bps", &stem::ExportOptions::bitrate_bps)
        .def_readwrite("nvenc_preset", &stem::ExportOptions::nvenc_preset)
        .def_readwrite("rc_lookahead", &stem::ExportOptions::rc_lookahead)
        .def_readwrite("enable_audio", &stem::ExportOptions::enable_audio)
        .def_readwrite("render_only", &stem::ExportOptions::render_only);

    py::class_<stem::ExportMetrics>(m, "ExportMetrics")
        .def_readonly("gl_vendor", &stem::ExportMetrics::gl_vendor)
        .def_readonly("gl_renderer", &stem::ExportMetrics::gl_renderer)
        .def_readonly("gl_version", &stem::ExportMetrics::gl_version)
        .def_readonly("cuda_device_name", &stem::ExportMetrics::cuda_device_name)
        .def_readonly("width", &stem::ExportMetrics::width)
        .def_readonly("height", &stem::ExportMetrics::height)
        .def_readonly("fps", &stem::ExportMetrics::fps)
        .def_readonly("frame_count", &stem::ExportMetrics::frame_count)
        .def_readonly("scene_hash", &stem::ExportMetrics::scene_hash)
        .def_readonly("cold_startup_ms", &stem::ExportMetrics::cold_startup_ms)
        .def_readonly("asset_compile_ms", &stem::ExportMetrics::asset_compile_ms)
        .def_readonly("cpu_eval_avg_ms", &stem::ExportMetrics::cpu_eval_avg_ms)
        .def_readonly("gl_gpu_avg_ms", &stem::ExportMetrics::gl_gpu_avg_ms)
        .def_readonly("gl_gpu_p95_ms", &stem::ExportMetrics::gl_gpu_p95_ms)
        .def_readonly("cuda_convert_avg_ms", &stem::ExportMetrics::cuda_convert_avg_ms)
        .def_readonly("cuda_convert_p95_ms", &stem::ExportMetrics::cuda_convert_p95_ms)
        .def_readonly("encode_submit_avg_ms", &stem::ExportMetrics::encode_submit_avg_ms)
        .def_readonly("frame_wall_avg_ms", &stem::ExportMetrics::frame_wall_avg_ms)
        .def_readonly("frame_wall_p95_ms", &stem::ExportMetrics::frame_wall_p95_ms)
        .def_readonly("finalize_flush_ms", &stem::ExportMetrics::finalize_flush_ms)
        .def_readonly("total_wall_ms", &stem::ExportMetrics::total_wall_ms)
        .def_readonly("export_fps", &stem::ExportMetrics::export_fps)
        .def_readonly("peak_vram_used_bytes", &stem::ExportMetrics::peak_vram_used_bytes)
        .def_readonly("raw_frame_readbacks", &stem::ExportMetrics::raw_frame_readbacks)
        .def_readonly("output_file_bytes", &stem::ExportMetrics::output_file_bytes);

    py::class_<stem::text::TextBounds>(m, "TextBounds")
        .def_readonly("width", &stem::text::TextBounds::width)
        .def_readonly("height", &stem::text::TextBounds::height)
        .def_readonly("ascent", &stem::text::TextBounds::ascent)
        .def_readonly("descent", &stem::text::TextBounds::descent);

    m.def("doctor_json", []() {
        return stem::doctor_report_to_json(stem::run_doctor());
    });
    m.def("doctor_text", []() {
        return stem::doctor_report_to_text(stem::run_doctor());
    });

    m.def("parse_svg_to_path_asset", [](
        const std::string& svg_xml,
        const std::string& cache_key,
        float target_height_px,
        stem::ColorRGBA default_color
    ) {
        return unwrap_or_throw(stem::text::parse_svg_to_path_asset(svg_xml, cache_key, target_height_px, default_color));
    }, py::arg("svg_xml"), py::arg("cache_key"), py::arg("target_height_px") = 46.0f, py::arg("default_color") = stem::ColorRGBA{0.96f, 0.97f, 0.99f, 1.0f});

    m.def("measure_text_bounds", [](
        const std::string& text,
        const std::string& font_path,
        float font_size_px
    ) {
        stem::text::FontAtlasCache cache;
        return unwrap_or_throw(cache.measure_text(text, font_path, font_size_px));
    }, py::arg("text"), py::arg("font_path") = "", py::arg("font_size_px") = 32.0f);

    m.def("resolve_font_variant", [](const std::string& variant) {
        return stem::text::FontAtlasCache::resolve_font_variant(variant);
    }, py::arg("variant") = "regular");

    m.def("eval_easing", &stem::eval_easing,
        py::arg("type"), py::arg("u"), py::arg("spring_omega") = 14.0, py::arg("spring_zeta") = 0.65);

    m.def("sample_hydrogen_orbital_3d", &stem::sample_hydrogen_orbital_3d,
        py::arg("spec"), py::arg("count") = 4096u, py::arg("seed") = 108u);

    m.def("render_scene_to_mp4", [](const stem::Scene& scene, const stem::ExportOptions& opts) {
        py::gil_scoped_release release;
        return unwrap_or_throw(stem::export_scene_range(scene, opts));
    });

    m.def("preview_scene", [](const stem::Scene& scene, stem::RationalFPS fps, double max_seconds) {
        py::gil_scoped_release release;
        unwrap_or_throw_void(stem::preview_scene_native(scene, fps, max_seconds));
    }, py::arg("scene"), py::arg("fps") = stem::RationalFPS{60, 1}, py::arg("max_seconds") = -1.0);

    m.def("render_single_frame_rgba", [](
        const stem::Scene& scene,
        double time_sec,
        int width,
        int height
    ) {
        py::gil_scoped_release release;
        auto buf = unwrap_or_throw(stem::render_single_frame_rgba8_debug(scene, time_sec, width, height));
        return py::bytes(reinterpret_cast<const char*>(buf.data()), buf.size());
    }, py::arg("scene"), py::arg("time_sec"), py::arg("width") = 1920, py::arg("height") = 1080);

    m.def("run_benchmark_suite", [](
        const stem::Scene* custom_demo_scene,
        const std::string& suite_name,
        const std::string& label,
        int runs,
        bool include_4k,
        const std::string& json_output_path,
        const std::string& csv_output_path,
        int interop_batch_size
    ) {
        stem::BenchmarkSuiteReport rep;
        {
            py::gil_scoped_release release;
            rep = unwrap_or_throw(stem::run_benchmark_suite(custom_demo_scene, suite_name, label, runs, include_4k, "outputs/bench_tmp", interop_batch_size));
        }
        const std::string j = stem::benchmark_report_to_json(rep);
        const std::string c = stem::benchmark_report_to_csv(rep);
        if (!json_output_path.empty()) {
            std::filesystem::path jp(json_output_path);
            if (jp.has_parent_path()) std::filesystem::create_directories(jp.parent_path());
            std::ofstream(jp, std::ios::binary) << j;
        }
        if (!csv_output_path.empty()) {
            std::filesystem::path cp(csv_output_path);
            if (cp.has_parent_path()) std::filesystem::create_directories(cp.parent_path());
            std::ofstream(cp, std::ios::binary) << c;
        }
        return j;
    }, py::arg("custom_demo_scene") = nullptr, py::arg("suite_name") = "baseline", py::arg("label") = "baseline", py::arg("runs") = 3, py::arg("include_4k") = false, py::arg("json_output_path") = "", py::arg("csv_output_path") = "", py::arg("interop_batch_size") = 8);
}
