#ifndef _ULIMIT_H
#define _ULIMIT_H

#define UL_GETFSIZE 1
#define UL_SETFSIZE 2

#include <sys/cdefs.h>

__BEGIN_DECLS
long int ulimit(int, ...);
__END_DECLS

#endif /* _ULIMIT_H */
