#include <RESTCore/Client.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include <RESTCore_GeneratedExample/Client.hpp>
#include <RESTCore_GeneratedExample/json_backend.hpp>

using namespace RESTCore;
namespace Gen = RESTCore_GeneratedExample;

static void print_usage(const char* argv0) {
    std::cout << "Usage:\n"
              << "  " << argv0 << " [--host HOST] [--port PORT] --info\n"
              << "  " << argv0 << " [--host HOST] [--port PORT] --shout TEXT [--upper {true|false|1|0}]\n\n"
              << "Defaults: HOST=127.0.0.1 PORT=9094\n"
              << "Examples:\n"
              << "  " << argv0 << " --info\n"
              << "  " << argv0 << " --shout hello --upper true\n";
}

static std::string url_encode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c=='-'||c=='_'||c=='.'||c=='~') out.push_back(static_cast<char>(c));
        else if (c == ' ') out.push_back('+');
        else { out.push_back('%'); out.push_back(hex[c>>4]); out.push_back(hex[c&15]); }
    }
    return out;
}

int main(int argc, char** argv) {
    std::string host = "127.0.0.1";
    std::string port = "9094";
    bool do_info = false;
    bool do_shout = false;
    std::string text;
    std::string upper = "true";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = argv[++i];
        else if (arg == "--info") do_info = true;
        else if (arg == "--shout" && i + 1 < argc) { do_shout = true; text = argv[++i]; }
        else if (arg == "--upper" && i + 1 < argc) { upper = argv[++i]; }
        else { print_usage(argv[0]); return 1; }
    }

    if (!do_info && !do_shout) { print_usage(argv[0]); return 1; }

    try {
        if (do_info) {
            auto [code, res] = Client::Get(false, host, port, "/info");
            std::cout << "HTTP " << code << "\n";
            Gen::Client::Message m = Gen::Client::Message::from_json_string(res.body());
            std::string service = m.get_string("service");
            std::string version = m.get_string("version");
            std::cout << "service: " << service << "\nversion: " << version << "\nendpoints:";
            // dump endpoints array if present
            if (m.has("endpoints")) {
                const auto& raw = m.raw();
                const auto& arr = Gen::Json::at(raw, "endpoints");
                std::cout << "\n";
                for (std::size_t i = 0; i < Gen::Json::size(arr); ++i) {
                    std::cout << "  - " << Gen::Json::as_string(Gen::Json::index(arr, i)) << "\n";
                }
            } else {
                std::cout << " (none)\n";
            }
        }
        if (do_shout) {
            std::string target = std::string("/shout?text=") + url_encode(text) + "&upper=" + url_encode(upper);
            auto [code, res] = Client::Get(false, host, port, target);
            std::cout << "HTTP " << code << "\n";
            Gen::Client::Message m = Gen::Client::Message::from_json_string(res.body());
            std::cout << m.get_string("result") << "\n";
        }
    } catch (const std::exception& ex) {
        std::cerr << "Request failed: " << ex.what() << "\n";
        return 2;
    }

    return 0;
}
