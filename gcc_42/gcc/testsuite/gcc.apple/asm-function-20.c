/* APPLE LOCAL file CW asm blocks */
/* { dg-do assemble { target powerpc*-*-* } } */
/* { dg-options { -fasm-blocks -O3 -Winline -Wall } } */
/* Radar 4381918 */

/* This should have no prologue/epilogue */
inline int numberFive (void);
asm int numberFive (void)
{					/* { dg-warning "can never be inlined" } */
	  				/* { dg-warning "inlining failed in call" "" { target *-*-* } 9 } */
    li r3, 1
}

void foo() {
  numberFive ();			// { dg-warning "called from here" }
}
