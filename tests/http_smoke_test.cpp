#define BOOST_TEST_MODULE HttpSuite
#include <boost/test/included/unit_test.hpp>

#include <thread>
#include <chrono>
#include <string>
#include <csignal>
#include <future>
#include <memory>
#include <mutex>

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
            // Stop server; additional stop() calls are safe (idempotent)
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

BOOST_AUTO_TEST_CASE(client_get_stream_invokes_chunk_callback) {
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    const unsigned short stream_port = find_free_port();

    auto ready = std::make_shared<std::promise<void>>();
    auto ready_flag = std::make_shared<std::once_flag>();
    auto signal_ready = [ready, ready_flag]() {
        std::call_once(*ready_flag, [&]() { ready->set_value(); });
    };

    std::thread stream_server([stream_port, signal_ready]() mutable {
        try {
            asio::io_context ioc;
            tcp::acceptor acceptor(ioc, {tcp::v4(), stream_port});
            signal_ready();

            tcp::socket socket(ioc);
            acceptor.accept(socket);

            asio::streambuf request;
            boost::system::error_code ec;
            asio::read_until(socket, request, "\r\n\r\n", ec);

            auto send = [&](std::string_view chunk) {
                boost::system::error_code write_ec;
                asio::write(socket, asio::buffer(chunk.data(), chunk.size()), write_ec);
            };

            send("HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/plain\r\n"
                 "Transfer-Encoding: chunked\r\n"
                 "\r\n");

            send("6\r\nHello \r\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            send("5\r\nWorld\r\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            send("1\r\n!\r\n");
            send("0\r\n\r\n");

            socket.shutdown(tcp::socket::shutdown_both, ec);
        } catch (...) {
            signal_ready();
        }
    });

    ready->get_future().wait();

    std::string collected;
    bool done_called = false;

    try {
        const unsigned status = RESTCore::Client::GetStream(
            false,
            "127.0.0.1",
            std::to_string(stream_port),
            "/stream",
            {},
            [&](std::string_view chunk, bool done) {
                collected.append(chunk.data(), chunk.size());
                if (done) {
                    done_called = true;
                }
            });

        BOOST_TEST(status == 200u);
    } catch (...) {
        stream_server.join();
        throw;
    }

    BOOST_TEST(done_called);
    BOOST_TEST(collected == "Hello World!");

    stream_server.join();
}

BOOST_AUTO_TEST_CASE(client_connection_get_stream_invokes_chunk_callback) {
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    const unsigned short stream_port = find_free_port();

    auto ready = std::make_shared<std::promise<void>>();
    auto ready_flag = std::make_shared<std::once_flag>();
    auto signal_ready = [ready, ready_flag]() {
        std::call_once(*ready_flag, [&]() { ready->set_value(); });
    };

    std::thread stream_server([stream_port, signal_ready]() mutable {
        try {
            asio::io_context ioc;
            tcp::acceptor acceptor(ioc, {tcp::v4(), stream_port});
            signal_ready();

            tcp::socket socket(ioc);
            acceptor.accept(socket);

            asio::streambuf request;
            boost::system::error_code ec;
            asio::read_until(socket, request, "\r\n\r\n", ec);

            auto send = [&](std::string_view chunk) {
                boost::system::error_code write_ec;
                asio::write(socket, asio::buffer(chunk.data(), chunk.size()), write_ec);
            };

            send("HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/plain\r\n"
                 "Transfer-Encoding: chunked\r\n"
                 "\r\n");

            send("6\r\nHello \r\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            send("5\r\nWorld\r\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            send("1\r\n!\r\n");
            send("0\r\n\r\n");

            socket.shutdown(tcp::socket::shutdown_both, ec);
        } catch (...) {
            signal_ready();
        }
    });

    ready->get_future().wait();

    RESTCore::Client::Connection connection(false, "127.0.0.1", std::to_string(stream_port));
    std::string collected;
    bool done_called = false;

    const unsigned status = RESTCore::Client::GetStream(
        connection,
        "/stream",
        {},
        [&](std::string_view chunk, bool done) {
            collected.append(chunk.data(), chunk.size());
            if (done) {
                done_called = true;
            }
        });

    BOOST_TEST(status == 200u);
    BOOST_TEST(done_called);
    BOOST_TEST(collected == "Hello World!");
    BOOST_TEST(!connection.is_open());

    stream_server.join();
}

BOOST_AUTO_TEST_SUITE_END()
