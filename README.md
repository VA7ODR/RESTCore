# RESTCore

A minimal example project that demonstrates:
- Server: a small synchronous HTTP(S) server built on Boost.Beast/ASIO
- Client: a simple HTTP/HTTPS client using Boost.Beast/ASIO

The repository also includes a small Boost.Test-based test suite that exercises the server and client together.


## Quick examples

A minimal taste of the API. These are intentionally small and synchronous so you can copy/paste them into a scratch file or a unit test.

- Server: create a RESTCore::Server, register a callback, bind to an address/port, and start. The server runs listener threads in the background; call stop() on shutdown.
- Client: use RESTCore::Client helpers (Get/Post/Put/Delete/Head) either with a full URL or with host/port/target arguments.

### Server example (HTTP)
```cpp
#include <RESTCore/Server.hpp>
#include <csignal>
#include <iostream>

int main() {
    using namespace RESTCore;

    // Optional: ignore SIGPIPE on Unix-like systems
    std::signal(SIGPIPE, SIG_IGN);

    Server server;

    // Simple handler: always 200 OK and echoes the target path
    server.set_callback([](const Server::Request& req,
                           Server::Response& res,
                           const std::string& /*client*/){
        res.result(boost::beast::http::status::ok);
        res.set(boost::beast::http::field::content_type, "text/plain; charset=utf-8");
        res.body() = std::string("Hello from RESTCore! You requested: ") + std::string(req.target());
        res.prepare_payload();
    });

    // Bind an HTTP listener and start in background threads
    server.listen_http("127.0.0.1", 8080);
    server.start();

    std::cout << "Server listening on http://127.0.0.1:8080 — press Enter to stop...\n";
    std::cin.get();

    server.stop();
}
```

### Client example (HTTP)
```cpp
#include <RESTCore/Client.hpp>
#include <iostream>

int main() {
    using namespace RESTCore;

    try {
        auto [status, res] = Client::Get(false, "127.0.0.1", "8080", "/hello");
        std::cout << "Status: " << status << "\n";
        std::cout << "Content-Type: " << res[boost::beast::http::field::content_type] << "\n";
        std::cout << "Body: \n" << res.body() << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "Request failed: " << ex.what() << "\n";
    }
}
```

Notes:
- For HTTPS, pass true as the first argument and ensure your system has CA certificates installed (the client performs peer verification by default).
- URL overloads are available too, e.g., `Client::Get("http://127.0.0.1:8080/hello");`.

## Project layout
- include/RESTCore: Public headers for the reusable library (Client.hpp, Server.hpp)
- src: Library implementation sources (Client.cpp, Server.cpp)
- tests: Boost.Test-based tests and fixtures
- CMakeLists.txt: Target-based build; library target RESTCore and test http_smoke_test

This layout follows common C++/CMake conventions: public headers live under include/<project>, sources under src, tests in tests, and external components in dedicated subdirectories.

## Build from a source tarball (Linux)
These steps assume a typical Debian/Ubuntu-like environment; adjust package names for your distro.

### 1) Install prerequisites
- CMake >= 3.16
- A C++20 compiler (GCC 10+ or Clang 12+ recommended)
- Boost (headers plus system and thread components)
- OpenSSL development libraries
- pthreads (usually part of glibc)

On Debian/Ubuntu:
```
sudo apt-get update
sudo apt-get install -y build-essential cmake \
    libboost-system-dev libboost-thread-dev \
    libssl-dev
```

### 2) Extract the tarball
```
# replace with your actual tarball name
 tar -xjf RESTCore-src-YYYYMMDD.tar.bz2
 cd RESTCore
```

### 3) Configure and build
Create a build directory (out-of-source build recommended):
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target RESTCore -j
```

To build the tests as well (they are enabled by default):
```
cmake --build build --target http_smoke_test -j
```



## Running tests
This project uses CTest with Boost.Test (single-header) for a lightweight setup.

From your build directory:
```
# Option A: build+run via CTest
cmake --build build --target http_smoke_test && \
  ctest --test-dir build --output-on-failure

# Option B: run the test executable directly
./build/http_smoke_test
```

Notes:
- The tests start a local HTTP server bound to 127.0.0.1 on an ephemeral port.
- The tests avoid port conflicts by probing for a free port and waiting until the server is ready.


## Troubleshooting
- If you see link errors about Boost, ensure the dev packages for Boost.System and Boost.Thread are installed and your compiler finds Boost headers.
- If OpenSSL is missing, install `libssl-dev` (or your distro’s equivalent).
- On systems with stricter OpenSSL provider defaults, you may need up-to-date OpenSSL (1.1.1+ recommended; 3.x works as well).


## License
This project is licensed under the MIT License. See the LICENSE file.

Copyright (C) 2025 James Baker.
