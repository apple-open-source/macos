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
#include <net/if_types.h>

#include <net/if_types.h>
#include <net/dlil.h>

#include "ppp_defs.h"		// public ppp values
#include "if_ppp.h"		// public ppp API
#include "if_ppplink.h"		// public link API
#include "ppp_if.h"
#include "ppp_domain.h"

/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

/* Macro facilities */

#define LKIFNET(lk)		(((struct ppp_link *)lk)->lk_ifnet)
#define LKNAME(lk) 		(((struct ppp_link*)lk)->lk_name)
#define LKUNIT(lk) 		(((struct ppp_link*)lk)->lk_unit)

#define LKIFFDEBUG(lk) 		(LKIFNET(lk) ? LKIFNET(lk)->if_flags & IFF_DEBUG : 0 )
#define LKIFNAME(lk) 		(LKIFNET(lk) ? LKIFNET(lk)->if_name : "???")
#define LKIFUNIT(lk) 		(LKIFNET(lk) ? LKIFNET(lk)->if_unit : 0)

#define LOGLKDBG(lk, text) \
    if (LKIFNET(lk) && (LKIFNET(lk)->if_flags & IFF_DEBUG)) {	\
        log text; 		\
    }


/* Link private data structure */
struct ppp_priv {
    void 		*host;		/* our client structure */
};


/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

void ppp_link_logmbuf(struct ppp_link *link, char *msg, struct mbuf *m);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

static TAILQ_HEAD(, ppp_link) 	ppp_link_head;


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
    struct ppp_link  	*link;
    u_short 		index = 0;

    TAILQ_FOREACH(link, &ppp_link_head, lk_next) {
        if (link->lk_index == index) {
            link = TAILQ_FIRST(&ppp_link_head); // reloop
            index++;
        }
    }
    return index;
}

/* -----------------------------------------------------------------------------
Attach a link 
----------------------------------------------------------------------------- */
int ppp_link_attach(struct ppp_link *link)
{
    struct ppp_priv *priv;

    if (!link->lk_ioctl || !link->lk_output) {
        return EINVAL;
    }

    MALLOC(priv, struct ppp_priv *, sizeof(struct ppp_priv), M_DEVBUF, M_WAITOK);
    if (!priv)
        return ENOMEM;

    bzero(priv, sizeof(struct ppp_priv));
    link->lk_ifnet = 0;
    link->lk_index = ppp_link_findfreeindex();
    link->lk_ppp_private = priv;
    TAILQ_INSERT_TAIL(&ppp_link_head, link, lk_next);
    
    return 0;
}

/* -----------------------------------------------------------------------------
Detach a link 
----------------------------------------------------------------------------- */
int ppp_link_detach(struct ppp_link *link)
{
    struct ppp_priv 	*priv = (struct ppp_priv *)link->lk_ppp_private;

    LOGLKDBG(link, (LOGVAL, "ppp_link_detach : (link = %s%d)\n", LKNAME(link), LKUNIT(link)));
    ppp_if_detachlink(link);
    ppp_proto_free(priv->host);
    TAILQ_REMOVE(&ppp_link_head, link, lk_next);
    FREE(priv, M_DEVBUF);
    link->lk_ppp_private = 0;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_link_event(struct ppp_link *link, u_int32_t event, void *data)
{

    switch (event) {
        case PPP_LINK_EVT_XMIT_OK:
            break;
        case PPP_LINK_EVT_INPUTERROR:
            ppp_if_linkerror(link);
            break;
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_link_input(struct ppp_link *link, struct mbuf *m)
{
    struct ppp_priv 	*priv = (struct ppp_priv *)link->lk_ppp_private;
    u_char 		*p;
    u_int16_t		proto, len;
    
    if (link->lk_ifnet && (link->lk_ifnet->if_flags & PPP_LOG_INPKT)) 
        ppp_link_logmbuf(link, "ppp_link_input", m);

    p = mtod(m, u_char *);
    if ((p[0] == PPP_ALLSTATIONS) && (p[1] == PPP_UI)) {
        m_adj(m, 2);
        p = mtod(m, u_char *);
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
        ppp_proto_input(priv->host, m);		// LCP/Auth/unexpected network protocol
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_link_control(struct ppp_link *link, u_int32_t cmd, void *data)
{
    int 	error = 0;
    u_int32_t	flags, mru;    
        
    switch (cmd) {
        case PPPIOCCONNECT:
            LOGLKDBG(link, (LOGVAL, "ppp_link_control : PPPIOCCONNECT, (link = %s%d), attach to interface = %d \n",
                LKNAME(link), LKUNIT(link), *(u_int32_t *)data));
            error = ppp_if_attachlink(link, *(u_int32_t *)data);
            break;
        case PPPIOCDISCONN:
            LOGLKDBG(link, (LOGVAL, "ppp_link_control : PPPIOCDISCONN, (link = %s%d), detach from interface = %s%d \n",
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
            LOGLKDBG(link, (LOGVAL, "ppp_link_control:  (link = %s%d), PPPIOCGFLAGS = 0x%x\n", 
                LKNAME(link), LKUNIT(link), link->lk_flags));
            *(u_int32_t *)data = link->lk_flags;
            break;

        case PPPIOCSFLAGS:
            LOGLKDBG(link, (LOGVAL, "ppp_link_control:  (link = %s%d), PPPIOCSFLAGS = 0x%x\n", 
                LKNAME(link), LKUNIT(link), *(u_int32_t *)data & SC_MASK));
            flags = *(u_int32_t *)data & SC_MASK;
            link->lk_flags = (link->lk_flags & ~SC_MASK) | flags;
            break;

        case PPPIOCGMRU:
            LOGLKDBG(link, (LOGVAL, "ppp_link_control:  (link = %s%d), PPPIOCGMRU = 0x%x\n", 
                LKNAME(link), LKUNIT(link), link->lk_mru));
            *(u_int32_t *)data = link->lk_mru;
            break;
            
        case PPPIOCSMRU:
            LOGLKDBG(link, (LOGVAL, "ppp_link_control:  (link = %s%d), PPPIOCSMRU = 0x%x\n", 
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
            if (link->lk_support & PPP_LINK_ASYNC)
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
    struct ppp_priv	*priv;

    TAILQ_FOREACH(link, &ppp_link_head, lk_next) {
        if (link->lk_index == index) {
            *data = (void *)link;
            priv = (struct ppp_priv *)link->lk_ppp_private;
            priv->host = host;
            return 0;
        }
    }
    return ENODEV;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_link_detachclient(struct ppp_link *link, void *host)
{
    struct ppp_priv	*priv = (struct ppp_priv *)link->lk_ppp_private;

    if (priv->host == host)
        priv->host = 0;
}

/* -----------------------------------------------------------------------------
we wend packet without link framing (FF03)
it's the reponsability of the driver to add the header, it the links need it.
it should be done accordingly to the ppp negociation as well.
----------------------------------------------------------------------------- */
int ppp_link_send(struct ppp_link *link, struct mbuf *m)
{
    u_char 	*p = mtod(m, u_char *);
    u_int16_t 	proto = ((u_int16_t)p[0] << 8) + p[1];

    // if pcomp has been negociated, remove leading 0 byte
    if ((link->lk_flags & SC_COMP_PROT) && !p[0]) {
	m_adj(m, 1);
    }
    
    // if accomp has not been negociated, or if the needs FF03, add the header
    if (!(link->lk_support & PPP_LINK_DEL_AC)) {
        if ((proto == PPP_LCP)
            || !(link->lk_flags & SC_COMP_AC)) {
        
        M_PREPEND(m, 2, M_DONTWAIT);
        if (m == 0) 
            return ENOBUFS;
        
        p = mtod(m, u_char *);
        p[0] = PPP_ALLSTATIONS;
        p[1] = PPP_UI;
      }
    }
    
    if (link->lk_ifnet && (link->lk_ifnet->if_flags & PPP_LOG_OUTPKT)) 
        ppp_link_logmbuf(link, "ppp_link_send", m);
    
    return (*link->lk_output)(link, m);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_link_logmbuf(struct ppp_link *link, char *msg, struct mbuf *m) 
{
    int 	i, lcount, copycount, count;
    char 	lbuf[16], *data;

    if (m == NULL)
        return;

    log(LOGVAL, "%s: [ifnet = %s%d] [link = %s%d]\n", msg,
            LKIFNAME(link), LKIFUNIT(link), LKNAME(link), LKUNIT(link));

    for (count = m->m_len, data = mtod(m, char*); m != NULL; ) {
        /* build a line of output */
        for(lcount = 0; lcount < sizeof(lbuf); lcount += copycount) {
            if (!count) {
                m = m->m_next;
                if (m == NULL)
                    break;
                count = m->m_len;
                data  = mtod(m,char*);
            }
            copycount = (count > sizeof(lbuf) - lcount) ? sizeof(lbuf) - lcount : count;
            bcopy(data, &lbuf[lcount], copycount);
            data  += copycount;
            count -= copycount;
        }

        /* output line (hex 1st, then ascii) */
        log(LOGVAL, "%s:  0x  ", msg);
        for(i = 0; i < lcount; i++) {
            if (i == 8) log(LOGVAL, "  ");
            log(LOGVAL, "%02x ", (u_char)lbuf[i]);
        }
        for( ; i < sizeof(lbuf); i++) {
            if (i == 8) log(LOGVAL, "  ");
            log(LOGVAL, "   ");
        }
        log(LOGVAL, "  '");
        for(i = 0; i < lcount; i++)
            log(LOGVAL, "%c",(lbuf[i]>=040 && lbuf[i]<=0176)?lbuf[i]:'.');
        log(LOGVAL, "'\n");
    }
}

