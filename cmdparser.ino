#include <Ethernet.h>
#include <stdlib.h>
#include "eepromsetup.h"

struct ipcmd {
  char name[10];
  int minparams;
  void (* handler)(EthernetClient, int, char **);
};

// command symbolic names are kept on array, to make parsing more elegant.
static struct ipcmd commands[] = {
  {"setio",    3, ipsetio},
  {"close",    0, ipclose},
  {"events",   1, ipevents},
  {"adcalibr", 4, ipadcalibr},
  {"timeout",  2, iptimeout},
  {"m2xfeed",  1, ipm2xfeed},
  {"m2xkey",   1, ipm2xkey},
  {"savesetup",0, ipsavesetup},
  {"showsetup",0, ipshowsetup},
  {"",0,NULL}
};

static int search_ad(char *name)
{
  for (int i=0;i < sizeof(adArr) / sizeof(struct ad);i++) {
    if (!strcmp(adArr[i].name,name))
      return i;
  }
  return -1;
}

static void reply2remote(EthernetClient c,char *cmd,char *ret,char *msg)
{
    c.write(cmd);
    c.write(ret);
    c.write(msg);
    c.write("\n");
}

static void ipadcalibr(EthernetClient client,int argc,char *argv[])
{
  int adindex=search_ad(argv[1]);
  if (adindex==-1) {
    reply2remote(client,argv[0],":ERR:UNKNOWN CHANNEL:",argv[1]);
    return;
  }
  if (!strcmp(argv[2],"min")) {
    float v=atof(argv[3]);
    adArr[adindex].mincal.analog=v;
    if (argc==4)  // use current digital value;
      adArr[adindex].mincal.digital=adArr[adindex].prev.digital;
    if (argc==5) // user what we got from remote
      adArr[adindex].mincal.digital=atoi(argv[4]);
    reply2remote(client,argv[0],":OK:",argv[1]);
    return;
  }
  else if (!strcmp(argv[2],"max")) {
    float v=atof(argv[3]);
    adArr[adindex].maxcal.analog=v;
    if (argc==4)  // use current digital value;
      adArr[adindex].maxcal.digital=adArr[adindex].prev.digital;
    if (argc==5) // user what we got from remote
      adArr[adindex].maxcal.digital=atoi(argv[4]);
    reply2remote(client,argv[0],":OK:",argv[1]);
    return;
  }
  else if (!strcmp(argv[2],"diff")) {
    return;
  }
  else {
    reply2remote(client,argv[0],":ERR:UNKNOWN PARAM:",argv[2]);
  }
}

// arm float to char* conversion is a bit complicated, so
// lets make a function for it.

static char *f2str(float val)
{
  static char str[12];
  String tmp;

  tmp=String(dtostrf(val,12,2,str));
  tmp.trim();
  tmp.toCharArray(str,12);
  return str;
}

static void ipshowsetup(EthernetClient client,int argc,char *argv[])
{
  char str[12];

  client.write("m2xfeed,");
  client.write(eepromsetup.m2xfeed);
  client.write("\nm2key,");
  client.write(eepromsetup.m2xkey);
  client.write("\n");

  for (int i=0;i < sizeof(adArr) / sizeof(struct ad);i++) {
    client.write("adcalibr,");
    client.write(adArr[i].name);
    client.write(",min,");
    client.write(f2str(adArr[i].mincal.analog));

    client.write(",");
    client.write(itoa(adArr[i].mincal.digital,str,10));
    client.write("\n");

    client.write("adcalibr,");
    client.write(adArr[i].name);
    client.write(",max,");
    client.write(f2str(adArr[i].maxcal.analog));

    client.write(",");
    client.write(itoa(adArr[i].maxcal.digital,str,10));
    client.write("\n");

    client.write("adcalibr,");
    client.write(adArr[i].name);
    client.write(",diff,");
    client.write(f2str(adArr[i].diff.analog));

    client.write("\n");
  }
  reply2remote(client,argv[0],":OK","");
  return;
}

static void ipsavesetup(EthernetClient client,int argc,char *argv[])
{
  eepWriteAll();
  return;
}

static void iptimeout(EthernetClient client,int argc,char *argv[])
{
  return;
}

static void ipm2xfeed(EthernetClient client,int argc,char *argv[])
{
  memcpy(eepromsetup.m2xfeed,argv[1],33);
  return;
}

static void ipm2xkey(EthernetClient client,int argc,char *argv[])
{
  memcpy(eepromsetup.m2xkey,argv[1],33);
  return;
}

static void ipsetio(EthernetClient client,int argc,char *argv[])
{   
  int port=atoi(argv[1]);
  int value=atoi(argv[2]);

  if (port > 11 && port < 14) {
    digitalWrite(port,value);
    reply2remote(client,argv[0],":OK","");
    return;
  }
  reply2remote(client,argv[0],":ERR:","OUT OF RANGE");
}


static void ipevents(EthernetClient c,int argc,char *argv[])
{
  if (!strcmp(argv[1],"start")) {
      addEventListener(c);
      reply2remote(c,argv[0],":OK","START");
      return;
  }
  if (!strcmp(argv[1],"stop")) {
      removeEventListener();
      reply2remote(c,argv[0],":OK","STOP");
      return;
  }
  reply2remote(c,argv[0],":ERR:UNKNOWN OPTION",argv[1]);
}

static void ipclose(EthernetClient c,int argc,char *argv[])
{   
  reply2remote(c,argv[0],":OK","");
  removeEventListener();
  c.stop();
}

// save pointer of each string from beginning and after comma to an array of char pointers
// change every comma to terminating zero.
// by this way the wptr array seems to stor N pcs of zero terminated char arrays.
void cmdParse(EthernetClient c,char *command)
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
  for (i=0;commands[i].handler!=NULL;i++) {
    if (!strcmp(wptr[0],commands[i].name)) {
      if (wcnt < commands[i].minparams) {
	reply2remote(c,wptr[0],":ERR:","NOT ENOUGH PARAMS");
	Serial.println(": not enough params");
        return;
      }
      else {
	commands[i].handler(c,wcnt,wptr);
	return;
      }
    }
  }
  reply2remote(c,wptr[0],":ERR:","COMMAND NOT FOUND");
}
