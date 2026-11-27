#pragma once

#include "native/core/scene.hpp"
#include "native/render/gl_loader.hpp"
#include "native/text/font_atlas.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace stem::render {

struct RenderTargetSlot {
    GLuint resolve_fbo = 0;
    GLuint color_tex = 0;
    GLuint timer_query = 0;
    bool timer_pending = false;
};

struct RetainedGraphGpu {
    GLuint vbo = 0;
    GLsizei vertex_count = 0;
    float total_arc_length = 0.0f;
    bool is_static = true;
    size_t capacity_vertices = 0;
};

struct PathColorBatch {
    GLsizei first_index = 0;
    GLsizei index_count = 0;
    GLuint cover_vbo = 0;
};

struct RetainedPathGpu {
    GLuint vbo = 0;
    GLuint ibo = 0;
    std::vector<PathColorBatch> batches;
};

struct RetainedOrbitalGpu {
    GLuint vbo = 0;
    GLsizei instance_count = 0;
    float depth_scale = 120.0f;
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    [[nodiscard]] Result<void> initialize(
        const gl::GlDispatch& gl,
        int width,
        int height,
        int ring_size = 3,
        int msaa_samples = 4
    );

    // Pre-uploads static PathAssets, static GraphItems, and pre-bakes TextItem glyph runs.
    [[nodiscard]] Result<void> prepare_scene_assets(const Scene& scene);

    // Renders `snapshot` into offscreen MSAA FBO and resolves into `slot(slot_index).color_tex`.
    [[nodiscard]] Result<void> render(const SceneSnapshot& snapshot, size_t slot_index);

    // Non-blocking query of completed GPU time for `slot_index` from previous cycle; returns -1.0f if unavailable.
    [[nodiscard]] float poll_slot_gpu_time_ms(size_t slot_index) const noexcept;

    // Blits a resolved slot color texture to the default window framebuffer (0) for interactive preview.
    void blit_slot_to_default_framebuffer(size_t slot_index, int win_w, int win_h) noexcept;

    // Debug/verification-only CPU readback of a resolved slot (RGBA8). Never called in production export.
    [[nodiscard]] Result<void> debug_readback_slot_rgba8(
        size_t slot_index,
        std::vector<uint8_t>& out_rgba
    );

    [[nodiscard]] size_t ring_size() const noexcept { return slots_.size(); }
    [[nodiscard]] const RenderTargetSlot& slot(size_t i) const noexcept { return slots_[i]; }
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] uint64_t debug_readback_count() const noexcept { return debug_readback_count_; }
    [[nodiscard]] double last_asset_compile_ms() const noexcept { return last_asset_compile_ms_; }
    [[nodiscard]] text::FontAtlasCache& font_cache() noexcept { return font_cache_; }

    void shutdown() noexcept;

private:
    Result<void> compile_programs();
    void update_graph_gpu(const GraphItem& item, const std::vector<Vec2>* override_pts);

    gl::GlDispatch gl_{};
    bool initialized_ = false;
    int width_ = 0;
    int height_ = 0;
    int msaa_samples_ = 4;

    GLuint msaa_fbo_ = 0;
    GLuint msaa_color_rb_ = 0;
    GLuint msaa_ds_rb_ = 0;
    std::vector<RenderTargetSlot> slots_;

    // Shader programs & cached uniform locations (avoid glGetUniformLocation string lookups in hot loop)
    GLuint prog_bg_ = 0;
    struct BgUniforms {
        GLint resolution = -1, cam_pos = -1, cam_zoom = -1, cam_rot = -1;
        GLint top_color = -1, bottom_color = -1, grid_color = -1, grid_spacing = -1, show_grid = -1;
        GLint starfield_intensity = -1, vignette_strength = -1, grain_amount = -1, perspective_grid = -1;
    } u_bg_{};

    struct CamUniforms {
        GLint resolution = -1, cam_pos = -1, cam_zoom = -1, cam_rot = -1;
    };

    GLuint prog_rect_ = 0;
    CamUniforms u_rect_{};
    GLuint prog_circle_ = 0;
    CamUniforms u_circle_{};
    GLuint prog_capsule_ = 0;
    CamUniforms u_capsule_{};
    GLuint prog_tri_ = 0;
    CamUniforms u_tri_{};

    GLuint prog_graph_ = 0;
    struct GraphUniforms : CamUniforms {
        GLint origin = -1, half_width = -1, half_width_px = -1, screen_space = -1, max_arc_len = -1, color = -1;
        GLint dash_len = -1, gap_len = -1;
    } u_graph_{};

    GLuint prog_path_ = 0;
    struct PathUniforms : CamUniforms {
        GLint pos = -1, scale = -1, rot = -1, screen_space = -1, tint = -1;
    } u_path_{};

    GLuint prog_orbital_ = 0;
    struct OrbitalUniforms : CamUniforms {
        GLint center = -1, scale = -1, morph = -1, reveal = -1, opacity = -1;
        GLint yaw = -1, pitch = -1, fov_persp = -1, flow_phase = -1;
        GLint collapse = -1, collapse_target = -1, dot_radius = -1;
        GLint screen_space = -1, depth_scale = -1, color = -1;
    } u_orbital_{};

    GLuint prog_text_ = 0;
    struct TextUniforms : CamUniforms {
        GLint atlas = -1, pos = -1, scale = -1, rot = -1, screen_space = -1, color = -1;
    } u_text_{};

    // VAOs & streaming instance VBOs (reused across frames without realloc unless growing)
    GLuint empty_vao_ = 0;
    GLuint inst_vao_rect_ = 0;
    GLuint inst_vbo_rect_ = 0;
    size_t cap_rect_ = 0;

    GLuint inst_vao_circle_ = 0;
    GLuint inst_vbo_circle_ = 0;
    size_t cap_circle_ = 0;

    GLuint inst_vao_capsule_ = 0;
    GLuint inst_vbo_capsule_ = 0;
    size_t cap_capsule_ = 0;

    GLuint tri_vao_ = 0;
    GLuint tri_vbo_ = 0;
    size_t cap_tri_verts_ = 0;

    GLuint graph_vao_ = 0;
    GLuint orbital_vao_ = 0;
    GLuint path_vao_ = 0;
    GLuint text_vao_ = 0;

    // Retained GPU asset caches
    std::unordered_map<ObjectId, RetainedGraphGpu> graph_cache_;
    std::unordered_map<ObjectId, RetainedOrbitalGpu> orbital_cache_;
    std::unordered_map<ObjectId, RetainedPathGpu> path_cache_;
    std::unordered_map<ObjectId, TextItem> text_specs_;
    text::FontAtlasCache font_cache_;

    // Reusable per-frame staging vectors (avoid heap churn in hot render loop)
    struct GpuRectInst {
        float cx, cy, sx, sy;
        float rot, corner_r, stroke_w, screen_space;
        float fr, fg, fb, fa;
        float sr, sg, sb, sa;
        float mat_mode, elevation, pad1, pad2;
    };
    struct GpuCircleInst {
        float cx, cy, radius, stroke_w;
        float screen_space, mat_mode, elevation, pad2;
        float fr, fg, fb, fa;
        float sr, sg, sb, sa;
    };
    struct GpuCapsuleInst {
        float ax, ay, bx, by;
        float radius, screen_space, pad0, pad1;
        float r, g, b, a;
    };
    struct GpuTriVert {
        float x, y, screen_space, pad;
        float r, g, b, a;
    };
    struct GpuGraphVert {
        float x, y, nx, ny;
        float arc_len, side;
    };
    struct GpuOrbitalVert {
        float r_perp_a, y_a, phi0_a, omega_a;
        float r_perp_b, y_b, phi0_b, omega_b;
        float size_scale, seed01;
    };

    enum class DrawKind : uint8_t { Rect, Circle, Capsule, Tri, Graph, OrbitalCloud, Path, Text };
    struct DrawItem {
        int z;
        DrawKind kind;
        uint32_t idx;
    };

    std::vector<DrawItem> scratch_draw_items_;
    std::vector<GpuRectInst> scratch_rects_;
    std::vector<GpuCircleInst> scratch_circles_;
    std::vector<GpuCapsuleInst> scratch_capsules_;
    std::vector<GpuTriVert> scratch_tris_;
    std::vector<GpuGraphVert> scratch_graph_verts_;

    double last_asset_compile_ms_ = 0.0;
    uint64_t debug_readback_count_ = 0;
};

}  // namespace stem::render
