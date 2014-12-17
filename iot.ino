	
	
static const char *streamNames[5];
static int counts[5];
static const char *ats[6];
static double values[6];
static int pos=0;
static int samplecnt=0;

void iotReset()
{
  memset(&counts,0,5);
  memset(&values,0,6);
  pos=0;
  samplecnt=0;
}

void iotName(char *name)
{
  streamNames[pos]=name;
}

void iotAddValue(float value,char *ts)
{
  values[samplecnt]=value;
  counts[pos]++;
  ats[samplecnt]=ts;
  samplecnt++;
}

void iotNext()
{
  pos++;
}

int iotRecCnt()
{
  return samplecnt;
}

int iotSend(char *id)
{ 
  return m2xClient.postMultiple(id, pos, streamNames,
				counts, ats, values);
}

void iotShowCounters()
{
  Serial.print("counters: ");
  for (int i=0;i<pos;i++) {
    Serial.print(counts[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void iotShowStreamnames()
{
  Serial.print("Streamnames: ");
  for (int i=0;i<pos;i++) {
    Serial.print(streamNames[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void iotShowTimes()
{
  Serial.print("Timestamps: ");
  for (int i=0;i<samplecnt;i++)  {
    Serial.print(ats[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void iotShowValues()
{
  Serial.print("Values: ");
  for (int i=0;i<samplecnt;i++) {
    Serial.print(values[i]);
    Serial.print(" ");
  }
  Serial.println();
}
    

