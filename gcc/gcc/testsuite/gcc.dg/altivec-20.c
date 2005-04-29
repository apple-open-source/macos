/* { dg-do compile { target powerpc*-*-* } } */
/* { dg-options "-maltivec" } */

#define vector __attribute__((vector_size(16)))

vector long long vbl;	/* { dg-error "use of AltiVec with 64-bit element size is not allowed; use 'int'" } */

