#include "RESTCore/Client.hpp"

#include <stdexcept>
#include <regex>
#include <utility>

using tcp = boost::asio::ip::tcp;
namespace net = boost::asio;
namespace ssl = net::ssl;
namespace beast = boost::beast;
namespace http = beast::http;

RESTCore::Client::ParsedUrl RESTCore::Client::parseUrl(const std::string& url) {
    // Very small/basic URL parser for http/https
    // Supports: http(s)://host[:port][/path?query]
    static const std::regex re(R"(^([Hh][Tt][Tt][Pp][Ss]?)://([^/:]+)(?::(\d+))?(\/.*)?$")");
    std::smatch m;
    if (!std::regex_match(url, m, re)) {
        throw std::invalid_argument("Unsupported or invalid URL: " + url);
    }
    ParsedUrl p;
    const auto scheme = m[1].str();
    p.https = (scheme == "https" || scheme == "HTTPS" || scheme == "Https");
    p.host = m[2].str();
    p.port = m[3].matched ? m[3].str() : (p.https ? "443" : "80");
    p.target = m[4].matched ? m[4].str() : std::string("/");
    return p;
}

RESTCore::Client::Connection::Connection(bool https,
                                         const std::string& host,
                                         const std::string& port)
    : https_(https),
      host_(host),
      port_(port),
      ioc_(std::make_shared<net::io_context>()),
      close_reason_() {
    tcp::resolver resolver{*ioc_};
    auto const results = resolver.resolve(host_, port_);

    if (!https_) {
        http_stream_ = std::make_unique<beast::tcp_stream>(*ioc_);
        http_stream_->connect(results);
    } else {
        ssl_ctx_ = std::make_unique<ssl::context>(ssl::context::tls_client);
        ssl_ctx_->set_default_verify_paths();
        ssl_ctx_->set_verify_mode(ssl::verify_peer);

        https_stream_ = std::make_unique<beast::ssl_stream<beast::tcp_stream>>(*ioc_, *ssl_ctx_);
        if(! SSL_set_tlsext_host_name(https_stream_->native_handle(), host_.c_str())) {
            beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
            throw beast::system_error{ec};
        }

        beast::get_lowest_layer(*https_stream_).connect(results);
        https_stream_->handshake(ssl::stream_base::client);
    }
}

RESTCore::Client::Connection::~Connection() { close(); }

RESTCore::Client::Connection::Connection(Connection&& other) noexcept
    : https_(other.https_),
      host_(std::move(other.host_)),
      port_(std::move(other.port_)),
      ioc_(std::move(other.ioc_)),
      http_stream_(std::move(other.http_stream_)),
      ssl_ctx_(std::move(other.ssl_ctx_)),
      https_stream_(std::move(other.https_stream_)),
      close_reason_(std::move(other.close_reason_)) {}

RESTCore::Client::Connection&
RESTCore::Client::Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        close();
        https_ = other.https_;
        host_ = std::move(other.host_);
        port_ = std::move(other.port_);
        ioc_ = std::move(other.ioc_);
        http_stream_ = std::move(other.http_stream_);
        ssl_ctx_ = std::move(other.ssl_ctx_);
        https_stream_ = std::move(other.https_stream_);
        close_reason_ = std::move(other.close_reason_);
    }
    return *this;
}

bool RESTCore::Client::Connection::is_open() const {
    if (!https_) {
        return http_stream_ && http_stream_->socket().is_open();
    }
    return https_stream_ && beast::get_lowest_layer(*https_stream_).socket().is_open();
}

void RESTCore::Client::Connection::close(const std::string& reason) {
    const bool was_open = is_open();

    if (!https_) {
        if (http_stream_) {
            beast::error_code ec;
            http_stream_->socket().shutdown(tcp::socket::shutdown_both, ec);
            http_stream_->socket().close(ec);
            http_stream_.reset();
        }
    } else {
        if (https_stream_) {
            beast::error_code ec;
            https_stream_->shutdown(ec);
            beast::get_lowest_layer(*https_stream_).socket().shutdown(tcp::socket::shutdown_both, ec);
            beast::get_lowest_layer(*https_stream_).socket().close(ec);
            https_stream_.reset();
        }
        ssl_ctx_.reset();
    }

    if (was_open) {
        if (!reason.empty()) {
            close_reason_ = reason;
        } else {
            close_reason_ = "Connection closed";
        }
    } else if (!reason.empty()) {
        close_reason_ = reason;
    }
}

const std::string& RESTCore::Client::Connection::last_close_reason() const { return close_reason_; }

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::Connection::request(http::verb method,
                                      const std::string& target,
                                      const Headers& headers,
                                      const std::string* body,
                                      const std::string* content_type) {
    if (!is_open()) {
        if (close_reason_.empty()) {
            throw std::runtime_error("Connection is closed");
        }
        throw std::runtime_error("Connection is closed: " + close_reason_);
    }

    http::request<http::string_body> req{method, target, 11};
    req.set(http::field::host, host_);
    req.set(http::field::user_agent, std::string{"HTTPClient/1.0 (Boost.Beast)"});
    req.keep_alive(true);
    for (const auto& kv : headers) {
        req.set(kv.first, kv.second);
    }
    if (body) {
        req.set(http::field::content_type,
                content_type ? *content_type : std::string{"application/octet-stream"});
        req.body() = *body;
        req.prepare_payload();
    }

    Response res;
    beast::flat_buffer buffer;

    try {
        if (!https_) {
            http::write(*http_stream_, req);
            http::read(*http_stream_, buffer, res);
        } else {
            http::write(*https_stream_, req);
            http::read(*https_stream_, buffer, res);
        }
    } catch (const beast::system_error& se) {
        std::string message;
        if (se.code() == http::error::end_of_stream) {
            message = "Peer closed the connection during the HTTP exchange (keep-alive unsupported or timed out): " +
                      se.code().message();
        } else {
            message = "HTTP keep-alive exchange failed: " + se.code().message();
        }
        close(message);
        throw beast::system_error{se.code(), message};
    }

    if (!res.keep_alive()) {
        close("Server indicated Connection: close (keep-alive disabled or mismatch).");
    } else {
        close_reason_.clear();
    }

    return {res.result_int(), std::move(res)};
}

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::Head(const std::string& url, const Headers& headers) {
    auto p = parseUrl(url);
    return request(p.https, http::verb::head, p.host, p.port, p.target, headers, nullptr, nullptr);
}

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::Get(const std::string& url, const Headers& headers) {
    auto p = parseUrl(url);
    return request(p.https, http::verb::get, p.host, p.port, p.target, headers, nullptr, nullptr);
}

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::Delete(const std::string& url, const Headers& headers) {
    auto p = parseUrl(url);
    return request(p.https, http::verb::delete_, p.host, p.port, p.target, headers, nullptr, nullptr);
}

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::Post(const std::string& url, const std::string& body, const std::string& content_type, const Headers& headers) {
    auto p = parseUrl(url);
    return request(p.https, http::verb::post, p.host, p.port, p.target, headers, &body, &content_type);
}

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::Put(const std::string& url, const std::string& body, const std::string& content_type, const Headers& headers) {
    auto p = parseUrl(url);
    return request(p.https, http::verb::put, p.host, p.port, p.target, headers, &body, &content_type);
}

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::Head(bool https,
                 const std::string& host,
                 const std::string& port,
                 const std::string& target,
                 const Headers& headers) {
    return request(https, http::verb::head, host, port, target, headers, nullptr, nullptr);
}

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::Get(bool https,
                const std::string& host,
                const std::string& port,
                const std::string& target,
                const Headers& headers) {
    return request(https, http::verb::get, host, port, target, headers, nullptr, nullptr);
}

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::Delete(bool https,
                   const std::string& host,
                   const std::string& port,
                   const std::string& target,
                   const Headers& headers) {
    return request(https, http::verb::delete_, host, port, target, headers, nullptr, nullptr);
}

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::Post(bool https,
                 const std::string& host,
                 const std::string& port,
                 const std::string& target,
                 const std::string& body,
                 const std::string& content_type,
                 const Headers& headers) {
    return request(https, http::verb::post, host, port, target, headers, &body, &content_type);
}

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::Put(bool https,
                const std::string& host,
                const std::string& port,
                const std::string& target,
                const std::string& body,
                const std::string& content_type,
                const Headers& headers) {
    return request(https, http::verb::put, host, port, target, headers, &body, &content_type);
}

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::Head(Connection& connection,
                 const std::string& target,
                 const Headers& headers) {
    return connection.request(http::verb::head, target, headers, nullptr, nullptr);
}

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::Get(Connection& connection,
                const std::string& target,
                const Headers& headers) {
    return connection.request(http::verb::get, target, headers, nullptr, nullptr);
}

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::Delete(Connection& connection,
                   const std::string& target,
                   const Headers& headers) {
    return connection.request(http::verb::delete_, target, headers, nullptr, nullptr);
}

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::Post(Connection& connection,
                 const std::string& target,
                 const std::string& body,
                 const std::string& content_type,
                 const Headers& headers) {
    return connection.request(http::verb::post, target, headers, &body, &content_type);
}

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::Put(Connection& connection,
                const std::string& target,
                const std::string& body,
                const std::string& content_type,
                const Headers& headers) {
    return connection.request(http::verb::put, target, headers, &body, &content_type);
}

std::tuple<unsigned, RESTCore::Client::Response>
RESTCore::Client::request(bool https,
                    http::verb method,
                    const std::string& host,
                    const std::string& port,
                    const std::string& target,
                    const Headers& headers,
                    const std::string* body,
                    const std::string* content_type) {
    net::io_context ioc;

    // Resolve
    tcp::resolver resolver{ioc};
    auto const results = resolver.resolve(host, port);

    // Common HTTP request setup
    http::request<http::string_body> req{method, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, std::string{"HTTPClient/1.0 (Boost.Beast)"});
    req.keep_alive(false);
    for (const auto& kv : headers) {
        req.set(kv.first, kv.second);
    }
    if (body) {
        req.set(http::field::content_type, content_type ? *content_type : std::string{"application/octet-stream"});
        req.body() = *body;
        req.prepare_payload(); // sets Content-Length
    }

    Response res;

    if (!https) {
        // Plain HTTP
        beast::tcp_stream stream{ioc};
        stream.connect(results);

        http::write(stream, req);
        beast::flat_buffer buffer;
        http::read(stream, buffer, res);

        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        // ignore shutdown errors

    } else {
        // HTTPS (TLS)
        ssl::context ctx{ssl::context::tls_client};
        ctx.set_default_verify_paths();
        ctx.set_verify_mode(ssl::verify_peer);

        beast::ssl_stream<beast::tcp_stream> stream{ioc, ctx};

        // SNI
        if(! SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
            throw beast::system_error{ec};
        }

        // Connect
        beast::get_lowest_layer(stream).connect(results);

        // TLS handshake
        stream.handshake(ssl::stream_base::client);

        http::write(stream, req);
        beast::flat_buffer buffer;
        http::read(stream, buffer, res);

        beast::error_code ec;
        // Graceful TLS shutdown (ignore errors to avoid nasty EOFs from servers)
        stream.shutdown(ec);
    }

    return {res.result_int(), std::move(res)};
}
