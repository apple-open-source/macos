/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include "buffer_unpackers.h"
#include "DSUtils.h"

//------------------------------------------------------------------------------------
//	* Get2FromBuffer
//------------------------------------------------------------------------------------

sInt32 Get2FromBuffer( tDataBufferPtr inAuthData, tDataList **inOutDataList, char **inOutItemOne, char **inOutItemTwo, unsigned int *outItemCount )
{
	sInt32			siResult		= eDSNoErr;
	tDataList		*dataList		= NULL;
	unsigned int	itemCount		= 0;
	
	try
	{
		// parse input first
		dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
		if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
		itemCount = dsDataListGetNodeCountPriv(dataList);
		if ( outItemCount != NULL )
			*outItemCount = itemCount;
		if ( itemCount < 2 ) throw( (sInt32)eDSInvalidBuffFormat );
		
		// this allocates a copy of the string
		*inOutItemOne = dsDataListGetNodeStringPriv(dataList, 1);
		if ( *inOutItemOne == nil ) throw( (sInt32)eDSInvalidBuffFormat );
		if ( strlen(*inOutItemOne) < 1 )
			siResult = eDSInvalidBuffFormat;
		
		// this allocates a copy of the string
		*inOutItemTwo = dsDataListGetNodeStringPriv(dataList, 2);
		if ( *inOutItemTwo == nil ) throw( (sInt32)eDSInvalidBuffFormat );
	}
	catch( sInt32 catchErr )
	{
		siResult = catchErr;
	}
	
	if ( inOutDataList != NULL )
	{
		*inOutDataList = dataList;
	}
	else
	{
		if ( dataList != NULL )
		{
			dsDataListDeallocatePriv(dataList);
			free(dataList);
			dataList = NULL;
		}
	}
	
	return siResult;
}


//------------------------------------------------------------------------------------
//	UnpackSambaBufferFirstThreeItems
//
//	Returns: ds err code
//------------------------------------------------------------------------------------

sInt32 UnpackSambaBufferFirstThreeItems( tDataBufferPtr inAuthData, tDataListPtr *outDataList, char **outUserName, unsigned char *outChallenge, unsigned long *outChallengeLen, unsigned char **outResponse, unsigned long *outResponseLen )
{
	sInt32				siResult					= eDSNoErr;
	tDataListPtr		dataList					= NULL;
	tDataNodePtr		pC8Node						= NULL;
	tDataNodePtr		pP24InputNode				= NULL;
	
	try
	{
		// parse input first
		dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
		if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
		if ( dsDataListGetNodeCountPriv(dataList) < 3 ) throw( (sInt32)eDSInvalidBuffFormat );
		
		// this allocates a copy of the string
		*outUserName = dsDataListGetNodeStringPriv(dataList, 1);
		if ( *outUserName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
		if ( strlen(*outUserName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );
		
		// these are not copies
		siResult = dsDataListGetNodePriv(dataList, 2, &pC8Node);
		if ( pC8Node == nil ) throw( (sInt32)eDSInvalidBuffFormat );
		if ( pC8Node->fBufferLength > 16 ) throw( (sInt32)eDSInvalidBuffFormat);
		if ( siResult != eDSNoErr ) throw( (sInt32)eDSInvalidBuffFormat );
		memmove(outChallenge, ((tDataBufferPriv*)pC8Node)->fBufferData, pC8Node->fBufferLength);
		*outChallengeLen = pC8Node->fBufferLength;
		
		siResult = dsDataListGetNodePriv(dataList, 3, &pP24InputNode);
		if ( siResult != eDSNoErr || pP24InputNode == nil )
			throw( (sInt32)eDSInvalidBuffFormat );
		
		*outResponse = (unsigned char *) malloc( pP24InputNode->fBufferLength );
		if ( *outResponse == NULL )
			throw( (sInt32)eMemoryError );
		*outResponseLen = pP24InputNode->fBufferLength;
		memmove(*outResponse, ((tDataBufferPriv*)pP24InputNode)->fBufferData, pP24InputNode->fBufferLength);
	}
	catch ( sInt32 error )
	{
		siResult = error;
	}
	
	*outDataList = dataList;
	
	return siResult;
}


//------------------------------------------------------------------------------------
//	UnpackSambaBuffer
//
//	Returns: ds err code
//------------------------------------------------------------------------------------

sInt32 UnpackSambaBuffer( tDataBufferPtr inAuthData, char **outUserName, unsigned char *outC8, unsigned char *outP24 )
{
	sInt32				siResult					= eDSNoErr;
	tDataListPtr		dataList					= NULL;
	unsigned char		*response					= NULL;
	unsigned long		challengeLen				= 0;
	unsigned long		responseLen					= 0;
	unsigned char		challengeBytes[17];
	
	siResult = UnpackSambaBufferFirstThreeItems( inAuthData, &dataList, outUserName, challengeBytes, &challengeLen, &response, &responseLen );
	if ( siResult == eDSNoErr ) {
		if ( dsDataListGetNodeCountPriv(dataList) != 3 ) 
			siResult = eDSInvalidBuffFormat;
		if ( challengeLen != kHashShadowChallengeLength || responseLen != kHashShadowResponseLength )
			siResult = eDSInvalidBuffFormat;
		
		if ( siResult == eDSNoErr ) {
			memmove( outC8, challengeBytes, challengeLen );
			memmove( outP24, response, responseLen );
		}
	}
	
	if ( dataList != NULL ) {
		dsDataListDeallocatePriv( dataList );
		free( dataList );
	}
	
	if ( response != NULL )
		free( response );
	
	return siResult;
}


//------------------------------------------------------------------------------------
//	UnpackNTLMv2Buffer
//
//	Returns: ds err code
//------------------------------------------------------------------------------------

sInt32 UnpackNTLMv2Buffer(
	tDataBufferPtr inAuthData,
	char **outNIName,
	unsigned char *outChal,
	unsigned char **outDigest,
	unsigned long *outDigestLen,
	char **outSambaName,
	char **outDomain)
{
	sInt32				siResult					= eDSNoErr;
	tDataListPtr		dataList					= NULL;
	unsigned long		challengeLen				= 0;
	
	siResult = UnpackSambaBufferFirstThreeItems( inAuthData, &dataList, outNIName, outChal, &challengeLen, outDigest, outDigestLen );
	if ( siResult == eDSNoErr ) {
		if ( dsDataListGetNodeCountPriv(dataList) != 5 || challengeLen != kHashShadowChallengeLength ) 
			siResult = eDSInvalidBuffFormat;
	}
	
	try
	{
		if ( siResult == eDSNoErr )
		{
			// this allocates a copy of the string
			*outSambaName = dsDataListGetNodeStringPriv(dataList, 4);
			if ( *outSambaName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
			if ( strlen(*outSambaName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );
			
			// this allocates a copy of the string
			*outDomain = dsDataListGetNodeStringPriv(dataList, 5);
			if ( *outDomain == nil ) {
				*outDomain = (char *)calloc(1,1);
				if ( *outDomain == nil ) throw( (sInt32)eMemoryError );
			}
		}
	}
	catch ( sInt32 error )
	{
		siResult = error;
	}
	
	if (dataList != NULL) {
		dsDataListDeallocatePriv(dataList);
		free(dataList);
		dataList = NULL;
	}
	
	return siResult;
}


//------------------------------------------------------------------------------------
//	UnpackMSCHAPv2Buffer
//
//	Returns: ds err code
//------------------------------------------------------------------------------------

sInt32 UnpackMSCHAPv2Buffer(
	tDataBufferPtr inAuthData,
	char **outNIName,
	unsigned char *outChal,
	unsigned char **outPeerChal,
	unsigned char **outDigest,
	unsigned long *outDigestLen,
	char **outSambaName)
{
	sInt32				siResult					= eDSNoErr;
	tDataListPtr		dataList					= NULL;
	tDataNodePtr		pP24InputNode				= NULL;
	unsigned long		challengeLen				= 0;
	unsigned long		peerChallengeLen			= 0;
	
	siResult = UnpackSambaBufferFirstThreeItems( inAuthData, &dataList, outNIName, outChal, &challengeLen, outPeerChal, &peerChallengeLen );
	if ( siResult == eDSNoErr ) {
		if ( dsDataListGetNodeCountPriv(dataList) != 5 || challengeLen != 16 || peerChallengeLen != 16 ) 
			siResult = eDSInvalidBuffFormat;
	}
	
	try
	{
		if ( siResult == eDSNoErr )
		{
			siResult = dsDataListGetNodePriv( dataList, 4, &pP24InputNode );
			if ( siResult != eDSNoErr || pP24InputNode == NULL )
				throw( (sInt32)eDSInvalidBuffFormat );
			
			*outDigest = (unsigned char *) malloc( pP24InputNode->fBufferLength );
			if ( *outDigest == NULL )
				throw( (sInt32)eMemoryError );
			*outDigestLen = pP24InputNode->fBufferLength;
			memmove(*outDigest, ((tDataBufferPriv*)pP24InputNode)->fBufferData, pP24InputNode->fBufferLength);
			
			// this allocates a copy of the string
			*outSambaName = dsDataListGetNodeStringPriv( dataList, 5 );
			if ( *outSambaName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
			if ( strlen(*outSambaName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );
		}
	}
	catch ( sInt32 error )
	{
		siResult = error;
	}
	
	if (dataList != NULL) {
		dsDataListDeallocatePriv(dataList);
		free(dataList);
		dataList = NULL;
	}
	
	return siResult;
}


//------------------------------------------------------------------------------------
//	UnpackMSCHAP2SessionKeyBuffer
//
//	Returns: ds err code
//------------------------------------------------------------------------------------

sInt32 UnpackMPPEKeyBuffer( tDataBufferPtr inAuthData, char **outUserName, unsigned char *outP24, int *outKeySize )
{
	sInt32				siResult					= eDSNoErr;
	tDataListPtr		dataList					= NULL;
	tDataNodePtr		pP24Node					= NULL;
	tDataNodePtr		pKeySizeNode				= NULL;
	
	try
	{
		// User name
		dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
		if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
		if ( dsDataListGetNodeCountPriv(dataList) != 3 ) throw( (sInt32)eDSInvalidBuffFormat );
		
		*outUserName = dsDataListGetNodeStringPriv(dataList, 1);
		if ( *outUserName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
		if ( strlen(*outUserName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );
		
		// P24 - NT response
		siResult = dsDataListGetNodePriv(dataList, 2, &pP24Node);
		if ( siResult != eDSNoErr )
			throw( siResult );
		if ( pP24Node == NULL || pP24Node->fBufferLength != 24)
			throw( (sInt32)eDSInvalidBuffFormat );
		memmove(outP24, ((tDataBufferPriv*)pP24Node)->fBufferData, pP24Node->fBufferLength);
		
		// key size (8 or 16)
		siResult = dsDataListGetNodePriv(dataList, 3, &pKeySizeNode);
		if ( siResult != eDSNoErr || pKeySizeNode == NULL || pKeySizeNode->fBufferLength != 1 )
			throw( (sInt32)eDSInvalidBuffFormat );
		
		*outKeySize = ((tDataBufferPriv*)pKeySizeNode)->fBufferData[0];
		if ( *outKeySize != 8 && *outKeySize != 16 )
			throw( (sInt32)eDSInvalidBuffFormat );
	}
	catch ( sInt32 error )
	{
		siResult = error;
	}
	
	if (dataList != NULL) {
		dsDataListDeallocatePriv(dataList);
		free(dataList);
		dataList = NULL;
	}
	
	return siResult;
}


//------------------------------------------------------------------------------------
//	UnpackDigestBuffer
//
//	Returns: ds err code
//------------------------------------------------------------------------------------

sInt32 UnpackDigestBuffer( tDataBufferPtr inAuthData, char **outUserName, digest_context_t *digestContext )
{
	sInt32				siResult					= eDSNoErr;
	tDataListPtr		dataList					= NULL;
	tDataNodePtr		pResponseNode				= NULL;
	unsigned int		itemCount					= 0;
	char				*challenge					= NULL;
	char				*challengePlus				= NULL;
	char				*response					= NULL;
	char				*method						= NULL;
	int					saslResult					= 0;

	try
	{
		siResult = Get2FromBuffer( inAuthData, &dataList, outUserName, &challenge, &itemCount );
		if ( siResult != noErr )
			throw( siResult );

		// this allocates a copy of the string
		method = dsDataListGetNodeStringPriv( dataList, 4 );
		if ( method == nil ) throw( (sInt32)eDSInvalidBuffFormat );
		if ( strlen(method) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );

		challengePlus = (char *) malloc( strlen(challenge) + 10 + strlen(method) );
		strcpy( challengePlus, challenge );
		strcat( challengePlus, ",method=\"" );
		strcat( challengePlus, method );
		strcat( challengePlus, "\"" );
		
		// these are not copies
		siResult = dsDataListGetNodePriv( dataList, 3, &pResponseNode );
		if ( siResult != eDSNoErr ) throw( (sInt32)eDSInvalidBuffFormat );
		if ( pResponseNode == NULL ) throw( (sInt32)eDSInvalidBuffFormat );
		
		response = (char *) calloc( 1, pResponseNode->fBufferLength + 1 );
		memmove( response, ((tDataBufferPriv*)pResponseNode)->fBufferData, ((tDataBufferPriv*)pResponseNode)->fBufferLength );
		
		// parse the digest strings
		saslResult = digest_server_parse( challengePlus, strlen(challengePlus), response, digestContext );
		if ( saslResult != 0 )
			throw( (sInt32)eDSAuthFailed );
	}
	catch ( sInt32 error )
	{
		siResult = error;
	}
	
	if ( challenge != NULL ) {
		free( challenge );
		challenge = NULL;
	}
	if ( challengePlus != NULL ) {
		free( challengePlus );
		challengePlus = NULL;
	}
	if ( method != NULL ) {
		free( method );
		method = NULL;
	}
	if ( response != NULL ) {
		free( response );
		response = NULL;
	}
	if (dataList != NULL) {
		dsDataListDeallocatePriv(dataList);
		free(dataList);
		dataList = NULL;
	}
	
	return siResult;
}


//--------------------------------------------------------------------------------------------------
//	UnpackCramBuffer
//
//	Returns: ds err code
//--------------------------------------------------------------------------------------------------

sInt32 UnpackCramBuffer( tDataBufferPtr inAuthData, char **outUserName, char **outChal, unsigned char **outResponse, unsigned long *outResponseLen )
{
	sInt32				siResult					= eDSNoErr;
	tDataListPtr		dataList					= NULL;
	tDataNodePtr		pResponseNode				= NULL;
	unsigned int		itemCount					= 0;
	
	try
	{
		siResult = Get2FromBuffer( inAuthData, &dataList, outUserName, outChal, &itemCount );
		if ( siResult != noErr )
			throw( siResult );
		if ( itemCount != 3 )
			throw( (sInt32)eDSInvalidBuffFormat );
		
		siResult = dsDataListGetNodePriv( dataList, 3, &pResponseNode );
		if ( siResult != eDSNoErr || pResponseNode == NULL || pResponseNode->fBufferLength < 32 )
			throw( (sInt32)eDSInvalidBuffFormat );
		
		*outResponse = (unsigned char *) malloc( pResponseNode->fBufferLength );
		if ( *outResponse == NULL )
			throw( (sInt32)eMemoryError );
		*outResponseLen = pResponseNode->fBufferLength;
		memmove( *outResponse, ((tDataBufferPriv*)pResponseNode)->fBufferData, pResponseNode->fBufferLength );
	}
	catch ( sInt32 error )
	{
		siResult = error;
		
		if ( *outUserName != NULL ) {
			free( *outUserName );
			*outUserName = NULL;
		}
		if ( *outChal != NULL ) {
			free( *outChal );
			*outChal = NULL;
		}
	}
	
	if ( dataList != NULL ) {
		dsDataListDeallocatePriv( dataList );
		free( dataList );
		dataList = NULL;
	}
	
	return siResult;
}


//--------------------------------------------------------------------------------------------------
//	UnpackAPOPBuffer
//
//	Returns: ds err code
//--------------------------------------------------------------------------------------------------

sInt32 UnpackAPOPBuffer( tDataBufferPtr inAuthData, char **outUserName, char **outChal, char **outResponse )
{
	sInt32				siResult					= eDSNoErr;
	tDataListPtr		dataList					= NULL;
	unsigned int		itemCount					= 0;
	
	try
	{
		siResult = Get2FromBuffer( inAuthData, &dataList, outUserName, outChal, &itemCount );
		if ( siResult != noErr )
			throw( siResult );
		if ( itemCount != 3 )
			throw( (sInt32)eDSInvalidBuffFormat );
		
		// this allocates a copy of the string
		*outResponse = dsDataListGetNodeStringPriv( dataList, 3 );
		if ( *outResponse == NULL )
			throw( (sInt32)eDSInvalidBuffFormat );
	}
	catch ( sInt32 error )
	{
		siResult = error;
		
		if ( *outUserName != NULL ) {
			free( *outUserName );
			*outUserName = NULL;
		}
		if ( *outChal != NULL ) {
			free( *outChal );
			*outChal = NULL;
		}
	}
	
	if ( dataList != NULL ) {
		dsDataListDeallocatePriv( dataList );
		free( dataList );
		dataList = NULL;
	}
	
	return siResult;
}


//--------------------------------------------------------------------------------------------------
//	RepackBufferForPWServer
//
//	Replace the user name with the uesr id.
//--------------------------------------------------------------------------------------------------

sInt32 RepackBufferForPWServer ( tDataBufferPtr inBuff, const char *inUserID, unsigned long inUserIDNodeNum, tDataBufferPtr *outBuff )
{
	sInt32 result = eDSNoErr;
    tDataListPtr dataList = NULL;
    tDataNodePtr dataNode = NULL;
	unsigned long index, nodeCount;
	unsigned long uidLen;
                
    if ( !inBuff || !inUserID || !outBuff )
        return eDSAuthParameterError;
    
    try
    {	
        uidLen = strlen(inUserID);
        *outBuff = ::dsDataBufferAllocatePriv( inBuff->fBufferLength + uidLen + 1 );
        if ( *outBuff == nil ) throw( (sInt32)eMemoryError );
        
        (*outBuff)->fBufferLength = 0;
        
        dataList = dsAuthBufferGetDataListAllocPriv(inBuff);
        if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
        
        nodeCount = dsDataListGetNodeCountPriv(dataList);
        if ( nodeCount < 1 ) throw( (sInt32)eDSInvalidBuffFormat );
        
        for ( index = 1; index <= nodeCount; index++ )
        {
            if ( index == inUserIDNodeNum )
            {
                // write 4 byte length
                memcpy( (*outBuff)->fBufferData + (*outBuff)->fBufferLength, &uidLen, sizeof(unsigned long) );
                (*outBuff)->fBufferLength += sizeof(unsigned long);
                
                // write uid
                memcpy( (*outBuff)->fBufferData + (*outBuff)->fBufferLength, inUserID, uidLen );
                (*outBuff)->fBufferLength += uidLen;
            }
            else
            {
                // get a node
                result = dsDataListGetNodeAllocPriv(dataList, index, &dataNode);
                if ( result != eDSNoErr ) throw( (sInt32)eDSInvalidBuffFormat );
            
                // copy it
                memcpy((*outBuff)->fBufferData + (*outBuff)->fBufferLength, &dataNode->fBufferLength, sizeof(unsigned long));
                (*outBuff)->fBufferLength += sizeof(unsigned long);
                
                memcpy( (*outBuff)->fBufferData + (*outBuff)->fBufferLength, dataNode->fBufferData, dataNode->fBufferLength );
                (*outBuff)->fBufferLength += dataNode->fBufferLength;
                
                // clean up
                dsDataBufferDeallocatePriv(dataNode);
            }
            
        }
        
        (void)dsDataListDeallocatePriv(dataList);
        free(dataList);
    }
    
    catch( sInt32 error )
    {
        result = error;
    }
    
    return result;
} // RepackBufferForPWServer


// ---------------------------------------------------------------------------
//	* GetUserNameFromAuthBuffer
//    retrieve the username from a standard auth buffer
//    buffer format should be 4 byte length followed by username, then optional
//    additional data after. Buffer length must be at least 5 (length + 1 character name)
// ---------------------------------------------------------------------------

sInt32 GetUserNameFromAuthBuffer ( tDataBufferPtr inAuthData, unsigned long inUserNameIndex, 
											  char  **outUserName )
{
	tDataListPtr dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
	if (dataList != NULL)
	{
		*outUserName = dsDataListGetNodeStringPriv(dataList, inUserNameIndex);
		// this allocates a copy of the string
		
		dsDataListDeallocatePriv(dataList);
		free(dataList);
		dataList = NULL;
		return eDSNoErr;
	}
	return eDSInvalidBuffFormat;
}

