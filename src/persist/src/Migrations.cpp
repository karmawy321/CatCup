#include "persist/Migrations.hpp"

#include "core/Model.hpp"

namespace editor::persist {

core::Result<int> migrateDocument(JsonValue& root) {
    if (!root.isObject()) {
        return core::Result<int>::fail("project document root must be an object");
    }
    auto& obj = std::get<JsonObject>(root.data);
    auto it = obj.find("schemaVersion");
    if (it == obj.end() || !it->second.isInt()) {
        return core::Result<int>::fail("project document is missing integer 'schemaVersion'");
    }
    int version = static_cast<int>(it->second.asInt());

    // v0 -> v1: early prototype files stored fps as a bare double and had no
    // text-track clip defaults. Normalize in place.
    if (version == 0) {
        if (const auto seqs = obj.find("sequences"); seqs != obj.end() && seqs->second.isArray()) {
            for (auto& seqVal : std::get<JsonArray>(seqs->second.data)) {
                if (!seqVal.isObject()) {
                    continue;
                }
                auto& seq = std::get<JsonObject>(seqVal.data);
                if (const auto fps = seq.find("fps"); fps != seq.end() && fps->second.isDouble()) {
                    const double d = fps->second.asDouble();
                    seq["fps"] = JsonObject{{"num", JsonValue(static_cast<std::int64_t>(d * 1000))},
                                            {"den", JsonValue(std::int64_t{1000})}};
                }
            }
        }
        version = 1;
        obj["schemaVersion"] = JsonValue(std::int64_t{1});
    }

    // v1 -> v2: Stage 2 everyday editor introduces transitions array on sequences.
    if (version == 1) {
        if (const auto seqs = obj.find("sequences"); seqs != obj.end() && seqs->second.isArray()) {
            for (auto& seqVal : std::get<JsonArray>(seqs->second.data)) {
                if (!seqVal.isObject()) {
                    continue;
                }
                auto& seq = std::get<JsonObject>(seqVal.data);
                if (seq.find("transitions") == seq.end()) {
                    seq["transitions"] = JsonArray{};
                }
            }
        }
        version = 2;
        obj["schemaVersion"] = JsonValue(std::int64_t{2});
    }

    if (version > core::kCurrentSchemaVersion) {
        return core::Result<int>::fail("project schema v" + std::to_string(version) +
                                       " is newer than this build (v" +
                                       std::to_string(core::kCurrentSchemaVersion) +
                                       "); update the editor to open it");
    }
    if (version < 1) {
        return core::Result<int>::fail("unsupported project schema v" + std::to_string(version));
    }
    return core::Result<int>::ok(version);
}

} // namespace editor::persist
