/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef ADDR_H
#define ADDR_H

/*
 * $Id: addr.h,v 1.1.1.3 2002/10/02 21:07:20 bbraun Exp $
 */


#include "pset.h"
#include "defs.h"
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

int addrlist_match(const pset_h addr_list, const struct sockaddr *addr);
void addrlist_dump(const pset_h addr_list, int fd);
void addrlist_free(pset_h addr_list);
status_e addrlist_add(pset_h addr_list, const char *str_addr);
status_e addrlist_remove(pset_h addr_list, const char *str_addr);
status_e addrlist_copy(const pset_h from, pset_h *to);
int check_hostname(const char *addr);

#endif   /* ADDR_H */

