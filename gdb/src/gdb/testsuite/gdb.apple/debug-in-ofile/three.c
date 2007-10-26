#include "dbg-in-ofile.h"

void
my_three (int inval, int *outval)
{
  int tmpval = inval * 3;
  my_four (tmpval, &tmpval);
  *outval = tmpval;
}
