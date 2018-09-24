//
//  tls_03_client.c
//  coretls
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

#include <tls_handshake.h>
#include <tls_record.h>
#include <tls_stream_parser.h>
#include <tls_ciphersuites.h>
#include <Security/SecCertificate.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecIdentity.h>
#include <Security/CipherSuite.h>
#include "appleSession.h"
#include "secCrypto.h"

#include "tls_regressions.h"
#include "tls_helpers.h"

#define DEBUG_ONLY __attribute__((unused))

static SSLCertificate g_cert;
static tls_private_key_t g_key;

typedef struct {
    // input test case parameters
    const char *hostname;
    int port;
    bool dtls;
    int protocol_min;
    int protocol_max;
    const uint16_t *ciphersuites;
    int num_ciphersuites;
    bool allow_resumption;
    uintptr_t session_id;
    const char *request;
    bool isECKey;

    // expected outputs of test case
    int err;
    bool is_session_resume;
    int read_ready_received;
    int write_ready_received;
    int certificate_requested;
    uint16_t negotiated_ciphersuite;
    tls_protocol_version negotiated_version;
    bool received_cert;
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

    dispatch_semaphore_t test_done;
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


static uint8_t alpn_http_1_1[] =  {0x08, 0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31};
__unused static tls_buffer alpnData = {
    .data = alpn_http_1_1,
    .length = sizeof(alpn_http_1_1),
};

static uint8_t npn_http_1_1[] =  {0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31};
static tls_buffer npnHttpData = {
    .data = npn_http_1_1,
    .length = sizeof(npn_http_1_1),
};




static int
tls_handshake_message_callback(tls_handshake_ctx_t ctx, tls_handshake_message_t event)
{
    myFilterCtx_t *myCtx = (myFilterCtx_t *)ctx;
    SecTrustRef trustRef = NULL;
    int err = 0;

    test_printf("%s: %p event = %d\n", __FUNCTION__, myCtx, event);

    switch(event) {
        case tls_handshake_message_certificate:
            require_noerr((err = tls_helper_set_peer_pubkey(myCtx->hdsk)), errOut);
            break;
        case tls_handshake_message_certificate_request:
            myCtx->certificate_requested++;
            if (myCtx->test->isECKey == true) {
                require_noerr((err = tls_handshake_set_client_auth_type(myCtx->hdsk, tls_client_auth_type_ECDSASign)), errOut);
            } else {
                require_noerr((err = tls_handshake_set_client_auth_type(myCtx->hdsk, tls_client_auth_type_RSASign)), errOut);
            }
            require_noerr((err = tls_handshake_set_identity(myCtx->hdsk, &g_cert, g_key)), errOut);
            break;
        case tls_handshake_message_server_hello:
            /* See if we have NPN: */
            {
                const tls_buffer *npnData;
                npnData = tls_handshake_get_peer_npn_data(myCtx->hdsk);
                if(npnData) {
                    test_printf("NPN Data = %p, %zd\n", npnData->data, npnData->length);
                    require_noerr((err=tls_handshake_set_npn_data(myCtx->hdsk, npnHttpData)), errOut);
                }
                npnData = tls_handshake_get_peer_alpn_data(myCtx->hdsk);
                if(npnData) {
                    test_printf("ALPN Data = %p, %zd\n", npnData->data, npnData->length);
                }
            }
            break;
        case tls_handshake_message_server_hello_done:
            /* Always says the cert is ok */
            require_noerr((err = tls_handshake_set_peer_trust(myCtx->hdsk, tls_handshake_trust_ok)), errOut);
            break;
        default:
            break;
    }

errOut:
    CFReleaseNull(trustRef);
    return err;
}

static void
tls_handshake_ready_callback(tls_handshake_ctx_t ctx, bool write, bool ready)
{
    myFilterCtx_t *myCtx = (myFilterCtx_t *)ctx;

    test_printf("%s: %s ready=%d\n", __FUNCTION__, write?"write":"read", ready);

    if(ready) {
        // Sending an HTTP get request...

        if(write) {
            myCtx->write_ready_received++;

            if(myCtx->write_ready_received==1) {

                test_printf("First handshake done, sending request\n");

                if(myCtx->test->request) {
                    tls_buffer inData;

                    inData.data = (uint8_t *)myCtx->test->request;
                    inData.length = strlen(myCtx->test->request);

                    tls_handshake_write_callback(ctx, inData, tls_record_type_AppData);
                }
            }
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
    return 0;
}

static int
tls_handshake_load_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey, tls_buffer *sessionData)
{
    return errSSLSessionNotFound;
}

static int
tls_handshake_delete_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey)
{
    return 0;
}

static int
tls_handshake_delete_all_sessions_callback(tls_handshake_ctx_t ctx)
{
    myFilterCtx_t DEBUG_ONLY *myCtx = (myFilterCtx_t *)ctx;

    test_printf("%s:%p\n", __FUNCTION__, myCtx);

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



static int SocketConnect(const char *hostName, int port)
{
    struct sockaddr_in  addr;
    struct in_addr      host;
	int					sock;
    int                 err;
    struct hostent      *ent = NULL;

    if (hostName[0] >= '0' && hostName[0] <= '9')
    {
        host.s_addr = inet_addr(hostName);
    }
    else {
		unsigned dex;
#define GETHOST_RETRIES 5
		/* seeing a lot of soft failures here that I really don't want to track down */
		for(dex=0; dex<GETHOST_RETRIES; dex++) {
			if(dex != 0) {
				printf("\n...retrying gethostbyname(%s)", hostName);
			}
			ent = gethostbyname(hostName);
			if(ent != NULL) {
				break;
			}
		}
        if(ent == NULL) {
			printf("\n***gethostbyname(%s) returned: %s\n", hostName, hstrerror(h_errno));
            return -1;
        }
        memcpy(&host, ent->h_addr, sizeof(struct in_addr));
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_addr = host;
    addr.sin_port = htons((u_short)port);

    addr.sin_family = AF_INET;
    err = connect(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));

    if(err!=0)
    {
        perror("connect failed");
        return err;
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

    test_printf("%s: %p, len = %zu\n", __FUNCTION__, ctx, record.length);
    size_t dlen = tls_record_decrypted_size(c->rec, record.length);

    mySSLAlloc(&out, dlen+1); // 1 extra byte for \0
    require(out.data, fail);

    require_noerr((err=tls_record_decrypt(c->rec, record, &out, &content_type)), fail);

    if(content_type!=tls_record_type_AppData) {
        test_printf("%s: %p, processing protocol message of type %d, len=%zu\n", __FUNCTION__, ctx, content_type, out.length);
        require_noerr_quiet((err = tls_handshake_process(c->hdsk, out, content_type)), fail);
    } else {
        if(c->read_ready_received<0)
            printf("Received data before read_ready\n");
        test_printf("%s: %p, received data record, len = %zu\n", __FUNCTION__, ctx, out.length);
        out.data[out.length]=0;
        //printf("DATA: %s\n", out.data);
    }

fail:
    mySSLFree(&out);
    c->err = err; // set the last error in context
    return err;
}

static
int init_context(myFilterCtx_t *c, tls_test_case *test)
{
    int err = errSecAllocate;
    memset(c, 0, sizeof(myFilterCtx_t));

    c->test = test;

    require((c->sock = SocketConnect(test->hostname, test->port))>=0, fail);

    struct ccrng_state *rng = CCRNGSTATE();
    if (!rng) {
        abort();
    }

    require((c->rec = tls_record_create(test->dtls, rng)), fail);
    require((c->hdsk = tls_handshake_create(test->dtls, false)), fail);
    require((c->parser = tls_stream_parser_create(c, tls_stream_parser_process)), fail);
    require((c->test_done=dispatch_semaphore_create(0)), fail);

    require_noerr((err=tls_handshake_set_callbacks(c->hdsk,
                                                          &tls_handshake_callbacks,
                                                          c)),
                    fail);
    require_noerr((err=tls_handshake_set_false_start(c->hdsk, true)), fail);
    require_noerr((err=tls_handshake_set_peer_hostname(c->hdsk, test->hostname, strlen(test->hostname))), fail);
    require_noerr((err=tls_handshake_set_npn_enable(c->hdsk, true)), fail);
    //require_noerr((err=tls_handshake_set_alpn_data(c->hdsk, alpnData)), fail);

    if(test->ciphersuites)
        require_noerr((err=tls_handshake_set_ciphersuites(c->hdsk, test->ciphersuites, test->num_ciphersuites)), fail);
    if(test->protocol_min)
        require_noerr((err=tls_handshake_set_min_protocol_version(c->hdsk, test->protocol_min)), fail);
    if(test->protocol_max)
        require_noerr((err=tls_handshake_set_max_protocol_version(c->hdsk, test->protocol_max)), fail);
    require_noerr((err=tls_handshake_set_resumption(c->hdsk,test->allow_resumption)), fail);

fail:
    return err;
}

static
void clean_context(myFilterCtx_t *c)
{
    if(c->hdsk) tls_handshake_destroy(c->hdsk);
    if(c->rec) tls_record_destroy(c->rec);
    if(c->parser) tls_stream_parser_destroy(c->parser);
    if(c->test_done) dispatch_release(c->test_done);
}


static int test_result(myFilterCtx_t *client)
{
    int err = 0;


    if(client->test->err) {
        if(client->err!=client->test->err) {
            printf("err: %d, expected %d\n", client->err, client->test->err);
            err = client->err?client->err:-1;
        }
    } else {
        if(client->read_ready_received!=client->test->read_ready_received) {
            printf("read_ready received: %d, expected %d\n",client->read_ready_received,client->test->read_ready_received);
            err = -1;
        }
        if(client->write_ready_received!=client->test->write_ready_received) {
            printf("write_ready received: %d, expected %d\n",client->write_ready_received,client->test->write_ready_received);
            err = -1;
        }

        uint16_t negotiated_ciphersuite = tls_handshake_get_negotiated_cipherspec(client->hdsk);
        test_printf("negotiated ciphersuite: %04x\n", negotiated_ciphersuite);
        if(client->test->negotiated_ciphersuite && (negotiated_ciphersuite!=client->test->negotiated_ciphersuite)) {
            printf("ciphersuite negotiated: %04x, expected %04x\n",negotiated_ciphersuite,client->test->negotiated_ciphersuite);
            err = -1;
        }

        if(client->test->certificate_requested!=client->certificate_requested) {
            printf("certificate requested: %d, expected %d\n", client->certificate_requested, client->test->certificate_requested);
            err = -1;
        }

        if(client->err!=0 && client->err!=-9805) {  //errSSLClosedGraceful is an ok error to get in normal case
            printf("err: %d\n", client->err);
            err = client->err;
        }

        if(err)
            printf("ciphersuite = %04x\n", negotiated_ciphersuite);

    }

    return err;
}

static int test_one_case(tls_test_case *test)
{
    int err;
    myFilterCtx_t client;
    dispatch_queue_t read_queue = NULL;
    dispatch_source_t socket_source  = NULL;

    require_noerr((err=init_context(&client, test)), out);

    dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 600LL*NSEC_PER_SEC); // 600 seconds

    read_queue = dispatch_queue_create("socket read queue", DISPATCH_QUEUE_SERIAL);
    socket_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t) client.sock, 0, read_queue);

    dispatch_source_set_cancel_handler(socket_source, ^{
        close(client.sock);
        dispatch_semaphore_signal(client.test_done);
    });

    dispatch_source_set_event_handler(socket_source, ^{
        ssize_t nr;
        int err = -1;
        tls_buffer readbuffer = {0,};

        unsigned long data = dispatch_source_get_data(socket_source);

        test_printf("source event data = %lu\n", data);

        if(data==0) {
            test_printf("EOF? Socket closed ?\n");
            err = -1;
            goto done;
        }

        require_noerr(mySSLAlloc(&readbuffer, data),done);

        nr = recv(client.sock, readbuffer.data, readbuffer.length, 0);
        require(nr>0, done);

        readbuffer.length = nr;
        test_printf("recvd %zd bytes, parse it\n", nr);

        require_noerr_quiet((err=tls_stream_parser_parse(client.parser, readbuffer)), done);

    done:
        test_printf("done, err=%d\n", err);

        mySSLFree(&readbuffer);
        if(err) {
            tls_handshake_close(client.hdsk);
            dispatch_source_cancel(socket_source);
        }
    });

    dispatch_resume(socket_source);

    tls_buffer peerID = {
        .data = (uint8_t *)&test->session_id,
        .length = sizeof(test->session_id),
    };

    // Start handshake...
    if(test->session_id)
        err = tls_handshake_negotiate(client.hdsk, &peerID);
    else
        err = tls_handshake_negotiate(client.hdsk, NULL);

    require_noerr(err, out);

    // Wait for test done.
    if(dispatch_semaphore_wait(client.test_done,  timeout)) {
        printf("Timeout while waiting for test done close, closing now\n");
    } else {
        test_printf("Test is done\n");
    }

    err = test_result(&client);


out:
    if(read_queue)
        dispatch_release(read_queue);

    if(socket_source) {
        dispatch_release(socket_source);
    }

    clean_context(&client);

    return err;
}


/* List of server and ciphers to test */

typedef struct _CipherSuiteName {
  uint16_t cipher;
  const char *name;
} CipherSuiteName;


/* prot: 0 = SSL3, 1=TLSv1.0, 2=TLSv1.1, 3=TLSv1.2 */
#define CIPHER(cipher) { cipher, #cipher}

static const CipherSuiteName ciphers[] = {
  //CIPHER(SSL_NULL_WITH_NULL_NULL), unsupported


#if 1
  /* RSA cipher suites */
  CIPHER(SSL_RSA_WITH_NULL_MD5),
  CIPHER(SSL_RSA_WITH_NULL_SHA),
  CIPHER(TLS_RSA_WITH_NULL_SHA256),
#endif


#if 1
  CIPHER(SSL_RSA_WITH_3DES_EDE_CBC_SHA),
  CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA),
  CIPHER(TLS_RSA_WITH_AES_128_CBC_SHA256),
  CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA),
  CIPHER(TLS_RSA_WITH_AES_256_CBC_SHA256),
#endif

#if 1
  /* DHE_RSA ciphers suites */
  CIPHER(SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA),
  CIPHER(TLS_DHE_RSA_WITH_AES_128_CBC_SHA),
  CIPHER(TLS_DHE_RSA_WITH_AES_128_CBC_SHA256),
  CIPHER(TLS_DHE_RSA_WITH_AES_256_CBC_SHA),
  CIPHER(TLS_DHE_RSA_WITH_AES_256_CBC_SHA256),
#endif


#if 1
  /* DH_anon cipher suites */
  CIPHER(SSL_DH_anon_WITH_3DES_EDE_CBC_SHA),
  CIPHER(TLS_DH_anon_WITH_AES_128_CBC_SHA),
  CIPHER(TLS_DH_anon_WITH_AES_128_CBC_SHA256),
  CIPHER(TLS_DH_anon_WITH_AES_256_CBC_SHA),
  CIPHER(TLS_DH_anon_WITH_AES_256_CBC_SHA256),
#endif

#if 1
  /* ECDHE_ECDSA cipher suites */
  CIPHER(TLS_ECDHE_ECDSA_WITH_NULL_SHA),
  CIPHER(TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA),
  CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA),
  CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256),
  CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA),
  CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384),
#endif

#if 1
  /* ECDHE_RSA cipher suites */
  CIPHER(TLS_ECDHE_RSA_WITH_NULL_SHA),
  CIPHER(TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA),
  CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA),
  CIPHER(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256),
  CIPHER(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA),
  CIPHER(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384), // Not supported by either gnutls or openssl
#endif

#if 1
  /* ECDH_anon cipher suites */
  CIPHER(TLS_ECDH_anon_WITH_NULL_SHA),
  CIPHER(TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA),
  CIPHER(TLS_ECDH_anon_WITH_AES_128_CBC_SHA),
  CIPHER(TLS_ECDH_anon_WITH_AES_256_CBC_SHA),
#endif

#if 0
  CIPHER(TLS_PSK_WITH_3DES_EDE_CBC_SHA),
  CIPHER(TLS_PSK_WITH_AES_128_CBC_SHA),
  CIPHER(TLS_PSK_WITH_AES_256_CBC_SHA),
  CIPHER(TLS_PSK_WITH_AES_128_CBC_SHA256),
  CIPHER(TLS_PSK_WITH_AES_256_CBC_SHA384),
  CIPHER(TLS_PSK_WITH_NULL_SHA),
  CIPHER(TLS_PSK_WITH_NULL_SHA256),
  CIPHER(TLS_PSK_WITH_NULL_SHA384),
#endif

#if 1
  CIPHER(TLS_RSA_WITH_AES_128_GCM_SHA256),
  CIPHER(TLS_RSA_WITH_AES_256_GCM_SHA384),

  CIPHER(TLS_DHE_RSA_WITH_AES_128_GCM_SHA256),
  CIPHER(TLS_DHE_RSA_WITH_AES_256_GCM_SHA384),

  CIPHER(TLS_DH_anon_WITH_AES_128_GCM_SHA256),
  CIPHER(TLS_DH_anon_WITH_AES_256_GCM_SHA384),

  CIPHER(TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256),
  CIPHER(TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384),

  CIPHER(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256),
  CIPHER(TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384),
#endif
};
static int nciphers = sizeof(ciphers)/sizeof(ciphers[0]);


tls_protocol_version protos[] = {
    tls_protocol_version_SSL_3,
    tls_protocol_version_TLS_1_0,
    tls_protocol_version_TLS_1_1,
    tls_protocol_version_TLS_1_2
};
int nprotos = sizeof(protos)/sizeof(protos[0]);


static bool ciphersuite_in_array(uint16_t ciphersuite, uint16_t *array, int n)
{
    int i;

    for(i=0;i<n;i++)
    {
        if(array[i]==ciphersuite)
            return true;
    }
    return false;
}

#define CIPHERSUITE_IN_ARRAY(_c_, _array_) ciphersuite_in_array((_c_), (_array_), sizeof(_array_)/sizeof((_array_[0])))

uint16_t mikes_supported_ciphers[] =
{
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
    TLS_RSA_WITH_AES_128_GCM_SHA256,
    TLS_RSA_WITH_AES_256_GCM_SHA384,
    TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_DHE_RSA_WITH_AES_256_GCM_SHA384

};
__unused
static bool mikes_is_cipher_supported(uint16_t ciphersuite)
{
    return CIPHERSUITE_IN_ARRAY(ciphersuite, mikes_supported_ciphers);
}

#if 0 // secg shutdown for repair in July 2014.
static bool secg_is_cipher_supported(uint16_t ciphersuite)
{
    HMAC_Algs mac = sslCipherSuiteGetMacAlgorithm(ciphersuite);
    int kem = sslCipherSuiteGetKeyExchangeMethod(ciphersuite);

    if(((mac == HA_SHA256 || mac == HA_SHA384)) &&
        ((kem == SSL_RSA) || (kem == SSL_DHE_RSA) || (kem == SSL_ECDHE_RSA)))
        return false;

    return true;
}
#endif

uint16_t openssl_supported_ciphers[] =
{
    SSL_RSA_WITH_NULL_MD5,
    SSL_RSA_WITH_NULL_SHA,
    TLS_RSA_WITH_NULL_SHA256,

    SSL_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_RSA_WITH_AES_128_CBC_SHA,
    TLS_RSA_WITH_AES_128_CBC_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA,
    TLS_RSA_WITH_AES_256_CBC_SHA256,

    /* DHE_RSA ciphers suites */
    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,

    /* DH_anon cipher suites */
    SSL_DH_anon_WITH_3DES_EDE_CBC_SHA,
    TLS_DH_anon_WITH_AES_128_CBC_SHA,
    TLS_DH_anon_WITH_AES_128_CBC_SHA256,
    TLS_DH_anon_WITH_AES_256_CBC_SHA,
    TLS_DH_anon_WITH_AES_256_CBC_SHA256,

    /* ECDHE_ECDSA cipher suites */
    TLS_ECDHE_ECDSA_WITH_NULL_SHA,
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,

    /* ECDHE_RSA cipher suites */
    TLS_ECDHE_RSA_WITH_NULL_SHA,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,

    /* ECDH_anon cipher suites */
    TLS_ECDH_anon_WITH_NULL_SHA,
    TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDH_anon_WITH_AES_128_CBC_SHA,
    TLS_ECDH_anon_WITH_AES_256_CBC_SHA,

    TLS_PSK_WITH_3DES_EDE_CBC_SHA,
    TLS_PSK_WITH_AES_128_CBC_SHA,
    TLS_PSK_WITH_AES_256_CBC_SHA,

    TLS_RSA_WITH_AES_128_GCM_SHA256,
    TLS_RSA_WITH_AES_256_GCM_SHA384,

    TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,

    TLS_DH_anon_WITH_AES_128_GCM_SHA256,
    TLS_DH_anon_WITH_AES_256_GCM_SHA384,

    TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256,

    TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384,

    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,

    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
};

static __unused bool openssl_rsa_rsa_is_cipher_supported(uint16_t ciphersuite)
{
    int kem = sslCipherSuiteGetKeyExchangeMethod(ciphersuite);
    if( (kem == SSL_ECDH_RSA) || (kem == SSL_ECDHE_ECDSA) || (kem == SSL_ECDH_ECDSA))
        return false;

    return CIPHERSUITE_IN_ARRAY(ciphersuite, openssl_supported_ciphers);
}

static __unused bool openssl_rsa_ecc_is_cipher_supported(uint16_t ciphersuite)
{
    int kem = sslCipherSuiteGetKeyExchangeMethod(ciphersuite);
    if( (kem == SSL_ECDH_RSA) || (kem == SSL_ECDHE_ECDSA) || (kem == SSL_ECDH_ECDSA))
        return false;

    return CIPHERSUITE_IN_ARRAY(ciphersuite, openssl_supported_ciphers);
}

static __unused bool openssl_ecc_rsa_is_cipher_supported(uint16_t ciphersuite)
{
    int kem = sslCipherSuiteGetKeyExchangeMethod(ciphersuite);
    if( (kem == SSL_RSA) || (kem == SSL_DHE_RSA) || (kem == SSL_ECDHE_RSA))
        return false;

    if(kem == SSL_ECDH_ECDSA)
        return false;

    return CIPHERSUITE_IN_ARRAY(ciphersuite, openssl_supported_ciphers);
}


static __unused bool openssl_ecc_ecc_is_cipher_supported(uint16_t ciphersuite)
{
    int kem = sslCipherSuiteGetKeyExchangeMethod(ciphersuite);
    if( (kem == SSL_RSA) || (kem == SSL_DHE_RSA) || (kem == SSL_ECDHE_RSA))
        return false;

    if(kem == SSL_ECDH_RSA)
        return false;

    return CIPHERSUITE_IN_ARRAY(ciphersuite, openssl_supported_ciphers);
}


uint16_t gnutls_supported_ciphers[] = {
    SSL_RSA_WITH_NULL_MD5,
    SSL_RSA_WITH_NULL_SHA,
    TLS_RSA_WITH_NULL_SHA256,

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

    SSL_DH_anon_WITH_3DES_EDE_CBC_SHA,
    TLS_DH_anon_WITH_AES_128_CBC_SHA,
    TLS_DH_anon_WITH_AES_128_CBC_SHA256,
    TLS_DH_anon_WITH_AES_256_CBC_SHA,
    TLS_DH_anon_WITH_AES_256_CBC_SHA256,

    TLS_ECDHE_ECDSA_WITH_NULL_SHA,
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,

    TLS_ECDHE_RSA_WITH_NULL_SHA,
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,

    TLS_ECDH_anon_WITH_NULL_SHA,
    TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA,
    TLS_ECDH_anon_WITH_AES_128_CBC_SHA,
    TLS_ECDH_anon_WITH_AES_256_CBC_SHA,

    TLS_PSK_WITH_3DES_EDE_CBC_SHA,
    TLS_PSK_WITH_AES_128_CBC_SHA,
    TLS_PSK_WITH_AES_256_CBC_SHA,
    TLS_PSK_WITH_AES_128_CBC_SHA256,
    TLS_PSK_WITH_NULL_SHA256,

    TLS_RSA_WITH_AES_128_GCM_SHA256,
    TLS_RSA_WITH_AES_256_GCM_SHA384,
    TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_DH_anon_WITH_AES_128_GCM_SHA256,
    TLS_DH_anon_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,


};

static __unused bool gnutls_rsa_rsa_is_cipher_supported(uint16_t ciphersuite)
{
    int kem = sslCipherSuiteGetKeyExchangeMethod(ciphersuite);
    if( (kem == SSL_ECDH_RSA) || (kem == SSL_ECDHE_ECDSA) || (kem == SSL_ECDH_ECDSA))
        return false;

    return CIPHERSUITE_IN_ARRAY(ciphersuite, gnutls_supported_ciphers);
}

static __unused bool gnutls_rsa_ecc_is_cipher_supported(uint16_t ciphersuite)
{
    int kem = sslCipherSuiteGetKeyExchangeMethod(ciphersuite);
    if( (kem == SSL_ECDH_RSA) || (kem == SSL_ECDHE_ECDSA) || (kem == SSL_ECDH_ECDSA))
        return false;

    return CIPHERSUITE_IN_ARRAY(ciphersuite, gnutls_supported_ciphers);
}

static __unused bool gnutls_ecc_rsa_is_cipher_supported(uint16_t ciphersuite)
{
    int kem = sslCipherSuiteGetKeyExchangeMethod(ciphersuite);
    if( (kem == SSL_RSA) || (kem == SSL_DHE_RSA) || (kem == SSL_ECDHE_RSA))
        return false;

    return CIPHERSUITE_IN_ARRAY(ciphersuite, gnutls_supported_ciphers);
}


static bool __unused gnutls_ecc_ecc_is_cipher_supported(uint16_t ciphersuite)
{
    int kem = sslCipherSuiteGetKeyExchangeMethod(ciphersuite);
    if( (kem == SSL_RSA) || (kem == SSL_DHE_RSA) || (kem == SSL_ECDHE_RSA))
        return false;

    return CIPHERSUITE_IN_ARRAY(ciphersuite, gnutls_supported_ciphers);
}


static bool client_auth_never(tls_protocol_version v)
{
    return false;
}

static bool __unused client_auth_always(tls_protocol_version v)
{
    return true;
}

/* For mike's server that disable client auth based on Server Name Indication extension.
   There are no extension in SSL3.0, so this server will always ask for client auth in SSL 3.0 */
__unused
static bool client_auth_ssl3_only(tls_protocol_version v)
{
    return v==tls_protocol_version_SSL_3;
}

/* For gnutls servers with EC certs, they will only support DH_anon ciphersuites in SSL 3
   OpenSSL server will happily use EC ciphersuites with SSL 3 */
static bool __unused client_auth_unless_ssl3(tls_protocol_version v)
{
    return v!=tls_protocol_version_SSL_3;
}


static bool default_supported_always(tls_protocol_version v)
{
    return true;
}


/* For gnutls servers with EC certs, they will only support DH_anon ciphersuites in SSL 3
 OpenSSL server will happily use EC ciphersuites with SSL 3 */
static bool __unused default_supported_unless_ssl3(tls_protocol_version v)
{
    return v!=tls_protocol_version_SSL_3;
}

//#define OPENSSL_SERVER "ariadne.apple.com"
//#define GNUTLS_SERVER "ariadne.apple.com"
//#define OPENSSL_SERVER "kuip.apple.com"
//#define GNUTLS_SERVER "kuip.apple.com"
//#define OPENSSL_SERVER "localhost"
//#define GNUTLS_SERVER "localhost"
#define OPENSSL_SERVER "192.168.2.1"
#define GNUTLS_SERVER "192.168.2.1"


static struct test_server {
    const char *host;
    int port;
    bool (*client_auth)(tls_protocol_version version); // Server request client cert
    bool (*ciphersuite_supported)(uint16_t ciphersuite);
    bool (*default_supported)(tls_protocol_version version); // if this server support any of the default ciphersuites
} servers[] = {
    { OPENSSL_SERVER, 4001, &client_auth_never, &openssl_rsa_rsa_is_cipher_supported, &default_supported_always}, //openssl s_server w/o client side auth, rsa/rsa
    { OPENSSL_SERVER, 4002, &client_auth_never, &openssl_rsa_ecc_is_cipher_supported, &default_supported_always}, //openssl s_server w/o client side auth, rsa/ecc
    { OPENSSL_SERVER, 4003, &client_auth_never, &openssl_ecc_rsa_is_cipher_supported, &default_supported_unless_ssl3}, //openssl s_server w/o client side auth, ecc/rsa
    { OPENSSL_SERVER, 4004, &client_auth_never, &openssl_ecc_ecc_is_cipher_supported, &default_supported_unless_ssl3}, //openssl s_server w/o client side auth, ecc/ecc
    { GNUTLS_SERVER, 5001, &client_auth_never, &gnutls_rsa_rsa_is_cipher_supported, &default_supported_always}, // gnutls-serv w/o client side auth, rsa/rsa,
    { GNUTLS_SERVER, 5002, &client_auth_never, &gnutls_rsa_ecc_is_cipher_supported, &default_supported_always}, // gnutls-serv w/o client side auth, rsa/ecc,
    { GNUTLS_SERVER, 5003, &client_auth_never, &gnutls_ecc_rsa_is_cipher_supported, &default_supported_unless_ssl3}, // gnutls-serv w/o client side auth, ecc/rsa,
    { GNUTLS_SERVER, 5004, &client_auth_never, &gnutls_ecc_ecc_is_cipher_supported, &default_supported_unless_ssl3}, // gnutls-serv w/o client side auth, ecc/ecc
//    { "www.mikestoolbox.org", 443, &client_auth_ssl3_only, &mikes_is_cipher_supported, &default_supported_always}, // mike's  w/o client side auth
//  { "tls.secg.org", 443, &client_auth_never, &secg_is_cipher_supported, &default_supported_always}, // secg ecc server w/o client side auth - This server generate DH params we dont support. This is fixed in Sundance.

    { OPENSSL_SERVER, 4011, &client_auth_always, &openssl_rsa_rsa_is_cipher_supported, &default_supported_always}, //openssl s_server w/ client side auth
    { OPENSSL_SERVER, 4012, &client_auth_always, &openssl_rsa_ecc_is_cipher_supported, &default_supported_always}, //openssl s_server w/ client side auth, rsa/ecc
    { OPENSSL_SERVER, 4013, &client_auth_always, &openssl_ecc_rsa_is_cipher_supported, &default_supported_unless_ssl3}, //openssl s_server w/ client side auth, ecc/rsa
    { OPENSSL_SERVER, 4014, &client_auth_always, &openssl_ecc_ecc_is_cipher_supported, &default_supported_unless_ssl3}, //openssl s_server w/ client side auth, ecc/ecc
    { GNUTLS_SERVER, 5011, &client_auth_always, &gnutls_rsa_rsa_is_cipher_supported, &default_supported_always}, // gnutls-serv w/ client side auth
    { GNUTLS_SERVER, 5012, &client_auth_always, &gnutls_rsa_ecc_is_cipher_supported, &default_supported_always}, // gnutls-serv w/ client side auth, rsa/ecc,
    { GNUTLS_SERVER, 5013, &client_auth_unless_ssl3, &gnutls_ecc_rsa_is_cipher_supported, &default_supported_unless_ssl3}, // gnutls-serv w/ client side auth, ecc/rsa,
    { GNUTLS_SERVER, 5014, &client_auth_unless_ssl3, &gnutls_ecc_ecc_is_cipher_supported, &default_supported_unless_ssl3}, // gnutls-serv w/ client side auth, ecc/ecc
//    { "www.mikestoolbox.net", 443, &client_auth_always, &mikes_is_cipher_supported, &default_supported_always}, // mike's  w/ client side auth
//  { "tls.secg.org", 8442, 3}, //secg ecc server w/ client side auth

};
int nservers = sizeof(servers)/sizeof(servers[0]);

__attribute__((unused))
static
int generate_test_cases(void)
{

    int err;
    int fails = 0;
    int success = 0;

    tls_test_case test;

#if 0
    for(int p=0; p<nservers;p++) {
        for(int pr=0; pr<nprotos ;pr++) {
            for (int i=0; i<0; i++) {
#else
    for (int p=0; p<nservers; p++) {
        for (int pr=0; pr<nprotos; pr++) {
            for (int i=0; i<nciphers; i++) {
#endif
            SKIP: {
                skip("Unsupported ciphersuite/protocol version combo", 1,
                     protos[pr]>=sslCipherSuiteGetMinSupportedTLSVersion(ciphers[i].cipher));

                memset(&test, 0, sizeof(test));

                test.num_ciphersuites=1;
                test.allow_resumption=false; // We don't test resumption in this test.
                test.ciphersuites=&ciphers[i].cipher;
                test.protocol_max=protos[pr];
                test.hostname=servers[p].host;
                test.port=servers[p].port;
                test.dtls=false;
                test.session_id = (p<<16) | (pr<<8) | (i+1);
                test.request = "GET / HTTP/1.0\r\n\r\n";

                if(!servers[p].ciphersuite_supported(ciphers[i].cipher))
                {
                    test.err = -9824; //FIXME
                } else {
                    test.err = 0;
                    test.read_ready_received = 1;
                    test.write_ready_received = 1;
                    test.negotiated_ciphersuite = ciphers[i].cipher;
                }

                int kem = sslCipherSuiteGetKeyExchangeMethod(ciphers[i].cipher);
                if((servers[p].client_auth(protos[pr])) && (kem != SSL_DH_anon) && (kem != SSL_ECDH_anon))
                    test.certificate_requested=1;

                test_log_start();
                test_printf("\n***** Testing case: (%d,%d,%d) -- %s:%d %40s -- exp err=%d\n", p, pr, i, servers[p].host, servers[p].port, ciphers[i].name, test.err);
                err = test_one_case(&test);
                test_printf("***** Tested case: (%d,%d,%d) -- err =%d\n", p, pr, i, err);
                ok(!err, "***** Test Failed: (%d,%d,%d) -- %s:%d %40s -- err = %d", p, pr, i, servers[p].host, servers[p].port, ciphers[i].name, err);
                if(err) {
                    fails++;
                } else {
                    success++;
                }
                test_log_end(err);
            }}

#if 1

            /* Connect to server with default ciphersuites */

            memset(&test, 0, sizeof(test));

            test.num_ciphersuites=0;
            test.ciphersuites=NULL;
            test.protocol_max=protos[pr];
            test.hostname=servers[p].host;
            test.port=servers[p].port;
            test.session_id = (p<<16) | (pr<<8) | (0xff);
            test.request = "GET / HTTP/1.0\r\n\r\n";


            if(!servers[p].default_supported(protos[pr]))
            {
                test.err = -9824; //FIXME
            } else {
                test.err = 0;
                test.read_ready_received = 1;
                test.write_ready_received = 1;
            }

            if(servers[p].client_auth(protos[pr]))
                test.certificate_requested=1;

            test_log_start();
            test_printf("\n***** Testing case: (%d,%d,ALL) -- %s:%d -- exp err=%d\n", p, pr, servers[p].host, servers[p].port, test.err);
            err = test_one_case(&test);
            test_printf("***** Tested case: (%d,%d,ALL) -- err = %d\n", p, pr, err);
            ok(!err, "***** Test Failed: (%d,%d,ALL) -- %s:%d -- err = %d\n\n", p, pr, servers[p].host, servers[p].port, err);
            if(err) {
                fails++;
            } else {
                success++;
            }
            test_log_end(err);
#endif

        }
    }

    printf("Generated tests - OK: %d, FAILS: %d\n", success, fails);

    return fails;

}

int handshake_ecc_test(void);
int handshake_ecc_test(void)
{
    int err;

    require_noerr((err=init_server_keys(true,
                                        ecclientcert_der, ecclientcert_der_len,
                                        ecclientkey_der, ecclientkey_der_len,
                                        &g_cert, &g_key)), fail);

    tls_test_case test0;
    memset(&test0, 0, sizeof(test0));

    test0.hostname="localhost";
    test0.port=4433;

    test0.num_ciphersuites = CipherSuiteCount;
    test0.ciphersuites = KnownCipherSuites;
    test0.request = "GET / HTTP/1.0\r\n\r\n";
    test0.read_ready_received = 1;
    test0.write_ready_received = 1;
    test0.certificate_requested = 1;
    test0.isECKey = true;

    test_log_start();
    test_printf("***** Testing case: test0 -- %s:%d\n", test0.hostname, test0.port);
    err=test_one_case(&test0);
    ok(!err, "***** Tested case: test0 -- %s:%d -- err =%d\n", test0.hostname, test0.port, err);
    test_log_end(err);

fail:
    clean_server_keys(g_key);
    return err;

}

int handshake_client_test(void);
int handshake_client_test(void)
{
    int err;

    require_noerr((err=init_server_keys(false,
                                        Server1_Cert_rsa_rsa_der, Server1_Cert_rsa_rsa_der_len,
                                        Server1_Key_rsa_der, Server1_Key_rsa_der_len,
                                        &g_cert, &g_key)), fail);
    tls_test_case test0;
    memset(&test0, 0, sizeof(test0));

    test0.hostname="www.google.com";
    test0.port=443;
    test0.num_ciphersuites = CipherSuiteCount;
    test0.ciphersuites = KnownCipherSuites;
    test0.request = "GET / HTTP/1.0\r\n\r\n";
    test0.read_ready_received = 1;
    test0.write_ready_received = 1;
    test0.isECKey = false;

    test_log_start();
    test_printf("***** Testing case: test0 -- %s:%d\n", test0.hostname, test0.port);
    err=test_one_case(&test0);
    ok(!err, "***** Tested case: test0 -- %s:%d -- err =%d\n", test0.hostname, test0.port, err);
    test_log_end(err);

    err = generate_test_cases();
    //printf("***** Failed %d generated tests\n", err);

fail:
    clean_server_keys(g_key);
    return err;
}


int tls_03_client(int argc, char * const argv[])
{
    plan_tests(1 + nservers*nprotos*(nciphers+1));

    handshake_client_test();
   // handshake_ecc_test();

    return 0;
}
