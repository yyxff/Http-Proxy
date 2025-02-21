#include "proxy.hpp"
#include <unistd.h>
#include <iostream>

Proxy::Proxy() : server_fd(-1) {}

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

void Proxy::setup_server() {
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
}

void Proxy::start_accepting() {
    while(true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        
        if (client_fd < 0) {
            std::cerr << "Failed to accept connection" << std::endl;
            continue;
        }


        threads.emplace_back(&Proxy::client_thread, this, client_fd);
    }
}

void Proxy::client_thread(Proxy* proxy, int client_fd) {
    proxy->handle_client(client_fd);
}

void Proxy::handle_client(int client_fd) {
    std::vector<char> buffer(4096);
    ssize_t bytes_received = recv(client_fd, buffer.data(), buffer.size(), 0);
    
    if (bytes_received <= 0) {
        close(client_fd);
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex);
    
    Request request;
    if (!request.parse(buffer)) {
        std::string response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(client_fd, response.c_str(), response.length(), 0);
        close(client_fd);
        return;
    }
    
    // TODO: Handle different request types (GET, POST, CONNECT)
    std::string response = "HTTP/1.1 200 OK\r\n\r\nHello, World!";
    send(client_fd, response.c_str(), response.length(), 0);
    close(client_fd);
}
