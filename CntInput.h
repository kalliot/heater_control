#ifndef __CNTINPUT__
#define __CNTINPUT__

#include "lists.h"

struct cnt {
  struct Node n;
  unsigned int irqno;
  char *name;
  int flags;
  time_t last_send;
  long avg_counter;
  int avg_samples;
  void (* irqh)(void);
  struct dig2a prev;
  struct dig2a prevsnd;
  struct dig2a diff;
  struct dig2a diff_factor;
  struct dig2a scale;
  struct dig2a measured;
};

class CntInput {
 public:
  CntInput(int measTimeout);
  int add(int irqno,char *name,float diff,
	  float diff_factor,float scale);
  int getCount(void);
  struct cnt * getNamed(char *name);
  struct cnt * getFirst();
  struct cnt * getNext(struct cnt *cnt);
  boolean verify(void);
  void calc(void);
  void reset(void);
  void evaluateConditions(void);
  boolean isTimeout(time_t ts,boolean advance);
  void buildIot(char *chbuf,Iot *iot);
  static int _evaluateCondition(struct Node *n,void *data);
  static int _calc(struct Node *n,void *data);
  static int _verify(struct Node *n,void *data);
  static int _reset(struct Node *n,void *data);
  static int _buildIot(struct Node *n,void *data);
  static int _timeout(struct Node *n,void *data);
 private:
  List _cntList;
  int _measTimeout;
}

#endif
