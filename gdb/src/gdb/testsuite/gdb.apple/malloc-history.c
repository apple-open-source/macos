#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *get_a_string (int value)
{
  int i;
  char *retVal = malloc (value);
  return retVal;
}

char *call_get_a_string (int len)
{
  char *retVal;
  int i;

  retVal = get_a_string (len);
  for (i = 0; i < len - 1; i++)
    retVal[i] = (char) (i % (128 - 32)) + 32;

  return retVal;
}

int main ()
{
  char *foo;
  
  foo = call_get_a_string (1000);
  free (foo); /* good stopping point after call_get_a_string */

  return 0; /* good stopping point after free */
}
