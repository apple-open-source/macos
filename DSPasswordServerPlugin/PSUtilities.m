/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#import "PSUtilities.h"
#import "AuthDBFileDefs.h"

// ---------------------------------------------------------------------------
//	* pwsf_GetServerListFromConfig
// ---------------------------------------------------------------------------

int pwsf_GetServerListFromConfig( CFMutableArrayRef *outServerList, ReplicaFile *inReplicaData )
{
	int status = 0;
	CFMutableArrayRef serverArray;
	
	if ( outServerList == NULL || inReplicaData == NULL )
		return kAuthFail;
	
	*outServerList = NULL;
	
	serverArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	if ( serverArray == NULL )
		return kAuthFail;
	
	status = pwsf_GetServerListFromXML( inReplicaData, serverArray );
	
	if ( CFArrayGetCount(serverArray) > 0 )
	{
		*outServerList = serverArray;
		status = 0;
	}
	else
	{
		CFRelease( serverArray );
	}
	
	return status;
}


// ---------------------------------------------------------------------------
//	* pwsf_GetServerListFromXML
// ---------------------------------------------------------------------------

int
pwsf_GetServerListFromXML( ReplicaFile *inReplicaFile, CFMutableArrayRef inOutServerList )
{
	CFDictionaryRef serverDict;
	CFStringRef serverID;
	CFStringRef ldapServerString = NULL;
	unsigned long repIndex;
	unsigned long repCount;
	int ipIndex = 0;
	int status = 0;
	sPSServerEntry serverEntry;
	char ldapServerStr[256] = {0};
	
	if ( inOutServerList == NULL )
		return kAuthFail;
	
	bzero( &serverEntry, sizeof(sPSServerEntry) );
	
	ldapServerString = [inReplicaFile currentServerForLDAP];
	if ( ldapServerString != NULL )
	{
		CFStringGetCString( ldapServerString, ldapServerStr, sizeof(ldapServerStr), kCFStringEncodingUTF8 );
		CFRelease( ldapServerString );
	}
	
	serverID = [inReplicaFile getUniqueID];
	if ( serverID != NULL ) {
		CFStringGetCString( serverID, serverEntry.id, sizeof(serverEntry.id), kCFStringEncodingUTF8 );
		CFRelease( serverID );
	}
	
	if ( serverEntry.id[0] == '\0' )
		return kAuthFail;
	
	serverDict = [inReplicaFile getParent];
	if ( serverDict != NULL )
	{
		for ( ipIndex = 0, status = 0; status == 0; ipIndex++ )
		{
			status = GetServerFromDict( serverDict, ipIndex, &serverEntry );
			if ( status == 0 )
			{
				if ( strcmp(serverEntry.ip, ldapServerStr) == 0 )
					serverEntry.currentServerForLDAP = YES;
					
				AppendToArrayIfUnique( inOutServerList, &serverEntry );
			}
		}
	}
	
	repCount = [inReplicaFile replicaCount];
	for ( repIndex = 0; repIndex < repCount; repIndex++ )
	{
		serverDict = [inReplicaFile getReplica:repIndex];
		if ( serverDict != NULL )
		{
			for ( ipIndex = 0, status = 0; status == 0; ipIndex++ )
			{
				status = GetServerFromDict( serverDict, ipIndex, &serverEntry );
				if ( status == 0 )
				{
					if ( strcmp(serverEntry.ip, ldapServerStr) == 0 )
						serverEntry.currentServerForLDAP = YES;
						
					AppendToArrayIfUnique( inOutServerList, &serverEntry );
				}
			}
		}
	}
	
	status = ( (CFArrayGetCount(inOutServerList) > 0) ? 0 : kAuthFail );
	return status;
}


// ---------------------------------------------------------------------------
//	* pwsf_ReadSyncDataFromServerWithCASTKey
// Format is:
//	Header: 40 bytes fixed size, padded with spaces
//	+MORE type byte-count compressed-record-length
//
//	Body: RC5 data (binary)
// ---------------------------------------------------------------------------

PWServerError pwsf_ReadSyncDataFromServerWithCASTKey(
	sPSContextData *inContext,
	unsigned char **dataBuffer,
	int *dataTypePtr,
	unsigned int *dataSizePtr,
	unsigned int *dataLenPtr )
{
    PWServerError result = { 0, kConnectionError };
	PWServerError connectionErr = { -1, kConnectionError };
	char castDescBuffer[kMoreHeaderReserveSize] = {0};
	char plainDescBuffer[kMoreHeaderReserveSize] = {0};
	unsigned char *buffer = NULL;
	unsigned long byteCount = 0;
	char *tptr = NULL;
	int tryIndex = 0;
	
	*dataBuffer = NULL;
	
	// PEEK the header data //
	// we don't know the length yet, so don't wait //
	// readFromServerGetData() reserves a byte for a zero-terminator, so add 1 to the length //
	// make sure we get the minimum amount of data for a valid response //
	while ( byteCount < sizeof("+OK\r\n") - 1 )
	{
		result = readFromServerGetData( inContext->fd, castDescBuffer, sizeof(castDescBuffer) + 1, &byteCount );
		if ( result.err != 0 || byteCount == 0 )
			return result;
		
		if ( byteCount < sizeof("+OK\r\n") - 1 )
			usleep( 10 );
	}
	
	// if the incoming data is a replication record, keep peeking until we get the fixed 40 bytes
	for (	tryIndex = 1;
			tryIndex < 3 &&
			byteCount > sizeof(kPWMoreDataPrefix) - 1 &&
			strncmp(castDescBuffer, kPWMoreDataPrefix, sizeof(kPWMoreDataPrefix) - 1) == 0 &&
			byteCount < kMoreHeaderReserveSize;
			tryIndex++ )
	{
		result = readFromServerGetData( inContext->fd, castDescBuffer, sizeof(castDescBuffer) + 1, &byteCount );
		if ( result.err != 0 || byteCount == 0 )
			return result;
		
		if ( byteCount < kMoreHeaderReserveSize )
			usleep( 10 );
	}
	
	// protocol enforcement
	if ( byteCount > sizeof(kPWMoreDataPrefix) - 1 &&
		 strncmp(castDescBuffer, kPWMoreDataPrefix, sizeof(kPWMoreDataPrefix) - 1) == 0 &&
		 byteCount < kMoreHeaderReserveSize )
	{
		return connectionErr;
	}
	
	// Consume the bytes off the TCP stack //
	recvfrom( inContext->fd, plainDescBuffer, byteCount, MSG_WAITALL, NULL, NULL );
	
	// if (byteCount % CAST_BLOCK) something_is_wrong();
	if ( inContext->castKeySet && !inContext->isUNIXDomainSocket )
	{
		CAST_cbc_encrypt(
			(unsigned char *)castDescBuffer,
			(unsigned char *)plainDescBuffer,
			byteCount,
			&inContext->castKey,
			inContext->castReceiveIV,
			CAST_DECRYPT );
	}
	
	// If this is a replication record, read the rest //
	if ( byteCount > sizeof(kPWMoreDataPrefix) - 1 &&
		 strncmp(plainDescBuffer, kPWMoreDataPrefix, sizeof(kPWMoreDataPrefix) - 1) == 0 )
	{
		// protocol enforcement
		if ( byteCount != kMoreHeaderReserveSize )
			return connectionErr;
		
		// parse the header
		sscanf( plainDescBuffer + 6, "%d %u %u", dataTypePtr, dataSizePtr, dataLenPtr );
		if ( !kDBTypeIsValid(*dataTypePtr) || *dataLenPtr > *dataSizePtr )
			return connectionErr;
		
		// make a read and decrypt buffer
		buffer = (unsigned char *) malloc( (*dataSizePtr) * 2 );
		if ( buffer == NULL ) {
			connectionErr.type = kGeneralError;
			return connectionErr;
		}
		
		// read the data blob
		if ( inContext->castKeySet && !inContext->isUNIXDomainSocket )
		{
			byteCount = recvfrom( inContext->fd, buffer + *dataSizePtr, *dataSizePtr, MSG_WAITALL, NULL, NULL );
			CAST_cbc_encrypt( buffer + *dataSizePtr, buffer, byteCount, &inContext->castKey, inContext->castReceiveIV, CAST_DECRYPT );
		}
		else
		{
			byteCount = recvfrom( inContext->fd, buffer, *dataSizePtr, MSG_WAITALL, NULL, NULL );
		}
		
		*dataBuffer = buffer;
	}
	else
	{
		tptr = strstr( plainDescBuffer, "\r\n" );
		if ( tptr != NULL ) {
			*tptr = '\0';
			result = readFromServerGetErrorCode( plainDescBuffer );
		}
	}
    
    return result;
}


