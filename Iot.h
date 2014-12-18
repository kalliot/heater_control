#ifndef __Iot__
#define __Iot__

#include <Arduino.h>
#include "M2XStreamClient.h"

class Iot {
  public:
    Iot(M2XStreamClient *m2xsc);
    void reset();
    void name(char *name);
    void addValue(float value,char *ts);
    void next();
    int  getRecCnt();
    int send(char *id);
    void showCounters();
    void showStreamnames();
    void showTimes();
    void showValues();
  private:
    M2XStreamClient *_m2xsc; 
    const char *_streamNames[5];
    int _counts[5];
    const char *_ats[6];
    double _values[6];
    int _pos;
    int _samplecnt;
};

#endif
