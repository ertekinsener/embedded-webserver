#ifndef WEBSERVER_INCLUDE_WEBSERVER_FLATBUFFER_IO_
#define WEBSERVER_INCLUDE_WEBSERVER_FLATBUFFER_IO_

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "flatbuffers/flatbuffer_builder.h"
#include "flatbuffers/verifier.h"
#include "webserver/http_types.h"

// Reading FlatBuffer requests and building FlatBuffer responses without a
// heap.
namespace webserver {

// flatbuffers::Allocator over one fixed buffer. The builder must be created
// with the arena size as initial size; it then never reallocates.
// FlatBuffers cannot report a failed allocation, so outgrowing the arena
// traps: callers bound the size of what they build (see the k*Bytes
// estimates of the API handlers).
class FlatBufferArena final : public flatbuffers::Allocator {
 public:
  explicit FlatBufferArena(std::span<std::uint8_t> storage) noexcept
      : storage_(storage) {}

  std::uint8_t* allocate(std::size_t size) override {
    if (in_use_ || size > storage_.size()) {
      __builtin_trap();
    }
    in_use_ = true;
    return storage_.data();
  }

  void deallocate(std::uint8_t* /*pointer*/, std::size_t /*size*/) override {
    in_use_ = false;
  }

  std::size_t size() const noexcept { return storage_.size(); }

 private:
  std::span<std::uint8_t> storage_;
  bool in_use_ = false;
};

// A FlatBufferBuilder writing into the response buffer:
//
//   ResponseBuilder out(response);
//   out.builder.Finish(api::CreateFoo(out.builder, ...));
//   out.Send(HttpStatus::kOk);
struct ResponseBuilder {
  explicit ResponseBuilder(HttpResponse& http_response) noexcept
      : response(http_response),
        arena(http_response.buffer()),
        builder(arena.size(), &arena, false, 8) {}

  // The finished buffer becomes the body. It stays valid after the builder
  // is gone: freeing it only marks the arena unused.
  void Send(HttpStatus status) noexcept {
    response.SetBody(status, {builder.GetBufferPointer(), builder.GetSize()},
                     kFlatBufferType);
  }

  HttpResponse& response;
  FlatBufferArena arena;
  flatbuffers::FlatBufferBuilder builder;
};

// The root table of a FlatBuffer request body, or nullptr after answering
// 415 (wrong content type) or 400 (not a valid T). The body comes from the
// network, so every offset is verified before anything is read.
template <typename T>
const T* DecodeRequest(const HttpRequest& request,
                       HttpResponse& response) noexcept {
  if (request.content_type != kFlatBufferType) {
    response.SetText(HttpStatus::kUnsupportedMediaType,
                     "Unsupported Media Type");
    return nullptr;
  }
  flatbuffers::Verifier verifier(request.body.data(), request.body.size());
  if (request.body.empty() || !verifier.VerifyBuffer<T>(nullptr)) {
    response.SetText(HttpStatus::kBadRequest, "Invalid FlatBuffer");
    return nullptr;
  }
  return flatbuffers::GetRoot<T>(request.body.data());
}

inline std::string_view ToStringView(
    const flatbuffers::String* text) noexcept {
  return text == nullptr ? std::string_view{}
                         : std::string_view(text->c_str(), text->size());
}

}  // namespace webserver

#endif  // WEBSERVER_INCLUDE_WEBSERVER_FLATBUFFER_IO_
