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
      a += 5;
    }
}

/* FIXME: This exposes a problem in the representation.  Is it 
   possible to have an exponential and a polynomial together?
   
   The first assignment constructs "a  ->  {2, *, 3}_1",
   while the second adds 5 as a polynomial function.

   The following two representations are not correct:
   "a  ->  {{2, *, 3}_1, +, 5}_1"
   "a  ->  {{2, +, 5}_1, *, 3}_1"
   
   The right solution is:
   "a  ->  {2, *, 3}_1 + {0, +, 5}_1"
   but this exposes yet again the "exp + poly" problem: the representation 
   is not homogen.  Going into a Taylor decomposition could solve this problem.
   
   This is too difficult for the moment, and does not happen often.
*/

/* Do nothing for this testcase.  */

/* FIXME. */

