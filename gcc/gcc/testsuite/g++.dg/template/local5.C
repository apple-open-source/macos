/* APPLE LOCAL file mainline 4.1 2005-06-17 4122333 */
struct Attribute { };

template <class T> bool operator == (const Attribute &attr, const T &value);

enum {
  anon = 123
};

void test(int foo)
{
  if (foo == anon) ;  /* { dg-bogus "anonymous type" } */
}
