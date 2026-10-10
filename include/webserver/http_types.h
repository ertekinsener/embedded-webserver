#ifndef WEBSERVER_INCLUDE_WEBSERVER_HTTP_TYPES_
#define WEBSERVER_INCLUDE_WEBSERVER_HTTP_TYPES_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace webserver {

enum class HttpMethod : std::uint8_t {
  kGet,
  kHead,
  kPost,
  kPut,
  kDelete,
  kUnknown,
};

// A request as the platform-independent server sees it. The adapter of the
// network stack fills it in; the views point into the adapter's buffers and
// are valid only during the handler call.
struct HttpRequest {
  HttpMethod method = HttpMethod::kUnknown;
  std::string_view path;           // Without the query string.
  std::string_view content_type;   // Media type, without parameters.
  std::string_view authorization;  // Value of the Authorization header.
  std::span<const std::uint8_t> body;
};

enum class HttpStatus : std::uint16_t {
  kOk = 200,
  kNoContent = 204,
  kBadRequest = 400,
  kUnauthorized = 401,
  kNotFound = 404,
  kMethodNotAllowed = 405,
  kPayloadTooLarge = 413,
  kUnsupportedMediaType = 415,
  kUnprocessableEntity = 422,
  kTooManyRequests = 429,
  kInternalError = 500,
  kServiceUnavailable = 503,
};

// "404 Not Found" etc., as used in a status line.
std::string_view StatusLine(HttpStatus status) noexcept;

// Media type of every API payload (schema/*.fbs).
inline constexpr std::string_view kFlatBufferType = "application/x-flatbuffers";

// Request limits the adapters enforce (400 / 413 beyond them); the web UI
// sends far less.
inline constexpr std::size_t kMaxRequestHeaderSize = 2048;
inline constexpr std::size_t kMaxRequestBodySize = 512;

// The response a handler fills in. The body either lies in `buffer()` (API
// responses, error texts) or is static data that outlives the response
// (embedded files). The header values are sent as C strings by some
// adapters: they must view NUL-terminated literals.
class HttpResponse {
 public:
  // Largest body built in place, e.g. the UART port list.
  static constexpr std::size_t kBufferSize = 4096;

  HttpResponse() = default;
  // The body may point into `buffer_`, so copies would dangle.
  HttpResponse(const HttpResponse&) = delete;
  HttpResponse& operator=(const HttpResponse&) = delete;

  // Back to "500, no body" for the next request.
  void Reset() noexcept {
    status = HttpStatus::kInternalError;
    content_type = "text/plain";
    content_encoding = {};
    cache_control = "no-store";
    body_ = {};
  }

  // `data` must lie in buffer() or outlive the response.
  void SetBody(HttpStatus code, std::span<const std::uint8_t> data,
               std::string_view type) noexcept {
    status = code;
    content_type = type;
    body_ = data;
  }

  void SetEmpty(HttpStatus code) noexcept {
    status = code;
    content_type = "text/plain";
    body_ = {};
  }

  // Status with a short plain-text body (copied, truncated to the buffer).
  void SetText(HttpStatus code, std::string_view text) noexcept {
    const std::size_t size = text.size() < buffer_.size() ? text.size()
                                                          : buffer_.size();
    std::memcpy(buffer_.data(), text.data(), size);
    SetBody(code, std::span(buffer_).first(size), "text/plain");
  }

  // Storage for bodies built in place (FlatBuffers, see flatbuffer_arena.h).
  std::span<std::uint8_t> buffer() noexcept { return buffer_; }
  std::span<const std::uint8_t> body() const noexcept { return body_; }

  HttpStatus status = HttpStatus::kInternalError;
  std::string_view content_type = "text/plain";
  std::string_view content_encoding;  // e.g. "gzip"; empty: none.
  std::string_view cache_control = "no-store";

 private:
  std::span<const std::uint8_t> body_;
  // FlatBuffers are built in place: keep scalars aligned.
  alignas(8) std::array<std::uint8_t, kBufferSize> buffer_{};
};

}  // namespace webserver

#endif  // WEBSERVER_INCLUDE_WEBSERVER_HTTP_TYPES_
