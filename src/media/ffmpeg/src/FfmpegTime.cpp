#include "media_ffmpeg/FfmpegTime.hpp"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/rational.h>
}

#include <cmath>
#include <cstring>

namespace editor::ffmpeg_detail {

core::Rational fromAvRational(AVRational r, core::Rational fallback) {
    if (r.den == 0 || r.num <= 0) {
        return fallback;
    }
    try {
        return core::Rational(r.num, r.den);
    } catch (...) {
        return fallback;
    }
}

core::Rational ptsToSeconds(std::int64_t pts, AVRational timeBase) {
    // seconds = pts * num / den, pre-cancelled to stay in int64 range.
    const std::int64_t n = timeBase.num;
    const std::int64_t d = timeBase.den <= 0 ? 1 : timeBase.den;
    try {
        return core::Rational(pts, 1) * core::Rational(n, d);
    } catch (...) {
        // Durations far outside media reality: degrade to double gracefully.
        const double s = static_cast<double>(pts) * static_cast<double>(n) /
                         static_cast<double>(d);
        const auto micros = static_cast<std::int64_t>(std::llround(s * 1000000.0));
        return core::Rational(micros, 1000000);
    }
}

std::int64_t secondsToAvTime(const core::Rational& t) {
    // Exact: num * AV_TIME_BASE / den. Media timelines never overflow this,
    // but stay checked and fall back rather than invoking UB.
    try {
        const core::Rational scaled = t * core::Rational(AV_TIME_BASE, 1);
        const std::int64_t whole = scaled.num() / scaled.den();
        return whole;
    } catch (...) {
        return static_cast<std::int64_t>(static_cast<double>(t) * AV_TIME_BASE);
    }
}

std::string avErrorString(int avErr) {
    char buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(avErr, buf, sizeof(buf));
    return std::string(buf);
}

} // namespace editor::ffmpeg_detail
