#ifndef __ADINPUT__
#define __ADINPUT__

#include <Time.h>
#include "Iot.h"
#include "lists.h"

#define AD_SAMPLECNT 10

struct dig2a
{
  int digital;
  float analog;
};

struct average {
  int samples;
  int curr_cnt;
  unsigned long sum;
  int *buff;
  int index;
};

struct ad {
  struct Node n;
  int port;
  char *name;
  int flags;
  time_t prev_ts;
  time_t last_send;
  long direction;
  int angular_velocity;
  int sampleindex;
  int samples[AD_SAMPLECNT];
  struct average avg;
  struct dig2a prev;
  struct dig2a diff;
  struct dig2a mincal;
  struct dig2a maxcal;
  struct dig2a measured;
  struct dig2a delta;
};

class AdInput {
 public:
  AdInput(int measTimeout);
  int add(int port,char *name,float diff,
	  int mind,float minf,int maxd,float maxf,int average=0);
  int getCount(void);
  struct ad * getNamed(char *name);
  struct ad * getFirst();
  struct ad * getNext(struct ad *ad);
  boolean verify(void);
  void calc(void);
  int  read(void);
  void reset(void);
  void evaluateConditions(void);
  boolean isTimeout(time_t ts,boolean advance);
  void buildIot(char *chbuf,Iot *iot);
  static int _evaluateCondition(struct Node *n,void *data);
  static int _calc(struct Node *n,void *data);
  static int _read(struct Node *n,void *data);
  static int _verify(struct Node *n,void *data);
  static int _reset(struct Node *n,void *data);
  static int _buildIot(struct Node *n,void *data);
  static int _timeout(struct Node *n,void *data);
  static int _filter(struct ad *a);
  static int _smoothe(struct average *avg,int val);
 private:
  List _adList;
  int _samples;
  int _count;
  int _measTimeout;
  static int _currTimeout;
  static boolean _isTimeout;
  static boolean _isChanged;
};

#endif
