#include "blubby.h"
#include "multiplier.h"

int 
bazFunc2 (MyBlubby &mine)
{
  LONG_TYPE multiplier;
  struct input_type input = { 20, 40 };
  multiplier = GET_MULTIPLIER (&input);
  return (int) (multiplier * mine.getValue ());
}
