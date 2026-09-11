#include "ai/AudioAnalysis.hpp"

#include <cmath>
#include <algorithm>

namespace editor::ai {

namespace {
core::Rational toRational(double sec) {
    if (!std::isfinite(sec) || sec < 0.0) return core::Rational(0);
    const auto ms = static_cast<std::int64_t>(std::llround(sec * 1000.0));
    return core::Rational(ms, 1000);
}
} // namespace

double AudioAnalysis::calculateRmsDb(const float* samples, std::size_t count) {
    if (samples == nullptr || count == 0) {
        return -100.0;
    }
    double sumSq = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double s = static_cast<double>(samples[i]);
        sumSq += s * s;
    }
    const double meanSq = sumSq / static_cast<double>(count);
    const double rms = std::sqrt(meanSq);
    if (rms <= 1e-7) {
        return -100.0;
    }
    const double db = 20.0 * std::log10(rms);
    return std::max(-100.0, db);
}

std::vector<SilenceInterval> AudioAnalysis::detectSilences(
    const float* pcmInterleaved,
    std::size_t totalSamples,
    int sampleRate,
    int channels,
    double thresholdDb,
    double minDurationSec,
    double windowSec
) {
    std::vector<SilenceInterval> intervals;
    if (pcmInterleaved == nullptr || totalSamples == 0 || sampleRate <= 0 || channels <= 0) {
        return intervals;
    }

    const std::size_t samplesPerWindow = std::max<std::size_t>(
        static_cast<std::size_t>(channels),
        static_cast<std::size_t>(static_cast<double>(sampleRate) * windowSec) * static_cast<std::size_t>(channels)
    );

    const double actualWindowSec = static_cast<double>(samplesPerWindow / channels) / static_cast<double>(sampleRate);

    bool inSilence = false;
    double silenceStart = 0.0;
    double silenceDur = 0.0;

    std::size_t offset = 0;
    while (offset < totalSamples) {
        const std::size_t chunkSize = std::min(samplesPerWindow, totalSamples - offset);
        const double db = calculateRmsDb(pcmInterleaved + offset, chunkSize);
        const double currentTime = static_cast<double>(offset / channels) / static_cast<double>(sampleRate);

        if (db < thresholdDb) {
            if (!inSilence) {
                inSilence = true;
                silenceStart = currentTime;
                silenceDur = actualWindowSec;
            } else {
                silenceDur += actualWindowSec;
            }
        } else {
            if (inSilence) {
                if (silenceDur >= minDurationSec) {
                    intervals.push_back(SilenceInterval{
                        toRational(silenceStart),
                        toRational(silenceDur)
                    });
                }
                inSilence = false;
                silenceDur = 0.0;
            }
        }

        offset += chunkSize;
    }

    if (inSilence && silenceDur >= minDurationSec) {
        intervals.push_back(SilenceInterval{
            toRational(silenceStart),
            toRational(silenceDur)
        });
    }

    return intervals;
}

} // namespace editor::ai
