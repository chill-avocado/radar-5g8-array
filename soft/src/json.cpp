//============================================================================
// json.cpp -- the JSON value declared in radar/json.hpp
//
// Recursive-descent parser, iterative-enough writer, no dependencies.  Two
// properties matter more than speed here:
//
//   1. Nothing throws.  A missing key gives the shared kNull instance, so a
//      chain of lookups down a path that does not exist still evaluates and
//      still hands back the caller's default.
//   2. Round trips are exact.  A document that is parsed and dumped and parsed
//      again gives the identical value, numbers included: every double is
//      written with the fewest digits that still read back bit for bit.
//============================================================================
#include "radar/json.hpp"

#include <cerrno>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace radar {

// The one shared null.  Every failed lookup returns a reference to this, which
// is why no accessor has to invent a value or throw.
const Json Json::kNull;

//----------------------------------------------------------------------------
// Structural accessors
//----------------------------------------------------------------------------
const Json& Json::operator[](const std::string& key) const {
    if (type_ != Type::Object) return kNull;
    auto it = obj_.find(key);
    return it == obj_.end() ? kNull : it->second;
}

const Json& Json::operator[](std::size_t i) const {
    if (type_ != Type::Array || i >= arr_.size()) return kNull;
    return arr_[i];
}

bool Json::has(const std::string& key) const {
    return type_ == Type::Object && obj_.find(key) != obj_.end();
}

std::size_t Json::size() const {
    if (type_ == Type::Array)  return arr_.size();
    if (type_ == Type::Object) return obj_.size();
    return 0;
}

void Json::set(const std::string& key, Json v) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        arr_.clear();
        str_.clear();
        num_  = 0.0;
        bool_ = false;
    }
    obj_[key] = std::move(v);
}

void Json::push(Json v) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        obj_.clear();
        str_.clear();
        num_  = 0.0;
        bool_ = false;
    }
    arr_.push_back(std::move(v));
}

//----------------------------------------------------------------------------
// Writing
//----------------------------------------------------------------------------
namespace {

/// Append `v` in the shortest form that reads back as the identical double.
/// JSON has no way to say infinity or not-a-number, so those become null --
/// the only legal representation, and the one every reader agrees on.
void write_number(std::string& out, double v) {
    if (!std::isfinite(v)) { out += "null"; return; }

    char buf[40];
    // Whole numbers inside the range a double represents exactly are written
    // without a decimal point, which keeps counts and bin indices readable.
    if (v == std::floor(v) && std::fabs(v) < 1.0e15) {
        std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(v));
        out += buf;
        return;
    }
    for (int prec = 15; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof buf, "%.*g", prec, v);
        if (std::strtod(buf, nullptr) == v) break;
    }
    out += buf;
}

void write_string(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    // Everything from 0x20 up, including UTF-8 continuation
                    // bytes, is legal JSON as it stands.
                    out += char(c);
                }
        }
    }
    out += '"';
}

void write_value(std::string& out, const Json& j, int indent, int level);

void write_children(std::string& out, const Json& j, int indent, int level) {
    const bool  pretty = indent > 0;
    const std::string pad     = pretty ? std::string(std::size_t(indent) * (level + 1), ' ') : std::string();
    const std::string pad_end = pretty ? std::string(std::size_t(indent) * level, ' ')       : std::string();

    if (j.type() == Json::Type::Array) {
        if (j.items().empty()) { out += "[]"; return; }
        out += '[';
        bool first = true;
        for (const Json& e : j.items()) {
            if (!first) out += ',';
            first = false;
            if (pretty) { out += '\n'; out += pad; }
            write_value(out, e, indent, level + 1);
        }
        if (pretty) { out += '\n'; out += pad_end; }
        out += ']';
    } else {
        if (j.members().empty()) { out += "{}"; return; }
        out += '{';
        bool first = true;
        for (const auto& kv : j.members()) {
            if (!first) out += ',';
            first = false;
            if (pretty) { out += '\n'; out += pad; }
            write_string(out, kv.first);
            out += ':';
            if (pretty) out += ' ';
            write_value(out, kv.second, indent, level + 1);
        }
        if (pretty) { out += '\n'; out += pad_end; }
        out += '}';
    }
}

void write_value(std::string& out, const Json& j, int indent, int level) {
    switch (j.type()) {
        case Json::Type::Null:   out += "null"; break;
        case Json::Type::Bool:   out += j.boolean() ? "true" : "false"; break;
        case Json::Type::Number: write_number(out, j.num()); break;
        case Json::Type::String: write_string(out, j.str()); break;
        case Json::Type::Array:
        case Json::Type::Object: write_children(out, j, indent, level); break;
    }
}

} // namespace

std::string Json::dump(int indent) const {
    std::string out;
    out.reserve(256);
    write_value(out, *this, indent, 0);
    return out;
}

bool Json::dump_file(const std::string& path, int indent) const {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    const std::string text = dump(indent);
    f.write(text.data(), std::streamsize(text.size()));
    f.put('\n');
    return bool(f);
}

//----------------------------------------------------------------------------
// Reading
//----------------------------------------------------------------------------
namespace {

/// Nesting limit.  A file with a thousand open brackets is not configuration,
/// it is an attempt to run the stack out, and it costs one counter to refuse.
constexpr int kMaxDepth = 200;

struct Parser {
    const std::string& s;
    std::size_t        i   = 0;
    std::string        err;

    explicit Parser(const std::string& text) : s(text) {}

    bool at_end() const { return i >= s.size(); }
    char peek()   const { return i < s.size() ? s[i] : '\0'; }

    /// Turn the current offset into something a person can act on.
    void fail(const std::string& what) {
        if (!err.empty()) return;      // keep the first, deepest failure
        std::size_t line = 1, col = 1;
        for (std::size_t k = 0; k < i && k < s.size(); ++k) {
            if (s[k] == '\n') { ++line; col = 1; } else { ++col; }
        }
        char buf[64];
        std::snprintf(buf, sizeof buf, " at line %zu column %zu", line, col);
        err = what + buf;
    }

    void skip_ws() {
        while (i < s.size()) {
            const char c = s[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++i;
            else break;
        }
    }

    bool literal(const char* word) {
        const std::size_t n = std::strlen(word);
        if (s.compare(i, n, word) != 0) return false;
        i += n;
        return true;
    }

    /// One UTF-16 code unit written as \uXXXX, already past the backslash-u.
    bool hex4(unsigned& out) {
        if (i + 4 > s.size()) return false;
        unsigned v = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = s[i + k];
            v <<= 4;
            if      (c >= '0' && c <= '9') v |= unsigned(c - '0');
            else if (c >= 'a' && c <= 'f') v |= unsigned(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= unsigned(c - 'A' + 10);
            else return false;
        }
        i += 4;
        out = v;
        return true;
    }

    static void append_utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out += char(cp);
        } else if (cp < 0x800) {
            out += char(0xC0 | (cp >> 6));
            out += char(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += char(0xE0 | (cp >> 12));
            out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        } else {
            out += char(0xF0 | (cp >> 18));
            out += char(0x80 | ((cp >> 12) & 0x3F));
            out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        }
    }

    bool parse_string(std::string& out) {
        if (peek() != '"') { fail("expected a string"); return false; }
        ++i;
        out.clear();
        while (true) {
            if (at_end()) { fail("string is never closed"); return false; }
            const unsigned char c = static_cast<unsigned char>(s[i]);
            if (c == '"') { ++i; return true; }
            if (c == '\\') {
                ++i;
                if (at_end()) { fail("string ends inside an escape"); return false; }
                const char e = s[i++];
                switch (e) {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    case 'u': {
                        unsigned cp = 0;
                        if (!hex4(cp)) { fail("\\u needs four hex digits"); return false; }
                        // A character outside the basic plane arrives as a
                        // high surrogate followed by a low one; join them back
                        // into the single code point they stand for.
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            if (i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                                const std::size_t save = i;
                                i += 2;
                                unsigned lo = 0;
                                if (hex4(lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                } else {
                                    i  = save;
                                    cp = 0xFFFD;   // lone high surrogate
                                }
                            } else {
                                cp = 0xFFFD;
                            }
                        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                            cp = 0xFFFD;           // lone low surrogate
                        }
                        append_utf8(out, cp);
                        break;
                    }
                    default:
                        fail("unknown escape in string");
                        return false;
                }
                continue;
            }
            if (c < 0x20) { fail("raw control character in string"); return false; }
            out += char(c);
            ++i;
        }
    }

    bool parse_number(double& out) {
        const std::size_t start = i;
        if (peek() == '-') ++i;
        if (peek() == '0') {
            ++i;
        } else if (peek() >= '1' && peek() <= '9') {
            while (peek() >= '0' && peek() <= '9') ++i;
        } else {
            fail("expected a number");
            return false;
        }
        if (peek() == '.') {
            ++i;
            if (!(peek() >= '0' && peek() <= '9')) { fail("digits must follow the decimal point"); return false; }
            while (peek() >= '0' && peek() <= '9') ++i;
        }
        if (peek() == 'e' || peek() == 'E') {
            ++i;
            if (peek() == '+' || peek() == '-') ++i;
            if (!(peek() >= '0' && peek() <= '9')) { fail("digits must follow the exponent"); return false; }
            while (peek() >= '0' && peek() <= '9') ++i;
        }
        // strtod on the validated span: the grammar check above has already
        // ruled out everything strtod would accept but JSON would not.
        const std::string span = s.substr(start, i - start);
        errno = 0;
        out = std::strtod(span.c_str(), nullptr);
        return true;
    }

    bool parse_value(Json& out, int depth) {
        if (depth > kMaxDepth) { fail("nesting is too deep"); return false; }
        skip_ws();
        if (at_end()) { fail("document ends where a value was expected"); return false; }

        switch (peek()) {
            case 'n':
                if (!literal("null"))  { fail("expected null"); return false; }
                out = Json();
                return true;
            case 't':
                if (!literal("true"))  { fail("expected true"); return false; }
                out = Json(true);
                return true;
            case 'f':
                if (!literal("false")) { fail("expected false"); return false; }
                out = Json(false);
                return true;
            case '"': {
                std::string v;
                if (!parse_string(v)) return false;
                out = Json(std::move(v));
                return true;
            }
            case '[': {
                ++i;
                out = Json::array();
                skip_ws();
                if (peek() == ']') { ++i; return true; }
                while (true) {
                    Json e;
                    if (!parse_value(e, depth + 1)) return false;
                    out.push(std::move(e));
                    skip_ws();
                    if (peek() == ',') { ++i; continue; }
                    if (peek() == ']') { ++i; return true; }
                    fail("expected a comma or a closing bracket");
                    return false;
                }
            }
            case '{': {
                ++i;
                out = Json::object();
                skip_ws();
                if (peek() == '}') { ++i; return true; }
                while (true) {
                    skip_ws();
                    std::string key;
                    if (!parse_string(key)) return false;
                    skip_ws();
                    if (peek() != ':') { fail("expected a colon after the key"); return false; }
                    ++i;
                    Json v;
                    if (!parse_value(v, depth + 1)) return false;
                    out.set(key, std::move(v));
                    skip_ws();
                    if (peek() == ',') { ++i; continue; }
                    if (peek() == '}') { ++i; return true; }
                    fail("expected a comma or a closing brace");
                    return false;
                }
            }
            default: {
                double v = 0;
                if (!parse_number(v)) return false;
                out = Json(v);
                return true;
            }
        }
    }
};

} // namespace

Json Json::parse(const std::string& text, std::string* err) {
    Parser p(text);
    Json   root;
    if (!p.parse_value(root, 0)) {
        if (err) *err = p.err;
        return Json();
    }
    p.skip_ws();
    if (!p.at_end()) {
        p.fail("unexpected text after the end of the document");
        if (err) *err = p.err;
        return Json();
    }
    if (err) err->clear();
    return root;
}

Json Json::parse_file(const std::string& path, std::string* err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (err) *err = "cannot open " + path;
        return Json();
    }
    std::ostringstream buf;
    buf << f.rdbuf();
    return parse(buf.str(), err);
}

} // namespace radar
