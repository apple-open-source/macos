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

/*
 ppp_comp.c - Compression protocol for ppp.

 based on if_ppp.c from bsd
 
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS ORpppall
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Drew D. Perkins
 * Carnegie Mellon University
 * 4910 Forbes Ave.
 * Pittsburgh, PA 15213
 * (412) 268-8576
 * ddp@andrew.cmu.edu
 *
 * Based on:
 *	@(#)if_sl.c	7.6.1.2 (Berkeley) 2/15/89
 *
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */


#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/vm.h>
#if 0
#include <net/netisr.h>
#endif
#include <sys/syslog.h>
#include <netinet/in.h>


#include "ppp_defs.h"		// public ppp values
#include "ppp_ip.h"
#include "if_ppplink.h"		// public link API
#include "ppp_domain.h"
#include "ppp_if.h"
#include "ppp_domain.h"
#include "ppp_comp.h"

/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

struct ppp_comp {

    TAILQ_ENTRY(ppp_comp) next;

    /* compressor identifier */
    u_int32_t	protocol;				/* CCP compression protocol number */

    void 	*userdata;			/* user data to pass to the compressor */

    /* compression call back functions */

    void	*(*comp_alloc) 
                (u_char *options, int opt_len);	/* Allocate space for a compressor (transmit side) */
    void	(*comp_free) 
                (void *state);			/* Free space used by a compressor */
    int		(*comp_init) 			/* Initialize a compressor */
                (void *state, u_char *options, int opt_len,
                int unit, int hdrlen, int mtu, int debug);
    void	(*comp_reset) 
                (void *state);			/* Reset a compressor */
    int		(*compress) 			/* Compress a packet */
                (void *state, mbuf_t *m);
    void	(*comp_stat) 			/* Return compression statistics */
                (void *state, struct compstat *stats);

    
    /* decompression call back functions */
    
    void	*(*decomp_alloc) 		/* Allocate space for a decompressor (receive side) */
                (u_char *options, int opt_len);
    void	(*decomp_free) 
                (void *state);			/* Free space used by a decompressor */	
    int		(*decomp_init) 			/* Initialize a decompressor */
                (void *state, u_char *options, int opt_len,
                int unit, int hdrlen, int mru, int debug);
    void	(*decomp_reset) 		/* Reset a decompressor */
                (void *state);
    int		(*decompress) 			/* Decompress a packet. */
                (void *state, mbuf_t *m);
    void	(*incomp) 			/* Update state for an incompressible packet received */
                (void *state, mbuf_t m);	
    void	(*decomp_stat) 			/* Return decompression statistics */
                (void *state, struct compstat *stats);
};


/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

struct ppp_comp *ppp_comp_find(u_int32_t proto);

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */
static TAILQ_HEAD(, ppp_comp) 	ppp_comp_head;

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_comp_init()
{
    TAILQ_INIT(&ppp_comp_head);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_comp_dispose()
{
    struct ppp_comp  	*comp;

    while (comp = TAILQ_FIRST(&ppp_comp_head)) {
        TAILQ_REMOVE(&ppp_comp_head, comp, next);
    	FREE(comp, M_TEMP);
    }
    
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
struct ppp_comp *ppp_comp_find(u_int32_t proto)
{
    struct ppp_comp  	*comp;

    TAILQ_FOREACH(comp, &ppp_comp_head, next)
        if (comp->protocol == proto)
            return comp;
    
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_comp_register(struct ppp_comp_reg *compreg, ppp_comp_ref *compref)
{
    struct ppp_comp *comp;

    /* sanity check */
    if (compreg == NULL
        || compreg->comp_alloc == NULL
        || compreg->comp_free == NULL
        || compreg->comp_init == NULL
        || compreg->comp_reset == NULL
        || compreg->compress == NULL
        || compreg->comp_stat == NULL
        || compreg->decomp_alloc == NULL
        || compreg->decomp_free == NULL
        || compreg->decomp_init == NULL
        || compreg->decomp_reset == NULL
        || compreg->decompress == NULL
        || compreg->incomp == NULL
        || compreg->decomp_stat == NULL)	
        return(EINVAL);
    
    comp = ppp_comp_find(compreg->compress_proto);
    if (comp != NULL)
        return(EEXIST);

    MALLOC(comp, struct ppp_comp *, sizeof(*comp), M_TEMP, M_WAITOK);
    if (comp == NULL)
        return(ENOMEM);
        
    bzero((char *)comp, sizeof(*comp));

    comp->protocol = compreg->compress_proto;
    comp->comp_alloc = compreg->comp_alloc;
    comp->comp_free = compreg->comp_free;
    comp->comp_init = compreg->comp_init;
    comp->comp_reset = compreg->comp_reset;
    comp->compress = compreg->compress;
    comp->comp_stat = compreg->comp_stat;
    comp->decomp_alloc = compreg->decomp_alloc;
    comp->decomp_free = compreg->decomp_free;
    comp->decomp_init = compreg->decomp_init;
    comp->decomp_reset = compreg->decomp_reset;
    comp->decompress = compreg->decompress;
    comp->incomp = compreg->incomp;
    comp->decomp_stat = compreg->decomp_stat;

    TAILQ_INSERT_TAIL(&ppp_comp_head, comp, next);
    
    *compref = comp;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_comp_deregister(ppp_comp_ref *compref)
{	
    struct ppp_comp	*comp = (struct ppp_comp *)compref;

    if (comp == NULL)	/* sanity check */
        return(EINVAL);

    TAILQ_REMOVE(&ppp_comp_head, comp, next);
    
    FREE(comp, M_TEMP);
    return(0);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_comp_setcompressor(struct ppp_if *wan, struct ppp_option_data *odp)
{
    int 			error = 0, nb;
    struct ppp_comp *cp;
    u_char 			ccp_option[CCP_MAX_OPTION_LENGTH];
    user_addr_t		ptr;
    int				transmit;
    
    if (proc_is64bit(current_proc())) {
        struct ppp_option_data64 *odp64 = (struct ppp_option_data64 *)odp;

        nb = odp64->length;
        ptr = odp64->ptr;
        transmit = odp64->transmit;
    } else {
        struct ppp_option_data32 *odp32 = (struct ppp_option_data32 *)odp;

        nb = odp32->length;
        ptr = CAST_USER_ADDR_T(odp32->ptr);
        transmit = odp32->transmit;
    }
    
    if (nb > sizeof(ccp_option))
        nb = sizeof(ccp_option);

    if (error = copyin(ptr, ccp_option, nb))
        return (error);

    if (ccp_option[1] < 2)	/* preliminary check on the length byte */
        return (EINVAL);

    cp = ppp_comp_find(ccp_option[0]);
    if (cp == 0) {
        LOGDBG(wan->net, ("ppp%d: no compressor for [%x %x %x], %x\n",
                ifnet_unit(wan->net), ccp_option[0], ccp_option[1],
                ccp_option[2], nb));

        return EINVAL;	/* no handler found */
    }

    if (transmit) {
        if (wan->xc_state)
            (*wan->xcomp->comp_free)(wan->xc_state);
        wan->xcomp = cp;
        wan->xc_state = cp->comp_alloc(ccp_option, nb);
        if (!wan->xc_state) {
            error = ENOMEM;
            LOGDBG(wan->net, ("ppp%d: comp_alloc failed\n", ifnet_unit(wan->net)));
        }
        wan->sc_flags &= ~SC_COMP_RUN;
    }
    else {
        if (wan->rc_state)
            (*wan->rcomp->decomp_free)(wan->rc_state);
        wan->rcomp = cp;
        wan->rc_state = cp->decomp_alloc(ccp_option, nb);
        if (!wan->rc_state) {
            error = ENOMEM;
            LOGDBG(wan->net, ("ppp%d: decomp_alloc failed\n", ifnet_unit(wan->net)));
        }
        wan->sc_flags &= ~SC_DECOMP_RUN;
    }
    
    return error;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_comp_getstats(struct ppp_if *wan, struct ppp_comp_stats *stats)
{

    bzero(stats, sizeof(struct ppp_comp_stats));
    if (wan->xc_state)
        (*wan->xcomp->comp_stat)(wan->xc_state, &stats->c);
    if (wan->rc_state)
        (*wan->rcomp->decomp_stat)(wan->rc_state, &stats->d);
}

/* -----------------------------------------------------------------------------
Handle a CCP packet.  `rcvd' is 1 if the packet was received,
0 if it is about to be transmitted.
mbuf points to the ccp payload (doesn't include FF03 and 80FD)
----------------------------------------------------------------------------- */
void ppp_comp_ccp(struct ppp_if *wan, mbuf_t m, int rcvd)
{
    u_char 	*p = mbuf_data(m);
    int 	slen;
    
    slen = CCP_LENGTH(p);
    if (slen > mbuf_pkthdr_len(m)) {
        LOGDBG(wan->net, ("ppp_comp_ccp: not enough data in mbuf (expected = %d, got = %d)\n",
		   slen, mbuf_pkthdr_len(m)));
	return;
    }
    
    switch (CCP_CODE(p)) {
    case CCP_CONFREQ:
    case CCP_TERMREQ:
    case CCP_TERMACK:
	/* CCP must be going down - disable compression */
        wan->sc_flags &= ~(rcvd ? SC_COMP_RUN : SC_DECOMP_RUN);
	break;

    case CCP_CONFACK:
	if (wan->sc_flags & SC_CCP_OPEN && !(wan->sc_flags & SC_CCP_UP)
	    && slen >= CCP_HDRLEN + CCP_OPT_MINLEN
	    && slen >= CCP_OPT_LENGTH(p + CCP_HDRLEN) + CCP_HDRLEN) {
	    if (rcvd) {
		/* peer is agreeing to send compressed packets. */
		if (wan->rc_state
		    && (*wan->rcomp->decomp_init)
			(wan->rc_state, p + CCP_HDRLEN, slen - CCP_HDRLEN,
			 ifnet_unit(wan->net), 0, wan->mru, ifnet_flags(wan->net) & IFF_DEBUG)) {
		    wan->sc_flags |= SC_DECOMP_RUN;
		    wan->sc_flags &= ~(SC_DC_ERROR | SC_DC_FERROR);
		}
	    } else {
		/* we're agreeing to send compressed packets. */
		if (wan->xc_state
		    && (*wan->xcomp->comp_init)
			(wan->xc_state, p + CCP_HDRLEN, slen - CCP_HDRLEN,
			 ifnet_unit(wan->net), 0, ifnet_mtu(wan->net), ifnet_flags(wan->net) & IFF_DEBUG)) {
		    wan->sc_flags |= SC_COMP_RUN;
		}
	    }
	}
	break;

    case CCP_RESETACK:
	if (wan->sc_flags & SC_CCP_UP) {
	    if (rcvd) {
		if (wan->rc_state && (wan->sc_flags & SC_DECOMP_RUN)) {
		    (*wan->rcomp->decomp_reset)(wan->rc_state);
		    wan->sc_flags &= ~SC_DC_ERROR;
		}
	    } else {
		if (wan->xc_state && (wan->sc_flags & SC_COMP_RUN))
		    (*wan->xcomp->comp_reset)(wan->xc_state);
	    }
	}
	break;
    }
}

/* -----------------------------------------------------------------------------
CCP is down; free (de)compressor state if necessary.
----------------------------------------------------------------------------- */
void ppp_comp_close(struct ppp_if *wan)
{

    wan->sc_flags &= ~(SC_CCP_OPEN || SC_CCP_UP || SC_COMP_RUN || SC_DECOMP_RUN);
    if (wan->xc_state) {
	(*wan->xcomp->comp_free)(wan->xc_state);
	wan->xc_state = NULL;
    }
    if (wan->rc_state) {
	(*wan->rcomp->decomp_free)(wan->rc_state);
	wan->rc_state = NULL;
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_comp_logmbuf(char *msg, mbuf_t m) 
{
    int 	i, lcount, copycount, count;
    char 	lbuf[16], *data;

    if (m == NULL)
        return;

    IOLog("%s: \n", msg);

    for (count = mbuf_len(m), data = mbuf_data(m); m != NULL; ) {
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

/* -----------------------------------------------------------------------------
return codes :
> 0 : compression done, buffer has changed, return new lenght
0 : compression not done, buffer has not changed, lenght is unchanged
< 0 : compression not done because of error, return -error 
----------------------------------------------------------------------------- */
int ppp_comp_compress(struct ppp_if *wan, mbuf_t *m)
{    
    if (wan->xc_state == 0 || (wan->sc_flags & SC_CCP_UP) == 0)
        return COMP_NOTDONE;
    
    return wan->xcomp->compress(wan->xc_state, m);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_comp_incompress(struct ppp_if *wan, mbuf_t m)
{
    
    if ((wan->rc_state == 0) || (wan->sc_flags & (SC_DC_ERROR | SC_DC_FERROR)))
        return 0;
        
    /* Uncompressed frame - pass to decompressor so it can update its dictionary if necessary. */
    wan->rcomp->incomp(wan->rc_state, m);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_comp_decompress(struct ppp_if *wan, mbuf_t *m)
{
    int err;
    
    if ((wan->rc_state == 0) || (wan->sc_flags & (SC_DC_ERROR | SC_DC_FERROR)))
        return DECOMP_ERROR;
            
    err = wan->rcomp->decompress(wan->rc_state, m);
    if (err != DECOMP_OK) {
        if (err == DECOMP_FATALERROR)
            wan->sc_flags |= SC_DC_FERROR;
        wan->sc_flags |= SC_DC_ERROR;
        ppp_if_error(wan->net);
    }

    return err;	
}
