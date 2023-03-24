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

    Catch::Session session{};
    session.applyCommandLine(argc, argv);

    int result = session.run();

    const Catch::Config& config = session.config();

    // if we're called for discovering tests, then print out the discovery results.
    if (!config.listTests()
        && !config.listTags()
        && !config.listReporters()
        && !config.listListeners())
    {
        clock::time_point end = clock::now();
        clock::duration time = end - start;
        auto timeMS = std::chrono::duration_cast<std::chrono::milliseconds>(time);
        std::cout << "Tests took " << timeMS.count() << " ms\n\n";
    }

    // global clean-up...

    return result;
}
