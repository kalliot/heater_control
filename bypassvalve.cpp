#include "bypassvalve.h"
#include "Iot.h"


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
  _upmsec=0;
}

void bypassValve::setConverter(conversion *c)
{
  _converter=c;
}

void bypassValve::setGuide(float val) {
  if (_prevVal != val) {
    _targetTemp=_converter->resolve(val);
    Serial.print("Target temp is ");
    Serial.println(_targetTemp);
    _prevVal=val;
  }
}


int bypassValve::turnBypass(time_t ts,Iot *iot) {
  int ret=1;
  float diff;
  long turntime;

  if (*_actualTemp==-273) return ret;

  diff = _targetTemp - *_actualTemp;
  turntime=_minTurnTime + abs(diff) * _timeMultiplier;

  if (turntime > _maxTurnTime)
    turntime=_maxTurnTime;

  if (diff > _sensitivity) {
    if ((ts - _changed) > _latency-1)
      _changed =ts;
    else {
      return ret;
    }
    _upmsec+=turntime;
    Serial.print("_upmsec=");
    Serial.println(_upmsec);
    _sched->oscillate(_up,turntime,LOW,1);
    iot->toggle("radiator_up",1,turntime);
  }
  else if (diff < (-1.0 * _sensitivity)) {
    if (_upmsec > _minTurnTime) {
      turntime=_upmsec * 0.3;
      _changed = ts;
      _upmsec=0;
    }
    else {
      if ((ts - _changed) > ((_latency*1.3)-1)) // latency is 30% bigger when turning down
	_changed =ts;
      else {
	return ret;
      }
    }
    _sched->oscillate(_dn,turntime,LOW,1);
    iot->toggle("radiator_dn",1,turntime);
  }
  else
    ret=0;
  return ret;
}


