#include "bypassvalve.h"


bypassValve::bypassValve(void) {
}

bypassValve::bypassValve(Timer *sched,char *name,float *actual,int up,int dn,int timeMultiplier,int latency,float sensitivity,int minTurnTime,int maxTurnTime) {
  set(sched,name,actual,up,dn,timeMultiplier,latency,sensitivity,minTurnTime,maxTurnTime);
}

void bypassValve::set(Timer *sched,char *name,float *actual,int up,int dn,int timeMultiplier,int latency,float sensitivity,int minTurnTime,int maxTurnTime) {
  _sched=sched;
  _name=name;
  _actualTemp=actual;
  _up=up;
  _dn=dn;
  _timeMultiplier=timeMultiplier;
  _latency=latency;
  _changed=0;
  _sensitivity=sensitivity;
  _minTurnTime=minTurnTime;
  _maxTurnTime=maxTurnTime;
  _targetTemp=-273;
  _prevVal=-273;
}

void bypassValve::setConverter(conversion *c)
{
  _converter=c;
}

void bypassValve::setGuide(float val) {
  if (_prevVal != val) {
    _targetTemp=_converter->resolve(val);
    _prevVal=val;
  }
}

int bypassValve::turnBypass(time_t ts) {
  int ret=1;
  float diff;
  long turntime;

  if (*_actualTemp==-273) return ret;

  if ((ts - _changed) > _latency-1)
    _changed =ts;
  else {
    return ret;
  }

  diff = _targetTemp - *_actualTemp;
  turntime=abs(diff) * _timeMultiplier;

  if (turntime > _maxTurnTime)
    turntime=_maxTurnTime;
  else if (turntime < _minTurnTime)
    turntime=_minTurnTime;

  if (diff > _sensitivity)
    _sched->oscillate(_up,turntime,LOW,1);
  else if (diff < (-1.0 * _sensitivity))
    _sched->oscillate(_dn,turntime,LOW,1);
  else
    ret=0;
  return ret;
}


