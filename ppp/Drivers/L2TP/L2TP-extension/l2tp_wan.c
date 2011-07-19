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

/* -----------------------------------------------------------------------------
*
*  History :
*
*  Sep 2002 - created from pptp_wan.c.
*
*  Theory of operation :
*
*  this file implements the l2tp driver for the ppp family
*
*  it's the responsability of the driver to update the statistics
*  whenever that makes sense
*     ifnet.if_lastchange = a packet is present, a packet starts to be sent
*     ifnet.if_ibytes = nb of correct PPP bytes received
*     ifnet.if_obytes = nb of correct PPP bytes sent 
*     ifnet.if_ipackets = nb of PPP packet received
*     ifnet.if_opackets = nb of PPP packet sent
*     ifnet.if_ierrors = nb on input packets in error
*     ifnet.if_oerrors = nb on ouptut packets in error
*
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Includes
----------------------------------------------------------------------------- */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <net/if_types.h>
#include <kern/clock.h>
#include <kern/locks.h>



#include "../../../Family/ppp_domain.h"
#include "../../../Family/if_ppplink.h"
#include "../../../Family/ppp_defs.h"
#include "../../../Family/if_ppp.h"
#include "l2tpk.h"
#include "l2tp_rfc.h"
#include "l2tp_wan.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

struct l2tp_wan {
    /* first, the ifnet structure... */
    struct ppp_link 	link;			/* ppp link structure */

    /* administrative info */
    TAILQ_ENTRY(l2tp_wan) next;
    void 		*rfc;			/* L2TP protocol structure */

    /* settings */
    
    /* output data */

    /* input data */

    /* log purpose */
};

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static int	l2tp_wan_output(struct ppp_link *link, mbuf_t m);
static int 	l2tp_wan_ioctl(struct ppp_link *link, u_long cmd, void *data);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

static TAILQ_HEAD(, l2tp_wan) 	l2tp_wan_head;

extern lck_mtx_t   *ppp_domain_mutex;

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_wan_init()
{

    TAILQ_INIT(&l2tp_wan_head);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_wan_dispose()
{

    // don't detach if links are in use
    if (TAILQ_FIRST(&l2tp_wan_head))
        return EBUSY;
        
    return 0;
}

/* -----------------------------------------------------------------------------
attach L2TP interface dlil layer
----------------------------------------------------------------------------- */
int l2tp_wan_attach(void *rfc, struct ppp_link **link)
{
    int 		ret;	
    struct l2tp_wan  	*wan, *wan1;
    struct ppp_link  	*lk;
    u_short 		unit;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    // Note : we allocate/find number/insert in queue in that specific order
    // because of funnels and race condition issues

    MALLOC(wan, struct l2tp_wan *, sizeof(struct l2tp_wan), M_TEMP, M_WAITOK);
    if (!wan)
        return ENOMEM;

    bzero(wan, sizeof(struct l2tp_wan));

	// find a unit and where to insert it, keep the list ordered
	unit = 0;
    wan1 = TAILQ_FIRST(&l2tp_wan_head);
    while (wan1) {
    	if (wan1->link.lk_unit > unit)
            break;

		unit = wan1->link.lk_unit + 1;
        wan1 = TAILQ_NEXT(wan1, next);
    }

	if (wan1)
		TAILQ_INSERT_BEFORE(wan1, wan, next);
	else
		TAILQ_INSERT_TAIL(&l2tp_wan_head, wan, next);
		
    lk = (struct ppp_link *) wan;
    
    // it's time now to register our brand new link
    lk->lk_name 	= (u_char*)L2TP_NAME;
    lk->lk_mtu 		= L2TP_MTU;
    lk->lk_mru 		= L2TP_MTU;;
    lk->lk_type 	= PPP_TYPE_L2TP;
    lk->lk_hdrlen 	= 80;	// ??? 
	l2tp_rfc_command(rfc, L2TP_CMD_GETBAUDRATE, &lk->lk_baudrate);
    lk->lk_ioctl 	= l2tp_wan_ioctl;
    lk->lk_output 	= l2tp_wan_output;
    lk->lk_unit 	= unit;
    lk->lk_support 	= 0;
    wan->rfc = rfc;

    ret = ppp_link_attach((struct ppp_link *)wan);
    if (ret) {
        IOLog("L2TP_wan_attach, error = %d, (ld = %p)\n", ret, wan);
        TAILQ_REMOVE(&l2tp_wan_head, wan, next);
        FREE(wan, M_TEMP);
        return ret;
    }
    
    //IOLog("L2TP_wan_attach, link index = %d, (ld = %p)\n", lk->lk_index, lk);

    *link = lk;
    
    return 0;
}

/* -----------------------------------------------------------------------------
detach L2TP interface dlil layer
----------------------------------------------------------------------------- */
void l2tp_wan_detach(struct ppp_link *link)
{
    struct l2tp_wan  	*wan = (struct l2tp_wan *)link;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    ppp_link_detach(link);
    TAILQ_REMOVE(&l2tp_wan_head, wan, next);
    FREE(wan, M_TEMP);
}

/* -----------------------------------------------------------------------------
called from l2tp_rfc when data are present
----------------------------------------------------------------------------- */
int l2tp_wan_input(struct ppp_link *link, mbuf_t m)
{
	struct timespec tv;	
    
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
	
    link->lk_ipackets++;
    link->lk_ibytes += mbuf_pkthdr_len(m);
	nanouptime(&tv);
	link->lk_last_recv = tv.tv_sec;
    ppp_link_input(link, m);	
    return 0;
}

/* -----------------------------------------------------------------------------
called from l2tp_rfc when xmit is full
----------------------------------------------------------------------------- */
void l2tp_wan_xmit_full(struct ppp_link *link)
{
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    link->lk_flags |= SC_XMIT_FULL;
}

/* -----------------------------------------------------------------------------
called from l2tp_rfc when there is an input error
----------------------------------------------------------------------------- */
void l2tp_wan_input_error(struct ppp_link *link)
{
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    ppp_link_event(link, PPP_LINK_EVT_INPUTERROR, 0);
}

/* -----------------------------------------------------------------------------
called from l2tp_rfc when xmit is ok again
----------------------------------------------------------------------------- */
void l2tp_wan_xmit_ok(struct ppp_link *link)
{
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
    link->lk_flags &= ~SC_XMIT_FULL;
    ppp_link_event(link, PPP_LINK_EVT_XMIT_OK, 0);
}

/* -----------------------------------------------------------------------------
Process an ioctl request to the ppp interface
----------------------------------------------------------------------------- */
int l2tp_wan_ioctl(struct ppp_link *link, u_long cmd, void *data)
{
    //struct l2tp_wan 	*wan = (struct l2tp_wan *)link;;
    int error = 0;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
    
    //LOGDBG(ifp, ("l2tp_wan_ioctl, cmd = 0x%x\n", cmd));

    switch (cmd) {
        default:
            error = ENOTSUP;
    }
    return error;
}

/* -----------------------------------------------------------------------------
This gets called at splnet from if_ppp.c at various times
when there is data ready to be sent
----------------------------------------------------------------------------- */
int l2tp_wan_output(struct ppp_link *link, mbuf_t m)
{
    struct l2tp_wan 	*wan = (struct l2tp_wan *)link;
    u_int32_t		len = mbuf_pkthdr_len(m);	// take it now, as output will change the mbuf
    int 		err;
	struct timespec tv;	
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
    
    if (err = l2tp_rfc_output(wan->rfc, m, 0)) {
        link->lk_oerrors++;
        return err;
    }

    link->lk_opackets++;
    link->lk_obytes += len;
	nanouptime(&tv);
	link->lk_last_xmit = tv.tv_sec;
    return 0;
}
