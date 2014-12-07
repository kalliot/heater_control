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
  struct dig2a prev;
  struct dig2a diff;
  struct dig2a mincal;
  struct dig2a maxcal;
  struct dig2a measured;
  struct dig2a delta;
};

struct ipcmd {
  char name[10];
  int minparams;
  void (* handler)(EthernetClient, int, char **);
};

void ipsetio(EthernetClient c,int argc,char *argv[]);
void ipevents(EthernetClient c,int argc,char *argv[]);
void ipclose(EthernetClient c,int argc,char *argv[]);

struct ipcmd ipCommands[] = {
  {"SETIO",3,ipsetio},
  {"CLOSE",0,ipclose},
  {"EVENTS",1,ipevents},
  {"",0,NULL}
};



struct ad adArr[]= { //prev   dif    min       max         meas    delta
  {0,"boiler",  0,     0,0.0, 0,0.5, 174,21.0, 935, 100.0,  0,0.0,  0,0.0},
  {1,"ambient", 0,     0,0.0, 0,0.3, 372,2.2,  964,  36.5,  0,0.0,  0,0.0},
  {2,"hothousewater",0,0,0.0, 0,0.5, 7,  21.0, 1023,100.0,  0,0.0,  0,0.0},
  {3,"radiator",0,     0,0.0, 0,0.3, 7,  21.0, 539,  36.5,  0,0.0,  0,0.0}
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

IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 103); // time-c.timefreq.bldrdoc.gov

const int timeZone = 0;     // UTC


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
    calcAdChannels(adArr,sizeof(adArr) / sizeof(struct ad));
    sendM2X(adArr,sizeof(adArr) / sizeof(struct ad));
    cnt=0;
  }
}


void ipsetio(EthernetClient client,int argc,char *argv[])
{   
  int port=atoi(argv[1]);
  int value=atoi(argv[2]);

  if (port > 11 && port < 14)
    digitalWrite(port,value);
  client.write(argv[0]);
  client.write(":OK\n");
}


void ipevents(EthernetClient c,int argc,char *argv[])
{
  if (!strcmp(argv[1],"START")) {
      addEventListener(c);
      c.write(argv[0]);
      c.write(":OK:START\n");
      return;
  }
  if (!strcmp(argv[1],"STOP")) {
      removeEventListener();
      c.write(argv[0]);
      c.write(":OK:STOP\n");
      return;
  }
  c.write(argv[0]);
  c.write(":ERR:UNKNOWN OPTION:");
  c.write(argv[1]);
  c.write("\n");
}

void ipclose(EthernetClient c,int argc,char *argv[])
{   
  c.write(argv[0]);
  c.write(":OK\n");
  removeEventListener();
  c.stop();
}

void parseCommand(EthernetClient c,char *command)
{ 
  int i;
  char *wptr[10];
  int wcnt=0;
  int commandlen=strlen(command);

  if (!commandlen) {
    Serial.println("zero len command");
      return;
  }
  wptr[wcnt++]=command;
  for (i=0;i<commandlen;) {
    if (command[i]==',') {
      command[i]=0;        
      wptr[wcnt]=&command[i+1];
      wcnt++;
      if (wcnt==10)
	break;
    }
    i++;
  }
  for (i=0;ipCommands[i].handler!=NULL;i++) {
    if (!strcmp(wptr[0],ipCommands[i].name)) {
      if (wcnt < ipCommands[i].minparams) {
        c.write(wptr[0]);
	c.write(":ERR:NOT ENOUGH PARAMS\n");
	Serial.println(": not enough params");
        return;
      }
      else {
	ipCommands[i].handler(c,wcnt,wptr);
	return;
      }
    }
  }
  c.write(wptr[0]);
  c.write(":ERR:COMMAND NOT FOUND\n");  
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
        parseCommand(ipclient,buff);
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
  Serial.println(timebuf);
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
    Serial.print(m2xpos);
    Serial.println(" records to send to M2X iot server");
    
    showCounters(counts,m2xpos);
    showStreamnames(streamNames,m2xpos);
    showTimes(ats,m2xpos);
    showValues(values,m2xpos);

    response = m2xClient.postMultiple(feedId, m2xpos, streamNames,
				      counts, ats, values);
    Serial.print("M2x client response code: ");
    Serial.println(response);
  }
  else
    Serial.println("nothing to send");
  
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

  for (int analogChannel = 0; analogChannel < cnt; analogChannel++) {
    digital = ad[analogChannel].measured.digital / AD_SAMPLECNT;
    // inter- and extrapolation
    ddelta=ad[analogChannel].delta.digital ;
    adelta=ad[analogChannel].delta.analog;

    ad[analogChannel].measured.analog  = ad[analogChannel].mincal.analog +
      (digital - ad[analogChannel].mincal.digital) * adelta / ddelta;

    ad[analogChannel].changed=0;
    if ((abs(ad[analogChannel].measured.analog - ad[analogChannel].prev.analog)) > ad[analogChannel].diff.analog) {
      ad[analogChannel].prev.analog = ad[analogChannel].measured.analog;
      ad[analogChannel].prev.digital = digital ;
      ad[analogChannel].changed=1;
    }
    ad[analogChannel].measured.digital = 0;
  }
  return 0;
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}


