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

/* -----------------------------------------------------------------------------
*
*  History :
*
*  Feb 2001 - 	Christophe Allie - created.
*
*  Theory of operation :
*
*  this file implements the link operations for ppp
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
#include <kern/locks.h>
#include <net/if_types.h>
#include <net/if.h>
#include <netinet/in.h>

#include "ppp_defs.h"		// public ppp values
#include "if_ppp.h"		// public ppp API
#include "if_ppplink.h"		// public link API
#include "ppp_domain.h"
#include "ppp_if.h"

/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

/* Macro facilities */

#define LKIFNET(lk)		(((struct ppp_link *)lk)->lk_ifnet)
#define LKNAME(lk) 		(((struct ppp_link*)lk)->lk_name)
#define LKUNIT(lk) 		(((struct ppp_link*)lk)->lk_unit)

#define LKIFFDEBUG(lk) 		(LKIFNET(lk) ? ifnet_flags(LKIFNET(lk)) & IFF_DEBUG : 0 )
#define LKIFNAME(lk) 		(LKIFNET(lk) ? ifnet_name(LKIFNET(lk)) : "???")
#define LKIFUNIT(lk) 		(LKIFNET(lk) ? ifnet_unit(LKIFNET(lk)) : 0)

#define LOGLKDBG(lk, text) \
    if (LKIFNET(lk) && (ifnet_flags(LKIFNET(lk)) & IFF_DEBUG)) {	\
        IOLog text; 		\
    }

/* 
    As the private structure contains only one field, 
    there is no need for an extra malloc .
    Can be changed later, if we need to keep
    more information in the private structure.
*/
#ifdef USE_PRIVATE_STRUCT
/* Link private data structure */
struct ppp_priv {
    void 		*host;		/* our client structure */
};
#endif

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */



/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

static TAILQ_HEAD(, ppp_link) 	ppp_link_head;
extern lck_mtx_t   *ppp_domain_mutex;

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_link_init()
{
    TAILQ_INIT(&ppp_link_head);
    
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_link_dispose()
{
     // can't dispose if serial lines are in use
    if (TAILQ_FIRST(&ppp_link_head))
        return EBUSY;

    return 0;
}

/* -----------------------------------------------------------------------------
find a free unit in the interface list
----------------------------------------------------------------------------- */
u_short ppp_link_findfreeindex()
{
    struct ppp_link  	*link = TAILQ_FIRST(&ppp_link_head);
    u_short 		index = 0;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    while (link) {
    	if (link->lk_index == index) {
            index++;
            link = TAILQ_FIRST(&ppp_link_head); // restart
        }
        else 
            link = TAILQ_NEXT(link, lk_next); // continue
    }
    return index;
}

/* -----------------------------------------------------------------------------
Attach a link 
----------------------------------------------------------------------------- */
int ppp_link_attach(struct ppp_link *link)
{
#ifdef USE_PRIVATE_STRUCT
    struct ppp_priv *priv;
#endif
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    if (!link->lk_ioctl || !link->lk_output) {
        return EINVAL;
    }

#ifdef USE_PRIVATE_STRUCT
    MALLOC(priv, struct ppp_priv *, sizeof(struct ppp_priv), M_TEMP, M_WAITOK);
    if (!priv)
        return ENOMEM;

    bzero(priv, sizeof(struct ppp_priv));
    link->lk_ppp_private = priv;
#else
    link->lk_ppp_private = 0;
#endif
    link->lk_ifnet = 0;
    link->lk_index = ppp_link_findfreeindex();
    TAILQ_INSERT_TAIL(&ppp_link_head, link, lk_next);
    
    return 0;
}

/* -----------------------------------------------------------------------------
Detach a link 
----------------------------------------------------------------------------- */
int ppp_link_detach(struct ppp_link *link)
{
#ifdef USE_PRIVATE_STRUCT
    struct ppp_priv 	*priv = (struct ppp_priv *)link->lk_ppp_private;
#endif

	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    LOGLKDBG(link, ("ppp_link_detach : (link = %s%d)\n", LKNAME(link), LKUNIT(link)));
    ppp_if_detachlink(link);
#ifdef USE_PRIVATE_STRUCT
    ppp_proto_free(priv->host);
    FREE(priv, M_TEMP);
#else
    ppp_proto_free(link->lk_ppp_private);
#endif
    TAILQ_REMOVE(&ppp_link_head, link, lk_next);
    link->lk_ppp_private = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_link_event(struct ppp_link *link, u_int32_t event, void *data)
{

	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    switch (event) {
        case PPP_LINK_EVT_XMIT_OK:
            if (link->lk_ifnet)
                ppp_if_xmit(link->lk_ifnet, 0);
            break;
        case PPP_LINK_EVT_INPUTERROR:
            if (link->lk_ifnet)
                ppp_if_error(link->lk_ifnet);
            break;
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_link_input(struct ppp_link *link, mbuf_t m)
{
#ifdef USE_PRIVATE_STRUCT
    struct ppp_priv 	*priv = (struct ppp_priv *)link->lk_ppp_private;
#endif
    u_char 		*p;
    u_int16_t		proto, len;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
    
    if (link->lk_ifnet && (ifnet_flags(link->lk_ifnet) & PPP_LOG_INPKT)) 
        ppp_link_logmbuf(link, "ppp_link_input", m);

	if (mbuf_len(m) < PPP_HDRLEN && 
		mbuf_pullup(&m, PPP_HDRLEN)) {
			if (m) {
				mbuf_freem(m);
				m = NULL;
			}
			IOLog("ppp_link_input: cannot pullup header\n");
			return 0;
	}

    p = mbuf_data(m);	// no alignment issue as p is *uchar.
    if ((p[0] == PPP_ALLSTATIONS) && (p[1] == PPP_UI)) {
        mbuf_adj(m, 2);
        p = mbuf_data(m);
    }
    proto = p[0];
    len = 1;
    if (!(proto & 0x1)) {  // lowest bit set for lowest byte of protocol
        proto = (proto << 8) + p[1];
        len = 2;
    } 
    
    if (link->lk_ifnet && (proto < 0xC000)) {
        ppp_if_input(link->lk_ifnet, m, proto, len);	// Network protocol
    }
    else {
#ifdef USE_PRIVATE_STRUCT
	ppp_proto_input(priv->host, m);		// LCP/Auth/unexpected network protocol
#else
        ppp_proto_input(link->lk_ppp_private, m);// LCP/Auth/unexpected network protocol
#endif
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_link_control(struct ppp_link *link, u_long cmd, void *data)
{
    int 	error = 0;
    u_int32_t	flags, mru;    
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
        
    switch (cmd) {
        case PPPIOCCONNECT:
            LOGLKDBG(link, ("ppp_link_control : PPPIOCCONNECT, (link = %s%d), attach to interface = %d \n",
                LKNAME(link), LKUNIT(link), *(u_int32_t *)data));
            error = ppp_if_attachlink(link, *(u_int32_t *)data);
            break;
        case PPPIOCDISCONN:
            LOGLKDBG(link, ("ppp_link_control : PPPIOCDISCONN, (link = %s%d), detach from interface = %s%d \n",
                LKNAME(link), LKUNIT(link), LKIFNAME(link), LKIFUNIT(link)));
            error = ppp_if_detachlink(link);
            break;
        default:
            error = ENOTSUP;
    }
    if (error != ENOTSUP)
        return error;
    
    error = (*link->lk_ioctl)(link, cmd, data);
    if (error != ENOTSUP) 
        return error;

    error = 0;
    switch (cmd) {

        case PPPIOCGFLAGS:
            LOGLKDBG(link, ("ppp_link_control:  (link = %s%d), PPPIOCGFLAGS = 0x%x\n", 
                LKNAME(link), LKUNIT(link), link->lk_flags));
            *(u_int32_t *)data = link->lk_flags;
            break;

        case PPPIOCSFLAGS:
            LOGLKDBG(link, ("ppp_link_control:  (link = %s%d), PPPIOCSFLAGS = 0x%x\n", 
                LKNAME(link), LKUNIT(link), *(u_int32_t *)data & SC_MASK));
            flags = *(u_int32_t *)data & SC_MASK;
            link->lk_flags = (link->lk_flags & ~SC_MASK) | flags;
            break;

        case PPPIOCGMRU:
            LOGLKDBG(link, ("ppp_link_control:  (link = %s%d), PPPIOCGMRU = 0x%x\n", 
                LKNAME(link), LKUNIT(link), link->lk_mru));
            *(u_int32_t *)data = link->lk_mru;
            break;
            
        case PPPIOCSMRU:
            LOGLKDBG(link, ("ppp_link_control:  (link = %s%d), PPPIOCSMRU = 0x%x\n", 
                LKNAME(link), LKUNIT(link), *(u_int32_t *)data));
            mru = *(u_int32_t *)data;
            link->lk_mru = mru;
            break;
            
                        
        case PPPIOCSASYNCMAP:
        case PPPIOCSRASYNCMAP:
        case PPPIOCSXASYNCMAP:
        case PPPIOCGASYNCMAP:
        case PPPIOCGRASYNCMAP:
        case PPPIOCGXASYNCMAP:
            if ((link->lk_support & PPP_LINK_ASYNC) == 0)
                error = EINVAL; 	// async link must support these ioctls
            break;


        default:
            error = EINVAL;
    }
    return error;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_link_attachclient(u_short index, void *host, struct ppp_link **data)
{
    struct ppp_link  	*link;
#ifdef USE_PRIVATE_STRUCT
    struct ppp_priv	*priv;
#endif

	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    TAILQ_FOREACH(link, &ppp_link_head, lk_next) {
        if (link->lk_index == index) {
            *data = (void *)link;
#ifdef USE_PRIVATE_STRUCT
            priv = (struct ppp_priv *)link->lk_ppp_private;
			if (priv->host)
				return EBUSY;	/* a client is already attached */
            priv->host = host;
#else
			if (link->lk_ppp_private)
				return EBUSY;	/* a client is already attached */
            link->lk_ppp_private = host;
#endif
            return 0;
        }
    }
    return ENODEV;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_link_detachclient(struct ppp_link *link, void *host)
{
#ifdef USE_PRIVATE_STRUCT
    struct ppp_priv	*priv = (struct ppp_priv *)link->lk_ppp_private;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    if (priv && (priv->host == host))
        priv->host = 0;
#else
    if (link->lk_ppp_private == host)
        link->lk_ppp_private = 0;
#endif
}

/* -----------------------------------------------------------------------------
we wend packet without link framing (FF03)
it's the reponsability of the driver to add the header, it the links need it.
it should be done accordingly to the ppp negociation as well.
----------------------------------------------------------------------------- */
int ppp_link_send(struct ppp_link *link, mbuf_t m)
{
    u_char 	*p = mbuf_data(m);	// no alignment issue as p is *uchar.
    u_int16_t 	proto = ((u_int16_t)p[0] << 8) + p[1];
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    // if pcomp has been negociated, remove leading 0 byte
    if ((link->lk_flags & SC_COMP_PROT) && !p[0]) {
	mbuf_adj(m, 1);
    }
    
    // if accomp has not been negociated, or if the needs FF03, add the header
    if (!(link->lk_support & PPP_LINK_DEL_AC)) {
        if ((proto == PPP_LCP)
            || !(link->lk_flags & SC_COMP_AC)) {
        
        if (mbuf_prepend(&m, 2, MBUF_DONTWAIT) != 0) 
            return ENOBUFS;
        
        p = mbuf_data(m);
        p[0] = PPP_ALLSTATIONS;
        p[1] = PPP_UI;
      }
    }
    
    if (link->lk_ifnet && (ifnet_flags(link->lk_ifnet) & PPP_LOG_OUTPKT)) 
        ppp_link_logmbuf(link, "ppp_link_send", m);

	/* link level packet are send oot of band */
    if ((proto >= 0xC000) && 
		(link->lk_support & PPP_LINK_OOB_QUEUE))
		mbuf_settype(m, MBUF_TYPE_OOBDATA);
		
    return (*link->lk_output)(link, m);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_link_logmbuf(struct ppp_link *link, char *msg, mbuf_t m) 
{
    int 	i, lcount, copycount, count;
    char 	lbuf[16], *data;

    if (m == NULL)
        return;

    IOLog("%s: [ifnet = %s%d] [link = %s%d]\n", msg,
            LKIFNAME(link), LKIFUNIT(link), LKNAME(link), LKUNIT(link));

    for (count = mbuf_len(m), data = mbuf_data(m); m != NULL; ) {	// no alignment issue as data is *uchar.
        /* build a line of output */
        for(lcount = 0; lcount < sizeof(lbuf); lcount += copycount) {
            if (!count) {
                m = mbuf_next(m);
                if (m == NULL)
                    break;
                count = mbuf_len(m);
                data  = mbuf_data(m);
            }
            copycount = (count > sizeof(lbuf) - lcount) ? sizeof(lbuf) - lcount : count;
            bcopy(data, &lbuf[lcount], copycount);
            data  += copycount;
            count -= copycount;
        }

        /* output line (hex 1st, then ascii) */
        IOLog("%s:  0x  ", msg);
        for(i = 0; i < lcount; i++) {
            if (i == 8) IOLog("  ");
            IOLog("%02x ", (u_char)lbuf[i]);
        }
        for( ; i < sizeof(lbuf); i++) {
            if (i == 8) IOLog("  ");
            IOLog("   ");
        }
        IOLog("  '");
        for(i = 0; i < lcount; i++)
            IOLog("%c",(lbuf[i]>=040 && lbuf[i]<=0176)?lbuf[i]:'.');
        IOLog("'\n");
    }
}

