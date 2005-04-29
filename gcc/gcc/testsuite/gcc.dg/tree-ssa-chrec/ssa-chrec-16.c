/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int main (void)
{
  int a = -100;
  int b = 2;
  int c = 3;
  int d = 4;
  
  /* Determining the number of iterations for the != or == is work in
     progress.  Same for polynomials of degree >= 2, where we have to
     find the zeros of the polynomial.  */
  while (d)
    {
      a += 23;
      d = a + d;
    }
}

/* a  ->  {-100, +, 23}_1
   d  ->  {4, +, {-77, +, 23}_1}_1
*/

/* FIXME. */
