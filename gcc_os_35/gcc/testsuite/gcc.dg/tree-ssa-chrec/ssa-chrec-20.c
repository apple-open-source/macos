/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int main ()
{
  int a = 3;
  int b = 2;
  
  while (a)
    {
      b += 5;
      a += b;
      
      /* Exercises the sum of a polynomial of degree 2 with an
	 evolution of degree 1:
	 
	 (loop_num = 1, chrec_var = {3, +, 7, +, 5}, to_add = 2).
	 The result should be:  {3, +, 9, +, 5}.  */
      a += 2;
    }
}

/* 
   b  ->  {2, +, 5}_1
   a  ->  {3, +, {9, +, 5}_1}_1
*/

/* FIXME. */
