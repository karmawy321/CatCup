#pragma once

// Minimal self-contained JSON value + parser + writer.
//
// Deliberate Stage 0 choice: the project format only needs objects, arrays,
// strings, integers, doubles and booleans. Depending on nothing keeps `core`
// and `persist` buildable offline with MSVC alone. If the schema outgrows
// this (or performance demands it), swap the implementation behind this same
// header for a pinned third-party parser — callers must not depend on the
// implementation details below beyond this interface.

#include "core/Result.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace editor::persist {

struct JsonValue;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

struct JsonValue {
    using Storage = std::variant<std::nullptr_t, bool, std::int64_t, double, std::string,
                                 JsonArray, JsonObject>;
    Storage data = nullptr;

    JsonValue() = default;
    JsonValue(std::nullptr_t) : data(nullptr) {}
    JsonValue(bool b) : data(b) {}
    JsonValue(std::int64_t i) : data(i) {}
    JsonValue(int i) : data(static_cast<std::int64_t>(i)) {}
    JsonValue(double d) : data(d) {}
    JsonValue(const char* s) : data(std::string(s)) {}
    JsonValue(std::string s) : data(std::move(s)) {}
    JsonValue(JsonArray a) : data(std::move(a)) {}
    JsonValue(JsonObject o) : data(std::move(o)) {}

    [[nodiscard]] bool isNull() const noexcept { return std::holds_alternative<std::nullptr_t>(data); }
    [[nodiscard]] bool isBool() const noexcept { return std::holds_alternative<bool>(data); }
    [[nodiscard]] bool isInt() const noexcept { return std::holds_alternative<std::int64_t>(data); }
    [[nodiscard]] bool isDouble() const noexcept { return std::holds_alternative<double>(data); }
    [[nodiscard]] bool isString() const noexcept { return std::holds_alternative<std::string>(data); }
    [[nodiscard]] bool isArray() const noexcept { return std::holds_alternative<JsonArray>(data); }
    [[nodiscard]] bool isObject() const noexcept { return std::holds_alternative<JsonObject>(data); }

    [[nodiscard]] const JsonObject& asObject() const { return std::get<JsonObject>(data); }
    [[nodiscard]] const JsonArray& asArray() const { return std::get<JsonArray>(data); }
    [[nodiscard]] const std::string& asString() const { return std::get<std::string>(data); }
    [[nodiscard]] std::int64_t asInt() const { return std::get<std::int64_t>(data); }
    [[nodiscard]] double asDouble() const { return std::get<double>(data); }
    [[nodiscard]] bool asBool() const { return std::get<bool>(data); }

    [[nodiscard]] const JsonValue* find(const std::string& key) const;
};

core::Result<JsonValue> parseJson(const std::string& text);
std::string dumpJson(const JsonValue& value, bool pretty = true);

// Field helpers with explicit error messages (schema debugging aid).
core::Result<std::int64_t> requireInt(const JsonObject& obj, const std::string& key);
core::Result<std::string> requireString(const JsonObject& obj, const std::string& key);
core::Result<double> requireDouble(const JsonObject& obj, const std::string& key);
core::Result<bool> requireBool(const JsonObject& obj, const std::string& key);
const JsonObject* requireObject(const JsonValue& v, std::string& err);
const JsonArray* requireArray(const JsonValue& v, std::string& err);

} // namespace editor::persist
