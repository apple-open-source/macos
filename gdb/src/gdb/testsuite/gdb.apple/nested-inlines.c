#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


int 
main (int argc, char **argv)
{
  int x;
  
  fprintf (stderr, "Here we go!!\n");
  
  x = foo ();

  fprintf (stderr, "We made it all the way to the top with %d\n", x);

  return 0;
}

int
foo (void)
{
  int  y;
  double r;

  y = bar ();

  r = sqrt (y);
  fprintf (stderr, "The squareroot of %d is %f.\n",  y, r);

  return y;
}

int
bar (void)
{
  int z;
  int result = 1;
  int n;
  int i;

  z = baz ();
  n = z % 10;

  for (i = 0; i < n; i++)
    if (i > 0)
      result = result * i;

  fprintf (stderr, "Multiplying the first ten numbers of %d is %d\n", z, result);

  return z;
}

int
baz (void)
{
  char *buffer;
  int len;
  int answer;

  buffer = (char *) malloc (80);
  fprintf (stderr, "Please enter a number: ");
  buffer[0] = '1';
  buffer[1] = '2';
  buffer[2] = '3';
  buffer[3] = '4';
  buffer[4] = '5';
  buffer[5] = '\0';
  /* fgets (buffer, 80, stdin); */
  fprintf (stderr, "buffer is %s\n", buffer);
  answer = atoi (buffer);

  return answer;
}
