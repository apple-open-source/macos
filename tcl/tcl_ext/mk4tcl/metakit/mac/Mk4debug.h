// used as prefix header in debug builds

#define q4_CHECK 1

void f4_AssertionFailed(const char*, const char*, int);
#define d4_assert(x)    if (x) ; else f4_AssertionFailed(# x, __FILE__, __LINE__)

#define DEBUG_NEW 2
#define nothrow nothrow_fake // this avoids a linker warning, yikes...
#include <DebugNew.h>
#define d4_new NEW
