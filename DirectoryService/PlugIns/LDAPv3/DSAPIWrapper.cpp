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

#include <stdio.h>
#include <DirectoryService/DirServicesConst.h>
#include <DirectoryService/DirServicesUtils.h>

#include "DSAPIWrapper.h"

DSAPIWrapper::DSAPIWrapper() :
	mDSRef(0),
	mNodeRef(0),
	mNodeListBuff(NULL),
	mNodeCount(0),
	mCurrentNodeAuthenticated(false),
	mCurrentNodeIsLDAP(false)
{
}


DSAPIWrapper::~DSAPIWrapper()
{
	if ( mNodeListBuff != NULL )
	{
		dsDataBufferDeAllocate( mDSRef, mNodeListBuff );
		mNodeListBuff = NULL;
	}
	
	this->CloseCurrentNodeRef();
	if ( mDSRef != 0 )
	{
		dsCloseDirService( mDSRef );
		mDSRef = 0;
	}
}


void
DSAPIWrapper::CloseCurrentNodeRef( void )
{
	if ( mNodeRef != 0 )
	{
		dsCloseDirNode( mNodeRef );
		mNodeRef = 0;
		mCurrentNodeAuthenticated = false;
		mCurrentNodeIsLDAP = false;
	}
}


tDirStatus
DSAPIWrapper::OpenDirectoryServices( void )
{
	tDirStatus status = eDSNoErr;
	
	if ( mDSRef == 0 )
		status = dsOpenDirService( &mDSRef );
	
	return status;
}


//-----------------------------------------------------------------------------
//	OpenSpecificPasswordServerNode
//
//	RETURNS: tDirStatus
//-----------------------------------------------------------------------------

tDirStatus
DSAPIWrapper::OpenSpecificPasswordServerNode( const char *inServerAddress )
{
	char pwServerNodeStr[256];
    tDataList *pDataList = nil;
	tDirStatus status = eDSNoErr;
	
	if ( inServerAddress == NULL )
		return eParameterError;
	
	status = this->OpenDirectoryServices();
	if ( status != eDSNoErr )
		return status;
	
	this->CloseCurrentNodeRef();
	
	strcpy( pwServerNodeStr, "/PasswordServer/only/" );
	strlcat( pwServerNodeStr, inServerAddress, sizeof(pwServerNodeStr) );
    
	pDataList = dsBuildFromPath( mDSRef, pwServerNodeStr, "/" );
	status = dsOpenDirNode( mDSRef, pDataList, &mNodeRef );
	dsDataListDeallocate( mDSRef, pDataList );
	free( pDataList );

	return status;
}


//-----------------------------------------------------------------------------
//	OpenLocalLDAPNode
//
//	RETURNS: tDirStatus
//-----------------------------------------------------------------------------

tDirStatus
DSAPIWrapper::OpenLocalLDAPNode( const char *inUser, const char *inPassword )
{
    tDirStatus					status				= eDSNoErr;
    tContextData				context				= NULL;
    tDataListPtr				patternList			= NULL;
	tDataBufferPtr				authBuff			= NULL;
	const size_t				authBuffSize		= 4096;
	tDataBufferPtr				authStepBuff		= NULL;
	tContextData				continueData		= NULL;
	tDataNodePtr				typeBuff			= NULL;
	long						len					= 0;
	
	if ( mCurrentNodeIsLDAP && mCurrentNodeAuthenticated )
		return eDSNoErr;
	
	status = this->OpenDirectoryServices();
	if ( status != eDSNoErr )
		return status;
	
	if ( mNodeListBuff == NULL )
	{
		mNodeListBuff = dsDataBufferAllocate( mDSRef, 4096 );
		if ( mNodeListBuff == NULL )
			return eMemoryError;
	}
	else
	{
		mNodeListBuff->fBufferLength = 0;
	}
	
	// find and don't open
	
	patternList = dsBuildFromPath( mDSRef, "/LDAPv3/127.0.0.1", "/" );
	if ( patternList == NULL )
		return eMemoryError;
	
	status = dsFindDirNodes( mDSRef, mNodeListBuff, patternList, eDSExact, &mNodeCount, &context );
	
	dsDataListDeallocate( mDSRef, patternList );
	free( patternList );
	patternList = NULL;
	
	if ( status != eDSNoErr )
		return status;
	
	if ( mNodeCount < 1 )
		return eDSNodeNotFound;
	
	status = this->OpenLocallyHostedNode( 1 );
	if ( status != eDSNoErr )
		return status;
	
	mCurrentNodeIsLDAP = true;
	
	if ( inUser != NULL && inPassword != NULL )
	{
		authBuff = dsDataBufferAllocate( mDSRef, authBuffSize );
		if ( authBuff == NULL )
			return eMemoryError;
		
		authStepBuff = dsDataBufferAllocate( mDSRef, 2048 );
		if ( authStepBuff == NULL )
			return eMemoryError;
		
		typeBuff = dsDataNodeAllocateString( mDSRef, kDSStdAuthNodeNativeClearTextOK );
		if ( typeBuff == NULL )
			return eMemoryError;
		
		len = strlen( inUser );
		memcpy( authBuff->fBufferData, &len, 4 );
		strlcpy( authBuff->fBufferData + 4, inUser, authBuffSize - 4 );
		authBuff->fBufferLength = 4 + len;
		
		len = strlen( inPassword );
		memcpy( authBuff->fBufferData + authBuff->fBufferLength, &len, 4 );
		strlcpy( authBuff->fBufferData + authBuff->fBufferLength + 4, inPassword, authBuffSize - authBuff->fBufferLength - 4 );
		authBuff->fBufferLength += 4 + len;
		
		status = dsDoDirNodeAuth( this->GetCurrentNodeRef(), typeBuff, false, authBuff, authStepBuff, &continueData );
		if ( status == eDSNoErr )
			mCurrentNodeAuthenticated = true;
		
		dsDataBufferDeAllocate( mDSRef, authBuff );
		dsDataBufferDeAllocate( mDSRef, authStepBuff );
		dsDataNodeDeAllocate( mDSRef, typeBuff );
	}
	
	return status;
}


//-----------------------------------------------------------------------------
//	OpenNodeByName
//
//	RETURNS: tDirStatus
//-----------------------------------------------------------------------------

tDirStatus
DSAPIWrapper::OpenNodeByName( const char *inNodeName, const char *inUser, const char *inPassword )
{
    tDirStatus					status				= eDSNoErr;
	tDataListPtr				nodeName			= NULL;
	
	nodeName = dsBuildFromPath( mDSRef, inNodeName, "/" );
	if ( nodeName == NULL )
		return eMemoryError;
		
	status = this->OpenNodeByName( nodeName, inUser, inPassword );
	
	dsDataListDeallocate( mDSRef, nodeName );
	
	return status;
}


//-----------------------------------------------------------------------------
//	OpenNodeByName
//
//	RETURNS: tDirStatus
//-----------------------------------------------------------------------------

tDirStatus
DSAPIWrapper::OpenNodeByName( tDataListPtr inNodeName, const char *inUser, const char *inPassword )
{
    tDirStatus					status				= eDSNoErr;
	tDataBufferPtr				authBuff			= NULL;
	const size_t				authBuffSize		= 4096;
	tDataBufferPtr				authStepBuff		= NULL;
	tContextData				continueData		= NULL;
	tDataNodePtr				typeBuff			= NULL;
	long						len					= 0;
	
	if ( inNodeName == NULL )
		return eParameterError;
	
	status = this->OpenDirectoryServices();
	if ( status != eDSNoErr )
		return status;
	
	if ( mNodeListBuff == NULL )
	{
		mNodeListBuff = dsDataBufferAllocate( mDSRef, 4096 );
		if ( mNodeListBuff == NULL )
			return eMemoryError;
	}
	else
	{
		mNodeListBuff->fBufferLength = 0;
	}
	
	// find and don't open
		
	status = dsOpenDirNode( mDSRef, inNodeName, &mNodeRef );
	if ( status != eDSNoErr )
		return status;
	
	if ( inUser != NULL && inPassword != NULL )
	{
		authBuff = dsDataBufferAllocate( mDSRef, authBuffSize );
		if ( authBuff == NULL )
			return eMemoryError;
		
		authStepBuff = dsDataBufferAllocate( mDSRef, 2048 );
		if ( authStepBuff == NULL )
			return eMemoryError;
		
		typeBuff = dsDataNodeAllocateString( mDSRef, kDSStdAuthNodeNativeNoClearText );
		if ( typeBuff == NULL )
			return eMemoryError;
		
		len = strlen( inUser );
		memcpy( authBuff->fBufferData, &len, 4 );
		strlcpy( authBuff->fBufferData + 4, inUser, authBuffSize - 4 );
		authBuff->fBufferLength = 4 + len;
		
		len = strlen( inPassword );
		memcpy( authBuff->fBufferData + authBuff->fBufferLength, &len, 4 );
		strlcpy( authBuff->fBufferData + authBuff->fBufferLength + 4, inPassword, authBuffSize - authBuff->fBufferLength - 4 );
		authBuff->fBufferLength += 4 + len;
		
		status = dsDoDirNodeAuth( this->GetCurrentNodeRef(), typeBuff, false, authBuff, authStepBuff, &continueData );
		if ( status == eDSNoErr )
			mCurrentNodeAuthenticated = true;
			
		dsDataBufferDeAllocate( mDSRef, authBuff );
		dsDataBufferDeAllocate( mDSRef, authStepBuff );
		dsDataNodeDeAllocate( mDSRef, typeBuff );
	}
	
	return status;	
}


//-----------------------------------------------------------------------------
//	GetLocallyHostedNodeList
//
//	RETURNS: tDirStatus
//-----------------------------------------------------------------------------

tDirStatus
DSAPIWrapper::GetLocallyHostedNodeList( void )
{
    tDirStatus					status				= eDSNoErr;
    tContextData				context				= NULL;
    
	status = this->OpenDirectoryServices();
	if ( status != eDSNoErr )
		return status;
	
	if ( mNodeListBuff == NULL )
	{
		mNodeListBuff = dsDataBufferAllocate( mDSRef, 4096 );
		if ( mNodeListBuff == NULL )
			return eMemoryError;
	}
	
	// find and don't open
	status = dsFindDirNodes( mDSRef, mNodeListBuff, NULL, eDSLocalHostedNodes, &mNodeCount, &context );
	if ( status != eDSNoErr )
		return status;
	
	if ( mNodeCount < 1 )
		status = eDSNodeNotFound;
	
	return status;
}


tDirStatus
DSAPIWrapper::OpenLocallyHostedNode( UInt32 inNodeIndex )
{
    tDirStatus			status				= eDSNoErr;
    tDataList			*nodeName			= NULL;
	
	// make sure the state is correct
	if ( mDSRef == 0 || mNodeListBuff == NULL || inNodeIndex > mNodeCount )
		return eParameterError;
	
	this->CloseCurrentNodeRef();
	
	status = dsGetDirNodeName( mDSRef, mNodeListBuff, inNodeIndex, &nodeName );
	if ( status != eDSNoErr )
		return status;
	
	status = dsOpenDirNode( mDSRef, nodeName, &mNodeRef );
	dsDataListDeallocate( mDSRef, nodeName );
	free( nodeName );
                        
	return status;
}


tDirStatus
DSAPIWrapper::OpenRecord(
	const char *inRecordType,
	const char *inRecordName,
	tRecordReference *outRecordRef,
	bool inCreate )
{
    tDirStatus				status				= eDSNoErr;
    tDataNodePtr			recordTypeNode		= NULL;
    tDataNodePtr			recordNameNode		= NULL;    

	// make sure the state is correct
	if ( mDSRef == 0 || mNodeRef == 0 || inRecordType == NULL || inRecordName == NULL || outRecordRef == NULL )
		return eParameterError;
	
	recordTypeNode = dsDataNodeAllocateString( mDSRef, inRecordType );
	recordNameNode = dsDataNodeAllocateString( mDSRef, inRecordName );
	
	status = dsOpenRecord( mNodeRef, recordTypeNode, recordNameNode, outRecordRef );
	
	if ( inCreate && status == eDSRecordNotFound )
		status = dsCreateRecordAndOpen( mNodeRef, recordTypeNode, recordNameNode, outRecordRef );
	
	if ( recordTypeNode ) {
		dsDataNodeDeAllocate( mDSRef, recordTypeNode );
		recordTypeNode = NULL;
	}
	if ( recordNameNode ) {
		dsDataNodeDeAllocate( mDSRef, recordNameNode );
		recordNameNode = NULL;
	}
	
	return status;
}


//-------------------------------------------------------------------------------------
//	AddShortName
//-------------------------------------------------------------------------------------

tDirStatus
DSAPIWrapper::AddShortName( tRecordReference inRecordRef, const char *inShortName )
{
    tDirStatus              status          = eDSNoErr;
    tDataNodePtr            attrTypeNode    = NULL;
    tDataNodePtr            attrNameNode    = NULL;    
    tAttributeValueEntryPtr attrValue       = NULL;
	
	// make sure the state is correct
	if ( mDSRef == 0 || mNodeRef == 0 || inRecordRef == 0 || inShortName == NULL )
		return eParameterError;
	
	attrTypeNode = dsDataNodeAllocateString( mDSRef, kDSNAttrRecordName );
	attrNameNode = dsDataNodeAllocateString( mDSRef, inShortName );
    
    // add the value if it's not already there
    if ( dsGetRecordAttributeValueByValue( inRecordRef, attrTypeNode, attrNameNode, &attrValue ) != eDSNoErr )
    {
        status = dsAddAttributeValue( inRecordRef, attrTypeNode, attrNameNode );
    }
	
	if ( attrTypeNode ) {
		dsDataNodeDeAllocate( mDSRef, attrTypeNode );
		attrTypeNode = NULL;
	}
	if ( attrNameNode ) {
		dsDataNodeDeAllocate( mDSRef, attrNameNode );
		attrNameNode = NULL;
	}
    if ( attrValue )
    {
        dsDeallocAttributeValueEntry( mDSRef, attrValue);
        attrValue = NULL;
    }
	
	return status;
}


//-------------------------------------------------------------------------------------
//	DoDirNodeAuthOnRecordType
//-------------------------------------------------------------------------------------

tDirStatus
DSAPIWrapper::DoDirNodeAuthOnRecordType(
	const char *inAuthType,
	bool inAuthOnly,
	tDataBufferPtr inSendDataBufPtr,
	tDataBufferPtr inResponseDataBufPtr,
	tContextData *inOutContinueData,
	const char *inRecType )
{
    tDirStatus				status				= eDSNoErr;
    tDataNodePtr			recordTypeNode		= NULL;
    tDataNodePtr			authTypeNode		= NULL;    

	recordTypeNode = dsDataNodeAllocateString( mDSRef, inRecType );
	authTypeNode = dsDataNodeAllocateString( mDSRef, inAuthType );
	
	status = dsDoDirNodeAuthOnRecordType( mNodeRef, authTypeNode, inAuthOnly, inSendDataBufPtr, inResponseDataBufPtr,
						inOutContinueData, recordTypeNode );
	
	if ( recordTypeNode ) {
		dsDataNodeDeAllocate( mDSRef, recordTypeNode );
		recordTypeNode = NULL;
	}
	if ( authTypeNode ) {
		dsDataNodeDeAllocate( mDSRef, authTypeNode );
		authTypeNode = NULL;
	}
	
	return status;
}


//-------------------------------------------------------------------------------------
//	GetServerAddressForUser
//-------------------------------------------------------------------------------------

tDirStatus
DSAPIWrapper::GetServerAddressForUser( const char *uname, char *serverAddress, char **userNodeName )
{
    tDataBuffer				   *tDataBuff			= NULL;
    tDirNodeReference			nodeRef				= 0;
    tDirStatus					status				= eDSNoErr;
    tContextData				context				= NULL;
	UInt32						nodeCount			= 0;
	UInt32						attrIndex			= 0;
	tDataList				   *nodeName			= NULL;
    tAttributeEntryPtr			pAttrEntry			= NULL;
	tDataList				   *pRecName			= NULL;
	tDataList				   *pRecType			= NULL;
	tDataList				   *pAttrType			= NULL;
	UInt32						recCount			= 0;
	tRecordEntry		  	 	*pRecEntry			= NULL;
	tAttributeListRef			attrListRef			= 0;
	char					   *pUserLocation		= NULL;
	tAttributeValueListRef		valueRef			= 0;
	tAttributeValueEntry  	 	*pValueEntry		= NULL;
	tDataList				   *pUserNode			= NULL;
	tDirNodeReference			userNodeRef			= 0;
	
	if ( uname == NULL )
		return eDSAuthUnknownUser;
	
	if ( userNodeName != NULL )
		*userNodeName = NULL;
	
	try
    {
		status = this->OpenDirectoryServices();
		if ( status != eDSNoErr )
			throw( status );
		
		tDataBuff = dsDataBufferAllocate( mDSRef, 4096 );
		if (tDataBuff == NULL)
			throw( (tDirStatus)eMemoryError );
        
		// find on search node
		status = dsFindDirNodes( mDSRef, tDataBuff, NULL, eDSSearchNodeName, &nodeCount, &context );
		if (status != eDSNoErr)
			throw( status );
		if ( nodeCount < 1 )
			throw( (tDirStatus)eDSNodeNotFound );
		
		status = dsGetDirNodeName( mDSRef, tDataBuff, 1, &nodeName );
		if (status != eDSNoErr)
			throw( status );
		
		status = dsOpenDirNode( mDSRef, nodeName, &nodeRef );
		dsDataListDeallocate( mDSRef, nodeName );
		free( nodeName );
		nodeName = NULL;
		if (status != eDSNoErr)
			throw( status );
		
		pRecName = dsBuildListFromStrings( mDSRef, uname, NULL );
		pRecType = dsBuildListFromStrings( mDSRef, kDSStdRecordTypeUsers, NULL );
		pAttrType = dsBuildListFromStrings( mDSRef, kDSNAttrMetaNodeLocation, NULL );
		
		recCount = 1;
		status = dsGetRecordList( nodeRef, tDataBuff, pRecName, eDSExact, pRecType, pAttrType, 0, &recCount, &context );
		if ( status != eDSNoErr )
			throw( status );
		if ( recCount == 0 )
			throw( (tDirStatus)eDSAuthUnknownUser );
		
		status = dsGetRecordEntry( nodeRef, tDataBuff, 1, &attrListRef, &pRecEntry );
		if ( status != eDSNoErr )
			throw( status );
		
		for ( attrIndex = 1; (attrIndex <= pRecEntry->fRecordAttributeCount) && (status == eDSNoErr); attrIndex++ )
		{
			status = dsGetAttributeEntry( nodeRef, tDataBuff, attrListRef, attrIndex, &valueRef, &pAttrEntry );
			if ( status == eDSNoErr && pAttrEntry != NULL )
			{
				if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
				{
					status = dsGetAttributeValue( nodeRef, tDataBuff, 1, valueRef, &pValueEntry );
					if ( status == eDSNoErr && pValueEntry != NULL )
					{
						pUserLocation = (char *) calloc( pValueEntry->fAttributeValueData.fBufferLength + 1, sizeof(char) );
						memcpy( pUserLocation, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );
					}
				}
				
				if ( pValueEntry != NULL )
					dsDeallocAttributeValueEntry( mDSRef, pValueEntry );
				pValueEntry = NULL;
				
				dsDeallocAttributeEntry( mDSRef, pAttrEntry );
				pAttrEntry = NULL;
				dsCloseAttributeValueList( valueRef );
				valueRef = 0;
			}
		}
		
		dsCloseAttributeList( attrListRef );
		attrListRef = 0;
		dsDeallocRecordEntry( mDSRef, pRecEntry );
		pRecEntry = NULL;
		
		if ( pUserLocation == NULL )
			throw( (tDirStatus)eDSAuthUnknownUser );
		
		
		pUserNode = dsBuildFromPath( mDSRef, pUserLocation, "/" );
		status = dsOpenDirNode( mDSRef, pUserNode, &userNodeRef );
		if ( status != eDSNoErr )
			throw( status );
		
		// assign after we know we can open the node
		if ( userNodeName != NULL )
			*userNodeName = pUserLocation;
		
		// now, get the config record
		if (pRecName != NULL) {
			dsDataListDeallocate( mDSRef, pRecName );
			free( pRecName );
			pRecName = NULL;
		}
		if (pRecType != NULL) {
			dsDataListDeallocate( mDSRef, pRecType );
			free( pRecType );
			pRecType = NULL;
		}
		if (pAttrType != NULL) {
			dsDataListDeallocate( mDSRef, pAttrType );
			free( pAttrType );
			pAttrType = NULL;
		}
		
		pRecName = dsBuildListFromStrings( mDSRef, "passwordserver", NULL );
		pRecType = dsBuildListFromStrings( mDSRef, kDSStdRecordTypeConfig, NULL );
		pAttrType = dsBuildListFromStrings( mDSRef, kDS1AttrPasswordServerLocation, NULL );
		
		recCount = 1;
		status = dsGetRecordList( nodeRef, tDataBuff, pRecName, eDSExact, pRecType, pAttrType, 0, &recCount, &context );
		if ( status != eDSNoErr )
			throw( status );
		if ( recCount == 0 )
			throw( (tDirStatus)eDSRecordNotFound );
		
		status = dsGetRecordEntry( nodeRef, tDataBuff, 1, &attrListRef, &pRecEntry );
		if ( status != eDSNoErr )
			throw( status );
		
		for ( attrIndex = 1; (attrIndex <= pRecEntry->fRecordAttributeCount) && (status == eDSNoErr); attrIndex++ )
		{
			status = dsGetAttributeEntry( nodeRef, tDataBuff, attrListRef, attrIndex, &valueRef, &pAttrEntry );
			if ( status == eDSNoErr && pAttrEntry != NULL )
			{
				if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrPasswordServerLocation ) == 0 )
				{
					status = dsGetAttributeValue( nodeRef, tDataBuff, 1, valueRef, &pValueEntry );
					if ( status == eDSNoErr && pValueEntry != NULL )
					{
						memcpy( serverAddress, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );
						serverAddress[pValueEntry->fAttributeValueData.fBufferLength] = '\0';
					}
				}
				
				if ( pValueEntry != NULL )
					dsDeallocAttributeValueEntry( mDSRef, pValueEntry );
				pValueEntry = NULL;
				
				dsDeallocAttributeEntry( mDSRef, pAttrEntry );
				pAttrEntry = NULL;
				dsCloseAttributeValueList( valueRef );
				valueRef = 0;
			}
		}
	}
	catch ( tDirStatus catchErr )
	{
		status = catchErr;
	}
	
	if ( attrListRef != 0 ) {
		dsCloseAttributeList( attrListRef );
		attrListRef = 0;
	}
	if ( pRecEntry != NULL ) {
		dsDeallocRecordEntry( mDSRef, pRecEntry );
		pRecEntry = NULL;
	}
	
    if (tDataBuff != NULL) {
		bzero( tDataBuff, tDataBuff->fBufferSize );
		dsDataBufferDeAllocate( mDSRef, tDataBuff );
		tDataBuff = NULL;
	}
	
	if (pUserLocation != NULL && userNodeName == NULL ) {
		free( pUserLocation );
		pUserLocation = NULL;
	}
	if (pRecName != NULL) {
		dsDataListDeallocate( mDSRef, pRecName );
		free( pRecName );
		pRecName = NULL;
	}
	if (pRecType != NULL) {
		dsDataListDeallocate( mDSRef, pRecType );
		free( pRecType );
		pRecType = NULL;
	}
	if (pAttrType != NULL) {
		dsDataListDeallocate( mDSRef, pAttrType );
		free( pAttrType );
		pAttrType = NULL;
	}
    if (nodeRef != 0) {
		dsCloseDirNode(nodeRef);
		nodeRef = 0;
	}
	
	return status;
}

//-------------------------------------------------------------------------------------
//	CopyRecordName
//-------------------------------------------------------------------------------------

char *
DSAPIWrapper::CopyRecordName( tRecordReference inRecordRef )
{
	tRecordEntryPtr recEntry = NULL;
	char *recName = NULL;

	if ( dsGetRecordReferenceInfo(inRecordRef, &recEntry) == eDSNoErr ) {
		dsGetRecordNameFromEntry( recEntry, &recName );
		dsDeallocRecordEntry( 0, recEntry );
	}

	return recName;
}
