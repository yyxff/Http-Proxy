#include <gtest/gtest.h>
#include "../src/Proxy.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include "../src/Logger.hpp"
#include <iostream>
#include <cstring>
#include <filesystem>
#include <future>

class ProxyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set test log path
        std::string test_log_path = "test_logs/test_proxy.log";
        Logger::getInstance().setLogPath(test_log_path);
        
        // Create proxy instance with port 0 (system assigned)
        proxy = std::make_unique<Proxy>(0);
        
        // Start proxy thread
        proxy_thread = std::thread([this]() {
            proxy->run();
        });
        // Wait for proxy thread to start and listen
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Get actual proxy port
        proxy_port = proxy->getPort();
    }

    void TearDown() override {
        proxy->stop();
        
        if (proxy_thread.joinable()) {
            // Wait for thread with timeout
            std::future<void> future = std::async(std::launch::async, [this]() {
                proxy_thread.join();
            });
            
            if (future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
                std::cerr << "Warning: Proxy thread join timed out" << std::endl;
                // Continue anyway, as we're in cleanup
            }
        }
        
        // Give resources time to clean up
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Create and connect to proxy client socket
    int create_client_socket() {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        EXPECT_GE(sock, 0) << "Failed to create client socket.";
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(proxy_port);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        EXPECT_EQ(result, 0) << "Failed to connect to proxy.";
        
        return sock;
    }

    std::unique_ptr<Proxy> proxy;
    std::thread proxy_thread;
    int proxy_port;
};

// ============== Test #1: GET ==============
TEST_F(ProxyTest, TestGet) {
    std::cout << "\n=== Starting TestGet ===" << std::endl;
    
    try {
        int client_sock = create_client_socket();
        std::string get_request = 
            "GET http://httpbin.org/get HTTP/1.1\r\n"
            "Host: httpbin.org\r\n"
            "Connection: close\r\n\r\n";
        
        ssize_t sent = send(client_sock, get_request.c_str(), get_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(get_request.size())) << "GET request not fully sent.";

        // Set receive timeout
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        // Read in a loop until there's no more data
        std::string response;
        while (true) {
            char buffer[4096];
            ssize_t received = recv(client_sock, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                break;
            }
            response.append(buffer, received);
        }

        std::cout << "GET response:\n" << response << std::endl;
        EXPECT_TRUE(response.find("200") != std::string::npos &&
                    response.find("\"url\": \"http://httpbin.org/get\"") != std::string::npos)
            << "Unexpected GET response: " << response;

        close(client_sock);
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestGet: " << e.what();
    }
    
    std::cout << "=== Completed TestGet ===" << std::endl;
}

// ============== Test #2: POST ==============
TEST_F(ProxyTest, TestPost) {
    std::cout << "\n=== Starting TestPost ===" << std::endl;
    
    try {
        int client_sock = create_client_socket();
        std::string body = "test=data";
        std::string post_request =
            "POST http://httpbin.org/post HTTP/1.1\r\n"
            "Host: httpbin.org\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "Connection: close\r\n\r\n" + body;
        
        ssize_t sent = send(client_sock, post_request.c_str(), post_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(post_request.size())) << "POST request not fully sent.";

        // Set receive timeout
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        // Read in a loop until there's no more data
        std::string response;
        while (true) {
            char buffer[4096];
            ssize_t received = recv(client_sock, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                break;
            }
            response.append(buffer, received);
        }

        std::cout << "POST response:\n" << response << std::endl;
        EXPECT_TRUE(response.find("200") != std::string::npos &&
                    response.find("\"form\": {") != std::string::npos &&
                    response.find("\"test\": \"data\"") != std::string::npos)
            << "Unexpected POST response: " << response;

        close(client_sock);
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestPost: " << e.what();
    }
    
    std::cout << "=== Completed TestPost ===" << std::endl;
}

// ============== Test #3: CONNECT ==============
TEST_F(ProxyTest, TestConnect) {
    std::cout << "\n=== Starting TestConnect ===" << std::endl;
    
    try {
        int client_sock = create_client_socket();
        
        // Send CONNECT request to establish tunnel to httpbin.org:80
        std::string connect_req =
            "CONNECT httpbin.org:80 HTTP/1.1\r\n"
            "Host: httpbin.org:80\r\n\r\n";
        ssize_t sent = send(client_sock, connect_req.c_str(), connect_req.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(connect_req.size())) << "CONNECT request not fully sent.";
        
        // Set receive timeout
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        
        char recv_buf[4096];
        ssize_t received = recv(client_sock, recv_buf, sizeof(recv_buf), 0);
        EXPECT_GT(received, 0) << "No data received for CONNECT.";
        
        std::string initial_resp(recv_buf, received);
        std::cout << "CONNECT initial response:\n" << initial_resp << std::endl;
        EXPECT_TRUE(initial_resp.find("200 Connection established") != std::string::npos)
            << "Unexpected CONNECT response: " << initial_resp;
        
        // Tunnel established, send GET request through tunnel to httpbin.org/get
        std::string tunneled_get =
            "GET /get HTTP/1.1\r\n"
            "Host: httpbin.org\r\n"
            "Connection: close\r\n\r\n";
        sent = send(client_sock, tunneled_get.c_str(), tunneled_get.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(tunneled_get.size()))
            << "Failed to send tunneled GET request.";
        
        // Read tunneled response (may need to loop until connection closes)
        std::string tunneled_response;
        size_t total_received = 0;
        const size_t MAX_SIZE = 1024 * 1024; // 1MB safety limit

        while (true) {
            received = recv(client_sock, recv_buf, sizeof(recv_buf), 0);
            if (received <= 0) {
                if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    // Timeout occurred, check if we have enough data
                    if (!tunneled_response.empty() && 
                        tunneled_response.find("\r\n\r\n") != std::string::npos) {
                        break;
                    }
                    continue;
                }
                break;
            }
            tunneled_response.append(recv_buf, received);
            total_received += received;

            // Safety check for maximum response size
            if (total_received > MAX_SIZE) {
                throw std::runtime_error("Response too large");
            }
            
            // Check if we have a complete response
            if (tunneled_response.find("\r\n\r\n") != std::string::npos &&
                tunneled_response.find("\"url\": \"http://httpbin.org/get\"") != std::string::npos) {
                break;
            }
        }
        std::cout << "Tunneled GET response:\n" << tunneled_response << std::endl;
        EXPECT_TRUE(tunneled_response.find("200") != std::string::npos &&
                    tunneled_response.find("\"url\": \"http://httpbin.org/get\"") != std::string::npos)
            << "Unexpected tunneled GET response: " << tunneled_response;
        
        close(client_sock);
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestConnect: " << e.what();
    }
    
    std::cout << "=== Completed TestConnect ===" << std::endl;
}

// ============== Test #4: Chunked Transfer Encoding ==============
TEST_F(ProxyTest, TestChunkedTransferEncoding) {
    std::cout << "\n=== Starting TestChunkedTransferEncoding ===" << std::endl;
    
    try {
        int client_sock = create_client_socket();
        std::string get_request = 
            "GET http://www.httpwatch.com/httpgallery/chunked/chunkedimage.aspx HTTP/1.1\r\n"
            "Host: www.httpwatch.com\r\n"
            "Connection: close\r\n\r\n";
        
        ssize_t sent = send(client_sock, get_request.c_str(), get_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(get_request.size())) << "GET request not fully sent.";

        // Set receive timeout
        struct timeval tv;
        tv.tv_sec = 10;  // add timeout, because chunked response may be large
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        // loop to read response
        std::string response;
        char buffer[8192];  // use larger buffer
        bool chunked_found = false;
        bool chunked_end_found = false;
        
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // timeout, but if chunked mark and end mark are found, consider test successful
                    if (chunked_found && chunked_end_found) {
                        break;
                    }
                    std::cout << "Timeout waiting for complete chunked response" << std::endl;
                    continue;
                }
                break;  // connection closed or error
            }
            
            buffer[received] = '\0';
            response.append(buffer, received);
            
            // check if chunked transfer encoding mark is included
            if (response.find("Transfer-Encoding: chunked") != std::string::npos) {
                chunked_found = true;
                std::cout << "Found chunked transfer encoding header" << std::endl;
            }
            
            // check if chunked end mark (0\r\n\r\n) is included
            if (response.find("0\r\n\r\n") != std::string::npos) {
                chunked_end_found = true;
                std::cout << "Found chunked end marker" << std::endl;
            }
            
            // if all needed marks are found, can end loop early
            if (chunked_found && chunked_end_found) {
                break;
            }
        }

        // output response headers for debugging
        size_t header_end = response.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            std::cout << "Response headers:\n" << response.substr(0, header_end + 4) << std::endl;
        } else {
            std::cout << "Could not find end of headers in response" << std::endl;
        }

        // check if response includes chunked transfer encoding
        EXPECT_TRUE(chunked_found) << "Response does not use chunked transfer encoding";
        
        // check if complete chunked response is received
        EXPECT_TRUE(chunked_end_found) << "Did not receive complete chunked response";
        
        // check if response status code is 200 OK
        EXPECT_TRUE(response.find("HTTP/1.1 200 OK") != std::string::npos) 
            << "Response status is not 200 OK";

        // check if response content type is image
        EXPECT_TRUE(response.find("Content-Type: image/") != std::string::npos) 
            << "Response is not an image";

        close(client_sock);
        
        // test caching - send second request
        std::cout << "Sending second request to test caching..." << std::endl;
        client_sock = create_client_socket();
        
        sent = send(client_sock, get_request.c_str(), get_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(get_request.size())) << "Second GET request not fully sent.";
        
        // read second response
        std::string second_response;
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            second_response.append(buffer, received);
            
            // if chunked end mark is found, can end loop early
            if (second_response.find("0\r\n\r\n") != std::string::npos) {
                break;
            }
        }
        
        close(client_sock);
        
        // check if second response is also 200 OK
        EXPECT_TRUE(second_response.find("HTTP/1.1 200 OK") != std::string::npos) 
            << "Second response status is not 200 OK";
        
        // check if response includes chunked transfer encoding
        EXPECT_TRUE(second_response.find("Transfer-Encoding: chunked") != std::string::npos)
            << "Response does not use chunked transfer encoding";
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestChunkedTransferEncoding: " << e.what();
    }
    
    std::cout << "=== Completed TestChunkedTransferEncoding ===" << std::endl;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
