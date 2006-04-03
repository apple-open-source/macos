#include <stdio.h>
struct outer {
  struct inner {
    int i;
  } mytestvar;
  void foo (struct inner *in);
  int c ;
};

void foo (struct inner *in)
{
   puts ("");
}

main ()
{
  struct outer a;
  a.c = 10; /* good place to put a breakpoint */
}
