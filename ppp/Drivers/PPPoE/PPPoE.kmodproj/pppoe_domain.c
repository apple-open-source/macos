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


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <sys/syslog.h>
#include <mach/vm_types.h>
#include <mach/kmod.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <kern/thread.h>


#include "PPPoE.h"
#include "pppoe_proto.h"
#include "pppoe_wan.h"
#include "pppoe_rfc.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */
#define LOGVAL LOG_INFO


/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */
int pppoe_domain_init(int);
int pppoe_domain_terminate(int);


/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

int 		pppoe_domain_inited = 0;
char 		*pppoe_domain_name = PPPOE_NAME;
struct domain 	pppoe_domain;



/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pppoe_domain_module_start(struct kmod_info *ki, void *data)
{
    boolean_t 	funnel_state;
    int		ret;

    funnel_state = thread_funnel_set(network_flock, TRUE);
    ret = pppoe_domain_init(0);
    thread_funnel_set(network_flock, funnel_state);

    return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pppoe_domain_module_stop(struct kmod_info *ki, void *data)
{
    boolean_t 	funnel_state;
    int		ret;

    funnel_state = thread_funnel_set(network_flock, TRUE);
    ret = pppoe_domain_terminate(0);
    thread_funnel_set(network_flock, funnel_state);

    return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pppoe_domain_init(int init_arg)
{
    int ret;

    log(LOGVAL, "pppoe_domain_init\n");

    if (pppoe_domain_inited)
        return(KERN_SUCCESS);

    bzero(&pppoe_domain, sizeof(struct domain));
    pppoe_domain.dom_family = PF_PPPOE;
    pppoe_domain.dom_name = pppoe_domain_name;

    // add domain cannot fail, just add the domain struct to the linked list
    net_add_domain(&pppoe_domain);

    log(LOGVAL, "pppoe_domain_init : dp->dom_refs =  : %d\n", pppoe_domain.dom_refs);

    // now, we can attach the protocol to the dlil interface
    // Fix Me : add support for multiple interfaces attachment
//    ret = pppoe_dlil_attach(APPLE_IF_FAM_ETHERNET, 0, &pppoe_dl_tag[0]);
    ret = pppoe_rfc_init();
    if (ret) {
        log(LOGVAL, "pppoe_domain_init : can't init PPPoE protocol RFC, err : %d\n", ret);
        net_del_domain(&pppoe_domain);
        return ret;
    }

    ret = pppoe_rfc_attach(0);
    if (ret) {
        log(LOGVAL, "pppoe_domain_init : can't attach PPPoE protocol, err : %d\n", ret);
        pppoe_rfc_dispose();
        net_del_domain(&pppoe_domain);
        return ret;
    }
    
    ret = pppoe_add(&pppoe_domain);
    if (ret) {
        log(LOGVAL, "pppoe_domain_init : can't add proto to PPPoE domain, err : %d\n", ret);
       // pppoe_dlil_detach(pppoe_dl_tag[0]);
        pppoe_rfc_dispose();
        net_del_domain(&pppoe_domain);
        return ret;
    }

    log(LOGVAL, "pppoe_domain_init2 : dp->dom_refs =  : %d\n", pppoe_domain.dom_refs);

    pppoe_wan_init();

    pppoe_domain_inited = 1;

    return(KERN_SUCCESS);
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pppoe_domain_terminate(int term_arg)
{
    int ret;

    log(LOGVAL, "pppoe_domain_terminate\n");

    if (!pppoe_domain_inited)
        return(KERN_SUCCESS);

    pppoe_wan_dispose();

    log(LOGVAL, "pppoe_domain_terminate : dp->dom_refs =  : %d\n", pppoe_domain.dom_refs);

    ret = pppoe_remove(&pppoe_domain);
    if (ret) {
        log(LOGVAL, "pppoe_domain_terminate : can't del proto from PPPoE domain, err : %d\n", ret);
        return ret;
    }

    log(LOGVAL, "pppoe_domain_terminate2 : dp->dom_refs =  : %d\n", pppoe_domain.dom_refs);

     pppoe_rfc_dispose();
    //ret = pppoe_dlil_detach(pppoe_dl_tag[0]);
    if (ret) {
        log(LOGVAL, "pppoe_domain_terminate : can't detach PPPoE, err : %d\n", ret);
        return ret;
    }

    ret = net_del_domain(&pppoe_domain);
    if (ret) {
        log(LOGVAL, "pppoe_domain_terminate : can't del PPPoE domain, err : %d\n", ret);
        return ret;
    }

    pppoe_domain_inited = 0;
    
    return(KERN_SUCCESS);
}
