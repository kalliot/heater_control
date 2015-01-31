#include <SPI.h>
#include <Ethernet.h>
#include <Time.h>
#include <Timer.h>
#include <Wire.h>
#include <EthernetUdp.h>
#include <DS1307RTC.h>
#include <stdio.h>
#include <jsonlite.h>
#include "LedControl.h"
#include "spi7seg.h"
#include "M2XStreamClient.h"
#include "Iot.h"
#include "bypassvalve.h"
#include "heatstore.h"
#include "conversion.h"
#include "adlimit.h"
#include "lists.h"

#define AD_SAMPLECNT 10
#define FLAGS_DATACHANGE 0x01
#define FLAGS_TIMEOUT 0x02
#define FLAGS_EVALUATE 0x04

#define OUTIO_ACTIVE 0x01


byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// mx2 credentials are stored with a tcpip config connection
// and saved to eeprom. check eepromsetup.ino
char feedId[33] = ""; // Feed you want to push to
char m2xKey[33] = ""; // Your M2X access key

// interrupt counter is not bound very elegantly to
// cntArr, I think about this later.
int volatile *_counters;


IPAddress ip(192,168,1,177);
IPAddress gateway(192,168,1, 1);
IPAddress subnet(255, 255, 0, 0);


struct dig2a
{
  int digital;
  float analog;
};


struct ad {
  int port;
  char *name;
  int flags;
  time_t last_send;
  struct dig2a prev;
  struct dig2a diff;
  struct dig2a mincal;
  struct dig2a maxcal;
  struct dig2a measured;
  struct dig2a delta;
};

struct cnt {
  unsigned int irqno;
  char *name;
  int flags;
  time_t last_send;
  long avg_counter;
  int avg_samples;
  void (* irqh)(void);
  struct dig2a prev;
  struct dig2a prevsnd;
  struct dig2a diff;
  struct dig2a diff_factor;
  struct dig2a scale;
  struct dig2a measured;
};

#define METHOD_HEATSTORE                 1
#define METHOD_HEATSTORE_CHANGELIMIT     2
#define METHOD_ANALOG                    3
#define METHOD_ANALOG_RECHECK            4

// simple conditions for operations
struct condition {
  int id;
  struct ad *source;
  void *target;
  int method;
};

// in future this table shall be stored mainly in eeprom.
// and so is it's size
struct ad adArr[]= {          //prev   dif    min       max         meas    delta
  {2,"boiler",       0,0,     0,0.0, 0,0.5, 174,21.0, 935, 100.0,  0,-273.0,  0,0.0},
  {3,"ambient",      0,0,     0,0.0, 0,0.3, 372,2.2,  964,  36.5,  0,-273.0,  0,0.0},
  {4,"hothousewater",0,0,     0,0.0, 0,0.5, 7,  21.0, 1023,100.0,  0,-273.0,  0,0.0},
  {5,"radiator",     0,0,     0,0.0, 0,0.3, 7,  21.0, 539,  36.5,  0,-273.0,  0,0.0}
};


Timer sched;
adlimit adl1(25.0);
bypassValve bp1(&sched,"heating",&adArr[3].measured.analog,30,31,1000,30,0.3,800,10000);
heatStore hs1(12,25,&adl1,24,1.5,30);

// counters are kept in array, this for preparing to have more of them.

struct cnt cntArr[] = {                //prev prevsnd    diff   factor  scale  measured
  { 1,"electricity", 0,0,0,0,irqh0,     0,0.0, 0,0.0,   0,0.1, 0,0.55, 50,1.0,  0,0.0},
  {-1,"water",       0,0,0,0,irqh1,     0,0.0, 0,0.0,   0,0.1, 0,0.55, 50,1.0,  0,0.0}
};

conversion radiatorConverter(16);

struct condition conditions[]= {
  {0,&adArr[0],&hs1, METHOD_HEATSTORE},               
  {1,&adArr[2],&hs1, METHOD_HEATSTORE_CHANGELIMIT},   
  {2,&adArr[1],&bp1, METHOD_ANALOG},                  
  {3,&adArr[3],&bp1, METHOD_ANALOG_RECHECK}         
};


int measTimeout=1800;

EthernetClient client;
M2XStreamClient m2xClient(&client, m2xKey,idler);
EthernetServer ipserver = EthernetServer(9000);
EthernetClient ipclient;
EthernetUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
Iot iot(&m2xClient);
int adReadCnt;
LedControl lc=LedControl(37,33,35,1);
spi7seg s7s = spi7seg(&lc);

void setup() {
  int succ;

  Serial.begin(9600);
  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(30, OUTPUT);
  pinMode(31, OUTPUT);
  Serial.println("Start");
  lc.setIntensity(0,1);
  s7s.number(0,8888);
  s7s.number(1,8888);
  eepReadAll();
  radiatorConverter.add(-30,33);
  radiatorConverter.add(-20,31);
  radiatorConverter.add(-10,28);
  radiatorConverter.add(  0,21);
  radiatorConverter.add( 10,11);
  radiatorConverter.add( 20, 2);
  bp1.setConverter(&radiatorConverter);
  // preparing for configurable channels.
  // in future the amount of channels will be specified with config
  // and this amount with channel names are saved to eeprom.
  if (!iot.setBufferSizes(5)) {
    Serial.println("Iot init failed\n");
    return;
  }
  eepShow();
  initAdChannels(adArr,sizeof(adArr) / sizeof(struct ad));
  setTime(RTC.get());
  succ=Ethernet.begin(mac);
  if (!succ) {
    Serial.println("Failed to configure Ethernet using DHCP");
    Ethernet.begin(mac, ip, gateway, subnet);
  }
  else {
    Serial.print("ip address is ");
    s7s.number(1,Ethernet.localIP()[3]);
    for (byte thisByte = 0; thisByte < 4; thisByte++) {
      // print the value of each byte of the IP address:
      Serial.print(Ethernet.localIP()[thisByte], DEC);
      Serial.print(".");
    }
    Serial.println();
  }


  resetAdChannels(adArr,(sizeof(adArr) / sizeof(struct ad)));
  ipserver.begin();
  Udp.begin(localPort);
  setSyncInterval(7200);
  setSyncProvider(getNtpTime);
  sched.every(1000,processSensors);
  sched.every(4000,disp7seg);
  sched.every(10000,evaluateConditions);
  sched.every(200,checkEthernet);
  _counters=(int *) malloc(sizeof(int) * sizeof(cntArr) / sizeof(struct cnt));
  for (int i=0;i<(sizeof(cntArr) / sizeof(struct cnt));i++) {
    if (cntArr[i].irqno!=-1) {
      cntArr[i].last_send=now();
      attachInterrupt(cntArr[i].irqno,cntArr[i].irqh, CHANGE);
    }
  }
}

void loop()
{
  sched.update();
  refreshEvents();
}

void idler()
{
  sched.update();
}

void irqh0()
{
  _counters[0]++;
}

void irqh1()
{
  _counters[1]++;
}

void disp7seg()
{
  dispRow0();
  dispRow1();
}

void dispRow0()
{
  static int i;
  int value;

  value = (int)(adArr[i].measured.analog * 100);
  s7s.number(0,value);
  i++;
  if (i==4)
    i=0;
}

void dispRow1()
{
  static int i;

  switch (i) {
  case 0:
    s7s.date(1);
    break;

  case 1:
  case 2:
    s7s.time(1);
    break;

  case 3:
    int value=(int)(cntArr[0].measured.analog * 100);
    s7s.number(1,value);
    break;
  }
  if (++i==4)
    i=0;
}

void processSensors()
{
  boolean timeouts=false;
  boolean cntTimeouts,cntChanges;
  boolean changes=false;
  time_t ts=now();
  int chcnt=(sizeof(adArr) / sizeof(struct ad));

  if (readAdChannels(adArr,chcnt)) {
    sched.oscillate(13,150, LOW,2);
    cntTimeouts=chkCntTimeouts(ts,false);
    cntChanges=chkCntChanged();
    timeouts=chkAdTimeouts(adArr,chcnt, ts, (cntTimeouts || cntChanges));
    changes=chkAdChange(adArr,chcnt);
    // if there is something to send, advance other channel sends,
    // in case they are enough near with timeout. This way we get
    // more values to send in same m2x packet.
    if (timeouts || changes) {
      if (!cntTimeouts)
	chkCntTimeouts(ts,true);
      chkAdTimeouts(adArr,chcnt, ts, true);
    }
    calcAdChannels(adArr,chcnt);
    calcCnt();
    sendM2X(adArr,chcnt);
    resetAdChannels(adArr,chcnt);
  }
}

void evaluateCondition(struct ad *source,time_t ts)
{
  float value;
  heatStore *hs;
  bypassValve *bpv;

  for (int i=0;i<sizeof(conditions) / sizeof(struct condition);i++) {
    if (conditions[i].source == source) {
      value=source->measured.analog;
      switch (conditions[i].method) {
      case METHOD_HEATSTORE:
	hs=(heatStore *) conditions[i].target;
	hs->refresh(ts,value);
	break;

      case METHOD_HEATSTORE_CHANGELIMIT:
	hs=(heatStore *) conditions[i].target;
	hs->chkTarget(ts,value);
	break;

      case METHOD_ANALOG:
	bpv=(class bypassValve *) conditions[i].target;
	bpv->setGuide(value);
	if (!bpv->turnBypass(ts))
	  conditions[i].source->flags &= ~FLAGS_EVALUATE;
	break;

      case METHOD_ANALOG_RECHECK:
	bpv=(class bypassValve *) conditions[i].target;
	conditions[i].source->flags &= ~FLAGS_EVALUATE;
	bpv->turnBypass(ts);
	break;
      }
    }
  }
}


void calcCnt()
{
  unsigned int tmpCnt;

  for (int i=0;i<(sizeof(cntArr) / sizeof(struct cnt));i++) {
    if (cntArr[i].irqno!=-1) {
      noInterrupts();
      tmpCnt=_counters[i];
      interrupts();
      cntArr[i].prev.analog=cntArr[i].measured.analog;
      cntArr[i].measured.analog = (tmpCnt-cntArr[i].prev.digital) / (cntArr[i].scale.digital * cntArr[i].scale.analog); 
      if (cntArr[i].measured.analog > 20.0) {  // debug code. Sometimes during development a very big kw is sent to iot host.
	cntArr[i].measured.analog = 20.0;
      }
      cntArr[i].avg_counter+=tmpCnt-cntArr[i].prev.digital;
      cntArr[i].avg_samples++;
      cntArr[i].prev.digital=tmpCnt;
    }
  }
}

void checkEthernet()
{
  char ch;
  char buff[80];
  int i=0;

  buff[0]=0;
  for (;;) {
    ipclient=ipserver.available();
    if (ipclient) {
      ch=ipclient.read();
      if (ch=='\r')
        continue;
      if (ch=='\n') {
        buff[i]=0;
        Serial.println(buff);
        cmdParse(ipclient,buff);
        break;
      }
      else
	buff[i++]=ch;
    }
    else
      break;
  }
}

// convert to string and add preceeding zero, if needed
String len2(int val)
{
  if (val>9)
    return String(val);
  else
    return "0"+String(val);
}

// m2x needs a special timestamp
char *buildTime(time_t ts,char *buff)
{
  String timebuf;

  timebuf=String(year(ts))+"-"+
    len2(month(ts))+"-"+
    len2(day(ts))+"T"+
    len2(hour(ts))+":"+
    len2(minute(ts))+":"+
    len2(second(ts))+"Z";

  timebuf.toCharArray(buff,21);
  return buff;
}

void cntPrepareForNext(int index,time_t ts)
{
  cntArr[index].last_send=ts;
  cntArr[index].prevsnd.analog=cntArr[index].measured.analog;
  cntArr[index].avg_samples=0;
  cntArr[index].avg_counter=0;
  cntArr[index].flags &= ~FLAGS_DATACHANGE;
  cntArr[index].flags &= ~FLAGS_TIMEOUT;
}

boolean chkCntTimeouts(time_t ts,boolean advance)
{
  boolean ret=false;
  int timeout;

  timeout= measTimeout;
  if (advance)
    timeout-=300;

  for (int i=0;i<(sizeof(cntArr) / sizeof(struct cnt)); i++) {
    if (cntArr[i].irqno!=-1) {
      if (ts-cntArr[i].last_send > timeout) {
	cntArr[i].flags |= FLAGS_TIMEOUT;
	ret=true;
      }
    }
  }
  return ret;
}

boolean chkCntChanged()
{
  float limit;
  bool changed;
  float diff;
  boolean ret=false;

  for (int i=0;i<(sizeof(cntArr) / sizeof(struct cnt));i++) {
    if (cntArr[i].irqno!=-1) {
      diff = abs(cntArr[i].measured.analog-cntArr[i].prevsnd.analog);
      limit = cntArr[i].diff.analog * (1.0 + cntArr[i].measured.analog * cntArr[i].diff_factor.analog);
      if (diff > limit) {
	cntArr[i].flags |= FLAGS_DATACHANGE;
	ret=true;
      }
    }
  }
  return ret;
}

float cntAvg(int index)
{
  return (cntArr[index].avg_counter / cntArr[index].avg_samples) / 
         (cntArr[index].scale.digital * cntArr[index].scale.analog);
}

void sendM2X(struct ad *ad,int cnt)
{
  char chbuf[22];
  char prevtime[22];
  int response;
  time_t ts;
  float currval;
  boolean added=false;

  if (timeStatus() == timeNotSet)
    return;
  ts=now();
  buildTime(ts,chbuf);
  iot.reset();

  // ad build part
  for (int chan = 0; chan < cnt; chan++) {    
    if (ad[chan].flags & FLAGS_DATACHANGE) {
      iot.name(ad[chan].name);
      iot.addValue(ad[chan].measured.analog,chbuf);
      iot.next();
    }
  }

  // counter build part
  for (int i=0;i<(sizeof(cntArr) / sizeof(struct cnt));i++) {
    if (cntArr[i].irqno!=-1) {
      if (cntArr[i].flags & FLAGS_DATACHANGE) {
	if (ts-cntArr[i].last_send > 30) {                   // value changed and there is more than 30 seconds after prev,
	  iot.addValue(cntAvg(i),buildTime(ts-10,prevtime)); // so this time we'll send two samples.
	}
	currval=cntArr[i].measured.analog;
	added=true;
      }
      else if (cntArr[i].flags & FLAGS_TIMEOUT) { // no big changes, only timeout
	currval=cntAvg(i);
	added=true;
      }
      if (added) {
	iot.name(cntArr[i].name);
	iot.addValue(currval,chbuf);
	iot.next();
	cntPrepareForNext(i,ts);
      }
    }
  }


  // send part
  if (iot.getRecCnt()) {
    if (feedId[0]==0) {
      Serial.println("feed id not known");
      return;
    }
    digitalWrite(13,1); // turn led on during iot send
    
    iot.showCounters();
    iot.showStreamnames();
    iot.showTimes();
    iot.showValues();

    response = iot.send(feedId);
    digitalWrite(13,0); // led off
    Serial.print("M2x client response code: ");
    Serial.println(response);
  }
  else
    Serial.print(".");
}

int initAdChannels(struct ad *ad, int cnt)
{
  for (int analogChannel = 0; analogChannel < cnt; analogChannel++) {
    ad[analogChannel].delta.digital=ad[analogChannel].maxcal.digital - ad[analogChannel].mincal.digital;
    ad[analogChannel].delta.analog=ad[analogChannel].maxcal.analog - ad[analogChannel].mincal.analog;
    ad[analogChannel].diff.digital=AD_SAMPLECNT * ad[analogChannel].delta.digital / ad[analogChannel].delta.analog * ad[analogChannel].diff.analog;
  }
}


// read just cumulates the digital value
// average and analog value is calculated elsewhere
int readAdChannels(struct ad *ad, int cnt)
{
  int digital=0;
  int ddelta;
  float adelta;

  adReadCnt++;
  for (int analogChannel = 0; analogChannel < cnt; analogChannel++) {
    digital= analogRead(ad[analogChannel].port);
    ad[analogChannel].measured.digital += digital;
  }
  if (adReadCnt==AD_SAMPLECNT)
    return 1;
  return 0;
}

boolean chkAdTimeouts(struct ad *ad, int cnt, time_t ts, boolean advance)
{
  time_t timeout=measTimeout;
  boolean ret=false;

  if (advance)
    timeout -= 300;
  for (int analogChannel = 0; analogChannel < cnt; analogChannel++) {
    if (ts-ad[analogChannel].last_send > timeout) {
      ad[analogChannel].flags |= FLAGS_DATACHANGE;
      ad[analogChannel].flags |= FLAGS_EVALUATE;
      ret=true;
    }
  }
  return ret;
}


boolean chkAdChange(struct ad *ad, int cnt)
{
  boolean ret=false;
  int digital=0;
  int seg7;

  for (int analogChannel = 0; analogChannel < cnt; analogChannel++) {
    digital = ad[analogChannel].measured.digital;
    if ((abs(digital - ad[analogChannel].prev.digital)) > ad[analogChannel].diff.digital) {
      ad[analogChannel].flags |= FLAGS_DATACHANGE;
      ad[analogChannel].flags |= FLAGS_EVALUATE;
      ret=true;
    }
  }
  return ret;
}



int calcAdChannels(struct ad *ad, int cnt)
{
  int digital=0;
  int ddelta;
  float adelta;
  time_t ts=now();
  int seg7;

  for (int analogChannel = 0; analogChannel < cnt; analogChannel++) {
    if (ad[analogChannel].flags & FLAGS_DATACHANGE) {
      digital = ad[analogChannel].measured.digital / adReadCnt;
      // inter- and extrapolation
      ddelta=ad[analogChannel].delta.digital;
      adelta=ad[analogChannel].delta.analog;

      ad[analogChannel].measured.analog  = ad[analogChannel].mincal.analog +
	(digital - ad[analogChannel].mincal.digital) * adelta / ddelta;
      ad[analogChannel].last_send=ts;
      ad[analogChannel].prev.analog = ad[analogChannel].measured.analog;
      ad[analogChannel].prev.digital = ad[analogChannel].measured.digital;
      evaluateCondition(&ad[analogChannel],ts);
    }
  }
  return 0;
}

void evaluateConditions() {
  int cnt=sizeof(adArr) / sizeof(struct ad);
  time_t ts=now();

  for (int i = 0; i < cnt; i++) {
    if (adArr[i].flags & FLAGS_EVALUATE)
      evaluateCondition(&adArr[i],ts);
  }
}

void resetAdChannels(struct ad *ad, int cnt)
{
  for (int analogChannel = 0; analogChannel < cnt; analogChannel++) {
    ad[analogChannel].flags &= ~FLAGS_DATACHANGE;
    ad[analogChannel].measured.digital = 0;
  }
  adReadCnt=0;
}
