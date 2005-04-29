/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */

int 
foo (int unknown_parm, int a, int b)
{
  int p;
  
  if (unknown_parm)
    {
      p = a + 2;
    }
  else
    {
      p = b + 1;
    }
  
  /* At this point the initial condition of "p" is unknown.  
     In this case, the analyzer has to keep the initial condition under a symbolic form.  */
  
  while (p)
    p--;
  
}

/* 
   p  ->  {p_1, +, -1}_1  
   
   or, when the Value Range Propagation does its work:
   
   p  ->  {[MIN_EXPR <p_4, p_6>, MAX_EXPR <p_4, p_6>], +, -1}_1
   
*/

/* FIXME. */
