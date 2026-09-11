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

    /// Calculate integrated loudness in LUFS (BS.1770 / EBU R128).
    static double calculateIntegratedLufs(
        const float* samples,
        std::size_t count,
        int sampleRate = 48000,
        int channels = 2
    );

    /// Calculate absolute peak level in dBFS.
    static double calculatePeakDb(const float* samples, std::size_t count);

    /// Normalize audio buffer to target LUFS (e.g. -14 LUFS for YouTube, -23 for Broadcast)
    /// while honoring a true-peak ceiling (e.g. -1.0 dBFS).
    static void normalizeLoudness(
        float* samples,
        std::size_t count,
        double currentLufs,
        double targetLufs = -14.0,
        double maxPeakDb = -1.0
    );

    /// Apply voice cleanup: high-pass rumble filter (80 Hz) and soft speech noise gate.
    static void applyVoiceCleanup(
        float* samples,
        std::size_t count,
        int sampleRate = 48000,
        int channels = 2,
        double rumbleCutoffHz = 80.0
    );
};

} // namespace editor::ai
