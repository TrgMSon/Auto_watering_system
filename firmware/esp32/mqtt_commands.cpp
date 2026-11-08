#include "mqtt_commands.h"
#include <climits>
namespace plant_mqtt {
namespace {
class Json {
 public:
  Json(const char* b, size_t n):p(b),end(b+n){}
  void ws(){while(p<end&&(*p==' '||*p=='\n'||*p=='\r'||*p=='\t'))++p;}
  bool take(char c){ws();if(p==end||*p!=c)return false;++p;return true;}
  bool done(){ws();return p==end;}
  bool str(char* out,size_t cap){
    if(!take('"'))return false;
    size_t n=0;
    while(p<end&&*p!='"'){
      unsigned char c=*p++;
      if(c<32||c>126||c=='\\'||n+1>=cap)return false;
      out[n++]=char(c);
    }
    if(p==end)return false;
    ++p;out[n]=0;return true;
  }
  bool number(uint32_t& out){
    ws();if(p==end||*p<'0'||*p>'9')return false;
    bool zero=*p=='0';out=0;unsigned digits=0;
    while(p<end&&*p>='0'&&*p<='9'){
      unsigned d=unsigned(*p++-'0');
      if((zero&&digits)||out>(UINT32_MAX-d)/10)return false;
      out=out*10+d;++digits;
    }
    return true;
  }
  bool boolean(bool& out){
    ws();
    if(end-p>=4&&!memcmp(p,"true",4)){p+=4;out=true;return true;}
    if(end-p>=5&&!memcmp(p,"false",5)){p+=5;out=false;return true;}
    return false;
  }
  bool decimal(double& out, bool& valid){
    ws();const char* start=p;
    if(p<end&&*p=='-')++p;
    if(p==end||*p<'0'||*p>'9')return false;
    if(*p=='0')++p;
    else while(p<end&&*p>='0'&&*p<='9')++p;
    if(p<end&&*p=='.'){
      ++p;const char* digits=p;
      while(p<end&&*p>='0'&&*p<='9')++p;
      if(digits==p)return false;
    }
    if(p<end&&(*p=='e'||*p=='E')){
      ++p;if(p<end&&(*p=='+'||*p=='-'))++p;
      const char* digits=p;while(p<end&&*p>='0'&&*p<='9')++p;
      if(digits==p)return false;
    }
    char token[kCommandBytes+1];const size_t n=size_t(p-start);
    if(n>kCommandBytes)return false;
    memcpy(token,start,n);token[n]=0;errno=0;out=strtod(token,nullptr);
    if(errno==ERANGE||!isfinite(out))valid=false;
    return true;
  }
 private:const char* p;const char* end;
};
bool same(const PumpCommand& a,const PumpCommand& b){
  if(a.isConfig!=b.isConfig||a.on!=b.on||a.seconds!=b.seconds)return false;
  if(!a.isConfig)return true;
  if(a.automatic!=b.automatic||a.valuesValid!=b.valuesValid||a.hasDuration!=b.hasDuration)return false;
  for(unsigned i=0;i<8;++i)if(a.values[i]!=b.values[i])return false;
  return true;
}
bool validConfigurationValues(const PumpCommand& c){
  if(!c.isConfig||!c.valuesValid)return false;
  for(double value:c.values)if(!isfinite(value))return false;
  const auto* v=c.values;
  return v[0]>=0&&v[0]<v[1]&&v[1]<=100&&
      v[2]>=20&&v[2]<=100&&floor(v[2])==v[2]&&
      (!c.hasDuration||(v[3]>=1&&v[3]<=10&&floor(v[3])==v[3]))&&
      v[4]>=0&&v[4]<=v[5]&&v[5]<=100&&v[6]<=v[7];
}
}
bool parsePump(const char* b,size_t n,const char* identity,PumpCommand& out){
  out=PumpCommand{};
  if(!b||!n||n>kCommandBytes)return false;
  Json j(b,n);PumpCommand c;unsigned seen=0,params=0;char key[32],value[80];
  if(!j.take('{'))return false;
  do {
    if(!j.str(key,sizeof(key))||!j.take(':'))return false;
    unsigned bit=0;
    if(!strcmp(key,"command_id")){bit=1;if(!j.str(c.id,sizeof(c.id))||!c.id[0])return false;}
    else if(!strcmp(key,"device_id")){bit=2;if(!j.str(value,sizeof(value))||strcmp(value,identity))return false;}
    else if(!strcmp(key,"timestamp")){bit=4;if(!j.str(value,sizeof(value))||!value[0])return false;}
    else if(!strcmp(key,"action")){bit=8;if(!j.str(value,sizeof(value))||strcmp(value,"PUMP_CONTROL"))return false;}
    else if(!strcmp(key,"params")){
      bit=16;if(!j.take('{'))return false;
      do {
        if(!j.str(key,sizeof(key))||!j.take(':'))return false;
        unsigned field=0;
        if(!strcmp(key,"state")){
          field=1;if(!j.str(value,sizeof(value)))return false;
          if(!strcmp(value,"ON"))c.on=true;else if(strcmp(value,"OFF"))return false;
        }else if(!strcmp(key,"duration_seconds")){field=2;if(!j.number(c.seconds)||!c.seconds||c.seconds>INT32_MAX)return false;}
        else return false;
        if(params&field)return false;
        params|=field;
      }while(j.take(','));
      if(!j.take('}'))return false;
    }else return false;
    if(seen&bit)return false;
    seen|=bit;
  }while(j.take(','));
  if(!j.take('}')||!j.done()||seen!=31||!(params&1)||(c.on&&!(params&2)))return false;
  // OFF may carry duration in the publisher; it has no operational meaning.
  if(!c.on)c.seconds=0;
  out=c;return true;
}
bool parseConfiguration(const char* b,size_t n,const char* identity,PumpCommand& out){
  out=PumpCommand{};
  if(!b||!n||n>kCommandBytes)return false;
  PumpCommand c;c.isConfig=true;
  Json j(b,n);unsigned seen=0;char key[40],value[80];
  if(!j.take('{'))return false;
  do {
    if(!j.str(key,sizeof(key))||!j.take(':'))return false;
    unsigned bit=0;
    if(!strcmp(key,"command_id")){bit=1;if(!j.str(c.id,sizeof(c.id))||!c.id[0])return false;}
    else if(!strcmp(key,"device_id")){bit=2;if(!j.str(value,sizeof(value))||strcmp(value,identity))return false;}
    else if(!strcmp(key,"timestamp")){bit=4;if(!j.str(value,sizeof(value))||!value[0])return false;}
    else if(!strcmp(key,"config")){
      bit=8;unsigned fields=0;
      const char* names[]={"soil_moisture_min","soil_moisture_max","min_water_level_alert",
        "max_watering_duration_minutes","air_humidity_min","air_humidity_max",
        "air_temperature_min","air_temperature_max","auto_mode"};
      if(!j.take('{'))return false;
      do {
        if(!j.str(key,sizeof(key))||!j.take(':'))return false;
        unsigned i=0;while(i<9&&strcmp(key,names[i]))++i;
        if(i==9||(fields&(1u<<i)))return false;
        fields|=1u<<i;
        if(i==3)c.hasDuration=true;
        if(i==8){if(!j.boolean(c.automatic))return false;}
        else if(!j.decimal(c.values[i],c.valuesValid))return false;
      }while(j.take(','));
      // The deployed publisher omits duration; all other fields remain required.
      if(!j.take('}')||(fields!=511&&fields!=(511u&~(1u<<3))))return false;
    }else return false;
    if(seen&bit)return false;
    seen|=bit;
  }while(j.take(','));
  if(!j.take('}')||!j.done()||seen!=15)return false;
  c.valuesValid=validConfigurationValues(c);
  out=c;return true;
}
const char* applyConfiguration(irrigation::System& system,const PumpCommand& c){
  // Revalidate at the owner boundary before integer conversion or mutation.
  if(!validConfigurationValues(c))return "INVALID_CONFIG";
  configuration::Config candidate=system.configurationStatus().active();
  candidate.soilStart=float(c.values[0]);candidate.soilStop=float(c.values[1]);
  candidate.waterMin=float(c.values[2]);
  // Resolve omission at owner execution time, not at ingress/parse time.
  if(c.hasDuration)candidate.maxRunSeconds=uint32_t(c.values[3])*60;
  switch(system.applyRemoteConfiguration(candidate,c.automatic)){
    case irrigation::RemoteConfigResult::Ok:return "OK";
    case irrigation::RemoteConfigResult::Invalid:return "INVALID_CONFIG";
    case irrigation::RemoteConfigResult::PumpRunning:return "PUMP_RUNNING";
    case irrigation::RemoteConfigResult::StorageError:return "STORAGE_ERROR";
  }
  return "STORAGE_ERROR";
}
void Commands::cancel(const char* reason){
  for(auto& r:records_)if(r.state==Record::Pending&&(!online_||!r.command.off())){
    r.state=Record::Done;r.message=reason;
  }
}
bool Commands::submit(const PumpCommand& c,uint32_t now){
  if(!online_)return false;
  reclaim(now);
  for(auto& r:records_)if(r.state!=Record::Free&&!strcmp(r.command.id,c.id)){
    if(!same(r.command,c))return false;
    // Pending duplicates do nothing. Completed duplicates can re-arm bounded ACK attempts.
    if(r.state==Record::Done&&!r.ackFinished&&!r.ackInFlight&&r.attempts<3)r.lastAck=now-2000;
    return true;
  }
  if(!c.off()&&offPending())return false;
  for(unsigned i=c.off()?0:1;i<16;++i)if(records_[i].state==Record::Free){
    auto& r=records_[i];r.command=c;r.generation=generation_;r.queued=now;r.state=Record::Pending;
    if(c.off())cancel("superseded by OFF");
    return true;
  }
  return false;
}
void Commands::reclaim(uint32_t now){
  for(auto& r:records_)if(r.state==Record::Done&&r.ackFinished&&!r.ackInFlight&&
      uint32_t(now-r.ackEndedMs)>=kDedupRetentionMs)r=Record{};
}
void Commands::attempted(int slot,uint32_t now,bool inFlight){
  auto& r=records_[slot];++r.attempts;r.lastAck=now;r.ackInFlight=inFlight;
  if(!inFlight)finishAttempt(slot,now);
}
void Commands::finishAttempt(int slot,uint32_t now){
  auto& r=records_[slot];r.ackInFlight=false;
  if(r.attempts>=3&&!r.ackFinished){r.ackFinished=true;r.ackEndedMs=now;}
}
void Commands::accepted(int slot,uint32_t now){
  auto& r=records_[slot];
  if(!r.ackFinished){r.ackFinished=true;r.ackEndedMs=now;}
  r.ackInFlight=false;
}
bool Commands::offPending()const{for(const auto& r:records_)if(r.state==Record::Pending&&r.command.off())return true;return false;}
int Commands::claim(uint32_t now){
  int best=-1;
  for(unsigned i=0;i<16;++i){auto& r=records_[i];if(r.state!=Record::Pending)continue;
    if(!online_||r.generation!=generation_||uint32_t(now-r.queued)>=1000){r.state=Record::Done;r.message="expired or disconnected";continue;}
    if(best<0||(r.command.off()&&!records_[best].command.off()))best=int(i);
  }
  if(best>=0)records_[best].state=Record::Admitted;
  return best;
}
void Commands::complete(int i,irrigation::Result result,const char* detail){
  auto& r=records_[i];r.success=result==irrigation::Result::Accepted;
  r.message=detail?detail:irrigation::name(result);r.state=Record::Done;
}
void Commands::completeConfiguration(int i,const char* message){
  auto& r=records_[i];r.success=!strcmp(message,"OK");r.message=message;r.state=Record::Done;
}
bool Commands::ack(uint32_t now,int& slot,char* b,size_t cap,const char* identity){
  if(!online_)return false;
  for(unsigned i=0;i<16;++i){auto& r=records_[i];
    if(r.state!=Record::Done||r.ackFinished||r.ackInFlight||r.attempts>=3||(r.attempts&&uint32_t(now-r.lastAck)<2000))continue;
    // Identifier is validated ASCII JSON without quotes/backslashes by parser below.
    int n=snprintf(b,cap,"{\"device_id\":\"%s\",\"command_id\":\"%s\",\"status\":\"%s\",\"action\":\"%s\",\"message\":\"%s\"}",
      identity,r.command.id,r.success?"SUCCESS":"FAILED",r.command.isConfig?"CONFIG":r.command.on?"ON":"OFF",r.message);
    if(n<0||size_t(n)>=cap)return false;
    slot=int(i);return true;
  }return false;
}
}
