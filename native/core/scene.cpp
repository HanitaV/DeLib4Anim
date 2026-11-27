#include "native/core/scene.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <numbers>

namespace stem {

namespace {

constexpr double kPi = std::numbers::pi_v<double>;
constexpr float kPiF = std::numbers::pi_v<float>;

template <typename T>
constexpr T lerp_val(const T& a, const T& b, float w) noexcept {
    return a * (1.0f - w) + b * w;
}

uint32_t hash_u32(uint32_t x) noexcept {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float hash_unit_f32(uint32_t seed, uint32_t idx) noexcept {
    const uint32_t h = hash_u32(seed ^ (idx * 0x9e3779b9u));
    return static_cast<float>(h & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

void fnv1a_mix(uint64_t& h, const void* ptr, size_t len) noexcept {
    const auto* bytes = static_cast<const uint8_t*>(ptr);
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<uint64_t>(bytes[i]);
        h *= 1099511628211ull;
    }
}

template <typename T>
void fnv1a_val(uint64_t& h, const T& val) noexcept {
    fnv1a_mix(h, &val, sizeof(T));
}

bool load_wav_pcm_mono_or_stereo(
    const std::string& path,
    int target_sr,
    std::vector<float>& out_stereo
) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (buf.size() < 44 || std::memcmp(buf.data(), "RIFF", 4) != 0 ||
        std::memcmp(buf.data() + 8, "WAVE", 4) != 0) {
        return false;
    }

    uint16_t audio_fmt = 0;
    uint16_t channels = 0;
    uint32_t sample_rate = 0;
    uint16_t bits_per_sample = 0;
    const uint8_t* data_ptr = nullptr;
    uint32_t data_size = 0;

    size_t pos = 12;
    while (pos + 8 <= buf.size()) {
        const char* id = reinterpret_cast<const char*>(buf.data() + pos);
        uint32_t chunk_sz = 0;
        std::memcpy(&chunk_sz, buf.data() + pos + 4, 4);
        pos += 8;
        if (pos + chunk_sz > buf.size()) break;
        if (std::memcmp(id, "fmt ", 4) == 0 && chunk_sz >= 16) {
            std::memcpy(&audio_fmt, buf.data() + pos + 0, 2);
            std::memcpy(&channels, buf.data() + pos + 2, 2);
            std::memcpy(&sample_rate, buf.data() + pos + 4, 4);
            std::memcpy(&bits_per_sample, buf.data() + pos + 14, 2);
        } else if (std::memcmp(id, "data", 4) == 0) {
            data_ptr = buf.data() + pos;
            data_size = chunk_sz;
            break;
        }
        pos += chunk_sz + (chunk_sz & 1u);
    }

    if (!data_ptr || channels == 0 || sample_rate == 0) return false;
    if (!((audio_fmt == 1 && bits_per_sample == 16) || (audio_fmt == 3 && bits_per_sample == 32))) {
        return false;
    }

    const size_t bytes_per_sample = bits_per_sample / 8u;
    const size_t frame_bytes = bytes_per_sample * channels;
    const size_t src_frames = data_size / frame_bytes;
    if (src_frames == 0) return false;

    std::vector<float> src_stereo(src_frames * 2);
    for (size_t i = 0; i < src_frames; ++i) {
        auto read_ch = [&](size_t ch) -> float {
            const uint8_t* p = data_ptr + i * frame_bytes + ch * bytes_per_sample;
            if (audio_fmt == 1) {
                int16_t s = 0;
                std::memcpy(&s, p, 2);
                return static_cast<float>(s) / 32768.0f;
            }
            float f = 0.0f;
            std::memcpy(&f, p, 4);
            return f;
        };
        const float l = read_ch(0);
        const float r = channels > 1 ? read_ch(1) : l;
        src_stereo[i * 2 + 0] = l;
        src_stereo[i * 2 + 1] = r;
    }

    if (static_cast<int>(sample_rate) == target_sr) {
        out_stereo = std::move(src_stereo);
        return true;
    }

    const double ratio = static_cast<double>(sample_rate) / static_cast<double>(target_sr);
    const size_t dst_frames = static_cast<size_t>(
        std::llround(static_cast<double>(src_frames) / ratio)
    );
    out_stereo.resize(dst_frames * 2);
    for (size_t i = 0; i < dst_frames; ++i) {
        const double src_pos = static_cast<double>(i) * ratio;
        const size_t i0 = std::min(static_cast<size_t>(src_pos), src_frames - 1);
        const size_t i1 = std::min(i0 + 1, src_frames - 1);
        const float frac = static_cast<float>(src_pos - static_cast<double>(i0));
        out_stereo[i * 2 + 0] =
            src_stereo[i0 * 2 + 0] * (1.0f - frac) + src_stereo[i1 * 2 + 0] * frac;
        out_stereo[i * 2 + 1] =
            src_stereo[i0 * 2 + 1] * (1.0f - frac) + src_stereo[i1 * 2 + 1] * frac;
    }
    return true;
}

}  // namespace

double eval_easing(EasingType type, double u, double spring_omega, double spring_zeta) noexcept {
    const double t = std::clamp(u, 0.0, 1.0);
    switch (type) {
        case EasingType::Step:
            return u >= 1.0 ? 1.0 : 0.0;
        case EasingType::Linear:
            return t;
        case EasingType::Smoothstep:
            return t * t * (3.0 - 2.0 * t);
        case EasingType::QuinticOut: {
            const double inv = 1.0 - t;
            return 1.0 - (inv * inv * inv * inv * inv);
        }
        case EasingType::QuinticInOut: {
            if (t < 0.5) {
                return 16.0 * t * t * t * t * t;
            }
            const double f = -2.0 * t + 2.0;
            return 1.0 - (f * f * f * f * f) * 0.5;
        }
        case EasingType::Spring: {
            if (t <= 0.0) return 0.0;
            if (t >= 1.0) return 1.0;
            const double zeta = std::clamp(spring_zeta, 0.05, 0.99);
            const double omega = std::max(spring_omega, 1.0);
            const double omega_d = omega * std::sqrt(1.0 - zeta * zeta);
            const double env = std::exp(-zeta * omega * t);
            const double raw =
                1.0 - env * (std::cos(omega_d * t) + (zeta * omega / omega_d) * std::sin(omega_d * t));
            // Smoothly settle to exact 1.0 at t=1.0 so adjacent segments are C0-continuous.
            const double end_env = std::exp(-zeta * omega);
            const double raw_end =
                1.0 - end_env * (std::cos(omega_d) + (zeta * omega / omega_d) * std::sin(omega_d));
            return std::abs(raw_end) > 1e-6 ? (raw / raw_end) : raw;
        }
        case EasingType::BackOut: {
            if (t <= 0.0) return 0.0;
            if (t >= 1.0) return 1.0;
            const double s = std::clamp(spring_zeta * 2.6, 0.35, 3.5);
            const double f = t - 1.0;
            return 1.0 + (s + 1.0) * f * f * f + s * f * f;
        }
        case EasingType::BackIn: {
            if (t <= 0.0) return 0.0;
            if (t >= 1.0) return 1.0;
            const double s = std::clamp(spring_zeta * 2.6, 0.35, 3.5);
            return (s + 1.0) * t * t * t - s * t * t;
        }
        case EasingType::ExpoOut: {
            if (t <= 0.0) return 0.0;
            if (t >= 1.0) return 1.0;
            constexpr double kNorm = 1.0 / (1.0 - 0.0009765625);
            return (1.0 - std::pow(2.0, -10.0 * t)) * kNorm;
        }
        case EasingType::Impulse: {
            if (t <= 0.0 || t >= 1.0) return 0.0;
            const double omega = std::max(spring_omega, 1.0);
            const double zeta = std::clamp(spring_zeta, 0.05, 2.0);
            if (zeta < 0.999) {
                const double sqrt_term = std::sqrt(1.0 - zeta * zeta);
                const double omega_d = omega * sqrt_term;
                auto pulse = [&](double x) noexcept -> double {
                    const double taper = 1.0 - x;
                    return std::exp(-zeta * omega * x) * std::sin(omega_d * x) * taper * taper;
                };
                const double u_peak = std::clamp(std::atan2(sqrt_term, zeta) / omega_d, 0.02, 0.45);
                return pulse(t) / std::max(1e-6, pulse(u_peak));
            }
            auto surge = [&](double x) noexcept -> double {
                const double taper = 1.0 - x;
                return (omega * x) * std::exp(1.0 - omega * x) * taper * taper;
            };
            const double u_peak = std::clamp(1.0 / (omega + 2.0), 0.02, 0.35);
            return surge(t) / std::max(1e-6, surge(u_peak));
        }
    }
    return t;
}

namespace {

template <typename T>
constexpr const T& segment_end_value(const KeyframeSegment<T>& seg) noexcept {
    // Impulse curves peak at v1 and return smoothly to v0 at u=1.
    return (seg.easing == EasingType::Impulse) ? seg.v0 : seg.v1;
}

}  // namespace

template <typename T>
void Track<T>::add_segment(
    double t0,
    double t1,
    T v0,
    T v1,
    EasingType easing,
    double spring_omega,
    double spring_zeta
) {
    if (t1 < t0) {
        std::swap(t0, t1);
        if (easing != EasingType::Impulse) {
            std::swap(v0, v1);
        }
    }
    KeyframeSegment<T> seg{
        .t0 = t0,
        .t1 = t1,
        .v0 = v0,
        .v1 = v1,
        .easing = easing,
        .spring_omega = spring_omega,
        .spring_zeta = spring_zeta,
    };
    const auto it = std::ranges::upper_bound(
        segments_,
        seg.t0,
        {},
        [](const KeyframeSegment<T>& s) { return s.t0; }
    );
    segments_.insert(it, seg);
}

template <typename T>
T Track<T>::evaluate(double t) const noexcept {
    if (segments_.empty()) {
        return default_value_;
    }
    if (t <= segments_.front().t0) {
        return segments_.front().v0;
    }
    if (t >= segments_.back().t1) {
        return segment_end_value(segments_.back());
    }

    // Binary search last segment whose t0 <= t
    auto it = std::ranges::upper_bound(
        segments_,
        t,
        {},
        [](const KeyframeSegment<T>& s) { return s.t0; }
    );
    if (it != segments_.begin()) {
        --it;
    }
    if (t >= it->t1) {
        return segment_end_value(*it);
    }
    const double span = it->t1 - it->t0;
    if (span <= 1e-12) {
        return segment_end_value(*it);
    }
    const double u = (t - it->t0) / span;
    const float w = static_cast<float>(eval_easing(it->easing, u, it->spring_omega, it->spring_zeta));
    return lerp_val(it->v0, it->v1, w);
}

template class Track<float>;
template class Track<Vec2>;
template class Track<ColorRGBA>;

Vec2 Trajectory2D::evaluate_pos(double t) const noexcept {
    if (samples.empty()) return {0.0f, 0.0f};
    if (samples.size() == 1 || dt <= 1e-9 || t <= t_start) {
        return samples.front();
    }
    const double rel = (t - t_start) / dt;
    const double max_idx = static_cast<double>(samples.size() - 1);
    if (rel >= max_idx) {
        return samples.back();
    }
    const size_t i0 = static_cast<size_t>(rel);
    const size_t i1 = std::min(i0 + 1, samples.size() - 1);
    const float frac = static_cast<float>(rel - static_cast<double>(i0));
    return lerp_val(samples[i0], samples[i1], frac);
}

Vec2 Trajectory2D::evaluate_vel(double t) const noexcept {
    if (samples.size() < 2 || dt <= 1e-9) return {0.0f, 0.0f};
    const double eps = dt * 0.5;
    const Vec2 p0 = evaluate_pos(std::max(t_start, t - eps));
    const Vec2 p1 = evaluate_pos(t + eps);
    const float inv = static_cast<float>(1.0 / (std::min(t + eps, t_start + (samples.size() - 1) * dt) -
                                                std::max(t_start, t - eps) + 1e-9));
    return (p1 - p0) * inv;
}

void GraphItem::rebuild_arc_lengths() {
    cumulative_lengths.resize(points.size(), 0.0f);
    float acc = 0.0f;
    for (size_t i = 1; i < points.size(); ++i) {
        acc += (points[i] - points[i - 1]).length();
        cumulative_lengths[i] = acc;
    }
}

uint64_t SceneSnapshot::state_fingerprint() const noexcept {
    uint64_t h = 14695981039346656037ull;
    fnv1a_val(h, time_sec);
    fnv1a_val(h, bg_top);
    fnv1a_val(h, bg_bottom);
    fnv1a_val(h, perspective_grid_opacity);
    fnv1a_val(h, camera.position);
    fnv1a_val(h, camera.zoom);
    fnv1a_val(h, camera.rotation_rad);
    for (const auto& r : rects) {
        fnv1a_val(h, r.z_index);
        fnv1a_val(h, static_cast<uint8_t>(r.screen_space ? 1 : 0));
        fnv1a_val(h, r.material_mode);
        fnv1a_val(h, r.center);
        fnv1a_val(h, r.size);
        fnv1a_val(h, r.rotation_rad);
        fnv1a_val(h, r.corner_radius);
        fnv1a_val(h, r.stroke_width);
        fnv1a_val(h, r.elevation);
        fnv1a_val(h, r.fill_color);
        fnv1a_val(h, r.stroke_color);
    }
    for (const auto& c : circles) {
        fnv1a_val(h, c.z_index);
        fnv1a_val(h, static_cast<uint8_t>(c.screen_space ? 1 : 0));
        fnv1a_val(h, c.material_mode);
        fnv1a_val(h, c.center);
        fnv1a_val(h, c.radius);
        fnv1a_val(h, c.stroke_width);
        fnv1a_val(h, c.elevation);
        fnv1a_val(h, c.fill_color);
        fnv1a_val(h, c.stroke_color);
    }
    for (const auto& cap : capsules) {
        fnv1a_val(h, cap.z_index);
        fnv1a_val(h, static_cast<uint8_t>(cap.screen_space ? 1 : 0));
        fnv1a_val(h, cap.a);
        fnv1a_val(h, cap.b);
        fnv1a_val(h, cap.radius);
        fnv1a_val(h, cap.color);
    }
    for (const auto& tri : triangles) {
        fnv1a_val(h, tri.z_index);
        fnv1a_val(h, static_cast<uint8_t>(tri.screen_space ? 1 : 0));
        fnv1a_val(h, tri.p0);
        fnv1a_val(h, tri.p1);
        fnv1a_val(h, tri.p2);
        fnv1a_val(h, tri.c0);
        fnv1a_val(h, tri.c1);
        fnv1a_val(h, tri.c2);
    }
    for (const auto& oc : orbital_clouds) {
        fnv1a_val(h, oc.id);
        fnv1a_val(h, oc.center);
        fnv1a_val(h, oc.scale);
        fnv1a_val(h, oc.morph);
        fnv1a_val(h, oc.reveal);
        fnv1a_val(h, oc.yaw_rad);
        fnv1a_val(h, oc.pitch_rad);
        fnv1a_val(h, oc.flow_phase_sec);
        fnv1a_val(h, oc.collapse);
        fnv1a_val(h, oc.color);
    }
    for (const auto& g : graphs) {
        fnv1a_val(h, g.id);
        fnv1a_val(h, g.origin);
        fnv1a_val(h, g.reveal);
        fnv1a_val(h, g.color);
        if (!g.dynamic_points.empty()) {
            fnv1a_mix(h, g.dynamic_points.data(), g.dynamic_points.size() * sizeof(Vec2));
        }
    }
    for (const auto& p : paths) {
        fnv1a_val(h, p.id);
        fnv1a_val(h, p.asset_id);
        fnv1a_val(h, p.position);
        fnv1a_val(h, p.scale);
        fnv1a_val(h, p.rotation_rad);
        fnv1a_val(h, p.tint);
    }
    for (const auto& t : texts) {
        fnv1a_val(h, t.id);
        fnv1a_val(h, t.position);
        fnv1a_val(h, t.scale);
        fnv1a_val(h, t.rotation_rad);
        fnv1a_val(h, t.color);
    }
    return h;
}

Scene::Scene(int width, int height, double duration_sec)
    : width_(width), height_(height), duration_sec_(duration_sec) {}

ObjectId Scene::add_trajectory(Trajectory2D traj) {
    if (traj.id == kInvalidId) traj.id = next_id_++;
    trajectory_index_[traj.id] = trajectories_.size();
    trajectories_.push_back(std::move(traj));
    return trajectories_.back().id;
}

ObjectId Scene::add_rect(RectItem item) { return add_rect_ref(std::move(item)).id; }
ObjectId Scene::add_circle(CircleItem item) { return add_circle_ref(std::move(item)).id; }
ObjectId Scene::add_vector(VectorItem item) { return add_vector_ref(std::move(item)).id; }
ObjectId Scene::add_graph(GraphItem item) { return add_graph_ref(std::move(item)).id; }
ObjectId Scene::add_orbital_cloud(OrbitalCloud3DItem item) { return add_orbital_cloud_ref(std::move(item)).id; }
ObjectId Scene::add_stickman(StickmanItem item) { return add_stickman_ref(std::move(item)).id; }
ObjectId Scene::add_particle_pool(ParticlePoolItem item) { return add_particle_pool_ref(std::move(item)).id; }

ObjectId Scene::register_path_asset(PathAsset asset) {
    if (asset.asset_id == kInvalidId) asset.asset_id = next_id_++;
    path_assets_.push_back(std::move(asset));
    return path_assets_.back().asset_id;
}

ObjectId Scene::add_path_instance(PathInstanceItem item) { return add_path_instance_ref(std::move(item)).id; }
ObjectId Scene::add_text(TextItem item) { return add_text_ref(std::move(item)).id; }

RectItem& Scene::add_rect_ref(RectItem item) {
    if (item.id == kInvalidId) item.id = next_id_++;
    return rects_.emplace_back(std::move(item));
}
CircleItem& Scene::add_circle_ref(CircleItem item) {
    if (item.id == kInvalidId) item.id = next_id_++;
    return circles_.emplace_back(std::move(item));
}
VectorItem& Scene::add_vector_ref(VectorItem item) {
    if (item.id == kInvalidId) item.id = next_id_++;
    return vectors_.emplace_back(std::move(item));
}
GraphItem& Scene::add_graph_ref(GraphItem item) {
    if (item.id == kInvalidId) item.id = next_id_++;
    if (item.cumulative_lengths.size() != item.points.size()) {
        item.rebuild_arc_lengths();
    }
    return graphs_.emplace_back(std::move(item));
}
OrbitalCloud3DItem& Scene::add_orbital_cloud_ref(OrbitalCloud3DItem item) {
    if (item.id == kInvalidId) item.id = next_id_++;
    return orbital_clouds_.emplace_back(std::move(item));
}
StickmanItem& Scene::add_stickman_ref(StickmanItem item) {
    if (item.id == kInvalidId) item.id = next_id_++;
    return stickmen_.emplace_back(std::move(item));
}
ParticlePoolItem& Scene::add_particle_pool_ref(ParticlePoolItem item) {
    if (item.id == kInvalidId) item.id = next_id_++;
    return particle_pools_.emplace_back(std::move(item));
}
PathInstanceItem& Scene::add_path_instance_ref(PathInstanceItem item) {
    if (item.id == kInvalidId) item.id = next_id_++;
    return path_instances_.emplace_back(std::move(item));
}
TextItem& Scene::add_text_ref(TextItem item) {
    if (item.id == kInvalidId) item.id = next_id_++;
    return texts_.emplace_back(std::move(item));
}

void Scene::add_cue(Cue cue) { cues_.push_back(std::move(cue)); }
void Scene::add_audio_event(AudioEvent ev) { audio_events_.push_back(std::move(ev)); }

const Trajectory2D* Scene::find_trajectory(ObjectId id) const noexcept {
    if (id == kInvalidId) return nullptr;
    const auto it = trajectory_index_.find(id);
    if (it == trajectory_index_.end()) return nullptr;
    return &trajectories_[it->second];
}

std::vector<OrbitalParticleSample> sample_hydrogen_orbital_3d(
    const OrbitalSpec& spec,
    uint32_t count,
    uint32_t seed
) {
    const int n = std::clamp(spec.n, 1, 8);
    const int l = std::clamp(spec.l, 0, n - 1);
    const int abs_m = std::clamp(std::abs(spec.m), 0, l);

    // 1. Radial PDF P_r(r) ~ r^2 * (rho^l * exp(-rho/2) * L_{n-l-1}^{2l+1}(rho))^2
    constexpr int kRBins = 2048;
    const double r_max = static_cast<double>(n) * (1.8 * static_cast<double>(n) + 11.0);
    const double dr = r_max / kRBins;
    const int p_lag = n - l - 1;
    const int k_lag = 2 * l + 1;

    auto eval_laguerre = [&](int p, int k, double x) -> double {
        if (p <= 0) return 1.0;
        if (p == 1) return 1.0 + k - x;
        double l_prev2 = 1.0;
        double l_prev1 = 1.0 + k - x;
        for (int i = 1; i < p; ++i) {
            const double l_next = ((2 * i + 1 + k - x) * l_prev1 - (i + k) * l_prev2) / static_cast<double>(i + 1);
            l_prev2 = l_prev1;
            l_prev1 = l_next;
        }
        return l_prev1;
    };

    std::vector<double> r_cdf(kRBins + 1, 0.0);
    for (int i = 0; i < kRBins; ++i) {
        const double r = (i + 0.5) * dr;
        const double rho = (2.0 * r) / static_cast<double>(n);
        const double lag = eval_laguerre(p_lag, k_lag, rho);
        const double log_rho_term = (l > 0 && rho > 1e-12) ? (static_cast<double>(l) * std::log(rho)) : (l == 0 ? 0.0 : -1e9);
        const double rad_amp = std::exp(log_rho_term - 0.5 * rho) * lag;
        const double w = (r * r) * (rad_amp * rad_amp);
        r_cdf[static_cast<size_t>(i + 1)] = r_cdf[static_cast<size_t>(i)] + (std::isfinite(w) ? w : 0.0);
    }
    const double r_tot = r_cdf.back() > 1e-30 ? r_cdf.back() : 1.0;
    for (double& v : r_cdf) v /= r_tot;
    r_cdf.back() = 1.0;

    // 2. Polar PDF P_u(u) ~ |P_l^m(u)|^2 on u = cos(theta) in [-1, 1]
    constexpr int kUBins = 1024;
    const double du = 2.0 / kUBins;
    auto eval_assoc_legendre = [&](int ell, int m_ord, double u) -> double {
        double pmm = 1.0;
        if (m_ord > 0) {
            const double sin_sq = std::max(0.0, 1.0 - u * u);
            const double somx2 = std::sqrt(sin_sq);
            double fact = 1.0;
            for (int i = 1; i <= m_ord; ++i) {
                pmm *= -fact * somx2;
                fact += 2.0;
            }
        }
        if (ell == m_ord) return pmm;
        double pmmp1 = u * (2 * m_ord + 1) * pmm;
        if (ell == m_ord + 1) return pmmp1;
        double pll = 0.0;
        for (int ll = m_ord + 2; ll <= ell; ++ll) {
            pll = ((2 * ll - 1) * u * pmmp1 - (ll + m_ord - 1) * pmm) / static_cast<double>(ll - m_ord);
            pmm = pmmp1;
            pmmp1 = pll;
        }
        return pll;
    };

    std::vector<double> u_cdf(kUBins + 1, 0.0);
    for (int i = 0; i < kUBins; ++i) {
        const double u = -1.0 + (i + 0.5) * du;
        const double plm = eval_assoc_legendre(l, abs_m, u);
        const double w = plm * plm;
        u_cdf[static_cast<size_t>(i + 1)] = u_cdf[static_cast<size_t>(i)] + (std::isfinite(w) ? w : 0.0);
    }
    const double u_tot = u_cdf.back() > 1e-30 ? u_cdf.back() : 1.0;
    for (double& v : u_cdf) v /= u_tot;
    u_cdf.back() = 1.0;

    auto invert_cdf = [](const std::vector<double>& cdf, double x0, double step, double xi) -> double {
        const auto it = std::ranges::upper_bound(cdf, xi);
        const size_t idx = std::min(cdf.size() - 2, static_cast<size_t>(std::max<ptrdiff_t>(0, (it - cdf.begin()) - 1)));
        const double c0 = cdf[idx];
        const double c1 = cdf[idx + 1];
        const double span = c1 - c0;
        const double frac = span > 1e-18 ? ((xi - c0) / span) : 0.5;
        return x0 + (static_cast<double>(idx) + std::clamp(frac, 0.0, 1.0)) * step;
    };

    std::vector<OrbitalParticleSample> samples(count);
    for (uint32_t i = 0; i < count; ++i) {
        const double ur = static_cast<double>(hash_unit_f32(seed, i * 3u + 0u));
        const double uu = static_cast<double>(hash_unit_f32(seed, i * 3u + 1u));
        const double uphi = static_cast<double>(hash_unit_f32(seed, i * 3u + 2u));

        const double r = invert_cdf(r_cdf, 0.0, dr, ur);
        const double u = std::clamp(invert_cdf(u_cdf, -1.0, du, uu), -1.0, 1.0);
        const double sin_t = std::sqrt(std::max(0.0, 1.0 - u * u));
        const double r_perp_a0 = r * sin_t;

        const double omega = (spec.m == 0)
            ? 0.0
            : (4.2 * static_cast<double>(spec.m)) / (r_perp_a0 * r_perp_a0 + 1.35);

        samples[i] = OrbitalParticleSample{
            .r_perp_px = static_cast<float>(r_perp_a0 * spec.bohr_radius_px),
            .y_px = static_cast<float>(r * u * spec.bohr_radius_px),
            .phi0_rad = static_cast<float>(uphi * 2.0 * kPi),
            .omega_rad_per_sec = static_cast<float>(omega),
        };
    }

    // Order samples monotonically by polar cosine cos(theta) so orbital morphs transition coherently by lobe.
    std::ranges::sort(samples, [](const OrbitalParticleSample& a, const OrbitalParticleSample& b) {
        const float ra = std::hypot(a.r_perp_px, a.y_px);
        const float rb = std::hypot(b.r_perp_px, b.y_px);
        const float ua = ra > 1e-5f ? (a.y_px / ra) : 0.0f;
        const float ub = rb > 1e-5f ? (b.y_px / rb) : 0.0f;
        if (ua != ub) return ua < ub;
        return ra < rb;
    });
    return samples;
}

void Scene::evaluate(double time_sec, SceneSnapshot& out) const {
    out.time_sec = time_sec;
    out.bg_top = background_.top_color.evaluate(time_sec);
    out.bg_bottom = background_.bottom_color.evaluate(time_sec);
    out.show_grid = background_.show_grid;
    out.grid_spacing = background_.grid_spacing;
    out.grid_color = background_.grid_color;
    out.starfield_intensity = background_.starfield_intensity;
    out.vignette_strength = background_.vignette_strength;
    out.grain_amount = background_.grain_amount;
    out.perspective_grid_opacity = background_.perspective_grid_opacity;

    // Stateless camera evaluation (follow uses fixed symmetric/causal kernel on trajectory, never state history).
    Vec2 cam_pos = camera_.position.evaluate(time_sec);
    const float follow_w = std::clamp(camera_.follow_weight.evaluate(time_sec), 0.0f, 1.0f);
    if (follow_w > 1e-4f) {
        if (const Trajectory2D* tr = find_trajectory(camera_.follow_trajectory_id)) {
            constexpr int kTaps = 5;
            Vec2 filtered{0.0f, 0.0f};
            float weight_sum = 0.0f;
            for (int k = 0; k < kTaps; ++k) {
                const double tau =
                    time_sec - camera_.follow_smoothing_window_sec * (static_cast<double>(k) / (kTaps - 1));
                const float wk = static_cast<float>(kTaps - k);
                filtered = filtered + tr->evaluate_pos(tau) * wk;
                weight_sum += wk;
            }
            const Vec2 target = filtered * (1.0f / weight_sum) + camera_.follow_offset;
            cam_pos = cam_pos * (1.0f - follow_w) + target * follow_w;
        }
    }
    out.camera = EvaluatedCamera{
        .position = cam_pos,
        .zoom = std::max(0.05f, camera_.zoom.evaluate(time_sec)),
        .rotation_rad = camera_.rotation_rad.evaluate(time_sec),
    };

    auto apply_parallax = [&](Vec2 world_p, bool screen_space, float parallax) noexcept -> Vec2 {
        if (screen_space || std::abs(parallax - 1.0f) <= 1e-5f) return world_p;
        return world_p + cam_pos * (1.0f - parallax);
    };

    out.rects.clear();
    out.circles.clear();
    out.capsules.clear();
    out.triangles.clear();
    out.graphs.clear();
    out.orbital_clouds.clear();
    out.paths.clear();
    out.texts.clear();

    // 1. Rects
    for (const auto& item : rects_) {
        const float vis = std::clamp(item.visible.evaluate(time_sec), 0.0f, 1.0f);
        if (vis <= 1e-4f) continue;
        Vec2 center = item.center.evaluate(time_sec);
        if (const Trajectory2D* tr = find_trajectory(item.trajectory_id)) {
            center = center + tr->evaluate_pos(time_sec);
        }
        center = apply_parallax(center, item.screen_space, item.parallax) + item.presentation_offset.evaluate(time_sec);
        const Vec2 sz = item.size.evaluate(time_sec);
        const Vec2 ps = item.presentation_scale.evaluate(time_sec);
        ColorRGBA fill = item.fill_color.evaluate(time_sec);
        ColorRGBA stroke = item.stroke_color.evaluate(time_sec);
        fill.a *= vis;
        stroke.a *= vis;
        out.rects.push_back(EvaluatedRect{
            .z_index = item.z_index,
            .screen_space = item.screen_space,
            .material_mode = item.material_mode,
            .center = center,
            .size = Vec2{sz.x * ps.x, sz.y * ps.y},
            .rotation_rad = item.rotation_rad.evaluate(time_sec) + item.presentation_rotation_rad.evaluate(time_sec),
            .corner_radius = item.corner_radius.evaluate(time_sec),
            .stroke_width = item.stroke_width.evaluate(time_sec),
            .elevation = std::max(0.0f, item.elevation.evaluate(time_sec)),
            .fill_color = fill,
            .stroke_color = stroke,
        });
    }

    // 2. Circles
    for (const auto& item : circles_) {
        const float vis = std::clamp(item.visible.evaluate(time_sec), 0.0f, 1.0f);
        if (vis <= 1e-4f) continue;
        Vec2 center = item.center.evaluate(time_sec);
        if (const Trajectory2D* tr = find_trajectory(item.trajectory_id)) {
            center = center + tr->evaluate_pos(time_sec);
        }
        center = apply_parallax(center, item.screen_space, item.parallax) + item.presentation_offset.evaluate(time_sec);
        const float p_scale = std::max(0.0f, item.presentation_scale.evaluate(time_sec));
        ColorRGBA fill = item.fill_color.evaluate(time_sec);
        ColorRGBA stroke = item.stroke_color.evaluate(time_sec);
        fill.a *= vis;
        stroke.a *= vis;
        out.circles.push_back(EvaluatedCircle{
            .z_index = item.z_index,
            .screen_space = item.screen_space,
            .material_mode = item.material_mode,
            .center = center,
            .radius = item.radius.evaluate(time_sec) * p_scale,
            .stroke_width = item.stroke_width.evaluate(time_sec) * p_scale,
            .elevation = std::max(0.0f, item.elevation.evaluate(time_sec)),
            .fill_color = fill,
            .stroke_color = stroke,
        });
    }

    // 3. Vectors (parametric capsule shaft + crisp triangle arrowhead)
    for (const auto& item : vectors_) {
        const float vis = std::clamp(item.visible.evaluate(time_sec), 0.0f, 1.0f);
        const float rev = std::clamp(item.reveal.evaluate(time_sec), 0.0f, 1.0f);
        if (vis <= 1e-4f || rev <= 1e-4f) continue;
        Vec2 orig = item.origin.evaluate(time_sec);
        if (const Trajectory2D* tr = find_trajectory(item.attach_trajectory_id)) {
            orig = orig + tr->evaluate_pos(time_sec) + item.attach_offset;
        }
        orig = apply_parallax(orig, item.screen_space, item.parallax) + item.presentation_offset.evaluate(time_sec);
        const float p_scale = std::max(0.0f, item.presentation_scale.evaluate(time_sec));
        const Vec2 full_delta = item.delta.evaluate(time_sec) * (rev * p_scale);
        const float len = full_delta.length();
        if (len < 1.0f) continue;
        const Vec2 dir = full_delta * (1.0f / len);
        const Vec2 perp{-dir.y, dir.x};
        const float head_len = std::min(item.head_length.evaluate(time_sec) * p_scale, len * 0.65f);
        const float head_half_w = item.head_width.evaluate(time_sec) * 0.5f * p_scale;
        const float shaft_r = item.shaft_width.evaluate(time_sec) * 0.5f * p_scale;
        ColorRGBA col = item.color.evaluate(time_sec);
        col.a *= vis;

        const Vec2 tip = orig + full_delta;
        const Vec2 neck = tip - dir * head_len;
        if ((neck - orig).length() > 0.5f) {
            out.capsules.push_back(EvaluatedCapsule{
                .z_index = item.z_index,
                .screen_space = item.screen_space,
                .a = orig,
                .b = neck + dir * (shaft_r * 0.4f),
                .radius = shaft_r,
                .color = col,
            });
        }
        out.triangles.push_back(EvaluatedTriangle{
            .z_index = item.z_index,
            .screen_space = item.screen_space,
            .p0 = tip,
            .p1 = neck + perp * head_half_w,
            .p2 = neck - perp * head_half_w,
            .c0 = col,
            .c1 = col,
            .c2 = col,
        });
    }

    // 4. Graphs & deterministic trajectory trails
    for (const auto& item : graphs_) {
        const float vis = std::clamp(item.visible.evaluate(time_sec), 0.0f, 1.0f);
        const float rev = std::clamp(item.reveal.evaluate(time_sec), 0.0f, 1.0f);
        if (vis <= 1e-4f || rev <= 1e-4f) continue;
        ColorRGBA col = item.color.evaluate(time_sec);
        col.a *= vis;
        const Vec2 graph_origin = apply_parallax(
            item.origin.evaluate(time_sec),
            item.screen_space,
            item.parallax
        ) + item.presentation_offset.evaluate(time_sec);

        EvaluatedGraphDraw draw{
            .id = item.id,
            .z_index = item.z_index,
            .screen_space = item.screen_space,
            .origin = graph_origin,
            .reveal = rev,
            .stroke_width = item.stroke_width.evaluate(time_sec),
            .dash_length = item.dash_length,
            .gap_length = item.gap_length,
            .color = col,
            .dirty_flags = DirtyTransform | DirtyStyle,
        };

        const float amp = item.deform_amplitude.evaluate(time_sec);
        if (item.trail_trajectory_id != kInvalidId) {
            if (const Trajectory2D* tr = find_trajectory(item.trail_trajectory_id)) {
                constexpr int kTrailSamples = 48;
                const double t0 = std::max(tr->t_start, time_sec - item.trail_duration_sec);
                if (time_sec > t0 + 1e-4) {
                    draw.dirty_flags |= DirtyGeometry;
                    draw.dynamic_points.reserve(kTrailSamples);
                    for (int s = 0; s < kTrailSamples; ++s) {
                        const double ts =
                            t0 + (time_sec - t0) * (static_cast<double>(s) / (kTrailSamples - 1));
                        draw.dynamic_points.push_back(tr->evaluate_pos(ts));
                    }
                }
            }
        } else if (std::abs(amp) > 1e-4f && !item.points.empty()) {
            draw.dirty_flags |= DirtyGeometry;
            draw.dynamic_points.resize(item.points.size());
            const float phase = static_cast<float>(time_sec) * item.deform_speed;
            const float sigma = item.deform_envelope_sigma;
            for (size_t i = 0; i < item.points.size(); ++i) {
                const Vec2 p = item.points[i];
                const float env = sigma > 1e-3f ? std::exp(-0.5f * (p.x * p.x) / (sigma * sigma)) : 1.0f;
                const float dy = amp * env * std::sin(p.x * item.deform_freq + phase);
                draw.dynamic_points[i] = Vec2{p.x, p.y + dy};
            }
        }
        if (item.fill_under) {
            const std::vector<Vec2>& src_pts = draw.dynamic_points.empty() ? item.points : draw.dynamic_points;
            if (src_pts.size() >= 2) {
                ColorRGBA c_top = item.fill_top_color.evaluate(time_sec);
                ColorRGBA c_bot = item.fill_bottom_color.evaluate(time_sec);
                c_top.a *= vis;
                c_bot.a *= vis;

                float max_h = 1e-3f;
                for (const Vec2& p : src_pts) {
                    max_h = std::max(max_h, std::abs(p.y - item.fill_baseline_y));
                }
                const float total_len = (!item.cumulative_lengths.empty()) ? item.cumulative_lengths.back() : 1.0f;
                const float max_arc = total_len * rev;
                float acc_arc = 0.0f;
                const Vec2 org = draw.origin;
                for (size_t k = 0; k + 1 < src_pts.size(); ++k) {
                    const float seg_len = (src_pts[k + 1] - src_pts[k]).length();
                    if (draw.dynamic_points.empty() && acc_arc > max_arc) break;
                    const Vec2 a_top = org + src_pts[k];
                    const Vec2 b_top = org + src_pts[k + 1];
                    const Vec2 a_bot{org.x + src_pts[k].x, org.y + item.fill_baseline_y};
                    const Vec2 b_bot{org.x + src_pts[k + 1].x, org.y + item.fill_baseline_y};
                    const float wa = std::clamp(std::abs(src_pts[k].y - item.fill_baseline_y) / max_h, 0.0f, 1.0f);
                    const float wb = std::clamp(std::abs(src_pts[k + 1].y - item.fill_baseline_y) / max_h, 0.0f, 1.0f);
                    const ColorRGBA ca_top = lerp_val(c_bot, c_top, wa);
                    const ColorRGBA cb_top = lerp_val(c_bot, c_top, wb);
                    out.triangles.push_back({item.z_index - 1, item.screen_space, a_bot, a_top, b_top, c_bot, ca_top, cb_top});
                    out.triangles.push_back({item.z_index - 1, item.screen_space, a_bot, b_top, b_bot, c_bot, cb_top, c_bot});
                    acc_arc += seg_len;
                }
            }
        }
        out.graphs.push_back(std::move(draw));
    }

    // 4b. 3D Quantum Orbital Probability Clouds
    for (const auto& oc : orbital_clouds_) {
        const float vis = std::clamp(oc.visible.evaluate(time_sec), 0.0f, 1.0f);
        const float rev = std::clamp(oc.reveal.evaluate(time_sec), 0.0f, 1.0f);
        if (vis <= 1e-4f || rev <= 1e-4f) continue;
        ColorRGBA col = oc.color.evaluate(time_sec);
        col.a *= vis * std::clamp(oc.opacity.evaluate(time_sec), 0.0f, 1.0f);
        out.orbital_clouds.push_back(EvaluatedOrbitalCloud{
            .id = oc.id,
            .z_index = oc.z_index,
            .screen_space = oc.screen_space,
            .center = oc.center.evaluate(time_sec),
            .scale = oc.scale.evaluate(time_sec),
            .morph = std::clamp(oc.morph.evaluate(time_sec), 0.0f, 1.0f),
            .reveal = rev,
            .opacity = std::clamp(oc.opacity.evaluate(time_sec), 0.0f, 1.0f),
            .yaw_rad = oc.yaw_rad.evaluate(time_sec),
            .pitch_rad = oc.pitch_rad.evaluate(time_sec),
            .fov_persp = oc.fov_persp.evaluate(time_sec),
            .flow_phase_sec = static_cast<float>(time_sec) * oc.flow_speed.evaluate(time_sec),
            .collapse = std::clamp(oc.collapse.evaluate(time_sec), 0.0f, 1.0f),
            .collapse_target = oc.collapse_target,
            .dot_radius_px = oc.dot_radius_px,
            .color = col,
        });
    }

    // 5. Stickman rig (updates joints & emits small limb capsules + head circle)
    for (const auto& sm : stickmen_) {
        const float vis = std::clamp(sm.visible.evaluate(time_sec), 0.0f, 1.0f);
        if (vis <= 1e-4f) continue;
        const Vec2 hip = sm.hip_pos.evaluate(time_sec);
        const float sc = sm.scale.evaluate(time_sec);
        const float lean = sm.torso_lean_rad.evaluate(time_sec);
        const float head_tilt = sm.head_tilt_rad.evaluate(time_sec);
        const float r = sm.stroke_width.evaluate(time_sec) * 0.5f * sc;
        ColorRGBA col = sm.color.evaluate(time_sec);
        col.a *= vis;

        const Vec2 torso_dir{std::sin(lean), std::cos(lean)};
        const Vec2 shoulder = hip + torso_dir * (68.0f * sc);
        const Vec2 neck_top = shoulder + torso_dir * (12.0f * sc);
        const Vec2 head_dir{std::sin(lean + head_tilt), std::cos(lean + head_tilt)};
        const Vec2 head_center = neck_top + head_dir * (18.0f * sc);

        auto limb = [&](int z, ColorRGBA lcol, Vec2 root, float a1, float l1, float a2, float l2) {
            const Vec2 j1 = root + Vec2{std::cos(a1), std::sin(a1)} * (l1 * sc);
            const Vec2 j2 = j1 + Vec2{std::cos(a2), std::sin(a2)} * (l2 * sc);
            out.capsules.push_back({z, sm.screen_space, root, j1, r, lcol});
            out.capsules.push_back({z, sm.screen_space, j1, j2, r * 0.9f, lcol});
        };

        const ColorRGBA back_col = sm.depth_shade_back_limbs
            ? ColorRGBA{col.r * 0.72f, col.g * 0.72f, col.b * 0.72f, col.a}
            : col;
        const int back_z = sm.depth_shade_back_limbs ? (sm.z_index - 1) : sm.z_index;

        // Back limbs (shaded for 2.5D depth separation like Median stickman)
        limb(back_z, back_col, shoulder, sm.arm_l_upper_rad.evaluate(time_sec), 36.0f, sm.arm_l_lower_rad.evaluate(time_sec), 34.0f);
        limb(back_z, back_col, hip, sm.leg_l_upper_rad.evaluate(time_sec), 44.0f, sm.leg_l_lower_rad.evaluate(time_sec), 42.0f);
        // Torso + neck
        out.capsules.push_back({sm.z_index, sm.screen_space, hip, neck_top, r * 1.15f, col});
        // Front limbs
        limb(sm.z_index, col, shoulder, sm.arm_r_upper_rad.evaluate(time_sec), 36.0f, sm.arm_r_lower_rad.evaluate(time_sec), 34.0f);
        limb(sm.z_index, col, hip, sm.leg_r_upper_rad.evaluate(time_sec), 44.0f, sm.leg_r_lower_rad.evaluate(time_sec), 42.0f);
        // Head (solid Median silhouette or hollow ring)
        out.circles.push_back(EvaluatedCircle{
            .z_index = sm.z_index,
            .screen_space = sm.screen_space,
            .center = head_center,
            .radius = 18.5f * sc,
            .stroke_width = sm.solid_head ? 0.0f : r * 2.0f,
            .fill_color = sm.solid_head ? col : ColorRGBA{col.r * 0.12f, col.g * 0.14f, col.b * 0.18f, col.a},
            .stroke_color = sm.solid_head ? ColorRGBA{0.0f, 0.0f, 0.0f, 0.0f} : col,
        });
    }

    // 6. Fixed-seed deterministic particle pools (stateless analytic evaluation -> instanced circles)
    for (const auto& pool : particle_pools_) {
        if (time_sec < pool.active_start_sec || time_sec > pool.active_end_sec) continue;
        const float vis = std::clamp(pool.visible.evaluate(time_sec), 0.0f, 1.0f);
        if (vis <= 1e-4f || pool.lifetime_sec <= 1e-4f) continue;
        const Vec2 base = pool.emitter_pos.evaluate(time_sec);
        const ColorRGBA base_col = pool.color.evaluate(time_sec);

        for (uint32_t i = 0; i < pool.count; ++i) {
            const float u0 = hash_unit_f32(pool.seed, i * 4u + 0u);
            const float u1 = hash_unit_f32(pool.seed, i * 4u + 1u);
            const float u2 = hash_unit_f32(pool.seed, i * 4u + 2u);
            const float u3 = hash_unit_f32(pool.seed, i * 4u + 3u);

            const double local_t =
                std::fmod(std::max(0.0, time_sec - pool.active_start_sec) + u0 * pool.lifetime_sec, pool.lifetime_sec);
            const float age01 = static_cast<float>(local_t / pool.lifetime_sec);
            const float env = std::sin(age01 * kPiF);
            if (env <= 1e-3f) continue;

            const Vec2 offset{
                (u1 - 0.5f) * pool.spread.x + pool.base_velocity.x * static_cast<float>(local_t),
                (u2 - 0.5f) * pool.spread.y + pool.base_velocity.y * static_cast<float>(local_t),
            };
            const float rad = pool.min_radius + u3 * (pool.max_radius - pool.min_radius);
            ColorRGBA c = base_col;
            c.a *= vis * env;
            out.circles.push_back(EvaluatedCircle{
                .z_index = pool.z_index,
                .screen_space = pool.screen_space,
                .center = base + offset,
                .radius = rad,
                .stroke_width = 0.0f,
                .fill_color = c,
                .stroke_color = {0.0f, 0.0f, 0.0f, 0.0f},
            });
        }
    }

    // 7. Retained vector path instances
    for (const auto& pi : path_instances_) {
        const float vis = std::clamp(pi.visible.evaluate(time_sec), 0.0f, 1.0f);
        if (vis <= 1e-4f) continue;
        Vec2 pos = pi.position.evaluate(time_sec);
        if (const Trajectory2D* tr = find_trajectory(pi.trajectory_id)) {
            pos = pos + tr->evaluate_pos(time_sec);
        }
        pos = apply_parallax(pos, pi.screen_space, pi.parallax) + pi.presentation_offset.evaluate(time_sec);
        ColorRGBA tint = pi.tint.evaluate(time_sec);
        tint.a *= vis;
        out.paths.push_back(EvaluatedPathDraw{
            .id = pi.id,
            .asset_id = pi.asset_id,
            .z_index = pi.z_index,
            .screen_space = pi.screen_space,
            .position = pos,
            .scale = pi.scale.evaluate(time_sec) * std::max(0.0f, pi.presentation_scale.evaluate(time_sec)),
            .rotation_rad = pi.rotation_rad.evaluate(time_sec) + pi.presentation_rotation_rad.evaluate(time_sec),
            .tint = tint,
        });
    }

    // 8. Retained text items
    for (const auto& txt : texts_) {
        const float vis = std::clamp(txt.visible.evaluate(time_sec), 0.0f, 1.0f);
        if (vis <= 1e-4f) continue;
        Vec2 pos = txt.position.evaluate(time_sec);
        if (const Trajectory2D* tr = find_trajectory(txt.trajectory_id)) {
            pos = pos + tr->evaluate_pos(time_sec);
        }
        pos = apply_parallax(pos, txt.screen_space, txt.parallax) + txt.presentation_offset.evaluate(time_sec);
        ColorRGBA col = txt.color.evaluate(time_sec);
        col.a *= vis;
        out.texts.push_back(EvaluatedTextDraw{
            .id = txt.id,
            .z_index = txt.z_index,
            .screen_space = txt.screen_space,
            .position = pos,
            .scale = txt.scale.evaluate(time_sec) * std::max(0.0f, txt.presentation_scale.evaluate(time_sec)),
            .rotation_rad = txt.rotation_rad.evaluate(time_sec) + txt.presentation_rotation_rad.evaluate(time_sec),
            .color = col,
        });
    }
}

uint64_t Scene::scene_hash() const noexcept {
    uint64_t h = 14695981039346656037ull;
    fnv1a_val(h, width_);
    fnv1a_val(h, height_);
    fnv1a_val(h, duration_sec_);
    fnv1a_val(h, rects_.size());
    fnv1a_val(h, circles_.size());
    fnv1a_val(h, vectors_.size());
    fnv1a_val(h, graphs_.size());
    fnv1a_val(h, orbital_clouds_.size());
    fnv1a_val(h, stickmen_.size());
    fnv1a_val(h, particle_pools_.size());
    fnv1a_val(h, path_assets_.size());
    fnv1a_val(h, texts_.size());
    for (const auto& t : texts_) {
        fnv1a_mix(h, t.text.data(), t.text.size());
    }
    return h;
}

Scene Scene::make_milestone1_showcase(int width, int height, double duration_sec) {
    Scene sc(width, height, duration_sec);
    sc.background().top_color.set_constant({0.055f, 0.082f, 0.135f, 1.0f});
    sc.background().bottom_color.set_constant({0.020f, 0.028f, 0.050f, 1.0f});
    sc.background().show_grid = true;
    sc.background().grid_spacing = 120.0f;

    // Top-left orientation & BT.709 color calibration strip (screen-space, y+ up from center)
    const float half_w = static_cast<float>(width) * 0.5f;
    const float half_h = static_cast<float>(height) * 0.5f;
    const std::array<ColorRGBA, 6> bars = {{
        {1.0f, 0.0f, 0.0f, 1.0f},  // Red
        {0.0f, 1.0f, 0.0f, 1.0f},  // Green
        {0.0f, 0.0f, 1.0f, 1.0f},  // Blue
        {1.0f, 1.0f, 0.0f, 1.0f},  // Yellow
        {0.0f, 1.0f, 1.0f, 1.0f},  // Cyan
        {1.0f, 1.0f, 1.0f, 1.0f},  // White
    }};
    for (size_t i = 0; i < bars.size(); ++i) {
        RectItem bar;
        bar.screen_space = true;
        bar.z_index = 30;
        bar.center.set_constant(Vec2{-half_w + 68.0f + static_cast<float>(i) * 52.0f, half_h - 42.0f});
        bar.size.set_constant(Vec2{44.0f, 26.0f});
        bar.corner_radius.set_constant(4.0f);
        bar.fill_color.set_constant(bars[i]);
        sc.add_rect(std::move(bar));
    }

    // Damped harmonic oscillator trajectory (fixed dt = 1/240s)
    Trajectory2D osc;
    osc.t_start = 0.0;
    osc.dt = 1.0 / 240.0;
    const size_t n_steps = static_cast<size_t>(duration_sec / osc.dt) + 4;
    osc.samples.resize(n_steps);
    for (size_t i = 0; i < n_steps; ++i) {
        const double t = static_cast<double>(i) * osc.dt;
        const double env = std::exp(-0.14 * t);
        const float x = static_cast<float>(340.0 * env * std::cos(2.6 * t));
        const float y = static_cast<float>(160.0 * env * std::sin(5.2 * t));
        osc.samples[i] = Vec2{x, y};
    }
    const ObjectId osc_id = sc.add_trajectory(std::move(osc));

    // Lissajous / phase curve revealed over first 4 seconds
    GraphItem curve;
    curve.z_index = 4;
    curve.stroke_width.set_constant(4.5f);
    curve.color.set_constant({0.25f, 0.72f, 0.98f, 0.65f});
    curve.reveal.add_segment(0.0, 4.0, 0.02f, 1.0f, EasingType::QuinticOut);
    for (int i = 0; i <= 480; ++i) {
        const double t = (duration_sec * static_cast<double>(i)) / 480.0;
        const double env = std::exp(-0.14 * t);
        curve.points.push_back(Vec2{
            static_cast<float>(340.0 * env * std::cos(2.6 * t)),
            static_cast<float>(160.0 * env * std::sin(5.2 * t)),
        });
    }
    sc.add_graph(std::move(curve));

    // Live trail behind oscillating mass
    GraphItem trail;
    trail.z_index = 9;
    trail.stroke_width.set_constant(6.5f);
    trail.color.set_constant({1.0f, 0.68f, 0.22f, 0.92f});
    trail.trail_trajectory_id = osc_id;
    trail.trail_duration_sec = 0.85;
    sc.add_graph(std::move(trail));

    // Oscillating mass body + restoring force vector
    CircleItem mass;
    mass.z_index = 14;
    mass.trajectory_id = osc_id;
    mass.radius.set_constant(26.0f);
    mass.stroke_width.set_constant(4.0f);
    mass.fill_color.set_constant({0.98f, 0.48f, 0.18f, 1.0f});
    mass.stroke_color.set_constant({1.0f, 0.92f, 0.80f, 1.0f});
    sc.add_circle(std::move(mass));

    VectorItem force_vec;
    force_vec.z_index = 16;
    force_vec.attach_trajectory_id = osc_id;
    force_vec.color.set_constant({0.30f, 0.95f, 0.58f, 1.0f});
    force_vec.shaft_width.set_constant(6.0f);
    force_vec.head_length.set_constant(22.0f);
    force_vec.head_width.set_constant(20.0f);
    for (int k = 0; k < 40; ++k) {
        const double t0 = (duration_sec * k) / 40.0;
        const double t1 = (duration_sec * (k + 1)) / 40.0;
        const Vec2 p0 = sc.find_trajectory(osc_id)->evaluate_pos(t0);
        const Vec2 p1 = sc.find_trajectory(osc_id)->evaluate_pos(t1);
        force_vec.delta.add_segment(t0, t1, p0 * -0.42f, p1 * -0.42f, EasingType::Linear);
    }
    sc.add_vector(std::move(force_vec));

    // Stickman presenter pointing toward the harmonic motion
    StickmanItem presenter;
    presenter.z_index = 18;
    presenter.hip_pos.set_constant({-560.0f, -180.0f});
    presenter.scale.set_constant(1.35f);
    presenter.torso_lean_rad.add_segment(0.0, 1.5, 0.0f, 0.12f, EasingType::Spring, 14.0, 0.60);
    presenter.arm_r_upper_rad.add_segment(0.2, 1.4, -1.2f, 0.22f, EasingType::Spring, 16.0, 0.58);
    presenter.arm_r_lower_rad.add_segment(0.2, 1.4, -0.8f, 0.30f, EasingType::Spring, 16.0, 0.58);
    sc.add_stickman(std::move(presenter));

    // Deterministic timeline progress bar at bottom edge
    RectItem progress;
    progress.screen_space = true;
    progress.z_index = 35;
    progress.center.add_segment(
        0.0,
        duration_sec,
        Vec2{-half_w, -half_h + 10.0f},
        Vec2{0.0f, -half_h + 10.0f},
        EasingType::Linear
    );
    progress.size.add_segment(
        0.0,
        duration_sec,
        Vec2{4.0f, 12.0f},
        Vec2{static_cast<float>(width), 12.0f},
        EasingType::Linear
    );
    progress.corner_radius.set_constant(0.0f);
    progress.fill_color.set_constant({0.28f, 0.82f, 0.98f, 1.0f});
    sc.add_rect(std::move(progress));

    // Audio test cues (explicitly marked as test tones)
    sc.add_audio_event(AudioEvent{
        .cue_id = "m1_start_click",
        .time_sec = 0.2,
        .duration_sec = 0.12,
        .gain = 0.35f,
        .pitch_hz = 660.0f,
        .sfx_kind = ProceduralSfxKind::Click,
        .is_test_tone = true,
    });
    sc.add_audio_event(AudioEvent{
        .cue_id = "m1_end_chime",
        .time_sec = std::max(0.5, duration_sec - 0.6),
        .duration_sec = 0.45,
        .gain = 0.35f,
        .pitch_hz = 880.0f,
        .sfx_kind = ProceduralSfxKind::Chime,
        .is_test_tone = true,
    });

    return sc;
}

Scene Scene::make_color_calibration_scene(int width, int height) {
    Scene sc(width, height, 1.0);
    sc.background().top_color.set_constant({0.0f, 0.0f, 0.0f, 1.0f});
    sc.background().bottom_color.set_constant({0.0f, 0.0f, 0.0f, 1.0f});
    sc.background().show_grid = false;

    const float qw = static_cast<float>(width) * 0.25f;
    const float qh = static_cast<float>(height) * 0.25f;

    // Asymmetric 4-quadrant orientation & BT.709 primary/white calibration blocks:
    // Top-Left = Pure Red (1,0,0), Top-Right = Pure Green (0,1,0),
    // Bottom-Left = Pure Blue (0,0,1), Bottom-Right = Pure White (1,1,1)
    auto add_quad = [&](Vec2 c, ColorRGBA col) {
        RectItem r;
        r.screen_space = true;
        r.center.set_constant(c);
        r.size.set_constant(Vec2{qw * 1.6f, qh * 1.6f});
        r.corner_radius.set_constant(0.0f);
        r.fill_color.set_constant(col);
        sc.add_rect(std::move(r));
    };
    add_quad(Vec2{-qw, +qh}, ColorRGBA{1.0f, 0.0f, 0.0f, 1.0f});  // Top-Left: Red
    add_quad(Vec2{+qw, +qh}, ColorRGBA{0.0f, 1.0f, 0.0f, 1.0f});  // Top-Right: Green
    add_quad(Vec2{-qw, -qh}, ColorRGBA{0.0f, 0.0f, 1.0f, 1.0f});  // Bottom-Left: Blue
    add_quad(Vec2{+qw, -qh}, ColorRGBA{1.0f, 1.0f, 1.0f, 1.0f});  // Bottom-Right: White

    // Center semi-transparent yellow swatch (alpha = 0.5 over black/quadrants) to test alpha compositing
    RectItem alpha_patch;
    alpha_patch.screen_space = true;
    alpha_patch.z_index = 10;
    alpha_patch.center.set_constant({0.0f, 0.0f});
    alpha_patch.size.set_constant({160.0f, 160.0f});
    alpha_patch.corner_radius.set_constant(0.0f);
    alpha_patch.fill_color.set_constant({1.0f, 1.0f, 0.0f, 0.5f});
    sc.add_rect(std::move(alpha_patch));

    return sc;
}

void cpu_rgba8_to_nv12_bt709_reference(
    const uint8_t* rgba,
    int width,
    int height,
    bool flip_y,
    ColorRGBA bg_color,
    std::vector<uint8_t>& out_y,
    std::vector<uint8_t>& out_uv
) {
    out_y.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 16u);
    out_uv.assign(static_cast<size_t>(width) * static_cast<size_t>(height / 2), 128u);

    auto fetch_comp = [&](int x, int y, double& r, double& g, double& b) {
        const int cx = std::clamp(x, 0, width - 1);
        const int cy = std::clamp(flip_y ? ((height - 1) - y) : y, 0, height - 1);
        const size_t idx = (static_cast<size_t>(cy) * static_cast<size_t>(width) + static_cast<size_t>(cx)) * 4u;
        const double a = static_cast<double>(rgba[idx + 3]) / 255.0;
        const double sr = static_cast<double>(rgba[idx + 0]) / 255.0;
        const double sg = static_cast<double>(rgba[idx + 1]) / 255.0;
        const double sb = static_cast<double>(rgba[idx + 2]) / 255.0;
        r = sr * a + static_cast<double>(bg_color.r) * (1.0 - a);
        g = sg * a + static_cast<double>(bg_color.g) * (1.0 - a);
        b = sb * a + static_cast<double>(bg_color.b) * (1.0 - a);
    };

    auto clamp_round = [](double v, double lo, double hi) -> uint8_t {
        const double c = std::clamp(v, lo, hi);
        return static_cast<uint8_t>(std::floor(c + 0.5));
    };

    for (int uv_y = 0; uv_y < height / 2; ++uv_y) {
        for (int uv_x = 0; uv_x < width / 2; ++uv_x) {
            const int x0 = uv_x * 2;
            const int y0 = uv_y * 2;
            double r00, g00, b00;
            double r10, g10, b10;
            double r01, g01, b01;
            double r11, g11, b11;
            fetch_comp(x0, y0, r00, g00, b00);
            fetch_comp(x0 + 1, y0, r10, g10, b10);
            fetch_comp(x0, y0 + 1, r01, g01, b01);
            fetch_comp(x0 + 1, y0 + 1, r11, g11, b11);

            auto calc_y = [&](double r, double g, double b) -> uint8_t {
                return clamp_round(16.0 + 219.0 * (0.2126 * r + 0.7152 * g + 0.0722 * b), 16.0, 235.0);
            };
            out_y[static_cast<size_t>(y0) * width + x0] = calc_y(r00, g00, b00);
            out_y[static_cast<size_t>(y0) * width + (x0 + 1)] = calc_y(r10, g10, b10);
            out_y[static_cast<size_t>(y0 + 1) * width + x0] = calc_y(r01, g01, b01);
            out_y[static_cast<size_t>(y0 + 1) * width + (x0 + 1)] = calc_y(r11, g11, b11);

            const double r_avg = 0.25 * (r00 + r10 + r01 + r11);
            const double g_avg = 0.25 * (g00 + g10 + g01 + g11);
            const double b_avg = 0.25 * (b00 + b10 + b01 + b11);

            const double cb =
                128.0 + 224.0 * (-(0.2126 / 1.8556) * r_avg - (0.7152 / 1.8556) * g_avg + 0.5 * b_avg);
            const double cr =
                128.0 + 224.0 * (0.5 * r_avg - (0.7152 / 1.5748) * g_avg - (0.0722 / 1.5748) * b_avg);
            const size_t uv_idx = static_cast<size_t>(uv_y) * width + static_cast<size_t>(uv_x * 2);
            out_uv[uv_idx + 0] = clamp_round(cb, 16.0, 240.0);
            out_uv[uv_idx + 1] = clamp_round(cr, 16.0, 240.0);
        }
    }
}

std::vector<float> mix_scene_audio_f32_stereo(
    const Scene& scene,
    double start_sec,
    double end_sec,
    int sample_rate
) {
    if (end_sec <= start_sec || sample_rate <= 0) return {};
    const size_t total_frames = static_cast<size_t>(std::llround((end_sec - start_sec) * sample_rate));
    std::vector<float> mix(total_frames * 2u, 0.0f);

    for (const auto& ev : scene.audio_events()) {
        if (ev.time_sec + ev.duration_sec < start_sec || ev.time_sec >= end_sec) {
            continue;
        }
        const int64_t dst_start = static_cast<int64_t>(std::llround((ev.time_sec - start_sec) * sample_rate));

        if (!ev.wav_path.empty()) {
            std::vector<float> wav_stereo;
            if (load_wav_pcm_mono_or_stereo(ev.wav_path, sample_rate, wav_stereo)) {
                const size_t wav_frames = wav_stereo.size() / 2u;
                for (size_t i = 0; i < wav_frames; ++i) {
                    const int64_t idx = dst_start + static_cast<int64_t>(i);
                    if (idx >= 0 && static_cast<size_t>(idx) < total_frames) {
                        mix[static_cast<size_t>(idx) * 2u + 0] += wav_stereo[i * 2u + 0] * ev.gain;
                        mix[static_cast<size_t>(idx) * 2u + 1] += wav_stereo[i * 2u + 1] * ev.gain;
                    }
                }
                continue;
            }
        }

        if (ev.sfx_kind == ProceduralSfxKind::None || ev.duration_sec <= 1e-4) continue;
        const size_t ev_frames = static_cast<size_t>(std::llround(ev.duration_sec * sample_rate));
        for (size_t i = 0; i < ev_frames; ++i) {
            const int64_t idx = dst_start + static_cast<int64_t>(i);
            if (idx < 0 || static_cast<size_t>(idx) >= total_frames) continue;
            const double t = static_cast<double>(i) / static_cast<double>(sample_rate);
            const double u = t / ev.duration_sec;
            float sample = 0.0f;

            switch (ev.sfx_kind) {
                case ProceduralSfxKind::Click: {
                    const double env = std::exp(-38.0 * u);
                    sample = static_cast<float>(env * std::sin(2.0 * kPi * ev.pitch_hz * t));
                    break;
                }
                case ProceduralSfxKind::Release: {
                    const double env = std::exp(-14.0 * u);
                    const double freq = ev.pitch_hz * (1.0 + 0.45 * u);
                    sample = static_cast<float>(env * std::sin(2.0 * kPi * freq * t));
                    break;
                }
                case ProceduralSfxKind::Slide: {
                    // Filtered gentle friction whoosh
                    const float n = hash_unit_f32(9001u, static_cast<uint32_t>(i)) * 2.0f - 1.0f;
                    const double env = std::sin(kPi * u);
                    const double mod = 0.5 + 0.5 * std::sin(2.0 * kPi * 180.0 * t * (0.6 + 0.8 * u));
                    sample = static_cast<float>(env * mod * n * 0.35);
                    break;
                }
                case ProceduralSfxKind::Contact: {
                    const double env = std::exp(-22.0 * u);
                    sample = static_cast<float>(
                        env * (0.75 * std::sin(2.0 * kPi * 115.0 * t) + 0.25 * std::sin(2.0 * kPi * 230.0 * t))
                    );
                    break;
                }
                case ProceduralSfxKind::Chime: {
                    const double env = std::exp(-6.5 * u) * std::min(1.0, u * 40.0);
                    sample = static_cast<float>(
                        env * (0.65 * std::sin(2.0 * kPi * ev.pitch_hz * t) +
                               0.35 * std::sin(2.0 * kPi * (ev.pitch_hz * 1.5) * t))
                    );
                    break;
                }
                case ProceduralSfxKind::None:
                    break;
            }

            mix[static_cast<size_t>(idx) * 2u + 0] += sample * ev.gain;
            mix[static_cast<size_t>(idx) * 2u + 1] += sample * ev.gain;
        }
    }

    // Soft clip to [-0.98, 0.98]
    for (float& s : mix) {
        s = std::clamp(s, -0.98f, 0.98f);
    }
    return mix;
}

}  // namespace stem
