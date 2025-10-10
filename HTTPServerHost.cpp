#include "HTTPServerHost.hpp"

#include <fstream>
#include <iostream>

using tcp = boost::asio::ip::tcp;
namespace net = boost::asio;
namespace ssl = net::ssl;
namespace beast = boost::beast;
namespace http = beast::http;

HTTPServerHost::HTTPServerHost() = default;
HTTPServerHost::~HTTPServerHost() { stop(); }

void HTTPServerHost::set_callback(Callback cb) { callback_ = std::move(cb); }

void HTTPServerHost::listen_http(const std::string& address, unsigned short port) {
    http_cfgs_.push_back(HttpListenerCfg{address, port});
}

void HTTPServerHost::listen_https(const std::string& address,
                                  unsigned short port,
                                  const std::string& cert_file,
                                  const std::string& key_file) {
    https_cfgs_.push_back(HttpsListenerCfg{address, port, cert_file, key_file});
}

void HTTPServerHost::start() {
    // Start HTTP listeners
    for (const auto& cfg : http_cfgs_) {
        auto rt = std::make_unique<ListenerRuntime>();
        rt->running.store(true);
        rt->address = cfg.address;
        rt->port = cfg.port;
        auto cb = callback_;
        rt->thread = std::thread([rt_ptr = rt.get(), cb, cfg]() mutable {
            http_accept_loop(rt_ptr, cb, cfg.address, cfg.port);
        });
        http_runtimes_.push_back(std::move(rt));
    }

    // Start HTTPS listeners
    for (const auto& cfg : https_cfgs_) {
        auto rt = std::make_unique<ListenerRuntime>();
        rt->running.store(true);
        rt->address = cfg.address;
        rt->port = cfg.port;
        auto cb = callback_;
        rt->thread = std::thread([rt_ptr = rt.get(), cb, cfg]() mutable {
            https_accept_loop(rt_ptr, cb, cfg.address, cfg.port, cfg.cert_file, cfg.key_file);
        });
        https_runtimes_.push_back(std::move(rt));
    }
}

void HTTPServerHost::stop() {
    auto wake = [](const std::string& addr, unsigned short port) {
        try {
            std::string connect_addr = addr;
            if (addr == "0.0.0.0") connect_addr = "127.0.0.1";
            else if (addr == "::") connect_addr = "::1";
            net::io_context ioc;
            tcp::socket sock{ioc};
            boost::system::error_code ec;
            auto ip = net::ip::make_address(connect_addr, ec);
            if (ec) return; // invalid addr, give up
            tcp::endpoint ep{ip, port};
            sock.connect(ep, ec);
            // immediately close; connection existing is enough to unblock accept
        } catch (...) {
            // ignore
        }
    };

    for (auto& rt : http_runtimes_) {
        bool was_running = rt->running.exchange(false);
        if (was_running) {
            wake(rt->address, rt->port);
        }
    }
    for (auto& rt : https_runtimes_) {
        bool was_running = rt->running.exchange(false);
        if (was_running) {
            wake(rt->address, rt->port);
        }
    }
    for (auto& rt : http_runtimes_) {
        if (rt->thread.joinable()) rt->thread.join();
    }
    for (auto& rt : https_runtimes_) {
        if (rt->thread.joinable()) rt->thread.join();
    }
    http_runtimes_.clear();
    https_runtimes_.clear();
}

void HTTPServerHost::http_accept_loop(ListenerRuntime* rt,
                                      Callback cb,
                                      std::string address,
                                      unsigned short port) {
    try {
        net::io_context ioc;
        tcp::endpoint ep{net::ip::make_address(address), port};
        tcp::acceptor acceptor{ioc};
        acceptor.open(ep.protocol());
        acceptor.set_option(net::socket_base::reuse_address(true));
        acceptor.bind(ep);
        acceptor.listen(net::socket_base::max_listen_connections);

        while (rt->running.load()) {
            tcp::socket socket{ioc};
            boost::system::error_code ec;
            acceptor.accept(socket, ec);
            if (ec) {
                if (!rt->running.load()) break;
                continue; // transient error, continue
            }
            std::thread(&HTTPServerHost::handle_http_session, std::move(socket), cb).detach();
        }
    } catch (const std::exception& e) {
        // Log and exit loop
        (void)e;
    }
}

void HTTPServerHost::https_accept_loop(ListenerRuntime* rt,
                                       Callback cb,
                                       std::string address,
                                       unsigned short port,
                                       std::string cert_file,
                                       std::string key_file) {
    try {
        net::io_context ioc;
        tcp::endpoint ep{net::ip::make_address(address), port};
        tcp::acceptor acceptor{ioc};
        acceptor.open(ep.protocol());
        acceptor.set_option(net::socket_base::reuse_address(true));
        acceptor.bind(ep);
        acceptor.listen(net::socket_base::max_listen_connections);

        auto ctx = std::make_shared<ssl::context>(ssl::context::tls_server);
        ctx->set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 | ssl::context::no_sslv3);
        ctx->use_certificate_chain_file(cert_file);
        ctx->use_private_key_file(key_file, ssl::context::file_format::pem);

        while (rt->running.load()) {
            tcp::socket socket{ioc};
            boost::system::error_code ec;
            acceptor.accept(socket, ec);
            if (ec) {
                if (!rt->running.load()) break;
                continue;
            }
            std::thread(&HTTPServerHost::handle_https_session, std::move(socket), cb, ctx).detach();
        }
    } catch (const std::exception& e) {
        // Log and exit loop
        (void)e;
    }
}

static std::string remote_addr_string(const tcp::socket& s) {
    boost::system::error_code ec;
    auto ep = s.remote_endpoint(ec);
    if (ec) return std::string("unknown");
    return ep.address().to_string() + ":" + std::to_string(ep.port());
}

void HTTPServerHost::handle_http_session(tcp::socket socket, Callback cb) {
    try {
        beast::tcp_stream stream{std::move(socket)};
        beast::flat_buffer buffer;
        HTTPServerHost::Request req;
        http::read(stream, buffer, req);

        HTTPServerHost::Response res{http::status::ok, req.version()};
        res.set(http::field::server, std::string{"HTTPServerHost/1.0"});
        res.keep_alive(false);

        const std::string client = remote_addr_string(stream.socket());

        if (cb) cb(req, res, client);

        // Ensure we have content-length if body set
        if (res.body().size() > 0 && res.find(http::field::content_length) == res.end()) {
            res.prepare_payload();
        }

        http::write(stream, res);

        boost::system::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    } catch (...) {
        // swallow
    }
}

void HTTPServerHost::handle_https_session(tcp::socket socket,
                                          Callback cb,
                                          const std::shared_ptr<ssl::context>& ssl_ctx) {
    try {
        beast::ssl_stream<beast::tcp_stream> stream{std::move(socket), *ssl_ctx};
        // Server-side TLS handshake
        stream.handshake(ssl::stream_base::server);

        beast::flat_buffer buffer;
        HTTPServerHost::Request req;
        http::read(stream, buffer, req);

        HTTPServerHost::Response res{http::status::ok, req.version()};
        res.set(http::field::server, std::string{"HTTPServerHost/1.0 (TLS)"});
        res.keep_alive(false);

        const std::string client = remote_addr_string(beast::get_lowest_layer(stream).socket());

        if (cb) cb(req, res, client);

        if (res.body().size() > 0 && res.find(http::field::content_length) == res.end()) {
            res.prepare_payload();
        }

        http::write(stream, res);

        boost::system::error_code ec;
        stream.shutdown(ec);
    } catch (...) {
        // swallow
    }
}
