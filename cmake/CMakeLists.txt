cmake_minimum_required(VERSION 3.20)
project(bot CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT DEFINED BOT_DIR OR NOT DEFINED BC_SDK)
  message(FATAL_ERROR "BOT_DIR and BC_SDK must be set")
endif()
# Normalize Windows arguments before embedding paths in generated glob checks.
file(TO_CMAKE_PATH "${BOT_DIR}" BOT_DIR)
file(TO_CMAKE_PATH "${BC_SDK}" BC_SDK)
file(GLOB_RECURSE BOT_SOURCES CONFIGURE_DEPENDS
  "${BOT_DIR}/src/*.cpp" "${BOT_DIR}/src/*.cc"
  "${BOT_DIR}/src/*.cxx" "${BOT_DIR}/src/*.c++")
add_executable(bot ${BOT_SOURCES})
target_include_directories(bot PRIVATE "${BC_SDK}/cpp" "${BOT_DIR}" "${BOT_DIR}/src")
# Match the event server: a single clang++ link with no wasm-opt pass (the
# server sandbox has no wasm-opt on PATH; this flag keeps local links
# identical even when a host Binaryen is installed).
target_link_options(bot PRIVATE --no-wasm-opt)
set_target_properties(bot PROPERTIES SUFFIX ".wasm")
