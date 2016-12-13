//
//  tls_02_self.c
//  coretls
//

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <AssertMacros.h>

#include "secCrypto.h"

#include <tls_ciphersuites.h>
#include <Security/SecCertificate.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecIdentity.h>
#include <Security/oidsalg.h>

#include "tls_regressions.h"

#include <tls_handshake.h>
#include <tls_cache.h>
#include <tls_helpers.h>

#include "sslMemory.h"

#define DEBUG_ONLY __attribute__((unused))

struct _myFilterCtx;



typedef struct {
    bool dtls;
    int client_protocol_min; // 0 means use the default
    int client_protocol_max; // 0 means use the default
    int server_protocol_min; // 0 means use the default
    int server_protocol_max; // 0 means use the default
    const uint16_t *ciphersuites;
    int num_ciphersuites;
    const uint16_t *client_ciphersuites;
    int num_client_ciphersuites;
    const uint16_t *server_ciphersuites;
    int num_server_ciphersuites;
    bool allow_resumption;
    uintptr_t session_id;
    bool client_trigger_renegotiation;
    bool server_trigger_renegotiation;
    bool server_allow_renegotiation;
    bool request_client_auth;
    bool server_rsa_key_exchange;
    bool fallback;

    tls_buffer *dh_parameters;
    unsigned min_dh_size;

    SSLCertificate *server_certs;
    tls_private_key_t server_key;
    SSLCertificate *client_certs;
    tls_private_key_t client_key;

    tls_buffer *peer_hostname;
    tls_buffer *ocsp_response;
    tls_buffer_list_t *ocsp_responder_id_list;
    tls_buffer *ocsp_request_extensions;
    bool client_ocsp_enable;
    bool server_ocsp_enable;

    bool sct_enable;
    tls_buffer_list_t *sct_list;

    bool client_extMS_enable;
    bool server_extMS_enable;

    int (*fuzzer)(struct _myFilterCtx *myCtx, const tls_buffer in, uint8_t content_type);
    intptr_t fuzz_ctx;

    tls_handshake_trust_t (*server_trust)(intptr_t trust_ctx);
    intptr_t server_trust_ctx;
    tls_handshake_trust_t (*client_trust)(intptr_t trust_ctx);
    intptr_t client_trust_ctx;

    bool forget_to_set_pubkey;

    tls_handshake_config_t client_config;
    tls_handshake_config_t server_config;

    const uint16_t *server_ec_curves;
    int num_server_ec_curves;
    const uint16_t *client_ec_curves;
    int num_client_ec_curves;

    const tls_signature_and_hash_algorithm *server_sigalgs;
    int num_server_sigalgs;
    const tls_signature_and_hash_algorithm *client_sigalgs;
    int num_client_sigalgs;

    // expected outputs of test case
    bool handshake_ok;
    int client_err;
    int server_err;
    bool is_session_resumed;
    bool is_session_resumption_proposed;
    int certificate_requested;
    uint16_t negotiated_ciphersuite;
    tls_protocol_version negotiated_version;
    uint16_t negotiated_ec_curve;
    bool received_cert;
} tls_test_case;

typedef struct _myFilterCtx {
    tls_test_case *test;
    bool server;
    tls_handshake_t filter;
    tls_cache_t cache;
    dispatch_group_t group;
    dispatch_queue_t queue;
    dispatch_source_t timer;
    struct _myFilterCtx *peer;
    int epoch;
    int peer_epoch;
    int n; // num of records written
    int read_ready;
    int write_ready;
    bool peer_ocsp_enabled;
    bool bad_client_hello_size; // true if we sent a client hello with a bad size.
    tls_buffer session_data; // resumed session data
    int err;
} myFilterCtx_t;

static const char *side(myFilterCtx_t *myCtx)
{
    return myCtx->server?"server":"client";
}

static int send_to_peer(myFilterCtx_t *myCtx, const tls_buffer data, uint8_t content_type)
{
    myFilterCtx_t *peerCtx = myCtx->peer;
    int record_epoch = myCtx->epoch;

    if(peerCtx==NULL) {
        test_printf("%s: [%s] peer has been disconnected\n", __FUNCTION__, side(myCtx));
        return -1; //errSSLRecordClosedAbort; // TODO better errors
    }

    /* Check if this is a client hello, and if the size is ok */
    if((content_type==tls_record_type_Handshake) &&
       (data.data[0]==tls_handshake_message_client_hello) &&
       (data.length>=256) && (data.length<512))
    {
        myCtx->bad_client_hello_size = true;
    }

    /* The peer queue can disappear, like a peer socket can close */
    if(peerCtx->queue) {

        /* Copy the data */
        tls_buffer localData;
        localData.data=malloc(data.length);
        localData.length=data.length;
        memcpy(localData.data, data.data, data.length);

        dispatch_group_async(peerCtx->group, peerCtx->queue, ^{
            /* If we dont simulate the epoch here, we can get in a situation where the handshake fail
             if we lose the ChangeCipherSpec message */
            if(peerCtx->err) {
                test_printf("%s: [%s] receiving message after errors, ignoring\n", __FUNCTION__, side(peerCtx));
            } else if(record_epoch!=peerCtx->peer_epoch) {
                test_printf("%s: [%s] wrong epoch: %d expected :%d, do not process\n", __FUNCTION__, side(peerCtx), record_epoch, peerCtx->peer_epoch);
            } else {
                int err;
                test_printf("%s: [%s] Processing (len=%zd, type=%d, p=%p,  data[0]=%d)\n", __FUNCTION__,
                            side(peerCtx), localData.length, content_type, localData.data, localData.data[0]);
                err = tls_handshake_process(peerCtx->filter, localData, content_type);
                test_printf("%s: [%s] Done Processing (len=%zd, type=%d, p=%p,  data[0]=%d) -- err=%d\n", __FUNCTION__,
                            side(peerCtx), localData.length, content_type, localData.data, localData.data[0], err);

                if(err && err!=-9849) { /* FIXME: hardcoded error - errSSLUnexpectedRecord */
                    test_printf("Error %d while processing data in %s, closing.\n", err, side(peerCtx));
                    myCtx->peer=NULL;
                    peerCtx->err = err;
                }
            }
            /* Free the data after it has been processed */
            free(localData.data);
        });
        return 0;
    } else {
        fprintf(stderr, "Peer ctx queue (%s) is NULL, returning error.\n", side(peerCtx));
        return -2;
    }
}

static int tls_handshake_write_callback(tls_handshake_ctx_t ctx, const tls_buffer data, uint8_t content_type)
{
    myFilterCtx_t *myCtx = (myFilterCtx_t *)ctx;

    myCtx->n++;

    test_printf("%s: [%s] (epoch=%d, len=%zd, type=%d, p=%p,  data[0]=%d)\n", __FUNCTION__, side(myCtx), myCtx->epoch, data.length, content_type, data.data, data.data[0]);

    if(myCtx->test->fuzzer) {
        return myCtx->test->fuzzer(myCtx, data, content_type);
    } else {
        return send_to_peer(myCtx, data, content_type);
    }
}

static int
tls_handshake_message_callback(tls_handshake_ctx_t ctx, tls_handshake_message_t event)
{
    myFilterCtx_t *myCtx = (myFilterCtx_t *)ctx;
    int err = 0;
    tls_handshake_trust_t trust;

    test_printf("%s: [%s] event = %d\n", __FUNCTION__, side(myCtx), event);

    switch(event) {
        case tls_handshake_message_client_hello_request:
            break;
        case tls_handshake_message_client_hello:
        case tls_handshake_message_server_hello:
            myCtx->peer_ocsp_enabled=tls_handshake_get_peer_ocsp_enabled(myCtx->filter);
            break;
        case tls_handshake_message_certificate_request:
            /* Client cert requested */
            /* We should test for other types of client auth */
            require_noerr((err=tls_handshake_set_client_auth_type(myCtx->filter, tls_client_auth_type_RSASign)), errOut);
            require_noerr((err=tls_handshake_set_identity(myCtx->filter, myCtx->test->client_certs, myCtx->test->client_key)), errOut);
            break;
        case tls_handshake_message_certificate:
            if(!myCtx->test->forget_to_set_pubkey) {
                require_noerr((err = tls_helper_set_peer_pubkey(myCtx->filter)), errOut);
            }
            break;
        case tls_handshake_message_server_hello_done:
            if(myCtx->test->server_trust) {
                trust = myCtx->test->server_trust(myCtx->test->server_trust_ctx);
            } else {
                trust = tls_handshake_trust_ok;
            }
            require_noerr((err = tls_handshake_set_peer_trust(myCtx->filter, trust)), errOut);
            break;
        case tls_handshake_message_finished:
            /* For client side cert */
            if(myCtx->server) {
                if(myCtx->test->client_trust) {
                    trust = myCtx->test->client_trust(myCtx->test->client_trust_ctx);
                } else {
                    trust = tls_handshake_trust_ok;
                }
                require_noerr((err = tls_handshake_set_peer_trust(myCtx->filter, trust)), errOut);
            }
        default:
            test_printf("%s: [%s] Not handling this event: %d\n", __FUNCTION__, side(myCtx), event);
            break;
    }

errOut:
    return err;
}

static void
tls_handshake_ready_callback(tls_handshake_ctx_t ctx, bool write, bool ready)
{
    myFilterCtx_t *myCtx = (myFilterCtx_t *)ctx;

    test_printf("%s: [%s] %s, ready=%s\n", __FUNCTION__, side(myCtx), write?"write":"read", ready?"true":"false");

    if(ready) {
        if(write) {
            myCtx->write_ready++;
        } else {
            myCtx->read_ready++;
        }

        if(myCtx->write_ready == myCtx->read_ready) {
            test_printf("%s: [%s] handshake %d all done\n", __FUNCTION__, side(myCtx), myCtx->read_ready);
            if(myCtx->write_ready==1) {
                if(myCtx->server && myCtx->test->server_trigger_renegotiation) {
                    test_printf("%s: [%s] first handshake %d all done, request renegotiation\n", __FUNCTION__, side(myCtx), myCtx->read_ready);

                    dispatch_group_async(myCtx->group, myCtx->queue, ^{
                        tls_handshake_request_renegotiation(myCtx->filter);
                    });
                }
                if(!myCtx->server && myCtx->test->client_trigger_renegotiation) {
                    test_printf("%s: [%s] first handshake %d all done, starting renegotiation\n", __FUNCTION__, side(myCtx), myCtx->read_ready);
                    dispatch_group_async(myCtx->group, myCtx->queue, ^{
                        tls_handshake_negotiate(myCtx->filter, NULL);
                    });
                }
            }
        }
    }
}

#warning There is probably a race condition here.
static int
tls_handshake_set_retransmit_timer_callback(tls_handshake_ctx_t ctx, int attempt)
{
    myFilterCtx_t *myCtx = (myFilterCtx_t *)ctx;

    test_printf("%s: [%s] set_retransmit_time attempt=%d\n", __FUNCTION__, side(myCtx), attempt);

    if(attempt==0) {
        if(myCtx->timer) {
            test_printf("%s: [%s] cancelling timer [%p]\n", __FUNCTION__, side(myCtx), myCtx->timer);
            dispatch_source_cancel(myCtx->timer);
            dispatch_release(myCtx->timer);
            myCtx->timer=NULL;
        }
    } else {
        assert(myCtx->timer==NULL);
        require((myCtx->timer=dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,
                                                     0, 0, myCtx->queue)), fail);
        test_printf("%s: [%s] Created timer [%p] attempt %d\n", __FUNCTION__, side(myCtx), myCtx->timer, attempt);

        dispatch_source_set_event_handler(myCtx->timer, ^{
            test_printf("%s: [%s], timer [%p] fired attempt %d\n", __FUNCTION__, side(myCtx), myCtx->timer, attempt);
            dispatch_source_cancel(myCtx->timer);
            dispatch_release(myCtx->timer);
            myCtx->timer=NULL;
            tls_handshake_retransmit_timer_expired(myCtx->filter);
        });

        dispatch_source_set_timer(myCtx->timer, dispatch_time(DISPATCH_TIME_NOW, attempt*NSEC_PER_SEC), DISPATCH_TIME_FOREVER, NSEC_PER_SEC);
        dispatch_resume(myCtx->timer);
    }

    return 0;
fail:
    return errSecAllocate;

}

static
int mySSLRecordInitPendingCiphersFunc(tls_handshake_ctx_t ref,
                                      uint16_t            selectedCipher,
                                      bool                server,
                                      tls_buffer           key)
{
    myFilterCtx_t *myCtx = (myFilterCtx_t *)ref;

    test_printf("%s: [%s], cipher=%04x, server=%d\n", __FUNCTION__, side(myCtx), selectedCipher, server);
    return 0;
}

static
int mySSLRecordAdvanceWriteCipherFunc(tls_handshake_ctx_t ref)
{
    myFilterCtx_t *myCtx = (myFilterCtx_t *)ref;

    test_printf("%s: [%s]\n", __FUNCTION__, side(myCtx));

    myCtx->epoch++;

    return 0;
}

static
int mySSLRecordRollbackWriteCipherFunc(tls_handshake_ctx_t ref)
{
    myFilterCtx_t *myCtx = (myFilterCtx_t *)ref;

    test_printf("%s: [%s]\n", __FUNCTION__, side(myCtx));

    myCtx->epoch--;

    return 0;
}

static
int mySSLRecordAdvanceReadCipherFunc(tls_handshake_ctx_t ref)
{
    myFilterCtx_t *myCtx = (myFilterCtx_t *)ref;
    test_printf("%s: [%s]\n", __FUNCTION__, side(myCtx));

    myCtx->peer_epoch++;

    return 0;
}

static
int mySSLRecordSetProtocolVersionFunc(tls_handshake_ctx_t ref,
                                      tls_protocol_version  protocolVersion)
{
    myFilterCtx_t *myCtx = (myFilterCtx_t *)ref;
    test_printf("%s: [%s] pv=%04x\n", __FUNCTION__, side(myCtx), protocolVersion);
    return 0;
}

static int
tls_handshake_save_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey, tls_buffer sessionData)
{
    myFilterCtx_t DEBUG_ONLY *myCtx = (myFilterCtx_t *)ctx;

    test_printf("%s:[%s]\n", __FUNCTION__, side(myCtx));

    test_printf("%s:[%s] key=[%p,%zd] data=[%p,%zd]\n",__FUNCTION__, side(myCtx), sessionKey.data, sessionKey.length, sessionData.data, sessionData.length);
    return tls_cache_save_session_data(myCtx->cache, &sessionKey, &sessionData, 0);
}

static int
tls_handshake_load_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey, tls_buffer *sessionData)
{
    myFilterCtx_t DEBUG_ONLY *myCtx = (myFilterCtx_t *)ctx;

    test_printf("%s:[%s]\n", __FUNCTION__, side(myCtx));


    int err = tls_cache_load_session_data(myCtx->cache, &sessionKey, sessionData);

    test_printf("%s:[%s] key=[%p,%zd] data=[%p,%zd], err=%d\n", __FUNCTION__, side(myCtx), sessionKey.data, sessionKey.length, sessionData->data, sessionData->length, err);

    // This may look weird: tls_cache_load_session_data() create a copy of the session data buffer, but coreTLS will not free it.
    // We can't free here, because coreTLS will need it after we return, so we keep a reference to it, and free it when we destry the myFilterCtx_t.
    // Secure Transport does the same thing. This issue is tracked by rdar://problem/16277298.
    SSLFreeBuffer(&myCtx->session_data);
    myCtx->session_data = *sessionData;

    return err;
}

static int
tls_handshake_delete_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey)
{
    myFilterCtx_t DEBUG_ONLY *myCtx = (myFilterCtx_t *)ctx;

    test_printf("%s:[%s]\n", __FUNCTION__, side(myCtx));

    return tls_cache_delete_session_data(myCtx->cache, &sessionKey);
}

static int
tls_handshake_delete_all_sessions_callback(tls_handshake_ctx_t ctx)
{
    myFilterCtx_t DEBUG_ONLY *myCtx = (myFilterCtx_t *)ctx;

    test_printf("%s:[%s]\n", __FUNCTION__, side(myCtx));

    return -1;
}

static
tls_handshake_callbacks_t tls_handshake_callbacks = {
    .write = tls_handshake_write_callback,
    .message = tls_handshake_message_callback,
    .ready = tls_handshake_ready_callback,
    .set_retransmit_timer = tls_handshake_set_retransmit_timer_callback,
    .init_pending_cipher = mySSLRecordInitPendingCiphersFunc,
    .advance_write_cipher = mySSLRecordAdvanceWriteCipherFunc,
    .rollback_write_cipher = mySSLRecordRollbackWriteCipherFunc,
    .advance_read_cipher = mySSLRecordAdvanceReadCipherFunc,
    .set_protocol_version = mySSLRecordSetProtocolVersionFunc,
    .load_session_data = tls_handshake_load_session_data_callback,
    .save_session_data = tls_handshake_save_session_data_callback,
    .delete_session_data = tls_handshake_delete_session_data_callback,
    .delete_all_sessions = tls_handshake_delete_all_sessions_callback,
};

typedef struct _CipherSuiteName {
    uint16_t cipher;
    const char *name;
} CipherSuiteName;

#define CIPHER(cipher) { cipher, #cipher},

/* SSL ciphers: support SSLv3, support client auth */
static const CipherSuiteName ssl_ciphers[] = {

    CIPHER(SSL_RSA_WITH_NULL_SHA)
    CIPHER(SSL_RSA_WITH_NULL_MD5)
    CIPHER(TLS_RSA_WITH_NULL_SHA256)

    CIPHER(SSL_RSA_WITH_RC4_128_MD5)
    CIPHER(SSL_RSA_WITH_RC4_128_SHA)
    CIPHER(SSL_RSA_WITH_3DES_EDE_CBC_SHA)

    CIPHER(SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA)
    CIPHER(TLS_DHE_RSA_WITH_AES_128_CBC_SHA)
    CIPHER(TLS_DHE_RSA_WITH_AES_256_CBC_SHA)

    CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA)
    CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA)
};
static int n_ssl_ciphers = sizeof(ssl_ciphers)/sizeof(ssl_ciphers[0]);

/* DH anon ciphers: support SSLv3, don't support Client Auth */
static const CipherSuiteName anon_ciphers[] = {
    CIPHER(SSL_DH_anon_WITH_RC4_128_MD5)
    CIPHER(SSL_DH_anon_WITH_3DES_EDE_CBC_SHA)
    CIPHER(TLS_DH_anon_WITH_AES_128_CBC_SHA)
    CIPHER(TLS_DH_anon_WITH_AES_256_CBC_SHA)
};
static int n_anon_ciphers = sizeof(anon_ciphers)/sizeof(anon_ciphers[0]);

/* PSK Ciphers: TLS only, no Client Auth */
static const CipherSuiteName psk_ciphers[] = {
    CIPHER(TLS_PSK_WITH_AES_256_CBC_SHA384)
    CIPHER(TLS_PSK_WITH_AES_128_CBC_SHA256)
    CIPHER(TLS_PSK_WITH_AES_256_CBC_SHA)
    CIPHER(TLS_PSK_WITH_AES_128_CBC_SHA)
    CIPHER(TLS_PSK_WITH_RC4_128_SHA)
    CIPHER(TLS_PSK_WITH_3DES_EDE_CBC_SHA)
    CIPHER(TLS_PSK_WITH_NULL_SHA384)
    CIPHER(TLS_PSK_WITH_NULL_SHA256)
    CIPHER(TLS_PSK_WITH_NULL_SHA)
};
static int n_psk_ciphers = sizeof(psk_ciphers)/sizeof(psk_ciphers[0]);

/* GCM Ciphers: only supported in TLS 1.2, do support client auth */
static const CipherSuiteName gcm_ciphers[] = {
    CIPHER(TLS_RSA_WITH_AES_256_GCM_SHA384)
    CIPHER(TLS_RSA_WITH_AES_128_GCM_SHA256)
    CIPHER(TLS_DHE_RSA_WITH_AES_256_GCM_SHA384)
    CIPHER(TLS_DHE_RSA_WITH_AES_128_GCM_SHA256)
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384)
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256)
    CIPHER(TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384)
    CIPHER(TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256)
};
static int n_gcm_ciphers = sizeof(gcm_ciphers)/sizeof(gcm_ciphers[0]);

/* ECDHE ciphers: only supported in TLS, do support client auth */
static const CipherSuiteName ecdhe_ciphers[] = {
    CIPHER(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384)
    CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256)
    CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA)
    CIPHER(TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA)
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA)
    CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA)
};
static int n_ecdhe_ciphers = sizeof(ecdhe_ciphers)/sizeof(ecdhe_ciphers[0]);

/* ECDH anon ciphers: TLS only, don't support Client Auth */
static const CipherSuiteName ecanon_ciphers[] = {
    CIPHER(TLS_ECDH_anon_WITH_NULL_SHA)
    CIPHER(TLS_ECDH_anon_WITH_RC4_128_SHA)
    CIPHER(TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA)
    CIPHER(TLS_ECDH_anon_WITH_AES_128_CBC_SHA)
    CIPHER(TLS_ECDH_anon_WITH_AES_256_CBC_SHA)
};
static int n_ecanon_ciphers = sizeof(ecanon_ciphers)/sizeof(ecanon_ciphers[0]);


static int protos[]={
    tls_protocol_version_SSL_3,
    tls_protocol_version_TLS_1_0,
    tls_protocol_version_TLS_1_1,
    tls_protocol_version_TLS_1_2,
    0, // use default.
};
static int nprotos = sizeof(protos)/sizeof(protos[0]);

static uint8_t shared_secret[] = "secret";

static tls_cache_t g_cache; // Global TLS session cache

static
int init_context(myFilterCtx_t *c, bool server, const char *name, tls_test_case *test, dispatch_group_t group)
{
    c->test=test;
    c->server=server;
    c->n=0;
    c->epoch=0;
    c->peer_epoch=0;
    c->group=group;
    c->timer=NULL;
    require((c->queue=dispatch_queue_create(name, DISPATCH_QUEUE_SERIAL)), fail);

    require((c->filter=tls_handshake_create(false, server)), fail);
    require((c->cache=g_cache), fail);

    require_noerr(tls_handshake_set_user_agent(c->filter, "tls_test"), fail);

    if(server) {
        if(test->server_config)
            require_noerr(tls_handshake_set_config(c->filter, test->server_config), fail);
        require_noerr(tls_handshake_set_client_auth(c->filter, test->request_client_auth), fail);
        require_noerr(tls_handshake_set_identity(c->filter, test->server_certs, test->server_key), fail);
        require_noerr(tls_handshake_set_renegotiation(c->filter, test->server_allow_renegotiation), fail);
        require_noerr(tls_handshake_set_ocsp_enable(c->filter, test->server_ocsp_enable), fail);
        if(test->ocsp_response)
            require_noerr(tls_handshake_set_ocsp_response(c->filter, test->ocsp_response), fail);
        require_noerr(tls_handshake_set_sct_list(c->filter, test->sct_list), fail);
        if(test->server_rsa_key_exchange)
            require_noerr(tls_set_encrypt_pubkey(c->filter, test->server_certs), fail);
        if(test->dh_parameters)
            require_noerr(tls_handshake_set_dh_parameters(c->filter, test->dh_parameters), fail);
        require_noerr(tls_handshake_set_ems_enable(c->filter, test->server_extMS_enable), fail);
    } else {
        if(test->client_config)
            require_noerr(tls_handshake_set_config(c->filter, test->client_config), fail);
        require_noerr(tls_handshake_set_ocsp_enable(c->filter, test->client_ocsp_enable), fail);
        if(test->ocsp_request_extensions)
            require_noerr(tls_handshake_set_ocsp_request_extensions(c->filter, *test->ocsp_request_extensions), fail);
        if(test->ocsp_responder_id_list)
            require_noerr(tls_handshake_set_ocsp_responder_id_list(c->filter, test->ocsp_responder_id_list), fail);
        if(test->peer_hostname) {
            require_noerr(tls_handshake_set_peer_hostname(c->filter, (char *)test->peer_hostname->data, test->peer_hostname->length), fail);
        } else {
            require_noerr(tls_handshake_set_peer_hostname(c->filter, "localhost", strlen("localhost")), fail);
        }
        require_noerr(tls_handshake_set_sct_enable(c->filter, test->sct_enable), fail);
        require_noerr(tls_handshake_set_fallback(c->filter, test->fallback), fail);
        if(test->min_dh_size)
            require_noerr(tls_handshake_set_min_dh_group_size(c->filter, test->min_dh_size), fail);
        require_noerr(tls_handshake_set_ems_enable(c->filter, test->client_extMS_enable), fail);
    }

    require_noerr(tls_handshake_set_callbacks(c->filter,
                                              &tls_handshake_callbacks,
                                              c), fail);

    if(test->ciphersuites) {
        require_noerr(tls_handshake_set_ciphersuites(c->filter, test->ciphersuites, test->num_ciphersuites), fail);
    } else {
        if(server && test->server_ciphersuites) {
            require_noerr(tls_handshake_set_ciphersuites(c->filter, test->server_ciphersuites, test->num_server_ciphersuites), fail);
        }
        if(!server && test->client_ciphersuites) {
            require_noerr(tls_handshake_set_ciphersuites(c->filter, test->client_ciphersuites, test->num_client_ciphersuites), fail);
        }
    }

    tls_buffer psk_secret = {
        .data = shared_secret,
        .length = sizeof(shared_secret),
    };

    require_noerr(tls_handshake_set_psk_secret(c->filter, &psk_secret), fail);

    /* Note: We set the min version first, then the max. 
       This mean that in the event of min>max, coretls will reset the min to max */
    if(server) {
        if(test->server_protocol_min)
            require_noerr(tls_handshake_set_min_protocol_version(c->filter, test->server_protocol_min), fail);
        if(test->server_protocol_max)
            require_noerr(tls_handshake_set_max_protocol_version(c->filter, test->server_protocol_max), fail);
    } else {
        if(test->client_protocol_min)
            require_noerr(tls_handshake_set_min_protocol_version(c->filter, test->client_protocol_min), fail);
        if(test->client_protocol_max)
            require_noerr(tls_handshake_set_max_protocol_version(c->filter, test->client_protocol_max), fail);
    }

    require_noerr(tls_handshake_set_resumption(c->filter, test->allow_resumption), fail);

    if (server && test->num_server_ec_curves) {
        require_noerr(tls_handshake_set_curves(c->filter, test->server_ec_curves, test->num_server_ec_curves), fail);
    }
    if (!server && test->num_client_ec_curves) {
        require_noerr(tls_handshake_set_curves(c->filter, test->client_ec_curves, test->num_client_ec_curves), fail);
    }

    if (server && test->num_server_sigalgs) {
        require_noerr(tls_handshake_set_sigalgs(c->filter, test->server_sigalgs, test->num_server_sigalgs), fail);
    }
    if (!server && test->num_client_sigalgs) {
        require_noerr(tls_handshake_set_sigalgs(c->filter, test->client_sigalgs, test->num_client_sigalgs), fail);
    }

    return 0;

fail:
    return -1;
}

static
void clean_context(myFilterCtx_t *c)
{
    test_printf("%s: [%s] timer [%p]\n", __FUNCTION__, side(c), c->timer);

    if(c->queue) {
        dispatch_sync(c->queue, ^{
            test_printf("%s: [%s] releasing timer [%p]\n", __FUNCTION__, side(c), c->timer);
            if(c->filter) tls_handshake_destroy(c->filter); c->filter = NULL;
            if(c->timer) dispatch_release(c->timer); c->timer = NULL;
            SSLFreeBuffer(&c->session_data);
        });

        if(c->queue) dispatch_release(c->queue); c->queue = NULL;
    }
}

static bool tls_buffer_equal(const tls_buffer *a, const tls_buffer *b)
{
    if(a->length!=b->length) return false;

    if(memcmp(a->data, b->data, a->length)) return false;

    return true;
}

static bool tls_buffer_list_equal(const tls_buffer_list_t *a, const tls_buffer_list_t *b)
{

    while(a && b) {
        if(!tls_buffer_equal(&a->buffer, &b->buffer)) return false;

        a=a->next;
        b=b->next;
    }

    /* Is there still items in one of the list ? */
    if(a || b) return false;

    return true;
}

const uint16_t anon_ciphersuites[] = {
    TLS_ECDH_anon_WITH_AES_256_CBC_SHA,
    TLS_ECDH_anon_WITH_AES_128_CBC_SHA,
    TLS_DH_anon_WITH_AES_256_CBC_SHA256,
    TLS_DH_anon_WITH_AES_256_CBC_SHA,
    TLS_DH_anon_WITH_AES_128_CBC_SHA256,
    TLS_DH_anon_WITH_AES_128_CBC_SHA,
};

const uint16_t legacy_ciphersuites[] = {
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_RSA_WITH_AES_256_GCM_SHA384,
    TLS_RSA_WITH_AES_128_GCM_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA256,
    TLS_RSA_WITH_AES_128_CBC_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA,
    TLS_RSA_WITH_AES_128_CBC_SHA,
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
    TLS_ECDHE_RSA_WITH_RC4_128_SHA,
    SSL_RSA_WITH_RC4_128_SHA,
    SSL_RSA_WITH_RC4_128_MD5,
};

const uint16_t DHE_ciphersuites[] = {
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_RSA_WITH_AES_256_GCM_SHA384,
    TLS_RSA_WITH_AES_128_GCM_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA256,
    TLS_RSA_WITH_AES_128_CBC_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA,
    TLS_RSA_WITH_AES_128_CBC_SHA,
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
    TLS_ECDHE_RSA_WITH_RC4_128_SHA,
    SSL_RSA_WITH_RC4_128_SHA,
    SSL_RSA_WITH_RC4_128_MD5,
};



const uint16_t standard_ciphersuites[] = {
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_RSA_WITH_AES_256_GCM_SHA384,
    TLS_RSA_WITH_AES_128_GCM_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA256,
    TLS_RSA_WITH_AES_128_CBC_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA,
    TLS_RSA_WITH_AES_128_CBC_SHA,
};

const uint16_t default_ciphersuites[] = {
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_RSA_WITH_AES_256_GCM_SHA384,
    TLS_RSA_WITH_AES_128_GCM_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA256,
    TLS_RSA_WITH_AES_128_CBC_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA,
    TLS_RSA_WITH_AES_128_CBC_SHA,
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,
};

const uint16_t ATSv1_ciphersuites[] = {
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
};

const uint16_t ATSv1_noPFS_ciphersuites[] = {
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,

    TLS_RSA_WITH_AES_256_GCM_SHA384,
    TLS_RSA_WITH_AES_128_GCM_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA256,
    TLS_RSA_WITH_AES_128_CBC_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA,
    TLS_RSA_WITH_AES_128_CBC_SHA,
};


static bool test_ciphersuites(tls_test_case *test, bool server, const uint16_t *ciphersuites, unsigned int n)
{
    unsigned expected_n = 0;
    const uint16_t *expected_ciphersuites = NULL;

    if(test->ciphersuites) {
        expected_n = test->num_ciphersuites;
        expected_ciphersuites = test->ciphersuites;
    } else if(!server && test->client_ciphersuites) {
            expected_n = test->num_client_ciphersuites;
            expected_ciphersuites = test->client_ciphersuites;
    } else if(server && test->server_ciphersuites) {
            expected_n = test->num_server_ciphersuites;
            expected_ciphersuites = test->server_ciphersuites;
    } else {
        switch(server?test->server_config:test->client_config) {
            case tls_handshake_config_ATSv1:
                expected_n = sizeof(ATSv1_ciphersuites)/sizeof(uint16_t);
                expected_ciphersuites = ATSv1_ciphersuites;
                break;
            case tls_handshake_config_ATSv1_noPFS:
                expected_n = sizeof(ATSv1_noPFS_ciphersuites)/sizeof(uint16_t);
                expected_ciphersuites = ATSv1_noPFS_ciphersuites;
                break;
            case tls_handshake_config_standard:
                expected_n = sizeof(standard_ciphersuites)/sizeof(uint16_t);
                expected_ciphersuites = standard_ciphersuites;
                break;
            case tls_handshake_config_legacy_DHE:
                expected_n = sizeof(DHE_ciphersuites)/sizeof(uint16_t);
                expected_ciphersuites = DHE_ciphersuites;
                break;
            case tls_handshake_config_RC4_fallback:
            case tls_handshake_config_TLSv1_RC4_fallback:
            case tls_handshake_config_legacy:
                expected_n = sizeof(legacy_ciphersuites)/sizeof(uint16_t);
                expected_ciphersuites = legacy_ciphersuites;
                break;
            case tls_handshake_config_3DES_fallback:
            case tls_handshake_config_TLSv1_3DES_fallback:
            case tls_handshake_config_TLSv1_fallback:
            case tls_handshake_config_default:
                expected_n = sizeof(default_ciphersuites)/sizeof(uint16_t);
                expected_ciphersuites = default_ciphersuites;
                break;
            case tls_handshake_config_anonymous:
                expected_n = sizeof(anon_ciphersuites)/sizeof(uint16_t);
                expected_ciphersuites = anon_ciphersuites;
                break;
            case tls_handshake_config_none:  /* none means custom, so always return true */
                expected_n = n;
                expected_ciphersuites = ciphersuites;
                break;
        }
    }

    return (n==expected_n) && (memcmp(ciphersuites, expected_ciphersuites, expected_n*sizeof(uint16_t))==0);

}

static int test_result(myFilterCtx_t *client, myFilterCtx_t *server)
{
    int err = 0;
    tls_test_case *test;

    assert(server->test==client->test);

    test = client->test;

    /* This should always be tested, no matter if handshake failed or succeeded */

    {
        tls_protocol_version pv_min, pv_max;

        tls_handshake_get_max_protocol_version(client->filter, &pv_max);
        tls_handshake_get_min_protocol_version(client->filter, &pv_min);
        if(pv_min>pv_max) {
            fprintf(stderr,"client has inconsistent protocol version (%04x>%04x)\n",pv_min, pv_max);
            err = -1;
        }
        tls_handshake_get_max_protocol_version(server->filter, &pv_max);
        tls_handshake_get_min_protocol_version(server->filter, &pv_min);
        if(pv_min>pv_max) {
            fprintf(stderr,"server has inconsistent protocol version (%04x>%04x)\n",pv_min, pv_max);
            err = -1;
        }

        const uint16_t *ciphersuites;
        unsigned int n;

        tls_handshake_get_ciphersuites(client->filter, &ciphersuites, &n);
        if(!test_ciphersuites(test, false, ciphersuites, n)) {
            fprintf(stderr,"client has unexpected ciphersuites\n");
            err = -1;
        }

        tls_handshake_get_ciphersuites(server->filter, &ciphersuites, &n);
        if(!test_ciphersuites(test, true, ciphersuites, n)) {
            fprintf(stderr,"server has unexpected ciphersuites\n");
            err = -1;
        }

        if(client->bad_client_hello_size) {
            fprintf(stderr,"client hello size was bad\n");
            err = -1;
        }
    }

    if(test->handshake_ok) {
        /* The handshake should have succeeded */

        int expected_handshake_count;

        if((test->client_trigger_renegotiation || test->server_trigger_renegotiation) && test->server_allow_renegotiation) {
            expected_handshake_count = 2;
        } else {
            expected_handshake_count = 1;
        }

        if(client->read_ready!=expected_handshake_count) {
            fprintf(stderr,"client read_ready received: %d, expected %d\n",client->read_ready, expected_handshake_count);
            err = -1;
        }
        if(client->write_ready!=expected_handshake_count) {
            fprintf(stderr,"client write_ready received: %d, expected %d\n",client->write_ready, expected_handshake_count);
            err = -1;
        }
        if(server->read_ready!=expected_handshake_count) {
            fprintf(stderr,"server read_ready received: %d, expected %d\n",server->read_ready, expected_handshake_count);
            err = -1;
        }
        if(server->write_ready!=expected_handshake_count) {
            fprintf(stderr,"server write_ready received: %d, expected %d\n",server->write_ready, expected_handshake_count);
            err = -1;
        }

        uint16_t client_negotiated_ciphersuite = tls_handshake_get_negotiated_cipherspec(client->filter);
        test_printf("client negotiated ciphersuite: %04x\n", client_negotiated_ciphersuite);
        uint16_t server_negotiated_ciphersuite = tls_handshake_get_negotiated_cipherspec(server->filter);
        test_printf("server negotiated ciphersuite: %04x\n", server_negotiated_ciphersuite);

        if(client_negotiated_ciphersuite!=server_negotiated_ciphersuite) {
            fprintf(stderr, "client and server ciphersuites do not match. client=%04x, server=%04x\n", client_negotiated_ciphersuite, server_negotiated_ciphersuite);
            err = -1;
        } else if(test->negotiated_ciphersuite && (client_negotiated_ciphersuite!=test->negotiated_ciphersuite)) {
            fprintf(stderr, "ciphersuite negotiated: %04x, expected %04x\n",client_negotiated_ciphersuite,client->test->negotiated_ciphersuite);
            err = -1;
        }

        /* Session resumption */
        bool client_session_match, server_session_match;
        bool client_session_proposed, server_session_proposed;
        tls_buffer client_sessionID, server_sessionID;
        tls_buffer client_proposed_sessionID, server_proposed_sessionID;

        client_session_match = tls_handshake_get_session_match(client->filter, &client_sessionID);
        server_session_match = tls_handshake_get_session_match(server->filter, &server_sessionID);
        client_session_proposed = tls_handshake_get_session_proposed(client->filter, &client_proposed_sessionID);
        server_session_proposed = tls_handshake_get_session_proposed(server->filter, &server_proposed_sessionID);

        if(client_session_proposed!=server_session_proposed) {
            fprintf(stderr, "client and server session proposed resumption state do no match. client=%d, server=%d\n",client_session_proposed, server_session_proposed);
            err = -1;
        }

        if(test->is_session_resumption_proposed!=client_session_proposed) {
            fprintf(stderr, "session proposed resumption state: %d, expected %d\n",
                    client_session_proposed, test->is_session_resumption_proposed);
            err = -1;
        } else if(client_session_proposed) {
            if((client_proposed_sessionID.length!=server_proposed_sessionID.length) ||
               (memcmp(client_proposed_sessionID.data, server_proposed_sessionID.data, client_proposed_sessionID.length)!=0))
            {
                fprintf(stderr, "session resumption was proposed, but proposed session IDs do not match\n");
                err = -1;
            }
        }

        if(client_session_match!=server_session_match) {
            fprintf(stderr, "client and server session resumption state do no match. client=%d, server=%d\n",
                    client_session_match, server_session_match);
            err = -1;
        }

        if(test->allow_resumption) {
            if(client_session_match!=test->is_session_resumed) {
                fprintf(stderr, "session resumption state: %d, expected %d\n", client_session_match, test->is_session_resumed);
                err = -1;
            } else if(client_session_match) {
                // Got session resumption as expected
                if((client_sessionID.length!=server_sessionID.length) ||
                   (memcmp(client_sessionID.data, server_sessionID.data, client_sessionID.length)!=0))
                {
                    fprintf(stderr, "session was resumed, but session IDs do not match\n");
                    err = -1;
                }
                if((client_sessionID.length!=client_proposed_sessionID.length) ||
                   (memcmp(client_sessionID.data, client_proposed_sessionID.data, client_sessionID.length)!=0))
                {
                    fprintf(stderr, "session was resumed, but session ID do not match proposed session ID\n");
                    err = -1;
                }
            }
        } else {
            if(client_session_match) {
                fprintf(stderr, "session resumed while not allowed\n");
                err = -1;
            }
        }

        /* OCSP */
        bool expected_peer_ocsp_enabled = test->client_ocsp_enable && test->server_ocsp_enable;
        const tls_buffer *ocsp_response = tls_handshake_get_peer_ocsp_response(client->filter);
        const tls_buffer_list_t *ocsp_responder_id_list = tls_handshake_get_peer_ocsp_responder_id_list(server->filter);
        const tls_buffer *ocsp_request_extensions = tls_handshake_get_peer_ocsp_request_extensions(server->filter);


        if(expected_peer_ocsp_enabled!=server->peer_ocsp_enabled) {
            fprintf(stderr, "server peer ocsp enable state: %d, expected %d\n", server->peer_ocsp_enabled, expected_peer_ocsp_enabled);
            err = -1;
        }
        if(expected_peer_ocsp_enabled!=client->peer_ocsp_enabled) {
            fprintf(stderr, "client peer ocsp enable state: %d, expected %d\n", client->peer_ocsp_enabled, expected_peer_ocsp_enabled);
            err = -1;
        }

        if(expected_peer_ocsp_enabled) {

            if(test->ocsp_response) {
                // we should have received an ocsp response.
                if(ocsp_response==NULL) {
                    fprintf(stderr, "Unexpected: no ocsp response received\n");
                    err = -1;
                } else if(!tls_buffer_equal(test->ocsp_response, ocsp_response)) {
                    fprintf(stderr, "OCSP response mismatch\n");
                    err = -1;
                }
            }

            if(test->ocsp_request_extensions) {
                // we should have received an ocsp response.
                if(ocsp_request_extensions==NULL) {
                    fprintf(stderr, "Unexpected: no ocsp request extension received\n");
                    err = -1;
                } else if(!tls_buffer_equal(test->ocsp_request_extensions, ocsp_request_extensions)) {
                    fprintf(stderr, "OCSP request extension mismatch\n");
                    err = -1;
                }
            }

            if(test->ocsp_responder_id_list) {
                // we should have received an ocsp response.
                if(ocsp_responder_id_list==NULL) {
                    fprintf(stderr, "Unexpected: no ocsp responder list received\n");
                    err = -1;
                } else if(!tls_buffer_list_equal(test->ocsp_responder_id_list, ocsp_responder_id_list)) {
                    fprintf(stderr, "OCSP responder list mismatch\n");
                    err = -1;
                }
            }
        }

        /* SCT */
        const tls_buffer_list_t *sct_list = tls_handshake_get_peer_sct_list(client->filter);

        if(test->sct_enable) {
            if(!tls_handshake_get_peer_sct_enabled(server->filter)) {
                fprintf(stderr, "Server should have received SCT enabled\n");
                err = -1;
            }
            if(!tls_buffer_list_equal(test->sct_list, sct_list)){
                fprintf(stderr, "Received SCT list mismatch\n");
                err = -1;
            }
        } else {
            if(tls_handshake_get_peer_sct_enabled(server->filter)) {
                fprintf(stderr, "Server should NOT have received SCT enabled\n");
                err = -1;
            }
            if(sct_list) {
                fprintf(stderr, "Client should not received a SCT list");
                err = -1;
            }
        }

        /* EC curves */
        uint16_t client_negotiated_ec_curve = tls_handshake_get_negotiated_curve(client->filter);
        test_printf("client negotiated ec curve: %04x\n", client_negotiated_ec_curve);
        uint16_t server_negotiated_ec_curve = tls_handshake_get_negotiated_curve(server->filter);
        test_printf("server negotiated ciphersuite: %04x\n", server_negotiated_ec_curve);

        if(client_negotiated_ec_curve != server_negotiated_ec_curve) {
            fprintf(stderr, "client and server ec curves do not match. client=%04x, server=%04x\n", client_negotiated_ec_curve, server_negotiated_ec_curve);
            err = -1;
        } else if(test->negotiated_ec_curve && (client_negotiated_ec_curve != test->negotiated_ec_curve)) {
            fprintf(stderr, "EC curve negotiated: %04x, expected %04x\n", client_negotiated_ec_curve, test->negotiated_ec_curve);
            err = -1;
        }


        /* Client Auth (TODO) */
        if(test->request_client_auth) {

        }

        /* Extended Master Secret */
        if (test->client_extMS_enable && test->server_extMS_enable) {
            if (!tls_handshake_get_negotiated_ems(client->filter) || !tls_handshake_get_negotiated_ems(server->filter)) {
                fprintf(stderr, "extended master secret should have been used\n");
                err = -1;
            }
        } else {
            if (tls_handshake_get_negotiated_ems(client->filter) || tls_handshake_get_negotiated_ems(server->filter)) {
                fprintf(stderr, "Unexpected: extended master secret should NOT have been used\n");
                err = -1;
            }
        }

    } else {
        /* The handshake should have failed or timed out*/
        if(test->client_err) { // IF we specified a specific error code, we test for it.
            if(client->err!=test->client_err) {
                fprintf(stderr, "client err: %d, expected %d\n", client->err, test->client_err);
                err = client->err?client->err:-1;
            }
        } else {  // Otherwise, just make sure it's either non 0 or that the handshake didnt complete (ready flags).
            if(client->read_ready && client->write_ready && (client->err==0 || client->err==-9805)) {
                fprintf(stderr, "client handshake completed, expected failure\n");
                err = -1;
            }
        }
        if(test->server_err) { // IF we specified a specific error code, we test for it.
            if(server->err!=test->server_err) {
                printf("server err: %d, expected %d\n", server->err, test->server_err);
                err = server->err?server->err:-1;
            }
        } else {  // Otherwise, just make sure it's non 0.
            if(server->read_ready && server->write_ready && (server->err==0 || server->err==-9805)) {
                printf("server handshake completed, expected failure\n");
                err = -1;
            }
        }
    }

    return err;
}


static SSLCertificate g_server_cert;        // This is an RSA cert.
static tls_private_key_t g_server_key;
static SSLCertificate g_server_ecdsa_cert;  // This is an ECDSA cert.
static tls_private_key_t g_server_ecdsa_key;
static SSLCertificate g_client_cert;
static tls_private_key_t g_client_key;

static
int test_one_case(tls_test_case *test)
{
    int err;
    myFilterCtx_t client_c = {0,};
    myFilterCtx_t server_c = {0,};

    dispatch_group_t dg = dispatch_group_create();
    dispatch_queue_t gq = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 600LL*NSEC_PER_SEC); // 10 seconds

    require_action(dg!=NULL, fail, err=-1);
    require_action(gq!=NULL, fail, err=-1);


    require_noerr((err=init_context(&client_c, false, "client write queue", test, dg)), fail);
    require_noerr((err=init_context(&server_c, true, "server write queue", test, dg)), fail);

    client_c.peer=&server_c;
    server_c.peer=&client_c;

    myFilterCtx_t *myCtx = &client_c;

    dispatch_group_async(dg, myCtx->queue, ^{

        tls_buffer peerID = {
            .data = (uint8_t *)&test->session_id,
            .length = sizeof(test->session_id),
        };

        int err;

        if(test->session_id) {
            err = tls_handshake_negotiate(myCtx->filter, &peerID);
        } else {
            err = tls_handshake_negotiate(myCtx->filter, NULL);
        }

        if(err) {
            test_printf("Error %d while initiating handshake, closing.\n", err);
            myCtx->peer = NULL;
            myCtx->err = err;
        }
    });

    /* Wait until both client and server queues are done */
    require_noerr((err=(int)dispatch_group_wait(dg, timeout)), fail);

    test_printf("Done - client: err=%d, read=%d, write=%d\n", client_c.err, client_c.read_ready, client_c.write_ready);
    test_printf("Done - server: err=%d, read=%d, write=%d\n", server_c.err, server_c.read_ready, server_c.write_ready);

    dispatch_sync(client_c.queue,^{
        tls_handshake_close(client_c.filter);
    });
    dispatch_sync(server_c.queue,^{
        tls_handshake_close(server_c.filter);
    });

    err = test_result(&client_c, &server_c);

fail:
    if(dg) dispatch_release(dg);
    clean_context(&client_c);
    clean_context(&server_c);
    return err;
}


static SSLCertificate * server_cert_for_cipher(uint16_t cs)
{
    KeyExchangeMethod kem = sslCipherSuiteGetKeyExchangeMethod(cs);
    if(kem==SSL_ECDHE_ECDSA)
        return &g_server_ecdsa_cert;
    else
        return &g_server_cert;
}

static tls_private_key_t server_key_for_cipher(uint16_t cs)
{
    KeyExchangeMethod kem = sslCipherSuiteGetKeyExchangeMethod(cs);
    if(kem==SSL_ECDHE_ECDSA)
        return g_server_ecdsa_key;
    else
        return g_server_key;
}

//
// MARK: positive self tests
//

static void good_tests(const CipherSuiteName *ciphers, size_t n_ciphers, int min_proto, bool csa)
{
    tls_test_case test = {0,};

    int i, j, err;

    for(i=0; i<n_ciphers; i++) {
        // Skip SSL3 for tls only ciphers:
        for(j=min_proto; j<nprotos; j++) {
            test.client_protocol_min = protos[j];
            test.client_protocol_max = protos[j];
            test.server_protocol_min = protos[j];
            test.server_protocol_max = protos[j];
            test.ciphersuites = &ciphers[i].cipher;
            test.num_ciphersuites = 1;
            test.allow_resumption = false;
            test.session_id = 0;
            test.server_certs = server_cert_for_cipher(ciphers[i].cipher);
            test.server_key = server_key_for_cipher(ciphers[i].cipher);
            test.client_certs = &g_client_cert;
            test.client_key = g_client_key;
            test.request_client_auth = csa;

            // expected outputs of test case
            test.handshake_ok = true;
            test.client_err = 0;
            test.server_err = 0;
            test.is_session_resumed = false;
            test.certificate_requested = 0;
            test.negotiated_ciphersuite = ciphers[i].cipher;
            test.negotiated_version = (protos[j]==0)? tls_protocol_version_TLS_1_2:protos[j];

            test_log_start();
            test_printf("Good Test case i=%d, j=%d , csa=%d (%s, %04x)\n", i, j, csa, ciphers[i].name, protos[j]);
            err = test_one_case(&test);
            ok(!err, "Good Test case (i=%d, j=%d , csa=%d  / %s, %04x)", i, j, csa, ciphers[i].name, protos[j]);
            test_log_end(err);
        }
    }
}

//
// MARK: all ciphers test
//
static void ciphersuites_tests(void)
{
    tls_test_case test = {0,};
    int err;

    for(int i=0; i<2; i++) {
        test.client_protocol_min = tls_protocol_version_SSL_3;
        test.client_protocol_max = tls_protocol_version_TLS_1_2;
        test.server_protocol_min = tls_protocol_version_SSL_3;
        test.server_protocol_max = tls_protocol_version_TLS_1_2;
        test.allow_resumption = false;
        test.session_id = 0;
        if(i==1) {
            test.server_certs = &g_server_ecdsa_cert;
            test.server_key = g_server_ecdsa_key;
        } else {
            test.server_certs = &g_server_cert;
            test.server_key = g_server_key;
        }
        test.client_certs = &g_client_cert;
        test.client_key = g_client_key;

        // expected outputs of test case
        test.handshake_ok = true;
        test.client_err = 0;
        test.server_err = 0;
        test.is_session_resumed = false;
        test.certificate_requested = 0;
        if(i==1) {
            test.negotiated_ciphersuite = TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384;
        } else {
            test.negotiated_ciphersuite = TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384;
        }
        test.negotiated_version = tls_protocol_version_TLS_1_2;

        test.ciphersuites = KnownCipherSuites;
        test.num_ciphersuites = CipherSuiteCount;

        test_log_start();
        test_printf("Test case: ciphersuites KnownCipherSuites\n");
        err = test_one_case(&test);
        ok(!err, "Test case: ciphersuites KnownCipherSuites");
        test_log_end(err);

        test.ciphersuites = NULL;
        test.num_ciphersuites = 0;

        test_log_start();
        test_printf("Test case: ciphersuites NULL\n");
        err = test_one_case(&test);
        ok(!err, "Test case: ciphersuites NULL");
        test_log_end(err);

    }
}


//
// MARK: resumption tests
//

// TODO: resumption tests for TLS only ciphers
static void resumption_tests(const CipherSuiteName *ciphers, size_t n_ciphers, int min_proto)
{
    tls_test_case test = {0,};

    int i, j, err;

    for(i=0; i<n_ciphers; i++) {
        // Skip SSL3 for tls only ciphers:
        for(j=min_proto; j<nprotos; j++) {
            test.client_protocol_min = protos[j];
            test.client_protocol_max = protos[j];
            test.server_protocol_min = protos[j];
            test.server_protocol_max = protos[j];
            test.ciphersuites = &ciphers[i].cipher;
            test.num_ciphersuites = 1;
            test.allow_resumption = true;
            test.session_id = (ciphers[i].cipher<<8 | j) + 1;
            test.server_certs = server_cert_for_cipher(ciphers[i].cipher);
            test.server_key = server_key_for_cipher(ciphers[i].cipher);
            // Extended master secret not supported for ssl3
            if (j > 0) {
                test.server_extMS_enable = true;
                test.client_extMS_enable = true;
            } else {
                test.server_extMS_enable = false;
                test.client_extMS_enable = false;
            }

            // expected outputs of test case
            test.handshake_ok = true;
            test.client_err = 0;
            test.server_err = 0;
            test.certificate_requested = 0;
            test.negotiated_ciphersuite = ciphers[i].cipher;
            test.negotiated_version = protos[j];

            test.is_session_resumption_proposed = false; // First one should not propose resumption
            test.is_session_resumed = false; // First one should not be a resumption

            test_log_start();
            test_printf("Resumption test case %d, %d (%s, %04x) - 1st connection\n", i, j, ciphers[i].name, protos[j]);
            err = test_one_case(&test);
            ok(!err, "Resumption test case (%d, %d / %s, %04x) - 1st connection", i, j, ciphers[i].name, protos[j]);
            test_log_end(err);

            test.is_session_resumption_proposed = true; // Second one should propose resumption
            test.is_session_resumed = true; // Second one should be a resumption

            test_log_start();
            test_printf("Resumption test case %d, %d (%s, %04x) - 2nd connection\n", i, j, ciphers[i].name, protos[j]);
            err = test_one_case(&test);
            ok(!err, "Resumption test case (%d, %d / %s, %04x) - 2nd connection", i, j, ciphers[i].name, protos[j]);
            test_log_end(err);
        }
    }
}

//
// MARK: resumption tests
//

// TODO: resumption tests with mismatched versions, and ciphers.
static void resumption_mismatch_tests(void)
{
    tls_test_case test = {0,};

    int i, err;

    uint16_t cipherA = TLS_RSA_WITH_AES_128_CBC_SHA;
    uint16_t cipherB = TLS_RSA_WITH_AES_256_CBC_SHA;

    tls_cache_empty(g_cache);

    // Case 0: Attempt to resume a TLS 1.2 connection with TLS 1.0.
    // Case 1: Attempt to resume a cipherA connection with cipherB.
    for(i=0; i<2; i++) {
        test.client_ciphersuites = &cipherA;
        test.num_client_ciphersuites = 1;
        test.allow_resumption = true;
        test.session_id = i+1;
        test.server_certs = server_cert_for_cipher(cipherA);
        test.server_key = server_key_for_cipher(cipherA);
        test.client_protocol_max = tls_protocol_version_TLS_1_2;

        // expected outputs of test case
        test.handshake_ok = true;
        test.client_err = 0;
        test.server_err = 0;
        test.certificate_requested = 0;
        test.negotiated_ciphersuite = cipherA;
        test.negotiated_version = tls_protocol_version_TLS_1_2;

        test_log_start();
        test_printf("Resumption mismatch test case (%d) - 1st connection\n", i);
        err = test_one_case(&test);
        ok(!err, "Resumption mismatch test case (%d) - 1st connection", i);
        test_log_end(err);

        switch (i) {
            case 0:
                test.client_protocol_max = tls_protocol_version_TLS_1_0;
                break;
            case 1:
                test.client_ciphersuites = &cipherB;
                test.negotiated_ciphersuite = cipherB;
                break;
            default:
                break;
        }

        test_log_start();
        test_printf("Resumption mismatch test case (%d) - 2nd connection\n", i);
        err = test_one_case(&test);
        ok(!err, "Resumption mismatch test case (%d) - 2nd connection", i);
        test_log_end(err);
    }
}

//
// MARK: renegotiation tests
//

static void renegotiation_tests(void)
{
    tls_test_case test = {0,};

    int t, err;
    uint16_t cipher = TLS_RSA_WITH_AES_128_CBC_SHA;
    for(t=0; t<8; t++) {
        test.ciphersuites = &cipher;
        test.num_ciphersuites = 1;
        test.allow_resumption = false;
        test.session_id = &renegotiation_tests; // this way the session is only for this test
        test.server_certs = &g_server_cert;
        test.server_key = g_server_key;

        test.client_trigger_renegotiation = t&1;
        test.server_trigger_renegotiation = t&2;
        test.server_allow_renegotiation = t&4;

        // expected outputs of test case
        test.handshake_ok = true;
        test.client_err = 0;
        test.server_err = 0;
        test.certificate_requested = 0;
        test.negotiated_ciphersuite = cipher;
        test.negotiated_version = tls_protocol_version_TLS_1_2;

        test.is_session_resumed = false;
        test_log_start();
        test_printf("Renegotiation case %d\n", t);
        err = test_one_case(&test);
        ok(!err, "Renegotiation test case (%d)", t);
        test_log_end(err);
    }
}

//
// MARK: "goto fail;" test
//

extern SSLCertificate google_cert0;

static void goto_fail_test(void)
{
    uint16_t cipher = TLS_DHE_RSA_WITH_AES_128_CBC_SHA;
    tls_protocol_version pv = tls_protocol_version_TLS_1_2;
    tls_test_case test = {0,};
    int err;

    test.client_protocol_min = pv;
    test.client_protocol_max = pv;
    test.server_protocol_min = pv;
    test.server_protocol_max = pv;
    test.ciphersuites = &cipher;
    test.num_ciphersuites = 1;
    test.allow_resumption = false;
    test.session_id = 0;

    // This is the key:
    test.server_certs = &google_cert0;
    test.server_key = g_server_key;
    test.client_certs = &g_client_cert;
    test.client_key = g_client_key;

    // expected outputs of test case
    test.handshake_ok = false;
    test.client_err = 0;
    test.server_err = 0;
    test.is_session_resumed = false;
    test.certificate_requested = 0;
    test.negotiated_ciphersuite = cipher;
    test.negotiated_version = pv;

    test_log_start();
    test_printf("Test case \"goto fail;\"\n");
    err = test_one_case(&test);
    ok(!err, "Test case \"goto fail;\"");
    test_log_end(err);
}

//
// MARK: Corrupted message len tests
//

tls_handshake_message_t messages[] = {
    tls_handshake_message_client_hello,
    tls_handshake_message_server_hello,
    tls_handshake_message_certificate,
    tls_handshake_message_server_hello_done,
    tls_handshake_message_server_key_exchange,
    tls_handshake_message_client_key_exchange,
    tls_handshake_message_finished,
};
int nmessages = sizeof(messages)/sizeof(messages[0]);

static int tls_handshake_fuzz_handshake_msg_len(myFilterCtx_t *myCtx, const tls_buffer in, uint8_t content_type)
{
    tls_handshake_message_t msgtype = (tls_handshake_message_t)(myCtx->test->fuzz_ctx & 0xff);
    uint8_t byte = (myCtx->test->fuzz_ctx>>8) & 0xff;

    if((content_type==tls_record_type_Handshake) && (in.data[0]==msgtype)) {
        tls_buffer out;
        int err;

        out.data=malloc(in.length);
        out.length=in.length;

        if(out.data==NULL) return -1;

        memcpy(out.data, in.data, in.length);
        out.data[3]=out.data[3]+byte;

        err = send_to_peer(myCtx, out, content_type);
        free(out.data);
        return err;
    } else {
        return send_to_peer(myCtx, in, content_type);
    }
}

static void corrupted_message_len_tests(void)
{
    tls_test_case test = {0,};
    int err;
    int i,j;

    /* Use a DHE cipher suite so we can test corruption of server key exchange messages */
    uint16_t cipher = TLS_DHE_RSA_WITH_AES_128_CBC_SHA;

    for(i=0; i<nmessages; i++)
    {
        for(j=0; j<2; j++)
        {
            test.client_protocol_min = tls_protocol_version_TLS_1_2;
            test.client_protocol_max = tls_protocol_version_TLS_1_2;
            test.server_protocol_min = tls_protocol_version_TLS_1_2;
            test.server_protocol_max = tls_protocol_version_TLS_1_2;
            test.ciphersuites = &cipher;
            test.num_ciphersuites = 1;
            test.allow_resumption = false;
            test.session_id = 0;
            test.server_certs = &g_server_cert;
            test.server_key = g_server_key;

            test.fuzzer = tls_handshake_fuzz_handshake_msg_len;
            test.fuzz_ctx = ((j?0xff:1)<<8) | messages[i];

            // expected outputs of test case
            test.handshake_ok = false; // should fail or timeout
            test.client_err = 0;  // unspecified
            test.server_err = 0;  // unspecifief
            test.is_session_resumed = false;
            test.certificate_requested = 0;
            test.negotiated_ciphersuite = cipher;
            test.negotiated_version = tls_protocol_version_TLS_1_2;

            test_log_start();
            test_printf("Test case corrupting message len (%d, %d)\n", i, j);
            err = test_one_case(&test);
            ok(!err, "Test case fuzzing message len (%d, %d)", i, j);
            test_log_end(err);
        }
    }
}

//
// MARK: Corrupted certificate len tests
//

static int decode_len(uint8_t *p)
{
    return (p[0]<<16)|(p[1]<<8)|p[2];
}

static void encode_len(int l, uint8_t *p)
{
    p[0]=(l>>16)&0xff;
    p[1]=(l>>8)&0xff;
    p[2]=(l)&0xff;
}

static int tls_handshake_fuzz_cert_len(myFilterCtx_t *myCtx, const tls_buffer in, uint8_t content_type)
{

    if((content_type==tls_record_type_Handshake) && (in.data[0]==tls_handshake_message_certificate)) {
        tls_buffer out;
        int err;

        out.data=malloc(in.length);
        out.length=in.length;

        if(out.data==NULL) return -1;

        memcpy(out.data, in.data, in.length);

        unsigned listLen = decode_len(out.data+4);

        switch(myCtx->test->fuzz_ctx){
            case 0:
                /* list len too big */
                encode_len(listLen+1, out.data+4);
                break;
            case 1:
                /* list len too small */
                encode_len(listLen-1, out.data+4);
                break;
            case 2:
                /* cert len too big */
                encode_len(listLen-2, out.data+7);
                break;
            case 3:
                /* cert len too short */
                encode_len(listLen-4, out.data+7);
                break;

            default:
                assert(0);
        }

        err = send_to_peer(myCtx, out, content_type);
        free(out.data);
        return err;
    } else {
        return send_to_peer(myCtx, in, content_type);
    }
}

static void corrupted_cert_len_tests(void)
{
    tls_test_case test = {0,};
    int err;
    int i;

    uint16_t cipher = TLS_DHE_RSA_WITH_AES_128_CBC_SHA;

    for(i=0; i<4; i++)
    {
        test.client_protocol_min = tls_protocol_version_TLS_1_2;
        test.client_protocol_max = tls_protocol_version_TLS_1_2;
        test.server_protocol_min = tls_protocol_version_TLS_1_2;
        test.server_protocol_max = tls_protocol_version_TLS_1_2;
        test.ciphersuites = &cipher;
        test.num_ciphersuites = 1;
        test.allow_resumption = false;
        test.session_id = 0;
        test.server_certs = &g_server_cert;
        test.server_key = g_server_key;
        test.client_certs = &g_client_cert;
        test.client_key = g_client_key;

        test.fuzzer = tls_handshake_fuzz_cert_len;
        test.fuzz_ctx = i;

        // expected outputs of test case
        test.handshake_ok = false; // should fail or timeout
        test.client_err = 0;  // unspecified
        test.server_err = 0;  // unspecified
        test.is_session_resumed = false;
        test.certificate_requested = 0;
        test.negotiated_ciphersuite = cipher;
        test.negotiated_version = tls_protocol_version_TLS_1_2;

        test_log_start();
        test_printf("Test case corrupting cert len (%d)\n", i);
        err = test_one_case(&test);
        ok(!err, "Test case fuzzing cert len (%d)", i);
        test_log_end(err);
    }
}

//
// MARK: trust setting tests
//

tls_handshake_trust_t trust_values[] = {
    tls_handshake_trust_ok,
    tls_handshake_trust_unknown,
    tls_handshake_trust_unknown_root,
    tls_handshake_trust_cert_expired,
    tls_handshake_trust_cert_invalid,
};
int ntrust_values = sizeof(trust_values)/sizeof(trust_values[0]);

static tls_handshake_trust_t truster(intptr_t trust_ctx)
{
    assert(trust_ctx<ntrust_values);
    return trust_values[trust_ctx];
}

static void trust_tests(void)
{
    tls_test_case test = {0,};
    int err;
    int i;

    uint16_t cipher = TLS_DHE_RSA_WITH_AES_128_CBC_SHA;

    for(i=0; i<ntrust_values; i++)
    {
        test.client_protocol_min = tls_protocol_version_TLS_1_2;
        test.client_protocol_max = tls_protocol_version_TLS_1_2;
        test.server_protocol_min = tls_protocol_version_TLS_1_2;
        test.server_protocol_max = tls_protocol_version_TLS_1_2;
        test.ciphersuites = &cipher;
        test.num_ciphersuites = 1;
        test.server_certs = &g_server_cert;
        test.server_key = g_server_key;
        test.client_certs = &g_client_cert;
        test.client_key = g_client_key;

        test.server_trust = truster;
        test.server_trust_ctx = i;

        // expected outputs of test case
        test.handshake_ok = (i==0); // should fail or timeout
        test.client_err = 0;  // unspecified
        test.server_err = 0;  // unspecifief
        test.is_session_resumed = false;
        test.certificate_requested = 0;
        test.negotiated_ciphersuite = cipher;
        test.negotiated_version = tls_protocol_version_TLS_1_2;

        test_log_start();
        test_printf("Test case trust (%d)\n", i);
        err = test_one_case(&test);
        ok(!err, "Test case trust (%d) - err=%d", i, err);
        test_log_end(err);
    }
}

static void client_auth_trust_tests(void)
{
    tls_test_case test = {0,};
    int err;
    int i, j;

    uint16_t cipher = TLS_DHE_RSA_WITH_AES_128_CBC_SHA;

    for(i=0; i<ntrust_values; i++)
    {
        for(j=0; j<2; j++)
        {
            test.client_protocol_min = tls_protocol_version_TLS_1_2;
            test.client_protocol_max = tls_protocol_version_TLS_1_2;
            test.server_protocol_min = tls_protocol_version_TLS_1_2;
            test.server_protocol_max = tls_protocol_version_TLS_1_2;
            test.ciphersuites = &cipher;
            test.num_ciphersuites = 1;
            test.server_certs = &g_server_cert;
            test.server_key = g_server_key;
            test.client_certs = (j) ? &g_client_cert : NULL;
            test.client_key = (j) ? g_client_key : NULL;
            test.request_client_auth = true;

            test.client_trust = truster;
            test.client_trust_ctx = i;

            // expected outputs of test case
            test.handshake_ok = (i==0); // should fail or timeout
            test.client_err = 0;  // unspecified
            test.server_err = 0;  // unspecifief
            test.is_session_resumed = false;
            test.certificate_requested = 0;
            test.negotiated_ciphersuite = cipher;
            test.negotiated_version = tls_protocol_version_TLS_1_2;

            test_log_start();
            test_printf("Test case client auth trust (%d:%d)\n", i, j);
            err = test_one_case(&test);
            ok(!err, "Test case client auth trust (%d:%d) - err=%d", i, j, err);
            test_log_end(err);
        }
    }
}


//
// MARK: cert type mismatch test
//

extern SSLCertificate ec_cert;

static void cert_mismatch_test(void)
{
    tls_test_case test = {0,};
    int err;

    uint16_t cipher = TLS_RSA_WITH_AES_128_CBC_SHA;

    test.client_protocol_min = tls_protocol_version_TLS_1_2;
    test.client_protocol_max = tls_protocol_version_TLS_1_2;
    test.server_protocol_min = tls_protocol_version_TLS_1_2;
    test.server_protocol_max = tls_protocol_version_TLS_1_2;
    test.ciphersuites = &cipher;
    test.num_ciphersuites = 1;
    test.allow_resumption = false;
    test.session_id = 0;
    test.server_certs = &ec_cert;
    test.server_key = g_server_key;
    test.client_certs = &g_client_cert;
    test.client_key = g_client_key;

    // expected outputs of test case
    test.handshake_ok = false; // should fail or timeout
    test.client_err = 0;  // unspecified
    test.server_err = 0;  // unspecifief
    test.is_session_resumed = false;
    test.certificate_requested = 0;
    test.negotiated_ciphersuite = cipher;
    test.negotiated_version = tls_protocol_version_TLS_1_2;

    test_log_start();
    test_printf("Test case cert mismatch\n");
    err = test_one_case(&test);
    ok(!err, "Test case cert mismatch - err=%d", err);
    test_log_end(err);
}

//
// MARK: OCSP test
//


/* A dummy ocsp response */
static uint8_t ocsp_response_data[] = {
    0x30, 0x82, 0x01, 0x72, 0x0a, 0x01, 0x00, 0xa0, 0x82, 0x01, 0x6b, 0x30,
    0x82, 0x01, 0x67, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30,
    0x01, 0x01, 0x04, 0x82, 0x01, 0x58, 0x30, 0x82, 0x01, 0x54, 0x30, 0x81,
    0xbe, 0xa1, 0x22, 0x30, 0x20, 0x31, 0x1e, 0x30, 0x1c, 0x06, 0x03, 0x55,
    0x04, 0x03, 0x13, 0x15, 0x63, 0x6f, 0x72, 0x65, 0x54, 0x4c, 0x53, 0x20,
    0x43, 0x41, 0x20, 0x43, 0x65, 0x72, 0x74, 0x20, 0x28, 0x52, 0x53, 0x41,
    0x29, 0x18, 0x0f, 0x32, 0x30, 0x31, 0x34, 0x30, 0x38, 0x32, 0x30, 0x32,
    0x31, 0x31, 0x33, 0x30, 0x37, 0x5a, 0x30, 0x62, 0x30, 0x60, 0x30, 0x3a,
    0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04,
    0x14, 0x20, 0xd4, 0x96, 0xb3, 0xfb, 0xd1, 0xb8, 0x84, 0x3a, 0x38, 0x14,
    0xdb, 0x33, 0xd1, 0x0d, 0xa8, 0xca, 0x96, 0xba, 0x13, 0x04, 0x14, 0xb2,
    0x23, 0x1b, 0x0f, 0x2c, 0x5a, 0xa2, 0x1d, 0xeb, 0x96, 0x34, 0xa7, 0x6f,
    0x9d, 0x97, 0x11, 0x81, 0x14, 0x61, 0xbb, 0x02, 0x01, 0x01, 0xa1, 0x11,
    0x18, 0x0f, 0x32, 0x30, 0x31, 0x34, 0x30, 0x38, 0x32, 0x30, 0x30, 0x30,
    0x30, 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x32, 0x30, 0x31, 0x34, 0x30,
    0x38, 0x32, 0x30, 0x32, 0x31, 0x31, 0x33, 0x30, 0x37, 0x5a, 0xa1, 0x23,
    0x30, 0x21, 0x30, 0x1f, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07,
    0x30, 0x01, 0x02, 0x04, 0x12, 0x04, 0x10, 0x4c, 0xc5, 0x63, 0xf2, 0x0a,
    0x84, 0x8c, 0x03, 0xa4, 0x0d, 0x97, 0xd1, 0xa2, 0xbb, 0x1e, 0xb2, 0x30,
    0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05,
    0x05, 0x00, 0x03, 0x81, 0x81, 0x00, 0x1b, 0x21, 0xd7, 0x01, 0xde, 0xb8,
    0x58, 0x4b, 0x79, 0x6a, 0xa3, 0x8b, 0xa7, 0xe0, 0xbd, 0xa8, 0xda, 0x58,
    0x48, 0xbb, 0xa7, 0xcd, 0xf7, 0x91, 0x15, 0xb3, 0x38, 0x70, 0xd9, 0x43,
    0x25, 0x72, 0x0e, 0xc3, 0x3d, 0xf9, 0xc7, 0x30, 0x2d, 0xb4, 0x9f, 0x1c,
    0x4b, 0x62, 0x31, 0x48, 0xb4, 0x9f, 0x00, 0xbd, 0x57, 0xb6, 0xec, 0xda,
    0xf0, 0xa2, 0x42, 0x61, 0xfc, 0xef, 0x73, 0xc5, 0x55, 0xc1, 0xf6, 0x72,
    0x79, 0xcf, 0x55, 0x01, 0x09, 0xe4, 0xd2, 0xee, 0xbd, 0xa6, 0x08, 0xc6,
    0x39, 0x3a, 0x17, 0x76, 0x98, 0xaa, 0x61, 0x82, 0xb9, 0x41, 0xe1, 0xbb,
    0x4f, 0x67, 0x5e, 0x0b, 0x5e, 0xfa, 0x3c, 0x12, 0x15, 0xbe, 0x90, 0x8e,
    0x29, 0xe6, 0x5c, 0x9b, 0xfc, 0xaf, 0x40, 0xa4, 0x31, 0xd7, 0xa4, 0xc6,
    0x71, 0x22, 0x01, 0xfa, 0xb2, 0xcd, 0x6e, 0x1f, 0x26, 0xdb, 0xb1, 0xa3,
    0xec, 0x43
};
static tls_buffer g_ocsp_response = {
    .data = ocsp_response_data,
    .length = sizeof(ocsp_response_data),
};


/* This is just dummy data */
static uint8_t dummy_data[] = {
    0x14, 0x20, 0xd4, 0x96, 0xb3, 0xfb, 0xd1, 0xb8, 0x84, 0x3a, 0x38, 0x14,
    0x82, 0x01, 0x67, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30,
    0x01, 0x01, 0x04, 0x82, 0x01, 0x58, 0x30, 0x82, 0x01, 0x54, 0x30, 0x81,
    0xbe, 0xa1, 0x22, 0x30, 0x20, 0x31, 0x1e, 0x30, 0x1c, 0x06, 0x03, 0x55,
    0x04, 0x03, 0x13, 0x15, 0x63, 0x6f, 0x72, 0x65, 0x54, 0x4c, 0x53, 0x20,
    0x30, 0x82, 0x01, 0x72, 0x0a, 0x01, 0x00, 0xa0, 0x82, 0x01, 0x6b, 0x30,
    0x29, 0x18, 0x0f, 0x32, 0x30, 0x31, 0x34, 0x30, 0x38, 0x32, 0x30, 0x32,
    0x31, 0x31, 0x33, 0x30, 0x37, 0x5a, 0x30, 0x62, 0x30, 0x60, 0x30, 0x3a,
    0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04,
    0x14, 0x20, 0xd4, 0x96, 0xb3, 0xfb, 0xd1, 0xb8, 0x84, 0x3a, 0x38, 0x14,
    0xdb, 0x33, 0xd1, 0x0d, 0xa8, 0xca, 0x96, 0xba, 0x13, 0x04, 0x14, 0xb2,
    0x23, 0x1b, 0x0f, 0x2c, 0x5a, 0xa2, 0x1d, 0xeb, 0x96, 0x34, 0xa7, 0x6f,
    0x9d, 0x97, 0x11, 0x81, 0x14, 0x61, 0xbb, 0x02, 0x01, 0x01, 0xa1, 0x11,
};

static tls_buffer g_ocsp_request_extension = {
    .data = dummy_data,
    .length = sizeof(dummy_data),
};

static tls_buffer_list_t g_ocsp_responder_id_list = {
    .next = NULL,
    .buffer.data = dummy_data,
    .buffer.length = sizeof(dummy_data),
};

static void ocsp_tests(void)
{
    tls_test_case test = {0,};
    int err;

    uint16_t cipher = TLS_RSA_WITH_AES_128_CBC_SHA;

    tls_cache_empty(g_cache);

    for(int i=0;i<4;i++) {
        test.client_protocol_min = tls_protocol_version_TLS_1_2;
        test.client_protocol_max = tls_protocol_version_TLS_1_2;
        test.server_protocol_min = tls_protocol_version_TLS_1_2;
        test.server_protocol_max = tls_protocol_version_TLS_1_2;
        test.ciphersuites = &cipher;
        test.num_ciphersuites = 1;
        test.server_certs = &g_server_cert;
        test.server_key = g_server_key;
        test.client_certs = &g_client_cert;
        test.client_key = g_client_key;

        test.ocsp_response = &g_ocsp_response;
        test.ocsp_request_extensions = &g_ocsp_request_extension;
        test.ocsp_responder_id_list = &g_ocsp_responder_id_list;
        test.client_ocsp_enable = i&0x1;
        test.server_ocsp_enable = i&0x2;
        test.allow_resumption = true;
        test.session_id = i + 1;

        // expected outputs of test case
        test.handshake_ok = true;
        test.client_err = 0;
        test.server_err = 0;
        test.certificate_requested = 0;
        test.negotiated_ciphersuite = cipher;
        test.negotiated_version = tls_protocol_version_TLS_1_2;

        test.is_session_resumption_proposed = false; // First one should not propose resumption
        test.is_session_resumed = false; // First one should not be a resumption

        test_log_start();
        test_printf("Test case oscp %d\n", i );
        err = test_one_case(&test);
        ok(!err, "Test case ocsp %d - err=%d", i, err);
        test_log_end(err);

        test.is_session_resumption_proposed = true; // Second one should propose resumption
        test.is_session_resumed = true; // Second one should be a resumption

        test_log_start();
        test_printf("Test case oscp %d (resumed)\n", i );
        err = test_one_case(&test);
        ok(!err, "Test case ocsp %d (resumed) - err=%d", i, err);
        test_log_end(err);
    }

}

//
// MARK: SCT tests
//

static
uint8_t server_D_sct_data[] = {
    0x00, 0xab, 0xa8, 0xb5, 0xb4, 0x7d, 0x00, 0x00, 0x1b, 0x46, 0x58, 0x28,
    0xc4, 0x0a, 0xc7, 0x0b, 0x03, 0xf6, 0x91, 0x70, 0xa3, 0x5f, 0xed, 0xc8,
    0x74, 0x40, 0x3c, 0xd0, 0x58, 0x1d, 0x3c, 0x8c, 0x16, 0x00, 0x00, 0x01,
    0x47, 0xdc, 0x04, 0x70, 0x0e, 0x00, 0x00, 0x04, 0x03, 0x00, 0x46, 0x30,
    0x44, 0x02, 0x20, 0x71, 0x55, 0x2f, 0x75, 0xa8, 0x3a, 0xfd, 0x01, 0x34,
    0x44, 0xc7, 0x84, 0x71, 0x8f, 0x1e, 0xc2, 0x36, 0xe2, 0x08, 0x07, 0x92,
    0x2b, 0x9f, 0x44, 0x0e, 0x84, 0x16, 0x08, 0xe0, 0xaf, 0xc5, 0xb9, 0x02,
    0x20, 0x29, 0x3e, 0x0f, 0x63, 0x5c, 0xe7, 0x0a, 0xea, 0x1f, 0x96, 0x4a,
    0x11, 0x86, 0x72, 0x06, 0xa5, 0x25, 0xc4, 0x5e, 0xf6, 0x92, 0xd8, 0x08,
    0x98, 0x17, 0xba, 0xf2, 0xfe, 0x50, 0x62, 0x36, 0x29
};

static
uint8_t server_D_cert_data[] = {
    0x30, 0x82, 0x02, 0xe4, 0x30, 0x82, 0x02, 0x4d, 0xa0, 0x03, 0x02, 0x01,
    0x02, 0x02, 0x01, 0x13, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
    0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00, 0x30, 0x52, 0x31, 0x0b, 0x30,
    0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x1a,
    0x30, 0x18, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x11, 0x63, 0x6f, 0x72,
    0x65, 0x6f, 0x73, 0x2d, 0x63, 0x74, 0x2d, 0x74, 0x65, 0x73, 0x74, 0x20,
    0x43, 0x41, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c,
    0x0a, 0x43, 0x61, 0x6c, 0x69, 0x66, 0x6f, 0x72, 0x6e, 0x69, 0x61, 0x31,
    0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x07, 0x0c, 0x09, 0x43, 0x75,
    0x70, 0x65, 0x72, 0x74, 0x69, 0x6e, 0x6f, 0x30, 0x1e, 0x17, 0x0d, 0x31,
    0x32, 0x30, 0x36, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a,
    0x17, 0x0d, 0x32, 0x32, 0x30, 0x36, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30,
    0x30, 0x30, 0x5a, 0x30, 0x72, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55,
    0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x17, 0x30, 0x15, 0x06, 0x03,
    0x55, 0x04, 0x0a, 0x0c, 0x0e, 0x63, 0x6f, 0x72, 0x65, 0x6f, 0x73, 0x2d,
    0x63, 0x74, 0x2d, 0x74, 0x65, 0x73, 0x74, 0x31, 0x13, 0x30, 0x11, 0x06,
    0x03, 0x55, 0x04, 0x08, 0x0c, 0x0a, 0x43, 0x61, 0x6c, 0x69, 0x66, 0x6f,
    0x72, 0x6e, 0x69, 0x61, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04,
    0x07, 0x0c, 0x09, 0x43, 0x75, 0x70, 0x65, 0x72, 0x74, 0x69, 0x6e, 0x6f,
    0x31, 0x21, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x18, 0x63,
    0x6f, 0x72, 0x65, 0x6f, 0x73, 0x2d, 0x63, 0x74, 0x2d, 0x74, 0x65, 0x73,
    0x74, 0x2e, 0x61, 0x70, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x30,
    0x81, 0x9f, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
    0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x81, 0x8d, 0x00, 0x30, 0x81, 0x89,
    0x02, 0x81, 0x81, 0x00, 0xba, 0x00, 0xc4, 0xfb, 0x3f, 0x9a, 0x86, 0x43,
    0x1a, 0x26, 0x99, 0x9d, 0x19, 0x67, 0x27, 0xaa, 0x44, 0xd4, 0xba, 0x2b,
    0xfe, 0x7b, 0x32, 0xe8, 0x2a, 0xc7, 0x89, 0x36, 0x41, 0xd7, 0xaf, 0xf4,
    0x97, 0x4d, 0x41, 0x7b, 0xc7, 0x80, 0xba, 0x79, 0xab, 0x9c, 0xeb, 0xcc,
    0x38, 0xb7, 0x83, 0xdf, 0x62, 0x7e, 0xaf, 0x6c, 0x32, 0x57, 0xc2, 0x41,
    0xea, 0x73, 0xa9, 0x45, 0xf8, 0xbe, 0xc2, 0x26, 0x0f, 0x01, 0xec, 0x3b,
    0x02, 0x24, 0x7d, 0x39, 0x5c, 0xa6, 0x9c, 0xdf, 0x4b, 0x1f, 0xd5, 0x4d,
    0xd2, 0x5e, 0x9f, 0x09, 0x4c, 0x68, 0x11, 0xa3, 0x02, 0xb1, 0x65, 0x42,
    0xef, 0x67, 0x25, 0x30, 0x93, 0x86, 0x6f, 0x37, 0x1c, 0x83, 0x62, 0xd1,
    0x24, 0xfa, 0x89, 0x4d, 0x00, 0x8e, 0x77, 0x6a, 0xfd, 0x79, 0x85, 0x3e,
    0x59, 0xed, 0x92, 0xdf, 0x8a, 0xa1, 0xca, 0xfd, 0xfe, 0x1b, 0xf7, 0x1f,
    0x02, 0x03, 0x01, 0x00, 0x01, 0xa3, 0x81, 0xa9, 0x30, 0x81, 0xa6, 0x30,
    0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0xf4, 0x42,
    0x90, 0xfd, 0x4c, 0xcd, 0x26, 0x10, 0x0b, 0xd7, 0x34, 0x22, 0xad, 0x23,
    0x26, 0xa0, 0x6c, 0xaf, 0xaa, 0x6c, 0x30, 0x7a, 0x06, 0x03, 0x55, 0x1d,
    0x23, 0x04, 0x73, 0x30, 0x71, 0x80, 0x14, 0xdc, 0x16, 0x44, 0x15, 0x3e,
    0x53, 0x27, 0xd8, 0x68, 0x66, 0x41, 0x40, 0x88, 0x90, 0xe4, 0x4e, 0x0a,
    0xda, 0x08, 0xa9, 0xa1, 0x56, 0xa4, 0x54, 0x30, 0x52, 0x31, 0x0b, 0x30,
    0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x1a,
    0x30, 0x18, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x11, 0x63, 0x6f, 0x72,
    0x65, 0x6f, 0x73, 0x2d, 0x63, 0x74, 0x2d, 0x74, 0x65, 0x73, 0x74, 0x20,
    0x43, 0x41, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c,
    0x0a, 0x43, 0x61, 0x6c, 0x69, 0x66, 0x6f, 0x72, 0x6e, 0x69, 0x61, 0x31,
    0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x07, 0x0c, 0x09, 0x43, 0x75,
    0x70, 0x65, 0x72, 0x74, 0x69, 0x6e, 0x6f, 0x82, 0x01, 0x01, 0x30, 0x09,
    0x06, 0x03, 0x55, 0x1d, 0x13, 0x04, 0x02, 0x30, 0x00, 0x30, 0x0d, 0x06,
    0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00,
    0x03, 0x81, 0x81, 0x00, 0x7a, 0x06, 0xe3, 0x17, 0xca, 0xee, 0xe0, 0x67,
    0x16, 0xfd, 0xf1, 0xad, 0x9f, 0xf8, 0xeb, 0xce, 0x03, 0x57, 0x7d, 0x90,
    0x6c, 0x85, 0xe0, 0x43, 0x3f, 0xb4, 0x3a, 0x08, 0x63, 0xef, 0x79, 0xf6,
    0xe1, 0xa3, 0x88, 0x32, 0xcf, 0x8f, 0x2f, 0xde, 0xd0, 0xc0, 0x92, 0x0b,
    0x16, 0xe1, 0xd4, 0x49, 0xd5, 0xb2, 0x84, 0x2e, 0x87, 0xfa, 0x1b, 0x5b,
    0x95, 0x51, 0x51, 0x0d, 0x29, 0x88, 0xd0, 0x8c, 0x10, 0x75, 0xe3, 0x78,
    0xb3, 0x4e, 0x39, 0xc1, 0xe4, 0xd0, 0x22, 0xb7, 0x64, 0xbe, 0xc3, 0x9d,
    0xff, 0x02, 0xc9, 0x66, 0xc3, 0x38, 0x4e, 0x88, 0xde, 0xa6, 0x75, 0x80,
    0xb3, 0x17, 0xb9, 0xfe, 0xfb, 0x64, 0xec, 0x3b, 0x16, 0xcd, 0xf0, 0x0d,
    0x15, 0xbf, 0x70, 0x42, 0xba, 0xe5, 0xec, 0x1d, 0x2f, 0xee, 0x0a, 0x2f,
    0xd7, 0x37, 0x9d, 0xc6, 0x0b, 0x26, 0xf3, 0xfb, 0x13, 0x69, 0x9f, 0x09
};

static
uint8_t server_D_key_data[] = {
    0x30, 0x82, 0x02, 0x5e, 0x02, 0x01, 0x00, 0x02, 0x81, 0x81, 0x00, 0xba,
    0x00, 0xc4, 0xfb, 0x3f, 0x9a, 0x86, 0x43, 0x1a, 0x26, 0x99, 0x9d, 0x19,
    0x67, 0x27, 0xaa, 0x44, 0xd4, 0xba, 0x2b, 0xfe, 0x7b, 0x32, 0xe8, 0x2a,
    0xc7, 0x89, 0x36, 0x41, 0xd7, 0xaf, 0xf4, 0x97, 0x4d, 0x41, 0x7b, 0xc7,
    0x80, 0xba, 0x79, 0xab, 0x9c, 0xeb, 0xcc, 0x38, 0xb7, 0x83, 0xdf, 0x62,
    0x7e, 0xaf, 0x6c, 0x32, 0x57, 0xc2, 0x41, 0xea, 0x73, 0xa9, 0x45, 0xf8,
    0xbe, 0xc2, 0x26, 0x0f, 0x01, 0xec, 0x3b, 0x02, 0x24, 0x7d, 0x39, 0x5c,
    0xa6, 0x9c, 0xdf, 0x4b, 0x1f, 0xd5, 0x4d, 0xd2, 0x5e, 0x9f, 0x09, 0x4c,
    0x68, 0x11, 0xa3, 0x02, 0xb1, 0x65, 0x42, 0xef, 0x67, 0x25, 0x30, 0x93,
    0x86, 0x6f, 0x37, 0x1c, 0x83, 0x62, 0xd1, 0x24, 0xfa, 0x89, 0x4d, 0x00,
    0x8e, 0x77, 0x6a, 0xfd, 0x79, 0x85, 0x3e, 0x59, 0xed, 0x92, 0xdf, 0x8a,
    0xa1, 0xca, 0xfd, 0xfe, 0x1b, 0xf7, 0x1f, 0x02, 0x03, 0x01, 0x00, 0x01,
    0x02, 0x81, 0x80, 0x21, 0x3f, 0xaf, 0xf6, 0x85, 0x99, 0x16, 0xb4, 0xfa,
    0x00, 0xba, 0x66, 0xe5, 0xba, 0x95, 0xd1, 0x8e, 0xfa, 0x43, 0xc9, 0x47,
    0x75, 0x38, 0x55, 0x5e, 0x08, 0x4b, 0x13, 0xc2, 0xd3, 0x4e, 0x65, 0xb7,
    0x82, 0x1c, 0xd9, 0x86, 0x81, 0x11, 0x54, 0x5c, 0x83, 0xf1, 0x76, 0x91,
    0x10, 0xe4, 0xe6, 0xd2, 0x91, 0x78, 0xc4, 0x2b, 0x7f, 0x9a, 0x7e, 0xf3,
    0xec, 0xf6, 0xee, 0x46, 0x17, 0xbb, 0x56, 0x8a, 0x2b, 0x00, 0x0d, 0xd6,
    0xc5, 0x6c, 0x03, 0x73, 0x36, 0xc7, 0x30, 0xd3, 0x79, 0xc7, 0xbd, 0x71,
    0x17, 0x92, 0xde, 0x76, 0x72, 0x0b, 0xb6, 0xc1, 0xfe, 0x78, 0x4f, 0x4e,
    0xac, 0x84, 0x25, 0xce, 0x5d, 0x15, 0x08, 0xb4, 0x3e, 0x26, 0xef, 0x3f,
    0xfe, 0x09, 0x25, 0x31, 0x82, 0xd6, 0x1d, 0xb1, 0xd2, 0x5e, 0x9b, 0x4d,
    0xd6, 0xfc, 0x1d, 0x8e, 0x9f, 0x40, 0xff, 0xc1, 0xba, 0x78, 0xe1, 0x02,
    0x41, 0x00, 0xe6, 0x00, 0xa6, 0x26, 0xdd, 0x88, 0xe6, 0x72, 0x28, 0x1f,
    0xc2, 0xb0, 0x87, 0xef, 0x72, 0x97, 0x32, 0x0c, 0xf2, 0xc0, 0xd9, 0x0c,
    0x2b, 0x06, 0x7d, 0xbe, 0xcc, 0x09, 0x58, 0xa8, 0xe2, 0x5b, 0xaa, 0xcf,
    0xf6, 0xb1, 0x67, 0x5a, 0x8b, 0x98, 0xa4, 0xed, 0x97, 0x51, 0xa7, 0x05,
    0xf7, 0xc1, 0xb1, 0x38, 0xed, 0x4d, 0xce, 0x41, 0xf0, 0x3f, 0x9d, 0x46,
    0x1f, 0x59, 0x39, 0x1a, 0xac, 0xb1, 0x02, 0x41, 0x00, 0xcf, 0x06, 0xf3,
    0x53, 0xd3, 0x12, 0x2f, 0xae, 0x81, 0xd0, 0x3e, 0xd2, 0x3f, 0x3b, 0x3d,
    0xd2, 0x61, 0xe2, 0xf8, 0x4e, 0x74, 0x00, 0xe1, 0xe5, 0x45, 0xa8, 0x88,
    0xcb, 0xff, 0xf1, 0xb4, 0x90, 0xe4, 0xb7, 0x5b, 0xa8, 0xf8, 0xa4, 0x85,
    0x5f, 0xfd, 0x2b, 0xb5, 0xd6, 0xd7, 0x8e, 0x0e, 0xac, 0x56, 0x31, 0x97,
    0xec, 0xc6, 0xf8, 0xaa, 0x6a, 0x05, 0xf2, 0x32, 0x94, 0xdd, 0xf4, 0x94,
    0xcf, 0x02, 0x41, 0x00, 0xd5, 0x55, 0xfc, 0xc7, 0x47, 0xec, 0xd7, 0x73,
    0x43, 0x6c, 0x52, 0x35, 0x53, 0xa0, 0xf1, 0xf4, 0xf3, 0xe3, 0xb6, 0xb6,
    0xd1, 0x9b, 0xcb, 0xbc, 0xb5, 0x9d, 0xe7, 0xbb, 0x33, 0x95, 0x52, 0x80,
    0x1c, 0x2b, 0xd1, 0x72, 0x33, 0x9f, 0x74, 0xa4, 0x1d, 0x36, 0x93, 0x88,
    0x95, 0x17, 0x9f, 0xfa, 0xf4, 0xdb, 0x0c, 0xa1, 0x82, 0x92, 0xfe, 0xb8,
    0xc2, 0xb4, 0x6c, 0x17, 0x62, 0x34, 0x2f, 0xc1, 0x02, 0x41, 0x00, 0x89,
    0xfb, 0x77, 0xf2, 0x46, 0x9b, 0xb8, 0x6b, 0xf6, 0xd9, 0x75, 0x05, 0x6c,
    0x5f, 0x6f, 0xb4, 0xe8, 0xc8, 0xfd, 0xf6, 0x4c, 0x1a, 0xca, 0x74, 0xa5,
    0x18, 0xcf, 0x14, 0x28, 0x62, 0x50, 0x96, 0xc1, 0xd9, 0xf3, 0x9d, 0x8b,
    0x1b, 0x1c, 0x49, 0xfd, 0xd3, 0x44, 0x3f, 0x0d, 0x2c, 0x01, 0x5b, 0x9b,
    0x97, 0x32, 0x4a, 0xfd, 0xd2, 0x7e, 0xc2, 0x6b, 0x74, 0x21, 0x82, 0x56,
    0xec, 0xcc, 0xc1, 0x02, 0x41, 0x00, 0x92, 0x28, 0xeb, 0xba, 0xba, 0x6a,
    0xd5, 0x85, 0x67, 0xfa, 0xc2, 0x2a, 0x28, 0x0a, 0x5a, 0x0c, 0xe3, 0x22,
    0x73, 0xa0, 0xe6, 0xf7, 0xd9, 0x30, 0x23, 0x93, 0xf7, 0x2b, 0x70, 0x70,
    0xd6, 0x8f, 0x4a, 0xf7, 0xc8, 0x0c, 0x07, 0x6c, 0x24, 0x0c, 0x15, 0xc7,
    0x3d, 0x6f, 0xd0, 0xc2, 0x71, 0x0a, 0xc8, 0xa4, 0xe8, 0xd5, 0x5b, 0x41,
    0x86, 0x36, 0x91, 0x8b, 0x1b, 0xfc, 0x7c, 0x10, 0x38, 0x06
};

static tls_buffer_list_t g_server_D_sct_list = {
    .next = NULL,
    .buffer.data = server_D_sct_data,
    .buffer.length = sizeof(server_D_sct_data),
};

static SSLCertificate g_server_D_cert;
static tls_private_key_t g_server_D_key;

static void sct_tests(void)
{
    tls_test_case test = {0,};
    int err;

    uint16_t cipher = TLS_RSA_WITH_AES_128_CBC_SHA;

    tls_cache_empty(g_cache);

    for(int i=0;i<4;i++) {
        test.client_protocol_min = tls_protocol_version_TLS_1_2;
        test.client_protocol_max = tls_protocol_version_TLS_1_2;
        test.server_protocol_min = tls_protocol_version_TLS_1_2;
        test.server_protocol_max = tls_protocol_version_TLS_1_2;
        test.ciphersuites = &cipher;
        test.num_ciphersuites = 1;
        test.server_certs = &g_server_D_cert;
        test.server_key = g_server_D_key;
        test.sct_enable = (i&1);
        test.sct_list = (i&2)?&g_server_D_sct_list:NULL;

        // expected outputs of test case
        test.handshake_ok = true;
        test.client_err = 0;
        test.server_err = 0;
        test.certificate_requested = 0;
        test.negotiated_ciphersuite = cipher;
        test.negotiated_version = tls_protocol_version_TLS_1_2;
        test.allow_resumption = true;
        test.session_id = i + 1;

        test.is_session_resumption_proposed = false; // First one should not propose resumption
        test.is_session_resumed = false; // First one should not be a resumption

        test_log_start();
        test_printf("Test case sct %d\n", i );
        err = test_one_case(&test);
        ok(!err, "Test case sct %d - err=%d", i, err);
        test_log_end(err);

        test.is_session_resumption_proposed = true; // Second one should propose resumption
        test.is_session_resumed = true; // Second one should be a resumption

        test_log_start();
        test_printf("Test case sct %d (resumed)\n", i );
        err = test_one_case(&test);
        ok(!err, "Test case sct %d (resumed) - err=%d", i, err);
        test_log_end(err);
    }
    
}

//
// MARK: negotiated versions tests (including default versions)
//

static void negotiated_version_tests(void)
{
    tls_test_case test = {0,};
    int err;

    uint16_t cipher = TLS_RSA_WITH_AES_128_CBC_SHA;
    int actual_client_min, actual_client_max, actual_server_min, actual_server_max;

    for(int i=0;i<nprotos;i++)
    for(int j=0;j<nprotos;j++)
    for(int k=0;k<nprotos;k++)
    for(int l=0;l<nprotos;l++) {

        test.client_protocol_min = protos[i];
        test.client_protocol_max = protos[j];
        test.server_protocol_min = protos[k];
        test.server_protocol_max = protos[l];
        test.ciphersuites = &cipher;
        test.num_ciphersuites = 1;
        test.server_certs = &g_server_cert;
        test.server_key = g_server_key;
        test.client_certs = &g_client_cert;
        test.client_key = g_client_key;


        /* expected results : */
        actual_client_min = (test.client_protocol_min==0)?tls_protocol_version_TLS_1_0:test.client_protocol_min;
        actual_server_min = (test.server_protocol_min==0)?tls_protocol_version_TLS_1_0:test.server_protocol_min;
        actual_client_max = (test.client_protocol_max==0)?tls_protocol_version_TLS_1_2:test.client_protocol_max;
        actual_server_max = (test.server_protocol_max==0)?tls_protocol_version_TLS_1_2:test.server_protocol_max;

        if(actual_client_min>actual_client_max) actual_client_min = actual_client_max;
        if(actual_server_min>actual_server_max) actual_server_min = actual_server_max;

        if((actual_client_min>actual_server_max) || (actual_client_max<actual_server_min)) {
            test.handshake_ok = false;
        } else {
            test.handshake_ok = true;
        }
        test.client_err = 0;
        test.server_err = 0;
        test.is_session_resumed = false;
        test.certificate_requested = 0;
        test.negotiated_ciphersuite = TLS_RSA_WITH_AES_128_CBC_SHA;
        test.negotiated_version = (actual_client_max>actual_server_max)?actual_server_max:actual_client_max;


        test_log_start();
        test_printf("Test case: negotiated version (%d:%d, %d:%d)\n", i, j, k, l);
        err = test_one_case(&test);
        ok(!err, "Test case: negotiated version (%d:%d, %d:%d)", i, j, k, l);
        test_log_end(err);
    }
}

//
// MARK: version intolerance tests
//

static void version_tolerance_tests(void)
{
    tls_test_case test = {0,};
    int err;

    uint16_t cipher = TLS_RSA_WITH_AES_128_CBC_SHA;

    int tls_client_req_versions[] = {
        0x0304, // TLS 1.3
        0x0305, // TLS 1.4
        0x03ff, // TLS 1.254
        0x0400, // TLS 2.0
        0x0401, // TLS 2.1
        0x0501, // TLS 3.1
        0x1010, // TLS 16.16 ...
    };

    for(int i=0;i<sizeof(tls_client_req_versions)/sizeof(tls_client_req_versions[0]);i++) {
        test.client_protocol_max = tls_client_req_versions[i];

        test.ciphersuites = &cipher;
        test.num_ciphersuites = 1;
        test.server_certs = &g_server_cert;
        test.server_key = g_server_key;
        test.client_certs = &g_client_cert;
        test.client_key = g_client_key;

        test.handshake_ok = true;
        test.client_err = 0;
        test.server_err = 0;
        test.is_session_resumed = false;
        test.certificate_requested = 0;
        test.negotiated_ciphersuite = TLS_RSA_WITH_AES_128_CBC_SHA;
        test.negotiated_version = tls_protocol_version_TLS_1_2;


        test_log_start();
        test_printf("Test case: version intolerance version (%d)\n", i);
        err = test_one_case(&test);
        ok(!err, "Test case: version intolerance (%d)", i);
        test_log_end(err);
    }
}

//
// MARK: DTLS  test
//

static void dtls_tests(void)
{
    tls_test_case test = {0,};
    int err;

    uint16_t cipher = TLS_RSA_WITH_AES_128_CBC_SHA;

    test.dtls = true;
    test.server_certs = &g_server_cert;
    test.server_key = g_server_key;
    test.client_certs = &g_client_cert;
    test.client_key = g_client_key;

    // expected outputs of test case
    test.handshake_ok = true;
    test.negotiated_ciphersuite = TLS_RSA_WITH_AES_128_CBC_SHA;
    test.negotiated_version = tls_protocol_version_DTLS_1_0;

    test.ciphersuites = &cipher;
    test.num_ciphersuites = 1;

    test_log_start();
    test_printf("Test case: dtls\n");
    err = test_one_case(&test);
    ok(!err, "Test case: dtls");
    test_log_end(err);

}

//
// MARK: dtls version intolerance tests
//

static void dtls_version_tolerance_tests(void)
{
    tls_test_case test = {0,};
    int err;

    uint16_t cipher = TLS_RSA_WITH_AES_128_CBC_SHA;

    int client_req_versions[] = {
        0xfefe, // DTLS 1.2
        0xfefd, // DTLS 1.3
        0xfdff, // DTLS 2.0
        0xf0f0, // DTLS 16.16 (?)
    };

    for(int i=0;i<sizeof(client_req_versions)/sizeof(client_req_versions[0]);i++) {
        test.dtls = true;
        test.client_protocol_max = client_req_versions[i];

        test.ciphersuites = &cipher;
        test.num_ciphersuites = 1;
        test.server_certs = &g_server_cert;
        test.server_key = g_server_key;
        test.client_certs = &g_client_cert;
        test.client_key = g_client_key;

        test.handshake_ok = true;
        test.client_err = 0;
        test.server_err = 0;
        test.is_session_resumed = false;
        test.certificate_requested = 0;
        test.negotiated_ciphersuite = TLS_RSA_WITH_AES_128_CBC_SHA;
        test.negotiated_version = tls_protocol_version_DTLS_1_0;


        test_log_start();
        test_printf("Test case: version intolerance version (%d)\n", i);
        err = test_one_case(&test);
        ok(!err, "Test case: version intolerance (%d)", i);
        test_log_end(err);
    }
}

//
// MARK: server_rsa_key_exchange_tests
// 

static void server_rsa_key_exchange_tests(void)
{
    tls_test_case test = {0, };
    int err;

    uint16_t cipher = TLS_RSA_WITH_AES_128_CBC_SHA;

    test.client_protocol_min = tls_protocol_version_TLS_1_2;
    test.client_protocol_max = tls_protocol_version_TLS_1_2;
    test.server_protocol_min = tls_protocol_version_TLS_1_2;
    test.server_protocol_max = tls_protocol_version_TLS_1_2;
    test.ciphersuites = &cipher;
    test.num_ciphersuites = 1;
    test.allow_resumption = false;
    test.session_id = 0;
    test.server_certs = &g_server_cert;
    test.server_key = g_server_key;
    test.client_certs = &g_client_cert;
    test.client_key = g_client_key;
    test.server_rsa_key_exchange = true;

    // expected outputs of test case
    test.handshake_ok = false; // should fail or timeout
    test.client_err = 0;  // unspecified
    test.server_err = 0;  // unspecified
    test.is_session_resumed = false;
    test.certificate_requested = 0;
    test.negotiated_ciphersuite = cipher;
    test.negotiated_version = tls_protocol_version_TLS_1_2;

    test_log_start();
    test_printf("Test case server rsa key exchange\n");
    err = test_one_case(&test);
    ok(!err, "Test case server rsa key exchange - err=%d", err);
    test_log_end(err);
}

//
// MARK: zero_padding_tests
//

static int tls_handshake_fuzz_server_hello(myFilterCtx_t *myCtx, const tls_buffer in, uint8_t content_type)
{

    if((content_type==tls_record_type_Handshake) && (in.data[0]==tls_handshake_message_server_hello)) {
        tls_buffer out;
        int err;

        out.data=malloc(in.length+16);
        out.length=in.length+16;

        if(out.data==NULL) return -1;

        memset(out.data, 0, out.length);
        memcpy(out.data, in.data, in.length);

        err = send_to_peer(myCtx, out, content_type);
        free(out.data);
        return err;
    } else {
        return send_to_peer(myCtx, in, content_type);
    }
}

static void zero_padding_tests(void)
{
    tls_test_case test = {0,};
    int err;
    int i;

    uint16_t cipher = TLS_RSA_WITH_AES_128_CBC_SHA;

    for(i=0; i<1; i++)
    {
        test.client_protocol_min = tls_protocol_version_TLS_1_2;
        test.client_protocol_max = tls_protocol_version_TLS_1_2;
        test.server_protocol_min = tls_protocol_version_TLS_1_2;
        test.server_protocol_max = tls_protocol_version_TLS_1_2;
        test.ciphersuites = &cipher;
        test.num_ciphersuites = 1;
        test.allow_resumption = false;
        test.session_id = 0;
        test.server_certs = &g_server_cert;
        test.server_key = g_server_key;
        test.client_certs = &g_client_cert;
        test.client_key = g_client_key;

        test.fuzzer = tls_handshake_fuzz_server_hello;
        test.fuzz_ctx = i;

        // expected outputs of test case
        test.handshake_ok = true; // should ignore extra HelloRequest messages and succeed
        test.client_err = 0;
        test.server_err = 0;
        test.is_session_resumed = false;
        test.certificate_requested = 0;
        test.negotiated_ciphersuite = cipher;
        test.negotiated_version = tls_protocol_version_TLS_1_2;

        test_log_start();
        test_printf("Test case zero padding (%d)\n", i);
        err = test_one_case(&test);
        ok(!err, "Test case zero padding (%d)", i);
        test_log_end(err);
    }
}

//
// MARK: client_hello_size test
//

static uint8_t client_hello_hostname[512] =
    "0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789ABCDEF"
    "0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789ABCDEF"
    "0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789ABCDEF"
    "0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789ABCDEF";

static void client_hello_size_test(void)
{
    tls_test_case test = {0, };
    int err;

    uint16_t cipher = TLS_RSA_WITH_AES_128_CBC_SHA;

    tls_buffer peer_hostname;

    peer_hostname.data = client_hello_hostname;

    for(int i=1; i<sizeof(client_hello_hostname); i++) {

        test.client_protocol_min = tls_protocol_version_TLS_1_2;
        test.client_protocol_max = tls_protocol_version_TLS_1_2;
        test.server_protocol_min = tls_protocol_version_TLS_1_2;
        test.server_protocol_max = tls_protocol_version_TLS_1_2;
        test.ciphersuites = &cipher;
        test.num_ciphersuites = 1;
        test.allow_resumption = false;
        test.session_id = 0;
        test.server_certs = &g_server_cert;
        test.server_key = g_server_key;
        test.client_certs = &g_client_cert;
        test.client_key = g_client_key;

        peer_hostname.length = i;

        test.peer_hostname = &peer_hostname;

        // expected outputs of test case
        test.handshake_ok = true; // should succeed
        test.client_err = 0;  // unspecified
        test.server_err = 0;  // unspecified
        test.is_session_resumed = false;
        test.certificate_requested = 0;
        test.negotiated_ciphersuite = cipher;
        test.negotiated_version = tls_protocol_version_TLS_1_2;

        test_log_start();
        test_printf("Test case client hello size (%d)\n", i);
        err = test_one_case(&test);
        ok(!err, "Test case client hello size (%d) - err=%d", i, err);
        test_log_end(err);

    }
}

//
// MARK: forget publickey test
//

static void forget_pubkey_test(void)
{
    uint16_t cipher = TLS_DHE_RSA_WITH_AES_128_CBC_SHA;
    tls_protocol_version pv = tls_protocol_version_TLS_1_2;
    tls_test_case test = {0,};
    int err;

    test.client_protocol_min = pv;
    test.client_protocol_max = pv;
    test.server_protocol_min = pv;
    test.server_protocol_max = pv;
    test.ciphersuites = &cipher;
    test.num_ciphersuites = 1;
    test.allow_resumption = false;
    test.session_id = 0;
    test.forget_to_set_pubkey = true;

    // This is the key:
    test.server_certs = &g_server_cert;
    test.server_key = g_server_key;
    test.client_certs = &g_client_cert;
    test.client_key = g_client_key;

    // expected outputs of test case
    test.handshake_ok = false;
    test.client_err = 0;
    test.server_err = -9838;
    test.is_session_resumed = false;
    test.certificate_requested = 0;
    test.negotiated_ciphersuite = cipher;
    test.negotiated_version = pv;

    test_log_start();
    test_printf("Test case forget pubkey\n");
    err = test_one_case(&test);
    ok(!err, "Test case forget pubkey\"");
    test_log_end(err);
}


//
// MARK: DHE tests
//

static uint8_t dh_parameters_512_data[] = {
    0x30, 0x46, 0x02, 0x41, 0x00, 0x85, 0xcd, 0xc1, 0x7e, 0x26, 0xeb, 0x37,
    0x84, 0x13, 0xd0, 0x3b, 0x07, 0xc1, 0x57, 0x7d, 0xf3, 0x55, 0x8d, 0xa0,
    0xc4, 0xa5, 0x03, 0xc4, 0x2c, 0xc6, 0xd5, 0xa6, 0x31, 0xcb, 0x68, 0xdf,
    0x5d, 0x96, 0x20, 0x1a, 0x15, 0x57, 0x49, 0x7d, 0xd7, 0x51, 0x65, 0x6e,
    0x37, 0xa8, 0xe3, 0xe9, 0xe1, 0x59, 0x2e, 0xd4, 0x57, 0x4a, 0xf0, 0xcb,
    0x0e, 0x85, 0x07, 0xdd, 0x35, 0xa7, 0xe3, 0xc6, 0xbb, 0x02, 0x01, 0x02
};

static tls_buffer dh_parameters_512 = {
    .data = dh_parameters_512_data,
    .length = sizeof(dh_parameters_512_data),
};

static uint8_t dh_parameters_768_data[] = {
    0x30, 0x66, 0x02, 0x61, 0x00, 0xe1, 0xa2, 0x50, 0xab, 0xb0, 0xdc, 0xef,
    0xe1, 0x2f, 0xd9, 0xde, 0x59, 0x86, 0x24, 0x43, 0x3b, 0xf3, 0x40, 0x9d,
    0x02, 0xcc, 0xe2, 0x70, 0x63, 0x46, 0x8d, 0x0f, 0xf3, 0x8a, 0xc6, 0xa0,
    0x1d, 0x7b, 0x30, 0x83, 0x10, 0x48, 0x40, 0x28, 0xa4, 0x3e, 0xbe, 0x4d,
    0xb6, 0xea, 0x90, 0x02, 0xae, 0x25, 0x93, 0xc0, 0xe8, 0x36, 0x5c, 0xc8,
    0xc8, 0x0b, 0x04, 0xd5, 0x05, 0xac, 0x67, 0x24, 0x4b, 0xa9, 0x42, 0x5a,
    0x03, 0x65, 0x4d, 0xd0, 0xc0, 0xbd, 0x78, 0x32, 0xd0, 0x8c, 0x0a, 0xf4,
    0xbf, 0xd1, 0x61, 0x86, 0x13, 0x13, 0x3b, 0x83, 0xce, 0xbf, 0x3b, 0xbc,
    0x8f, 0xf9, 0x4e, 0x50, 0xe3, 0x02, 0x01, 0x02
};

static tls_buffer dh_parameters_768 = {
    .data = dh_parameters_768_data,
    .length = sizeof(dh_parameters_768_data),
};

static uint8_t dh_parameters_1024_data[] = {
    0x30, 0x81, 0x87, 0x02, 0x81, 0x81, 0x00, 0xd5, 0x06, 0x69, 0xc6, 0xd4,
    0x98, 0x2b, 0xe3, 0x49, 0xe2, 0xa1, 0x9b, 0x82, 0xaf, 0x3f, 0xaa, 0xc3,
    0x86, 0x2a, 0x7a, 0xfa, 0x62, 0x12, 0x33, 0x45, 0x9f, 0x34, 0x57, 0xc6,
    0x6c, 0x88, 0x81, 0xa6, 0x5d, 0xa3, 0x43, 0xe5, 0x4d, 0x87, 0x4f, 0x69,
    0x3d, 0x2b, 0xc8, 0x18, 0xb6, 0xd7, 0x29, 0x53, 0x94, 0x0d, 0x73, 0x9b,
    0x08, 0x22, 0x73, 0x84, 0x7b, 0x5a, 0x03, 0x2e, 0xfc, 0x10, 0x9b, 0x35,
    0xc6, 0xa1, 0xca, 0x36, 0xd0, 0xcc, 0x3e, 0xa2, 0x04, 0x3a, 0x8a, 0xe8,
    0x87, 0xe8, 0x60, 0x72, 0xee, 0x99, 0xf3, 0x04, 0x0a, 0xd8, 0x1a, 0xe6,
    0xfc, 0xbc, 0xe1, 0xc5, 0x9d, 0x3a, 0xca, 0xf9, 0xfd, 0xbf, 0x58, 0xd3,
    0x4d, 0xde, 0x8b, 0x4a, 0xb5, 0x37, 0x1e, 0x6d, 0xf4, 0x22, 0x0f, 0xb7,
    0x48, 0x0a, 0xda, 0x82, 0x40, 0xc9, 0x55, 0x20, 0x01, 0x3b, 0x35, 0xb2,
    0x94, 0x68, 0xab, 0x02, 0x01, 0x02
};

static tls_buffer dh_parameters_1024 = {
    .data = dh_parameters_1024_data,
    .length = sizeof(dh_parameters_1024_data),
};


static void min_dh_size_test(void)
{
    tls_test_case test = {0, };
    int err;

    uint16_t cipher = TLS_DHE_RSA_WITH_AES_128_CBC_SHA;

    struct {
        tls_buffer *params;
        unsigned bits;
    } dh_params[] = {
        { &dh_parameters_512, 512},
        { &dh_parameters_768, 768},
        { &dh_parameters_1024, 1024},
        { NULL, 2048},
    };
    unsigned n_dh_params = sizeof(dh_params)/sizeof(dh_params[0]);

    unsigned min_dh_size[] = {
        0, // default;
        512,
        768,
        1024,
        2048,
    };
    unsigned n_min_dh_size = sizeof(min_dh_size)/sizeof(min_dh_size[0]);


    for(int i=0; i<n_dh_params; i++) {
    for(int j=0; j<n_min_dh_size; j++) {

        test.ciphersuites = &cipher;
        test.num_ciphersuites = 1;
        test.server_certs = &g_server_cert;
        test.server_key = g_server_key;
        test.client_certs = &g_client_cert;
        test.client_key = g_client_key;

        test.dh_parameters = dh_params[i].params;
        test.min_dh_size = min_dh_size[j];

        // expected outputs of test case

        if(dh_params[i].bits>=((min_dh_size[j]==0)?1024:min_dh_size[j]))
        {
            test.handshake_ok = true;   // should succeed
            test.client_err = 0;        // unspecified
        }
        else
        {
            test.handshake_ok = false;  // should succeed
            test.client_err = -9850;    // unspecified
        }

        test.server_err = 0;  // unspecified
        test.is_session_resumed = false;
        test.certificate_requested = 0;
        test.negotiated_ciphersuite = cipher;
        test.negotiated_version = tls_protocol_version_TLS_1_2;

        test_log_start();
        test_printf("Test case min dh size (%d, %d[%d])\n", i, j, min_dh_size[j]);
        err = test_one_case(&test);
        ok(!err, "Test case min dh size (%d, %d[%d]) - err=%d", i, j, min_dh_size[j], err);
        test_log_end(err);
    }}
}


//
// config tests
//

static void config_tests(void)
{
    tls_test_case test = {0,};
    int err;
    int i;

    for(i=0; i<=tls_handshake_config_TLSv1_3DES_fallback; i++)
    {
        test.client_config = i;
        test.server_config = i;
        test.server_certs = &g_server_cert;
        test.server_key = g_server_key;
        test.client_certs = &g_client_cert;
        test.client_key = g_client_key;

        // expected outputs of test case
        test.handshake_ok = true;

        test_log_start();
        test_printf("Test case config (%d)\n", i);
        err = test_one_case(&test);
        ok(!err, "Test case config (%d)", i);
        test_log_end(err);
    }
}

/* EC Curves server supports */
uint16_t serverCurves[] = {
    tls_curve_secp256r1,
    tls_curve_secp384r1,
    tls_curve_secp521r1
};
static int serverCurvesCount = sizeof(serverCurves)/sizeof(serverCurves[0]);

/* EC Curves client supports */
uint16_t ecCurves1[] = {
    tls_curve_secp256r1,
    tls_curve_secp384r1,
    tls_curve_secp521r1
};
static int n_ecCurves1 = sizeof(ecCurves1)/sizeof(ecCurves1[0]);
uint16_t ecCurves2[] = {
    tls_curve_secp384r1,
    tls_curve_secp521r1
};
static int n_ecCurves2 = sizeof(ecCurves2)/sizeof(ecCurves2[0]);
uint16_t ecCurves3[] = {
    tls_curve_sect193r1
};
static int n_ecCurves3 = sizeof(ecCurves3)/sizeof(ecCurves3[0]);

static void ec_curves_tests(const uint16_t *clientCurves, int clientCurvesCount, int negotiated_ec_curve)
{
    tls_test_case test = {0,};
    int err;

    uint16_t cipher = TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384;

    test.client_protocol_min = tls_protocol_version_TLS_1_2;
    test.client_protocol_max = tls_protocol_version_TLS_1_2;
    test.server_protocol_min = tls_protocol_version_TLS_1_2;
    test.server_protocol_max = tls_protocol_version_TLS_1_2;
    test.ciphersuites = &cipher;
    test.num_ciphersuites = 1;
    test.server_certs = &g_server_ecdsa_cert;
    test.server_key = g_server_ecdsa_key;
    test.client_certs = &g_client_cert;
    test.client_key = g_client_key;
    test.server_ec_curves = serverCurves;
    test.num_server_ec_curves = serverCurvesCount;
    test.client_ec_curves = clientCurves;
    test.num_client_ec_curves = clientCurvesCount;

    // expected outputs of test case
    if (negotiated_ec_curve == tls_curve_none) {
        test.handshake_ok = false;
    } else {
        test.handshake_ok = true;
        test.client_err = 0;
        test.server_err = 0;
        test.certificate_requested = 0;
        test.negotiated_ec_curve = negotiated_ec_curve;
    }

    test_log_start();
    test_printf("Test case: negotiated ec curves \n");
    err = test_one_case(&test);
    ok(!err, "Test case: negotiated ec curves");
    test_log_end(err);
}

//
// MARK: RSA ciphersuite + EC key
//

static void ciphersuite_key_mismatch_test(void)
{
    tls_test_case test = {0,};
    int err;

    uint16_t cipher = TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384;

    test.client_protocol_min = tls_protocol_version_TLS_1_2;
    test.client_protocol_max = tls_protocol_version_TLS_1_2;
    test.server_protocol_min = tls_protocol_version_TLS_1_2;
    test.server_protocol_max = tls_protocol_version_TLS_1_2;
    test.ciphersuites = &cipher;
    test.num_ciphersuites = 1;
    test.server_certs = &g_server_ecdsa_cert;
    test.server_key = g_server_ecdsa_key;
    test.client_certs = &g_client_cert;
    test.client_key = g_client_key;

    test.handshake_ok = false;

    test_log_start();
    test_printf("Test case: ciphersuite_key_mismatch_test\n");
    err = test_one_case(&test);
    ok(!err, "Test case: ciphersuite_key_mismatch_test");
    test_log_end(err);
}

//
// MARK: SigAlgs
//

static tls_signature_and_hash_algorithm sigAlgs[] = {
    {tls_hash_algorithm_SHA1,   tls_signature_algorithm_RSA},
    {tls_hash_algorithm_SHA256, tls_signature_algorithm_RSA},
    {tls_hash_algorithm_SHA384, tls_signature_algorithm_RSA},
    {tls_hash_algorithm_SHA512, tls_signature_algorithm_RSA},
    {tls_hash_algorithm_SHA1,   tls_signature_algorithm_ECDSA},
    {tls_hash_algorithm_SHA256, tls_signature_algorithm_ECDSA},
    {tls_hash_algorithm_SHA384, tls_signature_algorithm_ECDSA},
    {tls_hash_algorithm_SHA512, tls_signature_algorithm_ECDSA},
};
static int n_sigAlgs = sizeof(sigAlgs)/sizeof(sigAlgs[0]);

static void sigalgs_tests(void)
{
    tls_test_case test = {0,};
    int err;
    uint16_t cipher;

    for(int i=0;i<n_sigAlgs;i++)
    {
        switch(sigAlgs[i].signature) {
            case tls_signature_algorithm_RSA:
                cipher = TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384;
                test.server_certs = &g_server_cert;
                test.server_key = g_server_key;
                test.client_certs = &g_client_cert;
                test.client_key = g_client_key;
                break;
            case tls_signature_algorithm_ECDSA:
                cipher = TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384;
                test.server_certs = &g_server_ecdsa_cert;
                test.server_key = g_server_ecdsa_key;
                test.client_certs = &g_server_ecdsa_cert;
                test.client_key = g_server_ecdsa_key;
                break;
            default:
                assert(0);
                break;
        }

        test.client_protocol_min = tls_protocol_version_TLS_1_2;
        test.client_protocol_max = tls_protocol_version_TLS_1_2;
        test.server_protocol_min = tls_protocol_version_TLS_1_2;
        test.server_protocol_max = tls_protocol_version_TLS_1_2;
        test.ciphersuites = &cipher;
        test.num_ciphersuites = 1;
        test.server_sigalgs = &sigAlgs[i];
        test.num_server_sigalgs = 1;
        test.client_sigalgs = &sigAlgs[i];
        test.num_client_sigalgs = 1;
        test.request_client_auth = true;

        // expected outputs of test case
        test.handshake_ok = true;
        test.client_err = 0;
        test.server_err = 0;
        test.certificate_requested = 1;


        test_log_start();
        test_printf("Test case: sigalgs (%d)\n", i);
        err = test_one_case(&test);
        ok(!err, "Test case: siglags (%d)", i);
        test_log_end(err);
    }

}

static void fallback_tests(void)
{
    tls_test_case test = {0,};
    int err;

    uint16_t cipher = SSL_RSA_WITH_RC4_128_SHA;

    for(int i=0;i<nprotos-1;i++)
        for(int j=0;j<nprotos-1;j++) {
            test.client_protocol_min = tls_protocol_version_SSL_3;
            test.client_protocol_max = protos[i];
            test.server_protocol_min = tls_protocol_version_SSL_3;
            test.server_protocol_max = protos[j];
            test.ciphersuites = &cipher;
            test.num_ciphersuites = 1;
            test.server_certs = &g_server_cert;
            test.server_key = g_server_key;
            test.client_certs = &g_client_cert;
            test.client_key = g_client_key;
            test.fallback = true;

            if(test.client_protocol_max < test.server_protocol_max) {
                test.handshake_ok = false;
            } else {
                test.handshake_ok = true;
            }
            test.client_err = 0;
            test.server_err = 0;
            test.is_session_resumed = false;
            test.certificate_requested = 0;
            test.negotiated_ciphersuite = SSL_RSA_WITH_RC4_128_SHA;

            test_log_start();
            test_printf("Test case: fallback (%d, %d)\n", i, j);
            err = test_one_case(&test);
            ok(!err, "Test case: fallback (%d, %d)", i, j);
            test_log_end(err);
        }
}

static void extended_master_secret_tests(void)
{
    tls_test_case test = {0,};
    int err;
    int i, j;

    uint16_t cipher = TLS_RSA_WITH_AES_128_CBC_SHA;

    for (i = 0; i < 4; i++) {
        for(j=1; j<nprotos; j++) {
            test.client_protocol_min = protos[j];
            test.client_protocol_max = protos[j];
            test.server_protocol_min = protos[j];
            test.server_protocol_max = protos[j];
            test.ciphersuites = &cipher;
            test.num_ciphersuites = 1;
            test.allow_resumption = true;
            test.session_id = 0;
            test.server_certs = &g_server_cert;
            test.server_key = g_server_key;
            test.client_certs = &g_client_cert;
            test.client_key = g_client_key;

            test.client_extMS_enable = i&0x1;
            test.server_extMS_enable = i&0x2;

            // expected outputs of test case
            test.handshake_ok = true;
            test.client_err = 0;
            test.server_err = 0;
            test.is_session_resumed = false;
            test.certificate_requested = 0;
            test.negotiated_ciphersuite = cipher;
            test.negotiated_version = protos[j];

            test_log_start();
            test_printf("Test case extended master secret %d (%04x)\n", i, j );
            err = test_one_case(&test);
            ok(!err, "Test case extended master secret %d (%04x)- err=%d", i, j, err);
            test_log_end(err);
        }
    }
}

static void extended_master_secret_resumption_tests(void)
{
    tls_test_case test = {0,};
    int err;
    int i;

    uint16_t cipher = TLS_RSA_WITH_AES_128_CBC_SHA;
    test.client_protocol_min = tls_protocol_version_TLS_1_2;
    test.client_protocol_max = tls_protocol_version_TLS_1_2;
    test.server_protocol_min = tls_protocol_version_TLS_1_2;
    test.server_protocol_max = tls_protocol_version_TLS_1_2;
    test.ciphersuites = &cipher;
    test.num_ciphersuites = 1;
    test.allow_resumption = true;
    test.server_certs = &g_server_cert;
    test.server_key = g_server_key;
    test.client_certs = &g_client_cert;
    test.client_key = g_client_key;

    for (i = 0; i < 2; i++) {
        test.session_id = (cipher<<8 | i) + 1;
        test.server_extMS_enable = true;
        test.client_extMS_enable = i;

        // expected outputs of test case
        test.handshake_ok = true;
        test.client_err = 0;
        test.server_err = 0;
        test.is_session_resumed = false; // First one should not be a resumption
        test.certificate_requested = 0;
        test.negotiated_ciphersuite = cipher;
        if (test.client_extMS_enable)
            test.is_session_resumption_proposed = false;
        else
            test.is_session_resumption_proposed = false;

        test_log_start();
        test_printf("Extended master secret Resumption - 1st connection client: %d server: 1\n", i);
        err = test_one_case(&test);
        ok(!err, "Extended master secret Resumption - 1st connection client: %d server: 1 - err=%d", i, err);
        test_log_end(err);

        test.is_session_resumed = false; // Second one should be a resumption
        test.client_extMS_enable = !i;
        if (test.client_extMS_enable) {
            test.handshake_ok = true;
            test.is_session_resumption_proposed = true;
        } else {
            test.handshake_ok = false;
            test.is_session_resumption_proposed = false;
        }

        test_log_start();
        test_printf("Extended master secret Resumption - 2nd connection client: %d server: 1\n", !i);
        err = test_one_case(&test);
        ok(!err, "Extended master secret Resumption - 2nd connection client: %d server: 1 - err=%d", !i, err);
        test_log_end(err);
    }
    for (i = 0; i < 2; i++) {
        test.session_id = i+1;
        test.client_extMS_enable = true;
        test.server_extMS_enable = i;

        // expected outputs of test case
        test.handshake_ok = true;
        test.client_err = 0;
        test.server_err = 0;
        test.is_session_resumed = false; // First one should not be a resumption
        test.certificate_requested = 0;
        test.negotiated_ciphersuite = cipher;
        if (test.server_extMS_enable)
            test.is_session_resumption_proposed = false;
        else
            test.is_session_resumption_proposed = false;

        test_log_start();
        test_printf("Extended master secret Resumption - 1st connection client: 1 server: %d\n", i);
        err = test_one_case(&test);
        ok(!err, "Extended master secret Resumption - 1st connection client: 1 server: %d - err=%d", i, err);
        test_log_end(err);

        test.is_session_resumed = false; // Second one should be a resumption
        test.server_extMS_enable = !i;
        if (test.server_extMS_enable) {
            test.handshake_ok = true;
            test.is_session_resumption_proposed = true;
        } else {
            test.handshake_ok = false;
            test.is_session_resumption_proposed = false;
        }

        test_log_start();
        test_printf("Extended master secret Resumption - 2nd connection client: 1 server: %d\n", !i);
        err = test_one_case(&test);
        ok(!err, "Extended master secret Resumption - 2nd connection client: 1 server: %d - err=%d", !i, err);
        test_log_end(err);
    }

}

//
// MARK: main
//

int tls_02_self(int argc, char * const argv[])
{
    plan_tests(5 // init_*_keys + init cache
               + n_ssl_ciphers*nprotos*2      // good tests (ssl)
               + n_anon_ciphers*nprotos       // good tests (anon)
               + n_psk_ciphers*(nprotos-1)    // good tests (psk)
               + n_gcm_ciphers*(nprotos-3)*2  // good tests (gcm)
               + n_ecdhe_ciphers*(nprotos-1)*2  // good tests (ecdhe)
               + n_ecanon_ciphers*(nprotos-1) // good tests (ecanon)
               + n_ssl_ciphers*nprotos*2      // resumption tests (ssl)
               + n_anon_ciphers*nprotos*2     // resumption tests (anon)
               + n_psk_ciphers*(nprotos-1)*2  // resumption tests (psk)
               + n_gcm_ciphers*(nprotos-3)*2  // resumption tests (gcm)
               + n_ecdhe_ciphers*(nprotos-1)*2  // resumption tests (ecdhe)
               + n_ecanon_ciphers*(nprotos-1)*2 // resumption tests (ecanon)
               + 4 // ciphersuites test
               + 8 // renegotiation_tests
               + ntrust_values*3 //trust_tests + client_auth_trust_tests
               + nmessages*2     //corrupted_message_len_tests
               + 4 //corrupted_cert_len_tests
               + 1 //goto_fail test
               + 1 //cert_mismatch test
               + 8 // ocsp tests
               + 8 // sct tests
               + nprotos*nprotos*nprotos*nprotos // negotiated_version tests
               + 7 // version tolerance tests
               + 1 // dtls test
               + 4 // dtls version tolerance tests
               + 1 // rsa_server_key_exchange_test
               + 1 // zero_padding_tests
               + sizeof(client_hello_hostname)-1  // client_hello_size_test
               + 1 // forget_pubkey_test
               + 4 // resumption_mismatch_test
               + 20 // dhe size tests
               + 12 // config tests
               + 3 // ec curves test
               + 1 // ciphersuite_key_mismatch_test
               + n_sigAlgs // sigalgs test
               + (nprotos-1)*(nprotos-1) //fallback test
               + 4*(nprotos-1) //extended master secret tests
               + 8
               );

    int err;

    err = init_server_keys(false,
                           Server1_Cert_rsa_rsa_der, Server1_Cert_rsa_rsa_der_len,
                           Server1_Key_rsa_der, Server1_Key_rsa_der_len,
                           &g_server_cert, &g_server_key);

    ok(!err, "init server keys()");
    if(err) return 0;

    err = init_server_keys(false,
                           Server2_Cert_rsa_rsa_der, Server2_Cert_rsa_rsa_der_len,
                           Server2_Key_rsa_der, Server2_Key_rsa_der_len,
                           &g_client_cert, &g_client_key);

    ok(!err, "init client keys()");
    if(err) return 0;

    err = init_server_keys(false,
                           server_D_cert_data, sizeof(server_D_cert_data),
                           server_D_key_data, sizeof(server_D_key_data),
                           &g_server_D_cert, &g_server_D_key);
    ok(!err, "init server D keys()");
    if(err) return 0;


    err = init_server_keys(true,
                           eccert_der, eccert_der_len,
                           eckey_der, eckey_der_len,
                           &g_server_ecdsa_cert, &g_server_ecdsa_key);
    ok(!err, "init server ECDSA keys()");
    if(err) return 0;

    g_cache = tls_cache_create();
    ok(g_cache, "init session cache");
    if(!g_cache) return 0;

    good_tests(ssl_ciphers, n_ssl_ciphers, 0, false);
    good_tests(ssl_ciphers, n_ssl_ciphers, 0, true);
    good_tests(anon_ciphers, n_anon_ciphers, 0, false);
    good_tests(psk_ciphers, n_psk_ciphers, 1, false);
    good_tests(gcm_ciphers, n_gcm_ciphers, 3, false);
    good_tests(gcm_ciphers, n_gcm_ciphers, 3, true);
    good_tests(ecdhe_ciphers, n_ecdhe_ciphers, 1, false);
    good_tests(ecdhe_ciphers, n_ecdhe_ciphers, 1, true);
    good_tests(ecanon_ciphers, n_ecanon_ciphers, 1, false);
    resumption_tests(ssl_ciphers, n_ssl_ciphers, 0);
    resumption_tests(anon_ciphers, n_anon_ciphers, 0);
    resumption_tests(psk_ciphers, n_psk_ciphers, 1);
    resumption_tests(gcm_ciphers, n_gcm_ciphers, 3);
    resumption_tests(ecdhe_ciphers, n_ecdhe_ciphers, 1);
    resumption_tests(ecanon_ciphers, n_ecanon_ciphers, 1);

    ciphersuites_tests();
    renegotiation_tests();
    trust_tests();
    client_auth_trust_tests();
    corrupted_message_len_tests();
    corrupted_cert_len_tests();
    goto_fail_test();
    cert_mismatch_test();
    ocsp_tests();
    sct_tests();
    negotiated_version_tests();
    version_tolerance_tests();
    dtls_tests();
    dtls_version_tolerance_tests();
    server_rsa_key_exchange_tests();
    zero_padding_tests();
    client_hello_size_test();
    forget_pubkey_test();
    resumption_mismatch_tests();
    min_dh_size_test();
    config_tests();
    ec_curves_tests(ecCurves1, n_ecCurves1, tls_curve_secp256r1);
    ec_curves_tests(ecCurves2, n_ecCurves2, tls_curve_secp384r1);
    ec_curves_tests(ecCurves3, n_ecCurves3, tls_curve_none);
    ciphersuite_key_mismatch_test();
    sigalgs_tests();
    fallback_tests();

    extended_master_secret_tests();
    extended_master_secret_resumption_tests();
    clean_server_keys(g_server_key);
    clean_server_keys(g_client_key);
    clean_server_keys(g_server_D_key);
    clean_server_keys(g_server_ecdsa_key);

    tls_cache_destroy(g_cache);

    return 0;
}
