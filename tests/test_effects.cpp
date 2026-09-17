#include "TestHarness.hpp"

#include "effects/EffectSchema.hpp"
#include "effects/PixelPipeline.hpp"

#include <vector>
#include <cstdint>

using namespace editor;

TEST_CASE("effects: registry contains all core effects") {
    const auto& reg = effects::EffectRegistry::defaults();
    auto all = reg.all();
    CHECK(all.size() >= 5);

    auto colorAdj = reg.find("color_adjust");
    CHECK(colorAdj != nullptr);
    CHECK_EQ(colorAdj->type, std::string("color_adjust"));

    auto vignette = reg.find("vignette");
    CHECK(vignette != nullptr);

    auto blur = reg.find("blur");
    CHECK(blur != nullptr);

    auto sharpen = reg.find("sharpen");
    CHECK(sharpen != nullptr);

    auto chroma = reg.find("chroma_key");
    CHECK(chroma != nullptr);
}

TEST_CASE("effects: color adjust brightness, contrast, and saturation") {
    // 2x2 image: [Red, Green, Blue, Mid-gray]
    // RGBA format: R, G, B, A
    std::vector<uint8_t> pixels = {
        255, 0, 0, 255,      // Red
        0, 255, 0, 255,      // Green
        0, 0, 255, 255,      // Blue
        128, 128, 128, 255   // Mid-gray
    };

    // 1. Identity color adjust: no change
    std::vector<uint8_t> test = pixels;
    effects::PixelPipeline::applyColorAdjust(test.data(), 2, 2, 8, 0.0, 1.0, 1.0, 0.0, 0.0);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        CHECK_EQ(test[i], pixels[i]);
    }

    // 2. Saturation 0: should convert to grayscale (R == G == B)
    test = pixels;
    effects::PixelPipeline::applyColorAdjust(test.data(), 2, 2, 8, 0.0, 1.0, 0.0, 0.0, 0.0);
    // For pure red (255, 0, 0), Rec.709 lum = 0.2126 * 255 ~= 54
    CHECK_EQ(test[0], test[1]);
    CHECK_EQ(test[1], test[2]);
    CHECK(test[0] > 40 && test[0] < 70);

    // 3. Brightness +0.2: mid-gray should increase
    test = pixels;
    effects::PixelPipeline::applyColorAdjust(test.data(), 2, 2, 8, 0.2, 1.0, 1.0, 0.0, 0.0);
    // pixel 3 was 128 -> should be higher
    CHECK(test[12] > 128);
}

TEST_CASE("effects: chroma key makes keyed color transparent") {
    // 2x2 image:
    // [0] = Pure Green (key)
    // [1] = Red (different)
    // [2] = Blue (different)
    // [3] = Dark Green
    std::vector<uint8_t> pixels = {
        0, 255, 0, 255,      // Pure green
        255, 0, 0, 255,      // Pure red
        0, 0, 255, 255,      // Pure blue
        0, 120, 0, 255       // Dark green
    };

    // Key pure green (#00FF00) with similarity 0.4, smoothness 0.1
    const uint32_t greenKey = effects::PixelPipeline::parseHexColor("#00FF00");
    effects::PixelPipeline::applyChromaKey(pixels.data(), 2, 2, 8, greenKey, 0.4, 0.1);

    // Pure green pixel alpha should now be 0
    CHECK_EQ(pixels[3], 0);

    // Pure red pixel alpha should remain 255
    CHECK_EQ(pixels[7], 255);

    // Pure blue pixel alpha should remain 255
    CHECK_EQ(pixels[11], 255);
}

TEST_CASE("effects: vignette darkens corners more than center") {
    int w = 10;
    int h = 10;
    int stride = w * 4;
    std::vector<uint8_t> pixels(static_cast<std::size_t>(w * h * 4), 200);
    // Make alpha 255
    for (int i = 0; i < w * h; ++i) {
        pixels[static_cast<std::size_t>(i * 4 + 3)] = 255;
    }

    effects::PixelPipeline::applyVignette(pixels.data(), w, h, stride, 1.0, 0.5, 0.2);

    // Center pixel: (5, 5)
    std::size_t centerIdx = static_cast<std::size_t>((5 * w + 5) * 4);
    // Corner pixel: (0, 0)
    std::size_t cornerIdx = 0;

    // Corner should be significantly darker than center
    CHECK(pixels[cornerIdx] < pixels[centerIdx]);
    CHECK(pixels[cornerIdx + 1] < pixels[centerIdx + 1]);
    CHECK(pixels[cornerIdx + 2] < pixels[centerIdx + 2]);
}

TEST_CASE("effects: blur smooths a sharp step edge") {
    int w = 10;
    int h = 1;
    int stride = w * 4;
    // Left half 0, right half 200
    std::vector<uint8_t> pixels(static_cast<std::size_t>(w * h * 4), 0);
    for (int x = 5; x < 10; ++x) {
        pixels[static_cast<std::size_t>(x * 4 + 0)] = 200;
        pixels[static_cast<std::size_t>(x * 4 + 1)] = 200;
        pixels[static_cast<std::size_t>(x * 4 + 2)] = 200;
        pixels[static_cast<std::size_t>(x * 4 + 3)] = 255;
    }

    effects::PixelPipeline::applyBlur(pixels.data(), w, h, stride, 2);

    // x = 4 (which was 0) should now have received blurred light > 0
    CHECK(pixels[static_cast<std::size_t>(4 * 4)] > 0);
    // x = 5 (which was 200) should now have attenuated < 200
    CHECK(pixels[static_cast<std::size_t>(5 * 4)] < 200);
}

TEST_CASE("effects: sequential applyEffects pipeline execution") {
    std::vector<uint8_t> pixels = {
        0, 255, 0, 255,
        255, 0, 0, 255
    };

    std::vector<core::Effect> effList;

    // Effect 1: chroma_key for green
    core::Effect ck;
    ck.type = "chroma_key";
    ck.enabled = true;
    ck.params["similarity"] = 0.4;
    ck.params["smoothness"] = 0.1;
    ck.strParams["key_color"] = "#00FF00";
    effList.push_back(ck);

    // Effect 2: color_adjust
    core::Effect ca;
    ca.type = "color_adjust";
    ca.enabled = true;
    ca.params["brightness"] = 0.1;
    effList.push_back(ca);

    effects::PixelPipeline::applyEffects(pixels.data(), 2, 1, 8, effList);

    // Green pixel alpha should still be 0
    CHECK_EQ(pixels[3], 0);
    // Red pixel red channel should have increased due to brightness
    CHECK(pixels[4] >= 250);
}

TEST_CASE("effects: CapCut blend modes composite correctly") {
    // 50% gray base (128, 128, 128, 255)
    std::vector<uint8_t> bg = { 128, 128, 128, 255 };
    // 50% gray foreground (128, 128, 128, 255)
    std::vector<uint8_t> fg = { 128, 128, 128, 255 };

    // Multiply: 0.5 * 0.5 = 0.25 -> 64
    std::vector<uint8_t> mulOut = bg;
    effects::PixelPipeline::applyBlend(mulOut.data(), fg.data(), 1, 1, 4, 4,
                                       effects::BlendMode::Multiply, 1.0f);
    CHECK(mulOut[0] >= 63 && mulOut[0] <= 65);

    // Screen: 1 - (1-0.5)*(1-0.5) = 0.75 -> 191
    std::vector<uint8_t> screenOut = bg;
    effects::PixelPipeline::applyBlend(screenOut.data(), fg.data(), 1, 1, 4, 4,
                                       effects::BlendMode::Screen, 1.0f);
    CHECK(screenOut[0] >= 190 && screenOut[0] <= 193);

    // Darken & Lighten
    std::vector<uint8_t> darkBg = { 50, 200, 100, 255 };
    std::vector<uint8_t> lightFg = { 100, 100, 100, 255 };
    std::vector<uint8_t> darkOut = darkBg;
    effects::PixelPipeline::applyBlend(darkOut.data(), lightFg.data(), 1, 1, 4, 4,
                                       effects::BlendMode::Darken, 1.0f);
    CHECK_EQ(darkOut[0], 50);
    CHECK_EQ(darkOut[1], 100);

    // Soft Light identity test with neutral gray (0.5)
    std::vector<uint8_t> slOut = bg;
    effects::PixelPipeline::applyBlend(slOut.data(), fg.data(), 1, 1, 4, 4,
                                       effects::BlendMode::SoftLight, 1.0f);
    CHECK(slOut[0] >= 126 && slOut[0] <= 130);
}

TEST_CASE("effects: CapCut highlights and shadows curve adjustment") {
    // 3 pixels: Shadow (30), Mid (128), Highlight (220)
    std::vector<uint8_t> pixels = {
        30, 30, 30, 255,
        128, 128, 128, 255,
        220, 220, 220, 255
    };

    // Lift shadows (+0.5), keep highlights 0
    std::vector<uint8_t> shadowLift = pixels;
    effects::PixelPipeline::applyHighlightsShadows(shadowLift.data(), 3, 1, 12, 0.0, 0.5);
    // Shadow pixel (idx 0) should be raised
    CHECK(shadowLift[0] > pixels[0]);
    // Highlight pixel (idx 8) should remain relatively unaffected
    CHECK(std::abs(static_cast<int>(shadowLift[8]) - static_cast<int>(pixels[8])) <= 3);

    // Pull highlights (-0.5), keep shadows 0
    std::vector<uint8_t> highlightPull = pixels;
    effects::PixelPipeline::applyHighlightsShadows(highlightPull.data(), 3, 1, 12, -0.5, 0.0);
    // Highlight pixel should be reduced
    CHECK(highlightPull[8] < pixels[8]);
    // Shadow pixel should remain relatively unaffected
    CHECK(std::abs(static_cast<int>(highlightPull[0]) - static_cast<int>(pixels[0])) <= 3);
}

TEST_CASE("effects: CapCut primary color wheels math") {
    std::vector<uint8_t> pixels = {
        64, 64, 64, 255,
        128, 128, 128, 255
    };

    // Gain boost (gainY = 1.5)
    std::vector<uint8_t> test = pixels;
    effects::ColorWheelParams params;
    params.gainY = 1.5;
    effects::PixelPipeline::applyColorWheels(test.data(), 2, 1, 8, params);
    CHECK(test[0] > pixels[0]);
    CHECK(test[4] > pixels[4]);
}

TEST_CASE("effects: CapCut cubic bezier curve evaluation") {
    // 1D Bezier curve: p0=0, p1=0.25, p2=0.75, p3=1.0
    // Boundary conditions
    CHECK(std::abs(effects::PixelPipeline::evalCubicBezier(0.0, 0.25, 0.75, 1.0, 0.0) - 0.0) < 0.01);
    CHECK(std::abs(effects::PixelPipeline::evalCubicBezier(0.0, 0.25, 0.75, 1.0, 1.0) - 1.0) < 0.01);
    // Monotonic midpoint
    double mid = effects::PixelPipeline::evalCubicBezier(0.0, 0.25, 0.75, 1.0, 0.5);
    CHECK(mid > 0.0 && mid < 1.0);
}

TEST_CASE("effects: letterbox masks top and bottom correctly") {
    int w = 10, h = 10, stride = w * 4;
    std::vector<uint8_t> pixels(w * h * 4, 200);

    // 20% barHeight -> top 2 rows (y=0,1) and bottom 2 rows (y=8,9) masked to 0
    effects::PixelPipeline::applyLetterbox(pixels.data(), w, h, stride, 0.2, 0.0);

    // Check top row (y=0) is black
    CHECK_EQ(pixels[0], 0);
    CHECK_EQ(pixels[1], 0);
    CHECK_EQ(pixels[2], 0);

    // Check center row (y=5) is unaffected
    int centerIdx = (5 * w + 5) * 4;
    CHECK_EQ(pixels[centerIdx], 200);

    // Check bottom row (y=9) is black
    int bottomIdx = (9 * w + 5) * 4;
    CHECK_EQ(pixels[bottomIdx], 0);
}

TEST_CASE("effects: film grain adds bounded luminance-weighted noise") {
    int w = 8, h = 8, stride = w * 4;
    std::vector<uint8_t> pixels(w * h * 4, 128);

    effects::PixelPipeline::applyFilmGrain(pixels.data(), w, h, stride, 0.4, 1.0, false);

    bool changed = false;
    for (int i = 0; i < w * h; ++i) {
        uint8_t val = pixels[i * 4];
        if (val != 128) changed = true;
        // All channels identical in monochrome mode
        CHECK_EQ(pixels[i * 4 + 0], pixels[i * 4 + 1]);
        CHECK_EQ(pixels[i * 4 + 1], pixels[i * 4 + 2]);
    }
    CHECK(changed);
}

TEST_CASE("effects: chromatic aberration shifts R and B channels") {
    int w = 11, h = 3, stride = w * 4;
    std::vector<uint8_t> pixels(w * h * 4, 0);

    // Vertical white stripe at x=5 (center)
    for (int y = 0; y < h; ++y) {
        int idx = (y * w + 5) * 4;
        pixels[idx + 0] = 255;
        pixels[idx + 1] = 255;
        pixels[idx + 2] = 255;
        pixels[idx + 3] = 255;
    }

    // Shift X by +2 px
    effects::PixelPipeline::applyChromaticAberration(pixels.data(), w, h, stride, 2.0, 0.0);

    int y = 1;
    // At x=3: Blue shifted from x=5 -> Blue should be 255, Red should be 0
    int idx3 = (y * w + 3) * 4;
    CHECK_EQ(pixels[idx3 + 0], 0);
    CHECK_EQ(pixels[idx3 + 2], 255);

    // At x=7: Red shifted from x=5 -> Red should be 255, Blue should be 0
    int idx7 = (y * w + 7) * 4;
    CHECK_EQ(pixels[idx7 + 0], 255);
    CHECK_EQ(pixels[idx7 + 2], 0);

    // At x=5: Green remains in center
    int idx5 = (y * w + 5) * 4;
    CHECK_EQ(pixels[idx5 + 1], 255);
}

TEST_CASE("effects: bloom extracts specular highlights and diffuses glow") {
    int w = 7, h = 7, stride = w * 4;
    std::vector<uint8_t> pixels(w * h * 4, 0);

    // Bright center pixel at (3, 3)
    int centerIdx = (3 * w + 3) * 4;
    pixels[centerIdx + 0] = 255;
    pixels[centerIdx + 1] = 255;
    pixels[centerIdx + 2] = 255;
    pixels[centerIdx + 3] = 255;

    // Apply bloom with threshold 0.5, radius 2
    effects::PixelPipeline::applyBloom(pixels.data(), w, h, stride, 0.5, 2, 1.0);

    // Adjacent pixel (3, 4) was 0 -> should now have glow > 0
    int neighborIdx = (3 * w + 4) * 4;
    CHECK(pixels[neighborIdx + 0] > 0);
    CHECK(pixels[neighborIdx + 1] > 0);
    CHECK(pixels[neighborIdx + 2] > 0);
}

TEST_CASE("effects: split toning warms highlights and cools shadows") {
    // Pixel 0: shadow (40, 40, 40), Pixel 1: highlight (210, 210, 210)
    std::vector<uint8_t> pixels = {
        40, 40, 40, 255,
        210, 210, 210, 255
    };

    effects::PixelPipeline::applySplitToning(pixels.data(), 2, 1, 8, 0.6, 0.6, 0.0);

    // Shadow pixel: Blue should be greater than Red (Teal tone)
    CHECK(pixels[2] > pixels[0]);

    // Highlight pixel: Red should be greater than Blue (Orange tone)
    CHECK(pixels[4] > pixels[6]);
}

TEST_CASE("effects: retro vhs modulates scanlines") {
    int w = 6, h = 6, stride = w * 4;
    std::vector<uint8_t> pixels(w * h * 4, 200);

    effects::PixelPipeline::applyRetroVhs(pixels.data(), w, h, stride, 0.6, 0.0, 0.0);

    // Row 0 (y%3 == 0) should be darkened by scanline modulation
    CHECK(pixels[0] < 200);
    // Row 1 should be unaffected
    CHECK_EQ(pixels[1 * stride], 200);
}

TEST_CASE("effects: posterize quantizes color levels") {
    int w = 256, h = 1, stride = w * 4;
    std::vector<uint8_t> pixels(w * 4);
    for (int i = 0; i < 256; ++i) {
        pixels[i * 4 + 0] = static_cast<uint8_t>(i);
        pixels[i * 4 + 1] = static_cast<uint8_t>(i);
        pixels[i * 4 + 2] = static_cast<uint8_t>(i);
        pixels[i * 4 + 3] = 255;
    }

    effects::PixelPipeline::applyPosterize(pixels.data(), w, h, stride, 4);

    std::vector<uint8_t> uniqueVals;
    for (int i = 0; i < 256; ++i) {
        uint8_t v = pixels[i * 4];
        if (std::find(uniqueVals.begin(), uniqueVals.end(), v) == uniqueVals.end()) {
            uniqueVals.push_back(v);
        }
    }
    // Should have exactly 4 discrete levels
    CHECK_EQ(uniqueVals.size(), static_cast<size_t>(4));
}

TEST_CASE("effects: invert negates color channels") {
    std::vector<uint8_t> pixel = { 200, 50, 10, 255 };

    effects::PixelPipeline::applyInvert(pixel.data(), 1, 1, 4, 1.0);

    CHECK_EQ(pixel[0], 55);
    CHECK_EQ(pixel[1], 205);
    CHECK_EQ(pixel[2], 245);
    CHECK_EQ(pixel[3], 255);
}

TEST_CASE("effects: edge detect highlights contrast boundaries") {
    int w = 8, h = 6, stride = w * 4;
    // Step edge: left half 0, right half 255
    std::vector<uint8_t> pixels(w * h * 4, 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 4; x < w; ++x) {
            pixels[(y * w + x) * 4 + 0] = 255;
            pixels[(y * w + x) * 4 + 1] = 255;
            pixels[(y * w + x) * 4 + 2] = 255;
            pixels[(y * w + x) * 4 + 3] = 255;
        }
    }

    effects::PixelPipeline::applyEdgeDetect(pixels.data(), w, h, stride, 1.0, false);

    // Center boundary (y=2, x=3) should have strong detected edge
    int edgePx = pixels[(2 * w + 3) * 4];
    CHECK(edgePx > 100);

    // Flat region far to the right (y=2, x=6) should be 0
    int flatPx = pixels[(2 * w + 6) * 4];
    CHECK_EQ(flatPx, 0);
}

TEST_CASE("effects: mirror reflects horizontal and vertical axes") {
    // 4x2 image: [10, 20, 30, 40] on row 0, [50, 60, 70, 80] on row 1
    std::vector<uint8_t> pixels = {
        10, 10, 10, 255,  20, 20, 20, 255,  30, 30, 30, 255,  40, 40, 40, 255,
        50, 50, 50, 255,  60, 60, 60, 255,  70, 70, 70, 255,  80, 80, 80, 255
    };

    // Horizontal Flip (mode 0)
    std::vector<uint8_t> test = pixels;
    effects::PixelPipeline::applyMirror(test.data(), 4, 2, 16, 0);
    CHECK_EQ(test[0], 40);
    CHECK_EQ(test[12], 10);

    // Center Mirror H (mode 2: left copied to right reversed)
    test = pixels;
    effects::PixelPipeline::applyMirror(test.data(), 4, 2, 16, 2);
    CHECK_EQ(test[0], 10);
    CHECK_EQ(test[4], 20);
    CHECK_EQ(test[8], 20);
    CHECK_EQ(test[12], 10);
}

TEST_CASE("effects: registry contains all new cinema effects and transitions") {
    const auto& reg = effects::EffectRegistry::defaults();

    // Cinema transitions
    CHECK(reg.find("iris_circle") != nullptr);
    CHECK(reg.find("barn_doors_h") != nullptr);
    CHECK(reg.find("barn_doors_v") != nullptr);
    CHECK(reg.find("zoom_in") != nullptr);
    CHECK(reg.find("zoom_out") != nullptr);
    CHECK(reg.find("flash_dissolve") != nullptr);

    // Movie premiere effects
    CHECK(reg.find("letterbox") != nullptr);
    CHECK(reg.find("film_grain") != nullptr);
    CHECK(reg.find("chromatic_aberration") != nullptr);
    CHECK(reg.find("bloom") != nullptr);
    CHECK(reg.find("split_toning") != nullptr);
    CHECK(reg.find("retro_vhs") != nullptr);
    CHECK(reg.find("posterize") != nullptr);
    CHECK(reg.find("invert") != nullptr);
    CHECK(reg.find("edge_detect") != nullptr);
    CHECK(reg.find("mirror") != nullptr);
}

TEST_CASE("effects: sequential execution of new cinema effects in applyEffects") {
    int w = 10, h = 10, stride = w * 4;
    std::vector<uint8_t> pixels(w * h * 4, 100);

    std::vector<core::Effect> effList;
    core::Effect lb;
    lb.type = "letterbox";
    lb.enabled = true;
    lb.params["barHeight"] = 0.2;
    lb.params["feather"] = 0.0;
    effList.push_back(lb);

    core::Effect inv;
    inv.type = "invert";
    inv.enabled = true;
    inv.params["intensity"] = 1.0;
    effList.push_back(inv);

    effects::PixelPipeline::applyEffects(pixels.data(), w, h, stride, effList);

    // Row 0 was letterboxed to 0, then inverted to 255
    CHECK_EQ(pixels[0], 255);

    // Center pixel (row 5) was 100, then inverted to 155
    int centerIdx = (5 * w + 5) * 4;
    CHECK_EQ(pixels[centerIdx], 155);
}

int main() {
    return editor::tests::runAll();
}
