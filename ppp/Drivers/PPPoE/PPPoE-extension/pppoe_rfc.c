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
#include "PPPoE.h"
#include "pppoe_rfc.h"
#include "pppoe_dlil.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

//#define PPPENET_COMPAT 1

#define PPPOE_VER 	1
#define PPPOE_TYPE	1

#define PPPOE_PADI	0x9
#define PPPOE_PADO	0x7
#define PPPOE_PADR	0x19
#define PPPOE_PADS	0x65
#define PPPOE_PADT	0xa7

#define PPPOE_TAG_END_OF_LIST		0x0000
#define PPPOE_TAG_SERVICE_NAME		0x0101
#define PPPOE_TAG_AC_NAME		0x0102
#define PPPOE_TAG_HOST_UNIQ		0x0103
#define PPPOE_TAG_AC_COOKIE		0x0104
#define PPPOE_TAG_VENDOR_SPECIFIC	0x0105
#define PPPOE_TAG_RELAY_SESSION_ID	0x0110
#define PPPOE_TAG_SERVICE_NAME_ERROR	0x0201
#define PPPOE_TAG_AC_SYSTEM_ERROR	0x0202
#define PPPOE_TAG_GENERIC_ERROR		0x0203

#define PPPOE_TIMER_CONNECT 		20	 // let's have a connect timer of 20 seconds
#define PPPOE_TIMER_RING 		30	 // let's have a ring timer of 30 seconds
#define PPPOE_TIMER_RETRY 		3	 // let's have a retry period of 3 seconds

#define SERVER_NAME "Darwin\0"
#define SERVICE_NAME "Think-Different\0"

#define	PPPOE_HOST_UNIQ_LEN		64	// 64 bytes (Fix Me: dynamically allocate tag)
#define	PPPOE_RELAY_ID_LEN		64	// 64 bytes (Fix Me: dynamically allocate tag)
#define	PPPOE_AC_COOKIE_LEN		64	// 64 bytes (Fix Me: dynamically allocate tag)

#define PPPOE_TMPBUF_SIZE		1500

struct pppoe {
    u_int8_t ver:4;
    u_int8_t typ:4;
    u_int8_t code;
    u_int16_t sessid;
    u_int16_t len;
};

// a pppoe_tag is basically a buffer with information how much data it contains
// right now, and how much space it provides in total.
struct pppoe_tag {
    u_int16_t	len;		/* data length */
    u_int16_t	max_len;	/* buffer size */
    u_int8_t	*data;		/* pointer to actual data */
};

// utility macro that makes it easy to declare a pppoe_tag with static data
#define PPPOE_TAG(name, size)			\
    struct pppoe_tag	name;			\
    u_int8_t 		name##_BUF[size]

// utility macro to setup a pppoe_tag struct
#define PPPOE_TAG_SETUP(name)			\
    do {					\
        name.data = name##_BUF;			\
        name.max_len = sizeof(name##_BUF);	\
        name.len = 0;				\
        name.data[0] = 0;			\
    } while (0)

// utility macro to reset pppoe_tag struct after cloning
#define PPPOE_TAG_RESETUP(name)			\
    do {					\
        name.data = name##_BUF;			\
    } while (0)

// utility macro to make it easy to initialize a pppoe_tag with a C string.
// This macro also makes sure that no overflow can occur.
#define PPPOE_TAG_STRCPY(tag, str)		\
    do {					\
        strncpy(tag.data, str, tag.max_len);	\
        tag.len = strlen(tag.data);		\
    } while (0)

// utility macro that allows to easily compare two pppoe_tag structs.
#define PPPOE_TAG_CMP(tag1, tag2)		\
    ((tag1.len == tag2.len) ? memcmp(tag1.data, tag2.data, tag1.len) : 0)
            

struct pppoe_rfc {

    // administrative info
    TAILQ_ENTRY(pppoe_rfc) 	next;
    void 			*host; 			/* pointer back to the hosting structure */
    u_long	    		dl_tag;			/* associated datalink attachment dl_tag */
    pppoe_rfc_input_callback 	inputcb;		/* callback function when data are present */
    pppoe_rfc_event_callback 	eventcb;		/* callback function for events */
    u_int16_t	    		unit;			/* associated interface unit number */
    u_int32_t  			flags;			/* is client in loopback mode ? */
    u_int32_t  			uniqueid;		/* unique id (currently used for loopback) */

    // current state
    u_int16_t 			state;			/* PPPoE current state */
    
    // outgoing call
    PPPOE_TAG(ac_name, PPPOE_AC_NAME_LEN);		/* Access Concentrator we want to reach */
    PPPOE_TAG(service, PPPOE_SERVICE_LEN);		/* Service name we want to reach */
    u_int16_t	timer_connect_setup;			/* number of seconds to allow for an outgoing call */
    u_int16_t	timer_retry_setup;			/* number of seconds between retries */
    u_int16_t	timer_connect;				/* seconds remaining before aborting outgoing call */
    u_int16_t	timer_connect_resend;			/* seconds when needs teo start retransmitting */

    // incoming call
    PPPOE_TAG(serv_ac_name, PPPOE_AC_NAME_LEN);		/* Access Concentrator we offer */
    PPPOE_TAG(serv_service, PPPOE_SERVICE_LEN);		/* Service name we offer */
    u_int16_t	timer_ring_setup;			/* number of seconds to allow for an incoming call */
    u_int16_t	timer_ring;				/* second remaining before aborting incoming call */

    // commom outgoing/incoming call
    PPPOE_TAG(host_uniq, PPPOE_HOST_UNIQ_LEN);		/* client reserved cookie */
    PPPOE_TAG(ac_cookie, PPPOE_AC_COOKIE_LEN);		/* access concentrator reserved cookie */
    PPPOE_TAG(relay_id, PPPOE_RELAY_ID_LEN);		/* intermediate relay cookie */
    u_int16_t	session_id;				/* session id between client and server */
    u_int8_t	peer_address[ETHER_ADDR_LEN];		/* ethernet address we are connected to */

};


/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */
// change the algo to remove that var
u_int8_t 	pppenet_tmpbuf[PPPOE_TMPBUF_SIZE];

u_int16_t 	pppoe_unique_session_id = 1;
u_int32_t 	pppoe_unique_address = 1;

TAILQ_HEAD(, pppoe_rfc) 	pppoe_rfc_head;

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */
static u_int16_t handle_PADI(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from);
static u_int16_t handle_PADR(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from);
static u_int16_t handle_PADO(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from);
static u_int16_t handle_PADS(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from);
static u_int16_t handle_PADT(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from);
static u_int16_t handle_data(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from);
static u_int16_t handle_ctrl(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from);

static void send_event(struct pppoe_rfc *rfc, u_int32_t event, u_int32_t msg);
static void send_PAD(struct pppoe_rfc *rfc, u_int8_t *address, u_int16_t code, u_int16_t sessid,
                     struct pppoe_tag *ac_name, struct pppoe_tag *service,
                     struct pppoe_tag *host_uniq, struct pppoe_tag *ac_cookie, struct pppoe_tag *relay_id);

static u_int16_t add_tag(u_int8_t *data, u_int16_t tag, struct pppoe_tag *val);
static u_int16_t get_tag(struct mbuf *m, u_int16_t tag, struct pppoe_tag *val);

u_int16_t pppoe_rfc_input(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from, u_int16_t typ);
void pppoe_rfc_lower_output(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *to, u_int16_t typ);

extern void    m_copydata __P((struct mbuf *, int, int, caddr_t));


/* -----------------------------------------------------------------------------
intialize pppoe protocol on ethernet unit
need to change it to handle multiple ethernet interfaces
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_init()
{

    pppoe_dlil_init();
    TAILQ_INIT(&pppoe_rfc_head);
    return 0;
}

/* -----------------------------------------------------------------------------
dispose of a pppoe protocol
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_dispose()
{

    if (pppoe_dlil_dispose())
        return 1;
        
    return 0;
}


/* -----------------------------------------------------------------------------
intialize a new pppoe structure
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_new_client(void *host, void **data,
                         pppoe_rfc_input_callback input,
                         pppoe_rfc_event_callback event)
{
    struct pppoe_rfc 	*rfc;
    
    rfc = (struct pppoe_rfc *)_MALLOC(sizeof (struct pppoe_rfc), M_TEMP, M_WAITOK);
    if (rfc == 0)
        return 1;

    //log(LOG_INFO, "PPPoE new_client rfc = 0x%x\n", rfc);

    bzero(rfc, sizeof(struct pppoe_rfc));

    rfc->host = host;
    rfc->unit = 0xFFFF;
    rfc->uniqueid = pppoe_unique_address++;
    rfc->inputcb = input;
    rfc->eventcb = event;
    rfc->timer_connect_setup = PPPOE_TIMER_CONNECT;
    rfc->timer_ring_setup = PPPOE_TIMER_RING;
    rfc->timer_retry_setup = PPPOE_TIMER_RETRY;

    rfc->state = PPPOE_STATE_DISCONNECTED;
    PPPOE_TAG_SETUP(rfc->ac_name);
    PPPOE_TAG_SETUP(rfc->service);
    PPPOE_TAG_SETUP(rfc->serv_ac_name);
    PPPOE_TAG_SETUP(rfc->serv_service);
    PPPOE_TAG_SETUP(rfc->host_uniq);
    PPPOE_TAG_SETUP(rfc->ac_cookie);
    PPPOE_TAG_SETUP(rfc->relay_id);
    PPPOE_TAG_STRCPY(rfc->serv_ac_name, SERVER_NAME);
    PPPOE_TAG_STRCPY(rfc->serv_service, SERVICE_NAME);

    *data = rfc;

    TAILQ_INSERT_TAIL(&pppoe_rfc_head, rfc, next);

    return 0;
}

/* -----------------------------------------------------------------------------
dispose of a pppoe structure
----------------------------------------------------------------------------- */
void pppoe_rfc_free_client(void *data)
{
    struct pppoe_rfc 	*rfc = (struct pppoe_rfc *)data;
    
    if (rfc->flags & PPPOE_FLAG_DEBUG)
        log(LOG_INFO, "PPPoE free (0x%x)\n", rfc);

    if (rfc) {
    
        if (rfc->dl_tag)
            pppoe_dlil_detach(rfc->dl_tag);
        
        TAILQ_REMOVE(&pppoe_rfc_head, rfc, next);
        _FREE(rfc, M_TEMP);
    }
}

/* -----------------------------------------------------------------------------
connect the protocol to the service 'num'
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_connect(void *data, u_int8_t *ac_name, u_int8_t *service)
{
    struct pppoe_rfc 	*rfc = (struct pppoe_rfc *)data;
    u_int8_t       	broadcastaddr[ETHER_ADDR_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    u_int8_t       	emptyaddr[ETHER_ADDR_LEN] = { 0, 0, 0, 0, 0, 0 };

    if (rfc->state != PPPOE_STATE_DISCONNECTED)
        return 1;

    if (rfc->flags & PPPOE_FLAG_DEBUG)
        log(LOG_INFO, "PPPoE connect (0x%x): ac-name = '%s', service = '%s', \n", rfc, ac_name, service);

    if (rfc->unit == 0xFFFF) {
        // not attached to an interface yet. Use unit 0, for conveniency.
        // an application that doesn't specify the unit will use unit 0, if available.
        if (pppoe_dlil_attach(0, &rfc->dl_tag))
            return 1;
        rfc->unit = 0;
    }

    PPPOE_TAG_STRCPY(rfc->ac_name, ac_name);
    PPPOE_TAG_STRCPY(rfc->service, service);
    // use our rfc address for the uniq id, we now it won't change since we allocate ourserlves
    // FIXME sizeof(u_int32_t) might be != sizeof(pointer)
    *(u_int32_t *)&rfc->host_uniq.data[0] = (u_int32_t)rfc;
    rfc->host_uniq.len = sizeof(u_int32_t);
    rfc->ac_cookie.len = 0;
    rfc->relay_id.len = 0;
    
    rfc->state = PPPOE_STATE_LOOKING;
    rfc->timer_connect = rfc->timer_connect_setup;
    // resend PADI/PADR every PPPOE_TIMEOUT_RETRY seconds
    rfc->timer_connect_resend = rfc->timer_connect - rfc->timer_retry_setup;

    if (!memcmp(rfc->peer_address, emptyaddr, ETHER_ADDR_LEN))
        memcpy(rfc->peer_address, broadcastaddr, ETHER_ADDR_LEN);

    // if ac-name specified, try to reach it, otherwise, don't use name\
    // may be shoult use a '*' semantic in the address ?
    send_PAD(rfc, rfc->peer_address, PPPOE_PADI, 0, &rfc->ac_name, &rfc->service, &rfc->host_uniq, 0, 0);
    return 0;
}

/* -----------------------------------------------------------------------------
set server and service name
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_bind(void *data, u_int8_t *ac_name, u_int8_t *service)
{
    struct pppoe_rfc 	*rfc = (struct pppoe_rfc *)data;

    if (rfc->flags & PPPOE_FLAG_DEBUG)
        log(LOG_INFO, "PPPoE bind (0x%x): ac-name = '%s', service = '%s'\n", rfc,
        ac_name ? (char*)ac_name : "nul", service ? (char*)service : "nul");

    if (ac_name)
        PPPOE_TAG_STRCPY(rfc->serv_ac_name, ac_name);
    if (service)
        PPPOE_TAG_STRCPY(rfc->serv_service, service);
    return 0;
}

/* -----------------------------------------------------------------------------
accept an incoming connection
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_accept(void *data)
{
    struct pppoe_rfc 	*rfc = (struct pppoe_rfc *)data;

    if (rfc->state != PPPOE_STATE_RINGING)
        return 1;

    if (rfc->flags & PPPOE_FLAG_DEBUG)
        log(LOG_INFO, "PPPoE accept (0x%x)\n", rfc);

    // should check wih other numbers currently sets with  active sessions
    rfc->session_id = pppoe_unique_session_id++; // generate a session id

    // host_uniq and rfc->relay_session_id have been got from the previous PADR
    send_PAD(rfc, rfc->peer_address, PPPOE_PADS, rfc->session_id, &rfc->ac_name, &rfc->service,
             rfc->host_uniq.len ? &rfc->host_uniq : 0, 0, rfc->relay_id.len ? &rfc->relay_id : 0);
             
    rfc->state = PPPOE_STATE_CONNECTED;
    send_event(rfc, PPPOE_EVT_CONNECTED, 0);

    return 0;
}

/* -----------------------------------------------------------------------------
listen for incoming calls
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_listen(void *data)
{
    struct pppoe_rfc 	*rfc = (struct pppoe_rfc *)data;

    if (rfc->state != PPPOE_STATE_DISCONNECTED)
        return 1;

    if (rfc->flags & PPPOE_FLAG_DEBUG)
        log(LOG_INFO, "PPPoE listen (0x%x)\n", rfc);

    if (rfc->unit == 0xFFFF) {
        // not attached to an interface yet. Use unit 0, for conveniency.
        // an application that doesn't specify the unit will use unit 0, if available.
        if (pppoe_dlil_attach(0, &rfc->dl_tag))
            return 1;
        rfc->unit = 0;
    }

    rfc->state = PPPOE_STATE_LISTENING;
    
    return 0;
}

/* -----------------------------------------------------------------------------
abort current connection, depend on the state
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_abort(void *data)
{
    struct pppoe_rfc 	*rfc = (struct pppoe_rfc *)data;

    if (rfc->flags & PPPOE_FLAG_DEBUG)
        log(LOG_INFO, "PPPoE abort (0x%x)\n", rfc);

    switch (rfc->state) {
        case PPPOE_STATE_LOOKING:
        case PPPOE_STATE_CONNECTING:
        case PPPOE_STATE_LISTENING:
        case PPPOE_STATE_RINGING:
            rfc->state = PPPOE_STATE_DISCONNECTED;
            bzero(rfc->peer_address, sizeof(rfc->peer_address));
            send_event(rfc, PPPOE_EVT_DISCONNECTED, 0);
            break;
        case PPPOE_STATE_CONNECTED:
            pppoe_rfc_disconnect(data);
            break;
        case PPPOE_STATE_DISCONNECTED:
            break;
    }

    return 0;
}

/* -----------------------------------------------------------------------------
disconnect pppoe (only applies to connected state)
otherwise, should call abort
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_disconnect(void *data)
{
    struct pppoe_rfc 	*rfc = (struct pppoe_rfc *)data;

    if (rfc->state != PPPOE_STATE_CONNECTED) {
        return 1;
    }

    if (rfc->flags & PPPOE_FLAG_DEBUG)
        log(LOG_INFO, "PPPoE disconnect (0x%x)\n", rfc);

    send_PAD(rfc, rfc->peer_address, PPPOE_PADT, rfc->session_id, 0, 0, 0, 0, 0);

    rfc->state = PPPOE_STATE_DISCONNECTED;
    bzero(rfc->peer_address, sizeof(rfc->peer_address));
    send_event(rfc, PPPOE_EVT_DISCONNECTED, 0);

    return 0;
}

/* -----------------------------------------------------------------------------
clone the two data structures
NOTE : the host field is NOT copied, and rfc needs to be relinked
----------------------------------------------------------------------------- */
void pppoe_rfc_clone(void *data1, void *data2)
{
    struct pppoe_rfc 	*rfc2 = (struct pppoe_rfc *)data2;
    void 		*host2;

    host2 = rfc2->host;
    if (rfc2->dl_tag)
        pppoe_dlil_detach(rfc2->dl_tag);
    TAILQ_REMOVE(&pppoe_rfc_head, rfc2, next);
    memcpy(data2, data1, sizeof(struct pppoe_rfc));
    rfc2->host = host2;
    TAILQ_INSERT_TAIL(&pppoe_rfc_head, rfc2, next);
    // cannot fail, there is no attachment done, and it's is just refcnt bumping
    if (rfc2->dl_tag)
        pppoe_dlil_attach(rfc2->unit, &rfc2->dl_tag);

    // make the pointers point to the right place
    PPPOE_TAG_RESETUP(rfc2->ac_name);
    PPPOE_TAG_RESETUP(rfc2->service);
    PPPOE_TAG_RESETUP(rfc2->serv_ac_name);
    PPPOE_TAG_RESETUP(rfc2->serv_service);
    PPPOE_TAG_RESETUP(rfc2->host_uniq);
    PPPOE_TAG_RESETUP(rfc2->ac_cookie);
    PPPOE_TAG_RESETUP(rfc2->relay_id);    
}

/* -----------------------------------------------------------------------------
called by protocol family when slowtiemr expires
check the active timers and fires them appropriatly

refaire les timers
----------------------------------------------------------------------------- */
void pppoe_rfc_timer()
{
    struct pppoe_rfc  	*rfc;

    
    TAILQ_FOREACH(rfc, &pppoe_rfc_head, next) {

        switch (rfc->state) {
            case PPPOE_STATE_LOOKING:
            case PPPOE_STATE_CONNECTING:
                if (rfc->timer_connect == 0) {
                    if (rfc->flags & PPPOE_FLAG_DEBUG)
                        log(LOG_INFO, "PPPoE timer (0x%x): CONNECT_TIMER expires\n", rfc);
                    rfc->state = PPPOE_STATE_DISCONNECTED;
                    bzero(rfc->peer_address, sizeof(rfc->peer_address));
                    // double-check for the error number ?
                    send_event(rfc, PPPOE_EVT_DISCONNECTED, 
                        PPPOE_STATE_LOOKING ? EHOSTUNREACH : ECONNREFUSED);
                    break;
                }
                if (rfc->timer_connect == rfc->timer_connect_resend) {
                        rfc->timer_connect_resend -= rfc->timer_retry_setup;
                        send_PAD(rfc, rfc->peer_address, 
                        rfc->state == PPPOE_STATE_LOOKING ? PPPOE_PADI : PPPOE_PADR, 0, 
                        rfc->ac_name.len ? &rfc->ac_name : 0, &rfc->service,
                        &rfc->host_uniq, 
                        rfc->ac_cookie.len ? &rfc->ac_cookie : 0, 
                        rfc->relay_id.len ? &rfc->relay_id : 0);
                }
                rfc->timer_connect--;
                break;
            case PPPOE_STATE_RINGING:
                if (rfc->timer_ring == 0) {
                    if (rfc->flags & PPPOE_FLAG_DEBUG)
                        log(LOG_INFO, "PPPoE timer (0x%x): RING_TIMER expires\n", rfc);
                    rfc->state = PPPOE_STATE_DISCONNECTED;
                    bzero(rfc->peer_address, sizeof(rfc->peer_address));
                    send_event(rfc, PPPOE_EVT_DISCONNECTED, 0);
                    break;
                }
                rfc->timer_ring--; 
                break;
        }
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_output(void *data, struct mbuf *m)
{
    struct pppoe_rfc 	*rfc = (struct pppoe_rfc *)data;
    u_int8_t 		*d;
    struct pppoe	*p;
    struct mbuf *m0;
    u_int16_t 		skip, len;

    len = 0;
    for (m0 = m; m0 != 0; m0 = m0->m_next)
        len += m0->m_len;

    if (rfc->state != PPPOE_STATE_CONNECTED)
        return 1;

   // log(LOG_INFO, "PPPoE write, len = %d\n", len);
    //d = mtod(m, u_int8_t *);
    //log(LOG_INFO, "PPPoE write, data = %x %x %x %x %x %x \n", d[0], d[1], d[2], d[3], d[4], d[5]);

    skip = 0;
#if 0        // FF03 is not sent and ACFC option MUST not be negociated
             // test anyway if we start with FF03
    d = mtod(m, u_int8_t *);
    if (d[0] == 0xFF && d[1] == 3)
        skip = 2;
#endif

    M_PREPEND(m, sizeof(struct pppoe) - skip , M_WAIT);
    d = mtod(m, u_int8_t *);

    m->m_flags |= M_PKTHDR;
    p = (struct pppoe *)d;
    p->ver = PPPOE_VER;
    p->typ = PPPOE_TYPE;
    p->code = 0;
    p->sessid = rfc->session_id;
    p->len = len - skip; // tag len
    m->m_pkthdr.len = len + sizeof(struct pppoe) - skip;

    pppoe_rfc_lower_output(rfc, m, rfc->peer_address, PPPOE_ETHERTYPE_DATA);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_command(void *data, u_int32_t cmd, void *cmddata)
{
    struct pppoe_rfc 	*rfc = (struct pppoe_rfc *)data;
    u_int16_t		unit, error = 0;

    switch (cmd) {

        case PPPOE_CMD_SETFLAGS:
            if (rfc->flags & PPPOE_FLAG_DEBUG)
                log(LOG_INFO, "PPPoE command (0x%x): set flags = 0x%x\n", rfc, *(u_int32_t *)cmddata);
            rfc->flags = *(u_int32_t *)cmddata;
            break;

        case PPPOE_CMD_GETFLAGS:
            if (rfc->flags & PPPOE_FLAG_DEBUG)
                log(LOG_INFO, "PPPoE command (0x%x): get flags = 0x%x\n", rfc, rfc->flags);
            *(u_int32_t *)cmddata = rfc->flags;
            break;

        case PPPOE_CMD_SETUNIT:
            unit = *(u_int16_t *)cmddata;
            if (rfc->flags & PPPOE_FLAG_DEBUG)
                log(LOG_INFO, "PPPoE command (0x%x): set interface unit = %d\n", rfc, unit);
            if (rfc->unit != unit) {
               if (rfc->dl_tag) {
                    pppoe_dlil_detach(rfc->dl_tag);
                    rfc->dl_tag = 0;
                    rfc->unit = 0xFFFF;
                }
                if (unit != 0xFFFF) {
                    if (pppoe_dlil_attach(unit, &rfc->dl_tag))
                        return 1;
                    rfc->unit = unit;
                }
             }
            break;

        case PPPOE_CMD_GETUNIT:
            if (rfc->flags & PPPOE_FLAG_DEBUG)
                log(LOG_INFO, "PPPoE command (0x%x): get interface unit = %d\n", rfc, rfc->unit);
            *(u_int16_t *)cmddata = rfc->unit;
            break;

        case PPPOE_CMD_GETCONNECTTIMER:
            if (rfc->flags & PPPOE_FLAG_DEBUG)
                log(LOG_INFO, "PPPoE command (0x%x): get connect timer = %d\n", rfc, rfc->timer_connect_setup);
            *(u_int16_t *)cmddata = rfc->timer_connect_setup;
            break;

        case PPPOE_CMD_SETCONNECTTIMER:
            if (rfc->flags & PPPOE_FLAG_DEBUG)
                log(LOG_INFO, "PPPoE command (0x%x): set connect timer = %d\n", rfc, *(u_int16_t *)cmddata);
            rfc->timer_connect_setup = *(u_int16_t *)cmddata;
            break;

        case PPPOE_CMD_GETRINGTIMER:
            if (rfc->flags & PPPOE_FLAG_DEBUG)
                log(LOG_INFO, "PPPoE command (0x%x): get ring timer = %d\n", rfc, rfc->timer_ring_setup);
            *(u_int16_t *)cmddata = rfc->timer_ring_setup;
            break;

        case PPPOE_CMD_SETRINGTIMER:
            if (rfc->flags & PPPOE_FLAG_DEBUG)
                log(LOG_INFO, "PPPoE command (0x%x): set ring timer = %d\n", rfc, *(u_int16_t *)cmddata);
            rfc->timer_ring_setup = *(u_int16_t *)cmddata;
            break;

        case PPPOE_CMD_GETRETRYTIMER:
            if (rfc->flags & PPPOE_FLAG_DEBUG)
                log(LOG_INFO, "PPPoE command (0x%x): get retry timer = %d\n", rfc, rfc->timer_retry_setup);
            *(u_int16_t *)cmddata = rfc->timer_retry_setup;
            break;

        case PPPOE_CMD_SETRETRYTIMER:
            if (rfc->flags & PPPOE_FLAG_DEBUG)
                log(LOG_INFO, "PPPoE command (0x%x): set retry timer = %d\n", rfc, *(u_int16_t *)cmddata);
            rfc->timer_retry_setup = *(u_int16_t *)cmddata;
            break;

        // set the peer address to specify an ethernet address to connect to
        // must be called before every connect
        case PPPOE_CMD_SETPEERADDR:	
            if (rfc->flags & PPPOE_FLAG_DEBUG) {
                u_char *p = cmddata;
                log(LOG_INFO, "PPPoE command (0x%x): set peer ethernet address = %x:%x:%x:%x:%x:%x\n", rfc, p[0], p[1], p[2], p[3], p[4], p[5]);
            }
            if (rfc->state != PPPOE_STATE_DISCONNECTED) {
                error = 1;
                break;
            }
            memcpy(rfc->peer_address, cmddata, ETHER_ADDR_LEN);
            break;

        // return the ethernet address we are connected to
        // broadcast and zero-address are treated the same way.
        case PPPOE_CMD_GETPEERADDR:
            if (rfc->flags & PPPOE_FLAG_DEBUG) {
                u_char *p = rfc->peer_address;
                log(LOG_INFO, "PPPoE command (0x%x): get peer ethernet address = %x:%x:%x:%x:%x:%x\n", rfc, p[0], p[1], p[2], p[3], p[4], p[5]);
            }
            memcpy(cmddata, rfc->peer_address, ETHER_ADDR_LEN);
            break;

        default:
            if (rfc->flags & PPPOE_FLAG_DEBUG)
                log(LOG_INFO, "PPPoE command (0x%x): unknown command = %d\n", rfc, cmd);
    }

    return error;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_input(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from, u_int16_t typ)
{

    //log(LOG_INFO, "PPPoE input, rfc = 0x%x\n", rfc);
   switch (typ) {
        case PPPOE_ETHERTYPE_CTRL:
            return handle_ctrl(rfc, m, from);

        case PPPOE_ETHERTYPE_DATA:
            return handle_data(rfc, m, from);
    }

    return 0;
}

/* -----------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
-------------------------- private functions -----------------------------------
--------------------------------------------------------------------------------
-------------------------------------------------------------------------------
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
send an event up to our cliente
----------------------------------------------------------------------------- */
void send_event(struct pppoe_rfc *rfc, u_int32_t event, u_int32_t msg)
{
    if (rfc->flags & PPPOE_FLAG_DEBUG) {
        char  *text;
        switch (event) {
            case PPPOE_EVT_RINGING: text = "RINGING"; break;
            case PPPOE_EVT_CONNECTED: text = "CONNECTED"; break;
            case PPPOE_EVT_DISCONNECTED: text = "DISCONNECTED"; break;
        }
        log(LOG_INFO, "PPPoE event %s (0x%x)\n", text, rfc);
    }
    if (rfc->eventcb)
        (*rfc->eventcb)(rfc->host, event, msg);
}

/* -----------------------------------------------------------------------------
get the value for the tag
maxlen contains the max size of name (including null terminator)
return 1 if the tag was found and it could fit with maxlen, 0 otherwise
----------------------------------------------------------------------------- */
u_int16_t get_tag(struct mbuf *m, u_int16_t tag, struct pppoe_tag *val)
{
    u_int8_t 		*data, *tmpbuf;
    struct pppoe	*p;
    u_int16_t 		totallen, copylen, len;

    data = mtod(m, u_int8_t *);
    p = (struct pppoe *)data;
    totallen =  p->len;

    // Prevent buffer overflow - the PPPoE RFC gurantees us a maximum packet size, however, an attacker
    // might very well produce huge packets.
    if (totallen > PPPOE_TMPBUF_SIZE)
        totallen = PPPOE_TMPBUF_SIZE;

    copylen = MIN(totallen, m->m_len - sizeof(struct pppoe));

    tmpbuf = &pppenet_tmpbuf[0];
    memcpy(tmpbuf, data + sizeof(struct pppoe), copylen);

    for (m = m->m_next; m && (copylen < totallen); m = m->m_next) {
        if (m->m_len == 0)
            continue;

        len = MIN(totallen - copylen, m->m_len);
        data = mtod(m, u_int8_t *);
        memcpy(tmpbuf + copylen, data, len);
        copylen += len;
    }

    totallen = copylen;
    data = tmpbuf;

    val->len = 0;

    while (totallen > 0) {
        //log(LOG_INFO, "tag 0x%x 0x%x 0x%x 0x%x \n", data[0], data[1], data[2], data[3]);
        if (*(u_int16_t *)data == tag) {
            data += 2;
            copylen = *(u_int16_t *)data;
            if (copylen <= val->max_len) {
                memcpy(val->data, data + 2, copylen);
                val->len = copylen;
                if (val->len < val->max_len)
                    val->data[val->len] = 0;
                return 1;
            }
            //log(LOG_INFO, "got PADO : %s\n", name);
            break;
        }
        else {
            totallen -= 4 + (*(u_int16_t *)(data + 2));
            data += 4 + (*(u_int16_t *)(data + 2));
        }
    }

    return 0;
}

/* -----------------------------------------------------------------------------
add a tag to the data, return the len added
----------------------------------------------------------------------------- */
u_int16_t add_tag(u_int8_t *data, u_int16_t tag, struct pppoe_tag *val)
{
    
    *(u_int16_t *)data = htons(tag);
    data += 2;
    *(u_int16_t *)data = htons(val->len);
    data += 2;
    memcpy(data, val->data, val->len);

    return (val->len + 4);
}

/* -----------------------------------------------------------------------------
address MUST be a valid ethernet address (6 bytes length)
----------------------------------------------------------------------------- */
void send_PAD(struct pppoe_rfc *rfc, u_int8_t *address, u_int16_t code, u_int16_t sessid,
                     struct pppoe_tag *ac_name, struct pppoe_tag *service,
                     struct pppoe_tag *host_uniq, struct pppoe_tag *ac_cookie,
                     struct pppoe_tag *relay_id)
{
    struct mbuf 	*m;
    u_int8_t 		*data;
    u_int16_t 		len;
    struct pppoe	*p;

    if ((m = m_gethdr(M_WAIT, MT_DATA)) == NULL)
        return;

    MCLGET(m, M_WAIT);
    if (!(m->m_flags & M_EXT)) {
        m_freem(m);
        return;
    }
    //MH_ALIGN(m, sizeof(struct pppoe) + namelen + 4 + servlen + 4);
    data = mtod(m, u_int8_t *);

    p = (struct pppoe *)data;
    p->ver = PPPOE_VER;
    p->typ = PPPOE_TYPE;
    p->code = code;
    p->sessid = sessid;
    p->len = 0;

    data += sizeof(struct pppoe);

    if (service) {
        len = add_tag(data, PPPOE_TAG_SERVICE_NAME, service);
        p->len += len;
        data += len;
    }

    if (ac_name) {
        len = add_tag(data, PPPOE_TAG_AC_NAME, ac_name);
        p->len += len;
        data += len;
    }

    if (host_uniq) {
        len = add_tag(data, PPPOE_TAG_HOST_UNIQ, host_uniq);
        p->len += len;
        data += len;
    }

    if (ac_cookie) {
        len = add_tag(data, PPPOE_TAG_AC_COOKIE, ac_cookie);
        p->len += len;
        data += len;
    }

    if (relay_id) {
        len = add_tag(data, PPPOE_TAG_RELAY_SESSION_ID, relay_id);
        p->len += len;
        data += len;
    }

    m->m_len = sizeof(struct pppoe) + p->len;
    m->m_pkthdr.len = sizeof(struct pppoe) + p->len;
    p->len = htons(p->len);

    pppoe_rfc_lower_output(rfc, m, address, PPPOE_ETHERTYPE_CTRL);
}

/* -----------------------------------------------------------------------------
m contains ethernet header and the actual ethernet data
from MUST be a valid ethernet address (6 bytes length)
----------------------------------------------------------------------------- */
u_int16_t handle_PADI(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from)
{
    PPPOE_TAG(name, PPPOE_AC_NAME_LEN);
    PPPOE_TAG(service, PPPOE_SERVICE_LEN);
    PPPOE_TAG(hostuniq, PPPOE_HOST_UNIQ_LEN);
    PPPOE_TAG(relay, PPPOE_RELAY_ID_LEN);

    if (rfc->state != PPPOE_STATE_LISTENING)
        return 0;

    PPPOE_TAG_SETUP(name);
    PPPOE_TAG_SETUP(service);
    PPPOE_TAG_SETUP(hostuniq);
    PPPOE_TAG_SETUP(relay);

    get_tag(m, PPPOE_TAG_AC_NAME, &name);
    get_tag(m, PPPOE_TAG_SERVICE_NAME, &service);

    if (rfc->flags & PPPOE_FLAG_DEBUG)
        log(LOG_INFO, "PPPoE receive PADI (0x%x): requested service\\name = '%.64s\\%.64s', our service\\name = '%.64s\\%.64s'\n",
        rfc, service.data, name.data, rfc->serv_service.data, rfc->serv_ac_name.data);

    // if the client does not specify any name, we must offer ther service
    // if the client does not specify the service, we still accept it, but don't advertise
    // any service in the reply
    if ((!name.len || !PPPOE_TAG_CMP(name, rfc->serv_ac_name))
        && (!service.len || !PPPOE_TAG_CMP(service, rfc->serv_service))) {
        
        get_tag(m, PPPOE_TAG_HOST_UNIQ, &hostuniq);
        get_tag(m, PPPOE_TAG_RELAY_SESSION_ID, &relay);

        // could generate and use ac-cookie, but in that case we would need to change state
        // do it later...
        send_PAD(rfc, from, PPPOE_PADO, 0, &rfc->serv_ac_name, service.len ? &service : 0, hostuniq.len ? &hostuniq : 0, 0, relay.len ? &relay : 0);
        return 1;
    }

    return 0;
}

/* -----------------------------------------------------------------------------
m contains ethernet header and the actual ethernet data
from MUST be a valid ethernet address (6 bytes length)
----------------------------------------------------------------------------- */
u_int16_t handle_PADO(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from)
{
    PPPOE_TAG(name, PPPOE_AC_NAME_LEN);
    PPPOE_TAG(service, PPPOE_SERVICE_LEN);
    PPPOE_TAG(hostuniq, PPPOE_HOST_UNIQ_LEN);

    if (rfc->state != PPPOE_STATE_LOOKING)
        return 0;

    PPPOE_TAG_SETUP(name);
    PPPOE_TAG_SETUP(service);
    PPPOE_TAG_SETUP(hostuniq);

    get_tag(m, PPPOE_TAG_AC_NAME, &name);
    get_tag(m, PPPOE_TAG_SERVICE_NAME, &service);

    if (rfc->flags & PPPOE_FLAG_DEBUG)
        log(LOG_INFO, "PPPoE receive PADO (0x%x): offered service\\name = '%.64s\\%.64s', expected service\\name = '%.64s\\%.64s'\n",
        rfc, service.data, name.data, rfc->service.data, rfc->ac_name.data);

    // since we sent PPPOE_TAG_HOST_UNIQ in our PADI, the tag MUST be present in this PADO
    get_tag(m, PPPOE_TAG_HOST_UNIQ, &hostuniq);

    // the connecting rfc is identified by our host_uniq value
    // check if the ac-name and ac-service match our expectations
#ifndef PPPENET_COMPAT
    if (!PPPOE_TAG_CMP(hostuniq, rfc->host_uniq)
        && (!rfc->ac_name.len || !PPPOE_TAG_CMP(name, rfc->ac_name))
        && (!rfc->service.len || !PPPOE_TAG_CMP(service, rfc->service)) ) {
#endif
        get_tag(m, PPPOE_TAG_AC_COOKIE, &rfc->ac_cookie);
        get_tag(m, PPPOE_TAG_RELAY_SESSION_ID, &rfc->relay_id);
        
        memcpy(rfc->peer_address, from, ETHER_ADDR_LEN);
        // resend PADI/PADR every PPPOE_TIMEOUT_RETRY seconds
        rfc->timer_connect_resend = rfc->timer_connect - rfc->timer_retry_setup;

        send_PAD(rfc, rfc->peer_address, PPPOE_PADR, 0, 
                rfc->ac_name.len ? &rfc->ac_name : 0, &rfc->service,
                &rfc->host_uniq, 
                rfc->ac_cookie.len ? &rfc->ac_cookie : 0, 
                rfc->relay_id.len ? &rfc->relay_id : 0);
        rfc->state = PPPOE_STATE_CONNECTING;
        return 1;
#ifndef PPPENET_COMPAT
    }
#endif    
    return 0;
}

/* -----------------------------------------------------------------------------
m contains ethernet header and the actual ethernet data
from MUST be a valid ethernet address (6 bytes length)
----------------------------------------------------------------------------- */
u_int16_t handle_PADR(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from)
{
    PPPOE_TAG(name, PPPOE_AC_NAME_LEN);
    PPPOE_TAG(service, PPPOE_SERVICE_LEN);
    
    if (rfc->state != PPPOE_STATE_LISTENING)
        return 0;

    PPPOE_TAG_SETUP(name);
    PPPOE_TAG_SETUP(service);

    get_tag(m, PPPOE_TAG_AC_NAME, &name);
    get_tag(m, PPPOE_TAG_SERVICE_NAME, &service);
    
    if (rfc->flags & PPPOE_FLAG_DEBUG)
        log(LOG_INFO, "PPPoE receive PADR (0x%x): requested service\\name = '%.64s\\%.64s', our service\\name = '%.64s\\%.64s'\n", rfc, service.data, name.data, rfc->serv_service.data, rfc->serv_ac_name.data);

    // if the client does not specify any name, we must offer ther service
    // if the client does not specify the service, still accept it ?
    if ((!name.len || !PPPOE_TAG_CMP(name, rfc->serv_ac_name))
        && (!service.len || !PPPOE_TAG_CMP(service, rfc->serv_service))) {
        
        memcpy(rfc->peer_address, from, ETHER_ADDR_LEN);
        
        get_tag(m, PPPOE_TAG_HOST_UNIQ, &rfc->host_uniq);
        get_tag(m, PPPOE_TAG_RELAY_SESSION_ID, &rfc->relay_id);

        // change the state, so there is no other client trying to call...
        rfc->timer_ring = rfc->timer_ring_setup;
        rfc->state = PPPOE_STATE_RINGING;
        send_event(rfc, PPPOE_EVT_RINGING, 0);

        // only ring to the first client that matches...
        return 1;
    }

    return 0;
}

/* -----------------------------------------------------------------------------
m contains ethernet header and the actual ethernet data
from MUST be a valid ethernet address (6 bytes length)
----------------------------------------------------------------------------- */
u_int16_t handle_PADS(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from)
{
    PPPOE_TAG(hostuniq, PPPOE_HOST_UNIQ_LEN);
    struct pppoe 	*p = mtod(m, struct pppoe *);
    u_int16_t		sessid = ntohs(p->sessid);

    if (rfc->state != PPPOE_STATE_CONNECTING)
        return 0;

    PPPOE_TAG_SETUP(hostuniq);

    if (rfc->flags & PPPOE_FLAG_DEBUG)
        log(LOG_INFO, "PPPoE receive PADS (0x%x): session id = 0x%x\n", rfc, sessid);

    // since we sent PPPOE_TAG_HOST_UNIQ in our PADR, the tag MUST be present in this PADS
    get_tag(m, PPPOE_TAG_HOST_UNIQ, &hostuniq);

    // the connecting rfc is identified by our host_uniq value
#ifndef PPPENET_COMPAT
    if (!PPPOE_TAG_CMP(hostuniq, rfc->host_uniq)
        // shall we be restrictive, and check the ethernet address ?
        && !memcmp(rfc->peer_address, from, ETHER_ADDR_LEN)
        ) {
#endif
//        memcpy(rfc->peer_address, from, ETHER_ADDR_LEN);
        rfc->state = PPPOE_STATE_CONNECTED;
        rfc->session_id = sessid;
        send_event(rfc, PPPOE_EVT_CONNECTED, 0);

        return 1;
#ifndef PPPENET_COMPAT
    }
#endif   
    return 0;
}

/* -----------------------------------------------------------------------------
m contains ethernet header and the actual ethernet data
from MUST be a valid ethernet address (6 bytes length)
----------------------------------------------------------------------------- */
u_int16_t handle_PADT(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from)
{
    struct pppoe 	*p = mtod(m, struct pppoe *);
    u_int16_t 		sessid = ntohs(p->sessid);

    if (rfc->state != PPPOE_STATE_CONNECTED)
        return 0;

    if (rfc->flags & PPPOE_FLAG_DEBUG)
        log(LOG_INFO, "PPPoE receive PADT (0x%x): requested session id = 0x%x, our session id = 0x%x\n", rfc, sessid, rfc->session_id);

    if ((sessid == rfc->session_id) && !memcmp(rfc->peer_address, from, ETHER_ADDR_LEN)) {

        rfc->state = PPPOE_STATE_DISCONNECTED;
        bzero(rfc->peer_address, sizeof(rfc->peer_address));
        send_event(rfc, PPPOE_EVT_DISCONNECTED, 0);

        return 1;
    }
    return 0;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int16_t handle_ctrl(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from)
{
    struct pppoe 	*p = mtod(m, struct pppoe *);
    u_int16_t 		done = 0;

    //log(LOG_INFO, "handle_ctrl, rfc = 0x%x\n", rfc);

    switch (p->code) {
       case PPPOE_PADI:
            done = handle_PADI(rfc, m, from);
            break;
        case PPPOE_PADO:
            done = handle_PADO(rfc, m, from);
            break;
        case PPPOE_PADR:
            done = handle_PADR(rfc, m, from);
            break;
        case PPPOE_PADS:
            done = handle_PADS(rfc, m, from);
            break;
       case PPPOE_PADT:
            done = handle_PADT(rfc, m, from);
            break;
    }

    // time to free mbuf now...
    if (done) 
        m_freem(m);
    
    return done;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int16_t handle_data(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from)
{
    struct pppoe 	*p = mtod(m, struct pppoe *);
    u_int16_t 		sessid = ntohs(p->sessid);

    //log(LOG_INFO, "handle_data, rfc = 0x%x, from %x:%x:%x:%x:%x:%x\n", rfc, from[0],from[1],from[2],from[3],from[4],from[5] );

    // check identify the session, we must check the session id AND the address of the peer
    // we could be connected to 2 different AC with the same session id
    // or to 1 AC with 2 session id
    if ((rfc->state == PPPOE_STATE_CONNECTED)
        && (rfc->session_id == sessid)
        && !memcmp(rfc->peer_address, from, ETHER_ADDR_LEN)) {

        // adjust the packet length
        // don't only use the m_adj function, because the packet we get here is a etherneet packet
        // and ethernet packet have a minimum size of 64 bytes.
        // i.e when getting small packets, mbuf still has at least 64 bytes.
        // so we need to adjust the len accrding to the len field from the pppoe header
        // should do something safer, in case first buffer smaller than 6 bytes ???

        m->m_data += sizeof(struct pppoe);
        if (m->m_next)
            m->m_len -= sizeof(struct pppoe);
        else
            m->m_len = p->len;
        m->m_pkthdr.len = p->len;

        // packet is passed up to the host
        if (rfc->inputcb)
                (*rfc->inputcb)(rfc->host, m);

        // let's say the packet have been treated
        return 1;
    }

    // the packet was not for us
    return 0;
}

/* -----------------------------------------------------------------------------
called from pppoe_rfc when data need to be sent
----------------------------------------------------------------------------- */
void pppoe_rfc_lower_output(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *to, u_int16_t typ)
{

    //log(LOG_INFO, "PPPoE_output\n");
    if (rfc->flags & PPPOE_FLAG_LOOPBACK) {
        struct mbuf 	*m1;
        u_int8_t 	from[ETHER_ADDR_LEN] = { 0, 0, 0, 0, 0, 0 };
        *(u_int32_t *)&from[0] = rfc->uniqueid;        
    
        MGETHDR(m1, M_DONTWAIT, MT_DATA);
        if (m1 == 0) {
            m_freem(m);
            return;
        }
        MCLGET(m1, M_DONTWAIT);
        if (!(m1->m_flags & M_EXT)) {
            m_freem(m);
            m_freem(m1);
            return;
        }
        m_copydata(m, 0, m->m_pkthdr.len, mtod(m1, caddr_t));
        m1->m_len = m->m_pkthdr.len;
        m1->m_pkthdr.len = m->m_pkthdr.len;
        m_freem(m);
        pppoe_rfc_lower_input(rfc->dl_tag, m1, from, typ);
        return;
    }

    pppoe_dlil_output(rfc->dl_tag, m, to, typ);
}

/* -----------------------------------------------------------------------------
called from pppoe_dlil when pppoe data are present
----------------------------------------------------------------------------- */
void pppoe_rfc_lower_input(u_long dl_tag, struct mbuf *m, u_int8_t *from, u_int16_t typ)
{
    struct pppoe_rfc  	*rfc, *lastrfc;
    
    //log(LOG_INFO, "PPPoE inputdata, tag = %d\n", dl_tag);

    TAILQ_FOREACH(rfc, &pppoe_rfc_head, next) {
        // we use dl_tag because we only respond to the peer on the same interface
        if (rfc->dl_tag == dl_tag) {

            if (pppoe_rfc_input(rfc, m, from, typ))
                return;
                
            lastrfc = rfc;
        }
    }

    // the last matching rfc itself is irrelevant, just need unit number and tag information
    
    log(LOG_INFO, "PPPoE inputdata: unexpected %s packet on unit = %d\n", 
        (typ == PPPOE_ETHERTYPE_CTRL ? "control" : "data"), lastrfc->unit);
        
    if (typ == PPPOE_ETHERTYPE_DATA) {
        // in case of PPPOE_ETHERTYPE_DATA, send a PADT to the peer
        // trying to talk to us with an incorrect session id
        struct pppoe	*p = mtod(m, struct pppoe *);
        u_int16_t	sessid = ntohs(p->sessid);
        send_PAD(lastrfc, from, PPPOE_PADT, sessid, 0, 0, 0, 0, 0);
    }
    
    // nobody was intersted in the packet, just ignore it
    m_freem(m);
}

/* -----------------------------------------------------------------------------
calls when the lower layer is detaching
----------------------------------------------------------------------------- */
void pppoe_rfc_lower_detaching(u_long dl_tag)
{
    struct pppoe_rfc  	*rfc;
    
    TAILQ_FOREACH(rfc, &pppoe_rfc_head, next) {

        if (rfc->dl_tag == dl_tag) {

            if (rfc->flags & PPPOE_FLAG_DEBUG)
                log(LOG_INFO, "PPPoE lower layer detaching (0x%x): ethernet unit = %d\n", rfc, rfc->unit);
        
            pppoe_dlil_detach(rfc->dl_tag);
            rfc->dl_tag = 0;
            rfc->unit = 0xFFFF;
        
            if (rfc->state != PPPOE_STATE_DISCONNECTED) {
        
                rfc->state = PPPOE_STATE_DISCONNECTED;
                bzero(rfc->peer_address, sizeof(rfc->peer_address));
                send_event(rfc, PPPOE_EVT_DISCONNECTED, ENXIO);
            }
        }
    }
}
