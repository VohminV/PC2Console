cmake_minimum_required(VERSION 3.20)
project(PC2Console VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Host toolchain only: modern C++20 compiler (MSVC 19+/GCC 11+/Clang 13+).
# The Xbox 360 XDK compiler is NEVER used to build pc2c itself; it is only
# probed (filesystem-level) by the xbox360 backend.

if(MSVC)
  add_compile_options(/W4 /permissive- /EHsc)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

set(PC2C_LIB_SOURCES
  src/core/Logger.cpp
  src/core/Fs.cpp
  src/core/Hash.cpp
  src/core/Json.cpp
  src/discovery/Scanner.cpp
  src/analysis/Pe.cpp
  src/analysis/Inventory.cpp
  src/assets/Image.cpp
  src/assets/Profiles.cpp
  src/assets/Converter.cpp
  src/portability/ApiDb.cpp
  src/portability/Plan.cpp
  src/intermediate/Model.cpp
  src/build/Toolchain.cpp
  src/backends/Backend.cpp
  src/backends/Targets.cpp
  src/cli/App.cpp
  src/test/Suite.cpp
)

add_library(pc2c_lib STATIC ${PC2C_LIB_SOURCES})
target_include_directories(pc2c_lib PUBLIC include)

add_executable(pc2c apps/pc2c-cli/main.cpp)
target_link_libraries(pc2c PRIVATE pc2c_lib)

add_executable(pc2c-tests tests/test_runner.cpp)
target_link_libraries(pc2c-tests PRIVATE pc2c_lib)

enable_testing()
add_test(NAME pc2c-suite COMMAND pc2c-tests)
add_test(NAME pc2c-cli-test COMMAND pc2c test)
