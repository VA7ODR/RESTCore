#include <RESTCore/Server.hpp>
#include <boost/beast/http.hpp>
#include <csignal>
#include <iostream>
#include <string>
#include <algorithm>
#include <chrono>
#include <thread>

#include <RESTCore_GeneratedExample/Server.hpp>
#include <RESTCore_GeneratedExample/json_backend.hpp>

using namespace RESTCore;
namespace http = boost::beast::http;
namespace Gen = RESTCore_GeneratedExample;

static std::string url_decode(const std::string& s) {
    std::string out; out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '+') out.push_back(' ');
        else if (c == '%' && i + 2 < s.size()) {
            auto hex = s.substr(i+1, 2);
            char ch = static_cast<char>(std::strtoul(hex.c_str(), nullptr, 16));
            out.push_back(ch);
            i += 2;
        } else out.push_back(c);
    }
    return out;
}

static std::string get_query_value(const std::string& target, const std::string& key) {
    auto pos = target.find('?');
    if (pos == std::string::npos) return {};
    std::string q = target.substr(pos + 1);
    size_t start = 0;
    while (start < q.size()) {
        size_t amp = q.find('&', start);
        if (amp == std::string::npos) amp = q.size();
        std::string_view pair(q.data() + start, amp - start);
        size_t eq = pair.find('=');
        std::string k = std::string(pair.substr(0, eq == std::string_view::npos ? pair.size() : eq));
        std::string v = eq == std::string_view::npos ? std::string() : std::string(pair.substr(eq + 1));
        if (k == key) return url_decode(v);
        start = amp + 1;
    }
    return {};
}

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);

    std::string address = "127.0.0.1";
    unsigned short port = 9094;
    int duration_seconds = 0; // 0 => wait for Enter

    if (argc >= 2) address = argv[1];
    if (argc >= 3) port = static_cast<unsigned short>(std::stoi(argv[2]));
    if (argc >= 4) {
        std::string flag = argv[3];
        if (flag == "--duration" && argc >= 5) {
            duration_seconds = std::stoi(argv[4]);
        }
    }

    Server server;

    server.set_callback([](const Server::Request& req, Server::Response& res, const std::string& client){
        (void)client;
        res.set(http::field::server, "RESTCore GeneratedExample Server");

        // Route: GET /info (application/json)
        if (req.method() == http::verb::get && req.target().starts_with("/info")) {
            Gen::Server::Message m;
            // Build JSON: { service: "generated-example", version: "1.0", endpoints: ["/info", "/shout"] }
            auto& obj = m.raw();
            Gen::Json::set(obj, "service", Gen::Json::make_string("generated-example"));
            Gen::Json::set(obj, "version", Gen::Json::make_string("1.0"));
            auto arr = Gen::Json::make_array();
            Gen::Json::push_back(arr, Gen::Json::make_string("/info"));
            Gen::Json::push_back(arr, Gen::Json::make_string("/shout"));
            Gen::Json::set(obj, "endpoints", std::move(arr));

            res.result(http::status::ok);
            res.set(http::field::content_type, "application/json; charset=utf-8");
            res.body() = m.to_json_string();
            res.prepare_payload();
            return;
        }

        // Route: GET /shout?text=...&upper=true|false (application/json)
        if (req.method() == http::verb::get && std::string(req.target()).rfind("/shout", 0) == 0) {
            std::string target = std::string(req.target());
            std::string text = get_query_value(target, "text");
            std::string upper = get_query_value(target, "upper");
            bool do_upper = true;
            if (!upper.empty()) {
                std::string u = upper; std::transform(u.begin(), u.end(), u.begin(), ::tolower);
                do_upper = (u == "1" || u == "true" || u == "yes");
            }
            std::string result = text;
            if (do_upper) std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c){ return static_cast<char>(std::toupper(c)); });

            Gen::Server::Message m;
            Gen::Json::set(m.raw(), "result", Gen::Json::make_string(result));

            res.result(http::status::ok);
            res.set(http::field::content_type, "application/json; charset=utf-8");
            res.body() = m.to_json_string();
            res.prepare_payload();
            return;
        }

        // Not found
        res.result(http::status::not_found);
        res.set(http::field::content_type, "application/json; charset=utf-8");
        Gen::Server::Message m;
        Gen::Json::set(m.raw(), "error", Gen::Json::make_string("Not found"));
        res.body() = m.to_json_string();
        res.prepare_payload();
    });

    server.listen_http(address, port);
    server.start();

    std::cout << "openapi_generated_server listening on http://" << address << ":" << port
              << "\nEndpoints:\n  GET  /info (application/json)\n  GET  /shout?text=...&upper=true|false (application/json)\n";

    if (duration_seconds > 0) {
        std::cout << "Running for " << duration_seconds << " seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
    } else {
        std::cout << "Press Enter to stop...\n";
        std::cin.get();
    }

    server.stop();
    return 0;
}
