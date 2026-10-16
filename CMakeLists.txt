# embedded-webserver as the top-level project: the web server on a PC
# (host/), with its host tests. Firmware and other applications do not add
# this directory; they include webserver.cmake and call webserver_add().
#
#   git submodule update --init --recursive
#   cmake -S . -B build && cmake --build build
#   ctest --test-dir build --output-on-failure
cmake_minimum_required(VERSION 3.22)
project(embedded_webserver LANGUAGES CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/webserver.cmake")

# embedded-core (logger front end): the external/embedded-core submodule, or
# a checkout next to this one.
if(EXISTS "${WEBSERVER_DIR}/external/embedded-core/CMakeLists.txt")
  set(default_core_dir "${WEBSERVER_DIR}/external/embedded-core")
else()
  set(default_core_dir "${WEBSERVER_DIR}/../embedded-core")
endif()
set(WEBSERVER_EMBEDDED_CORE_DIR "${default_core_dir}"
    CACHE PATH "embedded-core source tree")
add_subdirectory("${WEBSERVER_EMBEDDED_CORE_DIR}" embedded-core)

enable_testing()
add_subdirectory(host)
