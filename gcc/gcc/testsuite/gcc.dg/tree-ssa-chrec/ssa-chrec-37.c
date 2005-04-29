/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int main ()
{
  int a;
  int b = 2;
  int c = 11;
  
  for (a = -123; a < 0; c += 12, b += 5)
    {
      a += b;
      
      /* The next stmt exercises the add_function_to_loop_evolution
	 (loop_num = 1, chrec_before = {-123, +, {2, +, 5}_1}_1, to_add = {11, +, 12}_1).
	 The result should be:  {-123, +, {13, +, 17}_1}_1.  */
      a += c;
    }
}

/* 
   b  ->  {2, +, 5}_1
   c  ->  {11, +, 12}_1
   a  ->  {-123, +, {13, +, 17}_1}_1
*/


/* FIXME. */
