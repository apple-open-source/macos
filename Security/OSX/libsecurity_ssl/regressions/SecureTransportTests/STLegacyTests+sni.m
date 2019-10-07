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

#include <utilities/SecCFRelease.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <mach/mach_time.h>

#if TARGET_OS_IPHONE
#include <Security/SecRSAKey.h>
#endif

#include "ssl-utils.h"
#import "STLegacyTests.h"

#define serverSelectedProtocol "baz"
#define serverAdvertisedProtocols "\x03""baz"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

@implementation STLegacyTests (sni)

typedef struct {
    SSLContextRef handle;
    uint32_t session_id;
    bool is_server;
    int comm;
} ssl_test_handle;


#pragma mark -
#pragma mark SecureTransport support

#if SECTRANS_VERBOSE_DEBUG
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
            require_string(sni == NULL, out, "Unexpected SNI");
        } else {
            require_string(sni != NULL &&
               length == sizeof(peername) &&
               (memcmp(sni, peername, sizeof(peername))==0), out,
               "SNI does not match");
        }
        require_noerr(SSLSetCertificate(ctx, server_certs), out);
        free(sni);

        tls_buffer alpnData;
        alpnData.length = strlen(serverSelectedProtocol);
        alpnData.data = malloc(alpnData.length);
        memcpy(alpnData.data, serverSelectedProtocol, alpnData.length);
        require_noerr_string(SSLSetALPNData(ctx, alpnData.data, alpnData.length), out, "Error setting alpn data");
        free(alpnData.data);

        tls_buffer npnData;
        npnData.length = strlen(serverAdvertisedProtocols);
        npnData.data = malloc(npnData.length);
        memcpy(npnData.data, serverAdvertisedProtocols, npnData.length);
        require_noerr_string(SSLSetNPNData(ctx, npnData.data, npnData.length), out, "Error setting npn data");
        free(npnData.data);

    }

out:
    SSLClose(ctx);
    CFReleaseNull(ctx);
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

    size_t length = 0;
    uint8_t *alpnData = NULL;
    alpnData = (uint8_t*)SSLGetALPNData(ctx, &length);
    if (alpnData != NULL) {
        require_noerr(memcmp(alpnData, serverSelectedProtocol, strlen(serverSelectedProtocol)), out);
    }

    length = 0;
    uint8_t *npnData = NULL;
    npnData = (uint8_t*)SSLGetNPNData(ctx, &length);
    if (npnData != NULL) {
        require_noerr_string(memcmp(npnData, serverAdvertisedProtocols, strlen(serverAdvertisedProtocols)),
                             out, "npn Data received does not match");
    }
out:
    SSLClose(ctx);
    CFReleaseNull(ctx);
    close(ssl->comm);

    pthread_exit((void *)(intptr_t)ortn);
    return NULL;
}

static SSLCipherSuite ciphers[] = {
    TLS_RSA_WITH_AES_128_CBC_SHA,
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

-(void)testSNI
{
    int j;
    pthread_t client_thread, server_thread;

    for(j = 0; j < nversions; j++)
    {
        int sp[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp)) exit(errno);

        ssl_test_handle *server, *client;

        uint32_t session_id = (j+1) << 16 | 1 << 8;
        server = ssl_test_handle_create(session_id, true /*server*/, sp[0]);
        client = ssl_test_handle_create(session_id, false/*client*/, sp[1]);

        XCTAssertEqual(errSecSuccess, SSLSetPeerID(server->handle, &session_id, sizeof(session_id)));
        XCTAssertEqual(errSecSuccess, SSLSetPeerID(client->handle, &session_id, sizeof(session_id)));
        const void *inputPeerId = NULL;
        size_t inputPeerIdLen;
        XCTAssertEqual(errSecSuccess, SSLGetPeerID(client->handle, &inputPeerId, &inputPeerIdLen));

        /* set fixed cipher on client and server */
        XCTAssertEqual(errSecSuccess, SSLSetEnabledCiphers(client->handle, &ciphers[0], 1));
        XCTAssertEqual(errSecSuccess, SSLSetEnabledCiphers(server->handle, &ciphers[0], 1));

        XCTAssertEqual(errSecSuccess, SSLSetProtocolVersionMax(client->handle, versions[j]));
        XCTAssertEqual(errSecSuccess, SSLSetPeerDomainName(client->handle, peername, sizeof(peername)));

        XCTAssertEqual(errSecSuccess, SSLSetProtocolVersionMax(server->handle, versions[j]));

        pthread_create(&client_thread, NULL, securetransport_client_thread, client);
        pthread_create(&server_thread, NULL, securetransport_server_thread, server);

        intptr_t server_err, client_err;
        pthread_join(client_thread, (void*)&client_err);
        pthread_join(server_thread, (void*)&server_err);

        free(client);
        free(server);

    }
}

@end

#pragma clang diagnostic pop
