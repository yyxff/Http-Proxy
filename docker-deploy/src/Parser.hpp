#ifndef PARSER_HPP
#define PARSER_HPP

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include "Logger.hpp"
#include "Request.hpp"
#include "Response.hpp"

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace asio = boost::asio;

class Parser{
    public:
    
        Request parseRequest(const std::vector<char> & data);
        Response parseResponse(const std::vector<char> & data);

    private:

        static inline Logger & logger = Logger::getInstance();

};

#endif
