#pragma once

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace ssl   = net::ssl;

// A minimal multi-connection HTTP(S) host that accepts incoming connections
// and invokes a user callback for each request, providing references to the
// Beast HTTP request and response, and the client endpoint address. The
// callback returns void; the server then replies with the response object
// that the callback modified.
//
// Notes:
// - Simple, synchronous per-connection handling on individual threads.
// - Each listener (HTTP/HTTPS) runs an accept loop on its own thread.
// - One request per connection (no keep-alive handling).
// - HTTPS requires certificate and private key files.
class HTTPServerHost {
public:
    using Request  = http::request<http::string_body>;
    using Response = http::response<http::string_body>;

    // Callback signature: the server provides:
    // - const Request& req
    // - Response& res (modify to set status/body/headers)
    // - const std::string& client_address (ip[:port] string)
    using Callback = std::function<void(const Request&, Response&, const std::string&)>;

    HTTPServerHost();
    ~HTTPServerHost();

    // Set the request handler callback. Must be set before start().
    void set_callback(Callback cb);

    // Queue an HTTP listener on address:port (e.g., "0.0.0.0", 8080).
    void listen_http(const std::string& address, unsigned short port);

    // Queue an HTTPS listener on address:port with certificate and key files.
    // cert_file: PEM certificate chain
    // key_file:  PEM private key
    void listen_https(const std::string& address,
                      unsigned short port,
                      const std::string& cert_file,
                      const std::string& key_file);

    // Start all configured listeners. Returns immediately.
    void start();

    // Stop all listeners and attempt to join threads.
    void stop();

private:
    struct HttpListenerCfg {
        std::string address;
        unsigned short port{};
    };

    struct HttpsListenerCfg {
        std::string address;
        unsigned short port{};
        std::string cert_file;
        std::string key_file;
    };

    struct ListenerRuntime {
        std::thread thread;
        std::atomic<bool> running{false};
        std::string address;
        unsigned short port{0};
    };

    // Configured endpoints
    std::vector<HttpListenerCfg> http_cfgs_;
    std::vector<HttpsListenerCfg> https_cfgs_;

    // Runtime threads
    std::vector<std::unique_ptr<ListenerRuntime>> http_runtimes_;
    std::vector<std::unique_ptr<ListenerRuntime>> https_runtimes_;

    // User callback (accessed concurrently by session threads)
    Callback callback_;

    // Accept loops (blocking accepts on their own threads)
    static void http_accept_loop(ListenerRuntime* rt,
                                 Callback cb,
                                 std::string address,
                                 unsigned short port);

    static void https_accept_loop(ListenerRuntime* rt,
                                  Callback cb,
                                  std::string address,
                                  unsigned short port,
                                  std::string cert_file,
                                  std::string key_file);

    // Session handlers (run on detached threads)
    static void handle_http_session(boost::asio::ip::tcp::socket socket,
                                    Callback cb);

    static void handle_https_session(boost::asio::ip::tcp::socket socket,
                                     Callback cb,
                                     const std::shared_ptr<ssl::context>& ssl_ctx);
};