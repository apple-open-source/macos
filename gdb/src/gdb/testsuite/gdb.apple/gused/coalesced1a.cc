#include "coalesced1.h"

int whatever ();

int
main ()
{
  MyCls a(5);
  float mmmmm = 5.0;

  double anothervar;
  anothervar = a.x;

  whatever();  /* a good place to stop in coalesced1a */
}
