#include "conversion.h"

conversion::conversion(struct convert *c,float adder)
{
  _ct=c;
  _adder=adder;
}

/* find boundings from conversion table
 * and convert / interpolate according to the table
 */
float conversion::resolve(float v)
{
  struct convert *start;
  struct convert *stop;
  float ret;

  for (int i=0; _ct[i].source != 0xff; i++) {
    if (_ct[i].source < v)
      start=&_ct[i];
    else {
      stop=&_ct[i];
      break;
    }
  }
  ret = (_adder + start->target) - ((v - start->source) / (stop->source - start->source) * (start->target - stop->target));
  return ret;
}

  
