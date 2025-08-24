#include "Request.hpp"
#include <sstream>
#include <algorithm>

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace asio = boost::asio;

std::atomic<int> Request::next_id(0);


std::string Request::getHeader(const std::string& key) const {
    auto it = headers.find(key);
    return (it != headers.end()) ? std::string(it->value()) : "";
}
