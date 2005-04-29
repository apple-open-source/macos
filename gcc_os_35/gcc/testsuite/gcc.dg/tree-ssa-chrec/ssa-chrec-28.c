/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int bar (void);

int foo ()
{
  int i;
  int a = 2;
  
  while (a)
    {
      a *= 3;
      
      for (i = 0; i < 100; i++)
	a += 4;
    }
}

/* FIXME:  We have to transform the evolution function of "a" into an infinite 
   sum, a  ->  {//2, *, 2//}, and then to add the 400 from the inner sum...  
   But this is quite difficult, and cases like this one do not happen often.
   
   (Francois Irigoin consider that this case falls into the 0.01 percent 
   rule, and it is no worth to implement a solution for this testcase in a 
   production compiler. )
*/

/* Do nothing for this testcase.  
   The following evolutions are detected:
   
   i  ->  {0, +, 1}_2
   a  ->  {{2, *, [-oo, +oo]}_1, +, 4}_2
   
*/

/* FIXME. */

