#include "webserver/netx_duo_adapter.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "logging/logger.h"
#include "nx_api.h"
#include "nx_web_http_server.h"
#include "webserver/http_types.h"

namespace webserver::netx_duo {
namespace {

// The request notify callback gets NX_WEB_HTTP_SERVER_*_REQUEST values, not
// NX_WEB_HTTP_METHOD_* (where 2 means PUT, not POST).
HttpMethod ToHttpMethod(UINT request_type) noexcept {
  switch (request_type) {
    case NX_WEB_HTTP_SERVER_GET_REQUEST:
      return HttpMethod::kGet;
    case NX_WEB_HTTP_SERVER_HEAD_REQUEST:
      return HttpMethod::kHead;
    case NX_WEB_HTTP_SERVER_POST_REQUEST:
      return HttpMethod::kPost;
    case NX_WEB_HTTP_SERVER_PUT_REQUEST:
      return HttpMethod::kPut;
    case NX_WEB_HTTP_SERVER_DELETE_REQUEST:
      return HttpMethod::kDelete;
    default:
      return HttpMethod::kUnknown;
  }
}

const CHAR* ToStatusLine(HttpStatus status) noexcept {
  switch (status) {
    case HttpStatus::kOk:
      return NX_WEB_HTTP_STATUS_OK;
    case HttpStatus::kNoContent:
      return NX_WEB_HTTP_STATUS_NO_CONTENT;
    case HttpStatus::kBadRequest:
      return NX_WEB_HTTP_STATUS_BAD_REQUEST;
    case HttpStatus::kNotFound:
      return NX_WEB_HTTP_STATUS_NOT_FOUND;
    case HttpStatus::kMethodNotAllowed:
      return NX_WEB_HTTP_STATUS_METHOD_NOT_ALLOWED;
    case HttpStatus::kUnsupportedMediaType:
      return NX_WEB_HTTP_STATUS_UNSUPPORTED_MEDIA;
    case HttpStatus::kUnprocessableEntity:
      return "422 Unprocessable Entity";  // Not defined by NetX.
    case HttpStatus::kInternalError:
      break;
  }
  return NX_WEB_HTTP_STATUS_INTERNAL_ERROR;
}

constexpr bool EqualsIgnoreCase(char lhs, char rhs) noexcept {
  const auto lower = [](char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
  };
  return lower(lhs) == lower(rhs);
}

constexpr bool StartsWithIgnoreCase(std::string_view value,
                                    std::string_view prefix) noexcept {
  if (value.size() < prefix.size()) {
    return false;
  }
  for (std::size_t i = 0; i < prefix.size(); ++i) {
    if (!EqualsIgnoreCase(value[i], prefix[i])) {
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

// The media type of the Content-Type header, without parameters
// ("application/json; charset=utf-8" -> "application/json").
constexpr std::string_view ContentType(std::string_view headers) noexcept {
  constexpr std::string_view kHeader = "Content-Type:";
  std::size_t position = 0;
  while (position < headers.size()) {
    std::size_t line_end = headers.find("\r\n", position);
    if (line_end == std::string_view::npos) {
      line_end = headers.size();
    }
    std::string_view line = headers.substr(position, line_end - position);
    if (StartsWithIgnoreCase(line, kHeader)) {
      line.remove_prefix(kHeader.size());
      return Trim(line.substr(0, line.find(';')));
    }
    position = line_end + 2;
  }
  return {};
}

static_assert(ContentType("Host: x\r\ncontent-type: a/b; q=1") == "a/b");
static_assert(ContentType("Host: x").empty());

// Copies the start of the request (the first packet) into `buffer` and
// returns the header block, without the blank line. Empty if there is no
// complete header block in it.
std::string_view ReadHeaders(NX_PACKET* packet,
                             std::span<char> buffer) noexcept {
  ULONG copied = 0;
  if (packet == nullptr ||
      nx_packet_data_extract_offset(packet, 0, buffer.data(), buffer.size(),
                                    &copied) != NX_SUCCESS) {
    return {};
  }
  const std::string_view data{buffer.data(), copied};
  const std::size_t end = data.find("\r\n\r\n");
  return end == std::string_view::npos ? std::string_view{}
                                       : data.substr(0, end);
}

// Reads the whole body (Content-Length bytes) into `buffer`. NetX receives
// further TCP segments as needed; a body split over several packets is
// common for POST. An absent Content-Length means no body.
bool ReadBody(NX_WEB_HTTP_SERVER* server, NX_PACKET* packet,
              std::span<std::uint8_t> buffer,
              std::span<const std::uint8_t>& body) noexcept {
  body = {};
  ULONG length = 0;
  if (nx_web_http_server_content_length_get(packet, &length) != NX_SUCCESS ||
      length == 0) {
    return true;
  }
  if (length > buffer.size()) {
    return false;
  }
  ULONG received = 0;
  while (received < length) {
    UINT chunk = 0;
    if (nx_web_http_server_content_get_extended(
            server, packet, received,
            reinterpret_cast<CHAR*>(buffer.data() + received),
            static_cast<UINT>(length - received), &chunk) != NX_SUCCESS ||
        chunk == 0) {
      return false;
    }
    received += chunk;
  }
  body = buffer.first(length);
  return true;
}

UINT SendResponse(NX_WEB_HTTP_SERVER* server,
                  const HttpResponse& response) noexcept {
  const std::string_view body{response.body != nullptr ? response.body : "",
                              response.body_size};
  NX_PACKET* packet = nullptr;
  UINT status = nx_web_http_server_callback_generate_response_header(
      server, &packet, const_cast<CHAR*>(ToStatusLine(response.status)),
      static_cast<UINT>(body.size()),
      const_cast<CHAR*>(response.content_type.data()), NX_NULL);
  if (status != NX_SUCCESS) {
    return status;
  }
  status = nx_packet_data_append(
      packet, const_cast<CHAR*>(body.data()), static_cast<ULONG>(body.size()),
      server->nx_web_http_server_packet_pool_ptr, NX_WAIT_FOREVER);
  if (status == NX_SUCCESS) {
    status = nx_web_http_server_callback_packet_send(server, packet);
  }
  if (status != NX_SUCCESS) {
    static_cast<void>(nx_packet_release(packet));
    return status;
  }
  return NX_WEB_HTTP_CALLBACK_COMPLETED;
}

}  // namespace

UINT HandleRequest(const HttpServer& http_server, NX_WEB_HTTP_SERVER* server,
                   UINT request_type, CHAR* resource,
                   NX_PACKET* packet) noexcept {
  std::array<char, kMaxHeaderSize> header_buffer;
  // FlatBuffers are read in place: keep the body aligned.
  alignas(8) std::array<std::uint8_t, kMaxBodySize> body_buffer;
  HttpResponse response;
  const std::string_view path =
      resource != nullptr ? std::string_view(resource) : "";

  const std::string_view headers = ReadHeaders(packet, header_buffer);
  std::span<const std::uint8_t> body;
  if (headers.empty() || !ReadBody(server, packet, body_buffer, body)) {
    logging::Warning("web", "unreadable request for {} (headers > {} B or "
                     "body > {} B?)", path, kMaxHeaderSize, kMaxBodySize);
    response.SetText(HttpStatus::kBadRequest, "Bad Request");
    return SendResponse(server, response);
  }

  const HttpRequest request{
      .method = ToHttpMethod(request_type),
      .path = path,
      .content_type = ContentType(headers),
      .body = {reinterpret_cast<const char*>(body.data()), body.size()},
  };
  http_server.HandleRequest(request, response);
  return SendResponse(server, response);
}

}  // namespace webserver::netx_duo
