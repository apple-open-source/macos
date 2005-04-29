/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int main(void)
{
  int a;
  int b;
  int c;

  /* nb_iterations 28 */
  for (a = 22; a < 50; a++)
    {
      /* nb_iterations 6 */
      for (b = 23; b < 50; b+=5)
	{
	  /* nb_iterations {78, +, -1}_1 */
	  for (c = a; c < 100; c++)
	    {
	      
	    }
	}
    }
}

/* The analyzer has to detect the following evolution functions:
   a  ->  {22, +, 1}_1
   b  ->  {23, +, 5}_2
   c  ->  {{22, +, 1}_1, +, 1}_3
*/
/* { dg-final { scan-tree-dump-times "set_nb_iterations_in_loop = 28" 1 "lptest"} } */
/* { dg-final { scan-tree-dump-times "set_nb_iterations_in_loop = 6" 1 "lptest"} } */
