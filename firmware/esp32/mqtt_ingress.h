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
  bool fragment(const char* topic,int topicSize,const char* data,int size,int total,int offset,bool retained,const char* expected,uint64_t epoch=0){
    if(offset==0){
      active_=false;used_=0;
      if(retained||!topic||topicSize<=0||size_t(topicSize)!=strlen(expected)||memcmp(topic,expected,size_t(topicSize))||total<=0||size_t(total)>kCommandBytes)return false;
      total_=size_t(total);active_=true;assemblyEpoch_=epoch;
    }
    if(!active_||retained||offset<0||size<0||!data||size_t(offset)!=used_||total<=0||size_t(total)!=total_||size_t(size)>total_-used_){active_=false;return false;}
    if(offset&&topicSize&&(topicSize<0||!topic||size_t(topicSize)!=strlen(expected)||memcmp(topic,expected,size_t(topicSize)))){active_=false;return false;}
    memcpy(assembly_+used_,data,size_t(size));used_+=size_t(size);
    if(used_!=total_)return true;
    active_=false;
    if(count_==4)return false;
    unsigned tail=(head_+count_)%4;memcpy(queue_[tail],assembly_,used_);length_[tail]=used_;epochs_[tail]=assemblyEpoch_;++count_;return true;
  }
  bool pop(char* out,size_t& size,uint64_t* epoch=nullptr){if(!count_)return false;size=length_[head_];memcpy(out,queue_[head_],size);if(epoch)*epoch=epochs_[head_];head_=(head_+1)%4;--count_;return true;}
 private:
  char assembly_[kCommandBytes]={},queue_[4][kCommandBytes]={};
  size_t used_=0,total_=0,length_[4]={};unsigned head_=0,count_=0;bool active_=false;
  uint64_t assemblyEpoch_=0,epochs_[4]={};
};
}
