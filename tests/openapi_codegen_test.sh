#!/usr/bin/env bash
set -euo pipefail

GEN_BIN="$1"
SPEC_FILE="$2"
OUT_DIR="$3"

# Clean output dir
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

# Run generator
"$GEN_BIN" --input "$SPEC_FILE" --output "$OUT_DIR"

API_DIR="$OUT_DIR/include/RESTCore_Pet_Store"
JSON_HPP="$API_DIR/json_backend.hpp"
CLIENT_HPP="$API_DIR/Client.hpp"
SERVER_HPP="$API_DIR/Server.hpp"

# Validate files exist
[ -d "$API_DIR" ] || { echo "Expected directory not found: $API_DIR"; exit 1; }
[ -f "$JSON_HPP" ] || { echo "Missing file: $JSON_HPP"; exit 1; }
[ -f "$CLIENT_HPP" ] || { echo "Missing file: $CLIENT_HPP"; exit 1; }
[ -f "$SERVER_HPP" ] || { echo "Missing file: $SERVER_HPP"; exit 1; }

# Validate basic content
# Namespace should reflect sanitized title: "Pet Store" -> Pet_Store

grep -q "namespace RESTCore_Pet_Store" "$JSON_HPP"
grep -q "namespace RESTCore_Pet_Store" "$CLIENT_HPP"
grep -q "namespace RESTCore_Pet_Store" "$SERVER_HPP"

# json_backend.hpp should include nlohmann/json.hpp and define adapter struct Json
grep -q "#include <nlohmann/json.hpp>" "$JSON_HPP"
grep -q "struct Json" "$JSON_HPP"

# Client.hpp should declare class Client with nested Message/Request/Response
grep -q "class Client" "$CLIENT_HPP"
grep -q "class Message" "$CLIENT_HPP"
grep -q "class Request" "$CLIENT_HPP"
grep -q "class Response" "$CLIENT_HPP"

# Server.hpp should declare struct Server with nested Message/Request/Response and Handlers
grep -q "struct Server" "$SERVER_HPP"
grep -q "class Message" "$SERVER_HPP"
grep -q "class Request" "$SERVER_HPP"
grep -q "class Response" "$SERVER_HPP"
grep -q "struct Handlers" "$SERVER_HPP"

echo "openapi_codegen test passed: generated files and contents look correct."