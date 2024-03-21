cmake_minimum_required(VERSION 3.16)

project(refsw-offline)

enable_language(C ASM CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)

list(APPEND shared_src
    src/refsw_lists.cpp
    src/refsw_tile.cpp
    src/pvr_regs.cpp
    src/pvr_mem.cpp
    src/TexCache.cpp
    src/TexCacheEntry.cpp
)

set(shared_defines -DTARGET_LINUX_x64 -DREFSW_OFFLINE -DFEAT_TA=0x60000002)
set(shared_libs -lpng -lz)
set(shared_includes src .)

add_library(refsw OBJECT ${shared_src})
target_compile_definitions(refsw PUBLIC ${shared_defines})
target_include_directories(refsw PUBLIC ${shared_includes})
target_link_libraries(refsw PUBLIC ${shared_libs})


add_executable(refsw-offline tools/refsw-offline.cpp)
target_link_libraries(refsw-offline refsw)

add_executable(refsw-tests tools/refsw-tests.cpp)
target_link_libraries(refsw-tests refsw)