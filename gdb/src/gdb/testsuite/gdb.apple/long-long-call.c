#include <stdio.h>

long long 
blah (long long *longlongptr)
{
  return *longlongptr * 5;
}

long long 
foo (int first, long long second, int third, long long fourth, int fifth, long long sixth, int seventh,
     long long eighth, int nineth, double tenth)
{
  int blahval, secondblahval;

  blahval = blah (&second);
  secondblahval = blah (&fourth) * blah (&sixth);
  

  printf ("first 0x%x, second 0x%llx, third 0x%x, fourth 0x%llx, fifth 0x%x, sixth 0x%llx, seventh 0x%x"
	  " eighth 0x%llx, nineth 0x%x, tenth %f\n",
	  first, second, third, fourth, fifth, sixth, seventh, eighth, nineth, tenth);

  return blahval * secondblahval;

}

int main ()
{
  long long blubby;

  blubby = foo (6, 0x8000000000LL, 7, 0xf000000000LL, 8, 0xe000000000LL, 9, 0xe100000000LL, 10, 10.1);

  return 0;
}
