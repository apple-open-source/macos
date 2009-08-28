#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

double func_over_signal_thrower (double, float, const char *);
int signal_thrower (int);
void myhandler (int);

main (int argc, char **argv, char **envp)
{
  int ret;
  char *sret;

  ret = func_over_signal_thrower (argc * 5.2, argc * 9999.33, argv[0]);
  printf ("%d\n", ret);

}

/* Throw a signal while down on the stack, make sure we can find 
   our way through _sigtramp() and get all the necessary information
   out of the sigcontext struct to backtrace our way out. */

double func_over_signal_thrower (double d, float f, const char *s)
{
  int c = d * f;
  c += s[0];
  f = f * c;
  c -= signal_thrower (c);
  return (d * f) / c;
}

int signal_thrower (int in)
{
  in--;
  signal (SIGALRM, myhandler);
  alarm (1);
  sleep (2);
  in = in - 2;
  return in;
}

void myhandler (int sig)
{
  signal (sig, myhandler);
}
