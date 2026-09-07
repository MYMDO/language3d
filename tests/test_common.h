#pragma once
#include <cstdio>
#include <cstdlib>

#define L3D_REQUIRE(expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "TEST FAILURE: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        std::abort(); \
    } \
} while (0)
