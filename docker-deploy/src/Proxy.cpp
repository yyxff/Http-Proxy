#include "Proxy.hpp"
#include <unistd.h>
#include <iostream>
#include <netdb.h>
#include <cstring>
#include <sys/select.h>
#include <arpa/inet.h>
#include <algorithm>
#include <ctime>
#include <thread>
#include <chrono>
#include "Cache.hpp"

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace asio = boost::asio;

Proxy::Proxy(int port) : 
    listen_fd(-1), 
    port(port), 
    logger(Logger::getInstance()),
    running(false),
    shutdown_requested(false) {}

Proxy::~Proxy() {
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    if (listen_fd >= 0) {
        close(listen_fd);
        logger.debug("closed server fd: "+to_string(listen_fd));
    }
}

void Proxy::run() {
    setup_server();
    start_accepting();
}

// Setup server socket with proper configurations
void Proxy::setup_server() {
    logger.info("setting up server...");
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        throw std::runtime_error("Failed to set socket options");
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);  // if port=0, the system will assign a available port

    if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        throw std::runtime_error("Failed to bind to port " + std::to_string(port));
    }

    // get the actual port
    if (port == 0) {
        socklen_t addrlen = sizeof(address);
        if (getsockname(listen_fd, (struct sockaddr *)&address, &addrlen) == -1) {
            throw std::runtime_error("Failed to get socket port");
        }
        port = ntohs(address.sin_port);
    }

    if (listen(listen_fd, 10) < 0) {
        throw std::runtime_error("Failed to listen");
    }
    running = true;
    logger.info("successfully set up server on port " + std::to_string(port));
}

void Proxy::start_accepting() {
    while(running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Set accept timeout to allow checking running flag
        struct timeval tv;
        tv.tv_sec = 1;  // 1 second timeout
        tv.tv_usec = 0;
        setsockopt(listen_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        logger.debug("build a new client fd "+to_string(client_fd));
        
        if (!running) break;
        
        if (client_fd < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) {
                continue;  // Timeout or interrupted, check running flag
            }
            logger.warning("Failed to accept connection: " + std::string(strerror(errno)));
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(thread_mutex);
            threads.emplace_back(&Proxy::client_thread, this, client_fd);
        }
    }
    
    logger.info("Accepting loop terminated");
}

void Proxy::client_thread(Proxy* proxy, int client_fd) {
    proxy->handle_client(client_fd);
    // logger.debug("handle client fd "+to_string(client_fd));
}

// Main request handler - parses request and routes to appropriate handler
void Proxy::handle_client(int client_fd) {
    logger.debug("handle client fd "+to_string(client_fd));
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    getpeername(client_fd, (struct sockaddr *)&addr, &addr_len);
    std::string client_ip = inet_ntoa(addr.sin_addr);
    
    std::vector<char> buffer(8192);
    ssize_t bytes_received = recv(client_fd, buffer.data(), buffer.size(), 0);
    logger.debug("received "+to_string(bytes_received)+" bytes from client "+to_string(client_fd));
    if (bytes_received <= 0) {
        if (bytes_received < 0) {
            logger.error("Failed to receive request: " + std::string(strerror(errno)));
        }
        close(client_fd);
        logger.debug("closed client fd: "+to_string(client_fd));
        return;
    }
    
    Request request;
    // parse buffer to request
    try{
        Parser parser;
        request = parser.parseRequest(buffer);
        logger.debug(request.getId()," on client fd "+to_string(client_fd)+" parse success "+request.getMethod());
    }catch(std::runtime_error & e){
        std::string response = "HTTP/1.1 400 Bad Request\r\n"
                              "Content-Length: 15\r\n\r\n"
                              "400 Bad Request";
        send(client_fd, response.c_str(), response.length(), 0);
        logger.warning(request.getId(), "Responding \"HTTP/1.1 400 Bad Request\"");
        close(client_fd);
        logger.debug(request.getId(),"handle client1 closed client fd: "+to_string(client_fd));
        return;
    }
    
    // Log the request with client IP and time
    logger.info(request.getId(), "\"" + request.getMethod() + " " + request.getUrl() + 
               " " + request.getVersion() + "\" from " + client_ip + " @ " + 
               logger.getCurrentTime());
    if (request.getMethod() == "GET") {
        handle_cache(client_fd, request);
        // handle_get(client_fd, request);
    }
    else if (request.getMethod() == "POST") {
        handle_post(client_fd, request);
    }
    else if (request.getMethod() == "CONNECT") {
        logger.debug(request.getId(), "handle connect start");
        handle_connect(client_fd, request);
        logger.debug(request.getId(), "handle connect done");
    }
    else {
        std::string response = "HTTP/1.1 405 Method Not Allowed\r\n"
                              "Allow: GET, POST, CONNECT\r\n"
                              "Content-Length: 21\r\n\r\n"
                              "405 Method Not Allowed";
        send(client_fd, response.c_str(), response.length(), 0);
        logger.warning(request.getId(), "Responding \"HTTP/1.1 405 Method Not Allowed\" for method \"" + 
                  request.getMethod() + "\"");
    }
    logger.debug(request.getId(),"ready to close fd "+to_string(client_fd));
    close(client_fd);
    logger.debug(request.getId(),"handle client2 closed client fd: "+to_string(client_fd));
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

    std::string request_get = build_get_request(request);
    logger.info(request.getId(), "Requesting \"" + request.getMethod() + " " + 
               request.getUrl() + " " + request.getVersion() + "\" from " + host_with_port);
    int server_fd = -1;
    try {
        server_fd = connect_to_server(host, server_port);
        
        // Set receive timeout to 10 seconds (same as test)
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
            throw std::runtime_error("Failed to set socket timeout");
        }

        // Send the request in chunks to handle large requests
        send_all(server_fd, request_get, request.getId());

        // receive full response from server
        std::string full_response = receive(server_fd, request.getId());

        Parser parser;
        Response response = parser.parseResponse(vector<char>(full_response.begin(),full_response.end()));
        
        // if ok, send response to client
        if (!full_response.empty()) {
            logger.info(request.getId(), "From " + host);
            send_all(client_fd, full_response, request.getId());
        } else {
            throw std::runtime_error("Empty response from server");
        }


        // cache it if ok
        if (Cache::isCacheable(response)){
            logger.debug(request.getId(), "response cacheable");
            Cache & cache = Cache::getInstance();
            CacheEntry cacheEntry("200", response.getHeadersStr(), response.getBody());
            logger.debug(request.getId(),"try to cache: "+request.getUrl());
            cache.addToCache(request.getUrl(), "200", response.getHeadersStr(), response.getBody());
        }else{
            logger.debug(request.getId(), "not cacheable");
        }
    }
    catch (const std::exception& e) {
        logger.error(request.getId(), "ERROR in handle_get: " + std::string(e.what()));
        std::string error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_fd, error_response.c_str(), error_response.length(), 0);
        logger.error(request.getId(), "Responding \"HTTP/1.1 502 Bad Gateway\"");
    }
    // close it
    close(server_fd);
    logger.debug(request.getId(),"closed server fd: "+to_string(server_fd));
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
    req += "Connection: close\r\n";
    
    bool has_content_length = false;
    bool has_content_type = false;
    
    // copy all original headers, but skip Host and Connection
    for (const auto& header : request.getHeaders()) {
        logger.debug(header.name_string().to_string()+to_string(header.name_string().to_string() != "Host"));
        if (header.name_string().to_string() != "Host" && header.name_string().to_string() != "Connection") {
            logger.debug(header.name_string().to_string());
            req += header.name_string().to_string() + ": " + header.value().to_string() + "\r\n";
            if (header.name_string().to_string() == "Content-Length") {
                has_content_length = true;
            }
            if (header.name_string().to_string() == "Content-Type") {
                has_content_type = true;
            }
        }
    }
    
    // Add Content-Type if missing and we have a body
    if (request.hasBody() && !has_content_type) {
        req += "Content-Type: application/x-www-form-urlencoded\r\n";
    }
    
    // Add Content-Length if missing and we have a body
    if (request.hasBody() && !has_content_length) {
        req += "Content-Length: " + std::to_string(request.getBody().length()) + "\r\n";
    }
    
    req += "\r\n";
    logger.debug("body"+request.getBody());
    if (request.hasBody()) {
        req += request.getBody();
    }
    logger.debug("get req:"+req);
    return req;
}

// POST: no need to cache
void Proxy::handle_post(int client_fd, const Request& request) {
    std::string host = extract_host(request.getUrl());
    auto [host_name, server_port] = parse_host_and_port(host);
    
    logger.info(request.getId(), "Requesting \"POST " + request.getUrl() + "\" from " + host);

    // build post request
    std::string request_post = build_post_request(request);
    logger.debug(request.getId(), "Sending request:\n" + request_post);
    int server_fd = -1;
    try {
        server_fd = connect_to_server(host_name, server_port);
        logger.info(request.getId(), "Successfully connected to server " + host_name + ":" + std::to_string(server_port));
        
        // Set receive timeout to 10 seconds (same as test)
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
            throw std::runtime_error("Failed to set socket timeout");
        }
        
        // Send the request in chunks to handle large requests
        send_all(server_fd, request_post, request.getId());
        
        // receive response from server
        std::string full_response = receive(server_fd, request.getId());
        
        // if ok, send response to client
        if (!full_response.empty()) {
            logger.info(request.getId(), "From " + host);
            send_all(client_fd, full_response, request.getId());
        } else {
            throw std::runtime_error("Empty response from server");
        }
        
    }
    catch (const std::exception& e) {
        logger.error(request.getId(), "ERROR in handle_post: " + std::string(e.what()));
        std::string error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_fd, error_response.c_str(), error_response.length(), 0);
        logger.error(request.getId(), "Responding \"HTTP/1.1 502 Bad Gateway\"");
    }
    // close it
    close(server_fd);
    logger.debug(request.getId(),"closed server fd: "+to_string(server_fd));
}

int Proxy::connect_to_server(const std::string& host, int port) {
    logger.debug("Attempting to connect to " + host + ":" + std::to_string(port));
    
    struct hostent *server = gethostbyname(host.c_str());
    if (server == NULL) {
        logger.error("Failed to resolve hostname " + host + ": " + std::string(hstrerror(h_errno)));
        throw std::runtime_error("Failed to resolve hostname");
    }
    logger.debug("Successfully resolved hostname " + host);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        logger.error("Failed to create socket: " + std::string(strerror(errno)));
        throw std::runtime_error("Failed to create socket");
    }
    logger.debug("Successfully created socket");

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    logger.debug("memcpy done");
    // Set connect timeout
    struct timeval tv;
    tv.tv_sec = 5;  // 5 seconds timeout
    tv.tv_usec = 0;
    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
        logger.error("Failed to set receive timeout: " + std::string(strerror(errno)));
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
        logger.error("Failed to set send timeout: " + std::string(strerror(errno)));
    }
    logger.debug("socket set done");

    // Convert IP to string for logging
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(server_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
    logger.debug("Attempting to connect to IP: " + std::string(ip_str) + ":" + std::to_string(port));

    if (connect(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(server_fd);
        logger.debug("closed server fd: "+to_string(server_fd));
        logger.error("Failed to connect to server: " + std::string(strerror(errno)));
        throw std::runtime_error("Failed to connect to server");
    }
    logger.debug("Successfully connected to server");

    return server_fd;
}

void Proxy::handle_connect(int client_fd, const Request& request) {
    // Extract host and port from CONNECT request
    std::string url = request.getUrl();
    auto [host, port] = parse_host_and_port(url);

    // Log the CONNECT request
    logger.info(request.getId(), "Requesting \"CONNECT " + url + " " + request.getVersion() + "\" from " + host);
    int server_fd = -1;
    try {
        // Connect to the destination server
        server_fd = connect_to_server(host, port);
        logger.info(request.getId(), "Successfully connected to destination server " + host + ":" + std::to_string(port)+" on fd "+to_string(server_fd));

        // Send 200 OK to client
        std::string response = "HTTP/1.1 200 Connection established\r\n\r\n";
        ssize_t sent = send(client_fd, response.c_str(), response.length(), 0);
        logger.debug(request.getId(), "send response "+to_string(sent)+"/"+to_string(response.size()));
        if (sent < 0) {
            throw std::runtime_error("Failed to send connection established response: " + std::string(strerror(errno)));
        }
        if (static_cast<size_t>(sent) != response.length()) {
            throw std::runtime_error("Failed to send complete response");
        }
        
        // Log the response
        logger.info(request.getId(), "Responding \"HTTP/1.1 200 Connection established\"");
        logger.info(request.getId(), "Starting CONNECT tunnel");

        // handle tunnel
        try {
            // Set up select() for both sockets
            fd_set read_fds;
            int max_fd = std::max(client_fd, server_fd) + 1;
            std::vector<char> buffer(8192);
            size_t total_bytes_client_to_server = 0;
            size_t total_bytes_server_to_client = 0;

            // pass data between client and server
            while (true) {
                FD_ZERO(&read_fds);
                FD_SET(client_fd, &read_fds);
                FD_SET(server_fd, &read_fds);

                // Set select timeout (5 seconds)
                struct timeval timeout;
                timeout.tv_sec = 5;
                timeout.tv_usec = 0;

                int activity = select(max_fd, &read_fds, NULL, NULL, &timeout);
                if (activity < 0) {
                    if (errno == EINTR) continue;
                    logger.debug(request.getId(),"activity == "+to_string(activity)+" break loop");
                    break;
                }
                if (activity == 0) break;  // timeout

                if (FD_ISSET(client_fd, &read_fds)) {
                    ssize_t bytes_read = recv(client_fd, buffer.data(), buffer.size(), 0);
                    if (bytes_read <= 0) {
                        logger.debug(request.getId(),"client->server break: read "+to_string(bytes_read)+" on fd: "+to_string(client_fd));
                        int er = errno;
                        logger.debug(request.getId(), "errno: "+to_string(er));
                        break;
                    }
                    size_t bytes_send = 0;
                    if ((bytes_send = send(server_fd, buffer.data(), bytes_read, 0)) <= 0) {
                        logger.debug(request.getId(),"client->server break: send "+to_string(bytes_read));
                        break;
                    }

                    total_bytes_client_to_server += bytes_read;
                    logger.debug(request.getId(),"send server "+to_string(bytes_send)+"/"+to_string(bytes_read));
                }

                if (FD_ISSET(server_fd, &read_fds)) {
                    ssize_t bytes_read = recv(server_fd, buffer.data(), buffer.size(), 0);
                    if (bytes_read <= 0) {
                        logger.debug(request.getId(),"server->client break: read "+to_string(bytes_read));
                        break;
                    }
                    size_t bytes_send = 0;
                    if ((bytes_send = send(client_fd, buffer.data(), bytes_read, 0)) <= 0) {
                        logger.debug(request.getId(),"server->client break: send "+to_string(bytes_read));
                        break;
                    }
                    total_bytes_server_to_client += bytes_read;
                    logger.debug(request.getId(),"send client "+to_string(bytes_send)+"/"+to_string(bytes_read));
                }
            }

            logger.info(request.getId(), "Tunnel closed. Total bytes: client->server=" + 
                        std::to_string(total_bytes_client_to_server) + ", server->client=" + 
                        std::to_string(total_bytes_server_to_client));
        }
        catch (const std::exception& e) {
            logger.error(request.getId(), "ERROR in tunnel thread: " + std::string(e.what()));
        }

        // close(server_fd);
        // logger.debug(request.getId(),"closed server fd: "+to_string(server_fd));
        // close(client_fd);
        // logger.debug(request.getId(),"handle connect1 closed client fd: "+to_string(client_fd));
        
    }
    catch (const std::exception& e) {
        logger.error(request.getId(), "ERROR in handle_connect: " + std::string(e.what()));
        std::string error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_fd, error_response.c_str(), error_response.length(), 0);
        logger.error(request.getId(), "Responding \"HTTP/1.1 502 Bad Gateway\"");
    }
    // close it
    close(server_fd);
    logger.debug(request.getId(),"closed server fd: "+to_string(server_fd));
}

void Proxy::stop() {
    if (!running) return;
    
    logger.info("Initiating proxy shutdown...");
    running = false;
    shutdown_requested = true;

    // Close server socket to interrupt accept
    if (listen_fd >= 0) {
        shutdown(listen_fd, SHUT_RDWR);
        close(listen_fd);
        logger.debug("closed listen fd: "+to_string(listen_fd));
        listen_fd = -1;
    }

    // Wait for all client threads with timeout
    {
        std::lock_guard<std::mutex> lock(thread_mutex);
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        threads.clear();
    }

    logger.info("Proxy shutdown complete");
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


// receive full response by recv and beast parser
std::string Proxy::receive(int server_fd, int id){
    std::string full_response;
    char buffer[8192];
    ssize_t bytes_received;

    beast::flat_buffer dynamic_buffer;
    http::response_parser<http::string_body> parser;
    boost::system::error_code ec;

    while (true) {
        // keep recv ing if not fet full response
        ssize_t bytes_received = recv(server_fd, buffer, sizeof(buffer), 0);
        full_response.append(buffer, bytes_received);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                logger.debug(id, "connection closed");
            } else {
                logger.error(id, "recv failed!");
            }
            break;
        }
        logger.debug(id, "get "+to_string(bytes_received));

        // append data to buffer
        dynamic_buffer.commit(
            boost::asio::buffer_copy(
                dynamic_buffer.prepare(bytes_received),  
                boost::asio::buffer(buffer, bytes_received)  
            )
        ); 

        // try to parse data
        while(dynamic_buffer.size() > 0){
            size_t bytes_rest = dynamic_buffer.size();
            size_t bytes_parsed = parser.put(dynamic_buffer.data(), ec);
            dynamic_buffer.consume(bytes_parsed); // clear processed data
            // logger.debug(id, "parsed "+to_string(bytes_parsed)+"/"+to_string(bytes_rest));

            if (ec) {
                if (ec == http::error::need_more) {
                    logger.debug(id, "need more");
                    break;
                } else {
                    logger.error(id, "failed to parse data, ec:"+ec.message()+" code: "+to_string(ec.value()));
                    break;
                }
            }
        }

        // get all data
        if (parser.is_done()) {
            http::response<http::string_body>  res = parser.release();
            logger.info(id, "successfully parsed response code("
                            +to_string(res.result_int())+") bodyLen("+to_string(res.body().size())+")");
            // logger.debug(full_response);
            break;
        }
    }
    return full_response;
}


// send full data to fd
void Proxy::send_all(int client_fd, std::string full_response, int id){
    size_t pos = full_response.find("\r\n");
    if (pos != std::string::npos) {
        std::string response_line = full_response.substr(0, pos);
        
        logger.info(id, "Full response length: " + 
                    std::to_string(full_response.length()) + " bytes");
        
        // Send response to client in chunks
        size_t total_sent = 0;
        while (total_sent < full_response.length()) {
            ssize_t sent = send(client_fd, full_response.c_str() + total_sent,
                                full_response.length() - total_sent, 0);
            if (sent < 0) {
                throw std::runtime_error("Failed to send response to client: " + 
                                        std::string(strerror(errno)));
            }
            total_sent += sent;
        }
        logger.info(id, "Successfully sent "+to_string(total_sent)+" bytes to client");
    } else {
        throw std::runtime_error("Invalid response format");
    }
}

void Proxy::handle_cache(int client_fd, const Request& request){
    Cache & cache = Cache::getInstance();
    
    Cache::CacheStatus status = cache.checkStatus(request.getUrl());
    CacheEntry * entry = cache.getEntry(request.getUrl());

    // judge cache decision
    CacheDecision cacheDecision;
    CacheHandler cacheHandler;
    CacheDecision::Decision decision = cacheDecision.makeDecision(request);
    // if need to send 
    if (cacheHandler.need_to_send(decision)){
        switch(decision){
            case CacheDecision::DIRECT:{
                handle_get(client_fd, request);
                break;
            }
            case CacheDecision::REVALIDATE:{
                revalid(client_fd, request);
                break;
            }
            case CacheDecision::NO_TRANSFORM:{
                handle_get(client_fd, request);
                break;
            }
        }
    }else{// if just return
        string response = cacheHandler.build_forward_response(decision, entry);
        send_all(client_fd, response, request.getId());
    }
    

}

void Proxy::revalid(int client_fd, const Request& request){
    logger.debug(request.getId(),"revalid");
    Cache & cache = Cache::getInstance();
    CacheEntry * cacheEntry = cache.getEntry(request.getUrl());
    string eTag = Cache::extractETag(cacheEntry->getResponseHeaders());
    if (eTag == ""){// no etag
        logger.debug(request.getId(),"has no etag, reget");
        handle_get(client_fd, request);
    }else{// has etag
        logger.debug(request.getId(),"has etag: "+eTag+" revalid it");
        handle_revalid(client_fd,request,eTag);
    }
}

void Proxy::returnCache(int client_fd, const Request& request){
    logger.debug(request.getId(),"return by cache");
    Cache & cache = Cache::getInstance();
    CacheEntry * cacheEntry = cache.getEntry(request.getUrl());
    send_all(client_fd, cacheEntry->getFullResponse(), request.getId());
    logger.debug(request.getId(),"done return cache");

}

// bool Peoxy::revalid_eTag(const Request& request, string & eTag){
//     Response response = handle_request();
//     if (response.getResult() == 304){

//     }else if(response.getResult() == 200)
// }

std::string Proxy::build_revalid_request(const Request& request, string & eTag) {
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
    req += "If-None-Match: "+eTag+"\r\n";
    req += "Accept: */*\r\n";
    req += "User-Agent: Mozilla/5.0\r\n";
    req += "\r\n";
    
    return req;
}


void Proxy::handle_revalid(int client_fd, const Request& request, string & eTag) {
    std::string host_with_port = extract_host(request.getUrl());
    auto [host, server_port] = parse_host_and_port(host_with_port);

    std::string request_get = build_revalid_request(request, eTag);
    logger.info(request.getId(), "Requesting \"" + request.getMethod() + " " + 
               request.getUrl() + " " + request.getVersion() + "\" from " + host_with_port);

    try {
        int server_fd = connect_to_server(host, server_port);
        
        // Set receive timeout to 10 seconds (same as test)
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
            throw std::runtime_error("Failed to set socket timeout");
        }

        // Send the request in chunks to handle large requests
        send_all(server_fd, request_get, request.getId());

        // receive full response from server
        std::string full_response = receive(server_fd, request.getId());

        Parser parser;
        Response response = parser.parseResponse(vector<char>(full_response.begin(),full_response.end()));


        // if 304, just use cache
        if (response.getResult() == 304){
            logger.debug(request.getId(),"304 not modified, just use cache");
            returnCache(client_fd, request);
            return;
        }else if(response.getResult() == 200){// if 200, send response to client
            logger.debug(request.getId(),"200  modified, use new response");
            send_all(client_fd, full_response, request.getId());
        }else {
            throw std::runtime_error("Empty response from server");
        }
        
        // close it
        close(server_fd);
        logger.debug(request.getId(),"closed server fd: "+to_string(server_fd));

        // cache it if ok
        if (Cache::isCacheable(response)){
            logger.debug(request.getId(), "response cacheable");
            Cache & cache = Cache::getInstance();
            CacheEntry cacheEntry("200", response.getHeadersStr(), response.getBody());
            logger.debug(request.getId(),"try to cache: "+request.getUrl());
            cache.addToCache(request.getUrl(), "200", response.getHeadersStr(), response.getBody());
        }else{
            logger.debug(request.getId(), "not cacheable");
        }
    }
    catch (const std::exception& e) {
        logger.error(request.getId(), "ERROR in handle_get: " + std::string(e.what()));
        std::string error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_fd, error_response.c_str(), error_response.length(), 0);
        logger.error(request.getId(), "Responding \"HTTP/1.1 502 Bad Gateway\"");
    }
}