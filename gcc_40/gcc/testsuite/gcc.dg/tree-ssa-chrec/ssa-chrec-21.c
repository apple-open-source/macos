/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int main ()
{
  int a = 3;
  int b = 2;
  
  while (b)
    {
      a *= 4;
      b *= a;
    }
}

/* a  ->  {3, *, 4}_1
   b  ->  {{2, *, 12}_1, *, 4}_1
*/

/* FIXME. */
