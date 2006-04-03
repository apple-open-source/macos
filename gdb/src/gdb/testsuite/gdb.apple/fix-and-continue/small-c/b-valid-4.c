#include <stdio.h>
#include <math.h>

/*  This version of b-generic adds a static function, which should be allowed.*/

static char *static1 = "hi there";
static char *static2 = "I love static data";
static char *static3 = "especially when picbase regs are needed to refer to it";

static void testfunc (void) { }

#ifndef VERSION
#define VERSION 1
#endif

int global_var = (VERSION * 10);
static int static_var = 5;
static int static_bss_int;
int bss_int;

int newly_added_integer;

foo ()
{
  printf ("Hi, I am in foo, version %d.\n", VERSION);
  baz ();
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

static_printer ()
{
  puts (static1);
  puts (static2);
  puts (static3);
}

flart ()
{
  newly_added_integer++;  /* reference the var so debug info is emitted */
}

int
slurry (char a, int b, double c)
{

  static_printer ();

}
