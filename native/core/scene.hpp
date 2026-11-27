#pragma once

#include "native/core/types.hpp"

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace stem {

enum class EasingType : uint8_t {
    Step = 0,
    Linear = 1,
    Smoothstep = 2,
    QuinticOut = 3,
    QuinticInOut = 4,
    Spring = 5,
    BackOut = 6,
    BackIn = 7,
    ExpoOut = 8,
    Impulse = 9,
};

[[nodiscard]] double eval_easing(
    EasingType type,
    double u,
    double spring_omega = 14.0,
    double spring_zeta = 0.65
) noexcept;

template <typename T>
struct KeyframeSegment {
    double t0 = 0.0;
    double t1 = 0.0;
    T v0{};
    T v1{};
    EasingType easing = EasingType::Linear;
    double spring_omega = 14.0;
    double spring_zeta = 0.65;
};

template <typename T>
class Track {
public:
    Track() = default;
    explicit Track(T constant) : default_value_(constant) {}

    void set_constant(T v) {
        segments_.clear();
        default_value_ = v;
    }

    void add_segment(
        double t0,
        double t1,
        T v0,
        T v1,
        EasingType easing = EasingType::QuinticInOut,
        double spring_omega = 14.0,
        double spring_zeta = 0.65
    );

    [[nodiscard]] bool is_animated() const noexcept { return !segments_.empty(); }
    [[nodiscard]] const T& default_value() const noexcept { return default_value_; }
    [[nodiscard]] const std::vector<KeyframeSegment<T>>& segments() const noexcept { return segments_; }

    // Deterministic, stateless evaluation for arbitrary seek order (e.g. 0 -> 5 -> 2 -> 5).
    [[nodiscard]] T evaluate(double t) const noexcept;

private:
    T default_value_{};
    std::vector<KeyframeSegment<T>> segments_;
};

using ScalarTrack = Track<float>;
using Vec2Track = Track<Vec2>;
using ColorTrack = Track<ColorRGBA>;

// Fixed-timestep baked or analytic trajectory for physics bodies, trails, and camera follow.
struct Trajectory2D {
    ObjectId id = kInvalidId;
    double t_start = 0.0;
    double dt = 1.0 / 240.0;
    std::vector<Vec2> samples;

    [[nodiscard]] Vec2 evaluate_pos(double t) const noexcept;
    [[nodiscard]] Vec2 evaluate_vel(double t) const noexcept;
};

struct CameraSpec {
    Vec2Track position{Vec2{0.0f, 0.0f}};
    ScalarTrack zoom{1.0f};
    ScalarTrack rotation_rad{0.0f};
    // Optional deterministic trajectory follow (stateless convolution window, never frame-accumulated).
    ObjectId follow_trajectory_id = kInvalidId;
    Vec2 follow_offset{0.0f, 0.0f};
    ScalarTrack follow_weight{0.0f};
    double follow_smoothing_window_sec = 0.18;
};

struct EvaluatedCamera {
    Vec2 position{0.0f, 0.0f};
    float zoom = 1.0f;
    float rotation_rad = 0.0f;
    auto operator<=>(const EvaluatedCamera&) const = default;
};

struct BackgroundSpec {
    ColorTrack top_color{ColorRGBA{0.055f, 0.075f, 0.120f, 1.0f}};
    ColorTrack bottom_color{ColorRGBA{0.024f, 0.031f, 0.055f, 1.0f}};
    bool show_grid = false;
    float grid_spacing = 100.0f;
    ColorRGBA grid_color{0.14f, 0.18f, 0.26f, 0.35f};
    float starfield_intensity = 0.0f;
    float vignette_strength = 0.22f;
    float grain_amount = 0.006f;
    float perspective_grid_opacity = 0.0f;
};

struct RectItem {
    ObjectId id = kInvalidId;
    int z_index = 0;
    bool screen_space = false;
    int material_mode = 0;  // 0=Solid, 1=MatteSurface, 2=LayeredBook, 3=SoftContactGlow, 4=Card25D
    ScalarTrack visible{1.0f};
    Vec2Track center{Vec2{0.0f, 0.0f}};
    Vec2Track size{Vec2{100.0f, 60.0f}};
    ScalarTrack rotation_rad{0.0f};
    ScalarTrack corner_radius{6.0f};
    ScalarTrack stroke_width{0.0f};
    ColorTrack fill_color{ColorRGBA{0.2f, 0.5f, 0.9f, 1.0f}};
    ColorTrack stroke_color{ColorRGBA{1.0f, 1.0f, 1.0f, 0.0f}};
    // Presentation acting & 2.5D depth kept strictly separate from scientific state.
    Vec2Track presentation_offset{Vec2{0.0f, 0.0f}};
    Vec2Track presentation_scale{Vec2{1.0f, 1.0f}};
    ScalarTrack presentation_rotation_rad{0.0f};
    ScalarTrack elevation{0.0f};
    float parallax = 1.0f;
    ObjectId trajectory_id = kInvalidId;
};

struct CircleItem {
    ObjectId id = kInvalidId;
    int z_index = 0;
    bool screen_space = false;
    int material_mode = 0;  // 0=Solid, 1=Sphere3D, 2=EmissiveGlow, 3=Disc25D
    ScalarTrack visible{1.0f};
    Vec2Track center{Vec2{0.0f, 0.0f}};
    ScalarTrack radius{20.0f};
    ScalarTrack stroke_width{0.0f};
    ColorTrack fill_color{ColorRGBA{0.95f, 0.55f, 0.20f, 1.0f}};
    ColorTrack stroke_color{ColorRGBA{1.0f, 1.0f, 1.0f, 0.0f}};
    Vec2Track presentation_offset{Vec2{0.0f, 0.0f}};
    ScalarTrack presentation_scale{1.0f};
    ScalarTrack elevation{0.0f};
    float parallax = 1.0f;
    ObjectId trajectory_id = kInvalidId;
};

struct VectorItem {
    ObjectId id = kInvalidId;
    int z_index = 10;
    bool screen_space = false;
    ScalarTrack visible{1.0f};
    Vec2Track origin{Vec2{0.0f, 0.0f}};
    Vec2Track delta{Vec2{120.0f, 0.0f}};
    ScalarTrack reveal{1.0f};
    ScalarTrack shaft_width{5.0f};
    ScalarTrack head_length{20.0f};
    ScalarTrack head_width{18.0f};
    ColorTrack color{ColorRGBA{0.25f, 0.85f, 0.55f, 1.0f}};
    Vec2Track presentation_offset{Vec2{0.0f, 0.0f}};
    ScalarTrack presentation_scale{1.0f};
    float parallax = 1.0f;
    ObjectId attach_trajectory_id = kInvalidId;
    Vec2 attach_offset{0.0f, 0.0f};
};

struct GraphItem {
    ObjectId id = kInvalidId;
    int z_index = 5;
    bool screen_space = false;
    ScalarTrack visible{1.0f};
    Vec2Track origin{Vec2{0.0f, 0.0f}};
    Vec2Track presentation_offset{Vec2{0.0f, 0.0f}};
    float parallax = 1.0f;
    std::vector<Vec2> points;
    std::vector<float> cumulative_lengths;
    ScalarTrack reveal{1.0f};
    ScalarTrack stroke_width{4.0f};
    ColorTrack color{ColorRGBA{0.35f, 0.75f, 1.0f, 1.0f}};
    float dash_length = 0.0f;  // 0 = solid line; >0 = dashed arc pattern
    float gap_length = 0.0f;
    // Optional dynamic deformation wave amplitude & frequency (evaluates deterministically).
    ScalarTrack deform_amplitude{0.0f};
    float deform_freq = 0.02f;
    float deform_speed = 4.0f;
    float deform_envelope_sigma = 0.0f;  // >0 applies Gaussian spatial packet envelope centered at x=0
    // Optional gradient area fill between the curve and baseline y.
    bool fill_under = false;
    float fill_baseline_y = 0.0f;
    ColorTrack fill_top_color{ColorRGBA{0.24f, 0.26f, 0.30f, 0.72f}};
    ColorTrack fill_bottom_color{ColorRGBA{0.03f, 0.03f, 0.04f, 0.04f}};
    // Optional trail mode sampling from a Trajectory2D up to current time t.
    ObjectId trail_trajectory_id = kInvalidId;
    double trail_duration_sec = 1.5;

    void rebuild_arc_lengths();
};

struct OrbitalSpec {
    int n = 1;
    int l = 0;
    int m = 0;
    float bohr_radius_px = 32.0f;
};

struct OrbitalParticleSample {
    float r_perp_px = 0.0f;
    float y_px = 0.0f;
    float phi0_rad = 0.0f;
    float omega_rad_per_sec = 0.0f;
};

// Exact deterministic importance sampler for 3D hydrogenic probability density |psi_{n,l,m}(r,theta,phi)|^2.
[[nodiscard]] std::vector<OrbitalParticleSample> sample_hydrogen_orbital_3d(
    const OrbitalSpec& spec,
    uint32_t count,
    uint32_t seed = 108u
);

struct OrbitalCloud3DItem {
    ObjectId id = kInvalidId;
    int z_index = 10;
    bool screen_space = false;
    uint32_t seed = 108u;
    uint32_t particle_count = 24000u;
    OrbitalSpec state_a{1, 0, 0, 36.0f};
    OrbitalSpec state_b{2, 1, 0, 22.0f};
    ScalarTrack morph{0.0f};
    ScalarTrack visible{1.0f};
    ScalarTrack reveal{1.0f};
    ScalarTrack opacity{1.0f};
    Vec2Track center{Vec2{0.0f, 0.0f}};
    ScalarTrack scale{1.0f};
    ScalarTrack yaw_rad{0.0f};
    ScalarTrack pitch_rad{0.28f};
    ScalarTrack fov_persp{0.0011f};
    ScalarTrack flow_speed{1.0f};
    ScalarTrack collapse{0.0f};
    Vec2 collapse_target{42.0f, -16.0f};
    float dot_radius_px = 2.05f;
    ColorTrack color{ColorRGBA{0.84f, 0.86f, 0.90f, 1.0f}};
};

struct StickmanItem {
    ObjectId id = kInvalidId;
    int z_index = 15;
    bool screen_space = false;
    bool solid_head = true;
    bool depth_shade_back_limbs = true;
    ScalarTrack visible{1.0f};
    Vec2Track hip_pos{Vec2{-300.0f, -100.0f}};
    ScalarTrack scale{1.0f};
    ScalarTrack torso_lean_rad{0.0f};
    ScalarTrack head_tilt_rad{0.0f};
    ScalarTrack arm_l_upper_rad{-2.1f};
    ScalarTrack arm_l_lower_rad{-1.8f};
    ScalarTrack arm_r_upper_rad{-0.6f};
    ScalarTrack arm_r_lower_rad{-0.2f};
    ScalarTrack leg_l_upper_rad{-1.85f};
    ScalarTrack leg_l_lower_rad{-1.65f};
    ScalarTrack leg_r_upper_rad{-1.25f};
    ScalarTrack leg_r_lower_rad{-1.45f};
    ScalarTrack stroke_width{6.0f};
    ColorTrack color{ColorRGBA{0.196f, 0.459f, 0.651f, 1.0f}};
};

struct ParticlePoolItem {
    ObjectId id = kInvalidId;
    int z_index = 8;
    bool screen_space = false;
    uint32_t seed = 1337u;
    uint32_t count = 256u;
    ScalarTrack visible{1.0f};
    Vec2Track emitter_pos{Vec2{0.0f, 0.0f}};
    Vec2 spread{400.0f, 250.0f};
    Vec2 base_velocity{35.0f, 20.0f};
    float lifetime_sec = 2.5f;
    float min_radius = 2.0f;
    float max_radius = 6.0f;
    ColorTrack color{ColorRGBA{0.45f, 0.78f, 1.0f, 0.65f}};
    double active_start_sec = 0.0;
    double active_end_sec = 1e9;
};

// Pre-tessellated vector path asset (retained in GPU VBO/IBO across frames).
struct PathVertex {
    float x = 0.0f;
    float y = 0.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct PathAsset {
    ObjectId asset_id = kInvalidId;
    std::string cache_key;
    std::vector<PathVertex> vertices;
    std::vector<uint32_t> indices;
    Vec2 bounds_min{0.0f, 0.0f};
    Vec2 bounds_max{0.0f, 0.0f};
};

struct PathInstanceItem {
    ObjectId id = kInvalidId;
    ObjectId asset_id = kInvalidId;
    int z_index = 20;
    bool screen_space = false;
    ScalarTrack visible{1.0f};
    Vec2Track position{Vec2{0.0f, 0.0f}};
    ScalarTrack scale{1.0f};
    ScalarTrack rotation_rad{0.0f};
    ColorTrack tint{ColorRGBA{1.0f, 1.0f, 1.0f, 1.0f}};
    Vec2Track presentation_offset{Vec2{0.0f, 0.0f}};
    ScalarTrack presentation_scale{1.0f};
    ScalarTrack presentation_rotation_rad{0.0f};
    float parallax = 1.0f;
    ObjectId trajectory_id = kInvalidId;
};

// Pre-shaped retained text item (glyph quads cached once per asset_id, never re-shaped per frame).
struct TextItem {
    ObjectId id = kInvalidId;
    int z_index = 25;
    bool screen_space = true;
    std::string text;
    std::string font_path;
    float font_size_px = 32.0f;
    int align_h = 0;  // -1 left, 0 center, +1 right
    ScalarTrack visible{1.0f};
    Vec2Track position{Vec2{0.0f, 0.0f}};
    ScalarTrack scale{1.0f};
    ScalarTrack rotation_rad{0.0f};
    ColorTrack color{ColorRGBA{0.96f, 0.97f, 0.99f, 1.0f}};
    Vec2Track presentation_offset{Vec2{0.0f, 0.0f}};
    ScalarTrack presentation_scale{1.0f};
    ScalarTrack presentation_rotation_rad{0.0f};
    float parallax = 1.0f;
    ObjectId trajectory_id = kInvalidId;
};

struct Cue {
    std::string cue_id;
    double time_sec = 0.0;
    std::string description;
};

enum class ProceduralSfxKind : uint8_t {
    None = 0,
    Click = 1,
    Release = 2,
    Slide = 3,
    Contact = 4,
    Chime = 5,
};

struct AudioEvent {
    std::string cue_id;
    double time_sec = 0.0;
    double duration_sec = 0.25;
    float gain = 0.45f;
    float pitch_hz = 440.0f;
    ProceduralSfxKind sfx_kind = ProceduralSfxKind::None;
    std::string wav_path;  // Optional external WAV asset (voice or custom SFX)
    bool is_test_tone = false;
};

// Evaluated flat frame snapshot produced by Scene::evaluate(time) for the GPU renderer.
struct EvaluatedCapsule {
    int z_index = 0;
    bool screen_space = false;
    Vec2 a{};
    Vec2 b{};
    float radius = 2.0f;
    ColorRGBA color{};
};

struct EvaluatedRect {
    int z_index = 0;
    bool screen_space = false;
    int material_mode = 0;
    Vec2 center{};
    Vec2 size{};
    float rotation_rad = 0.0f;
    float corner_radius = 0.0f;
    float stroke_width = 0.0f;
    float elevation = 0.0f;
    ColorRGBA fill_color{};
    ColorRGBA stroke_color{};
};

struct EvaluatedCircle {
    int z_index = 0;
    bool screen_space = false;
    int material_mode = 0;
    Vec2 center{};
    float radius = 10.0f;
    float stroke_width = 0.0f;
    float elevation = 0.0f;
    ColorRGBA fill_color{};
    ColorRGBA stroke_color{};
};

struct EvaluatedTriangle {
    int z_index = 0;
    bool screen_space = false;
    Vec2 p0{};
    Vec2 p1{};
    Vec2 p2{};
    ColorRGBA c0{};
    ColorRGBA c1{};
    ColorRGBA c2{};
};

struct EvaluatedOrbitalCloud {
    ObjectId id = kInvalidId;
    int z_index = 10;
    bool screen_space = false;
    Vec2 center{};
    float scale = 1.0f;
    float morph = 0.0f;
    float reveal = 1.0f;
    float opacity = 1.0f;
    float yaw_rad = 0.0f;
    float pitch_rad = 0.28f;
    float fov_persp = 0.0011f;
    float flow_phase_sec = 0.0f;
    float collapse = 0.0f;
    Vec2 collapse_target{};
    float dot_radius_px = 2.05f;
    ColorRGBA color{};
};

struct EvaluatedGraphDraw {
    ObjectId id = kInvalidId;
    int z_index = 0;
    bool screen_space = false;
    Vec2 origin{};
    float reveal = 1.0f;
    float stroke_width = 3.0f;
    float dash_length = 0.0f;
    float gap_length = 0.0f;
    ColorRGBA color{};
    uint32_t dirty_flags = DirtyNone;
    // Populated only when DirtyGeometry (deforming curve or dynamic trajectory trail).
    std::vector<Vec2> dynamic_points;
};

struct EvaluatedPathDraw {
    ObjectId id = kInvalidId;
    ObjectId asset_id = kInvalidId;
    int z_index = 0;
    bool screen_space = false;
    Vec2 position{};
    float scale = 1.0f;
    float rotation_rad = 0.0f;
    ColorRGBA tint{};
};

struct EvaluatedTextDraw {
    ObjectId id = kInvalidId;
    int z_index = 0;
    bool screen_space = true;
    Vec2 position{};
    float scale = 1.0f;
    float rotation_rad = 0.0f;
    ColorRGBA color{};
};

struct SceneSnapshot {
    double time_sec = 0.0;
    ColorRGBA bg_top{};
    ColorRGBA bg_bottom{};
    bool show_grid = false;
    float grid_spacing = 100.0f;
    ColorRGBA grid_color{};
    float starfield_intensity = 0.0f;
    float vignette_strength = 0.22f;
    float grain_amount = 0.006f;
    float perspective_grid_opacity = 0.0f;
    EvaluatedCamera camera{};

    std::vector<EvaluatedRect> rects;
    std::vector<EvaluatedCircle> circles;
    std::vector<EvaluatedCapsule> capsules;
    std::vector<EvaluatedTriangle> triangles;
    std::vector<EvaluatedGraphDraw> graphs;
    std::vector<EvaluatedOrbitalCloud> orbital_clouds;
    std::vector<EvaluatedPathDraw> paths;
    std::vector<EvaluatedTextDraw> texts;

    uint64_t state_fingerprint() const noexcept;
};

class Scene {
public:
    Scene(int width = 1920, int height = 1080, double duration_sec = 10.0);

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] double duration() const noexcept { return duration_sec_; }
    void set_resolution(int width, int height) noexcept {
        width_ = width;
        height_ = height;
    }
    void set_duration(double d) noexcept { duration_sec_ = d; }

    BackgroundSpec& background() noexcept { return background_; }
    const BackgroundSpec& background() const noexcept { return background_; }
    CameraSpec& camera() noexcept { return camera_; }
    const CameraSpec& camera() const noexcept { return camera_; }

    ObjectId add_trajectory(Trajectory2D traj);
    ObjectId add_rect(RectItem item);
    ObjectId add_circle(CircleItem item);
    ObjectId add_vector(VectorItem item);
    ObjectId add_graph(GraphItem item);
    ObjectId add_orbital_cloud(OrbitalCloud3DItem item);
    ObjectId add_stickman(StickmanItem item);
    ObjectId add_particle_pool(ParticlePoolItem item);
    ObjectId register_path_asset(PathAsset asset);
    ObjectId add_path_instance(PathInstanceItem item);
    ObjectId add_text(TextItem item);

    RectItem& add_rect_ref(RectItem item);
    CircleItem& add_circle_ref(CircleItem item);
    VectorItem& add_vector_ref(VectorItem item);
    GraphItem& add_graph_ref(GraphItem item);
    OrbitalCloud3DItem& add_orbital_cloud_ref(OrbitalCloud3DItem item);
    StickmanItem& add_stickman_ref(StickmanItem item);
    ParticlePoolItem& add_particle_pool_ref(ParticlePoolItem item);
    PathInstanceItem& add_path_instance_ref(PathInstanceItem item);
    TextItem& add_text_ref(TextItem item);

    void add_cue(Cue cue);
    void add_audio_event(AudioEvent ev);

    [[nodiscard]] const std::deque<GraphItem>& graphs() const noexcept { return graphs_; }
    [[nodiscard]] const std::deque<OrbitalCloud3DItem>& orbital_clouds() const noexcept { return orbital_clouds_; }
    [[nodiscard]] const std::vector<PathAsset>& path_assets() const noexcept { return path_assets_; }
    [[nodiscard]] const std::deque<TextItem>& texts() const noexcept { return texts_; }
    [[nodiscard]] const std::vector<Cue>& cues() const noexcept { return cues_; }
    [[nodiscard]] const std::vector<AudioEvent>& audio_events() const noexcept { return audio_events_; }
    [[nodiscard]] size_t stickman_count() const noexcept { return stickmen_.size(); }
    [[nodiscard]] const Trajectory2D* find_trajectory(ObjectId id) const noexcept;

    // Evaluate deterministic scene state at timestamp `time_sec` into reusable `out`.
    void evaluate(double time_sec, SceneSnapshot& out) const;
    [[nodiscard]] SceneSnapshot evaluate(double time_sec) const {
        SceneSnapshot snap;
        evaluate(time_sec, snap);
        return snap;
    }

    [[nodiscard]] uint64_t scene_hash() const noexcept;

    // Built-in verified scenes for Milestone 1, calibration tests, and Milestone 4 benchmarks.
    static Scene make_milestone1_showcase(int width = 1920, int height = 1080, double duration_sec = 10.0);
    static Scene make_color_calibration_scene(int width = 1920, int height = 1080);

private:
    int width_ = 1920;
    int height_ = 1080;
    double duration_sec_ = 10.0;
    ObjectId next_id_ = 1;

    BackgroundSpec background_;
    CameraSpec camera_;
    std::vector<Trajectory2D> trajectories_;
    std::unordered_map<ObjectId, size_t> trajectory_index_;
    std::deque<RectItem> rects_;
    std::deque<CircleItem> circles_;
    std::deque<VectorItem> vectors_;
    std::deque<GraphItem> graphs_;
    std::deque<OrbitalCloud3DItem> orbital_clouds_;
    std::deque<StickmanItem> stickmen_;
    std::deque<ParticlePoolItem> particle_pools_;
    std::vector<PathAsset> path_assets_;
    std::deque<PathInstanceItem> path_instances_;
    std::deque<TextItem> texts_;
    std::vector<Cue> cues_;
    std::vector<AudioEvent> audio_events_;
};

// CPU IEEE-754 double-precision ITU-R BT.709 Limited-Range RGBA8 -> NV12 reference for verification.
void cpu_rgba8_to_nv12_bt709_reference(
    const uint8_t* rgba,
    int width,
    int height,
    bool flip_y,
    ColorRGBA bg_color,
    std::vector<uint8_t>& out_y,
    std::vector<uint8_t>& out_uv
);

// Mixes scene AudioEvents (WAV files + procedural STEM SFX) into 48 kHz stereo float32 PCM.
std::vector<float> mix_scene_audio_f32_stereo(
    const Scene& scene,
    double start_sec,
    double end_sec,
    int sample_rate = 48000
);

}  // namespace stem
