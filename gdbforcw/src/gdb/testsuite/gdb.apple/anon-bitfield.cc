#include <stdint.h>

struct Foo
{
  uint8_t f1 : 2;
  uint8_t    : 6;
  uint8_t f2 : 3;
  uint8_t    : 5;
};

int 
main ()
{
  Foo mine = {1, 5};

  return (int) mine.f2;

}
