/* Test errors for constant strings.  */
/* { dg-do compile } */
/* { dg-options "-fgnu-runtime" } */

void foo()
{
  baz(@"hiya");  /* { dg-error "annot find interface declaration" } */
}

@interface NXConstantString
/* APPLE LOCAL begin constant strings */
{
  void *isa;
  char *str;
  int len;
}
/* APPLE LOCAL end constant strings */
@end

void bar()
{
  baz(@"howdah");
}
