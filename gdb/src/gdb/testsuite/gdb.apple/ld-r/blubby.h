#include "ubby.h"

class MyBlubby
{
 public:
  MyBlubby (int input) 
    {
      value.myValue = input;
    }
  int getValue (void) 
    {
      return value.myValue;
    }
 private:
  struct use_me value;
};

int bazFunc (MyBlubby &);
int barFunc (MyBlubby &);
int fooFunc (MyBlubby &);

#include "blubby2.h"
