// APPLE LOCAL file mainline 2006-01-22 4416452
// PR c++/25856

struct A; // { dg-error "forward" } 
A::~A() {} // { dg-error "undefined" }
