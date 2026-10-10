#include "posix_http_listener.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "logging/logger.h"
#include "webserver/http_headers.h"

namespace webserver::posix {
namespace {

bool SendAll(int socket, std::span<const std::uint8_t> data) noexcept {
  while (!data.empty()) {
    const ssize_t sent = ::send(socket, data.data(), data.size(), MSG_NOSIGNAL);
    if (sent <= 0) {
      return false;
    }
    data = data.subspan(static_cast<std::size_t>(sent));
  }
  return true;
}

bool SendText(int socket, std::string_view text) noexcept {
  return SendAll(socket, {reinterpret_cast<const std::uint8_t*>(text.data()),
                          text.size()});
}

bool ReceiveExactly(int socket, std::span<std::uint8_t> data) noexcept {
  while (!data.empty()) {
    const ssize_t received = ::recv(socket, data.data(), data.size(), 0);
    if (received <= 0) {
      return false;
    }
    data = data.subspan(static_cast<std::size_t>(received));
  }
  return true;
}

}  // namespace

bool HttpListener::Run(std::uint16_t port) noexcept {
  const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listener < 0) {
    return false;
  }
  const int reuse = 1;
  ::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);
  if (::bind(listener, reinterpret_cast<const sockaddr*>(&address),
             sizeof(address)) != 0 ||
      ::listen(listener, 8) != 0) {
    ::close(listener);
    return false;
  }
  logging::Info("web", "listening on http://localhost:{}/", port);
  while (true) {
    const int client = ::accept(listener, nullptr, nullptr);
    if (client < 0) {
      break;
    }
    Serve(client);
    ::close(client);
  }
  ::close(listener);
  return true;
}

void HttpListener::Serve(int client) noexcept {
  response_.Reset();

  // Read until the end of the header block; what follows is body.
  std::size_t received = 0;
  std::size_t header_end = std::string_view::npos;
  while (header_end == std::string_view::npos) {
    if (received == header_buffer_.size()) {
      response_.SetText(HttpStatus::kBadRequest, "Header too large");
      SendResponse(client);
      return;
    }
    const ssize_t count = ::recv(client, header_buffer_.data() + received,
                                 header_buffer_.size() - received, 0);
    if (count <= 0) {
      return;
    }
    received += static_cast<std::size_t>(count);
    header_end = std::string_view(header_buffer_.data(), received)
                     .find("\r\n\r\n");
  }
  const std::string_view head(header_buffer_.data(), header_end);

  // Request line: METHOD SP target SP version.
  const std::string_view request_line = head.substr(0, head.find("\r\n"));
  const std::size_t method_end = request_line.find(' ');
  const std::size_t target_end = request_line.find(' ', method_end + 1);
  if (method_end == std::string_view::npos ||
      target_end == std::string_view::npos) {
    response_.SetText(HttpStatus::kBadRequest, "Bad Request");
    SendResponse(client);
    return;
  }
  const std::string_view target =
      request_line.substr(method_end + 1, target_end - method_end - 1);

  std::size_t content_length = 0;
  const std::string_view length_text = http::FindHeader(head, "Content-Length");
  if (!length_text.empty() &&
      std::from_chars(length_text.data(),
                      length_text.data() + length_text.size(), content_length)
              .ec != std::errc{}) {
    response_.SetText(HttpStatus::kBadRequest, "Bad Content-Length");
    SendResponse(client);
    return;
  }
  if (content_length > body_buffer_.size()) {
    response_.SetText(HttpStatus::kPayloadTooLarge, "Payload Too Large");
    SendResponse(client);
    return;
  }
  // Body bytes that came with the header, then the rest.
  const std::size_t early =
      std::min(received - (header_end + 4), content_length);
  std::memcpy(body_buffer_.data(), header_buffer_.data() + header_end + 4,
              early);
  if (!ReceiveExactly(client, std::span(body_buffer_)
                                  .subspan(early, content_length - early))) {
    return;
  }

  const HttpRequest request{
      .method = http::ParseMethod(request_line.substr(0, method_end)),
      .path = http::PathOf(target),
      .content_type =
          http::MediaType(http::FindHeader(head, "Content-Type")),
      .authorization = http::FindHeader(head, "Authorization"),
      .body = std::span(body_buffer_).first(content_length),
  };
  server_.HandleRequest(request, response_);
  SendResponse(client);
}

bool HttpListener::SendResponse(int client) noexcept {
  std::array<char, 256> extra;
  const std::string_view extra_headers =
      http::FormatExtraHeaders(response_, extra);
  std::array<char, 16> length;
  const auto [length_end, error] =
      std::to_chars(length.begin(), length.end(), response_.body().size());
  static_cast<void>(error);

  return SendText(client, "HTTP/1.1 ") &&
         SendText(client, StatusLine(response_.status)) &&
         SendText(client, "\r\nContent-Type: ") &&
         SendText(client, response_.content_type) &&
         SendText(client, "\r\nContent-Length: ") &&
         SendText(client, std::string_view(length.data(), length_end)) &&
         SendText(client, "\r\n") && SendText(client, extra_headers) &&
         SendText(client, "Connection: close\r\n\r\n") &&
         SendAll(client, response_.body());
}

}  // namespace webserver::posix
