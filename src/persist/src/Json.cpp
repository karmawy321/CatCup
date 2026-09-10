#include "persist/Json.hpp"

#include <cctype>
#include <cstdio>
#include <sstream>

namespace editor::persist {

namespace {

struct Parser {
    const char* p;
    const char* end;
    std::string error;

    bool eof() const { return p >= end; }

    void skipWs() {
        while (!eof() && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
            ++p;
        }
    }

    bool consume(char c) {
        if (!eof() && *p == c) {
            ++p;
            return true;
        }
        return false;
    }

    bool consumeLiteral(const char* lit) {
        const char* q = p;
        for (const char* l = lit; *l != '\0'; ++l, ++q) {
            if (q >= end || *q != *l) {
                return false;
            }
        }
        p = q;
        return true;
    }

    core::Result<JsonValue> parseValue() {
        skipWs();
        if (eof()) {
            return core::Result<JsonValue>::fail("unexpected end of JSON");
        }
        switch (*p) {
        case '{': return parseObject();
        case '[': return parseArray();
        case '"': {
            auto s = parseString();
            if (s.isErr()) {
                return core::Result<JsonValue>::fail(s.error());
            }
            return core::Result<JsonValue>::ok(JsonValue(std::move(s.value())));
        }
        case 't':
            if (consumeLiteral("true")) {
                return core::Result<JsonValue>::ok(JsonValue(true));
            }
            break;
        case 'f':
            if (consumeLiteral("false")) {
                return core::Result<JsonValue>::ok(JsonValue(false));
            }
            break;
        case 'n':
            if (consumeLiteral("null")) {
                return core::Result<JsonValue>::ok(JsonValue(nullptr));
            }
            break;
        default:
            if (*p == '-' || (*p >= '0' && *p <= '9')) {
                return parseNumber();
            }
            break;
        }
        return core::Result<JsonValue>::fail(std::string("unexpected character '") + *p + "'");
    }

    core::Result<JsonValue> parseObject() {
        ++p; // {
        JsonObject obj;
        skipWs();
        if (consume('}')) {
            return core::Result<JsonValue>::ok(JsonValue(std::move(obj)));
        }
        while (true) {
            skipWs();
            if (eof() || *p != '"') {
                return core::Result<JsonValue>::fail("expected string key in object");
            }
            auto key = parseString();
            if (key.isErr()) {
                return core::Result<JsonValue>::fail(key.error());
            }
            skipWs();
            if (!consume(':')) {
                return core::Result<JsonValue>::fail("expected ':' in object");
            }
            auto val = parseValue();
            if (val.isErr()) {
                return val;
            }
            obj.emplace(std::move(key.value()), std::move(val.value()));
            skipWs();
            if (consume('}')) {
                break;
            }
            if (!consume(',')) {
                return core::Result<JsonValue>::fail("expected ',' or '}' in object");
            }
        }
        return core::Result<JsonValue>::ok(JsonValue(std::move(obj)));
    }

    core::Result<JsonValue> parseArray() {
        ++p; // [
        JsonArray arr;
        skipWs();
        if (consume(']')) {
            return core::Result<JsonValue>::ok(JsonValue(std::move(arr)));
        }
        while (true) {
            auto val = parseValue();
            if (val.isErr()) {
                return val;
            }
            arr.push_back(std::move(val.value()));
            skipWs();
            if (consume(']')) {
                break;
            }
            if (!consume(',')) {
                return core::Result<JsonValue>::fail("expected ',' or ']' in array");
            }
        }
        return core::Result<JsonValue>::ok(JsonValue(std::move(arr)));
    }

    core::Result<std::string> parseString() {
        ++p; // opening quote
        std::string out;
        while (!eof()) {
            const char c = *p++;
            if (c == '"') {
                return core::Result<std::string>::ok(std::move(out));
            }
            if (c == '\\') {
                if (eof()) {
                    break;
                }
                const char e = *p++;
                switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    // Basic \uXXXX support incl. surrogate pairs for emoji/CJK.
                    if (end - p < 4) {
                        return core::Result<std::string>::fail("truncated \\u escape");
                    }
                    unsigned code = 0;
                    for (int i = 0; i < 4; ++i) {
                        const char h = p[i];
                        code <<= 4;
                        if (h >= '0' && h <= '9') {
                            code |= static_cast<unsigned>(h - '0');
                        } else if (h >= 'a' && h <= 'f') {
                            code |= static_cast<unsigned>(h - 'a' + 10);
                        } else if (h >= 'A' && h <= 'F') {
                            code |= static_cast<unsigned>(h - 'A' + 10);
                        } else {
                            return core::Result<std::string>::fail("invalid \\u escape");
                        }
                    }
                    p += 4;
                    if (code >= 0xD800 && code <= 0xDBFF && end - p >= 6 && p[0] == '\\' &&
                        p[1] == 'u') {
                        unsigned lo = 0;
                        bool ok = true;
                        for (int i = 0; i < 4; ++i) {
                            const char h = p[2 + i];
                            lo <<= 4;
                            if (h >= '0' && h <= '9') {
                                lo |= static_cast<unsigned>(h - '0');
                            } else if (h >= 'a' && h <= 'f') {
                                lo |= static_cast<unsigned>(h - 'a' + 10);
                            } else if (h >= 'A' && h <= 'F') {
                                lo |= static_cast<unsigned>(h - 'A' + 10);
                            } else {
                                ok = false;
                            }
                        }
                        if (ok && lo >= 0xDC00 && lo <= 0xDFFF) {
                            p += 6;
                            code = 0x10000 + ((code - 0xD800) << 10) + (lo - 0xDC00);
                        }
                    }
                    // Encode UTF-8.
                    if (code < 0x80) {
                        out += static_cast<char>(code);
                    } else if (code < 0x800) {
                        out += static_cast<char>(0xC0 | (code >> 6));
                        out += static_cast<char>(0x80 | (code & 0x3F));
                    } else if (code < 0x10000) {
                        out += static_cast<char>(0xE0 | (code >> 12));
                        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (code & 0x3F));
                    } else {
                        out += static_cast<char>(0xF0 | (code >> 18));
                        out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
                        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (code & 0x3F));
                    }
                    break;
                }
                default: return core::Result<std::string>::fail("invalid escape");
                }
            } else {
                out += c;
            }
        }
        return core::Result<std::string>::fail("unterminated string");
    }

    core::Result<JsonValue> parseNumber() {
        const char* start = p;
        if (!eof() && *p == '-') {
            ++p;
        }
        while (!eof() && *p >= '0' && *p <= '9') {
            ++p;
        }
        bool isDouble = false;
        if (!eof() && *p == '.') {
            isDouble = true;
            ++p;
            while (!eof() && *p >= '0' && *p <= '9') {
                ++p;
            }
        }
        if (!eof() && (*p == 'e' || *p == 'E')) {
            isDouble = true;
            ++p;
            if (!eof() && (*p == '+' || *p == '-')) {
                ++p;
            }
            while (!eof() && *p >= '0' && *p <= '9') {
                ++p;
            }
        }
        const std::string tok(start, p);
        try {
            if (isDouble) {
                return core::Result<JsonValue>::ok(JsonValue(std::stod(tok)));
            }
            return core::Result<JsonValue>::ok(JsonValue(static_cast<std::int64_t>(std::stoll(tok))));
        } catch (...) {
            return core::Result<JsonValue>::fail("invalid number: " + tok);
        }
    }
};

void writeEscaped(std::string& out, const std::string& s) {
    out += '"';
    for (const char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out += c;
            }
            break;
        }
    }
    out += '"';
}

void dumpInto(std::string& out, const JsonValue& v, bool pretty, int indent) {
    const std::string pad(static_cast<std::size_t>(indent) * 2, ' ');
    const std::string padChild(static_cast<std::size_t>(indent + 1) * 2, ' ');
    if (v.isNull()) {
        out += "null";
    } else if (v.isBool()) {
        out += v.asBool() ? "true" : "false";
    } else if (v.isInt()) {
        out += std::to_string(v.asInt());
    } else if (v.isDouble()) {
        std::ostringstream os;
        os.precision(17);
        os << v.asDouble();
        out += os.str();
    } else if (v.isString()) {
        writeEscaped(out, v.asString());
    } else if (v.isArray()) {
        const auto& arr = v.asArray();
        if (arr.empty()) {
            out += "[]";
            return;
        }
        out += '[';
        if (pretty) {
            out += '\n';
        }
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (pretty) {
                out += padChild;
            }
            dumpInto(out, arr[i], pretty, indent + 1);
            if (i + 1 < arr.size()) {
                out += ',';
            }
            if (pretty) {
                out += '\n';
            }
        }
        if (pretty) {
            out += pad;
        }
        out += ']';
    } else {
        const auto& obj = v.asObject();
        if (obj.empty()) {
            out += "{}";
            return;
        }
        out += '{';
        if (pretty) {
            out += '\n';
        }
        std::size_t i = 0;
        for (const auto& [k, val] : obj) {
            if (pretty) {
                out += padChild;
            }
            writeEscaped(out, k);
            out += pretty ? ": " : ":";
            dumpInto(out, val, pretty, indent + 1);
            if (++i < obj.size()) {
                out += ',';
            }
            if (pretty) {
                out += '\n';
            }
        }
        if (pretty) {
            out += pad;
        }
        out += '}';
    }
}

} // namespace

const JsonValue* JsonValue::find(const std::string& key) const {
    if (!isObject()) {
        return nullptr;
    }
    const auto& obj = asObject();
    const auto it = obj.find(key);
    return it == obj.end() ? nullptr : &it->second;
}

core::Result<JsonValue> parseJson(const std::string& text) {
    Parser parser{text.data(), text.data() + text.size(), {}};
    auto v = parser.parseValue();
    if (v.isErr()) {
        return v;
    }
    parser.skipWs();
    if (!parser.eof()) {
        return core::Result<JsonValue>::fail("trailing characters after JSON document");
    }
    return v;
}

std::string dumpJson(const JsonValue& value, bool pretty) {
    std::string out;
    dumpInto(out, value, pretty, 0);
    if (pretty) {
        out += '\n';
    }
    return out;
}

core::Result<std::int64_t> requireInt(const JsonObject& obj, const std::string& key) {
    const auto it = obj.find(key);
    if (it == obj.end()) {
        return core::Result<std::int64_t>::fail("missing int field '" + key + "'");
    }
    if (!it->second.isInt()) {
        return core::Result<std::int64_t>::fail("field '" + key + "' is not an integer");
    }
    return core::Result<std::int64_t>::ok(it->second.asInt());
}

core::Result<std::string> requireString(const JsonObject& obj, const std::string& key) {
    const auto it = obj.find(key);
    if (it == obj.end()) {
        return core::Result<std::string>::fail("missing string field '" + key + "'");
    }
    if (!it->second.isString()) {
        return core::Result<std::string>::fail("field '" + key + "' is not a string");
    }
    return core::Result<std::string>::ok(it->second.asString());
}

core::Result<double> requireDouble(const JsonObject& obj, const std::string& key) {
    const auto it = obj.find(key);
    if (it == obj.end()) {
        return core::Result<double>::fail("missing double field '" + key + "'");
    }
    if (it->second.isDouble()) {
        return core::Result<double>::ok(it->second.asDouble());
    }
    if (it->second.isInt()) {
        return core::Result<double>::ok(static_cast<double>(it->second.asInt()));
    }
    return core::Result<double>::fail("field '" + key + "' is not a number");
}

core::Result<bool> requireBool(const JsonObject& obj, const std::string& key) {
    const auto it = obj.find(key);
    if (it == obj.end()) {
        return core::Result<bool>::fail("missing bool field '" + key + "'");
    }
    if (!it->second.isBool()) {
        return core::Result<bool>::fail("field '" + key + "' is not a boolean");
    }
    return core::Result<bool>::ok(it->second.asBool());
}

const JsonObject* requireObject(const JsonValue& v, std::string& err) {
    if (!v.isObject()) {
        err = "expected JSON object";
        return nullptr;
    }
    return &v.asObject();
}

const JsonArray* requireArray(const JsonValue& v, std::string& err) {
    if (!v.isArray()) {
        err = "expected JSON array";
        return nullptr;
    }
    return &v.asArray();
}

} // namespace editor::persist
