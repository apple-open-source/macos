#include "dbg-in-ofile.h"

void
my_five (int inval, int *outval)
{
  int tmpval = inval * 5;
  my_six (tmpval, &tmpval);
  *outval = tmpval;

}
