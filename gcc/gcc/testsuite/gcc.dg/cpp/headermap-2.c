/* APPLE LOCAL file headermaps */
/* Copyright (C) 2005 Free Software Foundation, Inc.  */

/* { dg-do compile } */
/* { dg-options "-I $srcdir/gcc.dg/cpp/headermap.hmap" } */

#define COUNT 1
#include <a.h> /* { dg-warning "mismatched case" } */
#include <Ba.h>  /* { dg-warning "mismatched case" } */
#include <C.h>  /* { dg-warning "mismatched case" } */
#import <c.h>

#if COUNT != 4
  #error COUNT not 4 in headermap-d21
#endif
