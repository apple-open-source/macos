/* { dg-do dummy } */
static int foo = 7;
extern int get_foo(void) ;

int main(void)
{
  if (get_foo() != 1)
    abort ();
  if (foo != 7)
    abort ();
  if (get_foo() != 2)
    abort ();
  return 0;
}
