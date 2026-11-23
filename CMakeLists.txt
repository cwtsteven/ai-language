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

# Embed the one language specification in both model prompts at configure time.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS docs/language.md)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/docs/language.md" LANGUAGE_SPEC)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(src/language_spec.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/language_spec.h" @ONLY)
add_library(language_spec INTERFACE)
target_include_directories(language_spec INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/generated")

add_library(typed_prefix src/typed_prefix.cpp src/pretty_printer.cpp)
target_include_directories(typed_prefix PUBLIC src)
target_compile_features(typed_prefix PUBLIC cxx_std_17)

add_executable(typed-prefix-test tests/typed_prefix_test.cpp)
target_link_libraries(typed-prefix-test PRIVATE typed_prefix)

add_library(model_plugin src/model_plugin.cpp)
target_include_directories(model_plugin PUBLIC src PRIVATE llama.cpp/vendor)
target_link_libraries(model_plugin PUBLIC llama)
target_compile_features(model_plugin PUBLIC cxx_std_17)

add_library(cli_options src/cli.cpp)
target_include_directories(cli_options PUBLIC src)
target_compile_features(cli_options PUBLIC cxx_std_17)

add_library(cj_codegen src/cj_codegen.cpp)
target_link_libraries(cj_codegen PUBLIC typed_prefix)
target_compile_features(cj_codegen PUBLIC cxx_std_17)

add_library(token_sampling src/sampling.cpp src/typed_sampler.cpp)
target_link_libraries(token_sampling PUBLIC typed_prefix llama)
target_compile_features(token_sampling PUBLIC cxx_std_17)

add_executable(typed-llama src/main.cpp)
target_link_libraries(typed-llama PRIVATE token_sampling model_plugin cli_options language_spec cj_codegen)
target_compile_features(typed-llama PRIVATE cxx_std_17)

enable_testing()
add_test(NAME typed-prefix-test COMMAND typed-prefix-test)
add_executable(functions-test tests/functions_test.cpp)
target_link_libraries(functions-test PRIVATE typed_prefix)
add_test(NAME functions-test COMMAND functions-test)

add_executable(pretty-printer-test tests/pretty_printer_test.cpp)
target_link_libraries(pretty-printer-test PRIVATE typed_prefix)
add_test(NAME pretty-printer-test COMMAND pretty-printer-test)

add_executable(model-plugin-test tests/model_plugin_test.cpp)
target_link_libraries(model-plugin-test PRIVATE model_plugin typed_prefix language_spec)
add_test(NAME model-plugin-test COMMAND model-plugin-test)
add_executable(cli-options-test tests/cli_options_test.cpp)
target_link_libraries(cli-options-test PRIVATE cli_options)
add_test(NAME cli-options-test COMMAND cli-options-test)
add_executable(planning-test tests/planning_test.cpp)
target_include_directories(planning-test PRIVATE src)
target_link_libraries(planning-test PRIVATE language_spec)
target_compile_definitions(planning-test PRIVATE LANGUAGE_SPEC_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/docs/language.md")
target_compile_features(planning-test PRIVATE cxx_std_17)
add_test(NAME planning-test COMMAND planning-test)
add_executable(sampling-test tests/sampling_test.cpp)
target_link_libraries(sampling-test PRIVATE token_sampling)
add_test(NAME sampling-test COMMAND sampling-test)
add_executable(cj-codegen-test tests/cj_codegen_test.cpp)
target_link_libraries(cj-codegen-test PRIVATE cj_codegen)
add_test(NAME cj-codegen-test COMMAND cj-codegen-test)
find_program(CANGJIE_COMPILER NAMES cjc)
if(CANGJIE_COMPILER)
    add_test(NAME cj-runtime-test COMMAND ${CMAKE_COMMAND}
        -DCLI=$<TARGET_FILE:typed-llama>
        -DCJC=${CANGJIE_COMPILER}
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cj-runtime-test
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cj_runtime_test.cmake)
    set_tests_properties(cj-runtime-test PROPERTIES TIMEOUT 120)
endif()
add_test(NAME cli-test COMMAND ${CMAKE_COMMAND}
    -DCLI=$<TARGET_FILE:typed-llama>
    -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-test
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.cmake)
