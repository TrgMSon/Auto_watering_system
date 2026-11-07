#include "ble_auth.h"
#include <string.h>
#include <stdio.h>
namespace ble_commands {
namespace {
bool utf8(const char* data) {
 const uint8_t* p=reinterpret_cast<const uint8_t*>(data);
 while(*p) {
  uint32_t c=*p++; unsigned more=0; uint32_t min=0;
  if(c<128) continue;
  if(c>=0xc2&&c<=0xdf){c&=31;more=1;min=0x80;}
  else if(c>=0xe0&&c<=0xef){c&=15;more=2;min=0x800;}
  else if(c>=0xf0&&c<=0xf4){c&=7;more=3;min=0x10000;} else return false;
  while(more--){if((*p&0xc0)!=0x80)return false;c=(c<<6)|(*p++&63);}
  if(c<min||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return false;
 }
 return true;
}
bool valid(const WifiRequest& r) {
 size_t s=0,p=0;
 while(s<sizeof(r.ssid)&&r.ssid[s])++s;
 while(p<sizeof(r.password)&&r.password[p])++p;
 if(s==sizeof(r.ssid)||p==sizeof(r.password))return false;
 return s>=1&&s<=32&&p>=8&&p<=63&&!strchr(r.ssid,'|')&&!strchr(r.password,'|')&&utf8(r.ssid)&&utf8(r.password);
}
class Json {
 const char* p_;const char* end_;
 bool hex4(uint32_t& n) {
  n=0;for(unsigned i=0;i<4;++i){if(p_==end_)return false;char c=*p_++;unsigned d;
   if(c>='0'&&c<='9')d=c-'0';else if(c>='a'&&c<='f')d=c-'a'+10;else if(c>='A'&&c<='F')d=c-'A'+10;else return false;
   n=(n<<4)|d;}return true;
 }
 public:
 Json(const char* p,size_t n):p_(p),end_(p+n){}
 void ws(){while(p_!=end_&&(*p_==' '||*p_=='\t'||*p_=='\n'||*p_=='\r'))++p_;}
 bool take(char c){ws();if(p_==end_||*p_!=c)return false;++p_;return true;}
 bool done(){ws();return p_==end_;}
 bool string(char* out,size_t cap){
  if(!take('"'))return false;
  size_t used=0;
  auto put=[&](uint8_t c){if(!c||used+1>=cap)return false;out[used++]=char(c);return true;};
  while(p_!=end_&&*p_!='"'){
   uint8_t c=uint8_t(*p_++);if(c<32)return false;
   if(c!='\\'){if(!put(c))return false;continue;}
   if(p_==end_)return false;
   char escape=*p_++;
   if(escape=='u'){
    uint32_t cp;if(!hex4(cp))return false;
    if(cp>=0xd800&&cp<=0xdbff){
     if(end_-p_<2||p_[0]!='\\'||p_[1]!='u')return false;
     p_+=2;uint32_t low;if(!hex4(low)||low<0xdc00||low>0xdfff)return false;
     cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);
    }else if(cp>=0xdc00&&cp<=0xdfff)return false;
    if(cp<0x80){if(!put(uint8_t(cp)))return false;}
    else if(cp<0x800){if(!put(0xc0|(cp>>6))||!put(0x80|(cp&63)))return false;}
    else if(cp<0x10000){if(!put(0xe0|(cp>>12))||!put(0x80|((cp>>6)&63))||!put(0x80|(cp&63)))return false;}
    else if(!put(0xf0|(cp>>18))||!put(0x80|((cp>>12)&63))||!put(0x80|((cp>>6)&63))||!put(0x80|(cp&63)))return false;
   }else{
    uint8_t decoded=0;
    switch(escape){case '"':case '\\':case '/':decoded=escape;break;case 'b':decoded=8;break;case 'f':decoded=12;break;case 'n':decoded=10;break;case 'r':decoded=13;break;case 't':decoded=9;break;default:return false;}
    if(!put(decoded))return false;
   }
  }
  if(p_==end_)return false;
  ++p_;out[used]=0;return utf8(out);
 }
};
}
bool validWifiRequest(const WifiRequest& request) { return valid(request); }
Code parseWifi(const char* bytes,size_t size,WifiRequest& request){
 request.clear();if(!bytes||!size||size>kWifiPayloadMax)return Code::BadJson;
 WifiRequest r;Json json(bytes,size);unsigned seen=0;if(!json.take('{'))return Code::BadJson;
 for(unsigned i=0;i<3;++i){
  char key[6]={};if(!json.string(key,sizeof(key))||!json.take(':'))return Code::BadJson;
  unsigned bit=!strcmp(key,"ssid")?1:!strcmp(key,"pass")?2:!strcmp(key,"sig")?4:0;
  if(!bit||(seen&bit))return Code::BadJson;
  seen|=bit;
  if(bit==4){
   char signature[kWifiPayloadMax+1]={};
   const bool ok=json.string(signature,sizeof(signature));
   bool sigOk=ok&&strlen(signature)==16;
   for(size_t j=0;sigOk&&j<16;++j)if(!((signature[j]>='0'&&signature[j]<='9')||(signature[j]>='a'&&signature[j]<='f')))sigOk=false;
   if(sigOk)memcpy(r.signature,signature,17);
   wipeWifi(signature,sizeof(signature));
   if(!ok)return Code::BadJson;
   if(!sigOk)return Code::BadSig;
  }else if(!json.string(bit==1?r.ssid:r.password,bit==1?sizeof(r.ssid):sizeof(r.password)))return Code::BadJson;
  if(json.take('}')){if(seen!=7||!json.done())return Code::BadJson;if(!valid(r))return Code::OutOfRange;request=r;return Code::Ok;}
  if(!json.take(','))return Code::BadJson;
 }
 return Code::BadJson;
}
size_t wifiCanonical(const char* nonce,const WifiRequest& r,char* out,size_t capacity){
 if(!out||!capacity)return 0;
 out[0]=0;if(!nonce||!valid(r))return 0;
 for(unsigned i=0;i<16;++i)if(!((nonce[i]>='0'&&nonce[i]<='9')||(nonce[i]>='a'&&nonce[i]<='f')))return 0;
 if(nonce[16])return 0;
 int n=snprintf(out,capacity,"%s|WIFI|%s|%s",nonce,r.ssid,r.password);
 if(n<0||size_t(n)>=capacity){wipeWifi(out,capacity);return 0;}return size_t(n);
}
WifiAuthentication Authenticator::authenticateWifi(const char* bytes, size_t size, uint32_t now,
    const WifiSecurity& security, uint64_t generation, uint16_t mtu,
    bool prepared, bool withResponse, uint16_t offset, uint64_t encryptedWriteGeneration) {
  WifiAuthentication out;
  const bool usable = valid_ && static_cast<uint32_t>(now - issuedMs_) < kNonceTtlMs;
  consume();
  // Neither gate may inspect/dereference payload on failure.
  if (!security.permitsWrite(generation,encryptedWriteGeneration)) { out.securityDenied = true; return out; }
  out.generation = generation;
  out.encryptedWriteGeneration = encryptedWriteGeneration;
  if (prepared || !withResponse || offset || mtu < 3 || !size ||
      size > kWifiPayloadMax || size > size_t(mtu - 3)) { out.transportDenied = true; return out; }
  if (!keySize_) { out.missingKey = true; return out; }
  if (!usable) { out.code = Code::NonceExpired; return out; }
  out.code = parseWifi(bytes, size, out.request);
  if (out.code != Code::Ok) return out;
  char canonical[kWifiCanonicalCapacity] = {};
  const size_t n = wifiCanonical(nonce_, out.request, canonical, sizeof(canonical));
  if (!n) { out.missingKey = true; return out; }
  out.code = verifySignature(canonical, n, out.request.signature, out.missingKey);
  wipeWifi(canonical, sizeof(canonical));
  if (out.code != Code::Ok || out.missingKey) out.request.clear();
  return out;
}
}
