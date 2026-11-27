#pragma once

#include "native/core/scene.hpp"

#include <string>

namespace stem::text {

// Parses an SVG document (including MathJax SVG output with <defs>, <g transform=...>, <use>, <path>,
// <rect>, <line>, <circle>, <polygon>) into a GPU-retained PathAsset centered at (0,0) and scaled to
// `target_height_px` (with y+ pointing upward in engine coordinates).
// Filled paths compile to triangle fans + cover bounds for exact Non-Zero Winding Stencil-then-Cover.
[[nodiscard]] Result<PathAsset> parse_svg_to_path_asset(
    const std::string& svg_xml,
    const std::string& cache_key,
    float target_height_px = 46.0f,
    ColorRGBA default_color = ColorRGBA{0.96f, 0.97f, 0.99f, 1.0f}
);

}  // namespace stem::text
