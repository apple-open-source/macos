//
//  SSLRecordInternal.h
//  Security
//
//  Created by Fabrice Gautier on 10/25/11.
//  Copyright (c) 2011 Apple, Inc. All rights reserved.
//


/* This header should be kernel compatible */

#ifndef _SSLRECORDINTERNAL_H_
#define _SSLRECORDINTERNAL_H_ 1

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>

#include "sslTypes.h"

typedef void *		SSLIOConnectionRef;

typedef int
(*SSLIOReadFunc) 			(SSLIOConnectionRef 	connection,
							 void 				*data, 			/* owned by
																 * caller, data
																 * RETURNED */
							 size_t 			*dataLength);	/* IN/OUT */
typedef int
(*SSLIOWriteFunc) 			(SSLIOConnectionRef 	connection,
							 const void 		*data,
							 size_t 			*dataLength);	/* IN/OUT */


/* Record layer creation functions, called from the SSLContext layer */

SSLRecordContextRef
SSLCreateInternalRecordLayer(bool dtls);

int
SSLSetInternalRecordLayerIOFuncs(
    SSLRecordContextRef ctx,
    SSLIOReadFunc       readFunc,
    SSLIOWriteFunc      writeFunc);

int
SSLSetInternalRecordLayerConnection(
    SSLRecordContextRef ctx,
    SSLIOConnectionRef  ioRef);

void
SSLDestroyInternalRecordLayer(SSLRecordContextRef ctx);


extern struct SSLRecordFuncs SSLRecordLayerInternal;

#endif
