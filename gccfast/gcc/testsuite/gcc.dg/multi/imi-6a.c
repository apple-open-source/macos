/* { dg-do run onestep imi-6b.c } */
/* { dg-options "-O3" } */

/* An obscure aliasing testcase: In this file, the types of 'x_1' and
   'x_2' and 'x' are not compatible.  But in the other file, they are
   all compatible.  */

extern struct 
{
  int x;
} *x_1;

extern struct 
{
  int x;
} *x_2;

extern struct 
{
  int x;
} x;

void foo(void)
{
  x.x = 4;
  x_2->x = 3;
  if (x.x != 3)
    abort ();
  x_1->x = 1;
  if (x_2->x != 1)
    abort ();
  if ((void *)x_1 != (void *)x_2)
    abort ();
  if ((void *)x_1 != &x)
    abort ();
}
