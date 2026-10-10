# The web server on a PC: a demo server with simulated UART ports (for UI
# development) and the host tests of the platform-independent code.
#
#   cmake -S webserver/host -B build/webserver-host
#   cmake --build build/webserver-host
#   ctest --test-dir build/webserver-host --output-on-failure
#   build/webserver-host/webserver_host 8080     # http://localhost:8080/
cmake_minimum_required(VERSION 3.22)
project(webserver_host LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/../webserver.cmake")
set(shared_dir "${WEBSERVER_REPO_DIR}/shared")

enable_testing()

# Same restrictions as on the targets.
set(host_options -Wall -Wextra -fno-exceptions -fno-rtti -g)

add_library(host_logging STATIC "${shared_dir}/source/logging/format.cc"
                                "${shared_dir}/source/logging/logger.cc")
target_include_directories(host_logging PUBLIC "${shared_dir}/include")
target_compile_options(host_logging PRIVATE ${host_options})

add_library(webserver_posix STATIC)
webserver_add(webserver_posix ADAPTER posix)
target_compile_options(webserver_posix PRIVATE ${host_options})
target_link_libraries(webserver_posix PUBLIC host_logging)

add_library(host_platform STATIC host_platform.cc simulated_ports.cc)
target_include_directories(host_platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(host_platform PRIVATE ${host_options})
target_link_libraries(host_platform PUBLIC webserver_posix)

add_executable(webserver_host main.cc)
target_compile_options(webserver_host PRIVATE ${host_options})
target_link_libraries(webserver_host PRIVATE host_platform)

add_executable(webserver_test test/webserver_test.cc)
target_compile_options(webserver_test PRIVATE ${host_options})
target_link_libraries(webserver_test PRIVATE host_platform)
add_test(NAME webserver_test COMMAND webserver_test)

# The browser side against the C++ side: the UI's FlatBuffers code talks to
# a running webserver_host (login, port list, settings, sessions).
add_test(NAME webserver_ui_smoke_test
         COMMAND "${NODE_EXECUTABLE}"
                 "${WEBSERVER_DIR}/web/scripts/smoke_test.mjs"
                 --server "$<TARGET_FILE:webserver_host>"
                 --out "${CMAKE_CURRENT_BINARY_DIR}/webserver_generated/web"
         WORKING_DIRECTORY "${WEBSERVER_DIR}/web")
