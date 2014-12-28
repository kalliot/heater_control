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

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// mx2 credentials are stored with a tcpip config connection
// and saved to eeprom. check eepromsetup.ino
char feedId[33] = ""; // Feed you want to push to
char m2xKey[33] = ""; // Your M2X access key

// interrupt counter is not bound very elegantly to
// cntArr, I think about this later.
volatile unsigned int _kwCounter = 0;   // counter for the number of interrupts

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
  unsigned int counter;
  char *name;
  int flags;
  time_t last_send;
  long avg_counter;
  int avg_samples;
  struct dig2a prev;
  struct dig2a prevsnd;
  struct dig2a diff;
  struct dig2a diff_factor;
  struct dig2a scale;
  struct dig2a measured;
};

// in future this table shall be stored mainly in eeprom.
// and so is it's size
struct ad adArr[]= {          //prev   dif    min       max         meas    delta
  {2,"boiler",       0,0,     0,0.0, 0,0.5, 174,21.0, 935, 100.0,  0,0.0,  0,0.0},
  {3,"ambient",      0,0,     0,0.0, 0,0.3, 372,2.2,  964,  36.5,  0,0.0,  0,0.0},
  {4,"hothousewater",0,0,     0,0.0, 0,0.5, 7,  21.0, 1023,100.0,  0,0.0,  0,0.0},
  {5,"radiator",     0,0,     0,0.0, 0,0.3, 7,  21.0, 539,  36.5,  0,0.0,  0,0.0}
};


// counters are kept in array, this for preparing to have more of them.

struct cnt cntArr[] = {         //prev prevsnd    diff   factor  scale  measured
  {0,"electricity",  0,0,0,0,     0,0.0, 0,0.0,   0,0.1, 0,0.55, 50,1.0,  0,0.0}
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
  Serial.println("Start");
  eepReadAll();
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
  cntArr[0].last_send=now();
  attachInterrupt(1, elmeter, CHANGE);
}

void loop()
{
  sched.update();
  refreshEvents();
}

void elmeter()
{
  _kwCounter++;
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
    calcKw();
    sendM2X(adArr,chcnt);
    resetAdChannels(adArr,chcnt);
  }
}


void calcKw()
{
  unsigned int tmpCnt;

  noInterrupts();
  tmpCnt=_kwCounter;
  interrupts();
  cntArr[0].prev.analog=cntArr[0].measured.analog;
  cntArr[0].measured.analog = (tmpCnt-cntArr[0].prev.digital) / (cntArr[0].scale.digital * cntArr[0].scale.analog); 
  if (cntArr[0].measured.analog > 20.0) {  // debug code. Sometimes during development a very big kw is sent to iot host.
    cntArr[0].measured.analog = 20.0;
  }
  cntArr[0].avg_counter+=tmpCnt-cntArr[0].prev.digital;
  cntArr[0].avg_samples++;
  cntArr[0].prev.digital=tmpCnt;
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

// converto to string and add preceeding zero, if needed
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
    if (ts-cntArr[i].last_send > timeout) {
      cntArr[i].flags |= FLAGS_TIMEOUT;
      ret=true;
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
    diff = abs(cntArr[i].measured.analog-cntArr[i].prevsnd.analog);
    limit = cntArr[i].diff.analog * (1.0 + cntArr[i].measured.analog * cntArr[i].diff_factor.analog);
    if (diff > limit) {
      cntArr[i].flags |= FLAGS_DATACHANGE;
      ret=true;
    }
  }
  return ret;
}

void sendM2X(struct ad *ad,int cnt)
{
  char chbuf[22];
  char prevtime[22];
  int response;
  time_t ts;
  float val;

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
    if (cntArr[i].flags & FLAGS_DATACHANGE) {
      if (ts-cntArr[i].last_send > 30) { // value changed and there is more than 30 seconds after prev
	if (cntArr[i].avg_samples > 3) {
	  // calculate average from previous samples
	  val=(cntArr[i].avg_counter / cntArr[i].avg_samples) / (cntArr[i].scale.digital * cntArr[i].scale.analog);
	}
	else
	  val=cntArr[i].prev.analog;

	buildTime(ts-10,prevtime);
	iot.name(cntArr[i].name);
	iot.addValue(val,prevtime);
	iot.addValue(cntArr[i].measured.analog,chbuf);
	iot.next();
	cntPrepareForNext(i,ts);
      }
      else { // value changed quicly after prev
	iot.name(cntArr[i].name);
	iot.addValue(cntArr[i].measured.analog,chbuf);
	iot.next();
	cntPrepareForNext(i,ts);
      }
    }
    else if (cntArr[i].flags & FLAGS_TIMEOUT) { // no big changes, only timeout
      val=(cntArr[i].avg_counter / cntArr[i].avg_samples) / (cntArr[i].scale.digital * cntArr[i].scale.analog);
      iot.name(cntArr[i].name);
      iot.addValue(val,chbuf);
      iot.next();
      cntPrepareForNext(i,ts);
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
