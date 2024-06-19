#include <iostream>

#include "VkEngine.hpp"
#include "Bassicraft.hpp"

int main(int argc, char* argv[]) {
    try {
        Bassicraft game;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 84;
    }
}