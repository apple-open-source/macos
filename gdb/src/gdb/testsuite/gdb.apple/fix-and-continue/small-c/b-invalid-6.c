#include <stdio.h>
#include <math.h>

#ifndef VERSION
#define VERSION 1
#endif

int global_var = (VERSION * 10);
static int static_var = 5;
static int static_bss_int;
int bss_int;

foo ()
{
  printf ("Hi, I am in foo, version %d.\n", VERSION);
  baz ();
}


bar (int c) /* ERROR: Argument added to function while active on stack */
{
  c = 5;  /* for -gused to emit it */
  printf ("I am in bar in b.c version %d\n", VERSION);

  global_var++;

  fred ();
  foo ();
}

baz ()
{
  puts ("I am baz.");
  fred ();
}
