/* $Id: strlcpy.h,v 1.1.1.1 2001/02/25 20:54:31 zarzycki Exp $ */

#ifndef _BSD_STRLCPY_H
#define _BSD_STRLCPY_H

#include "config.h"
#ifndef HAVE_STRLCPY
#include <sys/types.h>
size_t strlcpy(char *dst, const char *src, size_t siz);
#endif /* !HAVE_STRLCPY */

#endif /* _BSD_STRLCPY_H */
