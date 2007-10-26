/* Test errors for constant strings.  */
/* { dg-do compile } */
/* APPLE LOCAL begin radar 4674757 */
/* { dg-options "-fgnu-runtime -fno-constant-cfstrings" } */
/* { dg-skip-if "" { *-*-darwin* } { "-m64" } { "" } } */
/* APPLE LOCAL end radar 4674757 */


#ifdef __cplusplus
extern void baz(...);
#endif

void foo()
{
  baz(@"hiya");  /* { dg-error "annot find interface declaration" } */
}

@interface NXConstantString
{
  void *isa;
  char *str;
  int len;
}
@end

void bar()
{
  baz(@"howdah");
}
