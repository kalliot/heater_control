#ifndef __BYPASSVALVE__
#define __BYPASSVALVE__

#include <Arduino.h>
#include <Time.h>
#include <Timer.h>
#include "conversion.h"
#include "Iot.h"
#include "AdInput.h"

class bypassValve {
 public:
  bypassValve(void);
  bypassValve(Timer *sched,char *name,struct ad *ad,int up,int dn,int timeMultiplier,
	      int latency,float sensitivity,int minTurnTime,int maxTurnTime);
  void set(Timer *sched,char *name,struct ad *ad,int up,int dn,int timeMultiplier,
	   int latency,float sensitivity,int minTurnTime,int maxTurnTime);
  void setGuide(float val);
  int turnBypass(time_t ts,Iot *iot);
  void setConverter(conversion *c);

 private:
  char *_name;
  struct ad *_ad;
  Timer *_sched;
  time_t _changed;
  int _up;
  int _dn;
  long _upmsec;
  float _targetTemp;
  int _timeMultiplier;
  int _latency;         // seconds, how long does it take after up, or down op the realise change in targetAd.
  float _sensitivity;
  float _prevVal;
  int _minTurnTime;     // milliseconds
  int _maxTurnTime;     // milliseconds
  conversion *_converter;
};

#endif
