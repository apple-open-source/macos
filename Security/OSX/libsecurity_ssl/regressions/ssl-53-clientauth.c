
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
    SSL Client Auth tests:

    Test both the client and server side.

    Server side test goals:
    Verify Server behavior in the following cases:
    Server configuration:
      - when using kTryAuthenticate vs kAlwaysAuthenticate
      - with or without breakOnClientAuth.
      - AnonDH and PSK ciphersuites.
    Client configuration:
      - Client sends back no cert vs a cert.
      - Client sends back an unsupported cert.
      - Client sends back a malformed cert.
      - Client cert is trusted vs untrusted.
      - Client does not have private key (ie: Certificate Verify message should fail).
    Behavior to verify:
      - handshake pass or fail
      - SSLGetClientCertificateState returns expected results

    Client side test goals:
    Client configuration:
      - with or without breakOnCertRequest.
      - no cert, vs cert.
    Server config:
      - No client cert requested, vs client cert requested vs client cert required.
    Behavior to verify:
      - handshake pass or fail
      - SSLGetClientCertificateState returns expected results

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
    bool break_on_req;
    CFArrayRef certs;
    int auth; //expected client auth behavior of the server (0=no request, 1=optional , 2=required)
} ssl_client_handle;

static ssl_client_handle *
ssl_client_handle_create(bool break_on_req, int comm, CFArrayRef certs, CFArrayRef trustedCA, int auth)
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

    require_noerr(SSLSetTrustedRoots(ctx, trustedCA, true), out);


    /* Setting client certificate in advance */
    if (!break_on_req && certs) {
        require_noerr(SSLSetCertificate(ctx, certs), out);
    }

    if (break_on_req) {
        require_noerr(SSLSetSessionOption(ctx,
                kSSLSessionOptionBreakOnCertRequested, true), out);
    }

    handle->break_on_req = break_on_req;
    handle->comm = comm;
    handle->certs = certs;
    handle->st = ctx;
    handle->auth = auth;

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
    bool got_client_cert_req = false;
    SSLSessionState ssl_state;

    pthread_setname_np("client thread");

    require_noerr(ortn=SSLGetSessionState(ctx,&ssl_state), out);
    require_action(ssl_state==kSSLIdle, out, ortn = -1);

    do {
        ortn = SSLHandshake(ctx);
        require_noerr(SSLGetSessionState(ctx,&ssl_state), out);

        if (ortn == errSSLClientCertRequested) {
            require_string(ssl->auth, out, "cert req not expected");
            require_string(ssl_state==kSSLHandshake, out, "wrong client handshake state after errSSLClientCertRequested");
            require_string(!got_client_cert_req, out, "second client cert req");
            got_client_cert_req = true;

            SSLClientCertificateState clientState;
            SSLGetClientCertificateState(ctx, &clientState);
            require_string(clientState==kSSLClientCertRequested, out, "Wrong client cert state after cert request");

            require_string(ssl->break_on_req, out, "errSSLClientCertRequested in run not testing that");
            if(ssl->certs) {
                require_noerr(SSLSetCertificate(ctx, ssl->certs), out);
            }

        } else if (ortn == errSSLWouldBlock) {
            require_string(ssl_state==kSSLHandshake, out, "Wrong client handshake state after errSSLWouldBlock");
        }
    } while (ortn == errSSLWouldBlock || ortn == errSSLClientCertRequested);

    require_string((got_client_cert_req || !ssl->auth || !ssl->break_on_req), out, "didn't get client cert req as expected");


out:
    SSLClose(ssl->st);
    close(ssl->comm);
    pthread_exit((void *)(intptr_t)ortn);
    return NULL;
}


typedef struct {
    SSLContextRef st;
    int comm;
    SSLAuthenticate client_auth;
    CFArrayRef certs;
} ssl_server_handle;

static ssl_server_handle *
ssl_server_handle_create(SSLAuthenticate client_auth, int comm, CFArrayRef certs, CFArrayRef trustedCA)
{
    ssl_server_handle *handle = calloc(1, sizeof(ssl_server_handle));
    SSLContextRef ctx = SSLCreateContext(kCFAllocatorDefault, kSSLServerSide, kSSLStreamType);

    require(handle, out);
    require(ctx, out);

    require_noerr(SSLSetIOFuncs(ctx,
                                (SSLReadFunc)SocketRead, (SSLWriteFunc)SocketWrite), out);
    require_noerr(SSLSetConnection(ctx, (SSLConnectionRef)(intptr_t)comm), out);

    require_noerr(SSLSetCertificate(ctx, certs), out);

    require_noerr(SSLSetTrustedRoots(ctx, trustedCA, true), out);

    SSLAuthenticate auth;
    require_noerr(SSLSetClientSideAuthenticate(ctx, client_auth), out);
    require_noerr(SSLGetClientSideAuthenticate(ctx, &auth), out);
    require(auth==client_auth, out);

    handle->client_auth = client_auth;
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
tests(void)
{
    pthread_t client_thread, server_thread;
    CFArrayRef server_certs = server_chain();
    CFArrayRef trusted_ca = trusted_roots();

    ok(server_certs, "got server certs");
    ok(trusted_ca, "got trusted roots");

    int i, j, k;

    for (i=0; i<3; i++) {/* client side cert: 0 = no cert, 1 = trusted cert, 2 = untrusted cert. */
        for (j=0; j<2; j++) { /* break on cert request */
            for (k=0; k<3; k++) { /* server behvior: 0 = no cert request, 1 = optional cert, 2 = required cert */

                int sp[2];
                if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp)) exit(errno);
                fcntl(sp[0], F_SETNOSIGPIPE, 1);
                fcntl(sp[1], F_SETNOSIGPIPE, 1);

                bool break_on_req = (j!=0);
                SSLClientAuthenticationType auth = (k == 0) ? kNeverAuthenticate
                                                   : (k == 1) ? kTryAuthenticate
                                                   : kAlwaysAuthenticate;

                CFArrayRef client_certs = (i == 0) ? NULL
                                          : (i == 1) ? trusted_client_chain()
                                          : untrusted_client_chain();

                ssl_client_handle *client;
                client = ssl_client_handle_create(break_on_req, sp[0], client_certs, trusted_ca, auth);

                ssl_server_handle *server;
                server = ssl_server_handle_create(auth, sp[1], server_certs, trusted_ca);

                pthread_create(&client_thread, NULL, securetransport_ssl_client_thread, client);
                pthread_create(&server_thread, NULL, securetransport_ssl_server_thread, server);

                intptr_t server_err, client_err;
                int expected_server_err = 0, expected_client_err = 0;
                SSLClientCertificateState client_cauth_state, server_cauth_state;
                SSLClientCertificateState expected_client_cauth_state = 0, expected_server_cauth_state = 0;


                if(((k == 2) && (i != 1))     // Server requires good cert, but client not sending good cert,
                    || ((k == 1) && (i == 2)) // Or server request optionally, and client sending bad cert.
                   ) {
                    expected_client_err = errSSLPeerCertUnknown;
                    expected_server_err = errSSLXCertChainInvalid;
                }


                if(k != 0) {
                    if(i == 0) {
                        expected_client_cauth_state = kSSLClientCertRequested;
                        expected_server_cauth_state = kSSLClientCertRequested;
                    } else {
                        expected_client_cauth_state = kSSLClientCertSent;
                        expected_server_cauth_state = kSSLClientCertSent;
                    }
                }

                pthread_join(client_thread, (void*)&client_err);
                pthread_join(server_thread, (void*)&server_err);


                ok_status(SSLGetClientCertificateState(client->st, &client_cauth_state), "SSLGetClientCertificateState (client %d:%d:%d)", i, j, k);
                ok_status(SSLGetClientCertificateState(client->st, &server_cauth_state), "SSLGetClientCertificateState (server %d:%d:%d)", i, j, k);

                ok(client_err==expected_client_err, "unexpected error %d!=%d (client %d:%d:%d)", (int)client_err, expected_client_err, i, j, k);
                ok(server_err==expected_server_err, "unexpected error %d!=%d (server %d:%d:%d)", (int)server_err, expected_server_err, i, j, k);

                ok(client_cauth_state==expected_client_cauth_state, "unexpected client auth state %d!=%d (client %d:%d:%d)", client_cauth_state, expected_client_cauth_state, i, j, k);
                ok(server_cauth_state==expected_server_cauth_state, "unexpected client auth state %d!=%d (server %d:%d:%d)", server_cauth_state, expected_server_cauth_state, i, j, k);


                ssl_server_handle_destroy(server);
                ssl_client_handle_destroy(client);

                CFReleaseSafe(client_certs);
            }
        }
    }

    CFReleaseSafe(server_certs);
    CFReleaseSafe(trusted_ca);
}

int ssl_53_clientauth(int argc, char *const *argv)
{

    plan_tests(3 * 3 * 2 * 6 +  2 /*cert*/);


    tests();

    return 0;
}
