//
//  ssl-49-sni.c
//  libsecurity_ssl
//
//


#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>

#include <AssertMacros.h>
#include <Security/SecureTransportPriv.h> /* SSLSetOption */
#include <Security/SecureTransport.h>
#include <Security/SecPolicy.h>
#include <Security/SecTrust.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItem.h>
#include <Security/SecRandom.h>

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <mach/mach_time.h>

#if TARGET_OS_IPHONE
#include <Security/SecRSAKey.h>
#endif

#include "ssl_regressions.h"
#include "ssl-utils.h"

#include <tls_stream_parser.h>
#include <tls_handshake.h>
#include <tls_record.h>

/* extern struct ccrng_state *ccDRBGGetRngState(); */
#include <CommonCrypto/CommonRandomSPI.h>
#define CCRNGSTATE ccDRBGGetRngState()


typedef struct {
    SSLContextRef st;
    tls_stream_parser_t parser;
    tls_record_t record;
    tls_handshake_t hdsk;
} ssl_test_handle;


#pragma mark -
#pragma mark SecureTransport support

#if 0
static void hexdump(const char *s, const uint8_t *bytes, size_t len) {
	size_t ix;
    printf("socket %s(%p, %lu)\n", s, bytes, len);
	for (ix = 0; ix < len; ++ix) {
        if (!(ix % 16))
            printf("\n");
		printf("%02X ", bytes[ix]);
	}
	printf("\n");
}
#else
#define hexdump(string, bytes, len)
#endif

static OSStatus SocketWrite(SSLConnectionRef h, const void *data, size_t *length)
{
    ssl_test_handle *handle =(ssl_test_handle *)h;

	size_t len = *length;
	uint8_t *ptr = (uint8_t *)data;

    tls_buffer buffer;
    buffer.data = ptr;
    buffer.length = len;
    return tls_stream_parser_parse(handle->parser, buffer);
}

static OSStatus SocketRead(SSLConnectionRef h, void *data, size_t *length)
{
    return -36;
}

static int process(tls_stream_parser_ctx_t ctx, tls_buffer record)
{
    ssl_test_handle *h = (ssl_test_handle *)ctx;
    tls_buffer decrypted;
    uint8_t ct;
    int err;

    decrypted.length = tls_record_decrypted_size(h->record, record.length);
    decrypted.data = malloc(decrypted.length);

    require_action(decrypted.data, errOut, err=ENOMEM);
    require_noerr((err=tls_record_decrypt(h->record, record, &decrypted, &ct)), errOut);
    err=tls_handshake_process(h->hdsk, decrypted, ct);

errOut:
    return err;
}


static int
tls_handshake_message_callback(tls_handshake_ctx_t ctx, tls_handshake_message_t event)
{
    int err = 0;

    switch(event) {
        case tls_handshake_message_client_hello:
            err = -1234;
            break;
        default:
            err = -1;
            break;
    }

    return err;
}

static int
tls_handshake_set_protocol_version(tls_handshake_ctx_t ctx, tls_protocol_version protocolVersion)
{
    return 0;
}

static int
tls_handshake_write(tls_handshake_ctx_t ctx, const tls_buffer data, uint8_t content_type)
{
    return -36;
}

static int
tls_handshake_set_retransmit_timer(tls_handshake_ctx_t ctx, int attempt)
{
    return -1;
}


static
tls_handshake_callbacks_t tls_handshake_callbacks = {
    .message = tls_handshake_message_callback,
    .set_protocol_version = tls_handshake_set_protocol_version,
    .write = tls_handshake_write,
    .set_retransmit_timer = tls_handshake_set_retransmit_timer,
};


static void
ssl_test_handle_destroy(ssl_test_handle *handle)
{
    if(handle) {
        if(handle->parser) tls_stream_parser_destroy(handle->parser);
        if(handle->record) tls_record_destroy(handle->record);
        if(handle->hdsk) tls_handshake_destroy(handle->hdsk);
        if(handle->st) CFRelease(handle->st);
        free(handle);
    }
}

static uint16_t ciphers[] = {
    TLS_RSA_WITH_AES_128_CBC_SHA,
    //FIXME: re-enable this test when its fixed.
    //TLS_RSA_WITH_RC4_128_SHA,
};
static int nciphers = sizeof(ciphers)/sizeof(ciphers[0]);


static ssl_test_handle *
ssl_test_handle_create(bool server)
{
    ssl_test_handle *handle = calloc(1, sizeof(ssl_test_handle));
    SSLContextRef ctx = SSLCreateContext(kCFAllocatorDefault, server?kSSLServerSide:kSSLClientSide, kSSLStreamType);

    require(handle, out);
    require(ctx, out);

    require_noerr(SSLSetIOFuncs(ctx,
                                (SSLReadFunc)SocketRead, (SSLWriteFunc)SocketWrite), out);
    require_noerr(SSLSetConnection(ctx, (SSLConnectionRef)handle), out);

    require_noerr(SSLSetSessionOption(ctx,
                                      kSSLSessionOptionBreakOnServerAuth, true), out);

    /* Tell SecureTransport to not check certs itself: it will break out of the
     handshake to let us take care of it instead. */
    require_noerr(SSLSetEnableCertVerify(ctx, false), out);

    handle->st = ctx;
    handle->parser = tls_stream_parser_create(handle, process);
    handle->record = tls_record_create(false, CCRNGSTATE);
    handle->hdsk = tls_handshake_create(false, true); // server.
    tls_handshake_set_ciphersuites(handle->hdsk, ciphers, nciphers);

    tls_handshake_set_callbacks(handle->hdsk, &tls_handshake_callbacks, handle);

    return handle;

out:
    if (handle) free(handle);
    if (ctx) CFRelease(ctx);
    return NULL;
}

static SSLProtocolVersion versions[] = {
    kSSLProtocol3,
    kTLSProtocol1,
    kTLSProtocol11,
    kTLSProtocol12,
};
static int nversions = sizeof(versions)/sizeof(versions[0]);

static char peername[] = "peername";

static void
tests(void)
{
    int j;
    OSStatus ortn;

    for(j=0; j<nversions; j++)
    {
        ssl_test_handle *client;
        const tls_buffer *sni;

        client = ssl_test_handle_create(false);

        require(client, out);

        require_noerr(SSLSetProtocolVersionMax(client->st, versions[j]), out);
        require_noerr(SSLSetPeerDomainName(client->st, peername, sizeof(peername)), out);

        ortn = SSLHandshake(client->st);

        ok(ortn==-1234, "Unexpected Handshake exit code");

        sni = tls_handshake_get_sni_hostname(client->hdsk);

        if(versions[j]==kSSLProtocol3) {
            ok(sni==NULL || sni->data==NULL,"Unexpected SNI");
        } else {
            ok(sni!=NULL && sni->data!=NULL &&
               sni->length == sizeof(peername) &&
               (memcmp(sni->data, peername, sizeof(peername))==0),
               "SNI does not match");
        }

out:
        ssl_test_handle_destroy(client);

    }
}

int ssl_49_sni(int argc, char *const *argv)
{

    plan_tests(8);


    tests();

    return 0;
}
