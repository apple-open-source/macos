#include "coalesced2.h"

int whatever ();

main ()
{
  double ddddd = get_val ();

  whatever(); /* a good place to stop in coalesced2a */
}
