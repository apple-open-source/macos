/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O2 -ftree-loop-optimize -funroll-loops -fdump-tree-optimized" } */

void foo(void)
{
  int n = 16875;

  while (n)
    {
      if (n&1)
	bar (n);
      n >>= 1;
    }
}

static inline int power (long x, unsigned int n)
{
  long y = n % 2 ? x : 1;

  while (n >>= 1)
    {
      x = x * x;
      if (n % 2)
	y = y * x;
    }

  return y;
}

void test(long x)
{
  bar (power (x, 10));
  bar (power (x, 27));
}

/* All loops should be completely unrolled, so there should be no labels.  */
/* { dg-final { scan-tree-dump-times "<L" 0 "optimized"} } */
