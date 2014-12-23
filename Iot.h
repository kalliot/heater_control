#ifndef __Iot__
#define __Iot__

#include <Arduino.h>
#include "M2XStreamClient.h"

class Iot {
  public:
    Iot(M2XStreamClient *m2xsc);
    boolean setBufferSizes(int s);
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
    const char **_streamNames;
    int *_counts;
    const char **_ats;
    double *_values;
    int _pos;
    int _samplecnt;
    int _buffSizes;
};

#endif
