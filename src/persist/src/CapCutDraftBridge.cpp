#include "persist/CapCutDraftBridge.hpp"

#include "core/Ids.hpp"
#include "persist/AtomicFile.hpp"
#include "persist/Json.hpp"

#include <cmath>
#include <iostream>
#include <sstream>

namespace editor::persist {
namespace {

constexpr int64_t kMicrosPerSec = 1000000;

int64_t rationalToMicros(const core::Rational& r) {
    if (r.den() == 0) return 0;
    return (r.num() * kMicrosPerSec) / r.den();
}

core::Rational microsToRational(int64_t micros) {
    return core::Rational(micros, kMicrosPerSec);
}

core::Rational doubleToRational(double d) {
    if (d <= 0.0) return core::Rational(1, 1);
    int64_t scaled = static_cast<int64_t>(std::round(d * 100000.0));
    return core::Rational(scaled, 100000);
}

std::string extractTextPayload(const std::string& content) {
    if (content.empty()) return "Text";
    auto parsed = parseJson(content);
    if (parsed.isOk() && parsed.value().isObject()) {
        const auto* t = parsed.value().find("text");
        if (t && t->isString()) {
            return t->asString();
        }
    }
    // Fallback simple search
    auto pos = content.find("\"text\":\"");
    if (pos != std::string::npos) {
        pos += 8;
        auto endPos = content.find("\"", pos);
        if (endPos != std::string::npos) {
            return content.substr(pos, endPos - pos);
        }
    }
    return content;
}

} // namespace

core::Result<core::Project> CapCutDraftBridge::importDraft(const std::string& draftJsonPath) {
    using R = core::Result<core::Project>;
    auto textRes = readFile(draftJsonPath);
    if (textRes.isErr()) {
        return R::fail("cannot read CapCut draft file: " + textRes.error());
    }

    auto jsonRes = parseJson(textRes.value());
    if (jsonRes.isErr()) {
        return R::fail("invalid JSON in CapCut draft: " + jsonRes.error());
    }

    const auto& rootVal = jsonRes.value();
    if (!rootVal.isObject()) {
        return R::fail("root is not an object");
    }

    core::Project proj;
    proj.name = "CapCut Imported";
    const auto* nameVal = rootVal.find("name");
    if (nameVal && nameVal->isString() && !nameVal->asString().empty()) {
        proj.name = nameVal->asString();
    }

    core::Sequence seq;
    seq.id = core::IdGenerator::make("seq");
    seq.name = "Sequence 1";
    seq.fps = core::Rational(30, 1);
    seq.width = 1920;
    seq.height = 1080;

    const auto* fpsVal = rootVal.find("fps");
    if (fpsVal) {
        if (fpsVal->isDouble() && fpsVal->asDouble() > 0) {
            seq.fps = doubleToRational(fpsVal->asDouble());
        } else if (fpsVal->isInt() && fpsVal->asInt() > 0) {
            seq.fps = core::Rational(fpsVal->asInt(), 1);
        }
    }

    const auto* canvasVal = rootVal.find("canvas_config");
    if (canvasVal && canvasVal->isObject()) {
        const auto* w = canvasVal->find("width");
        const auto* h = canvasVal->find("height");
        if (w && w->isInt()) seq.width = w->asInt();
        if (h && h->isInt()) seq.height = h->asInt();
    }

    // Parse materials
    struct TextInfo {
        std::string text;
        double fontSize = 48.0;
    };
    std::map<std::string, TextInfo> textMaterials;
    std::map<std::string, double> speedMaterials;

    const auto* materialsVal = rootVal.find("materials");
    if (materialsVal && materialsVal->isObject()) {
        // Videos
        const auto* vids = materialsVal->find("videos");
        if (vids && vids->isArray()) {
            for (const auto& v : vids->asArray()) {
                if (!v.isObject()) continue;
                const auto* id = v.find("id");
                const auto* p = v.find("path");
                if (id && id->isString() && p && p->isString()) {
                    core::Asset a;
                    a.id = id->asString();
                    a.kind = core::AssetKind::Video;
                    a.path = p->asString();
                    const auto* dur = v.find("duration");
                    if (dur && dur->isInt()) {
                        a.duration = microsToRational(dur->asInt());
                    }
                    const auto* w = v.find("width");
                    if (w && w->isInt()) a.width = w->asInt();
                    const auto* h = v.find("height");
                    if (h && h->isInt()) a.height = h->asInt();
                    const auto* ha = v.find("has_audio");
                    if (ha && ha->isBool()) a.hasAudio = ha->asBool();
                    proj.assets[a.id] = std::move(a);
                }
            }
        }

        // Audios
        const auto* auds = materialsVal->find("audios");
        if (auds && auds->isArray()) {
            for (const auto& aItem : auds->asArray()) {
                if (!aItem.isObject()) continue;
                const auto* id = aItem.find("id");
                const auto* p = aItem.find("path");
                if (id && id->isString() && p && p->isString()) {
                    core::Asset a;
                    a.id = id->asString();
                    a.kind = core::AssetKind::Audio;
                    a.path = p->asString();
                    a.hasAudio = true;
                    const auto* dur = aItem.find("duration");
                    if (dur && dur->isInt()) {
                        a.duration = microsToRational(dur->asInt());
                    }
                    proj.assets[a.id] = std::move(a);
                }
            }
        }

        // Texts
        const auto* texts = materialsVal->find("texts");
        if (texts && texts->isArray()) {
            for (const auto& tItem : texts->asArray()) {
                if (!tItem.isObject()) continue;
                const auto* id = tItem.find("id");
                if (id && id->isString()) {
                    TextInfo ti;
                    const auto* c = tItem.find("content");
                    if (c && c->isString()) {
                        ti.text = extractTextPayload(c->asString());
                    }
                    const auto* fs = tItem.find("font_size");
                    if (fs && fs->isDouble()) ti.fontSize = fs->asDouble();
                    else if (fs && fs->isInt()) ti.fontSize = static_cast<double>(fs->asInt());
                    textMaterials[id->asString()] = std::move(ti);
                }
            }
        }

        // Speeds
        const auto* speeds = materialsVal->find("speeds");
        if (speeds && speeds->isArray()) {
            for (const auto& sItem : speeds->asArray()) {
                if (!sItem.isObject()) continue;
                const auto* id = sItem.find("id");
                const auto* spd = sItem.find("speed");
                if (id && id->isString() && spd) {
                    double sVal = 1.0;
                    if (spd->isDouble()) sVal = spd->asDouble();
                    else if (spd->isInt()) sVal = static_cast<double>(spd->asInt());
                    speedMaterials[id->asString()] = sVal;
                }
            }
        }
    }

    // Parse Tracks & Segments
    const auto* tracksVal = rootVal.find("tracks");
    if (tracksVal && tracksVal->isArray()) {
        for (const auto& trkItem : tracksVal->asArray()) {
            if (!trkItem.isObject()) continue;

            std::string tType = "video";
            const auto* typeVal = trkItem.find("type");
            if (typeVal && typeVal->isString()) {
                tType = typeVal->asString();
            }

            core::Track trk;
            trk.id = core::IdGenerator::make("trk");
            const auto* tidVal = trkItem.find("id");
            if (tidVal && tidVal->isString() && !tidVal->asString().empty()) {
                trk.id = tidVal->asString();
            }

            if (tType == "audio") trk.kind = core::TrackKind::Audio;
            else if (tType == "text") trk.kind = core::TrackKind::Text;
            else trk.kind = core::TrackKind::Video;

            const auto* segsVal = trkItem.find("segments");
            if (segsVal && segsVal->isArray()) {
                for (const auto& segItem : segsVal->asArray()) {
                    if (!segItem.isObject()) continue;
                    core::Clip clip;
                    clip.id = core::IdGenerator::make("clip");
                    const auto* sid = segItem.find("id");
                    if (sid && sid->isString()) clip.id = sid->asString();

                    const auto* matIdVal = segItem.find("material_id");
                    if (matIdVal && matIdVal->isString()) {
                        clip.assetId = matIdVal->asString();
                    }

                    // Target range (timeline placement)
                    const auto* tgt = segItem.find("target_timerange");
                    if (tgt && tgt->isObject()) {
                        const auto* s = tgt->find("start");
                        const auto* d = tgt->find("duration");
                        if (s && s->isInt()) clip.seqStart = microsToRational(s->asInt());
                        if (d && d->isInt()) {
                            clip.sourceOut = clip.sourceIn + microsToRational(d->asInt());
                        }
                    }

                    // Source range
                    const auto* src = segItem.find("source_timerange");
                    if (src && src->isObject()) {
                        const auto* s = src->find("start");
                        const auto* d = src->find("duration");
                        if (s && s->isInt()) clip.sourceIn = microsToRational(s->asInt());
                        if (d && d->isInt()) {
                            clip.sourceOut = clip.sourceIn + microsToRational(d->asInt());
                        }
                    }

                    // Speed
                    const auto* spdId = segItem.find("speed_id");
                    if (spdId && spdId->isString()) {
                        auto sit = speedMaterials.find(spdId->asString());
                        if (sit != speedMaterials.end() && sit->second > 0.0) {
                            clip.speed = doubleToRational(sit->second);
                        }
                    }
                    const auto* spVal = segItem.find("speed");
                    if (spVal) {
                        if (spVal->isDouble() && spVal->asDouble() > 0) {
                            clip.speed = doubleToRational(spVal->asDouble());
                        } else if (spVal->isInt() && spVal->asInt() > 0) {
                            clip.speed = core::Rational(spVal->asInt(), 1);
                        }
                    }

                    // Transform and opacity
                    const auto* clipObj = segItem.find("clip");
                    if (clipObj && clipObj->isObject()) {
                        const auto* sc = clipObj->find("scale");
                        if (sc && sc->isObject()) {
                            const auto* scX = sc->find("x");
                            if (scX && scX->isDouble()) clip.transform.scale = scX->asDouble();
                        }
                        const auto* rot = clipObj->find("rotation");
                        if (rot && rot->isDouble()) clip.transform.rotationDeg = rot->asDouble();
                        const auto* tr = clipObj->find("transform");
                        if (tr && tr->isObject()) {
                            const auto* tx = tr->find("x");
                            const auto* ty = tr->find("y");
                            if (tx && tx->isDouble()) clip.transform.x = tx->asDouble();
                            if (ty && ty->isDouble()) clip.transform.y = ty->asDouble();
                        }
                        const auto* alp = clipObj->find("alpha");
                        if (alp && alp->isDouble()) clip.opacity = alp->asDouble();
                    }

                    // Text clip properties
                    if (trk.kind == core::TrackKind::Text && !clip.assetId.empty()) {
                        auto tIt = textMaterials.find(clip.assetId);
                        if (tIt != textMaterials.end()) {
                            clip.text = tIt->second.text;
                            clip.fontSizePt = tIt->second.fontSize;
                        }
                        clip.assetId.clear();
                    }

                    trk.clipIds.push_back(clip.id);
                    seq.clips[clip.id] = std::move(clip);
                }
            }
            seq.tracks.push_back(std::move(trk));
        }
    }

    proj.sequences.push_back(std::move(seq));
    proj.activeSequenceId = proj.sequences.front().id;
    return R::ok(std::move(proj));
}

core::Result<void> CapCutDraftBridge::exportDraft(const core::Project& project,
                                                  const std::string& draftJsonPath) {
    using R = core::Result<void>;
    if (project.sequences.empty()) {
        return R::fail("no sequences to export to CapCut draft");
    }

    const auto& seq = project.sequences.front();
    JsonObject root;
    root["id"] = JsonValue(core::IdGenerator::make("draft"));
    root["version"] = JsonValue(static_cast<int64_t>(360000));
    root["new_version"] = JsonValue("185.0.0");
    root["name"] = JsonValue(project.name);
    root["fps"] = JsonValue(static_cast<double>(seq.fps));

    core::Rational seqDur(0, 1);
    for (const auto& [_, clip] : seq.clips) {
        core::Rational end = clip.seqStart + clip.seqDuration();
        if (end > seqDur) seqDur = end;
    }
    root["duration"] = JsonValue(rationalToMicros(seqDur));

    JsonObject canvas;
    canvas["width"] = JsonValue(static_cast<int64_t>(seq.width));
    canvas["height"] = JsonValue(static_cast<int64_t>(seq.height));
    canvas["ratio"] = JsonValue("original");
    root["canvas_config"] = JsonValue(std::move(canvas));

    // Materials
    JsonObject materials;
    JsonArray vids, auds, texts;

    for (const auto& [aid, a] : project.assets) {
        if (a.kind == core::AssetKind::Video) {
            JsonObject vo;
            vo["id"] = JsonValue(aid);
            vo["path"] = JsonValue(a.path);
            vo["duration"] = JsonValue(rationalToMicros(a.duration));
            vo["width"] = JsonValue(static_cast<int64_t>(a.width));
            vo["height"] = JsonValue(static_cast<int64_t>(a.height));
            vo["has_audio"] = JsonValue(a.hasAudio);
            vids.push_back(JsonValue(std::move(vo)));
        } else if (a.kind == core::AssetKind::Audio) {
            JsonObject ao;
            ao["id"] = JsonValue(aid);
            ao["path"] = JsonValue(a.path);
            ao["duration"] = JsonValue(rationalToMicros(a.duration));
            auds.push_back(JsonValue(std::move(ao)));
        }
    }

    // Text clips
    for (const auto& [cid, c] : seq.clips) {
        if (!c.text.empty()) {
            JsonObject to;
            to["id"] = JsonValue(cid);
            to["content"] = JsonValue("{\"text\":\"" + c.text + "\"}");
            to["font_size"] = JsonValue(c.fontSizePt);
            texts.push_back(JsonValue(std::move(to)));
        }
    }

    materials["videos"] = JsonValue(std::move(vids));
    materials["audios"] = JsonValue(std::move(auds));
    materials["texts"] = JsonValue(std::move(texts));
    root["materials"] = JsonValue(std::move(materials));

    // Tracks
    JsonArray tracks;
    for (const auto& trk : seq.tracks) {
        JsonObject to;
        to["id"] = JsonValue(trk.id);
        to["type"] = JsonValue(trk.kind == core::TrackKind::Video ? "video"
                              : (trk.kind == core::TrackKind::Audio ? "audio" : "text"));
        JsonArray segs;
        for (const auto& cid : trk.clipIds) {
            auto cit = seq.clips.find(cid);
            if (cit == seq.clips.end()) continue;
            const auto& c = cit->second;

            JsonObject so;
            so["id"] = JsonValue(c.id);
            so["material_id"] = JsonValue(c.text.empty() ? c.assetId : c.id);

            JsonObject tgt;
            tgt["start"] = JsonValue(rationalToMicros(c.seqStart));
            tgt["duration"] = JsonValue(rationalToMicros(c.seqDuration()));
            so["target_timerange"] = JsonValue(std::move(tgt));

            JsonObject src;
            src["start"] = JsonValue(rationalToMicros(c.sourceIn));
            src["duration"] = JsonValue(rationalToMicros(c.sourceOut - c.sourceIn));
            so["source_timerange"] = JsonValue(std::move(src));

            so["speed"] = JsonValue(static_cast<double>(c.speed));

            JsonObject clipObj;
            JsonObject sc;
            sc["x"] = JsonValue(c.transform.scale);
            sc["y"] = JsonValue(c.transform.scale);
            clipObj["scale"] = JsonValue(std::move(sc));
            clipObj["rotation"] = JsonValue(c.transform.rotationDeg);
            JsonObject tr;
            tr["x"] = JsonValue(c.transform.x);
            tr["y"] = JsonValue(c.transform.y);
            clipObj["transform"] = JsonValue(std::move(tr));
            clipObj["alpha"] = JsonValue(c.opacity);
            so["clip"] = JsonValue(std::move(clipObj));

            segs.push_back(JsonValue(std::move(so)));
        }
        to["segments"] = JsonValue(std::move(segs));
        tracks.push_back(JsonValue(std::move(to)));
    }
    root["tracks"] = JsonValue(std::move(tracks));

    std::string dumped = dumpJson(JsonValue(std::move(root)), true);
    return atomicWriteFile(draftJsonPath, dumped);
}

} // namespace editor::persist
