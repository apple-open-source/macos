/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header DirServicesPriv
 */

#ifndef __DirServicesPriv_h__
#define	__DirServicesPriv_h__	1

#include "DirServicesTypes.h"
#include "PrivateTypes.h"
#include "CDSRefTable.h"
#include "CBuff.h"

tDirStatus	VerifyTDataBuff		(	tDataBuffer	   *inBuff,
									tDirStatus		inNullErr,
									tDirStatus		inEmptyErr );
									
tDirStatus	VerifyTNodeList		(	tDataList	   *inDataList,
									tDirStatus		inNullErr,
									tDirStatus		inEmptyErr );
									
uInt32		CalcCRC				(	const char	   *inStr );

tDirStatus	IsStdBuffer			(	tDataBufferPtr	inOutDataBuff );

tDirStatus	IsNodePathStrBuffer	(	tDataBufferPtr	inOutDataBuff );

tDirStatus	IsFWReference		(	uInt32			inRef );

tDirStatus	IsRemoteReferenceMap(	uInt32			inRef );

tDirStatus	ExtractRecordEntry	(	tDataBufferPtr				inOutDataBuff,
									unsigned long				inRecordEntryIndex,
									tAttributeListRef		   *outAttributeListRef,
									tRecordEntryPtr			   *outRecEntryPtr );
									
tDirStatus	ExtractAttributeEntry (	tDataBufferPtr				inOutDataBuff,
									tAttributeListRef			inAttrListRef,
									unsigned long				inAttrInfoIndex,
									tAttributeValueListRef	   *outAttrValueListRef,
									tAttributeEntryPtr		   *outAttrInfoPtr );
									
tDirStatus	ExtractAttributeValue (	tDataBufferPtr				inOutDataBuff,
									tAttributeValueListRef		inAttrValueListRef,
									unsigned long				inAttrValueIndex,
									tAttributeValueEntryPtr	   *outAttrValue );

tDirStatus	ExtractDirNodeName	  (	tDataBufferPtr				inOutDataBuff,
									unsigned long				inDirNodeIndex,
									tDataListPtr			   *outDataList );
#endif
