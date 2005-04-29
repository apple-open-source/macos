// APPLE LOCAL file Radar 4054948
#include <stdlib.h>
#include <stddef.h>
#pragma options align=mac68k
#pragma export on
#pragma export off
#pragma options align=reset

struct foo {
  int f1;
  int f2;
  int f3;
  short f4;
  int f5;
  int f6;
};

int main() {
  if (offsetof(struct foo,f4) != 12)
    abort ();
  if (offsetof(struct foo,f5) != 16)
    abort ();
  if (offsetof(struct foo,f6) != 20)
    abort ();
}
  
