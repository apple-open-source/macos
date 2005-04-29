/* { dg-do run } */
/* { dg-options "-O2" } */

static int a (int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9, int i10)
{ return i10; }

int b (int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9, int i10)
{
  for(;;)
    if (i1)
      return a (i1, i2, i3, i4, i5, i6, i7, i8, i9, i10 + 1);
}

main() {
  int val = b(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
  if (val != 11)
    {
      printf ("failure: got %d, expected 11\n", val);
      abort() ;
    }
  else
    printf ("O.K.\n");
  exit(0);
}

