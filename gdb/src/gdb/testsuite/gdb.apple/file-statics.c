#include <string.h>

const int const_int = 5;
const int const_doesnt_match = 6;
const char *const_char = "I am a constant";
int global_int = 6;
static int static_int = 7;
static int static_doesnt_match = 8;

extern int foo (void);

int main ()
{
  /* Use all the variables so -gused won't strip them.  */
  foo ();

  if (strcmp (const_char, "something") != 0)
    return static_doesnt_match * const_doesnt_match 
      * const_int * global_int * static_int;
  else
    return 0;
}
