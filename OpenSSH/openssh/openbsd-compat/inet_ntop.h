/* $Id: inet_ntop.h,v 1.1.1.1 2001/05/03 16:51:29 zarzycki Exp $ */

#ifndef _BSD_RRESVPORT_H
#define _BSD_RRESVPORT_H

#include "config.h"

#ifndef HAVE_INET_NTOP
const char *                 
inet_ntop(int af, const void *src, char *dst, size_t size);
#endif /* !HAVE_INET_NTOP */

#endif /* _BSD_RRESVPORT_H */
