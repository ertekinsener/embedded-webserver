# Web server

A platform-independent web server for configuring the UART ports of a
device: an embedded single-page UI, a login, and a FlatBuffers API. The same
code runs on

| Platform | Network stack | Application | Adapter |
|---|---|---|---|
| STM32H755 CM7, ThreadX | NetX Duo web server addon | `CM7/app/source/app/netxduo_app.cc` | `adapters/netxduo` |
| Zephyr (`nucleo_h755zi_q/stm32h755xx/m7`, `native_sim`) | Zephyr HTTP server | `zephyr_app/` | `adapters/zephyr` |
| PC (Linux) | BSD sockets | `host/` (simulated ports, tests) | `adapters/posix` |

The firmware applications live in the
[stm32_webserver](https://github.com/ertekinsener/stm32_webserver) project;
paths below that start with `CM7/`, `zephyr_app/`, `tools/` or `docs/`
(except `docs/*.puml` here) refer to it.

Diagrams: `docs/webserver_components.puml` (structure),
`docs/webserver_request_sequence.puml` (login and settings),
`docs/web_ui_build.puml` (UI build); in stm32_webserver
`docs/storage/settings_storage.puml` (saved settings).

## Layout

```
embedded-webserver/
  include/webserver/   core headers (no network stack, no RTOS, no heap)
  source/              core sources
  adapters/<stack>/    one adapter per network stack
  schema/*.fbs         API payloads (C++ and TypeScript are generated)
  web/                 the UI (TypeScript, Tailwind) and its build scripts
  host/                PC build: demo server and tests
  webserver.cmake      webserver_add(<target> ADAPTER netxduo|zephyr|posix)
  CMakeLists.txt       top-level project: the host/ build
  external/            flatbuffers, embedded-core (logger front end)
```

## Using it in a firmware

```cmake
add_subdirectory(<path>/embedded-core)            # embedded_core::logging
include(<path>/embedded-webserver/webserver.cmake)
webserver_add(firmware ADAPTER netxduo)
target_link_libraries(firmware embedded_core::logging)
```

The sources compile as part of `firmware`, with its flags and the network
stack's headers and user configuration. `WEBSERVER_FLATBUFFERS_DIR` selects
another FlatBuffers tree than `external/flatbuffers` (it must match `flatc`).

- `WebApplication` wires everything: `ResourceRegistry` (embedded files),
  `ApiRouter` (exact path + method, 404/405, `Access::kSession` routes need a
  session), `AuthApi`/`SessionManager`, `UartApi`/`UartService`, `SystemApi`,
  `SettingsApi`/`SettingsStore`.
- `HttpServer` sees only `HttpRequest`/`HttpResponse`. An adapter copies a
  request into its own buffers (headers <= 2 KiB, body <= 512 B,
  `kMaxRequest*Size`) and sends the response; the response body is built in
  place in `HttpResponse::buffer()` (4 KiB) or points to flash.
- No heap: FlatBuffers responses are built with `FlatBufferArena` over the
  response buffer. FlatBuffers cannot report a failed allocation, so the
  handlers bound their output size first (`kPortBytes` etc., checked by the
  host tests); outgrowing the arena traps.
- Every request body comes from the network: `DecodeRequest<T>()` runs the
  `flatbuffers::Verifier` before anything is read; enums decoded by casting
  are checked against the port's capabilities.

## API

All payloads are FlatBuffers (`Content-Type: application/x-flatbuffers`);
errors are a status code with a short `text/plain` reason.

| Method, path | Access | Request | Answer |
|---|---|---|---|
| `POST /api/auth/login` | public | `LoginRequest` | 200 `LoginResponse {token, idle_timeout_s}`, 401, 429 |
| `POST /api/auth/logout` | session | - | 204 |
| `POST /api/auth/password` | session | `ChangePasswordRequest {current_password, new_password}` | 204 (other sessions end); 403 wrong current password; 422 new one not 8..64 bytes; 429 locked out; 500 changed but not saved |
| `GET /api/system/info` | session | - | 200 `SystemInfo` |
| `GET /api/uart/ports` | session | - | 200 `UartPortList` (capabilities + settings of every port) |
| `POST /api/uart/config` | session | `UartSettings` | 204; 404 unknown port; 422 unsupported or refused by the hardware |
| `POST /api/bridge/config` | session | `BridgeSettings` (mode: TCP server, RFC 2217, TCP client, UDP; local/remote endpoint; flush rules) | 204; 404 unknown port; 422 no bridge on the port, local port < 1024 or used by another bridge, client without remote, values out of range |
| `POST /api/settings/reset` | session | - | 204 saved settings deleted (defaults after the next start); 501 the platform saves nothing |

`/api/uart/config`, `/api/bridge/config` and `/api/auth/password` answer 500
when the change was applied but saving it failed.

`GET /api/uart/ports` also carries each port's Ethernet bridge settings and
status (`bridge`, `bridge_status`; absent for ports without bridge). A
platform offers a bridge by setting `UartPort::bridge_driver`
(docs/bridge/README.md; the CM7 firmware's is `bridge::NetworkBridge`).
`UartService` is used from the HTTP thread only, unless the platform passes
a `UartServiceLock` (`WebApplicationConfig::uart_lock`): the CM7 firmware
does, because RFC 2217 clients change line settings from the bridge thread.

A session is `Authorization: Bearer <32 hex digits>` (16 bytes from the
platform's random source). Sessions live in RAM (at most 4, least recently
used one evicted, 15 min idle timeout); a reboot logs everyone out.
After 5 failed logins in a row, logins are refused for 30 s.

The login is configured at build time: `WEBSERVER_ADMIN_USER`,
`WEBSERVER_ADMIN_PASSWORD` (default `admin`/`admin`, CMake warns),
`WEBSERVER_PASSWORD_ITERATIONS` (4096). Only a PBKDF2-HMAC-SHA-256 key of the
password with a random per-build-directory salt goes into the firmware. Plain
HTTP sends the password in clear text: use the device on a trusted network.
The password (not the user name) can be changed on the UI's Account page:
`SessionManager` derives a new key with a fresh 16-byte salt and the same
iterations, and keeps it in RAM or, with a settings storage, saves it.

### Saved settings

`SettingsStore` saves what the web UI changed as a `SavedSettings`
FlatBuffer (`schema/settings.fbs`, at most `SettingsStore::kMaxSize` =
1.5 KiB for 16 ports, host-tested): the settings of every port changed from
the UI (`UartPort::saved_config`), the bridge settings of every port with a
bridge, and the password key once it was changed. It saves after every
successful `/api/uart/config`, `/api/bridge/config` and `/api/auth/password`,
building the FlatBuffer in the response buffer (no extra RAM). Line settings
an RFC 2217 client sets go through `UartService::ApplyTemporary()` and are
not saved.

The platform passes a `SettingsStorage {load, save, erase}`
(`WebApplicationConfig::settings_storage`) and calls
`web.settings().Restore(buffer)` once its ports and bridges take settings;
settings a port refuses by then (other capabilities after an update) are
skipped. Without a storage everything works, but only until a restart
(`SystemInfo::settings_saved` tells the UI).

| Platform | Storage |
|---|---|
| CM7 firmware | `storage::FlashLog` in flash bank 2, sectors 6 and 7 (`CM7/app/source/app/settings_storage.cc`, docs/storage/README.md); B1 held at reset deletes it |
| PC | `webserver_host [port [settings-file]]`, in memory without a file |
| Zephyr | none yet (docs/TODO.md) |

### Multi-platform UART support

A port is a `UartPort {id, name, description, capabilities, driver, config}`
owned by the platform. `UartCapabilities` says what that port can do (baud
range, data bits, stop bits, parity, flow control, oversampling, whether FIFO
and TX/RX swap can be changed); `UartService` rejects everything else (422)
before `driver.apply` runs, and the UI only offers what the port reports. So
the same UI shows

- on the STM32/ThreadX firmware: 8 U(S)ARTs (id = U(S)ART number), 7/8 data
  bits, oversampling, FIFO and pin swap, flow control only where RTS/CTS
  pins are defined in `uart/ports.h`, baud range from the kernel clock
  (`CM7/app/source/app/web_uart_ports.cc`);
- on Zephyr: the U(S)ARTs enabled in the devicetree, everything
  `uart_configure()` can express, no FIFO/swap/oversampling
  (`zephyr_app/src/uart_ports.cc`).

## Web UI

`web/` is a TypeScript single-page app without a framework: login, a side
menu (drawer on phones) with an overview, one settings page per port, a
device page and an account page (password, saved settings). The API code uses the classes `flatc --ts --gen-object-api`
generates from `schema/*.fbs` and the FlatBuffers TypeScript runtime of the
`external/flatbuffers` submodule (so it always matches `flatc`). Tailwind is
compiled at build time and icons are inline SVG (Font Awesome Free), so the
UI needs nothing from the internet. The whole UI is about 22 KB in flash.

`web/scripts/build.mjs` (run by `webserver_add()`; diagram
`docs/web_ui_build.puml`): flatc, `tsc` type check, esbuild bundle,
Tailwind, content-hashed asset names (cached for a year; `index.html` is
`no-cache`), gzip, and `web_resources.{h,cc}` with `WebResources()` and the
`WEBSERVER_WEB_RESOURCES` X-macro list. Rebuilding the firmware rebuilds the
UI when a source changes; no reconfigure needed.

Requirements: `flatc`, `node` (>= 20) and `npm` on `PATH`. The first build
runs `npm ci` in `web/` (network access once); the tools are build-time only
and do not end up in the firmware.

## Building and testing on the PC

```bash
git submodule update --init --recursive
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
build/host/webserver_host 8080      # http://localhost:8080, admin/admin
```

embedded-core comes from `external/embedded-core`, or from a checkout next to
this one (`../embedded-core`); `WEBSERVER_EMBEDDED_CORE_DIR` overrides both.

- `webserver_test`: SHA-256/HMAC/PBKDF2 test vectors, sessions (timeout,
  lockout, eviction, password change), header parsing, routing, every API
  status code, saving and restoring settings, the port list and saved
  settings size bounds, and that `WEBSERVER_API_PATHS` lists every route.
- `webserver_ui_smoke_test`: the UI's TypeScript API code against the running
  C++ server (login, ports, settings, password, saved settings across server
  restarts, logout). Against any device:
  `node web/scripts/smoke_test.mjs --url http://192.168.1.1 --out <build>/webserver_generated/web`.

## Porting to another platform

1. Write an adapter that turns the stack's requests into `HttpRequest`
   (method, path without query, media type, `Authorization` value, body) and
   sends `HttpResponse` (status line from `StatusLine()`, `content_type`, the
   headers of `http::FormatExtraHeaders()`, `body()`).
   `http_headers.h` parses raw header blocks.
2. Provide `Platform {now_ms, random}` (a cryptographically secure source),
   a `SystemDescription` and a `UartPort` table with drivers; optionally a
   `SettingsStorage` (then call `settings().Restore()` at start).
3. `webserver_add(<target> ADAPTER <name>)` in CMake (add the adapter to
   `webserver.cmake`), and the logger front end (`embedded_core::logging`)
   with a sink.

Servers that register paths up front (like Zephyr) can use the
`WEBSERVER_WEB_RESOURCES` and `WEBSERVER_API_PATHS` X-macros.

## Zephyr notes

- `WEBSERVER_ZEPHYR_STATIC_RESOURCES` registers the files as Zephyr static
  resources and `WEBSERVER_ZEPHYR_API_RESOURCES` one dynamic resource per
  API path: Zephyr serves a dynamic resource to one client at a time (409 for
  the others), so one catch-all resource would fail the browser's parallel
  requests.
- `CONFIG_HTTP_SERVER_CAPTURE_HEADERS` with `MAX_HEADER_LEN=64` (the default
  32 drops the bearer token; a `static_assert` checks it). Zephyr's HTTP/1
  code does not fill `client->content_type`, so Content-Type is captured too.
- Build: `west build -b nucleo_h755zi_q/stm32h755xx/m7 zephyr_app`. On the PC
  without root: `west build -b native_sim/native/64 zephyr_app`, run
  `build/zephyr/zephyr.exe`, open http://localhost:8080 (host sockets, NSOS).
