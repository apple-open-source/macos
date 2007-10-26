#include <iostream>
#include "blubby.h"

int main ()
{
  class MyBlubby mine(5);
  int retValue;

  retValue = fooFunc (mine);
  retValue += barFunc (mine);
  retValue += bazFunc (mine);

  std::cout << "Return is " << retValue << "\n";

  retValue = fooFunc2 (mine);
  retValue += barFunc2 (mine);
  retValue += bazFunc2 (mine);

  std::cout << "Return is " << retValue << "\n";

  return 0;
}
