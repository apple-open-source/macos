/* APPLE LOCAL file 4209166 */
/* Contributed by Devang Patel  <dpatel@apple.com>  */

/* { dg-do compile } */
/* { dg-skip-if "No stabs" { mmix-*-* *-*-aix* *-*-netware* alpha*-*-* hppa*64*-*-* ia64-*-* } { "*" } { "" } } */
/* { dg-options "-gstabs+ -feliminate-unused-debug-symbols" } */

struct blah;
typedef struct blah *Foo;

int myFunc (Foo mine);
Foo giveMeOne (int);

int notherFunc (int input)
{
  Foo thisOne = giveMeOne (input);

  myFunc (thisOne);

  return 5;
}

/* { dg-final { scan-assembler ".stabs.*Foo:t\\(\[0-9\],\[0-9\]\\)\=\\(\[0-9\],\[0-9\]\\)" } } */
