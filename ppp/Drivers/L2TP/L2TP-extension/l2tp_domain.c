/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
#include <kern/locks.h>

#include "../../../Family/if_ppplink.h"
#include "../../../Family/ppp_domain.h"
#include "l2tp.h"
#include "l2tp_proto.h"
#include "l2tp_wan.h"
#include "l2tp_rfc.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

SYSCTL_NODE(_net_ppp, PF_PPP, l2tp, CTLFLAG_RW, 0, "");

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */
int l2tp_domain_init(int);
int l2tp_domain_terminate(int);

/* this function has not prototype in the .h file */
struct domain *pffinddomain(int pf);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

int 		l2tp_domain_inited = 0;
extern lck_mtx_t   *ppp_domain_mutex; 


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_domain_module_start(struct kmod_info *ki, void *data)
{
    //boolean_t 	funnel_state;
    int		ret;

    //funnel_state = thread_funnel_set(network_flock, TRUE);
    ret = l2tp_domain_init(0);
    //thread_funnel_set(network_flock, funnel_state);

    return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_domain_module_stop(struct kmod_info *ki, void *data)
{
    //boolean_t 	funnel_state;
    int		ret;

    //funnel_state = thread_funnel_set(network_flock, TRUE);
    ret = l2tp_domain_terminate(0);
    //thread_funnel_set(network_flock, funnel_state);

    return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_domain_init(int init_arg)
{
    int 	ret;
    struct domain *pppdomain;
    
    log(LOGVAL, "L2TP domain init\n");

    if (l2tp_domain_inited)
        return(KERN_SUCCESS);

    pppdomain = pffinddomain(PF_PPP);
    if (!pppdomain) {
        log(LOGVAL, "L2TP domain init : PF_PPP domain does not exist...\n");
        return KERN_FAILURE;
    }
	
	lck_mtx_lock(ppp_domain_mutex);
    	
    ret = l2tp_rfc_init();
    if (ret) {
        log(LOGVAL, "L2TP domain init : can't init l2tp protocol RFC, err : %d\n", ret);
        goto end;
    }
    
    ret = l2tp_add(pppdomain);
    if (ret) {
        log(LOGVAL, "L2TP domain init : can't add proto to l2tp domain, err : %d\n", ret);
        l2tp_rfc_dispose();
        goto end;
    }

    l2tp_wan_init();
    sysctl_register_oid(&sysctl__net_ppp_l2tp);
	
    l2tp_domain_inited = 1;
	log(LOGVAL, "L2TP domain init complete\n");

end:	
	lck_mtx_unlock(ppp_domain_mutex);
    return ret;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_domain_terminate(int term_arg)
{
    int 	ret = KERN_SUCCESS;
    struct domain *pppdomain;
    
    log(LOGVAL, "L2TP domain terminate\n");

    if (!l2tp_domain_inited)
        return(KERN_SUCCESS);

	lck_mtx_lock(ppp_domain_mutex);

    ret = l2tp_rfc_dispose();
    if (ret) {
        log(LOGVAL, "L2TP domain is in use and cannot terminate, err : %d\n", ret);
        goto end;
    }

    ret = l2tp_wan_dispose();
    if (ret) {
        log(LOGVAL, "L2TP domain terminate : l2tp_wan_dispose, err : %d\n", ret);
        goto end;
    }

    pppdomain = pffinddomain(PF_PPP);
    if (!pppdomain) {
        // humm.. should not happen
        log(LOGVAL, "L2TP domain terminate : PF_PPP domain does not exist...\n");
        ret = KERN_FAILURE;
		goto end;
    }
    
    ret = l2tp_remove(pppdomain);
    if (ret) {
        log(LOGVAL, "L2TP domain terminate : can't del proto from l2tp domain, err : %d\n", ret);
        goto end;
    }

    sysctl_unregister_oid(&sysctl__net_ppp_l2tp);

    l2tp_domain_inited = 0;
	
end:
	lck_mtx_unlock(ppp_domain_mutex);
    return ret;
}
