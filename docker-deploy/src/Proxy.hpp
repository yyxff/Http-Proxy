#ifndef PROXY_HPP
#define PROXY_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <stdexcept>
#include "Request.hpp"
#include <vector>
#include <thread>
#include <mutex>
#include "Logger.hpp"
#include "Parser.hpp"
#include "CacheDecision.hpp"
#include "CacheHandler.hpp"
#include "CacheMaster.hpp"
#include "ThreadPool.cpp"
#include "Conn.hpp"
#include <condition_variable>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <cerrno>

class Proxy {
public:
    // Default constructor - use system assigned port
    Proxy() : Proxy(0) {}
    
    // Constructor with specified port
    explicit Proxy(int port);
    
    // Destructor
    ~Proxy();
    
    // Start the proxy server
    void run();
    
    // Stop the proxy server
    void stop();
    
    // Get the current port
    int getPort() const { return port; }

private:
    // Setup server socket
    void setup_server();

    int init_epoll(int listen_fd);

    void wait_on_epoll(int epfd);
    
    // Accept client connections
    void start_accepting();
    
    // Static client thread function
    static void client_thread(Proxy* proxy, int client_fd);
    
    // Handle client request
    void handle_client(int client_fd);

    // Static server thread function
    static void server_thread(Proxy* proxy, int server_fd);

    // Handle server request
    void handle_server(int server_fd);
    
    // Handle GET request
    void handle_get(int client_fd, const Request& request);
    
    // Handle POST request
    void handle_post(int client_fd, const Request& request);
    
    // Handle CONNECT request
    void handle_connect(int client_fd, const Request& request);
    
    // Build GET request
    std::string build_get_request(const Request& request);
    
    // Build POST request
    std::string build_post_request(const Request& request);
    
    // Connect to target server
    int connect_to_server(const std::string& host, int port);
    
    // Extract host from URL
    std::string extract_host(const std::string& url);
    
    // Parse host and port
    std::pair<std::string, int> parse_host_and_port(const std::string& host_str);

    // receive full response by recv and beast parser
    std::string receive(int server_fd, int id);

    // send full data to fd
    void send_all(int client_fd, std::string full_response, int id);

    void handle_cache(int client_fd, const Request& request);

    void revalid(int client_fd, const Request& request);

    void returnCache(int client_fd, const Request& request);

    std::string build_revalid_request(const Request& request, string & eTag);

    void handle_revalid(int client_fd, const Request& request, string & eTag);

    void close_fd(int fd);

    Conn * get_conn(int fd);

    void disable_fd(int fd);

    void enable_fd(int fd);
    
    void register_to_epoll(int conn_fd);

    ssize_t forward(int from_fd, int to_fd);

    int listen_fd;
    int port;
    Logger& logger;
    bool running;
    // std::vector<std::thread> threads;
    std::mutex thread_mutex;
    std::condition_variable shutdown_cv;
    bool shutdown_requested;

    ThreadPool threadpool;
    int epfd = -1;
    // fd to its conn
    std::unordered_map<int, Conn *> fd_to_conn;
    // mutex to access the fd map
    std::mutex fd_map_mtx;
};

#endif
