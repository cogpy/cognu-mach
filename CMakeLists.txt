cmake_minimum_required(VERSION 3.10)
project(cogpy-hpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only library: INTERFACE target
# Note: The top-level include/ directory also contains GNU Mach kernel
# headers (mach/, device/, sys/, string.h) that conflict with host
# system headers. For the cogpy-hpp C++ library tests, we add a
# separate include path that only exposes the cog/ namespace headers.
add_library(cog INTERFACE)
target_include_directories(cog INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cog/..>
    $<INSTALL_INTERFACE:include>
)

# Prevent Mach kernel headers from shadowing system headers by
# ensuring the cog include path is searched AFTER system paths.
# We achieve this by using a wrapper include directory.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cog_include)
if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/cog_include/cog)
    file(CREATE_LINK
        ${CMAKE_CURRENT_SOURCE_DIR}/include/cog
        ${CMAKE_CURRENT_BINARY_DIR}/cog_include/cog
        SYMBOLIC)
endif()

# Override the include path to use only the symlinked cog/ directory
set_target_properties(cog PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "")
target_include_directories(cog INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/cog_include>
    $<INSTALL_INTERFACE:include>
)

# Test executable
add_executable(test_all test/test_all.cpp)
target_link_libraries(test_all PRIVATE cog)

enable_testing()
add_test(NAME cogpy_tests COMMAND test_all)

# Install headers
install(DIRECTORY include/cog DESTINATION include)
