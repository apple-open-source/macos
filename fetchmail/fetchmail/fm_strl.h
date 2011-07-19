#ifndef FM_STRL_H
#define FM_STRL_H

#include "config.h"

/* strlcpy/strlcat prototypes */
#ifndef HAVE_STRLCAT
size_t
strlcat(char *dst, const char *src, size_t siz);
#endif
#ifndef HAVE_STRLCPY
size_t
strlcpy(char *dst, const char *src, size_t siz);
#endif

#endif
