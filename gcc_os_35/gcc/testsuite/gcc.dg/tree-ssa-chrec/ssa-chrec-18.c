/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int bar (void);

int foo (int x)
{
  int a = -100;
  int b = 2;
  
  while (b)
    {
      if (x)
	a += 3;
      else
	a += bar ();
      
      /* Exercises the case when one of the branches of the if-phi-node cannot
	 be determined: [-oo, +oo].  
	 Since the evolution function is too difficult to handle in the expanded 
	 form, we have to keep it in its symbolic form:  "b  ->  {2, +, a_1}_1".  */
      b += a;
    }
}

/* a  ->  {-100, +, [min<t, 3>, max<t, 3>]}_1
   b  ->  {2, +, {[min<t, 3>, max<t, 3>] - 100, +, [min<t, 3>, max<t, 3>]}_1}_1
*/

/* FIXME. */
