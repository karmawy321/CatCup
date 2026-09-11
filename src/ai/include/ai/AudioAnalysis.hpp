#pragma once

#include "core/Rational.hpp"

#include <cstddef>
#include <vector>

namespace editor::ai {

struct SilenceInterval {
    core::Rational startSec;
    core::Rational durationSec;
};

class AudioAnalysis {
public:
    /// Calculate RMS in dBFS for a block of audio samples.
    static double calculateRmsDb(const float* samples, std::size_t count);

    /// Detect intervals of silence below thresholdDb (e.g. -35.0 dBFS)
    /// that last at least minDurationSec (e.g. 0.4s).
    static std::vector<SilenceInterval> detectSilences(
        const float* pcmInterleaved,
        std::size_t totalSamples,
        int sampleRate,
        int channels,
        double thresholdDb = -35.0,
        double minDurationSec = 0.4,
        double windowSec = 0.05
    );
};

} // namespace editor::ai
