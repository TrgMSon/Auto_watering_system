#include "../esp32/ble_telemetry.h"
#include <cassert>
#include "telemetry_water_fixture.h"
#include <iostream>
#include <limits>
#include <string>
using namespace ble_telemetry;
// Legacy fixtures have no confirmed water conversion; retain their null checks.
Packet serialize(const sensors::Reading& reading,bool pump,uint32_t now){
  Snapshot snapshot;snapshot.reading=reading;snapshot.actuatorOn=pump;
  return ble_telemetry::serialize(snapshot,now);
}
sensors::Reading reading(uint32_t now) {
  sensors::Reading r; r.sampledAtMs = now;
  r.soilValid = r.dhtValid = r.distanceValid = true;
  r.soilPercent = 42.5f; r.temperatureC = 31.8f; r.humidityPercent = 68;
  r.distanceCm = 25.4f; return r;
}
int main() {
  water_test::verify();
  auto r = reading(100);
  auto p = serialize(r, false, 100);
  assert(std::string(p.bytes) == "{\"soil\":42.5,\"temp\":31.8,\"hum\":68.0,\"water\":null,\"pump\":0}");
  assert(p.size == strlen(p.bytes) && p.size < kCapacity);
  p = serialize(r, true, 100);
  assert(std::string(p.bytes).find("\"pump\":1}") != std::string::npos);
  // Valid SRF05 distance is never emitted as uncalibrated water-column height.
  assert(std::string(p.bytes).find("\"water\":null") != std::string::npos);
  r.soilPercent = std::numeric_limits<float>::infinity(); r.temperatureC = NAN; r.humidityPercent = -1;
  p = serialize(r, false, 100);
  const std::string empty = "{\"soil\":null,\"temp\":null,\"hum\":null,\"water\":null,\"pump\":0}";
  assert(p.bytes == empty);
  r = reading(100); r.soilValid = r.dhtValid = false;
  assert(serialize(r, false, 100).bytes == empty);
  r = reading(100); r.dhtValid = false;
  assert(std::string(serialize(r, true, 100).bytes) ==
      "{\"soil\":42.5,\"temp\":null,\"hum\":null,\"water\":null,\"pump\":1}");
  r = reading(100);
  assert(serialize(r, false, 4100).bytes != empty);
  assert(serialize(r, false, 4101).bytes == empty);
  r = reading(UINT32_MAX - 100);
  assert(serialize(r, false, 10).bytes != empty);
  r.soilPercent = 100; r.temperatureC = 50; r.humidityPercent = 100;
  p = serialize(r, true, 10); assert(p.size < kCapacity && strlen(p.bytes) == p.size);
  r.soilPercent = std::numeric_limits<float>::max(); r.temperatureC = -INFINITY; r.humidityPercent = 101;
  assert(serialize(r, false, 10).bytes == empty);

  const uint8_t mac[6] = {0x24,0x6f,0x28,0x01,0xab,0x09}; char id[18], name[16];
  assert(identity(mac, id, name));
  assert(std::string(id) == "24:6F:28:01:AB:09" && std::string(name) == "WS-246F2801AB09");
  const uint8_t zero[6] = {}; assert(!identity(zero, id, name));
  const uint8_t multicast[6] = {1,2,3,4,5,6}; assert(!identity(multicast, id, name));

  // Publications continue but sensor acquisition stops: sensor age is separate.
  for (uint32_t start : {100u, UINT32_MAX - 1000}) {
    Snapshot mailbox; mailbox.reading = reading(start); mailbox.publishedAtMs = start;
    Session worker; assert(worker.connect(1, start)); worker.setMtu(1, 185);
    assert(worker.subscribe(1, 1, false));
    for (uint32_t elapsed : {2000u, 4000u, 6000u, 8000u}) {
      mailbox.publishedAtMs = start + elapsed; // Owner alive, sensor acquisition stopped.
      const Packet notification = serve(mailbox, start + elapsed);
      assert(worker.notification(start + elapsed, notification.size) == Session::Notify::Send);
      assert((notification.bytes == empty) == (elapsed > kFreshnessMs));
    }
    const char* bytes; uint16_t count;
    mailbox.publishedAtMs = start + 4000;
    const Packet atBoundary = serve(mailbox, start + 4000);
    assert(worker.read(atBoundary, 0, bytes, count));
    assert(std::string(bytes, count) != empty);
    const Packet expired = serve(mailbox, start + 4001);
    assert(worker.read(expired, 0, bytes, count));
    assert(std::string(bytes, count) == empty);
    // A new publication of the same old sample must not renew freshness.
    mailbox.publishedAtMs = start + 9000;
    assert(serve(mailbox, start + 9000).bytes == empty);
    // Recovery requires a new acquisition, not a new publication timestamp.
    mailbox.reading = reading(start + 9000);
    assert(serve(mailbox, start + 9000).bytes != empty);
    // Read Blob pins freshness at initial READ; crossing expiry cannot mix JSON.
    worker.setMtu(1, 23);
    assert(worker.read(atBoundary, 0, bytes, count));
    std::string whole(bytes, count);
    for (uint16_t offset = count; offset < atBoundary.size; offset += count) {
      assert(worker.read(expired, offset, bytes, count) && count > 0);
      whole.append(bytes, count);
    }
    assert(whole == atBoundary.bytes);
  }

  // Owner publishes pump ON once, then stops. Worker must not keep serving it.
  for (uint32_t start : {100u, UINT32_MAX - 100}) {
    Snapshot mailbox; mailbox.reading = reading(start); mailbox.actuatorOn = true;
    mailbox.publishedAtMs = start;
    Session worker; worker.connect(1, start); worker.setMtu(1, 23); worker.subscribe(1, 1, false);
    const Packet boundary = serve(mailbox, start + kPublicationMaxAgeMs);
    assert(boundary.size && std::string(boundary.bytes).find("\"pump\":1}") != std::string::npos);
    const char* bytes; uint16_t count;
    assert(worker.read(boundary, 0, bytes, count));
    std::string pinned(bytes, count);
    const Packet expired = serve(mailbox, start + kPublicationMaxAgeMs + 1);
    assert(expired.size == 0 && expired.bytes[0] == 0);
    // Existing Read Blob remains pinned across publication expiry.
    for (uint16_t offset = count; offset < boundary.size; offset += count) {
      assert(worker.read(expired, offset, bytes, count) && count > 0); pinned.append(bytes, count);
    }
    assert(pinned == boundary.bytes);
    assert(worker.readResult(expired, 0, bytes, count) == Session::ReadResult::Unavailable);
    assert(bytes == nullptr && count == 0);
    for (uint32_t elapsed : {2000u, 4000u, 6000u}) {
      const Packet notification = serve(mailbox, start + elapsed);
      assert(notification.size == 0);
      assert(worker.notification(start + elapsed, notification.size) == Session::Notify::None);
    }
    // Fresh publication restores access without fabricating OFF; old sensor stays null.
    mailbox.publishedAtMs = start + 8000;
    const Packet restored = serve(mailbox, start + 8000);
    assert(std::string(restored.bytes) ==
        "{\"soil\":null,\"temp\":null,\"hum\":null,\"water\":null,\"pump\":1}");
    worker.setMtu(1, 185);
    assert(worker.read(restored, 0, bytes, count) && count == restored.size);
    assert(worker.notification(start + 8000, restored.size) == Session::Notify::Send);
  }

  for (uint32_t start : {0u, UINT32_MAX - 1000}) {
    Session s;
    assert(s.notification(start, 50) == Session::Notify::None);
    assert(s.connect(7, start)); assert(!s.connect(8, start));
    assert(s.mtu == 23 && !s.subscribed && !s.resultsSubscribed);
    assert(!s.subscribe(8, 1, false)); assert(!s.subscribe(7, 2, false));
    assert(s.notification(start + 2000, 50) == Session::Notify::None);
    assert(s.subscribe(7, 1, false));
    assert(s.notification(start + 4000, 21) == Session::Notify::TooLarge);
    assert(s.notification(start + 6000, 20) == Session::Notify::Send);
    s.setMtu(8, 185); assert(s.mtu == 23);
    s.setMtu(7, 185);
    s.setMtu(7, 22); assert(s.mtu == 185);
    assert(s.notification(start + 8000, 182) == Session::Notify::Send);
    assert(s.notification(start + 10000, 183) == Session::Notify::TooLarge);
    assert(s.notification(start + 30000, 50) == Session::Notify::Send);
    assert(s.notification(start + 30000, 50) == Session::Notify::None); // no catch-up
    s.congested = true;
    assert(s.notification(start + 32000, 50) == Session::Notify::None);
    s.congested = false;
    assert(s.subscribe(7, 0, false));
    assert(s.notification(start + 34000, 50) == Session::Notify::None);
    assert(s.subscribe(7, 1, true));
    s.disconnect(8); assert(s.connected);
    s.disconnect(7); assert(!s.connected && !s.subscribed && !s.resultsSubscribed && s.mtu == 23);
    assert(s.connect(7, start + 35000)); // Reused connection ID cannot inherit subscription.
    assert(s.notification(start + 37000, 50) == Session::Notify::None);
  }
  // Read Blob remains consistent if a newer snapshot arrives between requests.
  Session s; s.connect(1, 0);
  p = serialize(reading(0), false, 0);
  Packet changed = serialize(sensors::Reading{}, true, 5000);
  const char* data; uint16_t length;
  assert(!s.read(p, 22, data, length));
  std::string assembled;
  for (uint16_t offset = 0; offset < p.size; offset += length) {
    assert(s.read(offset ? changed : p, offset, data, length));
    assert(length > 0 && length <= 22); assembled.append(data, length);
  }
  assert(assembled == p.bytes);
  assert(s.read(changed, p.size, data, length) && length == 0);
  assert(!s.read(changed, p.size + 1, data, length));
  assert(s.read(changed, 0, data, length));
  s.setMtu(1, 185);
  assert(s.read(changed, 0, data, length) && length == changed.size);
  s.disconnect(1); s.connect(1, 1);
  assert(!s.read(p, 22, data, length));
  std::cout << "PASS: BLE JSON/null/bounds/identity, session/subscription/MTU/cadence, reconnect and pinned long reads\n";
}

