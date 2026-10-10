#include "zephyr_adapter.h"

#include <zephyr/net/http/server.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "webserver/auth/session_manager.h"
#include "webserver/http_headers.h"
#include "webserver/http_types.h"

// Longer header values are dropped by Zephyr before the callback sees them.
static_assert(CONFIG_HTTP_SERVER_MAX_HEADER_LEN >=
                  sizeof("Bearer ") +
                      webserver::auth::SessionManager::kTokenLength,
              "Set CONFIG_HTTP_SERVER_MAX_HEADER_LEN=64 for the session "
              "token in the Authorization header");

HTTP_SERVER_REGISTER_HEADER_CAPTURE(webserver_authorization, "Authorization");
HTTP_SERVER_REGISTER_HEADER_CAPTURE(webserver_content_type, "Content-Type");

namespace webserver::zephyr {
namespace {

HttpMethod ToHttpMethod(enum http_method method) noexcept {
  switch (method) {
    case HTTP_GET:
      return HttpMethod::kGet;
    case HTTP_HEAD:
      return HttpMethod::kHead;
    case HTTP_POST:
      return HttpMethod::kPost;
    case HTTP_PUT:
      return HttpMethod::kPut;
    case HTTP_DELETE:
      return HttpMethod::kDelete;
    default:
      return HttpMethod::kUnknown;
  }
}

bool HasBody(enum http_method method) noexcept {
  return method == HTTP_POST || method == HTTP_PUT;
}

void Reset(Endpoint& endpoint) noexcept {
  endpoint.body_size = 0;
  endpoint.too_large = false;
  endpoint.started = false;
  endpoint.authorization_size = 0;
  endpoint.content_type_size = 0;
}

template <std::size_t kSize>
void Store(std::string_view value, std::array<char, kSize>& buffer,
           std::size_t& size) noexcept {
  if (value.size() <= buffer.size()) {
    value.copy(buffer.data(), value.size());
    size = value.size();
  }
}

// The captured headers come with the first callback of a request only.
void CaptureHeaders(const http_request_ctx& request,
                    Endpoint& endpoint) noexcept {
  if (request.headers_status != HTTP_HEADER_STATUS_OK) {
    return;
  }
  for (std::size_t i = 0; i < request.header_count; ++i) {
    const http_header& header = request.headers[i];
    if (http::EqualsIgnoreCase(header.name, "Authorization")) {
      Store(header.value, endpoint.authorization,
            endpoint.authorization_size);
    } else if (http::EqualsIgnoreCase(header.name, "Content-Type")) {
      Store(http::MediaType(header.value), endpoint.content_type,
            endpoint.content_type_size);
    }
  }
}

void AppendBody(const http_request_ctx& request,
                Endpoint& endpoint) noexcept {
  if (request.data_len > endpoint.body.size() - endpoint.body_size) {
    endpoint.too_large = true;
    return;
  }
  std::memcpy(endpoint.body.data() + endpoint.body_size, request.data,
              request.data_len);
  endpoint.body_size += request.data_len;
}

}  // namespace

int Adapter::Callback(http_client_ctx* client,
                      enum http_transaction_status status,
                      const http_request_ctx* request_ctx,
                      http_response_ctx* response_ctx, void* user_data) {
  auto* endpoint = static_cast<Endpoint*>(user_data);
  return endpoint->adapter.Handle(client, status, request_ctx, response_ctx,
                                  *endpoint);
}

int Adapter::Handle(http_client_ctx* client,
                    enum http_transaction_status status,
                    const http_request_ctx* request_ctx,
                    http_response_ctx* response_ctx,
                    Endpoint& endpoint) noexcept {
  if (status == HTTP_SERVER_TRANSACTION_ABORTED ||
      status == HTTP_SERVER_TRANSACTION_COMPLETE) {
    Reset(endpoint);
    return 0;
  }
  if (!endpoint.started) {
    endpoint.started = true;
    CaptureHeaders(*request_ctx, endpoint);
  }
  // For GET and DELETE the data is the query string, not a body.
  if (HasBody(client->method) && !endpoint.too_large) {
    AppendBody(*request_ctx, endpoint);
  }
  if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
    return 0;  // Answer once the whole body is in.
  }

  response_.Reset();
  if (endpoint.too_large) {
    response_.SetText(HttpStatus::kPayloadTooLarge, "Payload Too Large");
  } else {
    const auto* url = reinterpret_cast<const char*>(client->url_buffer);
    const HttpRequest request{
        .method = ToHttpMethod(client->method),
        .path = http::PathOf(url),
        .content_type = {endpoint.content_type.data(),
                         endpoint.content_type_size},
        .authorization = {endpoint.authorization.data(),
                          endpoint.authorization_size},
        .body = std::span(endpoint.body).first(endpoint.body_size),
    };
    server_.HandleRequest(request, response_);
  }
  Respond(response_ctx);
  return 0;
}

void Adapter::Respond(http_response_ctx* response_ctx) noexcept {
  std::size_t count = 0;
  const auto add = [&](const char* name, std::string_view value) {
    if (!value.empty()) {
      // The header values are NUL-terminated literals (see HttpResponse).
      headers_[count++] = {.name = name, .value = value.data()};
    }
  };
  add("Content-Type", response_.content_type);
  add("Content-Encoding", response_.content_encoding);
  add("Cache-Control", response_.cache_control);
  add("X-Content-Type-Options", "nosniff");

  response_ctx->status =
      static_cast<enum http_status>(static_cast<int>(response_.status));
  response_ctx->headers = headers_.data();
  response_ctx->header_count = count;
  response_ctx->body = response_.body().data();
  response_ctx->body_len = response_.body().size();
  response_ctx->final_chunk = true;
}

}  // namespace webserver::zephyr
