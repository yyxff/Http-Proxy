#ifndef PROXY_HPP
#define PROXY_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <stdexcept>

class Proxy {
private:
    int server_fd;
    const int PORT = 12345;

public:
    Proxy();
    void run();

private:
    void setup_server();
    void start_accepting();
    void handle_client(int client_fd);
};

#endif
