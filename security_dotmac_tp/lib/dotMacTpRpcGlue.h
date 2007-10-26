/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 * dotMacTpRpcGlue.h - Glue layer between CDSA and XMLRPC for .mac TP
 */
 
#ifndef	_DOTMAC_TP_RPC_GLUE_H_
#define _DOTMAC_TP_RPC_GLUE_H_

#include "dotMacTp.h"
#include <security_asn1/SecNssCoder.h>

#ifdef __cplusplus
extern "C" {
#endif

/* flavor of archive type */
typedef enum {
	DMAT_List,
	DMAT_Store,
	DMAT_Fetch,
	DMAT_Remove
} DotMacArchiveType;

OSStatus dotMacPostCertReq(
	DotMacCertTypeTag	certType,
	const CSSM_DATA		&userName,
	const CSSM_DATA		&password,
	const CSSM_DATA		&hostName,
	bool				renew,
	const CSSM_DATA		&csr,				// DER encoded 
	SecNssCoder			&coder,
	sint32				&estTime,			// possibly returned
	CSSM_DATA			&resultBodyData);	// possibly returned

/* post archive request */
OSStatus dotMacPostArchiveReq(
	uint32				version,
	DotMacCertTypeTag	certTypeTag,
	DotMacArchiveType	archiveType,
	const CSSM_DATA		&userName,
	const CSSM_DATA		&password,
	const CSSM_DATA		&hostName,
	const CSSM_DATA		*archiveName,
	const CSSM_DATA		*pfxIn,			// for store only
	const CSSM_DATA		*timeString,	// for store only
	const CSSM_DATA		*serialNumber,	// for store only
	CSSM_DATA			*pfxOut,		// RETURNED for fetch, allocated via alloc
	unsigned			*numArchives,	// RETURNED for list
	// at most one of the following is returned
	DotMacArchive		**archives_v1,	// RETURNED for list, allocated via alloc
	DotMacArchive_v2	**archives_v2,		
	Allocator			&mAlloc);

/* post "is request pending?" request */
OSStatus dotMacPostReqPendingPing(
	DotMacCertTypeTag	certType,
	const CSSM_DATA		&userName,
	const CSSM_DATA		&password,
	const CSSM_DATA		&hostName);

#ifdef __cplusplus
}
#endif

#endif	/* _DOTMAC_TP_RPC_GLUE_H_ */

