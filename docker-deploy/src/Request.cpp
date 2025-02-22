#include "Request.hpp"
#include <sstream>
#include <algorithm>

std::atomic<int> Request::next_id(0);

bool Request::parse(const std::vector<char>& data) {
    try {
        std::string request_str(data.begin(), data.end());
        std::istringstream request_stream(request_str);
        std::string line;
        
        // Parse request line
        if (!std::getline(request_stream, line)) {
            return false;
        }
        
        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // Parse method, URL and version
        std::istringstream line_stream(line);
        if (!(line_stream >> method >> url >> version)) {
            return false;
        }
        
        // Validate HTTP version
        if (version.substr(0, 5) != "HTTP/") {
            return false;
        }
        
        // Parse headers
        headers.clear();
        while (std::getline(request_stream, line) && line != "\r" && line != "") {
            if (line.back() == '\r') {
                line.pop_back();
            }
            
            size_t colon_pos = line.find(':');
            if (colon_pos == std::string::npos) {
                continue;
            }
            
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // Trim leading/trailing whitespace
            value.erase(0, value.find_first_not_of(" "));
            value.erase(value.find_last_not_of(" ") + 1);
            
            // Convert header name to proper case (e.g., "Content-Length")
            std::string proper_key = key;
            bool capitalize = true;
            for (char& c : proper_key) {
                if (capitalize) {
                    c = std::toupper(c);
                    capitalize = false;
                } else if (c == '-') {
                    capitalize = true;
                } else {
                    c = std::tolower(c);
                }
            }
            
            headers[proper_key] = value;
        }
        
        // Parse body if Content-Length is present
        body.clear();
        auto content_length_it = headers.find("Content-Length");
        if (content_length_it != headers.end()) {
            size_t content_length = std::stoul(content_length_it->second);
            
            // Find the start of the body (after \r\n\r\n)
            size_t body_start = request_str.find("\r\n\r\n");
            if (body_start != std::string::npos) {
                body_start += 4;  // Skip \r\n\r\n
                
                // Copy body data
                if (body_start < request_str.length()) {
                    body.assign(data.begin() + body_start,
                              data.begin() + std::min(body_start + content_length,
                                                    request_str.length()));
                }
            }
        }
        
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

std::string Request::getHeader(const std::string& key) const {
    auto it = headers.find(key);
    return (it != headers.end()) ? it->second : "";
}
