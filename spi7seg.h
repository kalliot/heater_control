#ifndef __SPI7SEG__
#define __SPI7SEG__

#include "LedControl.h"

class spi7seg {
public:
  spi7seg(LedControl *lc);
  void dot(boolean showdot);
  void time(int device,int line);
  void date(int device,int line);
  void number(int device,int line,int v);
  void number(int device,int line,float v);
private:
  LedControl *_lc;
  boolean _showdot;
};

#endif
