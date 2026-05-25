#include <iostream>
#include "Window.h"

int main()
{
        try
        {
                std::cout << "Hello, New engine!" << std::endl;
        }
        catch (std::exception& e)
        {
                std::cout << e.what() << std::endl;
                return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
}
