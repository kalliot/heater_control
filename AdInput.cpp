#include "heater_control.h"
#include "AdInput.h"
#include <string.h>

int     AdInput::_currTimeout=0;
boolean AdInput::_isTimeout=false;
boolean AdInput::_isChanged=false;

AdInput::AdInput(int measTimeout)
{
  _measTimeout=measTimeout;
  _count = 0;
}

int AdInput::add(int port,char *name,float diff,
		 int mind,float minf,int maxd,float maxf,int average)
{
  struct ad *a;
  int addr;
  time_t ts=now();

  a = (struct ad *) malloc(sizeof(struct ad));
  if (a!=NULL) {
    addr=(int) a;
    Serial.print(name);
    Serial.print(":");
    Serial.println(addr);
    a->port            = port;
    a->name            = name;
    a->avg.samples     = average;
    if (average > 0) {
      a->avg.buff = (int *) malloc(average * sizeof(int));
      if (a->avg.buff == NULL)
	a->avg.samples=0;
      else {
        a->avg.index = 0;
        a->avg.curr_cnt=0;
        a->avg.sum = 0;
      }
    }
    a->diff.analog     = diff;
    a->mincal.digital  = mind;
    a->mincal.analog   = minf;
    a->maxcal.digital  = maxd;
    a->maxcal.analog   = maxf;
    a->measured.analog = -273;
    a->prev_ts         = ts;
    a->prev.digital    = 0;
    a->delta.digital   = a->maxcal.digital - a->mincal.digital;
    a->delta.analog    = a->maxcal.analog - a->mincal.analog;
    a->n.ln_Name       = a->name;
    a->diff.digital    = a->delta.digital / a->delta.analog * a->diff.analog;
    a->flags &= ~FLAGS_DATACHANGE;
    a->sampleindex=0; // reset index
    _adList.addtail((struct Node *)a);
    _count++;
    return 1;
  }
  return 0;
}

struct ad * AdInput::getNamed(char *name)
{
  return (struct ad *) _adList.findname(_adList.getHead(),name);
}

struct ad * AdInput::getFirst()
{
  return (struct ad *) _adList.getHead();  
}

struct ad * AdInput::getNext(struct ad *ad)
{
  return (struct ad *) ad->n.ln_Succ;
}

int AdInput::getCount(void)
{
  return _count;
}

int AdInput::_smoothe(struct average *avg,int val)
{
  int retval;

  if (avg->samples==0)
    return val;
  if (val==-1)
    return  avg->sum / avg->curr_cnt;

  if (avg->curr_cnt < avg->samples) { // still filling
    avg->curr_cnt++;
  }
  else {
    if (avg->index == avg->samples)
      avg->index=0;
    avg->sum-=avg->buff[avg->index];
  }
  avg->buff[avg->index]=val;
  avg->sum+=val;
  avg->index++;
  return avg->sum / avg->curr_cnt;
}

int AdInput::_read(struct Node *n,void *data)
{
  struct ad *a=(struct ad *) n;

  if (a->sampleindex >= AD_SAMPLECNT)
    return 1;
  a->samples[a->sampleindex] = analogRead(a->port);
  a->sampleindex++;
  return 0;
}

int AdInput::_calc(struct Node *n,void *data)
{
  int ddelta;
  float adelta;
  struct ad *a=(struct ad *) n;
  long prev_direction;

  if (a->flags & FLAGS_DATACHANGE) {
    ddelta   = a->delta.digital;
    adelta   = a->delta.analog;
  
    a->measured.analog  = a->mincal.analog + (a->measured.digital - a->mincal.digital) * adelta / ddelta;
    a->prev.analog      = a->measured.analog;

    prev_direction = a->direction;
    a->direction = (3600 / (a->last_send - a->prev_ts)) * (a->measured.digital - a->prev.digital); // how many digital steps per hour.
    a->angular_velocity = a->direction - prev_direction;
    Serial.print("----> ");
    Serial.print(a->name);
    Serial.print(" direction=");
    Serial.print(a->direction);
    Serial.print(", angular_velocity=");
    Serial.println(a->angular_velocity);
  }
  return 0;
}


void AdInput::calc(void)
{
  time_t ts=now();
  _adList.iterForward(_adList.getHead(),_calc,&ts);
}


// find out the biggest anomaly from samples
// and remove it from the calc
int AdInput::_filter(struct ad *a)
{
  int i, avg,newavg;
  long sum=0,newsum=0;
  int candidateindex=0;
  int diff,maxdiff=0;

  for (i=0;i < AD_SAMPLECNT;i++)
    sum += a->samples[i];
  avg = sum / AD_SAMPLECNT;

  for (i=0;i < AD_SAMPLECNT;i++) {
    diff = a->samples[i]-avg;
    if (diff > maxdiff) {
      maxdiff=diff;
      candidateindex=i;
    }
  }

  newsum = sum - a->samples[candidateindex];
  newavg =  newsum / (AD_SAMPLECNT -1);
  return newavg;
}


int AdInput::_verify(struct Node *n,void *data)
{
  struct ad *a=(struct ad *) n;
  time_t *ts=(time_t *) data;
  int digital;

  digital = _smoothe(&a->avg,_filter(a));
  if ((abs(digital - a->measured.digital)) > a->diff.digital) {
    a->flags |= FLAGS_DATACHANGE;
    a->flags |= FLAGS_EVALUATE;
    a->prev_ts          = a->last_send;
    a->last_send        = *ts;
    a->prev.digital =   a->measured.digital;
    a->measured.digital = digital;
    _isChanged=true;
  }
  return 0;
}

int AdInput::read(void)
{
  _adList.iterForward(_adList.getHead(),_read,NULL);
  _samples++;
  if (_samples == AD_SAMPLECNT)
    return 1;
  return 0;
}

boolean AdInput::verify(void)
{ 
  time_t ts=now();
  _isChanged=false;
  _adList.iterForward(_adList.getHead(),_verify,&ts);
  return _isChanged;
}

int AdInput::_reset(struct Node *n,void *data)
{
  struct ad *a=(struct ad *) n;
  
  a->flags &= ~FLAGS_DATACHANGE;
  a->sampleindex=0; // reset index
  return 0;
}

void AdInput::reset(void)
{
  _adList.iterForward(_adList.getHead(),_reset,NULL);
  _samples = 0;
  _currTimeout = _measTimeout;
}

struct iotpar {
  Iot *iot;
  char *timebuff;
};

int AdInput::_buildIot(struct Node *n,void *data)
{
  struct ad *a=(struct ad *) n;
  struct iotpar *i=(struct iotpar *) data;

  if (a->flags & FLAGS_DATACHANGE) {
    i->iot->name(a->name);
    i->iot->addValue(a->measured.analog,i->timebuff);
    i->iot->next();
  }
  return 0;
}

void AdInput::buildIot(char *chbuf,Iot *iot)
{
  struct iotpar iop;

  iop.iot = iot;
  iop.timebuff = chbuf;
  _adList.iterForward(_adList.getHead(),_buildIot,&iop);
}

int AdInput::_timeout(struct Node *n,void *data)
{
  struct ad *a=(struct ad *) n;
  time_t *ts= (time_t *) data;
  int elapsed= (*ts - a->last_send);
  
  if (elapsed > _currTimeout) {
    Serial.print("timeout ");
    Serial.print(a->name);
    Serial.print(" elapsed seconds is ");
    Serial.print(elapsed);
    Serial.print(" currTimeout=");
    Serial.println(_currTimeout);

    a->prev.digital =   a->measured.digital;
    a->measured.digital = _smoothe(&a->avg,_filter(a));
    a->prev_ts          = a->last_send;
    a->last_send        = *ts;
    a->flags |= FLAGS_DATACHANGE;
    a->flags |= FLAGS_EVALUATE;
    _isTimeout=true;
  }
  return 0;
}

boolean AdInput::isTimeout(time_t ts,boolean advance)
{
  if (advance)
    _currTimeout = _measTimeout - 300;

  _isTimeout=false;
  _adList.iterForward(_adList.getHead(),_timeout,&ts);
  return _isTimeout;
}
