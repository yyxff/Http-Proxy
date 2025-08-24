#include "Response.hpp"
#include <sstream>
#include <algorithm>

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace asio = boost::asio;



std::string Response::getHeader(const std::string& key) const {
    auto it = headers.find(key);
    return (it != headers.end()) ? std::string(it->value()) : "";
}
