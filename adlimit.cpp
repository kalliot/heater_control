#include "adlimit.h"

adlimit::adlimit(float limit,int upper)
{
  _limit=limit;
  _upper=upper;
}

int adlimit::compare(float operand)
{
  if (operand > _limit)
    return _upper;
  else
    return !_upper;
}

