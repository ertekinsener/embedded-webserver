#include "netx_duo_adapter.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "logging/logger.h"
#include "nx_api.h"
#include "nx_web_http_server.h"
#include "webserver/http_headers.h"
#include "webserver/http_types.h"

namespace webserver::netx_duo {
namespace {

// Body bytes per packet sent. Big files go out in several packets, so the
// HTTP packet pool never has to hold a whole file.
constexpr std::size_t kSendChunkSize = 2048;

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

enum class BodyStatus : std::uint8_t { kOk, kTooLarge, kUnreadable };

// Reads the whole body (Content-Length bytes) into `buffer`. NetX receives
// further TCP segments as needed; a body split over several packets is
// common for POST. An absent Content-Length means no body.
BodyStatus ReadBody(NX_WEB_HTTP_SERVER* server, NX_PACKET* packet,
                    std::span<std::uint8_t> buffer,
                    std::span<const std::uint8_t>& body) noexcept {
  body = {};
  ULONG length = 0;
  if (nx_web_http_server_content_length_get(packet, &length) != NX_SUCCESS ||
      length == 0) {
    return BodyStatus::kOk;
  }
  if (length > buffer.size()) {
    return BodyStatus::kTooLarge;
  }
  ULONG received = 0;
  while (received < length) {
    UINT chunk = 0;
    if (nx_web_http_server_content_get_extended(
            server, packet, received,
            reinterpret_cast<CHAR*>(buffer.data() + received),
            static_cast<UINT>(length - received), &chunk) != NX_SUCCESS ||
        chunk == 0) {
      return BodyStatus::kUnreadable;
    }
    received += chunk;
  }
  body = buffer.first(length);
  return BodyStatus::kOk;
}

// NetX takes the header values as non-const C strings it does not modify.
CHAR* AsNetx(std::string_view text) noexcept {
  return const_cast<CHAR*>(text.data());
}

}  // namespace

UINT Adapter::HandleRequest(NX_WEB_HTTP_SERVER* server, UINT request_type,
                            CHAR* resource, NX_PACKET* packet) noexcept {
  response_.Reset();
  const std::string_view target =
      resource != nullptr ? std::string_view(resource) : "";

  const std::string_view headers = ReadHeaders(packet, header_buffer_);
  if (headers.empty()) {
    logging::Warning("web", "{}: headers larger than {} B", target,
                     kMaxRequestHeaderSize);
    response_.SetText(HttpStatus::kBadRequest, "Bad Request");
    return SendResponse(server);
  }
  std::span<const std::uint8_t> body;
  switch (ReadBody(server, packet, body_buffer_, body)) {
    case BodyStatus::kOk:
      break;
    case BodyStatus::kTooLarge:
      response_.SetText(HttpStatus::kPayloadTooLarge, "Payload Too Large");
      return SendResponse(server);
    case BodyStatus::kUnreadable:
      response_.SetText(HttpStatus::kBadRequest, "Bad Request");
      return SendResponse(server);
  }

  const HttpRequest request{
      .method = ToHttpMethod(request_type),
      .path = http::PathOf(target),
      .content_type =
          http::MediaType(http::FindHeader(headers, "Content-Type")),
      .authorization = http::FindHeader(headers, "Authorization"),
      .body = body,
  };
  server_.HandleRequest(request, response_);
  return SendResponse(server);
}

UINT Adapter::SendResponse(NX_WEB_HTTP_SERVER* server) noexcept {
  std::array<char, 256> extra;
  const std::string_view extra_headers =
      http::FormatExtraHeaders(response_, extra);
  const std::string_view status_line = StatusLine(response_.status);
  std::span<const std::uint8_t> body = response_.body();

  NX_PACKET* packet = nullptr;
  UINT status = nx_web_http_server_callback_generate_response_header_extended(
      server, &packet, AsNetx(status_line),
      static_cast<UINT>(status_line.size()), static_cast<UINT>(body.size()),
      AsNetx(response_.content_type),
      static_cast<UINT>(response_.content_type.size()),
      AsNetx(extra_headers), static_cast<UINT>(extra_headers.size()));
  if (status != NX_SUCCESS) {
    return status;
  }
  NX_PACKET_POOL* const pool = server->nx_web_http_server_packet_pool_ptr;
  // The first packet carries the header (and nothing else for an empty
  // body); every further one up to kSendChunkSize body bytes.
  while (true) {
    const std::span<const std::uint8_t> chunk =
        body.first(std::min(body.size(), kSendChunkSize));
    if (!chunk.empty()) {
      status = nx_packet_data_append(
          packet, const_cast<std::uint8_t*>(chunk.data()),
          static_cast<ULONG>(chunk.size()), pool, NX_WAIT_FOREVER);
    }
    if (status == NX_SUCCESS) {
      status = nx_web_http_server_callback_packet_send(server, packet);
    }
    if (status != NX_SUCCESS) {
      static_cast<void>(nx_packet_release(packet));
      return status;
    }
    body = body.subspan(chunk.size());
    if (body.empty()) {
      return NX_WEB_HTTP_CALLBACK_COMPLETED;
    }
    status = nx_packet_allocate(pool, &packet, NX_TCP_PACKET, NX_WAIT_FOREVER);
    if (status != NX_SUCCESS) {
      return status;
    }
  }
}

}  // namespace webserver::netx_duo
