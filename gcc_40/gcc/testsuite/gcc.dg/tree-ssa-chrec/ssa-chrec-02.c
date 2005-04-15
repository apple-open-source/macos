/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */

int main(void)
{
  int a;
  int b;
  int *c;

  /* The following loop runs exactly 3 times.  */
  for (a = 11; a < 50; a++)
    {
      /* The following loop runs exactly 9 times.  */
      for (b = 8; b < 50; b+=5)
        {
	  c[a + 5] = 5;
	  c[b] = 6;
	  a+=2;
        }
    }
}

/* The analyzer has to detect the following evolution functions:
   b  ->  {8, +, 5}_2
   a  ->  {{11, +, 19}_1, +, 2}_2
*/
/* { dg-final { scan-tree-dump-times "set_nb_iterations_in_loop = 3" 1 "lptest"} } */
/* { dg-final { scan-tree-dump-times "set_nb_iterations_in_loop = 9" 1 "lptest"} } */
