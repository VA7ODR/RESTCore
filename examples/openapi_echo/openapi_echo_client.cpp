#include <RESTCore/Client.hpp>
#include <iostream>
#include <string>
#include <vector>

using namespace RESTCore;

static void print_usage(const char* argv0) {
    std::cout << "Usage:\n"
              << "  " << argv0 << " [--host HOST] [--port PORT] --motd\n"
              << "  " << argv0 << " [--host HOST] [--port PORT] --echo TEXT\n\n"
              << "Defaults: HOST=127.0.0.1 PORT=9090\n"
              << "Examples:\n"
              << "  " << argv0 << " --motd\n"
              << "  " << argv0 << " --echo \"hello world\"\n";
}

int main(int argc, char** argv) {
    std::string host = "127.0.0.1";
    std::string port = "9090";
    bool do_motd = false;
    std::string echo_text;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = argv[++i];
        } else if (arg == "--motd") {
            do_motd = true;
        } else if (arg == "--echo" && i + 1 < argc) {
            echo_text = argv[++i];
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!do_motd && echo_text.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    try {
        if (do_motd) {
            auto [code, res] = Client::Get(false, host, port, "/motd");
            std::cout << "HTTP " << code << "\n" << res.body() << "\n";
        }
        if (!echo_text.empty()) {
            auto [code, res] = Client::Post(false, host, port, "/echo", echo_text, "text/plain");
            std::cout << "HTTP " << code << "\n" << res.body() << "\n";
        }
    } catch (const std::exception& ex) {
        std::cerr << "Request failed: " << ex.what() << "\n";
        return 2;
    }

    return 0;
}
