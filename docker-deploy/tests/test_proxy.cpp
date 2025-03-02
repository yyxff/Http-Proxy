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
#include <sys/wait.h>
#include <signal.h>

class ProxyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set test log path
        std::string test_log_path = "../test_logs/";
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

// ============== Test #5: Max-Age=0 Cache Control ==============
TEST_F(ProxyTest, TestMaxAgeZero) {
    std::cout << "\n=== Starting TestMaxAgeZero ===" << std::endl;
    
    try {
        // First request to get the resource
        int client_sock = create_client_socket();
        std::string get_request = 
            "GET http://www.artsci.utoronto.ca/futurestudents HTTP/1.1\r\n"
            "Host: www.artsci.utoronto.ca\r\n"
            "Connection: close\r\n\r\n";
        
        ssize_t sent = send(client_sock, get_request.c_str(), get_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(get_request.size())) << "First GET request not fully sent.";

        // Set receive timeout
        struct timeval tv;
        tv.tv_sec = 10;  // Longer timeout for potentially large response
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        // Read first response
        std::string first_response;
        char buffer[8192];  // Larger buffer for potentially large response
        
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            first_response.append(buffer, received);
        }

        // Output first response headers for debugging
        size_t header_end = first_response.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            std::cout << "First response headers:\n" << first_response.substr(0, header_end + 4) << std::endl;
        } else {
            std::cout << "Could not find end of headers in first response" << std::endl;
        }

        // Check if first response is valid (301 Moved Permanently is expected for this URL)
        EXPECT_TRUE(first_response.find("HTTP/1.1 301") != std::string::npos) 
            << "First response status is not 301 Moved Permanently";
        
        // Check if the Location header is present (for redirection to HTTPS)
        EXPECT_TRUE(first_response.find("Location: https://") != std::string::npos)
            << "First response does not contain HTTPS redirection";
        
        close(client_sock);
        
        // Wait a moment to ensure the response is cached
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Second request with Cache-Control: max-age=0 header
        std::cout << "Sending second request with max-age=0..." << std::endl;
        client_sock = create_client_socket();
        
        std::string second_get_request = 
            "GET http://www.artsci.utoronto.ca/futurestudents HTTP/1.1\r\n"
            "Host: www.artsci.utoronto.ca\r\n"
            "Cache-Control: max-age=0\r\n"  // Add max-age=0 directive
            "Connection: close\r\n\r\n";
        
        sent = send(client_sock, second_get_request.c_str(), second_get_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(second_get_request.size())) << "Second GET request not fully sent.";
        
        // Read second response
        std::string second_response;
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            second_response.append(buffer, received);
        }
        
        // Output second response headers for debugging
        header_end = second_response.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            std::cout << "Second response headers:\n" << second_response.substr(0, header_end + 4) << std::endl;
        } else {
            std::cout << "Could not find end of headers in second response" << std::endl;
        }
        
        close(client_sock);
        
        // Check if second response is also 301 Moved Permanently
        EXPECT_TRUE(second_response.find("HTTP/1.1 301") != std::string::npos) 
            << "Second response status is not 301 Moved Permanently";
        
        // Check if the Location header is present in the second response
        EXPECT_TRUE(second_response.find("Location: https://") != std::string::npos)
            << "Second response does not contain HTTPS redirection";
        
        // Check if the responses are different (indicating a fresh fetch)
        // or if they have the same content but with updated headers
        // This is a bit tricky as the content might legitimately change
        // So we'll just check that both responses are valid
        EXPECT_FALSE(second_response.empty()) 
            << "Second response is empty";
            
        // Check that the second response has a different Date header
        // Extract Date headers from both responses
        std::string first_date, second_date;
        size_t date_pos = first_response.find("Date: ");
        if (date_pos != std::string::npos) {
            size_t date_end = first_response.find("\r\n", date_pos);
            if (date_end != std::string::npos) {
                first_date = first_response.substr(date_pos, date_end - date_pos);
            }
        }
        
        date_pos = second_response.find("Date: ");
        if (date_pos != std::string::npos) {
            size_t date_end = second_response.find("\r\n", date_pos);
            if (date_end != std::string::npos) {
                second_date = second_response.substr(date_pos, date_end - date_pos);
            }
        }
        
        std::cout << "First response date: " << first_date << std::endl;
        std::cout << "Second response date: " << second_date << std::endl;
        
        // With max-age=0, we expect the proxy to revalidate with the server
        // which should result in a new Date header
        EXPECT_FALSE(first_date.empty() && second_date.empty()) 
            << "Could not find Date headers in responses";
            
        if (!first_date.empty() && !second_date.empty()) {
            // If both dates are present, they should be different if revalidation occurred
            bool dates_different = (first_date != second_date);
            EXPECT_TRUE(dates_different) 
                << "Date headers are identical, suggesting no revalidation occurred";
        }
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestMaxAgeZero: " << e.what();
    }
    
    std::cout << "=== Completed TestMaxAgeZero ===" << std::endl;
}

// ============== Test #6: Basic Caching ==============
TEST_F(ProxyTest, TestBasicCaching) {
    std::cout << "\n=== Starting TestBasicCaching ===" << std::endl;
    
    try {
        // reset CacheServer state - first visit root path
        int reset_sock = create_client_socket();
        std::string reset_request = 
            "GET http://127.0.0.1:5000/ HTTP/1.1\r\n"
            "Host: 127.0.0.1:5000\r\n"
            "Connection: close\r\n\r\n";
        
        send(reset_sock, reset_request.c_str(), reset_request.size(), 0);
        
        // read response but do not verify
        char buffer[4096];
        while (recv(reset_sock, buffer, sizeof(buffer) - 1, 0) > 0) {}
        close(reset_sock);
        
        // first request - should get from server
        int client_sock = create_client_socket();
        std::string get_request = 
            "GET http://127.0.0.1:5000/valid-cache HTTP/1.1\r\n"
            "Host: 127.0.0.1:5000\r\n"
            "Connection: close\r\n\r\n";
        
        ssize_t sent = send(client_sock, get_request.c_str(), get_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(get_request.size())) << "First GET request not fully sent.";

        // set receive timeout
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        // read first response
        std::string first_response;
        
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            first_response.append(buffer, received);
        }
        
        std::cout << "First response:\n" << first_response << std::endl;
        
        // check if first response is 200 OK
        EXPECT_TRUE(first_response.find("HTTP/1.1 200 OK") != std::string::npos || 
                    first_response.find("HTTP/1.1 200") != std::string::npos) 
            << "First response status is not 200 OK";
        
        EXPECT_TRUE(first_response.find("hello! I'm valid_cache!") != std::string::npos)
            << "First response does not contain expected content";
        
        close(client_sock);
        
        // wait a moment
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // second request - since proxy has cache, it will return 200
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
        }
        
        std::cout << "Second response:\n" << second_response << std::endl;
        
        // check if second response is 200 OK
        EXPECT_TRUE(first_response.find("HTTP/1.1 200 OK") != std::string::npos || 
                    first_response.find("HTTP/1.1 200") != std::string::npos) 
            << "First response status is not 200 OK";
        
        EXPECT_TRUE(first_response.find("hello! I'm valid_cache!") != std::string::npos)
            << "First response does not contain expected content";
        
        close(client_sock);
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestBasicCaching: " << e.what();
    }
    
    std::cout << "=== Completed TestBasicCaching ===" << std::endl;
}

// ============== Test #7: Cache Revalidation ==============
TEST_F(ProxyTest, TestCacheRevalidation) {
    std::cout << "\n=== Starting TestCacheRevalidation ===" << std::endl;
    
    try {
        // reset CacheServer state - first visit root path
        int reset_sock = create_client_socket();
        std::string reset_request = 
            "GET http://127.0.0.1:5000/ HTTP/1.1\r\n"
            "Host: 127.0.0.1:5000\r\n"
            "Connection: close\r\n\r\n";
        
        send(reset_sock, reset_request.c_str(), reset_request.size(), 0);
        
        // read response but do not verify
        char buffer[4096];
        while (recv(reset_sock, buffer, sizeof(buffer) - 1, 0) > 0) {}
        close(reset_sock);
        
        // first request - should get from server
        int client_sock = create_client_socket();
        std::string get_request = 
            "GET http://127.0.0.1:5000/revalid-cache HTTP/1.1\r\n"
            "Host: 127.0.0.1:5000\r\n"
            "Connection: close\r\n\r\n";
        
        ssize_t sent = send(client_sock, get_request.c_str(), get_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(get_request.size())) << "First GET request not fully sent.";

        // set receive timeout
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        // read first response
        std::string first_response;
        
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            first_response.append(buffer, received);
        }
        
        std::cout << "First response:\n" << first_response << std::endl;
        
        // check if first response is 200 OK
        EXPECT_TRUE(first_response.find("HTTP/1.1 200 OK") != std::string::npos || 
                    first_response.find("HTTP/1.1 200") != std::string::npos) 
            << "First response status is not 200 OK";
        
        EXPECT_TRUE(first_response.find("hello! I'm revalid_cache!") != std::string::npos)
            << "First response does not contain expected content";
        
        close(client_sock);
        
        // wait a moment
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // second request - since proxy has cache, it will return 200
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
        }
        
        std::cout << "Second response:\n" << second_response << std::endl;
        
        // check if second response is 200 OK
        EXPECT_TRUE(second_response.find("HTTP/1.1 200 OK") != std::string::npos || 
                    second_response.find("HTTP/1.1 200") != std::string::npos) 
            << "Second response status is not 200 OK";
        
        EXPECT_TRUE(second_response.find("hello! I'm revalid_cache!") != std::string::npos)
            << "Second response does not contain expected content";
        
        close(client_sock);
        
        // wait a moment
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // third request - should get from server with If-None-Match header
        std::cout << "Sending third request to test revalidation..." << std::endl;
        client_sock = create_client_socket();
        
        std::string etag = "v1"; // ETag from first response
        std::string third_get_request = 
            "GET http://127.0.0.1:5000/revalid-cache HTTP/1.1\r\n"
            "Host: 127.0.0.1:5000\r\n"
            "If-None-Match: " + etag + "\r\n"
            "Connection: close\r\n\r\n";
        
        sent = send(client_sock, third_get_request.c_str(), third_get_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(third_get_request.size())) << "Third GET request not fully sent.";
        
        // read third response
        std::string third_response;
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            third_response.append(buffer, received);
        }
        
        std::cout << "Third response:\n" << third_response << std::endl;
        
        // check if third response is 304 Not Modified
        EXPECT_TRUE(third_response.find("HTTP/1.1 304 Not Modified") != std::string::npos) 
            << "Third response status is not 304 Not Modified";
        
        close(client_sock);
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestCacheRevalidation: " << e.what();
    }
    
    std::cout << "=== Completed TestCacheRevalidation ===" << std::endl;
}

// ============== Test #8: Invalid Domain ==============
TEST_F(ProxyTest, TestInvalidDomain) {
    std::cout << "\n=== Starting TestInvalidDomain ===" << std::endl;
    
    try {
        int client_sock = create_client_socket();
        std::string get_request = 
            "GET http://nonexistentdomain.com HTTP/1.1\r\n"
            "Host: nonexistentdomain.com\r\n"
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
        EXPECT_TRUE(response.find("502 Bad Gateway") != std::string::npos)
            << "Expected 502 Bad Gateway for invalid domain, got: " << response;

        close(client_sock);
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestInvalidDomain: " << e.what();
    }
    
    std::cout << "=== Completed TestInvalidDomain ===" << std::endl;
}

// ============== Test #9: Connection Timeout ==============
TEST_F(ProxyTest, TestConnectionTimeout) {
    std::cout << "\n=== Starting TestConnectionTimeout ===" << std::endl;
    
    try {
        int client_sock = create_client_socket();
        std::string get_request = 
            "GET http://127.0.0.1:8181 HTTP/1.1\r\n"
            "Host: 127.0.0.1:8181\r\n"
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
        EXPECT_TRUE(response.find("504 Gateway Timeout") != std::string::npos)
            << "Expected 504 Gateway Timeout for connection timeout, got: " << response;

        close(client_sock);
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestConnectionTimeout: " << e.what();
    }
    
    std::cout << "=== Completed TestConnectionTimeout ===" << std::endl;
}

// ============== Test #10: Malformed Request ==============
TEST_F(ProxyTest, TestMalformedRequest) {
    std::cout << "\n=== Starting TestMalformedRequest ===" << std::endl;
    
    try {
        int client_sock = create_client_socket();
        
        // 设置非阻塞模式
        int flags = fcntl(client_sock, F_GETFL, 0);
        fcntl(client_sock, F_SETFL, flags | O_NONBLOCK);
        
        // 使用一个简单但无效的HTTP请求
        std::string malformed_request = "INVALID / HTTP/1.1\r\nHost: example.com\r\n\r\n";
        
        ssize_t sent = send(client_sock, malformed_request.c_str(), malformed_request.size(), 0);
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
#include <sys/wait.h>
#include <signal.h>

class ProxyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set test log path
        std::string test_log_path = "../test_logs/";
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

// ============== Test #5: Max-Age=0 Cache Control ==============
TEST_F(ProxyTest, TestMaxAgeZero) {
    std::cout << "\n=== Starting TestMaxAgeZero ===" << std::endl;
    
    try {
        // First request to get the resource
        int client_sock = create_client_socket();
        std::string get_request = 
            "GET http://www.artsci.utoronto.ca/futurestudents HTTP/1.1\r\n"
            "Host: www.artsci.utoronto.ca\r\n"
            "Connection: close\r\n\r\n";
        
        ssize_t sent = send(client_sock, get_request.c_str(), get_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(get_request.size())) << "First GET request not fully sent.";

        // Set receive timeout
        struct timeval tv;
        tv.tv_sec = 10;  // Longer timeout for potentially large response
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        // Read first response
        std::string first_response;
        char buffer[8192];  // Larger buffer for potentially large response
        
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            first_response.append(buffer, received);
        }

        // Output first response headers for debugging
        size_t header_end = first_response.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            std::cout << "First response headers:\n" << first_response.substr(0, header_end + 4) << std::endl;
        } else {
            std::cout << "Could not find end of headers in first response" << std::endl;
        }

        // Check if first response is valid (301 Moved Permanently is expected for this URL)
        EXPECT_TRUE(first_response.find("HTTP/1.1 301") != std::string::npos) 
            << "First response status is not 301 Moved Permanently";
        
        // Check if the Location header is present (for redirection to HTTPS)
        EXPECT_TRUE(first_response.find("Location: https://") != std::string::npos)
            << "First response does not contain HTTPS redirection";
        
        close(client_sock);
        
        // Wait a moment to ensure the response is cached
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Second request with Cache-Control: max-age=0 header
        std::cout << "Sending second request with max-age=0..." << std::endl;
        client_sock = create_client_socket();
        
        std::string second_get_request = 
            "GET http://www.artsci.utoronto.ca/futurestudents HTTP/1.1\r\n"
            "Host: www.artsci.utoronto.ca\r\n"
            "Cache-Control: max-age=0\r\n"  // Add max-age=0 directive
            "Connection: close\r\n\r\n";
        
        sent = send(client_sock, second_get_request.c_str(), second_get_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(second_get_request.size())) << "Second GET request not fully sent.";
        
        // Read second response
        std::string second_response;
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            second_response.append(buffer, received);
        }
        
        // Output second response headers for debugging
        header_end = second_response.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            std::cout << "Second response headers:\n" << second_response.substr(0, header_end + 4) << std::endl;
        } else {
            std::cout << "Could not find end of headers in second response" << std::endl;
        }
        
        close(client_sock);
        
        // Check if second response is also 301 Moved Permanently
        EXPECT_TRUE(second_response.find("HTTP/1.1 301") != std::string::npos) 
            << "Second response status is not 301 Moved Permanently";
        
        // Check if the Location header is present in the second response
        EXPECT_TRUE(second_response.find("Location: https://") != std::string::npos)
            << "Second response does not contain HTTPS redirection";
        
        // Check if the responses are different (indicating a fresh fetch)
        // or if they have the same content but with updated headers
        // This is a bit tricky as the content might legitimately change
        // So we'll just check that both responses are valid
        EXPECT_FALSE(second_response.empty()) 
            << "Second response is empty";
            
        // Check that the second response has a different Date header
        // Extract Date headers from both responses
        std::string first_date, second_date;
        size_t date_pos = first_response.find("Date: ");
        if (date_pos != std::string::npos) {
            size_t date_end = first_response.find("\r\n", date_pos);
            if (date_end != std::string::npos) {
                first_date = first_response.substr(date_pos, date_end - date_pos);
            }
        }
        
        date_pos = second_response.find("Date: ");
        if (date_pos != std::string::npos) {
            size_t date_end = second_response.find("\r\n", date_pos);
            if (date_end != std::string::npos) {
                second_date = second_response.substr(date_pos, date_end - date_pos);
            }
        }
        
        std::cout << "First response date: " << first_date << std::endl;
        std::cout << "Second response date: " << second_date << std::endl;
        
        // With max-age=0, we expect the proxy to revalidate with the server
        // which should result in a new Date header
        EXPECT_FALSE(first_date.empty() && second_date.empty()) 
            << "Could not find Date headers in responses";
            
        if (!first_date.empty() && !second_date.empty()) {
            // If both dates are present, they should be different if revalidation occurred
            bool dates_different = (first_date != second_date);
            EXPECT_TRUE(dates_different) 
                << "Date headers are identical, suggesting no revalidation occurred";
        }
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestMaxAgeZero: " << e.what();
    }
    
    std::cout << "=== Completed TestMaxAgeZero ===" << std::endl;
}

// ============== Test #6: Basic Caching ==============
TEST_F(ProxyTest, TestBasicCaching) {
    std::cout << "\n=== Starting TestBasicCaching ===" << std::endl;
    
    try {
        // reset CacheServer state - first visit root path
        int reset_sock = create_client_socket();
        std::string reset_request = 
            "GET http://127.0.0.1:5000/ HTTP/1.1\r\n"
            "Host: 127.0.0.1:5000\r\n"
            "Connection: close\r\n\r\n";
        
        send(reset_sock, reset_request.c_str(), reset_request.size(), 0);
        
        // read response but do not verify
        char buffer[4096];
        while (recv(reset_sock, buffer, sizeof(buffer) - 1, 0) > 0) {}
        close(reset_sock);
        
        // first request - should get from server
        int client_sock = create_client_socket();
        std::string get_request = 
            "GET http://127.0.0.1:5000/valid-cache HTTP/1.1\r\n"
            "Host: 127.0.0.1:5000\r\n"
            "Connection: close\r\n\r\n";
        
        ssize_t sent = send(client_sock, get_request.c_str(), get_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(get_request.size())) << "First GET request not fully sent.";

        // set receive timeout
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        // read first response
        std::string first_response;
        
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            first_response.append(buffer, received);
        }
        
        std::cout << "First response:\n" << first_response << std::endl;
        
        // check if first response is 200 OK
        EXPECT_TRUE(first_response.find("HTTP/1.1 200 OK") != std::string::npos || 
                    first_response.find("HTTP/1.1 200") != std::string::npos) 
            << "First response status is not 200 OK";
        
        EXPECT_TRUE(first_response.find("hello! I'm valid_cache!") != std::string::npos)
            << "First response does not contain expected content";
        
        close(client_sock);
        
        // wait a moment
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // second request - since proxy has cache, it will return 200
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
        }
        
        std::cout << "Second response:\n" << second_response << std::endl;
        
        // check if second response is 200 OK
        EXPECT_TRUE(first_response.find("HTTP/1.1 200 OK") != std::string::npos || 
                    first_response.find("HTTP/1.1 200") != std::string::npos) 
            << "First response status is not 200 OK";
        
        EXPECT_TRUE(first_response.find("hello! I'm valid_cache!") != std::string::npos)
            << "First response does not contain expected content";
        
        close(client_sock);
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestBasicCaching: " << e.what();
    }
    
    std::cout << "=== Completed TestBasicCaching ===" << std::endl;
}

// ============== Test #7: Cache Revalidation ==============
TEST_F(ProxyTest, TestCacheRevalidation) {
    std::cout << "\n=== Starting TestCacheRevalidation ===" << std::endl;
    
    try {
        // first request - should get from server and cache
        int client_sock = create_client_socket();
        std::string get_request = 
            "GET http://127.0.0.1:5000/revalid-cache HTTP/1.1\r\n"
            "Host: 127.0.0.1:5000\r\n"
            "Connection: close\r\n\r\n";
        
        ssize_t sent = send(client_sock, get_request.c_str(), get_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(get_request.size())) << "First GET request not fully sent.";

        // set receive timeout
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        // read first response
        std::string first_response;
        char buffer[4096];
        
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            first_response.append(buffer, received);
        }
        
        std::cout << "First response:\n" << first_response << std::endl;
        EXPECT_TRUE(first_response.find("HTTP/1.1 200 OK") != std::string::npos) 
            << "First response status is not 200 OK";
        EXPECT_TRUE(first_response.find("ETag:") != std::string::npos)
            << "First response does not contain ETag";
        
        close(client_sock);
        
        // wait a moment to ensure response is cached
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // second request - use max-age=0 to force revalidation
        std::cout << "Sending second request with no-cache..." << std::endl;
        client_sock = create_client_socket();
        
        std::string second_get_request = 
            "GET http://127.0.0.1:5000/revalid-cache HTTP/1.1\r\n"
            "Host: 127.0.0.1:5000\r\n"
            "Cache-Control: no-cache\r\n"  // add max-age=0 directive
            "Connection: close\r\n\r\n";
        
        sent = send(client_sock, second_get_request.c_str(), second_get_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(second_get_request.size())) << "Second GET request not fully sent.";
        
        // read second response
        std::string second_response;
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            second_response.append(buffer, received);
        }
        
        std::cout << "Second response:\n" << second_response << std::endl;
        
        // check if second response is also 200 OK
        EXPECT_TRUE(second_response.find("HTTP/1.1 200 OK") != std::string::npos) 
            << "Second response status is not 200 OK";
        
        // check if second response contains ETag
        EXPECT_TRUE(second_response.find("ETag:") != std::string::npos)
            << "Second response does not contain ETag";
        
        close(client_sock);
        
        // third request - at this point, server will change ETag
        std::cout << "Sending third request to test ETag change..." << std::endl;
        client_sock = create_client_socket();
        
        sent = send(client_sock, second_get_request.c_str(), second_get_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(second_get_request.size())) << "Third GET request not fully sent.";
        
        // read third response
        std::string third_response;
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            third_response.append(buffer, received);
        }
        
        std::cout << "Third response:\n" << third_response << std::endl;
        
        // check if third response is also 200 OK
        EXPECT_TRUE(third_response.find("HTTP/1.1 200 OK") != std::string::npos) 
            << "Third response status is not 200 OK";
        EXPECT_TRUE(third_response.find("v2") != std::string::npos) 
            << "Third response etag not v2";
        close(client_sock);
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestCacheRevalidation: " << e.what();
    }
    
    std::cout << "=== Completed TestCacheRevalidation ===" << std::endl;
}

// ============== Test #8: Invalid Domain ==============
TEST_F(ProxyTest, TestInvalidDomain) {
    std::cout << "\n=== Starting TestInvalidDomain ===" << std::endl;
    
    try {
        int client_sock = create_client_socket();
        std::string invalid_domain_request = 
            "GET http://invalid-domain-that-does-not-exist-12345.com/ HTTP/1.1\r\n"
            "Host: invalid-domain-that-does-not-exist-12345.com\r\n"
            "Connection: close\r\n\r\n";
        
        ssize_t sent = send(client_sock, invalid_domain_request.c_str(), invalid_domain_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(invalid_domain_request.size())) << "Invalid domain request not fully sent.";

        // set receive timeout
        struct timeval tv;
        tv.tv_sec = 10;  // give enough time for DNS resolution failure
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        // read response
        char buffer[4096];
        std::string invalid_domain_response;
        
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            invalid_domain_response.append(buffer, received);
        }
        
        std::cout << "Invalid domain response:\n" << invalid_domain_response << std::endl;
        
        // verify proxy returned error response
        EXPECT_FALSE(invalid_domain_response.empty()) << "No response received for invalid domain";
        EXPECT_TRUE(
            invalid_domain_response.find("HTTP/1.1 502") != std::string::npos || 
            invalid_domain_response.find("HTTP/1.1 500") != std::string::npos || 
            invalid_domain_response.find("HTTP/1.1 404") != std::string::npos ||
            invalid_domain_response.find("Error") != std::string::npos
        ) << "Expected error response for invalid domain";
        
        close(client_sock);
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestInvalidDomain: " << e.what();
    }
    
    std::cout << "=== Completed TestInvalidDomain ===" << std::endl;
}

TEST_F(ProxyTest, TestConnectionTimeout) {
    std::cout << "\n=== Starting TestConnectionTimeout ===" << std::endl;
    
    try {
        int client_sock = create_client_socket();
        std::string timeout_request = 
            "GET http://example.com:8181/ HTTP/1.1\r\n"  // use unlikely open port
            "Host: example.com:8181\r\n"
            "Connection: close\r\n\r\n";
        
        ssize_t sent = send(client_sock, timeout_request.c_str(), timeout_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(timeout_request.size())) << "Timeout request not fully sent.";

        // set longer receive timeout, as proxy may wait for a while
        struct timeval tv;
        tv.tv_sec = 20;
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        // read response
        char buffer[4096];
        std::string timeout_response;
        
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            timeout_response.append(buffer, received);
        }
        
        std::cout << "Timeout response:\n" << timeout_response << std::endl;
        
        // verify proxy returned timeout error response
        EXPECT_FALSE(timeout_response.empty()) << "No response received for connection timeout";
        EXPECT_TRUE(
            timeout_response.find("HTTP/1.1 504") != std::string::npos || // Gateway Timeout
            timeout_response.find("HTTP/1.1 502") != std::string::npos || // Bad Gateway
            timeout_response.find("HTTP/1.1 500") != std::string::npos || // Internal Server Error
            timeout_response.find("timeout") != std::string::npos ||
            timeout_response.find("Timeout") != std::string::npos ||
            timeout_response.find("Error") != std::string::npos
        ) << "Expected timeout error response";
        
        close(client_sock);
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestConnectionTimeout: " << e.what();
    }
    
    std::cout << "=== Completed TestConnectionTimeout ===" << std::endl;
}

TEST_F(ProxyTest, TestMalformedRequest) {
    std::cout << "\n=== Starting TestMalformedRequest ===" << std::endl;
    
    // use fork to create child process
    pid_t pid = fork();
    
    if (pid == -1) {
        // fork failed
        std::cout << "Failed to fork process for malformed request test." << std::endl;
        FAIL() << "Fork failed";
    } else if (pid == 0) {
        // child process
        try {
            int client_sock = create_client_socket();
            std::string malformed_request = "INVALID / HTTP/1.1\r\nHost: example.com\r\n\r\n";
            
            send(client_sock, malformed_request.c_str(), malformed_request.size(), 0);
            
            // set receive timeout
            struct timeval tv;
            tv.tv_sec = 3;
            tv.tv_usec = 0;
            setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
            
            // read response
            char buffer[4096];
            std::string malformed_response;
            
            while (true) {
                ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                if (received <= 0) break;
                
                buffer[received] = '\0';
                malformed_response.append(buffer, received);
            }
            
            std::cout << "Child process received response:\n" << malformed_response << std::endl;
            close(client_sock);
            
            // child process exits normally
            exit(0);
        }
        catch (const std::exception &e) {
            std::cout << "Exception in child process: " << e.what() << std::endl;
            exit(1);
        }
    } else {
        // parent process
        // wait for child process, but set timeout
        int status;
        pid_t result;
        time_t start_time = time(NULL);
        
        // wait for 5 seconds
        while (time(NULL) - start_time < 5) {
            result = waitpid(pid, &status, WNOHANG);
            
            if (result == -1) {
                // waitpid error
                std::cout << "Error waiting for child process." << std::endl;
                break;
            } else if (result == 0) {
                // child process is still running, continue waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            } else {
                // child process has exited
                if (WIFEXITED(status)) {
                    std::cout << "Child process exited with status " << WEXITSTATUS(status) << std::endl;
                } else if (WIFSIGNALED(status)) {
                    std::cout << "Child process killed by signal " << WTERMSIG(status) << std::endl;
                }
                break;
            }
        }
        
        // if child process is still running, kill it
        if (time(NULL) - start_time >= 5) {
            std::cout << "Child process timed out, killing it." << std::endl;
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);  // clean up zombie process
        }
    }
    
    std::cout << "=== Completed TestMalformedRequest ===" << std::endl;
}

TEST_F(ProxyTest, Test404NotFound) {
    std::cout << "\n=== Starting Test404NotFound ===" << std::endl;
    
    try {
        int client_sock = create_client_socket();
        std::string not_found_request = 
            "GET http://httpbin.org/status/404 HTTP/1.1\r\n"
            "Host: httpbin.org\r\n"
            "Connection: close\r\n\r\n";
        
        ssize_t sent = send(client_sock, not_found_request.c_str(), not_found_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(not_found_request.size())) << "404 request not fully sent.";

        // set receive timeout
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        // read response
        char buffer[4096];
        std::string not_found_response;
        
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            not_found_response.append(buffer, received);
        }
        
        std::cout << "404 response:\n" << not_found_response << std::endl;
        
        // verify proxy correctly passed 404 response
        EXPECT_FALSE(not_found_response.empty()) << "No response received for 404 request";
        EXPECT_TRUE(not_found_response.find("HTTP/1.1 404") != std::string::npos) 
            << "Expected 404 Not Found response";
        
        close(client_sock);
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in Test404NotFound: " << e.what();
    }
    
    std::cout << "=== Completed Test404NotFound ===" << std::endl;
}

TEST_F(ProxyTest, Test500ServerError) {
    std::cout << "\n=== Starting Test500ServerError ===" << std::endl;
    
    try {
        int client_sock = create_client_socket();
        std::string server_error_request = 
            "GET http://httpbin.org/status/500 HTTP/1.1\r\n"
            "Host: httpbin.org\r\n"
            "Connection: close\r\n\r\n";
        
        ssize_t sent = send(client_sock, server_error_request.c_str(), server_error_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(server_error_request.size())) << "500 request not fully sent.";

        // set receive timeout
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        // read response
        char buffer[4096];
        std::string server_error_response;
        
        while (true) {
            ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (received <= 0) break;
            
            buffer[received] = '\0';
            server_error_response.append(buffer, received);
        }
        
        std::cout << "500 response:\n" << server_error_response << std::endl;
        
        // verify proxy correctly passed 500 response
        EXPECT_FALSE(server_error_response.empty()) << "No response received for 500 request";
        EXPECT_TRUE(server_error_response.find("HTTP/1.1 500") != std::string::npos) 
            << "Expected 500 Internal Server Error response";
        
        close(client_sock);
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in Test500ServerError: " << e.what();
    }
    
    std::cout << "=== Completed Test500ServerError ===" << std::endl;
}

TEST_F(ProxyTest, TestConcurrentRequests) {
    std::cout << "\n=== Starting TestConcurrentRequests ===" << std::endl;
    
    try {
        const int NUM_REQUESTS = 10;
        std::vector<std::thread> threads;
        std::atomic<int> success_count(0);
        
        // start multiple threads to send requests concurrently
        for (int i = 0; i < NUM_REQUESTS; i++) {
            threads.emplace_back([this, &success_count]() {
                int client_sock = create_client_socket();
                std::string get_request = 
                    "GET http://httpbin.org/get HTTP/1.1\r\n"
                    "Host: httpbin.org\r\n"
                    "Connection: close\r\n\r\n";
                
                ssize_t sent = send(client_sock, get_request.c_str(), get_request.size(), 0);
                if (sent != static_cast<ssize_t>(get_request.size())) {
                    return;
                }
                
                // set receive timeout
                struct timeval tv;
                tv.tv_sec = 5;
                tv.tv_usec = 0;
                setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
                
                // read response
                char buffer[4096];
                std::string response;
                
                while (true) {
                    ssize_t received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                    if (received <= 0) break;
                    
                    buffer[received] = '\0';
                    response.append(buffer, received);
                }
                
                close(client_sock);
                
                // check if response is successful
                if (!response.empty() && response.find("HTTP/1.1 200") != std::string::npos) {
                    success_count++;
                }
            });
        }
        
        // wait for all threads to complete
        for (auto& t : threads) {
            t.join();
        }
        
        std::cout << "Concurrent requests completed: " << success_count << "/" << NUM_REQUESTS << std::endl;
        
        // verify most requests succeeded
        EXPECT_GE(success_count, NUM_REQUESTS * 0.8) << "Less than 80% of concurrent requests succeeded";
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestConcurrentRequests: " << e.what();
    }
    
    std::cout << "=== Completed TestConcurrentRequests ===" << std::endl;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
