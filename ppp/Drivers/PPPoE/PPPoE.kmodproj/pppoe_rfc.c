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

#include "PPPoE.h"
#include "pppoe_rfc.h"
#include "pppoe_dlil.h"


/* -----------------------------------------------------------------------------
Definitions
----------------------------------------------------------------------------- */

//#define PPPENET_COMPAT 1

struct pppoe_if {
    TAILQ_ENTRY(pppoe_if) next;
    u_short           	unit;
    u_long	    	dl_tag;
};

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


#define PPPOE_TIMER_CONNECT 		10	 // let's have a connect timer of 10 seconds
#define PPPOE_TIMER_RING 		30	 // let's have a ring timer of 30 seconds

#define SERVER_NAME "Darwin\0"
#define SERVICE_NAME "Think-Different\0"

#define	PPPOE_HOST_UNIQ_LEN		64	// 63 bytes + null char (need more ???)
#define	PPPOE_RELAY_ID_LEN		64	// 63 bytes + null char (need more ???)
#define	PPPOE_AC_COOKIE_LEN		64	// 63 bytes + null char (need more ???)

struct pppoe {
    u_int8_t ver:4;
    u_int8_t typ:4;
    u_int8_t code;
    u_int16_t sessid;
    u_int16_t len;
};

struct pppoe_rfc {

    // administrative info
    TAILQ_ENTRY(pppoe_rfc) 	next;
    void 			*host; 			/* pointer back to the hosting structure */
    u_long	    		dl_tag;			/* associated interface */
    pppoe_rfc_input_callback 	inputcb;		/* callback function when data are present */
    pppoe_rfc_event_callback 	eventcb;		/* callback function for events */
    u_int8_t  			loopback;		/* is client in loopback mode ? */

    // current state
    u_int8_t 		state;				/* PPPoE current state */
    
    // outgoing call
    u_int8_t 	ac_name[PPPOE_AC_NAME_LEN];		/* Access Concentrator we want to reach */
    u_int8_t 	service[PPPOE_SERVICE_LEN];		/* Service name we want to reach */
    u_int16_t	timer_connect;				/* second remaining before aborting outgoing call */

    // incoming call
    u_int8_t  	serv_ac_name[PPPOE_AC_NAME_LEN];	/* Access Concentrator we offer */
    u_int8_t  	serv_service[PPPOE_SERVICE_LEN];	/* Service name we offer */
    u_int16_t	timer_ring;

    // commom outgoing/incoming call
    u_int8_t 	host_uniq[PPPOE_HOST_UNIQ_LEN];		/* client reserved cookie */
    u_int8_t 	relay_id[PPPOE_RELAY_ID_LEN];		/* intermediate relay cookie */
    u_int16_t	session_id;				/* session id between client and server */
    u_int8_t	peer_address[ETHER_ADDR_LEN];		/* ethernet address we are connected to */

};


/* -----------------------------------------------------------------------------
Globals
----------------------------------------------------------------------------- */
// change the algo to remove that var
u_int8_t 	pppenet_tmpbuf[1500];

u_int16_t 	pppoe_unique_session_id = 1;
u_int32_t 	pppoe_unique_address = 1;

//#define queues 1

TAILQ_HEAD(, pppoe_if) 	pppoe_if_head;
#ifdef queues
TAILQ_HEAD(, pppoe_rfc) 	pppoe_rfc_head;
#else
#define MAX_PPPOE_RFC 32
struct pppoe_rfc *pppoe_rfc[MAX_PPPOE_RFC];
#endif

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

static void send_PAD(struct pppoe_rfc *rfc, u_int8_t *address, u_int16_t code, u_int16_t sessid,
                     u_int8_t *ac_name, u_int8_t *service,
                     u_int8_t *host_uniq, u_int8_t *ac_cookie, u_int8_t *relay_id);

static u_int16_t add_tag(u_int8_t *data, u_int16_t tag, u_int8_t *val);
static u_int16_t get_tag(struct mbuf *m, u_int16_t tag, u_int8_t *name, u_int16_t maxlen);

u_int16_t pppoe_rfc_input(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from, u_int16_t typ);
void pppoe_rfc_lower_output(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *to, u_int16_t typ);



/* -----------------------------------------------------------------------------
intialize pppoe protocol on ethernet unit
need to change it to handle multiple ethernet interfaces
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_init()
{
#ifndef  queues
    int i;
#endif
    log(LOG_INFO, "pppoe_rfc_init\n");

    TAILQ_INIT(&pppoe_if_head);
#ifdef  queues
    TAILQ_INIT(&pppoe_rfc_head);
#else
    for  (i = 0; i < MAX_PPPOE_RFC; i++) {
        pppoe_rfc[i] = 0;
    }
#endif
    return 0;
}

/* -----------------------------------------------------------------------------
dispose of a pppoe protocol
----------------------------------------------------------------------------- */
void pppoe_rfc_dispose()
{
    struct pppoe_if  	*pppoeif;

    log(LOG_INFO, "pppoe_rfc_dispose\n");

    while (pppoeif = TAILQ_FIRST(&pppoe_if_head))
        pppoe_rfc_detach(pppoeif->unit);
}

/* -----------------------------------------------------------------------------
attach pppoe to ethernet unit
----------------------------------------------------------------------------- */
int pppoe_rfc_attach(u_short unit)
{
    struct pppoe_if  	*pppoeif;
    u_long		dl_tag;
    int			ret;
    
    log(LOG_INFO, "pppoe_rfc_attach\n");

    MALLOC(pppoeif, struct pppoe_if *, sizeof(struct pppoe_if), M_TEMP, M_WAITOK);
    if (!pppoeif) {
        log(LOG_INFO, "pppoe_rfc_attach : Can't allocate attachment structure\n");
        return 1;
    }

    ret = pppoe_dlil_attach(APPLE_IF_FAM_ETHERNET, unit, &dl_tag);
    if (ret) {
        log(LOG_INFO, "pppoe_rfc_attach : Can't attach to interface\n");
        return ret;
    }

    bzero(pppoeif, sizeof(struct pppoe_if));

    pppoeif->unit = unit;
    pppoeif->dl_tag = dl_tag;

    TAILQ_INSERT_TAIL(&pppoe_if_head, pppoeif, next);
    return 0;
}

/* -----------------------------------------------------------------------------
detach pppoe from ethernet unit
----------------------------------------------------------------------------- */
int pppoe_rfc_detach(u_short unit)
{
    struct pppoe_if  	*pppoeif;

    log(LOG_INFO, "pppoe_rfc_detach\n");

    TAILQ_FOREACH(pppoeif, &pppoe_if_head, next) {
        if (pppoeif->unit == unit) {
            pppoe_dlil_detach(pppoeif->dl_tag);
            TAILQ_REMOVE(&pppoe_if_head, pppoeif, next);
            FREE(pppoeif, M_TEMP);
            break;
        }
    }

    return 0;
}

/* -----------------------------------------------------------------------------
intialize a new pppoe structure
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_new_client(void *host, u_int16_t unit, void **data,
                         pppoe_rfc_input_callback input,
                         pppoe_rfc_event_callback event)
{
    struct pppoe_rfc 	*rfc;
    struct pppoe_if  	*pppoeif;
#ifndef  queues
    int i;
#endif
    
    log(LOG_INFO, "pppoe_rfc_new_client\n");

    rfc = (struct pppoe_rfc *)_MALLOC(sizeof (struct pppoe_rfc), M_TEMP, M_WAITOK);
    if (rfc == 0)
        return 1;

    log(LOG_INFO, "pppoe_rfc_new_client rfc = 0x%x\n", rfc);

    bzero(rfc, sizeof(struct pppoe_rfc));

    rfc->host = host;
    rfc->inputcb = input;
    rfc->eventcb = event;

    TAILQ_FOREACH(pppoeif, &pppoe_if_head, next) {
        if (pppoeif->unit == unit) {
            rfc->dl_tag = pppoeif->dl_tag;
            break;
        }
    }

    rfc->state = PPPOE_STATE_DISCONNECTED;
    strcpy(rfc->serv_ac_name, SERVER_NAME);
    strcpy(rfc->serv_service, SERVICE_NAME);

    *data = rfc;

    log(LOG_INFO, "pppoe_rfc_new_client, tag = 0x%x\n", rfc->dl_tag);

#ifdef queues
    TAILQ_INSERT_TAIL(&pppoe_rfc_head, rfc, next);
#else
    for (i = 0; i < MAX_PPPOE_RFC; i++) {
        if (!pppoe_rfc[i]) {
            pppoe_rfc[i] = rfc;
            break;
        }
    }
#endif

    return 0;
}

/* -----------------------------------------------------------------------------
dispose of a pppoe structure
----------------------------------------------------------------------------- */
void pppoe_rfc_free_client(void *data)
{
    struct pppoe_rfc 	*rfc = (struct pppoe_rfc *)data;
#ifndef  queues
    int i;
#endif
    
    log(LOG_INFO, "pppoe_rfc_free_client, rfc = 0x%x \n", rfc);

    if (rfc) {
#ifdef queues
        TAILQ_REMOVE(&pppoe_rfc_head, rfc, next);
#else
    for (i = 0; i < MAX_PPPOE_RFC; i++) {
        if (rfc == pppoe_rfc[i]) {
            pppoe_rfc[i] = 0;
            break;
        }
    }
#endif
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


    if (rfc->state != PPPOE_STATE_DISCONNECTED)
        return 1;

    log(LOG_INFO, "pppoe_rfc_connect : ac-name = '%s', service = '%s'\n", ac_name, service);

    strcpy(rfc->ac_name, ac_name);
    strcpy(rfc->service, service);
    // use our rfc address for the uniq id, we now it won't change since we allocate ourserlves
    *(u_int32_t *)&rfc->host_uniq[0] = (u_int32_t)rfc;
    rfc->host_uniq[sizeof(u_int32_t)] = 0;

    rfc->state = PPPOE_STATE_LOOKING;
    rfc->timer_connect = PPPOE_TIMER_CONNECT; // let's have a connect timer of 10 seconds

    // if ac-name specified, try to reach it, otherwise, don't use name\
    // may be shoult use a '*' semantic in the address ?
    send_PAD(rfc, broadcastaddr, PPPOE_PADI, 0, rfc->ac_name, rfc->service, rfc->host_uniq, 0, 0);
    return 0;
}

/* -----------------------------------------------------------------------------
set server and service name
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_bind(void *data, u_int8_t *ac_name, u_int8_t *service)
{
    struct pppoe_rfc 	*rfc = (struct pppoe_rfc *)data;

    log(LOG_INFO, "pppoe_rfc_bind : ac-name = '%s', service = '%s'\n",
        ac_name ? (char*)ac_name : "nul", service ? (char*)service : "nul");

    if (ac_name)
        strcpy(rfc->serv_ac_name, ac_name);
    if (service)
        strcpy(rfc->serv_service, service);
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

    log(LOG_INFO, "pppoe_rfc_accept\n");

    // should check wih other numbers currently sets with  active sessions
    rfc->session_id = pppoe_unique_session_id++; // generate a session id

    // host_uniq and rfc->relay_session_id have been got from the previous PADR
    send_PAD(rfc, rfc->peer_address, PPPOE_PADS, rfc->session_id, rfc->ac_name, rfc->service,
             rfc->host_uniq[0] ? rfc->host_uniq : 0, 0, rfc->relay_id[0] ? rfc->relay_id : 0);

    rfc->state = PPPOE_STATE_CONNECTED;

    if (rfc->eventcb)
        (*rfc->eventcb)(rfc->host, PPPOE_EVT_CONNECTED, 0);

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

    log(LOG_INFO, "pppoe_rfc_listen\n");

    rfc->state = PPPOE_STATE_LISTENING;
    
    return 0;
}

/* -----------------------------------------------------------------------------
abort current connection, depend on the state
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_abort(void *data)
{
    struct pppoe_rfc 	*rfc = (struct pppoe_rfc *)data;

    log(LOG_INFO, "pppoe_rfc_abort\n");

    switch (rfc->state) {
        case PPPOE_STATE_LOOKING:
        case PPPOE_STATE_CONNECTING:
        case PPPOE_STATE_LISTENING:
        case PPPOE_STATE_RINGING:
            rfc->state = PPPOE_STATE_DISCONNECTED;
            bzero(rfc->peer_address, sizeof(rfc->peer_address));
            if (rfc->eventcb)
                    (*rfc->eventcb)(rfc->host, PPPOE_EVT_DISCONNECTED, 0);
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

    log(LOG_INFO, "pppoe_rfc_disconnect\n");

    send_PAD(rfc, rfc->peer_address, PPPOE_PADT, rfc->session_id, 0, 0, 0, 0, 0);

    rfc->state = PPPOE_STATE_DISCONNECTED;
    bzero(rfc->peer_address, sizeof(rfc->peer_address));
    if (rfc->eventcb)
            (*rfc->eventcb)(rfc->host, PPPOE_EVT_DISCONNECTED, 0);

    return 0;
}

/* -----------------------------------------------------------------------------
clone the two data structures
NOTE : the host field is NOT copied
----------------------------------------------------------------------------- */
void pppoe_rfc_clone(void *data1, void *data2)
{
    struct pppoe_rfc 	*rfc2 = (struct pppoe_rfc *)data2;
    void 		*host2;

    log(LOG_INFO, "pppoe_rfc_clone\n");

    host2 = rfc2->host;
    bcopy(data1, data2, sizeof(struct pppoe_rfc));
    rfc2->host = host2;
}

/* -----------------------------------------------------------------------------
address MUST be prepared to hold an ethernet address (6 bytes length)
----------------------------------------------------------------------------- */
void pppoe_rfc_getpeeraddr(void *data, u_int8_t *address)
{
    struct pppoe_rfc 	*rfc = (struct pppoe_rfc *)data;

    bcopy(rfc->peer_address, address, ETHER_ADDR_LEN);
}

/* -----------------------------------------------------------------------------
called by protocol family when slowtiemr expires
check the active timers and fires them appropriatly

refaire les timers
----------------------------------------------------------------------------- */
void pppoe_rfc_timer()
{
    struct pppoe_rfc  	*rfc;
#ifndef  queues
    int i;
#endif

#ifdef queues
    
    TAILQ_FOREACH(rfc, &pppoe_rfc_head, next) {

#else
        for (i = 0; i < MAX_PPPOE_RFC; i++) {

            rfc = pppoe_rfc[i];
            if (!rfc)
                continue;
#endif
        switch (rfc->state) {
            case PPPOE_STATE_LOOKING:
            case PPPOE_STATE_CONNECTING:
                if (rfc->timer_connect &&
                    (--rfc->timer_connect == 0)) {
                    log(LOG_INFO, "pppoe_rfc_timer, CONNECT_TIMER expires\n");
                    rfc->state = PPPOE_STATE_DISCONNECTED;
                    bzero(rfc->peer_address, sizeof(rfc->peer_address));
                    // double-check for the error number ?
                    if (rfc->eventcb)
                        (*rfc->eventcb)(rfc->host, PPPOE_EVT_DISCONNECTED, EHOSTUNREACH);
                }
                break;
            case PPPOE_STATE_RINGING:
                if (rfc->timer_ring &&
                    (--rfc->timer_ring == 0)) {
                    log(LOG_INFO, "pppoe_rfc_timer, RING_TIMER expires\n");
                    rfc->state = PPPOE_STATE_LISTENING;
                    bzero(rfc->peer_address, sizeof(rfc->peer_address));
                }
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

   // log(LOG_INFO, "pppoe_rfc_write, len = %d\n", len);
    //d = mtod(m, u_int8_t *);
    //log(LOG_INFO, "pppoe_rfc_write, data = %x %x %x %x %x %x \n", d[0], d[1], d[2], d[3], d[4], d[5]);

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


    switch (cmd) {

        case PPPOE_CMD_SETLOOPBACK:
           rfc->loopback = *(u_int32_t *)cmddata ? 1 : 0;
            log(LOG_INFO, "pppoe_rfc_command, rfc = 0x%x loopback cmd = %d tag = 0x%x\n", rfc, rfc->loopback, rfc->dl_tag);
           break;

        case PPPOE_CMD_GETLOOPBACK:
            *(u_int32_t *)cmddata = rfc->loopback;
            break;

        default:
            log(LOG_INFO, "pppoe_rfc_command, unknown cmd = %d, data = 0x%x\n", cmd, cmddata);
    }

    return 0;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int16_t pppoe_rfc_input(struct pppoe_rfc *rfc, struct mbuf *m, u_int8_t *from, u_int16_t typ)
{

    //log(LOG_INFO, "pppoe_rfc_input, rfc = 0x%x\n", rfc);
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
get the value for the tag
maxlen contains the max size of name (including null terminator)
return 1 if the tag was found and it could fit with maxlen, 0 otherwise
----------------------------------------------------------------------------- */
u_int16_t get_tag(struct mbuf *m, u_int16_t tag, u_int8_t *name, u_int16_t maxlen)
{
    u_int8_t 		*data, *tmpbuf;
    struct pppoe	*p;
    u_int16_t 		totallen, copylen, add, len;

    data = mtod(m, u_int8_t *);
    p = (struct pppoe *)data;
    totallen =  p->len;
    copylen = (totallen < (m->m_len - sizeof(struct pppoe))) ? totallen : m->m_len - sizeof(struct pppoe);

    add = 0;
    tmpbuf = &pppenet_tmpbuf[0];
    memcpy(tmpbuf + add, data + sizeof(struct pppoe), copylen);

    for (m = m->m_next; m && (copylen < totallen); m = m->m_next) {
        if (m->m_len == 0)
            continue;

        len = (totallen - copylen < m->m_len) ? totallen - copylen : m->m_len;
        data = mtod(m, u_int8_t *);
        memcpy(tmpbuf + add + copylen, data, len);
        copylen += len;
    }

    totallen = copylen + add;
    data = tmpbuf;

    *name = 0;
    
    while (totallen > 0) {
        //log(LOG_INFO, "tag 0x%x 0x%x 0x%x 0x%x \n", data[0], data[1], data[2], data[3]);
        if (*(u_int16_t *)data == tag) {
            data += 2;
            copylen = *(u_int16_t *)data;
            if (copylen > 255)
                copylen = 255;
            if (copylen < maxlen) {
                strncpy(name, data + 2, copylen);
                name[copylen] = 0;
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
u_int16_t add_tag(u_int8_t *data, u_int16_t tag, u_int8_t *val)
{
    u_int16_t 	len = strlen(val);
    
    *(u_int16_t *)data = htons(tag);
    data += 2;
    *(u_int16_t *)data = htons(len);
    data += 2;
    memcpy(data, val, len);

    return (len + 4);
}

/* -----------------------------------------------------------------------------
address MUST be a valid ethernet address (6 bytes length)
----------------------------------------------------------------------------- */
void send_PAD(struct pppoe_rfc *rfc, u_int8_t *address, u_int16_t code, u_int16_t sessid,
                     u_int8_t *ac_name, u_int8_t *service,
                     u_int8_t *host_uniq, u_int8_t *ac_cookie,
                     u_int8_t *relay_id)
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
    u_int8_t 	name[PPPOE_AC_NAME_LEN], service[PPPOE_SERVICE_LEN];
    u_int8_t 	hostuniq[PPPOE_HOST_UNIQ_LEN], relay[PPPOE_RELAY_ID_LEN];

    if (rfc->state != PPPOE_STATE_LISTENING)
        return 0;

    get_tag(m, PPPOE_TAG_AC_NAME, name, sizeof(name));
    get_tag(m, PPPOE_TAG_SERVICE_NAME, service, sizeof(service));

    log(LOG_INFO, "PPPoE handle_PADI, requested service\\name = '%s\\%s', our service\\name = '%s\\%s'\n",
        service, name, rfc->serv_service, rfc->serv_ac_name);

    // if the client does not specify any name, we must offer ther service
    // if the client does not specify the service, we still accept it, but don't advertise
    // any service in the reply
    // use strcasecmp instead of strcmp ??
    if ((!name[0] || !strcmp(name, rfc->serv_ac_name))
        && (!service[0] || !strcmp(service, rfc->serv_service))) {

        get_tag(m, PPPOE_TAG_HOST_UNIQ, hostuniq, sizeof(hostuniq));
        get_tag(m, PPPOE_TAG_RELAY_SESSION_ID, relay, sizeof(relay));

        // we could generate and use ac-cookie, but in that case we would need to change state
        // do it later...
        send_PAD(rfc, from, PPPOE_PADO, 0, rfc->serv_ac_name, service[0] ? service : 0, hostuniq[0] ? hostuniq : 0, 0, relay[0] ? relay : 0);
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
    u_int8_t 	name[PPPOE_AC_NAME_LEN], service[PPPOE_SERVICE_LEN];
    u_int8_t 	relay[PPPOE_RELAY_ID_LEN], cookie[PPPOE_AC_COOKIE_LEN];
    u_int8_t 	hostuniq[PPPOE_HOST_UNIQ_LEN];

    if (rfc->state != PPPOE_STATE_LOOKING)
        return 0;

    get_tag(m, PPPOE_TAG_AC_NAME, name, sizeof(name));
    get_tag(m, PPPOE_TAG_SERVICE_NAME, service, sizeof(service));

    log(LOG_INFO, "PPPoE handle_PADO, offered service\\name = '%s\\%s', expected service\\name = '%s\\%s'\n",
        service, name, rfc->service, rfc->ac_name);

    // since we sent PPPOE_TAG_HOST_UNIQ in our PADI, the tag MUST be present in this PADO
    get_tag(m, PPPOE_TAG_HOST_UNIQ, hostuniq, sizeof(hostuniq));

    // the connecting rfc is identified by our host_uniq value
    // check if we the ac-name and ac-service match our expectations
#ifndef PPPENET_COMPAT
    if (!strcmp(hostuniq, rfc->host_uniq)
        && (!rfc->ac_name[0] || !strcmp(name, rfc->ac_name))
        && (!rfc->service[0] || !strcmp(service, rfc->service)) ) {
#endif
        get_tag(m, PPPOE_TAG_AC_COOKIE, cookie, sizeof(cookie));
        get_tag(m, PPPOE_TAG_RELAY_SESSION_ID, relay, sizeof(relay));

        send_PAD(rfc, from, PPPOE_PADR, 0, rfc->ac_name[0] ? rfc->ac_name : 0, rfc->service,
                 hostuniq, cookie[0] ? cookie : 0, relay[0] ? relay : 0);
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
    u_int8_t 	name[PPPOE_AC_NAME_LEN], service[PPPOE_SERVICE_LEN];

    if (rfc->state != PPPOE_STATE_LISTENING)
        return 0;

    get_tag(m, PPPOE_TAG_AC_NAME, name, sizeof(name));
    get_tag(m, PPPOE_TAG_SERVICE_NAME, service, sizeof(service));

    log(LOG_INFO, "PPPoE handle_PADR, requested service\\name = '%s\\%s', our service\\name = '%s\\%s'\n", service, name, rfc->serv_service, rfc->serv_ac_name);

    // if the client does not specify any name, we must offer ther service
    // if the client does not specify the service, still accept it ?
    // use strcasecmp instead of strcmp ??
    if ((!name[0] || !strcmp(name, rfc->serv_ac_name))
        && (!service[0] || !strcmp(service, rfc->serv_service))) {
        
        memcpy(rfc->peer_address, from, ETHER_ADDR_LEN);
        
        log(LOG_INFO, "PPPOE_EVT_RINGING\n");

        get_tag(m, PPPOE_TAG_HOST_UNIQ, rfc->host_uniq, sizeof(rfc->host_uniq));
        get_tag(m, PPPOE_TAG_RELAY_SESSION_ID, rfc->relay_id, sizeof(rfc->relay_id));

        // change the state, so there is no other client trying to call...
        rfc->timer_ring = PPPOE_TIMER_RING;
        rfc->state = PPPOE_STATE_RINGING;
        if (rfc->eventcb)
                (*rfc->eventcb)(rfc->host, PPPOE_EVT_RINGING, 0);

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
    u_int8_t 		hostuniq[PPPOE_HOST_UNIQ_LEN];
    struct pppoe 	*p = mtod(m, struct pppoe *);
    u_int16_t		sessid = ntohs(p->sessid);

    if (rfc->state != PPPOE_STATE_CONNECTING)
        return 0;

    log(LOG_INFO, "PPPoE handle_PADS\n");

    // since we sent PPPOE_TAG_HOST_UNIQ in our PADR, the tag MUST be present in this PADS
    get_tag(m, PPPOE_TAG_HOST_UNIQ, hostuniq, sizeof(hostuniq));

    // the connecting rfc is identified by our host_uniq value
#ifndef PPPENET_COMPAT
    if (!strcmp(hostuniq, rfc->host_uniq)) {
#endif
        memcpy(rfc->peer_address, from, ETHER_ADDR_LEN);
        rfc->state = PPPOE_STATE_CONNECTED;
        rfc->session_id = sessid;
        if (rfc->eventcb)
                (*rfc->eventcb)(rfc->host, PPPOE_EVT_CONNECTED, 0);

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

    log(LOG_INFO, "PPPoE handle_PADT, requested session id = 0x%x', our session id = 0x%x\n", sessid, rfc->session_id);

    if ((sessid == rfc->session_id) && !memcmp(rfc->peer_address, from, ETHER_ADDR_LEN)) {

        rfc->state = PPPOE_STATE_DISCONNECTED;
        if (rfc->eventcb)
            (*rfc->eventcb)(rfc->host, PPPOE_EVT_DISCONNECTED, 0);

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

    //log(LOG_INFO, "handle_data, rfc = 0x%x\n", rfc);

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
    u_int8_t		from[ETHER_ADDR_LEN];

    //log(LOG_INFO, "pppoe_output\n");
    if (rfc->loopback) {
        // use the rfc addr to fake a unique ethernet address
        bzero(from, sizeof(from));
        *(u_int32_t *)&from[0] = (u_int32_t)rfc;
        pppoe_rfc_lower_input(rfc->dl_tag, m, from, typ);
        return;
    }

    pppoe_dlil_output(rfc->dl_tag, m, to, typ);
}

/* -----------------------------------------------------------------------------
called from pppoe_dlil when pppoe data are present
----------------------------------------------------------------------------- */
void pppoe_rfc_lower_input(u_long dl_tag, struct mbuf *m, u_int8_t *from, u_int16_t typ)
{
    struct pppoe_rfc  	*rfc;
#ifndef  queues
    int i;
#endif
    
    //log(LOG_INFO, "pppoe_rfc_inputdata, tag = %d\n", dl_tag);

#ifdef queues
    TAILQ_FOREACH(rfc, &pppoe_rfc_head, next) {
        // we use dl_tag because we only respond to the peer on the same interface
#else
    for (i = 0; i < MAX_PPPOE_RFC; i++) {
        rfc = pppoe_rfc[i];
        if (!rfc)
            continue;
#endif
        if (rfc->dl_tag == dl_tag) {

            if (pppoe_rfc_input(rfc, m, from, typ))
                return;
        }
    }
    log(LOG_INFO, "pppoe_rfc_inputdata end , tag = %d\n", dl_tag);
    // nobody was intersted in the packet, just ignore it
    m_freem(m);
}

