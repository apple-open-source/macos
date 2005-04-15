/* APPLE LOCAL file CW asm blocks */
/* Test single line asms */

/* { dg-do compile } */
/* { dg-options "-fasm-blocks" } */

void
bar ()
{
  asm { nop };
}
