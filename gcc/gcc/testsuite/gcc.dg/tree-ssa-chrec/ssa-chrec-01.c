/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */

int main(void)
{
  unsigned a;
  int b;
  int c;
  
  /* loop_1 runs exactly 4 times. */
  for (a = 22; a < 50; a+=1)
    {
      /* loop_2 runs exactly 6 times.  On exit, the variable B is equal to 53.  */
      for (b = 23; b < 50; b+=5)
	{
	  ++a;

	  /* loop_3 runs {{77, +, -7}_1, +, -1}_2 times.  */
	  for (c = a; c < 100; c++)
	    {
	      
	    }
	}
    }
}

/* The analyzer has to detect the following evolution functions:
   b  ->  {23, +, 5}_2
   a  ->  {{22, +, 7}_1, +, 1}_2
   c  ->  {{{23, +, 7}_1, +, 1}_2, +, 1}_3
*/
/* { dg-final { scan-tree-dump-times "set_nb_iterations_in_loop = 4" 1 "lptest"} } */
/* { dg-final { scan-tree-dump-times "set_nb_iterations_in_loop = 6" 1 "lptest"} } */


