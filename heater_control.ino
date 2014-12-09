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

#define AD_SAMPLECNT 10

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
char feedId[] = "<feed id>"; // Feed you want to push to
char m2xKey[] = "<M2X access key>"; // Your M2X access key

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
  int changed;
  int last_send;
  struct dig2a prev;
  struct dig2a diff;
  struct dig2a mincal;
  struct dig2a maxcal;
  struct dig2a measured;
  struct dig2a delta;
};


struct ad adArr[]= {          //prev   dif    min       max         meas    delta
  {0,"boiler",       0,0,     0,0.0, 0,0.5, 174,21.0, 935, 100.0,  0,0.0,  0,0.0},
  {1,"ambient",      0,0,     0,0.0, 0,0.3, 372,2.2,  964,  36.5,  0,0.0,  0,0.0},
  {2,"hothousewater",0,0,     0,0.0, 0,0.5, 7,  21.0, 1023,100.0,  0,0.0,  0,0.0},
  {3,"radiator",     0,0,     0,0.0, 0,0.3, 7,  21.0, 539,  36.5,  0,0.0,  0,0.0}
};

int readAdChannels(struct ad *ad, int cnt);
String buildServerMsg(struct ad *ad,int cnt);

Timer sched;
String httpReply;
int httpErr;
int httpLen;

EthernetClient client;
M2XStreamClient m2xClient(&client, m2xKey);
EthernetServer ipserver = EthernetServer(9000);
EthernetClient ipclient;



EthernetUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

void setup() {
  int succ;

  Serial.begin(9600);
  pinMode(13, OUTPUT);
  Serial.println("Start");
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
}


void loop()
{
  sched.update();
  refreshEvents();
}

void processAD()
{
  String message;
  static int cnt=0;

  readAdChannels(adArr,sizeof(adArr) / sizeof(struct ad));
  cnt++;
  if (cnt==AD_SAMPLECNT) {
    sched.oscillate(13,150, LOW,2);
    calcAdChannels(adArr,sizeof(adArr) / sizeof(struct ad));
    sendM2X(adArr,sizeof(adArr) / sizeof(struct ad));
    cnt=0;
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

String len2(int val)
{
  if (val>9)
    return String(val);
  else
    return "0"+String(val);
}

int sendM2X(struct ad *ad,int cnt)
{
  const char *streamNames[4];
  int counts[4];
  const char *ats[4]; 
  double values[4];
  int m2xpos=0;
  String timebuf;
  char chbuf[22];
  int response;
 
  timebuf=String(year())+"-"+
    len2(month())+"-"+
    len2(day())+"T"+
    len2(hour())+":"+
    len2(minute())+":"+
    len2(second())+"Z";
  
  timebuf.toCharArray(chbuf,21);
  for (int chan = 0; chan < cnt; chan++) {    
    if (ad[chan].changed) {
      values[m2xpos]=ad[chan].measured.analog;
      counts[m2xpos]=1;
      ats[m2xpos]=chbuf;
      streamNames[m2xpos]=ad[chan].name;
      m2xpos++;
    }
  }
  if (m2xpos) {
    digitalWrite(13,1);
    Serial.println();
    Serial.print(m2xpos);
    Serial.println(" records to send to M2X iot server");
    
    showCounters(counts,m2xpos);
    showStreamnames(streamNames,m2xpos);
    showTimes(ats,m2xpos);
    showValues(values,m2xpos);

    response = m2xClient.postMultiple(feedId, m2xpos, streamNames,
				      counts, ats, values);
    digitalWrite(13,0);
    Serial.print("M2x client response code: ");
    Serial.println(response);
  }
  else
    Serial.print(".");
  return m2xpos;
}


void showCounters(int *counters,int len)
{
  Serial.print("counters: ");
  for (int i=0;i<len;i++) {
    Serial.print(counters[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void showStreamnames(const char *names[],int len)
{
  Serial.print("Streamnames: ");
  for (int i=0;i<len;i++) {
    Serial.print(names[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void showTimes(const char *names[],int len)
{
  Serial.print("Timestamps: ");
  for (int i=0;i<len;i++)  {
    Serial.print(names[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void showValues(double *values,int len)
{
  Serial.print("Values: ");
  for (int i=0;i<len;i++) {
    Serial.print(values[i]);
    Serial.print(" ");
  }
  Serial.println();
}
    


int initAdChannels(struct ad *ad, int cnt)
{
  for (int analogChannel = 0; analogChannel < cnt; analogChannel++) {
    ad[analogChannel].delta.digital=ad[analogChannel].maxcal.digital - ad[analogChannel].mincal.digital;
    ad[analogChannel].delta.analog=ad[analogChannel].maxcal.analog - ad[analogChannel].mincal.analog;
  }
}


int readAdChannels(struct ad *ad, int cnt)
{
  int digital=0;
  int ddelta;
  float adelta,foo;

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
  float adelta,foo;
  int ts=now();
  bool timeout=false;

  for (int analogChannel = 0; analogChannel < cnt; analogChannel++) {
    digital = ad[analogChannel].measured.digital / AD_SAMPLECNT;
    // inter- and extrapolation
    ddelta=ad[analogChannel].delta.digital ;
    adelta=ad[analogChannel].delta.analog;

    ad[analogChannel].measured.analog  = ad[analogChannel].mincal.analog +
      (digital - ad[analogChannel].mincal.digital) * adelta / ddelta;

    ad[analogChannel].changed=0;
    timeout=(ts-ad[analogChannel].last_send > 1800);
    if (timeout || ((abs(ad[analogChannel].measured.analog - ad[analogChannel].prev.analog)) > ad[analogChannel].diff.analog)) {
      ad[analogChannel].prev.analog = ad[analogChannel].measured.analog;
      ad[analogChannel].prev.digital = digital ;
      ad[analogChannel].changed=1;
      ad[analogChannel].last_send=ts;
    }
    ad[analogChannel].measured.digital = 0;
  }
  return 0;
}




