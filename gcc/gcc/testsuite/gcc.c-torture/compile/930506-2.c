/* APPLE LOCAL  nested functions */
/* { dg-xfail-if "" { "*-*-darwin*" } { "*" } { "" } } */
#ifndef NO_TRAMPOLINES
int f1()
{
  { int ___() { foo(1); } bar(___); }
  return( { int ___() { foo(2); } bar(___);} );
}

int f2(int j)
{
  { int ___() { foo(j); } bar(___); }
  return( { int ___() { foo(j); } bar(___);} );
}
#else
int x;
#endif
