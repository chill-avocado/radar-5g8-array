//============================================================================
// json.hpp -- a small, dependency-free JSON value
//
// Used to read the array report the PCB generator emits, the radar config, the
// simulator scene and the calibration file, and to emit status for the web UI.
// Deliberately small: this is configuration and telemetry, not a hot path.
//
// Accessors never throw and never return a null reference.  Asking an object
// for a key it does not have gives a null Json, and asking a null Json for a
// number gives the default.  Configuration files are written by people, and a
// parser that aborts on a missing key is a parser that loses a night's data.
//============================================================================
#pragma once

#include "radar/core.hpp"

#include <initializer_list>
#include <map>
#include <string>
#include <vector>

namespace radar {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    Json(bool b)               : type_(Type::Bool),   bool_(b) {}
    Json(double d)             : type_(Type::Number), num_(d) {}
    Json(int i)                : type_(Type::Number), num_(double(i)) {}
    Json(long long i)          : type_(Type::Number), num_(double(i)) {}
    Json(unsigned long long i) : type_(Type::Number), num_(double(i)) {}
    Json(const char* s)        : type_(Type::String), str_(s) {}
    Json(std::string s)        : type_(Type::String), str_(std::move(s)) {}

    static Json array()  { Json j; j.type_ = Type::Array;  return j; }
    static Json object() { Json j; j.type_ = Type::Object; return j; }

    /// Parse. On malformed input returns a null Json and sets `err` if given.
    static Json parse(const std::string& text, std::string* err = nullptr);
    static Json parse_file(const std::string& path, std::string* err = nullptr);

    /// Serialise. indent == 0 gives one compact line.
    std::string dump(int indent = 0) const;
    bool        dump_file(const std::string& path, int indent = 2) const;

    Type type()      const { return type_; }
    bool is_null()   const { return type_ == Type::Null; }
    bool is_bool()   const { return type_ == Type::Bool; }
    bool is_num()    const { return type_ == Type::Number; }
    bool is_str()    const { return type_ == Type::String; }
    bool is_array()  const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    /// Object member. Returns a null Json when absent, so chaining is safe:
    ///   j["array"]["virtual_array_positions_mm"][0][1].num()
    const Json& operator[](const std::string& key) const;
    /// Array element. Returns a null Json when out of range.
    const Json& operator[](std::size_t i) const;

    bool        has(const std::string& key) const;
    std::size_t size() const;   ///< elements of an array, members of an object, else 0

    double      num(double def = 0.0) const     { return type_ == Type::Number ? num_ : def; }
    int         integer(int def = 0) const      { return type_ == Type::Number ? int(std::llround(num_)) : def; }
    std::string str(const std::string& def = "") const { return type_ == Type::String ? str_ : def; }
    bool        boolean(bool def = false) const { return type_ == Type::Bool ? bool_ : def; }

    /// Uniform templated accessor, for callers that prefer it. Same semantics.
    template <typename T> T get(T def = T{}) const;

    /// Building.
    void set(const std::string& key, Json v);
    void push(Json v);

    const std::vector<Json>&          items() const { return arr_; }
    const std::map<std::string, Json>& members() const { return obj_; }

private:
    Type                       type_ = Type::Null;
    bool                       bool_ = false;
    double                     num_  = 0.0;
    std::string                str_;
    std::vector<Json>          arr_;
    std::map<std::string, Json> obj_;

    static const Json          kNull;
};

template <> inline double      Json::get<double>(double def) const           { return num(def); }
template <> inline float       Json::get<float>(float def) const             { return float(num(def)); }
template <> inline int         Json::get<int>(int def) const                 { return integer(def); }
template <> inline bool        Json::get<bool>(bool def) const               { return boolean(def); }
template <> inline std::string Json::get<std::string>(std::string def) const { return str(def); }

} // namespace radar
