/*
 * Copyright (c) 2008,2011 Apple Inc. All Rights Reserved.
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
 * handletypes.h 
 */
#ifndef _H_SS_HANDLE_TYPES
#define _H_SS_HANDLE_TYPES

#include <Security/cssmtype.h>
#include <stdint.h>

#ifdef __cplusplus

namespace Security {
namespace SecurityServer {

#endif /* __cplusplus */


/* XXX/gh  Might have to be guarded thusly to protect ss_types.h */
/* #ifndef _H_SS_TYPES */

/*
 * These are all uint32_ts behind the curtain, but we try to be
 * explicit about which kind they are.
 * By protocol, each of these is in a different address space - i.e.
 * a KeyHandle and a DbHandle with the same value may or may not refer
 * to the same thing - it's up to the handle provider.
 * GenericHandle is for cases where a generic handle is further elaborated
 * with a "kind code" - currently for ACL manipulations only.
 */
typedef uint32_t DbHandle;			/* database handle               */
typedef uint32_t KeyHandle;			/* cryptographic key handle      */
typedef uint32_t RecordHandle;		/* data record identifier handle */
typedef uint32_t SearchHandle;		/* search (query) handle         */
typedef uint32_t GenericHandle;		/* for polymorphic handle uses   */

static const DbHandle noDb = 0;
static const KeyHandle noKey = 0;
static const RecordHandle noRecord = 0;
static const SearchHandle noSearch = 0;

/* #endif */  /* _H_SS_TYPES */

/*
 * Required for MIG-generated code; made sense when the above handle types
 * were all CSSM_HANDLEs
 */
typedef uint32_t IPCHandle;
typedef IPCHandle IPCDbHandle;
typedef IPCHandle IPCKeyHandle;
typedef IPCHandle IPCRecordHandle;
typedef IPCHandle IPCSearchHandle;
typedef IPCHandle IPCGenericHandle;

#ifdef __cplusplus

} // end namespace SecurityServer
} // end namespace Security

#endif /* __cplusplus */


#endif /* _H_SS_HANDLE_TYPES */
