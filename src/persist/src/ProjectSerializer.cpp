#include "persist/ProjectSerializer.hpp"

#include "persist/AtomicFile.hpp"
#include "persist/Migrations.hpp"

#include <filesystem>

namespace editor::persist {
namespace {

JsonValue rationalToJson(const core::Rational& r) {
    return JsonValue(JsonObject{{"num", JsonValue(r.num())}, {"den", JsonValue(r.den())}});
}

core::Result<core::Rational> rationalFromJson(const JsonValue& v, const std::string& ctx) {
    std::string err;
    const JsonObject* obj = requireObject(v, err);
    if (obj == nullptr) {
        return core::Result<core::Rational>::fail(ctx + ": " + err);
    }
    auto n = requireInt(*obj, "num");
    if (n.isErr()) {
        return core::Result<core::Rational>::fail(ctx + ": " + n.error());
    }
    auto d = requireInt(*obj, "den");
    if (d.isErr()) {
        return core::Result<core::Rational>::fail(ctx + ": " + d.error());
    }
    try {
        return core::Result<core::Rational>::ok(core::Rational(n.value(), d.value()));
    } catch (const std::exception& e) {
        return core::Result<core::Rational>::fail(ctx + ": " + e.what());
    }
}

JsonValue effectToJson(const core::Effect& e) {
    JsonObject params;
    for (const auto& [k, v] : e.params) {
        params.emplace(k, JsonValue(v));
    }
    JsonObject strParams;
    for (const auto& [k, v] : e.strParams) {
        strParams.emplace(k, JsonValue(v));
    }
    return JsonValue(JsonObject{
        {"type", JsonValue(e.type)},
        {"version", JsonValue(e.version)},
        {"enabled", JsonValue(e.enabled)},
        {"order", JsonValue(e.order)},
        {"params", JsonValue(std::move(params))},
        {"strParams", JsonValue(std::move(strParams))},
    });
}

core::Result<core::Effect> effectFromJson(const JsonValue& v) {
    std::string err;
    const JsonObject* obj = requireObject(v, err);
    if (obj == nullptr) {
        return core::Result<core::Effect>::fail("effect: " + err);
    }
    core::Effect e;
    auto t = requireString(*obj, "type");
    if (t.isErr()) {
        return core::Result<core::Effect>::fail(t.error());
    }
    e.type = t.value();
    if (const auto it = obj->find("version"); it != obj->end() && it->second.isInt()) {
        e.version = static_cast<int>(it->second.asInt());
    }
    if (const auto it = obj->find("enabled"); it != obj->end() && it->second.isBool()) {
        e.enabled = it->second.asBool();
    }
    if (const auto it = obj->find("order"); it != obj->end() && it->second.isInt()) {
        e.order = static_cast<int>(it->second.asInt());
    }
    if (const auto it = obj->find("params"); it != obj->end() && it->second.isObject()) {
        for (const auto& [k, pv] : it->second.asObject()) {
            if (pv.isDouble()) {
                e.params.emplace(k, pv.asDouble());
            } else if (pv.isInt()) {
                e.params.emplace(k, static_cast<double>(pv.asInt()));
            }
        }
    }
    if (const auto it = obj->find("strParams"); it != obj->end() && it->second.isObject()) {
        for (const auto& [k, pv] : it->second.asObject()) {
            if (pv.isString()) {
                e.strParams.emplace(k, pv.asString());
            }
        }
    }
    return core::Result<core::Effect>::ok(std::move(e));
}

} // namespace

core::Result<JsonValue> projectToJson(const core::Project& project) {
    JsonArray assets;
    for (const auto& [id, a] : project.assets) {
        assets.push_back(JsonValue(JsonObject{
            {"id", JsonValue(a.id)},
            {"kind", JsonValue(core::toString(a.kind))},
            {"path", JsonValue(a.path)},
            {"duration", rationalToJson(a.duration)},
            {"fps", rationalToJson(a.fps)},
            {"width", JsonValue(a.width)},
            {"height", JsonValue(a.height)},
            {"hash", JsonValue(a.hash)},
        }));
    }
    JsonArray sequences;
    for (const auto& seq : project.sequences) {
        JsonArray tracks;
        for (const auto& t : seq.tracks) {
            JsonArray clipIds;
            for (const auto& cid : t.clipIds) {
                clipIds.push_back(JsonValue(cid));
            }
            tracks.push_back(JsonValue(JsonObject{
                {"id", JsonValue(t.id)},
                {"kind", JsonValue(core::toString(t.kind))},
                {"name", JsonValue(t.name)},
                {"locked", JsonValue(t.locked)},
                {"visible", JsonValue(t.visible)},
                {"muted", JsonValue(t.muted)},
                {"clipIds", JsonValue(std::move(clipIds))},
            }));
        }
        JsonArray clips;
        for (const auto& [id, c] : seq.clips) {
            JsonArray effects;
            for (const auto& e : c.effects) {
                effects.push_back(effectToJson(e));
            }
            JsonObject tr{{"scale", JsonValue(c.transform.scale)},
                          {"x", JsonValue(c.transform.x)},
                          {"y", JsonValue(c.transform.y)},
                          {"rotationDeg", JsonValue(c.transform.rotationDeg)}};
            JsonArray keyframes;
            for (const auto& kf : c.keyframes) {
                JsonObject trKf{{"scale", JsonValue(kf.transform.scale)},
                                {"x", JsonValue(kf.transform.x)},
                                {"y", JsonValue(kf.transform.y)},
                                {"rotationDeg", JsonValue(kf.transform.rotationDeg)}};
                keyframes.push_back(JsonValue(JsonObject{
                    {"seqTime", rationalToJson(kf.seqTime)},
                    {"transform", JsonValue(std::move(trKf))},
                    {"opacity", JsonValue(kf.opacity)},
                    {"easing", JsonValue(kf.easing)},
                }));
            }
            clips.push_back(JsonValue(JsonObject{
                {"id", JsonValue(c.id)},
                {"assetId", JsonValue(c.assetId)},
                {"name", JsonValue(c.name)},
                {"sourceIn", rationalToJson(c.sourceIn)},
                {"sourceOut", rationalToJson(c.sourceOut)},
                {"seqStart", rationalToJson(c.seqStart)},
                {"enabled", JsonValue(c.enabled)},
                {"opacity", JsonValue(c.opacity)},
                {"speed", rationalToJson(c.speed)},
                {"transform", JsonValue(std::move(tr))},
                {"effects", JsonValue(std::move(effects))},
                {"keyframes", JsonValue(std::move(keyframes))},
                {"fadeInSec", JsonValue(c.fadeInSec)},
                {"fadeOutSec", JsonValue(c.fadeOutSec)},
                {"text", JsonValue(c.text)},
                {"fontFamily", JsonValue(c.fontFamily)},
                {"fontSizePt", JsonValue(c.fontSizePt)},
            }));
        }
        JsonArray transitions;
        for (const auto& tr : seq.transitions) {
            JsonObject params;
            for (const auto& [k, v] : tr.params) {
                params.emplace(k, JsonValue(v));
            }
            transitions.push_back(JsonValue(JsonObject{
                {"id", JsonValue(tr.id)},
                {"type", JsonValue(tr.type)},
                {"trackId", JsonValue(tr.trackId)},
                {"fromClipId", JsonValue(tr.fromClipId)},
                {"toClipId", JsonValue(tr.toClipId)},
                {"duration", rationalToJson(tr.duration)},
                {"alignment", JsonValue(core::toString(tr.alignment))},
                {"easing", JsonValue(tr.easing)},
                {"params", JsonValue(std::move(params))},
            }));
        }
        sequences.push_back(JsonValue(JsonObject{
            {"id", JsonValue(seq.id)},
            {"name", JsonValue(seq.name)},
            {"fps", rationalToJson(seq.fps)},
            {"width", JsonValue(seq.width)},
            {"height", JsonValue(seq.height)},
            {"tracks", JsonValue(std::move(tracks))},
            {"clips", JsonValue(std::move(clips))},
            {"transitions", JsonValue(std::move(transitions))},
        }));
    }
    return core::Result<JsonValue>::ok(JsonValue(JsonObject{
        {"schemaVersion", JsonValue(static_cast<std::int64_t>(project.schemaVersion))},
        {"name", JsonValue(project.name)},
        {"assets", JsonValue(std::move(assets))},
        {"sequences", JsonValue(std::move(sequences))},
        {"activeSequenceId", JsonValue(project.activeSequenceId)},
    }));
}

core::Result<core::Project> projectFromJson(const JsonValue& root) {
    std::string err;
    const JsonObject* obj = requireObject(root, err);
    if (obj == nullptr) {
        return core::Result<core::Project>::fail("project: " + err);
    }
    // Migrate a copy so the caller's document is untouched.
    JsonValue mutableRoot = root;
    if (auto m = migrateDocument(mutableRoot); m.isErr()) {
        return core::Result<core::Project>::fail(m.error());
    }
    const JsonObject& doc = mutableRoot.asObject();

    core::Project project;
    if (const auto it = doc.find("schemaVersion"); it != doc.end() && it->second.isInt()) {
        project.schemaVersion = static_cast<int>(it->second.asInt());
    }
    if (const auto it = doc.find("name"); it != doc.end() && it->second.isString()) {
        project.name = it->second.asString();
    }
    if (const auto it = doc.find("activeSequenceId");
        it != doc.end() && it->second.isString()) {
        project.activeSequenceId = it->second.asString();
    }

    if (const auto it = doc.find("assets"); it != doc.end()) {
        const JsonArray* arr = requireArray(it->second, err);
        if (arr == nullptr) {
            return core::Result<core::Project>::fail("assets: " + err);
        }
        for (const auto& av : *arr) {
            const JsonObject* ao = requireObject(av, err);
            if (ao == nullptr) {
                return core::Result<core::Project>::fail("asset: " + err);
            }
            core::Asset a;
            auto id = requireString(*ao, "id");
            if (id.isErr()) {
                return core::Result<core::Project>::fail(id.error());
            }
            a.id = id.value();
            if (const auto k = ao->find("kind"); k != ao->end() && k->second.isString()) {
                auto kind = core::assetKindFromString(k->second.asString());
                if (kind.isErr()) {
                    return core::Result<core::Project>::fail(kind.error());
                }
                a.kind = kind.value();
            }
            if (const auto f = ao->find("path"); f != ao->end() && f->second.isString()) {
                a.path = f->second.asString();
            }
            if (const auto f = ao->find("duration"); f != ao->end()) {
                auto r = rationalFromJson(f->second, "asset.duration");
                if (r.isErr()) {
                    return core::Result<core::Project>::fail(r.error());
                }
                a.duration = r.value();
            }
            if (const auto f = ao->find("fps"); f != ao->end()) {
                auto r = rationalFromJson(f->second, "asset.fps");
                if (r.isErr()) {
                    return core::Result<core::Project>::fail(r.error());
                }
                a.fps = r.value();
            }
            if (const auto f = ao->find("width"); f != ao->end() && f->second.isInt()) {
                a.width = f->second.asInt();
            }
            if (const auto f = ao->find("height"); f != ao->end() && f->second.isInt()) {
                a.height = f->second.asInt();
            }
            if (const auto f = ao->find("hash"); f != ao->end() && f->second.isString()) {
                a.hash = f->second.asString();
            }
            project.assets.emplace(a.id, std::move(a));
        }
    }

    if (const auto it = doc.find("sequences"); it != doc.end()) {
        const JsonArray* arr = requireArray(it->second, err);
        if (arr == nullptr) {
            return core::Result<core::Project>::fail("sequences: " + err);
        }
        for (const auto& sv : *arr) {
            const JsonObject* so = requireObject(sv, err);
            if (so == nullptr) {
                return core::Result<core::Project>::fail("sequence: " + err);
            }
            core::Sequence seq;
            auto id = requireString(*so, "id");
            if (id.isErr()) {
                return core::Result<core::Project>::fail(id.error());
            }
            seq.id = id.value();
            if (const auto f = so->find("name"); f != so->end() && f->second.isString()) {
                seq.name = f->second.asString();
            }
            if (const auto f = so->find("fps"); f != so->end()) {
                auto r = rationalFromJson(f->second, "sequence.fps");
                if (r.isErr()) {
                    return core::Result<core::Project>::fail(r.error());
                }
                seq.fps = r.value();
            }
            if (const auto f = so->find("width"); f != so->end() && f->second.isInt()) {
                seq.width = f->second.asInt();
            }
            if (const auto f = so->find("height"); f != so->end() && f->second.isInt()) {
                seq.height = f->second.asInt();
            }
            if (const auto f = so->find("tracks"); f != so->end()) {
                const JsonArray* tarr = requireArray(f->second, err);
                if (tarr == nullptr) {
                    return core::Result<core::Project>::fail("tracks: " + err);
                }
                for (const auto& tv : *tarr) {
                    const JsonObject* to = requireObject(tv, err);
                    if (to == nullptr) {
                        return core::Result<core::Project>::fail("track: " + err);
                    }
                    core::Track t;
                    auto tid = requireString(*to, "id");
                    if (tid.isErr()) {
                        return core::Result<core::Project>::fail(tid.error());
                    }
                    t.id = tid.value();
                    if (const auto k = to->find("kind"); k != to->end() && k->second.isString()) {
                        auto kind = core::trackKindFromString(k->second.asString());
                        if (kind.isErr()) {
                            return core::Result<core::Project>::fail(kind.error());
                        }
                        t.kind = kind.value();
                    }
                    if (const auto f2 = to->find("name"); f2 != to->end() && f2->second.isString()) {
                        t.name = f2->second.asString();
                    }
                    if (const auto f2 = to->find("locked"); f2 != to->end() && f2->second.isBool()) {
                        t.locked = f2->second.asBool();
                    }
                    if (const auto f2 = to->find("visible"); f2 != to->end() && f2->second.isBool()) {
                        t.visible = f2->second.asBool();
                    }
                    if (const auto f2 = to->find("muted"); f2 != to->end() && f2->second.isBool()) {
                        t.muted = f2->second.asBool();
                    }
                    if (const auto f2 = to->find("clipIds"); f2 != to->end()) {
                        const JsonArray* carr = requireArray(f2->second, err);
                        if (carr == nullptr) {
                            return core::Result<core::Project>::fail("clipIds: " + err);
                        }
                        for (const auto& cv : *carr) {
                            if (!cv.isString()) {
                                return core::Result<core::Project>::fail("clipId must be a string");
                            }
                            t.clipIds.push_back(cv.asString());
                        }
                    }
                    seq.tracks.push_back(std::move(t));
                }
            }
            if (const auto f = so->find("clips"); f != so->end()) {
                const JsonArray* carr = requireArray(f->second, err);
                if (carr == nullptr) {
                    return core::Result<core::Project>::fail("clips: " + err);
                }
                for (const auto& cv : *carr) {
                    const JsonObject* co = requireObject(cv, err);
                    if (co == nullptr) {
                        return core::Result<core::Project>::fail("clip: " + err);
                    }
                    core::Clip c;
                    auto cid = requireString(*co, "id");
                    if (cid.isErr()) {
                        return core::Result<core::Project>::fail(cid.error());
                    }
                    c.id = cid.value();
                    if (const auto f2 = co->find("assetId"); f2 != co->end() && f2->second.isString()) {
                        c.assetId = f2->second.asString();
                    }
                    if (const auto f2 = co->find("name"); f2 != co->end() && f2->second.isString()) {
                        c.name = f2->second.asString();
                    }
                    for (const auto* key : {"sourceIn", "sourceOut", "seqStart"}) {
                        const auto f2 = co->find(key);
                        if (f2 == co->end()) {
                            return core::Result<core::Project>::fail(
                                std::string("clip is missing '") + key + "'");
                        }
                        auto r = rationalFromJson(f2->second, std::string("clip.") + key);
                        if (r.isErr()) {
                            return core::Result<core::Project>::fail(r.error());
                        }
                        if (std::string(key) == "sourceIn") {
                            c.sourceIn = r.value();
                        } else if (std::string(key) == "sourceOut") {
                            c.sourceOut = r.value();
                        } else {
                            c.seqStart = r.value();
                        }
                    }
                    if (const auto f2 = co->find("enabled"); f2 != co->end() && f2->second.isBool()) {
                        c.enabled = f2->second.asBool();
                    }
                    if (const auto f2 = co->find("opacity");
                        f2 != co->end() && (f2->second.isDouble() || f2->second.isInt())) {
                        c.opacity = f2->second.isDouble() ? f2->second.asDouble()
                                                          : static_cast<double>(f2->second.asInt());
                    }
                    if (const auto f2 = co->find("speed"); f2 != co->end()) {
                        auto sp = rationalFromJson(f2->second, "clip.speed");
                        if (sp.isOk() && sp.value().num() > 0) {
                            c.speed = sp.value();
                        }
                    }
                    if (const auto f2 = co->find("transform");
                        f2 != co->end() && f2->second.isObject()) {
                        const auto& tr = f2->second.asObject();
                        if (const auto p = tr.find("scale");
                            p != tr.end() && (p->second.isDouble() || p->second.isInt())) {
                            c.transform.scale = p->second.isDouble()
                                                    ? p->second.asDouble()
                                                    : static_cast<double>(p->second.asInt());
                        }
                        if (const auto p = tr.find("x");
                            p != tr.end() && (p->second.isDouble() || p->second.isInt())) {
                            c.transform.x = p->second.isDouble() ? p->second.asDouble()
                                                                 : static_cast<double>(p->second.asInt());
                        }
                        if (const auto p = tr.find("y");
                            p != tr.end() && (p->second.isDouble() || p->second.isInt())) {
                            c.transform.y = p->second.isDouble() ? p->second.asDouble()
                                                                 : static_cast<double>(p->second.asInt());
                        }
                        if (const auto p = tr.find("rotationDeg");
                            p != tr.end() && (p->second.isDouble() || p->second.isInt())) {
                            c.transform.rotationDeg = p->second.isDouble()
                                                          ? p->second.asDouble()
                                                          : static_cast<double>(p->second.asInt());
                        }
                    }
                    if (const auto f2 = co->find("effects");
                        f2 != co->end() && f2->second.isArray()) {
                        for (const auto& ev : f2->second.asArray()) {
                            auto e = effectFromJson(ev);
                            if (e.isErr()) {
                                return core::Result<core::Project>::fail(e.error());
                            }
                            c.effects.push_back(std::move(e.value()));
                        }
                    }
                    if (const auto f2 = co->find("fadeInSec"); f2 != co->end() && (f2->second.isDouble() || f2->second.isInt())) {
                        c.fadeInSec = f2->second.isDouble() ? f2->second.asDouble() : static_cast<double>(f2->second.asInt());
                    }
                    if (const auto f2 = co->find("fadeOutSec"); f2 != co->end() && (f2->second.isDouble() || f2->second.isInt())) {
                        c.fadeOutSec = f2->second.isDouble() ? f2->second.asDouble() : static_cast<double>(f2->second.asInt());
                    }
                    if (const auto f2 = co->find("keyframes"); f2 != co->end() && f2->second.isArray()) {
                        for (const auto& kfv : f2->second.asArray()) {
                            if (kfv.isObject()) {
                                const auto& kfo = kfv.asObject();
                                core::Keyframe kf;
                                if (const auto st = kfo.find("seqTime"); st != kfo.end()) {
                                    auto r = rationalFromJson(st->second, "keyframe.seqTime");
                                    if (r.isOk()) kf.seqTime = r.value();
                                }
                                if (const auto op = kfo.find("opacity"); op != kfo.end() && (op->second.isDouble() || op->second.isInt())) {
                                    kf.opacity = op->second.isDouble() ? op->second.asDouble() : static_cast<double>(op->second.asInt());
                                }
                                if (const auto es = kfo.find("easing"); es != kfo.end() && es->second.isString()) {
                                    kf.easing = es->second.asString();
                                }
                                if (const auto tr = kfo.find("transform"); tr != kfo.end() && tr->second.isObject()) {
                                    const auto& tro = tr->second.asObject();
                                    if (const auto p = tro.find("scale"); p != tro.end() && (p->second.isDouble() || p->second.isInt())) {
                                        kf.transform.scale = p->second.isDouble() ? p->second.asDouble() : static_cast<double>(p->second.asInt());
                                    }
                                    if (const auto p = tro.find("x"); p != tro.end() && (p->second.isDouble() || p->second.isInt())) {
                                        kf.transform.x = p->second.isDouble() ? p->second.asDouble() : static_cast<double>(p->second.asInt());
                                    }
                                    if (const auto p = tro.find("y"); p != tro.end() && (p->second.isDouble() || p->second.isInt())) {
                                        kf.transform.y = p->second.isDouble() ? p->second.asDouble() : static_cast<double>(p->second.asInt());
                                    }
                                    if (const auto p = tro.find("rotationDeg"); p != tro.end() && (p->second.isDouble() || p->second.isInt())) {
                                        kf.transform.rotationDeg = p->second.isDouble() ? p->second.asDouble() : static_cast<double>(p->second.asInt());
                                    }
                                }
                                c.keyframes.push_back(std::move(kf));
                            }
                        }
                    }
                    if (const auto f2 = co->find("text"); f2 != co->end() && f2->second.isString()) {
                        c.text = f2->second.asString();
                    }
                    if (const auto f2 = co->find("fontFamily");
                        f2 != co->end() && f2->second.isString()) {
                        c.fontFamily = f2->second.asString();
                    }
                    if (const auto f2 = co->find("fontSizePt");
                        f2 != co->end() &&
                        (f2->second.isDouble() || f2->second.isInt())) {
                        c.fontSizePt = f2->second.isDouble()
                                           ? f2->second.asDouble()
                                           : static_cast<double>(f2->second.asInt());
                    }
                    seq.clips.emplace(c.id, std::move(c));
                }
            }
            if (const auto f = so->find("transitions"); f != so->end()) {
                const JsonArray* tarr = requireArray(f->second, err);
                if (tarr == nullptr) {
                    return core::Result<core::Project>::fail("transitions: " + err);
                }
                for (const auto& tv : *tarr) {
                    const JsonObject* to = requireObject(tv, err);
                    if (to == nullptr) {
                        return core::Result<core::Project>::fail("transition: " + err);
                    }
                    core::Transition tr;
                    auto tid = requireString(*to, "id");
                    if (tid.isErr()) return core::Result<core::Project>::fail(tid.error());
                    tr.id = tid.value();
                    if (const auto f2 = to->find("type"); f2 != to->end() && f2->second.isString()) {
                        tr.type = f2->second.asString();
                    }
                    if (const auto f2 = to->find("trackId"); f2 != to->end() && f2->second.isString()) {
                        tr.trackId = f2->second.asString();
                    }
                    if (const auto f2 = to->find("fromClipId"); f2 != to->end() && f2->second.isString()) {
                        tr.fromClipId = f2->second.asString();
                    }
                    if (const auto f2 = to->find("toClipId"); f2 != to->end() && f2->second.isString()) {
                        tr.toClipId = f2->second.asString();
                    }
                    if (const auto f2 = to->find("duration"); f2 != to->end()) {
                        auto r = rationalFromJson(f2->second, "transition.duration");
                        if (r.isErr()) return core::Result<core::Project>::fail(r.error());
                        tr.duration = r.value();
                    }
                    if (const auto f2 = to->find("alignment"); f2 != to->end() && f2->second.isString()) {
                        auto a = core::transitionAlignmentFromString(f2->second.asString());
                        if (a.isOk()) tr.alignment = a.value();
                    }
                    if (const auto f2 = to->find("easing"); f2 != to->end() && f2->second.isString()) {
                        tr.easing = f2->second.asString();
                    }
                    if (const auto f2 = to->find("params"); f2 != to->end() && f2->second.isObject()) {
                        for (const auto& [k, pv] : f2->second.asObject()) {
                            if (pv.isDouble()) tr.params[k] = pv.asDouble();
                            else if (pv.isInt()) tr.params[k] = static_cast<double>(pv.asInt());
                        }
                    }
                    seq.transitions.push_back(std::move(tr));
                }
            }
            project.sequences.push_back(std::move(seq));
        }
    }

    if (const auto r = project.validate(); r.isErr()) {
        return core::Result<core::Project>::fail("invalid project: " + r.error());
    }
    return core::Result<core::Project>::ok(std::move(project));
}

core::Result<void> saveProject(const core::Project& project, const std::string& path) {
    if (const auto r = project.validate(); r.isErr()) {
        return core::Result<void>::fail("cannot save invalid project: " + r.error());
    }
    // Persist a copy with project-relative asset paths; the caller's
    // in-memory document (absolute paths) is untouched.
    core::Project stored = project;
    for (auto& [id, asset] : stored.assets) {
        asset.path = storeAssetPath(path, asset.path);
    }
    auto json = projectToJson(stored);
    if (json.isErr()) {
        return core::Result<void>::fail(json.error());
    }
    return atomicWriteFile(path, dumpJson(json.value(), true));
}

core::Result<core::Project> loadProject(const std::string& path) {
    auto text = readFile(path);
    if (text.isErr()) {
        return core::Result<core::Project>::fail(text.error());
    }
    auto json = parseJson(text.value());
    if (json.isErr()) {
        return core::Result<core::Project>::fail("parse " + path + ": " + json.error());
    }
    auto project = projectFromJson(json.value());
    if (project.isErr()) {
        return core::Result<core::Project>::fail("load " + path + ": " + project.error());
    }
    for (auto& [id, asset] : project.value().assets) {
        asset.path = resolveAssetPath(path, asset.path);
    }
    return project;
}

std::string resolveAssetPath(const std::string& projectFile, const std::string& stored) {
    // NOTE: std::filesystem::path(std::string) uses the ANSI code page on
    // Windows, not UTF-8. ASCII/CWD-relative S1 fixtures are unaffected;
    // full Unicode-path support (u8string/wide) is tracked for S2 relink.
    namespace fs = std::filesystem;
    try {
        fs::path storedPath(stored);
        if (storedPath.is_absolute()) {
            return storedPath.lexically_normal().generic_string();
        }
        fs::path base(projectFile);
        if (!base.has_parent_path()) {
            return storedPath.lexically_normal().generic_string();
        }
        return (base.parent_path() / storedPath).lexically_normal().generic_string();
    } catch (...) {
        return stored;
    }
}

std::string storeAssetPath(const std::string& projectFile, const std::string& absolute) {
    namespace fs = std::filesystem;
    try {
        fs::path assetPath(absolute);
        if (!assetPath.is_absolute()) {
            return assetPath.lexically_normal().generic_string(); // keep as stored
        }
        fs::path base(projectFile);
        if (!base.has_parent_path()) {
            return assetPath.lexically_normal().generic_string();
        }
        std::error_code ec;
        fs::path rel = fs::relative(assetPath, base.parent_path(), ec);
        if (ec || rel.empty() || rel.is_absolute()) {
            return assetPath.lexically_normal().generic_string(); // other drive: absolute
        }
        return rel.lexically_normal().generic_string();
    } catch (...) {
        return absolute;
    }
}

} // namespace editor::persist
