#include "Request.hpp"

struct Conn {
    int client_fd;
    int server_fd;
    Request request;
};