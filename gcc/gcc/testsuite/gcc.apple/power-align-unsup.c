/* Darwin (Mac OS X) alignment exercises.  */

/* { dg-do compile { target powerpc-*-darwin[89]* } } */
/* { dg-options "-m64 -malign-power" } */

/* { dg-error "-malign-power is not supported for 64-bit Darwin" "" { target *-*-* } 1 } */

main() {}
