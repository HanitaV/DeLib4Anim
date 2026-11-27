#include "native/render/renderer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <format>

namespace stem::render {

namespace {

constexpr const char* kFullQuadVs = R"GLSL(#version 450 core
out vec2 v_uv;
void main() {
    vec2 pos = vec2((gl_VertexID == 1) ? 3.0 : -1.0, (gl_VertexID == 2) ? 3.0 : -1.0);
    v_uv = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)GLSL";

constexpr const char* kBgFs = R"GLSL(#version 450 core
in vec2 v_uv;
out vec4 frag_color;

uniform vec4 u_top_color;
uniform vec4 u_bottom_color;
uniform vec4 u_grid_color;
uniform vec2 u_resolution;
uniform vec2 u_cam_pos;
uniform float u_cam_zoom;
uniform float u_cam_rot;
uniform float u_grid_spacing;
uniform int u_show_grid;
uniform float u_starfield_intensity;
uniform float u_vignette_strength;
uniform float u_grain_amount;
uniform float u_perspective_grid;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 hash22(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}

float star_layer(vec2 uv_px, float cell_sz, float density, float cross_strength) {
    vec2 cell = floor(uv_px / cell_sz);
    vec2 f = fract(uv_px / cell_sz);
    float h = hash12(cell);
    if (h > density) return 0.0;
    vec2 jitter = 0.16 + 0.68 * hash22(cell + 17.3);
    vec2 dp = (f - jitter) * cell_sz;
    float d2 = dot(dp, dp);
    float r = 0.42 + 0.65 * fract(h * 97.13);
    float brightness = 0.22 + 0.78 * fract(h * 31.7);
    float core = exp(-d2 / (r * r));
    if (cross_strength > 0.001) {
        float rays = exp(-abs(dp.x) * 1.9 - abs(dp.y) * 0.28) + exp(-abs(dp.y) * 1.9 - abs(dp.x) * 0.28);
        core += cross_strength * 0.22 * rays * exp(-d2 * 0.035);
    }
    return brightness * core;
}

void main() {
    vec3 col = mix(u_bottom_color.rgb, u_top_color.rgb, clamp(v_uv.y, 0.0, 1.0));
    vec2 screen_p = (v_uv - 0.5) * u_resolution;

    if (u_starfield_intensity > 0.001) {
        // Three parallax crisp pin-prick star layers matching Median's obsidian night sky
        vec2 p1 = screen_p + u_cam_pos * 0.04;
        vec2 p2 = screen_p + u_cam_pos * 0.08;
        vec2 p3 = screen_p + u_cam_pos * 0.12;
        float s1 = star_layer(p1, 150.0, 0.20, 0.0);
        float s2 = star_layer(p2 + vec2(431.0, 197.0), 92.0, 0.14, 0.0);
        float s3 = star_layer(p3 + vec2(817.0, 523.0), 260.0, 0.075, 1.0);
        float stars = (s1 * 0.68 + s2 * 0.45 + s3 * 1.15) * u_starfield_intensity;
        col += vec3(0.95, 0.96, 0.99) * stars;
    }

    if (u_show_grid != 0 && u_grid_spacing > 1.0) {
        float cs = cos(-u_cam_rot);
        float sn = sin(-u_cam_rot);
        vec2 world_p = vec2(cs * screen_p.x - sn * screen_p.y, sn * screen_p.x + cs * screen_p.y) / u_cam_zoom + u_cam_pos;
        vec2 g = abs(fract(world_p / u_grid_spacing - 0.5) - 0.5) / (fwidth(world_p / u_grid_spacing) + 1e-5);
        float line = 1.0 - min(min(g.x, g.y), 1.0);
        vec2 axis_d = abs(world_p) / (fwidth(world_p) + 1e-5);
        float axis_line = 1.0 - min(min(axis_d.x, axis_d.y), 1.0);
        col = mix(col, u_grid_color.rgb, clamp(line * u_grid_color.a + axis_line * 0.25, 0.0, 0.85));
    }

    if (u_perspective_grid > 0.001 && v_uv.y < 0.48) {
        float horizon = 0.48;
        float dy = horizon - v_uv.y;
        float z_depth = 0.28 / max(dy, 0.003);
        float x_floor = (v_uv.x - 0.5) * (u_resolution.x / max(u_resolution.y, 1.0)) * z_depth * 1.7;
        vec2 gp = vec2(x_floor, z_depth) * 1.85;
        vec2 g = abs(fract(gp - 0.5) - 0.5) / (fwidth(gp) + 1e-5);
        float line = 1.0 - min(min(g.x, g.y), 1.0);
        float horizon_fade = smoothstep(0.0, 0.09, dy) * exp(-z_depth * 0.13);
        float radial_fade = exp(-dot(vec2(v_uv.x - 0.5, dy * 1.2), vec2(v_uv.x - 0.5, dy * 1.2)) * 2.4);
        col = mix(col, vec3(0.24, 0.25, 0.28), clamp(line * horizon_fade * radial_fade * u_perspective_grid, 0.0, 0.65));
    }

    if (u_vignette_strength > 0.001) {
        vec2 d = (v_uv - 0.5) * 1.414;
        float vig = 1.0 - u_vignette_strength * dot(d, d);
        col *= clamp(vig, 0.35, 1.0);
    }

    if (u_grain_amount > 0.0001) {
        float n = hash12(gl_FragCoord.xy) - 0.5;
        col += vec3(n * u_grain_amount);
    }

    frag_color = vec4(max(col, vec3(0.0)), 1.0);
}
)GLSL";

// Common camera transform helper injected into world/screen shaders
#define STEM_WORLD_TO_CLIP_GLSL \
"uniform vec2 u_resolution;\n" \
"uniform vec2 u_cam_pos;\n" \
"uniform float u_cam_zoom;\n" \
"uniform float u_cam_rot;\n" \
"vec2 to_screen_px(vec2 p, float screen_space) {\n" \
"    if (screen_space > 0.5) return p;\n" \
"    vec2 d = (p - u_cam_pos) * u_cam_zoom;\n" \
"    float cs = cos(u_cam_rot);\n" \
"    float sn = sin(u_cam_rot);\n" \
"    return vec2(cs * d.x - sn * d.y, sn * d.x + cs * d.y);\n" \
"}\n" \
"vec4 screen_px_to_clip(vec2 sp) {\n" \
"    return vec4(sp.x / (0.5 * u_resolution.x), sp.y / (0.5 * u_resolution.y), 0.0, 1.0);\n" \
"}\n"

constexpr const char* kRectVs = R"GLSL(#version 450 core
layout(location = 0) in vec4 i_geom;    // cx, cy, sx, sy
layout(location = 1) in vec4 i_params;  // rot, corner_r, stroke_w, screen_space
layout(location = 2) in vec4 i_fill;
layout(location = 3) in vec4 i_stroke;
layout(location = 4) in vec4 i_extra;   // mat_mode, elevation, 0, 0

out vec2 v_local_px;
out vec2 v_half_size;
out float v_corner_r;
out float v_stroke_w;
out float v_mat_mode;
out float v_elev;
out vec2 v_shadow_off_local;
out vec4 v_fill;
out vec4 v_stroke;

)GLSL" STEM_WORLD_TO_CLIP_GLSL R"GLSL(
void main() {
    const vec2 corners[6] = vec2[6](
        vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
        vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0)
    );
    vec2 q = corners[gl_VertexID];
    float scale_f = (i_params.w > 0.5) ? 1.0 : u_cam_zoom;
    vec2 half_sz = i_geom.zw * 0.5 * scale_f;
    float elev_px = max(0.0, i_extra.y) * scale_f;
    float shadow_pad = (elev_px > 0.05 && (i_extra.x < 2.5 || i_extra.x >= 3.5)) ? (elev_px * 2.2 + 4.0) : 0.0;
    float pad = 2.5 + i_params.z * scale_f + shadow_pad;
    vec2 local = q * (half_sz + vec2(pad));
    float rot = i_params.x + ((i_params.w > 0.5) ? 0.0 : u_cam_rot);
    float cs = cos(rot);
    float sn = sin(rot);
    vec2 rotated = vec2(cs * local.x - sn * local.y, sn * local.x + cs * local.y);
    vec2 center_sp = to_screen_px(i_geom.xy, i_params.w);
    vec2 sh_sp = vec2(0.0, -elev_px * 0.52);

    v_local_px = local;
    v_half_size = half_sz;
    v_corner_r = min(i_params.y * scale_f, min(half_sz.x, half_sz.y));
    v_stroke_w = i_params.z * scale_f;
    v_mat_mode = i_extra.x;
    v_elev = elev_px;
    v_shadow_off_local = vec2(cs * sh_sp.x + sn * sh_sp.y, -sn * sh_sp.x + cs * sh_sp.y);
    v_fill = i_fill;
    v_stroke = i_stroke;
    gl_Position = screen_px_to_clip(center_sp + rotated);
}
)GLSL";

constexpr const char* kRectFs = R"GLSL(#version 450 core
in vec2 v_local_px;
in vec2 v_half_size;
in float v_corner_r;
in float v_stroke_w;
in float v_mat_mode;
in float v_elev;
in vec2 v_shadow_off_local;
in vec4 v_fill;
in vec4 v_stroke;
out vec4 frag_color;

float sd_rounded_box(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float hash_px(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    if (v_mat_mode > 2.5 && v_mat_mode < 3.5) {
        // Material 3: SoftContactGlow — tight bright contact seam + soft elliptical pressure halo
        vec2 np = v_local_px / max(v_half_size, vec2(1.0));
        float core = exp(-np.y * np.y * 28.0) * exp(-np.x * np.x * 2.4);
        float halo = 0.46 * exp(-np.y * np.y * 4.2) * exp(-np.x * np.x * 1.6);
        float a = clamp(v_fill.a * (core + halo), 0.0, 1.0);
        if (a <= 0.002) discard;
        frag_color = vec4(v_fill.rgb, a);
        return;
    }

    float d = sd_rounded_box(v_local_px, v_half_size, v_corner_r);
    float aa = max(fwidth(d), 0.75);
    float fill_cov = 1.0 - smoothstep(-aa * 0.5, aa * 0.5, d);

    vec3 rgb = v_fill.rgb;
    if (v_mat_mode > 0.5 && v_mat_mode < 1.5) {
        // Material 1: MatteSurface — smooth Median matte finish + subtle top rim sheen
        float dither = (hash_px(gl_FragCoord.xy) - 0.5) * 0.004;
        float top_rim = smoothstep(v_half_size.y - 1.8, v_half_size.y, v_local_px.y) * 0.022;
        rgb = clamp(rgb + vec3(dither + top_rim), 0.0, 1.0);
    } else if (v_mat_mode >= 1.5 && v_mat_mode < 2.5) {
        // Material 2: LayeredBook — dark spine/cover boards + clean mid-gray page block with 4 crisp seams
        float board_h = clamp(v_half_size.y * 0.22, 2.5, 6.5);
        float spine_w = clamp(v_half_size.y * 0.34, 4.0, 10.0);
        float page_half_h = max(1.0, v_half_size.y - board_h);
        if (abs(v_local_px.y) < page_half_h &&
            v_local_px.x > (-v_half_size.x + spine_w) &&
            v_local_px.x < (v_half_size.x - 1.5)) {
            float u = (v_local_px.y / page_half_h) * 2.5;
            float dist_line = abs(fract(u) - 0.5) / max(fwidth(u), 0.02);
            float line_cov = 1.0 - smoothstep(0.4, 1.3, dist_line);
            vec3 page_col = mix(vec3(0.52, 0.53, 0.55), vec3(0.35, 0.36, 0.38), line_cov * 0.85);
            float cov_edge_x = smoothstep(-v_half_size.x + spine_w, -v_half_size.x + spine_w + 1.5, v_local_px.x);
            float cov_edge_y = 1.0 - smoothstep(page_half_h - 1.2, page_half_h, abs(v_local_px.y));
            rgb = mix(rgb, page_col, cov_edge_x * cov_edge_y * 0.96);
        } else {
            float dither = (hash_px(gl_FragCoord.xy) - 0.5) * 0.004;
            rgb = clamp(rgb + vec3(dither), 0.0, 1.0);
        }
    } else if (v_mat_mode >= 3.5) {
        // Material 4: Card25D — layered 2.5D UI/STEM panel (vertical sheen + top inner rim + bottom lip)
        float dither = (hash_px(gl_FragCoord.xy) - 0.5) * 0.004;
        float yn = clamp(v_local_px.y / max(v_half_size.y, 1.0), -1.0, 1.0);
        float inner_mask = 1.0 - smoothstep(-1.0, 0.2, d);
        float top_rim = (1.0 - smoothstep(0.0, 2.2, max(0.0, v_half_size.y - v_local_px.y))) * inner_mask * 0.065;
        float bot_lip = (1.0 - smoothstep(0.0, 2.6, max(0.0, v_local_px.y + v_half_size.y))) * inner_mask * 0.055;
        rgb = clamp(rgb + vec3(yn * 0.026 + top_rim - bot_lip + dither), 0.0, 1.0);
    }

    vec4 col = vec4(rgb, v_fill.a * fill_cov);
    if (v_stroke_w > 0.1 && v_stroke.a > 0.001) {
        float half_sw = v_stroke_w * 0.5;
        float sd_ring = abs(d) - half_sw;
        float stroke_cov = 1.0 - smoothstep(-aa * 0.5, aa * 0.5, sd_ring);
        float sa = v_stroke.a * stroke_cov;
        col.rgb = mix(col.rgb, v_stroke.rgb, sa);
        col.a = max(col.a, sa);
    }
    if (v_elev > 0.05) {
        float d_sh = sd_rounded_box(v_local_px - v_shadow_off_local, v_half_size, v_corner_r);
        float sh_blur = max(2.0, v_elev * 1.30);
        float sh_cov = 1.0 - smoothstep(-sh_blur * 0.35, sh_blur, d_sh);
        float sh_a = sh_cov * clamp(v_elev / 26.0, 0.0, 1.0) * 0.44 * v_fill.a;
        float out_a = col.a + sh_a * (1.0 - col.a);
        vec3 sh_rgb = vec3(0.008, 0.010, 0.016);
        col = vec4((col.rgb * col.a + sh_rgb * sh_a * (1.0 - col.a)) / max(out_a, 1e-5), out_a);
    }
    if (col.a <= 0.001) discard;
    frag_color = col;
}
)GLSL";

constexpr const char* kCircleVs = R"GLSL(#version 450 core
layout(location = 0) in vec4 i_geom;   // cx, cy, radius, stroke_w
layout(location = 1) in vec4 i_flags;  // screen_space, mat_mode, elevation, 0
layout(location = 2) in vec4 i_fill;
layout(location = 3) in vec4 i_stroke;

out vec2 v_local_px;
out float v_radius;
out float v_stroke_w;
out float v_mat_mode;
out float v_elev;
out vec4 v_fill;
out vec4 v_stroke;

)GLSL" STEM_WORLD_TO_CLIP_GLSL R"GLSL(
void main() {
    const vec2 corners[6] = vec2[6](
        vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
        vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0)
    );
    float scale_f = (i_flags.x > 0.5) ? 1.0 : u_cam_zoom;
    float r = i_geom.z * scale_f;
    float sw = i_geom.w * scale_f;
    float elev_px = max(0.0, i_flags.z) * scale_f;
    bool is_glow = (i_flags.y > 1.5 && i_flags.y < 2.5);
    float shadow_pad = (!is_glow && elev_px > 0.05) ? (elev_px * 2.2 + 4.0) : 0.0;
    float extent = is_glow ? (r * 2.5 + 3.0) : (r + sw * 0.5 + 2.5 + shadow_pad);
    vec2 local = corners[gl_VertexID] * extent;
    vec2 center_sp = to_screen_px(i_geom.xy, i_flags.x);

    v_local_px = local;
    v_radius = r;
    v_stroke_w = sw;
    v_mat_mode = i_flags.y;
    v_elev = is_glow ? 0.0 : elev_px;
    v_fill = i_fill;
    v_stroke = i_stroke;
    gl_Position = screen_px_to_clip(center_sp + local);
}
)GLSL";

constexpr const char* kCircleFs = R"GLSL(#version 450 core
in vec2 v_local_px;
in float v_radius;
in float v_stroke_w;
in float v_mat_mode;
in float v_elev;
in vec4 v_fill;
in vec4 v_stroke;
out vec4 frag_color;

void main() {
    if (v_mat_mode > 1.5 && v_mat_mode < 2.5) {
        // Material 2: EmissiveGlow — bright white core + soft radial bloom halo (Median electrons & photons)
        float rn = length(v_local_px) / max(v_radius, 0.5);
        float core = 1.0 - smoothstep(0.0, 0.44, rn);
        float halo = exp(-rn * rn * 2.1);
        float a = clamp(v_fill.a * (core * 0.96 + halo * 0.62), 0.0, 1.0);
        if (a <= 0.002) discard;
        vec3 rgb = mix(v_fill.rgb, vec3(1.0), core * 0.85);
        frag_color = vec4(rgb, a);
        return;
    }

    float d = length(v_local_px) - v_radius;
    float aa = max(fwidth(d), 0.75);
    float fill_cov = 1.0 - smoothstep(-aa * 0.5, aa * 0.5, d);

    vec3 rgb = v_fill.rgb;
    if (v_mat_mode > 0.5 && v_mat_mode < 1.5) {
        // Material 1: Sphere3D — matte Lambert + subtle specular + soft limb rim
        vec2 q = v_local_px / max(v_radius, 0.5);
        float r2 = min(dot(q, q), 1.0);
        vec3 n = vec3(q.x, q.y, sqrt(max(0.0, 1.0 - r2)));
        vec3 light_dir = normalize(vec3(-0.42, 0.58, 0.70));
        float diff = max(dot(n, light_dir), 0.0);
        vec3 half_vec = normalize(light_dir + vec3(0.0, 0.0, 1.0));
        float spec = pow(max(dot(n, half_vec), 0.0), 22.0) * 0.26;
        float rim = pow(1.0 - n.z, 3.0) * 0.10;
        rgb = clamp(rgb * (0.28 + 0.78 * diff) + vec3(spec + rim), 0.0, 1.0);
    } else if (v_mat_mode >= 2.5) {
        // Material 3: Disc25D — clean layered 2.5D flat disc with top crescent rim & subtle bottom lip
        vec2 q = v_local_px / max(v_radius, 0.5);
        float r_norm = length(q);
        float rim_ring = smoothstep(0.68, 0.94, r_norm) * (1.0 - smoothstep(0.94, 1.02, r_norm));
        float top_crescent = smoothstep(0.15, 0.85, q.y) * rim_ring * 0.11;
        float bot_lip = smoothstep(0.15, 0.85, -q.y) * rim_ring * 0.08;
        rgb = clamp(rgb + vec3(clamp(q.y, -1.0, 1.0) * 0.038 + top_crescent - bot_lip), 0.0, 1.0);
    }

    vec4 col = vec4(rgb, v_fill.a * fill_cov);
    if (v_stroke_w > 0.1 && v_stroke.a > 0.001) {
        float sd_ring = abs(d) - v_stroke_w * 0.5;
        float stroke_cov = 1.0 - smoothstep(-aa * 0.5, aa * 0.5, sd_ring);
        float sa = v_stroke.a * stroke_cov;
        col.rgb = mix(col.rgb, v_stroke.rgb, sa);
        col.a = max(col.a, sa);
    }
    if (v_elev > 0.05) {
        float d_sh = length(v_local_px - vec2(0.0, -v_elev * 0.52)) - v_radius;
        float sh_blur = max(2.0, v_elev * 1.30);
        float sh_cov = 1.0 - smoothstep(-sh_blur * 0.35, sh_blur, d_sh);
        float sh_a = sh_cov * clamp(v_elev / 26.0, 0.0, 1.0) * 0.44 * v_fill.a;
        float out_a = col.a + sh_a * (1.0 - col.a);
        vec3 sh_rgb = vec3(0.008, 0.010, 0.016);
        col = vec4((col.rgb * col.a + sh_rgb * sh_a * (1.0 - col.a)) / max(out_a, 1e-5), out_a);
    }
    if (col.a <= 0.001) discard;
    frag_color = col;
}
)GLSL";

constexpr const char* kCapsuleVs = R"GLSL(#version 450 core
layout(location = 0) in vec4 i_pts;    // ax, ay, bx, by
layout(location = 1) in vec4 i_params; // radius, screen_space, 0, 0
layout(location = 2) in vec4 i_color;

out vec2 v_local_px;
out float v_half_len;
out float v_radius;
out vec4 v_color;

)GLSL" STEM_WORLD_TO_CLIP_GLSL R"GLSL(
void main() {
    const vec2 corners[6] = vec2[6](
        vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
        vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0)
    );
    float scale_f = (i_params.y > 0.5) ? 1.0 : u_cam_zoom;
    vec2 pa = to_screen_px(i_pts.xy, i_params.y);
    vec2 pb = to_screen_px(i_pts.zw, i_params.y);
    vec2 mid = 0.5 * (pa + pb);
    vec2 diff = pb - pa;
    float len = length(diff);
    vec2 dir = (len > 1e-4) ? (diff / len) : vec2(1.0, 0.0);
    vec2 perp = vec2(-dir.y, dir.x);

    float rad = max(0.5, i_params.x * scale_f);
    float half_len = len * 0.5;
    float pad = rad + 2.0;
    vec2 q = corners[gl_VertexID];
    vec2 local = vec2(q.x * (half_len + pad), q.y * pad);
    vec2 sp = mid + dir * local.x + perp * local.y;

    v_local_px = local;
    v_half_len = half_len;
    v_radius = rad;
    v_color = i_color;
    gl_Position = screen_px_to_clip(sp);
}
)GLSL";

constexpr const char* kCapsuleFs = R"GLSL(#version 450 core
in vec2 v_local_px;
in float v_half_len;
in float v_radius;
in vec4 v_color;
out vec4 frag_color;

void main() {
    vec2 p = v_local_px;
    p.x = abs(p.x) - v_half_len;
    float d = length(vec2(max(p.x, 0.0), p.y)) - v_radius;
    float aa = max(fwidth(d), 0.75);
    float cov = 1.0 - smoothstep(-aa * 0.5, aa * 0.5, d);
    if (cov * v_color.a <= 0.001) discard;
    frag_color = vec4(v_color.rgb, v_color.a * cov);
}
)GLSL";

constexpr const char* kTriVs = R"GLSL(#version 450 core
layout(location = 0) in vec4 a_pos_flags; // x, y, screen_space, 0
layout(location = 1) in vec4 a_color;
out vec4 v_color;
)GLSL" STEM_WORLD_TO_CLIP_GLSL R"GLSL(
void main() {
    vec2 sp = to_screen_px(a_pos_flags.xy, a_pos_flags.z);
    v_color = a_color;
    gl_Position = screen_px_to_clip(sp);
}
)GLSL";

constexpr const char* kTriFs = R"GLSL(#version 450 core
in vec4 v_color;
out vec4 frag_color;
void main() {
    frag_color = v_color;
}
)GLSL";

constexpr const char* kGraphVs = R"GLSL(#version 450 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_normal;
layout(location = 2) in float a_arc_len;
layout(location = 3) in float a_side;

uniform vec2 u_origin;
uniform float u_half_width;
uniform float u_screen_space;

out float v_arc_len;
out float v_side;

)GLSL" STEM_WORLD_TO_CLIP_GLSL R"GLSL(
void main() {
    float scale_f = (u_screen_space > 0.5) ? 1.0 : u_cam_zoom;
    vec2 p_world = u_origin + a_pos;
    vec2 sp = to_screen_px(p_world, u_screen_space);
    float cs = (u_screen_space > 0.5) ? 1.0 : cos(u_cam_rot);
    float sn = (u_screen_space > 0.5) ? 0.0 : sin(u_cam_rot);
    vec2 n_rot = vec2(cs * a_normal.x - sn * a_normal.y, sn * a_normal.x + cs * a_normal.y);
    sp += n_rot * (a_side * (u_half_width * scale_f + 1.0));
    v_arc_len = a_arc_len;
    v_side = a_side * (u_half_width * scale_f + 1.0);
    gl_Position = screen_px_to_clip(sp);
}
)GLSL";

constexpr const char* kGraphFs = R"GLSL(#version 450 core
in float v_arc_len;
in float v_side;
uniform float u_max_arc_len;
uniform float u_half_width_px;
uniform float u_dash_len;
uniform float u_gap_len;
uniform vec4 u_color;
out vec4 frag_color;

void main() {
    float arc_cov = 1.0 - smoothstep(u_max_arc_len - 1.5, u_max_arc_len + 0.5, v_arc_len);
    float edge_d = abs(v_side) - u_half_width_px;
    float edge_cov = 1.0 - smoothstep(-0.65, 0.65, edge_d);
    float dash_cov = 1.0;
    if (u_dash_len > 0.5 && u_gap_len > 0.5) {
        float period = u_dash_len + u_gap_len;
        float phase = mod(v_arc_len, period);
        dash_cov = (1.0 - smoothstep(u_dash_len - 0.75, u_dash_len + 0.75, phase)) *
                   smoothstep(-0.75, 0.75, phase);
        if (phase < 1.0) dash_cov = max(dash_cov, smoothstep(-0.75, 0.75, period - phase));
    }
    float a = u_color.a * arc_cov * edge_cov * dash_cov;
    if (a <= 0.001) discard;
    frag_color = vec4(u_color.rgb, a);
}
)GLSL";

constexpr const char* kPathVs = R"GLSL(#version 450 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec4 a_color;

uniform vec2 u_pos;
uniform float u_scale;
uniform float u_rot;
uniform float u_screen_space;
uniform vec4 u_tint;

out vec4 v_color;

)GLSL" STEM_WORLD_TO_CLIP_GLSL R"GLSL(
void main() {
    float cs = cos(u_rot);
    float sn = sin(u_rot);
    vec2 local = vec2(cs * a_pos.x - sn * a_pos.y, sn * a_pos.x + cs * a_pos.y) * u_scale;
    vec2 sp = to_screen_px(u_pos + local, u_screen_space);
    v_color = a_color * u_tint;
    gl_Position = screen_px_to_clip(sp);
}
)GLSL";

constexpr const char* kPathFs = R"GLSL(#version 450 core
in vec4 v_color;
out vec4 frag_color;
void main() {
    frag_color = v_color;
}
)GLSL";

constexpr const char* kOrbitalVs = R"GLSL(#version 450 core
layout(location = 0) in vec4 i_state_a;  // r_perp_a, y_a, phi0_a, omega_a
layout(location = 1) in vec4 i_state_b;  // r_perp_b, y_b, phi0_b, omega_b
layout(location = 2) in vec2 i_meta;     // size_scale, seed01

uniform vec2 u_center;
uniform float u_scale;
uniform float u_morph;
uniform float u_reveal;
uniform float u_yaw;
uniform float u_pitch;
uniform float u_fov_persp;
uniform float u_flow_phase;
uniform float u_collapse;
uniform vec2 u_collapse_target;
uniform float u_dot_radius;
uniform float u_screen_space;
uniform float u_depth_scale;

out vec2 v_quad_uv;
out float v_depth_cue;
out float v_seed01;

)GLSL" STEM_WORLD_TO_CLIP_GLSL R"GLSL(
void main() {
    const vec2 corners[6] = vec2[6](
        vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
        vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0)
    );
    v_seed01 = i_meta.y;
    if (i_meta.y > u_reveal) {
        v_quad_uv = vec2(2.0);
        v_depth_cue = 1.0;
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }

    float phi_a = i_state_a.z + i_state_a.w * u_flow_phase;
    float phi_b = i_state_b.z + i_state_b.w * u_flow_phase;
    vec3 pa = vec3(i_state_a.x * cos(phi_a), i_state_a.y, i_state_a.x * sin(phi_a));
    vec3 pb = vec3(i_state_b.x * cos(phi_b), i_state_b.y, i_state_b.x * sin(phi_b));
    vec3 p3 = mix(pa, pb, u_morph) * u_scale;

    // Yaw around vertical axis, then camera pitch around X
    float cy = cos(u_yaw);
    float sy = sin(u_yaw);
    vec3 py = vec3(cy * p3.x + sy * p3.z, p3.y, -sy * p3.x + cy * p3.z);
    float cp_x = cos(u_pitch);
    float sp_x = sin(u_pitch);
    vec3 pv = vec3(py.x, cp_x * py.y - sp_x * py.z, sp_x * py.y + cp_x * py.z);

    vec3 c3 = mix(pv, vec3(u_collapse_target, 0.0), u_collapse);
    float persp = 1.0 / max(0.28, 1.0 - c3.z * u_fov_persp);
    float cam_s = (u_screen_space > 0.5) ? 1.0 : u_cam_zoom;
    float rad_px = max(0.9, u_dot_radius * i_meta.x * persp * cam_s);

    vec2 center_sp = to_screen_px(u_center + c3.xy * persp, u_screen_space);
    vec2 q = corners[gl_VertexID];
    v_quad_uv = q * 1.18;
    vec2 sp = center_sp + v_quad_uv * rad_px;
    float z_ndc = clamp(-c3.z / 950.0, -0.95, 0.95);
    v_depth_cue = clamp(0.76 + 0.34 * (c3.z / max(u_depth_scale, 40.0)), 0.34, 1.12);
    gl_Position = vec4(sp.x / (0.5 * u_resolution.x), sp.y / (0.5 * u_resolution.y), z_ndc, 1.0);
}
)GLSL";

constexpr const char* kOrbitalFs = R"GLSL(#version 450 core
in vec2 v_quad_uv;
in float v_depth_cue;
in float v_seed01;

uniform float u_reveal;
uniform float u_opacity;
uniform vec4 u_color;

out vec4 frag_color;

void main() {
    if (v_seed01 > u_reveal) discard;
    float r = length(v_quad_uv);
    float aa = max(fwidth(r), 0.14);
    float cov = 1.0 - smoothstep(1.0 - aa, 1.0 + aa * 0.4, r);
    if (cov <= 0.04) discard;

    vec2 q = v_quad_uv / max(1.0, r);
    float z_sp = sqrt(max(0.0, 1.0 - dot(q, q)));
    vec3 n = vec3(q.x, q.y, z_sp);
    vec3 L = normalize(vec3(-0.38, 0.58, 0.72));
    float diff = max(dot(n, L), 0.0);
    vec3 H = normalize(L + vec3(0.0, 0.0, 1.0));
    float spec = pow(max(dot(n, H), 0.0), 20.0) * 0.24;
    vec3 rgb = u_color.rgb * (0.28 + 0.78 * diff) * v_depth_cue + vec3(spec * v_depth_cue);
    frag_color = vec4(clamp(rgb, 0.0, 1.0), clamp(cov * u_opacity, 0.0, 1.0));
}
)GLSL";

constexpr const char* kTextVs = R"GLSL(#version 450 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;

uniform vec2 u_pos;
uniform float u_scale;
uniform float u_rot;
uniform float u_screen_space;

out vec2 v_uv;

)GLSL" STEM_WORLD_TO_CLIP_GLSL R"GLSL(
void main() {
    float cs = cos(u_rot);
    float sn = sin(u_rot);
    vec2 local = vec2(cs * a_pos.x - sn * a_pos.y, sn * a_pos.x + cs * a_pos.y) * u_scale;
    vec2 sp = to_screen_px(u_pos + local, u_screen_space);
    v_uv = a_uv;
    gl_Position = screen_px_to_clip(sp);
}
)GLSL";

constexpr const char* kTextFs = R"GLSL(#version 450 core
in vec2 v_uv;
uniform sampler2D u_atlas;
uniform vec4 u_color;
out vec4 frag_color;
void main() {
    float cov = texture(u_atlas, v_uv).r;
    float a = u_color.a * cov;
    if (a <= 0.002) discard;
    frag_color = vec4(u_color.rgb, a);
}
)GLSL";

Result<GLuint> compile_shader(const gl::GlDispatch& gl, GLenum type, const char* src, const char* label) {
    GLuint sh = gl.CreateShader(type);
    gl.ShaderSource(sh, 1, &src, nullptr);
    gl.CompileShader(sh);
    GLint ok = 0;
    gl.GetShaderiv(sh, gl::kCompileStatus, &ok);
    if (!ok) {
        GLint len = 0;
        gl.GetShaderiv(sh, gl::kInfoLogLength, &len);
        std::string log(static_cast<size_t>(std::max(1, len)), '\0');
        gl.GetShaderInfoLog(sh, len, nullptr, log.data());
        gl.DeleteShader(sh);
        return fail(std::format("Shader compile error ({}): {}", label, log));
    }
    return sh;
}

Result<GLuint> link_program(const gl::GlDispatch& gl, const char* vs_src, const char* fs_src, const char* label) {
    auto vs = compile_shader(gl, gl::kVertexShader, vs_src, label);
    if (!vs) return fail(vs.error());
    auto fs = compile_shader(gl, gl::kFragmentShader, fs_src, label);
    if (!fs) {
        gl.DeleteShader(*vs);
        return fail(fs.error());
    }

    GLuint prog = gl.CreateProgram();
    gl.AttachShader(prog, *vs);
    gl.AttachShader(prog, *fs);
    gl.LinkProgram(prog);
    gl.DeleteShader(*vs);
    gl.DeleteShader(*fs);

    GLint ok = 0;
    gl.GetProgramiv(prog, gl::kLinkStatus, &ok);
    if (!ok) {
        GLint len = 0;
        gl.GetProgramiv(prog, gl::kInfoLogLength, &len);
        std::string log(static_cast<size_t>(std::max(1, len)), '\0');
        gl.GetProgramInfoLog(prog, len, nullptr, log.data());
        gl.DeleteProgram(prog);
        return fail(std::format("Program link error ({}): {}", label, log));
    }
    return prog;
}

void set_camera_uniforms(
    const gl::GlDispatch& gl,
    GLint loc_res,
    GLint loc_pos,
    GLint loc_zoom,
    GLint loc_rot,
    int width,
    int height,
    const EvaluatedCamera& cam
) {
    gl.Uniform2f(loc_res, static_cast<float>(width), static_cast<float>(height));
    gl.Uniform2f(loc_pos, cam.position.x, cam.position.y);
    gl.Uniform1f(loc_zoom, cam.zoom);
    gl.Uniform1f(loc_rot, cam.rotation_rad);
}

template <typename T>
void ensure_upload_buffer(
    const gl::GlDispatch& gl,
    GLuint vbo,
    const std::vector<T>& vec,
    size_t& capacity_count
) {
    if (vec.empty()) return;
    const size_t bytes = vec.size() * sizeof(T);
    if (vec.size() > capacity_count) {
        capacity_count = std::max(vec.size(), capacity_count * 2u + 64u);
        gl.NamedBufferData(vbo, static_cast<gl::GLsizeiptr>(capacity_count * sizeof(T)), nullptr, gl::kStreamDraw);
    }
    gl.NamedBufferSubData(vbo, 0, static_cast<gl::GLsizeiptr>(bytes), vec.data());
}

}  // namespace

Renderer::~Renderer() {
    shutdown();
}

Result<void> Renderer::initialize(
    const gl::GlDispatch& gl,
    int width,
    int height,
    int ring_size,
    int msaa_samples
) {
    shutdown();
    gl_ = gl;
    width_ = width;
    height_ = height;
    msaa_samples_ = std::max(1, msaa_samples);
    const int n_slots = std::max(1, ring_size);

    if (auto r = compile_programs(); !r) {
        shutdown();
        return r;
    }

    // Shared 4x MSAA FBO (color + depth24/stencil8 for non-zero winding SVG paths)
    gl_.CreateFramebuffers(1, &msaa_fbo_);
    gl_.CreateRenderbuffers(1, &msaa_color_rb_);
    gl_.CreateRenderbuffers(1, &msaa_ds_rb_);
    gl_.NamedRenderbufferStorageMultisample(msaa_color_rb_, msaa_samples_, gl::kRgba8, width_, height_);
    gl_.NamedRenderbufferStorageMultisample(msaa_ds_rb_, msaa_samples_, gl::kDepth24Stencil8, width_, height_);
    gl_.NamedFramebufferRenderbuffer(msaa_fbo_, gl::kColorAttachment0, gl::kRenderbuffer, msaa_color_rb_);
    gl_.NamedFramebufferRenderbuffer(msaa_fbo_, gl::kDepthStencilAttachment, gl::kRenderbuffer, msaa_ds_rb_);
    if (gl_.CheckNamedFramebufferStatus(msaa_fbo_, gl::kFramebuffer) != gl::kFramebufferComplete) {
        shutdown();
        return fail("MSAA offscreen framebuffer is incomplete");
    }

    // Ring pool of single-sample resolve textures + FBOs + async GL_TIME_ELAPSED queries
    slots_.resize(static_cast<size_t>(n_slots));
    for (int i = 0; i < n_slots; ++i) {
        auto& s = slots_[static_cast<size_t>(i)];
        gl_.CreateTextures(gl::kTexture2D, 1, &s.color_tex);
        gl_.TextureStorage2D(s.color_tex, 1, gl::kRgba8, width_, height_);
        gl_.TextureParameteri(s.color_tex, gl::kTextureMinFilter, gl::kLinear);
        gl_.TextureParameteri(s.color_tex, gl::kTextureMagFilter, gl::kLinear);
        gl_.TextureParameteri(s.color_tex, gl::kTextureWrapS, gl::kClampToEdge);
        gl_.TextureParameteri(s.color_tex, gl::kTextureWrapT, gl::kClampToEdge);

        gl_.CreateFramebuffers(1, &s.resolve_fbo);
        gl_.NamedFramebufferTexture(s.resolve_fbo, gl::kColorAttachment0, s.color_tex, 0);
        if (gl_.CheckNamedFramebufferStatus(s.resolve_fbo, gl::kFramebuffer) != gl::kFramebufferComplete) {
            shutdown();
            return fail("Resolve offscreen framebuffer is incomplete");
        }
        gl_.GenQueries(1, &s.timer_query);
    }

    gl_.CreateVertexArrays(1, &empty_vao_);

    // Rect VAO (5 vec4 attributes including material_mode)
    gl_.CreateVertexArrays(1, &inst_vao_rect_);
    gl_.CreateBuffers(1, &inst_vbo_rect_);
    gl_.VertexArrayVertexBuffer(inst_vao_rect_, 0, inst_vbo_rect_, 0, sizeof(GpuRectInst));
    gl_.VertexArrayBindingDivisor(inst_vao_rect_, 0, 1);
    for (GLuint a = 0; a < 5; ++a) {
        gl_.EnableVertexArrayAttrib(inst_vao_rect_, a);
        gl_.VertexArrayAttribFormat(inst_vao_rect_, a, 4, gl::kFloat, GL_FALSE, a * 16u);
        gl_.VertexArrayAttribBinding(inst_vao_rect_, a, 0);
    }

    // Circle VAO
    gl_.CreateVertexArrays(1, &inst_vao_circle_);
    gl_.CreateBuffers(1, &inst_vbo_circle_);
    gl_.VertexArrayVertexBuffer(inst_vao_circle_, 0, inst_vbo_circle_, 0, sizeof(GpuCircleInst));
    gl_.VertexArrayBindingDivisor(inst_vao_circle_, 0, 1);
    for (GLuint a = 0; a < 4; ++a) {
        gl_.EnableVertexArrayAttrib(inst_vao_circle_, a);
        gl_.VertexArrayAttribFormat(inst_vao_circle_, a, 4, gl::kFloat, GL_FALSE, a * 16u);
        gl_.VertexArrayAttribBinding(inst_vao_circle_, a, 0);
    }

    // Capsule VAO
    gl_.CreateVertexArrays(1, &inst_vao_capsule_);
    gl_.CreateBuffers(1, &inst_vbo_capsule_);
    gl_.VertexArrayVertexBuffer(inst_vao_capsule_, 0, inst_vbo_capsule_, 0, sizeof(GpuCapsuleInst));
    gl_.VertexArrayBindingDivisor(inst_vao_capsule_, 0, 1);
    for (GLuint a = 0; a < 3; ++a) {
        gl_.EnableVertexArrayAttrib(inst_vao_capsule_, a);
        gl_.VertexArrayAttribFormat(inst_vao_capsule_, a, 4, gl::kFloat, GL_FALSE, a * 16u);
        gl_.VertexArrayAttribBinding(inst_vao_capsule_, a, 0);
    }

    // Triangle VAO
    gl_.CreateVertexArrays(1, &tri_vao_);
    gl_.CreateBuffers(1, &tri_vbo_);
    gl_.VertexArrayVertexBuffer(tri_vao_, 0, tri_vbo_, 0, sizeof(GpuTriVert));
    for (GLuint a = 0; a < 2; ++a) {
        gl_.EnableVertexArrayAttrib(tri_vao_, a);
        gl_.VertexArrayAttribFormat(tri_vao_, a, 4, gl::kFloat, GL_FALSE, a * 16u);
        gl_.VertexArrayAttribBinding(tri_vao_, a, 0);
    }

    // Graph strip VAO (attributes bound per-graph VBO)
    gl_.CreateVertexArrays(1, &graph_vao_);
    gl_.EnableVertexArrayAttrib(graph_vao_, 0);
    gl_.VertexArrayAttribFormat(graph_vao_, 0, 2, gl::kFloat, GL_FALSE, 0);
    gl_.VertexArrayAttribBinding(graph_vao_, 0, 0);
    gl_.EnableVertexArrayAttrib(graph_vao_, 1);
    gl_.VertexArrayAttribFormat(graph_vao_, 1, 2, gl::kFloat, GL_FALSE, 8);
    gl_.VertexArrayAttribBinding(graph_vao_, 1, 0);
    gl_.EnableVertexArrayAttrib(graph_vao_, 2);
    gl_.VertexArrayAttribFormat(graph_vao_, 2, 1, gl::kFloat, GL_FALSE, 16);
    gl_.VertexArrayAttribBinding(graph_vao_, 2, 0);
    gl_.EnableVertexArrayAttrib(graph_vao_, 3);
    gl_.VertexArrayAttribFormat(graph_vao_, 3, 1, gl::kFloat, GL_FALSE, 20);
    gl_.VertexArrayAttribBinding(graph_vao_, 3, 0);

    // 3D Quantum Orbital Cloud instanced VAO (stride = 40 bytes)
    gl_.CreateVertexArrays(1, &orbital_vao_);
    gl_.VertexArrayBindingDivisor(orbital_vao_, 0, 1);
    gl_.EnableVertexArrayAttrib(orbital_vao_, 0);
    gl_.VertexArrayAttribFormat(orbital_vao_, 0, 4, gl::kFloat, GL_FALSE, 0);
    gl_.VertexArrayAttribBinding(orbital_vao_, 0, 0);
    gl_.EnableVertexArrayAttrib(orbital_vao_, 1);
    gl_.VertexArrayAttribFormat(orbital_vao_, 1, 4, gl::kFloat, GL_FALSE, 16);
    gl_.VertexArrayAttribBinding(orbital_vao_, 1, 0);
    gl_.EnableVertexArrayAttrib(orbital_vao_, 2);
    gl_.VertexArrayAttribFormat(orbital_vao_, 2, 2, gl::kFloat, GL_FALSE, 32);
    gl_.VertexArrayAttribBinding(orbital_vao_, 2, 0);

    // Path VAO
    gl_.CreateVertexArrays(1, &path_vao_);
    gl_.EnableVertexArrayAttrib(path_vao_, 0);
    gl_.VertexArrayAttribFormat(path_vao_, 0, 2, gl::kFloat, GL_FALSE, offsetof(PathVertex, x));
    gl_.VertexArrayAttribBinding(path_vao_, 0, 0);
    gl_.EnableVertexArrayAttrib(path_vao_, 1);
    gl_.VertexArrayAttribFormat(path_vao_, 1, 4, gl::kFloat, GL_FALSE, offsetof(PathVertex, r));
    gl_.VertexArrayAttribBinding(path_vao_, 1, 0);

    // Text VAO
    gl_.CreateVertexArrays(1, &text_vao_);
    gl_.EnableVertexArrayAttrib(text_vao_, 0);
    gl_.VertexArrayAttribFormat(text_vao_, 0, 2, gl::kFloat, GL_FALSE, 0);
    gl_.VertexArrayAttribBinding(text_vao_, 0, 0);
    gl_.EnableVertexArrayAttrib(text_vao_, 1);
    gl_.VertexArrayAttribFormat(text_vao_, 1, 2, gl::kFloat, GL_FALSE, 8);
    gl_.VertexArrayAttribBinding(text_vao_, 1, 0);

    if (auto fr = font_cache_.initialize(gl_); !fr) {
        shutdown();
        return fr;
    }

    initialized_ = true;
    return {};
}

Result<void> Renderer::compile_programs() {
    auto load = [&](GLuint& dst, const char* vs, const char* fs, const char* name) -> Result<void> {
        auto p = link_program(gl_, vs, fs, name);
        if (!p) return fail(p.error());
        dst = *p;
        return {};
    };
    if (auto r = load(prog_bg_, kFullQuadVs, kBgFs, "bg"); !r) return r;
    if (auto r = load(prog_rect_, kRectVs, kRectFs, "rect"); !r) return r;
    if (auto r = load(prog_circle_, kCircleVs, kCircleFs, "circle"); !r) return r;
    if (auto r = load(prog_capsule_, kCapsuleVs, kCapsuleFs, "capsule"); !r) return r;
    if (auto r = load(prog_tri_, kTriVs, kTriFs, "tri"); !r) return r;
    if (auto r = load(prog_graph_, kGraphVs, kGraphFs, "graph"); !r) return r;
    if (auto r = load(prog_orbital_, kOrbitalVs, kOrbitalFs, "orbital"); !r) return r;
    if (auto r = load(prog_path_, kPathVs, kPathFs, "path"); !r) return r;
    if (auto r = load(prog_text_, kTextVs, kTextFs, "text"); !r) return r;

    auto cache_cam = [&](GLuint prog, CamUniforms& u) {
        u.resolution = gl_.GetUniformLocation(prog, "u_resolution");
        u.cam_pos = gl_.GetUniformLocation(prog, "u_cam_pos");
        u.cam_zoom = gl_.GetUniformLocation(prog, "u_cam_zoom");
        u.cam_rot = gl_.GetUniformLocation(prog, "u_cam_rot");
    };

    u_bg_.resolution = gl_.GetUniformLocation(prog_bg_, "u_resolution");
    u_bg_.cam_pos = gl_.GetUniformLocation(prog_bg_, "u_cam_pos");
    u_bg_.cam_zoom = gl_.GetUniformLocation(prog_bg_, "u_cam_zoom");
    u_bg_.cam_rot = gl_.GetUniformLocation(prog_bg_, "u_cam_rot");
    u_bg_.top_color = gl_.GetUniformLocation(prog_bg_, "u_top_color");
    u_bg_.bottom_color = gl_.GetUniformLocation(prog_bg_, "u_bottom_color");
    u_bg_.grid_color = gl_.GetUniformLocation(prog_bg_, "u_grid_color");
    u_bg_.grid_spacing = gl_.GetUniformLocation(prog_bg_, "u_grid_spacing");
    u_bg_.show_grid = gl_.GetUniformLocation(prog_bg_, "u_show_grid");
    u_bg_.starfield_intensity = gl_.GetUniformLocation(prog_bg_, "u_starfield_intensity");
    u_bg_.vignette_strength = gl_.GetUniformLocation(prog_bg_, "u_vignette_strength");
    u_bg_.grain_amount = gl_.GetUniformLocation(prog_bg_, "u_grain_amount");
    u_bg_.perspective_grid = gl_.GetUniformLocation(prog_bg_, "u_perspective_grid");

    cache_cam(prog_rect_, u_rect_);
    cache_cam(prog_circle_, u_circle_);
    cache_cam(prog_capsule_, u_capsule_);
    cache_cam(prog_tri_, u_tri_);

    cache_cam(prog_graph_, u_graph_);
    u_graph_.origin = gl_.GetUniformLocation(prog_graph_, "u_origin");
    u_graph_.half_width = gl_.GetUniformLocation(prog_graph_, "u_half_width");
    u_graph_.half_width_px = gl_.GetUniformLocation(prog_graph_, "u_half_width_px");
    u_graph_.screen_space = gl_.GetUniformLocation(prog_graph_, "u_screen_space");
    u_graph_.max_arc_len = gl_.GetUniformLocation(prog_graph_, "u_max_arc_len");
    u_graph_.color = gl_.GetUniformLocation(prog_graph_, "u_color");
    u_graph_.dash_len = gl_.GetUniformLocation(prog_graph_, "u_dash_len");
    u_graph_.gap_len = gl_.GetUniformLocation(prog_graph_, "u_gap_len");

    cache_cam(prog_orbital_, u_orbital_);
    u_orbital_.center = gl_.GetUniformLocation(prog_orbital_, "u_center");
    u_orbital_.scale = gl_.GetUniformLocation(prog_orbital_, "u_scale");
    u_orbital_.morph = gl_.GetUniformLocation(prog_orbital_, "u_morph");
    u_orbital_.reveal = gl_.GetUniformLocation(prog_orbital_, "u_reveal");
    u_orbital_.opacity = gl_.GetUniformLocation(prog_orbital_, "u_opacity");
    u_orbital_.yaw = gl_.GetUniformLocation(prog_orbital_, "u_yaw");
    u_orbital_.pitch = gl_.GetUniformLocation(prog_orbital_, "u_pitch");
    u_orbital_.fov_persp = gl_.GetUniformLocation(prog_orbital_, "u_fov_persp");
    u_orbital_.flow_phase = gl_.GetUniformLocation(prog_orbital_, "u_flow_phase");
    u_orbital_.collapse = gl_.GetUniformLocation(prog_orbital_, "u_collapse");
    u_orbital_.collapse_target = gl_.GetUniformLocation(prog_orbital_, "u_collapse_target");
    u_orbital_.dot_radius = gl_.GetUniformLocation(prog_orbital_, "u_dot_radius");
    u_orbital_.screen_space = gl_.GetUniformLocation(prog_orbital_, "u_screen_space");
    u_orbital_.depth_scale = gl_.GetUniformLocation(prog_orbital_, "u_depth_scale");
    u_orbital_.color = gl_.GetUniformLocation(prog_orbital_, "u_color");

    cache_cam(prog_path_, u_path_);
    u_path_.pos = gl_.GetUniformLocation(prog_path_, "u_pos");
    u_path_.scale = gl_.GetUniformLocation(prog_path_, "u_scale");
    u_path_.rot = gl_.GetUniformLocation(prog_path_, "u_rot");
    u_path_.screen_space = gl_.GetUniformLocation(prog_path_, "u_screen_space");
    u_path_.tint = gl_.GetUniformLocation(prog_path_, "u_tint");

    cache_cam(prog_text_, u_text_);
    u_text_.atlas = gl_.GetUniformLocation(prog_text_, "u_atlas");
    u_text_.pos = gl_.GetUniformLocation(prog_text_, "u_pos");
    u_text_.scale = gl_.GetUniformLocation(prog_text_, "u_scale");
    u_text_.rot = gl_.GetUniformLocation(prog_text_, "u_rot");
    u_text_.screen_space = gl_.GetUniformLocation(prog_text_, "u_screen_space");
    u_text_.color = gl_.GetUniformLocation(prog_text_, "u_color");
    return {};
}

void Renderer::update_graph_gpu(const GraphItem& item, const std::vector<Vec2>* override_pts) {
    const std::vector<Vec2>& pts = override_pts ? *override_pts : item.points;
    auto& gpu = graph_cache_[item.id];
    if (gpu.vbo == 0) {
        gl_.CreateBuffers(1, &gpu.vbo);
    }
    if (pts.size() < 2) {
        gpu.vertex_count = 0;
        gpu.total_arc_length = 0.0f;
        return;
    }

    scratch_graph_verts_.clear();
    scratch_graph_verts_.reserve(pts.size() * 2u);
    float arc = 0.0f;
    for (size_t i = 0; i < pts.size(); ++i) {
        if (i > 0) {
            arc += (pts[i] - pts[i - 1]).length();
        }
        Vec2 tangent{};
        if (i == 0) {
            tangent = (pts[1] - pts[0]).normalized();
        } else if (i + 1 == pts.size()) {
            tangent = (pts[i] - pts[i - 1]).normalized();
        } else {
            const Vec2 t0 = (pts[i] - pts[i - 1]).normalized();
            const Vec2 t1 = (pts[i + 1] - pts[i]).normalized();
            tangent = (t0 + t1).normalized();
            if (tangent.length() < 1e-4f) tangent = t1;
        }
        const Vec2 normal{-tangent.y, tangent.x};
        scratch_graph_verts_.push_back({pts[i].x, pts[i].y, normal.x, normal.y, arc, -1.0f});
        scratch_graph_verts_.push_back({pts[i].x, pts[i].y, normal.x, normal.y, arc, +1.0f});
    }

    if (scratch_graph_verts_.size() > gpu.capacity_vertices) {
        gpu.capacity_vertices = scratch_graph_verts_.size() + 64u;
        gl_.NamedBufferData(
            gpu.vbo,
            static_cast<gl::GLsizeiptr>(gpu.capacity_vertices * sizeof(GpuGraphVert)),
            nullptr,
            override_pts ? gl::kDynamicDraw : gl::kStaticDraw
        );
    }
    gl_.NamedBufferSubData(
        gpu.vbo,
        0,
        static_cast<gl::GLsizeiptr>(scratch_graph_verts_.size() * sizeof(GpuGraphVert)),
        scratch_graph_verts_.data()
    );
    gpu.vertex_count = static_cast<GLsizei>(scratch_graph_verts_.size());
    gpu.total_arc_length = std::max(arc, 1e-3f);
    gpu.is_static = !item.deform_amplitude.is_animated() &&
                    std::abs(item.deform_amplitude.default_value()) <= 1e-4f &&
                    item.trail_trajectory_id == kInvalidId;
}

Result<void> Renderer::prepare_scene_assets(const Scene& scene) {
    if (!initialized_) return fail("Renderer not initialized");
    const auto t0 = std::chrono::steady_clock::now();

    // Clear previous scene asset caches
    for (auto& [id, g] : graph_cache_) {
        if (g.vbo) gl_.DeleteBuffers(1, &g.vbo);
    }
    graph_cache_.clear();
    for (auto& [id, oc] : orbital_cache_) {
        if (oc.vbo) gl_.DeleteBuffers(1, &oc.vbo);
    }
    orbital_cache_.clear();
    for (auto& [id, p] : path_cache_) {
        if (p.vbo) gl_.DeleteBuffers(1, &p.vbo);
        if (p.ibo) gl_.DeleteBuffers(1, &p.ibo);
        for (auto& b : p.batches) {
            if (b.cover_vbo) gl_.DeleteBuffers(1, &b.cover_vbo);
        }
    }
    path_cache_.clear();
    text_specs_.clear();

    // 1. Pre-upload static graphs
    for (const auto& g : scene.graphs()) {
        update_graph_gpu(g, nullptr);
    }

    // 1b. Pre-sample and upload 3D Quantum Orbital Probability Clouds
    for (const auto& oc : scene.orbital_clouds()) {
        const uint32_t count = std::max<uint32_t>(1u, oc.particle_count);
        const auto sa = sample_hydrogen_orbital_3d(oc.state_a, count, oc.seed);
        const auto sb = sample_hydrogen_orbital_3d(oc.state_b, count, oc.seed + 7919u);

        std::vector<GpuOrbitalVert> verts(count);
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t h1 = oc.seed ^ (i * 2654435761u);
            h1 ^= h1 >> 16;
            h1 *= 0x7feb352du;
            h1 ^= h1 >> 15;
            const float u_sz = static_cast<float>(h1 & 0xFFFFu) / 65535.0f;
            const float u_rev = static_cast<float>((h1 >> 16) & 0xFFFFu) / 65535.0f;
            verts[i] = GpuOrbitalVert{
                .r_perp_a = sa[i].r_perp_px,
                .y_a = sa[i].y_px,
                .phi0_a = sa[i].phi0_rad,
                .omega_a = sa[i].omega_rad_per_sec,
                .r_perp_b = sb[i].r_perp_px,
                .y_b = sb[i].y_px,
                .phi0_b = sb[i].phi0_rad,
                .omega_b = sb[i].omega_rad_per_sec,
                .size_scale = 0.78f + 0.44f * u_sz,
                .seed01 = u_rev,
            };
        }
        RetainedOrbitalGpu gpu{};
        gl_.CreateBuffers(1, &gpu.vbo);
        gl_.NamedBufferData(
            gpu.vbo,
            static_cast<gl::GLsizeiptr>(verts.size() * sizeof(GpuOrbitalVert)),
            verts.data(),
            gl::kStaticDraw
        );
        gpu.instance_count = static_cast<GLsizei>(count);
        gpu.depth_scale = std::max({
            80.0f,
            static_cast<float>(oc.state_a.n * oc.state_a.n) * oc.state_a.bohr_radius_px * 1.4f,
            static_cast<float>(oc.state_b.n * oc.state_b.n) * oc.state_b.bohr_radius_px * 1.4f,
        });
        orbital_cache_[oc.id] = gpu;
    }

    // 2. Pre-upload retained PathAssets grouped into per-color stencil-then-cover batches
    for (const auto& pa : scene.path_assets()) {
        if (pa.vertices.empty() || pa.indices.size() < 3) continue;
        RetainedPathGpu gpu{};
        gl_.CreateBuffers(1, &gpu.vbo);
        gl_.NamedBufferData(
            gpu.vbo,
            static_cast<gl::GLsizeiptr>(pa.vertices.size() * sizeof(PathVertex)),
            pa.vertices.data(),
            gl::kStaticDraw
        );
        gl_.CreateBuffers(1, &gpu.ibo);
        gl_.NamedBufferData(
            gpu.ibo,
            static_cast<gl::GLsizeiptr>(pa.indices.size() * sizeof(uint32_t)),
            pa.indices.data(),
            gl::kStaticDraw
        );

        auto same_color = [](const PathVertex& a, const PathVertex& b) noexcept {
            return std::abs(a.r - b.r) < 1e-3f &&
                   std::abs(a.g - b.g) < 1e-3f &&
                   std::abs(a.b - b.b) < 1e-3f &&
                   std::abs(a.a - b.a) < 1e-3f;
        };

        const size_t n_idx = (pa.indices.size() / 3u) * 3u;
        size_t start = 0;
        while (start < n_idx) {
            const PathVertex ref = pa.vertices[pa.indices[start]];
            float min_x = 1e30f, min_y = 1e30f, max_x = -1e30f, max_y = -1e30f;
            size_t end = start;
            while (end < n_idx) {
                const PathVertex& v0 = pa.vertices[pa.indices[end + 0]];
                if (end > start && !same_color(ref, v0)) break;
                for (size_t k = 0; k < 3; ++k) {
                    const PathVertex& vk = pa.vertices[pa.indices[end + k]];
                    min_x = std::min(min_x, vk.x);
                    min_y = std::min(min_y, vk.y);
                    max_x = std::max(max_x, vk.x);
                    max_y = std::max(max_y, vk.y);
                }
                end += 3;
            }
            const float pad = 2.0f;
            const float x0 = min_x - pad;
            const float y0 = min_y - pad;
            const float x1 = max_x + pad;
            const float y1 = max_y + pad;
            const PathVertex cover[6] = {
                {x0, y0, ref.r, ref.g, ref.b, ref.a},
                {x1, y0, ref.r, ref.g, ref.b, ref.a},
                {x1, y1, ref.r, ref.g, ref.b, ref.a},
                {x0, y0, ref.r, ref.g, ref.b, ref.a},
                {x1, y1, ref.r, ref.g, ref.b, ref.a},
                {x0, y1, ref.r, ref.g, ref.b, ref.a},
            };
            PathColorBatch batch{};
            batch.first_index = static_cast<GLsizei>(start);
            batch.index_count = static_cast<GLsizei>(end - start);
            gl_.CreateBuffers(1, &batch.cover_vbo);
            gl_.NamedBufferData(batch.cover_vbo, sizeof(cover), cover, gl::kStaticDraw);
            gpu.batches.push_back(batch);
            start = end;
        }
        path_cache_[pa.asset_id] = std::move(gpu);
    }

    // 3. Pre-shape and bake glyph runs for all TextItems (never re-rasterized per frame)
    for (const auto& ti : scene.texts()) {
        text_specs_[ti.id] = ti;
        auto run = font_cache_.prepare_text_run(ti);
        if (!run) return fail(run.error());
    }

    const auto t1 = std::chrono::steady_clock::now();
    last_asset_compile_ms_ = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return {};
}

Result<void> Renderer::render(const SceneSnapshot& snapshot, size_t slot_index) {
    if (!initialized_) return fail("Renderer not initialized");
    if (slot_index >= slots_.size()) return fail("Invalid render target slot index");

    auto& slot = slots_[slot_index];
    gl_.BeginQuery(gl::kTimeElapsed, slot.timer_query);

    gl_.BindFramebuffer(gl::kFramebuffer, msaa_fbo_);
    gl_.Viewport(0, 0, width_, height_);
    gl_.Disable(gl::kDepthTest);
    gl_.Disable(gl::kCullFace);
    gl_.Enable(gl::kMultisample);
    gl_.Disable(gl::kFramebufferSrgb);
    gl_.Enable(gl::kBlend);
    gl_.BlendFuncSeparate(gl::kSrcAlpha, gl::kOneMinusSrcAlpha, gl::kOne, gl::kOneMinusSrcAlpha);
    gl_.ClearStencil(0);
    gl_.StencilMask(0xFF);
    gl_.ClearColor(snapshot.bg_bottom.r, snapshot.bg_bottom.g, snapshot.bg_bottom.b, 1.0f);
    gl_.Clear(gl::kColorBufferBit | gl::kStencilBufferBit);

    // 1. Background gradient + optional grid
    gl_.Disable(gl::kBlend);
    gl_.UseProgram(prog_bg_);
    set_camera_uniforms(gl_, u_bg_.resolution, u_bg_.cam_pos, u_bg_.cam_zoom, u_bg_.cam_rot, width_, height_, snapshot.camera);
    gl_.Uniform4f(u_bg_.top_color, snapshot.bg_top.r, snapshot.bg_top.g, snapshot.bg_top.b, 1.0f);
    gl_.Uniform4f(u_bg_.bottom_color, snapshot.bg_bottom.r, snapshot.bg_bottom.g, snapshot.bg_bottom.b, 1.0f);
    gl_.Uniform4f(u_bg_.grid_color, snapshot.grid_color.r, snapshot.grid_color.g, snapshot.grid_color.b, snapshot.grid_color.a);
    gl_.Uniform1f(u_bg_.grid_spacing, snapshot.grid_spacing);
    gl_.Uniform1i(u_bg_.show_grid, snapshot.show_grid ? 1 : 0);
    gl_.Uniform1f(u_bg_.starfield_intensity, snapshot.starfield_intensity);
    gl_.Uniform1f(u_bg_.vignette_strength, snapshot.vignette_strength);
    gl_.Uniform1f(u_bg_.grain_amount, snapshot.grain_amount);
    gl_.Uniform1f(u_bg_.perspective_grid, snapshot.perspective_grid_opacity);
    gl_.BindVertexArray(empty_vao_);
    gl_.DrawArrays(gl::kTriangles, 0, 3);
    gl_.Enable(gl::kBlend);

    // Build sorted z-index draw buckets across all primitive types using reusable staging buffer
    scratch_draw_items_.clear();
    for (uint32_t i = 0; i < snapshot.rects.size(); ++i) scratch_draw_items_.push_back({snapshot.rects[i].z_index, DrawKind::Rect, i});
    for (uint32_t i = 0; i < snapshot.circles.size(); ++i) scratch_draw_items_.push_back({snapshot.circles[i].z_index, DrawKind::Circle, i});
    for (uint32_t i = 0; i < snapshot.capsules.size(); ++i) scratch_draw_items_.push_back({snapshot.capsules[i].z_index, DrawKind::Capsule, i});
    for (uint32_t i = 0; i < snapshot.triangles.size(); ++i) scratch_draw_items_.push_back({snapshot.triangles[i].z_index, DrawKind::Tri, i});
    for (uint32_t i = 0; i < snapshot.graphs.size(); ++i) scratch_draw_items_.push_back({snapshot.graphs[i].z_index, DrawKind::Graph, i});
    for (uint32_t i = 0; i < snapshot.orbital_clouds.size(); ++i) scratch_draw_items_.push_back({snapshot.orbital_clouds[i].z_index, DrawKind::OrbitalCloud, i});
    for (uint32_t i = 0; i < snapshot.paths.size(); ++i) scratch_draw_items_.push_back({snapshot.paths[i].z_index, DrawKind::Path, i});
    for (uint32_t i = 0; i < snapshot.texts.size(); ++i) scratch_draw_items_.push_back({snapshot.texts[i].z_index, DrawKind::Text, i});

    std::ranges::stable_sort(scratch_draw_items_, [](const DrawItem& a, const DrawItem& b) {
        if (a.z != b.z) return a.z < b.z;
        return static_cast<uint8_t>(a.kind) < static_cast<uint8_t>(b.kind);
    });

    // Stage geometry into GPU buffers once per frame in sorted order
    scratch_rects_.clear();
    scratch_circles_.clear();
    scratch_capsules_.clear();
    scratch_tris_.clear();
    for (const auto& di : scratch_draw_items_) {
        switch (di.kind) {
            case DrawKind::Rect: {
                const auto& r = snapshot.rects[di.idx];
                scratch_rects_.push_back({
                    r.center.x, r.center.y, r.size.x, r.size.y,
                    r.rotation_rad, r.corner_radius, r.stroke_width, r.screen_space ? 1.0f : 0.0f,
                    r.fill_color.r, r.fill_color.g, r.fill_color.b, r.fill_color.a,
                    r.stroke_color.r, r.stroke_color.g, r.stroke_color.b, r.stroke_color.a,
                    static_cast<float>(r.material_mode), r.elevation, 0.0f, 0.0f,
                });
                break;
            }
            case DrawKind::Circle: {
                const auto& c = snapshot.circles[di.idx];
                scratch_circles_.push_back({
                    c.center.x, c.center.y, c.radius, c.stroke_width,
                    c.screen_space ? 1.0f : 0.0f, static_cast<float>(c.material_mode), c.elevation, 0.0f,
                    c.fill_color.r, c.fill_color.g, c.fill_color.b, c.fill_color.a,
                    c.stroke_color.r, c.stroke_color.g, c.stroke_color.b, c.stroke_color.a,
                });
                break;
            }
            case DrawKind::Capsule: {
                const auto& cap = snapshot.capsules[di.idx];
                scratch_capsules_.push_back({
                    cap.a.x, cap.a.y, cap.b.x, cap.b.y,
                    cap.radius, cap.screen_space ? 1.0f : 0.0f, 0.0f, 0.0f,
                    cap.color.r, cap.color.g, cap.color.b, cap.color.a,
                });
                break;
            }
            case DrawKind::Tri: {
                const auto& t = snapshot.triangles[di.idx];
                const float ss = t.screen_space ? 1.0f : 0.0f;
                scratch_tris_.push_back({t.p0.x, t.p0.y, ss, 0.0f, t.c0.r, t.c0.g, t.c0.b, t.c0.a});
                scratch_tris_.push_back({t.p1.x, t.p1.y, ss, 0.0f, t.c1.r, t.c1.g, t.c1.b, t.c1.a});
                scratch_tris_.push_back({t.p2.x, t.p2.y, ss, 0.0f, t.c2.r, t.c2.g, t.c2.b, t.c2.a});
                break;
            }
            default:
                break;
        }
    }

    ensure_upload_buffer(gl_, inst_vbo_rect_, scratch_rects_, cap_rect_);
    ensure_upload_buffer(gl_, inst_vbo_circle_, scratch_circles_, cap_circle_);
    ensure_upload_buffer(gl_, inst_vbo_capsule_, scratch_capsules_, cap_capsule_);
    ensure_upload_buffer(gl_, tri_vbo_, scratch_tris_, cap_tri_verts_);

    // Update only DirtyGeometry dynamic graphs / trails
    for (const auto& gd : snapshot.graphs) {
        if ((gd.dirty_flags & DirtyGeometry) != 0 && !gd.dynamic_points.empty()) {
            GraphItem tmp;
            tmp.id = gd.id;
            update_graph_gpu(tmp, &gd.dynamic_points);
        }
    }

    // Issue contiguous batched draws in sorted painter's order
    size_t off_rect = 0;
    size_t off_circle = 0;
    size_t off_capsule = 0;
    size_t off_tri = 0;

    size_t i = 0;
    while (i < scratch_draw_items_.size()) {
        const DrawKind k = scratch_draw_items_[i].kind;
        size_t j = i + 1;
        while (j < scratch_draw_items_.size() && scratch_draw_items_[j].kind == k) ++j;
        const size_t count = j - i;

        switch (k) {
            case DrawKind::Rect: {
                gl_.UseProgram(prog_rect_);
                set_camera_uniforms(gl_, u_rect_.resolution, u_rect_.cam_pos, u_rect_.cam_zoom, u_rect_.cam_rot, width_, height_, snapshot.camera);
                gl_.BindVertexArray(inst_vao_rect_);
                gl_.VertexArrayVertexBuffer(
                    inst_vao_rect_,
                    0,
                    inst_vbo_rect_,
                    static_cast<gl::GLintptr>(off_rect * sizeof(GpuRectInst)),
                    sizeof(GpuRectInst)
                );
                gl_.DrawArraysInstanced(gl::kTriangles, 0, 6, static_cast<GLsizei>(count));
                off_rect += count;
                break;
            }
            case DrawKind::Circle: {
                gl_.UseProgram(prog_circle_);
                set_camera_uniforms(gl_, u_circle_.resolution, u_circle_.cam_pos, u_circle_.cam_zoom, u_circle_.cam_rot, width_, height_, snapshot.camera);
                gl_.BindVertexArray(inst_vao_circle_);
                gl_.VertexArrayVertexBuffer(
                    inst_vao_circle_,
                    0,
                    inst_vbo_circle_,
                    static_cast<gl::GLintptr>(off_circle * sizeof(GpuCircleInst)),
                    sizeof(GpuCircleInst)
                );
                gl_.DrawArraysInstanced(gl::kTriangles, 0, 6, static_cast<GLsizei>(count));
                off_circle += count;
                break;
            }
            case DrawKind::Capsule: {
                gl_.UseProgram(prog_capsule_);
                set_camera_uniforms(gl_, u_capsule_.resolution, u_capsule_.cam_pos, u_capsule_.cam_zoom, u_capsule_.cam_rot, width_, height_, snapshot.camera);
                gl_.BindVertexArray(inst_vao_capsule_);
                gl_.VertexArrayVertexBuffer(
                    inst_vao_capsule_,
                    0,
                    inst_vbo_capsule_,
                    static_cast<gl::GLintptr>(off_capsule * sizeof(GpuCapsuleInst)),
                    sizeof(GpuCapsuleInst)
                );
                gl_.DrawArraysInstanced(gl::kTriangles, 0, 6, static_cast<GLsizei>(count));
                off_capsule += count;
                break;
            }
            case DrawKind::Tri: {
                gl_.UseProgram(prog_tri_);
                set_camera_uniforms(gl_, u_tri_.resolution, u_tri_.cam_pos, u_tri_.cam_zoom, u_tri_.cam_rot, width_, height_, snapshot.camera);
                gl_.BindVertexArray(tri_vao_);
                gl_.DrawArrays(gl::kTriangles, static_cast<GLint>(off_tri * 3u), static_cast<GLsizei>(count * 3u));
                off_tri += count;
                break;
            }
            case DrawKind::Graph: {
                gl_.UseProgram(prog_graph_);
                set_camera_uniforms(gl_, u_graph_.resolution, u_graph_.cam_pos, u_graph_.cam_zoom, u_graph_.cam_rot, width_, height_, snapshot.camera);
                gl_.BindVertexArray(graph_vao_);
                for (size_t idx = i; idx < j; ++idx) {
                    const auto& gd = snapshot.graphs[scratch_draw_items_[idx].idx];
                    const auto it = graph_cache_.find(gd.id);
                    if (it == graph_cache_.end() || it->second.vertex_count < 4) continue;
                    const float scale_f = gd.screen_space ? 1.0f : snapshot.camera.zoom;
                    const float hw_px = std::max(0.5f, gd.stroke_width * 0.5f * scale_f);
                    gl_.VertexArrayVertexBuffer(graph_vao_, 0, it->second.vbo, 0, sizeof(GpuGraphVert));
                    gl_.Uniform2f(u_graph_.origin, gd.origin.x, gd.origin.y);
                    gl_.Uniform1f(u_graph_.half_width, gd.stroke_width * 0.5f);
                    gl_.Uniform1f(u_graph_.half_width_px, hw_px);
                    gl_.Uniform1f(u_graph_.screen_space, gd.screen_space ? 1.0f : 0.0f);
                    gl_.Uniform1f(u_graph_.max_arc_len, it->second.total_arc_length * gd.reveal);
                    gl_.Uniform4f(u_graph_.color, gd.color.r, gd.color.g, gd.color.b, gd.color.a);
                    gl_.Uniform1f(u_graph_.dash_len, gd.dash_length * scale_f);
                    gl_.Uniform1f(u_graph_.gap_len, gd.gap_length * scale_f);
                    gl_.DrawArrays(gl::kTriangleStrip, 0, it->second.vertex_count);
                }
                break;
            }
            case DrawKind::OrbitalCloud: {
                gl_.ClearDepth(1.0);
                gl_.DepthMask(GL_TRUE);
                gl_.Clear(gl::kDepthBufferBit);
                gl_.Enable(gl::kDepthTest);
                gl_.DepthFunc(gl::kLess);
                gl_.Enable(gl::kSampleAlphaToCoverage);
                gl_.Disable(gl::kBlend);

                gl_.UseProgram(prog_orbital_);
                set_camera_uniforms(gl_, u_orbital_.resolution, u_orbital_.cam_pos, u_orbital_.cam_zoom, u_orbital_.cam_rot, width_, height_, snapshot.camera);
                gl_.BindVertexArray(orbital_vao_);

                for (size_t idx = i; idx < j; ++idx) {
                    const auto& oc = snapshot.orbital_clouds[scratch_draw_items_[idx].idx];
                    const auto it = orbital_cache_.find(oc.id);
                    if (it == orbital_cache_.end() || it->second.instance_count <= 0) continue;

                    gl_.VertexArrayVertexBuffer(orbital_vao_, 0, it->second.vbo, 0, sizeof(GpuOrbitalVert));
                    gl_.Uniform2f(u_orbital_.center, oc.center.x, oc.center.y);
                    gl_.Uniform1f(u_orbital_.scale, oc.scale);
                    gl_.Uniform1f(u_orbital_.morph, oc.morph);
                    gl_.Uniform1f(u_orbital_.reveal, oc.reveal);
                    gl_.Uniform1f(u_orbital_.opacity, oc.opacity);
                    gl_.Uniform1f(u_orbital_.yaw, oc.yaw_rad);
                    gl_.Uniform1f(u_orbital_.pitch, oc.pitch_rad);
                    gl_.Uniform1f(u_orbital_.fov_persp, oc.fov_persp);
                    gl_.Uniform1f(u_orbital_.flow_phase, oc.flow_phase_sec);
                    gl_.Uniform1f(u_orbital_.collapse, oc.collapse);
                    gl_.Uniform2f(u_orbital_.collapse_target, oc.collapse_target.x, oc.collapse_target.y);
                    gl_.Uniform1f(u_orbital_.dot_radius, oc.dot_radius_px);
                    gl_.Uniform1f(u_orbital_.screen_space, oc.screen_space ? 1.0f : 0.0f);
                    gl_.Uniform1f(u_orbital_.depth_scale, it->second.depth_scale);
                    gl_.Uniform4f(u_orbital_.color, oc.color.r, oc.color.g, oc.color.b, oc.color.a);
                    gl_.DrawArraysInstanced(gl::kTriangles, 0, 6, it->second.instance_count);
                }

                gl_.Disable(gl::kSampleAlphaToCoverage);
                gl_.Disable(gl::kDepthTest);
                gl_.DepthMask(GL_FALSE);
                gl_.Enable(gl::kBlend);
                break;
            }
            case DrawKind::Path: {
                gl_.UseProgram(prog_path_);
                set_camera_uniforms(gl_, u_path_.resolution, u_path_.cam_pos, u_path_.cam_zoom, u_path_.cam_rot, width_, height_, snapshot.camera);
                gl_.BindVertexArray(path_vao_);
                gl_.Enable(gl::kStencilTest);
                for (size_t idx = i; idx < j; ++idx) {
                    const auto& pd = snapshot.paths[scratch_draw_items_[idx].idx];
                    const auto it = path_cache_.find(pd.asset_id);
                    if (it == path_cache_.end() || it->second.batches.empty()) continue;

                    gl_.Uniform2f(u_path_.pos, pd.position.x, pd.position.y);
                    gl_.Uniform1f(u_path_.scale, pd.scale);
                    gl_.Uniform1f(u_path_.rot, pd.rotation_rad);
                    gl_.Uniform1f(u_path_.screen_space, pd.screen_space ? 1.0f : 0.0f);
                    gl_.Uniform4f(u_path_.tint, pd.tint.r, pd.tint.g, pd.tint.b, pd.tint.a);

                    for (const auto& b : it->second.batches) {
                        // Pass 1: Non-Zero Winding accumulation in stencil buffer (color writes disabled)
                        gl_.ColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                        gl_.StencilFunc(gl::kAlways, 0, 0xFF);
                        gl_.StencilOpSeparate(gl::kFront, gl::kKeep, gl::kKeep, gl::kIncrWrap);
                        gl_.StencilOpSeparate(gl::kBack, gl::kKeep, gl::kKeep, gl::kDecrWrap);
                        gl_.VertexArrayVertexBuffer(path_vao_, 0, it->second.vbo, 0, sizeof(PathVertex));
                        gl_.VertexArrayElementBuffer(path_vao_, it->second.ibo);
                        const void* idx_offset = reinterpret_cast<const void*>(static_cast<uintptr_t>(b.first_index) * sizeof(uint32_t));
                        gl_.DrawElements(gl::kTriangles, b.index_count, gl::kUnsignedInt, idx_offset);

                        // Pass 2: Cover quad paints per-batch color where stencil != 0 and resets stencil to 0 cleanly
                        gl_.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                        gl_.StencilFunc(gl::kNotequal, 0, 0xFF);
                        gl_.StencilOp(gl::kZero, gl::kZero, gl::kZero);
                        gl_.VertexArrayVertexBuffer(path_vao_, 0, b.cover_vbo, 0, sizeof(PathVertex));
                        gl_.VertexArrayElementBuffer(path_vao_, 0);
                        gl_.DrawArrays(gl::kTriangles, 0, 6);
                    }
                }
                gl_.Disable(gl::kStencilTest);
                break;
            }
            case DrawKind::Text: {
                gl_.UseProgram(prog_text_);
                set_camera_uniforms(gl_, u_text_.resolution, u_text_.cam_pos, u_text_.cam_zoom, u_text_.cam_rot, width_, height_, snapshot.camera);
                gl_.BindTextureUnit(0, font_cache_.atlas_texture());
                gl_.Uniform1i(u_text_.atlas, 0);
                gl_.BindVertexArray(text_vao_);
                for (size_t idx = i; idx < j; ++idx) {
                    const auto& td = snapshot.texts[scratch_draw_items_[idx].idx];
                    const auto spec_it = text_specs_.find(td.id);
                    if (spec_it == text_specs_.end()) continue;
                    auto run_res = font_cache_.prepare_text_run(spec_it->second);
                    if (!run_res || !*run_res || (*run_res)->vertex_count == 0) continue;
                    const auto* run = *run_res;
                    gl_.VertexArrayVertexBuffer(text_vao_, 0, run->vbo, 0, sizeof(text::GlyphQuadVertex));
                    gl_.Uniform2f(u_text_.pos, td.position.x, td.position.y);
                    gl_.Uniform1f(u_text_.scale, td.scale);
                    gl_.Uniform1f(u_text_.rot, td.rotation_rad);
                    gl_.Uniform1f(u_text_.screen_space, td.screen_space ? 1.0f : 0.0f);
                    gl_.Uniform4f(u_text_.color, td.color.r, td.color.g, td.color.b, td.color.a);
                    gl_.DrawArrays(gl::kTriangles, 0, run->vertex_count);
                }
                break;
            }
        }
        i = j;
    }

    // Resolve MSAA FBO into slot's single-sample CUDA-registered RGBA8 texture
    gl_.BlitNamedFramebuffer(
        msaa_fbo_,
        slot.resolve_fbo,
        0,
        0,
        width_,
        height_,
        0,
        0,
        width_,
        height_,
        gl::kColorBufferBit,
        gl::kNearest
    );

    gl_.EndQuery(gl::kTimeElapsed);
    slot.timer_pending = true;
    // Flush command stream so CUDA interop waits cleanly on GPU without blocking host CPU
    gl_.Flush();
    return {};
}

float Renderer::poll_slot_gpu_time_ms(size_t slot_index) const noexcept {
    if (slot_index >= slots_.size()) return -1.0f;
    const auto& s = slots_[slot_index];
    if (!s.timer_pending) return -1.0f;
    GLint available = 0;
    gl_.GetQueryObjectiv(s.timer_query, gl::kQueryResultAvailable, &available);
    if (!available) return -1.0f;
    gl::GLuint64 ns = 0;
    gl_.GetQueryObjectui64v(s.timer_query, gl::kQueryResult, &ns);
    return static_cast<float>(static_cast<double>(ns) * 1e-6);
}

void Renderer::blit_slot_to_default_framebuffer(size_t slot_index, int win_w, int win_h) noexcept {
    if (slot_index >= slots_.size() || win_w <= 0 || win_h <= 0) return;

    const double src_aspect = static_cast<double>(width_) / static_cast<double>(std::max(1, height_));
    const double dst_aspect = static_cast<double>(win_w) / static_cast<double>(std::max(1, win_h));
    int dx0 = 0;
    int dy0 = 0;
    int dx1 = win_w;
    int dy1 = win_h;
    if (std::abs(src_aspect - dst_aspect) > 1e-4) {
        if (dst_aspect > src_aspect) {
            const int fit_w = std::max(1, static_cast<int>(std::lround(static_cast<double>(win_h) * src_aspect)));
            dx0 = (win_w - fit_w) / 2;
            dx1 = dx0 + fit_w;
        } else {
            const int fit_h = std::max(1, static_cast<int>(std::lround(static_cast<double>(win_w) / src_aspect)));
            dy0 = (win_h - fit_h) / 2;
            dy1 = dy0 + fit_h;
        }
        gl_.BindFramebuffer(gl::kFramebuffer, 0);
        gl_.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        gl_.Clear(gl::kColorBufferBit);
    }

    gl_.BlitNamedFramebuffer(
        slots_[slot_index].resolve_fbo,
        0,
        0,
        0,
        width_,
        height_,
        dx0,
        dy0,
        dx1,
        dy1,
        gl::kColorBufferBit,
        gl::kLinear
    );
}

Result<void> Renderer::debug_readback_slot_rgba8(
    size_t slot_index,
    std::vector<uint8_t>& out_rgba
) {
    if (!initialized_ || slot_index >= slots_.size()) {
        return fail("Invalid slot for debug_readback_slot_rgba8");
    }
    gl_.Finish();
    const size_t bytes = static_cast<size_t>(width_) * static_cast<size_t>(height_) * 4u;
    out_rgba.resize(bytes);
    gl_.PixelStorei(gl::kPackAlignment, 1);
    gl_.GetTextureImage(
        slots_[slot_index].color_tex,
        0,
        gl::kRgba,
        gl::kUnsignedByte,
        static_cast<GLsizei>(bytes),
        out_rgba.data()
    );
    gl_.PixelStorei(gl::kPackAlignment, 4);
    ++debug_readback_count_;
    return {};
}

void Renderer::shutdown() noexcept {
    font_cache_.shutdown();
    if (initialized_) {
        for (auto& [id, g] : graph_cache_) {
            if (g.vbo) gl_.DeleteBuffers(1, &g.vbo);
        }
        for (auto& [id, oc] : orbital_cache_) {
            if (oc.vbo) gl_.DeleteBuffers(1, &oc.vbo);
        }
        for (auto& [id, p] : path_cache_) {
            if (p.vbo) gl_.DeleteBuffers(1, &p.vbo);
            if (p.ibo) gl_.DeleteBuffers(1, &p.ibo);
            for (auto& b : p.batches) {
                if (b.cover_vbo) gl_.DeleteBuffers(1, &b.cover_vbo);
            }
        }
        for (auto& s : slots_) {
            if (s.timer_query) gl_.DeleteQueries(1, &s.timer_query);
            if (s.resolve_fbo) gl_.DeleteFramebuffers(1, &s.resolve_fbo);
            if (s.color_tex) gl_.DeleteTextures(1, &s.color_tex);
        }
        if (msaa_fbo_) gl_.DeleteFramebuffers(1, &msaa_fbo_);
        if (msaa_color_rb_) gl_.DeleteRenderbuffers(1, &msaa_color_rb_);
        if (msaa_ds_rb_) gl_.DeleteRenderbuffers(1, &msaa_ds_rb_);

        const GLuint progs[] = {prog_bg_, prog_rect_, prog_circle_, prog_capsule_, prog_tri_, prog_graph_, prog_orbital_, prog_path_, prog_text_};
        for (GLuint p : progs) {
            if (p) gl_.DeleteProgram(p);
        }
        const GLuint vaos[] = {empty_vao_, inst_vao_rect_, inst_vao_circle_, inst_vao_capsule_, tri_vao_, graph_vao_, orbital_vao_, path_vao_, text_vao_};
        for (GLuint v : vaos) {
            if (v) gl_.DeleteVertexArrays(1, &v);
        }
        const GLuint bufs[] = {inst_vbo_rect_, inst_vbo_circle_, inst_vbo_capsule_, tri_vbo_};
        for (GLuint b : bufs) {
            if (b) gl_.DeleteBuffers(1, &b);
        }
    }
    graph_cache_.clear();
    orbital_cache_.clear();
    path_cache_.clear();
    text_specs_.clear();
    slots_.clear();
    msaa_fbo_ = msaa_color_rb_ = msaa_ds_rb_ = 0;
    prog_bg_ = prog_rect_ = prog_circle_ = prog_capsule_ = prog_tri_ = prog_graph_ = prog_orbital_ = prog_path_ = prog_text_ = 0;
    empty_vao_ = inst_vao_rect_ = inst_vao_circle_ = inst_vao_capsule_ = tri_vao_ = graph_vao_ = orbital_vao_ = path_vao_ = text_vao_ = 0;
    inst_vbo_rect_ = inst_vbo_circle_ = inst_vbo_capsule_ = tri_vbo_ = 0;
    cap_rect_ = cap_circle_ = cap_capsule_ = cap_tri_verts_ = 0;
    initialized_ = false;
}

}  // namespace stem::render
