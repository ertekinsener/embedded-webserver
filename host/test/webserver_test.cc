// Host tests of the platform-independent web server: crypto, sessions,
// header parsing, routing, the FlatBuffers APIs and the size bounds that
// keep responses inside the fixed buffers.

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "auth_generated.h"
#include "flatbuffers/flatbuffer_builder.h"
#include "flatbuffers/verifier.h"
#include "host_platform.h"
#include "system_generated.h"
#include "uart_generated.h"
#include "web_resources.h"
#include "webserver/auth/build_credentials.h"
#include "webserver/auth/session_manager.h"
#include "webserver/auth/sha256.h"
#include "webserver/http_headers.h"
#include "webserver/uart/uart_api.h"
#include "webserver/uart/uart_service.h"
#include "webserver/web_application.h"

namespace {

int failures = 0;

#define EXPECT_TRUE(condition)                                            \
  do {                                                                    \
    if (!(condition)) {                                                   \
      std::printf("%s:%d: EXPECT_TRUE(%s) failed\n", __FILE__, __LINE__, \
                  #condition);                                            \
      ++failures;                                                         \
    }                                                                     \
  } while (false)

#define EXPECT_EQ(actual, expected)                                        \
  do {                                                                     \
    const auto a_ = (actual);                                              \
    const auto e_ = (expected);                                            \
    if (!(a_ == e_)) {                                                     \
      std::printf("%s:%d: EXPECT_EQ(%s, %s) failed\n", __FILE__, __LINE__, \
                  #actual, #expected);                                     \
      ++failures;                                                          \
    }                                                                      \
  } while (false)

using webserver::HttpMethod;
using webserver::HttpRequest;
using webserver::HttpResponse;
using webserver::HttpStatus;
namespace api = webserver::api;
namespace auth = webserver::auth;

std::span<const std::uint8_t> Bytes(std::string_view text) {
  return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

std::string Hex(std::span<const std::uint8_t> data) {
  constexpr std::string_view kDigits = "0123456789abcdef";
  std::string text;
  for (std::uint8_t byte : data) {
    text += kDigits[byte >> 4];
    text += kDigits[byte & 0x0F];
  }
  return text;
}

// ---------------------------------------------------------------------------
// Crypto

void TestSha256() {
  EXPECT_EQ(Hex(auth::Sha256Of({})),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(Hex(auth::Sha256Of(Bytes("abc"))),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  // 56 bytes: the length no longer fits into the first padding block.
  EXPECT_EQ(Hex(auth::Sha256Of(Bytes(
                "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"))),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  // Fed in odd pieces.
  auth::Sha256 sha;
  const std::string million(1'000'000, 'a');
  for (std::size_t i = 0; i < million.size(); i += 997) {
    sha.Update(Bytes(std::string_view(million).substr(i, 997)));
  }
  EXPECT_EQ(Hex(sha.Finish()),
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

void TestHmacAndPbkdf2() {
  // RFC 4231, test cases 2 and 6 (key longer than a block).
  EXPECT_EQ(Hex(auth::HmacSha256(Bytes("Jefe"))
                    .Compute(Bytes("what do ya want for nothing?"))),
            "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
  const std::vector<std::uint8_t> long_key(131, 0xaa);
  EXPECT_EQ(Hex(auth::HmacSha256(long_key).Compute(Bytes(
                "Test Using Larger Than Block-Size Key - Hash Key First"))),
            "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");

  std::array<std::uint8_t, 32> key{};
  auth::Pbkdf2HmacSha256(Bytes("password"), Bytes("salt"), 1, key);
  EXPECT_EQ(Hex(key),
            "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
  auth::Pbkdf2HmacSha256(Bytes("password"), Bytes("salt"), 4096, key);
  EXPECT_EQ(Hex(key),
            "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
  // Two output blocks.
  std::array<std::uint8_t, 40> long_output{};
  auth::Pbkdf2HmacSha256(Bytes("passwordPASSWORDpassword"),
                         Bytes("saltSALTsaltSALTsaltSALTsaltSALTsalt"), 4096,
                         long_output);
  EXPECT_EQ(Hex(long_output),
            "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1"
            "c635518c7dac47e9");

  EXPECT_TRUE(auth::ConstantTimeEqual(Bytes("abc"), Bytes("abc")));
  EXPECT_TRUE(!auth::ConstantTimeEqual(Bytes("abc"), Bytes("abd")));
  EXPECT_TRUE(!auth::ConstantTimeEqual(Bytes("abc"), Bytes("ab")));
}

// ---------------------------------------------------------------------------
// Sessions

std::uint32_t fake_now_ms = 0;
std::uint8_t fake_random_seed = 0;

webserver::Platform FakePlatform() {
  return {.now_ms = [] noexcept { return fake_now_ms; },
          .random = [](std::span<std::uint8_t> bytes) noexcept {
            for (std::uint8_t& byte : bytes) {
              byte = fake_random_seed++;
            }
            return true;
          }};
}

std::string_view View(const auth::SessionManager::Token& token) {
  return {token.data(), token.size()};
}

void TestSessions() {
  // Built with the default login admin/admin.
  const auth::Credentials credentials = auth::BuildCredentials();
  EXPECT_EQ(credentials.username, std::string_view("admin"));
  const auth::SessionPolicy policy{
      .idle_timeout_ms = 1000, .max_failures = 3, .lockout_ms = 500};
  auth::SessionManager sessions(credentials, FakePlatform(), policy);
  fake_now_ms = 0;

  EXPECT_EQ(sessions.Login("admin", "wrong").error(),
            auth::LoginError::kInvalidCredentials);
  EXPECT_EQ(sessions.Login("root", "admin").error(),
            auth::LoginError::kInvalidCredentials);
  const auto token = sessions.Login("admin", "admin");
  EXPECT_TRUE(token.has_value());
  EXPECT_TRUE(sessions.Validate(View(*token)));
  EXPECT_TRUE(!sessions.Validate("not a token"));
  EXPECT_TRUE(!sessions.Validate(std::string(32, '0')));

  // The idle timer restarts on use.
  fake_now_ms = 900;
  EXPECT_TRUE(sessions.Validate(View(*token)));
  fake_now_ms = 1800;
  EXPECT_TRUE(sessions.Validate(View(*token)));
  fake_now_ms = 2800;
  EXPECT_TRUE(!sessions.Validate(View(*token)));

  // Logout.
  const auto second = sessions.Login("admin", "admin");
  sessions.Logout(View(*second));
  EXPECT_TRUE(!sessions.Validate(View(*second)));

  // Lockout after max_failures, also for the right password, until it ends.
  for (int i = 0; i < 3; ++i) {
    static_cast<void>(sessions.Login("admin", "bad"));
  }
  EXPECT_EQ(sessions.Login("admin", "admin").error(),
            auth::LoginError::kLockedOut);
  fake_now_ms += 500;
  EXPECT_TRUE(sessions.Login("admin", "admin").has_value());

  // The least recently used session makes room for a new one.
  std::vector<auth::SessionManager::Token> tokens;
  for (std::size_t i = 0; i < auth::SessionManager::kMaxSessions; ++i) {
    ++fake_now_ms;
    tokens.push_back(*sessions.Login("admin", "admin"));
  }
  ++fake_now_ms;
  EXPECT_TRUE(sessions.Validate(View(tokens[0])));  // Now the newest use.
  ++fake_now_ms;
  const auto extra = sessions.Login("admin", "admin");
  EXPECT_TRUE(sessions.Validate(View(*extra)));
  EXPECT_TRUE(sessions.Validate(View(tokens[0])));
  EXPECT_TRUE(!sessions.Validate(View(tokens[1])));

  EXPECT_EQ(auth::BearerToken("Bearer abc"), std::string_view("abc"));
  EXPECT_EQ(auth::BearerToken("bearer  abc "), std::string_view("abc"));
  EXPECT_TRUE(auth::BearerToken("Basic abc").empty());
  EXPECT_TRUE(auth::BearerToken("Bearer").empty());
}

// ---------------------------------------------------------------------------
// HTTP helpers

void TestHeaders() {
  namespace http = webserver::http;
  constexpr std::string_view kHead =
      "POST /api/x?a=1 HTTP/1.1\r\nHost: dev\r\n"
      "content-type: application/x-flatbuffers; charset=x\r\n"
      "Authorization:  Bearer t0k \r\nContent-Length: 12";
  static_assert(http::FindHeader(kHead, "Host") == "dev");
  static_assert(http::FindHeader(kHead, "AUTHORIZATION") == "Bearer t0k");
  static_assert(http::FindHeader(kHead, "Content-Length") == "12");
  static_assert(http::FindHeader(kHead, "Accept").empty());
  static_assert(http::MediaType(http::FindHeader(kHead, "Content-Type")) ==
                "application/x-flatbuffers");
  static_assert(http::PathOf("/api/x?a=1") == "/api/x");
  static_assert(http::ParseMethod("DELETE") == HttpMethod::kDelete);
  static_assert(http::ParseMethod("PATCH") == HttpMethod::kUnknown);

  HttpResponse response;
  response.content_encoding = "gzip";
  std::array<char, 128> buffer;
  EXPECT_EQ(http::FormatExtraHeaders(response, buffer),
            std::string_view("Content-Encoding: gzip\r\n"
                             "Cache-Control: no-store\r\n"
                             "X-Content-Type-Options: nosniff\r\n"));
  // NUL-terminated after the returned text, whatever the buffer held (NetX
  // checks strlen() against the length).
  buffer.fill('x');
  const std::string_view text = http::FormatExtraHeaders(response, buffer);
  EXPECT_EQ(buffer[text.size()], '\0');
  std::array<char, 30> small;
  small.fill('x');
  const std::string_view first = http::FormatExtraHeaders(response, small);
  EXPECT_EQ(first, std::string_view("Content-Encoding: gzip\r\n"));
  EXPECT_EQ(small[first.size()], '\0');
  std::array<char, 24> exact;  // No room for the terminator: left out.
  EXPECT_TRUE(http::FormatExtraHeaders(response, exact).empty());
  EXPECT_EQ(webserver::StatusLine(HttpStatus::kUnprocessableEntity),
            std::string_view("422 Unprocessable Entity"));
}

// ---------------------------------------------------------------------------
// The whole application

class Client {
 public:
  explicit Client(const webserver::HttpServer& server) : server_(server) {}

  const HttpResponse& Send(HttpMethod method, std::string_view path,
                           std::span<const std::uint8_t> body = {},
                           std::string_view content_type =
                               webserver::kFlatBufferType) {
    // Like the adapters: an aligned copy of the body.
    body_.assign(body.begin(), body.end());
    const HttpRequest request{
        .method = method,
        .path = path,
        .content_type = body.empty() ? std::string_view{} : content_type,
        .authorization = authorization_,
        .body = {body_.data(), body_.size()},
    };
    response_.Reset();
    server_.HandleRequest(request, response_);
    return response_;
  }

  void set_token(std::string_view token) {
    authorization_ = "Bearer " + std::string(token);
  }

 private:
  const webserver::HttpServer& server_;
  std::string authorization_;
  std::vector<std::uint8_t> body_;
  HttpResponse response_;
};

std::vector<std::uint8_t> Finished(flatbuffers::FlatBufferBuilder& builder) {
  return {builder.GetBufferPointer(),
          builder.GetBufferPointer() + builder.GetSize()};
}

std::vector<std::uint8_t> LoginBody(std::string_view user,
                                    std::string_view password) {
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(api::CreateLoginRequest(builder, builder.CreateString(user),
                                         builder.CreateString(password)));
  return Finished(builder);
}

std::vector<std::uint8_t> SettingsBody(std::uint8_t port, std::uint32_t baud,
                                       api::WordLength word_length =
                                           api::WordLength::Bits_8) {
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(api::CreateUartSettings(builder, port, baud, word_length));
  return Finished(builder);
}

std::vector<std::uint8_t> BridgeBody(std::uint8_t port, bool enabled,
                                     std::uint16_t tcp_port,
                                     std::uint16_t idle_gap_ms = 0,
                                     std::uint16_t max_frame = 1024) {
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(api::CreateBridgeSettings(builder, port, enabled, tcp_port,
                                           idle_gap_ms, max_frame));
  return Finished(builder);
}

template <typename T>
const T* Verified(const HttpResponse& response) {
  flatbuffers::Verifier verifier(response.body().data(),
                                 response.body().size());
  if (response.content_type != webserver::kFlatBufferType ||
      !verifier.VerifyBuffer<T>(nullptr)) {
    return nullptr;
  }
  return flatbuffers::GetRoot<T>(response.body().data());
}

void TestApplication() {
  webserver::WebApplication web{{
      .resources = webserver::WebResources(),
      .uart_ports = host::SimulatedPorts(),
      .credentials = auth::BuildCredentials(),
      .platform = host::HostPlatform(),
      .system = host::HostSystem(),
  }};
  Client client(web.server());

  // Every route is in WEBSERVER_API_PATHS (registered up front by Zephyr).
#define WEBSERVER_TEST_PATH(unused, n, path) path,
  constexpr std::string_view kPaths[] = {
      WEBSERVER_API_PATHS(WEBSERVER_TEST_PATH, _)};
  EXPECT_EQ(web.routes().size(), std::size(kPaths));
  for (const auto& route : web.routes()) {
    bool listed = false;
    for (std::string_view path : kPaths) {
      listed = listed || path == route.path;
    }
    EXPECT_TRUE(listed);
  }

  // Static files: index under "/" and "/index.html", gzipped.
  const HttpResponse& index = client.Send(HttpMethod::kGet, "/");
  EXPECT_EQ(index.status, HttpStatus::kOk);
  EXPECT_EQ(index.content_type, std::string_view("text/html"));
  EXPECT_EQ(index.content_encoding, std::string_view("gzip"));
  EXPECT_EQ(index.cache_control, std::string_view("no-cache"));
  EXPECT_TRUE(index.body().size() > 2 && index.body()[0] == 0x1f);
  EXPECT_EQ(client.Send(HttpMethod::kHead, "/index.html").body().size(), 0U);
  EXPECT_EQ(client.Send(HttpMethod::kGet, "/missing.js").status,
            HttpStatus::kNotFound);
  EXPECT_EQ(client.Send(HttpMethod::kPost, "/index.html").status,
            HttpStatus::kMethodNotAllowed);

  // Without a session.
  EXPECT_EQ(client.Send(HttpMethod::kGet, "/api/uart/ports").status,
            HttpStatus::kUnauthorized);
  EXPECT_EQ(client.Send(HttpMethod::kGet, "/api/nothing").status,
            HttpStatus::kNotFound);
  EXPECT_EQ(client.Send(HttpMethod::kGet, "/api/auth/login").status,
            HttpStatus::kMethodNotAllowed);
  EXPECT_EQ(client.Send(HttpMethod::kPost, "/api/auth/login",
                        LoginBody("admin", "nope"))
                .status,
            HttpStatus::kUnauthorized);
  EXPECT_EQ(client.Send(HttpMethod::kPost, "/api/auth/login",
                        LoginBody("admin", "admin"), "application/json")
                .status,
            HttpStatus::kUnsupportedMediaType);
  EXPECT_EQ(
      client.Send(HttpMethod::kPost, "/api/auth/login", Bytes("garbage!!"))
          .status,
      HttpStatus::kBadRequest);

  // Login.
  const HttpResponse& login = client.Send(HttpMethod::kPost, "/api/auth/login",
                                          LoginBody("admin", "admin"));
  EXPECT_EQ(login.status, HttpStatus::kOk);
  const auto* login_response = Verified<api::LoginResponse>(login);
  EXPECT_TRUE(login_response != nullptr && login_response->token() &&
              login_response->token()->size() == 32);
  if (login_response == nullptr || login_response->token() == nullptr) {
    return;
  }
  EXPECT_EQ(login_response->idle_timeout_s(), 15U * 60U);
  client.set_token(login_response->token()->string_view());

  // System info.
  const auto* info = Verified<api::SystemInfo>(
      client.Send(HttpMethod::kGet, "/api/system/info"));
  EXPECT_TRUE(info != nullptr && info->ipv4_address()->size() == 4 &&
              info->platform()->string_view() == "POSIX sockets");

  // Port list with capabilities.
  const HttpResponse& list = client.Send(HttpMethod::kGet, "/api/uart/ports");
  const auto* ports = Verified<api::UartPortList>(list);
  EXPECT_TRUE(ports != nullptr && ports->ports()->size() == 3);
  if (ports != nullptr) {
    const api::UartPort* sim1 = ports->ports()->Get(0);
    EXPECT_EQ(sim1->id(), 1);
    EXPECT_EQ(sim1->name()->string_view(), std::string_view("SIM1"));
    EXPECT_EQ(sim1->capabilities()->flow_controls()->size(), 4U);
    EXPECT_EQ(sim1->capabilities()->word_lengths()->Get(1),
              api::WordLength::Bits_8);
    EXPECT_TRUE(sim1->capabilities()->fifo());
    EXPECT_EQ(sim1->settings()->baud_rate(), 115'200U);
    EXPECT_TRUE(sim1->settings()->fifo_enabled());
    EXPECT_TRUE(!ports->ports()->Get(2)->capabilities()->fifo());
  }

  // Settings.
  EXPECT_EQ(client.Send(HttpMethod::kPost, "/api/uart/config",
                        SettingsBody(1, 230'400))
                .status,
            HttpStatus::kNoContent);
  EXPECT_EQ(host::SimulatedPorts()[0].config.baud_rate, 230'400U);
  EXPECT_EQ(client.Send(HttpMethod::kPost, "/api/uart/config",
                        SettingsBody(9, 9'600))
                .status,
            HttpStatus::kNotFound);
  // SIM1 has no 9 data bits; SIM3 refuses > 1 Mbit/s in its driver.
  EXPECT_EQ(client.Send(HttpMethod::kPost, "/api/uart/config",
                        SettingsBody(1, 9'600, api::WordLength::Bits_9))
                .status,
            HttpStatus::kUnprocessableEntity);
  EXPECT_EQ(client.Send(HttpMethod::kPost, "/api/uart/config",
                        SettingsBody(3, 1'500'000))
                .status,
            HttpStatus::kUnprocessableEntity);
  EXPECT_EQ(host::SimulatedPorts()[2].config.baud_rate, 9'600U);
  // Out-of-range enum values from the network.
  EXPECT_EQ(client.Send(HttpMethod::kPost, "/api/uart/config",
                        SettingsBody(1, 9'600,
                                     static_cast<api::WordLength>(77)))
                .status,
            HttpStatus::kUnprocessableEntity);

  // Bridge: SIM1 starts bridged on TCP 4001; SIM3 has no bridge.
  if (ports != nullptr) {
    const api::UartPort* sim1 = ports->ports()->Get(0);
    EXPECT_TRUE(sim1->bridge() != nullptr && sim1->bridge()->enabled() &&
                sim1->bridge()->tcp_port() == 4001);
    EXPECT_TRUE(sim1->bridge_status() != nullptr &&
                sim1->bridge_status()->listening());
    EXPECT_TRUE(ports->ports()->Get(2)->bridge() == nullptr);
  }
  const auto bridge = [&](std::vector<std::uint8_t> body) {
    return client.Send(HttpMethod::kPost, "/api/bridge/config", body).status;
  };
  EXPECT_EQ(bridge(BridgeBody(2, true, 4002, 20, 512)), HttpStatus::kNoContent);
  EXPECT_EQ(host::SimulatedPorts()[1].bridge.tcp_port, 4002);
  EXPECT_EQ(host::SimulatedPorts()[1].bridge.idle_gap_ms, 20);
  EXPECT_EQ(bridge(BridgeBody(2, true, 4001)),  // SIM1's port.
            HttpStatus::kUnprocessableEntity);
  EXPECT_EQ(bridge(BridgeBody(2, true, 80)), HttpStatus::kUnprocessableEntity);
  EXPECT_EQ(bridge(BridgeBody(2, true, 4002, 1001)),
            HttpStatus::kUnprocessableEntity);
  EXPECT_EQ(bridge(BridgeBody(2, true, 4002, 0, 1461)),
            HttpStatus::kUnprocessableEntity);
  EXPECT_EQ(bridge(BridgeBody(3, true, 4003)),
            HttpStatus::kUnprocessableEntity);
  EXPECT_EQ(bridge(BridgeBody(9, true, 4009)), HttpStatus::kNotFound);
  // Disabled bridges do not hold their TCP port.
  EXPECT_EQ(bridge(BridgeBody(1, false, 4001)), HttpStatus::kNoContent);
  EXPECT_EQ(bridge(BridgeBody(2, true, 4001)), HttpStatus::kNoContent);
  EXPECT_EQ(host::SimulatedPorts()[1].bridge.tcp_port, 4001);
  EXPECT_EQ(bridge(BridgeBody(2, false, 0)), HttpStatus::kNoContent);

  // Logout ends the session.
  EXPECT_EQ(client.Send(HttpMethod::kPost, "/api/auth/logout").status,
            HttpStatus::kNoContent);
  EXPECT_EQ(client.Send(HttpMethod::kGet, "/api/uart/ports").status,
            HttpStatus::kUnauthorized);
}

// The port list estimate must bound the real size, or FlatBufferArena could
// overflow on a device with many ports.
void TestPortListBound() {
  const std::string name(40, 'N');
  const std::string description(120, 'D');
  const webserver::UartCapabilities all{
      .word_lengths = {webserver::UartWordLength::kBits7,
                       webserver::UartWordLength::kBits8,
                       webserver::UartWordLength::kBits9},
      .stop_bits = {webserver::UartStopBits::kOne,
                    webserver::UartStopBits::kHalf,
                    webserver::UartStopBits::kTwo,
                    webserver::UartStopBits::kOneAndHalf},
      .parities = {webserver::UartParity::kNone, webserver::UartParity::kEven,
                   webserver::UartParity::kOdd},
      .flow_controls = {webserver::UartFlowControl::kNone,
                        webserver::UartFlowControl::kRts,
                        webserver::UartFlowControl::kCts,
                        webserver::UartFlowControl::kRtsCts},
      .oversampling = {webserver::UartOversampling::kBy16,
                       webserver::UartOversampling::kBy8},
      .fifo = true,
      .tx_rx_swap = true,
  };
  for (std::size_t count : {1U, 2U, 8U, 16U}) {
    std::vector<webserver::UartPort> ports(count);
    for (std::size_t i = 0; i < count; ++i) {
      ports[i] = {.id = static_cast<std::uint8_t>(i + 1),
                  .name = name,
                  .description = description,
                  .capabilities = all,
                  .driver = {},
                  .config = {.port_id = static_cast<std::uint8_t>(i + 1),
                             .baud_rate = 0xFFFFFFFF,
                             .fifo_enabled = true,
                             .tx_rx_swap = true}};
    }
    for (webserver::UartPort& port : ports) {
      port.bridge_driver = {
          .apply = [](void*, std::uint8_t,
                      const webserver::UartBridgeSettings&) noexcept {
            return true;
          },
          .status = [](void*, std::uint8_t,
                       webserver::UartBridgeStatus& status) noexcept {
            status = {.listening = true,
                      .connected = true,
                      .client_address = {255, 255, 255, 255},
                      .client_port = 65535,
                      .uart_to_net_bytes = 0xFFFFFFFF,
                      .net_to_uart_bytes = 0xFFFFFFFF,
                      .dropped_bytes = 0xFFFFFFFF,
                      .uart_errors = 0xFFFFFFFF};
            return true;
          },
          .context = nullptr};
      port.bridge = {.enabled = true,
                     .tcp_port = 65535,
                     .idle_gap_ms = 1000,
                     .max_frame = 1460};
    }
    webserver::UartService service(ports);
    webserver::UartApi uart_api(service);
    HttpResponse response;
    uart_api.ListPorts({}, response);
    const std::size_t bound =
        64 + count * (384 + name.size() + description.size());
    if (bound > response.buffer().size()) {
      EXPECT_EQ(response.status, HttpStatus::kInternalError);
      continue;
    }
    EXPECT_EQ(response.status, HttpStatus::kOk);
    EXPECT_TRUE(response.body().size() <= bound);
    std::printf("port list: %zu ports, %zu B (bound %zu B)\n", count,
                response.body().size(), bound);
  }
}

}  // namespace

int main() {
  host::InitLogging();
  TestSha256();
  TestHmacAndPbkdf2();
  TestSessions();
  TestHeaders();
  TestApplication();
  TestPortListBound();
  if (failures != 0) {
    std::printf("%d check(s) failed\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("all webserver tests passed\n");
  return EXIT_SUCCESS;
}
