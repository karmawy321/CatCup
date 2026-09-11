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

int main() {
    return editor::tests::runAll();
}
