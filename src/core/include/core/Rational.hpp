#pragma once

// Canonical rational time. Storage-only: never use float/double seconds or
// pixel positions as the persisted timeline representation.
//
// Portable (MSVC has no __int128): multiplication pre-cancels by gcd and
// checks overflow explicitly, throwing std::overflow_error.

#include <compare>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace editor::core {

class Rational {
public:
    constexpr Rational() noexcept : num_(0), den_(1) {}
    constexpr Rational(std::int64_t n) noexcept : num_(n), den_(1) {}
    Rational(std::int64_t n, std::int64_t d);

    static Rational fromFrames(std::int64_t frames, std::int64_t fps);
    static Rational fromFrames(std::int64_t frames, const Rational& fps);

    [[nodiscard]] constexpr std::int64_t num() const noexcept { return num_; }
    [[nodiscard]] constexpr std::int64_t den() const noexcept { return den_; }

    [[nodiscard]] Rational operator-() const { return Rational(-num_, den_); }

    [[nodiscard]] Rational operator+(const Rational& o) const;
    [[nodiscard]] Rational operator-(const Rational& o) const;
    [[nodiscard]] Rational operator*(const Rational& o) const;
    [[nodiscard]] Rational operator/(const Rational& o) const;

    [[nodiscard]] std::strong_ordering operator<=>(const Rational& o) const noexcept;
    [[nodiscard]] bool operator==(const Rational& o) const noexcept;

    /// Display/export conversion only — never persist the result.
    [[nodiscard]] explicit operator double() const noexcept {
        return static_cast<double>(num_) / static_cast<double>(den_);
    }

    /// Round to nearest frame index at the given rate.
    [[nodiscard]] std::int64_t toFramesRounded(std::int64_t fps) const;
    [[nodiscard]] std::int64_t toFramesRounded(const Rational& fps) const;

    [[nodiscard]] bool isNegative() const noexcept { return num_ < 0; }
    [[nodiscard]] bool isZero() const noexcept { return num_ == 0; }

    [[nodiscard]] std::string toString() const;

private:
    std::int64_t num_;
    std::int64_t den_; // always > 0, always reduced

    static std::int64_t gcd(std::int64_t a, std::int64_t b) noexcept;
    static std::int64_t checkedMul(std::int64_t a, std::int64_t b);
    static std::int64_t checkedAdd(std::int64_t a, std::int64_t b);
};

struct TimeRange {
    Rational start{};
    Rational duration{};

    [[nodiscard]] Rational end() const { return start + duration; }
    [[nodiscard]] bool contains(const Rational& t) const noexcept {
        return !(t < start) && t < (start + duration);
    }
    [[nodiscard]] bool overlaps(const TimeRange& o) const noexcept {
        return start < (o.start + o.duration) && o.start < (start + duration);
    }
};

} // namespace editor::core
