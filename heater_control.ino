#include <SPI.h>
#include <Ethernet.h>
#include <Time.h>
#include <Timer.h>
#include <Wire.h>
#include <EthernetUdp.h>
#include <DS1307RTC.h>
#include <stdio.h>
#include <jsonlite.h>
#include "M2XStreamClient.h"
#include "Iot.h"

#define AD_SAMPLECNT 10
#define FLAGS_DATACHANGE 0x01
#define FLAGS_TIMEOUT 0x02

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

struct variable {
  char *name;
  float value;
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


struct outIo {
  int port;
  char *name;
  int state;
  time_t last_change;
};

#define METHOD_TURNON      1
#define METHOD_CHANGELIMIT 2
#define METHOD_TABLE       3

// simple conditions for operations
struct condition {
  int id;
  struct ad *source;
  struct variable *limit;
  struct variable *limit2;
  void *target;
  int method;
};


struct variable variables[]= {
  {"boilTemp",      24.5},
  {"boilHysteresis", 1.5},
  {"boilReduced",   24.0},
  {"hhboil",        25.0},
  {"radiatorTemp",  22.0},
  {"radiatorAdd",   20.0}
};

struct variable *origVariables;

// in future this table shall be stored mainly in eeprom.
// and so is it's size
struct ad adArr[]= {          //prev   dif    min       max         meas    delta
  {2,"boiler",       0,0,     0,0.0, 0,0.5, 174,21.0, 935, 100.0,  0,0.0,  0,0.0},
  {3,"ambient",      0,0,     0,0.0, 0,0.3, 372,2.2,  964,  36.5,  0,0.0,  0,0.0},
  {4,"hothousewater",0,0,     0,0.0, 0,0.5, 7,  21.0, 1023,100.0,  0,0.0,  0,0.0},
  {5,"radiator",     0,0,     0,0.0, 0,0.3, 7,  21.0, 539,  36.5,  0,0.0,  0,0.0}
};


// counters are kept in array, this for preparing to have more of them.

struct cnt cntArr[] = {                //prev prevsnd    diff   factor  scale  measured
  { 1,"electricity", 0,0,0,0,irqh0,     0,0.0, 0,0.0,   0,0.1, 0,0.55, 50,1.0,  0,0.0},
  {-1,"water",       0,0,0,0,irqh1,     0,0.0, 0,0.0,   0,0.1, 0,0.55, 50,1.0,  0,0.0}
};

// outIoArr has io ports for relay control.
struct outIo outIoArr[] = {
  {12,"burnerfeed",0,0}
};

struct convert {
  int source;
  int target;
};

struct convert radiatorTab[]={ -30,22, // ambient -30 -> radiator 42 (remember to add "radiatorAdd")
			       -20,19, 
			       -10,16, 
			         0,13, 
			        10,10,
			        20, 2,
			       0xff,0xff}; // terminator

/*
 * METHOD_TURNON
 * limit  - turn target on, if source drops below this limit
 * limit2 - hysteresis, turn target off if source exceeds limit+hysteresis.
 * target - (struct outIo), io port which should be turned on.
 *
 * METHOD_CHANGELIMIT
 * limit  - if source drop below this, then modify limit2 variable
 * limit2 - variable to be changed according to limit.
 * target - what is the new value of limit2 if activated
 *          note that this does not have any hysteresis. It is assumed the
 *          logic, following limit2 variable has a hysteresis.
 *          When this rule does not happen, the limit2 variable is returned
 *          back to its original value (copied from origVariables).
 *
 * METHOD_TABLE
 * limit  - variable to be modified according to table. Variables original value has no purpose.
 * limit2 - value to be added after table calculations.
 * target - table of values. table can be coarse, more specific results are interpolated.
 *
 * configurations are kept in array to have a possibility to configure them with tcp/ip telnet style commands
 * and to store them to eeprom.
 */
struct condition conditions[]= {
  {0,&adArr[0],&variables[0],&variables[1],&outIoArr[0], METHOD_TURNON},       // start boiler if it is under 24.5, and off when it has reached 26.0
  {1,&adArr[2],&variables[3],&variables[0],&variables[2],METHOD_CHANGELIMIT},  // if hothousewater is under hhboil, then boilTemp should be boilReduced.
  {2,&adArr[1],&variables[4],&variables[5],radiatorTab,  METHOD_TABLE}         // changes in ambient will change radiator with the help of radiatorAdd variable and radiatorTab table.
};



Timer sched;
int measTimeout=1800;

EthernetClient client;
M2XStreamClient m2xClient(&client, m2xKey);
EthernetServer ipserver = EthernetServer(9000);
EthernetClient ipclient;
EthernetUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
Iot iot(&m2xClient);

void setup() {
  int succ;

  Serial.begin(9600);
  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);
  Serial.println("Start");
  eepReadAll();
  origVariables=(struct variable *) malloc(sizeof(variables));
  memcpy(origVariables,variables,sizeof(variables));
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
    for (byte thisByte = 0; thisByte < 4; thisByte++) {
      // print the value of each byte of the IP address:
      Serial.print(Ethernet.localIP()[thisByte], DEC);
      Serial.print(".");
    }
    Serial.println();
  }
  ipserver.begin();
  Udp.begin(localPort);
  setSyncInterval(7200);
  setSyncProvider(getNtpTime);
  sched.every(1000,processSensors);
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

void irqh0()
{
  _counters[0]++;
}

void irqh1()
{
  _counters[1]++;
}


int resetVariable(char *name)
{
  for (int i=0;i<sizeof(variables) / sizeof(struct variable);i++) {
    if (!strcmp(variables[i].name,name)) {
      variables[i].value=origVariables[i].value;
      return 0;
    }
  }
  return 1;
}

void processSensors()
{
  static int cnt=0;
  boolean timeouts=false;
  boolean cntTimeouts,cntChanges;
  boolean changes=false;
  time_t ts=now();
  int chcnt=(sizeof(adArr) / sizeof(struct ad));

  if (!cnt)
    resetAdChannels(adArr,chcnt);
  readAdChannels(adArr,chcnt);
  cnt++;
  if ((cnt % AD_SAMPLECNT)==0) {
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
  float limit;
  float off;
  struct outIo *targetIo;
  struct variable *var;
  struct convert *iTable;

  for (int i=0;i<sizeof(conditions) / sizeof(struct condition);i++) {
    if (conditions[i].source == source) {
      value=source->measured.analog;
      limit=conditions[i].limit->value;
      switch (conditions[i].method) {
      case METHOD_TURNON:
	targetIo=(struct outIo *) conditions[i].target;
	off=limit + conditions[i].limit2->value;
	if (value < limit && targetIo->state == 0) {
	  targetIo->state = 1;
	  targetIo->last_change = ts;
	  digitalWrite(targetIo->port,1);
	}
	else if (value > off && targetIo->state == 1 ) {
	  targetIo->state = 0;
	  targetIo->last_change = ts;
	  digitalWrite(targetIo->port,0);
	}
	break;

      case METHOD_CHANGELIMIT:
	var=(struct variable *) conditions[i].target;
	if (value < limit) 
	  conditions[i].limit2->value=var->value;
	else 
	  resetVariable(conditions[i].limit2->name);
	break;

      case METHOD_TABLE:
	iTable=(struct convert *) conditions[i].target;
	float adder=conditions[i].limit2->value;
        float converted;
	converted=resolveConversion(value,adder,iTable);
	break;
      }
    }
  }
}


/* find boundings from conversion table
 * and convert / interpolate according to the table
 */
float resolveConversion(float v,float adder,struct convert *ct)
{
  struct convert *start;
  struct convert *stop;

  for (int i=0; ct[i].source != 0xff; i++) {
    if (ct[i].source < v)
      start=&ct[i];
    else {
      stop=&ct[i];
      break;
    }
  }
  Serial.print("conversion for value ");
  Serial.print(v);
  Serial.print(" is ");
  Serial.print(adder+start->target);
  Serial.print(" - ");
  Serial.println(adder+stop->target);
  return 0.0;
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

  for (int analogChannel = 0; analogChannel < cnt; analogChannel++) {
    digital= analogRead(ad[analogChannel].port);
    ad[analogChannel].measured.digital += digital;
  }
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
      ret=true;
    }
  }
  return ret;
}


boolean chkAdChange(struct ad *ad, int cnt)
{
  boolean ret=false;
  int digital=0;

  for (int analogChannel = 0; analogChannel < cnt; analogChannel++) {
    digital = ad[analogChannel].measured.digital;
    if ((abs(digital - ad[analogChannel].prev.digital)) > ad[analogChannel].diff.digital) {
      ad[analogChannel].flags |= FLAGS_DATACHANGE;
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

  for (int analogChannel = 0; analogChannel < cnt; analogChannel++) {
    if (ad[analogChannel].flags & FLAGS_DATACHANGE) {
      digital = ad[analogChannel].measured.digital / AD_SAMPLECNT;
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

void resetAdChannels(struct ad *ad, int cnt)
{
  for (int analogChannel = 0; analogChannel < cnt; analogChannel++) {
    ad[analogChannel].flags &= ~FLAGS_DATACHANGE;
    ad[analogChannel].measured.digital = 0;
  }
}
