#include <stdlib.h>

struct inner
{
  int inner1;
  int inner2;
};

struct outer
{
  int spacer;
  struct inner inner;
};

struct outer *
make_outer (int one, int two, int spacer)
{
  struct outer *ret_val;
  ret_val = (struct outer *) malloc (sizeof (struct outer));
  ret_val->inner.inner1 = one;
  ret_val->inner.inner2 = two;
  ret_val->spacer = spacer;

  return ret_val;
}

int 
main ()
{
  struct outer *my_outer;

  my_outer = make_outer (1, 2, 3);
  my_outer->inner.inner1  = 10;
  return 0;
}
