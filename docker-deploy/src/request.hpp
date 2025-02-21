#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <vector>
#include <map>

class Request {
private:
    std::string method;
    std::string url;
    std::string version;
    std::map<std::string, std::string> headers;
    std::vector<char> body;

public:
    Request() {}
    
    bool parse(const std::vector<char>& data);

    std::string getMethod() const { return method; }
    std::string getUrl() const { return url; }
    std::string getVersion() const { return version; }
    std::string getHeader(const std::string& key) const;

    bool isGet() const { return method == "GET"; }
    bool isPost() const { return method == "POST"; }
    bool isConnect() const { return method == "CONNECT"; }
};

#endif

