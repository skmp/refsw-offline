/*
    This was part of libswirl
*/
//#include <license/bsd>

#include <cstdio>
#include <stdarg.h>

#include <filesystem>

#include "refsw_tile.h"
#include "pvr_mem.h"
#include "pvr_regs.h"


using namespace std;
using namespace std::filesystem;

void InitializePvr() {
    memset(pvr_regs, 0, sizeof(pvr_regs));
    memset(vram, 0, sizeof(vram));
}

#include "test/framework.inl"

#define NAME viewport_limits
#include "test/includer.inl"

struct { const char* name; void (*fn)(); } test_list[] = {
    CASE(viewport_limits),
};

int main(int argc, char **argv)
{
    BuildTwiddleTables();

    for (auto & test_case: test_list) {
        printf("Test Case: %s\n", test_case.name);
    }

    return 0;
}