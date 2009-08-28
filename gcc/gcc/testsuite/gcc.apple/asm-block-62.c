/* APPLE LOCAL file CW asm blocks */
/* { dg-do compile { target *-*-darwin* } } */
/* { dg-options { -fasm-blocks } } */
/* Radar 4197305 */

#if 1
@@		/* { dg-error "expected identifier or '\\(' before '@' token" } */
#endif
