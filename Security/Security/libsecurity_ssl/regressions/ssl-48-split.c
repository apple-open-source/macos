//
//  ssl-48-crashes.c
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


typedef struct {
    SSLContextRef st;
    bool is_server;
    int comm;
    CFArrayRef certs;
    int write_counter;
    tls_stream_parser_t parser;
    size_t write_size;
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
    int conn = handle->comm;
	size_t len = *length;
	uint8_t *ptr = (uint8_t *)data;

    if(handle->is_server) {
        //printf("SocketWrite: server write len=%zd\n", len);

        tls_buffer buffer;
        buffer.data = ptr;
        buffer.length = len;
        tls_stream_parser_parse(handle->parser, buffer);
    }

    do {
        ssize_t ret;
        do {
            hexdump("write", ptr, len);
            ret = write((int)conn, ptr, len);
        } while ((ret < 0) && (errno == EAGAIN || errno == EINTR));
        if (ret > 0) {
            len -= ret;
            ptr += ret;
        }
        else
            return -36;
    } while (len > 0);

    *length = *length - len;
    return errSecSuccess;
}

static OSStatus SocketRead(SSLConnectionRef h, void *data, size_t *length)
{
    const ssl_test_handle *handle=h;
    int conn = handle->comm;
	size_t len = *length;
	uint8_t *ptr = (uint8_t *)data;


    do {
        ssize_t ret;
        do {
            ret = read((int)conn, ptr, len);
        } while ((ret < 0) && (errno == EAGAIN || errno == EINTR));
        if (ret > 0) {
            len -= ret;
            ptr += ret;
        }
        else
            return -36;
    } while (len > 0);

    if(len!=0)
        printf("Something went wrong here... len=%d\n", (int)len);

    *length = *length - len;
    return errSecSuccess;
}

static int process(tls_stream_parser_ctx_t ctx, tls_buffer record)
{
    ssl_test_handle *handle = (ssl_test_handle *)ctx;

    // printf("processing record len=%zd, type=%d\n", record.length, record.data[0]);
    if(record.data[0]==tls_record_type_AppData) {
        handle->write_counter++;
        // printf("record count = %d\n", handle->write_counter);
    }

    return 0;
}


static void *securetransport_ssl_thread(void *arg)
{
    OSStatus ortn;
    ssl_test_handle * ssl = (ssl_test_handle *)arg;
    SSLContextRef ctx = ssl->st;
    bool got_server_auth = false;

    //uint64_t start = mach_absolute_time();
    do {
        ortn = SSLHandshake(ctx);

        if (ortn == errSSLServerAuthCompleted)
        {
            require_string(!got_server_auth, out, "second server auth");
            got_server_auth = true;
        }
    } while (ortn == errSSLWouldBlock
             || ortn == errSSLServerAuthCompleted);

    require_noerr_action_quiet(ortn, out,
                               fprintf(stderr, "Fell out of SSLHandshake with error: %d\n", (int)ortn));

    unsigned char ibuf[90000], obuf[45000];

    if (ssl->is_server) {
        size_t len;
        SecRandomCopyBytes(kSecRandomDefault, ssl->write_size, obuf);
        require_noerr(ortn = SSLWrite(ctx, obuf, ssl->write_size, &len), out);
        require_action(len == ssl->write_size, out, ortn = -1);
        require_noerr(ortn = SSLWrite(ctx, obuf, ssl->write_size, &len), out);
        require_action(len == ssl->write_size, out, ortn = -1);
    } else {
        size_t len = ssl->write_size*2;
        size_t olen;
        unsigned char *p = ibuf;
        while (len) {
            require_noerr(ortn = SSLRead(ctx, p, len, &olen), out);
            len -= olen;
            p += olen;
        }
    }

out:
    SSLClose(ctx);
    CFRelease(ctx);
    close(ssl->comm);
    pthread_exit((void *)(intptr_t)ortn);
    return NULL;
}

static void
ssl_test_handle_destroy(ssl_test_handle *handle)
{
    if(handle) {
        if(handle->parser) tls_stream_parser_destroy(handle->parser);
        free(handle);
    }
}

static ssl_test_handle *
ssl_test_handle_create(bool server, int comm, CFArrayRef certs)
{
    ssl_test_handle *handle = calloc(1, sizeof(ssl_test_handle));
    SSLContextRef ctx = SSLCreateContext(kCFAllocatorDefault, server?kSSLServerSide:kSSLClientSide, kSSLStreamType);

    require(handle, out);
    require(ctx, out);

    require_noerr(SSLSetIOFuncs(ctx,
                                (SSLReadFunc)SocketRead, (SSLWriteFunc)SocketWrite), out);
    require_noerr(SSLSetConnection(ctx, (SSLConnectionRef)handle), out);

    if (server)
        require_noerr(SSLSetCertificate(ctx, certs), out);

    require_noerr(SSLSetSessionOption(ctx,
                                      kSSLSessionOptionBreakOnServerAuth, true), out);

    /* Tell SecureTransport to not check certs itself: it will break out of the
     handshake to let us take care of it instead. */
    require_noerr(SSLSetEnableCertVerify(ctx, false), out);

    handle->is_server = server;
    handle->comm = comm;
    handle->certs = certs;
    handle->st = ctx;
    handle->write_counter = 0;
    handle->parser = tls_stream_parser_create(handle, process);

    return handle;

out:
    if (handle) free(handle);
    if (ctx) CFRelease(ctx);
    return NULL;
}

static SSLCipherSuite ciphers[] = {
    TLS_RSA_WITH_AES_128_CBC_SHA,
    //FIXME: re-enable this test when its fixed.
    //TLS_RSA_WITH_RC4_128_SHA,
};
static int nciphers = sizeof(ciphers)/sizeof(ciphers[0]);

static SSLProtocolVersion versions[] = {
    kSSLProtocol3,
    kTLSProtocol1,
    kTLSProtocol11,
    kTLSProtocol12,
};
static int nversions = sizeof(versions)/sizeof(versions[0]);

// { write size, expected count when nosplit, expected count when split }
static size_t wsizes[][3] = {
    {       1,  2,  2 },
    {       2,  2,  3 },
    {       3,  2,  3 },
    {       4,  2,  3 },
    {   16384,  2,  3 },
    {   16385,  4,  4 },
    {   16386,  4,  6 },
    {   16387,  4,  7 },
    {   16388,  4,  7 },
    {   32768,  4,  7 },
    {   32769,  6,  7 },
    {   32770,  6,  8 },
    {   32771,  6, 10 },
    {   32772,  6, 11 },
    {   32773,  6, 11 },
};
static int nwsizes = sizeof(wsizes)/sizeof(wsizes[0]);

static void
tests(void)
{
    pthread_t client_thread, server_thread;
    CFArrayRef server_certs = server_chain();
    ok(server_certs, "got server certs");

    int i,j,k,s;

    for(i=0; i<nciphers; i++)
    for(j=0; j<nversions; j++)
    for(k=0; k<nwsizes; k++)
    for(s=0; s<3; s++)
    {
        int sp[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp)) exit(errno);
        fcntl(sp[0], F_SETNOSIGPIPE, 1);
        fcntl(sp[1], F_SETNOSIGPIPE, 1);

        ssl_test_handle *server, *client;

        server = ssl_test_handle_create(true /*server*/, sp[0], server_certs);
        client = ssl_test_handle_create(false/*client*/, sp[1], NULL);

        server->write_size = wsizes[k][0];
        client->write_size = wsizes[k][0];

        require(client, out);
        require(server, out);

        require_noerr(SSLSetProtocolVersionMax(client->st, versions[j]), out);
        require_noerr(SSLSetEnabledCiphers(client->st, &ciphers[i], 1), out);
        if(s) {
            // s=0: default (should be enabled)
            // s=1: explicit enable
            // s=2: expliciti disable
            require_noerr(SSLSetSessionOption(server->st, kSSLSessionOptionSendOneByteRecord, (s==1)?true:false), out);
        }
        // printf("**** Test Case: i=%d, j=%d, k=%d (%zd), s=%d ****\n", i, j, k, wsizes[k][0], s);

        pthread_create(&client_thread, NULL, securetransport_ssl_thread, client);
        pthread_create(&server_thread, NULL, securetransport_ssl_thread, server);

        int server_err, client_err;
        pthread_join(client_thread, (void*)&client_err);
        pthread_join(server_thread, (void*)&server_err);

        ok(!server_err, "Server error = %d", server_err);
        ok(!client_err, "Client error = %d", client_err);

        /* one byte split is expected only for AES when using TLS 1.0 or lower, and when not disabled */
        bool expected_split = (i==0) && (s!=2) && (versions[j]<=kTLSProtocol1);
        int expected_count = (int)(expected_split ? wsizes[k][2]: wsizes[k][1]);

        is(server->write_counter, expected_count, "wrong number of data records");

        // fprintf(stderr, "Server write counter = %d, expected %d\n", server->write_counter, expected_count);

out:
        ssl_test_handle_destroy(client);
        ssl_test_handle_destroy(server);

    }
    CFReleaseNull(server_certs);
}

int ssl_48_split(int argc, char *const *argv)
{

    plan_tests(1 + nciphers*nversions*nwsizes*3 * 3);


    tests();

    return 0;
}
