// APPLE LOCAL file mainline 2005-12-27 4431091
// PR c++/25439

template<int> struct A;
template<> int A<0>; // { dg-error "invalid" }
