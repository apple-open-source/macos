/* APPLE LOCAL file 4339762 */
/* { dg-do compile { target powerpc*-*-darwin* } } */
/* { dg-options "-m64" } */
void constructor() {}
void destructor() {}
#pragma CALL_ON_LOAD constructor /* { dg-error "unsupported" } */
#pragma CALL_ON_UNLOAD destructor  /* { dg-error "unsupported" } */

