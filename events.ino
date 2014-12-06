#include <Timer.h>
#include <Ethernet.h>


static Timer timer;
static int id=-1;
static EthernetClient ec;

void processEvents()  
{
  Serial.println("event happened");
  ec.write("EVENT HAPPENED\n");
}

void addEventListener(EthernetClient c)
{
  int slot;
  ec=c;

  if (id==-1) {
    id=timer.every(3000,processEvents);
    Serial.println("events started");
  }
}

void removeEventListener()
{
  if (id==-1) return;
  timer.stop(id);
  Serial.println("all events stopped");
}

void refreshEvents()
{
  timer.update();
}
