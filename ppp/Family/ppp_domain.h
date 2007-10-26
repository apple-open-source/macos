/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#ifndef __PPP_DOMAIN_H__
#define __PPP_DOMAIN_H__


/* ppp_domain is self contained */
#include <sys/sysctl.h>
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
int ppp_proto_add();
int ppp_proto_remove();

int ppp_proto_input(void *data, mbuf_t m);
void ppp_proto_free(void *data);

SYSCTL_DECL(_net_ppp);

/* Logs facilities */

#define LOGVAL 		(LOG_DEBUG|LOG_RAS)
#define LOG(text) 	log(LOGVAL, text)
#define LOGDBG(ifp, text) \
    if (ifnet_flags(ifp) & IFF_DEBUG) {	\
        log text; 		\
    }

#define LOGRETURN(err, ret, text) \
    if (err) {			\
        log(LOGVAL, text, err); \
        return ret;		\
    }
	
#define LOGGOTOFAIL(err, text) \
    if (err) {			\
        log(LOGVAL, text, err); \
        goto fail;		\
    }

#define LOGNULLFAIL(ret, text) \
    if (ret == 0) {			\
        log(LOGVAL, text); \
        goto fail;		\
    }

#ifdef LOGDATA
#define LOGMBUF(text, m)   {		\
    short i;				\
    char *p = mtod((m), u_char *);	\
    log(LOGVAL, text);			\
    log(LOGVAL, " : 0x ");		\
    for (i = 0; i < (m)->m_len; i++)	\
       log(LOGVAL, "%x ", p[i]);	\
    log(LOGVAL, "\n");			\
}
#else
#define LOGMBUF(text, m)
#endif



/*
 * PPP queues.
 */
struct	pppqueue {
	mbuf_t head;
	mbuf_t tail;
	int	len;
	int	maxlen;
	int	drops;
};

int ppp_qfull(struct pppqueue *pppq);
void ppp_drop(struct pppqueue *pppq);
void ppp_enqueue(struct pppqueue *pppq, mbuf_t m);
mbuf_t ppp_dequeue(struct pppqueue *pppq);
void ppp_prepend(struct pppqueue *pppq, mbuf_t m);

#endif

#endif
