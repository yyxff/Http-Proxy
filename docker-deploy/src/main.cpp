#include "proxy.hpp"

int main() {
    try {
        Logger& logger = Logger::getInstance();
        logger.setLogPath("/var/log/erss/proxy.log");
        logger.info("starting proxy...");
        
        Proxy proxy(12345);
        proxy.run();
        
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
