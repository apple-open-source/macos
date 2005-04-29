/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details  " } */

int foo (int);

int main ()
{
  int res;
  int c[100][200];
  int a;
  int x;
  
  for (a = 1; a < 50; a++)
    {
      c[a+1][a] = 2;
      res += c[a][a];
      
      /* This case exercises the subscript coupling detection: the dependence
	 detectors have to determine that there is no dependence between 
	 c[a+1][a] and c[a][a].  */
    }
  
  return res + foo (c[12][13]);
}

/* { dg-final { diff-tree-dumps "ddall" } } */

