/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int main ()
{
  int a = 2;
  int b = 4;
  int c = 2;
  
  while (a)
    {
      a *= 3;
      for (c = -10; c < 0; c++)
	{
	  /* Exercises the build_exponential_evolution_in_loop function in the following context:
	     (multiply_evolution 
	       loop_num = 2
	       chrec_before = {2, *, 3}_1
	       to_mult = {4, *, 5}_1
	       res = {{2, *, 3}_1, *, {4, *, 5}_1}_2
	     )
	     
	     Then it exerces the chrec_apply in the following context:
	     (chrec_apply 
	       var = 2
	       chrec = {0, +, {4, *, 5}_1}_2
	       x = 10
	       res = {40, *, 5}_1
	     )
	     
	     Finally it tests the 
	     (add_to_evolution 
	       loop_num = 1
	       chrec_before = {{2, *, 3}_1, *, {4, *, 5}_1}_2
	       to_add = {40, *, 5}_1
	       res = {{2, *, {120, *, 5}_1}_1, *, {4, *, 5}_1}_2
	     )
	  */
	  a *= b;
	}
      b *= 5;
    }
}

/* 
   c  ->  {-10, +, 1}_2
   b  ->  {4, *, 5}_1
   a  ->  {{2, *, {120, *, 5}_1}_1, *, {4, *, 5}_1}_2
*/

/* FIXME. */
