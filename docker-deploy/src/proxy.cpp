#include "proxy.hpp"
#include <unistd.h>
#include <iostream>
#include <netdb.h>
#include <cstring>
#include <sys/select.h>
#include <arpa/inet.h>

Proxy::Proxy(int port) : 
    server_fd(-1), 
    port(port), 
    logger(Logger::getInstance()),
    running(false) {}

Proxy::~Proxy() {
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    if (server_fd >= 0) {
        close(server_fd);
    }
}

void Proxy::run() {
    setup_server();
    start_accepting();
}

// Setup server socket with proper configurations
void Proxy::setup_server() {
    logger.info("setting up server...");
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        throw std::runtime_error("Failed to set socket options");
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);  // if port=0, the system will assign a available port

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        throw std::runtime_error("Failed to bind to port " + std::to_string(port));
    }

    // get the actual port
    if (port == 0) {
        socklen_t addrlen = sizeof(address);
        if (getsockname(server_fd, (struct sockaddr *)&address, &addrlen) == -1) {
            throw std::runtime_error("Failed to get socket port");
        }
        port = ntohs(address.sin_port);
    }

    if (listen(server_fd, 10) < 0) {
        throw std::runtime_error("Failed to listen");
    }
    running = true;
    logger.info("successfully set up server on port " + std::to_string(port));
}

// Accept incoming client connections and create threads to handle them
void Proxy::start_accepting() {
    while(running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        
        if (!running) break;  // check if should stop
        
        if (client_fd < 0) {
            if (errno != EINTR) {  // ignore interrupted system calls
                logger.warning("Failed to accept connection: " + std::string(strerror(errno)));
            }
            continue;
        }

        threads.emplace_back(&Proxy::client_thread, this, client_fd);
    }
} 

void Proxy::client_thread(Proxy* proxy, int client_fd) {
    proxy->handle_client(client_fd);
}

// Main request handler - parses request and routes to appropriate handler
void Proxy::handle_client(int client_fd) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    getpeername(client_fd, (struct sockaddr *)&addr, &addr_len);
    std::string client_ip = inet_ntoa(addr.sin_addr);
    
    std::vector<char> buffer(8192);
    ssize_t bytes_received = recv(client_fd, buffer.data(), buffer.size(), 0);
    
    if (bytes_received <= 0) {
        close(client_fd);
        return;
    }
    
    Request request;
    if (!request.parse(buffer)) {
        std::string response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(client_fd, response.c_str(), response.length(), 0);
        logger.log(request.getId(), "Responding \"HTTP/1.1 400 Bad Request\"");
        close(client_fd);
        return;
    }
    
    // Log the request with client IP and time
    logger.log(request.getId(), "\"" + request.getMethod() + " " + request.getUrl() + 
               " " + request.getVersion() + "\" from " + client_ip + " @ " + 
               logger.getCurrentTime());
    
    if (request.getMethod() == "GET") {
        handle_get(client_fd, request);
    }
    else if (request.getMethod() == "POST") {
        handle_post(client_fd, request);
    }
    else if (request.getMethod() == "CONNECT") {
        handle_connect(client_fd, request);
    }
    else {
        std::string response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(client_fd, response.c_str(), response.length(), 0);
        logger.log(request.getId(), "Responding \"HTTP/1.1 400 Bad Request\"");
    }
    
    close(client_fd);
}

// from URL
std::string Proxy::extract_host(const std::string& url) {
    std::string host = url;
    if (host.substr(0, 7) == "http://") {
        host = host.substr(7);
    }
    size_t pos = host.find('/');
    if (pos != std::string::npos) {
        host = host.substr(0, pos);
    }
    return host;
}

std::string Proxy::build_get_request(const Request& request) {
    std::string url = request.getUrl();
    size_t pos = url.find("://");
    std::string path;
    
    if (pos != std::string::npos) {
        size_t path_pos = url.find('/', pos + 3);
        if (path_pos != std::string::npos) {
            path = url.substr(path_pos);
        } else {
            path = "/";
        }
    } else {
        path = url;
    }
    
    std::string req = "GET " + path + " " + request.getVersion() + "\r\n";
    req += "Host: " + extract_host(url) + "\r\n";
    req += "Connection: close\r\n";
    req += "Accept: */*\r\n";
    req += "User-Agent: Mozilla/5.0\r\n";
    req += "\r\n";
    
    return req;
}

// TODO: GET need to cache
void Proxy::handle_get(int client_fd, const Request& request) {
    std::string host_with_port = extract_host(request.getUrl());
    auto [host, server_port] = parse_host_and_port(host_with_port);

    std::string request_str = build_get_request(request);
    logger.log(request.getId(), "Requesting \"" + request.getMethod() + " " + 
               request.getUrl() + " " + request.getVersion() + "\" from " + host_with_port);

    try {
        int server_fd = connect_to_server(host, server_port);

        // Set receive timeout to 5 seconds
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
            throw std::runtime_error("Failed to set socket timeout");
        }

        if (send(server_fd, request_str.c_str(), request_str.length(), 0) < 0) {
            throw std::runtime_error("Failed to send request to server");
        }

        std::vector<char> buffer(8192);
        std::string full_response;
        ssize_t bytes_received;

        while ((bytes_received = recv(server_fd, buffer.data(), buffer.size(), 0)) > 0) {
            full_response.append(buffer.data(), bytes_received);
            // If we find the end of headers and content-length, we can stop reading
            if (full_response.find("\r\n\r\n") != std::string::npos) {
                break;
            }
        }

        if (bytes_received < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
            throw std::runtime_error("Failed to receive response from server: " + std::string(strerror(errno)));
        }

        if (!full_response.empty()) {
            size_t pos = full_response.find("\r\n");
            std::string response_line = full_response.substr(0, pos);
            logger.log(request.getId(), "Received \"" + response_line + "\" from " + host);
            logger.log(request.getId(), "Responding \"" + response_line + "\"");

            if (send(client_fd, full_response.c_str(), full_response.length(), 0) < 0) {
                throw std::runtime_error("Failed to send response to client");
            }
        } else {
            throw std::runtime_error("Empty response from server");
        }

        close(server_fd);
    }
    catch (const std::exception& e) {
        logger.log(request.getId(), "ERROR in handle_get: " + std::string(e.what()));
        std::string error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_fd, error_response.c_str(), error_response.length(), 0);
        logger.log(request.getId(), "Responding \"HTTP/1.1 502 Bad Gateway\"");
    }
}

std::string Proxy::build_post_request(const Request& request) {
    std::string url = request.getUrl();
    size_t pos = url.find("://");
    std::string path;
    
    if (pos != std::string::npos) {
        size_t path_pos = url.find('/', pos + 3);
        if (path_pos != std::string::npos) {
            path = url.substr(path_pos);
        } else {
            path = "/";
        }
    } else {
        path = url;
    }
    
    std::string req = "POST " + path + " " + request.getVersion() + "\r\n";
    req += "Host: " + extract_host(url) + "\r\n";
    
    // copy all original headers, but skip Host header
    for (const auto& header : request.getHeaders()) {
        if (header.first != "Host") {  // skip Host header
            req += header.first + ": " + header.second + "\r\n";
        }
    }
    req += "\r\n";
    
    // add request body if exists
    if (request.hasBody()) {
        req += request.getBody();
    }
    
    return req;
}

// POST: no need to cache
void Proxy::handle_post(int client_fd, const Request& request) {
    std::string host = extract_host(request.getUrl());
    auto [host_name, server_port] = parse_host_and_port(host);
    
    logger.log(request.getId(), "Requesting \"POST " + request.getUrl() + "\" from " + host);
    
    try {
        int server_fd = connect_to_server(host_name, server_port);
        
        // Set receive timeout to 5 seconds
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
            throw std::runtime_error("Failed to set socket timeout");
        }
        
        std::string post_request = build_post_request(request);
        logger.log(request.getId(), "DEBUG: Sending request:\n" + post_request);
        if (send(server_fd, post_request.c_str(), post_request.length(), 0) < 0) {
            throw std::runtime_error("Failed to send request to server");
        }
        
        std::vector<char> buffer(8192);
        std::string full_response;
        ssize_t bytes_received;
        
        while ((bytes_received = recv(server_fd, buffer.data(), buffer.size(), 0)) > 0) {
            full_response.append(buffer.data(), bytes_received);
        }
        
        if (bytes_received < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
            throw std::runtime_error("Failed to receive response from server: " + std::string(strerror(errno)));
        }
        
        if (!full_response.empty()) {
            size_t pos = full_response.find("\r\n");
            std::string response_line = full_response.substr(0, pos);
            logger.log(request.getId(), "Received \"" + response_line + "\" from " + host);
            logger.log(request.getId(), "Responding \"" + response_line + "\"");
            
            if (send(client_fd, full_response.c_str(), full_response.length(), 0) < 0) {
                throw std::runtime_error("Failed to send response to client");
            }
        } else {
            throw std::runtime_error("Empty response from server");
        }
        
        close(server_fd);
    }
    catch (const std::exception& e) {
        logger.log(request.getId(), "ERROR in handle_post: " + std::string(e.what()));
        std::string error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_fd, error_response.c_str(), error_response.length(), 0);
        logger.log(request.getId(), "Responding \"HTTP/1.1 502 Bad Gateway\"");
    }
}

int Proxy::connect_to_server(const std::string& host, int port) {
    struct hostent *server = gethostbyname(host.c_str());
    if (server == NULL) {
        throw std::runtime_error("Failed to resolve hostname");
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(server_fd);
        throw std::runtime_error("Failed to connect to server");
    }

    return server_fd;
}

void Proxy::handle_connect(int client_fd, const Request& request) {
    // Extract host and port from CONNECT request
    std::string url = request.getUrl();
    size_t colon_pos = url.find(':');
    if (colon_pos == std::string::npos) {
        throw std::runtime_error("Invalid CONNECT request");
    }

    std::string host = url.substr(0, colon_pos);
    int port = std::stoi(url.substr(colon_pos + 1));

    // Log the CONNECT request
    logger.log(request.getId(), "Requesting \"CONNECT " + url + " " + request.getVersion() + "\" from " + host);

    // Connect to the destination server
    int server_fd = connect_to_server(host, port);

    // Send 200 OK to client
    std::string response = "HTTP/1.1 200 Connection established\r\n\r\n";
    send(client_fd, response.c_str(), response.length(), 0);
    
    // Log the response
    logger.log(request.getId(), "Responding \"HTTP/1.1 200 Connection established\"");

    // Set up select() for both sockets
    fd_set read_fds;
    int max_fd = std::max(client_fd, server_fd) + 1;
    char buffer[8192];

    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(client_fd, &read_fds);
        FD_SET(server_fd, &read_fds);

        if (select(max_fd, &read_fds, NULL, NULL, NULL) < 0) {
            logger.log(request.getId(), "ERROR Select failed in tunnel: " + std::string(strerror(errno)));
            break;
        }

        if (FD_ISSET(client_fd, &read_fds)) {
            ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0) break;
            if (send(server_fd, buffer, bytes_read, 0) <= 0) break;
        }

        if (FD_ISSET(server_fd, &read_fds)) {
            ssize_t bytes_read = recv(server_fd, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0) break;
            if (send(client_fd, buffer, bytes_read, 0) <= 0) break;
        }
    }

    // Log tunnel closure
    logger.log(request.getId(), "Tunnel closed");

    close(server_fd);
    close(client_fd);
}

void Proxy::stop() {
    running = false;
    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
    }
    // wait for all client threads to finish
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads.clear();
}

std::pair<std::string, int> Proxy::parse_host_and_port(const std::string& host_str) {
    size_t colon_pos = host_str.find(':');
    if (colon_pos != std::string::npos) {
        std::string host = host_str.substr(0, colon_pos);
        int port = std::stoi(host_str.substr(colon_pos + 1));
        return {host, port};
    }
    return {host_str, 80};  // default to port 80
}
