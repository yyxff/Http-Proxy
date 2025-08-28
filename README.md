# HTTP Caching Proxy

## Description
This project implements a high-performance, multi-threaded HTTP caching proxy server that sits between clients and web servers. It forwards requests and caches responses according to HTTP/1.1 specifications, supporting various HTTP methods, caching mechanisms, and connection handling techniques.

## Features
- **HTTP Method Support**: Handles GET, POST, and CONNECT methods
- **Reactor Model**: Reactor modle with epoll
- **Multi-threading**: Supports concurrent connections with thread-per-connection model
- **Dynamic ThreadPool**: dispatch fd to a dynamic thread pool, RPS: 1000, response time(p50): 90ms
- **Caching Mechanism**: Implements HTTP caching with validation using ETags and Expires, etc.
- **Cache Segmentation**: we segment caches into 8 pieces, every segment has their own mutex lock. So we can use different caches at same time. It is at most 8 times more efficient.
- **Chunked Transfer Encoding**: Properly handles chunked responses
- **HTTPS Tunneling**: Supports CONNECT method for secure connections
- **Logging System**: Implemented Logging system with singleton pattern
- **Error Handling**: Comprehensive error handling for network issues and malformed requests
- **Docker Support**: Runs in containerized environments
- **Automated Test**: Automated test for basic behaviors and concurrency

## Installation
### Requirements
- C++17 or higher
- Boost libraries (Beast, Asio)
- CMake 3.10+
- Docker and Docker Compose (for containerized deployment)

### Deployment
```bash
# Clone the repository
git clone git@github.com:yyxff/Http-Proxy.git
cd Http-Proxy
```

### Docker Deployment
```bash
# Navigate to the project directory
cd docker-deploy

# Build and start the container
docker-compose up proxy --build

# View logs
docker-compose logs -f
# or view it in volume directory
cat ./logs/proxy.log

# Stop the container
docker-compose down
```

## Usage
The proxy listens on port 12345 by default. Docker has exposed it.
```
Configure your browser proxy for HTTP and HTTPS as
proxy: <proxy_ip> : <proxy_port>

example:
proxy: vcm-45xxx.vm.duke.edu : 12345
```

## Architecture
The proxy is built with a modular design:
- **Proxy**: Main class that handles client connections and request routing
- **Cache**: Thread-safe cache implementation with validation mechanisms
- **Parser**: HTTP request and response parser using Boost.Beast
- **Logger**: Logging system for debugging and monitoring

## Project Structure
```bash
docker-deploy/
├── src/
│   ├── main.cpp                 # Entry point for the proxy server
│   ├── Proxy.cpp                # Main proxy implementation
│   ├── Proxy.hpp                # Proxy class declaration
│   ├── Logger.cpp               # Logging functionality
│   ├── Logger.hpp               # Logger class declaration
│   ├── Cache.cpp                # Cache implementation
│   ├── Cache.hpp                # Cache class declaration
│   ├── CacheMaster.cpp          # Cache Master implementation
│   ├── CacheMaster.hpp          # Cache Master class declaration
│   ├── CacheEntry.cpp           # Cache Entry implementation
│   ├── CacheEntry.hpp           # Cache Entry class declaration
│   ├── CacheDecision.cpp        # Cache decision making logic
│   ├── CacheDecision.hpp        # CacheDecision class declaration
│   ├── CacheHandler.cpp         # Cache handling implementation
│   ├── CacheHandler.hpp         # CacheHandler class declaration
│   ├── Parser.cpp               # HTTP message parsing
│   ├── Parser.hpp               # HTTPParser class declaration
│   ├── CmakeList.txt            # compile file
│   └── Dockerfile               # Dockerfile
├── tests/
│   ├── test_proxy.cpp           # Comprehensive test suite
│   ├── CacheServer.py           # simple server for cache test
│   ├── run_tests.sh             # script to run tests
│   └── CMakeLists.txt           # Test build configuration
├── docker-compose.yml           # Docker Compose configuration
└── README.md                    # Project documentation
```

## Testing
The project includes a comprehensive test suite that verifies various aspects of the proxy:

> if you want to check details of behavior
> 
> you can check `./logs/proxy.log` and `./logs/DEBUG.log`
### basic http testing
```bash
# Build and run tests
cd docker-deploy/tests
./run_tests.sh
```
### cache behavior testing
```bash
cd docker-deploy

# before up, you may run this:
sudo docker-compose down

# start
sudo docker-compose up tester --build
```

### Test Coverage
- Basic HTTP methods (GET, POST)
- HTTPS tunneling (CONNECT)
- Caching and validation
- Chunked transfer encoding
- Error handling
- Concurrent requests

## Concurrency Testing
```bash
cd docker-deploy

# before up, you may run this:
sudo docker-compose down

# start
sudo docker-compose up swarm --build

# 1 access web ui

# now locust test web ui should be on http://0.0.0.0:8089 on this machine.
# if you are on VM, you may need to forward post 8089 to your local machine.

# 2 press start to use our default config(visiting a simple server on this machine)

# or configurate it by yourself
# number of users (peak concurrency)
# ramp up (users started per second)
# host(target url)
```

## Cache Control Coverage

> defaultly cache reponse for 1 hour

### Request
- no-store
- no-cache
- only-if-cached
- max-age
- min-fresh
- max-stale

### Response
- no-store
- no-cache
- must-revalidate
- max-age
- private

## Design Decisions
- **Thread-per-connection Model**: We chose this model for simplicity and isolation between connections.
- **Segmentation-for-caches**: we segment caches into 8 pieces, every segment has their own mutex lock. So we can use different caches at same time. It is at most 8 times more efficient.
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
