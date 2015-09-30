/*
 * Copyright (c) 1999-2001,2005-2008,2010-2012,2014 Apple Inc. All Rights Reserved.
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
 * sslUtils.c - Misc. OS independant SSL utility functions
 */

/* THIS FILE CONTAINS KERNEL CODE */

#include "sslUtils.h"
#include "sslTypes.h"
#include "sslDebug.h"

#include <AssertMacros.h>

#ifndef NDEBUG
void SSLDump(const unsigned char *data, unsigned long len)
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

unsigned int
SSLDecodeInt(const uint8_t *p, size_t length)
{
    unsigned int val = 0;
    check(length > 0 && length <= 4); //anything else would be an internal error.
    while (length--)
        val = (val << 8) | *p++;
    return val;
}

uint8_t *
SSLEncodeInt(uint8_t *p, size_t value, size_t length)
{
    unsigned char *retVal = p + length; /* Return pointer to char after int */
    check(length > 0 && length <= 4);  //anything else would be an internal error.
    while (length--)                    /* Assemble backwards */
    {   p[length] = (uint8_t)value;     /* Implicit masking to low byte */
        value >>= 8;
    }
    return retVal;
}

size_t
SSLDecodeSize(const uint8_t *p, size_t length)
{
    unsigned int val = 0;
    check(length > 0 && length <= 4); //anything else would be an internal error.
    while (length--)
        val = (val << 8) | *p++;
    return val;
}

uint8_t *
SSLEncodeSize(uint8_t *p, size_t value, size_t length)
{
    unsigned char *retVal = p + length; /* Return pointer to char after int */
    check(length > 0 && length <= 4);  //anything else would be an internal error.
    while (length--)                    /* Assemble backwards */
    {   p[length] = (uint8_t)value;     /* Implicit masking to low byte */
        value >>= 8;
    }
    return retVal;
}


uint8_t *
SSLEncodeUInt64(uint8_t *p, uint64_t value)
{
    p = SSLEncodeInt(p, (value>>32)&0xffffffff, 4);
    return SSLEncodeInt(p, value&0xffffffff, 4);
}


void
IncrementUInt64(sslUint64 *v)
{
    (*v)++;
}

void
SSLDecodeUInt64(const uint8_t *p, size_t length, uint64_t *v)
{
    check(length > 0 && length <= 8);
    if(length<=4) {
        *v=SSLDecodeInt(p, length);
    } else {
        *v=((uint64_t)SSLDecodeInt(p, length-4))<<32 | SSLDecodeInt(p+length-4, 4);
    }
}


#if	SSL_DEBUG

const char *protocolVersStr(SSLProtocolVersion prot)
{
	switch(prot) {
        case SSL_Version_Undetermined: return "SSL_Version_Undetermined";
        case SSL_Version_2_0: return "SSL_Version_2_0";
        case SSL_Version_3_0: return "SSL_Version_3_0";
        case TLS_Version_1_0: return "TLS_Version_1_0";
        case TLS_Version_1_1: return "TLS_Version_1_1";
        case TLS_Version_1_2: return "TLS_Version_1_2";
        default: sslErrorLog("protocolVersStr: bad prot\n"); return "BAD PROTOCOL";
 	}
 	return NULL;	/* NOT REACHED */
}

#endif	/* SSL_DEBUG */



