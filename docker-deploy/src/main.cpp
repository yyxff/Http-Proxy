#include "proxy.hpp"
#include <iostream>

int main() {
    try {
        Proxy proxy;
        proxy.run();
    }
    catch (const std::exception & e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
