/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */



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
    SSLContextRef st;
    bool is_server;
    int comm;
    CFArrayRef certs;
    int test;
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
    int conn = ((const ssl_test_handle *)h)->comm;
	size_t len = *length;
	uint8_t *ptr = (uint8_t *)data;

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

static int changepad=0;

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

    ptr=data;

    /* change pad in the data */
    if(changepad==1) {
        changepad=0;
        ptr[31]=ptr[31]^0x08^0xff; // expected padding was 8, changing it to 0xff to trigger integer underflow.
    }

    /* We are reading a data application header */
    if(*length==5 && ptr[0]==0x17) {
        switch(handle->test) {
            case 0:
                ptr[4]=32; // reduce the size to 2 blocks and trigger integer underflow.
                break;
            case 1:
                ptr[4]=48; // reduce the size to 3 blocks and triggering integer underflow in the padding.
                break;
            case 2:
                changepad=1;
                break;
            default:
                break;
        }
    }


    return errSecSuccess;
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

    unsigned char ibuf[8], obuf[8];
    size_t len;
    if (ssl->is_server) {
        SecRandomCopyBytes(kSecRandomDefault, sizeof(obuf), obuf);
        require_noerr_quiet(ortn = SSLWrite(ctx, obuf, sizeof(obuf), &len), out);
        require_action_quiet(len == sizeof(obuf), out, ortn = -1);
    } else {
        require_noerr_quiet(ortn = SSLRead(ctx, ibuf, sizeof(ibuf), &len), out);
        require_action_quiet(len == sizeof(ibuf), out, ortn = -1);
    }

out:
    SSLClose(ctx);
    CFRelease(ctx);
    close(ssl->comm);
    pthread_exit((void *)(intptr_t)ortn);
    return NULL;
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

    return handle;

out:
    if (handle) free(handle);
    if (ctx) CFRelease(ctx);
    return NULL;
}

static void
tests(void)
{
    pthread_t client_thread, server_thread;
    CFArrayRef server_certs = server_chain();
    ok(server_certs, "got server certs");

    int i;

    for(i=0; i<3; i++)
    {
        int sp[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp)) exit(errno);
        fcntl(sp[0], F_SETNOSIGPIPE, 1);
        fcntl(sp[1], F_SETNOSIGPIPE, 1);

        ssl_test_handle *server, *client;

        server = ssl_test_handle_create(true /*server*/, sp[0], server_certs);
        client = ssl_test_handle_create(false/*client*/, sp[1], NULL);

        server->test=i;
        client->test=i;

        require(client, out);
        require(server, out);

        SSLCipherSuite cipher = TLS_RSA_WITH_AES_128_CBC_SHA256;
        require_noerr(SSLSetEnabledCiphers(client->st, &cipher, 1), out);

        pthread_create(&client_thread, NULL, securetransport_ssl_thread, client);
        pthread_create(&server_thread, NULL, securetransport_ssl_thread, server);

        int server_err, client_err;
        pthread_join(client_thread, (void*)&client_err);
        pthread_join(server_thread, (void*)&server_err);


        ok(!server_err, "Server error = %d", server_err);
        /* tests 0/1 should cause errSSLClosedAbort, 2 should cause errSSLBadRecordMac */
        ok(client_err==((i==2)?errSSLBadRecordMac:errSSLClosedAbort), "Client error = %d", client_err);

out:
        free(client);
        free(server);

    }
    CFReleaseNull(server_certs);
}

int ssl_44_crashes(int argc, char *const *argv)
{

    plan_tests(3*2 + 1 /*cert*/);


    tests();

    return 0;
}
