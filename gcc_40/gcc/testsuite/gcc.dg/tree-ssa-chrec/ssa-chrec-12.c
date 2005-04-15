/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int bar (void);

int foo ()
{
  int a = -100;
  int b = 2;
  int c = 3;
  int d = 4;
  
  while (a)
    {
      a = a + b;
      
      /* Exercises if-phi-nodes.  */
      if (bar ())
	b = b + c;
      
      c = c + d;
    }
}

/* The analyzer has to detect the following evolution functions:
   c  ->  {3, +, 4}_1
   b  ->  {2, +, {[0, 3], +, [0, 4]}_1}_1
   a  ->  {-100, +, {2, +, {[0, 3], +, [0, 4]}_1}_1}_1
*/

/* FIXME. */
