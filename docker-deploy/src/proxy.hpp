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
    const int PORT = 12345;
    std::vector<std::thread> threads;
    std::mutex mutex;
    Logger* logger;

public:
    Proxy();
    ~Proxy();
    void run();

private:
    void setup_server();
    void start_accepting();
    void handle_client(int client_fd);
    static void client_thread(Proxy* proxy, int client_fd);
};

#endif
