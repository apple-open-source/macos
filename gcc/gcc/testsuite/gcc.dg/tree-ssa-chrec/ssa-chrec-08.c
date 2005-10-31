/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -fdump-tree-optimized" } */

void remove_me (void);

int main(void)
{
  int a = -100;
  int b = 2;
  int c = 3;
  int d = -5;
  int e = 3;
  int f = -100;
  
  while (a)
    {
      /* Exercises higher order polynomials.  */
      a = a + b;		/* a  ->  {-100, +, 2, +, 3, +, 4}_1 */
      b = b + c;		/* b  ->  {2, +, 3, +, 4}_1 */
      c = c + 4;		/* c  ->  {3, +, 4}_1 */
      
      d = d + 4;		/* d  ->  {-5, +, 4}_1 */
      e = e + d;		/* e  ->  {3, +, -1, +, 4}_1 */
      f = f + e;		/* f  ->  {-100, +, 2, +, 3, +, 4}_1 */
      
      if (a != f)		/* (a == f)  ->  {-98, +, 5, +, 7, +, 4}_1 */
	remove_me ();
    }
}

/* { dg-final { scan-tree-dump-times "remove_me" 0 "optimized"} } */
