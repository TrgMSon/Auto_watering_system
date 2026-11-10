#pragma once
#include "mqtt_commands.h"
namespace plant_mqtt {
// Externally locked. Capture early SUBACKs even if callback precedes subscribe()
// returning its ID. Match each exact ID independently, never infer from CONNACK.
class Subscriptions {
 public:
  void reset(){for(auto& id:ids_)id=-1;for(auto& e:events_)e=Event{};next_=0;}
  void requested(unsigned topic,int id){ids_[topic]=id;}
  void received(int id,bool granted){events_[next_++%4]=Event{id,granted};}
  int state(unsigned topic)const{
    if(ids_[topic]<0)return 0;
    for(const auto& e:events_)if(e.id==ids_[topic])return e.granted?1:-1;
    return 0;
  }
 private:
  struct Event {int id;bool granted;Event(int value=-1,bool ok=false):id(value),granted(ok){}};
  int ids_[2]={-1,-1};Event events_[4];unsigned next_=0;
};
// One SDK message assembly, four completed envelopes. External serialization.
class Ingress {
 public:
  void reset(){active_=false;used_=0;head_=count_=0;}
  bool fragment(const char* topic,int topicSize,const char* data,int size,int total,int offset,bool retained,const char* expected,uint64_t epoch=0,bool allowRetained=false){
    if(offset==0){
      active_=false;used_=0;
      if((retained&&!allowRetained)||!topic||topicSize<=0||size_t(topicSize)!=strlen(expected)||memcmp(topic,expected,size_t(topicSize))||total<=0||size_t(total)>kCommandBytes||count_==4)return false;
      total_=size_t(total);active_=true;assemblyEpoch_=epoch;pendingSlot_=(head_+count_)%4;
    }
    if(!active_||(retained&&!allowRetained)||offset<0||size<0||!data||size_t(offset)!=used_||total<=0||size_t(total)!=total_||size_t(size)>total_-used_){active_=false;return false;}
    if(offset&&topicSize&&(topicSize<0||!topic||size_t(topicSize)!=strlen(expected)||memcmp(topic,expected,size_t(topicSize)))){active_=false;return false;}
    // Assemble directly in the reserved queue slot. The previous separate
    // assembly buffer duplicated one full command-sized buffer per ingress.
    memcpy(queue_[pendingSlot_]+used_,data,size_t(size));used_+=size_t(size);
    if(used_!=total_)return true;
    active_=false;
    length_[pendingSlot_]=used_;epochs_[pendingSlot_]=assemblyEpoch_;++count_;return true;
  }
  bool pop(char* out,size_t& size,uint64_t* epoch=nullptr){if(!count_)return false;size=length_[head_];memcpy(out,queue_[head_],size);if(epoch)*epoch=epochs_[head_];head_=(head_+1)%4;--count_;return true;}
 private:
  char queue_[4][kCommandBytes]={};
  size_t used_=0,total_=0,length_[4]={};unsigned head_=0,count_=0,pendingSlot_=0;bool active_=false;
  uint64_t assemblyEpoch_=0,epochs_[4]={};
};
}
