#include <stdio.h>
#include <math.h>

#ifndef VERSION
#define VERSION 1
#endif

foo ()
{
  mprintf ("Hi, I am in foo, version %d.\n", VERSION); /* ERROR: mprintf() does not exist.  */
  baz ();
}
