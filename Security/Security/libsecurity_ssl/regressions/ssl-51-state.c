//
//  ssl-51-state.c
//  libsecurity_ssl
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

#include <sys/queue.h>


#define test_printf(x...)

/* extern struct ccrng_state *ccDRBGGetRngState(); */
#include <CommonCrypto/CommonRandomSPI.h>
#define CCRNGSTATE ccDRBGGetRngState()

struct RecQueueItem {
    STAILQ_ENTRY(RecQueueItem) next; /* link to next queued entry or NULL */
    tls_buffer                 record;
    size_t                     offset; /* byte reads from this one */
};

typedef struct {
    SSLContextRef st;
    tls_stream_parser_t parser;
    tls_record_t record;
    tls_handshake_t hdsk;
    STAILQ_HEAD(, RecQueueItem) rec_queue; // coretls server queue packet in this queue
    int ready_count;
} ssl_test_handle;


static
int tls_buffer_alloc(tls_buffer *buf, size_t length)
{
    buf->data = malloc(length);
    if(!buf->data) return -ENOMEM;
    buf->length = length;
    return 0;
}

static
int tls_buffer_free(tls_buffer *buf)
{
    free(buf->data);
    buf->data = NULL;
    buf->length = 0;
    return 0;
}

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
    ssl_test_handle *handle =(ssl_test_handle *)h;

    test_printf("%s: %p requesting len=%zd\n", __FUNCTION__, h, *length);

    struct RecQueueItem *item = STAILQ_FIRST(&handle->rec_queue);

    if(item==NULL) {
        test_printf("%s: %p no data available\n", __FUNCTION__, h);
        return errSSLWouldBlock;
    }

    size_t avail = item->record.length - item->offset;

    test_printf("%s: %p %zd bytes available in %p\n", __FUNCTION__, h, avail, item);

    if(avail > *length) {
        memcpy(data, item->record.data+item->offset, *length);
        item->offset += *length;
    } else {
        memcpy(data, item->record.data+item->offset, avail);
        *length = avail;
        STAILQ_REMOVE_HEAD(&handle->rec_queue, next);
        tls_buffer_free(&item->record);
        free(item);
    }

    test_printf("%s: %p %zd bytes read\n", __FUNCTION__, h, *length);


    return 0;
}

static int process(tls_stream_parser_ctx_t ctx, tls_buffer record)
{
    ssl_test_handle *h = (ssl_test_handle *)ctx;
    tls_buffer decrypted;
    uint8_t ct;
    int err;

    test_printf("%s: %p processing %zd bytes\n", __FUNCTION__, ctx, record.length);


    decrypted.length = tls_record_decrypted_size(h->record, record.length);
    decrypted.data = malloc(decrypted.length);

    require_action(decrypted.data, errOut, err=-ENOMEM);
    require_noerr((err=tls_record_decrypt(h->record, record, &decrypted, &ct)), errOut);

    test_printf("%s: %p decrypted %zd bytes, ct=%d\n", __FUNCTION__, ctx, decrypted.length, ct);

    err=tls_handshake_process(h->hdsk, decrypted, ct);

    test_printf("%s: %p processed, err=%d\n", __FUNCTION__, ctx, err);

errOut:
    return err;
}

static int
tls_handshake_write_callback(tls_handshake_ctx_t ctx, const tls_buffer data, uint8_t content_type)
{
    int err = 0;
    ssl_test_handle *handle = (ssl_test_handle *)ctx;

    test_printf("%s: %p writing data ct=%d, len=%zd\n", __FUNCTION__, ctx, content_type, data.length);

    struct RecQueueItem *item = malloc(sizeof(struct RecQueueItem));
    require_action(item, errOut, err=-ENOMEM);

    err=tls_buffer_alloc(&item->record, tls_record_encrypted_size(handle->record, content_type, data.length));
    require_noerr(err, errOut);

    err=tls_record_encrypt(handle->record, data, content_type, &item->record);
    require_noerr(err, errOut);

    item->offset = 0;

    test_printf("%s: %p queing %zd encrypted bytes, item=%p\n", __FUNCTION__, ctx, item->record.length, item);

    STAILQ_INSERT_TAIL(&handle->rec_queue, item, next);

    return 0;

errOut:
    if(item) {
        tls_buffer_free(&item->record);
        free(item);
    }
    return err;
}


static int
tls_handshake_message_callback(tls_handshake_ctx_t ctx, tls_handshake_message_t event)
{
    ssl_test_handle __unused *handle = (ssl_test_handle *)ctx;

    test_printf("%s: %p event = %d\n", __FUNCTION__, handle, event);

    int err = 0;

    return err;
}



static uint8_t appdata[] = "appdata";

tls_buffer appdata_buffer = {
    .data = appdata,
    .length = sizeof(appdata),
};


static void
tls_handshake_ready_callback(tls_handshake_ctx_t ctx, bool write, bool ready)
{
    ssl_test_handle *handle = (ssl_test_handle *)ctx;

    test_printf("%s: %p %s ready=%d\n", __FUNCTION__, handle, write?"write":"read", ready);

    if(ready) {
        if(write) {
            if(handle->ready_count == 0) {
                tls_handshake_request_renegotiation(handle->hdsk);
            } else {
                tls_handshake_write_callback(ctx, appdata_buffer, tls_record_type_AppData);
            }
            handle->ready_count++;;
        }
    }
}

static int
tls_handshake_set_retransmit_timer_callback(tls_handshake_ctx_t ctx, int attempt)
{
    ssl_test_handle __unused *handle = (ssl_test_handle *)ctx;

    test_printf("%s: %p attempt = %d\n", __FUNCTION__, handle, attempt);

    return -1;
}

static
int mySSLRecordInitPendingCiphersFunc(tls_handshake_ctx_t ref,
                                      uint16_t            selectedCipher,
                                      bool                server,
                                      tls_buffer           key)
{
    ssl_test_handle *handle = (ssl_test_handle *)ref;

    test_printf("%s: %p, cipher=%04x, server=%d\n", __FUNCTION__, ref, selectedCipher, server);
    return tls_record_init_pending_ciphers(handle->record, selectedCipher, server, key);
}

static
int mySSLRecordAdvanceWriteCipherFunc(tls_handshake_ctx_t ref)
{
    ssl_test_handle *handle = (ssl_test_handle *)ref;
    test_printf("%s: %p\n", __FUNCTION__, ref);
    return tls_record_advance_write_cipher(handle->record);
}

static
int mySSLRecordRollbackWriteCipherFunc(tls_handshake_ctx_t ref)
{
    ssl_test_handle *handle = (ssl_test_handle *)ref;
    test_printf("%s: %p\n", __FUNCTION__, ref);
    return tls_record_rollback_write_cipher(handle->record);
}

static
int mySSLRecordAdvanceReadCipherFunc(tls_handshake_ctx_t ref)
{
    ssl_test_handle *handle = (ssl_test_handle *)ref;
    test_printf("%s: %p\n", __FUNCTION__, ref);
    return tls_record_advance_read_cipher(handle->record);
}

static
int mySSLRecordSetProtocolVersionFunc(tls_handshake_ctx_t ref,
                                      tls_protocol_version  protocolVersion)
{
    ssl_test_handle *handle = (ssl_test_handle *)ref;
    test_printf("%s: %p, version=%04x\n", __FUNCTION__, ref, protocolVersion);
    return tls_record_set_protocol_version(handle->record, protocolVersion);
}


static int
tls_handshake_save_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey, tls_buffer sessionData)
{
    ssl_test_handle __unused *handle = (ssl_test_handle *)ctx;

    test_printf("%s: %p\n", __FUNCTION__, handle);

    return -1;
}

static int
tls_handshake_load_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey, tls_buffer *sessionData)
{
    ssl_test_handle __unused *handle = (ssl_test_handle *)ctx;

    test_printf("%s: %p\n", __FUNCTION__, handle);

    return -1;
}

static int
tls_handshake_delete_session_data_callback(tls_handshake_ctx_t ctx, tls_buffer sessionKey)
{
    ssl_test_handle __unused *handle = (ssl_test_handle *)ctx;

    test_printf("%s: %p\n", __FUNCTION__, handle);

    return -1;
}

static int
tls_handshake_delete_all_sessions_callback(tls_handshake_ctx_t ctx)
{
    ssl_test_handle __unused *handle = (ssl_test_handle *)ctx;

    test_printf("%s: %p\n", __FUNCTION__, handle);

    return -1;
}

/* TLS callbacks */
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
    TLS_PSK_WITH_AES_128_CBC_SHA,
};
static int nciphers = sizeof(ciphers)/sizeof(ciphers[0]);

static SSLCipherSuite ciphersuites[] = {
    TLS_PSK_WITH_AES_128_CBC_SHA,
};
static int nciphersuites = sizeof(ciphersuites)/sizeof(ciphersuites[0]);



static uint8_t shared_secret[] = "secret";

tls_buffer psk_secret = {
    .data = shared_secret,
    .length = sizeof(shared_secret),
};

static ssl_test_handle *
ssl_test_handle_create(bool server)
{
    ssl_test_handle *handle = calloc(1, sizeof(ssl_test_handle));
    SSLContextRef ctx = SSLCreateContext(kCFAllocatorDefault, server?kSSLServerSide:kSSLClientSide, kSSLStreamType);

    require(handle, out);
    require(ctx, out);

    require_noerr(SSLSetIOFuncs(ctx, (SSLReadFunc)SocketRead, (SSLWriteFunc)SocketWrite), out);
    require_noerr(SSLSetConnection(ctx, (SSLConnectionRef)handle), out);
    require_noerr(SSLSetSessionOption(ctx, kSSLSessionOptionBreakOnServerAuth, true), out);
    require_noerr(SSLSetEnabledCiphers(ctx, ciphersuites, nciphersuites), out);
    require_noerr(SSLSetPSKSharedSecret(ctx, shared_secret, sizeof(shared_secret)), out);

    handle->st = ctx;
    handle->parser = tls_stream_parser_create(handle, process);
    handle->record = tls_record_create(false, CCRNGSTATE);
    handle->hdsk = tls_handshake_create(false, true); // server.

    require_noerr(tls_handshake_set_ciphersuites(handle->hdsk, ciphers, nciphers), out);
    require_noerr(tls_handshake_set_callbacks(handle->hdsk, &tls_handshake_callbacks, handle), out);
    require_noerr(tls_handshake_set_psk_secret(handle->hdsk, &psk_secret), out);
    require_noerr(tls_handshake_set_renegotiation(handle->hdsk, true), out);

    // Initialize the record queue
    STAILQ_INIT(&handle->rec_queue);

    return handle;

out:
    if (handle) free(handle);
    if (ctx) CFRelease(ctx);
    return NULL;
}

static void
tests(void)
{
    OSStatus ortn;

    ssl_test_handle *client;
    SSLSessionState state;

    client = ssl_test_handle_create(false);

    require_action(client, out, ortn = -1);

    ortn = SSLGetSessionState(client->st, &state);
    require_noerr(ortn, out);
    is(state, kSSLIdle, "State should be Idle");

    do {
        ortn = SSLHandshake(client->st);

        require_noerr(ortn = SSLGetSessionState(client->st, &state), out);
        test_printf("SSLHandshake returned err=%d\n", (int)ortn);

        if (ortn == errSSLPeerAuthCompleted || ortn == errSSLWouldBlock)
        {
            require_action(state==kSSLHandshake, out, ortn = -1);
        }

    } while(ortn==errSSLWouldBlock ||
            ortn==errSSLPeerAuthCompleted);


    is(ortn, 0, "Unexpected SSLHandshake exit code");
    is(state, kSSLConnected, "State should be Connected");

    uint8_t buffer[128];
    size_t available = 0;

    test_printf("Initial handshake done\n");

    do {
        ortn = SSLRead(client->st, buffer, sizeof(buffer), &available);
        require_noerr(ortn = SSLGetSessionState(client->st, &state), out);

        test_printf("SSLRead returned err=%d, avail=%zd\n", (int)ortn, available);
        if (ortn == errSSLPeerAuthCompleted)
        {
            require_action(state==kSSLHandshake, out, ortn = -1);
        }

    } while(available==0);

    is(ortn, 0, "Unexpected SSLRead exit code");
    is(state, kSSLConnected, "State should be Connected");


out:
    is(ortn, 0, "Final result is non zero");
    ssl_test_handle_destroy(client);

}

int ssl_51_state(int argc, char *const *argv)
{

    plan_tests(6);

    tests();

    return 0;
}
