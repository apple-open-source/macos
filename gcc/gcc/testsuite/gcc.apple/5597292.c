/* APPLE LOCAL file 5597292 */
/* { dg-do compile } */
/* { dg-options "-static -O0 -gstabs+" } */
void * foo(unsigned int size)
{
  union {
    char _m[size];
  } *mem;
}
