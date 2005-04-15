/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int bar (void);

int foo ()
{
  int c = 7;
  
  /* This exercises the initial condition propagator: 
     Interval Copy Constant Propagation (ICCP).  */
  if (bar ())
    c = 2;
  else
    c += 3;
  
  while (c)
    {
      c += 5;
    }
}

/* 
   c  ->  {[2, 10], +, 5}_1
*/

/* FIXME. */
