#include "TestHarness.hpp"

#include "core/Rational.hpp"

using editor::core::Rational;

TEST_CASE("rational: reduction and sign normalization") {
    CHECK_EQ(Rational(2, 4), Rational(1, 2));
    CHECK_EQ(Rational(-2, 4), Rational(-1, 2));
    CHECK_EQ(Rational(2, -4), Rational(-1, 2));
    CHECK_EQ(Rational(0, 5), Rational(0));
}

TEST_CASE("rational: arithmetic") {
    CHECK_EQ(Rational(1, 3) + Rational(1, 6), Rational(1, 2));
    CHECK_EQ(Rational(3, 4) - Rational(1, 2), Rational(1, 4));
    CHECK_EQ(Rational(2, 3) * Rational(3, 4), Rational(1, 2));
    CHECK_EQ(Rational(2, 3) / Rational(4, 9), Rational(3, 2));
    // Frame-exact: 1/30 s * 30 fps == 1 frame worth.
    CHECK_EQ(Rational(1, 30) * Rational(30, 1), Rational(1, 1));
}

TEST_CASE("rational: ordering") {
    CHECK(Rational(1, 3) < Rational(1, 2));
    CHECK(Rational(2, 4) <= Rational(1, 2));
    CHECK(Rational(3, 2) > Rational(1, 1));
    CHECK(!(Rational(1, 2) < Rational(1, 2)));
}

TEST_CASE("rational: frame conversion is exact for fixture rate") {
    // Fixture: 8 s @ 30 fps == 240 frames.
    CHECK_EQ(Rational(8, 1).toFramesRounded(30), 240);
    CHECK_EQ(Rational::fromFrames(240, 30), Rational(8, 1));
    CHECK_EQ(Rational::fromFrames(45, 30), Rational(3, 2));
    // NTSC-style rate: 30000/1001.
    const Rational ntsc(30000, 1001);
    CHECK_EQ(Rational::fromFrames(60, ntsc).toFramesRounded(ntsc), 60);
}

TEST_CASE("rational: invalid construction throws") {
    bool threw = false;
    try {
        Rational bad(1, 0);
        (void)bad;
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

TEST_CASE("time range: contains and overlaps") {
    using editor::core::TimeRange;
    const TimeRange r{Rational(2, 1), Rational(3, 1)}; // [2,5)
    CHECK(r.contains(Rational(2, 1)));
    CHECK(r.contains(Rational(4, 1)));
    CHECK(!r.contains(Rational(5, 1))); // exclusive end
    CHECK(!r.contains(Rational(1, 1)));
    CHECK(r.overlaps(TimeRange{Rational(4, 1), Rational(3, 1)}));
    CHECK(!r.overlaps(TimeRange{Rational(5, 1), Rational(1, 1)}));
}

int main() {
    return editor::tests::runAll();
}
