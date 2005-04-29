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

#include <openssl/md5.h>
#include <openssl/sha.h>

#include <PasswordServer/AuthFile.h>
#include <PasswordServer/CAuthFileBase.h>
#include <PasswordServer/CPolicyGlobalXML.h>
#include <PasswordServer/CPolicyXML.h>
#include <PasswordServer/SASLCode.h>
#include "CNiMaps.h"
#include "CNiUtilities.h"
#include "CNetInfoPlugin.h"
#include "CLog.h"
#include "CSharedData.h"
#include "SMBAuth.h"
#include "NiLib2.h"

#define HMAC_MD5_SIZE 16

extern DSMutexSemaphore *gNetInfoMutex;
extern bool gServerOS;

extern "C" {
extern void CvtHex(HASH Bin, HASHHEX Hex);
};

static char sZeros[kHashRecoverableLength] = {0};

#pragma mark -
#pragma mark Policy Utilities
#pragma mark -

//------------------------------------------------------------------------------------
//	NIPasswordOkForPolicies
//
//	Returns: ds err code
//------------------------------------------------------------------------------------

sInt32 NIPasswordOkForPolicies( const char *inSpaceDelimitedPolicies, PWGlobalAccessFeatures *inGAccess, const char *inUsername, const char *inPassword )
{
	PWAccessFeatures access;
	sInt32 siResult = eDSNoErr;
	int result;
	
	if ( inPassword == NULL )
	{
		DBGLOG( kLogPlugin,  "PasswordOkForPolicies: no password" );
		return eDSNoErr;
	}
	
	if ( inGAccess->noModifyPasswordforSelf )
		return eDSPermissionError;
	
	// setup user policy default
	GetDefaultUserPolicies( &access );
	
	// apply user policies
	if ( inSpaceDelimitedPolicies != NULL )
		StringToPWAccessFeatures( inSpaceDelimitedPolicies, &access );
	
	try
	{
		if ( !access.canModifyPasswordforSelf )
			throw( (sInt32)eDSAuthFailed );
		
		result = pwsf_RequiredCharacterStatus( &access, inGAccess, inUsername, inPassword );
		switch( result )
		{
			case kAuthOK:						siResult = eDSNoErr;							break;
			case kAuthUserDisabled:				siResult = eDSAuthAccountDisabled;				break;
			case kAuthPasswordExpired:			siResult = eDSAuthPasswordExpired;				break;
			case kAuthPasswordNeedsChange:		siResult = eDSAuthPasswordQualityCheckFailed;   break;
			case kAuthPasswordTooShort:			siResult = eDSAuthPasswordTooShort;				break;
			case kAuthPasswordTooLong:			siResult = eDSAuthPasswordTooLong;				break;
			case kAuthPasswordNeedsAlpha:		siResult = eDSAuthPasswordNeedsLetter;			break;
			case kAuthPasswordNeedsDecimal:		siResult = eDSAuthPasswordNeedsDigit;			break;
			
			default:
				siResult = eDSAuthFailed;
				break;
		}
		/*
		int usingHistory:1;						// TRUE == user has a password history file
		int usingExpirationDate:1;				// TRUE == look at expirationDateGMT
		int usingHardExpirationDate:1;			// TRUE == look at hardExpirationDateGMT
		unsigned int historyCount:4;
		
		BSDTimeStructCopy expirationDateGMT;	// if exceeded, user is required to change the password at next login
		BSDTimeStructCopy hardExpireDateGMT;	// if exceeded, user is disabled
			
		*/
	}
	catch(sInt32 catchErr)
	{
		siResult = catchErr;
	}
	
    return siResult;	
}


//------------------------------------------------------------------------------------
//	NITestPolicies
//
//	Returns: ds err code
//------------------------------------------------------------------------------------

sInt32 NITestPolicies( const char *inSpaceDelimitedPolicies, PWGlobalAccessFeatures *inGAccess, sHashState *inOutHashState, struct timespec *inModDateOfPassword, const char *inHashPath )
{
	PWAccessFeatures access;
	int result;
	sInt32 siResult = eDSNoErr;
	
	if ( inHashPath == NULL )
	{
		DBGLOG( kLogPlugin,  "TestPolicies: no path" );
		return eDSNoErr;
	}
	
	GetDefaultUserPolicies( &access );
	
	if ( inSpaceDelimitedPolicies != NULL )
		StringToPWAccessFeatures( inSpaceDelimitedPolicies, &access );
		
	try
	{
		result = pwsf_TestDisabledStatus( &access, inGAccess, &(inOutHashState->creationDate), &(inOutHashState->lastLoginDate), &(inOutHashState->failedLoginAttempts) );
		if ( result == kAuthUserDisabled )
		{
			inOutHashState->disabled = 1;
			throw( (sInt32)eDSAuthAccountDisabled );
		}
		
		if ( inOutHashState->newPasswordRequired )
			throw( (sInt32)eDSAuthNewPasswordRequired );
		
		gmtime_r( (const time_t *)&inModDateOfPassword->tv_sec, &(inOutHashState->modDateOfPassword) );
		result = pwsf_ChangePasswordStatus( &access, inGAccess, &(inOutHashState->modDateOfPassword) );
		switch( result )
		{
			case kAuthPasswordNeedsChange:
				siResult = eDSAuthNewPasswordRequired;
				break;
				
			case kAuthPasswordExpired:
				siResult = eDSAuthPasswordExpired;
				break;
			
			default:
				break;
		}
	}
	catch(sInt32 catchErr)
	{
		siResult = catchErr;
	}
			
    return siResult;
}


//--------------------------------------------------------------------------------------------------
// * GetShadowHashGlobalPolicies ()
//--------------------------------------------------------------------------------------------------

sInt32 GetShadowHashGlobalPolicies( sNIContextData *inContext, PWGlobalAccessFeatures *inOutGAccess )
{
	sInt32				error					= eDSInvalidSession;
	ni_status			niResult				= NI_INVALIDDOMAIN;
	char				*nativeRecType			= NULL;
	char				*nativeAttrType			= NULL;
	char				*policyStr				= NULL;
    ni_id				niDirID;
	ni_namelist			niValues;
	
	if ( inContext == NULL || inOutGAccess == NULL )
		return eParameterError;
	
	bzero( inOutGAccess, sizeof(PWGlobalAccessFeatures) );
	
	nativeRecType = MapRecToNetInfoType( kDSStdRecordTypeConfig );
	if ( nativeRecType == NULL )
		return eDSInvalidRecordType;
	
	try
	{
		nativeAttrType = MapAttrToNetInfoType( kDS1AttrPasswordPolicyOptions );
		if ( nativeAttrType == NULL )
			throw( (sInt32)eDSInvalidAttributeType );
		
		gNetInfoMutex->Wait();
		void *aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
			error = IsValidRecordName( kShadowHashRecordName, nativeRecType, aNIDomain, niDirID );
			if ( error == eDSNoErr )
			{
				//lookup global policies attribute
				niResult = ni_lookupprop( aNIDomain, &niDirID, nativeAttrType, &niValues );
			}
		}
		gNetInfoMutex->Signal();
		if ( error != eDSNoErr ) 
			throw( error );
			
		if ( niResult != NI_OK ) throw( (sInt32)eDSAuthFailed );
		
		if ( niValues.ni_namelist_len >= 1 )
		{
			if ( ConvertGlobalXMLPolicyToSpaceDelimited( niValues.ni_namelist_val[0], &policyStr ) == 0 )
			{
				StringToPWGlobalAccessFeatures( policyStr, inOutGAccess );
			}
		}
		
		gNetInfoMutex->Wait();
		if (niValues.ni_namelist_len > 0)
		{
			ni_namelist_free( &niValues );
		}
		gNetInfoMutex->Signal();
	}
	catch( sInt32 catchErr )
	{
		error = catchErr;
	}
	
	if ( nativeRecType != NULL )
		delete nativeRecType;
	if ( nativeAttrType != NULL )
		delete nativeAttrType;
	if ( policyStr != NULL )
		free( policyStr );
	
	return error;
}


//--------------------------------------------------------------------------------------------------
// * SetShadowHashGlobalPolicies
//--------------------------------------------------------------------------------------------------

sInt32 SetShadowHashGlobalPolicies( sNIContextData *inContext, PWGlobalAccessFeatures *inGAccess )
{
	sInt32				siResult			= eDSAuthFailed;
	bool				bFreePropList		= false;
	ni_proplist			niPropList;
	ni_id				niDirID;
	ni_index			niWhere				= 0;
	ni_namelist			niValue;
	char				*nativeRecType		= NULL;
	char				*nativeAttrType		= NULL;
	void				*aNIDomain			= NULL;
	
	gNetInfoMutex->Wait();

	try
	{
		if ( inContext == nil || inGAccess == nil )
			throw( (sInt32)eDSAuthFailed );
		
		if ( (inContext->fEffectiveUID != 0) && (! UserIsAdmin(inContext->fAuthenticatedUserName, inContext)) )
			throw( (sInt32)eDSPermissionError );
		
		nativeRecType = MapRecToNetInfoType( kDSStdRecordTypeConfig );
		if ( nativeRecType == NULL )
			throw( (sInt32)eDSInvalidRecordType );
		
		nativeAttrType = MapAttrToNetInfoType( kDS1AttrPasswordPolicyOptions );
		if ( nativeAttrType == NULL )
			throw( (sInt32)eDSInvalidAttributeType );

		aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
			siResult = IsValidRecordName( kShadowHashRecordName, nativeRecType, aNIDomain, niDirID );
			if ( siResult != eDSNoErr )
			{
				NiLib2::Create( aNIDomain, "/config/"kShadowHashRecordName );
				siResult = IsValidRecordName( kShadowHashRecordName, nativeRecType, aNIDomain, niDirID );
			}
			if ( siResult != eDSNoErr )
				throw((sInt32)eDSAuthFailed);
			siResult = ::ni_read( aNIDomain, &niDirID, &niPropList );
			if ( siResult != eDSNoErr ) throw((sInt32)eDSAuthFailed);
			if ( niPropList.ni_proplist_val != NULL )
				bFreePropList = true;
			
			niWhere = ::ni_proplist_match( niPropList, nativeAttrType, nil );
			if (niWhere != NI_INDEX_NULL)
			{		
				niValue = niPropList.ni_proplist_val[niWhere].nip_val;
				
				if ( ( inContext->fEffectiveUID != 0 )
				 && ( (inContext->fAuthenticatedUserName == NULL) 
					  || (strcmp(inContext->fAuthenticatedUserName,"root") != 0) ) )
				{
					siResult = NiLib2::ValidateDir( inContext->fAuthenticatedUserName, &niPropList );
					if ( siResult != NI_OK )
						siResult = NiLib2::ValidateName( "root", &niPropList, niWhere );
				}
			}
			
			if ( siResult != eDSNoErr )
				siResult = MapNetInfoErrors( siResult );
			
			if (siResult == eDSNoErr)
			{
				char *xmlDataStr;
				char policyStr[2048];
				
				PWGlobalAccessFeaturesToString( inGAccess, policyStr );
				if ( ConvertGlobalSpaceDelimitedPolicyToXML( policyStr, &xmlDataStr ) == 0 )
				{
					NiLib2::DestroyDirVal( aNIDomain, &niDirID, (char*)nativeAttrType, niValue );
					siResult = NiLib2::InsertDirVal( aNIDomain, &niDirID, (char*)nativeAttrType, xmlDataStr, 0 );
					siResult = MapNetInfoErrors( siResult );
					free( xmlDataStr );
				}
			}
		}
		else
		{
			siResult = eDSInvalidSession;
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( bFreePropList )
	{
		ni_proplist_free( &niPropList );
	}
	
	gNetInfoMutex->Signal();
	
	if ( nativeRecType != NULL )
		delete nativeRecType;
	if ( nativeAttrType != NULL )
		delete nativeAttrType;

	return( siResult );

} // SetShadowHashGlobalPolicies


//----------------------------------------------------------------------------------------------------
//  ReadHashStateFile
//
//  Returns: -1 = error, 0 = ok.
//----------------------------------------------------------------------------------------------------

int ReadHashStateFile( const char *inFilePath, sHashState *inOutHashState )
{
	CFStringRef myReplicaDataFilePathRef;
	CFURLRef myReplicaDataFileRef;
	CFReadStreamRef myReadStreamRef;
	CFPropertyListRef myPropertyListRef;
	CFStringRef errorString;
	CFPropertyListFormat myPLFormat;
	struct stat sb;
	CFMutableDictionaryRef stateDict = NULL;
	CFStringRef keyString = NULL;
	CFDateRef dateValue = NULL;
	CFNumberRef numberValue = NULL;
	long aLongValue;
	short aShortValue;
	int returnValue = 0;
	
	if ( inFilePath == NULL || inOutHashState == NULL )
		return -1;
	
	if ( stat( inFilePath, &sb ) != 0 )
	{
		time_t now;
		
		// initialize the creation date.
		time( &now );
		gmtime_r( &now, &inOutHashState->creationDate );
		return -1;
	}
	
	gNetInfoMutex->Wait();
	
	try
	{
		myReplicaDataFilePathRef = CFStringCreateWithCString( kCFAllocatorDefault, inFilePath, kCFStringEncodingUTF8 );
		if ( myReplicaDataFilePathRef == NULL )
			throw( (int)-1 );
	
		myReplicaDataFileRef = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, myReplicaDataFilePathRef, kCFURLPOSIXPathStyle, false );
	
		CFRelease( myReplicaDataFilePathRef );
	
		if ( myReplicaDataFileRef == NULL )
			throw( (int)-1 );
	
		myReadStreamRef = CFReadStreamCreateWithFile( kCFAllocatorDefault, myReplicaDataFileRef );
	
		CFRelease( myReplicaDataFileRef );
	
		if ( myReadStreamRef == NULL )
			throw( (int)-1 );
	
		CFReadStreamOpen( myReadStreamRef );
	
		errorString = NULL;
		myPLFormat = kCFPropertyListXMLFormat_v1_0;
		myPropertyListRef = CFPropertyListCreateFromStream( kCFAllocatorDefault, myReadStreamRef, 0, kCFPropertyListMutableContainersAndLeaves, &myPLFormat, &errorString );
		
		CFReadStreamClose( myReadStreamRef );
		CFRelease( myReadStreamRef );
	
		if ( errorString != NULL )
		{
			//char errMsg[256];
			
			//if ( CFStringGetCString( errorString, errMsg, sizeof(errMsg), kCFStringEncodingUTF8 ) )
				//errmsg( "could not load the replica file, error = %s", errMsg );
			CFRelease( errorString );
		}
		
		if ( myPropertyListRef == NULL )
		{
			//errmsg( "could not load the replica file because the property list is empty." );
			throw( (int)-1 );
		}
		
		if ( CFGetTypeID(myPropertyListRef) != CFDictionaryGetTypeID() )
		{
			CFRelease( myPropertyListRef );
			//errmsg( "could not load the replica file because the property list is not a dictionary." );
			throw( (int)-1 );
		}
		
		stateDict = (CFMutableDictionaryRef)myPropertyListRef;
					
		keyString = CFStringCreateWithCString( kCFAllocatorDefault, "CreationDate", kCFStringEncodingUTF8 );
		if ( keyString != NULL )
		{
			if ( CFDictionaryGetValueIfPresent( stateDict, keyString, (const void **)&dateValue ) )
			{
				pwsf_ConvertCFDateToBSDTime( dateValue, &inOutHashState->creationDate );
			}
			CFRelease( keyString );
		}
		keyString = CFStringCreateWithCString( kCFAllocatorDefault, "LastLoginDate", kCFStringEncodingUTF8 );
		if ( keyString != NULL )
		{
			if ( CFDictionaryGetValueIfPresent( stateDict, keyString, (const void **)&dateValue ) )
			{
				pwsf_ConvertCFDateToBSDTime( dateValue, &inOutHashState->lastLoginDate );
			}
			CFRelease( keyString );
		}
		keyString = CFStringCreateWithCString( kCFAllocatorDefault, "FailedLoginCount", kCFStringEncodingUTF8 );
		if ( keyString != NULL )
		{
			if ( CFDictionaryGetValueIfPresent( stateDict, keyString, (const void **)&numberValue ) &&
					CFGetTypeID(numberValue) == CFNumberGetTypeID() &&
					CFNumberGetValue( (CFNumberRef)numberValue, kCFNumberLongType, &aLongValue) )
			{
				inOutHashState->failedLoginAttempts = (UInt16)aLongValue;
			}
			CFRelease( keyString );
		}
		keyString = CFStringCreateWithCString( kCFAllocatorDefault, "NewPasswordRequired", kCFStringEncodingUTF8 );
		if ( keyString != NULL )
		{
			if ( CFDictionaryGetValueIfPresent( stateDict, keyString, (const void **)&numberValue ) &&
					CFGetTypeID(numberValue) == CFNumberGetTypeID() &&
					CFNumberGetValue( (CFNumberRef)numberValue, kCFNumberSInt16Type, &aShortValue) )
			{
				inOutHashState->newPasswordRequired = (UInt16)aShortValue;
			}
			CFRelease( keyString );
		}
	
		CFRelease( stateDict );
	}
	catch( int errCode )
	{
		returnValue = errCode;
	}
	
	gNetInfoMutex->Signal();
	
	return returnValue;
}


//----------------------------------------------------------------------------------------------------
//  WriteHashStateFile
//
//  Returns: -1 = error, 0 = ok.
//----------------------------------------------------------------------------------------------------

int
WriteHashStateFile( const char *inFilePath, sHashState *inHashState )
{
	CFStringRef myReplicaDataFilePathRef;
	CFURLRef myReplicaDataFileRef;
	CFWriteStreamRef myWriteStreamRef;
	CFStringRef errorString;
	int err = 0;
    //struct stat sb;
	CFMutableDictionaryRef prefsDict;
	CFDateRef aDateRef;
	
	if ( inFilePath == NULL || inHashState == NULL )
		return -1;
	
	// make the dict
	prefsDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	if ( prefsDict == NULL )
		return -1;
	
	gNetInfoMutex->Wait();
	
	try
	{
		if ( pwsf_ConvertBSDTimeToCFDate( &inHashState->creationDate, &aDateRef ) )
		{
			CFDictionaryAddValue( prefsDict, CFSTR("CreationDate"), aDateRef );
			CFRelease( aDateRef );
		}
		if ( pwsf_ConvertBSDTimeToCFDate( &inHashState->lastLoginDate, &aDateRef ) )
		{
			CFDictionaryAddValue( prefsDict, CFSTR("LastLoginDate"), aDateRef );
			CFRelease( aDateRef );
		}
		
		CFNumberRef failedAttemptCountRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt16Type, &(inHashState->failedLoginAttempts) );
		if ( failedAttemptCountRef != NULL )
		{
			CFDictionaryAddValue( prefsDict, CFSTR("FailedLoginCount"), failedAttemptCountRef );
			CFRelease( failedAttemptCountRef );
		}
		
		CFNumberRef newPasswordRequiredRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt16Type, &(inHashState->newPasswordRequired) );
		if ( newPasswordRequiredRef != NULL )
		{
			CFDictionaryAddValue( prefsDict, CFSTR("NewPasswordRequired"), newPasswordRequiredRef );
			CFRelease( newPasswordRequiredRef );
		}
		
		// WARNING: make sure the path to the file exists or CFStream code is unhappy
		/*
			err = stat( kPWReplicaDir, &sb );
			if ( err != 0 )
			{
				// make sure the directory exists
				err = mkdir( kPWReplicaDir, S_IRWXU );
				if ( err != 0 )
					return -1;
			}
		*/
		
		myReplicaDataFilePathRef = CFStringCreateWithCString( kCFAllocatorDefault, inFilePath, kCFStringEncodingUTF8 );
		if ( myReplicaDataFilePathRef == NULL )
			throw(-1);
		
		myReplicaDataFileRef = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, myReplicaDataFilePathRef, kCFURLPOSIXPathStyle, false );
		
		CFRelease( myReplicaDataFilePathRef );
		
		if ( myReplicaDataFileRef == NULL )
			throw(-1);
		
		myWriteStreamRef = CFWriteStreamCreateWithFile( kCFAllocatorDefault, myReplicaDataFileRef );
		
		CFRelease( myReplicaDataFileRef );
		
		if ( myWriteStreamRef == NULL )
			throw(-1);
		
		CFWriteStreamOpen( myWriteStreamRef );
		chmod( inFilePath, 0600 );
		
		errorString = NULL;
		CFPropertyListWriteToStream( prefsDict, myWriteStreamRef, kCFPropertyListXMLFormat_v1_0, &errorString );
		
		CFWriteStreamClose( myWriteStreamRef );
		CFRelease( myWriteStreamRef );
		
		if ( errorString != NULL )
		{
			//char errMsg[256];
			
			//if ( CFStringGetCString( errorString, errMsg, sizeof(errMsg), kCFStringEncodingUTF8 ) )
				//errmsg( "could not save the replica file, error = %s", errMsg );
			CFRelease( errorString );
		}
	}
	catch(...)
	{
		err = -1;
	}
	
	gNetInfoMutex->Signal();
	
	if ( prefsDict != NULL )
		CFRelease( prefsDict );
	
	return err;
}


#pragma mark -
#pragma mark Hash Utilities
#pragma mark -

//----------------------------------------------------------------------------------------------------
//  NIHashesEqual
//
//  Returns: BOOL
//
// ================================================================================
//	Hash File Matrix (Tiger)
// ---------------------------------------------------------------------
//	Hash Type						 Desktop		 Server		Priority
// ---------------------------------------------------------------------
//		NT								X				X			3
//		LM							   Opt.				X			4
//	   SHA1							  Erase			  Erase			-
//	 CRAM-MD5											X			5
//	Salted SHA1						   Opt.			   Opt.			2
//	RECOVERABLE										   Opt.			6
//	Security Team Favorite			  Only			  Only			1
//	
// ================================================================================
//----------------------------------------------------------------------------------------------------

bool NIHashesEqual( const unsigned char *inUserHashes, const unsigned char *inGeneratedHashes )
{
	static int sPriorityMap[  ][ 2 ] =
	{
		//	start, len
						// security team favorite goes here //
		{ kHashOffsetToSaltedSHA1, kHashSaltedSHA1Length },						// Salted SHA1
		{ kHashOffsetToSHA1, kHashSecureLength },								// SHA1
		{ kHashOffsetToNT, kHashShadowOneLength },								// NT
		{ kHashOffsetToLM, 16 },												// LM
		{ kHashOffsetToCramMD5, kHashCramLength },								// CRAM-MD5
		{ kHashOffsetToRecoverable, kHashRecoverableLength },					// RECOVERABLE
		{ 0, 0 }																// END
	};
	
	int start, len;
	bool result = false;
	
	for ( int idx = 0; ; idx++ )
	{
		start = sPriorityMap[idx][0];
		len = sPriorityMap[idx][1];
		
		if ( start == 0 && len == 0 )
			break;
		
		// verify with the highest priority hash that exists
		if ( memcmp( inUserHashes + start, sZeros, len ) != 0 )
		{
			if ( memcmp( inUserHashes + start, inGeneratedHashes + start, len ) == 0 )
				result = true;
			
			// stop here - do not fallback to lower priority hashes
			break;
		}
	}
	
	return result;
}


//------------------------------------------------------------------------------------
//	NIGetStateFilePath
//
//	Returns: ds err code
//
//  <outStateFilePath> is malloc'd and must be freed by the caller.
//------------------------------------------------------------------------------------

sInt32 NIGetStateFilePath( const char *inHashPath, char **outStateFilePath )
{
	sInt32 siResult = eDSNoErr;
	
	if ( inHashPath == NULL || outStateFilePath == NULL )
		return eParameterError;
	
	*outStateFilePath = NULL;
	
	char *stateFilePath = (char *) calloc( strlen(inHashPath) + sizeof(kShadowHashStateFileSuffix) + 1, 1 );
	if ( stateFilePath != NULL )
	{
		strcpy( stateFilePath, inHashPath );
		strcat( stateFilePath, kShadowHashStateFileSuffix );
		*outStateFilePath = stateFilePath;
	}
	else
	{
		siResult = eMemoryError;
	}
	
	return siResult;
}


//--------------------------------------------------------------------------------------------------
// * GenerateShadowHashes
//--------------------------------------------------------------------------------------------------

void GenerateShadowHashes(
	const char *inPassword,
	long inPasswordLen,
	int	inAdditionalHashList,
	const unsigned char *inSHA1Salt,
	unsigned char *outHashes,
	unsigned long *outHashTotalLength )
{
	SHA_CTX				sha_context							= {};
	unsigned char		digestData[kHashSecureLength]		= {0};
	long				pos									= 0;
	
	/* start clean */
	bzero( outHashes, kHashTotalLength );
	
	/* NT */
	if ( (inAdditionalHashList & kNiPluginHashNT) )
		CalculateSMBNTHash( inPassword, outHashes );
	pos = kHashShadowOneLength;
	
	/* LM */
	if ( (inAdditionalHashList & kNiPluginHashLM) )
		CalculateSMBLANManagerHash( inPassword, outHashes + kHashShadowOneLength );
	pos = kHashShadowBothLength;
	
	/* SHA1 - Deprecated BUT required for automated legacy upgrades */
	if ( (inAdditionalHashList & kNiPluginHashSHA1) )
	{
		SHA1_Init( &sha_context );
		SHA1_Update( &sha_context, (unsigned char *)inPassword, inPasswordLen );
		SHA1_Final( digestData, &sha_context );
		memmove( outHashes + pos, digestData, kHashSecureLength );
	}
	pos += kHashSecureLength;
	
	/* CRAM-MD5 */
	if ( (inAdditionalHashList & kNiPluginHashCRAM_MD5) )
	{
		unsigned long cramHashLen = 0;
		pwsf_getHashCramMD5( (const unsigned char *)inPassword, inPasswordLen, outHashes + pos, &cramHashLen );
	}
	pos += kHashCramLength;
	
	/* 4-byte Salted SHA1 */
	if ( (inAdditionalHashList & kNiPluginHashSaltedSHA1) )
	{
		unsigned long salt;
		
		if ( inSHA1Salt != NULL )
		{
			memcpy( &salt, inSHA1Salt, 4 );
			memcpy( outHashes + pos, inSHA1Salt, 4 );
		}
		else
		{
			::srandom(getpid() + time(0));
			salt = (unsigned long) random();
			memcpy( outHashes + pos, &salt, 4 );
		}
		
		pos += 4;
		SHA1_Init( &sha_context );
		SHA1_Update( &sha_context, (unsigned char *)&salt, 4 );
		SHA1_Update( &sha_context, (unsigned char *)inPassword, inPasswordLen );
		SHA1_Final( digestData, &sha_context );
		memmove( outHashes + pos, digestData, kHashSecureLength );
		pos += kHashSecureLength;
	}
	else
	{
		pos += 4 + kHashSecureLength;
	}
	
	/* recoverable */
	if ( gServerOS && (inAdditionalHashList & kNiPluginHashRecoverable) )
	{
		unsigned char passCopy[kHashRecoverableLength];
		unsigned char iv[AES_BLOCK_SIZE];
		AES_KEY encryptAESKey;
		
		bzero( passCopy, sizeof(passCopy) );
		memcpy( passCopy, inPassword, inPasswordLen );
		
		bzero( &encryptAESKey, sizeof(encryptAESKey) );
		AES_set_encrypt_key( (const unsigned char *)"key4now-key4now-key4now", 128, &encryptAESKey );
		
		memcpy( iv, kAESVector, sizeof(iv) );
		AES_cbc_encrypt( passCopy, outHashes + pos, sizeof(passCopy), &encryptAESKey, iv, AES_ENCRYPT );
	}
	pos += kHashRecoverableLength;
	
	*outHashTotalLength = kHashTotalLength;
}


//--------------------------------------------------------------------------------------------------
// * UnobfuscateRecoverablePassword()
//--------------------------------------------------------------------------------------------------

sInt32 UnobfuscateRecoverablePassword(
	unsigned char *inData,
	unsigned char **outPassword,
	unsigned long *outPasswordLength )
{
	// un-obfuscate
	unsigned char iv[AES_BLOCK_SIZE];
	unsigned char passCopy[kHashRecoverableLength + AES_BLOCK_SIZE];
	AES_KEY decryptAESKey;
	
	if ( inData == NULL || outPassword == NULL || outPasswordLength == NULL )
		return eParameterError;
	
	bzero( passCopy, sizeof(passCopy) );
	
	bzero( &decryptAESKey, sizeof(decryptAESKey) );
	AES_set_decrypt_key( (const unsigned char *)"key4now-key4now-key4now", 128, &decryptAESKey );
	
	memcpy( iv, kAESVector, sizeof(iv) );
	AES_cbc_encrypt( inData, passCopy, kHashRecoverableLength, &decryptAESKey, iv, AES_DECRYPT );
	
	*outPasswordLength = strlen( (char *)passCopy );
	*outPassword = (unsigned char *) malloc( (*outPasswordLength) + 1 );
	if ( (*outPassword) == NULL )
		return eMemoryError;
	
	strlcpy( (char *)*outPassword, (char *)passCopy, (*outPasswordLength) + 1 );
	
	return eDSNoErr;
}


//--------------------------------------------------------------------------------------------------
// * GetHashSecurityLevelConfig ()
//--------------------------------------------------------------------------------------------------

sInt32 GetHashSecurityLevelConfig( void *inDomain, unsigned int *outHashList )
{
	sInt32				error					= eDSNoErr;
	ni_status			niResult				= NI_OK;
	char				*nativeRecType			= NULL;
	char				*nativeAttrType			= NULL;
	char				*policyStr				= NULL;
	unsigned int		idx						= 0;
    ni_id				niDirID;
	ni_namelist			niValues;
	
	if ( inDomain == NULL || outHashList == NULL )
		return eParameterError;
	
	*outHashList = gServerOS ? kNiPluginHashDefaultServerSet : kNiPluginHashDefaultSet;
	
	nativeRecType = MapRecToNetInfoType( kDSStdRecordTypeConfig );
	if ( nativeRecType == NULL )
		return eDSInvalidRecordType;
	
	try
	{
		error = IsValidRecordName( kShadowHashRecordName, nativeRecType, inDomain, niDirID );
		if ( error != eDSNoErr ) 
			throw( error );
		
		gNetInfoMutex->Wait();
		//lookup global policies attribute
		niResult = ni_lookupprop( inDomain, &niDirID, "optional_hash_list", &niValues );
		gNetInfoMutex->Signal();
		if ( niResult != NI_OK ) throw( (sInt32)eDSAuthFailed );
		
		// The attribute is present, so switch from the server default set to a minimum set.
		// To be strict, we could start with a zero-set. However, doing so is dangerous because
		// if the system ever has an attribute with no values, that would mean storing no hashes at all.
		if ( gServerOS )
			*outHashList = kNiPluginHashSaltedSHA1;
		
		// look for optional methods
		for ( idx = 0; idx < niValues.ni_namelist_len; idx++ )
		{
			if ( GetHashSecurityBitsForString( niValues.ni_namelist_val[idx], outHashList ) )
				break;
		}
		
		gNetInfoMutex->Wait();
		if (niValues.ni_namelist_len > 0)
		{
			ni_namelist_free( &niValues );
		}
		gNetInfoMutex->Signal();
	}
	catch( sInt32 catchErr )
	{
		error = catchErr;
	}
	
	if ( nativeRecType != NULL )
		delete nativeRecType;
	if ( nativeAttrType != NULL )
		delete nativeAttrType;
	if ( policyStr != NULL )
		free( policyStr );
	
	return error;
}


//--------------------------------------------------------------------------------------------------
// * GetHashSecurityLevelForUser ()
//--------------------------------------------------------------------------------------------------

sInt32 GetHashSecurityLevelForUser( const char *inHashList, unsigned int *outHashList )
{
	char				*hashListStr			= NULL;
	char				*hashListPtr			= NULL;
	char				*hashTypeStr			= NULL;
	char				*endPtr					= NULL;
	
	if ( inHashList == NULL || outHashList == NULL )
		return eParameterError;
	
	// legacy data value (returns the default set, with NT forced on and LM forced off)
	if ( strcasecmp( inHashList, kDSTagAuthAuthorityBetterHashOnly ) == 0 )
	{
		*outHashList |= kNiPluginHashNT;
		*outHashList &= (0x7FFF ^ kNiPluginHashLM);
		
		return eDSNoErr;
	}
	else
	if ( strncasecmp( inHashList, kNIHashNameListPrefix, sizeof(kNIHashNameListPrefix)-1 ) == 0 )
	{
		// look for optional methods
		hashListPtr = hashListStr = strdup( inHashList );
		hashListPtr += sizeof(kNIHashNameListPrefix) - 1;
		
		// require open and close brackets
		if ( *hashListPtr++ != '<' )
			return eParameterError;
		endPtr = strchr( hashListPtr, '>' );
		if ( endPtr == NULL )
			return eParameterError;
			
		*endPtr = '\0';
		
		*outHashList = 0;
	
		// walk the list
		while ( (hashTypeStr = strsep( &hashListPtr, "," )) != NULL )
		{
			if ( GetHashSecurityBitsForString( hashTypeStr, outHashList ) )
				break;
		}
		
		if ( hashListStr != NULL )
			free( hashListStr );
	}
	else
	{
		return eParameterError;
	}
		
	return eDSNoErr;
}


//--------------------------------------------------------------------------------------------------
// * GetHashSecurityBitsForString ()
//
//	Returns: TRUE if the caller should stop (secure mode)
//--------------------------------------------------------------------------------------------------

bool GetHashSecurityBitsForString( const char *inHashType, unsigned int *inOutHashList )
{
	bool returnVal = false;
	
	if ( strcasecmp( inHashType, kNIHashNameNT ) == 0 )
		*inOutHashList |= kNiPluginHashNT;
	else if ( strcasecmp( inHashType, kNIHashNameLM ) == 0 )
		*inOutHashList |= kNiPluginHashLM;
	else if ( strcasecmp( inHashType, kNIHashNameCRAM_MD5 ) == 0 )
		*inOutHashList |= kNiPluginHashCRAM_MD5;
	else if ( strcasecmp( inHashType, kNIHashNameSHA1 ) == 0 )
		*inOutHashList |= kNiPluginHashSaltedSHA1;
	else if ( strcasecmp( inHashType, kNIHashNameRecoverable ) == 0 )
		*inOutHashList |= kNiPluginHashRecoverable;
	else if ( strcasecmp( inHashType, kNIHashNameSecure ) == 0 )
	{
		// If the secure hash is used, all other hashes are OFF
		*inOutHashList = kNiPluginHashSecurityTeamFavorite;
		returnVal = true;
	}
	
	return returnVal;
}


//--------------------------------------------------------------------------------------------------
// * MSCHAPv2 ()
//--------------------------------------------------------------------------------------------------

sInt32 MSCHAPv2(
	const unsigned char *inC16,
	const unsigned char *inPeerC16,
	const unsigned char *inNTLMDigest,
	const char *inSambaName,
	const unsigned char *inOurHash,
	char *outMSCHAP2Response )
{
	unsigned char ourP24[kHashShadowResponseLength];
	unsigned char Challenge[8];
	sInt32 result = eDSAuthFailed;
	
	ChallengeHash( inPeerC16, inC16, inSambaName, Challenge );
	ChallengeResponse( Challenge, inOurHash, ourP24 );
	
	if ( memcmp( ourP24, inNTLMDigest, kHashShadowResponseLength ) == 0 )
	{
		GenerateAuthenticatorResponse( inOurHash, ourP24, Challenge, outMSCHAP2Response );
		result = eDSNoErr;
	}
	
	return result;
}


//--------------------------------------------------------------------------------------------------
// * CRAM_MD5 ()
//--------------------------------------------------------------------------------------------------

void NI_hmac_md5_import(HMAC_MD5_CTX *hmac, HMAC_MD5_STATE *state)
{
	bzero((char *)hmac, sizeof(HMAC_MD5_CTX));
		
	hmac->ictx.A = ntohl(state->istate[0]);
	hmac->ictx.B = ntohl(state->istate[1]);
	hmac->ictx.C = ntohl(state->istate[2]);
	hmac->ictx.D = ntohl(state->istate[3]);
	
	hmac->octx.A = ntohl(state->ostate[0]);
	hmac->octx.B = ntohl(state->ostate[1]);
	hmac->octx.C = ntohl(state->ostate[2]);
	hmac->octx.D = ntohl(state->ostate[3]);
	
	/* Init the counts to account for our having applied
	* 64 bytes of key; this works out to 0x200 (64 << 3; see
	* MD5Update above...) */
	hmac->ictx.Nl = hmac->octx.Nl = 0x200;
}


void NI_hmac_md5_final(unsigned char digest[HMAC_MD5_SIZE], HMAC_MD5_CTX *hmac)
{
	MD5_Final(digest, &hmac->ictx);  /* Finalize inner md5 */
	MD5_Update(&hmac->octx, digest, MD5_DIGEST_LENGTH); /* Update outer ctx */
	MD5_Final(digest, &hmac->octx); /* Finalize outer md5 */
}


sInt32 CRAM_MD5( const unsigned char *inHash, const char *inChallenge, const unsigned char *inResponse )
{
	sInt32 siResult = eDSAuthFailed;
	HMAC_MD5_STATE md5state;
	HMAC_MD5_CTX tmphmac;
	unsigned char digest[MD5_DIGEST_LENGTH];
	char correctAnswer[32];
	
	memcpy(&md5state, inHash, sizeof(HMAC_MD5_STATE));
	NI_hmac_md5_import(&tmphmac, (HMAC_MD5_STATE *) &md5state);
	MD5_Update(&(tmphmac.ictx), (const unsigned char *) inChallenge, strlen(inChallenge));
	NI_hmac_md5_final(digest, &tmphmac);
    
    /* convert to hex with lower case letters */
    CvtHex(digest, (unsigned char *)correctAnswer);
	
	if ( strncasecmp((char *)inResponse, correctAnswer, 32) == 0 )
		siResult = eDSNoErr;
	
	return siResult;
}


//--------------------------------------------------------------------------------------------------
// * Verify_APOP ()
//--------------------------------------------------------------------------------------------------

sInt32 Verify_APOP( const char *userstr, const unsigned char *inPassword, unsigned long inPasswordLen, const char *challenge, const char *response )
{
    sInt32 siResult = eDSAuthFailed;
    unsigned char digest[16];
    char digeststr[33];
    MD5_CTX ctx;

    if ( challenge == NULL || inPassword == NULL || response == NULL )
       return eParameterError;
	
    MD5_Init( &ctx );
    MD5_Update( &ctx, challenge, strlen(challenge) );
    MD5_Update( &ctx, inPassword, inPasswordLen );
    MD5_Final( digest, &ctx );
	
    /* convert digest from binary to ASCII hex */
	CvtHex(digest, (unsigned char *)digeststr);
	
    if ( strncasecmp(digeststr, response, 32) == 0 )
	{
      /* password verified! */
      siResult = eDSNoErr;
    }
	
    return siResult;
}


#pragma mark -
#pragma mark Record Utilities
#pragma mark -

// ---------------------------------------------------------------------------
//	* IsValidRecordName
// ---------------------------------------------------------------------------

sInt32 IsValidRecordName (	const char	*inRecName,
							const char	*inRecType,
							void		*inDomain,
							ni_id		&outDirID )
{
	sInt32			siResult	= eDSInvalidRecordName;
	char		   *pData		= nil;
	ni_status		niStatus	= NI_OK;

	gNetInfoMutex->Wait();

	try
	{
		if ( inDomain == nil ) throw( (sInt32)eDSInvalidDomain );
		if ( inRecName == nil ) throw( (sInt32)eDSInvalidRecordName );
		if ( inRecType == nil ) throw( (sInt32)eDSInvalidRecordType );

		pData = BuildRecordNamePath( inRecName, inRecType );
		if ( pData != nil )
		{
			niStatus = ::ni_pathsearch( inDomain, &outDirID, pData );
			if ( niStatus == NI_OK )
			{
				siResult = eDSNoErr;
			}
			free( pData );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // IsValidRecordName


// ---------------------------------------------------------------------------
//	* BuildRecordNamePath
// ---------------------------------------------------------------------------

char *BuildRecordNamePath( const char *inRecName, const char *inRecType )
{
	const char	   *pData		= nil;
	CString			csPath( 128 );
	char		   *outPath		= nil;
	uInt32			recNameLen	= 0;
	uInt32			nativeLen	= 0;
	uInt32			stdLen		= 0;

	try
	{
		if ( inRecName == nil ) throw( (sInt32)eDSInvalidRecordName );
		if ( inRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
		
		recNameLen	= ::strlen( inRecName );
		nativeLen	= sizeof(kDSNativeAttrTypePrefix) - 1;
		stdLen		= sizeof(kDSStdAttrTypePrefix) - 1;
		
		if ( ::strncmp( inRecName, kDSNativeAttrTypePrefix, nativeLen ) == 0  )
		{
			if ( recNameLen > nativeLen )
			{
				pData = inRecName + nativeLen;
				DBGLOG1( kLogPlugin, "BuildRecordNamePath:: Warning:Native record name path <%s> is being used", pData );
			}
		}
		else if ( ::strncmp( inRecName, kDSStdAttrTypePrefix, stdLen ) == 0  )
		{
			if ( recNameLen > stdLen )
			{
				pData = inRecName + stdLen;
			}
		}
		else
		{
			pData = inRecName;
			DBGLOG1( kLogPlugin, "BuildRecordNamePath:: Warning:Native record name path <%s> is being used", pData );
		}

		if ( pData != nil )
		{
			//KW check if the recordname has "/" or "\\" in it
			//if so then replace the "/" with "\\/" so that NetInfo can handle the forward slashes
			//separate of the subdirectory delimiters
			//also replace the "\\" with "\\\\" so that NetInfo can handle the backslashes if
			//the intent is for them to be inside the name itself
			if ( (::strstr( pData, "/" ) != nil) || (::strstr( pData, "\\" ) != nil) )
			{
				csPath.Set( "/" );
				csPath.Append( inRecType );
				csPath.Append( "/" );
				while(pData[0] != '\0')
				{
					if (pData[0] == '/')
					{
						csPath.Append( "\\/" );
					}
					else if (pData[0] == '\\')
					{
						csPath.Append( "\\\\" );
					}
					else
					{
						csPath.Append( pData[0] );
					}
					pData++;
				}
			}
			else
			{
				csPath.Set( "/" );
				csPath.Append( inRecType );
				csPath.Append( "/" );
				csPath.Append( pData );

			}
			//check for the case of trying to access the root of netinfo above all the records
			//ie. record type was "/" and record name was also "/" which led to csPath of "///\\/"
			if (strcmp(csPath.GetData(),"///\\/") == 0)
			{
				outPath = (char *)::calloc( 2, sizeof(char));
				if ( outPath == nil ) throw( (sInt32)eMemoryError );
				strcpy(outPath,"/");
			}
			else
			{
				outPath = (char *)::calloc( csPath.GetLength() + 1, sizeof(char));
				if ( outPath == nil ) throw( (sInt32)eMemoryError );
				strcpy(outPath,csPath.GetData());
			}
		}
	}

	catch( sInt32 err )
	{
		if (outPath != nil)
		{
			free(outPath);
			outPath = nil;
		}
	}

	return( outPath );

} // BuildRecordNamePath


// ---------------------------------------------------------------------------
//	* UserIsAdmin
// ---------------------------------------------------------------------------

bool UserIsAdmin( const char *inUserName, sNIContextData *inContext  )
{
	ni_id			niDirID;
	ni_status		niStatus	= NI_OK;
	ni_proplist		niPropList;
	ni_index		niIndex		= 0;
	bool			isAdmin		= false;

	if ( inUserName == NULL || inContext == NULL ) {
		DBGLOG( kLogPlugin,  "UserIsAdmin failed due to NULL input parameters" );
		return false;
	}
	
	gNetInfoMutex->Wait();

	void *aNIDomain = RetrieveNIDomain(inContext);
	if (aNIDomain != NULL)
	{
		niStatus = ::ni_pathsearch( aNIDomain, &niDirID, "/groups/admin" );

		if (niStatus == NI_OK) {
			niStatus = ::ni_read( aNIDomain, &niDirID, &niPropList );

			if (niStatus == NI_OK) {
				niIndex = ::ni_proplist_match( niPropList, "users", nil );
				if (niIndex != NI_INDEX_NULL) {
					niIndex = ni_namelist_match(niPropList.nipl_val[niIndex].nip_val,inUserName);
					isAdmin = (niIndex != NI_INDEX_NULL);
				}
				::ni_proplist_free( &niPropList );
			}
		}
	}	
	
	gNetInfoMutex->Signal();

	return isAdmin;
} //UserIsAdmin


// ---------------------------------------------------------------------------
//	* UserIsAdminInDomain
// ---------------------------------------------------------------------------

bool UserIsAdminInDomain( const char *inUserName, void *inDomain )
{
	ni_id			niDirID;
	ni_status		niStatus	= NI_OK;
	ni_proplist		niPropList;
	ni_index		niIndex		= 0;
	bool			isAdmin		= false;

	if ( inUserName == NULL || inDomain == NULL ) {
		DBGLOG( kLogPlugin,  "UserIsAdmin failed due to NULL input parameters" );
		return false;
	}
	
	gNetInfoMutex->Wait();

	niStatus = ::ni_pathsearch( inDomain, &niDirID, "/groups/admin" );

	if (niStatus == NI_OK) {
		niStatus = ::ni_read( inDomain, &niDirID, &niPropList );

		if (niStatus == NI_OK) {
			niIndex = ::ni_proplist_match( niPropList, "users", nil );
			if (niIndex != NI_INDEX_NULL) {
				niIndex = ni_namelist_match(niPropList.nipl_val[niIndex].nip_val,inUserName);
				isAdmin = (niIndex != NI_INDEX_NULL);
			}
			::ni_proplist_free( &niPropList );
		}
	}
	
	gNetInfoMutex->Signal();

	return isAdmin;
} //UserIsAdminInDomain


// ---------------------------------------------------------------------------
//	* GetGUIDForRecord
//
//	Returns: allocated string of GUID or NULL.
// ---------------------------------------------------------------------------

char *GetGUIDForRecord( sNIContextData *inContext, ni_id *inNIDirID )
{
	ni_status			niResultGUID			= NI_INVALIDDOMAIN;
	char			   *GUIDString				= NULL;
	ni_namelist			niValuesGUID;
	
	// get the GUID
	gNetInfoMutex->Wait();
	void *aNIDomain = RetrieveNIDomain(inContext);
	if (aNIDomain != NULL)
	{
		niResultGUID = ni_lookupprop( aNIDomain, inNIDirID, "generateduid", &niValuesGUID );
	}
	gNetInfoMutex->Signal();
	
	if ( (niResultGUID == NI_OK) && (niValuesGUID.ni_namelist_len > 0) )
	{
		if ( niValuesGUID.ni_namelist_val[0] != NULL )
			GUIDString = strdup( niValuesGUID.ni_namelist_val[0] );
		
		gNetInfoMutex->Wait();
		ni_namelist_free( &niValuesGUID );
		gNetInfoMutex->Signal();
	}
	
	return GUIDString;
}


#pragma mark -
#pragma mark Misc. Utilities
#pragma mark -

// ---------------------------------------------------------------------------
//	* ParseLocalCacheUserData
//    retrieve network nodename, user recordname, and user generated UID from authdata
//    format is version;tag;data
// ---------------------------------------------------------------------------

sInt32 ParseLocalCacheUserData (	const char	   *inAuthData,
									char		  **outNodeName,
									char		  **outRecordName,
									char		  **outGUID )
{
	char* authData = NULL;
	char* current = NULL;
	char* tempPtr = NULL;
	sInt32 result = eDSAuthFailed;

	if ( inAuthData == NULL )
	{
		return (sInt32)eDSEmptyBuffer;
	}
	if ( outNodeName == NULL )
	{
		return (sInt32)eDSEmptyNodeName;
	}
	if ( outRecordName == NULL )
	{
		return (sInt32)eDSEmptyRecordName;
	}
	if ( outGUID == NULL )
	{
		return (sInt32)eDSEmptyParameter;
	}

	authData = strdup(inAuthData);
	if (authData == NULL)
	{
		return eDSAuthFailed;
	}
	current = authData;
	do {
		tempPtr = strsep(&current, ":");
		if (tempPtr == NULL)
		{
			result = eDSEmptyNodeName;
			break;
		}
		*outNodeName = strdup(tempPtr);
		
		tempPtr = strsep(&current, ":");
		if (tempPtr == NULL)
		{
			result = eDSEmptyRecordName;
			break;
		}
		*outRecordName = strdup(tempPtr);
		
		tempPtr = strsep(&current, ":");
		if (tempPtr == NULL)
		{
			result = eDSEmptyParameter;
			break;
		}
		*outGUID = strdup(tempPtr);
		
		result = eDSNoErr;
	} while (false);
	
	free(authData);
	authData = NULL;
	
	if (result != eDSNoErr)
	{
		if (*outNodeName != NULL)
		{
			free(*outNodeName);
			*outNodeName = NULL;
		}
		if (*outRecordName != NULL)
		{
			free(*outRecordName);
			*outRecordName = NULL;
		}
		if (*outGUID != NULL)
		{
			free(*outGUID);
			*outGUID = NULL;
		}
	}
	return result;
} // ParseLocalCacheUserData


void* RetrieveNIDomain( sNIContextData* inContext )
{
	if (inContext == nil)
	{
		return(nil);
	}
	else
	{
		if (inContext->fDontUseSafeClose)
		{
			return(inContext->fDomain);
		}
		else if (inContext->fDomainPath != nil)
		{
			void *outDomain = RetrieveSharedNIDomain(inContext);
			return(outDomain);
		}
		else
		{
			return(nil);
		}
	}
} // RetrieveNIDomain


void* RetrieveSharedNIDomain( sNIContextData* inContext )
{
	void *outDomain = NULL;
	
	if (inContext == nil || inContext->fDomainPath == nil)
	{
		return(nil);
	}
	else
	{
		outDomain = CNetInfoPlugin::GetNIDomain(inContext->fDomainPath);
		if (outDomain == nil) //try to open it fresh since we lost it in the middle of context
		{
			char *domName = nil;
			gNetInfoMutex->Wait(); //redundant since should already be holding this
			sInt32 result = CNetInfoPlugin::SafeOpen( inContext->fDomainPath, 10, &domName );
			gNetInfoMutex->Signal();
			if ( result != eDSNoErr || domName == nil || (strcmp(domName,"") == 0) )
			{
				//failure to reopen
				DBGLOG1( kLogPlugin, "RetrieveSharedNIDomain: failed to reopen the NetInfo node domain <%s> connection lost in the middle of context", inContext->fDomainPath );
			}
			else
			{
				DSFreeString(inContext->fDomainName);
				inContext->fDomainName = domName;
				//try again
				outDomain = CNetInfoPlugin::GetNIDomain(inContext->fDomainPath);
			}
		}
		return(outDomain);
	}
} // RetrieveSharedNIDomain



// ---------------------------------------------------------------------------
//	* BuildDomainPathFromName
// ---------------------------------------------------------------------------

char* BuildDomainPathFromName( char* inDomainName )
{
	CString		csPathStr( 128 );
	char	   *pathStr			= nil;
	int			prefixLength    = 0;
	int			inputLength		= 0;

	if ( inDomainName == nil )
	{
		return nil;
	}
	
	inputLength = strlen(inDomainName);
	
	//need the kstrPrefixName prefix at a minimum
	prefixLength = strlen( kstrPrefixName );
	if ( inputLength < prefixLength )
	{
		return nil;		
	}
	else if (::strncmp( inDomainName, kstrPrefixName, prefixLength ) != 0)
	{
		return nil;
	}
	
	// Check for the local domain
	if ( (::strcmp( inDomainName, kstrLocalDomain ) == 0) ||
		 (::strcmp( inDomainName, kstrRootLocalDomain ) == 0) ||
		 (::strcmp( inDomainName, kstrDefaultLocalNodeName ) == 0 ) )
	{
		csPathStr.Set( kstrLocalDot );
	}
	//check for the root domain
	else if ( (::strcmp( inDomainName, kstrPrefixName ) == 0) ||
			  (::strcmp( inDomainName, kstrRootNodeName ) == 0) )
	{
		csPathStr.Set( kstrDelimiter );
	}
	//check for the explicitly declared parent domain
	else if ( ::strcmp( inDomainName, kstrParentDomain ) == 0 )
	{
		csPathStr.Set( kstrParentDotDot );
	}
	//check for all other domains with the prefix kstrRootNodeName
	else
	{
		prefixLength = strlen( kstrRootNodeName );
		if (    ( inputLength > prefixLength ) &&
					 ( strncmp( inDomainName, kstrRootNodeName, prefixLength ) == 0 ) )
		{
			csPathStr.Set( inDomainName + prefixLength ); //this strips off the kstrRootNodeName
		}
		else
		{
			return nil;
		}
	}

	pathStr = (char *)::calloc( csPathStr.GetLength() + 1, sizeof( char ) );
	strcpy( pathStr, csPathStr.GetData());
	
	return pathStr;
} // BuildDomainPathFromName


// ---------------------------------------------------------------------------
//	* NormalizeNIString
// ---------------------------------------------------------------------------

char* NormalizeNIString( char* inUTF8String, char* inNIAttributeType )
{
	CFStringRef originalCFStr = NULL;
	CFMutableStringRef normalizedCFStr = NULL;
	char* normalizedUTF8String = NULL;
	int index = 0;
	int maxNormalizedLength = 0;
	Boolean success = false;

	if ( inUTF8String == NULL )
	{
		return NULL;
	}
	
	if (inNIAttributeType != NULL && strcmp( inNIAttributeType, "realname" ) != 0)
	{
		// only normalize realname attribute
		return inUTF8String;
	}
	
	// search for end of the string or non-ASCII character
	// since it's a signed char, non-ASCII will be negative
	while ( inUTF8String[index] > '\0' )
	{
		++index;
	}
	
	if ( inUTF8String[index] == '\0' )
	{
		// all ASCII, nothing to be done
		return inUTF8String;
	}
	
	// we need to normalize the string to precomposed Unicode
	originalCFStr = CFStringCreateWithCString( NULL,inUTF8String,kCFStringEncodingUTF8 );
	if ( originalCFStr != NULL )
	{
		normalizedCFStr = CFStringCreateMutableCopy( NULL, 0, originalCFStr );
		if ( normalizedCFStr != NULL )
		{
			CFStringNormalize( normalizedCFStr,kCFStringNormalizationFormC );
			maxNormalizedLength = CFStringGetMaximumSizeForEncoding( CFStringGetLength(normalizedCFStr),
				kCFStringEncodingUTF8 );
			normalizedUTF8String = (char*)calloc( sizeof(char), maxNormalizedLength + 1 );
			success = CFStringGetCString( normalizedCFStr,normalizedUTF8String,
				maxNormalizedLength + 1,kCFStringEncodingUTF8 );
			CFRelease( normalizedCFStr );
		}
		CFRelease( originalCFStr );
	}
	
	if ( success == true )
	{
		return normalizedUTF8String;
	}
	else
	{
		DSFreeString( normalizedUTF8String );
		return inUTF8String;
	}
}



// ---------------------------------------------------------------------------
//	* NormalizeNINameList
// ---------------------------------------------------------------------------

void NormalizeNINameList( ni_namelist* inNameList, char* inNIAttributeType )
{
	unsigned int index = 0;
	
	if (inNIAttributeType != NULL && strcmp( inNIAttributeType, "realname" ) != 0)
	{
		// only normalize realname attribute
		return;
	}
	
	if (inNameList != NULL 
		&& inNameList->ni_namelist_len > 0 && inNameList->ni_namelist_val != NULL)
	{
		while ( inNameList->ni_namelist_val[index] != NULL && index < inNameList->ni_namelist_len )
		{
			char* normalizedString = NormalizeNIString( inNameList->ni_namelist_val[index], inNIAttributeType );
			if ( normalizedString != inNameList->ni_namelist_val[index] ) {
				free( inNameList->ni_namelist_val[index] );
				inNameList->ni_namelist_val[index] = normalizedString;
			}
			++index;
		}
	}
}
