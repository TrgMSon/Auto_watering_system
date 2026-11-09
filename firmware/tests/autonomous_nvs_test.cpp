#define ARDUINO 1
#include "../esp32/autonomous_nvs.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
using namespace autonomous_storage;
static std::vector<uint8_t> flash,staged;
static int fail=0,readCalls=0,commits=0,closes=0;
esp_err_t nvs_open(const char* ns,int,nvs_handle_t* h){assert(!strcmp(ns,"watering_auto"));*h=1;return fail==1?99:ESP_OK;}
void nvs_close(nvs_handle_t){++closes;}
esp_err_t nvs_set_blob(nvs_handle_t,const char* key,const void* bytes,size_t size){
 assert(!strcmp(key,"state"));if(fail==2)return 99;
 const auto p=static_cast<const uint8_t*>(bytes);staged.assign(p,p+size);return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t){++commits;if(fail==3)return 99;flash=staged;return ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t,const char* key,void* out,size_t* size){
 assert(!strcmp(key,"state"));++readCalls;
 if(fail==4||(!out&&fail==5)||(out&&fail==6))return 99;
 if(flash.empty())return ESP_ERR_NVS_NOT_FOUND;
 if(!out){*size=flash.size();return ESP_OK;}
 assert(*size>=flash.size());memcpy(out,flash.data(),flash.size());*size=flash.size();return ESP_OK;
}
int main(){
 NvsStorage nvs;uint8_t bytes[kRecordSize]={},back[kRecordSize];size_t n=0;
 assert(nvs.read(back,sizeof(back),n)==Read::Missing);unsigned cases=1;
 assert(nvs.writeCommit(bytes,sizeof(bytes)));assert(commits==1);
 assert(nvs.read(back,sizeof(back),n)==Read::Ok&&n==sizeof(bytes));++cases;
 for(int i=1;i<=3;++i){fail=i;int before=commits;assert(!nvs.writeCommit(bytes,sizeof(bytes)));
   if(i<3) { assert(commits==before); }
   ++cases;}
 for(int i=4;i<=6;++i){fail=i;assert(nvs.read(back,sizeof(back),n)==Read::Error&&n==0);++cases;}
 fail=0;flash.resize(kRecordSize+1);assert(nvs.read(back,sizeof(back),n)==Read::Error);++cases;
 assert(closes>0&&readCalls>0);
 std::cout<<"PASS "<<cases<<" NVS adapter scenarios (SDK shim only)\n";
}
