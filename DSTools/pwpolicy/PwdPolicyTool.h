/*
 * Copyright (c) 2000 - 2003 Apple Computer, Inc. All rights reserved.
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
 * @header PwdPolicyTool
 */


#ifndef __PwdPolicyTool_h__
#define __PwdPolicyTool_h__	1

#include <stdio.h>

#include <DirectoryService/DirectoryService.h>
#include <CoreServices/CoreServices.h>

#define kNewUserPass						"test"
#define kHashNameListPrefix					"HASHLIST:"

extern bool	gVerbose;

// Main App's error types
typedef enum {
	// NULL allocation errors
	kErrDataBufferAllocate			= 1,
	kErrDataNodeAllocateBlock,
	kErrDataNodeAllocateString,
	kErrDataListAllocate,
	kErrBuildFromPath,
	kErrGetPathFromList,
	kErrBuildListFromNodes,
	kErrBuildListFromStrings,
	kErrBuildListFromStringsAlloc,
	kErrDataListCopyList,
	kErrAllocAttributeValueEntry,

	// Error associations
	kErrOpenDirSrvc,
	kErrCloseDirSrvc,
	kErrDataListDeallocate,
	kErrDataBufferDeAllocate,
	kErrFindDirNodes,
	kErrOpenRecord,
	kErrCreateRecordAndOpen,
	kErrCloseRecord,
	kErrDeleteRecord,
	kErrDataNodeDeAllocate,

	kErrDataBuffDealloc,
	kErrCreateRecord,
	kErrAddAttribute,
	kErrAddAttributeValue,
	kErrSetAttributeValue,
	kErrGetRecAttrValueByIndex,
	kErrGetDirNodeName,
	kErrOpenDirNode,
	kErrCloseDirNode,
	kErrGetRecordList,

	kErrGetRecordEntry,
	kErrGetAttributeEntry,
	kErrGetAttributeValue,
	kErrDeallocAttributeValueEntry,
	kErrDoDirNodeAuth,
	kErrGetRecordNameFromEntry,
	kErrGetRecordTypeFromEntry,
	kErrGetRecordAttributeInfo,
	kErrGetRecordReferenceInfo,
	kErrGetDirNodeInfo,

	kErrGetDirNodeCount,
	kErrGetDirNodeList,
	kErrRemoveAttributeValue,
	kErr,

	kErrMemoryAlloc,
	kErrEmptyDataBuff,
	kErrEmptyDataParam,
	kErrBuffTooSmall,
	kErrMaxErrors,
	kUnknownErr = 0xFF
} eErrCodes;


class PwdPolicyTool
{
public:
						PwdPolicyTool			( void );
	virtual				~PwdPolicyTool			( void );
	
	tDirStatus			Initialize				( void );
	tDirStatus			Deinitialize			( void );

	tDirNodeReference	GetLocalNodeRef			( void );
	tDirNodeReference	GetSearchNodeRef		( void );
	tDirStatus			GetUserByName			( tDirNodeReference inNode,
													const char *inUserName,
													const char *inRecordType,
													char **outAuthAuthority,
													char **outNodeName );
												
	tDirStatus			OpenRecord				( tDirNodeReference inNodeRef,
													const char *inRecordType,
													const char *inRecordName,
													tRecordReference *outRecordRef,
													bool inCreate = false );
												
	void				ChangeAuthAuthorityToShadowHash( tRecordReference inRecordRef );
	int					SetUserHashList( tRecordReference inRecordRef, int firstArg, int argc, char * const *argv );
	
	tDirStatus			FindDirectoryNodes		( char *inNodeName, tDirPatternMatch inMatch, char **outNodeNameList[], bool inPrintNames = false );
	tDirStatus			OpenLocalNode			( tDirNodeReference *outNodeRef );
	tDirStatus			OpenDirNode				( char *inNodeName, tDirNodeReference *outNodeRef );
	tDirStatus			CloseDirectoryNode		( tDirNodeReference inNodeRef );
    tDirStatus			DoNodePWAuth			( tDirNodeReference inNode,
                                                    const char *inName,
                                                    const char *inPasswd,
													const char *inMethod,
                                                    const char *inUserName,
                                                    const char *inOther,
													const char *inRecordType,
													char *outResult );
	tDirStatus			DoNodeNativeAuth		( tDirNodeReference inNode, const char *inName, const char *inPasswd );
	
	long				GetHashTypes			( char **outHashTypesStr, bool inExcludeLMHash = false );
	long				SetHashTypes			( const char *inName, char *inPasswd, int arg1, int argc, char * const *argv );
	long				GetHashTypeArray		( CFMutableArrayRef *outHashTypeArray );
	
	tDirReference		GetDirRef				(void) { return fDSRef; };
	
protected:
	tDirStatus	OpenDirectoryServices	( void );
	tDirStatus	CloseDirectoryServices	( void );
	tDirStatus	AllocateTDataBuff		( void );
	tDirStatus	DeallocateTDataBuff		( void );
	tDirStatus	DoGetRecordList			( tDirNodeReference inNodeRef,
											const char *inRecName,
											const char *inRecType,
											const char *inAttrType,
											tDirPatternMatch inMatchType,
											char **outAuthAuthority,
											char **outNodeName );
	void		AppendHashTypeToArray	( const char *inHashType, CFMutableArrayRef inHashTypeArray );
	
private:
	tDirStatus	SetUpAuthBuffs			( tDataBuffer **inAuthBuff, UInt32 inAuthBuffSize, tDataBuffer **inStepBuff, UInt32 inStepBuffSize, tDataBuffer **inTypeBuff, const char *inAuthMethod );

	void		PrintError				( long inErrCode, const char *messageTag = NULL );
	tDirStatus	GetDataFromDataBuff		( tDirNodeReference inNodeRef,
											tDataBuffer *inTDataBuff,
											UInt32 inRecCount,
											char **outAuthAuthority,
											char **outNodeName );

	tDirReference			fDSRef;
	tDataBuffer			   *fTDataBuff;
	tDirNodeReference		fLocalNodeRef;
	tDirNodeReference		fSearchNodeRef;
	tDirNodeReference		fNodeRef;
};


#endif // __PwdPolicyTool_h__

