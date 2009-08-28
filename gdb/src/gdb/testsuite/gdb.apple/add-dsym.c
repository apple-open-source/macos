#include <string.h>
#include <stdlib.h>

extern int foo (int input);

static int
bar (char *in_char, int shift)
{
  int iter;
  int len = strlen(in_char);

  for (iter = 0; iter < len; iter++)
    {
      in_char[iter] += shift % 32;
    }
  return iter;
}

int
main ()
{
  int shift_amt = foo (111);
  int iter;
  char *in_char = malloc (40); /* good stopping point in main */

  for (iter = 0; iter < 40; iter++)
    in_char[iter] = 'a' + iter;
  in_char[39] = '\0';
  bar (in_char, shift_amt);

  return 0;
}

  
