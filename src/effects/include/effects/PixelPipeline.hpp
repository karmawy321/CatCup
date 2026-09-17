#pragma once

#include "core/Model.hpp"
#include <cstdint>
#include <vector>

namespace editor::effects {

enum class BlendMode {
    Normal,
    Screen,
    Multiply,
    Overlay,
    Darken,
    Lighten,
    ColorBurn,
    LinearBurn,
    ColorDodge,
    SoftLight
};

struct ColorWheelParams {
    double liftR = 0.0, liftG = 0.0, liftB = 0.0, liftY = 0.0;
    double gammaR = 1.0, gammaG = 1.0, gammaB = 1.0, gammaY = 1.0;
    double gainR = 1.0, gainG = 1.0, gainB = 1.0, gainY = 1.0;
    double offsetR = 0.0, offsetG = 0.0, offsetB = 0.0;
    double lumaMix = 1.0;
};

struct CurvePoint {
    double x = 0.0;
    double y = 0.0;
};

class PixelPipeline {
public:
    static void applyEffects(uint8_t* rgba, int width, int height, int strideBytes,
                             const std::vector<core::Effect>& effects);

    static void applyColorAdjust(uint8_t* rgba, int width, int height, int strideBytes,
                                 double brightness, double contrast, double saturation,
                                 double temperature, double tint);

    static void applyHighlightsShadows(uint8_t* rgba, int width, int height, int strideBytes,
                                       double highlights, double shadows);

    static void applyColorWheels(uint8_t* rgba, int width, int height, int strideBytes,
                                 const ColorWheelParams& params);

    static void applyColorCurves(uint8_t* rgba, int width, int height, int strideBytes,
                                 const std::vector<CurvePoint>& curveY,
                                 const std::vector<CurvePoint>& curveR,
                                 const std::vector<CurvePoint>& curveG,
                                 const std::vector<CurvePoint>& curveB);

    static double evalCubicBezier(double p0, double p1, double p2, double p3, double t);

    static void applyChromaKey(uint8_t* rgba, int width, int height, int strideBytes,
                               uint32_t keyColor, double similarity, double smoothness);

    static void applyVignette(uint8_t* rgba, int width, int height, int strideBytes,
                              double intensity, double radius, double softness);

    static void applyBlur(uint8_t* rgba, int width, int height, int strideBytes,
                          int radius);

    static void applySharpen(uint8_t* rgba, int width, int height, int strideBytes,
                             double amount);

    static void applyBlend(uint8_t* dstRgba, const uint8_t* srcRgba,
                           int width, int height, int dstStrideBytes, int srcStrideBytes,
                           BlendMode mode, double opacity = 1.0);

    static BlendMode parseBlendMode(const std::string& name);
    static const char* blendModeName(BlendMode mode);

    static void applyLetterbox(uint8_t* rgba, int width, int height, int strideBytes,
                               double barHeight, double feather = 0.0);

    static void applyFilmGrain(uint8_t* rgba, int width, int height, int strideBytes,
                               double intensity, double size = 1.0, bool colored = false);

    static void applyChromaticAberration(uint8_t* rgba, int width, int height, int strideBytes,
                                         double shiftX, double shiftY);

    static void applyBloom(uint8_t* rgba, int width, int height, int strideBytes,
                           double threshold, int radius, double intensity);

    static void applySplitToning(uint8_t* rgba, int width, int height, int strideBytes,
                                 double shadowTeal, double highlightOrange, double balance = 0.0);

    static void applyRetroVhs(uint8_t* rgba, int width, int height, int strideBytes,
                              double scanlines, double colorBleed, double noise);

    static void applyPosterize(uint8_t* rgba, int width, int height, int strideBytes,
                               int levels);

    static void applyInvert(uint8_t* rgba, int width, int height, int strideBytes,
                            double intensity = 1.0);

    static void applyEdgeDetect(uint8_t* rgba, int width, int height, int strideBytes,
                                double intensity = 1.0, bool invert = false);

    static void applyMirror(uint8_t* rgba, int width, int height, int strideBytes,
                            int mode);

    static uint32_t parseHexColor(const std::string& hex, uint32_t defaultColor = 0x0000FF00);
};

} // namespace editor::effects
