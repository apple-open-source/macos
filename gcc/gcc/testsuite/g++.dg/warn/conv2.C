// PR c++/13932
// APPLE LOCAL mainline
// { dg-options "-Wconversion" }

int i = 1.; // { dg-warning "converting" }
