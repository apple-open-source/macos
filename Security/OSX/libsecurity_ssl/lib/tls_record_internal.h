/*
 * Copyright (c) 2002,2005-2007,2010-2011,2014 Apple Inc. All Rights Reserved.
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
 * tls_record.h - Declarations of record layer callout struct to provide indirect calls to
 *     SSLv3 and TLS routines.
 */

#ifndef	_TLS_RECORD_INTERNAL_H_
#define _TLS_RECORD_INTERNAL_H_

#ifdef	__cplusplus
extern "C" {
#endif

// #include "sslRecord.h"

#include "sslTypes.h"
#include "sslMemory.h"
#include "SSLRecordInternal.h"
#include "sslContext.h"

#include <tls_record.h>

struct SSLRecordInternalContext;

typedef struct WaitingRecord
{   struct WaitingRecord    *next;
    size_t                  sent;
    /*
     * These two fields replace a dynamically allocated SSLBuffer;
     * the payload to write is contained in the variable-length
     * array data[].
     */
    size_t					length;
    uint8_t					data[1];
} WaitingRecord;


struct SSLRecordInternalContext
{
    tls_record_t        filter;

    /* Reference back to the SSLContext */
    SSLContextRef       sslCtx;

    /* buffering */
    SSLBuffer    		partialReadBuffer;
    size_t              amountRead;

    WaitingRecord       *recordWriteQueue;
};

#ifdef	__cplusplus
}
#endif

#endif 	/* _TLS_SSL_H_ */
