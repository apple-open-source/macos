/* { dg-do compile } */
#include "lineno1.h"
int bar (void)
{
  return do_something_2<1>();
}  

void;  // { dg-error "lineno1.C:8:" "correct filename" { target *-*-* } 0 }
// checks that after a template instantiation, the filename is correct

