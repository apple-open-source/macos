/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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

#ifndef _NETSHAREENUM_H_
#define _NETSHAREENUM_H_

#include <CoreFoundation/CoreFoundation.h>


#ifdef __cplusplus
extern "C" {
#endif
	
#define kNetCommentStrKey		CFSTR("NetCommentStr")
#define kNetShareTypeStrKey		CFSTR("NetShareTypeStr")
/*
 * share types
 */
#define	SMB_ST_DISK		0x0	/* A: */
#define	SMB_ST_PRINTER	0x1	/* LPT: */
#define	SMB_ST_COMM		0x2	/* COMM */
#define	SMB_ST_PIPE		0x3	/* IPC */
#define	SMB_ST_ANY		0x4	/* ????? */
	
	
int smb_netshareenum(SMBHANDLE inConnection, CFDictionaryRef *outDict, int DiskAndPrintSharesOnly);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _NETSHAREENUM_H_
