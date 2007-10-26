#include <stdio.h>

/* Note:  For the weak.exp tests to work correctly, each globally visible
   function defined in this file must also be declared in weak-prog.c. 
   So if you add a new function, add a new prototype to that file as well. */

int foo (void)
{
  printf ("In foo\n");
}
