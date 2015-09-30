//
//  handshake_client_test.c
//  libsecurity_ssl
//
//  Created by Fabrice Gautier on 9/9/13.
//
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <AssertMacros.h>


/* extern struct ccrng_state *ccDRBGGetRngState(); */
#include <CommonCrypto/CommonRandomSPI.h>
#define CCRNGSTATE() ccDRBGGetRngState()

int handshake_server_test(void);

#include <tls_handshake.h>
#include <tls_record.h>
#include <tls_stream_parser.h>
#include <tls_ciphersuites.h>

#include <Security/SecCertificate.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecIdentity.h>

#include "appleSession.h"
#include "secCrypto.h"


#define test_printf(x,...)
//#define test_printf printf
#define DEBUG_ONLY __attribute__((unused))


typedef struct {
    // input test case parameters
    int port;
    bool dtls;
    int protocol_min;
    int protocol_max;
    const uint16_t *ciphersuites;
    int num_ciphersuites;
    bool allow_resumption;
    SSLCertificate certs;
    const char *request;
} tls_test_case;


typedef struct {
    int sock;
    tls_record_t rec;
    tls_handshake_t hdsk;
    tls_stream_parser_t parser;
    tls_test_case *test;

    int err;
    int read_ready_received;
    int write_ready_received;
    int certificate_requested;

    dispatch_semaphore_t connection_done;
} myFilterCtx_t;

static
int mySSLAlloc(tls_buffer *buf, size_t len)
{
    buf->data=malloc(len);
    if(!buf->data)
        return errSecAllocate;
    buf->length=len;
    return errSecSuccess;
}

static void mySSLFree(tls_buffer *buf)
{
    if(buf->data)
        free(buf->data);
    buf->data=NULL;
    buf->length=0;
}

static int tls_handshake_write_callback(tls_handshake_ctx_t ctx, const tls_buffer data, uint8_t content_type)
{
    int err;
    myFilterCtx_t *myCtx = (myFilterCtx_t *)ctx;
    tls_buffer encrypted = {0, }, out;
    test_printf("%s: %p (rec.len=%zd)\n", __FUNCTION__, myCtx, data.length);

    err=mySSLAlloc(&encrypted, tls_record_encrypted_size(myCtx->rec, content_type, data.length));
    require_noerr(err, fail);

    err=tls_record_encrypt(myCtx->rec, data, content_type, &encrypted);
    require_noerr(err, fail);

    test_printf("%s: %p Writing %zd encrypted bytes\n", __FUNCTION__, myCtx, encrypted.length);

    out = encrypted;

    while(out.length) {
        ssize_t nwr;
        nwr = send(myCtx->sock, out.data, out.length, 0);
        if(nwr<0) {
            printf("Error writing %zd bytes to socket : %d\n", out.length, (int)nwr);
            err = (int)nwr;
            goto fail;
        }
        //require(nwr>=0, fail);
        out.data += nwr;
        out.length -= nwr;
    }

fail:
    mySSLFree(&encrypted);
    return err;
}


__unused static uint8_t alpn_http_1_1[] =  {0x08, 0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31};
__unused static tls_buffer alpnData = {
    .data = alpn_http_1_1,
    .length = sizeof(alpn_http_1_1),
};

__unused static uint8_t npn_http_1_1[] =  {0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31};
__unused static tls_buffer npnHttpData = {
    .data = npn_http_1_1,
    .length = sizeof(npn_http_1_1),
};




static int
tls_handshake_message_callback(tls_handshake_ctx_t ctx, tls_handshake_message_t event)
{
    myFilterCtx_t *myCtx = (myFilterCtx_t *)ctx;
    int err = 0;

    printf("%s: %p event = %d\n", __FUNCTION__, myCtx, event);

    switch(event) {
        case tls_handshake_message_certificate:
            /* Always says the cert is ok */
            require_noerr((err = tls_handshake_set_peer_trust(myCtx->hdsk, tls_handshake_trust_ok)), errOut);
            const SSLCertificate *cert = tls_handshake_get_peer_certificates(myCtx->hdsk);
            require_noerr((err = tls_set_peer_pubkey(myCtx->hdsk, cert)), errOut);
            break;
        case tls_handshake_message_client_hello:
            /* See if we have NPN: */
#if 0
            {
                const tls_buffer *npnData;
                npnData = tls_handshake_get_peer_npn_data(myCtx->hdsk);
                if(npnData) {
                    printf("NPN Data = %p, %zd\n", npnData->data, npnData->length);
                    require_noerr((err=tls_handshake_set_npn_data(myCtx->hdsk, npnHttpData)), errOut);
                }
                npnData = tls_handshake_get_peer_alpn_data(myCtx->hdsk);
                if(npnData) {
                    printf("ALPN Data = %p, %zd\n", npnData->data, npnData->length);
                }
            }
#endif
            break;
        default:
            break;
    }

errOut:
    return err;
}

/* defined in secCrypto.c */
int tls_set_peer_pubkey(tls_handshake_t hdsk, const SSLCertificate *certchain);


static void
tls_handshake_ready_callback(tls_handshake_ctx_t ctx, bool write, bool ready)
{
    myFilterCtx_t *myCtx = (myFilterCtx_t *)ctx;

    test_printf("%s: %s ready=%d\n", __FUNCTION__, write?"write":"read", ready);

    if(ready) {
        if(write) {
            // Sending an HTTP get request...
            myCtx->write_ready_received++;
        } else {
            myCtx->read_ready_received++;
        }
    }
}

static int
tls_handshake_set_retransmit_timer_callback(tls_handshake_ctx_t ctx, int attempt)
{
    myFilterCtx_t DEBUG_ONLY *myCtx = (myFilterCtx_t *)ctx;

    test_printf("%s: %p attempt=%d\n", __FUNCTION__, myCtx, attempt);

    return errSecUnimplemented;
}

static
int mySSLRecordInitPendingCiphersFunc(tls_handshake_ctx_t ref,
                                      uint16_t            selectedCipher,
                                      bool                server,
                                      tls_buffer           key)
{
    test_printf("%s: %s, cipher=%04x, server=%d\n", __FUNCTION__, ref, selectedCipher, server);
    myFilterCtx_t *c = (myFilterCtx_t *)ref;
    return tls_record_init_pending_ciphers(c->rec, selectedCipher, server, key);
}

static
int mySSLRecordAdvanceWriteCipherFunc(tls_handshake_ctx_t ref)
{
    test_printf("%s: %s\n", __FUNCTION__, ref);
    myFilterCtx_t *c = (myFilterCtx_t *)ref;
    return tls_record_advance_write_cipher(c->rec);
}

static
int mySSLRecordRollbackWriteCipherFunc(tls_handshake_ctx_t ref)
{
    test_printf("%s: %s\n", __FUNCTION__, ref);
    myFilterCtx_t *c = (myFilterCtx_t *)ref;
    return tls_record_rollback_write_cipher(c->rec);
}

static
int mySSLRecordAdvanceReadCipherFunc(tls_handshake_ctx_t ref)
{
    test_printf("%s: %s\n", __FUNCTION__, ref);
    myFilterCtx_t *c = (myFilterCtx_t *)ref;
    return tls_record_advance_read_cipher(c->rec);
}

static
int mySSLRecordSetProtocolVersionFunc(tls_handshake_ctx_t ref,
                                      tls_protocol_version  protocolVersion)
{
    test_printf("%s: %s, pv=%04x\n", __FUNCTION__, ref, protocolVersion);
    myFilterCtx_t *c = (myFilterCtx_t *)ref;
    return tls_record_set_protocol_version(c->rec, protocolVersion);
}


static int
tls_handshake_save_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey, tls_buffer sessionData)
{
    myFilterCtx_t DEBUG_ONLY *myCtx = (myFilterCtx_t *)ctx;

    test_printf("%s:%p\n", __FUNCTION__, myCtx);

    test_printf("key = %s data=[%p,%zd]\n", sessionKey.data, sessionData.data, sessionData.length);

    return sslAddSession(sessionKey, sessionData, 0);
}

static int
tls_handshake_load_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey, tls_buffer *sessionData)
{
    myFilterCtx_t DEBUG_ONLY *myCtx = (myFilterCtx_t *)ctx;

    test_printf("%s:%p\n", __FUNCTION__, myCtx);


    int err = sslGetSession(sessionKey, sessionData);

    test_printf("key = %s data=[%p,%zd], err=%d\n", sessionKey.data, sessionData->data, sessionData->length, err);

    return err;
}

static int
tls_handshake_delete_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey)
{
    myFilterCtx_t DEBUG_ONLY *myCtx = (myFilterCtx_t *)ctx;

    test_printf("%s:%p\n", __FUNCTION__, myCtx);

    return sslDeleteSession(sessionKey);
}

static int
tls_handshake_delete_all_sessions_callback(tls_handshake_ctx_t ctx)
{
    myFilterCtx_t DEBUG_ONLY *myCtx = (myFilterCtx_t *)ctx;

    test_printf("%s:%p\n", __FUNCTION__, myCtx);

    return sslCleanupSession();
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

#include <errno.h>

static int SocketListen(int port)
{
    struct sockaddr_in  sa;
	int					sock;

    if ((sock=socket(AF_INET, SOCK_STREAM, 0))==-1) {
        perror("socket");
        return -errno;
    }

    memset((char *) &sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind (sock, (struct sockaddr *)&sa, sizeof(sa))==-1)
    {
        perror("bind");
        return -errno;
    }

    if(listen(sock, 5)==-1)
    {
        perror("listen");
        return -errno;
    }

    return sock;
}

static
int tls_stream_parser_process(tls_stream_parser_ctx_t ctx, tls_buffer record)
{
    int err = errSecAllocate;
    myFilterCtx_t *c = (myFilterCtx_t *)ctx;
    tls_buffer out;
    uint8_t content_type;

    printf("%s: %p, len = %zu\n", __FUNCTION__, ctx, record.length);
    size_t dlen = tls_record_decrypted_size(c->rec, record.length);

    mySSLAlloc(&out, dlen+1); // 1 extra byte for \0
    require(out.data, fail);

    require_noerr((err=tls_record_decrypt(c->rec, record, &out, &content_type)), fail);

    if(content_type!=tls_record_type_AppData) {
        printf("%s: %p, processing protocol message of type %d, len=%zu\n", __FUNCTION__, ctx, content_type, out.length);
        require_noerr_quiet((err = tls_handshake_process(c->hdsk, out, content_type)), fail);
    } else {
        if(c->read_ready_received<0)
            printf("Received data before read_ready\n");
        printf("%s: %p, received data record, len = %zu\n", __FUNCTION__, ctx, out.length);
        out.data[out.length]=0;
        printf("DATA: %s\n", out.data);
    }

fail:
    mySSLFree(&out);
    c->err = err; // set the last error in context
    return err;
}

static
int init_connection(myFilterCtx_t **pc, int fd, tls_test_case *test)
{
    int err = errSecAllocate;
    myFilterCtx_t *c;

    require((c = malloc(sizeof(myFilterCtx_t))), fail);

    memset(c, 0, sizeof(myFilterCtx_t));
    c->sock = fd;

    struct ccrng_state *rng = CCRNGSTATE();
    if (!rng) {
        abort();
    }

    require((c->rec = tls_record_create(test->dtls, rng)), fail);
    require((c->hdsk = tls_handshake_create(test->dtls, true)), fail);
    require((c->parser = tls_stream_parser_create(c, tls_stream_parser_process)), fail);

    require_noerr((err=tls_handshake_set_callbacks(c->hdsk,
                                                          &tls_handshake_callbacks,
                                                          c)),
                    fail);
    //require_noerr((err=tls_handshake_set_npn_enable(c->hdsk, true)), fail);
    //require_noerr((err=tls_handshake_set_alpn_data(c->hdsk, alpnData)), fail);

    if(test->num_ciphersuites)
        require_noerr((err=tls_handshake_set_ciphersuites(c->hdsk, test->ciphersuites, test->num_ciphersuites)), fail);
    if(test->protocol_min)
        require_noerr((err=tls_handshake_set_min_protocol_version(c->hdsk, test->protocol_min)), fail);
    if(test->protocol_max)
        require_noerr((err=tls_handshake_set_max_protocol_version(c->hdsk, test->protocol_max)), fail);
    require_noerr((err=tls_handshake_set_resumption(c->hdsk,test->allow_resumption)), fail);
    require_noerr((err=tls_handshake_set_identity(c->hdsk, &server_cert, server_key)), fail);

    /* success, return allocated context */
    *pc = c;
    return 0;
fail:
    return err;
}

static
void clean_connection(myFilterCtx_t *c)
{
    if(c->hdsk) tls_handshake_destroy(c->hdsk);
    if(c->rec) tls_record_destroy(c->rec);
    if(c->parser) tls_stream_parser_destroy(c->parser);
    // close(c->sock) -- closed by the cancel ?;
}


static int test_server(tls_test_case *test)
{
    dispatch_queue_t read_queue = NULL;
    dispatch_source_t socket_source  = NULL;

    int server_sock = SocketListen(test->port);
    if(server_sock<0)
        return server_sock;

    printf("Listening to socket: %d\n", server_sock);


    require((read_queue = dispatch_queue_create("server read queue", DISPATCH_QUEUE_SERIAL)), fail);
    require((socket_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t) server_sock, 0, read_queue)), fail);

    dispatch_source_set_cancel_handler(socket_source, ^{
        close(server_sock);
    });

    dispatch_source_set_event_handler(socket_source, ^{
        int err;
        struct sockaddr my_sock;
        socklen_t my_socklen;
        int fd;
        dispatch_source_t fd_source;
        myFilterCtx_t *conn;

        fd = accept(server_sock, &my_sock, &my_socklen);

        require((fd>=0), connect_fail);
        printf("A to socket: %d\n", server_sock);

        fd_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t)fd, 0, read_queue);

        require_noerr((err=init_connection(&conn, fd, test)), connect_fail);

        printf("Created connection, fd=%d, conn=%p\n", fd, conn);


        dispatch_source_set_cancel_handler(fd_source, ^{
            printf("Cancelling connection, fd=%d, conn=%p\n", fd, conn);
            close(fd);
            clean_connection(conn);
        });

        dispatch_source_set_event_handler(fd_source, ^{
            ssize_t nr;
            int err = -1;
            tls_buffer readbuffer = {0,};

            unsigned long data = dispatch_source_get_data(fd_source);

            test_printf("[%d] source event data = %lu\n", fd, data);

            if(data==0) {
                test_printf("EOF? Socket closed ?\n");
                err = -1;
                goto done;
            }

            require_noerr(mySSLAlloc(&readbuffer, data),done);

            nr = recv(fd, readbuffer.data, readbuffer.length, 0);
            require(nr>0, done);

            readbuffer.length = nr;
            printf("recvd %zd bytes, parse it\n", nr);
            require_noerr_quiet((err=tls_stream_parser_parse(conn->parser, readbuffer)), done);

        done:
            test_printf("done, err=%d\n", err);

            mySSLFree(&readbuffer);
            if(err) {
                printf("Error while parsing incoming data, fd=%d, err = %d\n", fd, err);
                printf("Cancelling connection\n");
                dispatch_source_cancel(fd_source);
            }
        });

        dispatch_resume(fd_source);

        return;
connect_fail:
        printf("new connection failed\n");
        return;
    });


    dispatch_resume(socket_source);

    printf("Main server thread is now spinning...\n");
    while(1);

fail:
    if(read_queue)
        dispatch_release(read_queue);

    if(socket_source) {
        dispatch_source_cancel(socket_source);
        dispatch_release(socket_source);
    }

    return 0;
}


uint16_t server_ciphers[] = {
    SSL_RSA_WITH_NULL_MD5,
    SSL_RSA_WITH_NULL_SHA,
    TLS_RSA_WITH_NULL_SHA256,

    SSL_RSA_WITH_RC4_128_MD5,
    SSL_RSA_WITH_RC4_128_SHA,
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_RSA_WITH_AES_128_CBC_SHA,
    TLS_RSA_WITH_AES_128_CBC_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA,
    TLS_RSA_WITH_AES_256_CBC_SHA256,

    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,

    SSL_DH_anon_WITH_RC4_128_MD5,
    SSL_DH_anon_WITH_3DES_EDE_CBC_SHA,
    TLS_DH_anon_WITH_AES_128_CBC_SHA,
    TLS_DH_anon_WITH_AES_128_CBC_SHA256,
    TLS_DH_anon_WITH_AES_256_CBC_SHA,
    TLS_DH_anon_WITH_AES_256_CBC_SHA256,

    /* ECDHE_RSA cipher suites */
    TLS_ECDHE_RSA_WITH_RC4_128_SHA,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
};

int num_server_ciphers = sizeof(server_ciphers)/sizeof(server_ciphers[0]);

int handshake_server_test(void);
int handshake_server_test(void)
{
    int err;

    require_noerr((err=init_server_keys()), fail);

    tls_test_case test0;
    memset(&test0, 0, sizeof(test0));

    test0.port=10443;
    test0.num_ciphersuites = num_server_ciphers;
    test0.ciphersuites = server_ciphers;


    printf("***** Testing case: test0 -- %d\n", test0.port);

    err = test_server(&test0);

    printf("***** Tested case: test0 -- %d -- err =%d\n", test0.port, err);

fail:
    clean_server_keys();
    return err;
}


