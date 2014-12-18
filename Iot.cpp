/*
 Iot.h - construct messages to be sent to at&t:s m2x iot cloud
 Teppo Kallio, 2014
*/

#include "Iot.h"	
#include "M2XStreamClient.h"

Iot::Iot(M2XStreamClient *m2xsc) 
{
  _m2xsc = m2xsc;
  return;
}

void Iot::reset()
{
  memset(&_counts,0,5);
  memset(&_values,0,6);
  _pos=0;
  _samplecnt=0;
}

void Iot::name(char *name)
{
  _streamNames[_pos]=name;
}

void Iot::addValue(float value,char *ts)
{
  _values[_samplecnt]=value;
  _counts[_pos]++;
  _ats[_samplecnt]=ts;
  _samplecnt++;
}

void Iot::next()
{
  _pos++;
}

int Iot::getRecCnt()
{
  return _samplecnt;
}

int Iot::send(char *id)
{
  return _m2xsc->postMultiple(id, _pos, _streamNames,
                              _counts, _ats, _values);
}

void Iot::showCounters()
{
  Serial.print("counters: ");
  for (int i=0;i<_pos;i++) {
    Serial.print(_counts[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void Iot::showStreamnames()
{
  Serial.print("Streamnames: ");
  for (int i=0;i<_pos;i++) {
    Serial.print(_streamNames[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void Iot::showTimes()
{
  Serial.print("Timestamps: ");
  for (int i=0;i<_samplecnt;i++)  {
    Serial.print(_ats[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void Iot::showValues()
{
  Serial.print("Values: ");
  for (int i=0;i<_samplecnt;i++) {
    Serial.print(_values[i]);
    Serial.print(" ");
  }
  Serial.println();
}
    

