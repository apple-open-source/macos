/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * @header CPSUtilities
 */

#ifndef __CPSUtilities_h__
#define __CPSUtilities_h__	1

#include <sys/time.h>
#include <openssl/cast.h>
#include "CPSPluginDefines.h"

// CPSUtilities error codes
enum {
	kCPSUtilWaitingForConnection	= 1,
	kCPSUtilOK						= 0,
	kCPSUtilFail					= -1,
	kCPSUtilServiceUnavailable		= -2,
	kCPSUtilParameterError			= -3,
	kCPSUtilMemoryError				= -4
};

enum {
	kPWSearchLocalFile				= 0x0001,				// list of replicas from the local cache
	kPWSearchReplicaFile			= 0x0002,				// list of replicas from the password server's replication list
	kPWSearchDirectory				= 0x0004,				// list of replicas from the directory system (NetInfo or LDAP)
	kPWSearchRegisteredServices		= 0x0008				// search for passwordservers with Rendezvous
};


typedef enum {
	kReplicaIPSet_LocallyHosted,
	kReplicaIPSet_InSubnet,
	kReplicaIPSet_PrivateNet,
	kReplicaIPSet_Wide
} ReplicaIPLevel;

typedef enum PWServerErrorType {
	kPolicyError,
	kSASLError,
	kConnectionError
} PWServerErrorType;

typedef struct PWServerError {
    int err;
    PWServerErrorType type;
} PWServerError;


#ifdef __cplusplus
	extern "C" {
#endif

void psfwSetUSR1Debug( bool on );
void writeToServer( FILE *out, char *buf );
PWServerError readFromServer( int fd, char *buf, unsigned long bufLen );
void writeToServerWithCASTKey( FILE *out, char *buf, CAST_KEY *inKey, unsigned char *inOutIV );
PWServerError readFromServerWithCASTKey( int fd, char *buf, unsigned long bufLen, CAST_KEY *inKey, unsigned char *inOutIV );
PWServerError readFromServerGetData( int fd, char *buf, unsigned long bufLen, unsigned long *outByteCount );
PWServerError readFromServerGetLine( int fd, char *buf, unsigned long bufLen, bool inCanReadMore, unsigned long *inOutByteCount );
PWServerError readFromServerGetErrorCode( char *buf );

void ConvertHexToBinary( const char *inHexStr, unsigned char *outData, unsigned long *outLen );
int ConvertBinaryTo64( const char *inData, unsigned long inLen, char *outHexStr );
int Convert64ToBinary( const char *inHexStr, char *outData, unsigned long maxLen, unsigned long *outLen );

long ConnectToServer( sPSContextData *inContext );
Boolean Connected( sPSContextData *inContext );
long IdentifyReachableReplica( CFMutableArrayRef inServerArray, const char *inHexHash, sPSServerEntry *outReplica, int *outSock );
long ConvertCFArrayToServerArray( CFArrayRef inCFArray, sPSServerEntry **outServerArray, CFIndex *outCount );
void GetTrialTime( long inReplicaCount, struct timeval *outTrialTime );
long GetBigNumber( sPSContextData *inContext, char **outBigNumStr );

PWServerError SendFlush( sPSContextData *inContext,
	const char *inCommandStr,
	const char *inArg1Str,
	const char *inArg2Str );

PWServerError SendFlushRead( sPSContextData *inContext,
	const char *inCommandStr,
	const char *inArg1Str,
	const char *inArg2Str,
	char *inOutBuf,
	unsigned long inBufLen );

char *SendFlushReadAssembleCommand(
	const char *inCommandStr,
	const char *inArg1Str,
	const char *inArg2Str );
		
void StripRSAKey( char *inOutUserID );
long GetPasswordServerList( CFMutableArrayRef *outServerList, int inConfigSearchOptions );
long GetServerListFromLocalCache( CFMutableArrayRef inOutServerList );
long GetServerListFromFile( CFMutableArrayRef inOutServerList );
long GetServerListFromConfig( CFMutableArrayRef *outServerList, CReplicaFile *inReplicaData );
long GetServerListFromXML( CReplicaFile *inReplicaFile, CFMutableArrayRef inOutServerList );
long GetServerFromDict( CFDictionaryRef serverDict, sPSServerEntry *outServerEntry );
int SaveLocalReplicaCache( CFMutableArrayRef inReplicaArray, sPSServerEntry *inLastContactEntry );
void AppendToArrayIfUnique( CFMutableArrayRef inArray, sPSServerEntry *inServerEntry );
bool ReplicaInIPSet( sPSServerEntry *inReplica, ReplicaIPLevel inLevel );
long pwsf_LocalIPList( unsigned long **outIPList );
long getconn_async(const char *host, const char *port, struct timeval *inOpenTimeout, float *outConnectTime, int *inOutSocket);
long testconn_udp(const char *host, const char *port, int *outSocket);

#ifdef __cplusplus
	};
#endif

#endif	// __CPSUtilities_h__


