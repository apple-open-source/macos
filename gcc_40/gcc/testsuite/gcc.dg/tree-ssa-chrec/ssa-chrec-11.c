/* APPLE LOCAL file lno */
/* { dg-do compile } */ 
/* { dg-options "-O1 -floop-test -fdump-tree-lptest-details" } */


int main(void)
{
  int a = -100;
  int b = 2;
  
  int f = 6;
  int g = 7;
  int h = 8;
  
  /* Exercises complex loop exit conditions.  
     FIXME:  This is a strange case where the compiler cc1 and the wrapper gcc 
     don't produce the same representation: 

     (with gcc from command line)
     
     T.1_9 = f_2 | a_1;
     if (T.1_9 == 0)
       {
         goto <UL47e0>;
       }
    
     versus (with cc1 called from gdb):
     
      if (f_2 == 0)
        {
          if (a_1 == 0)
            {
              goto <ULc7e0>;
            }
          else
            {
              (void)0
            }
        }
      else
        {
          (void)0
        };
  */
  while (f || a)
    {
      a += b;
      
      f += g;
      g += h;
    }
}

/* 
   g  ->  {7, +, 8}_1
   f  ->  {6, +, {7, +, 8}_1}_1
   a  ->  {-100, +, 2}_1
*/

/* FIXME. */
