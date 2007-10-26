#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

void foo (void);
void bar (void);

void superroutine1 (int);
void superroutine2 (int);

void subroutine1 (int);
void subroutine2 (int);
void subroutine3 (int);
void subroutine4 (int);
void subroutine5 (int);
void subroutine6 (int);

void handler (int);


main ()
{
  int c = 0;
  puts ("Starting up");

  foo ();

  puts ("Waiting to get a signal");

  superroutine1 (10);

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
  subroutine1 (sig);
}

void
superroutine1 (int in)
{
  superroutine2 (in + 5);
}

void
superroutine2 (int in)
{
  signal (SIGALRM, handler);
  alarm (1);
  while (1)
    in++;
}

void
subroutine1 (int in)
{
  subroutine2 (in + 5);
}

void
subroutine2 (int in)
{
  subroutine3 (in + 5);
}

void
subroutine3 (int in)
{
  subroutine4 (in + 5);
}

void
subroutine4 (int in)
{
  subroutine5 (in + 5);
}

void
subroutine5 (int in)
{
  subroutine6 (in + 5);
}

void
subroutine6 (int in)
{
  int c = 10;
  in += c;              /* This is a good place for a breakpoint */
  while (in < 1000)
    in++;
}
