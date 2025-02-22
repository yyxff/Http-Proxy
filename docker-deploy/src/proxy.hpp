#ifndef PROXY_HPP
#define PROXY_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <stdexcept>
#include "request.hpp"
#include <vector>
#include <thread>
#include <mutex>
#include "logger.hpp"

class Proxy {
private:
    int server_fd;
    int port;
    std::vector<std::thread> threads;
    std::mutex mutex;
    Logger & logger;
    bool running;

public:
    // Proxy(int port = 12345);
    Proxy(int port = 0);
    ~Proxy();
    void run();
    void stop();
    int getPort() const { return port; }

private:
    void setup_server();
    void start_accepting();
    void handle_client(int client_fd);
    static void client_thread(Proxy* proxy, int client_fd);
    void handle_get(int client_fd, const Request& request);
    void handle_post(int client_fd, const Request& request);
    std::string extract_host(const std::string& url);
    std::string build_get_request(const Request& request);
    std::string build_post_request(const Request& request);
    void handle_connect(int client_fd, const Request& request);
    int connect_to_server(const std::string& host, int port);
    std::pair<std::string, int> parse_host_and_port(const std::string& host_str);
};

#endif
