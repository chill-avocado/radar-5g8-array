//============================================================================
// json.hpp -- a small, self-contained JSON value type
//
// No dependencies beyond the standard library.  It has to be solid in both
// directions: it reads the board geometry that KiCad's array script emits and
// the operator's config file, and it writes the status blob the web UI polls.
//
// Objects keep their insertion order, so a file written by save_config() reads
// in the same order it was defined rather than alphabetically shuffled.
// Lookup is therefore linear, which is the right trade for configuration-sized
// documents and never appears in a hot loop.
//============================================================================
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace radar {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool v)               : t_(Type::Bool),   b_(v) {}
    Json(int v)                : t_(Type::Number), num_(double(v)) {}
    Json(long v)               : t_(Type::Number), num_(double(v)) {}
    Json(long long v)          : t_(Type::Number), num_(double(v)) {}
    Json(unsigned v)           : t_(Type::Number), num_(double(v)) {}
    Json(unsigned long v)      : t_(Type::Number), num_(double(v)) {}
    Json(unsigned long long v) : t_(Type::Number), num_(double(v)) {}
    Json(float v)              : t_(Type::Number), num_(double(v)) {}
    Json(double v)             : t_(Type::Number), num_(v) {}
    Json(const char* v)        : t_(Type::String), str_(v ? v : "") {}
    Json(std::string v)        : t_(Type::String), str_(std::move(v)) {}

    static Json array()  { Json j; j.t_ = Type::Array;  return j; }
    static Json object() { Json j; j.t_ = Type::Object; return j; }

    /// Parse a complete document.  Throws std::runtime_error with the line and
    /// column of the first thing that is not JSON.
    static Json parse(const std::string& text);

    /// Serialise.  indent == 0 gives one compact line; anything larger gives
    /// that many spaces of indentation per level.
    std::string dump(int indent = 0) const;

    Type type()      const { return t_; }
    bool is_null()   const { return t_ == Type::Null; }
    bool is_bool()   const { return t_ == Type::Bool; }
    bool is_number() const { return t_ == Type::Number; }
    bool is_string() const { return t_ == Type::String; }
    bool is_array()  const { return t_ == Type::Array; }
    bool is_object() const { return t_ == Type::Object; }

    /// True when this is an object that carries `key`.
    bool has(const std::string& key) const;

    /// Element count of an array or object; 0 for anything else.
    std::size_t size() const;

    //--------------------------------------------------------------------
    // Object access.  The mutable form creates the member (and turns a null
    // into an object); the const form returns a shared null for a missing key
    // so that chains like j["a"]["b"].get<double>(0) never fault.
    //--------------------------------------------------------------------
    Json&       operator[](const std::string& key);
    const Json& operator[](const std::string& key) const;
    Json&       operator[](const char* key)             { return (*this)[std::string(key)]; }
    const Json& operator[](const char* key) const       { return (*this)[std::string(key)]; }

    //--------------------------------------------------------------------
    // Array access.  The mutable form grows the array as needed.
    //--------------------------------------------------------------------
    Json&       operator[](std::size_t i);
    const Json& operator[](std::size_t i) const;
    Json&       operator[](int i)                       { return (*this)[std::size_t(i)]; }
    const Json& operator[](int i) const                 { return (*this)[std::size_t(i)]; }

    void push_back(Json v);
    void set(const std::string& key, Json v);
    void erase(const std::string& key);
    void clear();

    const std::vector<std::pair<std::string, Json>>& items()    const { return obj_; }
    const std::vector<Json>&                         elements() const { return arr_; }

    //--------------------------------------------------------------------
    // Typed reads.  Every one takes the value to use when this node is absent
    // or is the wrong type, so callers never have to test first.
    //--------------------------------------------------------------------
    double      as_double(double def = 0.0) const;
    long long   as_int(long long def = 0) const;
    bool        as_bool(bool def = false) const;
    std::string as_string(const std::string& def = std::string()) const;

    template <typename T>
    T get(T def = T()) const {
        if constexpr (std::is_same_v<T, std::string>)      return as_string(def);
        else if constexpr (std::is_same_v<T, bool>)        return as_bool(def);
        else if constexpr (std::is_same_v<T, double>)      return T(as_double(double(def)));
        else if constexpr (std::is_same_v<T, float>)       return T(as_double(double(def)));
        else                                               return T(as_int(static_cast<long long>(def)));
    }

    /// Member of this object, or `def` when absent.
    template <typename T>
    T get(const std::string& key, T def = T()) const {
        return has(key) ? (*this)[key].template get<T>(def) : def;
    }

private:
    Type        t_ = Type::Null;
    bool        b_ = false;
    double      num_ = 0.0;
    std::string str_;
    std::vector<Json>                         arr_;
    std::vector<std::pair<std::string, Json>> obj_;

    void dump_to(std::string& out, int indent, int level) const;
};

} // namespace radar
