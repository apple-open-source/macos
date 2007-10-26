/* APPLE LOCAL file radar 5188592 */
/* { dg-do compile { target powerpc*-*-* } } */
/* { dg-options "-O1" } */
typedef struct ps {
  unsigned long long off;
} ps;

typedef struct pp {
  unsigned long long off;
} pp;
 
typedef struct pu {
  unsigned long long size;
} pu;

static void foo(ps * sc, unsigned long long pos) 
{ 
  if (pos >= sc->off) 
    sc->off = pos; 
}

static int foo2 (pu * src)
{
  return src->size;
}

static int bar (pu* src) 
{
  int err = 0;
  unsigned i, cnt;
  ps s1;
  for (i = 0; i < cnt;) 
  {
    pp p1; 
    foo (&s1, p1.off);
  }
  return err;
}  

int bar2 ()
{
  int err = 0;
  pu src = { 0 };
  pu dst = { 0 };
  while (!err)
  {
    dst.size = 0;
    err = bar (&src);
    if (foo2 (&src) > foo2 (&dst))
      (void)(*(int*) 0 = 0xdeadbeef);
  }
  return err;
} 
