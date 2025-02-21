#include "proxy.hpp"
#include "Logger.hpp"
#include <iostream>

int main() {
    Logger logger("./../logs");
    try {
        Proxy proxy;
        logger.info("starting proxy...");
        proxy.run();
    }
    catch (const std::exception & e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
