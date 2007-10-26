/* APPLE LOCAL file mainline */
/* { dg-do run } */

#include <stdlib.h>
#include <string.h>

template <class T>
struct Vec {
 T x, y;
 int  z;
 long long zz;
};

Vec<double> dd;
const char *enc = @encode(Vec<float>);
const char *enc2 = @encode(Vec<double>);

int main(void) {
  char *encode = @encode(long);

#if __OBJC2__
  if (strcmp (encode, "q"))
#else
  if (strcmp (encode, "l"))
#endif
    abort();

  if (strcmp (enc, "{Vec<float>=ffiq}"))
    abort();

  if (strcmp (enc2, "{Vec<double>=ddiq}"))
    abort();

  return 0;
}
