/* This file is really gdb.base/break.exp without some of the cruft. */

#  include <stdio.h>
#  include <stdlib.h>

int marker1 (void) { return (0); }
int marker2 (int a) { return (1); }
void marker3 (char *a, char *b) {}

char marker6 (char f)    /* line # 10 */
{
  return ((f + 13) & 0x7f);
}

void marker5 (int e)    /* line # 15 */
 {
    marker6 ((char) e & 0x7f);
 }

void marker4 (long d)   /* line # 20 */
 { 
    marker5 ((int) d / 2);
 }

/*
 *	This simple classical example of recursion is useful for
 *	testing stack backtraces and such.
 */

int factorial(int);

int
main (int argc, char **argv, char **envp)
{
    if (argc == 12345) {  /* an unlikely value < 2^16, in case uninited */
	fprintf (stderr, "usage:  factorial <number>\n");
	return 1;
    }
    printf ("%d\n", factorial (atoi ("6")));  /* line # 39 */

    marker1 ();
    marker2 (43);
    marker3 ("stack", "trace");
    marker4 (177601976L);                     /* line # 44 */

    argc = (argc == 12345); /* This is silly, but we can step off of it */
    return argc;
}

int factorial (int value)
{
    if (value > 1) {                          /* line # 52 */
	value *= factorial (value - 1);
    }
    return (value);
}

int multi_line_if_conditional (int a, int b, int c)
{
  if (a
      && b
      && c)
    return 0;
  else
    return 1;
}

int multi_line_while_conditional (int a, int b, int c)
{
  while (a
      && b
      && c)
    {
      a--, b--, c--;
    }
  return 0;
}
