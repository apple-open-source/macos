/* APPLE LOCAL file 5158236 */
/* While this test is portable to all targets, failure has only been observed on x86_64.  */
#include <stdlib.h>

struct LLM {
  unsigned long location;
  unsigned long length;
  long mpi[0];
};

struct VLLMI {
  long value;
  struct LLM inval;
};

void __attribute__ ((__noinline__))
callee1 (int flag)
{
  if (!flag)
    abort ();
}

void __attribute__ ((__noinline__))
callee2 (long v)
{
  if (v != 42)
    abort ();
}

void __attribute__ ((__noinline__))
testee (struct VLLMI test)
{
  /* This store gets dropped due to an unfortuante SRA/SSA interaction.  */
  struct LLM r2 = test.inval;
  callee1(4 == r2.location) ;
  callee2(test.value) ;
}

int
main ()
{
  struct VLLMI isvt = { 42, {4, 1} };
  testee (isvt);
  exit (0);
}
