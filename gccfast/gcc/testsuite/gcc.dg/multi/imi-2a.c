/* { dg-do compile onestep imi-2b.c } */
struct bar
{
  int x;
};

int getx (struct bar *y)  /* { dg-error "previous declaration" } */
{
  return y->x;
}
