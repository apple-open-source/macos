/* $Id: setproctitle.h,v 1.1.1.2 2003/04/01 04:02:24 zarzycki Exp $ */

#ifndef _BSD_SETPROCTITLE_H
#define _BSD_SETPROCTITLE_H

#include "config.h"

#ifndef HAVE_SETPROCTITLE
void setproctitle(const char *fmt, ...);
void compat_init_setproctitle(int argc, char *argv[]);
#endif

#endif /* _BSD_SETPROCTITLE_H */
