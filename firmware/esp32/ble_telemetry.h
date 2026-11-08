#pragma once
#include "telemetry_snapshot.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace ble_telemetry {
constexpr char kServiceUuid[] = "07975e87-679e-531b-b48b-d227f4570000";
constexpr size_t kCapacity = 96;
constexpr uint32_t kNotifyIntervalMs = 2000;
constexpr uint32_t kFreshnessMs = 4000;
// Owner publishes at least every 100 ms during normal service. Bound the
// actuator observation age separately from sensor acquisition freshness.
constexpr uint32_t kPublicationMaxAgeMs = 250;
struct Packet { char bytes[kCapacity] = {}; uint16_t size = 0; };
// Internal mailbox data, never sent on the wire. Retain acquisition time so
// freshness does not depend on the owner continuing to publish.
using Snapshot = telemetry_snapshot::Snapshot;
inline void number(char (&out)[16], float value, bool valid, float low, float high) {
  if (!valid || !isfinite(value) || value < low || value > high) {
    memcpy(out, "null", 5);
  } else {
    snprintf(out, sizeof(out), "%.1f", static_cast<double>(value));
  }
}
inline Packet serialize(const Snapshot& snapshot, uint32_t now) {
  const auto& r = snapshot.reading;
  const bool fresh = static_cast<uint32_t>(now - r.sampledAtMs) <= kFreshnessMs;
  char soil[16], temp[16], hum[16], water[16];
  number(soil, r.soilPercent, fresh && r.soilValid, 0, 100);
  number(temp, r.temperatureC, fresh && r.dhtValid, 0, 50);
  number(hum, r.humidityPercent, fresh && r.dhtValid, 0, 100);
  number(water, snapshot.waterPercent, fresh && snapshot.waterValid && r.distanceValid, 0, 100);
  Packet p;
  // Calibrated operational %, not water-column cm or actual tank volume %.
  const int n = snprintf(p.bytes, sizeof(p.bytes),
      "{\"soil\":%s,\"temp\":%s,\"hum\":%s,\"water\":%s,\"pump\":%u}",
      soil, temp, hum, water, snapshot.actuatorOn ? 1u : 0u);
  if (n > 0 && static_cast<size_t>(n) < sizeof(p.bytes)) p.size = static_cast<uint16_t>(n);
  else p.bytes[0] = 0;  // Never publish truncated JSON.
  return p;
}
inline Packet serve(const Snapshot& snapshot, uint32_t now) {
  if (static_cast<uint32_t>(now - snapshot.publishedAtMs) > kPublicationMaxAgeMs) return Packet{};
  return serialize(snapshot, now);
}
inline bool identity(const uint8_t (&mac)[6], char (&id)[18], char (&name)[16]) {
  bool any = false, allFF = true;
  for (uint8_t b : mac) { any |= b != 0; allFF &= b == 0xff; }
  if (!any || allFF || (mac[0] & 1)) return false;
  snprintf(id, sizeof(id), "%02X:%02X:%02X:%02X:%02X:%02X",
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  snprintf(name, sizeof(name), "WS-%02X%02X%02X%02X%02X%02X",
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return true;
}
// Owned exclusively by the BLE worker; callbacks enqueue bounded events.
class Session {
 public:
  bool connected = false, subscribed = false, resultsSubscribed = false, congested = false;
  uint16_t connection = 0, mtu = 23;
  bool connect(uint16_t id, uint32_t now) {
    if (connected) return false;
    *this = Session{}; connected = true; connection = id; lastNotify_ = now; return true;
  }
  void disconnect(uint16_t id) { if (matches(id)) *this = Session{}; }
  bool matches(uint16_t id) const { return connected && connection == id; }
  void setMtu(uint16_t id, uint16_t value) { if (matches(id) && value >= 23) mtu = value; }
  bool subscribe(uint16_t id, uint16_t value, bool results) {
    if (!matches(id) || value > 1) return false;  // Notify only, no indications.
    (results ? resultsSubscribed : subscribed) = value == 1;
    return true;
  }
  enum class Notify { None, TooLarge, Send };
  Notify notification(uint32_t now, size_t length) {
    if (static_cast<uint32_t>(now - lastNotify_) < kNotifyIntervalMs) return Notify::None;
    lastNotify_ = now;  // No catch-up burst, even after skipped/congested periods.
    if (!connected || !subscribed || congested || !length) return Notify::None;
    return length > static_cast<size_t>(mtu - 3) ? Notify::TooLarge : Notify::Send;
  }
  // Pin one whole snapshot across ATT Read/Read Blob requests, even if the
  // mailbox changes. This is ATT long-read handling, not a new wire protocol.
  enum class ReadResult { Ok, Unavailable, InvalidOffset };
  ReadResult readResult(const Packet& latest, uint16_t offset, const char*& data, uint16_t& length) {
    data = nullptr; length = 0;
    if (!offset && !latest.size) { haveRead_ = false; return ReadResult::Unavailable; }
    if (!offset) { read_ = latest; haveRead_ = true; }
    if (!haveRead_ || offset > read_.size) return ReadResult::InvalidOffset;
    const uint16_t left = read_.size - offset;
    length = left < mtu - 1 ? left : mtu - 1;
    data = read_.bytes + offset;
    return ReadResult::Ok;
  }
  bool read(const Packet& latest, uint16_t offset, const char*& data, uint16_t& length) {
    return readResult(latest, offset, data, length) == ReadResult::Ok;
  }
 private:
  uint32_t lastNotify_ = 0;
  bool haveRead_ = false;
  Packet read_;
};
}
