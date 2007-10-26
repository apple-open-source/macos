#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <spawn.h>

#if defined (__ppc64__) || defined (__x86_64__)
  char *global_str = "I am 64 bit.\n";
#elif defined (__ppc__) || defined (__i386__)
  char *global_str = "I am 32 bit.\n";
#else
  error "Unsupported architecture for test."
#endif

int
main (int argc, char **argv)
{
  char *in_environ;

  in_environ = getenv ("FROM_GDB");

  printf ("%s", global_str);

  return 0; /* Good place to put a breakpoint */
}
