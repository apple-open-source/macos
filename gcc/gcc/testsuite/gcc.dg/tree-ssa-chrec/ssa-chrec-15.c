/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int main (void)
{
  int a;
  int b;
  int c;
  
  /* Exercises the MINUS_EXPR.  loop_1 runs 50 times.  */
  for (a = 100; a > 50; a--)
    {
      
    }
}

/* The analyzer has to detect the following evolution function:
   a  ->  {100, +, -1}_1
*/

/* { dg-final { scan-tree-dump-times "set_nb_iterations_in_loop = 50" 1 "lptest"} } */

