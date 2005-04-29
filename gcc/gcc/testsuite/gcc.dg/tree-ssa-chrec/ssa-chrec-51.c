/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */

int 
foo (int j)
{
  int i = 0;
  int temp_var;
  
  while (i < 100)
    {
      /* This exercises the analyzer on strongly connected
	 components: here "i -> temp_var -> i".  */
      temp_var = i + j;
      i = temp_var + 2;
    }
  
  return i;
}

/* FIXME. */

