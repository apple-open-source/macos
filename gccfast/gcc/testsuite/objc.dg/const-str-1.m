/* Test errors for constant strings.  */
/* { dg-do compile } */
/* APPLE LOCAL constant cfstrings */
/* { dg-options "-fno-constant-cfstrings -fgnu-runtime" } */

/* APPLE LOCAL begin Objective-C++ */
#ifdef __cplusplus
extern void baz(...);
#endif
/* APPLE LOCAL end Objective-C++ */

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
