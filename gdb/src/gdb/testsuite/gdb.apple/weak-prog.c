#include <stdio.h>

/* declare all externally visible functions in libweak.c */
int foo (void) __attribute((weak_import));

main (int argc, char **argv)
{
  puts ("hi, in main");
  if (foo)
    foo ();
  puts ("finished in main");
}
