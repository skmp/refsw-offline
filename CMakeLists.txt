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
set(shared_includes src . ${CMAKE_CURRENT_BINARY_DIR}/autogen)


list(APPEND tests 
    1st_tri
    2nd_tri
    big_tri
)

list(APPEND offline_src 
    tools/refsw-offline.cpp
)

####### boilerplate and automation from now on ######
add_library(refsw OBJECT ${shared_src})
target_compile_definitions(refsw PUBLIC ${shared_defines})
target_include_directories(refsw PUBLIC ${shared_includes})
target_link_libraries(refsw PUBLIC ${shared_libs})

add_executable(refsw-offline ${offline_src})
target_link_libraries(refsw-offline refsw)

set(test_src "")

foreach(test IN LISTS tests)
    list(APPEND test_src "test/cases/${test}.cpp")
    set_property(
        SOURCE test/cases/${test}.cpp
        PROPERTY COMPILE_DEFINITIONS
        testCase=test_${test}
    )
endforeach()

set(file_test_decl "${CMAKE_CURRENT_BINARY_DIR}/autogen/gen/test_decl.inl")

file(WRITE ${file_test_decl} "")

foreach(test IN LISTS tests)
    file(APPEND ${file_test_decl} "void test_${test}();")
endforeach()

set(file_test_list "${CMAKE_CURRENT_BINARY_DIR}/autogen/gen/test_list.inl")

file(WRITE ${file_test_list} "")

foreach(test IN LISTS tests)
    file(APPEND ${file_test_list} "{\"${test}\", &test_${test}},")
endforeach()

add_executable(refsw-tests tools/refsw-tests.cpp ${test_src})
target_link_libraries(refsw-tests refsw)