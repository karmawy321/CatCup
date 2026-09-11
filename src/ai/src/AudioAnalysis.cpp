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

double AudioAnalysis::calculatePeakDb(const float* samples, std::size_t count) {
    if (samples == nullptr || count == 0) return -100.0;
    float maxVal = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        const float a = std::abs(samples[i]);
        if (a > maxVal) maxVal = a;
    }
    if (maxVal <= 1e-7f) return -100.0;
    const double db = 20.0 * std::log10(static_cast<double>(maxVal));
    return std::max(-100.0, db);
}

double AudioAnalysis::calculateIntegratedLufs(
    const float* samples,
    std::size_t count,
    int /*sampleRate*/,
    int /*channels*/
) {
    if (samples == nullptr || count == 0) return -70.0;

    double sumSq = 0.0;
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
    const double b0 = 1.0, b1 = -2.0, b2 = 1.0;
    const double a1 = -1.99004745483398, a2 = 0.99007225036621;

    for (std::size_t i = 0; i < count; ++i) {
        const double x0 = static_cast<double>(samples[i]);
        const double y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x0;
        y2 = y1; y1 = y0;
        sumSq += y0 * y0;
    }

    const double meanSq = sumSq / static_cast<double>(count);
    if (meanSq <= 1e-12) return -70.0;

    const double lufs = -0.691 + 10.0 * std::log10(meanSq);
    return std::max(-70.0, std::min(0.0, lufs));
}

void AudioAnalysis::normalizeLoudness(
    float* samples,
    std::size_t count,
    double currentLufs,
    double targetLufs,
    double maxPeakDb
) {
    if (samples == nullptr || count == 0) return;
    if (currentLufs <= -69.0) return;

    const double deltaDb = targetLufs - currentLufs;
    double gain = std::pow(10.0, deltaDb / 20.0);

    const double curPeak = calculatePeakDb(samples, count);
    if (curPeak > -90.0) {
        const double prospectivePeak = curPeak + deltaDb;
        if (prospectivePeak > maxPeakDb) {
            const double allowedDelta = maxPeakDb - curPeak;
            gain = std::pow(10.0, allowedDelta / 20.0);
        }
    }

    for (std::size_t i = 0; i < count; ++i) {
        double val = static_cast<double>(samples[i]) * gain;
        if (val > 0.95) {
            val = 0.95 + 0.05 * std::tanh((val - 0.95) / 0.05);
        } else if (val < -0.95) {
            val = -0.95 + 0.05 * std::tanh((val + 0.95) / 0.05);
        }
        samples[i] = static_cast<float>(val);
    }
}

void AudioAnalysis::applyVoiceCleanup(
    float* samples,
    std::size_t count,
    int sampleRate,
    int channels,
    double rumbleCutoffHz
) {
    if (samples == nullptr || count == 0 || sampleRate <= 0 || channels <= 0) return;

    const double dt = 1.0 / static_cast<double>(sampleRate);
    const double rc = 1.0 / (2.0 * 3.141592653589793 * rumbleCutoffHz);
    const double alpha = rc / (rc + dt);

    for (int ch = 0; ch < channels; ++ch) {
        double prevX = 0.0;
        double prevY = 0.0;
        for (std::size_t i = static_cast<std::size_t>(ch); i < count; i += static_cast<std::size_t>(channels)) {
            const double curX = static_cast<double>(samples[i]);
            const double curY = alpha * (prevY + curX - prevX);
            prevX = curX;
            prevY = curY;

            double filtered = curY;
            const double absVal = std::abs(filtered);
            if (absVal < 0.003) {
                filtered *= (absVal / 0.003);
            }
            samples[i] = static_cast<float>(filtered);
        }
    }
}

} // namespace editor::ai
