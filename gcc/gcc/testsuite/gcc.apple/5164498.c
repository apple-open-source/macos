/* APPLE LOCAL file radar 5164498 */
/* { dg-do run { target i?86-*-* } } */
/* { dg-skip-if "" { i?86-*-darwin* } { "-m64" } { "" } } */
/* { dg-options "-O2 -fno-inline" } */

extern void abort (void);

#define __dynamicstackalign __attribute__ ((__force_align_arg_pointer__))
#define WINAPI __dynamicstackalign

static int foo (int a, int b, int c, int d, int e)
{
   if (a != 1 || b != 2 || c != 3 || d != 4 || e != 5)
     abort ();
   return b+c;
}

int WINAPI bar (int a, int b, int c, int d)
{
   return foo (a, b, c, d, 5);
}


int main ()
{
   int ret;
   ret = bar (1, 2, 3, 4);
   return 0;
}

