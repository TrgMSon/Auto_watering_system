#include "ble_server.h"
#include "ble_telemetry.h"
#include "ble_crypto.h"
#include "ble_command_bridge.h"
#include "mqtt_commands.h"
#include "ble_pairing_esp32.h"
#include "ble_wifi_service.h"
#include "wifi_build_config.h"
#include <Arduino.h>
#include "esp32-hal-bt.h"
#include "esp32-hal-alloc-ble-mem.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_mac.h"
#include "freertos/queue.h"
#include <atomic>

#if !defined(CONFIG_BLUEDROID_ENABLED)
#error F3.3a requires the installed ESP32 Bluedroid configuration
#endif

namespace plant_ble {
namespace {
using ble_telemetry::Packet;
using ble_telemetry::Session;
enum Attribute { Service, SensorDecl, Sensor, SensorCccd, PumpDecl, Pump,
  ConfigDecl, Config, NonceDecl, Nonce, ResultsDecl, Results, ResultsCccd,
#if !WIFI_FIXED_ENABLED
  WifiDecl, Wifi,
#endif
  TimeDecl, Time, Count };
QueueHandle_t mailbox = nullptr;
QueueHandle_t events = nullptr;
// Callbacks copy bounded metadata and authenticated payload bytes, never a stack pointer.
enum class Kind { AdvData, ScanData, AdvStarted, Registered, Table, Started,
  Connected, Disconnected, Mtu, Congested, Read, Write, Execute, PairWindow, SecurityRequest, Authentication, RejectNumeric, RejectPasskey };
struct Event {
  Kind kind;
  esp_gatt_if_t interfaceId = ESP_GATT_IF_NONE;
  uint16_t connection = 0, handle = 0, offset = 0, length = 0, value = 0;
  uint32_t transaction = 0;
  int status = 0;
  bool response = false, prepared = false;
  uint16_t table[Count] = {};
  uint64_t generation = 0;
  uint64_t pumpIngressEpoch = 0;
  uint8_t peer[6] = {};
  uint8_t authMode = 0;
#if !WIFI_FIXED_ENABLED
  uint64_t encryptedWriteGeneration = 0;
  uint32_t wifiPayloadTicket = 0;
#endif
  char payload[ble_commands::kConfigPayloadMax] = {};
};
std::atomic<bool> eventOverflow{false};
std::atomic<uint16_t> sensorCccdHandle{0}, resultsCccdHandle{0}, pumpHandle{0}, configHandle{0}, timeHandle{0};
#if !WIFI_FIXED_ENABLED
std::atomic<uint16_t> wifiHandle{0};
#endif
ble_pairing::State pairing;
ble_pairing::EspSdk pairingSdk;
ble_pairing::Binding securityBinding;
#if !WIFI_FIXED_ENABLED
ble_wifi::WriteBuffer wifiInput;
#endif
portMUX_TYPE securityMux=portMUX_INITIALIZER_UNLOCKED;
struct SecurityLock {
  SecurityLock(){portENTER_CRITICAL(&securityMux);}
  ~SecurityLock(){portEXIT_CRITICAL(&securityMux);}
};
bool startSecurityProcedure(const uint8_t* peer,uint64_t generation){
  SecurityLock lock;return securityBinding.begin(peer,generation);
}
void cancelSecurityProcedure(uint64_t generation){
  SecurityLock lock;securityBinding.cancel(generation);
}
Session session;
uint64_t sessionGeneration = 0;
ble_commands::EspCrypto crypto;
ble_commands::Authenticator authentication(crypto);
const ble_commands::KeySource* keySource = nullptr;
esp_gatt_if_t interfaceId = ESP_GATT_IF_NONE;
uint16_t handles[Count] = {};
esp_gatts_attr_db_t attributes[Count] = {};
uint8_t uuids[WIFI_FIXED_ENABLED ? 7 : 8][16] = {};
uint16_t primaryUuid = ESP_GATT_UUID_PRI_SERVICE;
uint16_t declarationUuid = ESP_GATT_UUID_CHAR_DECLARE;
uint16_t cccdUuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
uint8_t properties[WIFI_FIXED_ENABLED ? 6 : 7] = {ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
  ESP_GATT_CHAR_PROP_BIT_WRITE, ESP_GATT_CHAR_PROP_BIT_WRITE, ESP_GATT_CHAR_PROP_BIT_READ,
#if WIFI_FIXED_ENABLED
  ESP_GATT_CHAR_PROP_BIT_NOTIFY, ESP_GATT_CHAR_PROP_BIT_WRITE};
#else
  ESP_GATT_CHAR_PROP_BIT_NOTIFY, ESP_GATT_CHAR_PROP_BIT_WRITE, ESP_GATT_CHAR_PROP_BIT_WRITE};
#endif
uint8_t empty[2] = {};
bool serviceReady = false, advDataReady = false, scanReady = false;
bool advertisePending = false, advertiseInFlight = false;
uint32_t failures = 0, mtuSkips = 0;
esp_ble_adv_params_t advParams = {};

// All session/GATT/advertising state below is exclusively BLE-worker-owned.
bool checked(esp_err_t result) {
  if (result == ESP_OK) return true;
  ++failures; return false;
}
Packet latest(uint32_t now) {
  ble_telemetry::Snapshot snapshot;
  xQueuePeek(mailbox, &snapshot, 0);
  return ble_telemetry::serve(snapshot, now);
}
void attribute(unsigned index, uint16_t uuidSize, uint8_t* uuid, uint16_t permission,
    uint16_t capacity, uint16_t size, uint8_t* value, bool automatic) {
  auto& a = attributes[index];
  a.attr_control.auto_rsp = automatic ? ESP_GATT_AUTO_RSP : ESP_GATT_RSP_BY_APP;
  a.att_desc.uuid_length = uuidSize; a.att_desc.uuid_p = uuid;
  a.att_desc.perm = permission; a.att_desc.max_length = capacity;
  a.att_desc.length = size; a.att_desc.value = value;
}
void prepareProfile() {
  // UUIDs in the little-endian byte order required by Bluedroid.
  const uint8_t base[16] = {0x00,0x00,0x57,0xf4,0x27,0xd2,0x8b,0xb4,
      0x1b,0x53,0x9e,0x67,0x87,0x5e,0x97,0x07};
  for (unsigned i = 0; i < sizeof(uuids)/sizeof(uuids[0]); ++i) { memcpy(uuids[i], base, 16); uuids[i][0] = i; }
  attribute(Service, ESP_UUID_LEN_16, reinterpret_cast<uint8_t*>(&primaryUuid),
      ESP_GATT_PERM_READ, 16, 16, uuids[0], true);
  const unsigned declarations[] = {SensorDecl, PumpDecl, ConfigDecl, NonceDecl,
      ResultsDecl,
#if !WIFI_FIXED_ENABLED
      WifiDecl,
#endif
      TimeDecl};
  const unsigned values[] = {Sensor, Pump, Config, Nonce, Results,
#if !WIFI_FIXED_ENABLED
      Wifi,
#endif
      Time};
  const unsigned operationSuffix[] = {1,2,3,4,5,
#if !WIFI_FIXED_ENABLED
      6,
#endif
      7};
  const unsigned uuidSlot[] = {1,2,3,4,5,
#if !WIFI_FIXED_ENABLED
      6,
#endif
      WIFI_FIXED_ENABLED ? 6u : 7u};
  const unsigned propertySlot[] = {0,1,2,3,4,
#if !WIFI_FIXED_ENABLED
      5,
#endif
      WIFI_FIXED_ENABLED ? 5u : 6u};
  constexpr unsigned characteristicCount=WIFI_FIXED_ENABLED?6u:7u;
  for (unsigned i = 0; i < characteristicCount; ++i) {
    attribute(declarations[i], ESP_UUID_LEN_16, reinterpret_cast<uint8_t*>(&declarationUuid),
        ESP_GATT_PERM_READ, 1, 1, &properties[propertySlot[i]], true);
    // WIFI only uses encrypted WRITE (no ordinary WRITE bit); CONFIG uses ordinary authenticated WRITE.
    const uint16_t permission = (i == 0 || i == 3) ? ESP_GATT_PERM_READ :
        ble_commands::encryptedWifiWrite(operationSuffix[i]) ? ESP_GATT_PERM_WRITE_ENCRYPTED : ble_commands::authenticatedWrite(operationSuffix[i]) ? ESP_GATT_PERM_WRITE : 0;
    attribute(values[i], ESP_UUID_LEN_128, uuids[uuidSlot[i]], permission,
        // Let the worker see over-limit writes (up to stack's ATT limit), so
        // rejecting them still consumes the nonce. Copy capacity is bounded by endpoint (128 pump/TIME, 192 CONFIG).
        (ble_commands::encryptedWifiWrite(operationSuffix[i]) || ble_commands::authenticatedWrite(operationSuffix[i])) ? ESP_GATT_MAX_ATTR_LEN : i == 3 ? 16 : ble_telemetry::kCapacity, 0, empty, false);
  }
  for (unsigned index : {unsigned(SensorCccd), unsigned(ResultsCccd)})
    attribute(index, ESP_UUID_LEN_16, reinterpret_cast<uint8_t*>(&cccdUuid),
        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2, 2, empty, false);
}
void response(uint16_t conn, uint32_t transaction, esp_gatt_status_t status, esp_gatt_rsp_t* rsp = nullptr) {
  checked(esp_ble_gatts_send_response(interfaceId, conn, transaction, status, rsp));
}
bool manualHandle(uint16_t handle) {
  const unsigned indices[] = {Sensor, SensorCccd, Pump, Config, Nonce, Results, ResultsCccd,
#if !WIFI_FIXED_ENABLED
      Wifi,
#endif
      Time};
  for (unsigned i : indices) if (handles[i] && handles[i] == handle) return true;
  return false;  // Service and declarations are answered by the stack.
}
void enqueue(const Event& event) {
  if (eventOverflow.load() || xQueueSend(events, &event, 0) != pdTRUE) {
    eventOverflow.store(true);
    ble_command_bridge::disable(); // Invalidate unadmitted commands immediately.
    // A dropped CONNECT must not leave an untracked peer attached.
    if (event.kind == Kind::Connected) esp_ble_gatts_close(event.interfaceId, event.connection);
  }
}
void gapEvent(esp_gap_ble_cb_event_t kind, esp_ble_gap_cb_param_t* p) {
  Event e;
  switch (kind) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: e.kind = Kind::AdvData; e.status = p->adv_data_raw_cmpl.status; break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT: e.kind = Kind::ScanData; e.status = p->scan_rsp_data_raw_cmpl.status; break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: e.kind = Kind::AdvStarted; e.status = p->adv_start_cmpl.status; break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
      e.kind=Kind::SecurityRequest;memcpy(e.peer,p->ble_security.ble_req.bd_addr,6);
      {SecurityLock lock;e.generation=securityBinding.capture(e.peer);}break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
      e.kind=Kind::Authentication;memcpy(e.peer,p->ble_security.auth_cmpl.bd_addr,6);
      e.status=p->ble_security.auth_cmpl.success?1:0;
      e.authMode=p->ble_security.auth_cmpl.auth_mode;
      {SecurityLock lock;
        e.generation=securityBinding.complete(e.peer);
        if(!e.status)securityBinding.bonded(e.generation,false);
      }
      if(!e.status&&ble_command_bridge::current(e.generation)){
#if !WIFI_FIXED_ENABLED
        {SecurityLock lock;wifiInput.clear();}
#endif
        ble_wifi_service::connection(0);
      }
      break;
    case ESP_GAP_BLE_NC_REQ_EVT:
      e.kind=Kind::RejectNumeric;memcpy(e.peer,p->ble_security.key_notif.bd_addr,6);break;
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:
      e.kind=Kind::RejectPasskey;memcpy(e.peer,p->ble_security.ble_req.bd_addr,6);break;
    default: return;
  }
  enqueue(e);
}
void gattEvent(esp_gatts_cb_event_t kind, esp_gatt_if_t id, esp_ble_gatts_cb_param_t* p) {
  Event e; e.interfaceId = id;
  switch (kind) {
    case ESP_GATTS_REG_EVT: e.kind = Kind::Registered; e.status = p->reg.status; break;
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
      e.kind = Kind::Table; e.status = p->add_attr_tab.status; e.length = p->add_attr_tab.num_handle;
      if (e.status == ESP_GATT_OK && e.length == Count) memcpy(e.table, p->add_attr_tab.handles, sizeof(e.table));
      break;
    case ESP_GATTS_START_EVT: e.kind = Kind::Started; e.status = p->start.status; break;
    case ESP_GATTS_CONNECT_EVT:
      e.kind = Kind::Connected; e.connection = p->connect.conn_id;
      e.generation = ble_command_bridge::connect(e.connection);
      if (!e.generation) { esp_ble_gatts_close(id, e.connection); return; }
      memcpy(e.peer,p->connect.remote_bda,6);
      {SecurityLock lock;
#if !WIFI_FIXED_ENABLED
        wifiInput.clear();
#endif
        securityBinding.connect(e.peer,e.generation);}
      ble_wifi_service::connection(0);
      break;
    case ESP_GATTS_DISCONNECT_EVT:
      e.kind = Kind::Disconnected; e.connection = p->disconnect.conn_id;
      e.generation = ble_command_bridge::generation(e.connection);
      ble_command_bridge::disconnect(e.connection); // Before owner can claim old work.
      {SecurityLock lock;
#if !WIFI_FIXED_ENABLED
        if(e.generation)wifiInput.clear();
#endif
        securityBinding.disconnect(e.generation);}
      if(e.generation)ble_wifi_service::connection(0);
      break;
    case ESP_GATTS_MTU_EVT: e.kind = Kind::Mtu; e.connection = p->mtu.conn_id; e.value = p->mtu.mtu; break;
    case ESP_GATTS_CONGEST_EVT:
      e.kind = Kind::Congested; e.connection = p->congest.conn_id; e.value = p->congest.congested; break;
    case ESP_GATTS_READ_EVT:
      e.kind = Kind::Read; e.connection = p->read.conn_id; e.transaction = p->read.trans_id;
      e.handle = p->read.handle; e.offset = p->read.offset; e.response = p->read.need_rsp; break;
    case ESP_GATTS_WRITE_EVT:
      e.pumpIngressEpoch = plant_mqtt::commandIngressEpoch();
      e.kind = Kind::Write; e.connection = p->write.conn_id; e.transaction = p->write.trans_id;
      e.handle = p->write.handle; e.offset = p->write.offset; e.length = p->write.len;
      e.response = p->write.need_rsp; e.prepared = p->write.is_prep;
      // Copy CCCD or bounded authenticated command bytes only; other endpoint data is discarded.
      if (e.length == 2 && (e.handle == sensorCccdHandle.load() || e.handle == resultsCccdHandle.load()))
        e.value = p->write.value[0] | (uint16_t(p->write.value[1]) << 8);
      if ((e.handle == pumpHandle.load() || e.handle == timeHandle.load() || e.handle == configHandle.load()) && !e.prepared &&
          e.length <= (e.handle == configHandle.load() ? ble_commands::kConfigPayloadMax : ble_commands::kPayloadMax))
        memcpy(e.payload, p->write.value, e.length);
#if !WIFI_FIXED_ENABLED
      if(e.handle==wifiHandle.load()){
        // Exact-IDF source guarantees encrypted permission BEFORE this callback.
        e.generation=ble_command_bridge::generation(e.connection);
        memcpy(e.peer,p->write.bda,6);
        if(!e.prepared&&e.response&&!e.offset){
          SecurityLock lock;
          e.encryptedWriteGeneration=securityBinding.captureWrite(e.peer,e.generation);
          if(e.encryptedWriteGeneration&&ble_wifi_service::available())
            e.wifiPayloadTicket=wifiInput.store(e.generation,reinterpret_cast<const char*>(p->write.value),e.length);
        }
      }
#endif
      break;
    case ESP_GATTS_EXEC_WRITE_EVT:
      e.kind = Kind::Execute; e.connection = p->exec_write.conn_id; e.transaction = p->exec_write.trans_id; break;
    default: return;
  }
  if (e.kind == Kind::Read || e.kind == Kind::Write || e.kind == Kind::Execute ||
      e.kind == Kind::Mtu || e.kind == Kind::Congested)
    e.generation = ble_command_bridge::generation(e.connection);
  enqueue(e);
}
void process(const Event& e) {
  if (e.kind != Kind::Registered && e.interfaceId != ESP_GATT_IF_NONE && e.interfaceId != interfaceId) return;
  if ((e.kind == Kind::Read || e.kind == Kind::Write || e.kind == Kind::Execute ||
      e.kind == Kind::Mtu || e.kind == Kind::Congested) &&
      (e.generation != sessionGeneration || !ble_command_bridge::current(e.generation))) return;
  switch (e.kind) {
    case Kind::PairWindow:
      if(pairing.ready()){
        pairing.window.set(e.value!=0,e.transaction);
        Serial.println(e.value?(pairing.window.open(millis())?"BLE pairing open request applied":"BLE pairing open request expired"):
            "BLE pairing close request applied");
      }
      else Serial.println("BLE pairing unavailable; window closed");
      break;
    case Kind::SecurityRequest: {
      bool accept=ble_command_bridge::current(e.generation)&&
          pairing.admit(e.peer,e.generation,millis(),pairingSdk)&&startSecurityProcedure(e.peer,e.generation);
      if(!accept){
        pairing.authenticated(e.peer,e.generation,false,false,pairingSdk);
        {SecurityLock lock;securityBinding.bonded(e.generation,false);}
        if(ble_command_bridge::current(e.generation))ble_wifi_service::connection(0);
      }
      if(!pairingSdk.respond(e.peer,accept)){
        if(accept)cancelSecurityProcedure(e.generation);
        pairing.authenticated(e.peer,e.generation,false,false,pairingSdk);
        {SecurityLock lock;securityBinding.bonded(e.generation,false);}
        if(ble_command_bridge::current(e.generation))ble_wifi_service::connection(0);
        Serial.println("BLE security response failed");
      }else Serial.println(accept?"BLE pairing admitted":"BLE pairing rejected");
      break;
    }
    case Kind::Authentication: {
      if(!ble_command_bridge::current(e.generation)||!pairing.matches(e.peer,e.generation))break;
      bool scBond=(e.authMode&ESP_LE_AUTH_REQ_SC_BOND)==ESP_LE_AUTH_REQ_SC_BOND;
      bool ok=pairing.authenticated(e.peer,e.generation,e.status==1,scBond,pairingSdk);
      {SecurityLock lock;securityBinding.bonded(e.generation,ok);}
      ble_wifi_service::connection(ok?e.generation:0);
      Serial.println(ok?"BLE auth/bond confirmed; connection encryption=UNCONFIRMED; WIFI requires per-WRITE stack evidence":
          "BLE security failed; gate=CLOSED");
      break;
    }
    case Kind::RejectNumeric:
      checked(esp_ble_confirm_reply(const_cast<uint8_t*>(e.peer),false));break;
    case Kind::RejectPasskey:
      checked(esp_ble_passkey_reply(const_cast<uint8_t*>(e.peer),false,0));break;
    case Kind::AdvData: advDataReady = e.status == ESP_BT_STATUS_SUCCESS; if (!advDataReady) ++failures; break;
    case Kind::ScanData: scanReady = e.status == ESP_BT_STATUS_SUCCESS; if (!scanReady) ++failures; break;
    case Kind::AdvStarted:
      advertiseInFlight = false;
      if (e.status != ESP_BT_STATUS_SUCCESS) { ++failures; advertisePending = true; }
      break;
    case Kind::Registered:
      if (e.status != ESP_GATT_OK) { ++failures; break; }
      interfaceId = e.interfaceId;
      checked(esp_ble_gatts_create_attr_tab(attributes, interfaceId, Count, 0)); break;
    case Kind::Table:
      if (e.status != ESP_GATT_OK || e.length != Count) { ++failures; break; }
      memcpy(handles, e.table, sizeof(handles));
      sensorCccdHandle.store(handles[SensorCccd]); resultsCccdHandle.store(handles[ResultsCccd]);
      pumpHandle.store(handles[Pump]); configHandle.store(handles[Config]); timeHandle.store(handles[Time]);
#if !WIFI_FIXED_ENABLED
      wifiHandle.store(handles[Wifi]);
#endif
      checked(esp_ble_gatts_start_service(handles[Service])); break;
    case Kind::Started: serviceReady = e.status == ESP_GATT_OK; if (!serviceReady) ++failures; break;
    case Kind::Connected:
      if (!ble_command_bridge::current(e.generation)) break;
      authentication.reset();
      if (session.connected) session.disconnect(session.connection);
      sessionGeneration = e.generation;
      session.connect(e.connection, millis());
      pairing.connect(e.peer,e.generation);
      if(pairing.ready()){
        bool bonded=false;
        if(pairingSdk.bonded(e.peer,bonded)&&bonded &&
            pairing.admit(e.peer,e.generation,millis(),pairingSdk)&&startSecurityProcedure(e.peer,e.generation)){
          if(!pairingSdk.encrypt(e.peer)){
            cancelSecurityProcedure(e.generation);
            pairing.authenticated(e.peer,e.generation,false,false,pairingSdk);
            Serial.println("BLE re-encryption request failed; gate=CLOSED");
          }else Serial.println("BLE bonded reconnect: encryption requested");
        }
      }
      break;
    case Kind::Disconnected:
      if (e.generation == sessionGeneration && session.matches(e.connection)) {
        pairing.disconnect(e.generation);
        authentication.reset(); session.disconnect(e.connection); sessionGeneration = 0; advertisePending = true;
      }
      break;
    case Kind::Mtu: session.setMtu(e.connection, e.value); break;
    case Kind::Congested: if (session.matches(e.connection)) session.congested = e.value != 0; break;
    case Kind::Read: {
      if (!e.response || !manualHandle(e.handle)) break;
      esp_gatt_rsp_t rsp = {};
      rsp.attr_value.handle = e.handle;
      rsp.attr_value.offset = e.offset;
      esp_gatt_status_t status = ESP_GATT_READ_NOT_PERMIT;
      if (session.matches(e.connection) && e.handle == handles[Sensor]) {
        const Packet packet = latest(millis());
        const char* data = nullptr; uint16_t size = 0;
        const auto result = session.readResult(packet, e.offset, data, size);
        if (result == Session::ReadResult::Ok) {
          memcpy(rsp.attr_value.value, data, size); rsp.attr_value.len = size; status = ESP_GATT_OK;
        } else status = result == Session::ReadResult::Unavailable ? ESP_GATT_ERR_UNLIKELY : ESP_GATT_INVALID_OFFSET;
      } else if (session.matches(e.connection) && e.handle == handles[Nonce]) {
        const char* bytes = nullptr; uint16_t size = 0;
        if (authentication.readNonce(e.offset, millis(), bytes, size)) {
          rsp.attr_value.len = size; memcpy(rsp.attr_value.value, bytes, size); status = ESP_GATT_OK;
        } else status = e.offset ? ESP_GATT_INVALID_OFFSET : ESP_GATT_ERR_UNLIKELY;
      } else if (session.matches(e.connection) &&
          (e.handle == handles[SensorCccd] || e.handle == handles[ResultsCccd])) {
        if (e.offset <= 2) {
          const uint8_t value[2] = {static_cast<uint8_t>(e.handle == handles[SensorCccd] ?
              session.subscribed : session.resultsSubscribed), 0};
          rsp.attr_value.len = 2 - e.offset;
          memcpy(rsp.attr_value.value, value + e.offset, rsp.attr_value.len);
          status = ESP_GATT_OK;
        } else status = ESP_GATT_INVALID_OFFSET;
      }
      response(e.connection, e.transaction, status, status == ESP_GATT_OK ? &rsp : nullptr);
      break;
    }
    case Kind::Write: {
      if (!manualHandle(e.handle)) break;
      esp_gatt_status_t status = ESP_GATT_WRITE_NOT_PERMIT;
      const bool cccd = e.handle == handles[SensorCccd] || e.handle == handles[ResultsCccd];
#if !WIFI_FIXED_ENABLED
      if(e.handle==handles[Wifi]&&session.matches(e.connection)){
        Serial.printf("[WIFI] stage=ble-write-received bytes=%u prepared=%u response=%u offset=%u\n",
            unsigned(e.length),unsigned(e.prepared),unsigned(e.response),unsigned(e.offset));
        struct Payload {char bytes[ble_wifi::kWriteCapacity]={};~Payload(){ble_commands::wipeWifi(bytes,sizeof(bytes));}} payload;
        size_t payloadSize=0;bool copied;
        {SecurityLock lock;copied=wifiInput.take(e.wifiPayloadTicket,e.generation,payload.bytes,sizeof(payload.bytes),payloadSize);}
        if(e.prepared||!e.response||e.offset||!e.length||e.length>ble_wifi::kWriteCapacity||e.length>session.mtu-3){
          Serial.println("[WIFI] stage=ble-write-rejected reason=transport");
          authentication.consume();
          if(e.response)response(e.connection,e.transaction,e.prepared?ESP_GATT_REQ_NOT_SUPPORTED:
              e.offset?ESP_GATT_INVALID_OFFSET:ESP_GATT_INVALID_ATTR_LEN);
          break;
        }
        bool bond=false;
        if(e.encryptedWriteGeneration!=e.generation||!pairing.matches(e.peer,e.generation)||
            !pairing.security.bondedPeer(e.generation)||!pairingSdk.bonded(e.peer,bond)||!bond){
          Serial.println("[WIFI] stage=ble-write-rejected reason=security");
          authentication.consume();response(e.connection,e.transaction,ESP_GATT_INSUF_AUTHENTICATION);break;
        }
        if(!copied||payloadSize!=e.length||!ble_wifi_service::available()){
          Serial.println("[WIFI] stage=ble-write-rejected reason=handoff-unavailable");
          authentication.consume();response(e.connection,e.transaction,ESP_GATT_INSUF_RESOURCE);break;
        }
        const auto auth=authentication.authenticateWifi(payload.bytes,e.length,millis(),pairing.security,e.generation,session.mtu,
            false,true,0,e.encryptedWriteGeneration);
        if(auth.securityDenied||auth.missingKey){
          Serial.printf("[WIFI] stage=ble-auth-rejected security=%u missing-key=%u\n",
              unsigned(auth.securityDenied),unsigned(auth.missingKey));
          response(e.connection,e.transaction,ESP_GATT_INSUF_AUTHENTICATION);break;
        }
        if(auth.transportDenied){
          Serial.println("[WIFI] stage=ble-auth-rejected reason=transport");
          response(e.connection,e.transaction,ESP_GATT_INVALID_ATTR_LEN);break;
        }
        Serial.printf("[WIFI] stage=ble-auth-result code=%s\n",ble_commands::name(auth.code));
        if(!session.resultsSubscribed||session.congested||size_t(session.mtu-3)<ble_wifi::kBusinessMax){
          Serial.printf("[WIFI] stage=ble-handoff-rejected notify=%u congested=%u mtu=%u\n",
              unsigned(session.resultsSubscribed),unsigned(session.congested),unsigned(session.mtu));
          response(e.connection,e.transaction,ESP_GATT_INSUF_RESOURCE);break;
        }
        if(!ble_wifi_service::reserve(auth,millis())){
          Serial.println("[WIFI] stage=ble-handoff-rejected reason=busy");
          response(e.connection,e.transaction,ESP_GATT_INSUF_RESOURCE);break;
        }
        Serial.printf("[WIFI] stage=ble-handoff-queued bytes=%u\n",unsigned(e.length));
        // ACK the bounded handoff now, never wait up to 30 seconds for connection.
        response(e.connection,e.transaction,ESP_GATT_OK);break;
      }
#endif
      if ((e.handle == handles[Pump] || e.handle == handles[Time] || e.handle == handles[Config]) && session.matches(e.connection)) {
        // Consume even when this WRITE cannot enter the authenticated path.
        if (e.handle == handles[Config] ?
            !ble_commands::configWriteFits(e.length, session.mtu, e.prepared, e.response, e.offset) :
            (e.prepared || !e.response || e.offset || !e.length ||
             e.length > ble_commands::kPayloadMax || e.length > session.mtu - 3)) {
          authentication.consume();
          if (e.response) response(e.connection, e.transaction, e.prepared ? ESP_GATT_REQ_NOT_SUPPORTED : ESP_GATT_INVALID_ATTR_LEN);
          break;
        }
        ble_commands::Authentication auth;
        if (e.handle == handles[Config]) {
          const auto config = authentication.authenticateConfig(e.payload, e.length, millis());
          auth.command.op = ble_commands::Op::Config; auth.command.config = config.request;
          auth.code = config.code; auth.missingKey = config.missingKey;
        } else auth = authentication.authenticate(e.payload, e.length, millis(),
            e.handle == handles[Time] ? ble_commands::Op::Time : ble_commands::Op::Unknown);
        if (auth.missingKey) { response(e.connection, e.transaction, ESP_GATT_INSUF_AUTHENTICATION); break; }
        if (auth.command.op == ble_commands::Op::Unknown) {
          response(e.connection, e.transaction, ESP_GATT_INVALID_PDU); break;
        }
        if (!session.resultsSubscribed || session.congested || size_t(session.mtu - 3) < ble_commands::kMaxResultBytes ||
            !ble_command_bridge::reserve(e.generation, e.transaction, auth.command, auth.code, millis(), e.pumpIngressEpoch)) {
          response(e.connection, e.transaction, ESP_GATT_INSUF_RESOURCE); break;
        }
        // GATT response is deferred until the reserved result is available.
        // Only the owner can turn an authenticated pending request into OK.
        break;
      }
      if (e.prepared) status = ESP_GATT_REQ_NOT_SUPPORTED;
      else if (session.matches(e.connection) && cccd) {
        if (e.offset) status = ESP_GATT_INVALID_OFFSET;
        else if (e.length != 2) status = ESP_GATT_INVALID_ATTR_LEN;
        else {
          status = session.subscribe(e.connection, e.value, e.handle == handles[ResultsCccd]) ?
              ESP_GATT_OK : ESP_GATT_CCC_CFG_ERR;
        }
      }
      if (e.response) response(e.connection, e.transaction, status);
      break;
    }
    case Kind::Execute:
      authentication.consume();
      response(e.connection, e.transaction, ESP_GATT_REQ_NOT_SUPPORTED);
      break;
    default: break;
  }
}
bool initialize() {
  uint8_t mac[6]; char deviceId[18], name[16];
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK || !ble_telemetry::identity(mac, deviceId, name)) return false;
  Serial.printf("BLE device_id=%s name=%s (F3.3c.1 authenticated pump/TIME)\n", deviceId, name);
  authentication.loadKey(keySource);
  if (!authentication.hasKey()) Serial.println("BLE authenticated commands disabled: no provisioned key source");
  prepareProfile();
  // Arduino core's BLE memory-retention header above prevents initArduino from
  // releasing BLE memory. No Wi-Fi or Classic Bluetooth is started.
  if (!btStartMode(BT_MODE_BLE) || esp_bluedroid_init() != ESP_OK || esp_bluedroid_enable() != ESP_OK) return false;
  if (esp_ble_gap_register_callback(gapEvent) != ESP_OK ||
      esp_ble_gatts_register_callback(gattEvent) != ESP_OK ||
      esp_ble_gatt_set_local_mtu(185) != ESP_OK || esp_ble_gap_set_device_name(name) != ESP_OK) return false;
  Serial.println(pairing.setup(pairingSdk)?
#if WIFI_FIXED_ENABLED
      "BLE SC bonding ready (Just Works, no MITM); pairing window CLOSED; BLE Wi-Fi setup omitted":
#else
      "BLE SC bonding ready (Just Works, no MITM); pairing window CLOSED; WIFI requires encrypted WRITE":
#endif
      "BLE security setup failed; pairing disabled, gate=CLOSED");
  uint8_t advertisement[21] = {2, ESP_BLE_AD_TYPE_FLAG, ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
      17, ESP_BLE_AD_TYPE_128SRV_CMPL};
  memcpy(advertisement + 5, uuids[0], 16);
  uint8_t scanResponse[17] = {16, ESP_BLE_AD_TYPE_NAME_CMPL};
  memcpy(scanResponse + 2, name, 15);
  advParams.adv_int_min = 0x100; advParams.adv_int_max = 0x180;
  advParams.adv_type = ADV_TYPE_IND; advParams.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  advParams.channel_map = ADV_CHNL_ALL; advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
  if (esp_ble_gap_config_adv_data_raw(advertisement, sizeof(advertisement)) != ESP_OK ||
      esp_ble_gap_config_scan_rsp_data_raw(scanResponse, sizeof(scanResponse)) != ESP_OK ||
      esp_ble_gatts_app_register(0) != ESP_OK) return false;
  if(!ble_wifi_service::begin())Serial.println("Wi-Fi runtime worker unavailable");
  advertisePending = true;
  return true;
}
void worker(void*) {
  if (!initialize()) {
    eventOverflow.store(true);
    ble_command_bridge::disable();
    Serial.println("BLE initialization failed; telemetry unavailable, irrigation unchanged");
    vTaskDelete(nullptr); return;
  }
  uint32_t lastAdvertisingMs = millis() - 2000, lastDiagnosticMs = millis();
  uint32_t reportedFailures = 0, reportedSkips = 0;
  bool transportFailed = false;
  bool lastPairWindow=false;
  for (;;) {
    if (eventOverflow.load() && !transportFailed) {
      // Fail closed on lost lifecycle/CCCD events. A reboot is required; never
      // risk inheriting an old subscription after queue overflow.
      transportFailed = true; ++failures;
#if !WIFI_FIXED_ENABLED
      {SecurityLock lock;wifiInput.clear();}
#endif
      pairing.window.set(false,0);pairing.disconnect(sessionGeneration);ble_wifi_service::connection(0);
      authentication.reset();
      esp_ble_gap_stop_advertising();
      if (session.connected) {
        esp_ble_gatts_close(interfaceId, session.connection);
        session.disconnect(session.connection);
      }
      xQueueReset(events);
    }
    Event event;
    for (unsigned i = 0; !transportFailed && i < 16 && xQueueReceive(events, &event, 0) == pdTRUE; ++i)
      process(event);
    const uint32_t now = millis();
    bool pairWindow=pairing.ready()&&pairing.window.open(now);
    if(pairWindow!=lastPairWindow){
      Serial.println(pairWindow?"BLE pairing window OPEN (60s from USB request)":"BLE pairing window CLOSED");
      lastPairWindow=pairWindow;
    }
    ble_command_bridge::expire(now);
    const Packet packet = latest(now);
    uint32_t errors, skips;
    if (!transportFailed && !eventOverflow.load() && uxQueueMessagesWaiting(events) == 0) {
      ble_commands::Dispatch::Slot result;
      if (ble_command_bridge::current(sessionGeneration) && ble_command_bridge::result(result) &&
          result.generation == sessionGeneration && result.connection == session.connection) {
        char wire[ble_commands::kResultCapacity];
        const size_t size = ble_commands::resultJson(result.command.op, result.code, wire);
        if (!size) {
          response(result.connection, result.transaction, ESP_GATT_ERR_UNLIKELY);
          ble_command_bridge::release(result.ticket);
        } else if (session.resultsSubscribed && !session.congested && size <= size_t(session.mtu - 3)) {
          // Both admission and delivery are fenced by connection generation.
          response(result.connection, result.transaction, ESP_GATT_OK);
          if (ble_command_bridge::current(result.generation))
            checked(esp_ble_gatts_send_indicate(interfaceId, result.connection, handles[Results], size,
                reinterpret_cast<uint8_t*>(wire), false));
          ble_command_bridge::release(result.ticket);
        } else if (uint32_t(now - result.queuedMs) >= 3000) {
          response(result.connection, result.transaction, ESP_GATT_ERR_UNLIKELY);
          ble_command_bridge::release(result.ticket);
        }
      }
#if !WIFI_FIXED_ENABLED
      uint64_t wifiGeneration=0;ble_commands::Code wifiCode;
      if(ble_wifi_service::result(now,wifiGeneration,wifiCode)){
        if(wifiGeneration!=sessionGeneration||!ble_command_bridge::current(wifiGeneration))ble_wifi_service::release(wifiGeneration);
        else if(session.resultsSubscribed&&!session.congested){
          char wifiWire[ble_commands::kResultCapacity];size_t wifiSize=ble_wifi::wifiResult(wifiCode,wifiWire,sizeof(wifiWire));
          if(wifiSize&&wifiSize<=size_t(session.mtu-3)){
            if(ble_command_bridge::current(wifiGeneration))checked(esp_ble_gatts_send_indicate(interfaceId,session.connection,handles[Results],wifiSize,
                reinterpret_cast<uint8_t*>(wifiWire),false));
            ble_wifi_service::release(wifiGeneration);
          }
        }
      }
#endif
      if (!session.connected && serviceReady && advDataReady && scanReady &&
          advertisePending && !advertiseInFlight && uint32_t(now - lastAdvertisingMs) >= 2000) {
        lastAdvertisingMs = now;
        if (checked(esp_ble_gap_start_advertising(&advParams))) {
          advertisePending = false; advertiseInFlight = true;
        }
      }
      const auto action = session.notification(now,
          ble_command_bridge::current(sessionGeneration) ? packet.size : 0);
      if (action == Session::Notify::TooLarge) ++mtuSkips;
      else if (action == Session::Notify::Send)
        checked(esp_ble_gatts_send_indicate(interfaceId, session.connection, handles[Sensor],
            packet.size, const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(packet.bytes)), false));
    }
    errors = failures; skips = mtuSkips;
    if (uint32_t(now - lastDiagnosticMs) >= 10000) {
      lastDiagnosticMs = now;
      if (errors != reportedFailures || skips != reportedSkips) {
        Serial.printf("BLE diagnostics errors=%lu mtu_notify_skips=%lu event_overflow=%u\n",
            static_cast<unsigned long>(errors), static_cast<unsigned long>(skips), transportFailed ? 1u : 0u);
        reportedFailures = errors; reportedSkips = skips;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));  // BLE worker only, never safety owner.
  }
}
}
bool begin(const ble_commands::KeySource* keys) {
  if (mailbox) return false;  // Boot-only, no repeated radio allocation.
  keySource = keys; // Provider lifetime must cover boot initialization. Default missing.
  events = xQueueCreate(32, sizeof(Event));
  mailbox = xQueueCreate(1, sizeof(ble_telemetry::Snapshot));
  if (!events || !mailbox) {
    if (events) vQueueDelete(events);
    if (mailbox) vQueueDelete(mailbox);
    events = nullptr; mailbox = nullptr;
    return false;
  }
  const ble_telemetry::Snapshot initial;
  xQueueOverwrite(mailbox, &initial);
  if (xTaskCreate(worker, "plant-ble", 4096, nullptr, 1, nullptr) == pdPASS) return true;
  vQueueDelete(mailbox); vQueueDelete(events);
  mailbox = nullptr; events = nullptr;
  return false;
}
bool requestPairing(bool open,bool manual,bool pumpOn){
  auto command=open?ble_pairing::Command::Open:ble_pairing::Command::Close;
  if(!ble_pairing::ownerAllows(command,manual,pumpOn)||!events||eventOverflow.load())return false;
  Event event;event.kind=Kind::PairWindow;event.value=open?1:0;event.transaction=millis();
  return xQueueSend(events,&event,0)==pdTRUE;
}
void publish(const telemetry_snapshot::Snapshot& snapshot) {
  if (!mailbox) return;
  xQueueOverwrite(mailbox, &snapshot);
}
}
