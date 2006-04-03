/* APPLE LOCAL file 4168392 */
/* Support for Microsoft-style anonymous union and
   struct aggregation.  */

/* { dg-options "-fms-extensions -pedantic" } */
/* { dg-do run } */

#include <stdlib.h>
#include <stddef.h>
#define CHECK_IF(E) if(!(E)) abort()

typedef struct _FOO {
  int a;
  int b;
} FOO;

struct BAZ {
  int c;
};

typedef struct _BAR {
  int bar;
  struct {
    BAZ; /* { dg-warning "ISO C\\+\\+ prohibits anonymous structs" } */
    BAZ baz;
  }; /* { dg-warning "ISO C\\+\\+ prohibits anonymous structs" } */
  union {
    int e;
    FOO; /* { dg-warning "ISO C\\+\\+ prohibits anonymous structs" } */
    FOO foo;
  };
} BAR;

BAR g;

int main(void) {
  CHECK_IF (sizeof (g) == 5 * sizeof (int));

  g.e = 4;
  g.c = 5;
  g.baz.c = 6;
  g.foo.b = 7;
  CHECK_IF (g.b == 7);
  CHECK_IF (g.a == 4);
  CHECK_IF (g.foo.a == 4);

  CHECK_IF (offsetof(BAR, a) == offsetof(BAR, e));
  CHECK_IF (offsetof(BAR, a) == offsetof(BAR, foo));
  CHECK_IF (offsetof(BAR, e) - offsetof(BAR, c) >= 2 * sizeof(BAZ));

  return 0;
}   
