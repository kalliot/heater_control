#include <Time.h>
#include "LedControl.h"
#include "spi7seg.h"

/**
 * spi7seg.cpp
 * Assumes the display unit is SPI two row 7 segment display
 * See: https://www.tindie.com/products/rajbex/double-row-4-digit-seven-segment-blue-led-display-module-with-spi-interface/
 * Todo: support for multiple cards.
 *
 */

spi7seg::spi7seg(LedControl *lc)
{
  _lc=lc;
  for (int i=0; i < lc->getDeviceCount(); i++) {
    _lc->shutdown(i, false);
    _lc->setIntensity(i, 3);
    _lc->clearDisplay(i);
  }
}

void spi7seg::dot(boolean showdot)
{
  _showdot=showdot;
}

void spi7seg::time(int device,int line)
{
  int ones;
  int tens;
  int hundreds;
  int thousands;

  int min=minute();
  int hours=hour();
  int row=line*4;

  ones=min % 10;
  min=min/10;
  tens=min % 10;
  
  hundreds=hours % 10;
  hours=hours/10;
  thousands=hours % 10;

  _lc->setDigit(device,row+3,(byte)thousands,false);
  _lc->setDigit(device,row+2,(byte)hundreds,true);
  _lc->setDigit(device,row+1,(byte)tens,false);
  _lc->setDigit(device,row+0,(byte)ones,false);
}

void spi7seg::date(int device,int line)
{
  int ones;
  int tens;
  int hundreds;
  int thousands;

  int days=day();
  int months=month();
  int row=line*4;

  ones=days % 10;
  days=days/10;
  tens=days % 10;
  
  hundreds=months % 10;
  months=months/10;
  thousands=months % 10;

  _lc->setDigit(device,row+3,(byte)tens,false);
  _lc->setDigit(device,row+2,(byte)ones,true);
  _lc->setDigit(device,row+1,(byte)thousands,false);
  _lc->setDigit(device,row+0,(byte)hundreds,false);
}

void spi7seg::number(int device,int line,float v) {
  int val;

  if (v<0)
    val=(int)(10 * v);
  else
    val=(int)(100 * v);
  number(device,line,val);
}

void spi7seg::number(int device,int line,int v) {
  int ones;
  int tens;
  int hundreds;
  int thousands;
  int row=line*4;

  if(v < -999 || v > 9999)
    return;
  if(v<0) {
    v=v*-1;
    ones=v%10;
    v=v/10;
    tens=v%10;
    v=v/10;
    hundreds=v;
    _lc->setChar(device,row+3,'-',false);
    _lc->setDigit(device,row+2,(byte)hundreds,false);
    _lc->setDigit(device,row+1,(byte)tens,_showdot);
  }
  else {
    ones=v%10;
    v=v/10;
    tens=v%10;
    v=v/10;
    hundreds=v%10;
    v=v/10;
    thousands=v;	
    _lc->setDigit(device,row+3,(byte)thousands,false);
    _lc->setDigit(device,row+2,(byte)hundreds,_showdot);
    _lc->setDigit(device,row+1,(byte)tens,false);
  }
  _lc->setDigit(device,row+0,(byte)ones,false);
}
