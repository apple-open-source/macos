/* APPLE LOCAL file radar 4905666 */
/* { dg-do compile } */
/* { dg-options "-O2 -m64" } */
inline void bar1 (long long *bm, long long bit)
{
  bm[bit/(sizeof (long long)*8)] &=
    ~(1UL << ((bit) % (sizeof (long long)*8)));
}

inline int bar2 (long long *bm, long long len)
{
  long long i;
  for (i = 0;
       i < (len + (sizeof (long long)*8)-1) / (sizeof (long long)*8); i++)
  {
    return (i*(sizeof (long long)*8));
  }
  return -1;
}

long long data[((sizeof (long long)*8)-1) / (sizeof (long long)*8)];

extern "C" long long foo (void)
{
  int index = bar2 (data, 0);
  bar1 (data, index);
  return 0;
}
