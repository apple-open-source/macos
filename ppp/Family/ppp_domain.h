/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


#ifndef __PPP_DOMAIN_H__
#define __PPP_DOMAIN_H__


/* ppp_domain is self contained */
#include "ppp_defs.h"
#include "if_ppplink.h"
#include "if_ppp.h"


#define PPPPROTO_CTL		1		/* control protocol for ifnet layer */

#define PPP_NAME		"PPP"		/* ppp family name */


struct sockaddr_ppp {
    u_int8_t	ppp_len;			/* sizeof(struct sockaddr_ppp) + variable part */
    u_int8_t	ppp_family;			/* AF_PPPCTL */
    u_int16_t	ppp_proto;			/* protocol coding address */
    u_int32_t 	ppp_cookie;			/* one long for protocol with few info */
    // variable len, the following are protocol specific addresses
};


struct ppp_link_event_data {
     u_int16_t          lk_index;
     u_int16_t          lk_unit;
     char               lk_name[IFNAMSIZ];
};

/* Define PPP events, as subclass of NETWORK_CLASS events */

#define KEV_PPP_NET_SUBCLASS 	3
#define KEV_PPP_LINK_SUBCLASS 	4



#ifdef KERNEL

int ppp_domain_init();
int ppp_domain_dispose();

int ppp_proto_input(void *data, struct mbuf *m);
void ppp_proto_free(void *data);


/* Logs facilities */

#define LOGVAL 		LOG_DEBUG
#define LOG(text) 	log(LOGVAL, text)
#define LOGDBG(ifp, text) \
    if (ifp->if_flags & IFF_DEBUG) {	\
        log text; 		\
    }

#define LOGRETURN(err, ret, text) \
    if (err) {			\
        log(LOGVAL, text, err); \
        return ret;		\
    }

#endif

#endif