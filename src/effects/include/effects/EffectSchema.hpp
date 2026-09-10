#pragma once

// Effect parameter schemas. The inspector (Stage 1+) builds its sliders and
// numeric fields from these descriptors; the renderer reads the same
// registry. No hard-coded effect UI — new effects register here.

#include <string>
#include <vector>

namespace editor::effects {

enum class ParamType { Float, Int, Bool, Text };

struct ParamDef {
    std::string name;
    ParamType type = ParamType::Float;
    double min = 0.0;
    double max = 1.0;
    double def = 0.0;
    bool keyframable = false;
    std::string unit; // e.g. "px", "%", "deg", "dB", "s"
};

struct EffectDef {
    std::string type;
    int version = 1;
    std::string displayName;
    std::vector<ParamDef> params;
};

class EffectRegistry {
public:
    static const EffectRegistry& defaults();
    [[nodiscard]] const EffectDef* find(const std::string& type) const;
    [[nodiscard]] const std::vector<EffectDef>& all() const noexcept { return defs_; }

private:
    std::vector<EffectDef> defs_;
};

} // namespace editor::effects
