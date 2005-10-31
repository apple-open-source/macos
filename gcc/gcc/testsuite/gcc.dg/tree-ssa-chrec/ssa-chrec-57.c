/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-optimized" } */

void remove_me (void);

int main (void)
{
  int a = -100;
  int b = 0;
  int c = 3;
  
  for (a = 0; a < 100; a++)
    {
      if (b > c)
	remove_me ();
      b = b + 2;
      c = c + 3;
    }
}


/* { dg-final { scan-tree-dump-times "remove_me" 0 "optimized"} } */

