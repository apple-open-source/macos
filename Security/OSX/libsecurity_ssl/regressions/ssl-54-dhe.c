
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
    SSL DHE tests:

    Test both the client and server side.

    Test Goal:
      - Make sure that handshake fail when dh param size is too small.

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
ssl_client_handle_create(int comm, CFArrayRef trustedCA, unsigned dhe_size)
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

    require_noerr(SSLSetDHEEnabled(ctx, true), out);

    if(dhe_size)
        require_noerr(SSLSetMinimumDHGroupSize(ctx, dhe_size), out);

    handle->comm = comm;
    handle->st = ctx;
    handle->dhe_size = dhe_size;

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
ssl_server_handle_create(int comm, CFArrayRef certs, const void *dhParams, size_t dhParamsLen)
{
    ssl_server_handle *handle = calloc(1, sizeof(ssl_server_handle));
    SSLContextRef ctx = SSLCreateContext(kCFAllocatorDefault, kSSLServerSide, kSSLStreamType);
    SSLCipherSuite cipher = TLS_DHE_RSA_WITH_AES_256_CBC_SHA256;

    require(handle, out);
    require(ctx, out);

    require_noerr(SSLSetIOFuncs(ctx,
                                (SSLReadFunc)SocketRead, (SSLWriteFunc)SocketWrite), out);
    require_noerr(SSLSetConnection(ctx, (SSLConnectionRef)(intptr_t)comm), out);

    require_noerr(SSLSetCertificate(ctx, certs), out);

    require_noerr(SSLSetEnabledCiphers(ctx, &cipher, 1), out);

    if(dhParams)
        require_noerr(SSLSetDiffieHellmanParams(ctx, dhParams, dhParamsLen), out);

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



static
unsigned client_dhe_sizes[] = {
    0,  // default, don't set.
    256, // will resolve to 512.
    512,
    768,
    1024,
    2048,
    4096, // will resolve to 2048.
};


static const unsigned n_client_dhe_sizes = sizeof(client_dhe_sizes)/sizeof(client_dhe_sizes[0]);

static uint8_t dh_parameters_256_data[] = {
    0x30, 0x26, 0x02, 0x21, 0x00, 0xd8, 0x23, 0xeb, 0xcb, 0x41, 0xd0, 0x3a,
    0xc4, 0x9a, 0x2a, 0x2a, 0x4f, 0x35, 0xf7, 0x4f, 0xd9, 0xc5, 0x2e, 0xf8,
    0x44, 0xa7, 0x74, 0xe3, 0x84, 0x98, 0x9f, 0xad, 0x58, 0xd5, 0x15, 0xb4,
    0xf3, 0x02, 0x01, 0x02
};

static uint8_t dh_parameters_512_data[] = {
    0x30, 0x46, 0x02, 0x41, 0x00, 0x85, 0xcd, 0xc1, 0x7e, 0x26, 0xeb, 0x37,
    0x84, 0x13, 0xd0, 0x3b, 0x07, 0xc1, 0x57, 0x7d, 0xf3, 0x55, 0x8d, 0xa0,
    0xc4, 0xa5, 0x03, 0xc4, 0x2c, 0xc6, 0xd5, 0xa6, 0x31, 0xcb, 0x68, 0xdf,
    0x5d, 0x96, 0x20, 0x1a, 0x15, 0x57, 0x49, 0x7d, 0xd7, 0x51, 0x65, 0x6e,
    0x37, 0xa8, 0xe3, 0xe9, 0xe1, 0x59, 0x2e, 0xd4, 0x57, 0x4a, 0xf0, 0xcb,
    0x0e, 0x85, 0x07, 0xdd, 0x35, 0xa7, 0xe3, 0xc6, 0xbb, 0x02, 0x01, 0x02
};

static uint8_t dh_parameters_768_data[] = {
    0x30, 0x66, 0x02, 0x61, 0x00, 0xe1, 0xa2, 0x50, 0xab, 0xb0, 0xdc, 0xef,
    0xe1, 0x2f, 0xd9, 0xde, 0x59, 0x86, 0x24, 0x43, 0x3b, 0xf3, 0x40, 0x9d,
    0x02, 0xcc, 0xe2, 0x70, 0x63, 0x46, 0x8d, 0x0f, 0xf3, 0x8a, 0xc6, 0xa0,
    0x1d, 0x7b, 0x30, 0x83, 0x10, 0x48, 0x40, 0x28, 0xa4, 0x3e, 0xbe, 0x4d,
    0xb6, 0xea, 0x90, 0x02, 0xae, 0x25, 0x93, 0xc0, 0xe8, 0x36, 0x5c, 0xc8,
    0xc8, 0x0b, 0x04, 0xd5, 0x05, 0xac, 0x67, 0x24, 0x4b, 0xa9, 0x42, 0x5a,
    0x03, 0x65, 0x4d, 0xd0, 0xc0, 0xbd, 0x78, 0x32, 0xd0, 0x8c, 0x0a, 0xf4,
    0xbf, 0xd1, 0x61, 0x86, 0x13, 0x13, 0x3b, 0x83, 0xce, 0xbf, 0x3b, 0xbc,
    0x8f, 0xf9, 0x4e, 0x50, 0xe3, 0x02, 0x01, 0x02
};

static uint8_t dh_parameters_1024_data[] = {
    0x30, 0x81, 0x87, 0x02, 0x81, 0x81, 0x00, 0xd5, 0x06, 0x69, 0xc6, 0xd4,
    0x98, 0x2b, 0xe3, 0x49, 0xe2, 0xa1, 0x9b, 0x82, 0xaf, 0x3f, 0xaa, 0xc3,
    0x86, 0x2a, 0x7a, 0xfa, 0x62, 0x12, 0x33, 0x45, 0x9f, 0x34, 0x57, 0xc6,
    0x6c, 0x88, 0x81, 0xa6, 0x5d, 0xa3, 0x43, 0xe5, 0x4d, 0x87, 0x4f, 0x69,
    0x3d, 0x2b, 0xc8, 0x18, 0xb6, 0xd7, 0x29, 0x53, 0x94, 0x0d, 0x73, 0x9b,
    0x08, 0x22, 0x73, 0x84, 0x7b, 0x5a, 0x03, 0x2e, 0xfc, 0x10, 0x9b, 0x35,
    0xc6, 0xa1, 0xca, 0x36, 0xd0, 0xcc, 0x3e, 0xa2, 0x04, 0x3a, 0x8a, 0xe8,
    0x87, 0xe8, 0x60, 0x72, 0xee, 0x99, 0xf3, 0x04, 0x0a, 0xd8, 0x1a, 0xe6,
    0xfc, 0xbc, 0xe1, 0xc5, 0x9d, 0x3a, 0xca, 0xf9, 0xfd, 0xbf, 0x58, 0xd3,
    0x4d, 0xde, 0x8b, 0x4a, 0xb5, 0x37, 0x1e, 0x6d, 0xf4, 0x22, 0x0f, 0xb7,
    0x48, 0x0a, 0xda, 0x82, 0x40, 0xc9, 0x55, 0x20, 0x01, 0x3b, 0x35, 0xb2,
    0x94, 0x68, 0xab, 0x02, 0x01, 0x02
};


static
struct {
    const void *dhParams;
    size_t dhParamsLen;
} server_dhe_params[] = {
    {dh_parameters_256_data, sizeof(dh_parameters_256_data)},
    {dh_parameters_512_data, sizeof(dh_parameters_512_data)},
    {dh_parameters_768_data, sizeof(dh_parameters_768_data)},
    {dh_parameters_1024_data, sizeof(dh_parameters_1024_data)},
    {NULL, 0}, // default is a 2048
};

static const unsigned n_server_dhe_params = sizeof(server_dhe_params)/sizeof(server_dhe_params[0]);

static
int expected_client_error[n_server_dhe_params][n_client_dhe_sizes] = {
//Client:
// (default)
//     1024,   512,   512,   768,  1024,  2048,  2048   // Server:
    { -9850, -9850, -9850, -9850, -9850, -9850, -9850}, //    256
    { -9850,     0,     0, -9850, -9850, -9850, -9850}, //    512
    { -9850,     0,     0,     0, -9850, -9850, -9850}, //    768
    {     0,     0,     0,     0,     0, -9850, -9850}, //   1024
    {     0,     0,     0,     0,     0,     0,     0}, //   default(2048)
};

static void
tests(void)
{
    pthread_t client_thread, server_thread;
    CFArrayRef server_certs = server_chain();
    CFArrayRef trusted_ca = trusted_roots();

    ok(server_certs, "got server certs");
    ok(trusted_ca, "got trusted roots");

    int i, j;

    for (i=0; i<n_server_dhe_params; i++) {
        for (j=0; j<n_client_dhe_sizes; j++) {

            int sp[2];
            if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp)) exit(errno);
            fcntl(sp[0], F_SETNOSIGPIPE, 1);
            fcntl(sp[1], F_SETNOSIGPIPE, 1);

            ssl_client_handle *client;
            client = ssl_client_handle_create(sp[0], trusted_ca, client_dhe_sizes[j]);
            ok(client!=NULL, "could not create client handle (%d:%d)", i, j);


            ssl_server_handle *server;
            server = ssl_server_handle_create(sp[1], server_certs, server_dhe_params[i].dhParams, server_dhe_params[i].dhParamsLen);
            ok(server!=NULL, "could not create server handle (%d:%d)", i, j);

            pthread_create(&client_thread, NULL, securetransport_ssl_client_thread, client);
            pthread_create(&server_thread, NULL, securetransport_ssl_server_thread, server);

            intptr_t server_err, client_err;

            pthread_join(client_thread, (void*)&client_err);
            pthread_join(server_thread, (void*)&server_err);


            ok(client_err==expected_client_error[i][j], "unexpected error %d!=%d (client %d:%d)", (int)client_err, expected_client_error[i][j], i, j);

            ssl_server_handle_destroy(server);
            ssl_client_handle_destroy(client);
        }
    }

    CFReleaseSafe(server_certs);
    CFReleaseSafe(trusted_ca);
}

int ssl_54_dhe(int argc, char *const *argv)
{

    plan_tests(n_server_dhe_params * n_client_dhe_sizes * 3 + 2 /*cert*/);


    tests();

    return 0;
}
