/* $Id: grabmyaddr.h,v 1.5 2004/06/11 16:00:16 ludvigm Exp $ */

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _GRABMYADDR_H
#define _GRABMYADDR_H

#include <dispatch/dispatch.h>

struct myaddrs {
	struct myaddrs *next;
	struct sockaddr_storage *addr;
	int sock;
    dispatch_source_t source;
	int udp_encap;
	int	in_use;
	char *ifname;
};

extern void clear_myaddr (void);
extern void grab_myaddrs (void);
extern void update_myaddrs (void*);
extern int autoconf_myaddrsport (void);
extern u_short getmyaddrsport (struct sockaddr_storage *);
extern struct myaddrs *newmyaddr (void);
extern struct myaddrs *dupmyaddr (struct myaddrs *);
extern void insmyaddr (struct myaddrs *, struct myaddrs **);
extern void delmyaddr (struct myaddrs *);
extern int initmyaddr (void);
extern int getsockmyaddr (struct sockaddr *);
extern struct myaddrs *find_myaddr (struct sockaddr *, int);
extern int pfroute_init(void);
extern void pfroute_close(void);


#endif /* _GRABMYADDR_H */
