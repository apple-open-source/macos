#include <stdio.h>
#include <math.h>

/*  This version of b-generic adds a static function, which should be allowed.*/

static char *static1 = "moo town fun";
static char *static2 = "hi there";
static char *static3 = "I love static data";
static char *static4 = "but not global data";
static char *static5 = "especially when picbase regs are needed to refer to it";
static char *static6 = "this should upset things a bit eh";

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
  puts (static1);  // static1 from b-valid-4.c, not this file's static1!
  puts (static5);
  puts (static6);
}

flart ()
{
  newly_added_integer++;  /* reference the var so debug info is emitted */
}
