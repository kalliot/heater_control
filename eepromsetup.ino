/*
 * eepromsetup.ino
 * All configuration variables can be stored to eeprom.
 * Eeprom is nice place for them, because each Arduino
 * processortype has an eeprom area.
 * Uno : 1044 bytes
 * Mega: 4096 bytes
 */

#include <EEPROM.h>
#include "eepromsetup.h"

#define VERNUM 0x100
#define EEPROM_MAX 4096

struct eepromsetup eepromsetup;

void eepShow()
{
  Serial.print("EEPROM struct size is ");
  Serial.println(sizeof(eepromsetup));
  Serial.print("measTimeout is ");
  Serial.println(eepromsetup.meastimeout);
  Serial.print("m2xfeed is ");
  Serial.println(eepromsetup.m2xfeed);
  Serial.print("m2xkey is ");
  Serial.println(eepromsetup.m2xkey);

  for (int i=0;i<4;i++) {
    Serial.print(i);
    Serial.print(" ");
    Serial.print(eepromsetup.ad[i].mindigital);
    Serial.print(" ");
    Serial.print(eepromsetup.ad[i].minvalue);
    Serial.print(" ");
    Serial.print(eepromsetup.ad[i].maxdigital);
    Serial.print(" ");
    Serial.print(eepromsetup.ad[i].maxvalue);
    Serial.print(" ");
    Serial.println(eepromsetup.ad[i].diff);
  }
}

static void blockread(void *ptr,int from,int len)
{
  int i;
  unsigned char *c=(unsigned char *) ptr;

  for (i=0;i<len;i++) {
    *c=EEPROM.read(from);
    c++;
    from++;
  }
}

static void blockwrite(void *ptr,int target,int len)
{
  int i;
  uint8_t *c=(uint8_t *) ptr;

  for (i=0;i<len;i++) {
    EEPROM.write(target,*c);
    c++;
    target++;
    if (target==EEPROM_MAX)
      break;
  }
}

void eepReadAll()
{
  blockread(&eepromsetup,0,sizeof(eepromsetup));
  Serial.print("EEPROM content is ");

  if (eepromsetup.id==VERNUM) {
     Serial.println("valid");
     strcpy(feedId,eepromsetup.m2xfeed);
     strcpy(m2xKey,eepromsetup.m2xkey);
  }
  else {
    Serial.println("invalid, resetting default values");
    eepWriteAll();
  }
}

#define BOILER  0  
#define AMBIENT 1
#define HOTHOUSEWATER 2
#define RADIATOR  3

void eepWriteAll()
{
  struct ad *ad;

  ad = adinput.getFirst();
  int i=0;
  do {
    eepromsetup.ad[i].mindigital = ad->mincal.digital;
    eepromsetup.ad[i].minvalue   = ad->mincal.analog;
    eepromsetup.ad[i].maxdigital = ad->maxcal.digital;
    eepromsetup.ad[i].maxvalue   = ad->maxcal.analog;
    eepromsetup.ad[i].diff       = ad->diff.analog;
    i++;
  } while ((ad = adinput.getNext(ad)) != NULL);  
  eepromsetup.id=VERNUM;
  eepromsetup.meastimeout=measTimeout;
  // feedId and key need not to be copied, they are stored directly
  // to eepromsetup
  blockwrite(&eepromsetup,0,sizeof(eepromsetup));
}
		



