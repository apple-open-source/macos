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


#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/domain.h>

#include <machine/spl.h>

#include <net/if_var.h>

#include "../../../Family/if_ppplink.h"
#include "../../../Family/ppp_domain.h"
#include "PPTP.h"
#include "pptp_rfc.h"
#include "pptp_ip.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

#define PPTP_VER 	1
#define PPTP_TYPE	1

#define PPTP_STATE_XMIT_FULL	0x00000001	/* xmit if full */
#define PPTP_STATE_NEW_SEQUENCE	0x00000002	/* we have a seq number to acknowledge */
#define PPTP_STATE_PEERSTARTED	0x00000004	/* peer has sent its first packet, initial peer_sequence is known */

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
    u_int16_t		call_id;			/* our session id */
    u_int16_t		peer_call_id;			/* peer's session id */
    u_int16_t		our_window;			/* our recv window */
    u_int16_t		peer_window;			/* peer's recv window */
    u_int16_t		send_window;			/* current send window */
    u_int16_t		send_timeout;			/* send timeout */
    u_int32_t		our_last_seq;			/* last seq number we sent */
    u_int32_t		our_last_seq_acked;		/* last seq number acked */
    u_int32_t		peer_last_seq;			/* highest last seq number we received */
    u_int16_t		peer_ppd;			/* peer packet processing delay */
    u_int32_t		maxtimeout;			/* maximum timeout (scaled) */

    // Adaptative time-out calculation, see PPTP rfc for details
    u_int32_t		sample_seq;			/* sequence number being currently sampled */
    u_int16_t		sample;				/* sample round trip time measured for the current packet */
    u_int32_t		rtt;				/* calculated round-trip time (scaled) */
    int32_t		dev;				/* deviation time (scaled) */
    u_int32_t		ato;				/* adaptative timeout (scaled) */
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

/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */

TAILQ_HEAD(, pptp_rfc) 	pptp_rfc_head;

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
    
    rfc = (struct pptp_rfc *)_MALLOC(sizeof (struct pptp_rfc), M_TEMP, M_WAITOK);
    if (rfc == 0)
        return 1;

    //log(LOG_INFO, "PPTP new_client rfc = 0x%x\n", rfc);

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
    
    if (rfc->flags & PPTP_FLAG_DEBUG)
        log(LOG_INFO, "PPTP free (0x%x)\n", rfc);

    if (rfc) {
            
        TAILQ_REMOVE(&pptp_rfc_head, rfc, next);
        _FREE(rfc, M_TEMP);
    }
}

/* -----------------------------------------------------------------------------
called by protocol family when fast timer expires
----------------------------------------------------------------------------- */
void pptp_rfc_fasttimer()
{
    struct pptp_rfc  	*rfc;
    struct pptp_gre	*p;
    u_int16_t 		len;
    struct mbuf		*m;

    TAILQ_FOREACH(rfc, &pptp_rfc_head, next) {

        if (rfc->state & PPTP_STATE_NEW_SEQUENCE) {
        
            if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
                return;

            // build an ack packet, without data
            len = sizeof(struct pptp_gre) - 4;
            m->m_len = len;
            m->m_pkthdr.len = len;
                        
            // probably some of it should move to pptp_ip when we implement 
            // a more modular GRE handler
            p = mtod(m, struct pptp_gre *);
            p->flags = PPTP_GRE_FLAGS_K;
            p->flags_vers = PPTP_GRE_VER | PPTP_GRE_FLAGS_A;
            p->proto_type = htons(PPTP_GRE_TYPE);
            p->payload_len = htons(len); 
            p->call_id = htons(rfc->peer_call_id);
            /* XXX use seq_num in the structure to put the ack */
            p->seq_num = htonl(rfc->peer_last_seq);
            rfc->state &= ~PPTP_STATE_NEW_SEQUENCE;
                
            //log(LOG_INFO, "pptp_rfc_fasttimer, output delayed ACK = %d\n", rfc->peer_last_seq);
            pptp_ip_output(m, rfc->peer_address);
        }
    }
}

/* -----------------------------------------------------------------------------
called by protocol family when fast timer expires
----------------------------------------------------------------------------- */
void pptp_rfc_slowtimer()
{
    struct pptp_rfc  	*rfc;

    TAILQ_FOREACH(rfc, &pptp_rfc_head, next) {

        if (rfc->send_timeout && (--rfc->send_timeout == 0)) {
                
            //log(LOG_INFO, "pptp_rfc_slowtimer, send timer expires for packet = %d\n", rfc->sample_seq);
            rfc->rtt = DELTA * rfc->rtt;
            rfc->ato = MAX(MIN_TIMEOUT, MIN(rfc->rtt + (CHI * rfc->dev), rfc->maxtimeout));

            rfc->send_window = (rfc->send_window / 2) + (rfc->send_window % 2);
            rfc->sample = 0;
            rfc->our_last_seq_acked = rfc->sample_seq;
            //log(LOG_INFO, "pptp_rfc_slowtimer, new ato = %d, new send window = %d\n", rfc->ato, rfc->send_window);
            
            if (rfc->state & PPTP_STATE_XMIT_FULL) {
                //log(LOG_INFO, "pptp_rfc_slowtimer PPTP_EVT_XMIT_OK\n");
                rfc->state &= ~PPTP_STATE_XMIT_FULL;
                if (rfc->eventcb) 
                    (*rfc->eventcb)(rfc->host, PPTP_EVT_XMIT_OK, 0);
            }
        }
        
        if (rfc->sample)
            rfc->sample++;
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int16_t pptp_rfc_output(void *data, struct mbuf *m)
{
    struct pptp_rfc 	*rfc = (struct pptp_rfc *)data;
    u_int8_t 		*d;
    struct pptp_gre	*p;
    struct mbuf *m0;
    u_int16_t 		len, size;

    len = 0;
    for (m0 = m; m0 != 0; m0 = m0->m_next)
        len += m0->m_len;

    // log(LOG_INFO, "PPTP write, len = %d\n", len);
    //d = mtod(m, u_int8_t *);
    //log(LOG_INFO, "PPTP write, data = %x %x %x %x %x %x \n", d[0], d[1], d[2], d[3], d[4], d[5]);

    size = 8 + 4;
    if (rfc->state & PPTP_STATE_NEW_SEQUENCE)
        size += 4;

    M_PREPEND(m, size, M_WAIT);
    if (m == 0)
        return 1;
    d = mtod(m, u_int8_t *);

    m->m_flags |= M_PKTHDR;
    m->m_pkthdr.len = len + size;

    p = (struct pptp_gre *)d;
    bzero(p, size);
    
    // probably some of it should move to pptp_ip when we implement 
    // a more modular GRE handler
    rfc->our_last_seq++;
    p->flags = PPTP_GRE_FLAGS_K + PPTP_GRE_FLAGS_S;
    p->flags_vers = PPTP_GRE_VER;
    p->proto_type = htons(PPTP_GRE_TYPE);
    p->payload_len = htons(len); 
    p->call_id = htons(rfc->peer_call_id);
    p->seq_num = htonl(rfc->our_last_seq);
    if (rfc->state & PPTP_STATE_NEW_SEQUENCE) {
        p->flags_vers |= PPTP_GRE_FLAGS_A;
        p->ack_num = htonl(rfc->peer_last_seq);
        rfc->state &= ~PPTP_STATE_NEW_SEQUENCE;
    }

    if (ROUND32DIFF(rfc->our_last_seq, rfc->our_last_seq_acked) >= rfc->send_window) {
        //log(LOG_INFO, "pptp_rfc_output PPTP_STATE_XMIT_FULL\n");
        rfc->state |= PPTP_STATE_XMIT_FULL;
        if (rfc->eventcb)
            (*rfc->eventcb)(rfc->host, PPTP_EVT_XMIT_FULL, 0);
    }    
    
    if (rfc->sample == 0) {
        rfc->sample = 1;
        rfc->sample_seq = rfc->our_last_seq;
        rfc->send_timeout = rfc->ato >> SCALE_FACTOR;
        //log(LOG_INFO, "pptp_rfc_output, will sample packet = %d, timeout = %d\n", rfc->our_last_seq, rfc->ato);
    }
    //log(LOG_INFO, "pptp_rfc_output, SEND packet = %d\n", rfc->our_last_seq);

    pptp_ip_output(m, rfc->peer_address);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int16_t pptp_rfc_command(void *data, u_int32_t cmd, void *cmddata)
{
    struct pptp_rfc 	*rfc = (struct pptp_rfc *)data;
    u_int16_t		error = 0;

    switch (cmd) {

        case PPTP_CMD_SETFLAGS:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                log(LOG_INFO, "PPTP command (0x%x): set flags = 0x%x\n", rfc, *(u_int32_t *)cmddata);
            rfc->flags = *(u_int32_t *)cmddata;
            break;

        case PPTP_CMD_SETWINDOW:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                log(LOG_INFO, "PPTP command (0x%x): set window = 0x%x\n", rfc, *(u_int16_t *)cmddata);
            rfc->our_window = *(u_int16_t *)cmddata;
            break;

        case PPTP_CMD_SETPEERWINDOW:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                log(LOG_INFO, "PPTP command (0x%x): set peer window = 0x%x\n", rfc, *(u_int16_t *)cmddata);
            rfc->peer_window = *(u_int16_t *)cmddata;
            rfc->send_window = (rfc->peer_window / 2) + (rfc->peer_window % 2);
            break;

        case PPTP_CMD_SETCALLID:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                log(LOG_INFO, "PPTP command (0x%x): set call id = 0x%x\n", rfc, *(u_int16_t *)cmddata);
            rfc->call_id = *(u_int16_t *)cmddata;
            break;

        case PPTP_CMD_SETPEERCALLID:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                log(LOG_INFO, "PPTP command (0x%x): set peer call id = 0x%x\n", rfc, *(u_int16_t *)cmddata);
            rfc->peer_call_id = *(u_int16_t *)cmddata;
            break;

        // set the peer address to specify IP address
        // MUST be called before using the connection
        case PPTP_CMD_SETPEERADDR:	
            if (rfc->flags & PPTP_FLAG_DEBUG) {
                u_char *p = cmddata;
                log(LOG_INFO, "PPTP command (0x%x): set peer IP address = %d.%d.%d.%d\n", rfc, p[0], p[1], p[2], p[3]);
            }
            rfc->peer_address = *(u_int32_t *)cmddata;
            break;

        case PPTP_CMD_SETPEERPPD:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                log(LOG_INFO, "PPTP command (0x%x): set peer PPD = 0x%x\n", rfc, *(u_int16_t *)cmddata);
            rfc->peer_ppd = *(u_int16_t *)cmddata;
            rfc->rtt = rfc->peer_ppd;
            rfc->rtt <<= SCALE_FACTOR;
            rfc->ato = MAX(MIN_TIMEOUT, MIN(rfc->rtt + (CHI * rfc->dev), rfc->maxtimeout));
            break;

        case PPTP_CMD_SETMAXTIMEOUT:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                log(LOG_INFO, "PPTP command (0x%x): set max timeout = %d seconds\n", rfc, *(u_int16_t *)cmddata);
            rfc->maxtimeout = *(u_int16_t *)cmddata * 2;	// convert the timer in slow timeout ticks
            rfc->maxtimeout <<= SCALE_FACTOR;
            rfc->ato = MIN(rfc->ato, rfc->maxtimeout);
            break;

        default:
            if (rfc->flags & PPTP_FLAG_DEBUG)
                log(LOG_INFO, "PPTP command (0x%x): unknown command = %d\n", rfc, cmd);
    }

    return error;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int16_t handle_data(struct pptp_rfc *rfc, struct mbuf *m, u_int32_t from)
{
    struct pptp_gre 	*p = mtod(m, struct pptp_gre *);
    u_int16_t 		size;
    u_int32_t		ack;
    int32_t 		diff;

    //log(LOG_INFO, "handle_data, rfc = 0x%x, from 0x%x, known peer address = 0x%x, our callid = 0x%x, target callid = 0x%x\n", rfc, from, rfc->peer_address, rfc->call_id, ntohs(p->call_id));    
    
    // check identify the session, we must check the session id AND the address of the peer
    // we could be connected to 2 different AC with the same session id
    // or to 1 AC with 2 session id
    if ((rfc->call_id == ntohs(p->call_id))
        && (rfc->peer_address == from)) {

        size = 8;
        if (p->flags_vers & PPTP_GRE_FLAGS_A) {	// handle window

            // depending if seq is present, take ack at the appropriate offset
            ack = (p->flags & PPTP_GRE_FLAGS_S) ? p->ack_num : p->seq_num;
 
            //log(LOG_INFO, "handle_data, contains ACK for packet = %d (rfc->our_last_seq = %d, rfc->our_last_seq_acked + 1 = %d)\n", ack, rfc->our_last_seq, rfc->our_last_seq_acked + 1);
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
                    //log(LOG_INFO, "handle_data PPTP_EVT_XMIT_OK\n");
                    rfc->state &= ~PPTP_STATE_XMIT_FULL;
                    if (rfc->eventcb) 
                        (*rfc->eventcb)(rfc->host, PPTP_EVT_XMIT_OK, 0);
                }
            }
            size += 4;
            
        }
        
        if (p->flags & PPTP_GRE_FLAGS_S) {
            size += 4;

            //log(LOG_INFO, "handle_data, contains SEQ packet = %d (rfc->peer_last_seq = %d)\n", p->seq_num, rfc->peer_last_seq);

            if ((rfc->state & PPTP_STATE_PEERSTARTED) == 0) {
                rfc->peer_last_seq = p->seq_num - 1;	// initial peer_last_sequence
                rfc->state |= PPTP_STATE_PEERSTARTED;
            }
                
            // check for packets out of sequence
            // could optionnally reorder packets
            if (SEQ_GT(p->seq_num, rfc->peer_last_seq)) {
            
                if (rfc->peer_last_seq + 1 != p->seq_num) { 
 
                    //log(LOG_INFO, "handle_data, contains unexpected SEQ  = %d (rfc->peer_last_seq = %d)\n", p->seq_num, rfc->peer_last_seq);
                   if (rfc->eventcb)
                        (*rfc->eventcb)(rfc->host, PPTP_EVT_INPUTERROR, 0);
                }
                
                rfc->peer_last_seq = p->seq_num;
                rfc->state |= PPTP_STATE_NEW_SEQUENCE;
                m_adj(m, size);
                // packet is passed up to the host
                if (rfc->inputcb)
                    (*rfc->inputcb)(rfc->host, m);
                    
            }
            else 
                m_freem(m);
        }
        else 
            m_freem(m);
            
        // let's say the packet have been treated
        return 1;
    }

    // the packet was not for us
    return 0;
}

/* -----------------------------------------------------------------------------
called from pptp_ip when pptp data are present
----------------------------------------------------------------------------- */
int pptp_rfc_lower_input(struct mbuf *m, u_int32_t from)
{
    struct pptp_rfc  	*rfc;
    
    //log(LOG_INFO, "PPTP inputdata\n");
    
    TAILQ_FOREACH(rfc, &pptp_rfc_head, next)
        if (handle_data(rfc, m, from))
            return 1;
            
    // nobody was interested in the packet, just ignore it
    return 0;
}
