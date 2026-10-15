# Platform-independent web server (webserver/README.md), included by the CM7
# firmware, the Zephyr application and the host build:
#
#   include(<repo>/webserver/webserver.cmake)
#   webserver_add(<target> ADAPTER netxduo|zephyr|posix)
#
# Adds the core, the adapter for the network stack and the generated code to
# <target>: FlatBuffers headers (flatc), the web UI as byte arrays (Node.js,
# web/scripts/build.mjs) and the login credentials. The target must also be
# able to include "logging/logger.h" (shared/include) and link the logger.
#
# Needs flatc, node and npm on PATH. The first build runs `npm ci` in
# webserver/web (needs network access once).
#
# Login (cache variables): WEBSERVER_ADMIN_USER, WEBSERVER_ADMIN_PASSWORD
# (stored in the firmware only as a PBKDF2-HMAC-SHA-256 key),
# WEBSERVER_PASSWORD_ITERATIONS, WEBSERVER_PASSWORD_SALT (random, generated
# once per build directory).

set(WEBSERVER_DIR "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(WEBSERVER_REPO_DIR "${WEBSERVER_DIR}/.." ABSOLUTE)

set(WEBSERVER_ADMIN_USER "admin" CACHE STRING "Web UI user name")
set(WEBSERVER_ADMIN_PASSWORD "admin" CACHE STRING
    "Web UI password (only its PBKDF2 key goes into the firmware)")
set(WEBSERVER_PASSWORD_ITERATIONS "4096" CACHE STRING
    "PBKDF2 iterations of the web UI password (a login costs this many HMACs)")
if(NOT WEBSERVER_PASSWORD_SALT)
  string(RANDOM LENGTH 32 ALPHABET "0123456789abcdef" salt)
  set(WEBSERVER_PASSWORD_SALT "${salt}" CACHE STRING
      "PBKDF2 salt of the web UI password (32 hex digits)" FORCE)
endif()

set(webserver_core_sources
    "${WEBSERVER_DIR}/source/api_router.cc"
    "${WEBSERVER_DIR}/source/auth/auth_api.cc"
    "${WEBSERVER_DIR}/source/auth/session_manager.cc"
    "${WEBSERVER_DIR}/source/auth/sha256.cc"
    "${WEBSERVER_DIR}/source/http_server.cc"
    "${WEBSERVER_DIR}/source/http_types.cc"
    "${WEBSERVER_DIR}/source/resource_registry.cc"
    "${WEBSERVER_DIR}/source/settings/settings_api.cc"
    "${WEBSERVER_DIR}/source/settings/settings_store.cc"
    "${WEBSERVER_DIR}/source/system/system_api.cc"
    "${WEBSERVER_DIR}/source/uart/uart_api.cc"
    "${WEBSERVER_DIR}/source/uart/uart_codec.cc"
    "${WEBSERVER_DIR}/source/uart/uart_service.cc"
    "${WEBSERVER_DIR}/source/web_application.cc")

file(GLOB webserver_schemas CONFIGURE_DEPENDS "${WEBSERVER_DIR}/schema/*.fbs")

# C++ headers for the schemas, at build time.
function(_webserver_generate_flatbuffers out_dir out_headers)
  find_program(FLATC_EXECUTABLE flatc REQUIRED)
  set(headers)
  foreach(schema IN LISTS webserver_schemas)
    get_filename_component(name "${schema}" NAME_WE)
    set(header "${out_dir}/${name}_generated.h")
    add_custom_command(
      OUTPUT "${header}"
      COMMAND "${FLATC_EXECUTABLE}" --cpp --scoped-enums -o "${out_dir}"
              "${schema}"
      # Schemas include each other (settings.fbs uses uart.fbs).
      DEPENDS ${webserver_schemas}
      COMMENT "flatc ${name}.fbs"
      VERBATIM)
    list(APPEND headers "${header}")
  endforeach()
  set(${out_headers} "${headers}" PARENT_SCOPE)
endfunction()

# The PBKDF2 key of the password, at configure time.
function(_webserver_generate_credentials out_dir out_source)
  find_program(NODE_EXECUTABLE node REQUIRED)
  if(NOT WEBSERVER_PASSWORD_SALT MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "WEBSERVER_PASSWORD_SALT must be hex digits")
  endif()
  execute_process(
    COMMAND "${NODE_EXECUTABLE}" "${WEBSERVER_DIR}/web/scripts/derive_key.mjs"
            "${WEBSERVER_ADMIN_PASSWORD}" "${WEBSERVER_PASSWORD_SALT}"
            "${WEBSERVER_PASSWORD_ITERATIONS}"
    OUTPUT_VARIABLE key_hex
    RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Deriving the web UI password key failed")
  endif()
  if(WEBSERVER_ADMIN_PASSWORD STREQUAL "admin")
    message(WARNING "The web UI uses the default login admin/admin; set "
                    "WEBSERVER_ADMIN_PASSWORD for a real device.")
  endif()
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," WEBSERVER_KEY_BYTES
                       "${key_hex}")
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," WEBSERVER_SALT_BYTES
                       "${WEBSERVER_PASSWORD_SALT}")
  configure_file("${WEBSERVER_DIR}/cmake/build_credentials.cc.in"
                 "${out_dir}/build_credentials.cc" @ONLY)
  set(${out_source} "${out_dir}/build_credentials.cc" PARENT_SCOPE)
endfunction()

# The web UI as C++ byte arrays, at build time.
function(_webserver_build_web_ui out_dir out_sources)
  find_program(NODE_EXECUTABLE node REQUIRED)
  find_program(NPM_EXECUTABLE npm REQUIRED)
  find_program(FLATC_EXECUTABLE flatc REQUIRED)
  set(web_dir "${WEBSERVER_DIR}/web")
  # Stamp in the build directory: several build directories share
  # web/node_modules, and each reinstalls only when the lock file changes.
  set(npm_stamp "${out_dir}/npm_ci.stamp")
  add_custom_command(
    OUTPUT "${npm_stamp}"
    COMMAND "${NPM_EXECUTABLE}" ci --no-audit --no-fund
    COMMAND "${CMAKE_COMMAND}" -E touch "${npm_stamp}"
    DEPENDS "${web_dir}/package.json" "${web_dir}/package-lock.json"
    WORKING_DIRECTORY "${web_dir}"
    COMMENT "npm ci (web UI build tools)"
    VERBATIM)

  file(GLOB_RECURSE web_sources CONFIGURE_DEPENDS "${web_dir}/src/*")
  set(outputs "${out_dir}/web_resources.cc" "${out_dir}/web_resources.h")
  add_custom_command(
    OUTPUT ${outputs}
    COMMAND "${NODE_EXECUTABLE}" "${web_dir}/scripts/build.mjs" --out
            "${out_dir}" --flatc "${FLATC_EXECUTABLE}"
    # The script leaves unchanged outputs alone; touch them so this rule
    # does not run again.
    COMMAND "${CMAKE_COMMAND}" -E touch_nocreate ${outputs}
    DEPENDS ${web_sources} ${webserver_schemas} "${web_dir}/index.html"
            "${web_dir}/favicon.svg" "${web_dir}/tsconfig.json"
            "${web_dir}/scripts/build.mjs" "${npm_stamp}"
    WORKING_DIRECTORY "${web_dir}"
    COMMENT "Building the web UI"
    VERBATIM)
  set(${out_sources} "${outputs}" PARENT_SCOPE)
endfunction()

function(webserver_add target)
  cmake_parse_arguments(ARG "" "ADAPTER" "" ${ARGN})
  set(adapter_dir "${WEBSERVER_DIR}/adapters/${ARG_ADAPTER}")
  if(ARG_ADAPTER STREQUAL "netxduo")
    set(adapter_sources "${adapter_dir}/netx_duo_adapter.cc")
  elseif(ARG_ADAPTER STREQUAL "zephyr")
    set(adapter_sources "${adapter_dir}/zephyr_adapter.cc")
  elseif(ARG_ADAPTER STREQUAL "posix")
    set(adapter_sources "${adapter_dir}/posix_http_listener.cc")
  else()
    message(FATAL_ERROR "webserver_add: ADAPTER must be netxduo, zephyr or "
                        "posix, not '${ARG_ADAPTER}'")
  endif()

  set(gen_dir "${CMAKE_CURRENT_BINARY_DIR}/webserver_generated")
  _webserver_generate_flatbuffers("${gen_dir}" flatbuffer_headers)
  _webserver_generate_credentials("${gen_dir}" credentials_source)
  _webserver_build_web_ui("${gen_dir}/web" web_sources)

  target_sources(
    "${target}"
    PRIVATE ${webserver_core_sources} ${adapter_sources}
            ${flatbuffer_headers} "${credentials_source}" ${web_sources})
  # PUBLIC, so a library built with webserver_add() passes them on.
  target_include_directories(
    "${target}"
    PUBLIC "${WEBSERVER_DIR}/include" "${adapter_dir}" "${gen_dir}"
            "${gen_dir}/web"
            "${WEBSERVER_REPO_DIR}/external/flatbuffers/include")
endfunction()
