/* APPLE LOCAL file coalescing */
/* Make sure that weak '::new' and '::delete' operators wind
   up in a coalesced section and get called via a stub.  */
/* Author: Ziemowit Laski <zlaski@apple.com>  */
/* { dg-do compile { target powerpc*-*-darwin* } } */

extern "C" void free(void *);

void operator delete(void*) throw() __attribute__((weak));
void operator delete(void* p) throw() { free(p); }

void operator delete(void*, int) throw() __attribute__((weak));

void *operator new(unsigned long) throw() __attribute__((weak));
void *operator new(unsigned long) throw() { return (void *)0; }

int *foo(void) {
  int *n = new int();
  ::operator delete(n, 0);
  ::operator delete(n);
  return 0;
}

/* { dg-final { scan-assembler "coal" } } */

/* { dg-final { scan-assembler "bl L__Znwm.stub" } } */
/* { dg-final { scan-assembler "bl L__ZdlPvi.stub" } } */
/* { dg-final { scan-assembler "bl L__ZdlPv.stub" } } */
