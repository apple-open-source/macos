//
//  tlsnke.c
//  tlsnke
//
//  Created by Fabrice Gautier on 11/11/11.
//  Copyright (c) 2011 Apple, Inc. All rights reserved.
//

#include <mach/mach_types.h>
#include <sys/kernel_types.h>
#include <sys/kpi_socket.h>
#include <sys/kpi_socketfilter.h>
#include <sys/kpi_mbuf.h>
#include <sys/malloc.h>
#include <sys/socketvar.h>
#include <sys/queue.h>
#include <sys/mbuf.h>
#include <sys/param.h>

#include <netinet/in.h>

#include <IOKit/IOLib.h>

#include <stdint.h>

/* For IOLog */
#include <IOKit/IOLib.h>
#include <stdarg.h>

#define DEBUG_ASSERT_COMPONENT_NAME_STRING "tlsnke"
#define DEBUG_ASSERT_PRODUCTION_CODE 0

#include <AssertMacros.h>


#include "tls_record.h"
#include "sslMemory.h"
#include "tlsnke.h"

/*
 Used a registered creator type here - to register for one - go to the
 Apple Developer Connection Datatype Registration page
 <http://developer.apple.com/datatype/>
 */
#define MYBUNDLEID		"com.apple.kext.tlsnke"


#define TLS_DEBUG 0
#define TLS_TEST  0  /* To enable the bsd devfs interface for testing the utun code path */
/* ==================================== */


typedef struct dtls_ctx *dtls_ctx_t; /* forward declaration, see below */


/* =================================== */

/* global dtls contexts table */

/* TODO: LOCK/UNLOCK global context table */

#define N_DTLS_MAX 1
static dtls_ctx_t g_dtls_contexts[N_DTLS_MAX];

static void clear_dtls_contexts(void)
{
    memset(g_dtls_contexts, 0, sizeof(g_dtls_contexts));
}

static dtls_ctx_t get_dtls_context(int dtls_handle)
{
    if(dtls_handle>=0 && dtls_handle<N_DTLS_MAX) {
        return g_dtls_contexts[dtls_handle];
    } else {
        return NULL;
    }
}

static int register_dtls_context(dtls_ctx_t dtls_ref)
{
    int i;
    for(i=0; i<N_DTLS_MAX; i++) {
        if(g_dtls_contexts[i]==NULL) {
            g_dtls_contexts[i]=dtls_ref;
            return i;
        }
    }
    return -1;
}

static int unregister_dtls_context(dtls_ctx_t dtls_ref)
{
    int i;
    for(i=0; i<N_DTLS_MAX; i++) {
        if(g_dtls_contexts[i]==dtls_ref) {
            g_dtls_contexts[i]=NULL;
            return i;
        }
    }
    return -1;
}

/* =================================== */
// MARK:  Utility Functions

/*
 * Messages to the system log
 */

static void
tls_printf(const char *fmt, ...)
{
#if TLS_DEBUG
	va_list listp;
	char log_buffer[252];
    
    log_buffer[250]='\n';
    log_buffer[251]=0;
	
    va_start(listp, fmt);
    
	vsnprintf(log_buffer, sizeof(log_buffer)-2, fmt, listp);
	//printf("%s", log_buffer);
    IOLog("%s", log_buffer);
	va_end(listp);
#endif
}

static void
tls_dump(const unsigned char *p, size_t len)
{
#if TLS_DEBUG
    size_t i;
    for(i=0; i<len; i++) {
        tls_printf("%02x ", p[i]);
        if((i&0x1F)==0x1F) {
            tls_printf("\n");
        }
    }
    tls_printf("\n");
#endif
}



/* =================================== */

#if TLS_TEST
/* the PktQueueItem record is used to store packet information for queued packets. */
struct PktQueueItem {
	STAILQ_ENTRY(PktQueueItem) next; /* link to next queued entry or NULL */
	mbuf_t                  data;
	sflt_data_flag_t		flags;
};
#endif

/* the PktQueueItem record is used to store packet information for queued packets. */
struct RecQueueItem {
	STAILQ_ENTRY(RecQueueItem) next; /* link to next queued entry or NULL */
	tls_buffer				record;
	sflt_data_flag_t		flags;
};

/* Internal DTLS context for socket filter */

struct dtls_ctx {
    socket_t socket;    /* socket to which we are attached - may not be needed */
    bool has_from; 
    struct sockaddr from; /* from address */
    bool has_to;
    struct sockaddr to;   /* to address */
    bool wait_for_key;
    tls_record_t ssl_ctx; /* ctx for actual SSL implementation */
    STAILQ_HEAD(, RecQueueItem) in_queue;
#ifdef TLS_TEST
    bool queue_to_tlsnkedev;      /* for testing only, if true, queue decrypted incoming data
                                   in the tlsnkedev queue instead of sending to utun or userland */
#endif
};


/* =================================== */

#if TLS_TEST
/* Q for incoming data */
static int tlsnkedev_queue(mbuf_t m);
#endif


/*===== DTLS IO Callbacks =====*/
// MARK:  DTLS IO Callbacks

static
int DTLSIOWriteFunc (dtls_ctx_t dtls_ref, const void *data, size_t dataLength)
{
    /* Write data callback: */
    mbuf_t out_mbuf=NULL;

    check(dtls_ref);
    
    //const unsigned char *p=data;
    //tls_printf("tlsnke(%p):%s - IOWrite len=%d\n", dtls_ref, __FUNCTION__, *dataLength);
    //tls_dump(p, *dataLength);

    /* create a new data mbuf */
    require_noerr(mbuf_allocpacket(M_NOWAIT, dataLength, NULL, &out_mbuf), fail);
    require(out_mbuf, fail);
    require_noerr(mbuf_copyback(out_mbuf, 0, dataLength, data, M_NOWAIT), fail);
    
    //tls_printf("tlsnke(%p):%s - inject to=%p mbuf flags=0x%x\n", dtls_ref, __FUNCTION__, dtls_ref->has_to?&dtls_ref->to:NULL, mbuf_flags(out_mbuf));

    /* out_mbuf is freed here in anycase */
    return sock_inject_data_out(dtls_ref->socket, dtls_ref->has_to?&dtls_ref->to:NULL, out_mbuf, NULL, 0);

fail:
    /* If we allocated an mbuf, and something failed, we have to free it */
    if(out_mbuf) {
        mbuf_freem(out_mbuf);
    }

    return ENOMEM;
}


/*===== RNG =====*/
//FIXME: There should be a static ccrng object defined in corecrypto for the kernel.
//The ccrng_system interface for the kernel is not optimal as it requires an init call

#include <corecrypto/ccrng.h>

void read_random(void* buffer, u_int numBytes);

static int tlsnke_random_generate(struct ccrng_state *rng, unsigned long outlen, void *out)
{
    read_random(out, (u_int)outlen);
    return 0;
}

struct ccrng_state tlsnke_random = {
    .generate = tlsnke_random_generate,
};

static
dtls_ctx_t dtls_create_context(socket_t so)
{
    dtls_ctx_t dtls_ref;
    
    dtls_ref = (dtls_ctx_t)IOMalloc(sizeof(struct dtls_ctx));
    require(dtls_ref, fail);
    memset(dtls_ref, 0, sizeof(struct dtls_ctx));

    dtls_ref->socket=so;
    STAILQ_INIT(&dtls_ref->in_queue);
    dtls_ref->ssl_ctx = tls_record_create(true, &tlsnke_random);
    require(dtls_ref->ssl_ctx, fail);

    return dtls_ref;
    
fail:
    if(dtls_ref) {
        IOFree(dtls_ref, sizeof(struct dtls_ctx));
    }
    return NULL;
}


static
void dtls_free_context(dtls_ctx_t dtls_ref)
{
    /* TODO: LOCK this dtls ref */

    /* TODO: Clear incoming mbuf queue */
    tls_record_destroy(dtls_ref->ssl_ctx);

    unregister_dtls_context(dtls_ref);

    IOFree(dtls_ref, sizeof(struct dtls_ctx));
    /* TODO: UNLOCK */
}

/* Get the tls_record_hdr from the control mbuf */
static 
tls_record_hdr_t dtls_get_header(mbuf_t *control)
{
    struct cmsghdr *cm;
    tls_record_hdr_t hdr;

    if(control==NULL) {
        return NULL;
    }
    
    if((*control)==NULL) {
        return NULL;
    }

    /* Needs to be in one mbuf */
    if (mbuf_next(*control)) {
        return NULL;
    }
    /* Control mbuf needs to be big enough for the tls_record_hdr struct - <rdar://problem/11204421> */
    if (mbuf_len(*control) < CMSG_LEN(sizeof(struct tls_record_hdr))) {
        return NULL;
    }
    
    cm = mbuf_data(*control);
    
    if(cm==NULL) {
        return NULL;
    }
    
    hdr=(tls_record_hdr_t)CMSG_DATA(cm);
    
    return hdr;
}


/* encrypt a dtls record */


static
int dtls_process_output_packet(dtls_ctx_t dtls_ref, mbuf_t data,  tls_record_hdr_t hdr)
{
    /* This should be thread safe as this may be called from both
     the userland socket or from utun -- maybe ? */

    tls_buffer in_data;
    tls_buffer out_data;
    size_t len = mbuf_pkthdr_len(data);
    size_t olen;
    int err = ENOMEM;

    in_data.data = NULL;
    out_data.data = NULL;

    /* If its not zero, lets set it the protocol version */
    if(hdr->protocol_version) {
        tls_record_set_protocol_version(dtls_ref->ssl_ctx, hdr->protocol_version);
    }

    olen = tls_record_encrypted_size(dtls_ref->ssl_ctx, hdr->content_type, len);

    in_data.data = IOMalloc(len);
    in_data.length = len;
    require(in_data.data, out);

    out_data.data = IOMalloc(olen);
    out_data.length = olen;
    require(out_data.data, out);

    /* ...then the data */
    err = mbuf_copydata(data, 0, len, in_data.data);
    require_noerr(err, out);

    tls_dump(in_data.data, in_data.length);

    err = tls_record_encrypt(dtls_ref->ssl_ctx, in_data, hdr->content_type, &out_data);
    require_noerr(err, out);

    err = DTLSIOWriteFunc(dtls_ref, out_data.data, out_data.length);

out:

    if(in_data.data) {
        IOFree(in_data.data, len);
    }
    if(out_data.data) {
        IOFree(out_data.data, olen);
    }

    return err;
}


static void dtls_process_one_record(dtls_ctx_t dtls_ref, tls_buffer record)
{
    tls_buffer decrypted;
    uint8_t contentType;

    struct cmsghdr *cmsg;
    tls_record_hdr_t hdr;
    size_t cbuf_len = CMSG_SPACE(sizeof(*hdr));
    uint8_t *cbuf; //cbuf_len;

    mbuf_t data;
    mbuf_t control;
    struct sockaddr *from;

    /* decrypt record */

    size_t len = tls_record_decrypted_size(dtls_ref->ssl_ctx, record.length);
    decrypted.length = len;
    decrypted.data = IOMalloc(len);
    require(decrypted.data, out);

    require_noerr(tls_record_decrypt(dtls_ref->ssl_ctx, record, &decrypted, &contentType), out);

    /* create a new data mbuf */
    require_noerr(mbuf_allocpacket(M_NOWAIT, decrypted.length, NULL, &data), out);
    require_noerr(mbuf_copyback(data, 0, decrypted.length, decrypted.data, M_NOWAIT), out);

    /* Create a new control mbuf to store the DTLS header */
    require_noerr(mbuf_get(M_NOWAIT, MBUF_TYPE_CONTROL, &control), out);
    require(mbuf_maxlen(control)>=cbuf_len, out);
    mbuf_setlen(control, cbuf_len);
    cbuf = mbuf_data(control);

    cmsg = (struct cmsghdr *)cbuf;
    cmsg->cmsg_len = CMSG_LEN(sizeof(*hdr));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_TLS_HEADER;

    hdr = (tls_record_hdr_t)CMSG_DATA(cmsg);
    hdr->content_type = contentType;
    hdr->protocol_version = (record.data[1]<<8)|record.data[2]; // Hack. PV should just be ignored by upper layer, but legacy ST does not.

    /* We pause processing when getting a change cipher spec */
    if(hdr->content_type==tls_record_type_ChangeCipher) {
        tls_printf("tlsnke(%p):%s got a ChangeCipher message\n", dtls_ref, __FUNCTION__);
        dtls_ref->wait_for_key=true;
    }

    if(dtls_ref->has_from) {
        from=&dtls_ref->from;
    }
    else {
        from=NULL;
    }

    tls_printf("tlsnke(%p):%s injecting packet d=%p c=%p h=%p, from=%p, flags=0x%x\n", dtls_ref, __FUNCTION__, data, control, hdr, from, mbuf_flags(data));

    errno_t err;
    err = sock_inject_data_in(dtls_ref->socket, from, data, control, 0);
    if(err) {
        tls_printf("tlsnke(%p):%s error while injecting packet err = %d\n", dtls_ref, __FUNCTION__, err);
        mbuf_free(data);
        mbuf_free(control);
    }

out:
    if(decrypted.data) {
        IOFree(decrypted.data, decrypted.length);
    }

}


static void dtls_process_incoming_queue(dtls_ctx_t dtls_ref)
{
    /* TODO: LOCK this dtls_ref */
    tls_printf("tlsnke(%p):%s \n", dtls_ref, __FUNCTION__);

    while(!STAILQ_EMPTY(&dtls_ref->in_queue) && !dtls_ref->wait_for_key) {
        struct RecQueueItem *in_q = STAILQ_FIRST(&dtls_ref->in_queue);
        STAILQ_REMOVE_HEAD(&dtls_ref->in_queue, next);
        dtls_process_one_record(dtls_ref, in_q->record);
        if(in_q->record.data) {
            IOFree(in_q->record.data, in_q->record.length);
        }
        IOFree(in_q, sizeof(*in_q));
    }
}


/* =================================== */
// MARK: Socket Filter Functions


static	void
tls_unregistered_fn(sflt_handle handle)
{
    tls_printf("tlsnke:%s\n", __FUNCTION__);
}

static	errno_t
tls_attach_fn(void **cookie, socket_t so)
{
    tls_printf("tlsnke:%s (so=%p)\n", __FUNCTION__, so);
    
    dtls_ctx_t dtls_ref;
    
    dtls_ref = dtls_create_context(so);
    
    *cookie=dtls_ref;
    
    if(dtls_ref) {
        return 0;
    }
    else {
        return -1;
    }
}

static void	
tls_detach_fn(void *cookie, socket_t so)
{
    dtls_ctx_t dtls_ref = (dtls_ctx_t)cookie;
    tls_printf("tlsnke(%p):%s\n", cookie, __FUNCTION__);

    dtls_free_context(dtls_ref);
}

static	void
tls_notify_fn(void *cookie, socket_t so, sflt_event_t event, void *param)
{		
    tls_printf("tlsnke(%p):%s - so: %p - evt: %d\n", cookie, __FUNCTION__, so, event);
}


static struct RecQueueItem *dtls_parse_record(dtls_ctx_t dtls_ref, mbuf_t data, size_t pkt_off, size_t pkt_len)
{
    size_t offset = pkt_off;
    mbuf_t location;
    struct RecQueueItem	*tlq = NULL;

    /* FIXME: not sure what really happens when this fails */
    require_noerr(mbuf_pulldown(data, &offset, DTLS_RECORD_HEADER_SIZE, &location), fail);

    require((tlq = (struct RecQueueItem *)IOMalloc(sizeof (struct RecQueueItem))), fail);
    memset(tlq, 0, sizeof (*tlq));

    tls_buffer header;
    header.data = mbuf_data(location)+offset;
    header.length = DTLS_RECORD_HEADER_SIZE;

    require_noerr(tls_record_parse_header(dtls_ref->ssl_ctx, header, &tlq->record.length, NULL), fail);
    tlq->record.length += DTLS_RECORD_HEADER_SIZE;

    tls_printf("tlsnke(%p): %s rec len=%d, pkt_len=%d\n", dtls_ref, __FUNCTION__, tlq->record.length, pkt_len);

    require(tlq->record.length<=pkt_len, fail);

    tlq->record.data = IOMalloc(tlq->record.length);
    require(tlq->record.data, fail);

    require_noerr(mbuf_copydata(location, offset, tlq->record.length, tlq->record.data), fail);

    return tlq;

fail:
    if(tlq) {
        if(tlq->record.data) {
            IOFree(tlq->record.data, tlq->record.length);
        }
        IOFree(tlq, sizeof (struct RecQueueItem));
    }

    return NULL;
}

static	errno_t
tls_data_in_fn(void *cookie, socket_t so, const struct sockaddr *from,
               mbuf_t *data, mbuf_t *control, sflt_data_flag_t flags)
{
    /* TODO: LOCK ? */
    dtls_ctx_t dtls_ref = (dtls_ctx_t)cookie;
    struct tls_record_hdr *hdr;

    tls_printf("tlsnke(%p):%s so=%p, data=%p/l=%d, control=%p/l=%d, from=%p, flags=0x%x\n",
               cookie, __FUNCTION__, so,
               data?*data:(void*)-1, (data && *data)?mbuf_pkthdr_len(*data):-1,
               control?*control:(void*)-1, (control && *control)?mbuf_pkthdr_len(*control):-1,
               from, flags);

    // If this packet already has a DTLS header, just drop it through */
    hdr = dtls_get_header(control);
    
    if(hdr) {
        tls_printf("tlsnke(%p):%s This was already processed. hdr=%p\n", cookie, __FUNCTION__, hdr);
        
#if TLS_TEST
        /* if switch enabled and this is a application data packet, send to tlsnkedev Q */
        if((dtls_ref->queue_to_tlsnkedev) && (hdr->content_type==tls_record_type_AppData)) {
            /* Q data into tlsnkedev Q */
            tls_printf("tlsnke(%p):%s Sending packet to tlsnkdev. data=%p\n", cookie, __FUNCTION__, *data);
            err = tlsnkedev_queue(*data);
            verify_noerr_action(err, return err);
            /* no error, lets free the control mbuf... */
            mbuf_freem(*control);
            /* ... and swallow */
            return EJUSTRETURN;
        }
#endif

        /* keep the packet moving up in the stack to userland */
        return 0;
    }
    tls_printf("tlsnke(%p):%s Queuing the data %p\n", cookie, __FUNCTION__, data);

    /* Parse and Queue the records */
    if(data) {

        size_t pkt_len = mbuf_pkthdr_len(*data);
        size_t pkt_off = 0;

        tls_printf("tlsnke(%p): %s pkt_len = %d\n", cookie, __FUNCTION__, pkt_len);

        while(pkt_len>=DTLS_RECORD_HEADER_SIZE) {
            struct RecQueueItem	*tlq = dtls_parse_record(dtls_ref, *data, pkt_off, pkt_len);

            /* If we fail, we return an error, so the socket layer will free that mbuf */
            verify_action(tlq, return ENOMEM);

            pkt_len -= tlq->record.length;
            pkt_off += tlq->record.length;

            tls_printf("tlsnke(%p): %s pkt_off=%d, pkt_len=%d\n", cookie, __FUNCTION__, pkt_off, pkt_len);

            STAILQ_INSERT_TAIL(&dtls_ref->in_queue, tlq, next);
        }
    }
    
    if(from) {
        dtls_ref->has_from=true;
        memcpy(&dtls_ref->from, from, sizeof(struct sockaddr));
    } else {
        dtls_ref->has_from=false;
    }

    dtls_process_incoming_queue(dtls_ref);

    return EJUSTRETURN;
}

static	errno_t	
tls_data_out_fn(void *cookie, socket_t so, const struct sockaddr *to, mbuf_t *data,
                mbuf_t *control, sflt_data_flag_t flags)
{
    /* TODO: LOCK ? */
    errno_t err;
    dtls_ctx_t dtls_ref = (dtls_ctx_t)cookie;
    tls_record_hdr_t hdr;
    
    tls_printf("tlsnke(%p):%s so=%p, data=%p/l=%d, control=%p/l=%d, to=%p, flags=0x%x\n", cookie, __FUNCTION__, so,
               data?*data:(void *)-1, (data && *data)?mbuf_pkthdr_len(*data):-1,
               control?*control:(void *)-1, (control && *control)?mbuf_pkthdr_len(*control):-1, to, flags);

    hdr = dtls_get_header(control);
    
    /* It may be possible for a data to be NULL. Never seen it, but this is not documented
       otherwise, so lets not rely on current behaviour */

    if(hdr && data) {
        if(to) {
            dtls_ref->has_to=true;
            memcpy(&dtls_ref->to, to, sizeof(struct sockaddr));
        } else {
            dtls_ref->has_to=false;
        }
            
        err = dtls_process_output_packet(dtls_ref, *data, hdr);
        verify_noerr_action(err, return err);

        /* We have to free the mbufs only if we have no error */
        mbuf_freem(*data);
        mbuf_freem(*control);

        return EJUSTRETURN;
    } else {
        /* No TLS header, just send. Dont process this */
        return 0;
    }
}

static	errno_t	
tls_connect_in_fn(void *cookie, socket_t so, const struct sockaddr *from)
{
    tls_printf("tlsnke(%p):%s\n", cookie, __FUNCTION__);
    return 0;
}

static	errno_t	
tls_connect_out_fn(void *cookie, socket_t so, const struct sockaddr *to)
{
    tls_printf("tlsnke(%p):%s\n", cookie, __FUNCTION__);
    return 0;
}

static	errno_t	
tls_bind_fn(void *cookie, socket_t so, const struct sockaddr *to)
{
    tls_printf("tlsnke(%p):%s\n", cookie, __FUNCTION__);
    return 0;
}

static	errno_t	
tls_setoption_fn(void *cookie, socket_t so, sockopt_t opt)
{
    /* TODO : LOCK ? */
    errno_t err;
    int rc;
    dtls_ctx_t dtls_ref = (dtls_ctx_t)cookie;

    int level = sockopt_level(opt);
    int name = sockopt_name(opt);
    size_t valsize = sockopt_valsize(opt);
    
    tls_printf("tlsnke(%p):%s - %x %x %d\n", cookie, __FUNCTION__, level, name, valsize);

    /* We only handle SOL_SOCKET level options */
    if(level!=SOL_SOCKET) {
        return 0;
    }
    
    switch (name) {
        case SO_TLS_INIT_CIPHER:
            {
                uint16_t selectedCipher;
                bool server;
                tls_buffer key;
                unsigned char *buf;
                
                verify_action(valsize>=4, return EINVAL);
                verify_action(valsize<=4096, return EINVAL);
            
                buf=IOMalloc(valsize);
                
                verify_action(buf, return ENOMEM);
                err=sockopt_copyin(opt, buf, valsize);
                verify_noerr_action(err, return err);
                    
                selectedCipher = (buf[0] << 8) | buf[1];
                server = buf[2];
                key.length = valsize - 3;
                key.data = buf + 3;

                tls_printf("tlsnke(%p):%s - Init Ciphers. cipherspec=%04x. server=%d.\n", dtls_ref, __FUNCTION__, selectedCipher, server);
                rc = tls_record_init_pending_ciphers(dtls_ref->ssl_ctx, selectedCipher, server, key);
                tls_printf("tlsnke(%p):%s - Init Ciphers done rc=%d\n", dtls_ref, __FUNCTION__, rc);
            }
            break;
        case SO_TLS_ADVANCE_READ_CIPHER:
            check(dtls_ref->wait_for_key);
            rc = tls_record_advance_read_cipher(dtls_ref->ssl_ctx);
            tls_printf("tlsnke(%p):%s - Read Cipher Advanced, process incoming queue now.\n", dtls_ref, __FUNCTION__);
            dtls_ref->wait_for_key=false;
            dtls_process_incoming_queue(dtls_ref);
            break;
        case SO_TLS_ADVANCE_WRITE_CIPHER:
            tls_printf("tlsnke(%p):%s - Advancing write cipher.\n", dtls_ref, __FUNCTION__);
            rc = tls_record_advance_write_cipher(dtls_ref->ssl_ctx);
            break;
        case SO_TLS_ROLLBACK_WRITE_CIPHER:
            tls_printf("tlsnke(%p):%s - Rolling back write cipher.\n", dtls_ref, __FUNCTION__);
            rc = tls_record_rollback_write_cipher(dtls_ref->ssl_ctx);
            break;
        case SO_TLS_PROTOCOL_VERSION:
            {
                tls_protocol_version pv;
                verify_action(valsize==sizeof(pv), return EINVAL);
                err=sockopt_copyin(opt, &pv, sizeof(pv));
                verify_noerr_action(err, return err);
                tls_printf("tlsnke(%p):%s - Setting protocol version %04x\n", dtls_ref, __FUNCTION__, pv);
                rc = tls_record_set_protocol_version(dtls_ref->ssl_ctx, pv);
            }
            break;
        case SO_TLS_SERVICE_WRITE_QUEUE:
            tls_printf("tlsnke(%p):%s - Servicing the write Queue.\n", dtls_ref, __FUNCTION__);
            rc = 0; //Nothing to do anymore //SSLRecordLayerInternal.serviceWriteQueue(dtls_ref->ssl_ctx);
            break;
        default:
            /* We just ignore other option. We dont normally get any */
            return 0;
    }
    
    /* Option was a valid filter specific option, return JUSTRETURN
       or the result from the call */
    if(rc==0) {
        return EJUSTRETURN;
    }
    else {
        return rc;
    }
}

static	errno_t
tls_getoption_fn(void *cookie, socket_t so, sockopt_t opt)
{
    errno_t err;
    dtls_ctx_t dtls_ref = (dtls_ctx_t)cookie;
    
    int level = sockopt_level(opt);
    int name = sockopt_name(opt);
    size_t valsize = sockopt_valsize(opt);
    
    int handle;
    
    if(level!=SOL_SOCKET) {
        return 0;
    }
    
    switch (name) {
        case SO_TLS_HANDLE:
            handle=register_dtls_context(dtls_ref);
            verify_action(handle>=0, return EBUSY); /* No space in the global table */
            verify_action(valsize==sizeof(handle), return EINVAL);
            err = sockopt_copyout(opt, &handle, valsize);
            verify_noerr_action(err, return err);
            tls_printf("tlsnke(%p):%s - get handle : %d\n", cookie, __FUNCTION__, handle);
            break;
        default:
            /* We just ignore other options. Another option we often get is SO_NREAD. */
            return 0;
    }
    

    return EJUSTRETURN;
}

static	errno_t	
tls_listen_fn(void *cookie, socket_t so)
{
    tls_printf("tlsnke(%p):%s\n", cookie, __FUNCTION__);

    return 0;
}


/* Dispatch vector for TCPLogger IPv4 socket functions */
static const struct sflt_filter tls_sflt_filter_ip4 = {
	TLS_HANDLE_IP4,         /* sflt_handle - use a registered creator type - <http://developer.apple.com/datatype/> */
	SFLT_PROG,			/* sf_flags */
	MYBUNDLEID,				/* sf_name - cannot be nil else param err results */
	tls_unregistered_fn,    /* sf_unregistered_func */
	tls_attach_fn,          /* sf_attach_func - cannot be nil else param err results */			
	tls_detach_fn,			/* sf_detach_func - cannot be nil else param err results */
	tls_notify_fn,			/* sf_notify_func */
	NULL,					/* sf_getpeername_func */
	NULL,					/* sf_getsockname_func */
	tls_data_in_fn,			/* sf_data_in_func */
	tls_data_out_fn,		/* sf_data_out_func */
	tls_connect_in_fn,		/* sf_connect_in_func */
	tls_connect_out_fn,		/* sf_connect_out_func */
	tls_bind_fn,			/* sf_bind_func */
	tls_setoption_fn,		/* sf_setoption_func */
	tls_getoption_fn,		/* sf_getoption_func */
	tls_listen_fn,			/* sf_listen_func */
	NULL					/* sf_ioctl_func */
};

#if TLS_TEST
/* BSD interface for testing only */
#include <sys/conf.h>
#include <miscfs/devfs/devfs.h>
#include <sys/uio.h>

/* Queue mbuf to tlsnkdev in Q */
static STAILQ_HEAD(, PktQueueItem) tlsnkedev_inq;
static lck_grp_attr_t *tlsnkedev_lock_grp_attr;
static lck_grp_t *tlsnkedev_lock_grp;
static lck_attr_t *tlsnkedev_lock_attr;
static lck_mtx_t *tlsnkedev_lock;


static int tlsnkedev_queue(mbuf_t m)
{
    struct PktQueueItem	*tlq= (struct PktQueueItem *)IOMalloc(sizeof (struct PktQueueItem));

    tls_printf("tlsnkedev:%s\n", __FUNCTION__);

    /* If we can't allocate, we just drop */
    verify_action(tlq, return ENOMEM);
    tlq->data = m;

    lck_mtx_lock(tlsnkedev_lock);
    STAILQ_INSERT_TAIL(&tlsnkedev_inq, tlq, next);
    wakeup(&tlsnkedev_inq);
    lck_mtx_unlock(tlsnkedev_lock);

    return 0;
}

static int tlsnkedev_open(__unused dev_t dev, int flags, __unused int devtype, __unused struct proc *p)
{
    dtls_ctx_t dtls_ref;

    tls_printf("tlsnkedev:%s\n", __FUNCTION__);

    dtls_ref = get_dtls_context(0);
    dtls_ref->queue_to_tlsnkedev=true;

    /* Reinitialize the Q */
    STAILQ_INIT(&tlsnkedev_inq);

    tls_printf("tlsnkedev:%s. inq first=%p last=%p\n", __FUNCTION__, tlsnkedev_inq.stqh_first, tlsnkedev_inq.stqh_last);

    return 0;
}

static int tlsnkedev_close(__unused dev_t dev, __unused int flags, __unused int mode, __unused struct proc *p)
{
    dtls_ctx_t dtls_ref;

    tls_printf("tlsnkedev:%s\n", __FUNCTION__);

    dtls_ref = get_dtls_context(0);
    dtls_ref->queue_to_tlsnkedev=false;

    return 0;
}

static int tlsnkedev_read(dev_t dev, struct uio *uio, int ioflag)
{
    int err=0;
    size_t mlen;
    uint32_t amnt;
    user_ssize_t ulen;
    char *buffer=NULL;
    mbuf_t m;
    struct PktQueueItem *in_q;

    tls_printf("tlsnkedev:%s\n", __FUNCTION__);

    ulen=uio_resid(uio);

    lck_mtx_lock(tlsnkedev_lock);
    if(STAILQ_EMPTY(&tlsnkedev_inq)) {
        msleep(&tlsnkedev_inq, tlsnkedev_lock, 0, __FUNCTION__, NULL);
    }
    in_q = STAILQ_FIRST(&tlsnkedev_inq);
    STAILQ_REMOVE_HEAD(&tlsnkedev_inq, next);
    lck_mtx_unlock(tlsnkedev_lock);

    tls_printf("tlsnkedev:%s. got %p from inq\n", __FUNCTION__, in_q);

    verify_action(in_q, return EAGAIN); // This should not happen since we are blocking.

    m = in_q->data;
    verify_action(m, return EFAULT); // we should never queue NULL mbufs

    mlen=mbuf_len(m);
    check(mlen); // we should never queue empty mbufs, but we can just return a read of 0.
    check(ulen); // user requested 0 bytes? sure, but this will drop the mbuf.

    amnt = MIN((uint32_t)mlen, (uint32_t)ulen);

    buffer=IOMalloc(amnt);
    verify_action(buffer, return ENOMEM); // We drop the mbuf in that case too.

    require_noerr(err=mbuf_copydata(m, 0, amnt, buffer), out);
    require_noerr(err=uiomove(buffer, amnt, uio), out);

out:
    IOFree(in_q, sizeof (struct PktQueueItem));
    mbuf_freem(m);
    IOFree(buffer, amnt);
    return 0;
}

static int tlsnkedev_write(dev_t dev, struct uio *uio, int ioflag)
{
    int err=0;
    int len;
    char *buffer=NULL;
    mbuf_t m=NULL;

    tls_printf("tlsnkedev:%s\n", __FUNCTION__);

    len=(int)uio_resid(uio);

    buffer=IOMalloc(len);
    verify_action(buffer, return ENOMEM);

    require_noerr(err=uiomove(buffer, len, uio), out);
    require_noerr(err=mbuf_allocpacket(M_NOWAIT, len, NULL, &m), out);
    require_noerr(err=mbuf_copyback(m, 0, len, buffer, M_NOWAIT), out);


    err=tls_utun_crypto_kpi_send_fn((caddr_t)get_dtls_context(0), &m);

out:
    IOFree(buffer, len);

    return err;
}

static struct cdevsw tlsnkedev_cdevsw =
{
    tlsnkedev_open,    /* open */
    tlsnkedev_close,   /* close */
    tlsnkedev_read,    /* read */
    tlsnkedev_write,   /* write */
    eno_ioctl,      /* ioctl */
    eno_stop,       /* stop */
    eno_reset,      /* reset */
    NULL,           /* tty's */
    eno_select,     /* select */
    eno_mmap,       /* mmap */
    eno_strat,      /* strategy */
    eno_getc,       /* getc */
    eno_putc,       /* putc */
    0               /* type */
};

static void tlsnkedev_devinit(void)
{
    int ret;

    ret = cdevsw_add(-1, &tlsnkedev_cdevsw);

    tls_printf("tlsnkedev:%s - major=%d\n", __FUNCTION__, ret);

    devfs_make_node(makedev(ret, 0), DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0666, "tlsnke");


    /* allocate lock group attribute and group */
    tlsnkedev_lock_grp_attr = lck_grp_attr_alloc_init();
    tlsnkedev_lock_grp =  lck_grp_alloc_init("tlsnkedev_lock", tlsnkedev_lock_grp_attr);
    /* Allocate lock attribute */
    tlsnkedev_lock_attr = lck_attr_alloc_init();
    tlsnkedev_lock = lck_mtx_alloc_init(tlsnkedev_lock_grp, tlsnkedev_lock_attr);


}
#endif /* TLS_TEST */

kern_return_t tlsnke_start(kmod_info_t * ki, void *d);
kern_return_t tlsnke_stop(kmod_info_t *ki, void *d);

kern_return_t tlsnke_start(kmod_info_t * ki, void *d)
{
    kern_return_t retval;
    
    tls_printf("tlsnke:%s\n", __FUNCTION__);
    
#if TLS_TEST
    //Init the BSD device
    tlsnkedev_devinit();
#endif

    // set up the tag value associated with this NKE in preparation for swallowing packets and re-injecting them
	gOSMallocTag = OSMalloc_Tagalloc(MYBUNDLEID, OSMT_DEFAULT); // don't want the flag set to OSMT_PAGEABLE since
                                                                // it would indicate that the memory was pageable.
	verify_action(gOSMallocTag, return KERN_MEMORY_ERROR);
    
    clear_dtls_contexts();

    retval = sflt_register(&tls_sflt_filter_ip4, PF_INET, SOCK_DGRAM, IPPROTO_UDP);

out:
    return retval;
}

kern_return_t tlsnke_stop(kmod_info_t *ki, void *d)
{
    kern_return_t retval;
    tls_printf("tlsnke:%s\n", __FUNCTION__);
    
    retval = sflt_unregister(TLS_HANDLE_IP4);

    return KERN_SUCCESS;
}
