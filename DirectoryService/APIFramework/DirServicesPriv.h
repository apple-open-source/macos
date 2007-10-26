/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 * @header DirServicesPriv
 */

#ifndef __DirServicesPriv_h__
#define	__DirServicesPriv_h__	1

#include <DirectoryService/DirServicesTypes.h>
#include <DirectoryServiceCore/PrivateTypes.h>
#include <DirectoryService/CDSRefTable.h>
#include <DirectoryServiceCore/CBuff.h>

/*!
 * @defined kDSStdNotifyxxxxx
 * @discussion notification tags that can be registered for with SystemConfiguration
 * to receive notifications of events occuring in the DirectoryService daemon on the
 * local machine. Not placed in public headers since might deprecate in favor of BSD
 * daemon notification mechanism at later date.
 */
#define		kDSStdNotifyTypePrefix					"com.apple.DirectoryService.NotifyTypeStandard:"
#define		kDSStdNotifySearchPolicyChanged			"com.apple.DirectoryService.NotifyTypeStandard:SearchPolicyChanged"
#define		kDSStdNotifyDirectoryNodeAdded			"com.apple.DirectoryService.NotifyTypeStandard:DirectoryNodeAdded"
#define		kDSStdNotifyDirectoryNodeDeleted		"com.apple.DirectoryService.NotifyTypeStandard:DirectoryNodeDeleted"
#define		kDSStdNotifyDHCPOptionsAvailable		"com.apple.DirectoryService.NotifyTypeStandard:DHCPOptionsAvailable"
#define		kDSStdNotifyDHCPConfigStateChanged		"com.apple.DirectoryService.NotifyTypeStandard:DHCPConfigStateChanged"
#define		kDSStdNotifyContactSearchPolicyChanged	"com.apple.DirectoryService.NotifyTypeStandard:ContactSearchPolicyChanged"

tDirStatus	VerifyTDataBuff		(	tDataBuffer	   *inBuff,
									tDirStatus		inNullErr,
									tDirStatus		inEmptyErr );
									
tDirStatus	VerifyTNodeList		(	tDataList	   *inDataList,
									tDirStatus		inNullErr,
									tDirStatus		inEmptyErr );
									
UInt32		CalcCRC				(	const char	   *inStr );
UInt32		CalcCRCWithLength	(	const void	   *inData,
													UInt32 inLength );

tDirStatus	IsStdBuffer			(	tDataBufferPtr	inOutDataBuff );

tDirStatus	IsNodePathStrBuffer	(	tDataBufferPtr	inOutDataBuff );

tDirStatus	IsFWReference		(	UInt32			inRef );

tDirStatus	IsRemoteReferenceMap(	UInt32			inRef );

tDirStatus	ExtractRecordEntry	(	tDataBufferPtr				inOutDataBuff,
									UInt32						inRecordEntryIndex,
									tAttributeListRef		   *outAttributeListRef,
									tRecordEntryPtr			   *outRecEntryPtr );
									
tDirStatus	ExtractAttributeEntry (	tDataBufferPtr				inOutDataBuff,
									tAttributeListRef			inAttrListRef,
									UInt32						inAttrInfoIndex,
									tAttributeValueListRef	   *outAttrValueListRef,
									tAttributeEntryPtr		   *outAttrInfoPtr );
									
tDirStatus ExtractNextAttributeEntry (	tDataBufferPtr				inOutDataBuff,
										tAttributeListRef			inAttrListRef,
										UInt32						inAttrInfoIndex,
										SInt32					   *inOutOffset,
										tAttributeValueListRef	   *outAttrValueListRef,
										tAttributeEntryPtr		   *outAttrInfoPtr );

tDirStatus	ExtractAttributeValue (	tDataBufferPtr				inOutDataBuff,
									tAttributeValueListRef		inAttrValueListRef,
									UInt32						inAttrValueIndex,
									tAttributeValueEntryPtr	   *outAttrValue );

tDirStatus ExtractNextAttributeValue (	tDataBufferPtr				inOutDataBuff,
										tAttributeValueListRef		inAttrValueListRef,
										UInt32						inAttrValueIndex,
										SInt32					   *inOutOffset,
										tAttributeValueEntryPtr	   *outAttrValue );

tDirStatus	ExtractDirNodeName	  (	tDataBufferPtr				inOutDataBuff,
									UInt32						inDirNodeIndex,
									tDataListPtr			   *outDataList );

tDirStatus MakeGDNIFWRef		  (	tDataBufferPtr				inOutDataBuff,
									tAttributeListRef		   *outAttributeListRef );

const char *dsGetPluginNamePriv	( UInt32 inNodeRefNum, UInt32 inPID );

#endif
