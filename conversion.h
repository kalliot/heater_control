#ifndef __CONVERSION__
#define __CONVERSION__

struct convert {
  int source;
  int target;
};

class conversion {
public:
  conversion(struct convert *c,float adder);
  float resolve(float v);
private:
  struct convert *_ct;
  float _adder;
};

#endif
