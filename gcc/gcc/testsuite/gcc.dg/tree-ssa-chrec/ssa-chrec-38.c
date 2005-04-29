/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int main ()
{
  int a = 3;
  int b = 2;
  int c = 11;
  int d = -5;
  
  while (a)
    {
      b += 5;
      a += b;

      for (d = -5; d < 0; d++)
	{
	  /* Exercises the build_polynomial_evolution_in_loop function in the following context:
	     (add_to_evolution 
	       loop_num = 2
	       chrec_before = {3, +, 7, +, 5}_1
	       to_add = {11, +, 12}_1
	       res = {{3, +, 7, +, 5}_1, +, {11, +, 12}_1}_2
	     )
	     
	     This also exercises the chrec_apply function in the following context:
	     (chrec_apply 
	       var = 2
	       chrec = {0, +, {11, +, 12}_1}_2
	       x = 5
	       res = {55, +, 60}_1
	     )
	  */
	  a += c;
	}
      c += 12;
    }
}

/* 
   b  ->  {2, +, 5}_1
   c  ->  {11, +, 12}_1
   d  ->  {-5, +, 1}_2
   a  ->  {{3, +, 62, +, 65}_1, +, {11, +, 12}_1}_2
*/

/* FIXME. */
