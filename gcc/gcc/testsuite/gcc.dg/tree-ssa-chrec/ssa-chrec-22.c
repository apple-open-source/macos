/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int main ()
{
  int a = 2;
  int b = 4;
  
  while (a)
    {
      a *= 3;
      a *= b;
      b *= 5;
    }
}

/* 
   b  ->  {4, *, 5}_1
   a  ->  {2, *, {12, *, 5}_1}_1
*/

/* FIXME. */
