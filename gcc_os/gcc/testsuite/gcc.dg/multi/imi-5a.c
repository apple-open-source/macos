/* { dg-do compile onestep imi-5b.c } */
extern int bar(void);

int bar(void) /* { dg-error "previous declaration" } */
{
  return 1;
}

