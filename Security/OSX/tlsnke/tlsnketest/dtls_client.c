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


/*
 *  dtlsEchoClient.c
 *  Security
 *
 *  Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
 *
 */

#include <Security/Security.h>

#include "ssl-utils.h"

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

#include "tlssocket.h"

#define SERVER "10.0.2.1"
#define PORT 23232
#define BUFLEN 128
#define COUNT 10

#if 0
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
#endif


/* print a '.' every few seconds to keep UI alive while connecting */
static time_t lastTime = (time_t)0;
#define TIME_INTERVAL		3

static void sslOutputDot()
{
	time_t thisTime = time(0);
	
	if((thisTime - lastTime) >= TIME_INTERVAL) {
		printf("."); fflush(stdout);
		lastTime = thisTime;
	}
}

static void printSslErrStr(
                    const char 	*op,
                    OSStatus 	err)
{
	printf("*** %s: %ld\n", op, (long)err);
}

/* 2K should be enough for everybody */
#define MTU 2048


int dtls_client(const char *hostname, int bypass);

int dtls_client(const char *hostname, int bypass)
{
    int fd;
    int tlsfd;
    struct sockaddr_in sa;
    
    printf("Running dtls_client test with hostname=%s, bypass=%d\n", hostname, bypass);

    if ((fd=socket(AF_INET, SOCK_DGRAM, 0))==-1) {
        perror("socket");
        exit(-1);
    }
    
    memset((char *) &sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(PORT);
    if (inet_aton(hostname, &sa.sin_addr)==0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }
    
    if(connect(fd, (struct sockaddr *)&sa, sizeof(sa))==-1)
    {
        perror("connect");
        return errno;
    }
    
    /* Change to non blocking io */
    fcntl(fd, F_SETFL, O_NONBLOCK);
    
    SSLRecordContextRef c=(intptr_t)fd;
    
    
    OSStatus            ortn;
    SSLContextRef       ctx = NULL;
    
    SSLClientCertificateState certState;
    SSLCipherSuite negCipher;
    SSLProtocol negVersion;
    
	/*
	 * Set up a SecureTransport session.
	 */
    
    ctx = SSLCreateContextWithRecordFuncs(kCFAllocatorDefault, kSSLClientSide, kSSLDatagramType, &TLSSocket_Funcs);
    if(!ctx) {
        printSslErrStr("SSLCreateContextWithRecordFuncs", -1);
        return -1;
    }

    printf("Attaching filter\n");
    ortn = TLSSocket_Attach(fd);
    if(ortn) {
		printSslErrStr("TLSSocket_Attach", ortn);
		return ortn;        
    }
    
    if(bypass) {
        tlsfd = open("/dev/tlsnke", O_RDWR);
        if(tlsfd<0) {
            perror("opening tlsnke dev");
            exit(-1);
        }
    }

    ortn = SSLSetRecordContext(ctx, c);
	if(ortn) {
		printSslErrStr("SSLSetRecordContext", ortn);
		return ortn;
	}
    
    ortn = SSLSetMaxDatagramRecordSize(ctx, 600);
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
    
    printf("Handshake...\n");

    do {
		ortn = SSLHandshake(ctx);
	    if(ortn == errSSLWouldBlock) {
            /* keep UI responsive */
            sslOutputDot();
	    }
    } while (ortn == errSSLWouldBlock);
    
    
    SSLGetClientCertificateState(ctx, &certState);
	SSLGetNegotiatedCipher(ctx, &negCipher);
	SSLGetNegotiatedProtocolVersion(ctx, &negVersion);
    
    int count;
    size_t len;
    ssize_t sreadLen, swriteLen;
    size_t readLen, writeLen;

    char buffer[BUFLEN];
    
    count = 0;
    while(count<COUNT) {
        int timeout = 10000;
        
        snprintf(buffer, BUFLEN, "Message %d", count);
        len = strlen(buffer);
        
        if(bypass) {
            /* Send data through the side channel, kind of like utun would */
            swriteLen=write(tlsfd, buffer, len);
            if(swriteLen<0) {
                perror("write to tlsfd");
                break;
            }
            writeLen=swriteLen;
        } else {
            ortn=SSLWrite(ctx, buffer, len, &writeLen);
            if(ortn) {
                printSslErrStr("SSLWrite", ortn);
                break;
            }
        }

        printf("Wrote %lu bytes\n", writeLen);
        
        count++;
        
        if(bypass) {
            do {
                sreadLen=read(tlsfd, buffer, BUFLEN);
            } while((sreadLen==-1) && (errno==EAGAIN) && (timeout--));
            if((sreadLen==-1) && (errno==EAGAIN)) {
                printf("Read timeout...\n");
                continue;
            }
            if(sreadLen<0) {
                perror("read from tlsfd");
                break;
            }
            readLen=sreadLen;
        }
        else {
            do {
                ortn=SSLRead(ctx, buffer, BUFLEN, &readLen);
            } while((ortn==errSSLWouldBlock) && (timeout--));
            if(ortn==errSSLWouldBlock) {
                printf("SSLRead timeout...\n");
                continue;
            }
            if(ortn) {
                printSslErrStr("SSLRead", ortn);
                break;
            }
        }

        buffer[readLen]=0;
        printf("Received %lu bytes: %s\n", readLen, buffer);
        
    }
    
    SSLClose(ctx);
    
    SSLDisposeContext(ctx);
    
    return ortn;
}

