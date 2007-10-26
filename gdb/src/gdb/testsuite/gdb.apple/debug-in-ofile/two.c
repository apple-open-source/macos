#include "dbg-in-ofile.h"

void
my_two (int inval, int *outval)
{
  int tmpval = inval * 2;
  my_three (tmpval, &tmpval);
  *outval = tmpval;
}
