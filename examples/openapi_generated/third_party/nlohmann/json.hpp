#pragma once
// Extremely small, non-conformant stub of nlohmann::json sufficient for the
// generated example. It supports only a tiny subset: objects, arrays, strings,
// booleans, integers, and doubles; minimal parsing and dumping. Do NOT use in
// production.

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <variant>
#include <stdexcept>
#include <cctype>
#include <cstdint>

namespace nlohmann {

class json {
public:
    using object_t = std::map<std::string, json>;
    using array_t  = std::vector<json>;

private:
    using variant_t = std::variant<std::nullptr_t, object_t, array_t, std::string, bool, std::int64_t, std::uint64_t, double>;
    variant_t data_;

public:
    json() : data_(nullptr) {}
    json(std::nullptr_t) : data_(nullptr) {}
    json(object_t obj) : data_(std::move(obj)) {}
    json(array_t arr) : data_(std::move(arr)) {}
    json(const char* s) : data_(std::string(s)) {}
    json(std::string s) : data_(std::move(s)) {}
    json(bool b) : data_(b) {}
    json(std::int64_t i) : data_(i) {}
    json(std::uint64_t u) : data_(u) {}
    json(double d) : data_(d) {}

    static json object() { return json(object_t{}); }
    static json array()  { return json(array_t{}); }

    // Type checks
    bool is_object() const { return std::holds_alternative<object_t>(data_); }
    bool is_array()  const { return std::holds_alternative<array_t>(data_); }
    bool is_string() const { return std::holds_alternative<std::string>(data_); }
    bool is_boolean() const { return std::holds_alternative<bool>(data_); }
    bool is_number() const { return std::holds_alternative<std::int64_t>(data_) || std::holds_alternative<std::uint64_t>(data_) || std::holds_alternative<double>(data_); }
    bool is_null() const { return std::holds_alternative<std::nullptr_t>(data_); }

    // Object helpers
    bool contains(const std::string& k) const {
        if (!is_object()) return false;
        const auto& obj = std::get<object_t>(data_);
        return obj.find(k) != obj.end();
    }

    json& operator[](const std::string& k) {
        if (!is_object()) data_ = object_t{};
        auto& obj = std::get<object_t>(data_);
        return obj[k];
    }
    const json& at(const std::string& k) const {
        if (!is_object()) throw std::out_of_range("json: not an object");
        const auto& obj = std::get<object_t>(data_);
        auto it = obj.find(k);
        if (it == obj.end()) throw std::out_of_range("json: key not found");
        return it->second;
    }

    // Array helpers
    void push_back(json v) {
        if (!is_array()) data_ = array_t{};
        auto& arr = std::get<array_t>(data_);
        arr.push_back(std::move(v));
    }
    std::size_t size() const {
        if (is_array()) return std::get<array_t>(data_).size();
        if (is_object()) return std::get<object_t>(data_).size();
        return 0;
    }
    const json& at(std::size_t i) const {
        if (!is_array()) throw std::out_of_range("json: not an array");
        const auto& arr = std::get<array_t>(data_);
        if (i >= arr.size()) throw std::out_of_range("json: index out of range");
        return arr[i];
    }

    // Typed getters
    template<typename T>
    T get() const {
        if constexpr (std::is_same_v<T, std::string>) {
            if (!is_string()) throw std::runtime_error("json: not a string");
            return std::get<std::string>(data_);
        } else if constexpr (std::is_same_v<T, bool>) {
            if (!is_boolean()) throw std::runtime_error("json: not a bool");
            return std::get<bool>(data_);
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            if (std::holds_alternative<std::int64_t>(data_)) return std::get<std::int64_t>(data_);
            if (std::holds_alternative<std::uint64_t>(data_)) return static_cast<std::int64_t>(std::get<std::uint64_t>(data_));
            if (std::holds_alternative<double>(data_)) return static_cast<std::int64_t>(std::get<double>(data_));
            throw std::runtime_error("json: not a number");
        } else if constexpr (std::is_same_v<T, std::uint64_t>) {
            if (std::holds_alternative<std::uint64_t>(data_)) return std::get<std::uint64_t>(data_);
            if (std::holds_alternative<std::int64_t>(data_)) return static_cast<std::uint64_t>(std::get<std::int64_t>(data_));
            if (std::holds_alternative<double>(data_)) return static_cast<std::uint64_t>(std::get<double>(data_));
            throw std::runtime_error("json: not a number");
        } else if constexpr (std::is_same_v<T, double>) {
            if (std::holds_alternative<double>(data_)) return std::get<double>(data_);
            if (std::holds_alternative<std::int64_t>(data_)) return static_cast<double>(std::get<std::int64_t>(data_));
            if (std::holds_alternative<std::uint64_t>(data_)) return static_cast<double>(std::get<std::uint64_t>(data_));
            throw std::runtime_error("json: not a number");
        } else {
            static_assert(sizeof(T) == 0, "Unsupported json::get<T>() type");
        }
    }

    // Dump to compact JSON
    std::string dump() const {
        switch (data_.index()) {
            case 0: return "null";
            case 1: { // object
                const auto& obj = std::get<object_t>(data_);
                std::string out = "{";
                bool first = true;
                for (const auto& kv : obj) {
                    if (!first) out += ","; first = false;
                    out += '"' + escape(kv.first) + '"' + ':' + kv.second.dump();
                }
                out += '}';
                return out;
            }
            case 2: { // array
                const auto& arr = std::get<array_t>(data_);
                std::string out = "[";
                for (std::size_t i = 0; i < arr.size(); ++i) {
                    if (i) out += ",";
                    out += arr[i].dump();
                }
                out += "]";
                return out;
            }
            case 3: { // string
                return '"' + escape(std::get<std::string>(data_)) + '"';
            }
            case 4: { // bool
                return std::get<bool>(data_) ? "true" : "false";
            }
            case 5: { // int64
                return std::to_string(std::get<std::int64_t>(data_));
            }
            case 6: { // uint64
                return std::to_string(std::get<std::uint64_t>(data_));
            }
            case 7: { // double
                return std::to_string(std::get<double>(data_));
            }
            default: return "null";
        }
    }

    // Minimal parser (supports objects with string keys, arrays, strings, numbers, booleans, null)
    static json parse(const std::string& s) {
        const char* p = s.c_str();
        skip_ws(p);
        json v = parse_value(p);
        skip_ws(p);
        return v;
    }

private:
    static void skip_ws(const char*& p) {
        while (*p && std::isspace(static_cast<unsigned char>(*p))) ++p;
    }
    static std::string parse_string_raw(const char*& p) {
        if (*p != '"') throw std::runtime_error("json parse: expected string");
        ++p; std::string out;
        while (*p && *p != '"') {
            if (*p == '\\') {
                ++p; if (!*p) break; char c = *p++;
                switch (c) { case '"': out += '"'; break; case '\\': out += '\\'; break; case '/': out += '/'; break; case 'b': out += '\b'; break; case 'f': out += '\f'; break; case 'n': out += '\n'; break; case 'r': out += '\r'; break; case 'u': /* ignore unicode in stub */ out += '?'; for (int i=0;i<4 && *p;i++,++p){} break; default: out += c; break; }
            } else { out += *p++; }
        }
        if (*p == '"') ++p; else throw std::runtime_error("json parse: unterminated string");
        return out;
    }
    static json parse_value(const char*& p) {
        skip_ws(p);
        if (*p == '{') {
            ++p; object_t obj; skip_ws(p);
            if (*p == '}') { ++p; return json(obj); }
            while (*p) {
                skip_ws(p);
                std::string key = parse_string_raw(p);
                skip_ws(p);
                if (*p != ':') throw std::runtime_error("json parse: expected colon");
                ++p; skip_ws(p);
                json val = parse_value(p);
                obj.emplace(std::move(key), std::move(val));
                skip_ws(p);
                if (*p == ',') { ++p; continue; }
                if (*p == '}') { ++p; break; }
                throw std::runtime_error("json parse: expected , or }");
            }
            return json(obj);
        } else if (*p == '[') {
            ++p; array_t arr; skip_ws(p);
            if (*p == ']') { ++p; return json(arr); }
            while (*p) {
                json val = parse_value(p);
                arr.push_back(std::move(val));
                skip_ws(p);
                if (*p == ',') { ++p; continue; }
                if (*p == ']') { ++p; break; }
                throw std::runtime_error("json parse: expected , or ]");
            }
            return json(arr);
        } else if (*p == '"') {
            return json(parse_string_raw(p));
        } else if (std::strncmp(p, "true", 4) == 0) {
            p += 4; return json(true);
        } else if (std::strncmp(p, "false", 5) == 0) {
            p += 5; return json(false);
        } else if (std::strncmp(p, "null", 4) == 0) {
            p += 4; return json(nullptr);
        } else {
            // number
            const char* start = p; bool has_dot = false;
            if (*p == '-') ++p;
            while (std::isdigit(static_cast<unsigned char>(*p))) ++p;
            if (*p == '.') { has_dot = true; ++p; while (std::isdigit(static_cast<unsigned char>(*p))) ++p; }
            std::string num(start, p - start);
            if (num.empty() || num == "-" ) throw std::runtime_error("json parse: invalid number");
            try {
                if (has_dot) return json(std::stod(num));
                long long i = std::stoll(num);
                if (i < 0) return json(static_cast<std::int64_t>(i));
                return json(static_cast<std::uint64_t>(i));
            } catch (...) {
                throw std::runtime_error("json parse: invalid number");
            }
        }
    }
    static std::string escape(const std::string& s) {
        std::string out; out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        out += '?';
                    } else {
                        out += c;
                    }
            }
        }
        return out;
    }
};

} // namespace nlohmann
