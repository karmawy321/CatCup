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
