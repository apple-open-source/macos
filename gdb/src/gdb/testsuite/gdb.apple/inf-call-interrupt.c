#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

void foo (void);
void bar (void);

void subroutine (int);

void handler (int);


main ()
{
  puts ("Starting up");

  foo ();

  puts ("Waiting to get a signal");

  signal (SIGALRM, handler);
  alarm (1);
  sleep (2);

  puts ("Shutting down");
}

void
foo (void)
{
  puts ("hi in foo");
}

void 
bar (void)
{
  char *nuller = 0;

  puts ("hi in bar");

  *nuller = 'a';      /* try to cause a segfault */
}

void
handler (int sig)
{
  subroutine (sig);
}

void
subroutine (int in)
{
  while (in < 100)
    in++;
}
