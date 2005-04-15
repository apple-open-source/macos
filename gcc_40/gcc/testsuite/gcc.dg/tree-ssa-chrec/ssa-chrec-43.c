/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int main ()
{
  int a = 1;
  int b = 2;
  int c = 0;
  int d = 5;
  int e;
  
  while (a) 
    {
      /* The following statement produces the evolution function:
	 (add_to_evolution 
	   loop_num = 1
	   chrec_before = 1
	   to_add = {{2, +, 0}_1, +, 10}_1
	   res = {{{1, +, 2}_1, +, 0}_1, +, 10}_1
	 )
	 Note that the evolution of B in the inner loop_2 is not
	 relevant to the evolution of A in the loop_1.  */
      a += b; 
      
      /* And finally the following statement produces the expected scev:
	 (add_to_evolution 
	   loop_num = 1
	   chrec_before = {{{1, +, 2}_1, +, 0}_1, +, 10}_1
	   to_add = {5, +, 9}_1
	   res = {{{1, +, 7}_1, +, 9}_1, +, 10}_1
	 )
	 That ends this not so formal proof ("CQFD" in french ;-).  */
      a += d;
      
      for (e = 0; e < 10; e++)
	b += c;
      /* After having analyzed this loop, the overall effect is added to the evolution of b.  
	 This corresponds to the following operation:
	 (add_to_evolution 
	   loop_num = 1
	   chrec_before = {2, +, {0, +, 1}_1}_2
	   to_add = {0, +, 10}_1
	   res = {{{2, +, 0}_1, +, 10}_1, +, {0, +, 1}_1}_2
	 ).
	 Note that the variable c has not yet been updated in the loop, and thus its value 
	 at this version is "{0, +, 1}_1".  Since the loop_2 runs exactly 10 times, the overall
	 effect of the loop is "10 * {0, +, 1}_1": that is the TO_ADD argument.  
      */
      
      c += 1;
      d += 9;
    }
}

/* 
   c  ->  {0, +, 1}_1
   e  ->  {0, +, 1}_2
   b  ->  {{2, +, 0, +, 10}_1, +, {0, +, 1}_1}_2
   d  ->  {5, +, 9}_1
   a  ->  {1, +, 7, +, 9, +, 10}_1
*/

/* FIXME. */
