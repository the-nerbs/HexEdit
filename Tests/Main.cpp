#include "Stdafx.h"

#include "utils/CoInitialize.h"

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

int main(int argc, char* argv[])
{
    CCoInitialize com;

    if (FAILED(com.m_hr))
    {
        return EXIT_FAILURE;
    }

    int result = Catch::Session().run(argc, argv);

    // global clean-up...

    return result;
}
