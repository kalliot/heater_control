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
  int last_send;
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
  sched.every(1000,processAD);
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

void processAD()
{
  String message;
  static int cnt=0;

  if (!cnt)
    resetAdChannels(adArr,sizeof(adArr) / sizeof(struct ad));
  readAdChannels(adArr,sizeof(adArr) / sizeof(struct ad));
  cnt++;
  if ((cnt % AD_SAMPLECNT)==0) {
    sched.oscillate(13,150, LOW,2);
    calcAdChannels(adArr,sizeof(adArr) / sizeof(struct ad));
    calcKw();
    sendM2X(adArr,sizeof(adArr) / sizeof(struct ad));
    resetAdChannels(adArr,sizeof(adArr) / sizeof(struct ad));
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
}

void sendM2X(struct ad *ad,int cnt)
{
  char chbuf[22];
  char prevtime[22];
  int response;
  boolean timeout=false;
  time_t ts;
  boolean sendingad=false;

  ts=now();
  buildTime(ts,chbuf);
  iot.reset();
  // ad build part
  for (int chan = 0; chan < cnt; chan++) {    
    if (ad[chan].flags & FLAGS_DATACHANGE) {
      iot.name(ad[chan].name);
      iot.addValue(ad[chan].measured.analog,chbuf);
      iot.next();
      sendingad=true;
    }
  }

  // counter build part
  if (sendingad) // if already sending ad values, add counters a bit earlier to same packet
    timeout=(ts-cntArr[0].last_send > measTimeout-300);
  else
    timeout=(ts-cntArr[0].last_send > measTimeout);
  float diff=abs(cntArr[0].measured.analog-cntArr[0].prevsnd.analog);
  bool changed;
  float limit;
  float val;

  limit = cntArr[0].diff.analog * (1.0 + cntArr[0].measured.analog * cntArr[0].diff_factor.analog);
  changed=(diff > limit);
  if (changed) {
    if (ts-cntArr[0].last_send > 30) {
      if (cntArr[0].avg_samples > 3) {
        // calculate average from previous samples
	val=(cntArr[0].avg_counter / cntArr[0].avg_samples) / (cntArr[0].scale.digital * cntArr[0].scale.analog); 
      }
      else
	val=cntArr[0].prev.analog;

      buildTime(ts-10,prevtime);
      iot.name(cntArr[0].name);
      iot.addValue(val,prevtime);
      iot.addValue(cntArr[0].measured.analog,chbuf);
      iot.next();
      cntPrepareForNext(0,ts);
     }
    else {
      iot.name(cntArr[0].name);
      iot.addValue(cntArr[0].measured.analog,chbuf);
      iot.next();
      cntPrepareForNext(0,ts);
    }
  }
  else if (timeout) {
    val=(cntArr[0].avg_counter / cntArr[0].avg_samples) / (cntArr[0].scale.digital * cntArr[0].scale.analog); 
    iot.name(cntArr[0].name);
    iot.addValue(val,chbuf);
    iot.next();
    cntPrepareForNext(0,ts);
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

int calcAdChannels(struct ad *ad, int cnt)
{
  int digital=0;
  int ddelta;
  float adelta;
  int ts=now();
  bool timeout=false;

  for (int analogChannel = 0; analogChannel < cnt; analogChannel++) {
    digital = ad[analogChannel].measured.digital / AD_SAMPLECNT;
    // inter- and extrapolation
    ddelta=ad[analogChannel].delta.digital ;
    adelta=ad[analogChannel].delta.analog;

    ad[analogChannel].measured.analog  = ad[analogChannel].mincal.analog +
      (digital - ad[analogChannel].mincal.digital) * adelta / ddelta;

    ad[analogChannel].flags &= ~FLAGS_DATACHANGE;
    timeout=(ts-ad[analogChannel].last_send > measTimeout);
    if (timeout || ((abs(ad[analogChannel].measured.analog - ad[analogChannel].prev.analog)) > ad[analogChannel].diff.analog)) {
      ad[analogChannel].prev.analog = ad[analogChannel].measured.analog;
      ad[analogChannel].prev.digital = digital ;
      ad[analogChannel].flags |= FLAGS_DATACHANGE;
      ad[analogChannel].last_send=ts;
    }
  }
  return 0;
}

void resetAdChannels(struct ad *ad, int cnt)
{
  for (int analogChannel = 0; analogChannel < cnt; analogChannel++) 
    ad[analogChannel].measured.digital = 0;
}
