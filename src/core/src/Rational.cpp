#include "core/Rational.hpp"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>

namespace editor::core {

namespace {
std::int64_t abs64(std::int64_t v) noexcept {
    return v < 0 ? -v : v;
}
} // namespace

std::int64_t Rational::gcd(std::int64_t a, std::int64_t b) noexcept {
    a = abs64(a);
    b = abs64(b);
    while (b != 0) {
        const std::int64_t r = a % b;
        a = b;
        b = r;
    }
    return a == 0 ? 1 : a;
}

std::int64_t Rational::checkedMul(std::int64_t a, std::int64_t b) {
    if (a == 0 || b == 0) {
        return 0;
    }
    const std::int64_t aa = abs64(a);
    const std::int64_t bb = abs64(b);
    if (aa > (std::numeric_limits<std::int64_t>::max)() / bb) {
        throw std::overflow_error("Rational multiplication overflow");
    }
    return a * b;
}

std::int64_t Rational::checkedAdd(std::int64_t a, std::int64_t b) {
    if ((b > 0 && a > (std::numeric_limits<std::int64_t>::max)() - b) ||
        (b < 0 && a < (std::numeric_limits<std::int64_t>::min)() - b)) {
        throw std::overflow_error("Rational addition overflow");
    }
    return a + b;
}

Rational::Rational(std::int64_t n, std::int64_t d) {
    if (d == 0) {
        throw std::invalid_argument("Rational denominator must be non-zero");
    }
    if (d < 0) {
        n = -n;
        d = -d;
    }
    const std::int64_t g = gcd(n, d);
    num_ = n / g;
    den_ = d / g;
}

Rational Rational::fromFrames(std::int64_t frames, std::int64_t fps) {
    if (fps <= 0) {
        throw std::invalid_argument("fps must be positive");
    }
    return Rational(frames, fps);
}

Rational Rational::fromFrames(std::int64_t frames, const Rational& fps) {
    // frames / fps  ==  frames * (fps.den / fps.num)
    if (fps.num() <= 0) {
        throw std::invalid_argument("fps must be positive");
    }
    return Rational(frames, 1) / fps;
}

Rational Rational::operator+(const Rational& o) const {
    // a/b + c/d with cross-cancellation to keep intermediates small.
    const std::int64_t g = gcd(den_, o.den_);
    const std::int64_t l1 = den_ / g;
    const std::int64_t l2 = o.den_ / g;
    const std::int64_t left = checkedMul(num_, l2);
    const std::int64_t right = checkedMul(o.num_, l1);
    const std::int64_t n = checkedAdd(left, right);
    const std::int64_t d = checkedMul(l1, o.den_);
    return Rational(n, d);
}

Rational Rational::operator-(const Rational& o) const {
    return *this + (-o);
}

Rational Rational::operator*(const Rational& o) const {
    // Cancel cross-gcds before multiplying.
    const std::int64_t g1 = gcd(num_, o.den_);
    const std::int64_t g2 = gcd(o.num_, den_);
    const std::int64_t a = num_ / g1;
    const std::int64_t b = o.den_ / g1;
    const std::int64_t c = o.num_ / g2;
    const std::int64_t d = den_ / g2;
    return Rational(checkedMul(a, c), checkedMul(d, b));
}

Rational Rational::operator/(const Rational& o) const {
    if (o.num_ == 0) {
        throw std::invalid_argument("Rational division by zero");
    }
    Rational inv(o.den_, o.num_);
    if (o.num_ < 0) { // keep denominator positive after flip
        inv = Rational(-o.den_, -o.num_);
    }
    return *this * inv;
}

std::strong_ordering Rational::operator<=>(const Rational& o) const noexcept {
    // Exact cross-multiplied compare; fall back to long double only when the
    // reduced products would overflow int64 (never for timeline magnitudes).
    const std::int64_t g = gcd(den_, o.den_);
    const std::int64_t l1 = den_ / g;
    const std::int64_t l2 = o.den_ / g;
    try {
        const std::int64_t left = checkedMul(num_, l2);
        const std::int64_t right = checkedMul(o.num_, l1);
        if (left < right) {
            return std::strong_ordering::less;
        }
        if (left > right) {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    } catch (const std::overflow_error&) {
        const long double left =
            static_cast<long double>(num_) * static_cast<long double>(l2);
        const long double right =
            static_cast<long double>(o.num_) * static_cast<long double>(l1);
        if (left < right) {
            return std::strong_ordering::less;
        }
        if (left > right) {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equivalent;
    }
}

bool Rational::operator==(const Rational& o) const noexcept {
    return num_ == o.num_ && den_ == o.den_;
}

std::int64_t Rational::toFramesRounded(std::int64_t fps) const {
    if (fps <= 0) {
        throw std::invalid_argument("fps must be positive");
    }
    const long double v = static_cast<long double>(num_) / static_cast<long double>(den_) *
                          static_cast<long double>(fps);
    return static_cast<std::int64_t>(std::llround(v));
}

std::int64_t Rational::toFramesRounded(const Rational& fps) const {
    const long double rate =
        static_cast<long double>(fps.num()) / static_cast<long double>(fps.den());
    const long double v =
        static_cast<long double>(num_) / static_cast<long double>(den_) * rate;
    return static_cast<std::int64_t>(std::llround(v));
}

std::string Rational::toString() const {
    std::ostringstream os;
    os << num_;
    if (den_ != 1) {
        os << '/' << den_;
    }
    return os.str();
}

} // namespace editor::core
