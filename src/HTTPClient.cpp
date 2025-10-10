#include "RESTCore/HTTPClient.hpp"

#include <stdexcept>
#include <regex>

using tcp = boost::asio::ip::tcp;
namespace net = boost::asio;
namespace ssl = net::ssl;
namespace beast = boost::beast;
namespace http = beast::http;

HTTPClient::ParsedUrl HTTPClient::parseUrl(const std::string& url) {
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

std::tuple<unsigned, HTTPClient::Response>
HTTPClient::Head(const std::string& url, const Headers& headers) {
    auto p = parseUrl(url);
    return request(p.https, http::verb::head, p.host, p.port, p.target, headers, nullptr, nullptr);
}

std::tuple<unsigned, HTTPClient::Response>
HTTPClient::Get(const std::string& url, const Headers& headers) {
    auto p = parseUrl(url);
    return request(p.https, http::verb::get, p.host, p.port, p.target, headers, nullptr, nullptr);
}

std::tuple<unsigned, HTTPClient::Response>
HTTPClient::Delete(const std::string& url, const Headers& headers) {
    auto p = parseUrl(url);
    return request(p.https, http::verb::delete_, p.host, p.port, p.target, headers, nullptr, nullptr);
}

std::tuple<unsigned, HTTPClient::Response>
HTTPClient::Post(const std::string& url, const std::string& body, const std::string& content_type, const Headers& headers) {
    auto p = parseUrl(url);
    return request(p.https, http::verb::post, p.host, p.port, p.target, headers, &body, &content_type);
}

std::tuple<unsigned, HTTPClient::Response>
HTTPClient::Put(const std::string& url, const std::string& body, const std::string& content_type, const Headers& headers) {
    auto p = parseUrl(url);
    return request(p.https, http::verb::put, p.host, p.port, p.target, headers, &body, &content_type);
}

std::tuple<unsigned, HTTPClient::Response>
HTTPClient::Head(bool https,
                 const std::string& host,
                 const std::string& port,
                 const std::string& target,
                 const Headers& headers) {
    return request(https, http::verb::head, host, port, target, headers, nullptr, nullptr);
}

std::tuple<unsigned, HTTPClient::Response>
HTTPClient::Get(bool https,
                const std::string& host,
                const std::string& port,
                const std::string& target,
                const Headers& headers) {
    return request(https, http::verb::get, host, port, target, headers, nullptr, nullptr);
}

std::tuple<unsigned, HTTPClient::Response>
HTTPClient::Delete(bool https,
                   const std::string& host,
                   const std::string& port,
                   const std::string& target,
                   const Headers& headers) {
    return request(https, http::verb::delete_, host, port, target, headers, nullptr, nullptr);
}

std::tuple<unsigned, HTTPClient::Response>
HTTPClient::Post(bool https,
                 const std::string& host,
                 const std::string& port,
                 const std::string& target,
                 const std::string& body,
                 const std::string& content_type,
                 const Headers& headers) {
    return request(https, http::verb::post, host, port, target, headers, &body, &content_type);
}

std::tuple<unsigned, HTTPClient::Response>
HTTPClient::Put(bool https,
                const std::string& host,
                const std::string& port,
                const std::string& target,
                const std::string& body,
                const std::string& content_type,
                const Headers& headers) {
    return request(https, http::verb::put, host, port, target, headers, &body, &content_type);
}

std::tuple<unsigned, HTTPClient::Response>
HTTPClient::request(bool https,
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