/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int foo (int ParmN)
{
  int a = 3;
  int b = 2;
  int d = -5;
  
  while (a)
    {
      b += 25;
      a += b;
      
      for (d = -5; d < 0; d++)
	{
	  /* Exercises the build_polynomial_evolution_in_loop in the following context:
	     (add_to_evolution 
	       loop_num = 2
	       chrec_before = {3, +, {27, +, 25}_1}_1
	       to_add = ParmN_15
	       res = {{3, +, {27, +, 25}_1}_1, +, ParmN_15}_2
	     )
	     
	     Then it exercises the add_expr_to_loop_evolution in the following context:
	     (add_to_evolution 
	       loop_num = 1
	       chrec_before = {{3, +, {27, +, 25}_1}_1, +, ParmN_15}_2
	       to_add = ParmN_15 * 5
	       res = {{3, +, {ParmN_15 * 5 + 27, +, 25}_1}_1, +, ParmN_15}_2
	     )
	  */
	  a += ParmN;
	}
    }
}

/* 
   b  ->  {2, +, 25}_1
   d  ->  {-5, +, 1}_2
   a  ->  {{3, +, {ParmN * 5 + 27, +, 25}_1}_1, +, ParmN}_2
*/

/* FIXME. */
