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

#ifndef __CNIUTILITIES__
#define __CNIUTILITIES__

#include "CNiPlugIn.h"
#include "chap_ms.h"

//auth type tags
#define kNIHashNameListPrefix			"HASHLIST:"
#define kNIHashNameNT					"SMB-NT"
#define kNIHashNameLM					"SMB-LAN-MANAGER"
#define kNIHashNameCRAM_MD5				"CRAM-MD5"
#define kNIHashNameSHA1					"SALTED-SHA1"
#define kNIHashNameRecoverable			"RECOVERABLE"
#define kNIHashNameSecure				"SECURE"


sInt32 NIPasswordOkForPolicies( const char *inSpaceDelimitedPolicies, PWGlobalAccessFeatures *inGAccess, const char *inUsername, const char *inPassword );
sInt32 NITestPolicies( const char *inSpaceDelimitedPolicies, PWGlobalAccessFeatures *inGAccess, sHashState *inOutHashState, struct timespec *inModDateOfPassword, const char *inHashPath );
bool NIHashesEqual( const unsigned char *inUserHashes, const unsigned char *inGeneratedHashes );
sInt32 NIGetStateFilePath( const char *inHashPath, char **outStateFilePath );

sInt32 GetShadowHashGlobalPolicies( sNIContextData *inContext, PWGlobalAccessFeatures *inOutGAccess );
sInt32 SetShadowHashGlobalPolicies( sNIContextData *inContext, PWGlobalAccessFeatures *inGAccess );
int ReadHashStateFile( const char *inFilePath, sHashState *inOutHashState );
int WriteHashStateFile( const char *inFilePath, sHashState *inHashState );

void GenerateShadowHashes	(	const char *inPassword,
								long inPasswordLen,
								int	inAdditionalHashList,
								const unsigned char *inSHA1Salt,
								unsigned char *outHashes,
								unsigned long *outHashTotalLength );
							
sInt32 UnobfuscateRecoverablePassword( unsigned char *inData, unsigned char **outPassword, unsigned long *outPasswordLength );
sInt32 GetHashSecurityLevelConfig( void *inDomain, unsigned int *outHashList );
sInt32 GetHashSecurityLevelForUser( const char *inHashList, unsigned int *outHashList );
bool GetHashSecurityBitsForString( const char *inHashType, unsigned int *inOutHashList );

sInt32 MSCHAPv2				(	const unsigned char *inC16,
								const unsigned char *inPeerC16,
								const unsigned char *inNTLMDigest,
								const char *inSambaName,
								const unsigned char *inOurHash,
								char *outMSCHAP2Response );
	
sInt32 CRAM_MD5				(	const unsigned char *inHash,
								const char *inChallenge,
								const unsigned char *inResponse );

sInt32 Verify_APOP			(	const char *userstr,
								const unsigned char *inPassword,
								unsigned long inPasswordLen,
								const char *challenge,
								const char *response );

sInt32 IsValidRecordName	(	const char	*inRecName,
								const char	*inRecType,
								void		*inDomain,
								ni_id		&outDirID );

char *BuildRecordNamePath( const char *inRecName, const char *inRecType );
bool UserIsAdmin( const char *inUserName, sNIContextData *inContext );
bool UserIsAdminInDomain( const char *inUserName, void *inDomain );
char *GetGUIDForRecord( sNIContextData *inContext, ni_id *inNIDirID );

sInt32 ParseLocalCacheUserData	(	const char *inAuthData,
									char **outNodeName,
									char **outRecordName,
									char **outGUID );
									
void* RetrieveNIDomain			(	sNIContextData* inContext );
void* RetrieveSharedNIDomain	(	sNIContextData* inContext );
char* BuildDomainPathFromName	(	char* inDomainName );
char* NormalizeNIString			(	char* inUTF8String, char* inNIAttributeType );
void NormalizeNINameList		(	ni_namelist* inNameList, char* inNIAttributeType );

#endif

