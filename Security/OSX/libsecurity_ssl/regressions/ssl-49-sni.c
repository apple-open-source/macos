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

typedef struct {
    SSLContextRef handle;
    uint32_t session_id;
    bool is_server;
    int comm;
} ssl_test_handle;


#pragma mark -
#pragma mark SecureTransport support

#if 0
static void hexdump(const uint8_t *bytes, size_t len) {
    size_t ix;
    printf("socket write(%p, %lu)\n", bytes, len);
    for (ix = 0; ix < len; ++ix) {
        if (!(ix % 16))
            printf("\n");
        printf("%02X ", bytes[ix]);
    }
    printf("\n");
}
#else
#define hexdump(bytes, len)
#endif


static OSStatus SocketWrite(SSLConnectionRef h, const void *data, size_t *length)
{
    size_t len = *length;
    uint8_t *ptr = (uint8_t *)data;

    do {
        ssize_t ret;
        do {
            hexdump(ptr, len);
            ret = write((int)h, ptr, len);
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
    size_t len = *length;
    uint8_t *ptr = (uint8_t *)data;

    do {
        ssize_t ret;
        do {
            ret = read((int)h, ptr, len);
        } while ((ret < 0) && (errno == EAGAIN || errno == EINTR));
        if (ret > 0) {
            len -= ret;
            ptr += ret;
        } else {
            printf("read error(%d): ret=%zd, errno=%d\n", (int)h, ret, errno);
            return -errno;
        }
    } while (len > 0);

    *length = *length - len;
    return errSecSuccess;
}

static char peername[] = "localhost";

static void *securetransport_server_thread(void *arg)
{
    OSStatus ortn;
    ssl_test_handle * ssl = (ssl_test_handle *)arg;
    SSLContextRef ctx = ssl->handle;
    CFArrayRef server_certs = server_chain();

    do {
        ortn = SSLHandshake(ctx);
    } while (ortn == errSSLWouldBlock);

    ok(ortn==errSSLClientHelloReceived, "Unexpected Handshake exit code");

    if (ortn == errSSLClientHelloReceived) {
        char *sni = NULL;
        size_t length = 0;
        SSLCopyRequestedPeerNameLength(ctx, &length);
        if (length > 0) {
            sni = malloc(length);
            SSLCopyRequestedPeerName(ctx, sni, &length);
        }

        SSLProtocol version = 0;
        require_noerr(SSLGetProtocolVersionMax(ctx, &version), out);
        if (version == kSSLProtocol3) {
            ok(sni==NULL, "Unexpected SNI");
        } else {
            ok(sni!=NULL &&
               length == sizeof(peername) &&
               (memcmp(sni, peername, sizeof(peername))==0),
               "SNI does not match");
        }
        require_noerr(SSLSetCertificate(ctx, server_certs), out);
        free(sni);
    }

out:
    SSLClose(ctx);
    SSLDisposeContext(ctx);
    close(ssl->comm);
    CFReleaseSafe(server_certs);

    pthread_exit((void *)(intptr_t)ortn);
    return NULL;
}

static void *securetransport_client_thread(void *arg)
{
    OSStatus ortn;
    ssl_test_handle * ssl = (ssl_test_handle *)arg;
    SSLContextRef ctx = ssl->handle;

    do {
        ortn = SSLHandshake(ctx);
    } while (ortn == errSSLWouldBlock || ortn != errSSLClosedGraceful);

    SSLClose(ctx);
    SSLDisposeContext(ctx);
    close(ssl->comm);

    pthread_exit((void *)(intptr_t)ortn);
    return NULL;
}

static SSLCipherSuite ciphers[] = {
    TLS_RSA_WITH_AES_128_CBC_SHA,
    //FIXME: re-enable this test when its fixed.
    //TLS_RSA_WITH_RC4_128_SHA,
};

static ssl_test_handle *
ssl_test_handle_create(uint32_t session_id, bool server, int comm)
{
    ssl_test_handle *handle = calloc(1, sizeof(ssl_test_handle));
    SSLContextRef ctx = SSLCreateContext(kCFAllocatorDefault, server?kSSLServerSide:kSSLClientSide, kSSLStreamType);

    require(handle, out);
    require(ctx, out);

    require_noerr(SSLSetIOFuncs(ctx,
                                (SSLReadFunc)SocketRead, (SSLWriteFunc)SocketWrite), out);
    require_noerr(SSLSetConnection(ctx, (SSLConnectionRef)(intptr_t)comm), out);

    if (server)
        require_noerr(SSLSetSessionOption(ctx,
                                          kSSLSessionOptionBreakOnClientHello, true), out);
    else
        require_noerr(SSLSetSessionOption(ctx,
                                          kSSLSessionOptionBreakOnServerAuth, true), out);

    /* Tell SecureTransport to not check certs itself: it will break out of the
     handshake to let us take care of it instead. */
    require_noerr(SSLSetEnableCertVerify(ctx, false), out);

    handle->handle = ctx;
    handle->is_server = server;
    handle->session_id = session_id;
    handle->comm = comm;

    return handle;

out:
    if (handle) free(handle);
    if (ctx) CFRelease(ctx);
    return NULL;
}

static SSLProtocol versions[] = {
    kSSLProtocol3,
    kTLSProtocol1,
    kTLSProtocol11,
    kTLSProtocol12,
};
static int nversions = sizeof(versions)/sizeof(versions[0]);

static void
tests(void)
{
    int j;
    pthread_t client_thread, server_thread;

    for(j=0; j<nversions; j++)
    {
        int sp[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp)) exit(errno);

        ssl_test_handle *server, *client;

        uint32_t session_id = (j+1) << 16 | 1 << 8;
        server = ssl_test_handle_create(session_id, true /*server*/, sp[0]);
        client = ssl_test_handle_create(session_id, false/*client*/, sp[1]);

        require_noerr(SSLSetPeerID(server->handle, &session_id, sizeof(session_id)), out);
        require_noerr(SSLSetPeerID(client->handle, &session_id, sizeof(session_id)), out);

        /* set fixed cipher on client and server */
        require_noerr(SSLSetEnabledCiphers(client->handle, &ciphers[0], 1), out);
        require_noerr(SSLSetEnabledCiphers(server->handle, &ciphers[0], 1), out);

        require_noerr(SSLSetProtocolVersionMax(client->handle, versions[j]), out);
        require_noerr(SSLSetPeerDomainName(client->handle, peername, sizeof(peername)), out);

        require_noerr(SSLSetProtocolVersionMax(server->handle, versions[j]), out);

        pthread_create(&client_thread, NULL, securetransport_client_thread, client);
        pthread_create(&server_thread, NULL, securetransport_server_thread, server);

        int server_err, client_err;
        pthread_join(client_thread, (void*)&client_err);
        pthread_join(server_thread, (void*)&server_err);

    out:
        free(client);
        free(server);

    }
}

int ssl_49_sni(int argc, char *const *argv)
{

    plan_tests(8);


    tests();

    return 0;
}
