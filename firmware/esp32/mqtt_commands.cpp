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
 private:const char* p;const char* end;
};
bool same(const PumpCommand& a,const PumpCommand& b){return a.on==b.on&&a.seconds==b.seconds;}
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
void Commands::cancel(const char* reason){
  for(auto& r:records_)if(r.state==Record::Pending&&(!online_||r.command.on)){
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
  if(c.on&&offPending())return false;
  for(unsigned i=c.on?1:0;i<16;++i)if(records_[i].state==Record::Free){
    auto& r=records_[i];r.command=c;r.generation=generation_;r.queued=now;r.state=Record::Pending;
    if(!c.on)cancel("superseded by OFF");
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
bool Commands::offPending()const{for(const auto& r:records_)if(r.state==Record::Pending&&!r.command.on)return true;return false;}
int Commands::claim(uint32_t now){
  int best=-1;
  for(unsigned i=0;i<16;++i){auto& r=records_[i];if(r.state!=Record::Pending)continue;
    if(!online_||r.generation!=generation_||uint32_t(now-r.queued)>=1000){r.state=Record::Done;r.message="expired or disconnected";continue;}
    if(best<0||(!r.command.on&&records_[best].command.on))best=int(i);
  }
  if(best>=0)records_[best].state=Record::Admitted;
  return best;
}
void Commands::complete(int i,irrigation::Result result,const char* detail){
  auto& r=records_[i];r.success=result==irrigation::Result::Accepted;
  r.message=detail?detail:irrigation::name(result);r.state=Record::Done;
}
bool Commands::ack(uint32_t now,int& slot,char* b,size_t cap,const char* identity){
  if(!online_)return false;
  for(unsigned i=0;i<16;++i){auto& r=records_[i];
    if(r.state!=Record::Done||r.ackFinished||r.ackInFlight||r.attempts>=3||(r.attempts&&uint32_t(now-r.lastAck)<2000))continue;
    // Identifier is validated ASCII JSON without quotes/backslashes by parser below.
    int n=snprintf(b,cap,"{\"device_id\":\"%s\",\"command_id\":\"%s\",\"status\":\"%s\",\"action\":\"%s\",\"message\":\"%s\"}",
      identity,r.command.id,r.success?"SUCCESS":"FAILED",r.command.on?"ON":"OFF",r.message);
    if(n<0||size_t(n)>=cap)return false;
    slot=int(i);return true;
  }return false;
}
}
