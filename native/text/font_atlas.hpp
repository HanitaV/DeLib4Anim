#pragma once

#include "native/core/scene.hpp"
#include "native/render/gl_loader.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace stem::text {

struct TextBounds {
    float width = 0.0f;
    float height = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
};

struct GlyphQuadVertex {
    float x = 0.0f;
    float y = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
};

struct CachedTextRun {
    GLuint vbo = 0;
    GLsizei vertex_count = 0;
    std::string cached_text;
    std::string cached_font;
    float cached_size_px = 0.0f;
    int cached_align_h = 0;
    TextBounds bounds{};
};

class FontAtlasCache {
public:
    FontAtlasCache();
    ~FontAtlasCache();

    FontAtlasCache(const FontAtlasCache&) = delete;
    FontAtlasCache& operator=(const FontAtlasCache&) = delete;

    [[nodiscard]] Result<void> initialize(const gl::GlDispatch& gl);
    void shutdown() noexcept;

    // Shapes/caches a TextItem into GPU VBO once; re-uses retained VBO when string/font/size unchanged.
    [[nodiscard]] Result<const CachedTextRun*> prepare_text_run(const TextItem& item);

    // CPU-side bounding box measurement (used for storyboard layout & overlap warnings).
    [[nodiscard]] Result<TextBounds> measure_text(
        const std::string& text,
        const std::string& font_path,
        float font_size_px
    );

    [[nodiscard]] GLuint atlas_texture() const noexcept { return atlas_tex_; }
    [[nodiscard]] size_t baked_glyph_count() const noexcept { return glyph_map_.size(); }

    static std::string resolve_default_font_path();
    static std::string resolve_font_variant(std::string_view variant);

private:
    struct FontFace;
    struct BakedGlyph {
        float x0 = 0.0f;
        float y0 = 0.0f;
        float x1 = 0.0f;
        float y1 = 0.0f;
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 0.0f;
        float v1 = 0.0f;
        float advance_x = 0.0f;
    };

    struct GlyphKey {
        uint32_t font_id = 0;
        uint32_t codepoint = 0;
        uint16_t size_q = 0;
        bool operator==(const GlyphKey&) const = default;
    };
    struct GlyphKeyHash {
        size_t operator()(const GlyphKey& k) const noexcept {
            return (static_cast<size_t>(k.font_id) << 40) ^
                   (static_cast<size_t>(k.size_q) << 21) ^
                   static_cast<size_t>(k.codepoint);
        }
    };

    Result<FontFace*> get_or_load_font(const std::string& font_path);
    Result<BakedGlyph> get_or_bake_glyph(FontFace& face, uint32_t codepoint, float size_px);

    gl::GlDispatch gl_{};
    bool initialized_ = false;
    GLuint atlas_tex_ = 0;
    static constexpr int kAtlasSize = 4096;
    int cursor_x_ = 2;
    int cursor_y_ = 2;
    int row_height_ = 0;

    std::unordered_map<std::string, std::unique_ptr<FontFace>> fonts_;
    uint32_t next_font_id_ = 1;
    std::unordered_map<GlyphKey, BakedGlyph, GlyphKeyHash> glyph_map_;
    std::unordered_map<ObjectId, CachedTextRun> runs_;
};

}  // namespace stem::text
