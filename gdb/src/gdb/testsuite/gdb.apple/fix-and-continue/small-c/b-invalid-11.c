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

  blargity_blarg(5);  /* ERROR: This function does not exist. */
}

bar ()
{
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

int
slurry (char a, int b, double c)
{

  if (global_var == 0)
    static_var = static_bss_int = bss_int = VERSION;
  else
    {
      static_var++;
      static_bss_int++;
      bss_int++;
    }
}
