/* $Id: basename.h,v 1.1.1.1 2003/04/01 04:02:24 zarzycki Exp $ */

#ifndef _BASENAME_H 
#define _BASENAME_H
#include "config.h"

#if !defined(HAVE_BASENAME)

char *basename(const char *path);

#endif /* !defined(HAVE_BASENAME) */
#endif /* _BASENAME_H */
