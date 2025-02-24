#include "Parser.hpp"

Request Parser::parseRequest(const std::vector<char> & data){
    
    http::request_parser<http::string_body> parser;
    http::request<http::string_body> request;
    beast::flat_buffer dynamic_buffer;
    boost::system::error_code ec;

    while (true) {

        // append data to buffer
        dynamic_buffer.commit(
            boost::asio::buffer_copy(
                dynamic_buffer.prepare(data.size()),  
                boost::asio::buffer(data.data(), data.size())  
            )
        ); 

        // try to parse data
        parser.put(dynamic_buffer.data(), ec);
        dynamic_buffer.consume(dynamic_buffer.size()); // clear processed data

        if (ec) {
            if (ec == http::error::need_more) {
                logger.debug("need more");
                continue;
            } else {
                logger.error("failed to parse data, ec:"+ec.message()+" code: "+to_string(ec.value()));
                break;
            }
        }

        // get all data
        if (parser.is_done()) {
            request = parser.release();
            logger.info("successfully parsed request method("
                            +request.method_string().to_string()+") bodyLen("+to_string(request.body().size())+")");
            // logger.debug(full_response);
            Request req(request);
            return req;
        }
    }
    throw std::runtime_error("failed to parse data, ec:"+ec.message()+" code: "+to_string(ec.value()));
}