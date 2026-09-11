#pragma once

#include "core/Rational.hpp"

#include <string>
#include <vector>

namespace editor::ai {

struct CaptionCue {
    core::Rational startSec;
    core::Rational durationSec;
    std::string text;
};

class CaptionEngine {
public:
    /// Parse SRT format subtitles into timed CaptionCues.
    static std::vector<CaptionCue> parseSrt(const std::string& srtContent);

    /// Parse WebVTT format subtitles into timed CaptionCues.
    static std::vector<CaptionCue> parseVtt(const std::string& vttContent);

    /// Export CaptionCues to standard SRT subtitle string.
    static std::string exportSrt(const std::vector<CaptionCue>& cues);

    /// Split a transcript into evenly distributed, chunked caption cues across a given duration.
    static std::vector<CaptionCue> chunkTranscript(
        const std::string& transcript,
        core::Rational totalDuration,
        std::size_t wordsPerCue = 4
    );
};

} // namespace editor::ai
