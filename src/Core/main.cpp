#include <iostream>

#include "Core/Application.h"

int main()
{
    try {
        std::cout << "Hello, New engine!" << std::endl;
        Engine::Application app {};
        app.run();
    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
