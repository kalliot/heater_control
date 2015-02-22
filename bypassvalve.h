#ifndef __BYPASSVALVE__
#define __BYPASSVALVE__

#include <Arduino.h>
#include <Time.h>
#include <Timer.h>
#include "conversion.h"

class bypassValve {
 public:
  bypassValve(void);
  bypassValve(Timer *sched,char *name,float *actual,int up,int dn,int timeMultiplier,
	      int latency,float sensitivity,int minTurnTime,int maxTurnTime);
  void set(Timer *sched,char *name,float *actual,int up,int dn,int timeMultiplier,
	   int latency,float sensitivity,int minTurnTime,int maxTurnTime);
  void setGuide(float val);
  int turnBypass(time_t ts);
  void setConverter(conversion *c);

 private:
  char *_name;
  float *_actualTemp;
  Timer *_sched;
  time_t _changed;
  int _up;
  int _dn;
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
