/* APPLE LOCAL file weak types 5954418 */
/* { dg-do compile } */
/* { dg-final { scan-assembler "weak_definition __ZTI1B" } } */
/* { dg-final { scan-assembler "weak_definition __ZTI1A" } } */
/* Radar 5954418 */

#define WEAK __attribute__ ((weak)) 

class WEAK A {
  virtual void foo();
};

class B : public A {
  virtual void foo();
};

void A::foo() { }

void B::foo() { }
