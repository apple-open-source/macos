/* { dg-do run onestep imi-1b.c } */
static int foo = 1;
int get_foo(void) 
{
  return foo++;
}
