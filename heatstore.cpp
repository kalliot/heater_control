#include <Arduino.h>
#include "heatstore.h"


heatStore::heatStore(int port,float target,class adlimit *adl,float target2,float hysteresis,int latency)
{
  _port=port;
  _target=target;
  _originalTarget=target;
  if (_target2==0)
    _target2=target;
  else
    _target2=target2;
  _hysteresis=hysteresis;
   _latency=latency;
  _state=0;
  _lastChk=0;
}

void heatStore::chkTarget(time_t ts,float v)
{
  if (_adl->compare(v))
    _target=_target2;
  else
    _target=_originalTarget;
  refresh(ts,_current);
}


void heatStore::refresh(time_t ts,float current)
{
  _current=current;
  if (ts-_lastChk >= _latency) {
    _lastChk=ts;
    if (current < _target && _state==0) {
      _state = 1;
      digitalWrite(_port,1);
    }
    if (current > _target + _hysteresis && _state==1) {
      _state = 0;
      digitalWrite(_port,0);
    }
  }
}



