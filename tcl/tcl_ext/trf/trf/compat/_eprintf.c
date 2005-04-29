#include <tcl.h>
#include <stdio.h>

/* This is used by the `assert' macro.  */
void
__eprintf (string, expression, line, filename)
     CONST char *string;
     CONST char *expression;
     int line;
     CONST char *filename;
{
  fprintf (stderr, string, expression, line, filename);
  fflush (stderr);
  abort ();
}

