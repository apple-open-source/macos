#include "dbg-in-ofile.h"

void
my_four (int inval, int *outval)
{
  int tmpval = inval * 4;
  my_five (tmpval, &tmpval);
  *outval = tmpval;
}
