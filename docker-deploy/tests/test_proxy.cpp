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

class ProxyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set test-specific log path
        std::string test_log_path = "test_logs/test_proxy.log";
        Logger::getInstance().setLogPath(test_log_path);
        
        // create proxy, let the system assign port
        proxy = std::make_unique<Proxy>();
        
        // Start proxy in a separate thread
        proxy_thread = std::thread([this]() {
            proxy->run();
        });
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // get the actual port
        current_port = proxy->getPort();
    }

    void TearDown() override {
        proxy->stop();  
        if (proxy_thread.joinable()) {
            proxy_thread.join();
        }
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
    // start local Python HTTP server
    system("python3 -m http.server 8080 &");
    std::this_thread::sleep_for(std::chrono::seconds(1));  // wait for server to start
    
    int client_sock = createClientSocket();
    
    // use local server instead of example.com
    std::string request = "GET http://127.0.0.1:8080/ HTTP/1.1\r\n"
                         "Host: 127.0.0.1:8080\r\n"
                         "Connection: close\r\n\r\n";
    
    send(client_sock, request.c_str(), request.length(), 0);
    
    // Receive response
    char buffer[8192];
    ssize_t received = recv(client_sock, buffer, sizeof(buffer), 0);
    EXPECT_GT(received, 0);
    
    std::string response(buffer, received);
    // Accept both HTTP/1.0 and HTTP/1.1 200 responses
    EXPECT_TRUE(response.find(" 200 ") != std::string::npos);
    
    close(client_sock);
    
    // clean up: close Python HTTP server
    system("pkill -f 'python3 -m http.server 8080'");
}

// Test POST request
TEST_F(ProxyTest, TestPost) {
    int client_sock = createClientSocket();
    
    std::string body = "test=data";
    std::string request = "POST http://httpbin.org/post HTTP/1.1\r\n"
                         "Host: httpbin.org\r\n"
                         "Content-Length: " + std::to_string(body.length()) + "\r\n"
                         "Connection: close\r\n\r\n" + body;
    
    send(client_sock, request.c_str(), request.length(), 0);
    
    char buffer[8192];
    ssize_t received = recv(client_sock, buffer, sizeof(buffer), 0);
    EXPECT_GT(received, 0);
    
    std::string response(buffer, received);
    EXPECT_TRUE(response.find("HTTP/1.1 200") != std::string::npos);
    
    close(client_sock);
}

// Test CONNECT request
TEST_F(ProxyTest, TestConnect) {
    int client_sock = createClientSocket();
    
    std::string request = "CONNECT www.google.com:443 HTTP/1.1\r\n"
                         "Host: www.google.com:443\r\n\r\n";
    
    send(client_sock, request.c_str(), request.length(), 0);
    
    char buffer[8192];
    ssize_t received = recv(client_sock, buffer, sizeof(buffer), 0);
    EXPECT_GT(received, 0);
    
    std::string response(buffer, received);
    EXPECT_TRUE(response.find("HTTP/1.1 200 Connection established") != std::string::npos);
    
    close(client_sock);
}

// Test malformed request
TEST_F(ProxyTest, TestMalformedRequest) {
    int client_sock = createClientSocket();
    
    // Send malformed request
    std::string request = "INVALID REQUEST\r\n\r\n";
    send(client_sock, request.c_str(), request.length(), 0);
    
    char buffer[8192];
    ssize_t received = recv(client_sock, buffer, sizeof(buffer), 0);
    EXPECT_GT(received, 0);
    
    std::string response(buffer, received);
    EXPECT_TRUE(response.find("HTTP/1.1 400 Bad Request") != std::string::npos);
    
    close(client_sock);
}

// Test unsupported method
TEST_F(ProxyTest, TestUnsupportedMethod) {
    int client_sock = createClientSocket();
    
    // Send PUT request (unsupported)
    std::string request = "PUT http://example.com/test HTTP/1.1\r\n"
                         "Host: example.com\r\n\r\n";
    send(client_sock, request.c_str(), request.length(), 0);
    
    char buffer[8192];
    ssize_t received = recv(client_sock, buffer, sizeof(buffer), 0);
    EXPECT_GT(received, 0);
    
    std::string response(buffer, received);
    EXPECT_TRUE(response.find("HTTP/1.1 405 Method Not Allowed") != std::string::npos);
    
    close(client_sock);
}

// Test concurrent requests
TEST_F(ProxyTest, TestConcurrentRequests) {
    // start local Python HTTP server
    system("python3 -m http.server 8080 &");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    const int NUM_REQUESTS = 5;
    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);
    
    auto make_request = [&]() {
        int client_sock = createClientSocket();
        std::string request = "GET http://127.0.0.1:8080/ HTTP/1.1\r\n"
                            "Host: 127.0.0.1:8080\r\n"
                            "Connection: close\r\n\r\n";
        
        send(client_sock, request.c_str(), request.length(), 0);
        
        char buffer[8192];
        ssize_t received = recv(client_sock, buffer, sizeof(buffer), 0);
        if (received > 0) {
            std::string response(buffer, received);
            if (response.find(" 200 ") != std::string::npos) {
                success_count++;
            }
        }
        close(client_sock);
    };
    
    // Launch concurrent requests
    for (int i = 0; i < NUM_REQUESTS; i++) {
        threads.emplace_back(make_request);
    }
    
    // Wait for all requests to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(success_count, NUM_REQUESTS);
    
    // clean up: close Python HTTP server
    system("pkill -f 'python3 -m http.server 8080'");
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
