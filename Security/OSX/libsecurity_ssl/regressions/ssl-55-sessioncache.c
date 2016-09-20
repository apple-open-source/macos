
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

#include <utilities/array_size.h>
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

/*
    SSL Session Cache tests:

    Test both the client and server side.

    Test Goal:
      - Make sure that resumption fails after session cache TTL.

    Behavior to verify:
      - handshake pass or fail

*/


static OSStatus SocketWrite(SSLConnectionRef conn, const void *data, size_t *length)
{
	size_t len = *length;
	uint8_t *ptr = (uint8_t *)data;

    do {
        ssize_t ret;
        do {
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

static OSStatus SocketRead(SSLConnectionRef conn, void *data, size_t *length)
{
	size_t len = *length;
	uint8_t *ptr = (uint8_t *)data;

    do {
        ssize_t ret;
        do {
            ret = read((int)conn, ptr, len);
        } while ((ret < 0) && (errno == EINPROGRESS || errno == EAGAIN || errno == EINTR));
        if (ret > 0) {
            len -= ret;
            ptr += ret;
        } else {
            printf("read error(%d): ret=%zd, errno=%d\n", (int)conn, ret, errno);
            return -errno;
        }
    } while (len > 0);

    *length = *length - len;
    return errSecSuccess;
}

typedef struct {
    SSLContextRef st;
    int comm;
    unsigned dhe_size;
} ssl_client_handle;

static ssl_client_handle *
ssl_client_handle_create(int comm, bool anyRoot, CFArrayRef trustedCA, bool trustedCAOnly, CFArrayRef trustedLeafs, uint32_t cache_ttl, uintptr_t peerID)
{
    ssl_client_handle *handle = calloc(1, sizeof(ssl_client_handle));
    SSLContextRef ctx = SSLCreateContext(kCFAllocatorDefault, kSSLClientSide, kSSLStreamType);

    require(handle, out);
    require(ctx, out);

    require_noerr(SSLSetIOFuncs(ctx,
        (SSLReadFunc)SocketRead, (SSLWriteFunc)SocketWrite), out);
    require_noerr(SSLSetConnection(ctx, (SSLConnectionRef)(intptr_t)comm), out);
    static const char *peer_domain_name = "localhost";
    require_noerr(SSLSetPeerDomainName(ctx, peer_domain_name,
        strlen(peer_domain_name)), out);

    require_noerr(SSLSetAllowsAnyRoot(ctx, anyRoot), out);
    require_noerr(SSLSetTrustedRoots(ctx, trustedCA, trustedCAOnly), out);
#if !TARGET_OS_IPHONE
    require_noerr(SSLSetTrustedLeafCertificates(ctx, trustedLeafs), out);
#endif

    require_noerr(SSLSetSessionCacheTimeout(ctx, cache_ttl), out);

    require_noerr(SSLSetPeerID(ctx, &peerID, sizeof(peerID)), out);

    handle->comm = comm;
    handle->st = ctx;

    return handle;

out:
    if (ctx)
        CFRelease(ctx);
    if (handle)
        free(handle);

    return NULL;
}

static void
ssl_client_handle_destroy(ssl_client_handle *handle)
{
    if(handle) {
        SSLClose(handle->st);
        CFRelease(handle->st);
        free(handle);
    }
}

static void *securetransport_ssl_client_thread(void *arg)
{
    OSStatus ortn;
    ssl_client_handle * ssl = (ssl_client_handle *)arg;
    SSLContextRef ctx = ssl->st;
    SSLSessionState ssl_state;

    pthread_setname_np("client thread");

    require_noerr(ortn=SSLGetSessionState(ctx,&ssl_state), out);
    require_action(ssl_state==kSSLIdle, out, ortn = -1);

    do {
        ortn = SSLHandshake(ctx);
        require_noerr(SSLGetSessionState(ctx,&ssl_state), out);

        if (ortn == errSSLWouldBlock) {
            require_string(ssl_state==kSSLHandshake, out, "Wrong client handshake state after errSSLWouldBlock");
        }
    } while (ortn == errSSLWouldBlock);

out:
    SSLClose(ssl->st);
    close(ssl->comm);
    pthread_exit((void *)(intptr_t)ortn);
    return NULL;
}


typedef struct {
    SSLContextRef st;
    int comm;
    CFArrayRef certs;

} ssl_server_handle;

static ssl_server_handle *
ssl_server_handle_create(int comm, CFArrayRef certs, uint32_t cache_ttl)
{
    ssl_server_handle *handle = calloc(1, sizeof(ssl_server_handle));
    SSLContextRef ctx = SSLCreateContext(kCFAllocatorDefault, kSSLServerSide, kSSLStreamType);
    SSLCipherSuite cipher = TLS_RSA_WITH_AES_256_CBC_SHA256;
    uintptr_t peerID = 0xdeadbeef;

    require(handle, out);
    require(ctx, out);

    require_noerr(SSLSetIOFuncs(ctx,
                                (SSLReadFunc)SocketRead, (SSLWriteFunc)SocketWrite), out);
    require_noerr(SSLSetConnection(ctx, (SSLConnectionRef)(intptr_t)comm), out);

    require_noerr(SSLSetCertificate(ctx, certs), out);

    require_noerr(SSLSetEnabledCiphers(ctx, &cipher, 1), out);

    require_noerr(SSLSetSessionCacheTimeout(ctx, cache_ttl), out);

    require_noerr(SSLSetPeerID(ctx, &peerID, sizeof(peerID)), out);

    handle->comm = comm;
    handle->certs = certs;
    handle->st = ctx;

    return handle;

out:
    if (ctx)
        CFRelease(ctx);
    if (handle)
        free(handle);

    return NULL;
}

static void
ssl_server_handle_destroy(ssl_server_handle *handle)
{
    if(handle) {
        SSLClose(handle->st);
        CFRelease(handle->st);
        free(handle);
    }
}

static void *securetransport_ssl_server_thread(void *arg)
{
    OSStatus ortn;
    ssl_server_handle * ssl = (ssl_server_handle *)arg;
    SSLContextRef ctx = ssl->st;
    SSLSessionState ssl_state;

    pthread_setname_np("server thread");

    require_noerr(ortn=SSLGetSessionState(ctx,&ssl_state), out);
    require_action(ssl_state==kSSLIdle, out, ortn = -1);

    do {
        ortn = SSLHandshake(ctx);
        require_noerr(SSLGetSessionState(ctx,&ssl_state), out);

        if (ortn == errSSLWouldBlock) {
            require_action(ssl_state==kSSLHandshake, out, ortn = -1);
        }
    } while (ortn == errSSLWouldBlock);

    require_noerr_quiet(ortn, out);

    require_action(ssl_state==kSSLConnected, out, ortn = -1);

out:
    SSLClose(ssl->st);
    close(ssl->comm);
    pthread_exit((void *)(intptr_t)ortn);
    return NULL;
}


static void
tests_cache_ttl(void)
{
    pthread_t client_thread, server_thread;
    CFArrayRef server_certs = server_chain();
    CFArrayRef trusted_ca = trusted_roots();

    ok(server_certs, "ttl: got server certs");
    ok(trusted_ca, "ttl: got trusted roots");

    int i, j, k;

    for (i=0; i<2; i++) {  // client cache TTL
        for (j=0; j<2; j++) { // Server cache TTL
            for (k=0; k<2; k++) {
                ssl_client_handle *client = NULL;
                ssl_server_handle *server = NULL;

                int sp[2];
                if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp)) exit(errno);
                fcntl(sp[0], F_SETNOSIGPIPE, 1);
                fcntl(sp[1], F_SETNOSIGPIPE, 1);

                client = ssl_client_handle_create(sp[0], false, trusted_ca, true, NULL, i, (i<<8)|(j+1));
                ok(client!=NULL, "ttl: could not create client handle (%d:%d:%d)", i, j, k);
                require(client, errOut);

                server = ssl_server_handle_create(sp[1], server_certs, j);
                ok(server!=NULL, "ttl: could not create server handle (%d:%d:%d)", i, j, k);
                require(server, errOut);
                pthread_create(&client_thread, NULL, securetransport_ssl_client_thread, client);
                pthread_create(&server_thread, NULL, securetransport_ssl_server_thread, server);

                intptr_t server_err, client_err;

                pthread_join(client_thread, (void*)&client_err);
                pthread_join(server_thread, (void*)&server_err);

                Boolean resumed;
                unsigned char sessionID[32];
                size_t sessionIDLength = sizeof(sessionID);

                ok(client_err==0, "ttl: unexpected error %ld (client %d:%d:%d)", client_err, i, j, k);
                ok(server_err==0, "ttl: unexpected error %ld (server %d:%d:%d)", server_err, i, j, k);
                ok_status(SSLGetResumableSessionInfo(client->st, &resumed, sessionID, &sessionIDLength), "SSLGetResumableSessionInfo");

                ok((bool)resumed == (bool)(k && (!i) && (!j)), "ttl: Unexpected resumption state=%d (%d:%d:%d)", resumed, i, j, k);

            errOut:
                ssl_server_handle_destroy(server);
                ssl_client_handle_destroy(client);

                /* Sleep two seconds so that Session cache TTL can expire */
                sleep(2);
            }
        }
    }

    CFReleaseSafe(server_certs);
    CFReleaseSafe(trusted_ca);
}

static void
tests_cache_trust(void)
{
    pthread_t client_thread, server_thread;
    CFArrayRef server_certs = server_chain();
    CFArrayRef trusted_ca = trusted_roots();
    CFMutableArrayRef trusted_ca2 = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, trusted_ca);
    CFArrayAppendArray(trusted_ca2, trusted_ca, CFRangeMake(0, CFArrayGetCount(trusted_ca)));

    ok(server_certs, "trust: got server certs");
    ok(trusted_ca, "trust: got trusted roots");
    ok(trusted_ca2, "trust: got trusted roots extra");

    int any, ca, caonly, leaf, k;

    // Test cache and trust options:


    for (any=0; any<2; any++) // any root ?
    for (ca=0; ca<2; ca++) // trustedCA ?
    for (caonly=0; caonly<2; caonly++) // leaf>
#if TARGET_OS_IPHONE
    {
        leaf = 0;
#else
    for (leaf=0; leaf<2; leaf++)
    {
#endif
        // attempt initial connection, then resumed connection, but all with same peer id (0xdeadbeef)
        for (k=0; k<2; k++) {
            ssl_client_handle *client = NULL;
            ssl_server_handle *server = NULL;

            int sp[2];
            if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp)) exit(errno);
            fcntl(sp[0], F_SETNOSIGPIPE, 1);
            fcntl(sp[1], F_SETNOSIGPIPE, 1);

            client = ssl_client_handle_create(sp[0], any, ca?trusted_ca:trusted_ca2, caonly, leaf?NULL:trusted_ca, 300, 0xdeadbeef);
            ok(client!=NULL, "trust: could not create client handle (%d:%d:%d:%d:%d)", any, ca, caonly, leaf, k);
            require(client, errOut);

            server = ssl_server_handle_create(sp[1], server_certs, 300);
            ok(server!=NULL, "trust: could not create server handle (%d:%d:%d:%d:%d)", any, ca, caonly, leaf, k);
            require(server, errOut);

            pthread_create(&client_thread, NULL, securetransport_ssl_client_thread, client);
            pthread_create(&server_thread, NULL, securetransport_ssl_server_thread, server);

            intptr_t server_err, client_err;

            pthread_join(client_thread, (void*)&client_err);
            pthread_join(server_thread, (void*)&server_err);

            Boolean resumed;
            unsigned char sessionID[32];
            size_t sessionIDLength = sizeof(sessionID);

            ok(client_err==0, "trust: unexpected error %ld (client %d:%d:%d:%d:%d)", client_err, any, ca, caonly, leaf, k);
            ok(server_err==0, "trust: unexpected error %ld (server %d:%d:%d:%d:%d)", server_err, any, ca, caonly, leaf, k);
            ok_status(SSLGetResumableSessionInfo(client->st, &resumed, sessionID, &sessionIDLength), "SSLGetResumableSessionInfo");

            ok((bool)resumed == (bool)(k), "trust: Unexpected resumption state=%d (%d:%d:%d:%d:%d)", resumed, any, ca, caonly, leaf, k);

        errOut:
            ssl_server_handle_destroy(server);
            ssl_client_handle_destroy(client);

        }
    }

    CFReleaseSafe(server_certs);
    CFReleaseSafe(trusted_ca);
}

int ssl_55_sessioncache(int argc, char *const *argv)
{

#if TARGET_OS_IPHONE
#define N_TRUST_TESTS 8
#else
#define N_TRUST_TESTS 16
#endif

    plan_tests(/*ttl :*/ 6 * 8 + 2  + /* trust:*/ N_TRUST_TESTS*6*2 + 3);


    tests_cache_ttl();

    tests_cache_trust();

    return 0;
}
