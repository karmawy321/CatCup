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
        // Stages 2-3 extend this table (transitions, color, masks, speed).
        // Each new entry needs its CPU reference + inspector metadata together.
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
