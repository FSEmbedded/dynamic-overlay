include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

# Prevent GTest from overriding compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Re-enable exceptions and RTTI for test code — gtest requires both
set(TEST_COMPILE_OPTIONS -Wall -Wextra -Wpedantic)

set(TEST_SOURCES
    test_error.cpp
    test_string_utils.cpp
    test_ini_parser.cpp
    test_posix_utils.cpp
    test_overlay_description.cpp
    test_device_parser.cpp
    test_dynamic_mounting_helpers.cpp
    test_scope_guard.cpp
    test_overlay_config.cpp
)

add_executable(dynamic_overlay_tests ${TEST_SOURCES})

target_compile_features(dynamic_overlay_tests PRIVATE cxx_std_17)
set_target_properties(dynamic_overlay_tests PROPERTIES CXX_EXTENSIONS OFF)

target_compile_options(dynamic_overlay_tests PRIVATE ${TEST_COMPILE_OPTIONS})

target_include_directories(dynamic_overlay_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)

# Build only the testable source files (no main.cpp, no hardware-dependent modules)
target_sources(dynamic_overlay_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/posix_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/ini_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/device_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/overlay_config.cpp
)

target_compile_definitions(dynamic_overlay_tests PRIVATE ENABLE_LOGGING)

target_link_libraries(dynamic_overlay_tests PRIVATE GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(dynamic_overlay_tests)
