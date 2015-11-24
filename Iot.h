#ifndef __Iot__
#define __Iot__

#include <Arduino.h>

class Iot {
 public:
  Iot();
  boolean setBufferSizes(int s);
  void reset();
  void name(char *name);
  void addValue(float value,char *ts);
  void next();
  int  getRecCnt();
  int  toggle(char *name,int state,int duration);
  int  send();
  void showCounters();
  void showStreamnames();
  void showTimes();
  void showValues();
 private:
  char pubkey[45];
  char subkey[45];
  char channel[20];
  const char **_streamNames;
  int *_counts;
  const char **_ats;
  double *_values;
  int _pos;
  int _samplecnt;
  int _buffSizes;
};

#endif
