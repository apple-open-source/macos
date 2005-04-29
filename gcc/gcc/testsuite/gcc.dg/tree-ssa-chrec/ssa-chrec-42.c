/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int main ()
{
  int a = 1;
  int b = 2;
  int c = 0;
  int d = 5;
  
  while (a) 
    {
      a += b;
      a += d;
      
      b += c;
      c += 1;
      d += 9;
    }
}

/* 
   c  ->  {0, +, 1}_1
   b  ->  {2, +, 0, +, 1}_1
   d  ->  {5, +, 9}_1
   a  ->  {1, +, 7, +, 9, +, 1}_1
*/

/* FIXME. */
