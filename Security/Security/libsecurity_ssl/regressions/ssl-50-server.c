
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

#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>


#if TARGET_OS_IPHONE
#include <Security/SecRSAKey.h>
#endif

#include "ssl_regressions.h"
#include "ssl-utils.h"

typedef struct {
    SSLContextRef st;
    int comm;
    CFArrayRef certs;
} ssl_test_handle;


// MARK: -
// MARK: SecureTransport support

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


/* Listen to on port */
static int SocketListen(int port)
{
    struct sockaddr_in  sa;
    int					sock;
    int                 val  = 1;

    if ((sock=socket(AF_INET, SOCK_STREAM, 0))==-1) {
        perror("socket");
        return -errno;
    }

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&val, sizeof(val));

    memset((char *) &sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind (sock, (struct sockaddr *)&sa, sizeof(sa))==-1)
    {
        perror("bind");
        return -errno;
    }

    if(listen(sock, 5)==-1)
    {
        perror("listen");
        return -errno;
    }

    return sock;
}


static OSStatus SocketWrite(SSLConnectionRef conn, const void *data, size_t *length)
{
	size_t len = *length;
	uint8_t *ptr = (uint8_t *)data;

    do {
        ssize_t ret;
        do {
            hexdump(ptr, len);
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


static SSLContextRef make_ssl_ref(bool server, int sock, CFArrayRef certs)
{
    SSLContextRef ctx = SSLCreateContext(kCFAllocatorDefault, server?kSSLServerSide:kSSLClientSide, kSSLStreamType);
    require(ctx, out);

    require_noerr(SSLSetIOFuncs(ctx, (SSLReadFunc)SocketRead, (SSLWriteFunc)SocketWrite), out);
    require_noerr(SSLSetConnection(ctx, (SSLConnectionRef)(intptr_t)sock), out);
    require_noerr(SSLSetCertificate(ctx, certs), out);

    return ctx;
out:
    if (ctx)
        CFRelease(ctx);
    return NULL;
}


static ssl_test_handle *
ssl_test_handle_create(int comm, CFArrayRef certs)
{
    ssl_test_handle *handle = calloc(1, sizeof(ssl_test_handle));
    if (handle) {
        handle->comm = comm;
        handle->certs = certs;
        handle->st = make_ssl_ref(true, comm, certs);
    }
    return handle;
}

static void *securetransport_ssl_thread(void *arg)
{
    OSStatus ortn;
    int sock = (int)arg;

    int socket = accept(sock, NULL, NULL);

    CFArrayRef server_certs = server_chain();
    ssl_test_handle * ssl = ssl_test_handle_create(socket, server_certs);
    SSLContextRef ctx = ssl->st;

    pthread_setname_np("server thread");

    //uint64_t start = mach_absolute_time();
    do {
        ortn = SSLHandshake(ctx);
    } while (ortn == errSSLWouldBlock);

    require_noerr_action_quiet(ortn, out,
                               fprintf(stderr, "Fell out of SSLHandshake with error: %d\n", (int)ortn));

    //uint64_t elapsed = mach_absolute_time() - start;
    //fprintf(stderr, "setr elapsed: %lld\n", elapsed);

    /*
    SSLProtocol proto = kSSLProtocolUnknown;
    require_noerr_quiet(SSLGetNegotiatedProtocolVersion(ctx, &proto), out); */

    SSLCipherSuite cipherSuite;
    require_noerr_quiet(ortn = SSLGetNegotiatedCipher(ctx, &cipherSuite), out);
    //fprintf(stderr, "st negotiated %s\n", sslcipher_itoa(cipherSuite));


out:
    CFRelease(server_certs);

    SSLClose(ctx);
    CFRelease(ctx);
    if(ssl) {
        close(ssl->comm);
        free(ssl);
    }
    pthread_exit((void *)(intptr_t)ortn);
    return NULL;
}



static void
tests(void)
{
    pthread_t server_thread;
    int socket;

    socket = SocketListen(4443);

    ok(socket>=0, "SocketListen failed");
    if(socket<0) {
        return;
    }
    //fprintf(stderr, "session_id: %d\n", session_id);

    pthread_create(&server_thread, NULL, securetransport_ssl_thread, socket);

    system("/usr/bin/openssl s_client -msg -debug -connect localhost:4443");

    int server_err;
    pthread_join(server_thread, (void*)&server_err);

    ok(!server_err, "Server thread failed err=%d", server_err);
}

int ssl_50_server(int argc, char *const *argv)
{

    plan_tests(1 + 1 /*cert*/);


    tests();

    return 0;
}
