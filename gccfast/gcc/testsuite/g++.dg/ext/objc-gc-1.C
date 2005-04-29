/* A compile-only test for preprocessor macros. */
/* Developed by Ziemowit Laski  <zlaski@apple.com>  */
/* { dg-do preprocess { target *-*-darwin* } } */

#ifndef __strong
#error __strong not defined!
#endif

#if __strong  /* { dg-error "#if with no expression" } */
#error __strong not empty
#endif

