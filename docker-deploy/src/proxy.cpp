#include "proxy.hpp"
#include <unistd.h>
#include <iostream>
#include <netdb.h>
#include <cstring>
#include <sys/select.h>

Proxy::Proxy() : server_fd(-1),logger(Logger::getInstance()) {}

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
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        throw std::runtime_error("Failed to set socket options");
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        throw std::runtime_error("Failed to bind to port");
    }

    if (listen(server_fd, 10) < 0) {
        throw std::runtime_error("Failed to listen");
    }
    logger.info("successfully set up server!");
}

// Accept incoming client connections and create threads to handle them
void Proxy::start_accepting() {
    while(true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        
        if (client_fd < 0) {
            std::cerr << "Failed to accept connection" << std::endl;
            continue;
        }

        // Create a new thread for each client connection
        threads.emplace_back(&Proxy::client_thread, this, client_fd);
    }
} 

void Proxy::client_thread(Proxy* proxy, int client_fd) {
    proxy->handle_client(client_fd);
}

// Main request handler - parses request and routes to appropriate handler
void Proxy::handle_client(int client_fd) {
    std::vector<char> buffer(4096);
    ssize_t bytes_received = recv(client_fd, buffer.data(), buffer.size(), 0);
    
    if (bytes_received <= 0) {
        close(client_fd);
        return;
    }
    
    Request request;
    if (!request.parse(buffer)) {
        std::string response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(client_fd, response.c_str(), response.length(), 0);
        close(client_fd);
        return;
    }
    
    try {
        // Route request to appropriate handler based on HTTP method
        if (request.isConnect()) {
            // handle_connect(client_fd, request);  // tunneling
        }
        else if (request.isGet()) {
            handle_get(client_fd, request);      // GET
        }
        else if (request.isPost()) {
            handle_post(client_fd, request);     // POST
        }
        else {
            std::string response = "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
            send(client_fd, response.c_str(), response.length(), 0);
            close(client_fd);
        }
    }
    catch (const std::exception& e) {
        std::string response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_fd, response.c_str(), response.length(), 0);
        close(client_fd);
    }
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
    std::string req;
    std::string url = request.getUrl();
    size_t pos = url.find('/');
    std::string path = pos != std::string::npos ? url.substr(pos) : "/";
    
    req = "GET " + path + " " + request.getVersion() + "\r\n";
    req += "Host: " + extract_host(url) + "\r\n";
    req += "Connection: close\r\n\r\n";
    
    return req;
}

// TODO: GET need to cache
void Proxy::handle_get(int client_fd, const Request& request) {
    std::string host = extract_host(request.getUrl());
    // int server_fd = connect_to_server(host, 80);
    
    // build and send GET request to the original server
    std::string get_request = build_get_request(request);
    send(server_fd, get_request.c_str(), get_request.length(), 0);
    
    // receive the response from the original server
    std::vector<char> response_buffer(4096);
    std::string full_response;
    ssize_t bytes_received;
    
    while ((bytes_received = recv(server_fd, response_buffer.data(), response_buffer.size(), 0)) > 0) {
        full_response.append(response_buffer.data(), bytes_received);
    }
    
    // send the response to the client
    if (!full_response.empty()) {
        send(client_fd, full_response.c_str(), full_response.length(), 0);
    } else {
        std::string error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_fd, error_response.c_str(), error_response.length(), 0);
    }
    
    close(server_fd);
    close(client_fd);
}

std::string Proxy::build_post_request(const Request& request) {
    std::string req;
    std::string url = request.getUrl();
    size_t pos = url.find('/');
    std::string path = pos != std::string::npos ? url.substr(pos) : "/";
    
    req = "POST " + path + " " + request.getVersion() + "\r\n";
    req += "Host: " + extract_host(url) + "\r\n";
    
    // Copy all original headers
    // for (const auto& header : request.getHeaders()) {
    //     req += header.first + ": " + header.second + "\r\n";
    // }
    req += "\r\n";
    
    // Add body if exists
    // if (request.hasBody()) {
    //     req += request.getBody();
    // }
    
    return req;
}

// POST: no need to cache
void Proxy::handle_post(int client_fd, const Request& request) {
    std::string host = extract_host(request.getUrl());
    // int server_fd = connect_to_server(host, 80);
    
    // Build and send POST request to the original server
    std::string post_request = build_post_request(request);
    send(server_fd, post_request.c_str(), post_request.length(), 0);
    
    // Receive the response from the original server
    std::vector<char> response_buffer(4096);
    std::string full_response;
    ssize_t bytes_received;
    
    while ((bytes_received = recv(server_fd, response_buffer.data(), response_buffer.size(), 0)) > 0) {
        full_response.append(response_buffer.data(), bytes_received);
    }
    
    // Send the response to the client
    if (!full_response.empty()) {
        send(client_fd, full_response.c_str(), full_response.length(), 0);
    } else {
        std::string error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_fd, error_response.c_str(), error_response.length(), 0);
    }
    
    close(server_fd);
    close(client_fd);
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

    // Connect to the destination server
    int server_fd = connect_to_server(host, port);

    // Send 200 OK to client
    std::string response = "HTTP/1.1 200 Connection established\r\n\r\n";
    send(client_fd, response.c_str(), response.length(), 0);

    // Set up select() for both sockets
    fd_set read_fds;
    int max_fd = std::max(client_fd, server_fd) + 1;
    char buffer[4096];

    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(client_fd, &read_fds);
        FD_SET(server_fd, &read_fds);

        if (select(max_fd, &read_fds, NULL, NULL, NULL) < 0) {
            break;
        }

        if (FD_ISSET(client_fd, &read_fds)) {
            ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0) break;
            send(server_fd, buffer, bytes_read, 0);
        }

        if (FD_ISSET(server_fd, &read_fds)) {
            ssize_t bytes_read = recv(server_fd, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0) break;
            send(client_fd, buffer, bytes_read, 0);
        }
    }

    close(server_fd);
    close(client_fd);
}
