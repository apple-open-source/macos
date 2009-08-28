/* APPLE LOCAL file 5782111 */
/* Identifiers containing UCNs should be prefixed with an underscore,
   like other symbols.  */
/* { dg-do compile { target *-*-darwin* } } */
/* { dg-options "-std=c99" } */

int foobar\u00C0;

/* { dg-final { scan-assembler "_foobar" } } */
/* { dg-final { scan-assembler-not "\[^_\]foobar" } } */
