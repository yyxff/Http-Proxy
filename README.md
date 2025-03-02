# HTTP Caching Proxy

## Description
This project implements a high-performance, multi-threaded HTTP caching proxy server that sits between clients and web servers. It forwards requests and caches responses according to HTTP/1.1 specifications, supporting various HTTP methods, caching mechanisms, and connection handling techniques.

## Features
- **HTTP Method Support**: Handles GET, POST, and CONNECT methods
- **Caching Mechanism**: Implements HTTP caching with validation using ETags.
- **Multi-threading**: Supports concurrent connections with thread-per-connection model
- **Chunked Transfer Encoding**: Properly handles chunked responses
- **HTTPS Tunneling**: Supports CONNECT method for secure connections
- **Error Handling**: Comprehensive error handling for network issues and malformed requests
- **Docker Support**: Runs in containerized environments

## Installation
### Requirements
- C++17 or higher
- Boost libraries (Beast, Asio)
- CMake 3.10+
- Docker and Docker Compose (for containerized deployment)

### Local Development
```bash
# Clone the repository
git clone https://gitlab.oit.duke.edu/yy465/erss-hwk2-yy465-jh730.git
cd erss-hwk2-yy465-jh730

### Docker Deployment
```bash
# Navigate to the project directory
cd docker-deploy

# Build and start the container
docker-compose up --build

# View logs
docker-compose logs -f

# Stop the container
docker-compose down
```

## Usage
The proxy listens on port 12345 by default. Configure your browser or application to use this proxy by setting the proxy host to the machine's IP address and the port to 12345.

## Architecture
The proxy is built with a modular design:
- **Proxy**: Main class that handles client connections and request routing
- **Cache**: Thread-safe cache implementation with validation mechanisms
- **Parser**: HTTP request and response parser using Boost.Beast
- **Logger**: Logging system for debugging and monitoring

## Project Structure
```bash
docker-deploy/
├── src/                  # Source code
│   ├── Cache.cpp         # Cache implementation
│   ├── Cache.hpp
│   ├── CacheEntry.cpp    # Cache entry class
│   ├── CacheEntry.hpp
│   ├── Dockerfile        # Docker configuration
│   ├── Logger.cpp        # Logging system
│   ├── Logger.hpp
│   ├── Parser.cpp        # HTTP parser
│   ├── Parser.hpp
│   ├── Proxy.cpp         # Main proxy implementation
│   ├── Proxy.hpp
│   ├── Request.cpp       # HTTP request representation
│   ├── Request.hpp
│   ├── Response.cpp      # HTTP response representation
│   ├── Response.hpp
│   └── main.cpp          # Entry point
├── tests/                # Test suite
│   ├── CacheServer.py    # Test server for cache validation
│   ├── locust.py         # Load testing script
│   └── test_proxy.cpp    # Test cases
├── build/                # Build directory
├── logs/                 # Log files
└── docker-compose.yml    # Docker Compose configuration
```

## Testing
The project includes a comprehensive test suite that verifies various aspects of the proxy:

```bash
# Build and run tests
cd docker-deploy/tests
python3 CacheServer.py
./run_tests.sh
```

### Test Coverage
- Basic HTTP methods (GET, POST)
- HTTPS tunneling (CONNECT)
- Caching and validation
- Chunked transfer encoding
- Error handling
- Concurrent requests

## Design Decisions
- **Thread-per-connection Model**: We chose this model for simplicity and isolation between connections.
- **Boost.Beast for HTTP Parsing**: Leveraging a robust library for HTTP parsing to handle the complexities of the protocol.
- **Mutex-based Cache Synchronization**: Ensuring thread safety with fine-grained locks.
- **Select-based I/O Multiplexing**: For efficient handling of CONNECT tunneling.

## Challenges and Solutions
For a detailed discussion of challenges faced during development and their solutions, please refer to our [Danger Log](dangerlog.md).

## Roadmap
Future improvements we're considering:
- HTTP/2 support
- More sophisticated caching strategies
- Performance optimizations for high-concurrency scenarios
- Enhanced security features

## Authors and acknowledgment
- Jingheng Huan (jh730)
- Yangfan Ye (yy465)

Special thanks to:
- The Boost community for their excellent libraries

## License
This project is part of the ECE 568: Engineering Robust Server Software course at Duke University.

## Project status
This project is currently completed as a course assignment. While we are not actively developing it further, the codebase demonstrates a fully functional HTTP caching proxy with comprehensive test coverage.
