/* APPLE LOCAL file -Wlong-double */

/* We warn by default, so no specific option needed.  */

/* { dg-do compile { target "*-*-darwin*" } } */
/* { dg-options "" } */

long double ld;  /* { dg-warning "use of `long double' type" } */
/* { dg-warning "is reported only once" "" { target *-*-* } 8 } */
/* { dg-warning "disable this warning" "" { target *-*-* } 8 } */

