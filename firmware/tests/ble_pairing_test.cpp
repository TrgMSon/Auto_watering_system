#include "../esp32/ble_pairing.h"
#include "../esp32/ble_auth.h"
#include "../esp32/pump_serial.h"
#include <string>
#include <cassert>
#include <iostream>
using namespace ble_pairing;
struct FakeSdk:Sdk{
 bool setupOk=true,lookupOk=true,bond=false;unsigned setupCalls=0;
 bool setup()override{++setupCalls;return setupOk;}
 bool bonded(const uint8_t*,bool& found)override{found=bond;return lookupOk;}
 bool encrypt(const uint8_t*)override{return true;}
 bool respond(const uint8_t*,bool)override{return true;}
};
int main(){
 pump::SerialCommands serial;const char* line=nullptr;Command parsed=Command::None;
 for(char c:std::string("BLE_PAIR\n"))if(serial.feedLine(c,line))parsed=command(line);
 assert(parsed==Command::Open);
 for(char c:std::string(70,'x')+"\n")if(serial.feedLine(c,line))assert(command(line)==Command::None);

 assert(command("BLE_PAIR")==Command::Open&&command("BLE_PAIR_CLOSE")==Command::Close);
 assert(command(nullptr)==Command::None&&command("BLE_PAIR x")==Command::None);
 assert(ownerAllows(Command::Open,true,false));assert(!ownerAllows(Command::Open,false,false));assert(!ownerAllows(Command::Open,true,true));
 assert(ownerAllows(Command::Close,false,true));
 for(uint32_t start:{0u,UINT32_MAX-100u}){
  Window w;assert(!w.open(start));w.set(true,start);assert(w.open(start)&&w.open(start+59999));assert(!w.open(start+60000));
  w.set(true,start);w.set(false,start+1);assert(!w.open(start+2));
 }
 FakeSdk sdk;State s;uint8_t a[]={1,2,3,4,5,6},b[]={6,5,4,3,2,1};
 assert(s.setup(sdk));s.connect(a,1);assert(!s.admit(a,1,0,sdk));
 s.window.set(true,100);assert(s.admit(a,1,101,sdk));s.window.set(false,102);
 assert(s.admit(a,1,70000,sdk)); // admitted procedure may finish after close/expiry
 sdk.bond=true;assert(s.authenticated(a,1,true,true,sdk));
 assert(!s.security.permits(1)); // AUTH_CMPL alone must never claim encryption
 s.disconnect(1);s.connect(a,2);assert(!s.security.permits(1)&&!s.security.permits(2));
 assert(s.admit(a,2,70001,sdk)); // known bond may reconnect with window closed
 assert(!s.authenticated(a,1,true,true,sdk));
 assert(!s.authenticated(b,2,true,true,sdk));
 assert(!s.authenticated(a,2,false,true,sdk));assert(!s.security.permits(2));
 sdk.bond=false;assert(!s.admit(a,2,70002,sdk));
 s.window.set(true,70002);assert(s.admit(a,2,70002,sdk));sdk.bond=true;
 assert(!s.authenticated(a,2,true,false,sdk));
 sdk.lookupOk=false;assert(!s.admit(a,2,70003,sdk));sdk.lookupOk=true;
 FakeSdk failed;failed.setupOk=false;State disabled;assert(!disabled.setup(failed));disabled.window.set(true,0);disabled.connect(a,8);assert(!disabled.admit(a,8,0,failed));
 Binding binding;binding.connect(a,1);assert(binding.capture(a)==1&&!binding.capture(b));assert(binding.begin(a,1));
 binding.disconnect(1);binding.connect(a,2);assert(!binding.begin(a,2));
 assert(binding.complete(a)==1);assert(!binding.complete(a));assert(binding.begin(a,2));
 assert(!binding.complete(b));binding.cancel(2);assert(!binding.complete(a));
 assert(binding.begin(a,2));binding.disconnect(2);assert(!binding.capture(a));assert(binding.complete(a)==2);
 assert(ble_commands::authenticatedWrite(3)&&!ble_commands::authenticatedWrite(6));
 std::cout<<"PASS: pairing window/rollover/USB admission/bond policy/setup failure, peer+procedure generation and unconfirmed encryption fail closed\n";
}
