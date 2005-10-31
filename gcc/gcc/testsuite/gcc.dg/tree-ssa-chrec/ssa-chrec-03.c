/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int main(void)
{
  int a;
  int b;
  int *c;

  /* loop_1 runs exactly 5 times.  */
  for (a = 11; a < 50; a++)
    {
      /* loop_2 runs exactly 7 times.  */
      for (b = 8; b < 50; b+=5)
        {
	  c[a++] = 5;
	  c[b++] = 6;
        }
    }
}

/* The analyzer has to detect the following evolution functions:
   b  ->  {8, +, 6}_2
   a  ->  {{11, +, 8}_1, +, 1}_2
*/
/* { dg-final { scan-tree-dump-times "set_nb_iterations_in_loop = 5" 1 "lptest"} } */
/* { dg-final { scan-tree-dump-times "set_nb_iterations_in_loop = 7" 1 "lptest"} } */

