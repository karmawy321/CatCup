// Shared FFmpeg helpers: exact rational conversions + error strings.
// Only translation units inside media/ffmpeg and export/ffmpeg use this.

#pragma once

#include "core/Rational.hpp"

#include <string>

struct AVRational;

namespace editor::ffmpeg_detail {

/// Exact AVRational -> Rational (0/0 and den==0 guarded to fallback).
core::Rational fromAvRational(AVRational r, core::Rational fallback);
/// Stream timestamp (pts in time_base units) -> seconds, exact.
core::Rational ptsToSeconds(std::int64_t pts, AVRational timeBase);
/// Seconds -> AV_TIME_BASE units for av_seek_frame.
std::int64_t secondsToAvTime(const core::Rational& t);
std::string avErrorString(int avErr);

} // namespace editor::ffmpeg_detail
