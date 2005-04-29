/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int bar (void);

void foo ()
{
  int a = -100;
  int b = 2;
  
  while (b)
    {
      if (bar ())
	a += 3;
      else
	a = 2;
      
      /* Exercises the case when one of the branches of the if-phi-node is a constant.
	 FIXME:  
	 - What is the chrec representation of such an evolution?  
	 - Does this kind of code exist in real codes?  */
      b += a;
    }
}

/* For the moment the analyzer is expected to output a "don't know" answer, 
   both for the initial condition and for the evolution part.  This is done 
   in the merge condition branches information.  
   
   a  ->  [-oo, +oo]
   b  ->  {2, +, a_1}_1
*/

/* FIXME. */
