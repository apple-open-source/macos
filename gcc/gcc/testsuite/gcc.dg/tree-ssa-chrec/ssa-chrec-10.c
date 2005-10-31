/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */

void bar (int);

int foo (void)
{
  int a;
  int x;
  int c[100][100];
  
  /* loop_1 runs 39 times.  */
  for (a = 11; a < 50; a++)
    {
      /* Array access functions have to be analyzed.  */
      x = a + 5;
      c[x][a+1] = c[x+2][a+3] + c[x-1][a+2];
    }
  bar (c[1][2]);
}

/* The analyzer has to detect the scalar functions:
   a   ->  {11, +, 1}_1
   x   ->  {16, +, 1}_1
   x+2 ->  {18, +, 1}_1
   x-1 ->  {15, +, 1}_1
*/

/* { dg-final { scan-tree-dump-times "set_nb_iterations_in_loop = 39" 1 "lptest"} } */
