#include "wifi_sdk/sdk_fake.h"
#include "../esp32/wifi_esp32_adapters.h"
#include <cassert>
#include <algorithm>
#include <iostream>
using namespace ble_wifi;using ble_commands::WifiRequest;
extern const char* WIFI_PROVISION_BARRIER;
WifiRequest candidate(){WifiRequest r;memset(r.ssid,'s',32);strcpy(r.password,"fixture-passphrase");return r;}
void acknowledge(uint32_t token){shim::emit(WIFI_PROVISION_BARRIER,0,&token);}
size_t index(const char* name){auto p=std::find(shim::calls.begin(),shim::calls.end(),name);assert(p!=shim::calls.end());return size_t(p-shim::calls.begin());}
static uint32_t completedClock=0;
static uint32_t monotonicNow(){return completedClock;}
int main(){
 auto r=candidate();
 // Real ESP adapter + fake IDF API: no host network/NVS access.
 shim::reset();{
  EspRadioSdk sdk;EspRadio radio(sdk);
  assert(radio.startRam(r));uint32_t first=shim::posted;
  shim::associated=true;memcpy(shim::ap.ssid,r.ssid,32);shim::ip.ip.addr=0x0201a8c0;shim::ip.netmask.addr=0x00ffffff;
  shim::emit(IP_EVENT);assert(radio.poll()==Radio::State::Pending); // old event and link cannot cross barrier
  acknowledge(first);assert(radio.poll()==Radio::State::Pending); // old IP explicitly cleared
  assert(index("stop")<index("barrier")&&index("RAM")<index("config")&&index("config")<index("connect"));
  assert(!memcmp(shim::config.sta.ssid,r.ssid,32)&&!memcmp(shim::config.sta.password,r.password,strlen(r.password)));
  assert(shim::config.sta.failure_retry_cnt==0);
  shim::ip.ip.addr=0x0201a8c0;shim::ip.netmask.addr=0x00ffffff;
  shim::ap.ssid[0]='x';assert(radio.poll()==Radio::State::Pending);
  memcpy(shim::ap.ssid,r.ssid,32);shim::ip.ip.addr=0;assert(radio.poll()==Radio::State::Pending);assert(radio.ipv4()==0);
  shim::ip.ip.addr=0x0201a8c0;shim::ap.authmode=0;assert(radio.poll()==Radio::State::Pending);
  shim::ap.authmode=WIFI_AUTH_WPA2_PSK;
  for(uint32_t invalid:{0xffffffffu,0x0100007fu,0x010000e0u,0x0001a8c0u,0xff01a8c0u}){
   shim::ip.ip.addr=invalid;assert(radio.poll()==Radio::State::Pending);
  }
  shim::ip.ip.addr=0x0201a8c0;assert(radio.poll()==Radio::State::CandidateIp);
  // Success can be followed by another explicit attempt; init remains once.
  assert(radio.startRam(r));uint32_t second=shim::posted;assert(second!=first);
  acknowledge(first);shim::emit(IP_EVENT);assert(radio.poll()==Radio::State::Pending);
  acknowledge(second);assert(radio.poll()==Radio::State::Pending);
  assert(std::count(shim::calls.begin(),shim::calls.end(),"wifi_init")==1);
  for(unsigned i=0;i<33;++i)shim::emit(IP_EVENT);
  assert(radio.poll()==Radio::State::Failed&&radio.faulted());assert(!radio.startRam(r));
 }
 assert(shim::handlers.empty()&&!shim::netifExists);
 for(const char* failure:{"netif_init","loop_init","netif_create","wifi_init","RAM","mode","register","stop","barrier","dhcp_stop","clear_ip","get_ip","dhcp_start","protocol","config","start","connect"}){
  shim::reset();{
   EspRadioSdk sdk;EspRadio radio(sdk);shim::fail=failure;
   bool started=radio.startRam(r);
   if(started){acknowledge(shim::posted);assert(radio.poll()==Radio::State::Failed);}
   else assert(radio.faulted());
   shim::fail.clear(); // permit checked destructor cleanup
  }
  assert(shim::handlers.empty()&&!shim::netifExists);
 }
 // Single-blob schema and exact-byte round trip, with actual NVS adapter shim.
 shim::reset();uint8_t mac[]={2,3,4,5,6,7},wrong[]={2,3,4,5,6,8};
 NvsCredentialBlob blob;CredentialStorage storage(blob,mac);WifiRequest loaded;
 assert(storage.load(loaded)==Read::Missing);
 assert(storage.commitVerified(r)==Persistence::Verified);
 assert(storage.load(loaded)==Read::Ok&&!memcmp(loaded.ssid,r.ssid,33)&&!strcmp(loaded.password,r.password));
 {WifiRequest utf8;strcpy(utf8.ssid,"Caf\xc3\xa9");strcpy(utf8.password,"p\xc3\xa4ssword");
  uint8_t bytes[kCredentialMax]={};size_t n=encodeCredentials(utf8,mac,bytes);WifiRequest decoded;
  assert(n&&decodeCredentials(bytes,n,mac,decoded)&&!strcmp(decoded.ssid,utf8.ssid)&&!strcmp(decoded.password,utf8.password));
  ble_commands::wipeWifi(bytes,sizeof(bytes));}
 const auto good=shim::record;
 CredentialStorage wrongDevice(blob,wrong);assert(wrongDevice.load(loaded)==Read::Error);
 for(size_t i=0;i<good.size();++i){shim::record=good;shim::record[i]^=1;assert(storage.load(loaded)==Read::Error);}
 for(size_t n=0;n<good.size();++n){shim::record.assign(good.begin(),good.begin()+n);assert(storage.load(loaded)!=Read::Ok);}
 shim::record=good;shim::record.push_back(0);assert(storage.load(loaded)==Read::Error);
 shim::record.assign(kCredentialMax+1,0);assert(storage.load(loaded)==Read::Error);
 shim::record=good;shim::fail="open_write";assert(storage.commitVerified(r)==Persistence::NotCommitted&&shim::record==good);shim::fail.clear();
 WifiRequest next=r;next.password[0]='X';
 for(const char* fail:{"write","commit","read","open_read"}){
  shim::record=good;shim::fail=fail;size_t writesBefore=std::count(shim::calls.begin(),shim::calls.end(),"write");
  assert(storage.commitVerified(next)==Persistence::Unconfirmed);
  assert(shim::record!=good); // ambiguous error can leave NEW data; never rollback or retry
  assert(size_t(std::count(shim::calls.begin(),shim::calls.end(),"write"))==writesBefore+1);
  shim::fail.clear();
 }
 memset(next.password,'p',63);next.password[63]=0;
 assert(storage.commitVerified(next)==Persistence::Verified&&shim::record.size()==kCredentialMax);
 next.password[0]=0;assert(storage.commitVerified(next)==Persistence::NotCommitted);
 // Attempt + exact ESP SDK/NVS adapter implementations, with only SDK calls shimmed.
 shim::reset();{
  EspRadioSdk sdk;EspRadio radio(sdk);NvsCredentialBlob backend;CredentialStorage realStorage(backend,mac);
  Attempt attempt(monotonicNow);ble_commands::WifiSecurity security;security.connect(9);security.update(9,true,true,true);
  ble_commands::WifiAuthentication auth;auth.code=ble_commands::Code::Ok;auth.generation=9;auth.request=r;
  assert(attempt.reserve(9,auth,true,false,185,0));attempt.service(0,9,radio,realStorage,security);
  assert(std::count(shim::calls.begin(),shim::calls.end(),"write")==0);
  acknowledge(shim::posted);attempt.service(1,9,radio,realStorage,security);
  assert(std::count(shim::calls.begin(),shim::calls.end(),"write")==0);
  shim::associated=true;memcpy(shim::ap.ssid,r.ssid,32);shim::ip.ip.addr=0x0201a8c0;shim::ip.netmask.addr=0x00ffffff;
  completedClock=502;attempt.service(2,9,radio,realStorage,security);ble_commands::Code code;
  assert(attempt.result(9,502,code)&&code==ble_commands::Code::WifiOk&&attempt.persistence()==Persistence::Verified);
  attempt.release();
  assert(attempt.reserve(9,auth,true,false,185,503));attempt.service(503,9,radio,realStorage,security);
  acknowledge(shim::posted);attempt.service(504,9,radio,realStorage,security);
  shim::associated=true;shim::ip.ip.addr=0x0201a8c0;shim::ip.netmask.addr=0x00ffffff;shim::fail="read";
  completedClock=1005;attempt.service(505,9,radio,realStorage,security);
  assert(attempt.result(9,1005,code)&&code==ble_commands::Code::WifiFail&&attempt.persistence()==Persistence::Unconfirmed);
  assert(std::count(shim::calls.begin(),shim::calls.end(),"write")==2);shim::fail.clear();
 }
 std::cout<<"PASS: real ESP radio/NVS adapter SDK shims, API failures/barriers/live identity/IP/overflow, record corruption and persistence ambiguity\n";
}
