cmake_minimum_required(VERSION 3.16)
project(typed_decode LANGUAGES C CXX)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp/CMakeLists.txt")
    message(FATAL_ERROR "Missing llama.cpp. Run: git submodule update --init --recursive")
endif()

set(LLAMA_BUILD_COMMON OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_APP OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
add_subdirectory(llama.cpp)

add_library(typed_prefix src/typed_prefix.cpp)
target_include_directories(typed_prefix PUBLIC src)
target_compile_features(typed_prefix PUBLIC cxx_std_17)

add_executable(typed-prefix-test tests/typed_prefix_test.cpp)
target_link_libraries(typed-prefix-test PRIVATE typed_prefix)

add_executable(typed-llama src/main.cpp src/typed_sampler.cpp)
target_link_libraries(typed-llama PRIVATE typed_prefix llama)
target_compile_features(typed-llama PRIVATE cxx_std_17)

enable_testing()
add_test(NAME typed-prefix-test COMMAND typed-prefix-test)
