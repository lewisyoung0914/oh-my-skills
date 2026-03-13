cmake_minimum_required(VERSION 3.24)
project(skillctl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  # Treat all source files as UTF-8 to avoid code page issues with non-ASCII
  add_compile_options(/utf-8)
endif()

option(SKILLCTL_BUILD_TESTS "Build skillctl tests" ON)

find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)

add_library(skillctl_core
  src/cli/runtime.cpp
  src/config/config.cpp
  src/coord/coord.cpp
  src/frontmatter/frontmatter.cpp
  src/security/token_store.cpp
  src/api/api_client.cpp
  src/error/error.cpp
  src/output/output.cpp
  src/util/fs.cpp
  src/util/http.cpp
  src/util/strings.cpp
)

target_include_directories(skillctl_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(skillctl_core
  PUBLIC
    nlohmann_json::nlohmann_json
    yaml-cpp::yaml-cpp
    CURL::libcurl
)

if(WIN32)
  target_compile_definitions(skillctl_core PRIVATE NOMINMAX)
  target_link_libraries(skillctl_core PRIVATE crypt32)
endif()

add_executable(skillctl apps/skillctl/main.cpp)
target_link_libraries(skillctl PRIVATE skillctl_core CLI11::CLI11)

if(SKILLCTL_BUILD_TESTS)
  enable_testing()
  find_package(Catch2 3 CONFIG REQUIRED)
  find_package(httplib CONFIG REQUIRED)
  add_executable(skillctl_tests
    tests/test_coord.cpp
    tests/test_frontmatter.cpp
    tests/test_config.cpp
    tests/test_security.cpp
    tests/test_error.cpp
    tests/test_output.cpp
    tests/test_api_integration.cpp
  )
  target_link_libraries(skillctl_tests PRIVATE skillctl_core Catch2::Catch2WithMain httplib::httplib)
  add_test(NAME skillctl_tests COMMAND skillctl_tests)
endif()

