#include <gtest/gtest.h>
#include "../src/proxy.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include "../src/logger.hpp"
#include <iostream>

class ProxyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set test-specific log path
        std::string test_log_path = "test_logs/test_proxy.log";
        Logger::getInstance().setLogPath(test_log_path);
        
        // create proxy with port 0 (system will assign a port)
        proxy = std::make_unique<Proxy>(0);
        
        // Start proxy in a separate thread
        proxy_thread = std::thread([this]() {
            proxy->run();
        });
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // get the actual port
        current_port = proxy->getPort();
    }

    void TearDown() override {
        // Stop the proxy first
        proxy->stop();  
        if (proxy_thread.joinable()) {
            proxy_thread.join();
        }
        
        // Clean up any remaining Python HTTP server
        system("pkill -f 'python3 -m http.server 8080' 2>/dev/null || true");
        
        // Give some time for resources to be released
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Helper function to create client socket
    int createClientSocket() {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        EXPECT_GE(sock, 0);
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(current_port);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        EXPECT_EQ(result, 0);
        
        return sock;
    }

    std::unique_ptr<Proxy> proxy;
    std::thread proxy_thread;
    int current_port;
};

// Test GET request
TEST_F(ProxyTest, TestGet) {
    std::cout << "\n=== Starting TestGet ===" << std::endl;
    
    // start local Python HTTP server with proper cleanup
    std::string start_server = "python3 -m http.server 8080 > /dev/null 2>&1 & echo $!";
    FILE* pipe = popen(start_server.c_str(), "r");
    if (!pipe) {
        FAIL() << "Failed to start Python HTTP server";
    }
    char buffer[128];
    std::string pid;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        pid += buffer;
    }
    pclose(pipe);
    pid = pid.substr(0, pid.find_first_of("\n\r"));
    
    std::cout << "Started Python HTTP server on port 8080 (PID: " << pid << ")" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));  // wait for server to start
    
    try {
        int client_sock = createClientSocket();
        std::cout << "Created client socket" << std::endl;
        
        // use local server instead of example.com
        std::string request = "GET http://127.0.0.1:8080/ HTTP/1.1\r\n"
                             "Host: 127.0.0.1:8080\r\n"
                             "Connection: close\r\n\r\n";
        
        std::cout << "Sending GET request:\n" << request << std::endl;
        ssize_t sent = send(client_sock, request.c_str(), request.length(), 0);
        if (sent != static_cast<ssize_t>(request.length())) {
            close(client_sock);
            throw std::runtime_error("Failed to send complete request");
        }
        
        // Set receive timeout
        struct timeval tv;
        tv.tv_sec = 5;  // 5 seconds timeout
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        
        // Receive response
        char resp_buffer[8192];
        ssize_t received = recv(client_sock, resp_buffer, sizeof(resp_buffer), 0);
        if (received <= 0) {
            close(client_sock);
            throw std::runtime_error("Failed to receive response");
        }
        
        std::string response(resp_buffer, received);
        std::cout << "Received response:\n" << response << std::endl;
        // Accept both HTTP/1.0 and HTTP/1.1 200 responses
        EXPECT_TRUE(response.find(" 200 ") != std::string::npos);
        
        close(client_sock);
    } catch (const std::exception& e) {
        std::cerr << "Test failed with error: " << e.what() << std::endl;
        // Ensure cleanup happens even if test fails
        if (!pid.empty()) {
            std::string kill_cmd = "kill " + pid + " 2>/dev/null || true";
            system(kill_cmd.c_str());
        }
        FAIL() << e.what();
    }
    
    // clean up: close Python HTTP server using stored PID
    if (!pid.empty()) {
        std::string kill_cmd = "kill " + pid + " 2>/dev/null || true";
        system(kill_cmd.c_str());
        std::cout << "Cleaned up Python HTTP server (PID: " << pid << ")" << std::endl;
    }
    
    std::cout << "=== Completed TestGet ===\n" << std::endl;
}

// Test POST request
TEST_F(ProxyTest, TestPost) {
    int client_sock = createClientSocket();
    
    std::string body = "test=data";
    std::string request = "POST http://httpbin.org/post HTTP/1.1\r\n"
                         "Host: httpbin.org\r\n"
                         "Content-Type: application/x-www-form-urlencoded\r\n"
                         "Content-Length: " + std::to_string(body.length()) + "\r\n"
                         "Connection: close\r\n\r\n" + body;
    
    std::cout << "Sending POST request:\n" << request << std::endl;
    ssize_t sent = send(client_sock, request.c_str(), request.length(), 0);
    EXPECT_EQ(sent, static_cast<ssize_t>(request.length()));
    std::cout << "Successfully sent " << sent << " bytes" << std::endl;
    
    // Set receive timeout
    struct timeval tv;
    tv.tv_sec = 10;  // 10 seconds timeout
    tv.tv_usec = 0;
    setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    
    std::vector<char> response_buffer;
    char buffer[8192];
    ssize_t received;
    bool headers_complete = false;
    size_t content_length = 0;
    size_t body_start = 0;
    bool is_chunked = false;
    
    while ((received = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {
        std::cout << "Received " << received << " bytes" << std::endl;
        response_buffer.insert(response_buffer.end(), buffer, buffer + received);
        
        std::string response(response_buffer.begin(), response_buffer.end());
        
        if (!headers_complete) {
            size_t header_end = response.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                headers_complete = true;
                std::string headers = response.substr(0, header_end);
                std::cout << "Response headers:\n" << headers << std::endl;
                body_start = header_end + 4;
                
                // Look for Content-Length
                size_t cl_pos = headers.find("Content-Length: ");
                if (cl_pos != std::string::npos) {
                    size_t cl_end = headers.find("\r\n", cl_pos);
                    std::string cl_str = headers.substr(cl_pos + 16, cl_end - (cl_pos + 16));
                    content_length = std::stoul(cl_str);
                    std::cout << "Found Content-Length: " << content_length << std::endl;
                }
                
                // Look for chunked encoding
                if (headers.find("Transfer-Encoding: chunked") != std::string::npos) {
                    is_chunked = true;
                    std::cout << "Found chunked encoding" << std::endl;
                }
            }
        }
        
        if (headers_complete) {
            if (content_length > 0) {
                if (response_buffer.size() >= body_start + content_length) {
                    std::cout << "Received complete response with Content-Length" << std::endl;
                    break;
                }
            } else if (is_chunked) {
                if (response.find("\r\n0\r\n\r\n", body_start) != std::string::npos) {
                    std::cout << "Received complete chunked response" << std::endl;
                    break;
                }
            } else if (received == 0) {
                std::cout << "Server closed connection" << std::endl;
                break;
            }
        }
    }
    
    if (received < 0) {
        std::cout << "Error receiving response: " << strerror(errno) << std::endl;
        FAIL() << "Failed to receive response: " << strerror(errno);
    }
    
    std::string full_response(response_buffer.begin(), response_buffer.end());
    std::cout << "Full response:\n" << full_response << std::endl;
    
    EXPECT_TRUE(full_response.find("HTTP/1.1 200") != std::string::npos);
    EXPECT_TRUE(full_response.find("\"test\": \"data\"") != std::string::npos);
    
    close(client_sock);
}

// Test CONNECT request
TEST_F(ProxyTest, TestConnect) {
    std::cout << "\n=== Starting TestConnect ===" << std::endl;
    
    int client_sock = createClientSocket();
    std::cout << "Created client socket" << std::endl;
    
    // Use httpbin.org instead of Google, as it's more reliable for testing
    std::string request = "CONNECT httpbin.org:80 HTTP/1.1\r\n"
                         "Host: httpbin.org:80\r\n\r\n";
    
    std::cout << "Sending CONNECT request:\n" << request << std::endl;
    ssize_t sent = send(client_sock, request.c_str(), request.length(), 0);
    EXPECT_GT(sent, 0);
    std::cout << "Successfully sent " << sent << " bytes" << std::endl;
    
    // Set a shorter receive timeout (2 seconds)
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    
    char buffer[8192];
    ssize_t received = recv(client_sock, buffer, sizeof(buffer), 0);
    if (received <= 0) {
        std::cout << "Failed to receive response: " << strerror(errno) << std::endl;
        close(client_sock);
        FAIL() << "Failed to receive response: " << strerror(errno);
        return;
    }
    
    std::string response(buffer, received);
    std::cout << "Received response:\n" << response << std::endl;
    EXPECT_TRUE(response.find("HTTP/1.1 200 Connection established") != std::string::npos);
    
    close(client_sock);
    std::cout << "=== Completed TestConnect ===\n" << std::endl;
}

// Test malformed request
TEST_F(ProxyTest, TestMalformedRequest) {
    std::cout << "\n=== Starting TestMalformedRequest ===" << std::endl;
    
    int client_sock = createClientSocket();
    std::cout << "Created client socket" << std::endl;
    
    // Send malformed request
    std::string request = "INVALID REQUEST\r\n\r\n";
    std::cout << "Sending malformed request:\n" << request << std::endl;
    send(client_sock, request.c_str(), request.length(), 0);
    
    char buffer[8192];
    ssize_t received = recv(client_sock, buffer, sizeof(buffer), 0);
    EXPECT_GT(received, 0);
    
    std::string response(buffer, received);
    std::cout << "Received response:\n" << response << std::endl;
    EXPECT_TRUE(response.find("HTTP/1.1 400 Bad Request") != std::string::npos);
    
    close(client_sock);
    std::cout << "=== Completed TestMalformedRequest ===\n" << std::endl;
}

// Test unsupported method
TEST_F(ProxyTest, TestUnsupportedMethod) {
    std::cout << "\n=== Starting TestUnsupportedMethod ===" << std::endl;
    
    int client_sock = createClientSocket();
    std::cout << "Created client socket" << std::endl;
    
    // Send PUT request (unsupported)
    std::string request = "PUT http://example.com/test HTTP/1.1\r\n"
                         "Host: example.com\r\n\r\n";
    std::cout << "Sending unsupported PUT request:\n" << request << std::endl;
    send(client_sock, request.c_str(), request.length(), 0);
    
    char buffer[8192];
    ssize_t received = recv(client_sock, buffer, sizeof(buffer), 0);
    EXPECT_GT(received, 0);
    
    std::string response(buffer, received);
    std::cout << "Received response:\n" << response << std::endl;
    EXPECT_TRUE(response.find("HTTP/1.1 405 Method Not Allowed") != std::string::npos);
    
    close(client_sock);
    std::cout << "=== Completed TestUnsupportedMethod ===\n" << std::endl;
}

// Test concurrent requests
TEST_F(ProxyTest, TestConcurrentRequests) {
    std::cout << "\n=== Starting TestConcurrentRequests ===" << std::endl;
    
    // start local Python HTTP server
    system("python3 -m http.server 8080 &");
    std::cout << "Started Python HTTP server on port 8080" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    const int NUM_REQUESTS = 5;
    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);
    
    auto make_request = [&]() {
        int client_sock = createClientSocket();
        std::string request = "GET http://127.0.0.1:8080/ HTTP/1.1\r\n"
                            "Host: 127.0.0.1:8080\r\n"
                            "Connection: close\r\n\r\n";
        
        std::cout << "Thread sending request:\n" << request << std::endl;
        send(client_sock, request.c_str(), request.length(), 0);
        
        char buffer[8192];
        ssize_t received = recv(client_sock, buffer, sizeof(buffer), 0);
        if (received > 0) {
            std::string response(buffer, received);
            std::cout << "Thread received response with status: " 
                     << (response.find(" 200 ") != std::string::npos ? "200" : "non-200") << std::endl;
            if (response.find(" 200 ") != std::string::npos) {
                success_count++;
            }
        }
        close(client_sock);
    };
    
    // Launch concurrent requests
    std::cout << "Launching " << NUM_REQUESTS << " concurrent requests" << std::endl;
    for (int i = 0; i < NUM_REQUESTS; i++) {
        threads.emplace_back(make_request);
    }
    
    // Wait for all requests to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "All threads completed. Success count: " << success_count << std::endl;
    EXPECT_EQ(success_count, NUM_REQUESTS);
    
    // clean up: close Python HTTP server
    system("pkill -f 'python3 -m http.server 8080'");
    std::cout << "=== Completed TestConcurrentRequests ===\n" << std::endl;
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
