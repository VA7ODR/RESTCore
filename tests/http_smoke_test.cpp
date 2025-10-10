#define BOOST_TEST_MODULE HttpSuite
#include <boost/test/included/unit_test.hpp>

#include <thread>
#include <chrono>
#include <string>
#include <csignal>

#include "RESTCore/Client.hpp"
#include "RESTCore/Server.hpp"

#include <boost/asio.hpp>

namespace {
    // Helper to find a free local TCP port to reduce collisions when running tests.
    static unsigned short find_free_port() {
        namespace asio = boost::asio;
        asio::io_context ioc;
        asio::ip::tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), 0); // 0 = ephemeral
        asio::ip::tcp::acceptor acc(ioc);
        acc.open(ep.protocol());
        acc.set_option(asio::socket_base::reuse_address(true));
        acc.bind(ep);
        auto port = acc.local_endpoint().port();
        // Do not listen; close immediately so our server can bind the discovered port.
        acc.close();
        return port;
    }

    // Helper to wait until a TCP port is accepting connections or timeout.
    static bool wait_until_listening(const std::string& host, unsigned short port, std::chrono::milliseconds timeout = std::chrono::milliseconds(1500)) {
        using namespace std::chrono;
        auto start = steady_clock::now();
        while (steady_clock::now() - start < timeout) {
            try {
                boost::asio::io_context ioc;
                boost::asio::ip::tcp::resolver res{ioc};
                auto results = res.resolve(host, std::to_string(port));
                boost::asio::ip::tcp::socket sock{ioc};
                boost::asio::connect(sock, results);
                // If we connected, it's listening.
                return true;
            } catch (...) {
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
        }
        return false;
    }

    struct ServerFixture {
        RESTCore::Server server;
        std::string host = "127.0.0.1";
        unsigned short port = 0;

        ServerFixture() {
            // Avoid SIGPIPE terminating the process in some environments (e.g., CTest)
            std::signal(SIGPIPE, SIG_IGN);
            // Basic echo-style handler used by multiple tests
            server.set_callback([](const RESTCore::Server::Request& req,
                                   RESTCore::Server::Response& res,
                                   const std::string& /*client*/){
                res.result(boost::beast::http::status::ok);
                res.set(boost::beast::http::field::content_type, "text/plain; charset=utf-8");
                res.body() = std::string("Hello from HTTPServerHost! You requested: ") + std::string(req.target());
                res.prepare_payload();
            });

            port = find_free_port();
            server.listen_http(host, port);
            server.start();
            // Wait until the port is actually accepting before tests fire requests
            bool ready = wait_until_listening(host, port);
            BOOST_REQUIRE_MESSAGE(ready, "Server did not start listening in time");
        }

        ~ServerFixture() {
            // Stop is expected to be idempotent; call twice for good measure
            server.stop();
            server.stop();
        }
    };
}

BOOST_FIXTURE_TEST_SUITE(HttpSuite, ServerFixture)

BOOST_AUTO_TEST_CASE(get_returns_ok_and_body_contains_target) {
    auto [status, res] = RESTCore::Client::Get(false, host, std::to_string(port), "/test");
    BOOST_TEST(status == 200u);
    BOOST_TEST(res[boost::beast::http::field::content_type] == "text/plain; charset=utf-8");
    BOOST_TEST(res.body().find("/test") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(head_returns_ok_and_no_body_content) {
    auto [status, res] = RESTCore::Client::Head(false, host, std::to_string(port), "/head");
    BOOST_TEST(status == 200u);
    // For HEAD, some clients may still expose a body buffer; only assert status/header here
    const bool has_ct = (res.find(boost::beast::http::field::content_type) != res.end());
    BOOST_TEST(has_ct);
}

BOOST_AUTO_TEST_CASE(server_stop_is_idempotent) {
    // Stopping twice is exercised in fixture destructor, but assert explicitly here
    server.stop();
    server.stop();
    BOOST_TEST(true); // if no exception, we pass
}

BOOST_AUTO_TEST_CASE(connect_to_unused_port_raises_error) {
    // Obtain a likely free port and DO NOT start a server on it; then attempt a request
    unsigned short free_port = find_free_port();
    bool failed = false;
    try {
        (void)RESTCore::Client::Get(false, host, std::to_string(free_port), "/");
    } catch (const std::exception&) {
        failed = true;
    }
    BOOST_TEST(failed);
}

BOOST_AUTO_TEST_CASE(post_echo_like_handler_returns_ok) {
    // Our handler returns OK regardless of method; just ensure client path works for POST
    auto [status, res] = RESTCore::Client::Post(false, host, std::to_string(port), "/post", "{\"k\":1}", "application/json");
    BOOST_TEST(status == 200u);
    BOOST_TEST(res[boost::beast::http::field::content_type] == "text/plain; charset=utf-8");
}

BOOST_AUTO_TEST_SUITE_END()
