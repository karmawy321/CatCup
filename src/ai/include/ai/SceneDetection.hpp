#pragma once

#include "core/Rational.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace editor::ai {

struct SceneCut {
    core::Rational timestamp;
    double score; // difference metric [0..1]
};

class SceneDetection {
public:
    /// Calculate normalized Mean Absolute Difference (MAD) in [0.0, 1.0] between two RGBA buffers.
    static double calculateFrameDifference(
        const uint8_t* frameA,
        const uint8_t* frameB,
        int width,
        int height,
        int strideBytes
    );

    /// Detect scene cut points given a series of frame timestamps and their difference scores.
    static std::vector<SceneCut> findCutsFromScores(
        const std::vector<core::Rational>& timestamps,
        const std::vector<double>& diffScores,
        double threshold = 0.25
    );
};

} // namespace editor::ai
