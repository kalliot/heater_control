#ifndef __EEPROMSETUP__
#define __EEPROMSETUP__

struct adsetup {
  int mindigital;
  float minvalue;
  int maxdigital;
  float maxvalue;
  float diff;
};

struct eepromsetup {
  int id;
  int meastimeout;
  char m2xfeed[33];
  char m2xkey[33];
  struct adsetup ad[4];
};

extern struct eepromsetup eepromsetup;

#endif
