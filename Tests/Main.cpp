#include "Stdafx.h"

#include "utils/CoInitialize.h"

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <chrono>
#include <iostream>

int main(int argc, char* argv[])
{
    CCoInitialize com;

    if (FAILED(com.m_hr))
    {
        return EXIT_FAILURE;
    }

    using clock = std::chrono::steady_clock;
    clock::time_point start = clock::now();

    int result = Catch::Session().run(argc, argv);

    clock::time_point end = clock::now();
    clock::duration time = end - start;
    auto timeMS = std::chrono::duration_cast<std::chrono::milliseconds>(time);
    std::cout << "Tests took " << timeMS.count() << " ms\n\n";

    // global clean-up...

    return result;
}
