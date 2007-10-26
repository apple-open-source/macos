/* APPLE LOCAL file mainline 2006-11-03 4815483 */
/* { dg-options "-O2" } */
/* { dg-do run } */

struct S { int i; } s;

void f (struct S *q)
{
  int a, *p;
  p = &a;
  if (q)
    p = &q->i;
  *p = 1;
}

void abort (void);

int main (void)
{
  f (&s);
  if (s.i != 1)
    abort ();
  return 0;
}
