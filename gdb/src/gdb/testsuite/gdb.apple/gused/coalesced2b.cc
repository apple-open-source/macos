#include <iostream>
#include "coalesced2.h"

int
whatever ()
{
  double ddddd = get_val ();
  std::cout << ddddd << std::endl;
  return (int) ddddd; /* a good place to stop in coalesced2b */
}
