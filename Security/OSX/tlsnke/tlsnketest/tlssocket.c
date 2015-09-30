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


#include <Security/SecureTransportPriv.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <net/kext_net.h>

#include "tlssocket.h"
#include "tlsnke.h"

#include <AssertMacros.h>
#include <errno.h>

/* TLSSocket functions */

static 
int TLSSocket_Read(SSLRecordContextRef ref,
                        SSLRecord *rec)
{
    int socket = (int)ref;
    int rc;
    ssize_t sz;
    struct sockaddr_in client_addr;
    int avail;
    socklen_t avail_size;
    struct cmsghdr *cmsg;
    tls_record_hdr_t hdr;
    struct msghdr msg;
    struct iovec iov;
    int cbuf_len=CMSG_SPACE(sizeof(*hdr))+1024;
    uint8_t cbuf[cbuf_len];
   

    //    printf("%s: Waiting for some data...\n", __FUNCTION__);
    /* PEEK only... */
    char b;
    rc = (int)recv(socket, &b, 1, MSG_PEEK);
    
    if(rc==-1)
    {
        if(errno==EAGAIN)
            return errSSLRecordWouldBlock;
        else {
            perror("recv");
            return errno;
        }
    }
    
    /* get the next packet size */
    avail_size = sizeof(avail);
    rc = getsockopt(socket, SOL_SOCKET, SO_NREAD, &avail, &avail_size);
    
    check_noerr(rc); 
    check(avail_size==sizeof(avail));
    
    if(rc || (avail_size !=sizeof(avail)))
        return errSSLRecordInternal;

    //    printf("%s: Available = %d\n", __FUNCTION__, avail);
    
    if(avail==0)
        return errSSLRecordWouldBlock;

        
    /* Allocate a buffer */
    rec->contents.data = malloc(avail);
    rec->contents.length = avail;
    
    /* read the message */
    iov.iov_base = rec->contents.data;
    iov.iov_len = rec->contents.length;
    msg.msg_name = &client_addr;
    msg.msg_namelen = sizeof(client_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cbuf;
    msg.msg_controllen = cbuf_len;
    
    sz = recvmsg(socket, &msg, 0);
    check(sz==avail);
    
    //    printf("%s: received = %ld, ctrl: l=%d f=%x\n", __FUNCTION__, sz, msg.msg_controllen, msg.msg_flags);
    rec->contents.length = sz;
    
    cmsg = CMSG_FIRSTHDR(&msg);
    check(cmsg);
    if(!cmsg)
        return 0;
    
    check(cmsg->cmsg_type == SCM_TLS_HEADER);
    check(cmsg->cmsg_level == SOL_SOCKET);
    check(cmsg->cmsg_len == CMSG_LEN(sizeof(*hdr)));
    hdr = (tls_record_hdr_t)CMSG_DATA(cmsg);
    check(hdr);
    
    /* print msg info */
    /*
    printf("%s: rc=%d, msg: %ld , cmsg = %d, %x, %x, hdr = %d, %x - from %s:%d\n", __FUNCTION__, rc,
           iov.iov_len,
           cmsg->cmsg_len, cmsg->cmsg_level, cmsg->cmsg_type,
           hdr->content_type, hdr->protocol_version,
           inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port)); 
    */
    rec->contentType = hdr->content_type;
    rec->protocolVersion = hdr->protocol_version;
    
    if(rec->contentType==SSL_RecordTypeChangeCipher) {
        printf("%s: Received ChangeCipherSpec message\n", __FUNCTION__);
    }
    return 0;
}

static
int TLSSocket_Free(SSLRecordContextRef ref,
                         SSLRecord rec)
{
    free(rec.contents.data);
    return 0;
}

static 
int TLSSocket_Write(SSLRecordContextRef ref,
                          SSLRecord rec)
{
    int socket = (int)ref;
    ssize_t sz;
    
    struct msghdr msg;
    struct iovec iov;
    tls_record_hdr_t hdr;
    struct cmsghdr *cmsg;
    int cbuf_len=CMSG_SPACE(sizeof(*hdr));
    uint8_t cbuf[cbuf_len];

    if(rec.contentType==SSL_RecordTypeChangeCipher) {
        printf("%s: Sending ChangeCipherSpec message\n", __FUNCTION__);
    }
    // printf("%s: fd=%d, rec.len=%ld\n", __FUNCTION__, socket, rec.contents.length);

    /* write the message */
    iov.iov_base = rec.contents.data;
    iov.iov_len = rec.contents.length;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cbuf;
    msg.msg_controllen = cbuf_len;

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_TLS_HEADER;
    cmsg->cmsg_len = CMSG_LEN(sizeof(*hdr));
    hdr = (tls_record_hdr_t)CMSG_DATA(cmsg);
    hdr->content_type = rec.contentType;
    hdr->protocol_version = rec.protocolVersion;
    
    /* print msg info */
    sz = sendmsg(socket, &msg, 0);
    
    if(sz<0)
        perror("sendmsg");
    
    /*
       printf("%s: sz=%ld, msg: %ld , cmsg = %d, %d, %04x\n", __FUNCTION__, sz,
           iov.iov_len,
           cmsg->cmsg_len, cmsg->cmsg_level, cmsg->cmsg_type);
    */
    
    check(sz==rec.contents.length);

    if(sz<0)
        return (int)sz;
    else
        return 0;
}


static
int TLSSocket_InitPendingCiphers(SSLRecordContextRef   ref,
                                       uint16_t              selectedCipher,
                                       bool                  server,
                                       SSLBuffer             key)
{
    int socket = (int)ref;
    int rc;
    char *buf;
    
    buf = malloc(key.length+3);
    buf[0] = selectedCipher >> 8;
    buf[1] = selectedCipher & 0xff;
    buf[2] = server;
    memcpy(buf+3, key.data, key.length);
    
    printf("%s: cipher=%04x, keylen=%ld\n", __FUNCTION__, selectedCipher, key.length);
    
    rc = setsockopt(socket, SOL_SOCKET, SO_TLS_INIT_CIPHER, buf, (socklen_t)(key.length+3));
    
    printf("%s: rc=%d\n", __FUNCTION__, rc);
    
    free(buf);
    
    return rc;
}

static 
int TLSSocket_AdvanceWriteCipher(SSLRecordContextRef ref)
{
    int socket = (int)ref;
    int rc;
    rc = setsockopt(socket, SOL_SOCKET, SO_TLS_ADVANCE_WRITE_CIPHER, NULL, 0);
    
    printf("%s: rc=%d\n", __FUNCTION__, rc);
    
    return rc;
}

static 
int TLSSocket_RollbackWriteCipher(SSLRecordContextRef ref)
{
    int socket = (int)ref;
    int rc;
    rc = setsockopt(socket, SOL_SOCKET, SO_TLS_ROLLBACK_WRITE_CIPHER, NULL, 0);
    
    printf("%s: rc=%d\n", __FUNCTION__, rc);
    
    return rc;
}

static 
int TLSSocket_AdvanceReadCipher(SSLRecordContextRef    ref)
{
    int socket = (int)ref;
    int rc;
    rc = setsockopt(socket, SOL_SOCKET, SO_TLS_ADVANCE_READ_CIPHER, NULL, 0);
    
    printf("%s: rc=%d\n", __FUNCTION__, rc);
    
    return rc;
}

static 
int TLSSocket_SetProtocolVersion(SSLRecordContextRef    ref,
                                 SSLProtocolVersion     protocolVersion)
{
    int socket = (int)ref;
    int rc;
    rc = setsockopt(socket, SOL_SOCKET, SO_TLS_PROTOCOL_VERSION, &protocolVersion, sizeof(protocolVersion));
    
    printf("%s: rc=%d\n", __FUNCTION__, rc);
    
    return rc;
}


static
int TLSSocket_ServiceWriteQueue(SSLRecordContextRef    ref)
{
    int socket = (int)ref;
    int rc;
    rc = setsockopt(socket, SOL_SOCKET, SO_TLS_SERVICE_WRITE_QUEUE, NULL, 0);

    return rc;
}


static
int TLSSocket_SetOption(SSLRecordContextRef    ref,
                        SSLRecordOption        option,
                        bool                   value)
{
    /* This is not implemented, and is not needed for DTLS */
    return EINVAL;
}

const struct SSLRecordFuncs TLSSocket_Funcs = {
    .read                = TLSSocket_Read,
    .write               = TLSSocket_Write,
    .initPendingCiphers  = TLSSocket_InitPendingCiphers,
    .advanceWriteCipher  = TLSSocket_AdvanceWriteCipher,
    .rollbackWriteCipher = TLSSocket_RollbackWriteCipher,
    .advanceReadCipher   = TLSSocket_AdvanceReadCipher,
    .setProtocolVersion  = TLSSocket_SetProtocolVersion,
    .free                = TLSSocket_Free,
    .serviceWriteQueue   = TLSSocket_ServiceWriteQueue,
    .setOption           = TLSSocket_SetOption,
};


/* TLSSocket SPIs */

int TLSSocket_Attach(int socket)
{
    
    /* Attach the TLS socket filter and return handle */
    struct so_nke so_tlsnke;
    int rc;
    int handle;
    socklen_t len;
    
    memset(&so_tlsnke, 0, sizeof(so_tlsnke));
    so_tlsnke.nke_handle = TLS_HANDLE_IP4;
    rc=setsockopt(socket, SOL_SOCKET, SO_NKE, &so_tlsnke, sizeof(so_tlsnke));
    if(rc)
        return rc;

    len = sizeof(handle);
    rc = getsockopt(socket, SOL_SOCKET, SO_TLS_HANDLE, &handle, &len);
    if(rc)
        return rc;

    assert(len==sizeof(handle));
    
    return handle;
}

