#include "blubby.h"
#include "multiplier.h"

int 
fooFunc (MyBlubby &mine)
{
  LONG_TYPE multiplier;
  struct input_type input = { 20, 40 };
  multiplier = GET_MULTIPLIER (&input);
  return (int) (multiplier * mine.getValue ());
}

