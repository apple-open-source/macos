// APPLE LOCAL file mainline 2006-01-22 4416452
// PR c++/25858

namespace N {
  template<int> struct A {};
}

struct B N::A<0> {}; // { dg-error "invalid" }
