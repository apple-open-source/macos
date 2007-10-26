/* APPLE LOCAL file mainline */
/* Test to make sure that indirect jumps compile.  */
/* APPLE LOCAL 4461050 */
/* { dg-options "-mno-att-stubs" { target i?86-*-* } } */
extern void bar(void);
void foo() { bar(); }
