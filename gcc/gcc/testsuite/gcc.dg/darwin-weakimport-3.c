/* APPLE LOCAL file mainline 4.2 2005-12-05 4290187 */
/* { dg-do compile { target *-*-darwin* } } */
/* { dg-require-weak "" } */

/* { dg-final { scan-assembler-not "coalesced" } } */

extern void foo(void) __attribute__((weak_import));

void foo(void)
{
}
