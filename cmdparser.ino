#include <Ethernet.h>
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
  {"",0,NULL}
};


static void ipadcalibr(EthernetClient client,int argc,char *argv[])
{
  for (int i=0;i < sizeof(adArr) / sizeof(struct ad);i++) {
    if (!strcmp(adArr[i].name,argv[1])) {
      if (!strcmp(argv[2],"min")) {
	return;
      }
      else if (!strcmp(argv[2],"max")) {
	return;
      }
      else if (!strcmp(argv[2],"diff")) {
	return;
      }
      else {
	client.write(argv[0]);
	client.write(":ERR:UNKNOWN PARAMETER:");
	client.write(argv[2]);
	client.write("\n");
      }
      return;
    }
  }
  client.write(argv[0]);
  client.write(":ERR:UNKNOWN CHANNEL:");
  client.write(argv[1]);
  client.write("\n");
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

  if (port > 11 && port < 14)
    digitalWrite(port,value);
  client.write(argv[0]);
  client.write(":OK\n");
}


static void ipevents(EthernetClient c,int argc,char *argv[])
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

static void ipclose(EthernetClient c,int argc,char *argv[])
{   
  c.write(argv[0]);
  c.write(":OK\n");
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
        c.write(wptr[0]);
	c.write(":ERR:NOT ENOUGH PARAMS\n");
	Serial.println(": not enough params");
        return;
      }
      else {
	commands[i].handler(c,wcnt,wptr);
	return;
      }
    }
  }
  c.write(wptr[0]);
  c.write(":ERR:COMMAND NOT FOUND\n");  
}
