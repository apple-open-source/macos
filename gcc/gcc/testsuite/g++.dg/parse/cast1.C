// PR c++/13536
// { dg-options "-w" }

#include <typeinfo>

void f() {
  (long((char*)0));
  sizeof ((long((char*)0)));
  typeid ((long((char*)0)));
}
