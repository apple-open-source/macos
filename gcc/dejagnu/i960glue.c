#include "../newlib/libc/ctype/ctype_.c"
#include "../newlib/libc/stdlib/strtol.c"
#include "../newlib/libc/stdlib/atoi.c"
#include "../newlib/libc/reent/impure.c"


/* these are for ld -r -wrap */

void
__real_exit (int code)
{
  exit (code);
}

void
__real_abort (void)
{
  abort ();
}
