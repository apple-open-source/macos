/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */

int main(void)
{
  int a = 5;
  int b = 6;
  int c = 20;
  
  while (a <= 100)
    {
      int i;
      
      a = b;
      for (i = 0; i <= 12; i++)
	{
	  a++;
	}
      b = b + c;
    }
}

/* This example has been distilled from Pattern1 that cannot be
   handled: "Big steps, small steps" from the ICS'01 paper "Monotonic
   Evolution" by Peng Wu.
   
   The analyzer has to detect the following evolution functions:
   i  ->  {0, +, 1}_2
   b  ->  {6, +, 20}_1
   a  ->  {{6, +, 20}_1, +, 1}_2
*/

/* FIXME. */
