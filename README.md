# RESTCore

Reference documentation: https://va7odr.github.io/RESTCore/html/index.html

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

This layout follows common C++/CMake conventions: public headers live under `include/<project>`, sources under `src`, tests in `tests`, and external components in dedicated subdirectories.

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
This project uses plain CTest with Boost.Test (single-header) — no legacy dashboard targets required.

From your build directory:
```
# Option A: build+run via CTest
cmake --build build --target http_smoke_test && \
  ctest --test-dir build --output-on-failure

# Option B: run the test executable directly
./build/http_smoke_test
```

Notes:
- We no longer use the old CTest/CDash dashboard targets (Experimental/Nightly/Continuous). If you saw them before, they came from including the CTest module. We now call `enable_testing()` only, so those targets are not generated.
- The tests start a local HTTP server bound to 127.0.0.1 on an ephemeral port.
- The tests avoid port conflicts by probing for a free port and waiting until the server is ready.

## API documentation (Doxygen)
If Doxygen is installed, you can generate HTML API docs via the `docs` target:
```
cmake --build <your-build-dir> --target docs
```
- Output is written under `docs/html` as configured by `Doxyfile` in the repository root.
- RESTCore public headers are documented and will appear in the generated reference.
- The OpenAPI generator emits Doxygen-friendly comments in generated headers (`json_backend.hpp`, `Client.hpp`, `Server.hpp`), so your generated SDKs can be included in your own Doxygen runs.


## Troubleshooting
- If you see link errors about Boost, ensure the dev packages for Boost.System and Boost.Thread are installed and your compiler finds Boost headers.
- If OpenSSL is missing, install `libssl-dev` (or your distro’s equivalent).
- On systems with stricter OpenSSL provider defaults, you may need up-to-date OpenSSL (1.1.1+ recommended; 3.x works as well).


## License
This project is licensed under the MIT License. See the LICENSE file.

Copyright (C) 2025 James Baker.


## OpenAPI → C++ codegen (using RESTCore)
If you are considering generating C++ client/server code from an OpenAPI spec that targets RESTCore::Client and RESTCore::Server, see docs/OPENAPI_CODEGEN_PLAN.md for a detailed design and work estimate.

Summary:
- An MVP covering OpenAPI 3.0 with JSON bodies, path/query/header params, and basic response handling is estimated at ~3–5 weeks for a single engineer.
- The generated client would wrap RESTCore::Client; the server side would emit a router that binds to RESTCore::Server via set_callback.
- Extended features (security schemes, multipart, oneOf/anyOf/allOf, templates) add 2–6+ weeks depending on scope.


## OpenAPI code generator (MVP scaffold)
A minimal generator executable is available as the target `openapi_codegen`.

- Build:
  cmake --build <your-build-dir> --target openapi_codegen

- Usage:
  openapi_codegen --output <dir> [--input <openapi.(json|yaml)>] [--name <ApiName>]

What it does (today):
- Derives an `ApiName` from `info.title` in the spec (or from the file name), sanitizes it to a C++-safe identifier, and creates a namespace scope `RESTCore_ApiName`.
- Emits three headers under the output directory:
  - include/RESTCore_`ApiName`/json_backend.hpp — a small adapter that uses nlohmann::json by default (backend-swappable in principle)
  - include/RESTCore_`ApiName`/Client.hpp — a client facade with nested JSON-backed message types: `class Message`, `class Request`, `class Response`
  - include/RESTCore_`ApiName`/Server.hpp — a server facade with nested message types and a `struct Handlers` interface (one virtual per operation will be generated later)

Message classes are JSON-backed but present a JSON-agnostic API:
- Presence check: `bool has(const std::string& key) const`
- Typed getters/setters: `get_string/int64/uint64/double/bool` and `set_string/int64/uint64/double/bool`
- Convenience example (field-specific accessors may be generated later):
  `const std::string& favourite_pet();` and `void set_favourite_pet(std::string);`

Documentation:
- The generated headers include Doxygen-friendly comments, so they show up cleanly in your API docs.
- The generator itself does not depend on nlohmann/json at build time; the generated code includes `<nlohmann/json.hpp>`.

Notes:
- Your consumer project should bring nlohmann/json (via package manager, FetchContent, or system install).
- The long-term plan remains to generate full client/server code from OpenAPI as described in docs/OPENAPI_CODEGEN_PLAN.md, with `nlohmann/json` as the default JSON backend and backend-swappability retained.


## OpenAPI echo example (server + client)
Note: This example was hand-written; it was not generated by the openapi_codegen tool. The included openapi.yaml serves as a reference to the API shape.
This repository includes a tiny end-to-end example consisting of:
- A simple OpenAPI 3.0 spec (text/plain) defining two operations: GET /motd and POST /echo
- A server executable implementing those endpoints using RESTCore::Server
- A client executable invoking them using RESTCore::Client

Files:
- examples/openapi_echo/openapi.yaml — the OpenAPI 3.0 spec (plain-text bodies)
- examples/openapi_echo/openapi_echo_server.cpp — example server
- examples/openapi_echo/openapi_echo_client.cpp — example client

Enable the example during CMake configure:
- Option: RESTCORE_BUILD_OPENAPI_ECHO_EXAMPLE (default: OFF)

Example (out-of-source build shown):
```
cmake -S . -B build -DRESTCORE_BUILD_OPENAPI_ECHO_EXAMPLE=ON
cmake --build build --target openapi_echo_server openapi_echo_client -j
```

Run the server (defaults to 127.0.0.1:9090):
```
# Bind to 127.0.0.1:9090 and wait for Enter to stop
./build/examples/openapi_echo/openapi_echo_server

# Bind to 0.0.0.0:8080
./build/examples/openapi_echo/openapi_echo_server 0.0.0.0 8080

# For automation/CI you can run for a fixed duration (no stdin required)
./build/examples/openapi_echo/openapi_echo_server 127.0.0.1 9090 --duration 5
```

Call it with the client:
```
# MOTD
./build/examples/openapi_echo/openapi_echo_client --host 127.0.0.1 --port 9090 --motd

# Echo
./build/examples/openapi_echo/openapi_echo_client --host 127.0.0.1 --port 9090 --echo "hello world"
```

API shape (OpenAPI 3.0):
- GET /motd → 200 text/plain: "MOTD: Welcome to RESTCore!"
- POST /echo (text/plain) → 200 text/plain: "Echo: <UPPERCASED_INPUT>"

Note: The example keeps the payloads text/plain to minimize dependencies and keep the code simple. The same structure can be adapted to JSON if needed.


## OpenAPI generated example (using openapi_codegen)
This second, more advanced example runs the `openapi_codegen` tool at build time to generate C++ headers (JSON model facade) and uses them in a server and a client.

Highlights:
- JSON responses and query parameters.
- Demonstrates including and using generated headers: `RESTCore_GeneratedExample/Server.hpp` and `.../Client.hpp`.
- For convenience, the example provides a tiny local stub for `<nlohmann/json.hpp>` under `examples/openapi_generated/third_party` so it builds out-of-the-box. In real projects, depend on the real nlohmann/json instead.

Files:
- examples/openapi_generated/openapi.yaml — OpenAPI 3.0 spec with /info and /shout (JSON)
- examples/openapi_generated/openapi_generated_server.cpp — server using generated JSON Message types for responses
- examples/openapi_generated/openapi_generated_client.cpp — client parsing JSON responses with generated types

Enable during CMake configure:
- Option: RESTCORE_BUILD_OPENAPI_GENERATED_EXAMPLE (default: OFF)

Build targets:
```
cmake -S . -B build -DRESTCORE_BUILD_OPENAPI_GENERATED_EXAMPLE=ON
cmake --build build --target openapi_generated_server openapi_generated_client -j
```

Run it (defaults: 127.0.0.1:9094):
```
# Start server (press Enter to stop)
./build/examples/openapi_generated/openapi_generated_server

# Or run for a fixed duration (CI-friendly)
./build/examples/openapi_generated/openapi_generated_server 127.0.0.1 9094 --duration 5

# Query service info
./build/examples/openapi_generated/openapi_generated_client --host 127.0.0.1 --port 9094 --info

# Call shout (query params), print JSON "result"
./build/examples/openapi_generated/openapi_generated_client --host 127.0.0.1 --port 9094 --shout "hello world" --upper true
```

Expected outputs:
- /info ⇒ JSON object with fields `service`, `version`, and `endpoints` array.
- /shout ⇒ JSON object with field `result` (uppercased if `upper=true`).
