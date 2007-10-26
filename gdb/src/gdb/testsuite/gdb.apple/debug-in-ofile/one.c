#include "dbg-in-ofile.h"

void
my_one (int inval, int *outval)
{
  int tmpval = inval * 1;
  my_two (tmpval, &tmpval);
  *outval = tmpval;
}
