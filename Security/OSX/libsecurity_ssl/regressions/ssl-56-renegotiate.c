
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
    SSL Renegotiation tests:

    Test both the client and server side.

    Test Goal:
      - Make sure that renegotiation works on both client and server.

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
    bool renegotiate;
} ssl_client_handle;

static ssl_client_handle *
ssl_client_handle_create(int comm, bool renegotiate)
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

    require_noerr(SSLSetSessionOption(ctx, kSSLSessionOptionBreakOnServerAuth, TRUE), out);

    require_noerr(SSLSetAllowsAnyRoot(ctx, TRUE), out);


    handle->comm = comm;
    handle->st = ctx;
    handle->renegotiate = renegotiate;

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
    bool peer_auth_received = false;

    pthread_setname_np("client thread");

    require_noerr(ortn=SSLGetSessionState(ctx,&ssl_state), out);
    require_action(ssl_state==kSSLIdle, out, ortn = -1);

    do {
        ortn = SSLHandshake(ctx);
        require_noerr(SSLGetSessionState(ctx,&ssl_state), out);

        if (ortn == errSSLPeerAuthCompleted) {
            require_action(!peer_auth_received, out, ortn = -1);
            peer_auth_received = true;
        }
        if (ortn == errSSLWouldBlock) {
            require_string(ssl_state==kSSLHandshake, out, "Wrong client handshake state after errSSLWouldBlock");
        }
    } while (ortn == errSSLWouldBlock || ortn == errSSLPeerAuthCompleted);

    require_noerr(ortn, out);
    require_action(ssl_state==kSSLConnected, out, ortn = -1);
    require_action(peer_auth_received, out, ortn = -1);

    if(ssl->renegotiate) {
        // Renegotiate then write
        require_noerr(SSLReHandshake(ctx), out);

        peer_auth_received = false;

        do {
            ortn = SSLHandshake(ctx);
            require_noerr(SSLGetSessionState(ctx,&ssl_state), out);
            if (ortn == errSSLPeerAuthCompleted) {
                require_action(!peer_auth_received, out, ortn = -1);
                peer_auth_received = true;
            }
            if (ortn == errSSLWouldBlock) {
                require_action(ssl_state==kSSLHandshake, out, ortn = -1);
            }
        } while (ortn == errSSLWouldBlock || ortn == errSSLPeerAuthCompleted);

        require_noerr(ortn, out);
        require_action(ssl_state==kSSLConnected, out, ortn = -1);
        require_action(peer_auth_received, out, ortn = -1);

        unsigned char obuf[100];

        size_t len = sizeof(obuf);
        size_t olen;
        unsigned char *p = obuf;

        require_action(errSecSuccess==SecRandomCopyBytes(kSecRandomDefault, len, p), out, ortn = -1);

        while (len) {
            require_noerr(ortn = SSLWrite(ctx, p, len, &olen), out);
            len -= olen;
            p += olen;
        }
    } else {
        // just read.
        unsigned char ibuf[100];

        peer_auth_received = false;

        size_t len = sizeof(ibuf);
        size_t olen;
        unsigned char *p = ibuf;
        while (len) {
            ortn = SSLRead(ctx, p, len, &olen);

            require_noerr(SSLGetSessionState(ctx,&ssl_state), out);

            if (ortn == errSSLPeerAuthCompleted) {
                require_action(!peer_auth_received, out, ortn = -1);
                peer_auth_received = true;
            } else {
                require_noerr(ortn, out);
            }

            /* If we get data, we should have renegotiated */
            if(olen) {
                require_noerr(ortn, out);
                require_action(ssl_state==kSSLConnected, out, ortn = -1);
                require_action(peer_auth_received, out, ortn = -1);
            }

            len -= olen;
            p += olen;
        }
    }

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
    bool renegotiate;
} ssl_server_handle;

static ssl_server_handle *
ssl_server_handle_create(int comm, CFArrayRef certs, bool renegotiate)
{
    ssl_server_handle *handle = calloc(1, sizeof(ssl_server_handle));
    SSLContextRef ctx = SSLCreateContext(kCFAllocatorDefault, kSSLServerSide, kSSLStreamType);
    SSLCipherSuite cipher = TLS_RSA_WITH_AES_256_CBC_SHA256;

    require(handle, out);
    require(ctx, out);

    require_noerr(SSLSetIOFuncs(ctx,
                                (SSLReadFunc)SocketRead, (SSLWriteFunc)SocketWrite), out);
    require_noerr(SSLSetConnection(ctx, (SSLConnectionRef)(intptr_t)comm), out);

    require_noerr(SSLSetCertificate(ctx, certs), out);

    require_noerr(SSLSetEnabledCiphers(ctx, &cipher, 1), out);

    require_noerr(SSLSetSessionOption(ctx, kSSLSessionOptionBreakOnClientHello, TRUE), out);
    require_noerr(SSLSetSessionOption(ctx, kSSLSessionOptionAllowRenegotiation, TRUE), out);

    handle->comm = comm;
    handle->certs = certs;
    handle->st = ctx;
    handle->renegotiate = renegotiate;

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
    bool client_hello_received = false;

    pthread_setname_np("server thread");

    require_noerr(ortn=SSLGetSessionState(ctx,&ssl_state), out);
    require_action(ssl_state==kSSLIdle, out, ortn = -1);

    do {
        ortn = SSLHandshake(ctx);
        require_noerr(SSLGetSessionState(ctx,&ssl_state), out);
        if (ortn == errSSLClientHelloReceived) {
            require_action(!client_hello_received, out, ortn = -1);
            client_hello_received = true;
        }
        if (ortn == errSSLWouldBlock) {
            require_action(ssl_state==kSSLHandshake, out, ortn = -1);
        }
    } while (ortn == errSSLWouldBlock || ortn == errSSLClientHelloReceived);

    require_noerr(ortn, out);
    require_action(ssl_state==kSSLConnected, out, ortn = -1);
    require_action(client_hello_received, out, ortn = -1);

    if(ssl->renegotiate) {
        // Renegotiate then write
        require_noerr(SSLReHandshake(ctx), out);

        client_hello_received = false;

        do {
            ortn = SSLHandshake(ctx);
            require_noerr(SSLGetSessionState(ctx,&ssl_state), out);
            if (ortn == errSSLClientHelloReceived) {
                require_action(!client_hello_received, out, ortn = -1);
                client_hello_received = true;
            }
            if (ortn == errSSLWouldBlock) {
                require_action(ssl_state==kSSLHandshake, out, ortn = -1);
            }
        } while (ortn == errSSLWouldBlock || ortn == errSSLClientHelloReceived);

        require_noerr(ortn, out);
        require_action(ssl_state==kSSLConnected, out, ortn = -1);
        require_action(client_hello_received, out, ortn = -1);

        unsigned char obuf[100];

        size_t len = sizeof(obuf);
        size_t olen;
        unsigned char *p = obuf;

        require_action(errSecSuccess==SecRandomCopyBytes(kSecRandomDefault, len, p), out, ortn = -1);

        while (len) {
            require_noerr(ortn = SSLWrite(ctx, p, len, &olen), out);
            len -= olen;
            p += olen;
        }
    } else {
        // just read
        unsigned char ibuf[100];

        client_hello_received = false;

        size_t len = sizeof(ibuf);
        size_t olen;
        unsigned char *p = ibuf;
        while (len) {
            ortn = SSLRead(ctx, p, len, &olen);

            require_noerr(SSLGetSessionState(ctx,&ssl_state), out);

            if (ortn == errSSLClientHelloReceived) {
                require_action(!client_hello_received, out, ortn = -1);
                client_hello_received = true;
            } else {
                require_noerr(ortn, out);
            }

            /* If we get data, we should have renegotiated */
            if(olen) {
                require_noerr(ortn, out);
                require_action(ssl_state==kSSLConnected, out, ortn = -1);
                require_action(client_hello_received, out, ortn = -1);
            }

            len -= olen;
            p += olen;
        }
    }

out:
    SSLClose(ssl->st);
    close(ssl->comm);
    pthread_exit((void *)(intptr_t)ortn);
    return NULL;
}


static void
test_renego(bool client_renego)
{
    pthread_t client_thread, server_thread;
    CFArrayRef server_certs = server_chain();

    ok(server_certs, "renego: got server certs");


    int sp[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp)) exit(errno);
    fcntl(sp[0], F_SETNOSIGPIPE, 1);
    fcntl(sp[1], F_SETNOSIGPIPE, 1);

    ssl_client_handle *client;
    client = ssl_client_handle_create(sp[0], client_renego);
    ok(client!=NULL, "renego: could not create client handle");


    ssl_server_handle *server;
    server = ssl_server_handle_create(sp[1], server_certs, !client_renego);
    ok(server!=NULL, "renego: could not create server handle");

    pthread_create(&client_thread, NULL, securetransport_ssl_client_thread, client);
    pthread_create(&server_thread, NULL, securetransport_ssl_server_thread, server);

    intptr_t server_err, client_err;

    pthread_join(client_thread, (void*)&client_err);
    pthread_join(server_thread, (void*)&server_err);

    ok(client_err==0, "renego: unexpected error %ld (client)", client_err);
    ok(server_err==0, "renego: unexpected error %ld (server)", server_err);

    ssl_server_handle_destroy(server);
    ssl_client_handle_destroy(client);


    CFReleaseSafe(server_certs);
}


int ssl_56_renegotiate(int argc, char *const *argv)
{
    plan_tests(10);

    test_renego(false); // server side trigger renego.
    test_renego(true); // client side trigger renego.

    return 0;
}
