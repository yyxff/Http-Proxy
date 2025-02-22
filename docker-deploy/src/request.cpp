#include "request.hpp"
#include <sstream>
#include <iostream>

std::atomic<int> Request::next_id(0);

bool Request::parse(const std::vector<char>& data) {
    std::string raw_data(data.begin(), data.end());
    std::istringstream stream(raw_data);
    std::string line;

    if (!std::getline(stream, line)) {
        return false;
    }
    
    std::istringstream request_line(line);
    request_line >> method >> url >> version;
    
    while (std::getline(stream, line) && line != "\r") {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 2); // Skip ": "
            headers[key] = value;
        }
    }
    
    return true;
}

std::string Request::getHeader(const std::string& key) const {
    auto it = headers.find(key);
    if (it != headers.end()) {
        return it->second;
    }
    return "";
}

