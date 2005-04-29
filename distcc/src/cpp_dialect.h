#if !defined(__cpp_dialect_h__)
#define __cpp_dialect_h__

#include <stdlib.h> // NULL

extern int   seen_opt_x;
extern char *opt_x_ext;

extern char *dialect_lookup(char *key); // may return NULL
extern char *ext_lookup(char *key);     // may return NULL

#endif
