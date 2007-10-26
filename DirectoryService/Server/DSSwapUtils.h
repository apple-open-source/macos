/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header DSSwapUtils
 * Provides routines to byte swap endian data buffers.
 */

#ifndef __DSSwapUtils_h__
#define __DSSwapUtils_h__ 1

#ifndef __BIG_ENDIAN__

#include <machine/byte_order.h>
#include "DirServicesConst.h"
#include "DirServicesTypes.h"

#ifndef dsBool
	#define	dsBool	int
#endif

enum { kDSSwapToBig, kDSSwapToHost };

#ifdef __cplusplus
extern "C" {
#endif

    void DSSwapObjectData	(	UInt32 type,
								char* data,
								UInt32 size,
								dsBool swapAuth,
								dsBool isCustomCall,
								UInt32 inCustomRequestNum,
								const char* inPluginName,
								dsBool isAPICallResponse,
								dsBool inToBig);
    
    void DSSwapStandardBuf(char* data, UInt32 size, dsBool inToBig);
    void DSSwapRecordEntry(char* data, UInt32 type, dsBool inToBig);

    UInt32 DSGetLong(void* ptr, dsBool inToBig);
    unsigned short DSGetShort(void* ptr, dsBool inToBig);
    
    UInt32 DSGetAndSwapLong(void* ptr, dsBool inToBig);
    unsigned short DSGetAndSwapShort(void* ptr, dsBool inToBig);
    
    void DSSwapLong(void* ptr, dsBool inToBig);
    void DSSwapShort(void* ptr, dsBool inToBig);


#ifdef __cplusplus
}
#endif

#endif

#endif
