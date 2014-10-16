/*
 * Copyright (c) 2011,2013-2014 Apple Inc. All Rights Reserved.
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
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <CoreFoundation/CoreFoundation.h>

#include <AssertMacros.h>
#include <Security/SecureTransportPriv.h> /* SSLSetOption */
#include <Security/SecureTransport.h>

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <mach/mach_time.h>


#include "ssl_regressions.h"


typedef struct {
    uint32_t session_id;
    bool is_session_resume;
    SSLContextRef st;
    bool is_server;
    bool client_side_auth;
    bool dh_anonymous;
    int comm;
    CFArrayRef certs;
} ssl_test_handle;



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

static int SocketConnect(const char *hostName, int port)
{
    struct sockaddr_in  addr;
    struct in_addr      host;
	int					sock;
    int                 err;
    struct hostent      *ent;

    if (hostName[0] >= '0' && hostName[0] <= '9')
    {
        host.s_addr = inet_addr(hostName);
    }
    else {
		unsigned dex;
#define GETHOST_RETRIES 5
		/* seeing a lot of soft failures here that I really don't want to track down */
		for(dex=0; dex<GETHOST_RETRIES; dex++) {
			if(dex != 0) {
				printf("\n...retrying gethostbyname(%s)", hostName);
			}
			ent = gethostbyname(hostName);
			if(ent != NULL) {
				break;
			}
		}
        if(ent == NULL) {
			printf("\n***gethostbyname(%s) returned: %s\n", hostName, hstrerror(h_errno));
            return -1;
        }
        memcpy(&host, ent->h_addr, sizeof(struct in_addr));
    }


    sock = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_addr = host;
    addr.sin_port = htons((u_short)port);

    addr.sin_family = AF_INET;
    err = connect(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));

    if(err!=0)
    {
        perror("connect failed");
        return err;
    }

    /* make non blocking */
    fcntl(sock, F_SETFL, O_NONBLOCK);


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
            if (ret < 0)
                perror("send");
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

static
OSStatus SocketRead(
                    SSLConnectionRef 	connection,
                    void 				*data,
                    size_t 				*dataLength)
{
    int fd = (int)connection;
    ssize_t len;
    
    len = read(fd, data, *dataLength);
   
    if(len<0) {
        int theErr = errno;
        switch(theErr) {
            case EAGAIN:
                //printf("SocketRead: EAGAIN\n");
                *dataLength=0;
                /* nonblocking, no data */
                return errSSLWouldBlock;
            default:
                perror("SocketRead");
                return -36;
        }
    }
    
    if(len<(ssize_t)*dataLength) {
        *dataLength=len;
        return errSSLWouldBlock;
    }
    
    return errSecSuccess;
}

static SSLContextRef make_ssl_ref(int sock, SSLProtocol maxprot, Boolean false_start)
{
    SSLContextRef ctx = NULL;

    require_noerr(SSLNewContext(false, &ctx), out);
    require_noerr(SSLSetIOFuncs(ctx,
                                (SSLReadFunc)SocketRead, (SSLWriteFunc)SocketWrite), out);
    require_noerr(SSLSetConnection(ctx, (SSLConnectionRef)(intptr_t)sock), out);

    require_noerr(SSLSetSessionOption(ctx,
                                      kSSLSessionOptionBreakOnServerAuth, true), out);
    
    require_noerr(SSLSetSessionOption(ctx,
                                      kSSLSessionOptionFalseStart, false_start), out);

    require_noerr(SSLSetProtocolVersionMax(ctx, maxprot), out);

    return ctx;
out:
    if (ctx)
        SSLDisposeContext(ctx);
    return NULL;
}

const char request[]="GET / HTTP/1.1\n\n";
char reply[2048];

static OSStatus securetransport(ssl_test_handle * ssl)
{
    OSStatus ortn;
    SSLContextRef ctx = ssl->st;
    SecTrustRef trust = NULL;
    bool got_server_auth = false, got_client_cert_req = false;

    ortn = SSLHandshake(ctx);
    //fprintf(stderr, "Fell out of SSLHandshake with error: %ld\n", (long)ortn);
    
    size_t sent, received;
    const char *r=request;
    size_t l=sizeof(request);

    do {
        
        ortn = SSLWrite(ctx, r, l, &sent);
        
        if(ortn == errSSLWouldBlock) {
                r+=sent;
                l-=sent;
        }
        
        if (ortn == errSSLServerAuthCompleted)
        {
            require_string(!got_server_auth, out, "second server auth");
            require_string(!got_client_cert_req, out, "got client cert req before server auth");
            got_server_auth = true;
            require_string(!trust, out, "Got errSSLServerAuthCompleted twice?");
            /* verify peer cert chain */
            require_noerr(SSLCopyPeerTrust(ctx, &trust), out);
            SecTrustResultType trust_result = 0;
            /* this won't verify without setting up a trusted anchor */
            require_noerr(SecTrustEvaluate(trust, &trust_result), out);
        }
    } while(ortn == errSSLWouldBlock || ortn == errSSLServerAuthCompleted);

    //fprintf(stderr, "\nHTTP Request Sent\n");

    require_noerr_action_quiet(ortn, out, printf("SSLWrite failed with err %ld\n", (long)ortn));

    require_string(got_server_auth, out, "never got server auth");

    do {
        ortn = SSLRead(ctx, reply, sizeof(reply)-1, &received);
        //fprintf(stderr, "r"); usleep(1000);
    } while(ortn == errSSLWouldBlock);
    
    //fprintf(stderr, "\n");
    
    require_noerr_action_quiet(ortn, out, printf("SSLRead failed with err %ld\n", (long)ortn));

    reply[received]=0;

    //fprintf(stderr, "HTTP reply:\n");
    //fprintf(stderr, "%s\n",reply);
    
out:
    SSLClose(ctx);
    SSLDisposeContext(ctx);
    if (trust) CFRelease(trust);

    return ortn;
}

static ssl_test_handle *
ssl_test_handle_create(int comm, SSLProtocol maxprot, Boolean false_start)
{
    ssl_test_handle *handle = calloc(1, sizeof(ssl_test_handle));
    if (handle) {
        handle->comm = comm;
        handle->st = make_ssl_ref(comm, maxprot, false_start);
    }
    return handle;
}

static 
struct s_server {
    char *host;
    int port;
    SSLProtocol maxprot;
} servers[] = {
    /* Good tls 1.2 servers */
    {"encrypted.google.com", 443, kTLSProtocol12 },
    {"www.amazon.com",443, kTLSProtocol12 },
    //{"www.mikestoolbox.org",443, kTLSProtocol12 },
};

#define NSERVERS (int)(sizeof(servers)/sizeof(servers[0]))
#define NLOOPS 1

static void
tests(void)
{
    int p;
    int fs;
    
    for(p=0; p<NSERVERS;p++) {
    for(int loops=0; loops<NLOOPS; loops++) {
    for(fs=0;fs<2; fs++) {

        ssl_test_handle *client;

        int s;
        OSStatus r;

        s=SocketConnect(servers[p].host, servers[p].port);
        if(s<0) {
            fail("connect failed with err=%d - %s:%d (try %d)", s, servers[p].host, servers[p].port, loops);
            break;
        }

        client = ssl_test_handle_create(s, servers[p].maxprot, fs);

        r=securetransport(client);
        ok(!r, "handshake failed with err=%ld - %s:%d (try %d), false start=%d", (long)r, servers[p].host, servers[p].port, loops, fs);

        close(s);
        free(client);
    } } }
}

int ssl_47_falsestart(int argc, char *const *argv)
{
        plan_tests(NSERVERS*NLOOPS*2);

        tests();

        return 0;
}
