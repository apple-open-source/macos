#include <stdio.h>

/* declare all externally visible functions in libweak.c */
int foo (void) __attribute((weak_import));

main ()
{
  puts ("hi, in main");
  if (foo)
    foo ();
  puts ("finished in main");
}
