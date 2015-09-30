/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
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


#include <Security/Security.h>
#include <Security/SecBase.h>

#include "../sslViewer/sslAppUtils.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h> /* close() */
#include <string.h> /* memset() */
#include <fcntl.h>
#include <time.h>

#ifdef NO_SERVER
#include <securityd/spi.h>
#endif

#define PORT 23232

#include "ssl-utils.h"

static void dumppacket(const unsigned char *data, unsigned long len)
{
    unsigned long i;
    for(i=0;i<len;i++)
    {
        if((i&0xf)==0) printf("%04lx :",i);
        printf(" %02x", data[i]);
        if((i&0xf)==0xf) printf("\n");
    }
    printf("\n");
}


/* 2K should be enough for everybody */
#define MTU 2048
static unsigned char readBuffer[MTU];
static unsigned int  readOff=0;
static size_t        readLeft=0;

static
OSStatus SocketRead(
                    SSLConnectionRef 	connection,
                    void 				*data,
                    size_t 				*dataLength)
{
    int fd = (int)connection;
    ssize_t len;
    uint8_t *d=readBuffer;

    if(readLeft==0)
    {
        len = read(fd, readBuffer, MTU);

        if(len>0) {
            readOff=0;
            readLeft=(size_t) len;
            printf("SocketRead: %ld bytes... epoch: %02x seq=%02x%02x\n",
                   len, d[4], d[9], d[10]);
        } else {
            int theErr = errno;
            switch(theErr) {
                case EAGAIN:
                    // printf("SocketRead: EAGAIN\n");
                    *dataLength=0;
                    /* nonblocking, no data */
                    return errSSLWouldBlock;
                default:
                    perror("SocketRead");
                    return errSecIO;
            }
        }
    }

    if(readLeft<*dataLength) {
        *dataLength=readLeft;
    }

    memcpy(data, readBuffer+readOff, *dataLength);
    readLeft-=*dataLength;
    readOff+=*dataLength;


    return errSecSuccess;

}


static
OSStatus SocketWrite(
                     SSLConnectionRef   connection,
                     const void         *data,
                     size_t 			*dataLength)	/* IN/OUT */
{
    int fd = (int)connection;
    ssize_t len;
    OSStatus err = errSecSuccess;
    const uint8_t *d=data;

#if 1
    if((rand()&3)==1) {
        /* drop 1/8 packets */
        printf("SocketWrite: Drop %ld bytes... epoch: %02x seq=%02x%02x\n",
               *dataLength, d[4], d[9], d[10]);
        return errSecSuccess;
    }
#endif

    len = send(fd, data, *dataLength, 0);

    if(len>0) {
        *dataLength=(size_t)len;

        printf("SocketWrite: Sent %ld bytes... epoch: %02x seq=%02x%02x\n",
               len, d[4], d[9], d[10]);

        return err;
    }

    int theErr = errno;
    switch(theErr) {
        case EAGAIN:
            /* nonblocking, no data */
            err = errSSLWouldBlock;
            break;
        default:
            perror("SocketWrite");
            err = errSecIO;
            break;
    }

    return err;

}


int main(int argc, char **argv)
{
    struct sockaddr_in sa; /* server address for bind */
    struct sockaddr_in ca; /* client address for connect */
    int fd;
    ssize_t l;

#ifdef NO_SERVER
# if DEBUG
    securityd_init();
# endif
#endif

    if ((fd=socket(AF_INET, SOCK_DGRAM, 0))==-1) {
        perror("socket");
        return errno;
    }

    time_t seed=time(NULL);
//    time_t seed=1298952496;
    srand((unsigned)seed);
    printf("Random drop initialized with seed = %lu\n", seed);

    memset((char *) &sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(PORT);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind (fd, (struct sockaddr *)&sa, sizeof(sa))==-1)
    {
        perror("bind");
        return errno;
    }

    printf("Waiting for first packet...\n");
    /* PEEK only... */
    socklen_t slen=sizeof(ca);
    char b;
    if((l=recvfrom(fd, &b, 1, MSG_PEEK, (struct sockaddr *)&ca, &slen))==-1)
    {
        perror("recvfrom");
        return errno;
    }

    printf("Received packet from %s (%ld), connecting...\n", inet_ntoa(ca.sin_addr), l);

    if(connect(fd, (struct sockaddr *)&ca, sizeof(ca))==-1)
    {
        perror("connect");
        return errno;
    }

    /* Change to non blocking */
    fcntl(fd, F_SETFL, O_NONBLOCK);


    SSLConnectionRef c=(SSLConnectionRef)(intptr_t)fd;


    OSStatus            ortn;
    SSLContextRef       ctx = NULL;

    SSLClientCertificateState certState;
    SSLCipherSuite negCipher;

	/*
	 * Set up a SecureTransport session.
	 */
	ortn = SSLNewDatagramContext(true, &ctx);
	if(ortn) {
		printSslErrStr("SSLNewDatagramContext", ortn);
		return ortn;
	}

	ortn = SSLSetIOFuncs(ctx, SocketRead, SocketWrite);
	if(ortn) {
		printSslErrStr("SSLSetIOFuncs", ortn);
        return ortn;
	}

    ortn = SSLSetConnection(ctx, c);
	if(ortn) {
		printSslErrStr("SSLSetConnection", ortn);
		return ortn;
	}

    ortn = SSLSetDatagramHelloCookie(ctx, &ca, 32);
    if(ortn) {
		printSslErrStr("SSLSetDatagramHelloCookie", ortn);
        return ortn;
	}

    ortn = SSLSetMaxDatagramRecordSize(ctx, 400);
    if(ortn) {
		printSslErrStr("SSLSetMaxDatagramRecordSize", ortn);
        return ortn;
	}

    /* Lets not verify the cert, which is a random test cert */
    ortn = SSLSetEnableCertVerify(ctx, false);
    if(ortn) {
        printSslErrStr("SSLSetEnableCertVerify", ortn);
        return ortn;
    }

    ortn = SSLSetCertificate(ctx, server_chain());
    if(ortn) {
        printSslErrStr("SSLSetCertificate", ortn);
        return ortn;
    }

    ortn = SSLSetClientSideAuthenticate(ctx, kAlwaysAuthenticate);
    if(ortn) {
        printSslErrStr("SSLSetCertificate", ortn);
        return ortn;
    }

    printf("Server Handshake...\n");
    do {
		ortn = SSLHandshake(ctx);
	    if(ortn == errSSLWouldBlock) {
		/* keep UI responsive */
		sslOutputDot();
	    }
    } while (ortn == errSSLWouldBlock);

    if(ortn) {
        printSslErrStr("SSLHandshake", ortn);
        return ortn;
    }

    SSLGetClientCertificateState(ctx, &certState);
	SSLGetNegotiatedCipher(ctx, &negCipher);

    printf("Server Handshake done. Cipher is %s\n", sslGetCipherSuiteString(negCipher));

    unsigned char buffer[MTU];
    size_t len, readLen;

    while(1) {
        while((ortn=SSLRead(ctx, buffer, MTU, &readLen))==errSSLWouldBlock);
        if(ortn) {
            printSslErrStr("SSLRead", ortn);
            break;
        }
        buffer[readLen]=0;
        printf("Received %lu bytes:\n", readLen);
        dumppacket(buffer, readLen);

        ortn=SSLWrite(ctx, buffer, readLen, &len);
        if(ortn) {
            printSslErrStr("SSLRead", ortn);
            break;
        }
        printf("Echoing %lu bytes\n", len);
    }

    SSLDisposeContext(ctx);

    return ortn;
}
