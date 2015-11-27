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
  char pubkey[45];
  char subkey[45];
  char channel[20];
  struct adsetup ad[4];
};

extern struct eepromsetup eepromsetup;

#endif
