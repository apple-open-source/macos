/* APPLE LOCAL begin mainline UCNs 2005-04-17 3892809 */
/* { dg-do compile } */
/* { dg-options "-std=c99" } */
#define a b(
#define b(x) q
int a\U0000000z );
/* APPLE LOCAL end mainline UCNs 2005-04-17 3892809 */
