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



//#define PPP_COMPRESS 1
#ifdef PPP_COMPRESS



#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <machine/spl.h>
#include <net/if.h>
#include <net/netisr.h>

#include "ppp_global.h"
#include <sys/syslog.h>

#define PACKETPTR	struct mbuf *
#include "../../include/net/ppp-comp.h"

#include "ppp_if.h"
#include "ppp_comp.h"

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

// List of compressors we know about.

extern struct compressor ppp_bsd_compress;
extern struct compressor ppp_deflate, ppp_deflate_draft;

static struct compressor *ppp_compressors[8] = {
#if DO_BSD_COMPRESS
#if defined(PPP_BSDCOMP)
    &ppp_bsd_compress,
#endif
#endif
#if DO_DEFLATE
#if defined(PPP_DEFLATE)
    &ppp_deflate,
    &ppp_deflate_draft,
#endif
#endif
    NULL
};



/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_comp_alloc(struct ppp_if *sc)
{
    sc->sc_xc_state = NULL;
    sc->sc_rc_state = NULL;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_comp_dealloc(struct ppp_if *sc)
{
    ppp_comp_close(sc);
    sc->sc_xc_state = NULL;
    sc->sc_rc_state = NULL;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_comp_setcompressor(struct ppp_if *sc, struct ppp_option_data *odp)
{
    int 			s, error = 0, nb;
    struct compressor 		**cp;
    u_char 			ccp_option[CCP_MAX_OPTION_LENGTH];

    nb = odp->length;
    if (nb > sizeof(ccp_option))
        nb = sizeof(ccp_option);

    if ((error = copyin(odp->ptr, ccp_option, nb)) != 0)
        return (error);

    if (ccp_option[1] < 2)	/* preliminary check on the length byte */
        return (EINVAL);

    for (cp = ppp_compressors; *cp; cp++) {
        if ((*cp)->compress_proto == ccp_option[0])
            break;
    }

    if (*cp) {
         // Found a handler for the protocol - try to allocate a compressor or decompressor.
        error = 0;
        s = splnet();
        if (odp->transmit) {
            if (sc->sc_xc_state)
                (*sc->sc_xcomp->comp_free)(sc->sc_xc_state);
            sc->sc_xcomp = *cp;
            sc->sc_xc_state = (*cp)->comp_alloc(ccp_option, nb);
            if (!sc->sc_xc_state) {
                error = ENOBUFS;
                if (sc->sc_flags & SC_DEBUG)
                    printf("ppp%d: comp_alloc failed\n", sc->sc_if.if_unit);
            }
            splimp();
            sc->sc_flags &= ~SC_COMP_RUN;
        }
        else {
            if (sc->sc_rc_state)
                (*sc->sc_rcomp->decomp_free)(sc->sc_rc_state);
            sc->sc_rcomp = *cp;
            sc->sc_rc_state = (*cp)->decomp_alloc(ccp_option, nb);
            if (!sc->sc_rc_state) {
                error = ENOBUFS;
                if (sc->sc_flags & SC_DEBUG)
                    printf("ppp%d: decomp_alloc failed\n", sc->sc_if.if_unit);
            }
            splimp();
            sc->sc_flags &= ~SC_DECOMP_RUN;
        }
        splx(s);
        return error;
    }

    if (sc->sc_flags & SC_DEBUG)
        printf("ppp%d: no compressor for [%x %x %x], %x\n",
               sc->sc_if.if_unit, ccp_option[0], ccp_option[1],
               ccp_option[2], nb);

    return EINVAL;	/* no handler found */
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ppp_comp_getstats(struct ppp_if *sc, struct ppp_comp_stats *stats)
{

    bzero(stats, sizeof(struct ppp_comp_stats));
    if (sc->sc_xc_state)
        (*sc->sc_xcomp->comp_stat)(sc->sc_xc_state, &stats->c);
    if (sc->sc_rc_state)
        (*sc->sc_rcomp->decomp_stat)(sc->sc_rc_state, &stats->d);
}

/* -----------------------------------------------------------------------------
Handle a CCP packet.  `rcvd' is 1 if the packet was received,
0 if it is about to be transmitted.
----------------------------------------------------------------------------- */
void ppp_ccp(struct ppp_if *sc, struct mbuf *m, int rcvd)
{
    u_char *dp, *ep;
    struct mbuf *mp;
    int slen, s;

    /*
     * Get a pointer to the data after the PPP header.
     */
    if (m->m_len <= PPP_HDRLEN) {
	mp = m->m_next;
	if (mp == NULL)
	    return;
	dp = (mp != NULL)? mtod(mp, u_char *): NULL;
    } else {
	mp = m;
	dp = mtod(mp, u_char *) + PPP_HDRLEN;
    }

    ep = mtod(mp, u_char *) + mp->m_len;
    if (dp + CCP_HDRLEN > ep)
	return;
    slen = CCP_LENGTH(dp);
    if (dp + slen > ep) {
	if (sc->sc_flags & SC_DEBUG)
	    printf("if_ppp/ccp: not enough data in mbuf (%p+%x > %p+%x)\n",
		   dp, slen, mtod(mp, u_char *), mp->m_len);
	return;
    }

    switch (CCP_CODE(dp)) {
    case CCP_CONFREQ:
    case CCP_TERMREQ:
    case CCP_TERMACK:
	/* CCP must be going down - disable compression */
	if (sc->sc_flags & SC_CCP_UP) {
	    s = splimp();
	    sc->sc_flags &= ~(SC_CCP_UP | SC_COMP_RUN | SC_DECOMP_RUN);
	    splx(s);
	}
	break;

    case CCP_CONFACK:
	if (sc->sc_flags & SC_CCP_OPEN && !(sc->sc_flags & SC_CCP_UP)
	    && slen >= CCP_HDRLEN + CCP_OPT_MINLEN
	    && slen >= CCP_OPT_LENGTH(dp + CCP_HDRLEN) + CCP_HDRLEN) {
	    if (!rcvd) {
		/* we're agreeing to send compressed packets. */
		if (sc->sc_xc_state != NULL
		    && (*sc->sc_xcomp->comp_init)
			(sc->sc_xc_state, dp + CCP_HDRLEN, slen - CCP_HDRLEN,
			 sc->sc_if.if_unit, 0, sc->sc_flags & SC_DEBUG)) {
		    s = splimp();
		    sc->sc_flags |= SC_COMP_RUN;
		    splx(s);
		}
	    } else {
		/* peer is agreeing to send compressed packets. */
		if (sc->sc_rc_state != NULL
		    && (*sc->sc_rcomp->decomp_init)
			(sc->sc_rc_state, dp + CCP_HDRLEN, slen - CCP_HDRLEN,
			 sc->sc_if.if_unit, 0, sc->sc_mru,
			 sc->sc_flags & SC_DEBUG)) {
		    s = splimp();
		    sc->sc_flags |= SC_DECOMP_RUN;
		    sc->sc_flags &= ~(SC_DC_ERROR | SC_DC_FERROR);
		    splx(s);
		}
	    }
	}
	break;

    case CCP_RESETACK:
	if (sc->sc_flags & SC_CCP_UP) {
	    if (!rcvd) {
		if (sc->sc_xc_state && (sc->sc_flags & SC_COMP_RUN))
		    (*sc->sc_xcomp->comp_reset)(sc->sc_xc_state);
	    } else {
		if (sc->sc_rc_state && (sc->sc_flags & SC_DECOMP_RUN)) {
		    (*sc->sc_rcomp->decomp_reset)(sc->sc_rc_state);
		    s = splimp();
		    sc->sc_flags &= ~SC_DC_ERROR;
		    splx(s);
		}
	    }
	}
	break;
    }
}

/* -----------------------------------------------------------------------------
CCP is down; free (de)compressor state if necessary.
----------------------------------------------------------------------------- */
void ppp_comp_close(struct ppp_if *sc)
{
    if (sc->sc_xc_state) {
	(*sc->sc_xcomp->comp_free)(sc->sc_xc_state);
	sc->sc_xc_state = NULL;
    }
    if (sc->sc_rc_state) {
	(*sc->sc_rcomp->decomp_free)(sc->sc_rc_state);
	sc->sc_rc_state = NULL;
    }
}
#endif