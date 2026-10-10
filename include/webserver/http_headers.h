#ifndef WEBSERVER_INCLUDE_WEBSERVER_HTTP_HEADERS_
#define WEBSERVER_INCLUDE_WEBSERVER_HTTP_HEADERS_

#include <cstddef>
#include <span>
#include <string_view>

#include "webserver/http_types.h"

// Header helpers for adapters whose network stack hands over the raw header
// block (NetX Duo, the POSIX server).
namespace webserver::http {

constexpr char ToLower(char c) noexcept {
  return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
}

constexpr bool EqualsIgnoreCase(std::string_view lhs,
                                std::string_view rhs) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    if (ToLower(lhs[i]) != ToLower(rhs[i])) {
      return false;
    }
  }
  return true;
}

constexpr std::string_view Trim(std::string_view text) noexcept {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
    text.remove_prefix(1);
  }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
    text.remove_suffix(1);
  }
  return text;
}

// Value of header `name` (case-insensitive) in a CRLF-separated header block,
// trimmed; empty if absent. A request line in front of the headers is
// skipped, since it contains no ':' before its first space.
constexpr std::string_view FindHeader(std::string_view headers,
                                      std::string_view name) noexcept {
  std::size_t position = 0;
  while (position < headers.size()) {
    std::size_t line_end = headers.find("\r\n", position);
    if (line_end == std::string_view::npos) {
      line_end = headers.size();
    }
    const std::string_view line =
        headers.substr(position, line_end - position);
    const std::size_t colon = line.find(':');
    if (colon != std::string_view::npos &&
        EqualsIgnoreCase(line.substr(0, colon), name)) {
      return Trim(line.substr(colon + 1));
    }
    position = line_end + 2;
  }
  return {};
}

// "application/json; charset=utf-8" -> "application/json".
constexpr std::string_view MediaType(std::string_view content_type) noexcept {
  return Trim(content_type.substr(0, content_type.find(';')));
}

// Splits "/path?query" and returns the path.
constexpr std::string_view PathOf(std::string_view target) noexcept {
  return target.substr(0, target.find('?'));
}

constexpr HttpMethod ParseMethod(std::string_view method) noexcept {
  if (method == "GET") return HttpMethod::kGet;
  if (method == "HEAD") return HttpMethod::kHead;
  if (method == "POST") return HttpMethod::kPost;
  if (method == "PUT") return HttpMethod::kPut;
  if (method == "DELETE") return HttpMethod::kDelete;
  return HttpMethod::kUnknown;
}

// Writes the response headers besides the status line, Content-Length and
// Content-Type ("Name: value\r\n" each) into `buffer`. Returns the written
// part; headers that do not fit are left out.
std::string_view FormatExtraHeaders(const HttpResponse& response,
                                    std::span<char> buffer) noexcept;

}  // namespace webserver::http

#endif  // WEBSERVER_INCLUDE_WEBSERVER_HTTP_HEADERS_
