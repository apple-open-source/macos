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


#include <sys/systm.h>
#include <sys/kpi_mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/domain.h>
#include <kern/locks.h>

#include "../../../Family/if_ppplink.h"
#include "../../../Family/ppp_domain.h"
#include "PPTP.h"
#include "pptp_rfc.h"
#include "pptp_ip.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */
/* Wcast-align fix - cast away alignment warning when buffer is aligned */
#define ALIGNED_CAST(type)	(type)(void *) 

#define PPTP_VER 	1
#define PPTP_TYPE	1

#define PPTP_STATE_XMIT_FULL	0x00000001	/* xmit if full */
#define PPTP_STATE_NEW_SEQUENCE	0x00000002	/* we have a seq number to acknowledge */
#define PPTP_STATE_PEERSTARTED	0x00000004	/* peer has sent its first packet, initial peer_sequence is known */
#define PPTP_STATE_FREEING		0x00000008	/* structure is scheduled to be freed a.s.a.p */

struct pptp_gre {
    u_int8_t 	flags;
    u_int8_t 	flags_vers;
    u_int16_t 	proto_type;
    u_int16_t 	payload_len;
    u_int16_t 	call_id;
    u_int32_t 	seq_num;
    u_int32_t 	ack_num;
};

/*
 * PPTP sequence numbers are 32 bit integers operated
 * on with modular arithmetic.  These macros can be
 * used to compare such integers.
 */
#define SEQ_LT(a,b)     ((int)((a)-(b)) < 0)
#define SEQ_LEQ(a,b)    ((int)((a)-(b)) <= 0)
#define SEQ_GT(a,b)     ((int)((a)-(b)) > 0)
#define SEQ_GEQ(a,b)    ((int)((a)-(b)) >= 0)

struct pptp_elem {
    TAILQ_ENTRY(pptp_elem)	next;
    mbuf_t 		packet;
    u_int32_t			seqno;
};


struct pptp_rfc {

    // administrative info
    TAILQ_ENTRY(pptp_rfc) 	next;
    void 			*host; 			/* pointer back to the hosting structure */
    pptp_rfc_input_callback 	inputcb;		/* callback function when data are present */
    pptp_rfc_event_callback 	eventcb;		/* callback function for events */
    u_int32_t  			flags;			/* miscellaneous flags */
    u_int32_t  			state;			/* state information */
    
    // pptp info
    u_int32_t		peer_address;			/* ip address we are connected to */
    u_int32_t		our_address;			/* our ip address */
    u_int16_t		call_id;			/* our session id */
    u_int16_t		peer_call_id;			/* peer's session id */
    u_int16_t		our_window;			/* our recv window */
    u_int16_t		peer_window;			/* peer's recv window */
    u_int16_t		send_window;			/* current send window */
    u_int16_t		send_timeout;			/* send timeout */
    u_int16_t		recv_timeout;			/* recv timeout */
    u_int32_t		our_last_seq;			/* last seq number we sent */
    u_int32_t		our_last_seq_acked;		/* last seq number acked */
    u_int32_t		peer_last_seq;			/* highest last seq number we received */
    u_int16_t		peer_ppd;			/* peer packet processing delay */
    u_int32_t		maxtimeout;			/* maximum timeout (scaled) */
    u_int32_t		baudrate;			/* tunnel baudrate */

    // Adaptative time-out calculation, see PPTP rfc for details
    u_int32_t		sample_seq;			/* sequence number being currently sampled */
    u_int16_t		sample;				/* sample round trip time measured for the current packet */
    u_int32_t		rtt;				/* calculated round-trip time (scaled) */
    int32_t		dev;				/* deviation time (scaled) */
    u_int32_t		ato;				/* adaptative timeout (scaled) */
	
	TAILQ_HEAD(, pptp_elem) recv_queue;		/* sequenced data recv queue */

};

// Adaptative time-out constants
#define SCALE_FACTOR 	8 			//  shift everything by 8 bits to keep precision
#define ALPHA 		8 			//  1/8
#define BETA 		4 			//  1/4
#define CHI 		4			//
#define DELTA 		2 			//  
#define MIN_TIMEOUT	(2 << SCALE_FACTOR)	//  min timeout will be 1 second
#define MAX_TIMEOUT	(128 << SCALE_FACTOR)	//  max timeout will be 64 seconds


#define ROUND32DIFF(a, b)  	((a >= b) ? (a - b) : (0xFFFFFFFF - b + a + 1))
#define ABS(a) 			(a >= 0 ? a : -a)

#define RECV_TIMEOUT_DEF	2	// 1 second
#define RECV_MAXLEN_DEF		3	// 3 packets max pending

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

TAILQ_HEAD(, pptp_rfc) 	pptp_rfc_head;
extern lck_mtx_t	*ppp_domain_mutex;

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
intialize pptp protocol
----------------------------------------------------------------------------- */
u_int16_t pptp_rfc_init()
{

    pptp_ip_init();
    TAILQ_INIT(&pptp_rfc_head);
    return 0;
}

/* -----------------------------------------------------------------------------
dispose of a pptp protocol
----------------------------------------------------------------------------- */
u_int16_t pptp_rfc_dispose()
{

    if (TAILQ_FIRST(&pptp_rfc_head))
        return 1;
    if (pptp_ip_dispose())
        return 1;
    return 0;
}

/* -----------------------------------------------------------------------------
intialize a new pptp structure
----------------------------------------------------------------------------- */
u_int16_t pptp_rfc_new_client(void *host, void **data,
                         pptp_rfc_input_callback input, 
                         pptp_rfc_event_callback event)
{
    struct pptp_rfc 	*rfc;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
    
    rfc = (struct pptp_rfc *)_MALLOC(sizeof (struct pptp_rfc), M_TEMP, M_WAITOK);
    if (rfc == 0)
        return 1;

    //IOLog("PPTP new_client rfc = %p\n", rfc);

    bzero(rfc, sizeof(struct pptp_rfc));

    rfc->host = host;
    rfc->inputcb = input;
    rfc->eventcb = event;
    rfc->peer_last_seq = 0;
    rfc->our_last_seq_acked = 0;
    rfc->our_last_seq = 0;

    // let's use some default values
    rfc->peer_window = 64;
    rfc->send_window = 32;
    rfc->peer_ppd = 0;
    rfc->maxtimeout = MAX_TIMEOUT; 
    rfc->sample = 0;
    rfc->dev = 0;
    rfc->rtt = rfc->peer_ppd;
    rfc->ato = MIN_TIMEOUT;

    TAILQ_INIT(&rfc->recv_queue);

    *data = rfc;

    TAILQ_INSERT_TAIL(&pptp_rfc_head, rfc, next);

    return 0;
}

/* -----------------------------------------------------------------------------
dispose of a pptp structure
----------------------------------------------------------------------------- */
void pptp_rfc_free_client(void *data)
{
    struct pptp_rfc 	*rfc = (struct pptp_rfc *)data;
    struct pptp_elem	*recv_elem;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    if (rfc) {
		if (rfc->flags & PPTP_FLAG_DEBUG)
			IOLog("PPTP free (%p)\n", rfc);
		rfc->state |= PPTP_STATE_FREEING;
		rfc->host = 0;
		rfc->inputcb = 0;
		rfc->eventcb = 0;
		while((recv_elem = TAILQ_FIRST(&rfc->recv_queue))) {
			TAILQ_REMOVE(&rfc->recv_queue, recv_elem, next);
			mbuf_freem(recv_elem->packet);
			_FREE(recv_elem, M_TEMP);
		}
	}
}

/* -----------------------------------------------------------------------------
main body of function used to be pptp_rfc_fasttimer... called by protocol family when fast timer expired:
now it's called when slow timer expired because of <rdar://problem/7617885> 
----------------------------------------------------------------------------- */
static void pptp_rfc_delayed_ack(struct pptp_rfc *rfc)
{
    struct pptp_gre	*p, p_data;
    u_int16_t 		len;
    mbuf_t			m;

    if (rfc->state & PPTP_STATE_NEW_SEQUENCE) {
        
        if ((mbuf_gethdr(MBUF_DONTWAIT, MBUF_TYPE_DATA, &m)) != 0)
            return;

        // build an ack packet, without data
        len = sizeof(struct pptp_gre) - 4;
        mbuf_setlen(m, len);
        mbuf_pkthdr_setlen(m, len);

        // probably some of it should move to pptp_ip when we implement 
        // a more modular GRE handler
        p = &p_data;
	memcpy(p, mbuf_data(m), sizeof(p_data));
        p->flags = PPTP_GRE_FLAGS_K;
        p->flags_vers = PPTP_GRE_VER | PPTP_GRE_FLAGS_A;
        p->proto_type = htons(PPTP_GRE_TYPE);
        p->payload_len = 0; 
        p->call_id = htons(rfc->peer_call_id);
        /* XXX use seq_num in the structure to put the ack */
        p->seq_num = htonl(rfc->peer_last_seq);
        rfc->state &= ~PPTP_STATE_NEW_SEQUENCE;
	memcpy(mbuf_data(m), p, sizeof(p_data));

        //IOLog("pptp_rfc_delayed_ack, output delayed ACK = %d\n", rfc->peer_last_seq);
        pptp_ip_output(m, rfc->our_address, rfc->peer_address);
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void pptp_rfc_input_recv_queue(struct pptp_rfc *rfc)
{
    struct pptp_elem 	*elem = TAILQ_FIRST(&rfc->recv_queue);

	if (elem == 0)
		return;
		
	//IOLog("pptp_rfc_input_recv_queue , unexpected SEQ  = %d (rfc->peer_last_seq = %d)\n", elem->seqno, rfc->peer_last_seq);

	/* warn upper layers of missing packet */
	if (rfc->eventcb)
		(*rfc->eventcb)(rfc->host, PPTP_EVT_INPUTERROR, 0);

	rfc->peer_last_seq = elem->seqno - 1;
	rfc->state |= PPTP_STATE_NEW_SEQUENCE; /* to send ack later */

	do {

		if (elem->seqno == (rfc->peer_last_seq+1)) {		/* another packet to send up */

			rfc->peer_last_seq = elem->seqno;
			TAILQ_REMOVE(&rfc->recv_queue, elem, next);
			(*rfc->inputcb)(rfc->host, elem->packet);
			_FREE(elem, M_TEMP);

		} 
		else {
			rfc->recv_timeout = RECV_TIMEOUT_DEF;
			break;
		}
	} while ((elem = TAILQ_FIRST(&rfc->recv_queue)));
}

/* -----------------------------------------------------------------------------
called by protocol family when fast timer expires
----------------------------------------------------------------------------- */
void pptp_rfc_slowtimer()
{
    struct pptp_rfc  	*rfc;

	rfc = TAILQ_FIRST(&pptp_rfc_head);
	while (rfc) {

		if (rfc->state & PPTP_STATE_FREEING) {
			struct pptp_rfc  	*next_rfc = TAILQ_NEXT(rfc, next);
			TAILQ_REMOVE(&pptp_rfc_head, rfc, next);
			_FREE(rfc, M_TEMP);
			rfc = next_rfc;
			continue;
		}

		pptp_rfc_delayed_ack(rfc);

        if (rfc->send_timeout && (--rfc->send_timeout == 0)) {
                
            //IOLog("pptp_rfc_slowtimer, send timer expires for packet = %d\n", rfc->sample_seq);
            rfc->rtt = DELTA * rfc->rtt;
            rfc->ato = MAX(MIN_TIMEOUT, MIN(rfc->rtt + (CHI * rfc->dev), rfc->maxtimeout));

            rfc->send_window = (rfc->send_window / 2) + (rfc->send_window % 2);
            rfc->sample = 0;
            rfc->our_last_seq_acked = rfc->sample_seq;
            //IOLog("pptp_rfc_slowtimer, new ato = %d, new send window = %d\n", rfc->ato, rfc->send_window);
            
            if (rfc->state & PPTP_STATE_XMIT_FULL) {
                //IOLog("pptp_rfc_slowtimer PPTP_EVT_XMIT_OK\n");
                rfc->state &= ~PPTP_STATE_XMIT_FULL;
                if (rfc->eventcb) 
                    (*rfc->eventcb)(rfc->host, PPTP_EVT_XMIT_OK, 0);
            }
        }
        
        if (rfc->recv_timeout && (--rfc->recv_timeout == 0)) {
		
			pptp_rfc_input_recv_queue(rfc);
		}
		
        if (rfc->sample)
            rfc->sample++;

		rfc = TAILQ_NEXT(rfc, next);
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int16_t pptp_rfc_output(void *data, mbuf_t m)
{
    struct pptp_rfc 	*rfc = (struct pptp_rfc *)data;
    u_int8_t 		*d;
    struct pptp_gre	p;
    mbuf_t m0;
    u_int16_t 		len, i;

    if (rfc->state & PPTP_STATE_FREEING) {
        mbuf_freem(m);
        return ENXIO;
    }

    len = 0;
	i = 0;
    for (m0 = m; m0 != 0; m0 = mbuf_next(m0)) {
        len += mbuf_len(m0);
		i++;
		
		if (i > 32) {
			struct socket 	*so = (struct socket *)rfc->host;
			struct ppp_link *link = ALIGNED_CAST(struct ppp_link *)so->so_tpcb;

			IOLog("PPTP output packet contains too many mbufs, circular route suspected for %s%d\n", ifnet_name(link->lk_ifnet), ifnet_unit(link->lk_ifnet));
			
			mbuf_freem(m);
			return ENETUNREACH;
		};
	}

    // IOLog("PPTP write, len = %d\n", len);
    //d = mtod(m, u_int8_t *);
    //IOLog("PPTP write, data = %x %x %x %x %x %x \n", d[0], d[1], d[2], d[3], d[4], d[5]);

    if (mbuf_prepend(&m, sizeof(struct pptp_gre), MBUF_WAITOK) != 0)		// always include ACK
        return ENOBUFS;
    d = mbuf_data(m);

    // No need to set MBUF_PKTHDR, since m must already be a header
    
    mbuf_pkthdr_setlen(m, len + sizeof(struct pptp_gre));

    bzero(&p, sizeof(struct pptp_gre));
    
    // probably some of it should move to pptp_ip when we implement 
    // a more modular GRE handler
    rfc->our_last_seq++;
    p.flags = PPTP_GRE_FLAGS_K + PPTP_GRE_FLAGS_S;
    p.flags_vers = PPTP_GRE_VER;
    p.proto_type = htons(PPTP_GRE_TYPE);
    p.payload_len = htons(len); 
    p.call_id = htons(rfc->peer_call_id);
    p.seq_num = htonl(rfc->our_last_seq);
    p.flags_vers |= PPTP_GRE_FLAGS_A; // always include ack
    p.ack_num = htonl(rfc->peer_last_seq);
    rfc->state &= ~PPTP_STATE_NEW_SEQUENCE;
    memcpy(d, &p, sizeof(struct pptp_gre));     // Wcast-align fix - memcpy for unaligned access

    if (ROUND32DIFF(rfc->our_last_seq, rfc->our_last_seq_acked) >= rfc->send_window) {
        //IOLog("pptp_rfc_output PPTP_STATE_XMIT_FULL\n");
        rfc->state |= PPTP_STATE_XMIT_FULL;
        if (rfc->eventcb)
            (*rfc->eventcb)(rfc->host, PPTP_EVT_XMIT_FULL, 0);
    }    
    
    if (rfc->sample == 0) {
        rfc->sample = 1;
        rfc->sample_seq = rfc->our_last_seq;
        rfc->send_timeout = rfc->ato >> SCALE_FACTOR;
        //IOLog("pptp_rfc_output, will sample packet = %d, timeout = %d\n", rfc->our_last_seq, rfc->ato);
    }
    //IOLog("pptp_rfc_output, SEND packet = %d\n", rfc->our_last_seq);

    pptp_ip_output(m, rfc->our_address, rfc->peer_address);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int16_t pptp_rfc_command(void *data, u_int32_t cmd, void *cmddata)
{
    struct pptp_rfc 	*rfc = (struct pptp_rfc *)data;
    u_int16_t		error = 0;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);

    switch (cmd) {

        case PPTP_CMD_SETFLAGS:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                IOLog("PPTP command (%p): set flags = 0x%x\n", rfc, *(u_int32_t *)cmddata);
            rfc->flags = *(u_int32_t *)cmddata;
            break;

        case PPTP_CMD_SETWINDOW:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                IOLog("PPTP command (%p): set window = 0x%x\n", rfc, *(u_int16_t *)cmddata);
            rfc->our_window = *(u_int16_t *)cmddata;
            break;
		
        case PPTP_CMD_SETBAUDRATE:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                IOLog("PPTP command (%p): set baudrate of the tunnel = %d\n", rfc, *(u_int32_t *)cmddata);
			rfc->baudrate = *(u_int32_t *)cmddata;	
            break;

        case PPTP_CMD_GETBAUDRATE:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                IOLog("PPTP command (%p): get baudrate of the tunnel = %d\\n", rfc, rfc->baudrate);
            *(u_int32_t *)cmddata = rfc->baudrate;
            break;

        case PPTP_CMD_SETPEERWINDOW:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                IOLog("PPTP command (%p): set peer window = 0x%x\n", rfc, *(u_int16_t *)cmddata);
            rfc->peer_window = *(u_int16_t *)cmddata;
            rfc->send_window = (rfc->peer_window / 2) + (rfc->peer_window % 2);
            break;

        case PPTP_CMD_SETCALLID:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                IOLog("PPTP command (%p): set call id = 0x%x\n", rfc, *(u_int16_t *)cmddata);
            rfc->call_id = *(u_int16_t *)cmddata;
            break;

        case PPTP_CMD_SETPEERCALLID:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                IOLog("PPTP command (%p): set peer call id = 0x%x\n", rfc, *(u_int16_t *)cmddata);
            rfc->peer_call_id = *(u_int16_t *)cmddata;
            break;

        // set the peer address to specify IP address
        // MUST be called before using the connection
        case PPTP_CMD_SETPEERADDR:	
            if (rfc->flags & PPTP_FLAG_DEBUG) {
                u_char *p = cmddata;
                IOLog("PPTP command (%p): set peer IP address = %d.%d.%d.%d\n", rfc, p[0], p[1], p[2], p[3]);
            }
            rfc->peer_address = *(u_int32_t *)cmddata;
            break;

        case PPTP_CMD_SETOURADDR:	
            if (rfc->flags & PPTP_FLAG_DEBUG) {
                u_char *p = cmddata;
                IOLog("PPTP command (%p): set our IP address = %d.%d.%d.%d\n", rfc, p[0], p[1], p[2], p[3]);
            }
            rfc->our_address = *(u_int32_t *)cmddata;
            break;

        case PPTP_CMD_SETPEERPPD:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                IOLog("PPTP command (%p): set peer PPD = 0x%x\n", rfc, *(u_int16_t *)cmddata);
            rfc->peer_ppd = *(u_int16_t *)cmddata;
            rfc->rtt = rfc->peer_ppd;
            rfc->rtt <<= SCALE_FACTOR;
            rfc->ato = MAX(MIN_TIMEOUT, MIN(rfc->rtt + (CHI * rfc->dev), rfc->maxtimeout));
            break;

        case PPTP_CMD_SETMAXTIMEOUT:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                IOLog("PPTP command (%p): set max timeout = %d seconds\n", rfc, *(u_int16_t *)cmddata);
            rfc->maxtimeout = *(u_int16_t *)cmddata * 2;	// convert the timer in slow timeout ticks
            rfc->maxtimeout <<= SCALE_FACTOR;
            rfc->ato = MIN(rfc->ato, rfc->maxtimeout);
            break;

        default:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                IOLog("PPTP command (%p): unknown command = %d\n", rfc, cmd);
    }

    return error;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int16_t handle_data(struct pptp_rfc *rfc, mbuf_t m, u_int32_t from)
{
    struct pptp_gre 	*p, p_data;
    u_int16_t 		size;
    u_int32_t		ack;
    int32_t 		diff;
	int				qlen;
	struct pptp_elem 	*elem, *new_elem;

    p = &p_data;
    memcpy(p, mbuf_data(m), sizeof(p_data));

    //IOLog("handle_data, rfc = %p, from 0x%x, known peer address = 0x%x, our callid = 0x%x, target callid = 0x%x\n", rfc, from, rfc->peer_address, rfc->call_id, ntohs(p->call_id));    
    
    // check identify the session, we must check the session id AND the address of the peer
    // we could be connected to 2 different AC with the same session id
    // or to 1 AC with 2 session id
    if (!((rfc->call_id == ntohs(p->call_id))
        && (rfc->peer_address == from))) {
		// the packet was not for us
		return 0;
	}

	size = 8;
	if (p->flags_vers & PPTP_GRE_FLAGS_A) {	// handle window

		// depending if seq is present, take ack at the appropriate offset
		ack = (p->flags & PPTP_GRE_FLAGS_S) ? ntohl(p->ack_num) : ntohl(p->seq_num);

		//IOLog("handle_data, contains ACK for packet = %d (rfc->our_last_seq = %d, rfc->our_last_seq_acked + 1 = %d)\n", ack, rfc->our_last_seq, rfc->our_last_seq_acked + 1);
		if (SEQ_GT(ack, rfc->our_last_seq_acked)
			&& SEQ_LEQ(ack, rfc->our_last_seq)) {

			if (rfc->sample && SEQ_GEQ(ack, rfc->sample_seq)) {
							
				diff = (rfc->sample << SCALE_FACTOR) - rfc->rtt;
				rfc->dev += (ABS(diff) - rfc->dev) / BETA;
				rfc->rtt += diff / ALPHA;
				rfc->ato = MAX(MIN_TIMEOUT, MIN(rfc->rtt + (CHI * rfc->dev), rfc->maxtimeout));
				rfc->send_window = MIN(rfc->send_window + 1, rfc->peer_window);
				rfc->sample = 0;
				rfc->send_timeout = 0;
			}

			rfc->our_last_seq_acked = ack;
			if (rfc->state & PPTP_STATE_XMIT_FULL) {
				//IOLog("handle_data PPTP_EVT_XMIT_OK\n");
				rfc->state &= ~PPTP_STATE_XMIT_FULL;
				if (rfc->eventcb) 
					(*rfc->eventcb)(rfc->host, PPTP_EVT_XMIT_OK, 0);
			}
		}
		size += 4;
		
	}
	
	if (!(p->flags & PPTP_GRE_FLAGS_S))
		goto dropit;
   
	//IOLog("handle_data, contains SEQ packet = %d (rfc->peer_last_seq = %d)\n", ntohl(p->seq_num), rfc->peer_last_seq);

	if (!rfc->inputcb)
		goto dropit;

	size += 4;

	if ((rfc->state & PPTP_STATE_PEERSTARTED) == 0) {
		rfc->peer_last_seq = ntohl(p->seq_num) - 1;	// initial peer_last_sequence
		rfc->state |= PPTP_STATE_PEERSTARTED;
	}
		
	// check for packets out of sequence and reorder packets
	if (SEQ_GT(ntohl(p->seq_num), rfc->peer_last_seq + 1)) {
		
		qlen = 0;
		TAILQ_FOREACH(elem, &rfc->recv_queue, next) {
			if (ntohl(p->seq_num) == elem->seqno)	
				goto dropit;					/* already queued - drop it */
			qlen++;
			if (SEQ_LT(ntohl(p->seq_num), elem->seqno))
				break;
		}
		new_elem = (struct pptp_elem *)_MALLOC(sizeof (struct pptp_elem), M_TEMP, M_NOWAIT);
		if (new_elem == 0)
			goto dropit;
		new_elem->seqno = ntohl(p->seq_num);
		mbuf_adj(m, size); // remove pptp header
		new_elem->packet = m;

		if (elem)
			TAILQ_INSERT_BEFORE(elem, new_elem, next);
		else
			TAILQ_INSERT_TAIL(&rfc->recv_queue, new_elem, next);   

		/* if queue is already long, don't wait, input packets immediatly. Missing packet was probably lost 
			otherwise, arm the timer if not already armed */
		if (qlen >= RECV_MAXLEN_DEF) 
			pptp_rfc_input_recv_queue(rfc);
		else if (rfc->recv_timeout == 0)
			rfc->recv_timeout = RECV_TIMEOUT_DEF;


	} else if (SEQ_LT(ntohl(p->seq_num), rfc->peer_last_seq + 1)) {
		rfc->state |= PPTP_STATE_NEW_SEQUENCE;		/* its a dup thats already been ack'd - drop it and ack */
		goto dropit;					
		
	} else {
		/* packet we are waiting for */
		rfc->peer_last_seq = ntohl(p->seq_num);
		rfc->state |= PPTP_STATE_NEW_SEQUENCE;
		mbuf_adj(m, size);
		
		(*rfc->inputcb)(rfc->host, m); // packet is passed up to the host	
			
		/* now check for other packets on the queue that can be sent up.  */
		while ((elem = TAILQ_FIRST(&rfc->recv_queue))) {
		
			if (elem->seqno == (rfc->peer_last_seq+1)) {		/* another packet to send up */

				rfc->peer_last_seq = elem->seqno;
				TAILQ_REMOVE(&rfc->recv_queue, elem, next);				
				(*rfc->inputcb)(rfc->host, elem->packet);
				_FREE(elem, M_TEMP);

			} else {
				rfc->recv_timeout = RECV_TIMEOUT_DEF;
				break;
			}
		}
	}
		
	// let's say the packet have been treated
	return 1;

dropit:
	mbuf_freem(m);
	return 1;
}

/* -----------------------------------------------------------------------------
called from pptp_ip when pptp data are present
----------------------------------------------------------------------------- */
int pptp_rfc_lower_input(mbuf_t m, u_int32_t from)
{
    struct pptp_rfc  	*rfc;
	
	lck_mtx_assert(ppp_domain_mutex, LCK_MTX_ASSERT_OWNED);
    
    //IOLog("PPTP inputdata\n");
    
    TAILQ_FOREACH(rfc, &pptp_rfc_head, next)
        if (handle_data(rfc, m, from))
            return 1;
            
    // nobody was interested in the packet, just ignore it
    return 0;
}
