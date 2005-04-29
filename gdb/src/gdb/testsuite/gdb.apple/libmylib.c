#include <stdio.h>

int foo (void)
{
  printf ("In foo\n");
}

/* Despite having the same name as libSystem's puts(), this function will
   never be called.  Alas, gdb will put breakpoints and such on this, not
   understanding the dyld name binding semantics fully. */

int puts (const char *in)
{
  printf ("puts is being called in libmylib.c: `%s'\n", in);
}
