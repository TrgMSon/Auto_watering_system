#include "../esp32/ble_pairing_esp32.h"
#include <cassert>
#include <iostream>
static int calls=0,failAt=0,count=1;static bool listFail=false,apiFail=false;static esp_ble_bond_dev_t* lastBuffer=nullptr;
static uint8_t peer[]={1,2,3,4,5,6};
int esp_ble_gap_set_security_param(int parameter,void* data,uint8_t n){
 assert(n==1);const uint8_t expected[]={9,3,16,3,3,1};assert(parameter==calls&&*static_cast<uint8_t*>(data)==expected[calls]);
 ++calls;return calls==failAt?1:0;
}
int esp_ble_get_bond_device_num(){return count;}
int esp_ble_get_bond_device_list(int* n,esp_ble_bond_dev_t* out){lastBuffer=out;memset(out,0x5a,sizeof(*out)*size_t(*n));memcpy(out[0].bd_addr,peer,6);return listFail?1:0;}
int esp_ble_set_encryption(esp_bd_addr_t p,int action){assert(!memcmp(p,peer,6)&&action==ESP_BLE_SEC_ENCRYPT);return apiFail?1:0;}
int esp_ble_gap_security_rsp(esp_bd_addr_t p,bool){assert(!memcmp(p,peer,6));return apiFail?1:0;}
int main(){
 ble_pairing::EspSdk sdk;
 for(int failure=0;failure<=6;++failure){calls=0;failAt=failure;assert(sdk.setup()==(failure==0));}
 bool found=false;assert(sdk.bonded(peer,found)&&found);
 const uint8_t* bytes=reinterpret_cast<uint8_t*>(lastBuffer);for(size_t i=0;i<sizeof(esp_ble_bond_dev_t);++i)assert(bytes[i]==0);
 listFail=true;assert(!sdk.bonded(peer,found)&&!found);for(size_t i=0;i<sizeof(esp_ble_bond_dev_t);++i)assert(bytes[i]==0);listFail=false;
 count=0;assert(sdk.bonded(peer,found)&&!found);count=17;assert(!sdk.bonded(peer,found));count=-1;assert(!sdk.bonded(peer,found));
 assert(sdk.encrypt(peer)&&sdk.respond(peer,true)&&sdk.respond(peer,false));apiFail=true;assert(!sdk.encrypt(peer)&&!sdk.respond(peer,true));
 std::cout<<"PASS: exact ESP security SDK setup parameters/failures, bounded bond lookup/key wipe and checked encryption/response calls (shim)\n";
}
