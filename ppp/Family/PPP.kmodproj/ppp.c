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

/* -----------------------------------------------------------------------------
 *
 *  History :
 *
 *  Jun 2000 - 	add support for ppp generic interfaces

 *  Nov 1999 - 	Christophe Allie - created.
 *		basic support fo ppp family
 *
 *  Theory of operation :
 *
 *  this file creates is loaded as a Kernel Extension.
 *  it creates the necessary ppp components and plumbs everything together.
 *
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
  Includes
----------------------------------------------------------------------------- */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <kern/thread.h>
#include <net/dlil.h>
#include <sys/systm.h>

#include "ppp.h"
#include "ppp_domain.h"
#include "ppp_fam.h"

//#include "ppp_ald.h"
#include "ppp_ip.h"


/* -----------------------------------------------------------------------------
 Definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
 Forward declarations
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
 PPP globals
----------------------------------------------------------------------------- */
static int 	ppp_inited = 0;


/* ----------------------------------------------------------------------------- 
 NKE entry point, start routine
----------------------------------------------------------------------------- */
int ppp_module_start(struct kmod_info *ki, void *data)
{
    extern int ppp_init(int);
    boolean_t 	funnel_state;
    int		ret;

    funnel_state = thread_funnel_set(network_flock, TRUE);
    ret = ppp_init(0);
    thread_funnel_set(network_flock, funnel_state);

    return ret;
}

/* -----------------------------------------------------------------------------
  NKE entry point, stop routine
----------------------------------------------------------------------------- */
int ppp_module_stop(struct kmod_info *ki, void *data)
{
    extern int ppp_terminate(int);
    boolean_t 	funnel_state;
    int		ret;

    funnel_state = thread_funnel_set(network_flock, TRUE);
    ret = ppp_terminate(0);
    thread_funnel_set(network_flock, funnel_state);

    return ret;
}

/* -----------------------------------------------------------------------------
init function
----------------------------------------------------------------------------- */
int ppp_init(int init_arg)
{
    int 	ret;
    
//    log(LOGVAL, "ppp_init\n");

    if (ppp_inited)
        return KERN_SUCCESS;

    /* register the ppp network and ppp link module */
    ret = ppp_fam_init();
    LOGRETURN(ret, KERN_FAILURE, "ppp_init: ppp_fam_init error = 0x%x\n");

    /* add the ppp domain */
    ppp_domain_init();

    /* init the async ppp link discipline interface */
//    ppp_ald_init();

    /* init ip protocol */
    ppp_ip_init(0);
    
    /* NKE is ready ! */
    ppp_inited = 1;
    return KERN_SUCCESS;
}

/* -----------------------------------------------------------------------------
terminate function
----------------------------------------------------------------------------- */
int ppp_terminate(int term_arg)
{
    int ret;

//    log(LOGVAL, "ppp_terminate\n");

    if (!ppp_inited)
        return(KERN_SUCCESS);

    /* remove ip protocol */
    ppp_ip_dispose(0);

    /* remove async line disc support */
//    ppp_ald_dispose();

    /* remove the pppdomain */
    ppp_domain_dispose();

    ret = ppp_fam_dispose();
    LOGRETURN(ret, KERN_FAILURE, "ppp_terminate: ppp_fam_dispose error = 0x%x\n");

    return KERN_SUCCESS;
}
