// APPLE LOCAL file mainline 4.2 2006-03-01 4311680
// { dg-do compile { target arm*-*-*eabi* } }
// { dg-options "-fvisibility=hidden" }
// Class data should be exported.
// { dg-final { scan-not-hidden "_ZTI1A" } }
// { dg-final { scan-not-hidden "_ZTS1A" } }
// { dg-final { scan-not-hidden "_ZTV1B" } }
// { dg-final { scan-not-hidden "_ZTI1B" } }
// { dg-final { scan-not-hidden "_ZTS1B" } }

struct A {};
struct B : virtual public A {};
B b;
