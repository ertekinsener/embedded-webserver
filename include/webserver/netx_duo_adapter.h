#ifndef APP_INCLUDE_WEBSERVER_NETX_DUO_ADAPTER_
#define APP_INCLUDE_WEBSERVER_NETX_DUO_ADAPTER_


#include <array>
#include <span>
#include <string_view>

#include "nx_api.h"
#include "nx_web_http_server.h"
#include "webserver/http_server.h"
#include "webserver/http_types.h"
namespace webserver {

namespace NetXDuoAdapter {

namespace {
HttpMethod ConvertRequestType(UINT request_type) noexcept {
  switch (request_type) {
    case NX_WEB_HTTP_METHOD_GET:
      return HttpMethod::Get;
    case NX_WEB_HTTP_METHOD_PUT:
      return HttpMethod::Put;
    case NX_WEB_HTTP_METHOD_POST:
      return HttpMethod::Post;
    case NX_WEB_HTTP_METHOD_DELETE:
      return HttpMethod::Delete;
    case NX_WEB_HTTP_METHOD_HEAD:
      return HttpMethod::Head;
    default:
      return HttpMethod::Unknown;
  }
}

constexpr std::string_view ExtractContentType(
    std::string_view http_request) noexcept {
  constexpr std::string_view kHeader = "Content-Type:";
  auto equals_ignore_case = [](char lhs, char rhs) constexpr noexcept {
    if (lhs >= 'A' && lhs <= 'Z') {
      lhs = static_cast<char>(lhs + ('a' - 'A'));
    }
    if (rhs >= 'A' && rhs <= 'Z') {
      rhs = static_cast<char>(rhs + ('a' - 'A'));
    }
    return lhs == rhs;
  };
  auto starts_with_ignore_case =
      [&](std::string_view value, std::string_view prefix) constexpr noexcept {
        if (value.size() < prefix.size()) {
          return false;
        }
        for (std::size_t i = 0; i < prefix.size(); ++i) {
          if (!equals_ignore_case(value[i], prefix[i])) {
            return false;
          }
        }
        return true;
      };
  std::size_t position = 0;
  while (position < http_request.size()) {
    const std::size_t line_end = http_request.find("\r\n", position);
    if (line_end == std::string_view::npos) {
      return {};
    }
    std::string_view line = http_request.substr(position, line_end - position);
    if (starts_with_ignore_case(line, kHeader)) {
      line.remove_prefix(kHeader.size());
      while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
        line.remove_prefix(1);
      }
      const std::size_t semicolon = line.find(';');
      if (semicolon != std::string_view::npos) {
        line = line.substr(0, semicolon);
      }
      while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
        line.remove_suffix(1);
      }
      return line;
    }
    // Empty line = end of HTTP headers.
    if (line.empty()) {
      return {};
    }
    position = line_end + 2;
  }
  return {};
}

struct HttpPacketView {
  std::string_view headers;
  std::string_view body;
};

bool ExtractHttpPacket(NX_PACKET* packet, std::span<char> buffer,
                       HttpPacketView& result) noexcept {
  result = {};

  if (packet == nullptr || buffer.empty()) {
    return false;
  }

  ULONG packet_length = 0;

  if (nx_packet_length_get(packet, &packet_length) != NX_SUCCESS) {
    return false;
  }

  if (packet_length > buffer.size()) {
    return false;
  }

  ULONG bytes_retrieved = packet_length;

  if (nx_packet_data_retrieve(packet, buffer.data(), &bytes_retrieved) !=
      NX_SUCCESS) {
    return false;
  }

  const std::string_view http_data{buffer.data(), bytes_retrieved};

  constexpr std::string_view kHeaderSeparator = "\r\n\r\n";

  const std::size_t separator_pos = http_data.find(kHeaderSeparator);

  if (separator_pos == std::string_view::npos) {
    return false;
  }

  result.headers = http_data.substr(0, separator_pos);

  result.body = http_data.substr(separator_pos + kHeaderSeparator.size());

  return true;
}

}  // namespace

UINT send_response(NX_WEB_HTTP_SERVER* server_ptr, std::string_view content,
                   std::string_view content_type, CHAR* status_code) {
  NX_PACKET* response_packet = nullptr;

  UINT status = nx_web_http_server_callback_generate_response_header(
      server_ptr, &response_packet, status_code,
      static_cast<UINT>(content.size()), const_cast<CHAR*>(content_type.data()),
      NX_NULL);
uint32_t content_length = static_cast<uint32_t>(content.size());
  if (status != NX_SUCCESS) {
    return status;
  }

  status = nx_packet_data_append(
      response_packet, const_cast<CHAR*>(content.data()),
      static_cast<UINT>(content.size()),
      server_ptr->nx_web_http_server_packet_pool_ptr, NX_WAIT_FOREVER);

  if (status != NX_SUCCESS) {
    (void)nx_packet_release(response_packet);
    return status;
  }

  status = nx_web_http_server_callback_packet_send(server_ptr, response_packet);

  if (status != NX_SUCCESS) {
    (void)nx_packet_release(response_packet);
    return status;
  }

  return NX_WEB_HTTP_CALLBACK_COMPLETED;
}

UINT handle_request(HttpServer& http_server, NX_WEB_HTTP_SERVER* server_ptr,
                    UINT request_type, CHAR* resource,
                    NX_PACKET* packet_ptr) noexcept {
  std::array<char, 2048> buffer{};
  HttpPacketView packet_view{};
  const std::string_view requested_resource{resource != nullptr ? resource
                                                                : ""};
  if (!ExtractHttpPacket(packet_ptr, buffer, packet_view)) {
    return NX_WEB_HTTP_CALLBACK_COMPLETED;
  }

  HttpRequest request;

  request.method = ConvertRequestType(request_type);
  request.path = requested_resource;
  request.content_type = ExtractContentType(packet_view.headers);
  request.body = packet_view.body;

  // if (requested_resource.rfind("/api/", 0) == 0) {
  //   ULONG body_length = 0;
  //   std::array<char, HttpResponse::kOwnedCapacity> request_body{};

  //   if (packet_ptr != nullptr) {
  //     if (nx_packet_length_get(packet_ptr, &body_length) != NX_SUCCESS ||
  //         body_length > request_body.size() ||
  //         nx_packet_data_retrieve(packet_ptr, request_body.data(),
  //                                 body_length) != NX_SUCCESS) {
  //       return send_response(server_ptr, "Request body too large",
  //       "text/plain",
  //                            const_cast<CHAR*>(NX_WEB_HTTP_STATUS_BAD_REQUEST));
  //     }
  //   }

  // const bool is_get = request_type == NX_WEB_HTTP_SERVER_GET_REQUEST;
  // const bool is_post = request_type == NX_WEB_HTTP_SERVER_POST_REQUEST;
  // if (!is_get && !is_post) {
  //   return send_response(
  //       server_ptr, "Method Not Allowed", "text/plain",
  //       const_cast<CHAR*>(NX_WEB_HTTP_STATUS_NOT_IMPLEMENTED));
  // }

  HttpResponse response;
  // http_adapter.handle(is_get ? "GET" : "POST", requested_resource,
  //                     is_post ? "application/x-flatbuffers" : "",
  //                     {request_body.data(), is_get ? 0U : body_length},
  //                     response);

  http_server.handle_request(request, response);

  const CHAR* status = NX_WEB_HTTP_STATUS_INTERNAL_ERROR;
  switch (response.status) {
    case 200:
      status = NX_WEB_HTTP_STATUS_OK;
      break;
    case 204:
      status = NX_WEB_HTTP_STATUS_NO_CONTENT;
      break;
    case 400:
      status = NX_WEB_HTTP_STATUS_BAD_REQUEST;
      break;
    case 404:
      status = NX_WEB_HTTP_STATUS_NOT_FOUND;
      break;
    case 405:
      status = NX_WEB_HTTP_STATUS_METHOD_NOT_ALLOWED;
      break;
    default:
      break;
  }

  return send_response(
      server_ptr,
      {response.body != nullptr ? response.body : "", response.body_size},
      response.content_type, const_cast<CHAR*>(status));
}
}  // namespace NetXDuoAdapter
}  // namespace webserver


#endif /* APP_INCLUDE_WEBSERVER_NETX_DUO_ADAPTER_ */
