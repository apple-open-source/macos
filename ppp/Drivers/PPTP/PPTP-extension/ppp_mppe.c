/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * ppp_mppe.c - MPPE "compressor/decompressor" module.
 *
 * Copyright (c) 1994 Árpád Magosányi <mag@bunuel.tii.matav.hu>
 * All rights reserved.
 * Copyright (c) 1999 Tim Hockin, Cobalt Networks Inc. <thockin@cobaltnet.com>
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, provided that the above copyright
 * notice appears in all copies.  This software is provided without any
 * warranty, express or implied. The Australian National University
 * makes no representations about the suitability of this software for
 * any purpose.
 *
 * IN NO EVENT SHALL THE AUSTRALIAN NATIONAL UNIVERSITY BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
 * THE AUSTRALIAN NATIONAL UNIVERSITY HAS BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * THE AUSTRALIAN NATIONAL UNIVERSITY SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE AUSTRALIAN NATIONAL UNIVERSITY HAS NO
 * OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
 * OR MODIFICATIONS.
 *
 * From: deflate.c,v 1.1 1996/01/18 03:17:48 paulus Exp
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <machine/spl.h>
#include <kern/clock.h>

#include <IOKit/IOLib.h>

#include <net/if_types.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <net/bpf.h>

#include "slcompress.h"
#include "ppp_defs.h"
#include "ppp_comp.h"
#include "crypto/rc4.h"
#include "crypto/sha1.h"
#include "ppp_mppe.h"

/*
 * State for a mppe "(de)compressor".
 */
struct ppp_mppe_state {
    unsigned int	ccount; /*coherency count */
    struct rc4_state	rc4_state;
    unsigned char	session_key[MPPE_MAX_KEY_LEN];
    unsigned char	master_key[MPPE_MAX_KEY_LEN];
    int			keylen;
    int                 stateless;
    int                 decomp_error;
    unsigned int	bits;
    int			unit;
    int			debug;
    int			mru;
    struct compstat 	stats;
};

#define MPPE_CCOUNT_FROM_PACKET(ibuf)	((((ibuf)[0] & 0x0f) << 8) + (ibuf)[1])
#define MPPE_BITS(ibuf) 	((ibuf)[0] & 0xf0 )
#define MPPE_CTRLHI(state)	((((state)->ccount & 0xf00)>>8)|((state)->bits))
#define MPPE_CTRLLO(state)	((state)->ccount & 0xff)
 

static void	*mppe_comp_alloc __P((unsigned char *, int));
static void	mppe_comp_free __P((void *));
static int	mppe_comp_init __P((void *, unsigned char *,
					int, int, int, int, int));
static int	mppe_compress __P((void *, mbuf_t *));
static void	mppe_incomp __P((void *, mbuf_t ));
static int	mppe_decompress __P((void *, mbuf_t *));
static void	mppe_comp_reset __P((void *));
static void	mppe_comp_stats __P((void *, struct compstat *));

static ppp_comp_ref 	ppp_mppe_ref;
static u_char 		ppp_mppe_tmp[2048];

/* -----------------------------------------------------------------------------
register the compressor to ppp 
----------------------------------------------------------------------------- */
int
ppp_mppe_init(void)
{  
    struct ppp_comp_reg reg = {
        CI_MPPE,			/* compress_proto */
        mppe_comp_alloc,		/* comp_alloc */
        mppe_comp_free,			/* comp_free */
        mppe_comp_init,			/* comp_init */
        mppe_comp_reset,		/* comp_reset */
        mppe_compress,			/* compress */
        mppe_comp_stats,		/* comp_stat */
        mppe_comp_alloc,		/* decomp_alloc */
        mppe_comp_free,			/* decomp_free */
        mppe_comp_init,			/* decomp_init */
        mppe_comp_reset,		/* decomp_reset */
        mppe_decompress,		/* decompress */
        mppe_incomp,			/* incomp */
        mppe_comp_stats,		/* decomp_stat */
    };

    return ppp_comp_register(&reg, &ppp_mppe_ref);
}
     
/* -----------------------------------------------------------------------------
unregister the compressor to ppp 
----------------------------------------------------------------------------- */
int
ppp_mppe_dispose(void)
{
    ppp_comp_deregister(ppp_mppe_ref);
    return 0;
}

/* -----------------------------------------------------------------------------
 Algorithm for key derivation from RFC 3079
 InitialKey is 8 octets long for 56-bit and 40-bit
 session keys, 16 octets long for 128 bit session keys.
 CurrentKey is same as InitialSessionKey when this
 routine is called for the first time for the session.
 ResultKey contains the new key on output.
----------------------------------------------------------------------------- */
static void
GetNewKeyFromSHA(unsigned char *initialKey, unsigned char *currentKey, 
    unsigned long keyLen, unsigned char *resultKey)
{

    struct sha1_ctxt 	context;
    unsigned char 	digest[20];
    static unsigned char pad1[40] =
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static unsigned char pad2[40] =
        {0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
        0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
        0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
        0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2};

    sha1_init(&context);
    sha1_loop(&context, initialKey, keyLen);
    sha1_loop(&context, pad1, 40);
    sha1_loop(&context, currentKey, keyLen);
    sha1_loop(&context, pad2, 40);
    sha1_result(&context, digest);
    bcopy(digest, resultKey, keyLen);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
mppe_synchronize_key(struct ppp_mppe_state *state)
{

    /* get new keys and flag our state as such */
    rc4_init(&(state->rc4_state), state->session_key, state->keylen);

    state->bits=MPPE_BIT_FLUSHED|MPPE_BIT_ENCRYPTED;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
mppe_initialize_key(struct ppp_mppe_state *state)
{

    /* generate new session keys */
    GetNewKeyFromSHA(state->master_key, state->master_key, state->keylen, state->session_key);

    if(state->keylen == 8) {
	/* cripple them from 64bit->40bit */
        state->session_key[0] = MPPE_40_SALT0;
        state->session_key[1] = MPPE_40_SALT1;
        state->session_key[2] = MPPE_40_SALT2;
    }

    mppe_synchronize_key(state);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
mppe_change_key(struct ppp_mppe_state *state)
{
    unsigned char InterimKey[16];

    /* get temp keys */
    GetNewKeyFromSHA(state->master_key, state->session_key,
	state->keylen, InterimKey);

    /* build RC4 keys from the temp keys */
    rc4_init(&(state->rc4_state), InterimKey, state->keylen);

    /* make new session keys */
    rc4_crypt(&(state->rc4_state), InterimKey, state->session_key, state->keylen);

    if(state->keylen == 8)
    {
	/* cripple them from 64->40 bits*/
        state->session_key[0] = MPPE_40_SALT0;
        state->session_key[1] = MPPE_40_SALT1;
        state->session_key[2] = MPPE_40_SALT2;
    }

    /* make the final rc4 keys */
    rc4_init(&(state->rc4_state), state->session_key, state->keylen);

    state->bits |= MPPE_BIT_FLUSHED;
}


#ifdef DEBUG
/* Utility procedures to print a buffer in hex/ascii */
static void
ppp_print_hex (register __u8 *out, const __u8 *in, int count)
{
	register __u8 next_ch;
	static char hex[] = "0123456789ABCDEF";

	while (count-- > 0) {
		next_ch = *in++;
		*out++ = hex[(next_ch >> 4) & 0x0F];
		*out++ = hex[next_ch & 0x0F];
		++out;
	}
}


static void
ppp_print_char (register __u8 *out, const __u8 *in, int count)
{
	register __u8 next_ch;

	while (count-- > 0) {
		next_ch = *in++;

		if (next_ch < 0x20 || next_ch > 0x7e)
			*out++ = '.';
		else {
			*out++ = next_ch;
			if (next_ch == '%')   /* printk/syslogd has a bug !! */
				*out++ = '%';
		}
	}
	*out = '\0';
}


static void
ppp_print_buffer (const __u8 *name, const __u8 *buf, int count)
{
	__u8 line[44];

	if (name != (__u8 *) NULL)
		printk (KERN_DEBUG "ppp: %s, count = %d\n", name, count);

	while (count > 8) {
		memset (line, 32, 44);
		ppp_print_hex (line, buf, 8);
		ppp_print_char (&line[8 * 3], buf, 8);
		printk (KERN_DEBUG "%s\n", line);
		count -= 8;
		buf += 8;
	}

	if (count > 0) {
		memset (line, 32, 44);
		ppp_print_hex (line, buf, count);
		ppp_print_char (&line[8 * 3], buf, count);
		printk (KERN_DEBUG "%s\n", line);
	}
}
#endif

/* -----------------------------------------------------------------------------
cleanup the compressor 
----------------------------------------------------------------------------- */
static void
mppe_comp_free(void *arg)
{
    struct ppp_mppe_state *state = (struct ppp_mppe_state *) arg;

    if (state) {
	    FREE(state, M_TEMP);
    }
}

/* -----------------------------------------------------------------------------
allocate space for a compressor 
----------------------------------------------------------------------------- */
static void *
mppe_comp_alloc(unsigned char *options, int opt_len)
{
    struct ppp_mppe_state *state;

    if ((opt_len > (CILEN_MPPE + MPPE_MAX_KEY_LEN))
       || options[0] != CI_MPPE || options[1] != CILEN_MPPE) {
	    IOLog("compress rejected: opt_len=%u,o[0]=%x,o[1]=%x\n",
		opt_len,options[0],options[1]);
	    return NULL;
    }

    MALLOC(state, struct ppp_mppe_state *, sizeof(*state), M_TEMP, M_WAITOK);
    if (state == NULL)
	return NULL;

    bzero(state, sizeof (struct ppp_mppe_state));

    /* just copy the keys */
    bcopy(options + CILEN_MPPE, state->master_key, MPPE_MAX_KEY_LEN);
    bcopy(options + CILEN_MPPE, state->session_key, MPPE_MAX_KEY_LEN);

    return (void *) state;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int
mppe_comp_init(void *arg, unsigned char *options, int opt_len, int unit, 
		int hdrlen, int mtu, int debug)
{
    struct ppp_mppe_state *state = (struct ppp_mppe_state *)arg;
    unsigned char mppe_opts;

    if (opt_len != CILEN_MPPE
       || options[0] != CI_MPPE || options[1] != CILEN_MPPE) {
    	IOLog("mppe compress rejected: opt_len=%u,o[0]=%x,o[1]=%x\n",
	    opt_len, options[0], options[1]);
	return 0;
    }

    MPPE_CI_TO_OPTS(&options[2], mppe_opts);
    if (mppe_opts & MPPE_OPT_40)
	state->keylen = 8;
    else if (mppe_opts & MPPE_OPT_128)
	state->keylen = 16;
    else {
	IOLog("mppe compress rejected, unknown key length\n");
	return 0;
    }

    state->ccount = 0;
    state->unit  = unit;
    state->debug = debug;
    state->stateless = mppe_opts & MPPE_OPT_STATEFUL ? 0 : 1;

    mppe_initialize_key(state);

    return 1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
mppe_comp_reset(void *arg)
{
    struct ppp_mppe_state *state = (struct ppp_mppe_state *)arg;

    (state->stats).in_count = 0;
    (state->stats).bytes_out = 0;
    (state->stats).ratio = 0;

    mppe_synchronize_key(state);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
mppe_update_count(struct ppp_mppe_state *state)
{
    if(!state->stateless)
    {
        if ( 0xff == (state->ccount&0xff)){ 
	    /* time to change keys */
	    if ( 0xfff == (state->ccount&0xfff)){
		state->ccount = 0;
	    } else {
		(state->ccount)++;
	    }
	    mppe_change_key(state);
        } else {
            state->ccount++;
        }
    } else {
        if ( 0xFFF == (state->ccount & 0xFFF)) {
	    state->ccount = 0;
        } else {
	    (state->ccount)++;
	}
       	mppe_change_key(state);
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int
mppe_compress(void *arg, mbuf_t *m)
{
    struct ppp_mppe_state 	*state = (struct ppp_mppe_state *) arg;
    mbuf_t			m1;
    int 			isize, proto = ntohs(*(u_int16_t *)mbuf_data(*m));
    u_char 			*p;

    /* Check that the protocol is in the range we handle. */
    if (proto < 0x0021 || proto > 0x00FA )
	return COMP_NOTDONE;

    for (m1 = *m, isize = 0; m1 ; m1 = mbuf_next(m1))
        isize += mbuf_len(m1);

    if (mbuf_getpacket(MBUF_WAITOK, &m1) != 0) {
	IOLog("mppe_compress: no mbuf available\n");
        return COMP_NOTDONE;
    }

    /* fist transform mbuf into linear data buffer */
	if (isize > sizeof(ppp_mppe_tmp)) {
		IOLog("%s: packet too big\n",__FUNCTION__);
        return COMP_NOTDONE;
	}

    mbuf_copydata(*m, 0, isize, ppp_mppe_tmp);

#ifdef DEBUG
    ppp_print_buffer("mppe_encrypt", ppp_mppe_tmp, isize);
#endif
    
    p = mbuf_data(m1);
    p[0] = MPPE_CTRLHI(state);
    p[1] = MPPE_CTRLLO(state);

    state->bits=MPPE_BIT_ENCRYPTED;
    mppe_update_count(state);

    /* read from rptr, write to wptr */
    rc4_crypt(&(state->rc4_state), ppp_mppe_tmp, p + 2, isize);

    mbuf_freem(*m);
    mbuf_setlen(m1, isize + 2);
    mbuf_pkthdr_setlen(m1, isize + 2);
    *m = m1;
    
    (state->stats).comp_bytes += isize;
    (state->stats).comp_packets++;

#ifdef DEBUG
    ppp_print_buffer("mppe_encrypt out", p, isize + 2);
#endif

    return COMP_OK;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
mppe_comp_stats(void *arg, struct compstat *stats)
{
    struct ppp_mppe_state *state = (struct ppp_mppe_state *)arg;

    /* since we don't REALLY compress at all, this should be OK */
    (state->stats).in_count = (state->stats).unc_bytes;
    (state->stats).bytes_out = (state->stats).comp_bytes;

#if 0 /* FP not allowed in the kernel */
    /* this _SHOULD_ always be 1 */
    (state->stats).ratio = (state->stats).in_count/(state->stats).bytes_out;
#endif

    *stats = state->stats;
   
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int
mppe_decompress(void *arg, mbuf_t *m)
{
    struct ppp_mppe_state 	*state = (struct ppp_mppe_state *) arg;
    mbuf_t			m1;
    int 			seq, isize;

    for (m1 = *m, isize = 0; m1 ; m1 = mbuf_next(m1))
        isize += mbuf_len(m1);
        
    if (isize <= 2) {
	if (state->debug) {
	    IOLog("mppe_decompress%d: short packet (len=%d)\n",
		state->unit, isize);
	}
	return DECOMP_ERROR;
    }
	if (isize > sizeof(ppp_mppe_tmp)) {
		IOLog("%s: packet too big\n",__FUNCTION__);
        return COMP_NOTDONE;
	}
		
    /* fist transform mbuf into linear data buffer */
    mbuf_copydata(*m, 0, isize, ppp_mppe_tmp);

    /* Check the sequence number. */
    seq = MPPE_CCOUNT_FROM_PACKET(ppp_mppe_tmp);

    if(!state->stateless && (MPPE_BITS(ppp_mppe_tmp) & MPPE_BIT_FLUSHED)) {
        state->decomp_error = 0;
        state->ccount = seq;
    }

    if(state->decomp_error) {
        return DECOMP_ERROR;
    }

    if (seq != state->ccount) {
	if (state->debug) {
	    IOLog("mppe_decompress%d: bad seq # %d, expected %d\n",
		   state->unit, seq, state->ccount);
	}

        while(state->ccount != seq) {
            mppe_update_count(state);
	}
    }

    /*
     * Compressors don't deal with A/C and compression proto field.
     * However, the inner protocol field comes from the decompressed data.
     */

    if(!(MPPE_BITS(ppp_mppe_tmp) & MPPE_BIT_ENCRYPTED)) {
        IOLog("ERROR: not an encrypted packet");
        mppe_synchronize_key(state);
	return DECOMP_ERROR;
    } else {
	if(!state->stateless && (MPPE_BITS(ppp_mppe_tmp) & MPPE_BIT_FLUSHED))
	    mppe_synchronize_key(state);
	mppe_update_count(state);

        if (mbuf_getpacket(MBUF_WAITOK, &m1) != 0) {
            IOLog("mppe_decompress: no mbuf available\n");
            return DECOMP_ERROR;
        }
        
	/* decrypt - adjust for MPPE_OVHD - mru should be OK */
	rc4_crypt(&(state->rc4_state), ppp_mppe_tmp + 2, mbuf_data(m1), isize - 2);

        mbuf_freem(*m);
        mbuf_setlen(m1, isize - 2);
        mbuf_pkthdr_setlen(m1, isize - 2);
        *m = m1;

	(state->stats).unc_bytes += (isize - 2);
	(state->stats).unc_packets ++;

	return DECOMP_OK;
    }
}

/* -----------------------------------------------------------------------------
Incompressible data has arrived - add it to the history 
----------------------------------------------------------------------------- */
static void
mppe_incomp(void *arg, mbuf_t m)
{
    struct ppp_mppe_state *state = (struct ppp_mppe_state *)arg;

    (state->stats).inc_bytes += mbuf_pkthdr_len(m);
    (state->stats).inc_packets++;
}
