/* APPLE LOCAL file 4278461 */
/* { dg-do execute { target "i?86-*-darwin*" } } */
/* { dg-options "-fPIC -msse2" } */
/* Radar 4278461, GCC used a pic-base it neglected to set.  */
typedef double __v2df __attribute__ ((__vector_size__ (16)));
double __attribute__ ((noinline))
xsqrt( double x )
{
  __v2df f, g;
  double _d;
  x += x;
  g = __extension__ (__v2df){ x, 0 };
  f = __builtin_ia32_sqrtsd( g );
  _d = __builtin_ia32_vec_ext_v2df (f, 0);
  return (_d);
}

double global_x, global_y;

main ()
{
  global_x = 100.0;
  printf ("", &global_x, &global_y);	/* Frighten the optimizer.  */
  global_y = xsqrt (global_x / 2.0);	/* Compensate for the "x += x;" in xsqrt().  */
  if (global_y != 10.0)
    abort ();
  exit (0);
}
