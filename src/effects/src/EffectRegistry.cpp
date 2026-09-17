#include "effects/EffectSchema.hpp"

namespace editor::effects {

const EffectRegistry& EffectRegistry::defaults() {
    static const EffectRegistry registry = [] {
        EffectRegistry r;
        r.defs_.push_back(EffectDef{
            "transform",
            1,
            "Transform",
            {
                {"scale", ParamType::Float, 0.01, 8.0, 1.0, true, "x"},
                {"posX", ParamType::Float, -4096.0, 4096.0, 0.0, true, "px"},
                {"posY", ParamType::Float, -4096.0, 4096.0, 0.0, true, "px"},
                {"rotation", ParamType::Float, -180.0, 180.0, 0.0, true, "deg"},
            },
        });
        r.defs_.push_back(EffectDef{
            "opacity",
            1,
            "Opacity",
            {{{"opacity", ParamType::Float, 0.0, 1.0, 1.0, true, "%"}}},
        });
        r.defs_.push_back(EffectDef{
            "volume",
            1,
            "Volume",
            {
                {"gainDb", ParamType::Float, -60.0, 12.0, 0.0, true, "dB"},
                {"fadeIn", ParamType::Float, 0.0, 10.0, 0.0, false, "s"},
                {"fadeOut", ParamType::Float, 0.0, 10.0, 0.0, false, "s"},
            },
        });
        // Stage 2 Transitions
        r.defs_.push_back(EffectDef{
            "crossfade",
            1,
            "Crossfade",
            {
                {"duration", ParamType::Float, 0.1, 5.0, 1.0, false, "s"},
            },
        });
        r.defs_.push_back(EffectDef{
            "dip_black",
            1,
            "Dip to Black",
            {
                {"duration", ParamType::Float, 0.1, 5.0, 1.0, false, "s"},
            },
        });
        r.defs_.push_back(EffectDef{
            "dip_white",
            1,
            "Dip to White",
            {
                {"duration", ParamType::Float, 0.1, 5.0, 1.0, false, "s"},
            },
        });
        r.defs_.push_back(EffectDef{
            "wipe_left",
            1,
            "Wipe Left",
            {
                {"duration", ParamType::Float, 0.1, 5.0, 1.0, false, "s"},
                {"softness", ParamType::Float, 0.0, 1.0, 0.0, false, "%"},
            },
        });
        r.defs_.push_back(EffectDef{
            "wipe_right",
            1,
            "Wipe Right",
            {
                {"duration", ParamType::Float, 0.1, 5.0, 1.0, false, "s"},
                {"softness", ParamType::Float, 0.0, 1.0, 0.0, false, "%"},
            },
        });
        r.defs_.push_back(EffectDef{
            "wipe_up",
            1,
            "Wipe Up",
            {
                {"duration", ParamType::Float, 0.1, 5.0, 1.0, false, "s"},
                {"softness", ParamType::Float, 0.0, 1.0, 0.0, false, "%"},
            },
        });
        r.defs_.push_back(EffectDef{
            "wipe_down",
            1,
            "Wipe Down",
            {
                {"duration", ParamType::Float, 0.1, 5.0, 1.0, false, "s"},
                {"softness", ParamType::Float, 0.0, 1.0, 0.0, false, "%"},
            },
        });
        // Stage 3 Creative Effects
        r.defs_.push_back(EffectDef{
            "color_adjust",
            1,
            "Color Adjust",
            {
                {"brightness", ParamType::Float, -1.0, 1.0, 0.0, true, ""},
                {"contrast", ParamType::Float, 0.0, 2.0, 1.0, true, ""},
                {"saturation", ParamType::Float, 0.0, 2.0, 1.0, true, ""},
                {"temperature", ParamType::Float, -1.0, 1.0, 0.0, true, ""},
                {"tint", ParamType::Float, -1.0, 1.0, 0.0, true, ""},
            },
        });
        r.defs_.push_back(EffectDef{
            "vignette",
            1,
            "Vignette",
            {
                {"intensity", ParamType::Float, 0.0, 1.0, 0.5, true, "%"},
                {"radius", ParamType::Float, 0.1, 1.5, 0.7, true, ""},
                {"softness", ParamType::Float, 0.0, 1.0, 0.5, true, "%"},
            },
        });
        r.defs_.push_back(EffectDef{
            "blur",
            1,
            "Blur",
            {
                {"radius", ParamType::Float, 0.0, 50.0, 10.0, true, "px"},
            },
        });
        r.defs_.push_back(EffectDef{
            "sharpen",
            1,
            "Sharpen",
            {
                {"amount", ParamType::Float, 0.0, 2.0, 0.5, true, ""},
            },
        });
        r.defs_.push_back(EffectDef{
            "chroma_key",
            1,
            "Chroma Key",
            {
                {"similarity", ParamType::Float, 0.0, 1.0, 0.3, true, "%"},
                {"smoothness", ParamType::Float, 0.0, 1.0, 0.1, true, "%"},
            },
        });
        r.defs_.push_back(EffectDef{
            "highlights_shadows",
            1,
            "Highlights & Shadows",
            {
                {"highlights", ParamType::Float, -1.0, 1.0, 0.0, true, ""},
                {"shadows", ParamType::Float, -1.0, 1.0, 0.0, true, ""},
            },
        });
        r.defs_.push_back(EffectDef{
            "color_wheels",
            1,
            "Color Wheels",
            {
                {"liftR", ParamType::Float, -0.5, 0.5, 0.0, true, ""},
                {"liftG", ParamType::Float, -0.5, 0.5, 0.0, true, ""},
                {"liftB", ParamType::Float, -0.5, 0.5, 0.0, true, ""},
                {"liftY", ParamType::Float, -0.5, 0.5, 0.0, true, ""},
                {"gammaR", ParamType::Float, 0.1, 4.0, 1.0, true, ""},
                {"gammaG", ParamType::Float, 0.1, 4.0, 1.0, true, ""},
                {"gammaB", ParamType::Float, 0.1, 4.0, 1.0, true, ""},
                {"gammaY", ParamType::Float, 0.1, 4.0, 1.0, true, ""},
                {"gainR", ParamType::Float, 0.0, 4.0, 1.0, true, ""},
                {"gainG", ParamType::Float, 0.0, 4.0, 1.0, true, ""},
                {"gainB", ParamType::Float, 0.0, 4.0, 1.0, true, ""},
                {"gainY", ParamType::Float, 0.0, 4.0, 1.0, true, ""},
                {"offsetR", ParamType::Float, -1.0, 1.0, 0.0, true, ""},
                {"offsetG", ParamType::Float, -1.0, 1.0, 0.0, true, ""},
                {"offsetB", ParamType::Float, -1.0, 1.0, 0.0, true, ""},
                {"lumaMix", ParamType::Float, 0.0, 1.0, 1.0, true, ""},
            },
        });
        r.defs_.push_back(EffectDef{
            "blend_mode",
            1,
            "Blend Mode",
            {
                {"mode", ParamType::Int, 0.0, 9.0, 0.0, false, ""},
                {"opacity", ParamType::Float, 0.0, 1.0, 1.0, true, "%"},
            },
        });
        // Cinema Transitions
        r.defs_.push_back(EffectDef{
            "iris_circle",
            1,
            "Iris Circle",
            {
                {"duration", ParamType::Float, 0.1, 5.0, 1.0, false, "s"},
            },
        });
        r.defs_.push_back(EffectDef{
            "barn_doors_h",
            1,
            "Barn Doors (H)",
            {
                {"duration", ParamType::Float, 0.1, 5.0, 1.0, false, "s"},
            },
        });
        r.defs_.push_back(EffectDef{
            "barn_doors_v",
            1,
            "Barn Doors (V)",
            {
                {"duration", ParamType::Float, 0.1, 5.0, 1.0, false, "s"},
            },
        });
        r.defs_.push_back(EffectDef{
            "zoom_in",
            1,
            "Zoom In",
            {
                {"duration", ParamType::Float, 0.1, 5.0, 1.0, false, "s"},
            },
        });
        r.defs_.push_back(EffectDef{
            "zoom_out",
            1,
            "Zoom Out",
            {
                {"duration", ParamType::Float, 0.1, 5.0, 1.0, false, "s"},
            },
        });
        r.defs_.push_back(EffectDef{
            "flash_dissolve",
            1,
            "Flash Dissolve",
            {
                {"duration", ParamType::Float, 0.1, 5.0, 1.0, false, "s"},
            },
        });
        // Movie Premiere Cinematic Effects
        r.defs_.push_back(EffectDef{
            "letterbox",
            1,
            "Letterbox 2.35:1",
            {
                {"barHeight", ParamType::Float, 0.0, 0.5, 0.12, true, "%"},
                {"feather", ParamType::Float, 0.0, 0.2, 0.0, true, "%"},
            },
        });
        r.defs_.push_back(EffectDef{
            "film_grain",
            1,
            "35mm Film Grain",
            {
                {"intensity", ParamType::Float, 0.0, 1.0, 0.25, true, "%"},
                {"size", ParamType::Float, 1.0, 4.0, 1.0, false, "px"},
                {"colored", ParamType::Int, 0.0, 1.0, 0.0, false, ""},
            },
        });
        r.defs_.push_back(EffectDef{
            "chromatic_aberration",
            1,
            "Chromatic Aberration",
            {
                {"shiftX", ParamType::Float, -50.0, 50.0, 5.0, true, "px"},
                {"shiftY", ParamType::Float, -50.0, 50.0, 0.0, true, "px"},
            },
        });
        r.defs_.push_back(EffectDef{
            "bloom",
            1,
            "Cinematic Bloom",
            {
                {"threshold", ParamType::Float, 0.1, 1.0, 0.65, true, "%"},
                {"radius", ParamType::Float, 1.0, 40.0, 12.0, true, "px"},
                {"intensity", ParamType::Float, 0.0, 2.0, 0.7, true, ""},
            },
        });
        r.defs_.push_back(EffectDef{
            "split_toning",
            1,
            "Split Toning (Teal & Orange)",
            {
                {"shadowTeal", ParamType::Float, 0.0, 1.0, 0.4, true, "%"},
                {"highlightOrange", ParamType::Float, 0.0, 1.0, 0.4, true, "%"},
                {"balance", ParamType::Float, -1.0, 1.0, 0.0, true, ""},
            },
        });
        r.defs_.push_back(EffectDef{
            "retro_vhs",
            1,
            "Retro VHS Tape",
            {
                {"scanlines", ParamType::Float, 0.0, 1.0, 0.35, true, "%"},
                {"colorBleed", ParamType::Float, 0.0, 20.0, 3.0, true, "px"},
                {"noise", ParamType::Float, 0.0, 1.0, 0.15, true, "%"},
            },
        });
        r.defs_.push_back(EffectDef{
            "posterize",
            1,
            "Posterize",
            {
                {"levels", ParamType::Int, 2.0, 64.0, 6.0, true, ""},
            },
        });
        r.defs_.push_back(EffectDef{
            "invert",
            1,
            "Invert",
            {
                {"intensity", ParamType::Float, 0.0, 1.0, 1.0, true, "%"},
            },
        });
        r.defs_.push_back(EffectDef{
            "edge_detect",
            1,
            "Edge Detect",
            {
                {"intensity", ParamType::Float, 0.0, 4.0, 1.0, true, ""},
                {"invert", ParamType::Int, 0.0, 1.0, 0.0, false, ""},
            },
        });
        r.defs_.push_back(EffectDef{
            "mirror",
            1,
            "Mirror Reflection",
            {
                {"mode", ParamType::Int, 0.0, 4.0, 0.0, false, ""},
            },
        });
        return r;
    }();
    return registry;
}

const EffectDef* EffectRegistry::find(const std::string& type) const {
    for (const auto& d : defs_) {
        if (d.type == type) {
            return &d;
        }
    }
    return nullptr;
}

} // namespace editor::effects
