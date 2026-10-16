# C++ Networking with Boost.Asio and Linux epoll

A collection of C++17 networking examples demonstrating TCP and UDP communication, asynchronous I/O, persistent client connections, server push messaging, and Linux epoll-based networking.

## Examples

### Boost.Asio
- TCP client and server
- Asynchronous TCP server
- Persistent TCP client
- Push server
- UDP client and server

### Linux
- epoll-based server
- POSIX sockets server

## Requirements

- C++17 compiler
- CMake 3.20+
- Boost development headers
- Linux for the epoll and POSIX socket examples

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel


:

```bash
mkdir -p .github/workflows

cat > .github/workflows/ci.yml <<'EOF'
name: C++ Networking CI

on:
  push:
  pull_request:

jobs:
  build:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake g++ libboost-dev

      - name: Configure
        run: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

      - name: Build
        run: cmake --build build --parallel
