#include "webserver/settings/settings_store.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "flatbuffers/flatbuffer_builder.h"
#include "flatbuffers/verifier.h"
#include "logging/logger.h"
#include "settings_generated.h"
#include "webserver/flatbuffer_io.h"
#include "webserver/uart/uart_codec.h"

namespace webserver {
namespace {

template <typename T>
std::span<const std::uint8_t> Bytes(const flatbuffers::Vector<T>* vector) {
  return vector == nullptr
             ? std::span<const std::uint8_t>{}
             : std::span<const std::uint8_t>(vector->data(), vector->size());
}

}  // namespace

bool SettingsStore::Restore(std::span<std::uint8_t> scratch) noexcept {
  if (storage_.load == nullptr || scratch.size() < kMaxSize) {
    return false;
  }
  const std::size_t size =
      storage_.load(storage_.context, scratch.first(kMaxSize));
  flatbuffers::Verifier verifier(scratch.data(), size);
  if (size == 0 || !verifier.VerifyBuffer<api::SavedSettings>(nullptr)) {
    if (size != 0) {
      logging::Warning("settings", "saved settings are invalid, ignored");
    }
    return false;
  }
  const auto* saved = flatbuffers::GetRoot<api::SavedSettings>(scratch.data());

  std::size_t refused = 0;
  if (const auto* ports = saved->uart()) {
    for (const api::UartSettings* port : *ports) {
      refused += uart_.Apply(ToUartConfig(*port)) ? 0 : 1;
    }
  }
  if (const auto* bridges = saved->bridges()) {
    for (const api::BridgeSettings* bridge : *bridges) {
      const auto settings = ToBridgeSettings(*bridge);
      refused +=
          settings && uart_.ApplyBridge(bridge->port_id(), *settings) ? 0 : 1;
    }
  }
  if (const api::SavedPassword* password = saved->password()) {
    if (!sessions_.SetPasswordKey(Bytes(password->salt()),
                                  password->iterations(),
                                  Bytes(password->derived_key()))) {
      logging::Warning("settings", "saved password is invalid, ignored");
    }
  }
  logging::Info("settings", "restored ({} refused)", refused);
  return true;
}

bool SettingsStore::Save(std::span<std::uint8_t> scratch) noexcept {
  if (!available() || scratch.size() < kMaxSize) {
    return false;
  }
  std::span<const std::uint8_t> data;
  {
    const UartService::Guard guard = uart_.Locked();
    const std::span<const UartPort> ports = uart_.ports();
    if (ports.size() > kMaxUartPorts) {
      return false;
    }
    // Within kMaxSize for at most kMaxUartPorts ports (host-tested), so the
    // arena cannot overflow.
    FlatBufferArena arena(scratch.first(kMaxSize));
    flatbuffers::FlatBufferBuilder builder(arena.size(), &arena, false, 8);

    std::array<flatbuffers::Offset<api::UartSettings>, kMaxUartPorts> uart{};
    std::array<flatbuffers::Offset<api::BridgeSettings>, kMaxUartPorts>
        bridges{};
    std::size_t uart_count = 0;
    std::size_t bridge_count = 0;
    for (const UartPort& port : ports) {
      if (port.saved_config) {
        uart[uart_count++] =
            CreateUartSettings(builder, port.id, *port.saved_config);
      }
      if (port.bridge_driver.apply != nullptr) {
        bridges[bridge_count++] =
            CreateBridgeSettings(builder, port.id, port.bridge);
      }
    }
    flatbuffers::Offset<api::SavedPassword> password;
    if (sessions_.password_changed()) {
      const auth::Credentials& credentials = sessions_.credentials();
      password = api::CreateSavedPassword(
          builder,
          builder.CreateVector(credentials.salt.data(),
                               credentials.salt.size()),
          credentials.iterations,
          builder.CreateVector(credentials.derived_key.data(),
                               credentials.derived_key.size()));
    }
    const auto uart_vector = builder.CreateVector(uart.data(), uart_count);
    const auto bridge_vector =
        builder.CreateVector(bridges.data(), bridge_count);
    builder.Finish(api::CreateSavedSettings(builder, uart_vector,
                                            bridge_vector, password));
    // Stays valid in `scratch` after the builder is gone.
    data = {builder.GetBufferPointer(), builder.GetSize()};
  }
  if (!storage_.save(storage_.context, data)) {
    logging::Error("settings", "saving {} bytes failed", data.size());
    return false;
  }
  return true;
}

bool SettingsStore::Erase() noexcept {
  if (storage_.erase == nullptr || !storage_.erase(storage_.context)) {
    return false;
  }
  logging::Warning("settings", "saved settings deleted");
  return true;
}

}  // namespace webserver
