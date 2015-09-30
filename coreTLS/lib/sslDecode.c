/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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
 * sslDecode.c - Decoding of TLS handshake structures
 */

#include "sslDecode.h"
#include "sslUtils.h"
#include "sslDebug.h"
#include "sslMemory.h"
#include "tls_handshake_priv.h"


size_t SSLEncodedBufferListSize(const tls_buffer_list_t *list, int itemLenSize)
{
    size_t len = 0;

    while(list) {
        len += itemLenSize;
        len += list->buffer.length;
        list = list->next;
    }

    return len;
}

uint8_t *
SSLEncodeBufferList(const tls_buffer_list_t *list, int itemLenSize, uint8_t *p)
{
    while(list) {
        p = SSLEncodeSize(p, list->buffer.length, itemLenSize);
        memcpy(p, list->buffer.data, list->buffer.length); p += list->buffer.length;
        list = list->next;
    }
    return p;
}

int
SSLDecodeBufferList(uint8_t *p, size_t listLen, int itemLenSize, tls_buffer_list_t **list)
{
    int err = 0;

    tls_buffer_list_t *first = NULL;
    tls_buffer_list_t *last = NULL;

    while (listLen > 0)
    {
        size_t itemLen;
        tls_buffer_list_t *item;
        if (listLen < itemLenSize) {
            sslErrorLog("SSLDecodeBufferList: length decode error 2\n");
            err = errSSLProtocol;
            goto errOut;
        }
        itemLen = SSLDecodeInt(p,itemLenSize);
        p += itemLenSize;
        if (listLen < itemLen + itemLenSize) {
            sslErrorLog("SSLDecodeBufferList: length decode error 3\n");
            err = errSSLProtocol;
            goto errOut;
        }
        if(itemLen==0) {
            sslErrorLog("SSLDecodeBufferList: lenght decode error 4 (empty item)\n");
            err = errSSLProtocol;
            goto errOut;
        }
        item = (tls_buffer_list_t *)sslMalloc(sizeof(tls_buffer_list_t));
        if(item == NULL) {
            err = errSSLAllocate;
            goto errOut;
        }
        if ((err = SSLAllocBuffer(&item->buffer, itemLen))) {
            sslFree(item);
            goto errOut;
        }
        item->next = NULL;
        memcpy(item->buffer.data, p, itemLen);
        p += itemLen;

        if(first==NULL) {
            first=item;
            last=item;
        } else {
            last->next=item;
            last=item;
        }
        listLen -= itemLenSize+itemLen;
    }

    *list = first;
    return 0;

errOut:
    tls_free_buffer_list(first);
    return err;
}
