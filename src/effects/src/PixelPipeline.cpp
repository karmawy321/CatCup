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
        }
    }
}

} // namespace editor::effects
