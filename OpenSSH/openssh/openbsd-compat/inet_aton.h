/* $Id: inet_aton.h,v 1.1.1.1 2001/02/25 20:54:30 zarzycki Exp $ */

#ifndef _BSD_INET_ATON_H
#define _BSD_INET_ATON_H

#include "config.h"

#ifndef HAVE_INET_ATON
int inet_aton(const char *cp, struct in_addr *addr);
#endif /* HAVE_INET_ATON */

#endif /* _BSD_INET_ATON_H */
