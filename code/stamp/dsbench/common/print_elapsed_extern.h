#pragma once

#include <chrono>
#include <stdio.h>
extern std::chrono::time_point<std::chrono::high_resolution_clock> ____executionStartTimeDebug;
// #define PRINT_ELAPSED_FROM_START() { printf("%s:%d %dms\n", __FILE__, __LINE__, (int) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - ____executionStartTimeDebug).count()); }
#define PRINT_ELAPSED_FROM_START()
