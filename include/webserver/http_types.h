#ifndef APP_INCLUDE_WEBSERVER_HTTP_TYPES_
#define APP_INCLUDE_WEBSERVER_HTTP_TYPES_

#include <array>
#include <cstddef>
#include <cstring>
#include <string_view>

namespace webserver {

enum class HttpMethod {
  kGet,
  kHead,
  kPost,
  kPut,
  kDelete,
  kUnknown,
};

// A request as the HTTP server sees it. The views point into the adapter's
// receive buffer and are valid only during the handler call.
struct HttpRequest {
  HttpMethod method = HttpMethod::kUnknown;
  std::string_view path;
  std::string_view content_type;
  std::string_view body;
};

// Status codes the handlers use; the adapter maps them to NetX status lines.
enum class HttpStatus {
  kOk = 200,
  kNoContent = 204,
  kBadRequest = 400,
  kNotFound = 404,
  kMethodNotAllowed = 405,
  kUnsupportedMediaType = 415,
  kUnprocessableEntity = 422,
  kInternalError = 500,
};

// The response a handler fills in. Short bodies are copied into
// `owned_body`; longer ones (static resources) are referenced in place and
// must outlive the response.
struct HttpResponse {
  static constexpr std::size_t kOwnedCapacity = 512;

  HttpResponse() = default;
  // `body` may point into `owned_body`, so copies would dangle.
  HttpResponse(const HttpResponse&) = delete;
  HttpResponse& operator=(const HttpResponse&) = delete;

  void SetBody(std::string_view data, std::string_view type) noexcept {
    body_size = data.size();
    content_type = type;
    if (data.size() <= owned_body.size()) {
      std::memcpy(owned_body.data(), data.data(), data.size());
      body = owned_body.data();
    } else {
      body = data.data();
    }
  }

  void SetEmptyBody(std::string_view type) noexcept {
    body_size = 0;
    content_type = type;
    body = owned_body.data();
  }

  // Status with a short plain-text body, e.g. for errors.
  void SetText(HttpStatus code, std::string_view text) noexcept {
    status = code;
    SetBody(text, "text/plain");
  }

  HttpStatus status = HttpStatus::kInternalError;
  // Passed to NetX as a C string: must view a NUL-terminated literal.
  std::string_view content_type = "text/plain";
  const char* body = nullptr;
  std::size_t body_size = 0;
  std::array<char, kOwnedCapacity> owned_body{};
};

}  // namespace webserver

#endif  // APP_INCLUDE_WEBSERVER_HTTP_TYPES_
