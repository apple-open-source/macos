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
	virtual	   ~PwdPolicyTool			( void );

	long		Initialize				( void );
	long		Deinitialize			( void );

	tDirNodeReference	GetLocalNodeRef			( void );
	tDirNodeReference	GetSearchNodeRef		( void );
	long				GetUserByName			( tDirNodeReference inNode,
													const char *inUserName,
													char **outAuthAuthority,
													char **outNodeName );
												
	tDirStatus			OpenRecord				( tDirNodeReference inNodeRef,
													const char *inRecordType,
													const char *inRecordName,
													tRecordReference *outRecordRef,
													bool inCreate = false );
												
	void				ChangeAuthAuthorityToShadowHash( tRecordReference inRecordRef );
	int					SetUserHashList( tRecordReference inRecordRef, int firstArg, int argc, char * const *argv );
	
	long				FindDirectoryNodes		( char *inNodeName, tDirPatternMatch inMatch, char **outNodeName, bool inPrintNames = false );
	long				OpenDirNode				( char *inNodeName, tDirNodeReference *outNodeRef );
	long				CloseDirectoryNode		( tDirNodeReference inNodeRef );
    long				DoNodePWAuth			( tDirNodeReference inNode,
                                                    const char *inUserName,
                                                    char *inPasswd,
													const char *inMethod,
                                                    char *inUserName,
                                                    const char *inOther,
													char *outResult );
	long				DoNodeNativeAuth		( tDirNodeReference inNode, const char *inName, char *inPasswd );
	
	long				GetHashTypes			( char **outHashTypesStr, bool inExcludeLMHash = false );
	long				SetHashTypes			( const char *inName, char *inPasswd, int arg1, int argc, char * const *argv );
	long				GetHashTypeArray		( CFMutableArrayRef *outHashTypeArray );
	
	tDirReference		GetDirRef				(void) { return fDSRef; };
	
protected:
	long		OpenDirectoryServices	( void );
	long		CloseDirectoryServices	( void );
	long		AllocateTDataBuff		( void );
	long		DeallocateTDataBuff		( void );
	long		DoGetRecordList			( tDirNodeReference inNodeRef,
											const char *inRecName,
											char *inRecType,
											char *inAttrType,
											tDirPatternMatch inMatchType,
											char **outAuthAuthority,
											char **outNodeName );
	void		AppendHashTypeToArray	( const char *inHashType, CFMutableArrayRef inHashTypeArray );
	
private:
	long		FillAuthBuff			( tDataBuffer *inAuthBuff, unsigned long inCount, unsigned long inLen, const void *inData ... );
	long		SetUpAuthBuffs			( tDataBuffer **inAuthBuff, unsigned long inAuthBuffSize, tDataBuffer **inStepBuff, unsigned long inStepBuffSize, tDataBuffer **inTypeBuff, const char *inAuthMethod );

	void		PrintError				( long inErrCode, const char *messageTag = NULL );
	long		GetDataFromDataBuff		( tDirNodeReference inNodeRef,
											tDataBuffer *inTDataBuff,
											unsigned long inRecCount,
											char **outAuthAuthority,
											char **outNodeName );

	tDirReference			fDSRef;
	tDataBuffer			   *fTDataBuff;
	tDirNodeReference		fLocalNodeRef;
	tDirNodeReference		fSearchNodeRef;
	tDirNodeReference		fNodeRef;
};


#endif // __PwdPolicyTool_h__

