#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

int foo1 (int);
int foo2 (int);
int foo3 (int);
long foo4 (long);
int foo5 (int);
int foo6 (int);
int foo7 (int);
int foo8 (int);

int sigfoo1 (int);
int sigfoo2 (int);
int sigfoo3 (int);
long sigfoo4 (long);
int sigfoo5 (int);
int sigfoo6 (int);
int sigfoo7 (int);
int sigfoo8 (int);

/* Use getpid () in an attempt to keep this file from being
   optimized away if compiled w/ opt.  */

int
main ()
{
  int rslt;

  rslt = foo1 (getpid ());

  rslt += sigfoo1 (getpid ());

  return rslt;
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
  return foo5 (d + getpid ());
}

int foo5 (int d5)
{
  return foo6 (d5 + getpid ());
}

int foo6 (int d6)
{
  return foo7 (d6 + getpid ());
}

int foo7 (int d7)
{
  return foo8 (d7 + getpid ());
}

int foo8 (int d8)
{
  return d8 + getpid ();
}

static int count = 0;

static void
handler (int sig)
{
  signal (sig, handler);
  ++count;
  sigfoo4 (getpid ());
}

static void
func1 ()
{
  ++count;
}

static void
func2 ()
{
  ++count;
}

int sigfoo1 (int a)
{
  return sigfoo2 (a + getpid ());
}

int sigfoo2 (int b)
{
  return sigfoo3 (b +  getpid ());
}

int sigfoo3 (int c)
{
  signal (SIGALRM, handler);
  alarm (1);
  ++count; /* first */
  alarm (1);
  ++count; /* second */
  func1 ();
  alarm (1);
  func2 ();
  sleep(2);
}

long sigfoo4 (long d)
{
  return sigfoo5 (d + getpid ());
}

int sigfoo5 (int d5)
{
  return sigfoo6 (d5 + getpid ());
}

int sigfoo6 (int d6)
{
  return sigfoo7 (d6 + getpid ());
}

int sigfoo7 (int d7)
{
  return sigfoo8 (d7 + getpid ());
}

int sigfoo8 (int d8)
{
  return d8 + getpid ();
}
