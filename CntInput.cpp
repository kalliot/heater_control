#include "CntInput.h"
#include <Time.h>

CntInput::CntInput(int measTimeout)
{
  _measTimeout = measTimeout;
}

int CntInput::add(int irqno,char *name,float diff,
		  float diff_factor,float scale)
{
  struct cnt *c;

  c = (struct cnt *) malloc(sizeof(struct cnt));
  if (c!=NULL) {
    c->irqno       = irqno;
    c->name        = name;
    c->diff        = diff;
    c->diff_factor = diff_factor;
    c->scale       = scale;
    c->n.ln_Name   = c->name;
    c->last_send   = now();
    _cntlist.addtail((struct Node *) c);
    _count++;
  }
  return 0;
}

int CntInput::getCount(void)
{
  return _count;
}

int CntInput::startRead(void)
{
  _counters=(int *) malloc(sizeof(int) * _count);
  /*
  for (int i=0;i<_count;i++) {
    if (cntArr[i].irqno!=-1) {
      cntArr[i].last_send=now();
      attachInterrupt(cntArr[i].irqno,cntArr[i].irqh, CHANGE);
    }
  }
  */
}

struct cnt * CntInput::getNamed(char *name)
{
  return (struct cnt *) _cntList.findname(_cntList.getHead(),name);
}

struct cnt * CntInput::getFirst()
{
  return (struct cnt *) _cntList.getHead();  
}

struct cnt * CntInput::getNext(struct cnt *cnt)
{
  return (struct cnt *) cnt->n.ln_Succ;
}

int CntInput::_calc(struct Node *n,void *data)
{
  struct cnt *c=(struct cnt *) n;
  unsigned int tmpCnt;

  if (c->irqno != -1) {
    noInterrupts();
    tmpCnt = _counters[i];
    interrupts();
    c->prev.analog=c->measured.analog;
    c->measured.analog = (tmpCnt-c->prev.digital) / (c->scale.digital * c->scale.analog); 
    if (c->measured.analog > 20.0) {  // debug code. Sometimes during development a very big kw is sent to iot host.
      c->measured.analog = 20.0;
    }
    c->avg_counter += tmpCnt-c->prev.digital;
    c->avg_samples++;
    c->prev.digital=tmpCnt;
  }
  return 0;
}

int CntInput::calc(void)
{
  _cntList.iterForward(_cntList.getHead(),_read,NULL);
  _samples++;
  if (_samples == AD_SAMPLECNT)
    return 1;
  return 0;
}
