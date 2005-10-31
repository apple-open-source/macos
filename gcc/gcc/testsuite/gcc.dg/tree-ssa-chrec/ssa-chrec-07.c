/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -fdump-tree-optimized" } */

void remove_me (void);

int main(void)
{
  int a = -100;
  int b = 2;
  int d = -1;
  int e = -100;
  
  while (a)
    {
      /* Exercises higher order polynomials.  */
      a = a + b;		/* a  ->  {-100, +, {2, +, 3}_1}_1 */
      b = b + 3;		/* b  ->  {2, +, 3}_1 */

      d = d + 3;		/* d  ->  {-1, +, 3}_1 */
      e = e + d;		/* e  ->  {-100, +, {2, +, 3}_1}_1 */
      
      if (a != e)		/* a  ->  {-98, +, {5, +, 3}_1}_1 */
	remove_me ();
    }
}

/* { dg-final { scan-tree-dump-times "remove_me" 0 "optimized"} } */
