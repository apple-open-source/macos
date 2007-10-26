// APPLE LOCAL file mainline 4.2 2006-03-01 4311680
// { dg-do compile { target arm*-*-symbianelf* } }
// Class data should not be exported.
// { dg-final { scan-hidden "_ZTI1A" } }
// { dg-final { scan-hidden "_ZTS1A" } }
// { dg-final { scan-hidden "_ZTV1B" } }
// { dg-final { scan-hidden "_ZTI1B" } }
// { dg-final { scan-hidden "_ZTS1B" } }

struct A {};
struct B : virtual public A {};
B b;
