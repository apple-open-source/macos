/*
 *  DirServiceUtils.cpp
 *  NeST
 *
 *  Created by admin on Fri Apr 04 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#include <stdio.h>

#include "DSUtils.h"

DSUtils::DSUtils()
{
	mDSRef = 0;
	mNodeRef = 0;
	mNodeListBuff = NULL;
	mNodeCount = 0;
	mCurrentNodeAuthenticated = false;
	mCurrentNodeIsLDAP = false;
}


DSUtils::~DSUtils()
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
DSUtils::CloseCurrentNodeRef( void )
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
DSUtils::OpenDirectoryServices( void )
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
DSUtils::OpenSpecificPasswordServerNode( const char *inServerAddress )
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
	strcat( pwServerNodeStr, inServerAddress );
    
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
DSUtils::OpenLocalLDAPNode( const char *inUser, const char *inPassword )
{
    tDirStatus					status				= eDSNoErr;
    tContextData				context				= NULL;
    tDataListPtr				patternList			= NULL;
	tDataBufferPtr				authBuff			= NULL;
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
		authBuff = dsDataBufferAllocate( mDSRef, 4096 );
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
		strcpy( authBuff->fBufferData + 4, inUser );
		authBuff->fBufferLength = 4 + len;
		
		len = strlen( inPassword );
		memcpy( authBuff->fBufferData + authBuff->fBufferLength, &len, 4 );
		strcpy( authBuff->fBufferData + authBuff->fBufferLength + 4, inPassword );
		authBuff->fBufferLength += 4 + len;
		
		status = dsDoDirNodeAuth( this->GetCurrentNodeRef(), typeBuff, false, authBuff, authStepBuff, &continueData );
		if ( status == eDSNoErr )
			mCurrentNodeAuthenticated = true;
	}
	
	return status;
}


//-----------------------------------------------------------------------------
//	OpenNodeByName
//
//	RETURNS: tDirStatus
//-----------------------------------------------------------------------------

tDirStatus
DSUtils::OpenNodeByName( const char *inNodeName, const char *inUser, const char *inPassword )
{
    tDirStatus					status				= eDSNoErr;
    tDataListPtr				nodeName			= NULL;
	tDataBufferPtr				authBuff			= NULL;
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
	
	nodeName = dsBuildFromPath( mDSRef, inNodeName, "/" );
	if ( nodeName == NULL )
		return eMemoryError;
		
	status = dsOpenDirNode( mDSRef, nodeName, &mNodeRef );
	dsDataListDeallocate( mDSRef, nodeName );
	free( nodeName );
	if ( status != eDSNoErr )
		return status;
	
	if ( inUser != NULL && inPassword != NULL )
	{
		authBuff = dsDataBufferAllocate( mDSRef, 4096 );
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
		strcpy( authBuff->fBufferData + 4, inUser );
		authBuff->fBufferLength = 4 + len;
		
		len = strlen( inPassword );
		memcpy( authBuff->fBufferData + authBuff->fBufferLength, &len, 4 );
		strcpy( authBuff->fBufferData + authBuff->fBufferLength + 4, inPassword );
		authBuff->fBufferLength += 4 + len;
		
		status = dsDoDirNodeAuth( this->GetCurrentNodeRef(), typeBuff, false, authBuff, authStepBuff, &continueData );
		if ( status == eDSNoErr )
			mCurrentNodeAuthenticated = true;
	}
	
	return status;	
}


//-----------------------------------------------------------------------------
//	OpenNetInfoParentNode
//
//	RETURNS: tDirStatus
//
//	Opens the NetInfo parent node if the local machine is hosting one.
//-----------------------------------------------------------------------------

tDirStatus
DSUtils::OpenNetInfoParentNode( void )
{
    tDirStatus					status				= eDSNoErr;
	tDataList					*pDataList			= NULL;
    
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

	pDataList = dsBuildFromPath( mDSRef, "/NetInfo/..", "/" );
	status = dsOpenDirNode( mDSRef, pDataList, &mNodeRef );
	dsDataListDeallocate( mDSRef, pDataList );
	free( pDataList );
	
	return status;
}


//-----------------------------------------------------------------------------
//	GetLocallyHostedNodeList
//
//	RETURNS: tDirStatus
//-----------------------------------------------------------------------------

tDirStatus
DSUtils::GetLocallyHostedNodeList( void )
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
DSUtils::OpenLocallyHostedNode( unsigned long inNodeIndex )
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
DSUtils::OpenRecord(
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


//--------------------------------------------------------------------------------------------------
// * DoActionOnCurrentNode ()
//--------------------------------------------------------------------------------------------------

tDirStatus
DSUtils::DoActionOnCurrentNode( void )
{
	fprintf( stderr, "warning: DSUtils::DoActionOnCurrentNode() called, does not perform any action." );
	return (tDirStatus)0;
}


//--------------------------------------------------------------------------------------------------
// * DoActionForAllLocalNodes ()
//
//  Returns: the first error encountered
//--------------------------------------------------------------------------------------------------

tDirStatus
DSUtils::DoActionForAllLocalNodes( void )
{
    tDirStatus					status				= eDSNoErr;
    tDirStatus					status1				= eDSNoErr;
    unsigned long				index				= 0;
    unsigned long				nodeCount			= 0;
	
	status = this->GetLocallyHostedNodeList();
	if ( status != eDSNoErr )
		return status;
	
	nodeCount = this->GetLocallyHostedNodeCount();
	for ( index = 1; index <= nodeCount; index++ )
	{
		status1 = this->OpenLocallyHostedNode( index );
		if ( status1 != eDSNoErr )
		{
			if ( status == eDSNoErr )
				status = status1;
			continue;
		}
		
		status1 = this->DoActionOnCurrentNode();
		if ( status1 != eDSNoErr && status == eDSNoErr )
			status = status1;
	}
	
	return status;
}


//--------------------------------------------------------------------------------------------------
// * FillAuthBuff ()
//
//		inCount		== Number of uInt32, void* pairs
//		va args		== uInt32, void* pairs
//--------------------------------------------------------------------------------------------------

tDirStatus
DSUtils::FillAuthBuff( tDataBuffer *inAuthBuff, unsigned long inCount, unsigned long inLen, ... )
{
	tDirStatus			error		= eDSNoErr;
	unsigned long		curr		= 0;
	unsigned long		buffSize	= 0;
	unsigned long		count		= inCount;
	unsigned long		len			= inLen;
	const void	 		*data		= NULL;
	bool		firstPass	= true;
	char	   *p			= nil;
	va_list		args;

	// If the buffer is nil, we have nowhere to put the data
	if ( inAuthBuff == nil )
		return eParameterError;
	
	// If the buffer is nil, we have nowhere to put the data
	if ( inAuthBuff->fBufferData == nil )
	{
		return( eDSEmptyBuffer );
	}
	
	// Get buffer info
	p		 = inAuthBuff->fBufferData;
	buffSize = inAuthBuff->fBufferSize;
	
	// Set up the arg list
	va_start( args, inLen );
	data = va_arg( args, void * );
	
	while ( count-- > 0 )
	{
		if ( !firstPass )
		{
			len = va_arg( args, unsigned long );
			data = va_arg( args, void * );
		}
		
		if ( (curr + len) > buffSize )
		{
			// This is bad, lets bail
			return( eDSBufferTooSmall );
		}
		
		::memcpy( &(p[ curr ]), &len, sizeof( long ) );
		curr += sizeof( long );

		if ( len > 0 )
		{
			memcpy( &(p[ curr ]), data, len );
			curr += len;
		}
		firstPass = false;
	}

	inAuthBuff->fBufferLength = curr;

	return( error );

} // FillAuthBuff


//-------------------------------------------------------------------------------------
//	GetServerAddressForUser
//-------------------------------------------------------------------------------------

tDirStatus
DSUtils::GetServerAddressForUser( const char *uname, char *serverAddress, char **userNodeName )
{
    tDataBuffer				   *tDataBuff			= NULL;
    tDirNodeReference			nodeRef				= 0;
    tDirStatus					status				= eDSNoErr;
    tContextData				context				= NULL;
	unsigned long				nodeCount			= 0;
	unsigned long				attrIndex			= 0;
	tDataList				   *nodeName			= NULL;
    tAttributeEntryPtr			pAttrEntry			= NULL;
	tDataList				   *pRecName			= NULL;
	tDataList				   *pRecType			= NULL;
	tDataList				   *pAttrType			= NULL;
	unsigned long				recCount			= 0;
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

