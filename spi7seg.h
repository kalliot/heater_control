#ifndef __SPI7SEG__
#define __SPI7SEG__

#include "LedControl.h"

class spi7seg {
public:
  spi7seg(LedControl *lc);
  void time(int line);
  void date(int line);
  void number(int line,int v);

private:
  LedControl *_lc;
};

#endif
