/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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

#ifndef __PSUtilitiesDefs_h__
#define __PSUtilitiesDefs_h__	1

#include <sys/time.h>
#include <openssl/cast.h>
#include <PasswordServer/CPSPluginDefines.h>

#define kSASLPluginDisabledPath		"/usr/lib/sasl2/disabled"

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
	kReplicaIPSet_CurrentForLDAP,
	kReplicaIPSet_LocallyHosted,
	kReplicaIPSet_InSubnet,
	kReplicaIPSet_PrivateNet,
	kReplicaIPSet_Wide
} ReplicaIPLevel;

typedef enum PWServerErrorType {
	kPolicyError,
	kSASLError,
	kConnectionError,
	kGeneralError
} PWServerErrorType;

typedef struct PWServerError {
    int err;
    PWServerErrorType type;
} PWServerError;

typedef struct SASLMechInfo {
	char name[SASL_MECHNAMEMAX + 1];
	char filename[256];
	bool requiresPlain;
} SASLMechInfo;

typedef enum PWKnownMech {
	kKnownMechLM,
	kKnownMechNT,
	kKnownMechNTLMv2,
	kKnownMechMS_CHAPv2,
	kKnownMechCRAM_MD5,
	kKnownMechDIGEST_MD5,						// DIGEST-MD5 used by LoginWindow (native)
	kKnownMechWEBDAV_DIGEST,					// DIGEST-MD5 used by Web
	kKnownMechAPOP,
	kKnownMechPPS,
	kKnownMechOther,
	kKnownMechCount
} PWKnownMech;

typedef struct PWAuthStats {
	uint16_t structVersion;						// 0=the following fields, 1+ indicates a future struct
	uint16_t connCount;							// number of established connections to the password server
	time_t timeStampStart;						// the second when the sample was started
	time_t timeStampEnd;						// the second when the sample was recorded
	uint8_t replicatorIncomingPct;				// the percentage of capacity used by the incoming replicator
	uint8_t replicatorOutgoingPct;				// the percentage of capacity used by the outgoing replicator
	uint16_t authGoodCount[kKnownMechCount];
	uint16_t authBadCount[kKnownMechCount];
} PWAuthStats;

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

int ConnectToServer( sPSContextData *inContext );
Boolean Connected( sPSContextData *inContext );
int IdentifyReachableReplica( CFMutableArrayRef inServerArray, const char *inHexHash, sPSServerEntry *outReplica, int *outSock );

int IdentifyReachableReplicaByIP(
	sPSServerEntry *entrylist,
	CFIndex servCount,
	const char *inHexHash,
	sPSServerEntry *outReplica,
	int *outSock );

int ConvertCFArrayToServerArray( CFArrayRef inCFArray, sPSServerEntry **outServerArray, CFIndex *outCount );
int GetBigNumber( sPSContextData *inContext, char **outBigNumStr );

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
int GetPasswordServerList( CFMutableArrayRef *outServerList, int inConfigSearchOptions );
int GetPasswordServerListForKeyHash( CFMutableArrayRef *outServerList, int inConfigSearchOptions, const char *inKeyHash );
int GetServerListFromLocalCache( CFMutableArrayRef inOutServerList );
int GetServerListFromFile( CFMutableArrayRef inOutServerList );
int GetServerListFromFileForKeyHash( CFMutableArrayRef inOutServerList, const char *inKeyHash );
int GetServerFromDict( CFDictionaryRef serverDict, int inIPIndex, sPSServerEntry *outServerEntry );
int SaveLocalReplicaCache( CFMutableArrayRef inReplicaArray, sPSServerEntry *inLastContactEntry );
void AppendToArrayIfUnique( CFMutableArrayRef inArray, sPSServerEntry *inServerEntry );
ReplicaIPLevel ReplicaPriority( sPSServerEntry *inReplica, in_addr_t *iplist );
bool ReplicaInIPSet( sPSServerEntry *inReplica, ReplicaIPLevel inLevel );
int pwsf_LocalIPList( in_addr_t **outIPList );
int getconn_async(const char *host, const char *port, struct timeval *inOpenTimeout, float *outConnectTime, int *inOutSocket);
int testconn_udp(const char *host, const char *port, int *outSocket);
int pwsf_pingpws( const char *host, const struct timespec *timeout );
pid_t pwsf_ProcessIsRunning( const char *inProcName );
bool pwsf_GetSASLMechInfo( const char *inMechName, char **outPluginFileName, bool *outRequiresPlainTextOnDisk );
int pwsf_mkdir_p( const char *path, mode_t mode );
int EnumerateDirectory( const char *inDirPath, const char *inStartsWith, CFMutableArrayRef *outFileArray );
int pwsf_EnumerateDirectory( const char *inDirPath, const char *inStartsWith, CFMutableArrayRef *outFileArray );
int pwsf_LaunchTask(const char *path, char *const argv[]);
int pwsf_LaunchTaskWithIO(
	const char *path,
	char *const argv[],
	const char* inputBuf,
	char* outputBuf,
	size_t outputBufSize,
	bool *outExitedBeforeInput);
int pwsf_LaunchTaskWithIO2(
	const char *path,
	char *const argv[],
	const char* inputBuf,
	char* outputBuf,
	size_t outputBufSize,
	char* errBuf,
	size_t errBufSize);

#ifdef __cplusplus
};
#endif

#endif	// __PSUtilitiesDefs_h__


