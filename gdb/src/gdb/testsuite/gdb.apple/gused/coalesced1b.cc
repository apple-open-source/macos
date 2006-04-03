#include <iostream>
#include "coalesced1.h"

int
whatever ()
{
  MyCls var(10);
  float mmmmm = 5.0;
  std::cout << var.x << mmmmm << std::endl;
  return var.x; /* a good place to stop in coalesced1b */
}
