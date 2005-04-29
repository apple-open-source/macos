/* APPLE LOCAL file CW asm blocks */
/* Test whole asm functions.  */

/* { dg-do run } */
/* { dg-options "-fasm-blocks" } */

void abort (void);

asm int
foo (register int argx, register int argy)
{
  add r3, argx, argy
}

#define rsltreg r3

static asm int
foo1 (register int argx, register int argy)
{  register int loc1, loc2;
  nop
    li loc1, 100
    li rsltreg,0
    b @a34
  add rsltreg, argx, argy
@a34
    add rsltreg, rsltreg, loc1
    blr
}

int
main ()
{
  if (foo (22, 23) != 45)
    abort ();
  if (foo1 (1, 2) != 100)
    abort ();
  return 0;
}

