/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#include <net/if_var.h>

#include "../../../Family/if_ppplink.h"
#include "../../../Family/ppp_domain.h"
#include "PPPoE.h"
#include "pppoe_proto.h"
#include "pppoe_wan.h"
#include "pppoe_rfc.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */
int pppoe_domain_init(int);
int pppoe_domain_terminate(int);

/* this function has not prototype in the .h file */
struct domain *pffinddomain(int pf);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

int 		pppoe_domain_inited = 0;



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
    int 	ret;
    struct domain *pppdomain;
    
    log(LOGVAL, "PPPoE domain init\n");

    if (pppoe_domain_inited)
        return(KERN_SUCCESS);

    pppdomain = pffinddomain(PF_PPP);
    if (!pppdomain) {
        log(LOGVAL, "PPPoE domain init : PF_PPP domain does not exist...\n");
        return KERN_FAILURE;
    }
    
    ret = pppoe_rfc_init();
    if (ret) {
        log(LOGVAL, "PPPoE domain init : can't init PPPoE protocol RFC, err : %d\n", ret);
        return ret;
    }
    
    ret = pppoe_add(pppdomain);
    if (ret) {
        log(LOGVAL, "PPPoE domain init : can't add proto to PPPoE domain, err : %d\n", ret);
        pppoe_rfc_dispose();
        return ret;
    }

    pppoe_wan_init();

    pppoe_domain_inited = 1;

    return(KERN_SUCCESS);
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pppoe_domain_terminate(int term_arg)
{
    int 	ret;
    struct domain *pppdomain;
    
    log(LOGVAL, "PPPoE domain terminate\n");

    if (!pppoe_domain_inited)
        return(KERN_SUCCESS);

    ret = pppoe_rfc_dispose();
    if (ret) {
        log(LOGVAL, "PPPoE domain is in use and cannot terminate, err : %d\n", ret);
        return ret;
    }

    ret = pppoe_wan_dispose();
    if (ret) {
        log(LOGVAL, "PPPoE domain terminate : pppoe_wan_dispose, err : %d\n", ret);
        return ret;
    }

    pppdomain = pffinddomain(PF_PPP);
    if (!pppdomain) {
        // humm.. should not happen
        log(LOGVAL, "PPPoE domain terminate : PF_PPP domain does not exist...\n");
        return KERN_FAILURE;
    }
    
    ret = pppoe_remove(pppdomain);
    if (ret) {
        log(LOGVAL, "PPPoE domain terminate : can't del proto from PPPoE domain, err : %d\n", ret);
        return ret;
    }

    pppoe_domain_inited = 0;
    
    return(KERN_SUCCESS);
}
