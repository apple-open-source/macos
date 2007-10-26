// APPLE LOCAL file mainline 2005-12-19 4407995
// PR c++/24915

struct A
{
  template<int> void foo() {}
  template<int> int foo() {}
};
