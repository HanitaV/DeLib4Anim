#include "native/text/font_atlas.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>

namespace stem::text {

struct FontAtlasCache::FontFace {
    uint32_t id = 0;
    std::vector<uint8_t> ttf_bytes;
    stbtt_fontinfo info{};
};

namespace {

std::vector<uint32_t> decode_utf8(const std::string& s) {
    std::vector<uint32_t> out;
    size_t i = 0;
    while (i < s.size()) {
        const uint8_t c = static_cast<uint8_t>(s[i]);
        uint32_t cp = 0;
        size_t len = 1;
        if ((c & 0x80u) == 0) {
            cp = c;
        } else if ((c & 0xE0u) == 0xC0u && i + 1 < s.size()) {
            cp = ((c & 0x1Fu) << 6) | (static_cast<uint8_t>(s[i + 1]) & 0x3Fu);
            len = 2;
        } else if ((c & 0xF0u) == 0xE0u && i + 2 < s.size()) {
            cp = ((c & 0x0Fu) << 12) |
                 ((static_cast<uint8_t>(s[i + 1]) & 0x3Fu) << 6) |
                 (static_cast<uint8_t>(s[i + 2]) & 0x3Fu);
            len = 3;
        } else if ((c & 0xF8u) == 0xF0u && i + 3 < s.size()) {
            cp = ((c & 0x07u) << 18) |
                 ((static_cast<uint8_t>(s[i + 1]) & 0x3Fu) << 12) |
                 ((static_cast<uint8_t>(s[i + 2]) & 0x3Fu) << 6) |
                 (static_cast<uint8_t>(s[i + 3]) & 0x3Fu);
            len = 4;
        } else {
            cp = '?';
        }
        out.push_back(cp);
        i += len;
    }
    return out;
}

}  // namespace

std::string FontAtlasCache::resolve_default_font_path() {
    return resolve_font_variant("regular");
}

std::string FontAtlasCache::resolve_font_variant(std::string_view variant) {
#if defined(_WIN32)
    static constexpr const char* kRegular[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/consola.ttf",
    };
    static constexpr const char* kSemibold[] = {
        "C:/Windows/Fonts/seguisb.ttf",
        "C:/Windows/Fonts/segoeuib.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
    };
    static constexpr const char* kBold[] = {
        "C:/Windows/Fonts/segoeuib.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
        "C:/Windows/Fonts/calibrib.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
    };
    static constexpr const char* kItalic[] = {
        "C:/Windows/Fonts/segoeuii.ttf",
        "C:/Windows/Fonts/ariali.ttf",
        "C:/Windows/Fonts/calibrii.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
    };
    static constexpr const char* kSemiboldItalic[] = {
        "C:/Windows/Fonts/seguisbi.ttf",
        "C:/Windows/Fonts/segoeuiz.ttf",
        "C:/Windows/Fonts/segoeuii.ttf",
        "C:/Windows/Fonts/arialbi.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
    };
    static constexpr const char* kBoldItalic[] = {
        "C:/Windows/Fonts/segoeuiz.ttf",
        "C:/Windows/Fonts/arialbi.ttf",
        "C:/Windows/Fonts/calibriz.ttf",
        "C:/Windows/Fonts/segoeuib.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
    };
#else
    static constexpr const char* kRegular[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    };
    static constexpr const char* kSemibold[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    };
    static constexpr const char* kBold[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    };
    static constexpr const char* kItalic[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Oblique.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Italic.ttf",
    };
    static constexpr const char* kSemiboldItalic[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-BoldOblique.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-BoldItalic.ttf",
    };
    static constexpr const char* kBoldItalic[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-BoldOblique.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-BoldItalic.ttf",
    };
#endif

    auto pick = [](const char* const* list, size_t n) -> std::string {
        for (size_t i = 0; i < n; ++i) {
            if (std::filesystem::exists(list[i])) {
                return list[i];
            }
        }
        return {};
    };

    if (variant == "bold_italic") return pick(kBoldItalic, std::size(kBoldItalic));
    if (variant == "semibold_italic") return pick(kSemiboldItalic, std::size(kSemiboldItalic));
    if (variant == "italic") return pick(kItalic, std::size(kItalic));
    if (variant == "bold") return pick(kBold, std::size(kBold));
    if (variant == "semibold") return pick(kSemibold, std::size(kSemibold));
    return pick(kRegular, std::size(kRegular));
}

FontAtlasCache::FontAtlasCache() = default;

FontAtlasCache::~FontAtlasCache() {
    shutdown();
}

Result<void> FontAtlasCache::initialize(const gl::GlDispatch& gl) {
    shutdown();
    gl_ = gl;

    gl_.CreateTextures(gl::kTexture2D, 1, &atlas_tex_);
    gl_.TextureStorage2D(atlas_tex_, 1, gl::kR8, kAtlasSize, kAtlasSize);
    gl_.TextureParameteri(atlas_tex_, gl::kTextureMinFilter, gl::kLinear);
    gl_.TextureParameteri(atlas_tex_, gl::kTextureMagFilter, gl::kLinear);
    gl_.TextureParameteri(atlas_tex_, gl::kTextureWrapS, gl::kClampToEdge);
    gl_.TextureParameteri(atlas_tex_, gl::kTextureWrapT, gl::kClampToEdge);

    std::vector<uint8_t> zeros(static_cast<size_t>(kAtlasSize) * kAtlasSize, 0u);
    gl_.PixelStorei(gl::kUnpackAlignment, 1);
    gl_.TextureSubImage2D(
        atlas_tex_,
        0,
        0,
        0,
        kAtlasSize,
        kAtlasSize,
        gl::kRed,
        gl::kUnsignedByte,
        zeros.data()
    );
    gl_.PixelStorei(gl::kUnpackAlignment, 4);

    initialized_ = true;
    return {};
}

void FontAtlasCache::shutdown() noexcept {
    if (initialized_) {
        for (auto& [id, run] : runs_) {
            if (run.vbo != 0) {
                gl_.DeleteBuffers(1, &run.vbo);
                run.vbo = 0;
            }
        }
        if (atlas_tex_ != 0) {
            gl_.DeleteTextures(1, &atlas_tex_);
            atlas_tex_ = 0;
        }
    }
    runs_.clear();
    glyph_map_.clear();
    fonts_.clear();
    cursor_x_ = 2;
    cursor_y_ = 2;
    row_height_ = 0;
    initialized_ = false;
}

Result<FontAtlasCache::FontFace*> FontAtlasCache::get_or_load_font(const std::string& font_path) {
    const std::string resolved = font_path.empty() ? resolve_default_font_path() : font_path;
    if (resolved.empty()) {
        return fail("No TrueType system font found and no font_path provided");
    }
    if (auto it = fonts_.find(resolved); it != fonts_.end()) {
        return it->second.get();
    }

    std::ifstream ifs(resolved, std::ios::binary);
    if (!ifs) {
        return fail(std::format("Failed to open font file: {}", resolved));
    }
    auto face = std::make_unique<FontFace>();
    face->id = next_font_id_++;
    face->ttf_bytes.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (face->ttf_bytes.empty() ||
        !stbtt_InitFont(&face->info, face->ttf_bytes.data(), stbtt_GetFontOffsetForIndex(face->ttf_bytes.data(), 0))) {
        return fail(std::format("stbtt_InitFont failed for: {}", resolved));
    }

    FontFace* ptr = face.get();
    fonts_[resolved] = std::move(face);
    return ptr;
}

Result<FontAtlasCache::BakedGlyph> FontAtlasCache::get_or_bake_glyph(
    FontFace& face,
    uint32_t codepoint,
    float size_px
) {
    const float clamped_px = std::clamp(size_px, 8.0f, 160.0f);
    const uint16_t size_q = static_cast<uint16_t>(std::lround(clamped_px * 2.0f));
    const GlyphKey key{face.id, codepoint, size_q};
    if (auto it = glyph_map_.find(key); it != glyph_map_.end()) {
        return it->second;
    }

    constexpr float kOversample = 2.0f;
    const float scale = stbtt_ScaleForPixelHeight(&face.info, clamped_px);
    const float bake_scale = stbtt_ScaleForPixelHeight(&face.info, clamped_px * kOversample);
    int glyph_idx = stbtt_FindGlyphIndex(&face.info, static_cast<int>(codepoint));
    if (glyph_idx == 0 && codepoint != ' ') {
        glyph_idx = stbtt_FindGlyphIndex(&face.info, '?');
    }

    int adv_w = 0;
    int lsb = 0;
    stbtt_GetGlyphHMetrics(&face.info, glyph_idx, &adv_w, &lsb);

    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    stbtt_GetGlyphBitmapBox(&face.info, glyph_idx, bake_scale, bake_scale, &x0, &y0, &x1, &y1);
    const int gw = std::max(0, x1 - x0);
    const int gh = std::max(0, y1 - y0);

    constexpr float inv_os = 1.0f / kOversample;
    BakedGlyph bg{};
    bg.advance_x = static_cast<float>(adv_w) * scale;
    // stb_truetype y0/y1 are y-down from baseline; convert to engine y-up coordinates
    // while mapping 2x supersampled bitmap box back into logical font pixels.
    bg.x0 = static_cast<float>(x0) * inv_os;
    bg.x1 = static_cast<float>(x1) * inv_os;
    bg.y1 = static_cast<float>(-y0) * inv_os;  // top (higher y)
    bg.y0 = static_cast<float>(-y1) * inv_os;  // bottom (lower y)

    if (gw > 0 && gh > 0) {
        constexpr int kPad = 2;
        if (cursor_x_ + gw + kPad >= kAtlasSize) {
            cursor_x_ = 2;
            cursor_y_ += row_height_ + kPad;
            row_height_ = 0;
        }
        if (cursor_y_ + gh + kPad >= kAtlasSize) {
            return fail("Font glyph atlas (4096x4096) exhausted");
        }

        if (initialized_) {
            std::vector<uint8_t> bmp(static_cast<size_t>(gw) * static_cast<size_t>(gh), 0u);
            stbtt_MakeGlyphBitmap(&face.info, bmp.data(), gw, gh, gw, bake_scale, bake_scale, glyph_idx);
            gl_.PixelStorei(gl::kUnpackAlignment, 1);
            gl_.TextureSubImage2D(
                atlas_tex_,
                0,
                cursor_x_,
                cursor_y_,
                gw,
                gh,
                gl::kRed,
                gl::kUnsignedByte,
                bmp.data()
            );
            gl_.PixelStorei(gl::kUnpackAlignment, 4);
        }

        const float inv = 1.0f / static_cast<float>(kAtlasSize);
        bg.u0 = static_cast<float>(cursor_x_) * inv;
        bg.v0 = static_cast<float>(cursor_y_) * inv;
        bg.u1 = static_cast<float>(cursor_x_ + gw) * inv;
        bg.v1 = static_cast<float>(cursor_y_ + gh) * inv;

        cursor_x_ += gw + kPad;
        row_height_ = std::max(row_height_, gh);
    }

    glyph_map_[key] = bg;
    return bg;
}

Result<TextBounds> FontAtlasCache::measure_text(
    const std::string& text,
    const std::string& font_path,
    float font_size_px
) {
    auto face_res = get_or_load_font(font_path);
    if (!face_res) return fail(face_res.error());
    FontFace* face = *face_res;

    const float scale = stbtt_ScaleForPixelHeight(&face->info, std::clamp(font_size_px, 8.0f, 160.0f));
    int ascent = 0;
    int descent = 0;
    int line_gap = 0;
    stbtt_GetFontVMetrics(&face->info, &ascent, &descent, &line_gap);
    const float line_h = static_cast<float>(ascent - descent + line_gap) * scale;

    const auto cps = decode_utf8(text);
    float max_w = 0.0f;
    float cur_x = 0.0f;
    int lines = 1;
    for (size_t i = 0; i < cps.size(); ++i) {
        if (cps[i] == '\n') {
            max_w = std::max(max_w, cur_x);
            cur_x = 0.0f;
            ++lines;
            continue;
        }
        auto g = get_or_bake_glyph(*face, cps[i], font_size_px);
        if (!g) return fail(g.error());
        cur_x += g->advance_x;
        if (i + 1 < cps.size() && cps[i + 1] != '\n') {
            const int k = stbtt_GetCodepointKernAdvance(
                &face->info,
                static_cast<int>(cps[i]),
                static_cast<int>(cps[i + 1])
            );
            cur_x += static_cast<float>(k) * scale;
        }
    }
    max_w = std::max(max_w, cur_x);
    return TextBounds{
        .width = max_w,
        .height = static_cast<float>(lines) * line_h,
        .ascent = static_cast<float>(ascent) * scale,
        .descent = static_cast<float>(descent) * scale,
    };
}

Result<const CachedTextRun*> FontAtlasCache::prepare_text_run(const TextItem& item) {
    if (!initialized_) {
        return fail("FontAtlasCache not initialized");
    }

    auto& run = runs_[item.id];
    const bool up_to_date =
        run.vbo != 0 &&
        run.cached_text == item.text &&
        run.cached_font == item.font_path &&
        std::abs(run.cached_size_px - item.font_size_px) < 1e-3f &&
        run.cached_align_h == item.align_h;
    if (up_to_date) {
        return &run;
    }

    auto face_res = get_or_load_font(item.font_path);
    if (!face_res) return fail(face_res.error());
    FontFace* face = *face_res;

    const float clamped_px = std::clamp(item.font_size_px, 8.0f, 160.0f);
    const float scale = stbtt_ScaleForPixelHeight(&face->info, clamped_px);
    int ascent = 0;
    int descent = 0;
    int line_gap = 0;
    stbtt_GetFontVMetrics(&face->info, &ascent, &descent, &line_gap);
    const float line_h = static_cast<float>(ascent - descent + line_gap) * scale;

    const auto cps = decode_utf8(item.text);

    // Split into lines so horizontal alignment applies per line accurately
    std::vector<std::vector<uint32_t>> lines;
    lines.emplace_back();
    for (uint32_t cp : cps) {
        if (cp == '\n') {
            lines.emplace_back();
        } else {
            lines.back().push_back(cp);
        }
    }

    std::vector<float> line_widths(lines.size(), 0.0f);
    float max_line_w = 0.0f;
    for (size_t li = 0; li < lines.size(); ++li) {
        float lw = 0.0f;
        const auto& line = lines[li];
        for (size_t i = 0; i < line.size(); ++i) {
            auto g = get_or_bake_glyph(*face, line[i], clamped_px);
            if (!g) return fail(g.error());
            lw += g->advance_x;
            if (i + 1 < line.size()) {
                lw += static_cast<float>(stbtt_GetCodepointKernAdvance(
                          &face->info,
                          static_cast<int>(line[i]),
                          static_cast<int>(line[i + 1])
                      )) *
                      scale;
            }
        }
        line_widths[li] = lw;
        max_line_w = std::max(max_line_w, lw);
    }

    const float total_h = static_cast<float>(lines.size()) * line_h;
    const float top_baseline_y =
        (total_h * 0.5f) - static_cast<float>(ascent) * scale;

    std::vector<GlyphQuadVertex> verts;
    verts.reserve(cps.size() * 6u);

    for (size_t li = 0; li < lines.size(); ++li) {
        float pen_x = 0.0f;
        if (item.align_h == 0) {
            pen_x = -line_widths[li] * 0.5f;  // Centered horizontally around anchor
        } else if (item.align_h > 0) {
            pen_x = -line_widths[li];         // Right-aligned
        }
        const float baseline_y = top_baseline_y - static_cast<float>(li) * line_h;
        const auto& line = lines[li];
        for (size_t i = 0; i < line.size(); ++i) {
            auto g = get_or_bake_glyph(*face, line[i], clamped_px);
            if (!g) return fail(g.error());

            if (g->x1 > g->x0 && g->y1 > g->y0) {
                const float x0 = pen_x + g->x0;
                const float x1 = pen_x + g->x1;
                const float y0 = baseline_y + g->y0;
                const float y1 = baseline_y + g->y1;

                // Texture top (v0) maps to glyph top (y1); bottom (v1) maps to y0
                verts.push_back({x0, y0, g->u0, g->v1});
                verts.push_back({x1, y0, g->u1, g->v1});
                verts.push_back({x1, y1, g->u1, g->v0});

                verts.push_back({x0, y0, g->u0, g->v1});
                verts.push_back({x1, y1, g->u1, g->v0});
                verts.push_back({x0, y1, g->u0, g->v0});
            }

            pen_x += g->advance_x;
            if (i + 1 < line.size()) {
                pen_x += static_cast<float>(stbtt_GetCodepointKernAdvance(
                             &face->info,
                             static_cast<int>(line[i]),
                             static_cast<int>(line[i + 1])
                         )) *
                         scale;
            }
        }
    }

    if (run.vbo == 0) {
        gl_.CreateBuffers(1, &run.vbo);
    }
    if (!verts.empty()) {
        gl_.NamedBufferData(
            run.vbo,
            static_cast<gl::GLsizeiptr>(verts.size() * sizeof(GlyphQuadVertex)),
            verts.data(),
            gl::kStaticDraw
        );
    }
    run.vertex_count = static_cast<GLsizei>(verts.size());
    run.cached_text = item.text;
    run.cached_font = item.font_path;
    run.cached_size_px = item.font_size_px;
    run.cached_align_h = item.align_h;
    run.bounds = TextBounds{
        .width = max_line_w,
        .height = total_h,
        .ascent = static_cast<float>(ascent) * scale,
        .descent = static_cast<float>(descent) * scale,
    };
    return &run;
}

}  // namespace stem::text
