/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int main ()
{
  int b = 2;
  
  while (b)
    {
      /* Exercises the MULT_EXPR.  */
      b = 2*b;
    }
}

/* b  ->  {2, *, 2}_1
*/

/* FIXME. */

