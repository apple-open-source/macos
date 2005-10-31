/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-optimized" } */

void remove_me (void);

int main (void)
{
  int a, b;
  int N = 100;
  
  a = 0;
  b = 0; 
  while (a < N)
    {
      if (b >= 5*N - 4)
      	remove_me ();
      a++;
      b+=5;
    }
}

/* { dg-final { scan-tree-dump-times "remove_me" 0 "optimized"} } */
