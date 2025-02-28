#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/lexical_cast.hpp>
#include "Logger.hpp"
#include "Request.hpp"

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace asio = boost::asio;


class Response {
private:
    int id;
    http::fields headers;
    std::string body;
    static inline Logger & logger = Logger::getInstance();
    http::response<http::string_body> response;
    std::string requestStr;

public:
    Response() : id(-1){}
    Response(http::response<http::string_body> res):id(id),response(res),headers(res.base()),body(res.body()){}

    int getId() const { return id; }
    
    // std::string getMethod() const { return request.method_string().to_string(); }
    // std::string getUrl() const { return request.target().to_string(); }
    std::string getVersion() const { 
        return "HTTP/" 
        + std::to_string(response.version() / 10) 
        + "." 
        + std::to_string(response.version() % 10); 
    }
    std::string getHeader(const std::string& key) const;

    // bool isGet() const { return getMethod() == "GET"; }
    // bool isPost() const { return getMethod() == "POST"; }
    // bool isConnect() const { return getMethod() == "CONNECT"; }

    const http::fields & getHeaders() const { return headers; }
    bool hasBody() const { return !body.empty(); }
    std::string getHeadersStr(){return boost::lexical_cast<std::string>(response.base());}
    std::string getBody() const { return body; }
    int getResult() const { return response.result_int();}
};

#endif

