#include <SPI.h>
#include <Ethernet.h>
#include <Time.h>
#include <Timer.h>
#include <Wire.h>
#include <EthernetUdp.h>
#include <stdio.h>
#include "heater_control.h"
#include "LedControl.h"
#include <PubNub.h>
#include "spi7seg.h"
#include "Iot.h"
#include "bypassvalve.h"
#include "heatstore.h"
#include "conversion.h"
#include "adlimit.h"
#include "AdInput.h"
#include "lists.h"


#define OUTIO_ACTIVE 0x01


byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };


// interrupt counter is not bound very elegantly to
// cntArr, I think about this later.
int volatile *_counters;


IPAddress ip(192,168,1,177);
IPAddress gateway(192,168,1, 1);
IPAddress subnet(255, 255, 0, 0);

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


Timer sched;
adlimit adl1(38.0);
bypassValve bp1;
heatStore hs1(BOILER_LED,62,&adl1,67,4.0,30);

// counters are kept in array, this for preparing to have more of them.

struct cnt cntArr[] = {                //prev prevsnd    diff   factor  scale  measured
  { 1,"electricity", 0,0,0,0,irqh0,     0,0.0, 0,0.0,   0,0.1, 0,0.55, 50,1.0,  0,0.0},
  {-1,"water",       0,0,0,0,irqh1,     0,0.0, 0,0.0,   0,0.1, 0,0.55, 50,1.0,  0,0.0}
};

conversion radiatorConverter(18);

struct condition conditions[]= {
  {0,NULL,&hs1, METHOD_HEATSTORE},               
  {1,NULL,&hs1, METHOD_HEATSTORE_CHANGELIMIT},   
  {2,NULL,&bp1, METHOD_ANALOG},                  
  {3,NULL,&bp1, METHOD_ANALOG_RECHECK}         
};


int measTimeout=1800;

EthernetServer ipserver = EthernetServer(9000);
EthernetClient ipclient;
EthernetUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
Iot iot;
AdInput adinput(measTimeout);
int adReadCnt;
LedControl lc=LedControl(37,33,35,1);
spi7seg s7s = spi7seg(&lc);

void setup() {
  int succ;

  Serial.begin(9600);
  pinMode(SEND_LED, OUTPUT);
  pinMode(CHK_LED, OUTPUT);
  pinMode(BOILER_LED, OUTPUT);
  pinMode(UP_LED, OUTPUT);
  pinMode(DN_LED, OUTPUT);
  pinMode(EL_LED, OUTPUT);
  Serial.println("Start");
  led_check();
  lc.setIntensity(0,1);
  s7s.number(0,0,8888);
  s7s.number(0,1,8888);
  eepReadAll();
  eepShow();
  iot.start();
  adinput.add(10,"boiler",       0.7, 94,  0.0,  1023, 80.0);
  adinput.add(11,"ambient",      0.3, 574, 0.0,  1020, 33.0, 180);
  adinput.add(12,"hothousewater",0.7, 109, 20.7, 1011, 52.9);
  adinput.add(13,"radiator",     0.2, 247, 21.0, 800,  36.5);

  conditions[0].source = adinput.getNamed("boiler");
  conditions[1].source = adinput.getNamed("hothousewater");
  conditions[2].source = adinput.getNamed("ambient");
  conditions[3].source = adinput.getNamed("radiator");

  bp1 = bypassValve(&sched,"heating",adinput.getNamed("radiator"),UP_LED,DN_LED,1000,90,0.01,1000,10000);

  radiatorConverter.add(-30,32);
  radiatorConverter.add(-20,30);
  radiatorConverter.add(-10,26);
  radiatorConverter.add(  0,20);
  radiatorConverter.add( 10,11);
  radiatorConverter.add( 20, 4);
  radiatorConverter.add( 30, 0);
  bp1.setConverter(&radiatorConverter);
  hs1.setIot(&iot);
  // preparing for configurable channels.
  // in future the amount of channels will be specified with config
  // and this amount with channel names are saved to eeprom.
  if (!iot.setBufferSizes(5)) {
    Serial.println("Iot init failed\n");
    return;
  }
  digitalWrite(SEND_LED,1);
  succ=Ethernet.begin(mac);
  digitalWrite(SEND_LED,0);
  if (!succ) {
    Serial.println("Failed to configure Ethernet using DHCP");
    digitalWrite(CHK_LED,1);
  }
  else {
    Serial.print("ip address is ");
    s7s.number(0,1,Ethernet.localIP()[3]);
    s7s.dot(true);
    for (byte thisByte = 0; thisByte < 4; thisByte++) {
      // print the value of each byte of the IP address:
      Serial.print(Ethernet.localIP()[thisByte], DEC);
      Serial.print(".");
    }
    Serial.println();
  }
  adinput.reset();
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

void led_check()
{
    for (int i=44;i<50;i++) {
      digitalWrite(i,1); 
      delay(40);
      digitalWrite(i,0); 
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
  static struct ad *ad=adinput.getFirst();

  s7s.number(0,0,ad->measured.analog);
  ad = adinput.getNext(ad);  
  if (ad->n.ln_Succ == NULL)
    ad = adinput.getFirst();
}

void dispRow1()
{
  static int i;

  switch (i) {
  case 0:
    s7s.date(0,1);
    break;

  case 1:
  case 2:
    s7s.time(0,1);
    break;

  case 3:
    s7s.number(0,1,cntArr[0].measured.analog);
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

  if (adinput.read()) {
    cntTimeouts=chkCntTimeouts(ts,false);
    cntChanges=chkCntChanged();
    timeouts=adinput.isTimeout(ts, (cntTimeouts || cntChanges));
    changes=adinput.verify();
    // if there is something to send, advance other channel sends,
    // in case they are enough near with timeout. This way we get
    // more values to send in same m2x packet.
    if (timeouts || changes) {
      if (!cntTimeouts)
	chkCntTimeouts(ts,true);
      adinput.isTimeout(ts,true);
    }
    adinput.calc();
    calcCnt();
    sendM2X();
    adinput.reset();
  }
}

void evaluateConditions(void)
{
  time_t ts=now();
  struct ad *ad;

  for (ad=adinput.getFirst();ad->n.ln_Succ!=NULL;ad=adinput.getNext(ad)) {
    evaluateCondition(ad,ts);
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
	if (!bpv->turnBypass(ts,&iot))
	  conditions[i].source->flags &= ~FLAGS_EVALUATE;
	break;

      case METHOD_ANALOG_RECHECK:
	bpv=(class bypassValve *) conditions[i].target;
	conditions[i].source->flags &= ~FLAGS_EVALUATE;
	bpv->turnBypass(ts,&iot);
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

void sendM2X(void)
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
  if (ts < 1262296800)
    return;
  buildTime(ts,chbuf);
  iot.reset();

  adinput.buildIot(chbuf,&iot);
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
    iot.showCounters();
    iot.showStreamnames();
    iot.showTimes();
    iot.showValues();

    response = iot.send();
    Serial.print("M2x client response code: ");
    Serial.println(response);
  }
  else
    Serial.print(".");
}

