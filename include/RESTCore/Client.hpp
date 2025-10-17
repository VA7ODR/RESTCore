#pragma once

/**
 * \file Client.hpp
 * \brief Synchronous HTTP/HTTPS client based on Boost.Beast/ASIO.
 *
 * This header declares the RESTCore::Client helper class which provides a
 * minimal, blocking API for issuing HTTP(S) requests. It is intentionally
 * small and dependency-light, suitable for tests, tools, and simple apps.
 */

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

namespace RESTCore {

/**
 * \brief Minimal synchronous HTTP/HTTPS client utilities.
 *
 * The Client exposes static convenience methods for common HTTP verbs. Two
 * sets of overloads are available:
 * - host/port/target form where you explicitly choose HTTPS via a boolean
 * - URL form that accepts basic http(s):// URLs and infers the scheme/port
 *
 * For longer conversations, construct a Client::Connection to reuse a
 * keep-alive capable socket across multiple requests and close it explicitly
 * when finished.
 *
 * The functions return a tuple of (status_code, response). Status code is an
 * unsigned integer (e.g., 200) copied from the Boost.Beast response object.
 *
 * Exceptions:
 * - Network, DNS, TLS, or protocol errors are reported via thrown exceptions
 *   (std::system_error, boost::system::system_error, std::invalid_argument for
 *   bad URLs, etc.). Callers may handle exceptions to detect failures.
 *
 * Thread-safety: All functions are re-entrant; they do not share state.
 */
class Client {
public:
    /// Case-insensitive headers are not required here; we use std::map for simplicity.
    using Headers = std::map<std::string, std::string>;
    /// HTTP response type with a string body.
    using Response = boost::beast::http::response<boost::beast::http::string_body>;
    /// Callback invoked for streamed responses; chunk is transient and only valid for the call.
    using ChunkCallback = std::function<void(std::string_view chunk, bool done)>;

    /**
     * \brief Persistent HTTP(S) connection helper for keep-alive interactions.
     *
     * Construct an instance to open a connection, issue one or more requests,
     * and call close() (or let the destructor run) to tear it down.
     */
    class Connection {
    public:
        Connection(bool https, const std::string& host, const std::string& port);
        ~Connection();

        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;
        Connection(Connection&&) noexcept;
        Connection& operator=(Connection&&) noexcept;

        /// Close the underlying socket/stream. Safe to call multiple times.
        void close(const std::string& reason = {});

        /// True if the underlying stream is currently open.
        bool is_open() const;

        /// Human-readable reason describing the most recent closure.
        /// Empty while the connection remains open.
        const std::string& last_close_reason() const;

        /// Issue a request over the persistent connection.
        std::tuple<unsigned, Response>
        request(boost::beast::http::verb method,
                const std::string& target,
                const Headers& headers = {},
                const std::string* body = nullptr,
                const std::string* content_type = nullptr);

        /// Issue a streamed request over the persistent connection.
        unsigned
        stream_request(boost::beast::http::verb method,
                       const std::string& target,
                       const Headers& headers,
                       const std::string* body,
                       const std::string* content_type,
                       const ChunkCallback& on_chunk);

    private:
        bool https_{false};
        std::string host_;
        std::string port_;
        std::shared_ptr<boost::asio::io_context> ioc_;
        std::unique_ptr<boost::beast::tcp_stream> http_stream_;
        std::unique_ptr<boost::asio::ssl::context> ssl_ctx_;
        std::unique_ptr<boost::beast::ssl_stream<boost::beast::tcp_stream>> https_stream_;
        std::string close_reason_;
    };

    /**
     * \name Synchronous helpers by verb (host/port/target form)
     * @{ */
    /** Issue an HTTP HEAD request. */
    static std::tuple<unsigned, Response>
    Head(bool https,
         const std::string& host,
         const std::string& port,
         const std::string& target,
         const Headers& headers = {});

    /** Issue an HTTP GET request. */
    static std::tuple<unsigned, Response>
    Get(bool https,
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const Headers& headers = {});

    /** Issue an HTTP DELETE request. */
    static std::tuple<unsigned, Response>
    Delete(bool https,
           const std::string& host,
           const std::string& port,
           const std::string& target,
           const Headers& headers = {});

    /** Issue an HTTP POST request with an optional body and content-type. */
    static std::tuple<unsigned, Response>
    Post(bool https,
         const std::string& host,
         const std::string& port,
         const std::string& target,
         const std::string& body,
         const std::string& content_type = "application/json",
         const Headers& headers = {});

    /** Issue an HTTP PUT request with an optional body and content-type. */
    static std::tuple<unsigned, Response>
    Put(bool https,
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const std::string& body,
        const std::string& content_type = "application/json",
        const Headers& headers = {});

    /**
     * @name Persistent connection helpers
     * Issue requests using a pre-established Connection (keep-alive).
     * @{ */
    static std::tuple<unsigned, Response>
    Head(Connection& connection, const std::string& target, const Headers& headers = {});

    static std::tuple<unsigned, Response>
    Get(Connection& connection, const std::string& target, const Headers& headers = {});

    static std::tuple<unsigned, Response>
    Delete(Connection& connection, const std::string& target, const Headers& headers = {});

    static std::tuple<unsigned, Response>
    Post(Connection& connection,
         const std::string& target,
         const std::string& body,
         const std::string& content_type = "application/json",
         const Headers& headers = {});

    static std::tuple<unsigned, Response>
    Put(Connection& connection,
        const std::string& target,
        const std::string& body,
        const std::string& content_type = "application/json",
        const Headers& headers = {});
    /** @} */
    /** @} */

    /**
     * \name URL convenience overloads
     * Basic http(s)://host[:port][/path?query] support.
     * @{ */
    static std::tuple<unsigned, Response>
    Head(const std::string& url, const Headers& headers = {});

    static std::tuple<unsigned, Response>
    Get(const std::string& url, const Headers& headers = {});

    static std::tuple<unsigned, Response>
    Delete(const std::string& url, const Headers& headers = {});

    static std::tuple<unsigned, Response>
    Post(const std::string& url,
         const std::string& body,
         const std::string& content_type = "application/json",
         const Headers& headers = {});

    static std::tuple<unsigned, Response>
    Put(const std::string& url,
        const std::string& body,
        const std::string& content_type = "application/json",
        const Headers& headers = {});
    /** @} */

    /**
     * \name Streaming helpers
     * invoke a callback for each chunk; return the HTTP status code.
     * @{ */
    static unsigned
    GetStream(bool https,
              const std::string& host,
              const std::string& port,
              const std::string& target,
              const ChunkCallback& on_chunk);

    static unsigned
    GetStream(bool https,
              const std::string& host,
              const std::string& port,
              const std::string& target,
              const Headers& headers,
              const ChunkCallback& on_chunk);

    static unsigned
    PostStream(bool https,
               const std::string& host,
               const std::string& port,
               const std::string& target,
               const std::string& body,
               const ChunkCallback& on_chunk);

    static unsigned
    PostStream(bool https,
               const std::string& host,
               const std::string& port,
               const std::string& target,
               const std::string& body,
               const std::string& content_type,
               const Headers& headers,
               const ChunkCallback& on_chunk);

    static unsigned
    GetStream(Connection& connection,
              const std::string& target,
              const ChunkCallback& on_chunk);

    static unsigned
    GetStream(Connection& connection,
              const std::string& target,
              const Headers& headers,
              const ChunkCallback& on_chunk);

    static unsigned
    PostStream(Connection& connection,
               const std::string& target,
               const std::string& body,
               const ChunkCallback& on_chunk);

    static unsigned
    PostStream(Connection& connection,
               const std::string& target,
               const std::string& body,
               const std::string& content_type,
               const Headers& headers,
               const ChunkCallback& on_chunk);

    static unsigned
    GetStream(const std::string& url,
              const ChunkCallback& on_chunk);

    static unsigned
    GetStream(const std::string& url,
              const Headers& headers,
              const ChunkCallback& on_chunk);

    static unsigned
    PostStream(const std::string& url,
               const std::string& body,
               const ChunkCallback& on_chunk);

    static unsigned
    PostStream(const std::string& url,
               const std::string& body,
               const std::string& content_type,
               const Headers& headers,
               const ChunkCallback& on_chunk);
private:
    using tcp = boost::asio::ip::tcp;

    /**
     * \brief Internal request dispatch used by all verb helpers.
     *
     * Constructs a Beast request, connects (with or without TLS), sends the
     * request, and reads a full response into memory.
     */
    static std::tuple<unsigned, Response>
    request(bool https,
            boost::beast::http::verb method,
            const std::string& host,
            const std::string& port,
            const std::string& target,
            const Headers& headers,
            const std::string* body,
            const std::string* content_type);

    static unsigned
    stream_request(bool https,
                   boost::beast::http::verb method,
                   const std::string& host,
                   const std::string& port,
                   const std::string& target,
                   const Headers& headers,
                   const std::string* body,
                   const std::string* content_type,
                   const ChunkCallback& on_chunk);

    /// Parsed representation of a small subset of URLs supported by the URL overloads.
    struct ParsedUrl { bool https; std::string host; std::string port; std::string target; };

    /** Parse a basic http(s) URL. Throws std::invalid_argument on invalid input. */
    static ParsedUrl parseUrl(const std::string& url);
};

} // namespace RESTCore
