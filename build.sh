#!/bin/env bash
echo "Check deps.sh for packages needed for build"

echo "Compiling...."
g++ \
        src/main.cpp \
        src/refrend_base.cpp \
        src/refsw_pixel.cpp \
        src/refsw.cpp \
        src/pvr_regs.cpp \
        src/pvr_mem.cpp \
        src/TexCache.cpp \
        src/TexCacheEntry.cpp \
    -DTARGET_LINUX_x64 -DREFSW_OFFLINE -DFEAT_HAS_SOFTREND -DFEAT_TA=0x60000002 \
    -lpng -lz \
    -g -O0 \
    -o refsw-offline
