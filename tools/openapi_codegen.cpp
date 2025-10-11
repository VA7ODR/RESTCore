#include <algorithm>
#include <cctype>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <filesystem>
#include <cstring>
// For JSON parsing of OpenAPI specs and intermediate canonical model
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace {

std::string trim(std::string s) {
    auto not_space = [](int ch){ return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

std::string collapse_underscores(std::string s) {
    std::string out; out.reserve(s.size());
    bool prev_us = false;
    for (char ch : s) {
        if (ch == '_') {
            if (!prev_us) out.push_back('_');
            prev_us = true;
        } else {
            out.push_back(ch);
            prev_us = false;
        }
    }
    return out;
}

bool is_cpp_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}
bool is_cpp_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool is_cpp_keyword(const std::string& s) {
    static const std::set<std::string> kws = {
        "alignas","alignof","and","and_eq","asm","atomic_cancel","atomic_commit","atomic_noexcept",
        "auto","bitand","bitor","bool","break","case","catch","char","char8_t","char16_t","char32_t",
        "class","compl","concept","const","consteval","constexpr","constinit","const_cast","continue","co_await",
        "co_return","co_yield","decltype","default","delete","do","double","dynamic_cast","else","enum","explicit",
        "export","extern","false","float","for","friend","goto","if","inline","int","long","mutable","namespace",
        "new","noexcept","not","not_eq","nullptr","operator","or","or_eq","private","protected","public","reflexpr",
        "register","reinterpret_cast","requires","return","short","signed","sizeof","static","static_assert","static_cast",
        "struct","switch","synchronized","template","this","thread_local","throw","true","try","typedef","typeid","typename",
        "union","unsigned","using","virtual","void","volatile","wchar_t","while","xor","xor_eq"
    };
    return kws.count(s) > 0;
}

std::string sanitize_api_name(std::string name) {
    name = trim(name);
    if (name.empty()) name = "Api";

    // Replace non-alnum with underscore
    for (char& ch : name) {
        if (!std::isalnum(static_cast<unsigned char>(ch))) ch = '_';
    }
    name = collapse_underscores(name);

    // Ensure first char is valid
    if (!name.empty() && !is_cpp_ident_start(name.front())) {
        name.insert(name.begin(), '_');
    }

    // Truncate to 64 chars
    if (name.size() > 64) name.resize(64);

    if (is_cpp_keyword(name)) name += "_API";

    if (name.empty()) name = "Api";
    return name;
}

std::optional<std::string> read_file(const fs::path& p) {
    std::ifstream ifs(p, std::ios::in | std::ios::binary);
    if (!ifs) return std::nullopt;
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

std::optional<std::string> derive_name_from_spec(const fs::path& input) {
    auto data = read_file(input);
    if (!data) return std::nullopt;
    const std::string& s = *data;

    // Try JSON: find info.title
    try {
        std::regex re_json(R"REGEX(\"info\"\s*:\s*\{[^}]*\"title\"\s*:\s*\"([^\"]+)\")REGEX",
                            std::regex::ECMAScript);
        std::smatch m;
        if (std::regex_search(s, m, re_json)) {
            return trim(m[1].str());
        }
    } catch (...) {}

    // Try YAML-ish: title: <text> near 'info:' block
    try {
        std::regex re_info(R"REGEX(^info\s*:\s*$)REGEX", std::regex::icase | std::regex::multiline);
        std::regex re_title(R"REGEX(^\s*title\s*:\s*(.+?)\s*$)REGEX", std::regex::icase | std::regex::multiline);
        std::smatch m_info;
        if (std::regex_search(s, m_info, re_info)) {
            auto pos = m_info.position();
            auto tail = s.substr(static_cast<size_t>(pos));
            std::smatch m_title;
            if (std::regex_search(tail, m_title, re_title)) {
                auto t = m_title[1].str();
                // strip quotes if present
                if (!t.empty() && (t.front()=='"' || t.front()=='\'')) {
                    if (t.size() >= 2 && t.back()==t.front()) t = t.substr(1, t.size()-2);
                }
                return trim(t);
            }
        }
    } catch (...) {}

    return std::nullopt;
}

struct Args {
    std::optional<fs::path> input;
    std::optional<std::string> name;
    std::optional<fs::path> output;
};

std::optional<Args> parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        auto next = [&]() -> std::string_view {
            if (i + 1 >= argc) return {};
            return std::string_view{argv[++i]};
        };
        if (arg == "--input" || arg == "-i") {
            auto v = next(); if (v.empty()) return std::nullopt; a.input = fs::path{std::string(v)};
        } else if (arg == "--output" || arg == "-o") {
            auto v = next(); if (v.empty()) return std::nullopt; a.output = fs::path{std::string(v)};
        } else if (arg == "--name" || arg == "-n") {
            auto v = next(); if (v.empty()) return std::nullopt; a.name = std::string(v);
        } else if (arg == "--help" || arg == "-h") {
            return std::nullopt;
        } else {
            // Treat as input if not flagged and looks like a path
            if (!a.input && (arg.find('=') == std::string_view::npos)) {
                a.input = fs::path{std::string(arg)};
            } else {
                std::cerr << "Unknown argument: " << arg << "\n";
                return std::nullopt;
            }
        }
    }
    if (!a.output) return std::nullopt;
    return a;
}

void write_text_file(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream ofs(p, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs) {
        std::error_code ec(errno, std::generic_category());
        throw std::system_error(ec, "Failed to open file for write: " + p.string());
    }
    ofs << content;
}

std::string header_prologue() {
    return std::string("// Generated by openapi_codegen (MVP scaffold)\n") +
           "// Default JSON backend: nlohmann::json\n"
           "// This file is part of a generated SDK and may be regenerated.\n\n"
           "#pragma once\n\n";
}

std::string gen_json_backend_hpp(const std::string& apiName) {
    std::ostringstream oss;
    oss << header_prologue();
    // Doxygen file header
    oss << "/**\\file\n"
           " \\brief JSON backend adapter wrapping nlohmann::json for generated SDKs.\n"
           "\n"
           " This header is generated by openapi_codegen and provides a very small\n"
           " adapter API (struct Json) used by generated Client/Server code to\n"
           " construct and inspect JSON values without exposing the underlying\n"
           " library in public model APIs.\n"
           "*/\n\n";
    oss << "#include <string>\n#include <nlohmann/json.hpp>\n\n";
    oss << "namespace RESTCore_" << apiName << " {\n";
    oss << "/**\\brief Minimal JSON adapter facade for generated code.\n"
           "\n"
           " The public model types remain JSON-agnostic (std::string, numbers,\n"
           " bool, std::optional, STL containers). Only this adapter is used by\n"
           " the generated glue that parses/serializes HTTP bodies.\n"
           "*/\n";
    oss << "struct Json {\n";
    oss << "    using value = nlohmann::json;\n\n";
    oss << "    /// Parse a JSON string into a DOM value (throws on parse error).\n";
    oss << "    static value parse(const std::string& s) { return nlohmann::json::parse(s); }\n";
    oss << "    /// Serialize a DOM value to a minified JSON string.\n";
    oss << "    static std::string dump(const value& v)  { return v.dump(); }\n\n";
    oss << "    /// Constructors for common JSON types\n";
    oss << "    static value make_object() { return nlohmann::json::object(); }\n";
    oss << "    static value make_array()  { return nlohmann::json::array(); }\n";
    oss << "    static value make_null()   { return nullptr; }\n";
    oss << "    static value make_bool(bool b) { return value(b); }\n";
    oss << "    static value make_int(std::int64_t i) { return value(i); }\n";
    oss << "    static value make_uint(std::uint64_t u) { return value(u); }\n";
    oss << "    static value make_double(double d) { return value(d); }\n";
    oss << "    static value make_string(const std::string& s) { return value(s); }\n\n";
    oss << "    /// Type predicates\n";
    oss << "    static bool is_object(const value& v) { return v.is_object(); }\n";
    oss << "    static bool is_array(const value& v)  { return v.is_array(); }\n";
    oss << "    static bool is_null(const value& v)   { return v.is_null(); }\n";
    oss << "    static bool is_bool(const value& v)   { return v.is_boolean(); }\n";
    oss << "    static bool is_number(const value& v) { return v.is_number(); }\n";
    oss << "    static bool is_string(const value& v) { return v.is_string(); }\n\n";
    oss << "    /// Object/array helpers\n";
    oss << "    static bool has_key(const value& v, const std::string& k) { return v.contains(k); }\n";
    oss << "    static value& at(value& v, const std::string& k) { return v[k]; }\n";
    oss << "    static const value& at(const value& v, const std::string& k) { return v.at(k); }\n";
    oss << "    static void set(value& obj, const std::string& k, value val) { obj[k] = std::move(val); }\n\n";
    oss << "    static void push_back(value& arr, value val) { arr.push_back(std::move(val)); }\n";
    oss << "    static std::size_t size(const value& arr) { return arr.size(); }\n";
    oss << "    static const value& index(const value& arr, std::size_t i) { return arr.at(i); }\n\n";
    oss << "    /// Typed conversions (throw on type mismatch)\n";
    oss << "    static bool           as_bool(const value& v)   { return v.get<bool>(); }\n";
    oss << "    static std::int64_t   as_int(const value& v)    { return v.get<std::int64_t>(); }\n";
    oss << "    static std::uint64_t  as_uint(const value& v)   { return v.get<std::uint64_t>(); }\n";
    oss << "    static double         as_double(const value& v) { return v.get<double>(); }\n";
    oss << "    static std::string    as_string(const value& v) { return v.get<std::string>(); }\n";
    oss << "};\n";
    oss << "} // namespace RESTCore_" << apiName << "\n";
    return oss.str();
}

std::string gen_client_hpp(const std::string& apiName) {
    std::ostringstream oss;
    oss << header_prologue();
    // Doxygen file header
    oss << "/**\\file\n"
           " \\brief Generated REST client facade and JSON-backed message types.\n"
           "\n"
           " Contains RESTCore_" << apiName << "::Client and its nested Message, Request,\n"
           " and Response types. Request/Response wrap a JSON object with typed\n"
           " get/set helpers while keeping the public API JSON-agnostic for users.\n"
           "*/\n\n";
    oss << "#include <RESTCore/Client.hpp>\n#include <string>\n#include <cstdint>\n#include <utility>\n#include <stdexcept>\n";
    oss << "#include <RESTCore_" << apiName << "/json_backend.hpp>\n\n";
    oss << "namespace RESTCore_" << apiName << " {\n";
    oss << "/**\\brief Client facade bound to RESTCore::Client transport.*/\n";
    oss << "class Client {\n";
    oss << "    std::string base_url_;\n";
    oss << "public:\n";
    oss << "    // --- Nested JSON message types ---\n";
    oss << "    /**\\brief JSON-backed message with typed accessors.\n"
           "\n"
           "     Usage example:\n"
           "     \n"
           "     ```cpp\n"
           "     Client::Request req;\n"
           "     req.set_string(\"favourite_pet\", \"Waffles\");\n"
           "     auto name = req.get_string(\"favourite_pet\");\n"
           "     ```\n"
           "\n"
           "     The generator can also emit field-specific helpers, e.g.:\n"
           "     `const std::string& favourite_pet();` and `void set_favourite_pet(std::string);`\n"
           "    */\n";
    oss << "    class Message {\n";
    oss << "        Json::value obj_;\n";
    oss << "    public:\n";
    oss << "        Message() : obj_(Json::make_object()) {}\n";
    oss << "        explicit Message(Json::value v) : obj_(std::move(v)) {}\n";
    oss << "        /// Parse from a JSON string.\n";
    oss << "        static Message from_json_string(const std::string& s) { return Message{Json::parse(s)}; }\n";
    oss << "        /// Serialize to a compact JSON string.\n";
    oss << "        std::string to_json_string() const { return Json::dump(obj_); }\n";
    oss << "        /// Return true if key exists in the underlying object.\n";
    oss << "        bool has(const std::string& key) const { return Json::has_key(obj_, key); }\n";
    oss << "        /// Typed getters (throw std::out_of_range if key missing; type error on mismatch).\n";
    oss << "        std::string   get_string(const std::string& key) const { return Json::as_string(Json::at(obj_, key)); }\n";
    oss << "        std::int64_t  get_int64 (const std::string& key) const { return Json::as_int(Json::at(obj_, key)); }\n";
    oss << "        std::uint64_t get_uint64(const std::string& key) const { return Json::as_uint(Json::at(obj_, key)); }\n";
    oss << "        double        get_double(const std::string& key) const { return Json::as_double(Json::at(obj_, key)); }\n";
    oss << "        bool          get_bool  (const std::string& key) const { return Json::as_bool(Json::at(obj_, key)); }\n";
    oss << "        /// Typed setters\n";
    oss << "        void set_string(const std::string& key, std::string v) { Json::set(obj_, key, Json::make_string(v)); }\n";
    oss << "        void set_int64 (const std::string& key, std::int64_t v) { Json::set(obj_, key, Json::make_int(v)); }\n";
    oss << "        void set_uint64(const std::string& key, std::uint64_t v){ Json::set(obj_, key, Json::make_uint(v)); }\n";
    oss << "        void set_double(const std::string& key, double v)       { Json::set(obj_, key, Json::make_double(v)); }\n";
    oss << "        void set_bool  (const std::string& key, bool v)         { Json::set(obj_, key, Json::make_bool(v)); }\n";
    oss << "        /// Raw JSON access (advanced)\n";
    oss << "        const Json::value& raw() const noexcept { return obj_; }\n";
    oss << "        Json::value&       raw()       noexcept { return obj_; }\n";
    oss << "        // Helper macro for field-specific accessors the generator can emit later:\n";
    oss << "        //   #define RESTCORE_JSON_STRING_FIELD(Func, Key) \\\n            const std::string Func() const { return this->get_string(Key); } \\\n            void set_##Func(std::string v) { this->set_string(Key, std::move(v)); }\n";
    oss << "    };\n";
    oss << "    /// Alias for request message payloads (identical to Message).\n";
    oss << "    class Request : public Message { using Message::Message; };\n";
    oss << "    /// Alias for response message payloads (identical to Message).\n";
    oss << "    class Response: public Message { using Message::Message; };\n\n";
    oss << "    /// Construct a client bound to a base URL (e.g., https://host:port).\n";
    oss << "    explicit Client(std::string base_url) : base_url_(std::move(base_url)) {}\n";
    oss << "    /// Return the base URL configured for this client.\n";
    oss << "    const std::string& base_url() const noexcept { return base_url_; }\n";
    oss << "    // TODO: Generated methods per operationId will be added here.\n";
    oss << "};\n";
    oss << "} // namespace RESTCore_" << apiName << "\n";
    return oss.str();
}

std::string gen_server_hpp(const std::string& apiName) {
    std::ostringstream oss;
    oss << header_prologue();
    // Doxygen file header
    oss << "/**\\file\n"
           " \\brief Generated server routing facade and JSON-backed message types.\n"
           "\n"
           " Contains RESTCore_" << apiName << "::Server with nested Message, Request,\n"
           " and Response types. Handlers will be declared per operationId.\n"
           "*/\n\n";
    oss << "#include <RESTCore/Server.hpp>\n#include <memory>\n#include <string>\n#include <cstdint>\n#include <utility>\n";
    oss << "#include <RESTCore_" << apiName << "/json_backend.hpp>\n\n";
    oss << "namespace RESTCore_" << apiName << " {\n";
    oss << "// The generated server will parse HTTP bodies into these JSON-backed message types\n";
    oss << "struct Server {\n";
    oss << "    /**\\brief JSON-backed message with typed accessors (server side).*/\n";
    oss << "    class Message {\n";
    oss << "        Json::value obj_;\n";
    oss << "    public:\n";
    oss << "        Message() : obj_(Json::make_object()) {}\n";
    oss << "        explicit Message(Json::value v) : obj_(std::move(v)) {}\n";
    oss << "        /// Parse from a JSON string.\n";
    oss << "        static Message from_json_string(const std::string& s) { return Message{Json::parse(s)}; }\n";
    oss << "        /// Serialize to a compact JSON string.\n";
    oss << "        std::string to_json_string() const { return Json::dump(obj_); }\n";
    oss << "        /// Return true if key exists in the underlying object.\n";
    oss << "        bool has(const std::string& key) const { return Json::has_key(obj_, key); }\n";
    oss << "        /// Typed getters\n";
    oss << "        std::string   get_string(const std::string& key) const { return Json::as_string(Json::at(obj_, key)); }\n";
    oss << "        std::int64_t  get_int64 (const std::string& key) const { return Json::as_int(Json::at(obj_, key)); }\n";
    oss << "        std::uint64_t get_uint64(const std::string& key) const { return Json::as_uint(Json::at(obj_, key)); }\n";
    oss << "        double        get_double(const std::string& key) const { return Json::as_double(Json::at(obj_, key)); }\n";
    oss << "        bool          get_bool  (const std::string& key) const { return Json::as_bool(Json::at(obj_, key)); }\n";
    oss << "        /// Typed setters\n";
    oss << "        void set_string(const std::string& key, std::string v) { Json::set(obj_, key, Json::make_string(v)); }\n";
    oss << "        void set_int64 (const std::string& key, std::int64_t v) { Json::set(obj_, key, Json::make_int(v)); }\n";
    oss << "        void set_uint64(const std::string& key, std::uint64_t v){ Json::set(obj_, key, Json::make_uint(v)); }\n";
    oss << "        void set_double(const std::string& key, double v)       { Json::set(obj_, key, Json::make_double(v)); }\n";
    oss << "        void set_bool  (const std::string& key, bool v)         { Json::set(obj_, key, Json::make_bool(v)); }\n";
    oss << "        /// Raw JSON access (advanced)\n";
    oss << "        const Json::value& raw() const noexcept { return obj_; }\n";
    oss << "        Json::value&       raw()       noexcept { return obj_; }\n";
    oss << "        // Macro for field-specific accessors (to be emitted by the generator per endpoint schema)\n";
    oss << "        //   #define RESTCORE_JSON_STRING_FIELD(Func, Key) \\\n            const std::string Func() const { return this->get_string(Key); } \\\n            void set_##Func(std::string v) { this->set_string(Key, std::move(v)); }\n";
    oss << "    };\n";
    oss << "    /// Alias for request message payloads (identical to Message).\n";
    oss << "    class Request : public Message { using Message::Message; };\n";
    oss << "    /// Alias for response message payloads (identical to Message).\n";
    oss << "    class Response: public Message { using Message::Message; };\n\n";
    oss << "    /**\\brief User-implemented handlers. One virtual per operationId will be generated.*/\n";
    oss << "    struct Handlers {\n";
    oss << "        virtual ~Handlers() = default;\n";
    oss << "        // The generator will declare one method per operationId, e.g.:\n";
    oss << "        //   virtual Response MessageOne(const Request& req) = 0;\n";
    oss << "    };\n\n";
    oss << "    // Router glue will be generated to parse HTTP requests into Request and serialize Response back.\n";
    oss << "    static void bind(RESTCore::Server& srv, std::shared_ptr<Handlers> impl) { (void)srv; (void)impl; /* TODO */ }\n";
    oss << "};\n";
    oss << "} // namespace RESTCore_" << apiName << "\n";
    return oss.str();
}

// ---- Canonical model skeleton + loader (PR1) ----------------------------------

struct OAInfo { std::string title; std::string version; };

struct OADocument {
    std::string openapi_version;
    OAInfo info;
    nlohmann::json raw;       // Original parsed doc
    nlohmann::json resolved;  // Copy with in-doc $ref resolved (subset)
};

static bool is_yaml_path(const fs::path& p) {
    auto ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
    return (ext == ".yaml" || ext == ".yml");
}

// Extremely small YAML parser for a tiny subset: mappings and sequences with scalar values.
// Intended only to support simple fixtures. Complex YAML (anchors, inline maps {a:b}, multi-line blocks)
// is not supported and will throw.
static nlohmann::json parse_yaml_minimal(const std::string& text) {
    struct Node { enum Kind{Obj, Arr, Str} kind; nlohmann::json j; int indent; };
    std::vector<Node> stack;
    auto push_obj = [&](int ind){ stack.push_back({Node::Obj, nlohmann::json::object(), ind}); };
    auto push_arr = [&](int ind){ stack.push_back({Node::Arr, nlohmann::json::array(),  ind}); };

    auto to_scalar = [](std::string v)->nlohmann::json{
        v = trim(v);
        if (v.empty()) return nlohmann::json("");
        if ((v.front()=='"' && v.back()=='"') || (v.front()=='\'' && v.back()=='\'')) {
            if (v.size()>=2) return v.substr(1, v.size()-2);
        }
        // bools
        if (v == "true") return true; if (v=="false") return false;
        // null
        if (v == "null" || v == "~") return nullptr;
        // fallback string (strip trailing # comments)
        auto hash = v.find(" #"); if (hash != std::string::npos) v = v.substr(0, hash);
        return v;
    };

    auto count_indent = [](const std::string& s){ int n=0; for(char c: s){ if(c==' ') ++n; else break; } return n; };

    std::istringstream iss(text);
    std::string line;
    int line_no = 0;
    while (std::getline(iss, line)) {
        ++line_no;
        // strip CR
        if (!line.empty() && line.back()=='\r') line.pop_back();
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;
        if (trimmed.size()>=1 && trimmed[0]=='#') continue;
        int ind = count_indent(line);

        // collapse stack to current indentation
        while (!stack.empty() && ind < stack.back().indent) {
            auto node = stack.back(); stack.pop_back();
            if (stack.empty()) { stack.push_back(node); break; }
            auto& parent = stack.back();
            if (parent.kind == Node::Arr) {
                parent.j.push_back(node.j);
            } else if (parent.kind == Node::Obj) {
                // previous key was added as placeholder; nothing to do here
            }
        }

        // sequence item?
        if (trimmed.rfind("- ", 0) == 0) {
            std::string rest = trimmed.substr(2);
            if (stack.empty() || stack.back().kind != Node::Arr || stack.back().indent != ind) {
                // start a new array at this indent
                push_arr(ind);
            }
            if (rest.find(':') != std::string::npos) {
                // object item in array
                push_obj(ind + 2);
                // fall through to mapping parse
                trimmed = rest;
                ind += 2;
            } else {
                // scalar item
                stack.back().j.push_back(to_scalar(rest));
                continue;
            }
        }

        // mapping line key: value
        auto pos = trimmed.find(':');
        if (pos == std::string::npos) {
            throw std::runtime_error("Unsupported YAML syntax at line " + std::to_string(line_no) + ": " + trimmed);
        }
        std::string key = trim(trimmed.substr(0, pos));
        std::string val = trim(trimmed.substr(pos+1));
        if (val == "|") {
            throw std::runtime_error("YAML block scalars not supported in minimal parser");
        }
        // ensure object at this indent
        if (stack.empty() || stack.back().kind != Node::Obj || stack.back().indent != ind) {
            push_obj(ind);
        }
        auto& obj = stack.back().j;
        if (val.empty()) {
            // nested block follows
            obj[key] = nlohmann::json::object();
            // descend
            push_obj(ind + 2);
        } else {
            // simple scalar value
            // Inline objects/arrays like { a: b } or [1,2] not supported in minimal mode
            if (!val.empty() && (val.front()=='{' || val.front()=='[')) {
                throw std::runtime_error("Inline YAML collections are not supported in minimal parser");
            }
            obj[key] = to_scalar(val);
        }
    }

    // collapse remaining stack
    while (stack.size() > 1) {
        auto node = stack.back(); stack.pop_back();
        auto& parent = stack.back();
        if (parent.kind == Node::Arr) parent.j.push_back(node.j);
        // if parent is object, nodes were already attached by key assignment
    }
    if (stack.empty()) return nlohmann::json::object();
    return stack.front().j;
}

static nlohmann::json parse_spec_file(const fs::path& p) {
    auto data_opt = read_file(p);
    if (!data_opt) throw std::runtime_error("Failed to read input: " + p.string());
    const std::string& data = *data_opt;
    // Heuristic by extension, but allow braces to force JSON
    bool as_yaml = is_yaml_path(p);
    if (!as_yaml) {
        // also detect leading '{'
        std::string s = trim(std::string(data));
        if (!s.empty() && s.front()!='{') {
            // still JSON possibly without leading brace? assume JSON when extension is .json
        }
    }
    if (as_yaml) {
        return parse_yaml_minimal(data);
    }
    // JSON
    return nlohmann::json::parse(data);
}

static void resolve_in_doc_refs(nlohmann::json& doc) {
    // Textually inline {"$ref":"#/components/schemas/<Name>"} occurrences with the
    // corresponding schema object from components.schemas. This covers the common
    // case where a $ref object has no siblings.
    if (!(doc.contains("components") && doc["components"].contains("schemas"))) return;
    auto& schemas = doc["components"]["schemas"];
    std::string json_text = doc.dump();

    // Collect referenced schema names
    std::set<std::string> names;
    std::regex re(R"REGEX(#/components/schemas/([A-Za-z0-9_\-\.]+))REGEX");
    for (std::sregex_iterator it(json_text.begin(), json_text.end(), re), end; it != end; ++it) {
        names.insert((*it)[1].str());
    }

    auto replace_all = [](std::string& s, const std::string& from, const std::string& to){
        if (from.empty()) return;
        std::size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
    };

    for (const auto& name : names) {
        if (!schemas.contains(name)) continue;
        std::string needle = std::string("{\"$ref\":\"#/components/schemas/") + name + "\"}";
        std::string repl   = schemas[name].dump();
        replace_all(json_text, needle, repl);
    }

    // Re-parse back to JSON
    doc = nlohmann::json::parse(json_text);
}

static OADocument load_openapi_document(const fs::path& p) {
    nlohmann::json doc = parse_spec_file(p);
    OADocument out;
    out.raw = doc;
    if (doc.contains("openapi")) out.openapi_version = doc["openapi"].get<std::string>();
    if (doc.contains("info")) {
        auto& info = doc["info"];
        if (info.contains("title")) out.info.title = info["title"].get<std::string>();
        if (info.contains("version")) out.info.version = info["version"].get<std::string>();
    }
    out.resolved = doc; // deep copy
    resolve_in_doc_refs(out.resolved);
    return out;
}

void print_usage(const char* argv0) {
    std::cerr << "Usage: " << argv0 << " --output <dir> [--input <openapi.(json|yaml)>] [--name <ApiName>]\n";
    std::cerr << "\nNotes:\n";
    std::cerr << "- If --name is omitted, the tool attempts to derive it from the OpenAPI info.title;\n";
    std::cerr << "  if that fails, it uses the input filename stem; if no input, defaults to 'Api'.\n";
    std::cerr << "- Generated scope: RESTCore_<ApiName>::Client and RESTCore_<ApiName>::Server\n";
    std::cerr << "- Default JSON backend in generated code: nlohmann::json (adapter provided).\n";
}

} // namespace

int main(int argc, char** argv) {
    auto parsed = parse_args(argc, argv);
    if (!parsed) {
        print_usage(argv[0]);
        return 1;
    }

    Args args = *parsed;

    std::string apiName;
    if (args.name) {
        apiName = sanitize_api_name(*args.name);
    } else if (args.input) {
        auto derived = derive_name_from_spec(*args.input);
        if (!derived) {
            apiName = sanitize_api_name(args.input->stem().string());
        } else {
            apiName = sanitize_api_name(*derived);
        }
    } else {
        apiName = sanitize_api_name("Api");
    }

    fs::path out = *args.output;
    fs::path baseInclude = out / "include" / (std::string{"RESTCore_"} + apiName);

    // PR1: If an input spec is provided, load and resolve it now (no codegen usage yet).
    if (args.input) {
        try {
            OADocument doc = load_openapi_document(*args.input);
            std::size_t path_count = 0;
            if (doc.raw.contains("paths") && doc.raw["paths"].is_object()) path_count = doc.raw["paths"].size();
            std::size_t schema_count = 0;
            if (doc.raw.contains("components") && doc.raw["components"].contains("schemas") && doc.raw["components"]["schemas"].is_object()) {
                schema_count = doc.raw["components"]["schemas"].size();
            }
            std::cout << "Loaded OpenAPI " << doc.openapi_version << ": '" << doc.info.title << "' v" << doc.info.version
                      << " (paths=" << path_count << ", schemas=" << schema_count << ")\n";
        } catch (const std::exception& ex) {
            std::cerr << "Warning: failed to load/parse spec '" << args.input->string() << "': " << ex.what() << "\n";
        }
    }

    try {
        write_text_file(baseInclude / "json_backend.hpp", gen_json_backend_hpp(apiName));
        write_text_file(baseInclude / "Client.hpp",        gen_client_hpp(apiName));
        write_text_file(baseInclude / "Server.hpp",        gen_server_hpp(apiName));
    } catch (const std::exception& ex) {
        std::cerr << "Generation failed: " << ex.what() << "\n";
        return 2;
    }

    std::cout << "Generated SDK scaffold for API '" << apiName << "' under: " << (out) << "\n";
    std::cout << "  - " << (baseInclude / "json_backend.hpp") << "\n";
    std::cout << "  - " << (baseInclude / "Client.hpp") << "\n";
    std::cout << "  - " << (baseInclude / "Server.hpp") << "\n";

    return 0;
}
