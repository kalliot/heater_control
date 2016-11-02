#include "bypassvalve.h"
#include "Iot.h"


bypassValve::bypassValve(void) {
}

bypassValve::bypassValve(Timer *sched,char *name,struct ad *ad,int up,int dn,int timeMultiplier,int latency,float sensitivity,int minTurnTime,int maxTurnTime) {
  set(sched,name,ad,up,dn,timeMultiplier,latency,sensitivity,minTurnTime,maxTurnTime);
}

void bypassValve::set(Timer *sched,char *name,struct ad *ad,int up,int dn,int timeMultiplier,int latency,float sensitivity,int minTurnTime,int maxTurnTime) {
  _sched=sched;
  _name=name;
  _ad=ad;
  _ad=ad;
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
    _prevVal=val;
  }
}

int bypassValve::turnBypass(time_t ts,Iot *iot) {
  int ret=1;
  float diff;
  long turntime;

  if (_ad->measured.analog==-273) return 0; // prog has just started.


  diff = _targetTemp - _ad->measured.analog;
  turntime=_minTurnTime + abs(diff) * _timeMultiplier;

  if (turntime > _maxTurnTime)
    turntime=_maxTurnTime;

  if (diff > _sensitivity) {              // temp is too cold
    if ((ts - _changed) > _latency-1)
      _changed =ts;
    else
      return 0;

    if (turntime < (2 * _minTurnTime)) {
      if (_ad->direction > 0) { // temp is a bit cold but increasing, do nothing
	return 0;
      }
      else {
	if ((_ad->direction != _ad->angular_velocity) && (_ad->angular_velocity > 70)) {
	  // temp is a bit cold, and decreasing. But it is turning up, so doing nothing.
	  return 0;
	}
      }
    }
    _sched->oscillate(_up,turntime,LOW,1);
    iot->toggle("radiator_up",1,turntime);
  }
  else if (diff < (-1.0 * _sensitivity)) { // temp is too hot
    if ((ts - _changed) > ((_latency*1.3)-1)) // latency is 30% bigger when turning down, because system cools down slowly.
      _changed =ts;
    else
      return 0;

    if (turntime < (3 * _minTurnTime)) {
      if  (_ad->direction < 0) { // temp is a bit hot, but decreasing, do nothing.
	return 0;
      }
      else {
	if ((_ad->direction != _ad->angular_velocity) && (_ad->angular_velocity < -70)) {
	  // temp is a bit hot, and increasing. But it is turning down, so do nothing.
	  return 0;
	}
      }
    }
    _sched->oscillate(_dn,turntime,LOW,1);
    iot->toggle("radiator_dn",1,turntime);
  }
  else
    ret=0;
  return ret;
}


