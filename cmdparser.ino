#include <Ethernet.h>

struct ipcmd {
  char name[10];
  int minparams;
  void (* handler)(EthernetClient, int, char **);
};

static struct ipcmd commands[] = {
  {"SETIO",3,ipsetio},
  {"CLOSE",0,ipclose},
  {"EVENTS",1,ipevents},
  {"",0,NULL}
};




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

