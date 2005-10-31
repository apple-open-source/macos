/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details -fdump-tree-optimized" } */

void remove_me (void);

int main(void)
{
  int a;
  int b = 22;

  /* loop_1 runs exactly 28 times.  */
  for (a = 22; a < 50; a++)	/* a -> {22, +, 1}_1 */
    {
      if (a > b)		/* This condition is always false. */
	remove_me ();
      b = b + 2; 		/* b -> {22, +, 2}_1 */
    }
}

/* { dg-final { scan-tree-dump-times "set_nb_iterations_in_loop = 28" 1 "lptest"} } */
/* { dg-final { scan-tree-dump-times "remove_me" 0 "optimized"} } */
