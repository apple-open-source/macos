/*
 * Copyright (c) 2008 Apple Computer, Inc. All rights reserved.
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

#ifndef __DSAPIWrapper__
#define __DSAPIWrapper__

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesTypes.h>

class DSAPIWrapper
{
	public:
	
											DSAPIWrapper();
		virtual								~DSAPIWrapper();
	
		virtual void						CloseCurrentNodeRef( void );
		virtual tDirStatus					OpenDirectoryServices( void );
		virtual tDirStatus					OpenSpecificPasswordServerNode( const char *inServerAddress );
		virtual tDirStatus					OpenLocalLDAPNode( const char *inUser, const char *inPassword );
		virtual tDirStatus					OpenNodeByName( const char *inNodeName, const char *inUser, const char *inPassword );
		virtual tDirStatus					OpenNodeByName( tDataListPtr inNodeName, const char *inUser, const char *inPassword );
		virtual tDirStatus					GetLocallyHostedNodeList( void );
		virtual tDirStatus					OpenLocallyHostedNode( UInt32 inNodeIndex );
		
		virtual tDirStatus					OpenRecord(
													const char *inRecordType,
													const char *inRecordName,
													tRecordReference *outRecordRef,
													bool inCreate = false );
		
		virtual tDirStatus					AddShortName(
													tRecordReference inRecordRef,
													const char *inShortName );
		
		virtual tDirStatus					DoDirNodeAuthOnRecordType(
													const char *inAuthType,
													bool inAuthOnly,
													tDataBufferPtr inSendDataBufPtr,
													tDataBufferPtr inResponseDataBufPtr,
													tContextData *inOutContinueData,
													const char *inRecType );
											
		virtual tDirStatus					GetServerAddressForUser( const char *uname, char *serverAddress, char **userNodeName );
		
		tDirReference						GetDSRef( void ) { return mDSRef; };
		tDirNodeReference					GetCurrentNodeRef( void ) { return mNodeRef; };
		UInt32								GetLocallyHostedNodeCount( void ) { return mNodeCount; };
		char								*CopyRecordName( tRecordReference inRecordRef );

	protected:
		
		tDirReference mDSRef;
		tDirNodeReference mNodeRef;
		tDataBuffer *mNodeListBuff;
		UInt32 mNodeCount;
		bool mCurrentNodeAuthenticated;
		bool mCurrentNodeIsLDAP;
};

#endif
