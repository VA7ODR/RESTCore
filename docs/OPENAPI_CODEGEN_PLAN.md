Title: OpenAPI → C++ (RESTCore) Code Generator: Scope, Design, and Effort Estimate

Overview
- Goal: Generate strongly-typed C++20 server and client stubs that use RESTCore::Server and RESTCore::Client for transport, from an OpenAPI (v3.x) specification.
- Output: A small, header-first SDK for clients and a request-routing server skeleton that calls user-implemented handlers.
- Target users: Teams using RESTCore for tests, tools, or minimal microservices who want repeatable API scaffolding.

High-level deliverables
1) Parser/Model
   - YAML/JSON loader for OpenAPI 3.0/3.1 and in-memory canonical model (subset initially).
   - Resolve $ref (local file and in-doc), components, and shared schemas.
   - Normalization (e.g., transform to C++-friendly names, compute HTTP paths, parameter sources, media types).

2) Type mapping
   - Map OpenAPI schemas to C++20 types with optionality and defaults.
   - Primitives: string, number, integer, boolean, null, enums, arrays, objects.
   - Complex constructs: oneOf/anyOf/allOf (initially: tagged discriminators and simple unions; defer full generality).
   - Nullable and required handling.

3) Serialization
   - JSON serialization/deserialization adapters for generated models. Use a lightweight single-header JSON library or pluggable backends.
   - Initially, provide a minimal JSON backend (e.g., nlohmann/json if allowed, or a tiny internal helper) guarded by a compile switch.

4) Client generator
   - Generate a class per tag (or one client with nested types) with methods per operationId.
   - Each method prepares the path, path/query/header params, and calls RESTCore::Client::{Get|Post|Put|Delete|Head}.
   - Handle request/response bodies (application/json initially) with automatic (de)serialization.
   - Provide error handling policy (status_code return + body, or throwing helpers).

5) Server generator
   - Emit a router based on RESTCore::Server callback signature.
   - Router matches method+path, parses path/query/header params, (de)serializes body, and calls user stubs.
   - Generate handler interface (pure virtual or free functions) for each operationId with typed parameters.
   - Provide a glue function that binds the router to a RESTCore::Server via set_callback.

6) Tooling
   - A CLI tool openapi_codegen that reads an OpenAPI YAML/JSON and emits files into an output directory, with options:
     --language c++
     --client / --server / --both
     --namespace <ns>
     --output <dir>
     --http-client RESTCore
     --json-backend <backend>

MVP scope (Phase 1)
- OpenAPI 3.0.x (JSON or YAML)
- HTTP methods: GET, POST, PUT, DELETE, HEAD
- Params: path, query, header (no cookie in MVP)
- Request/Response bodies: application/json only; status codes 2xx success (single default response type)
- Security: none (or pass-through headers for Authorization for now)
- Types: primitives, arrays, objects (properties), enums. No oneOf/anyOf/allOf in MVP aside from trivial composition.
- Codegen outputs:
  - include/<ns>/models/*.hpp for schemas
  - include/<ns>/client.hpp for client API
  - include/<ns>/server.hpp + src/<ns>/router.cpp for server routing glue

Architecture choices
- Parser: Use a mature OpenAPI model library if permitted (e.g., OpenAPI-Parser in C++ is scarce). Practically, embed a minimal YAML/JSON loader (yaml-cpp + nlohmann/json) and build our own small model for the MVP subset. Keep the model isolated to swap later if needed.
- Templates: Use simple text templates (mustache-like) or C++ string emitters. For simplicity and minimal deps, start with C++ emitters, then migrate to templates when feature set grows.
- Serialization: Pluggable via traits. Default to nlohmann/json if allowed; otherwise add a very small internal adapter for primitives/arrays/objects used by generated code.
- HTTP transport: Always RESTCore. Make the HTTP abstraction thin to enable other transports in the future if desired.

Generated code shape (illustrative)
- Client:
  namespace myapi {
    struct Pet { std::string id; std::string name; };
    class Client {
      std::string base_url;
    public:
      explicit Client(std::string base) : base_url(std::move(base)) {}
      Result<Pet> getPetById(const std::string& petId);
      Result<void> deletePet(const std::string& petId);
    };
  }
- Server:
  namespace myapi {
    struct Handlers { virtual ~Handlers() = default; virtual Pet getPetById(std::string petId) = 0; virtual void deletePet(std::string petId) = 0; };
    void bind_server(RESTCore::Server& srv, std::shared_ptr<Handlers> impl);
  }

Effort estimate
Assumptions:
- Single engineer familiar with C++, CMake, and Boost.
- Goal is a practical MVP with tests, not a full spec-complete tool.

Phase 0 — Research and scaffolding (1–2 days)
- Decide on YAML/JSON libraries and set up minimal models.
- Define naming rules and namespace strategy.

Phase 1 — MVP (2–3 weeks)
- Basic OpenAPI loader and canonical model (path ops, params, schemas subset).
- Type mapping for primitives/arrays/objects/enums.
- JSON (de)serialization generation for models.
- Client codegen using RESTCore::Client; implement URL building and params.
- Server router codegen using RESTCore::Server callback; matching and parsing.
- Unit/integration tests with a couple of sample OpenAPI specs.

Phase 2 — Quality and ergonomics (1–2 weeks)
- Error handling policy (Result/exception), optional retries, timeouts.
- Better response handling (multiple status codes, headers).
- Opt-in features: Authorization header passthrough, api-key injection, basic Bearer JWT handling.
- Improve generated code formatting and docs.

Phase 3 — Extended features (2–4 weeks, optional)
- oneOf/anyOf/allOf handling, discriminators.
- Multipart/form-data, application/x-www-form-urlencoded.
- Cookie params and security schemes.
- Server URL variables, multiple servers per spec, server indexing.
- Codegen templates system and customization hooks.

Total for a solid MVP: ~3–5 weeks
- With subsequent hardening/features: +2–6 weeks depending on scope.

Risks and constraints
- OpenAPI is large; a focused subset is key to hit timelines.
- YAML/JSON library selection impacts portability and build complexity.
- C++ name generation and collision handling need careful design.
- Error handling and backward compatibility for generated code should be stable early.

Testing strategy
- Golden tests: spec → generated files → compare against checked-in goldens.
- Integration tests: build generated server/client, run http_smoke_test-like scenarios.
- Fuzz minimal JSON parsers if we don’t rely on mature libs.

Next steps
- Confirm MVP scope and library choices (nlohmann/json and yaml-cpp acceptable?).
- If yes, create a new tool target openapi_codegen and a sample spec with end-to-end generation in a follow-up change.



Update (2025-10-10)
- Default JSON backend: nlohmann/json. The generated code includes a small adapter at include/RESTCore_`ApiName`/json_backend.hpp wrapping nlohmann::json, while remaining backend-swappable in principle.
- Naming convention: We emit a dedicated namespace scope per spec: RESTCore_`ApiName`, containing Client and Server.
  - `ApiName` derivation (first match wins):
    1) info.x-codegen-name, if present
    2) info.title
    3) Input filename stem
  - Sanitization to a C++-safe identifier:
    - Trim whitespace
    - Replace non-alphanumeric chars with '_'
    - If first char is not [A-Za-z_], prefix '_'
    - Collapse multiple underscores
    - Truncate to 64 characters
    - If the result is a C++ keyword, append '_API'
    - If empty, use 'Api'
- Tooling: Added a minimal generator executable openapi_codegen (MVP scaffold).
  - Usage: openapi_codegen --output <dir> [--input <openapi.(json|yaml)>] [--name <ApiName>]
  - It generates headers: include/RESTCore_<ApiName>/json_backend.hpp, Client.hpp, and Server.hpp.
  - The generator itself has no dependency on nlohmann/json; the generated code references it by include and expects the consumer project to provide it.

Status (2025-10-10)
- PR1 completed: Canonical model skeleton in place, loader supports JSON and a minimal subset of YAML (sufficient for fixtures/tests), and an in-document $ref resolver inlines `#/components/schemas/*`.
- Tests updated: `tests/openapi_codegen_test.sh` now exercises both JSON (petstore.json) and YAML (tests/data/tiny.yaml) inputs and validates generated namespaces and markers.
- Scope guardrails remain: external `$ref` (file/URL), complex YAML features (anchors, inline maps), and advanced schema composition (oneOf/anyOf/allOf) are out-of-scope for MVP and deferred to later phases.
