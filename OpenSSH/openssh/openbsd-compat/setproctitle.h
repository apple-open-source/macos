/* $Id: setproctitle.h,v 1.1.1.1 2001/02/25 20:54:30 zarzycki Exp $ */

#ifndef _BSD_SETPROCTITLE_H
#define _BSD_SETPROCTITLE_H

#include "config.h"

#ifndef HAVE_SETPROCTITLE
void setproctitle(const char *fmt, ...);
#endif

#endif /* _BSD_SETPROCTITLE_H */
