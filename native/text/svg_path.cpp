#include "native/text/svg_path.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <format>
#include <numbers>
#include <unordered_map>
#include <vector>

namespace stem::text {

namespace {

constexpr float kPiF = std::numbers::pi_v<float>;

struct Mat2D {
    // [ a c tx ]
    // [ b d ty ]
    float a = 1.0f, b = 0.0f, c = 0.0f, d = 1.0f, tx = 0.0f, ty = 0.0f;

    [[nodiscard]] Vec2 apply(Vec2 p) const noexcept {
        return Vec2{a * p.x + c * p.y + tx, b * p.x + d * p.y + ty};
    }

    [[nodiscard]] Mat2D mul(const Mat2D& rhs) const noexcept {
        return Mat2D{
            .a = a * rhs.a + c * rhs.b,
            .b = b * rhs.a + d * rhs.b,
            .c = a * rhs.c + c * rhs.d,
            .d = b * rhs.c + d * rhs.d,
            .tx = a * rhs.tx + c * rhs.ty + tx,
            .ty = b * rhs.tx + d * rhs.ty + ty,
        };
    }
};

std::string trim(std::string_view sv) {
    size_t s = 0;
    while (s < sv.size() && std::isspace(static_cast<unsigned char>(sv[s]))) ++s;
    size_t e = sv.size();
    while (e > s && std::isspace(static_cast<unsigned char>(sv[e - 1]))) --e;
    return std::string(sv.substr(s, e - s));
}

bool parse_hex_color(const std::string& val, ColorRGBA& out) {
    if (val.empty() || val == "none" || val == "transparent") return false;
    if (val == "currentColor" || val == "white") {
        out = {1.0f, 1.0f, 1.0f, 1.0f};
        return true;
    }
    if (val == "black") {
        out = {0.0f, 0.0f, 0.0f, 1.0f};
        return true;
    }
    if (val[0] == '#') {
        auto hex2 = [](char hi, char lo) -> int {
            auto hv = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
                if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
                return 0;
            };
            return hv(hi) * 16 + hv(lo);
        };
        if (val.size() == 7) {
            out = {
                hex2(val[1], val[2]) / 255.0f,
                hex2(val[3], val[4]) / 255.0f,
                hex2(val[5], val[6]) / 255.0f,
                1.0f,
            };
            return true;
        }
        if (val.size() == 4) {
            out = {
                hex2(val[1], val[1]) / 255.0f,
                hex2(val[2], val[2]) / 255.0f,
                hex2(val[3], val[3]) / 255.0f,
                1.0f,
            };
            return true;
        }
    }
    if (val.rfind("rgb(", 0) == 0) {
        const size_t close = val.find(')');
        if (close != std::string::npos) {
            float r = 0.0f, g = 0.0f, b = 0.0f;
            int idx = 0;
            size_t p = 4;
            while (p < close && idx < 3) {
                while (p < close && (std::isspace(static_cast<unsigned char>(val[p])) || val[p] == ',')) ++p;
                if (p >= close) break;
                char* end = nullptr;
                const float v = std::strtof(val.data() + p, &end);
                if (end == val.data() + p) break;
                const float norm = (end < val.data() + close && *end == '%') ? (v * 0.01f) : (v / 255.0f);
                if (idx == 0) r = norm;
                else if (idx == 1) g = norm;
                else b = norm;
                ++idx;
                p = static_cast<size_t>(end - val.data());
                if (p < close && val[p] == '%') ++p;
            }
            if (idx == 3) {
                out = {std::clamp(r, 0.0f, 1.0f), std::clamp(g, 0.0f, 1.0f), std::clamp(b, 0.0f, 1.0f), 1.0f};
                return true;
            }
        }
    }
    return false;
}

std::vector<float> parse_number_list(std::string_view sv) {
    std::vector<float> nums;
    size_t i = 0;
    while (i < sv.size()) {
        while (i < sv.size() && (std::isspace(static_cast<unsigned char>(sv[i])) || sv[i] == ',')) ++i;
        if (i >= sv.size()) break;
        const char* start = sv.data() + i;
        char* end = nullptr;
        const float val = std::strtof(start, &end);
        if (end == start) {
            ++i;
            continue;
        }
        nums.push_back(val);
        i = static_cast<size_t>(end - sv.data());
    }
    return nums;
}

Mat2D parse_transform(const std::string& attr) {
    Mat2D m{};
    size_t pos = 0;
    while (pos < attr.size()) {
        while (pos < attr.size() && (std::isspace(static_cast<unsigned char>(attr[pos])) || attr[pos] == ',')) ++pos;
        const size_t open = attr.find('(', pos);
        if (open == std::string::npos) break;
        const size_t close = attr.find(')', open + 1);
        if (close == std::string::npos) break;
        const std::string op = trim(std::string_view(attr).substr(pos, open - pos));
        const auto args = parse_number_list(std::string_view(attr).substr(open + 1, close - open - 1));
        Mat2D local{};
        if (op == "translate" && !args.empty()) {
            local.tx = args[0];
            local.ty = args.size() > 1 ? args[1] : 0.0f;
        } else if (op == "scale" && !args.empty()) {
            local.a = args[0];
            local.d = args.size() > 1 ? args[1] : args[0];
        } else if (op == "matrix" && args.size() >= 6) {
            local.a = args[0];
            local.b = args[1];
            local.c = args[2];
            local.d = args[3];
            local.tx = args[4];
            local.ty = args[5];
        } else if (op == "rotate" && !args.empty()) {
            const float rad = args[0] * (kPiF / 180.0f);
            const float cs = std::cos(rad);
            const float sn = std::sin(rad);
            if (args.size() >= 3) {
                const float cx = args[1];
                const float cy = args[2];
                Mat2D t1{1, 0, 0, 1, cx, cy};
                Mat2D r{cs, sn, -sn, cs, 0, 0};
                Mat2D t2{1, 0, 0, 1, -cx, -cy};
                local = t1.mul(r).mul(t2);
            } else {
                local = Mat2D{cs, sn, -sn, cs, 0, 0};
            }
        }
        m = m.mul(local);
        pos = close + 1;
    }
    return m;
}

void flatten_quad_bezier(Vec2 p0, Vec2 p1, Vec2 p2, float tol_sq, std::vector<Vec2>& out, int depth = 0) {
    const Vec2 mid = (p0 + p2) * 0.5f;
    const Vec2 d = p1 - mid;
    if (depth >= 9 || (d.x * d.x + d.y * d.y) <= tol_sq) {
        out.push_back(p2);
        return;
    }
    const Vec2 q0 = (p0 + p1) * 0.5f;
    const Vec2 q1 = (p1 + p2) * 0.5f;
    const Vec2 r = (q0 + q1) * 0.5f;
    flatten_quad_bezier(p0, q0, r, tol_sq, out, depth + 1);
    flatten_quad_bezier(r, q1, p2, tol_sq, out, depth + 1);
}

void flatten_cubic_bezier(
    Vec2 p0,
    Vec2 p1,
    Vec2 p2,
    Vec2 p3,
    float tol_sq,
    std::vector<Vec2>& out,
    int depth = 0
) {
    // Distance of control points p1, p2 from chord p0->p3
    const Vec2 dir = p3 - p0;
    const float len2 = dir.x * dir.x + dir.y * dir.y;
    float d1_sq = 0.0f;
    float d2_sq = 0.0f;
    if (len2 > 1e-8f) {
        const float inv = 1.0f / len2;
        const float c1 = (p1.x - p0.x) * dir.y - (p1.y - p0.y) * dir.x;
        const float c2 = (p2.x - p0.x) * dir.y - (p2.y - p0.y) * dir.x;
        d1_sq = (c1 * c1) * inv;
        d2_sq = (c2 * c2) * inv;
    } else {
        d1_sq = (p1 - p0).x * (p1 - p0).x + (p1 - p0).y * (p1 - p0).y;
        d2_sq = (p2 - p0).x * (p2 - p0).x + (p2 - p0).y * (p2 - p0).y;
    }
    if (depth >= 9 || (d1_sq <= tol_sq && d2_sq <= tol_sq)) {
        out.push_back(p3);
        return;
    }
    const Vec2 p01 = (p0 + p1) * 0.5f;
    const Vec2 p12 = (p1 + p2) * 0.5f;
    const Vec2 p23 = (p2 + p3) * 0.5f;
    const Vec2 p012 = (p01 + p12) * 0.5f;
    const Vec2 p123 = (p12 + p23) * 0.5f;
    const Vec2 p0123 = (p012 + p123) * 0.5f;
    flatten_cubic_bezier(p0, p01, p012, p0123, tol_sq, out, depth + 1);
    flatten_cubic_bezier(p0123, p123, p23, p3, tol_sq, out, depth + 1);
}

std::vector<std::vector<Vec2>> parse_svg_path_d(const std::string& d, float tol_sq) {
    std::vector<std::vector<Vec2>> subpaths;
    std::vector<Vec2> cur_poly;

    size_t i = 0;
    const size_t n = d.size();
    char cmd = 0;
    Vec2 cur{0.0f, 0.0f};
    Vec2 start{0.0f, 0.0f};
    Vec2 last_ctrl{0.0f, 0.0f};
    char last_cmd = 0;

    auto skip_ws_comma = [&]() {
        while (i < n && (std::isspace(static_cast<unsigned char>(d[i])) || d[i] == ',')) ++i;
    };
    auto has_number = [&]() -> bool {
        skip_ws_comma();
        if (i >= n) return false;
        const char ch = d[i];
        return std::isdigit(static_cast<unsigned char>(ch)) || ch == '-' || ch == '+' || ch == '.';
    };
    auto read_num = [&]() -> float {
        skip_ws_comma();
        const char* s = d.data() + i;
        char* end = nullptr;
        const float v = std::strtof(s, &end);
        if (end == s) {
            ++i;
            return 0.0f;
        }
        i = static_cast<size_t>(end - d.data());
        return v;
    };
    auto read_flag = [&]() -> int {
        skip_ws_comma();
        if (i >= n) return 0;
        if (d[i] == '0' || d[i] == '1') {
            return d[i++] - '0';
        }
        return static_cast<int>(read_num()) != 0 ? 1 : 0;
    };

    auto flush_poly = [&]() {
        if (cur_poly.size() >= 2) {
            subpaths.push_back(std::move(cur_poly));
        }
        cur_poly.clear();
    };

    while (i < n) {
        skip_ws_comma();
        if (i >= n) break;
        if (std::isalpha(static_cast<unsigned char>(d[i]))) {
            cmd = d[i++];
        } else if (cmd == 0) {
            ++i;
            continue;
        }

        const bool rel = std::islower(static_cast<unsigned char>(cmd)) != 0;
        const char uc = static_cast<char>(std::toupper(static_cast<unsigned char>(cmd)));

        if (uc == 'Z') {
            if (!cur_poly.empty()) {
                cur_poly.push_back(start);
                flush_poly();
            }
            cur = start;
            last_cmd = 'Z';
            cmd = 0;
            continue;
        }

        if (!has_number()) {
            cmd = 0;
            continue;
        }

        switch (uc) {
            case 'M': {
                flush_poly();
                const float x = read_num() + (rel ? cur.x : 0.0f);
                const float y = read_num() + (rel ? cur.y : 0.0f);
                cur = Vec2{x, y};
                start = cur;
                cur_poly.push_back(cur);
                last_cmd = 'M';
                // Subsequent coordinate pairs after M are implicit L/l commands per SVG spec
                cmd = rel ? 'l' : 'L';
                break;
            }
            case 'L': {
                const float x = read_num() + (rel ? cur.x : 0.0f);
                const float y = read_num() + (rel ? cur.y : 0.0f);
                cur = Vec2{x, y};
                cur_poly.push_back(cur);
                last_cmd = 'L';
                break;
            }
            case 'H': {
                const float x = read_num() + (rel ? cur.x : 0.0f);
                cur.x = x;
                cur_poly.push_back(cur);
                last_cmd = 'H';
                break;
            }
            case 'V': {
                const float y = read_num() + (rel ? cur.y : 0.0f);
                cur.y = y;
                cur_poly.push_back(cur);
                last_cmd = 'V';
                break;
            }
            case 'C': {
                const Vec2 c1{read_num() + (rel ? cur.x : 0.0f), read_num() + (rel ? cur.y : 0.0f)};
                const Vec2 c2{read_num() + (rel ? cur.x : 0.0f), read_num() + (rel ? cur.y : 0.0f)};
                const Vec2 p{read_num() + (rel ? cur.x : 0.0f), read_num() + (rel ? cur.y : 0.0f)};
                flatten_cubic_bezier(cur, c1, c2, p, tol_sq, cur_poly);
                last_ctrl = c2;
                cur = p;
                last_cmd = 'C';
                break;
            }
            case 'S': {
                const Vec2 c1 = (last_cmd == 'C' || last_cmd == 'S') ? (cur * 2.0f - last_ctrl) : cur;
                const Vec2 c2{read_num() + (rel ? cur.x : 0.0f), read_num() + (rel ? cur.y : 0.0f)};
                const Vec2 p{read_num() + (rel ? cur.x : 0.0f), read_num() + (rel ? cur.y : 0.0f)};
                flatten_cubic_bezier(cur, c1, c2, p, tol_sq, cur_poly);
                last_ctrl = c2;
                cur = p;
                last_cmd = 'S';
                break;
            }
            case 'Q': {
                const Vec2 c1{read_num() + (rel ? cur.x : 0.0f), read_num() + (rel ? cur.y : 0.0f)};
                const Vec2 p{read_num() + (rel ? cur.x : 0.0f), read_num() + (rel ? cur.y : 0.0f)};
                flatten_quad_bezier(cur, c1, p, tol_sq, cur_poly);
                last_ctrl = c1;
                cur = p;
                last_cmd = 'Q';
                break;
            }
            case 'T': {
                const Vec2 c1 = (last_cmd == 'Q' || last_cmd == 'T') ? (cur * 2.0f - last_ctrl) : cur;
                const Vec2 p{read_num() + (rel ? cur.x : 0.0f), read_num() + (rel ? cur.y : 0.0f)};
                flatten_quad_bezier(cur, c1, p, tol_sq, cur_poly);
                last_ctrl = c1;
                cur = p;
                last_cmd = 'T';
                break;
            }
            case 'A': {
                const float rx = std::abs(read_num());
                const float ry = std::abs(read_num());
                const float rot_deg = read_num();
                const int large_arc = read_flag();
                const int sweep = read_flag();
                const Vec2 p{read_num() + (rel ? cur.x : 0.0f), read_num() + (rel ? cur.y : 0.0f)};
                if (rx < 1e-4f || ry < 1e-4f) {
                    cur = p;
                    cur_poly.push_back(cur);
                } else {
                    const float phi = rot_deg * (kPiF / 180.0f);
                    const float cp = std::cos(phi);
                    const float sp = std::sin(phi);
                    const float dx2 = (cur.x - p.x) * 0.5f;
                    const float dy2 = (cur.y - p.y) * 0.5f;
                    const float x1p = cp * dx2 + sp * dy2;
                    const float y1p = -sp * dx2 + cp * dy2;
                    float rx_s = rx;
                    float ry_s = ry;
                    const float lam = (x1p * x1p) / (rx_s * rx_s) + (y1p * y1p) / (ry_s * ry_s);
                    if (lam > 1.0f) {
                        const float s = std::sqrt(lam);
                        rx_s *= s;
                        ry_s *= s;
                    }
                    const float num = std::max(
                        0.0f,
                        rx_s * rx_s * ry_s * ry_s - rx_s * rx_s * y1p * y1p - ry_s * ry_s * x1p * x1p
                    );
                    const float den = rx_s * rx_s * y1p * y1p + ry_s * ry_s * x1p * x1p;
                    const float sign = (large_arc == sweep) ? -1.0f : 1.0f;
                    const float fac = den > 1e-8f ? sign * std::sqrt(num / den) : 0.0f;
                    const float cxp = fac * (rx_s * y1p / ry_s);
                    const float cyp = fac * (-ry_s * x1p / rx_s);
                    const float cx = cp * cxp - sp * cyp + (cur.x + p.x) * 0.5f;
                    const float cy = sp * cxp + cp * cyp + (cur.y + p.y) * 0.5f;

                    auto vec_angle = [](float ux, float uy, float vx, float vy) -> float {
                        const float dot = ux * vx + uy * vy;
                        const float len = std::hypot(ux, uy) * std::hypot(vx, vy);
                        const float c = std::clamp(dot / std::max(len, 1e-8f), -1.0f, 1.0f);
                        const float ang = std::acos(c);
                        return (ux * vy - uy * vx) < 0.0f ? -ang : ang;
                    };
                    const float theta1 = vec_angle(1.0f, 0.0f, (x1p - cxp) / rx_s, (y1p - cyp) / ry_s);
                    float dtheta = vec_angle(
                        (x1p - cxp) / rx_s,
                        (y1p - cyp) / ry_s,
                        (-x1p - cxp) / rx_s,
                        (-y1p - cyp) / ry_s
                    );
                    if (sweep == 0 && dtheta > 0.0f) dtheta -= 2.0f * kPiF;
                    if (sweep != 0 && dtheta < 0.0f) dtheta += 2.0f * kPiF;

                    const int segs = std::clamp(static_cast<int>(std::ceil(std::abs(dtheta) / 0.18f)), 4, 48);
                    for (int s = 1; s <= segs; ++s) {
                        const float a = theta1 + dtheta * (static_cast<float>(s) / segs);
                        const float ex = rx_s * std::cos(a);
                        const float ey = ry_s * std::sin(a);
                        cur_poly.push_back(Vec2{cx + cp * ex - sp * ey, cy + sp * ex + cp * ey});
                    }
                    cur = p;
                }
                last_cmd = 'A';
                break;
            }
            default:
                cmd = 0;
                break;
        }
    }
    flush_poly();
    return subpaths;
}

struct XmlTag {
    std::string name;
    bool is_closing = false;
    bool self_closing = false;
    std::unordered_map<std::string, std::string> attrs;
};

bool next_xml_tag(const std::string& xml, size_t& pos, XmlTag& out) {
    out = XmlTag{};
    while (pos < xml.size()) {
        const size_t lt = xml.find('<', pos);
        if (lt == std::string::npos) return false;
        if (lt + 3 < xml.size() && xml.compare(lt, 4, "<!--") == 0) {
            const size_t end = xml.find("-->", lt + 4);
            pos = (end == std::string::npos) ? xml.size() : (end + 3);
            continue;
        }
        if (lt + 1 < xml.size() && (xml[lt + 1] == '?' || xml[lt + 1] == '!')) {
            const size_t gt = xml.find('>', lt + 2);
            pos = (gt == std::string::npos) ? xml.size() : (gt + 1);
            continue;
        }
        const size_t gt = xml.find('>', lt + 1);
        if (gt == std::string::npos) return false;
        pos = gt + 1;

        std::string_view inner(xml.data() + lt + 1, gt - lt - 1);
        size_t i = 0;
        while (i < inner.size() && std::isspace(static_cast<unsigned char>(inner[i]))) ++i;
        if (i < inner.size() && inner[i] == '/') {
            out.is_closing = true;
            ++i;
        }
        if (!inner.empty() && inner.back() == '/') {
            out.self_closing = true;
            inner.remove_suffix(1);
        }
        const size_t name_start = i;
        while (i < inner.size() && !std::isspace(static_cast<unsigned char>(inner[i]))) ++i;
        out.name = std::string(inner.substr(name_start, i - name_start));

        while (i < inner.size()) {
            while (i < inner.size() && std::isspace(static_cast<unsigned char>(inner[i]))) ++i;
            if (i >= inner.size()) break;
            const size_t ks = i;
            while (i < inner.size() && inner[i] != '=' && !std::isspace(static_cast<unsigned char>(inner[i]))) ++i;
            const std::string key(inner.substr(ks, i - ks));
            while (i < inner.size() && std::isspace(static_cast<unsigned char>(inner[i]))) ++i;
            if (i >= inner.size() || inner[i] != '=') continue;
            ++i;
            while (i < inner.size() && std::isspace(static_cast<unsigned char>(inner[i]))) ++i;
            if (i >= inner.size()) break;
            const char q = inner[i];
            if (q == '"' || q == '\'') {
                ++i;
                const size_t vs = i;
                while (i < inner.size() && inner[i] != q) ++i;
                out.attrs[key] = std::string(inner.substr(vs, i - vs));
                if (i < inner.size()) ++i;
            } else {
                const size_t vs = i;
                while (i < inner.size() && !std::isspace(static_cast<unsigned char>(inner[i]))) ++i;
                out.attrs[key] = std::string(inner.substr(vs, i - vs));
            }
        }
        return true;
    }
    return false;
}

struct StyleState {
    Mat2D transform{};
    ColorRGBA current_color{0.96f, 0.97f, 0.99f, 1.0f};
    ColorRGBA fill{0.96f, 0.97f, 0.99f, 1.0f};
    bool has_fill = true;
    ColorRGBA stroke{0.96f, 0.97f, 0.99f, 1.0f};
    bool has_stroke = false;
    float stroke_width = 0.0f;
};

void apply_style_kv(const std::string& key, const std::string& val, bool is_container, StyleState& st) {
    if (key == "color") {
        ColorRGBA c = st.current_color;
        if (parse_hex_color(val, c)) {
            st.current_color = c;
        }
        return;
    }
    if (key == "fill") {
        if (val == "none") {
            st.has_fill = false;
        } else if (val == "currentColor") {
            st.fill = st.current_color;
            st.has_fill = true;
        } else {
            ColorRGBA c = st.fill;
            if (parse_hex_color(val, c)) {
                st.fill = c;
                st.has_fill = true;
                if (is_container) {
                    st.current_color = c;
                }
            }
        }
        return;
    }
    if (key == "stroke") {
        if (val == "none") {
            st.has_stroke = false;
        } else if (val == "currentColor") {
            st.stroke = st.current_color;
            st.has_stroke = true;
        } else {
            ColorRGBA c = st.stroke;
            if (parse_hex_color(val, c)) {
                st.stroke = c;
                st.has_stroke = true;
                if (is_container && !st.has_fill) {
                    st.current_color = c;
                }
            }
        }
        return;
    }
    if (key == "stroke-width") {
        const auto nums = parse_number_list(val);
        if (!nums.empty()) st.stroke_width = nums[0];
    }
}

void apply_inline_style(const std::string& style_str, bool is_container, StyleState& st) {
    size_t pos = 0;
    while (pos < style_str.size()) {
        const size_t semi = style_str.find(';', pos);
        const std::string_view decl = semi == std::string::npos
            ? std::string_view(style_str).substr(pos)
            : std::string_view(style_str).substr(pos, semi - pos);
        const size_t colon = decl.find(':');
        if (colon != std::string_view::npos) {
            const std::string key = trim(decl.substr(0, colon));
            const std::string val = trim(decl.substr(colon + 1));
            apply_style_kv(key, val, is_container, st);
        }
        if (semi == std::string::npos) break;
        pos = semi + 1;
    }
}

struct FlattenedShape {
    std::vector<std::vector<Vec2>> subpaths;
    ColorRGBA color;
    bool is_stroke = false;
    float stroke_width = 0.0f;
};

}  // namespace

Result<PathAsset> parse_svg_to_path_asset(
    const std::string& svg_xml,
    const std::string& cache_key,
    float target_height_px,
    ColorRGBA default_color
) {
    if (svg_xml.empty()) {
        return fail("Empty SVG input");
    }

    // Estimate tolerance in SVG user units (MathJax uses ~1000 units per ex)
    float view_h = 1000.0f;
    if (const size_t vb = svg_xml.find("viewBox=\""); vb != std::string::npos) {
        const size_t q2 = svg_xml.find('"', vb + 9);
        if (q2 != std::string::npos) {
            const auto nums = parse_number_list(std::string_view(svg_xml).substr(vb + 9, q2 - vb - 9));
            if (nums.size() == 4 && nums[3] > 1e-3f) {
                view_h = nums[3];
            }
        }
    }
    const float tol = std::max(0.05f, view_h / 900.0f);
    const float tol_sq = tol * tol;

    std::unordered_map<std::string, std::string> defs_paths;
    std::vector<StyleState> stack;
    StyleState root_style{};
    root_style.current_color = default_color;
    root_style.fill = default_color;
    root_style.stroke = default_color;
    stack.push_back(root_style);

    bool in_defs = false;
    std::vector<FlattenedShape> shapes;

    size_t pos = 0;
    XmlTag tag;
    while (next_xml_tag(svg_xml, pos, tag)) {
        if (tag.is_closing) {
            if (tag.name == "defs") {
                in_defs = false;
            }
            if (stack.size() > 1 && (tag.name == "g" || tag.name == "svg" || tag.name == "defs")) {
                stack.pop_back();
            }
            continue;
        }

        const bool is_container = (tag.name == "g" || tag.name == "svg");
        StyleState st = stack.back();
        if (auto it = tag.attrs.find("transform"); it != tag.attrs.end()) {
            st.transform = st.transform.mul(parse_transform(it->second));
        }
        if (auto it = tag.attrs.find("color"); it != tag.attrs.end()) {
            apply_style_kv("color", it->second, is_container, st);
        }
        if (auto it = tag.attrs.find("fill"); it != tag.attrs.end()) {
            apply_style_kv("fill", it->second, is_container, st);
        }
        if (auto it = tag.attrs.find("stroke"); it != tag.attrs.end()) {
            apply_style_kv("stroke", it->second, is_container, st);
        }
        if (auto it = tag.attrs.find("stroke-width"); it != tag.attrs.end()) {
            apply_style_kv("stroke-width", it->second, is_container, st);
        }
        if (auto it = tag.attrs.find("style"); it != tag.attrs.end()) {
            apply_inline_style(it->second, is_container, st);
        }

        if (tag.name == "defs") {
            if (!tag.self_closing) {
                in_defs = true;
                stack.push_back(st);
            }
            continue;
        }

        if (in_defs) {
            if (tag.name == "path") {
                auto id_it = tag.attrs.find("id");
                auto d_it = tag.attrs.find("d");
                if (id_it != tag.attrs.end() && d_it != tag.attrs.end()) {
                    defs_paths[id_it->second] = d_it->second;
                }
            }
            if (!tag.self_closing && (tag.name == "g" || tag.name == "svg")) {
                stack.push_back(st);
            }
            continue;
        }

        auto emit_subpaths = [&](std::vector<std::vector<Vec2>> raw_polys, const StyleState& cur_st) {
            if (raw_polys.empty()) return;
            for (auto& poly : raw_polys) {
                for (Vec2& p : poly) {
                    p = cur_st.transform.apply(p);
                }
            }
            if (cur_st.has_fill) {
                shapes.push_back(FlattenedShape{raw_polys, cur_st.fill, false, 0.0f});
            }
            if (cur_st.has_stroke && cur_st.stroke_width > 0.0f) {
                // Convert transform scale into stroke width approximately
                const float det_scale = std::sqrt(std::abs(cur_st.transform.a * cur_st.transform.d - cur_st.transform.b * cur_st.transform.c));
                shapes.push_back(FlattenedShape{std::move(raw_polys), cur_st.stroke, true, cur_st.stroke_width * std::max(det_scale, 0.1f)});
            }
        };

        if (tag.name == "path") {
            if (auto d_it = tag.attrs.find("d"); d_it != tag.attrs.end()) {
                emit_subpaths(parse_svg_path_d(d_it->second, tol_sq), st);
            }
        } else if (tag.name == "use") {
            std::string href;
            if (auto it = tag.attrs.find("xlink:href"); it != tag.attrs.end()) href = it->second;
            else if (auto it2 = tag.attrs.find("href"); it2 != tag.attrs.end()) href = it2->second;
            if (!href.empty() && href[0] == '#') href = href.substr(1);
            if (auto p_it = defs_paths.find(href); p_it != defs_paths.end()) {
                const float ux = tag.attrs.contains("x") ? std::strtof(tag.attrs["x"].c_str(), nullptr) : 0.0f;
                const float uy = tag.attrs.contains("y") ? std::strtof(tag.attrs["y"].c_str(), nullptr) : 0.0f;
                StyleState ust = st;
                ust.transform = ust.transform.mul(Mat2D{1, 0, 0, 1, ux, uy});
                emit_subpaths(parse_svg_path_d(p_it->second, tol_sq), ust);
            }
        } else if (tag.name == "rect") {
            const float x = tag.attrs.contains("x") ? std::strtof(tag.attrs["x"].c_str(), nullptr) : 0.0f;
            const float y = tag.attrs.contains("y") ? std::strtof(tag.attrs["y"].c_str(), nullptr) : 0.0f;
            const float w = tag.attrs.contains("width") ? std::strtof(tag.attrs["width"].c_str(), nullptr) : 0.0f;
            const float h = tag.attrs.contains("height") ? std::strtof(tag.attrs["height"].c_str(), nullptr) : 0.0f;
            if (w > 0.0f && h > 0.0f) {
                std::vector<Vec2> r = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}, {x, y}};
                emit_subpaths({std::move(r)}, st);
            }
        } else if (tag.name == "line") {
            const float x1 = tag.attrs.contains("x1") ? std::strtof(tag.attrs["x1"].c_str(), nullptr) : 0.0f;
            const float y1 = tag.attrs.contains("y1") ? std::strtof(tag.attrs["y1"].c_str(), nullptr) : 0.0f;
            const float x2 = tag.attrs.contains("x2") ? std::strtof(tag.attrs["x2"].c_str(), nullptr) : 0.0f;
            const float y2 = tag.attrs.contains("y2") ? std::strtof(tag.attrs["y2"].c_str(), nullptr) : 0.0f;
            st.has_fill = false;
            if (!st.has_stroke) {
                st.has_stroke = true;
                st.stroke = default_color;
                st.stroke_width = std::max(st.stroke_width, 20.0f);
            }
            emit_subpaths({{{x1, y1}, {x2, y2}}}, st);
        }

        if (!tag.self_closing && (tag.name == "g" || tag.name == "svg")) {
            stack.push_back(st);
        }
    }

    if (shapes.empty()) {
        return fail("SVG contained no renderable geometry");
    }

    // Compute raw SVG bounds, then normalize: flip Y (SVG y-down -> engine y-up), center at (0,0), scale to target_height_px
    Vec2 min_p{1e30f, 1e30f};
    Vec2 max_p{-1e30f, -1e30f};
    for (const auto& sh : shapes) {
        for (const auto& poly : sh.subpaths) {
            for (const Vec2& p : poly) {
                min_p.x = std::min(min_p.x, p.x);
                min_p.y = std::min(min_p.y, p.y);
                max_p.x = std::max(max_p.x, p.x);
                max_p.y = std::max(max_p.y, p.y);
            }
        }
    }
    const float raw_h = std::max(1e-3f, max_p.y - min_p.y);
    const float scale = target_height_px / raw_h;
    const Vec2 raw_center{(min_p.x + max_p.x) * 0.5f, (min_p.y + max_p.y) * 0.5f};

    auto map_pt = [&](Vec2 p) -> Vec2 {
        return Vec2{(p.x - raw_center.x) * scale, -(p.y - raw_center.y) * scale};
    };

    PathAsset asset;
    asset.cache_key = cache_key;
    asset.bounds_min = Vec2{(min_p.x - raw_center.x) * scale, -(max_p.y - raw_center.y) * scale};
    asset.bounds_max = Vec2{(max_p.x - raw_center.x) * scale, -(min_p.y - raw_center.y) * scale};

    // Compile filled polygons via Non-Zero Winding Triangle Fan from contour anchor (perfect for Stencil-then-Cover!)
    // and stroked polylines as extruded miter/quad segments directly into triangles.
    for (const auto& sh : shapes) {
        if (!sh.is_stroke) {
            for (const auto& poly : sh.subpaths) {
                if (poly.size() < 3) continue;
                const Vec2 origin = map_pt(poly[0]);
                for (size_t k = 1; k + 1 < poly.size(); ++k) {
                    const Vec2 p1 = map_pt(poly[k]);
                    const Vec2 p2 = map_pt(poly[k + 1]);
                    const uint32_t base = static_cast<uint32_t>(asset.vertices.size());
                    asset.vertices.push_back({origin.x, origin.y, sh.color.r, sh.color.g, sh.color.b, sh.color.a});
                    asset.vertices.push_back({p1.x, p1.y, sh.color.r, sh.color.g, sh.color.b, sh.color.a});
                    asset.vertices.push_back({p2.x, p2.y, sh.color.r, sh.color.g, sh.color.b, sh.color.a});
                    asset.indices.push_back(base + 0);
                    asset.indices.push_back(base + 1);
                    asset.indices.push_back(base + 2);
                }
            }
        } else {
            const float half_w = std::max(0.6f, sh.stroke_width * scale * 0.5f);
            for (const auto& poly : sh.subpaths) {
                for (size_t k = 0; k + 1 < poly.size(); ++k) {
                    const Vec2 a = map_pt(poly[k]);
                    const Vec2 b = map_pt(poly[k + 1]);
                    const Vec2 dir = (b - a).normalized();
                    if (dir.length() < 1e-5f) continue;
                    const Vec2 n{-dir.y * half_w, dir.x * half_w};
                    const uint32_t base = static_cast<uint32_t>(asset.vertices.size());
                    asset.vertices.push_back({a.x - n.x, a.y - n.y, sh.color.r, sh.color.g, sh.color.b, sh.color.a});
                    asset.vertices.push_back({a.x + n.x, a.y + n.y, sh.color.r, sh.color.g, sh.color.b, sh.color.a});
                    asset.vertices.push_back({b.x + n.x, b.y + n.y, sh.color.r, sh.color.g, sh.color.b, sh.color.a});
                    asset.vertices.push_back({b.x - n.x, b.y - n.y, sh.color.r, sh.color.g, sh.color.b, sh.color.a});
                    asset.indices.push_back(base + 0);
                    asset.indices.push_back(base + 1);
                    asset.indices.push_back(base + 2);
                    asset.indices.push_back(base + 0);
                    asset.indices.push_back(base + 2);
                    asset.indices.push_back(base + 3);
                }
            }
        }
    }

    if (asset.indices.empty()) {
        return fail("SVG tessellation yielded zero triangles");
    }
    return asset;
}

}  // namespace stem::text
