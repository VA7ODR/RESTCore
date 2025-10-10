#include <RESTCore/Server.hpp>
#include <boost/beast/http.hpp>
#include <csignal>
#include <iostream>
#include <string>
#include <algorithm>
#include <chrono>
#include <thread>

using namespace RESTCore;
namespace http = boost::beast::http;

static std::string trim(const std::string& s) {
    auto b = s.begin();
    while (b != s.end() && std::isspace(static_cast<unsigned char>(*b))) ++b;
    auto e = s.end();
    do { if (e == s.begin()) break; --e; } while (std::isspace(static_cast<unsigned char>(*e)));
    if (b == s.end()) return {};
    return std::string(b, e + 1);
}

int main(int argc, char** argv) {
    // Ignore SIGPIPE on Unix-like systems to avoid abrupt termination on client disconnects
    std::signal(SIGPIPE, SIG_IGN);

    // Defaults
    std::string address = "127.0.0.1";
    unsigned short port = 9090;
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
        (void)client; // unused

        // Default headers
        res.set(http::field::server, "RESTCore OpenAPI Echo Server");

        // Route: GET /motd => returns a simple message of the day
        if (req.method() == http::verb::get && req.target() == "/motd") {
            res.result(http::status::ok);
            res.set(http::field::content_type, "text/plain; charset=utf-8");
            res.body() = "MOTD: Welcome to RESTCore!";
            res.prepare_payload();
            return;
        }

        // Route: POST /echo with text/plain => returns body uppercased and prefixed
        if (req.method() == http::verb::post && req.target() == "/echo") {
            // Validate content type (best-effort)
            auto ct = req[http::field::content_type];
            if (!ct.empty() && std::string(ct).find("text/plain") == std::string::npos) {
                res.result(http::status::unsupported_media_type);
                res.set(http::field::content_type, "text/plain; charset=utf-8");
                res.body() = "Expected Content-Type: text/plain";
                res.prepare_payload();
                return;
            }
            std::string body = trim(req.body());
            std::string up = body;
            std::transform(up.begin(), up.end(), up.begin(), [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
            res.result(http::status::ok);
            res.set(http::field::content_type, "text/plain; charset=utf-8");
            res.body() = std::string("Echo: ") + up;
            res.prepare_payload();
            return;
        }

        // Not found
        res.result(http::status::not_found);
        res.set(http::field::content_type, "text/plain; charset=utf-8");
        res.body() = "Not found";
        res.prepare_payload();
    });

    server.listen_http(address, port);
    server.start();

    std::cout << "openapi_echo_server listening on http://" << address << ":" << port
              << "\nEndpoints:\n  GET  /motd\n  POST /echo (text/plain)\n";

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
