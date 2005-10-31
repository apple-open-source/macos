/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details -fdump-tree-optimized" } */

void remove_me (void);

int main(void)
{
  int a;
  int b;
  int c;
  
  /* loop_1 runs 2 times.  */
  for (a = 22; a < 83; a+=1)	/* a  ->  {22, +, 60}_1 */
    {
      c = a;
      
      /* loop_2 runs exactly 6 times.  */
      for (b = 23; b < 50; b+=5) /* b  ->  {23, +, 5}_2 */
	{
	  ++a;
	}
      /* The following stmt exercises the value of B on the exit of the loop.
	 In this case the value of B out of the loop is that of the evolution
	 function of B applied to the number of iterations the inner loop_2 runs.  
	 Value (B) = {23, +, 5}_2 (6) = 53.  */

      /* At this point, the variable A has the evolution function:
	 {{22, +, 6}_1, +, 1}_2.  */
      if (b != 53 
	  || a != c + 6)
	remove_me ();
      
      a = a + b;
      /* At this point, the variable A has the evolution function:
	 {{22, +, 59}_1, +, 1}_2.  The evolution of the variable B in
	 the loop_2 does not matter, and is not recorded in the
	 evolution of A.  The above statement is equivalent to: 
	 "a = a + 53", ie. the scalar value of B on exit of the loop_2. */
      
      if (a != c + 59)
	remove_me ();
      
      /* And finally the a+=1 from the FOR_STMT produces the evolution
	 function: {{22, +, 60}_1, +, 1}_2.  */
    }
}

/* { dg-final { scan-tree-dump-times "set_nb_iterations_in_loop = 2" 1 "lptest"} } */
/* { dg-final { scan-tree-dump-times "set_nb_iterations_in_loop = 6" 1 "lptest"} } */
/* { dg-final { scan-tree-dump-times "remove_me" 0 "optimized"} } */
