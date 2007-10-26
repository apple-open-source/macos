#include <stdio.h>

struct foo
{
  int a;
  union
  {
    int b;
    double c;
  };
  struct
  {
    int d;
    double e; 
  };
};

int main ()
{
  struct foo mine = {1, 2, 3, 4.0};
  struct foo *nother = &mine;

  printf ("Hello World %d %d\n", mine.a, mine.d);
  return 0;
}
