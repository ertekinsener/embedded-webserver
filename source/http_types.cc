#include "webserver/http_types.h"

#include <cstddef>
#include <span>
#include <string_view>

#include "webserver/http_headers.h"

namespace webserver {

std::string_view StatusLine(HttpStatus status) noexcept {
  switch (status) {
    case HttpStatus::kOk:
      return "200 OK";
    case HttpStatus::kNoContent:
      return "204 No Content";
    case HttpStatus::kBadRequest:
      return "400 Bad Request";
    case HttpStatus::kUnauthorized:
      return "401 Unauthorized";
    case HttpStatus::kNotFound:
      return "404 Not Found";
    case HttpStatus::kMethodNotAllowed:
      return "405 Method Not Allowed";
    case HttpStatus::kPayloadTooLarge:
      return "413 Payload Too Large";
    case HttpStatus::kUnsupportedMediaType:
      return "415 Unsupported Media Type";
    case HttpStatus::kUnprocessableEntity:
      return "422 Unprocessable Entity";
    case HttpStatus::kTooManyRequests:
      return "429 Too Many Requests";
    case HttpStatus::kServiceUnavailable:
      return "503 Service Unavailable";
    case HttpStatus::kInternalError:
      break;
  }
  return "500 Internal Server Error";
}

namespace http {
namespace {

class HeaderWriter {
 public:
  explicit HeaderWriter(std::span<char> buffer) noexcept : buffer_(buffer) {}

  // All or nothing, so a header is never cut off.
  void Add(std::string_view name, std::string_view value) noexcept {
    if (value.empty() ||
        size_ + name.size() + value.size() + 4 > buffer_.size()) {
      return;
    }
    Append(name);
    Append(": ");
    Append(value);
    Append("\r\n");
  }

  std::string_view text() const noexcept { return {buffer_.data(), size_}; }

 private:
  void Append(std::string_view text) noexcept {
    text.copy(buffer_.data() + size_, text.size());
    size_ += text.size();
  }

  std::span<char> buffer_;
  std::size_t size_ = 0;
};

}  // namespace

std::string_view FormatExtraHeaders(const HttpResponse& response,
                                    std::span<char> buffer) noexcept {
  HeaderWriter writer(buffer);
  writer.Add("Content-Encoding", response.content_encoding);
  writer.Add("Cache-Control", response.cache_control);
  writer.Add("X-Content-Type-Options", "nosniff");
  return writer.text();
}

}  // namespace http
}  // namespace webserver
