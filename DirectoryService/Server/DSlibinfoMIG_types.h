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
* @header DSlibinfoMIG_types.h
 */

#ifndef __DSLIBINFOMIG_TYPES_H_
#define	__DSLIBINFOMIG_TYPES_H_

#ifndef kDSStdMachDSLookupPortName
#define kDSStdMachDSLookupPortName	"com.apple.system.DirectoryService.libinfo_v1"
#endif

#ifndef MAX_MIG_INLINE_DATA
#define MAX_MIG_INLINE_DATA 16384
#endif

typedef char* inline_data_t;
typedef char* proc_name_t;

struct sLibinfoRequest
{
    mach_port_t         fReplyPort;
    int32_t             fProcedure;
    char                *fBuffer;
    int32_t             fBufferLen;
    mach_vm_address_t   fCallbackAddr;
    audit_token_t       fToken;
};

#endif
