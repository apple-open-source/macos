/* APPLE LOCAL file CW asm blocks */
/* { dg-do compile { target *-*-darwin* } } */
/* { dg-options { -fasm-blocks } } */
/* Radar 4766972 */

int f() { @@ }	/* { dg-error "expected expression before '@' token" } */
