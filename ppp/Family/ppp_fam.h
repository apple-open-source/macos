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


#ifndef __PPP_FAM_H__
#define __PPP_FAM_H__


#include <net/dlil.h>


/* PPP Logs facilities */

#if 0
#define LOGVAL 		LOG_INFO

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

struct ppp_fam {
    struct if_proto 	*ip_proto;
    u_long	    	ip_tag;
};


int ppp_fam_init();
int ppp_fam_dispose();


#endif