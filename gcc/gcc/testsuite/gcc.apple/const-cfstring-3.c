/* APPLE LOCAL file constant cfstrings */
/* Test whether the internally generated CFConstantString type
   descriptor agrees with what CoreFoundation actually provides. */
/* Developed by Ziemowit Laski <zlaski@apple.com>.  */
/* { dg-options "-fconstant-cfstrings" } */
/* { dg-do compile { target *-*-darwin* } } */

int __CFConstantStringClassReference[10];
