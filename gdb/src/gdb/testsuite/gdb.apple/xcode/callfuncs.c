#include <sys/types.h>
#include <unistd.h>

int foo1 (int);
int foo2 (int);
int foo3 (int);
long foo4 (long);

/* Use getpid () in an attempt to keep this file from being
   optimized away if compiled w/ opt.  */

int
main ()
{
  return foo1 (getpid ());
}

int foo1 (int a)
{
  return foo2 (a + getpid ());
}

int foo2 (int b)
{
  return foo3 (b +  getpid ());
}

int foo3 (int c)
{
  return foo4 (c + getpid ());
}

long foo4 (long d)
{
  return d + getpid ();
}
