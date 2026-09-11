#include "ai/SceneDetection.hpp"

#include <cmath>
#include <algorithm>

namespace editor::ai {

double SceneDetection::calculateFrameDifference(
    const uint8_t* frameA,
    const uint8_t* frameB,
    int width,
    int height,
    int strideBytes
) {
    if (frameA == nullptr || frameB == nullptr || width <= 0 || height <= 0 || strideBytes <= 0) {
        return 0.0;
    }

    double diffSum = 0.0;
    for (int y = 0; y < height; ++y) {
        const uint8_t* rowA = frameA + y * strideBytes;
        const uint8_t* rowB = frameB + y * strideBytes;
        for (int x = 0; x < width; ++x) {
            const int idx = x * 4;
            const int dr = std::abs(static_cast<int>(rowA[idx + 0]) - static_cast<int>(rowB[idx + 0]));
            const int dg = std::abs(static_cast<int>(rowA[idx + 1]) - static_cast<int>(rowB[idx + 1]));
            const int db = std::abs(static_cast<int>(rowA[idx + 2]) - static_cast<int>(rowB[idx + 2]));
            diffSum += static_cast<double>(dr + dg + db);
        }
    }

    const double totalMax = static_cast<double>(width) * static_cast<double>(height) * 3.0 * 255.0;
    return totalMax > 0.0 ? (diffSum / totalMax) : 0.0;
}

std::vector<SceneCut> SceneDetection::findCutsFromScores(
    const std::vector<core::Rational>& timestamps,
    const std::vector<double>& diffScores,
    double threshold
) {
    std::vector<SceneCut> cuts;
    if (timestamps.size() != diffScores.size() || diffScores.empty()) {
        return cuts;
    }

    for (std::size_t i = 0; i < diffScores.size(); ++i) {
        if (diffScores[i] >= threshold) {
            bool isLocalMax = true;
            if (i > 0 && diffScores[i - 1] > diffScores[i]) {
                isLocalMax = false;
            }
            if (i + 1 < diffScores.size() && diffScores[i + 1] > diffScores[i]) {
                isLocalMax = false;
            }
            if (isLocalMax) {
                cuts.push_back(SceneCut{timestamps[i], diffScores[i]});
            }
        }
    }

    return cuts;
}

} // namespace editor::ai
