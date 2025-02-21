#include "proxy.hpp"
#include "Logger.hpp"
#include <iostream>

int main() {
    Logger & logger = Logger::getInstance();
    try {
        Proxy proxy;
        logger.info(1,"starting proxy...");
        proxy.run();
    }
    catch (const std::exception & e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
