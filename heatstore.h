#ifndef __HEATSTORE__
#define __HEATSTORE__

#include <Time.h>
#include "adlimit.h"

class heatStore {
public:
  heatStore(int port,float target,class adlimit *adl,float target2=0,float hysteresis=2,int latency=30);
  void chkTarget(time_t ts,float v);
  void refresh(time_t ts,float current);
  
private:
  int _port;
  int _state;
  int _latency;
  time_t _lastChk;
  float _originalTarget;
  class adlimit *_adl;
  float _target;
  float _target2;
  float _current;
  float _hysteresis;
};

#endif
