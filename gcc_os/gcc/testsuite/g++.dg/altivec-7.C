/* APPLE LOCAL file AltiVec */
/* { dg-do run { target powerpc-apple-darwin* } } */
/* { dg-options "-faltivec -Wall" } */

#include <stdio.h>

void DumpVUC(vector unsigned char v)
{
}

#ifndef TEST_FUNCTION_TEMPLATE
#define TEST_FUNCTION_TEMPLATE
#endif

#ifdef TEST_FUNCTION_TEMPLATE
template <int I>
#endif
void vectorTest()
{
  typedef vector unsigned char VUC;

  // Multiple initializers with expressions
  const unsigned char kFoo = 0;
  enum              { kBar = 1 };
  VUC v1 = {kFoo,kBar,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
  VUC v2 = {kBar*kFoo,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

  VUC v3 = (VUC)(kFoo,kBar,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
  VUC v4 = (VUC)(kBar*kFoo,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);


  // Single initializers
  VUC v5 = {42};
  VUC v6 = {40+2};

  VUC v7 = (VUC)(42);
  VUC v8 = (VUC)(40+2);

  DumpVUC(v1);  DumpVUC(v2);  DumpVUC(v3);  DumpVUC(v4);
  DumpVUC(v5);  DumpVUC(v6);  DumpVUC(v7);  DumpVUC(v8);
#ifdef TEST_FUNCTION_TEMPLATE
  VUC v9 = (VUC)(I,1,2,I+3,4,5,6,7,8,9,10,11,12,13,14,15);
  DumpVUC(v9);
#endif
}


int main (int argc, char * const argv[])
{
#ifdef TEST_FUNCTION_TEMPLATE
	vectorTest<0>();
#else
	vectorTest();
#endif
    return 0;
}

