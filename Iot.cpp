/*
 Iot.cpp - construct messages to be sent to at&t:s m2x iot cloud
 Teppo Kallio, 2014
*/

#include <stdio.h>
#include <PubNub.h>
#include "heater_control.h"
#include "Iot.h"	
#include "M2XStreamClient.h"



Iot::Iot(M2XStreamClient *m2xsc) 
{
  _m2xsc = m2xsc;
  strcpy(pubkey,"demo");
  strcpy(subkey,"demo");
  strcpy(channel,"demo_tutorial");
  PubNub.begin(pubkey, subkey);
  return;
}

boolean Iot::setBufferSizes(int s)
{
  _streamNames=(const char **) malloc(sizeof(char *) * s);
  _counts=(int * ) malloc(sizeof(int) * s);
  _ats=(const char **) malloc(sizeof(char *) * s * 2);
  _values=(double *) malloc(sizeof(double) * s * 2);
  if (_streamNames==NULL || _counts==NULL || _ats==NULL || _values==NULL) {
    Serial.println("malloc failed");
    return false;
  }
  _buffSizes=s;
  return true;
}

void Iot::reset()
{
  memset(_counts,0,sizeof(int) * _buffSizes);
  memset(_values,0,sizeof(double) * _buffSizes * 2);
  _pos=0;
  _samplecnt=0;
}

void Iot::name(char *name)
{
  _streamNames[_pos]=name;
}

void Iot::addValue(float value,char *ts)
{
  if (_samplecnt >= _buffSizes * 2) {
    Serial.println("_samplecnt overflow");
    return;
  }
  _values[_samplecnt]=value;
  _counts[_pos]++;
  _ats[_samplecnt]=ts;
  _samplecnt++;
}

void Iot::next()
{
  if (_pos < _buffSizes)
    _pos++;
  else {
    Serial.println("_pos overflow");
    return;
  }
}

int Iot::getRecCnt()
{
  return _samplecnt;
}

int Iot::toggle(char *name,int state,int duration)
{
  EthernetClient *pclient;
  char msg[128];

  sprintf(msg,"{\"measurements\":[],\"states\":[{\"name\":\"%s\",\"value\":\%d\,\"duration\":%d}]}",name,state,duration);
  Serial.print("Iot::send pubnub: ");
  Serial.println(msg);
  
  digitalWrite(SEND_LED,1); 
  pclient = PubNub.publish(channel, msg);
  digitalWrite(SEND_LED,0); 
  if (!pclient) {
    Serial.println("publishing error");
  }
  else
    pclient->stop();

}

int Iot::send(char *id)
{
   EthernetClient *pclient;

   char msg[256]="{\"measurements\":[";
   int i,j,ret;
   char s[16];

   for (i=0;i<_pos;i++) {
       strcat(msg,"{\"name\":\"");
       strcat(msg,_streamNames[i]);
       strcat(msg,"\",\"values\":[");
       for (j=0;j<_counts[i];j++) {
          dtostrf(_values[i+j],4,1,s);
          strcat(msg,s);
          strcat(msg,",");
       }
      msg[strlen(msg)-1]=0;
      strcat(msg,"]},");
   }
   if (_pos) {
      msg[strlen(msg)-1]=0;
      strcat(msg,"],\"states\":[]}");
      Serial.print("Iot::send pubnub: ");
      Serial.println(msg);

      digitalWrite(SEND_LED,1); 
      pclient = PubNub.publish(channel, msg);
      if (!pclient) {
         Serial.println("publishing error");
      }
      else
         pclient->stop();
   }
   ret=_m2xsc->postDeviceUpdates(id, _pos, _streamNames,
                               _counts, _ats, _values);
   digitalWrite(SEND_LED,0); 
   return ret;
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
    

