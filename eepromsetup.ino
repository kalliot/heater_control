#include <EEPROM.h>

#define VERNUM 0x100

struct adsetup {
  int mindigital;
  float minvalue;
  int maxdigital;
  float maxvalue;
};
	
struct {
  int id;
  int meastimeout;
  struct adsetup ad[4];
} eepromsetup;

void eepShow()
{
  Serial.print("EEPROM struct size is ");
  Serial.println(sizeof(eepromsetup));
  Serial.print("measTimeout is ");
  Serial.println(eepromsetup.meastimeout);
  for (int i=0;i<4;i++) {
    Serial.print(i);
    Serial.print(" ");
    Serial.print(eepromsetup.ad[i].mindigital);
    Serial.print(" ");
    Serial.print(eepromsetup.ad[i].minvalue);
    Serial.print(" ");
    Serial.print(eepromsetup.ad[i].maxdigital);
    Serial.print(" ");
    Serial.println(eepromsetup.ad[i].maxvalue);
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
    if (target==2048)
      break;
  }
}

void eepReadAll()
{
  blockread(&eepromsetup,0,sizeof(eepromsetup));
  Serial.print("EEPROM content is ");

  if (eepromsetup.id==VERNUM)
    Serial.println("valid");
  else {
    Serial.println("invalid");
    eepWriteAll();
  }
}

#define BOILER  0  
#define AMBIENT 1
#define HOTHOUSEWATER 2
#define RADIATOR  3

void eepWriteAll()
{
  for (int i=0;i<4;i++) {
    eepromsetup.ad[i].mindigital = adArr[i].mincal.digital;
    eepromsetup.ad[i].minvalue   = adArr[i].mincal.analog;
    eepromsetup.ad[i].maxdigital = adArr[i].maxcal.digital;
    eepromsetup.ad[i].maxvalue   = adArr[i].maxcal.analog;
  }
  eepromsetup.id=VERNUM;
  eepromsetup.meastimeout=measTimeout;
  blockwrite(&eepromsetup,0,sizeof(eepromsetup));
}
		



