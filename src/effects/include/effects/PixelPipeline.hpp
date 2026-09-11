#pragma once

#include "core/Model.hpp"
#include <cstdint>
#include <vector>

namespace editor::effects {

class PixelPipeline {
public:
    static void applyEffects(uint8_t* rgba, int width, int height, int strideBytes,
                             const std::vector<core::Effect>& effects);

    static void applyColorAdjust(uint8_t* rgba, int width, int height, int strideBytes,
                                 double brightness, double contrast, double saturation,
                                 double temperature, double tint);

    static void applyChromaKey(uint8_t* rgba, int width, int height, int strideBytes,
                               uint32_t keyColor, double similarity, double smoothness);

    static void applyVignette(uint8_t* rgba, int width, int height, int strideBytes,
                              double intensity, double radius, double softness);

    static void applyBlur(uint8_t* rgba, int width, int height, int strideBytes,
                          int radius);

    static void applySharpen(uint8_t* rgba, int width, int height, int strideBytes,
                             double amount);

    static uint32_t parseHexColor(const std::string& hex, uint32_t defaultColor = 0x0000FF00);
};

} // namespace editor::effects
