/* APPLE LOCAL file non lvalue assign */
/* { dg-do compile } */
/* { dg-options "-fnon-lvalue-assign -faltivec" } */

int foo(void) {

  char *p;
  long l;
  short s;
  vector unsigned int vui;
  volatile int *pvi;

  (long *)p = &l; /* { dg-warning "target of assignment not really an lvalue" } */
  ((long *)p)++;  /* { dg-warning "target of assignment not really an lvalue" } */
  (short)l = 2;   /* { dg-error "invalid lvalue" } */
  (long)s = 3;    /* { dg-error "invalid lvalue" } */
  (int)pvi = 4;   /* { dg-warning "target of assignment not really an lvalue" } */
  (int)pvi &= 5;  /* { dg-warning "target of assignment not really an lvalue" } */

  (vector float)vui = (vector float)(1.0, 2.0, 3.0, 4.0); /* { dg-warning "target of assignment not really an lvalue" } */

  return 0;
}
