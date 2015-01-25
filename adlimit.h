#ifndef __ADLIMIT__
#define __ADLIMIT__

class adlimit {
public:
  adlimit(float limit,int upper=1);
  int compare(float operand); 
private:
  float _limit;
  int _upper;
};

#endif
