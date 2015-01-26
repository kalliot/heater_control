#ifndef __CONVERSION__
#define __CONVERSION__

#include "lists.h"

struct convert {
  struct Node n;
  int source;
  int target;
};

class conversion {
public:
  conversion(float adder);
  int add(int source,int target);
  float resolve(float v);
  static int callback(struct Node *n,void *data);
private:
  List _cList;
  float _adder;
};

#endif
