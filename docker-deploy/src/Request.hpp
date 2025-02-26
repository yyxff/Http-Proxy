#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include "Logger.hpp"

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace asio = boost::asio;


class Request {
private:
    static std::atomic<int> next_id;
    int id;
    http::fields headers;
    std::string body;
    static inline Logger & logger = Logger::getInstance();
    http::request<http::string_body> request;
    std::string requestStr;

public:
    Request() : id(-1){}
    Request(http::request<http::string_body> req):id(next_id++),request(req),headers(req.base()),body(req.body()){}

    int getId() const { return id; }
    
    std::string getMethod() const { return request.method_string().to_string(); }
    std::string getUrl() const { return request.target().to_string(); }
    std::string getVersion() const { 
        return "HTTP/" 
        + std::to_string(request.version() / 10) 
        + "." 
        + std::to_string(request.version() % 10); 
    }
    std::string getHeader(const std::string& key) const;

    bool isGet() const { return getMethod() == "GET"; }
    bool isPost() const { return getMethod() == "POST"; }
    bool isConnect() const { return getMethod() == "CONNECT"; }

    const http::fields & getHeaders() const { return headers; }
    bool hasBody() const { return !body.empty(); }
    std::string getBody() const { return body; }
};

#endif

