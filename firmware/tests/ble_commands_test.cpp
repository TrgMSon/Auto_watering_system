#include "pump/mode_fixture.h"
#include "pump/Arduino.h"
#include "../esp32/ble_auth.h"
#include "../esp32/ble_dispatch.h"
#include "../esp32/ble_command_bridge.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <cassert>
#include <iostream>
#include <string>
#include <cstring>
using namespace ble_commands;
uint32_t fakeNow = 0;
int outputLevel = -1, writes = 0, outputModes = 0;
namespace {
// Test fixture only. Production has neither this key nor a default key source.
const char testKey[] = "test-ble-secret-key";
struct Keys : KeySource {
  const char* value = testKey;
  bool load(uint8_t* out, size_t capacity, size_t& size) const override {
    size = strlen(value); if (size > capacity) return false;
    memcpy(out, value, size); return true;
  }
};
struct HostCrypto : Crypto {
  unsigned reads = 0; bool fail = false;
  bool random(uint8_t (&out)[8]) override {
    const uint8_t vector[] = {0x84,0xa9,0xf2,0x1b,0x3c,0x0e,0x5a,0x1d};
    memcpy(out, vector, 8); out[0] += reads++; return !fail;
  }
  bool hmac(const uint8_t* key, size_t size, const char* message, size_t n, uint8_t (&out)[32]) override {
    unsigned length = 0;
    return !fail && HMAC(EVP_sha256(), key, int(size),
        reinterpret_cast<const unsigned char*>(message), n, out, &length) && length == 32;
  }
};
std::string signature(HostCrypto& crypto, const char* key, const std::string& canonical) {
  uint8_t digest[32]; assert(crypto.hmac(reinterpret_cast<const uint8_t*>(key), strlen(key),
      canonical.data(), canonical.size(), digest));
  std::string out; const char hex[] = "0123456789abcdef";
  for (unsigned i = 0; i < 8; ++i) { out += hex[digest[i] >> 4]; out += hex[digest[i] & 15]; }
  return out;
}
std::string payload(const char* op, const char* seconds, const std::string& sig) {
  return std::string("{\"cmd\":\"") + op + "\",\"dur\":" + seconds + ",\"sig\":\"" + sig + "\"}";
}
std::string nonce(Authenticator& auth, uint32_t now) {
  const char* bytes; uint16_t size; assert(auth.readNonce(0, now, bytes, size) && size == 16);
  return std::string(bytes, size);
}
Authentication send(Authenticator& auth, const std::string& p, uint32_t now) {
  return auth.authenticate(p.data(), p.size(), now);
}
sensors::Reading sample(float soil = 20, float distance = 5) {
  sensors::Reading r; r.sampledAtMs = fakeNow; r.soilValid = r.distanceValid = true;
  r.soilPercent = soil; r.distanceCm = distance; return r;
}
Command on(uint32_t duration = 5) { Command c; c.op = Op::On; c.seconds = duration; return c; }
Command off() { Command c; c.op = Op::Off; return c; }
}
int main() {
  HostCrypto crypto; Keys keys;
  assert(signature(crypto, testKey, "84a9f21b3c0e5a1d|PUMP_ON|300") == "48ba618920e3dcb4");
  assert(signature(crypto, testKey, "84a9f21b3c0e5a1d|PUMP_OFF|0") == "238041d3721ef169");
  assert(signature(crypto, testKey, "84a9f21b3c0e5a1d|PUMP_ON|0300") != "48ba618920e3dcb4");
  assert(signature(crypto, "wrong-key", "84a9f21b3c0e5a1d|PUMP_ON|300") != "48ba618920e3dcb4");
  HostCrypto vectorCrypto; Authenticator vectorAuth(vectorCrypto); vectorAuth.loadKey(&keys);
  assert(nonce(vectorAuth, 0) == "84a9f21b3c0e5a1d");
  assert(send(vectorAuth, payload("PUMP_ON", "300", "48ba618920e3dcb4"), 0).code == Code::Ok);
  vectorCrypto.reads = 0; nonce(vectorAuth, 0);
  assert(send(vectorAuth, payload("PUMP_OFF", "0", "238041d3721ef169"), 0).code == Code::Ok);
  // These are authentication vectors only, never dispatched to a pump.
  const std::string dummy = payload("PUMP_ON", "5", "0000000000000000");
  Command command;
  assert(parse(dummy.data(), dummy.size(), command) == Code::Ok && command.seconds == 5);
  for (const char* duration : {"-1", "4294967296", "999999999999999999999999", "601", "0"}) {
    auto p = payload("PUMP_ON", duration, "0000000000000000");
    assert(parse(p.data(), p.size(), command) == Code::OutOfRange);
  }
  for (const char* duration : {"5.0", "5e0", "05", "true", "null", "\"5\"", "[]", "+5"}) {
    auto p = payload("PUMP_ON", duration, "0000000000000000");
    assert(parse(p.data(), p.size(), command) == Code::BadJson);
  }
  for (const std::string& p : {
      std::string("{}"), dummy + "x", dummy.substr(0, dummy.size()-1),
      std::string("{\"cmd\":\"PUMP_ON\",\"cmd\":\"PUMP_OFF\",\"dur\":0}"),
      std::string("{\"dur\":5,\"dur\":5,\"cmd\":\"PUMP_ON\"}"),
      std::string("{\"sig\":\"x\",\"sig\":\"x\",\"cmd\":\"PUMP_ON\"}"),
      std::string("{\"cmd\":\"PUMP_\\u004fN\",\"dur\":5,\"sig\":\"x\"}"),
      std::string("{\"cmd\":\"PUMP_ON\",\"dur\":5,\"extra\":0}"), std::string(129, 'x')})
    assert(parse(p.data(), p.size(), command) == Code::BadJson);
  auto p = payload("PUMP_OFF", "1", "0000000000000000");
  assert(parse(p.data(), p.size(), command) == Code::OutOfRange);
  p = payload("PUMP_ON", "5", "ABCDEF0123456789");
  assert(parse(p.data(), p.size(), command) == Code::BadSig);
  p = " \n{ \"sig\":\"0000000000000000\",\"dur\":5,\"cmd\":\"PUMP_ON\" }\t";
  assert(parse(p.data(), p.size(), command) == Code::Ok);
  p = dummy; p.insert(1, 1, '\0'); assert(parse(p.data(), p.size(), command) == Code::BadJson);
  p = dummy + std::string(kPayloadMax - dummy.size(), ' ');
  assert(parse(p.data(), p.size(), command) == Code::Ok);

  for (uint32_t start : {100u, UINT32_MAX - 1000}) {
    Authenticator auth(crypto); auth.loadKey(&keys);
    assert(send(auth, dummy, start).code == Code::NonceExpired);
    auto n = nonce(auth, start);
    p = payload("PUMP_ON", "5", signature(crypto, testKey, n + "|PUMP_ON|5"));
    assert(send(auth, p, start + 29999).code == Code::Ok);
    assert(send(auth, p, start + 29999).code == Code::NonceExpired); // replay
    n = nonce(auth, start);
    p = payload("PUMP_ON", "5", signature(crypto, testKey, n + "|PUMP_ON|5"));
    assert(send(auth, p, start + 30000).code == Code::NonceExpired);
    n = nonce(auth, start); const auto old = n;
    n = nonce(auth, start);
    p = payload("PUMP_ON", "5", signature(crypto, testKey, old + "|PUMP_ON|5"));
    assert(send(auth, p, start).code == Code::BadSig); // READ replaces old nonce
    n = nonce(auth, start);
    const char* part; uint16_t count; unsigned reads = crypto.reads;
    assert(auth.readNonce(8, start + 10, part, count) && count == 8 && n.substr(8) == std::string(part, count));
    assert(crypto.reads == reads);
    assert(auth.readNonce(16, start, part, count) && !count);
    assert(!auth.readNonce(17, start, part, count));
    assert(send(auth, "{\"cmd\":\"PUMP_ON\"}", start).code == Code::BadJson);
    assert(send(auth, dummy, start).code == Code::NonceExpired);
    nonce(auth, start); auth.consume(); assert(send(auth, dummy, start).code == Code::NonceExpired);
    nonce(auth, start); auth.reset(); assert(!auth.readNonce(8, start, part, count));
    assert(send(auth, dummy, start).code == Code::NonceExpired);
  }
  Authenticator missing(crypto); nonce(missing, 0);
  assert(send(missing, dummy, 0).missingKey); // Production default is fail closed.
  missing.loadKey(&keys); assert(send(missing, dummy, 0).code == Code::NonceExpired);
  Authenticator wrong(crypto); Keys wrongKeys; wrongKeys.value = "wrong-key"; wrong.loadKey(&wrongKeys);
  auto n = nonce(wrong, 0);
  p = payload("PUMP_OFF", "0", signature(crypto, testKey, n + "|PUMP_OFF|0"));
  assert(send(wrong, p, 0).code == Code::BadSig);
  Authenticator canonical(crypto); canonical.loadKey(&keys); n = nonce(canonical, 0);
  p = payload("PUMP_ON", "5", signature(crypto, testKey, n + "|PUMP_ON|05"));
  assert(send(canonical, p, 0).code == Code::BadSig);
  assert(send(canonical, p, 0).code == Code::NonceExpired);
  n = nonce(canonical, 0); crypto.fail = true;
  assert(send(canonical, dummy, 0).missingKey); crypto.fail = false;
  Authenticator rngFail(crypto); crypto.fail = true;
  const char* bytes; uint16_t size; assert(!rngFail.readNonce(0, 0, bytes, size)); crypto.fail = false;
  Keys encoded; encoded.value = "dGVzdC1ibGUtc2VjcmV0LWtleQ";
  Authenticator rawKey(crypto); rawKey.loadKey(&encoded); n = nonce(rawKey, 0);
  p = payload("PUMP_OFF", "0", signature(crypto, encoded.value, n + "|PUMP_OFF|0"));
  assert(send(rawKey, p, 0).code == Code::Ok); // Whole key string, not decoded.

  Dispatch d; const auto generation = d.connect(7); assert(generation && !d.connect(8));
  Dispatch::Slot slot;
  for (unsigned i = 1; i < Dispatch::kSlots; ++i) assert(d.reserve(generation, i, on(), Code::Ok, 0));
  assert(!d.reserve(generation, 99, on(), Code::Ok, 0));
  assert(!d.result(slot)); // No OK before owner.
  assert(d.reserve(generation, 100, off(), Code::Ok, 0));
  assert(d.claim(slot) && slot.command.op == Op::Off); // Reserved OFF lane beats ON saturation.
  d.complete(slot.ticket, Code::Ok);
  assert(d.result(slot) && slot.command.op == Op::Off && slot.code == Code::Ok); d.release(slot.ticket);
  assert(!d.claim(slot)); // Earlier pending ON requests were superseded.
  while (d.result(slot)) { assert(slot.code == Code::None); d.release(slot.ticket); }
  assert(d.reserve(generation, 101, on(), Code::Ok, UINT32_MAX - 500));
  d.expire(499); assert(!d.claim(slot)); assert(d.result(slot) && slot.code == Code::None); d.release(slot.ticket);
  assert(d.reserve(generation, 102, on(), Code::Ok, 0));
  d.disconnect(7); const auto next = d.connect(7); assert(next && next != generation);
  assert(!d.claim(slot) && !d.result(slot));
  assert(!d.reserve(generation, 103, on(), Code::Ok, 0));
  assert(d.reserve(next, 104, on(), Code::Ok, 0) && d.claim(slot)); const auto admitted = slot.ticket;
  d.disconnect(7); const auto newer = d.connect(7); assert(newer != next);
  d.complete(admitted, Code::Ok); assert(!d.result(slot)); // Old result never reaches reused conn_id.
  Dispatch invalidOff; const auto validGeneration = invalidOff.connect(1);
  assert(invalidOff.reserve(validGeneration, 1, on(), Code::Ok, 0));
  assert(invalidOff.reserve(validGeneration, 2, off(), Code::BadSig, 0));
  assert(invalidOff.claim(slot) && slot.command.op == Op::On); // Unauthenticated OFF cannot cancel ON.

  pump::testGuardEnable(true);
  test_mode::FreshSystem system; system.begin(); system.update(sample());
  assert(execute(system, on(6)) == Code::OutOfRange && outputLevel == LOW);
  assert(execute(system, on(121)) == Code::OutOfRange && outputLevel == LOW);
  assert(execute(system, on()) == Code::Ok && outputLevel == HIGH);
  const auto run = pump::run();
  assert(execute(system, on(1)) == Code::PumpBusy);
  assert(pump::run().generation == run.generation && pump::run().startedMs == run.startedMs && pump::run().durationMs == run.durationMs);
  assert(execute(system, off()) == Code::Ok && outputLevel == LOW);
  assert(execute(system, on()) == Code::None); // Locked: explicit contract gap, no invented code.
  system.begin(); system.update(sample()); assert(execute(system, on(1)) == Code::Ok);
  fakeNow += 1000; pump::testGuardInterrupt(); system.service();
  assert(system.status().decision.state == watering::State::Cooldown && execute(system, on()) == Code::None);
  system.begin(); assert(execute(system, on()) == Code::SensorError);
  system.update(sample(20, 12.26f)); assert(execute(system, on()) == Code::LowWater);
  system.begin(); system.update(sample()); fakeNow += 4001;
  assert(execute(system, on()) == Code::SensorError);
  system.begin(); system.update(sample(80)); system.command("AUTO");
  assert(execute(system, on()) == Code::None && system.status().mode == irrigation::Mode::Auto);
  pump::testGuardFailure(true, false); system.begin(); system.update(sample());
  assert(execute(system, on()) == Code::None && outputLevel == LOW);
  pump::testGuardFailure(false, false);

  // Full auth -> bridge -> real owner System path. Nothing happens before owner.
  system.begin(); system.update(sample());
  const auto bridgeGeneration = ble_command_bridge::connect(42);
  Authenticator auth(crypto); auth.loadKey(&keys);
  n = nonce(auth, fakeNow);
  p = payload("PUMP_ON", "5", signature(crypto, testKey, n + "|PUMP_ON|5"));
  const auto verified = send(auth, p, fakeNow); assert(verified.code == Code::Ok);
  assert(ble_command_bridge::reserve(bridgeGeneration, 1, verified.command, verified.code, fakeNow));
  assert(outputLevel == LOW && !ble_command_bridge::result(slot));
  ble_command_bridge::disconnect(42); ble_command_bridge::serviceOwner(system);
  assert(outputLevel == LOW); // Old unadmitted request dropped at disconnect ingress.
  const auto freshGeneration = ble_command_bridge::connect(42); assert(freshGeneration != bridgeGeneration);
  assert(ble_command_bridge::reserve(freshGeneration, 2, verified.command, Code::Ok, fakeNow));
  ble_command_bridge::serviceOwner(system);
  assert(outputLevel == HIGH && ble_command_bridge::result(slot) && slot.code == Code::Ok);
  ble_command_bridge::disconnect(42);
  assert(outputLevel == HIGH && !ble_command_bridge::result(slot)); // Admitted run is not stopped by BLE disconnect.
  system.stopManual();
  char wire[kResultCapacity];
  assert(resultJson(Op::On, Code::Ok, wire) && std::string(wire) == "{\"op\":\"PUMP_ON\",\"code\":\"OK\"}");
  assert(resultJson(Op::Off, Code::NonceExpired, wire) == kMaxResultBytes);
  assert(!resultJson(Op::Unknown, Code::BadJson, wire) && !resultJson(Op::On, Code::None, wire));
  std::cout << "PASS: BLE HMAC vectors/parser/nonce, bounded priority dispatch/generation, real owner safety and result ordering\n";
}
