#pragma once

#include <cmath>
#include <cstdint>
#include <format>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace stem {

using Error = std::string;

// Lightweight C++20 std::expected-compatible Result<T> (keeps strict C++20 compatibility for nvcc).
template <typename E>
struct Unexpected {
    E error;
};

template <typename E>
Unexpected(E) -> Unexpected<E>;

inline Unexpected<Error> fail(Error msg) {
    return Unexpected<Error>{std::move(msg)};
}

template <typename T, typename E = Error>
class [[nodiscard]] Expected {
public:
    Expected(const T& v) : data_(std::in_place_index<0>, v) {}
    Expected(T&& v) : data_(std::in_place_index<0>, std::move(v)) {}
    Expected(const Unexpected<E>& u) : data_(std::in_place_index<1>, u.error) {}
    Expected(Unexpected<E>&& u) : data_(std::in_place_index<1>, std::move(u.error)) {}

    [[nodiscard]] bool has_value() const noexcept { return data_.index() == 0; }
    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] T& value() & { return std::get<0>(data_); }
    [[nodiscard]] const T& value() const& { return std::get<0>(data_); }
    [[nodiscard]] T&& value() && { return std::get<0>(std::move(data_)); }

    [[nodiscard]] T& operator*() & { return std::get<0>(data_); }
    [[nodiscard]] const T& operator*() const& { return std::get<0>(data_); }
    [[nodiscard]] T* operator->() { return &std::get<0>(data_); }
    [[nodiscard]] const T* operator->() const { return &std::get<0>(data_); }

    [[nodiscard]] const E& error() const& { return std::get<1>(data_); }
    [[nodiscard]] E& error() & { return std::get<1>(data_); }

private:
    std::variant<T, E> data_;
};

template <typename E>
class [[nodiscard]] Expected<void, E> {
public:
    Expected() = default;
    Expected(const Unexpected<E>& u) : err_(u.error) {}
    Expected(Unexpected<E>&& u) : err_(std::move(u.error)) {}

    [[nodiscard]] bool has_value() const noexcept { return !err_.has_value(); }
    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }
    [[nodiscard]] const E& error() const& { return *err_; }
    [[nodiscard]] E& error() & { return *err_; }

private:
    std::optional<E> err_;
};

template <typename T>
using Result = Expected<T, Error>;

using ObjectId = uint32_t;
inline constexpr ObjectId kInvalidId = 0;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2 operator+(const Vec2& o) const noexcept { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2& o) const noexcept { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const noexcept { return {x * s, y * s}; }
    [[nodiscard]] float length() const noexcept { return std::hypot(x, y); }
    [[nodiscard]] Vec2 normalized() const noexcept {
        const float len = length();
        return len > 1e-7f ? Vec2{x / len, y / len} : Vec2{0.0f, 0.0f};
    }
    auto operator<=>(const Vec2&) const = default;
};

struct ColorRGBA {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;

    constexpr ColorRGBA operator+(const ColorRGBA& o) const noexcept {
        return {r + o.r, g + o.g, b + o.b, a + o.a};
    }
    constexpr ColorRGBA operator*(float s) const noexcept {
        return {r * s, g * s, b * s, a * s};
    }
    auto operator<=>(const ColorRGBA&) const = default;
};

// Rational FPS to derive deterministic timestamps strictly from frame index.
struct RationalFPS {
    int64_t num = 60;
    int64_t den = 1;

    [[nodiscard]] constexpr bool valid() const noexcept { return num > 0 && den > 0; }

    [[nodiscard]] constexpr RationalFPS reduced() const noexcept {
        if (!valid()) return *this;
        const int64_t g = std::gcd(num, den);
        return {num / g, den / g};
    }

    // Exact timestamp in seconds for frame_index (never derived from wall clock).
    [[nodiscard]] constexpr double time_at_frame(int64_t frame_index) const noexcept {
        return (static_cast<double>(frame_index) * static_cast<double>(den)) /
               static_cast<double>(num);
    }

    [[nodiscard]] constexpr double to_double() const noexcept {
        return static_cast<double>(num) / static_cast<double>(den);
    }

    [[nodiscard]] int64_t frame_count_for_range(double start_sec, double end_sec) const noexcept {
        if (end_sec <= start_sec || !valid()) return 0;
        const double duration = end_sec - start_sec;
        return static_cast<int64_t>(std::llround(duration * to_double()));
    }
};

enum DirtyFlags : uint32_t {
    DirtyNone = 0,
    DirtyTransform = 1u << 0,
    DirtyStyle = 1u << 1,
    DirtyVisibility = 1u << 2,
    DirtyGeometry = 1u << 3,
    DirtyAll = DirtyTransform | DirtyStyle | DirtyVisibility | DirtyGeometry,
};

}  // namespace stem
