#include <Arduino.h>
#include <stdlib.h>
#include "conversion.h"

conversion::conversion(float adder)
{
  _adder=adder;
}

int conversion::add(int source,int target)
{
  struct convert *c;

  c=(struct convert *) malloc(sizeof(struct convert));
  if (c!=NULL) {
    c->source=source;
    c->target=target;
    _cList.addtail((struct Node *)c);
    return 1;
  }
  return 0;
}

int conversion::callback(struct Node *n,void *data)
{
  float *value = (float *) data;
  struct convert *c=(struct convert *) n;

  if (c->source >= *value)
    return 1;
  return 0;
}

/* find boundings from conversion list
 * and convert / interpolate according to the list nodes
 */
float conversion::resolve(float v)
{
  struct convert *start;
  struct convert *stop;
  float ret;
  struct Node *n;

  n     = _cList.iterForward(_cList.getHead(),callback,(void *)&v);
  stop  = (struct convert *) n;
  start = (struct convert *) n->ln_Pred;
  
  ret = (_adder + start->target) - ((v - start->source) / (stop->source - start->source) * (start->target - stop->target));
  return ret;
}

  
