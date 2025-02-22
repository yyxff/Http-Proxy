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
#include <cstdio>

class ProxyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 指定测试日志路径，避免和生产日志混在一起
        std::string test_log_path = "test_logs/test_proxy.log";
        Logger::getInstance().setLogPath(test_log_path);
        
        // 创建一个代理实例，端口设为 0 让系统自动分配
        proxy = std::make_unique<Proxy>(0);
        
        // 启动代理线程
        proxy_thread = std::thread([this]() {
            proxy->run();
        });
        // 稍微等一下，保证代理线程已启动并监听端口
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 获取代理实际端口
        proxy_port = proxy->getPort();
    }

    void TearDown() override {
        // 停止代理
        proxy->stop();  
        if (proxy_thread.joinable()) {
            proxy_thread.join();
        }
        
        // 杀掉可能残留的 python http.server
        system("pkill -f 'python3 -m http.server 8080' 2>/dev/null || true");
        
        // 给一点时间让资源释放
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 创建并连接到代理的客户端 socket
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
    
    // 启动本地 Python HTTP 服务器
    std::string start_server_cmd = "python3 -m http.server 8080 > /dev/null 2>&1 & echo $!";
    FILE* pipe = popen(start_server_cmd.c_str(), "r");
    ASSERT_TRUE(pipe != nullptr) << "Failed to start python3 -m http.server";
    
    char buffer[128];
    std::string python_pid;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        python_pid += buffer;
    }
    pclose(pipe);
    python_pid = python_pid.substr(0, python_pid.find_first_of("\n\r"));
    std::cout << "Started python http.server on 8080 (PID=" << python_pid << ")" << std::endl;
    
    // 等待服务器就绪
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 构建 GET 请求
    try {
        int client_sock = create_client_socket();
        std::string get_request = 
            "GET http://127.0.0.1:8080/ HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Connection: close\r\n\r\n";
        
        // 发送请求
        ssize_t sent = send(client_sock, get_request.c_str(), get_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(get_request.size())) << "GET request not fully sent.";
        
        // 设置读超时，避免死等
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        
        // 接收响应
        char recv_buf[4096];
        ssize_t received = recv(client_sock, recv_buf, sizeof(recv_buf), 0);
        EXPECT_GT(received, 0) << "No data received for GET.";
        
        std::string response(recv_buf, received);
        std::cout << "GET response:\n" << response << std::endl;
        
        // 简单判断响应码是否含有 "200"
        EXPECT_TRUE(response.find("200") != std::string::npos) 
            << "Expected '200' in response, got:\n" << response;
        
        close(client_sock);
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestGet: " << e.what();
    }

    // 清理 python 服务器
    if (!python_pid.empty()) {
        std::string kill_cmd = "kill " + python_pid + " 2>/dev/null || true";
        system(kill_cmd.c_str());
        std::cout << "Stopped python http.server (PID=" << python_pid << ")\n";
    }
    
    std::cout << "=== Completed TestGet ===" << std::endl;
}

// ============== Test #2: POST ==============
TEST_F(ProxyTest, TestPost) {
    std::cout << "\n=== Starting TestPost ===" << std::endl;
    
    // 启动本地 Python HTTP 服务器
    std::string start_server_cmd = "python3 -m http.server 8080 > /dev/null 2>&1 & echo $!";
    FILE* pipe = popen(start_server_cmd.c_str(), "r");
    ASSERT_TRUE(pipe != nullptr) << "Failed to start python3 -m http.server";
    
    char buffer[128];
    std::string python_pid;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        python_pid += buffer;
    }
    pclose(pipe);
    python_pid = python_pid.substr(0, python_pid.find_first_of("\n\r"));
    std::cout << "Started python http.server on 8080 (PID=" << python_pid << ")" << std::endl;
    
    // 等待服务器就绪
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 构建 POST 请求
    try {
        int client_sock = create_client_socket();
        std::string body = "test=data";
        std::string post_request =
            "POST http://127.0.0.1:8080/demo HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "Connection: close\r\n\r\n" + 
            body;
        
        // 发送请求
        ssize_t sent = send(client_sock, post_request.c_str(), post_request.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(post_request.size())) << "POST request not fully sent.";
        
        // 设置读超时
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        
        // 接收响应
        char recv_buf[4096];
        ssize_t received = recv(client_sock, recv_buf, sizeof(recv_buf), 0);
        EXPECT_GT(received, 0) << "No data received for POST.";
        
        std::string response(recv_buf, received);
        std::cout << "POST response:\n" << response << std::endl;
        
        // Python http.server 对 POST 请求默认返回 501，但也许是 200；只要不挂死就是通过
        // 如果你想要求它一定是 501 或 200 可以写: find("501") != npos || find("200") != npos
        EXPECT_TRUE(
            response.find("200") != std::string::npos ||
            response.find("501") != std::string::npos
        ) << "Expected '200' or '501' in response, got:\n" << response;
        
        close(client_sock);
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestPost: " << e.what();
    }

    // 清理 python 服务器
    if (!python_pid.empty()) {
        std::string kill_cmd = "kill " + python_pid + " 2>/dev/null || true";
        system(kill_cmd.c_str());
        std::cout << "Stopped python http.server (PID=" << python_pid << ")\n";
    }
    
    std::cout << "=== Completed TestPost ===" << std::endl;
}

// ============== Test #3: CONNECT ==============
// 注意：python -m http.server 并不会真的处理 CONNECT。
// 我们只验证：1) 代理返回 "HTTP/1.1 200 Connection established"；
//            2) 代理成功建立隧道后，用 GET 请求能拿到响应(或至少不阻塞)。
TEST_F(ProxyTest, TestConnect) {
    std::cout << "\n=== Starting TestConnect ===" << std::endl;
    
    // 启动本地 Python HTTP 服务器
    std::string start_server_cmd = "python3 -m http.server 8080 > /dev/null 2>&1 & echo $!";
    FILE* pipe = popen(start_server_cmd.c_str(), "r");
    ASSERT_TRUE(pipe != nullptr) << "Failed to start python3 -m http.server";
    
    char buffer[128];
    std::string python_pid;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        python_pid += buffer;
    }
    pclose(pipe);
    python_pid = python_pid.substr(0, python_pid.find_first_of("\n\r"));
    std::cout << "Started python http.server on 8080 (PID=" << python_pid << ")" << std::endl;

    // 等待服务器就绪
    std::this_thread::sleep_for(std::chrono::seconds(1));

    try {
        int client_sock = create_client_socket();
        
        // 发送 CONNECT 请求
        std::string connect_req =
            "CONNECT 127.0.0.1:8080 HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n\r\n";
        ssize_t sent = send(client_sock, connect_req.c_str(), connect_req.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(connect_req.size())) << "CONNECT request not fully sent.";
        
        // 设置读超时
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        
        // 接收代理对 CONNECT 的初始响应
        char recv_buf[4096];
        ssize_t received = recv(client_sock, recv_buf, sizeof(recv_buf), 0);
        EXPECT_GT(received, 0) << "No data received for CONNECT.";
        
        std::string resp(recv_buf, received);
        std::cout << "CONNECT initial response:\n" << resp << std::endl;
        EXPECT_TRUE(resp.find("200 Connection established") != std::string::npos)
            << "Proxy did not return 200 Connection established. Got:\n" << resp;
        
        // 现在，代理已经建立隧道，把后续数据转发给 127.0.0.1:8080
        // 我们再发一个普通的 GET
        std::string get_in_tunnel =
            "GET / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8080\r\n"
            "Connection: close\r\n\r\n";
        sent = send(client_sock, get_in_tunnel.c_str(), get_in_tunnel.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(get_in_tunnel.size())) 
            << "Failed to send tunneled GET request.";
        
        // 读取隧道中返回的响应
        received = recv(client_sock, recv_buf, sizeof(recv_buf), 0);
        EXPECT_GT(received, 0) << "No data received for tunneled GET.";
        
        std::string tunneled_response(recv_buf, received);
        std::cout << "CONNECT tunneled GET response:\n" << tunneled_response << std::endl;
        
        // 判断一下是否有 200
        EXPECT_TRUE(tunneled_response.find("200") != std::string::npos ||
                    tunneled_response.find("HTTP/1.0 200 OK") != std::string::npos)
            << "Expected 200 in tunneled GET response:\n" << tunneled_response;
        
        close(client_sock);
    }
    catch (const std::exception &e) {
        FAIL() << "Exception in TestConnect: " << e.what();
    }

    // 清理 python 服务器
    if (!python_pid.empty()) {
        std::string kill_cmd = "kill " + python_pid + " 2>/dev/null || true";
        system(kill_cmd.c_str());
        std::cout << "Stopped python http.server (PID=" << python_pid << ")\n";
    }

    std::cout << "=== Completed TestConnect ===" << std::endl;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
