#include "effects/PixelPipeline.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace editor::effects {

uint32_t PixelPipeline::parseHexColor(const std::string& hex, uint32_t defaultColor) {
    std::string s = hex;
    if (!s.empty() && s[0] == '#') {
        s = s.substr(1);
    }
    if (s.size() == 6) {
        try {
            return static_cast<uint32_t>(std::stoul(s, nullptr, 16));
        } catch (...) {
            return defaultColor;
        }
    }
    return defaultColor;
}

void PixelPipeline::applyColorAdjust(uint8_t* rgba, int width, int height, int strideBytes,
                                     double brightness, double contrast, double saturation,
                                     double temperature, double tint) {
    if (width <= 0 || height <= 0 || rgba == nullptr) return;

    // Fast path: all defaults -> no op
    if (std::abs(brightness) < 0.001 && std::abs(contrast - 1.0) < 0.001 &&
        std::abs(saturation - 1.0) < 0.001 && std::abs(temperature) < 0.001 &&
        std::abs(tint) < 0.001) {
        return;
    }

    const double tempR = temperature * 0.15;
    const double tempB = -temperature * 0.15;
    const double tintG = -tint * 0.15;

    for (int y = 0; y < height; ++y) {
        uint8_t* row = rgba + y * strideBytes;
        for (int x = 0; x < width; ++x) {
            uint8_t* px = row + x * 4;
            double r = px[0] / 255.0;
            double g = px[1] / 255.0;
            double b = px[2] / 255.0;

            // Brightness
            r += brightness;
            g += brightness;
            b += brightness;

            // Contrast
            r = (r - 0.5) * contrast + 0.5;
            g = (g - 0.5) * contrast + 0.5;
            b = (b - 0.5) * contrast + 0.5;

            // Temperature & Tint
            r += tempR;
            b += tempB;
            g += tintG;

            // Saturation
            double lum = 0.2126 * r + 0.7152 * g + 0.0722 * b;
            r = lum + (r - lum) * saturation;
            g = lum + (g - lum) * saturation;
            b = lum + (b - lum) * saturation;

            px[0] = static_cast<uint8_t>(std::clamp(r * 255.0, 0.0, 255.0));
            px[1] = static_cast<uint8_t>(std::clamp(g * 255.0, 0.0, 255.0));
            px[2] = static_cast<uint8_t>(std::clamp(b * 255.0, 0.0, 255.0));
        }
    }
}

void PixelPipeline::applyChromaKey(uint8_t* rgba, int width, int height, int strideBytes,
                                   uint32_t keyColor, double similarity, double smoothness) {
    if (width <= 0 || height <= 0 || rgba == nullptr) return;

    const double keyR = ((keyColor >> 16) & 0xFF) / 255.0;
    const double keyG = ((keyColor >> 8) & 0xFF) / 255.0;
    const double keyB = (keyColor & 0xFF) / 255.0;

    const double sim = std::clamp(similarity, 0.0, 1.0);
    const double smooth = std::clamp(smoothness, 0.001, 1.0);
    const double maxDist = std::sqrt(3.0);

    for (int y = 0; y < height; ++y) {
        uint8_t* row = rgba + y * strideBytes;
        for (int x = 0; x < width; ++x) {
            uint8_t* px = row + x * 4;
            double r = px[0] / 255.0;
            double g = px[1] / 255.0;
            double b = px[2] / 255.0;

            double dR = r - keyR;
            double dG = g - keyG;
            double dB = b - keyB;
            double dist = std::sqrt(dR * dR + dG * dG + dB * dB) / maxDist;

            double alphaMult = 1.0;
            if (dist < sim) {
                alphaMult = 0.0;
            } else if (dist < sim + smooth) {
                alphaMult = (dist - sim) / smooth;
            }

            px[3] = static_cast<uint8_t>(std::clamp(px[3] * alphaMult, 0.0, 255.0));

            // Green spill suppression if key is green-dominant
            if (keyG > keyR && keyG > keyB && alphaMult < 0.9) {
                double maxRB = std::max(r, b);
                if (g > maxRB) {
                    px[1] = static_cast<uint8_t>(std::clamp(maxRB * 255.0, 0.0, 255.0));
                }
            }
        }
    }
}

void PixelPipeline::applyVignette(uint8_t* rgba, int width, int height, int strideBytes,
                                  double intensity, double radius, double softness) {
    if (width <= 0 || height <= 0 || rgba == nullptr || intensity <= 0.0) return;

    const double cx = width * 0.5;
    const double cy = height * 0.5;
    const double maxDist = std::sqrt(cx * cx + cy * cy);
    const double rOuter = std::clamp(radius, 0.1, 1.5);
    const double rInner = rOuter * (1.0 - std::clamp(softness, 0.0, 1.0));
    const double invRange = (rOuter > rInner) ? 1.0 / (rOuter - rInner) : 1.0;
    const double inten = std::clamp(intensity, 0.0, 1.0);

    for (int y = 0; y < height; ++y) {
        uint8_t* row = rgba + y * strideBytes;
        double dy = y - cy;
        for (int x = 0; x < width; ++x) {
            double dx = x - cx;
            double dist = std::sqrt(dx * dx + dy * dy) / maxDist;

            if (dist > rInner) {
                double f = std::clamp((dist - rInner) * invRange, 0.0, 1.0);
                double scale = 1.0 - f * inten;
                uint8_t* px = row + x * 4;
                px[0] = static_cast<uint8_t>(std::clamp(px[0] * scale, 0.0, 255.0));
                px[1] = static_cast<uint8_t>(std::clamp(px[1] * scale, 0.0, 255.0));
                px[2] = static_cast<uint8_t>(std::clamp(px[2] * scale, 0.0, 255.0));
            }
        }
    }
}

void PixelPipeline::applyBlur(uint8_t* rgba, int width, int height, int strideBytes,
                              int radius) {
    if (radius <= 0 || width <= 0 || height <= 0 || rgba == nullptr) return;
    radius = std::min(radius, 50);

    std::vector<uint8_t> tmp(height * strideBytes);
    const int bpp = 4;

    // Horizontal pass: rgba -> tmp
    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = rgba + y * strideBytes;
        uint8_t* dstRow = tmp.data() + y * strideBytes;

        int rSum = 0, gSum = 0, bSum = 0, aSum = 0;
        int count = 0;

        for (int x = -radius; x <= radius; ++x) {
            int cx = std::clamp(x, 0, width - 1);
            rSum += srcRow[cx * bpp + 0];
            gSum += srcRow[cx * bpp + 1];
            bSum += srcRow[cx * bpp + 2];
            aSum += srcRow[cx * bpp + 3];
            count++;
        }

        for (int x = 0; x < width; ++x) {
            dstRow[x * bpp + 0] = static_cast<uint8_t>(rSum / count);
            dstRow[x * bpp + 1] = static_cast<uint8_t>(gSum / count);
            dstRow[x * bpp + 2] = static_cast<uint8_t>(bSum / count);
            dstRow[x * bpp + 3] = static_cast<uint8_t>(aSum / count);

            int xRemove = std::clamp(x - radius, 0, width - 1);
            int xAdd = std::clamp(x + radius + 1, 0, width - 1);

            rSum += srcRow[xAdd * bpp + 0] - srcRow[xRemove * bpp + 0];
            gSum += srcRow[xAdd * bpp + 1] - srcRow[xRemove * bpp + 1];
            bSum += srcRow[xAdd * bpp + 2] - srcRow[xRemove * bpp + 2];
            aSum += srcRow[xAdd * bpp + 3] - srcRow[xRemove * bpp + 3];
        }
    }

    // Vertical pass: tmp -> rgba
    for (int x = 0; x < width; ++x) {
        int rSum = 0, gSum = 0, bSum = 0, aSum = 0;
        int count = 0;

        for (int y = -radius; y <= radius; ++y) {
            int cy = std::clamp(y, 0, height - 1);
            const uint8_t* p = tmp.data() + cy * strideBytes + x * bpp;
            rSum += p[0];
            gSum += p[1];
            bSum += p[2];
            aSum += p[3];
            count++;
        }

        for (int y = 0; y < height; ++y) {
            uint8_t* p = rgba + y * strideBytes + x * bpp;
            p[0] = static_cast<uint8_t>(rSum / count);
            p[1] = static_cast<uint8_t>(gSum / count);
            p[2] = static_cast<uint8_t>(bSum / count);
            p[3] = static_cast<uint8_t>(aSum / count);

            int yRemove = std::clamp(y - radius, 0, height - 1);
            int yAdd = std::clamp(y + radius + 1, 0, height - 1);

            const uint8_t* pRem = tmp.data() + yRemove * strideBytes + x * bpp;
            const uint8_t* pAdd = tmp.data() + yAdd * strideBytes + x * bpp;

            rSum += pAdd[0] - pRem[0];
            gSum += pAdd[1] - pRem[1];
            bSum += pAdd[2] - pRem[2];
            aSum += pAdd[3] - pRem[3];
        }
    }
}

void PixelPipeline::applySharpen(uint8_t* rgba, int width, int height, int strideBytes,
                                 double amount) {
    if (amount <= 0.0 || width <= 0 || height <= 0 || rgba == nullptr) return;
    std::vector<uint8_t> blurred(height * strideBytes);
    std::memcpy(blurred.data(), rgba, height * strideBytes);
    applyBlur(blurred.data(), width, height, strideBytes, 2);

    for (int y = 0; y < height; ++y) {
        uint8_t* orig = rgba + y * strideBytes;
        const uint8_t* blur = blurred.data() + y * strideBytes;
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < 3; ++c) {
                double o = orig[x * 4 + c];
                double b = blur[x * 4 + c];
                double val = o + amount * (o - b);
                orig[x * 4 + c] = static_cast<uint8_t>(std::clamp(val, 0.0, 255.0));
            }
        }
    }
}

BlendMode PixelPipeline::parseBlendMode(const std::string& name) {
    if (name == "screen") return BlendMode::Screen;
    if (name == "multiply") return BlendMode::Multiply;
    if (name == "overlay") return BlendMode::Overlay;
    if (name == "darken") return BlendMode::Darken;
    if (name == "lighten") return BlendMode::Lighten;
    if (name == "color_burn") return BlendMode::ColorBurn;
    if (name == "linear_burn") return BlendMode::LinearBurn;
    if (name == "color_dodge") return BlendMode::ColorDodge;
    if (name == "soft_light") return BlendMode::SoftLight;
    return BlendMode::Normal;
}

const char* PixelPipeline::blendModeName(BlendMode mode) {
    switch (mode) {
        case BlendMode::Screen: return "screen";
        case BlendMode::Multiply: return "multiply";
        case BlendMode::Overlay: return "overlay";
        case BlendMode::Darken: return "darken";
        case BlendMode::Lighten: return "lighten";
        case BlendMode::ColorBurn: return "color_burn";
        case BlendMode::LinearBurn: return "linear_burn";
        case BlendMode::ColorDodge: return "color_dodge";
        case BlendMode::SoftLight: return "soft_light";
        case BlendMode::Normal:
        default: return "normal";
    }
}

static inline double calcBlendOp(double fg, double bg, BlendMode mode) {
    switch (mode) {
        case BlendMode::Screen:
            return 1.0 - (1.0 - fg) * (1.0 - bg);
        case BlendMode::Multiply:
            return fg * bg;
        case BlendMode::Overlay:
            return (bg < 0.5) ? (2.0 * bg * fg) : (1.0 - 2.0 * (1.0 - bg) * (1.0 - fg));
        case BlendMode::Darken:
            return (std::min)(fg, bg);
        case BlendMode::Lighten:
            return (std::max)(fg, bg);
        case BlendMode::ColorBurn:
            return (fg < 1e-6) ? 0.0 : (std::max)(0.0, 1.0 - (1.0 - bg) / fg);
        case BlendMode::LinearBurn:
            return (std::max)(0.0, fg + bg - 1.0);
        case BlendMode::ColorDodge:
            return (fg > 1.0 - 1e-6) ? 1.0 : (std::min)(1.0, bg / (1.0 - fg));
        case BlendMode::SoftLight:
            return (fg < 0.5)
                ? (2.0 * bg * fg + bg * bg * (1.0 - 2.0 * fg))
                : (std::sqrt((std::max)(0.0, bg)) * (2.0 * fg - 1.0) + 2.0 * bg * (1.0 - fg));
        case BlendMode::Normal:
        default:
            return fg;
    }
}

void PixelPipeline::applyBlend(uint8_t* dstRgba, const uint8_t* srcRgba,
                               int width, int height, int dstStrideBytes, int srcStrideBytes,
                               BlendMode mode, double opacity) {
    if (dstRgba == nullptr || srcRgba == nullptr || width <= 0 || height <= 0) return;
    const double op = std::clamp(opacity, 0.0, 1.0);
    if (op <= 0.0) return;

    for (int y = 0; y < height; ++y) {
        uint8_t* dRow = dstRgba + y * dstStrideBytes;
        const uint8_t* sRow = srcRgba + y * srcStrideBytes;

        for (int x = 0; x < width; ++x) {
            uint8_t* dp = dRow + x * 4;
            const uint8_t* sp = sRow + x * 4;

            double fgA = (sp[3] / 255.0) * op;
            double bgA = dp[3] / 255.0;
            if (fgA <= 1e-6) continue;

            double fgR = sp[0] / 255.0;
            double fgG = sp[1] / 255.0;
            double fgB = sp[2] / 255.0;

            double bgR = dp[0] / 255.0;
            double bgG = dp[1] / 255.0;
            double bgB = dp[2] / 255.0;

            double bR = calcBlendOp(fgR, bgR, mode);
            double bG = calcBlendOp(fgG, bgG, mode);
            double bB = calcBlendOp(fgB, bgB, mode);

            // CapCut SoftLightBlendStraight formula
            double outA = fgA + bgA - fgA * bgA;
            if (outA > 1e-6) {
                double cR = fgR * fgA * (1.0 - bgA) + bgR * bgA * (1.0 - fgA) + fgA * bgA * bR;
                double cG = fgG * fgA * (1.0 - bgA) + bgG * bgA * (1.0 - fgA) + fgA * bgA * bG;
                double cB = fgB * fgA * (1.0 - bgA) + bgB * bgA * (1.0 - fgA) + fgA * bgA * bB;

                dp[0] = static_cast<uint8_t>(std::clamp(cR / outA * 255.0, 0.0, 255.0));
                dp[1] = static_cast<uint8_t>(std::clamp(cG / outA * 255.0, 0.0, 255.0));
                dp[2] = static_cast<uint8_t>(std::clamp(cB / outA * 255.0, 0.0, 255.0));
                dp[3] = static_cast<uint8_t>(std::clamp(outA * 255.0, 0.0, 255.0));
            }
        }
    }
}

void PixelPipeline::applyHighlightsShadows(uint8_t* rgba, int width, int height, int strideBytes,
                                           double highlights, double shadows) {
    if (width <= 0 || height <= 0 || rgba == nullptr) return;
    if (std::abs(highlights) < 0.001 && std::abs(shadows) < 0.001) return;

    // CapCut combine_adjust polynomial parameter curves from SeekModeScript.lua:
    // getHighlightParam(p) = 1.0 + 0.503*p + 0.183*p^2 + 0.147*p^3 + 0.067*p^4
    // getShadowParam(p)    = 1.0 - 0.503*p + 0.183*p^2 - 0.147*p^3 + 0.067*p^4
    const double hp = highlights;
    const double hp2 = hp * hp;
    const double hp3 = hp2 * hp;
    const double hp4 = hp3 * hp;
    const double pHigh = 1.0 + 0.503 * hp + 0.183 * hp2 + 0.147 * hp3 + 0.067 * hp4;

    const double sp = shadows;
    const double sp2 = sp * sp;
    const double sp3 = sp2 * sp;
    const double sp4 = sp3 * sp;
    const double pShad = 1.0 - 0.503 * sp + 0.183 * sp2 - 0.147 * sp3 + 0.067 * sp4;

    for (int y = 0; y < height; ++y) {
        uint8_t* row = rgba + y * strideBytes;
        for (int x = 0; x < width; ++x) {
            uint8_t* px = row + x * 4;
            for (int c = 0; c < 3; ++c) {
                double val = px[c] / 255.0;

                // Adjust shadow: pow(c, p) + (p - 1.0) * (c^2 - c^3)
                if (std::abs(shadows) >= 0.001) {
                    double c2 = val * val;
                    double c3 = c2 * val;
                    val = std::pow(val, pShad) + (pShad - 1.0) * (c2 - c3);
                }

                // Adjust highlight: 1.0 - pow(1 - c, p) - (p - 1.0) * (t^2 - t^3)
                if (std::abs(highlights) >= 0.001) {
                    val = std::clamp(val, 0.0, 1.0);
                    double t = 1.0 - val;
                    double t2 = t * t;
                    double t3 = t2 * t;
                    val = 1.0 - std::pow(t, pHigh) - (pHigh - 1.0) * (t2 - t3);
                }

                px[c] = static_cast<uint8_t>(std::clamp(val * 255.0, 0.0, 255.0));
            }
        }
    }
}

static inline double capcutWheelMath(double srcColor, double lift, double gamma, double gain, double offset) {
    lift = (std::min)(lift, 0.4995);
    double liftX = lift / (lift - 0.5);
    gain = (std::max)(gain, 0.001);
    double gainX = 1.0 / gain;
    liftX = (std::min)(liftX, gainX - 0.0005);

    double resColor = (srcColor - offset - liftX) / (gainX - liftX) + offset;
    if (resColor >= 0.0) {
        resColor = std::pow(resColor, gamma);
    } else {
        resColor = -std::pow(-resColor, gamma);
    }
    return resColor;
}

void PixelPipeline::applyColorWheels(uint8_t* rgba, int width, int height, int strideBytes,
                                     const ColorWheelParams& params) {
    if (width <= 0 || height <= 0 || rgba == nullptr) return;

    const double offsetGray = 0.2126 * params.offsetR + 0.7152 * params.offsetG + 0.0722 * params.offsetB;

    for (int y = 0; y < height; ++y) {
        uint8_t* row = rgba + y * strideBytes;
        for (int x = 0; x < width; ++x) {
            uint8_t* px = row + x * 4;

            double r = px[0] / 255.0 + params.offsetR;
            double g = px[1] / 255.0 + params.offsetG;
            double b = px[2] / 255.0 + params.offsetB;

            double baseY = 0.2126 * r + 0.7152 * g + 0.0722 * b;
            baseY = capcutWheelMath(baseY, params.liftY, params.gammaY, params.gainY, offsetGray);

            r = capcutWheelMath(r, params.liftR, params.gammaR, params.gainR, params.offsetR);
            g = capcutWheelMath(g, params.liftG, params.gammaG, params.gainG, params.offsetG);
            b = capcutWheelMath(b, params.liftB, params.gammaB, params.gainB, params.offsetB);

            // Preserve luminance via LumaMix
            double resY = 0.2126 * r + 0.7152 * g + 0.0722 * b;
            double yDelta = (baseY - resY) * params.lumaMix;
            r += yDelta;
            g += yDelta;
            b += yDelta;

            px[0] = static_cast<uint8_t>(std::clamp(r * 255.0, 0.0, 255.0));
            px[1] = static_cast<uint8_t>(std::clamp(g * 255.0, 0.0, 255.0));
            px[2] = static_cast<uint8_t>(std::clamp(b * 255.0, 0.0, 255.0));
        }
    }
}

double PixelPipeline::evalCubicBezier(double p0, double p1, double p2, double p3, double t) {
    double it = 1.0 - t;
    return it * it * it * p0 + 3.0 * it * it * t * p1 + 3.0 * it * t * t * p2 + t * t * t * p3;
}

static void buildCurveLut(const std::vector<CurvePoint>& points, std::vector<uint8_t>& lut) {
    lut.resize(256);
    if (points.empty()) {
        for (int i = 0; i < 256; ++i) lut[i] = static_cast<uint8_t>(i);
        return;
    }
    if (points.size() == 2) {
        // Linear between points[0] and points[1]
        for (int i = 0; i < 256; ++i) {
            double x = i / 255.0;
            double y = points[0].y + (points[1].y - points[0].y) *
                ((x - points[0].x) / (std::max)(1e-6, points[1].x - points[0].x));
            lut[i] = static_cast<uint8_t>(std::clamp(y * 255.0, 0.0, 255.0));
        }
        return;
    }

    // 4-point cubic Bezier
    CurvePoint p0 = points[0];
    CurvePoint p1 = points.size() > 1 ? points[1] : p0;
    CurvePoint p2 = points.size() > 2 ? points[2] : p1;
    CurvePoint p3 = points.back();

    for (int i = 0; i < 256; ++i) {
        double targetX = i / 255.0;
        // Binary search for t: 20 iterations matching CapCut's computeT2 in curveLut.frag
        double startT = 0.0, endT = 1.0, halfT = 0.5;
        for (int it = 0; it < 20; ++it) {
            double curX = PixelPipeline::evalCubicBezier(p0.x, p1.x, p2.x, p3.x, halfT);
            if (curX < targetX) startT = halfT;
            else endT = halfT;
            halfT = (startT + endT) * 0.5;
        }
        double y = PixelPipeline::evalCubicBezier(p0.y, p1.y, p2.y, p3.y, halfT);
        lut[i] = static_cast<uint8_t>(std::clamp(y * 255.0, 0.0, 255.0));
    }
}

void PixelPipeline::applyColorCurves(uint8_t* rgba, int width, int height, int strideBytes,
                                     const std::vector<CurvePoint>& curveY,
                                     const std::vector<CurvePoint>& curveR,
                                     const std::vector<CurvePoint>& curveG,
                                     const std::vector<CurvePoint>& curveB) {
    if (width <= 0 || height <= 0 || rgba == nullptr) return;

    std::vector<uint8_t> lutY, lutR, lutG, lutB;
    bool hasY = !curveY.empty();
    bool hasR = !curveR.empty();
    bool hasG = !curveG.empty();
    bool hasB = !curveB.empty();
    if (!hasY && !hasR && !hasG && !hasB) return;

    if (hasY) buildCurveLut(curveY, lutY);
    if (hasR) buildCurveLut(curveR, lutR);
    if (hasG) buildCurveLut(curveG, lutG);
    if (hasB) buildCurveLut(curveB, lutB);

    for (int y = 0; y < height; ++y) {
        uint8_t* row = rgba + y * strideBytes;
        for (int x = 0; x < width; ++x) {
            uint8_t* px = row + x * 4;

            if (hasR) px[0] = lutR[px[0]];
            if (hasG) px[1] = lutG[px[1]];
            if (hasB) px[2] = lutB[px[2]];

            if (hasY) {
                double luma = 0.2126 * px[0] + 0.7152 * px[1] + 0.0722 * px[2];
                int lumaIdx = std::clamp(static_cast<int>(luma), 0, 255);
                double newLuma = lutY[lumaIdx];
                double diff = newLuma - luma;
                px[0] = static_cast<uint8_t>(std::clamp(px[0] + diff, 0.0, 255.0));
                px[1] = static_cast<uint8_t>(std::clamp(px[1] + diff, 0.0, 255.0));
                px[2] = static_cast<uint8_t>(std::clamp(px[2] + diff, 0.0, 255.0));
            }
        }
    }
}

void PixelPipeline::applyLetterbox(uint8_t* rgba, int width, int height, int strideBytes,
                                   double barHeight, double feather) {
    if (width <= 0 || height <= 0 || rgba == nullptr || barHeight <= 0.0) return;

    barHeight = std::clamp(barHeight, 0.0, 0.5);
    feather = std::clamp(feather, 0.0, 0.2);

    const double topBar = height * barHeight;
    const double bottomBar = height * (1.0 - barHeight);
    const double featherPx = height * feather;

    for (int y = 0; y < height; ++y) {
        double factor = 1.0;
        if (y < topBar) {
            if (featherPx > 1e-4 && y >= topBar - featherPx) {
                factor = (y - (topBar - featherPx)) / featherPx;
            } else {
                factor = 0.0;
            }
        } else if (y >= bottomBar) {
            if (featherPx > 1e-4 && y <= bottomBar + featherPx) {
                factor = (bottomBar + featherPx - y) / featherPx;
            } else {
                factor = 0.0;
            }
        }
        factor = std::clamp(factor, 0.0, 1.0);
        if (factor >= 0.9999) continue;

        uint8_t* row = rgba + y * strideBytes;
        for (int x = 0; x < width; ++x) {
            uint8_t* px = row + x * 4;
            px[0] = static_cast<uint8_t>(std::clamp(px[0] * factor, 0.0, 255.0));
            px[1] = static_cast<uint8_t>(std::clamp(px[1] * factor, 0.0, 255.0));
            px[2] = static_cast<uint8_t>(std::clamp(px[2] * factor, 0.0, 255.0));
        }
    }
}

void PixelPipeline::applyFilmGrain(uint8_t* rgba, int width, int height, int strideBytes,
                                   double intensity, double size, bool colored) {
    if (width <= 0 || height <= 0 || rgba == nullptr || intensity <= 0.0) return;

    intensity = std::clamp(intensity, 0.0, 1.0);
    const int grainScale = (std::max)(1, static_cast<int>(std::clamp(size, 1.0, 4.0)));

    auto hashNoise = [](int x, int y, int channel) -> double {
        uint32_t n = static_cast<uint32_t>(x) * 374761393u
                   + static_cast<uint32_t>(y) * 668265263u
                   + static_cast<uint32_t>(channel) * 3628273u;
        n = (n ^ (n >> 13)) * 1274126177u;
        return ((n ^ (n >> 16)) & 0xFFF) / 4095.0 - 0.5; // [-0.5, 0.5]
    };

    for (int y = 0; y < height; ++y) {
        uint8_t* row = rgba + y * strideBytes;
        int gy = y / grainScale;
        for (int x = 0; x < width; ++x) {
            int gx = x / grainScale;
            uint8_t* px = row + x * 4;

            double lum = (0.2126 * px[0] + 0.7152 * px[1] + 0.0722 * px[2]) / 255.0;
            // Film grain organic envelope: maximum in midtones/shadows, reduced in extreme highlights
            double env = 1.0 - 0.85 * (lum * lum);

            if (!colored) {
                double n = hashNoise(gx, gy, 0) * intensity * env * 255.0;
                px[0] = static_cast<uint8_t>(std::clamp(px[0] + n, 0.0, 255.0));
                px[1] = static_cast<uint8_t>(std::clamp(px[1] + n, 0.0, 255.0));
                px[2] = static_cast<uint8_t>(std::clamp(px[2] + n, 0.0, 255.0));
            } else {
                double nr = hashNoise(gx, gy, 1) * intensity * env * 255.0;
                double ng = hashNoise(gx, gy, 2) * intensity * env * 255.0;
                double nb = hashNoise(gx, gy, 3) * intensity * env * 255.0;
                px[0] = static_cast<uint8_t>(std::clamp(px[0] + nr, 0.0, 255.0));
                px[1] = static_cast<uint8_t>(std::clamp(px[1] + ng, 0.0, 255.0));
                px[2] = static_cast<uint8_t>(std::clamp(px[2] + nb, 0.0, 255.0));
            }
        }
    }
}

void PixelPipeline::applyChromaticAberration(uint8_t* rgba, int width, int height, int strideBytes,
                                             double shiftX, double shiftY) {
    if (width <= 0 || height <= 0 || rgba == nullptr) return;
    if (std::abs(shiftX) < 0.001 && std::abs(shiftY) < 0.001) return;

    std::vector<uint8_t> src(height * strideBytes);
    std::memcpy(src.data(), rgba, height * strideBytes);

    int sx = static_cast<int>(std::round(shiftX));
    int sy = static_cast<int>(std::round(shiftY));

    for (int y = 0; y < height; ++y) {
        uint8_t* dstRow = rgba + y * strideBytes;

        int ry = std::clamp(y - sy, 0, height - 1);
        int by = std::clamp(y + sy, 0, height - 1);
        const uint8_t* srcRowR = src.data() + ry * strideBytes;
        const uint8_t* srcRowB = src.data() + by * strideBytes;

        for (int x = 0; x < width; ++x) {
            int rx = std::clamp(x - sx, 0, width - 1);
            int bx = std::clamp(x + sx, 0, width - 1);

            dstRow[x * 4 + 0] = srcRowR[rx * 4 + 0]; // Shifted Red
            // Green remains unshifted
            dstRow[x * 4 + 2] = srcRowB[bx * 4 + 2]; // Counter-shifted Blue
        }
    }
}

void PixelPipeline::applyBloom(uint8_t* rgba, int width, int height, int strideBytes,
                               double threshold, int radius, double intensity) {
    if (width <= 0 || height <= 0 || rgba == nullptr || intensity <= 0.0 || radius <= 0) return;

    threshold = std::clamp(threshold, 0.1, 0.99);
    radius = std::clamp(radius, 1, 40);

    std::vector<uint8_t> highlights(height * strideBytes, 0);

    // 1. Isolate specular highlights above threshold
    for (int y = 0; y < height; ++y) {
        const uint8_t* src = rgba + y * strideBytes;
        uint8_t* dst = highlights.data() + y * strideBytes;
        for (int x = 0; x < width; ++x) {
            double lum = (0.2126 * src[x * 4 + 0] + 0.7152 * src[x * 4 + 1] + 0.0722 * src[x * 4 + 2]) / 255.0;
            if (lum > threshold) {
                double f = (lum - threshold) / (1.0 - threshold);
                dst[x * 4 + 0] = static_cast<uint8_t>(src[x * 4 + 0] * f);
                dst[x * 4 + 1] = static_cast<uint8_t>(src[x * 4 + 1] * f);
                dst[x * 4 + 2] = static_cast<uint8_t>(src[x * 4 + 2] * f);
                dst[x * 4 + 3] = 255;
            }
        }
    }

    // 2. Diffuse/blur highlights
    applyBlur(highlights.data(), width, height, strideBytes, radius);

    // 3. Screen/add blurred glow back onto base image
    for (int y = 0; y < height; ++y) {
        uint8_t* base = rgba + y * strideBytes;
        const uint8_t* glow = highlights.data() + y * strideBytes;
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < 3; ++c) {
                double b = base[x * 4 + c] / 255.0;
                double g = (glow[x * 4 + c] / 255.0) * intensity;
                double val = 1.0 - (1.0 - b) * (1.0 - std::clamp(g, 0.0, 1.0));
                base[x * 4 + c] = static_cast<uint8_t>(std::clamp(val * 255.0, 0.0, 255.0));
            }
        }
    }
}

void PixelPipeline::applySplitToning(uint8_t* rgba, int width, int height, int strideBytes,
                                     double shadowTeal, double highlightOrange, double balance) {
    if (width <= 0 || height <= 0 || rgba == nullptr) return;
    if (std::abs(shadowTeal) < 0.001 && std::abs(highlightOrange) < 0.001) return;

    shadowTeal = std::clamp(shadowTeal, 0.0, 1.0);
    highlightOrange = std::clamp(highlightOrange, 0.0, 1.0);
    const double mid = std::clamp(0.5 + balance * 0.25, 0.1, 0.9);

    for (int y = 0; y < height; ++y) {
        uint8_t* row = rgba + y * strideBytes;
        for (int x = 0; x < width; ++x) {
            uint8_t* px = row + x * 4;
            double r = px[0] / 255.0;
            double g = px[1] / 255.0;
            double b = px[2] / 255.0;
            double lum = 0.2126 * r + 0.7152 * g + 0.0722 * b;

            if (lum < mid) {
                // Shadow Teal / Cyan push
                double w = ((mid - lum) / mid) * shadowTeal;
                r -= w * 0.25;
                g += w * 0.15;
                b += w * 0.40;
            } else {
                // Highlight Orange / Amber push
                double w = ((lum - mid) / (1.0 - mid)) * highlightOrange;
                r += w * 0.35;
                g += w * 0.15;
                b -= w * 0.25;
            }

            px[0] = static_cast<uint8_t>(std::clamp(r * 255.0, 0.0, 255.0));
            px[1] = static_cast<uint8_t>(std::clamp(g * 255.0, 0.0, 255.0));
            px[2] = static_cast<uint8_t>(std::clamp(b * 255.0, 0.0, 255.0));
        }
    }
}

void PixelPipeline::applyRetroVhs(uint8_t* rgba, int width, int height, int strideBytes,
                                  double scanlines, double colorBleed, double noise) {
    if (width <= 0 || height <= 0 || rgba == nullptr) return;

    scanlines = std::clamp(scanlines, 0.0, 1.0);
    colorBleed = std::clamp(colorBleed, 0.0, 20.0);
    noise = std::clamp(noise, 0.0, 1.0);

    std::vector<uint8_t> src;
    int bleedPx = static_cast<int>(std::round(colorBleed));
    if (bleedPx > 0) {
        src.resize(height * strideBytes);
        std::memcpy(src.data(), rgba, height * strideBytes);
    }

    for (int y = 0; y < height; ++y) {
        uint8_t* row = rgba + y * strideBytes;
        const uint8_t* srcRow = bleedPx > 0 ? (src.data() + y * strideBytes) : nullptr;

        // Scanline intensity: darkens alternating rows
        double lineMod = 1.0 - scanlines * (0.35 * (y % 3 == 0 ? 1.0 : 0.0));

        for (int x = 0; x < width; ++x) {
            uint8_t* px = row + x * 4;

            // Red color bleed
            if (srcRow != nullptr && bleedPx > 0) {
                int bx = std::clamp(x - bleedPx, 0, width - 1);
                px[0] = srcRow[bx * 4 + 0];
            }

            // Scanline modulation
            if (scanlines > 0.0) {
                px[0] = static_cast<uint8_t>(std::clamp(px[0] * lineMod, 0.0, 255.0));
                px[1] = static_cast<uint8_t>(std::clamp(px[1] * lineMod, 0.0, 255.0));
                px[2] = static_cast<uint8_t>(std::clamp(px[2] * lineMod, 0.0, 255.0));
            }

            // Subtle tape noise
            if (noise > 0.0) {
                uint32_t n = static_cast<uint32_t>(x * 2654435761u ^ y * 805459861u);
                double val = ((n & 0xFF) / 255.0 - 0.5) * noise * 64.0;
                px[0] = static_cast<uint8_t>(std::clamp(px[0] + val, 0.0, 255.0));
                px[1] = static_cast<uint8_t>(std::clamp(px[1] + val, 0.0, 255.0));
                px[2] = static_cast<uint8_t>(std::clamp(px[2] + val, 0.0, 255.0));
            }
        }
    }
}

void PixelPipeline::applyPosterize(uint8_t* rgba, int width, int height, int strideBytes,
                                   int levels) {
    if (width <= 0 || height <= 0 || rgba == nullptr) return;
    levels = std::clamp(levels, 2, 64);
    const double step = 255.0 / (levels - 1);

    for (int y = 0; y < height; ++y) {
        uint8_t* row = rgba + y * strideBytes;
        for (int x = 0; x < width; ++x) {
            uint8_t* px = row + x * 4;
            for (int c = 0; c < 3; ++c) {
                double val = std::round(px[c] / step) * step;
                px[c] = static_cast<uint8_t>(std::clamp(val, 0.0, 255.0));
            }
        }
    }
}

void PixelPipeline::applyInvert(uint8_t* rgba, int width, int height, int strideBytes,
                               double intensity) {
    if (width <= 0 || height <= 0 || rgba == nullptr || intensity <= 0.0) return;
    const double inten = std::clamp(intensity, 0.0, 1.0);

    for (int y = 0; y < height; ++y) {
        uint8_t* row = rgba + y * strideBytes;
        for (int x = 0; x < width; ++x) {
            uint8_t* px = row + x * 4;
            for (int c = 0; c < 3; ++c) {
                double orig = px[c];
                double inv = 255.0 - orig;
                px[c] = static_cast<uint8_t>(std::clamp(orig * (1.0 - inten) + inv * inten, 0.0, 255.0));
            }
        }
    }
}

void PixelPipeline::applyEdgeDetect(uint8_t* rgba, int width, int height, int strideBytes,
                                    double intensity, bool invert) {
    if (width <= 0 || height <= 0 || rgba == nullptr || intensity <= 0.0) return;

    std::vector<uint8_t> gray(width * height);
    for (int y = 0; y < height; ++y) {
        const uint8_t* row = rgba + y * strideBytes;
        for (int x = 0; x < width; ++x) {
            gray[y * width + x] = static_cast<uint8_t>(
                0.2126 * row[x * 4 + 0] + 0.7152 * row[x * 4 + 1] + 0.0722 * row[x * 4 + 2]);
        }
    }

    const double inten = std::clamp(intensity, 0.0, 4.0);

    for (int y = 1; y < height - 1; ++y) {
        uint8_t* row = rgba + y * strideBytes;
        for (int x = 1; x < width - 1; ++x) {
            // Sobel 3x3 kernels
            int gx = -gray[(y - 1) * width + (x - 1)] + gray[(y - 1) * width + (x + 1)]
                     - 2 * gray[y * width + (x - 1)] + 2 * gray[y * width + (x + 1)]
                     - gray[(y + 1) * width + (x - 1)] + gray[(y + 1) * width + (x + 1)];

            int gy = -gray[(y - 1) * width + (x - 1)] - 2 * gray[(y - 1) * width + x] - gray[(y - 1) * width + (x + 1)]
                     + gray[(y + 1) * width + (x - 1)] + 2 * gray[(y + 1) * width + x] + gray[(y + 1) * width + (x + 1)];

            double mag = std::sqrt(gx * gx + gy * gy) * inten;
            double outVal = invert ? (255.0 - mag) : mag;
            uint8_t b = static_cast<uint8_t>(std::clamp(outVal, 0.0, 255.0));

            row[x * 4 + 0] = b;
            row[x * 4 + 1] = b;
            row[x * 4 + 2] = b;
        }
    }
}

void PixelPipeline::applyMirror(uint8_t* rgba, int width, int height, int strideBytes,
                               int mode) {
    if (width <= 0 || height <= 0 || rgba == nullptr) return;

    if (mode == 0) {
        // Horizontal Flip
        for (int y = 0; y < height; ++y) {
            uint8_t* row = rgba + y * strideBytes;
            for (int x = 0; x < width / 2; ++x) {
                int rx = width - 1 - x;
                for (int c = 0; c < 4; ++c) {
                    std::swap(row[x * 4 + c], row[rx * 4 + c]);
                }
            }
        }
    } else if (mode == 1) {
        // Vertical Flip
        std::vector<uint8_t> tmpRow(strideBytes);
        for (int y = 0; y < height / 2; ++y) {
            int ry = height - 1 - y;
            uint8_t* rowA = rgba + y * strideBytes;
            uint8_t* rowB = rgba + ry * strideBytes;
            std::memcpy(tmpRow.data(), rowA, strideBytes);
            std::memcpy(rowA, rowB, strideBytes);
            std::memcpy(rowB, tmpRow.data(), strideBytes);
        }
    } else if (mode == 2) {
        // Center Mirror Horizontal (Left to Right)
        for (int y = 0; y < height; ++y) {
            uint8_t* row = rgba + y * strideBytes;
            for (int x = width / 2; x < width; ++x) {
                int srcX = width - 1 - x;
                for (int c = 0; c < 4; ++c) {
                    row[x * 4 + c] = row[srcX * 4 + c];
                }
            }
        }
    } else if (mode == 3) {
        // Center Mirror Vertical (Top to Bottom)
        for (int y = height / 2; y < height; ++y) {
            int srcY = height - 1 - y;
            const uint8_t* srcRow = rgba + srcY * strideBytes;
            uint8_t* dstRow = rgba + y * strideBytes;
            std::memcpy(dstRow, srcRow, strideBytes);
        }
    } else if (mode == 4) {
        // Quad Kaleidoscope
        for (int y = 0; y < height; ++y) {
            int srcY = y < height / 2 ? y : (height - 1 - y);
            const uint8_t* srcRow = rgba + srcY * strideBytes;
            uint8_t* dstRow = rgba + y * strideBytes;
            for (int x = 0; x < width; ++x) {
                int srcX = x < width / 2 ? x : (width - 1 - x);
                for (int c = 0; c < 4; ++c) {
                    dstRow[x * 4 + c] = srcRow[srcX * 4 + c];
                }
            }
        }
    }
}

void PixelPipeline::applyEffects(uint8_t* rgba, int width, int height, int strideBytes,
                                 const std::vector<core::Effect>& effects) {
    if (rgba == nullptr || width <= 0 || height <= 0 || effects.empty()) return;

    for (const auto& eff : effects) {
        if (!eff.enabled) continue;

        if (eff.type == "color_adjust") {
            const auto getP = [&](const std::string& key, double def) {
                auto it = eff.params.find(key);
                return it != eff.params.end() ? it->second : def;
            };
            applyColorAdjust(rgba, width, height, strideBytes,
                             getP("brightness", 0.0),
                             getP("contrast", 1.0),
                             getP("saturation", 1.0),
                             getP("temperature", 0.0),
                             getP("tint", 0.0));
        } else if (eff.type == "highlights_shadows") {
            const auto getP = [&](const std::string& key, double def) {
                auto it = eff.params.find(key);
                return it != eff.params.end() ? it->second : def;
            };
            applyHighlightsShadows(rgba, width, height, strideBytes,
                                  getP("highlights", 0.0),
                                  getP("shadows", 0.0));
        } else if (eff.type == "color_wheels") {
            const auto getP = [&](const std::string& key, double def) {
                auto it = eff.params.find(key);
                return it != eff.params.end() ? it->second : def;
            };
            ColorWheelParams p;
            p.liftR = getP("liftR", 0.0); p.liftG = getP("liftG", 0.0); p.liftB = getP("liftB", 0.0); p.liftY = getP("liftY", 0.0);
            p.gammaR = getP("gammaR", 1.0); p.gammaG = getP("gammaG", 1.0); p.gammaB = getP("gammaB", 1.0); p.gammaY = getP("gammaY", 1.0);
            p.gainR = getP("gainR", 1.0); p.gainG = getP("gainG", 1.0); p.gainB = getP("gainB", 1.0); p.gainY = getP("gainY", 1.0);
            p.offsetR = getP("offsetR", 0.0); p.offsetG = getP("offsetG", 0.0); p.offsetB = getP("offsetB", 0.0);
            p.lumaMix = getP("lumaMix", 1.0);
            applyColorWheels(rgba, width, height, strideBytes, p);
        } else if (eff.type == "vignette") {
            const auto getP = [&](const std::string& key, double def) {
                auto it = eff.params.find(key);
                return it != eff.params.end() ? it->second : def;
            };
            applyVignette(rgba, width, height, strideBytes,
                          getP("intensity", 0.5),
                          getP("radius", 0.7),
                          getP("softness", 0.5));
        } else if (eff.type == "blur") {
            auto it = eff.params.find("radius");
            double r = it != eff.params.end() ? it->second : 10.0;
            applyBlur(rgba, width, height, strideBytes, static_cast<int>(r));
        } else if (eff.type == "sharpen") {
            auto it = eff.params.find("amount");
            double a = it != eff.params.end() ? it->second : 0.5;
            applySharpen(rgba, width, height, strideBytes, a);
        } else if (eff.type == "chroma_key") {
            const auto getP = [&](const std::string& key, double def) {
                auto it = eff.params.find(key);
                return it != eff.params.end() ? it->second : def;
            };
            uint32_t keyRgb = 0x0000FF00;
            auto itStr = eff.strParams.find("color");
            if (itStr != eff.strParams.end()) {
                keyRgb = parseHexColor(itStr->second, keyRgb);
            }
            applyChromaKey(rgba, width, height, strideBytes,
                           keyRgb,
                           getP("similarity", 0.3),
                           getP("smoothness", 0.1));
        } else if (eff.type == "letterbox") {
            const auto getP = [&](const std::string& key, double def) {
                auto it = eff.params.find(key);
                return it != eff.params.end() ? it->second : def;
            };
            applyLetterbox(rgba, width, height, strideBytes,
                           getP("barHeight", 0.12),
                           getP("feather", 0.0));
        } else if (eff.type == "film_grain") {
            const auto getP = [&](const std::string& key, double def) {
                auto it = eff.params.find(key);
                return it != eff.params.end() ? it->second : def;
            };
            applyFilmGrain(rgba, width, height, strideBytes,
                           getP("intensity", 0.25),
                           getP("size", 1.0),
                           getP("colored", 0.0) >= 0.5);
        } else if (eff.type == "chromatic_aberration") {
            const auto getP = [&](const std::string& key, double def) {
                auto it = eff.params.find(key);
                return it != eff.params.end() ? it->second : def;
            };
            applyChromaticAberration(rgba, width, height, strideBytes,
                                     getP("shiftX", 5.0),
                                     getP("shiftY", 0.0));
        } else if (eff.type == "bloom") {
            const auto getP = [&](const std::string& key, double def) {
                auto it = eff.params.find(key);
                return it != eff.params.end() ? it->second : def;
            };
            applyBloom(rgba, width, height, strideBytes,
                       getP("threshold", 0.65),
                       static_cast<int>(getP("radius", 12.0)),
                       getP("intensity", 0.7));
        } else if (eff.type == "split_toning") {
            const auto getP = [&](const std::string& key, double def) {
                auto it = eff.params.find(key);
                return it != eff.params.end() ? it->second : def;
            };
            applySplitToning(rgba, width, height, strideBytes,
                             getP("shadowTeal", 0.4),
                             getP("highlightOrange", 0.4),
                             getP("balance", 0.0));
        } else if (eff.type == "retro_vhs") {
            const auto getP = [&](const std::string& key, double def) {
                auto it = eff.params.find(key);
                return it != eff.params.end() ? it->second : def;
            };
            applyRetroVhs(rgba, width, height, strideBytes,
                          getP("scanlines", 0.35),
                          getP("colorBleed", 3.0),
                          getP("noise", 0.15));
        } else if (eff.type == "posterize") {
            auto it = eff.params.find("levels");
            double lvl = it != eff.params.end() ? it->second : 6.0;
            applyPosterize(rgba, width, height, strideBytes, static_cast<int>(lvl));
        } else if (eff.type == "invert") {
            auto it = eff.params.find("intensity");
            double inten = it != eff.params.end() ? it->second : 1.0;
            applyInvert(rgba, width, height, strideBytes, inten);
        } else if (eff.type == "edge_detect") {
            const auto getP = [&](const std::string& key, double def) {
                auto it = eff.params.find(key);
                return it != eff.params.end() ? it->second : def;
            };
            applyEdgeDetect(rgba, width, height, strideBytes,
                            getP("intensity", 1.0),
                            getP("invert", 0.0) >= 0.5);
        } else if (eff.type == "mirror") {
            auto it = eff.params.find("mode");
            double m = it != eff.params.end() ? it->second : 0.0;
            applyMirror(rgba, width, height, strideBytes, static_cast<int>(m));
        }
    }
}

} // namespace editor::effects
