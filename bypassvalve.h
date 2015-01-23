#ifndef __BYPASSVALVE__
#define __BYPASSVALVE__

#include <Arduino.h>
#include <Time.h>
#include <Timer.h>

class bypassValve {
 public:
  bypassValve(Timer *sched,char *name,float *actual,int up,int dn,int timeMultiplier,int latency,float sensitivity,int minTurnTime,int maxTurnTime);
  void setTarget(float val);
  int turnBypass(time_t ts);

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
  int _minTurnTime;     // milliseconds
  int _maxTurnTime;     // milliseconds
};

#endif
