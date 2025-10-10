#pragma once

#include <string>
#include <tuple>
#include <map>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

class HTTPClient {
public:
    using Headers = std::map<std::string, std::string>;
    using Response = boost::beast::http::response<boost::beast::http::string_body>;

    // Synchronous helpers by verb (host/port/target form)
    static std::tuple<unsigned, Response>
    Head(bool https,
         const std::string& host,
         const std::string& port,
         const std::string& target,
         const Headers& headers = {});

    static std::tuple<unsigned, Response>
    Get(bool https,
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const Headers& headers = {});

    static std::tuple<unsigned, Response>
    Delete(bool https,
           const std::string& host,
           const std::string& port,
           const std::string& target,
           const Headers& headers = {});

    static std::tuple<unsigned, Response>
    Post(bool https,
         const std::string& host,
         const std::string& port,
         const std::string& target,
         const std::string& body,
         const std::string& content_type = "application/json",
         const Headers& headers = {});

    static std::tuple<unsigned, Response>
    Put(bool https,
        const std::string& host,
        const std::string& port,
        const std::string& target,
        const std::string& body,
        const std::string& content_type = "application/json",
        const Headers& headers = {});

    // URL convenience overloads (basic http/https URL support)
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

private:
    using tcp = boost::asio::ip::tcp;

    static std::tuple<unsigned, Response>
    request(bool https,
            boost::beast::http::verb method,
            const std::string& host,
            const std::string& port,
            const std::string& target,
            const Headers& headers,
            const std::string* body,
            const std::string* content_type);

    struct ParsedUrl { bool https; std::string host; std::string port; std::string target; };
    static ParsedUrl parseUrl(const std::string& url);
};