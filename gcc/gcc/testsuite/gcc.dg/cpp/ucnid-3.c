/* APPLE LOCAL begin mainline UCNs 2005-04-17 3892809 */
/* { dg-do compile } */
/* { dg-options "-std=c99" } */

#define paste(x, y) x ## y

int paste(\u00aa, \u0531) = 3;

/* APPLE LOCAL end mainline UCNs 2005-04-17 3892809 */
