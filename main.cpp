#include <iostream>
#include <thread>
#include <chrono>
#include "HTTPClient.hpp"
#include "HTTPServerHost.hpp"

int main() {
    try {
        // 1) Start a local HTTP server with a stub callback
        HTTPServerHost server;
        server.set_callback([](const HTTPServerHost::Request& req,
                               HTTPServerHost::Response& res,
                               const std::string& client){
            // Apply the same response regardless of request
            (void)client; // unused in this stub
            res.result(boost::beast::http::status::ok);
            res.set(boost::beast::http::field::content_type, "text/plain; charset=utf-8");
            res.body() = std::string("Hello from HTTPServerHost! You requested: ") + std::string(req.target());
            res.prepare_payload();
        });
        server.listen_http("127.0.0.1", 8080);
        server.start();

        // Small delay to ensure the server is listening
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        // 2) Use the HTTPClient to GET from our local server
        auto [status, res] = HTTPClient::Get(false, "127.0.0.1", "8080", "/test");
        std::cout << "Client GET http://127.0.0.1:8080/test -> status " << status << "\n";
        std::cout << "Response body:\n" << res.body() << std::endl;

        // 3) Stop the server
        server.stop();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
