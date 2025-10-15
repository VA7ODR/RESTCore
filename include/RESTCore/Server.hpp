#pragma once

/**
 * \file Server.hpp
 * \brief Simple synchronous HTTP/HTTPS server built on Boost.Beast/ASIO.
 *
 * The RESTCore::Server accepts connections on configured endpoints and invokes
 * a user-provided callback per request, letting the user fill a response.
 * It is designed for tests and small tools rather than production loads.
 */

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

namespace RESTCore {

/**
 * \brief Minimal multi-connection HTTP(S) host.
 *
 * Behavior and limitations:
 * - One thread per listener (HTTP or HTTPS) running a blocking accept loop.
 * - A new session thread per accepted connection. Sessions may handle
 *   multiple requests when keep-alive support is enabled via
 *   set_keep_alive_enabled().
 * - HTTPS listeners require PEM certificate and private key files.
 * - The server is intended for functional tests and small utilities.
 */
class Server {
public:
    /// Request type exposed to the user callback.
    using Request  = http::request<http::string_body>;
    /// Response type that the user callback should modify.
    using Response = http::response<http::string_body>;

    /**
     * \brief Request handler signature.
     *
     * The server provides:
     * - const Request& req: the parsed HTTP request
     * - Response& res: modify this to set status, headers and body
     * - const std::string& client_address: textual ip[:port]
     */
    using Callback = std::function<void(const Request&, Response&, const std::string&)>;

    /// Construct an idle server without listeners.
    Server();
    /// Destructor stops listeners and joins threads when possible.
    ~Server();

    /** Enable or disable HTTP keep-alive handling (disabled by default). */
    void set_keep_alive_enabled(bool enabled);

    /** Set the request handler callback. Must be set before start(). */
    void set_callback(Callback cb);

    /** Queue an HTTP listener on address:port (e.g., "0.0.0.0", 8080). */
    void listen_http(const std::string& address, unsigned short port);

    /**
     * Queue an HTTPS listener on address:port.
     *
     * \param cert_file Path to a PEM certificate chain file.
     * \param key_file  Path to a PEM private key file.
     */
    void listen_https(const std::string& address,
                      unsigned short port,
                      const std::string& cert_file,
                      const std::string& key_file);

    /** Start all configured listeners. Non-blocking. */
    void start();

    /** Stop all listeners and attempt to join their threads. Idempotent. */
    void stop();

private:
    /// Configuration for an HTTP listener.
    struct HttpListenerCfg {
        std::string address;           ///< Bind address (e.g., "127.0.0.1").
        unsigned short port{};         ///< TCP port.
    };

    /// Configuration for an HTTPS listener.
    struct HttpsListenerCfg {
        std::string address;           ///< Bind address.
        unsigned short port{};         ///< TCP port.
        std::string cert_file;         ///< PEM certificate chain.
        std::string key_file;          ///< PEM private key.
    };

    /// Runtime data for a running listener.
    struct ListenerRuntime {
        std::thread thread;            ///< Background thread running accept loop.
        std::atomic<bool> running{false}; ///< Set true while accept loop is active.
        std::string address;           ///< Bound address for diagnostics.
        unsigned short port{0};        ///< Bound port for diagnostics.
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
                                 unsigned short port,
                                 bool keep_alive_enabled);

    static void https_accept_loop(ListenerRuntime* rt,
                                  Callback cb,
                                  std::string address,
                                  unsigned short port,
                                  std::string cert_file,
                                  std::string key_file,
                                  bool keep_alive_enabled);

    // Session handlers (run on detached threads)
    static void handle_http_session(boost::asio::ip::tcp::socket socket,
                                    Callback cb,
                                    bool keep_alive_enabled);

    static void handle_https_session(boost::asio::ip::tcp::socket socket,
                                     Callback cb,
                                     const std::shared_ptr<ssl::context>& ssl_ctx,
                                     bool keep_alive_enabled);

    bool keep_alive_enabled_{false};
};

} // namespace RESTCore
