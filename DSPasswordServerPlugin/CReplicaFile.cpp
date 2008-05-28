/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
/*
 *  CReplicaFile.cpp
 *  PasswordServer
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <openssl/md5.h>
#include <time.h>
#include <syslog.h>

#include "AuthFile.h"
#include "CReplicaFile.h"
#include "CPSUtilities.h"

#define errmsg(A, args...)			syslog(LOG_ALERT,(A),##args)

//----------------------------------------------------------------------------------------------------
//	MergeReplicaLists
//
//	Returns: TRUE if <inOutList2> was changed.
//----------------------------------------------------------------------------------------------------

bool
CReplicaFile::MergeReplicaLists( CReplicaFile *inList1, CReplicaFile *inOutList2 )
{
	CFIndex index, repCount;
	CFMutableDictionaryRef repDict;
	CFMutableDictionaryRef keepRepDict;
	CFDateRef lastSyncDate = NULL;
	CFDateRef targetLastSyncDate = NULL;
	CFDateRef sinceNeverReferenceDate = NULL;
	char replicaName[256];
	bool changed = false;
	
	if ( inList1 == NULL || inOutList2 == NULL )
		return false;
	
	sinceNeverReferenceDate = CFDateCreate( kCFAllocatorDefault, kCFAbsoluteTimeIntervalSince1970 );
	
	repCount = inList1->ReplicaCount();
	for ( index = 0; index < repCount; index++ )
	{
		repDict = (CFMutableDictionaryRef)inList1->GetReplica( index );
		if ( repDict == NULL )
			continue;
		
		inList1->GetNameOfReplica( repDict, replicaName );
		keepRepDict = inOutList2->GetReplicaByName( replicaName );
		if ( keepRepDict == NULL )
		{
			inOutList2->AddReplica( repDict );
			changed = true;
		}
		else
		{
			// get the last sync dates
			if ( CFDictionaryGetValueIfPresent( repDict, CFSTR(kPWReplicaSyncDateKey), (const void **)&lastSyncDate ) )
			{
				if ( CFGetTypeID(lastSyncDate) != CFDateGetTypeID() )
					lastSyncDate = sinceNeverReferenceDate;
			}
			else
			{
				lastSyncDate = sinceNeverReferenceDate;
			}
						
			if ( CFDictionaryGetValueIfPresent( keepRepDict, CFSTR(kPWReplicaSyncDateKey), (const void **)&targetLastSyncDate ) )
			{
				if ( CFGetTypeID(targetLastSyncDate) != CFDateGetTypeID() )
				{
					// this is the dict we're keeping so fix it
					CFDictionaryRemoveValue( keepRepDict, CFSTR(kPWReplicaSyncDateKey) );
					targetLastSyncDate = sinceNeverReferenceDate;
				}
			}
			else
			{
				targetLastSyncDate = sinceNeverReferenceDate;
			}
			
			if ( targetLastSyncDate != NULL && lastSyncDate != NULL &&
				 CFDateCompare( targetLastSyncDate, lastSyncDate, NULL ) == kCFCompareGreaterThan )
			{
				CFStringRef idRangeBegin;
				CFStringRef idRangeEnd;
				CFStringRef ip;
				CFStringRef dns;
				CFStringRef replicaStatus;
				
				// get 'em
				if ( ! CFDictionaryGetValueIfPresent( repDict, CFSTR(kPWReplicaIDRangeBeginKey), (const void **)&idRangeBegin ) )
					idRangeBegin = NULL;
				if ( ! CFDictionaryGetValueIfPresent( repDict, CFSTR(kPWReplicaIDRangeEndKey), (const void **)&idRangeEnd ) )
					idRangeEnd = NULL;
				if ( ! CFDictionaryGetValueIfPresent( repDict, CFSTR(kPWReplicaIPKey), (const void **)&ip ) )
					ip = NULL;
				if ( ! CFDictionaryGetValueIfPresent( repDict, CFSTR(kPWReplicaDNSKey), (const void **)&dns ) )
					dns = NULL;
				if ( ! CFDictionaryGetValueIfPresent( repDict, CFSTR(kPWReplicaStatusKey), (const void **)&replicaStatus ) )
					replicaStatus = NULL;
				
				// set 'em
				if ( idRangeBegin != NULL )
					CReplicaFile::AddOrReplaceValueStatic( keepRepDict, CFSTR(kPWReplicaIDRangeBeginKey), idRangeBegin );
				if ( idRangeEnd != NULL )
					CReplicaFile::AddOrReplaceValueStatic( keepRepDict, CFSTR(kPWReplicaIDRangeEndKey), idRangeEnd );
				if ( ip != NULL )
					CReplicaFile::AddOrReplaceValueStatic( keepRepDict, CFSTR(kPWReplicaIPKey), ip );
				if ( dns != NULL )
					CReplicaFile::AddOrReplaceValueStatic( keepRepDict, CFSTR(kPWReplicaDNSKey), dns );
				if ( replicaStatus != NULL )
					CReplicaFile::AddOrReplaceValueStatic( keepRepDict, CFSTR(kPWReplicaStatusKey), replicaStatus );
				
				// note that we don't update the lastSyncDate key because we want to keep *our* last sync date
				changed = true;
			}
			
			CFRelease( keepRepDict );
		}
	}
	
	if ( sinceNeverReferenceDate != NULL )
		CFRelease( sinceNeverReferenceDate );
	
	return changed;
}


//----------------------------------------------------------------------------------------------------
//	CReplicaFile
//
//	Standard constructor, loads the default replica file
//----------------------------------------------------------------------------------------------------

CReplicaFile::CReplicaFile()
{
	mReplicaDict = NULL;
	mReplicaArray = NULL;
	mDirty = false;
	mSelfName[0] = 0;
	bzero( &mReplicaFileModDate, sizeof(mReplicaFileModDate) );
	
	LoadXMLData( kPWReplicaFile );
	
	if ( mReplicaDict == NULL )
	{
		// make a new replication dictionary
		mReplicaDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	}
}


//----------------------------------------------------------------------------------------------------
//	CReplicaFile
//
//	Alternate constructor, makes a replica object from xml data.
//----------------------------------------------------------------------------------------------------

CReplicaFile::CReplicaFile( const char *xmlDataStr )
{
	CFDataRef xmlData;
	CFStringRef errorString;
	
	mReplicaDict = NULL;
	mReplicaArray = NULL;
	mDirty = false;
	mSelfName[0] = 0;
	
	if ( xmlDataStr != NULL )
	{
		xmlData = CFDataCreate( kCFAllocatorDefault, (const unsigned char *)xmlDataStr, strlen(xmlDataStr) );
		if ( xmlData != NULL )
		{
			mReplicaDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, xmlData, kCFPropertyListMutableContainersAndLeaves, &errorString );
		
			CFRelease( xmlData );
		}
	}
	
	if ( mReplicaDict == NULL )
	{
		// make a new replication dictionary
		mReplicaDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	}
}


//----------------------------------------------------------------------------------------------------
//	CReplicaFile
//
//	Alternate constructor, loads a replica file other than the default
//----------------------------------------------------------------------------------------------------

CReplicaFile::CReplicaFile( bool inLoadCustomFile, const char *inFilePath )
{
	mReplicaDict = NULL;
	mReplicaArray = NULL;
	
	if ( inLoadCustomFile )
		LoadXMLData( inFilePath );
	
	if ( mReplicaDict == NULL )
	{
		// make a new replication dictionary
		mReplicaDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	}
}


//----------------------------------------------------------------------------------------------------
//	CReplicaFile
//
//	Copy constructor
//----------------------------------------------------------------------------------------------------

CReplicaFile::CReplicaFile( const CReplicaFile &inReplicaFile )
{
	mReplicaDict = CFDictionaryCreateMutableCopy( kCFAllocatorDefault, 0, inReplicaFile.mReplicaDict );
	mReplicaArray = NULL;
	mDirty = inReplicaFile.mDirty;
	mReplicaFileModDate = inReplicaFile.mReplicaFileModDate;
	strlcpy( mSelfName, inReplicaFile.mSelfName, sizeof(mSelfName) );
}


//----------------------------------------------------------------------------------------------------
//	~CReplicaFile
//
//	Destructor
//----------------------------------------------------------------------------------------------------

CReplicaFile::~CReplicaFile()
{
	if ( mReplicaDict != NULL )
		CFRelease( mReplicaDict );
		
	// mReplicaArray is a pointer to an object in mReplicaDict
}


ReplicaPolicy
CReplicaFile::GetReplicaPolicy( void )
{
	ReplicaPolicy result = kReplicaNone;
	char statusStr[256];
	
	if ( mReplicaDict == NULL )
		return kReplicaNone;
	
	// key is "Status"
	if ( ! GetCStringFromDictionary( mReplicaDict, CFSTR("Status"), sizeof(statusStr), statusStr ) )
		return kReplicaNone;
	
	if ( strcasecmp( statusStr, kPWReplicaStatusAllow ) == 0 )
		result = kReplicaAllowAll;
	else
	if ( strcasecmp( statusStr, kPWReplicaStatusUseACL ) == 0 )
		result = kReplicaUseACL;
	
	return result;
}


void
CReplicaFile::SetReplicaPolicy( ReplicaPolicy inPolicy )
{
	char *valueStr = NULL;
	CFStringRef valString;
	
	switch( inPolicy )
	{
		case kReplicaNone:
			valueStr = "None";
			break;
			
		case kReplicaAllowAll:
			valueStr = kPWReplicaStatusAllow;
			break;
			
		case kReplicaUseACL:
			valueStr = kPWReplicaStatusUseACL;
			break;
	}
	
	if ( valueStr != NULL )
	{
		valString = CFStringCreateWithCString( kCFAllocatorDefault, valueStr, kCFStringEncodingUTF8 );
		if ( valString != NULL )
		{
			this->AddOrReplaceValue( CFSTR("Status"), valString );
			CFRelease( valString );
		}
	}
}


void
CReplicaFile::GetSelfName( char *outName )
{
	if ( outName != NULL )
		strcpy( outName, mSelfName );
}


void
CReplicaFile::SetSelfName( const char *inSelfName )
{
	if ( inSelfName != NULL )
		strlcpy( mSelfName, inSelfName, sizeof(mSelfName) );
}

		
bool
CReplicaFile::IPAddressIsInACL( UInt32 inIPAddress )
{
	CFArrayRef aclArray;
	CFStringRef aclItemString;
	char aclItem[21];
	bool result = false;
	struct in_addr ip;
	UInt32 mask = 0L;
	int maskbits;
	char *maskbitsPtr;
	CFIndex aclIndex;
	CFIndex aclCount;
	
	if ( mReplicaDict == NULL )
		return false;
	
	if ( ! CFDictionaryGetValueIfPresent( mReplicaDict, CFSTR("ACL"), (const void **)&aclArray ) )
		return false;
	
	if ( CFGetTypeID(aclArray) != CFArrayGetTypeID() )
		return false;
	
	aclCount = CFArrayGetCount( aclArray );
	for ( aclIndex = 0; aclIndex < aclCount; aclIndex++ )
	{
		aclItemString = (CFStringRef)CFArrayGetValueAtIndex( aclArray, aclIndex );
		if ( CFGetTypeID( aclItemString ) != CFStringGetTypeID() )
			continue;
		
		if ( ! CFStringGetCString( aclItemString, aclItem, sizeof(aclItem), kCFStringEncodingUTF8 ) )
			continue;
		
		//SRVLOG( kLogPOP3Chat, "aclItem=%s", aclItem );
		
		if ( aclItem[0] != '\0' )
		{
			maskbits = 32;
			maskbitsPtr = strchr( aclItem, '/' );
			if ( maskbitsPtr != NULL ) {
				sscanf( maskbitsPtr + 1, "%d", &maskbits );
				*maskbitsPtr = '\0';
			}
			
			for ( int lup = 0; lup < maskbits; lup++ )
				mask = (mask >> 1) | 0x80000000;
			
			inet_aton( aclItem, &ip );
			
			//SRVLOG( kLogPOP3Chat, "ip=%l, mask=%l", ip.s_addr, mask );
			
			if ( (inIPAddress & mask) == (ip.s_addr & mask) ) {
				result = true;
				break;
			}
		}
	}
	while ( aclItem[0] != '\0' );
	
	return result;
}


UInt32
CReplicaFile::ReplicaCount( void )
{
	UInt32 result = 0;
	
	if ( mReplicaArray == NULL )
	{
		mReplicaArray = GetArrayForKey( CFSTR(kPWReplicaReplicaKey) );
		if ( mReplicaArray == NULL )
		{
			CFDictionaryRef parentDict = this->GetParent();
			if ( parentDict != NULL )
			{
				CFMutableArrayRef theArray = NULL;
				if ( CFDictionaryGetValueIfPresent( parentDict, CFSTR(kPWReplicaReplicaKey), (const void **)&theArray ) &&
					 CFGetTypeID(theArray) == CFArrayGetTypeID() )
				{
					mReplicaArray = theArray;
				}
			}
		}
	}
	
	if ( mReplicaArray != NULL )
		result = CFArrayGetCount( mReplicaArray );
		
	return result;
}


CFDictionaryRef
CReplicaFile::GetReplica( UInt32 index )
{
	CFDictionaryRef replicaDict = NULL;
	
	if ( mReplicaArray == NULL )
		mReplicaArray = GetArrayForKey( CFSTR(kPWReplicaReplicaKey) );
	
	if ( mReplicaArray != NULL )
	{
		replicaDict = (CFDictionaryRef) CFArrayGetValueAtIndex( mReplicaArray, index );
		if ( replicaDict != NULL && CFGetTypeID(replicaDict) != CFDictionaryGetTypeID() )
			return NULL;
	}
	
	return replicaDict;
}


bool
CReplicaFile::IsActive( void )
{
	CFDataRef thingy;
	
	if ( mReplicaDict != NULL && CFDictionaryGetValueIfPresent( mReplicaDict, CFSTR("Decommission"), (const void **)&thingy ) )
		return false;
	
	return true;
}


bool
CReplicaFile::GetUniqueID( char *outIDStr )
{
	CFStringRef idString;
	
	if ( outIDStr == NULL || mReplicaDict == NULL )
		return false;
		
	*outIDStr = '\0';
	
	if ( ! CFDictionaryGetValueIfPresent( mReplicaDict, CFSTR(kPWReplicaIDKey), (const void **)&idString ) )
		return false;
	
	if ( CFGetTypeID(idString) != CFStringGetTypeID() )
		return false;
	
	return CFStringGetCString( idString, outIDStr, 33, kCFStringEncodingUTF8 );
}


CFStringRef
CReplicaFile::CurrentServerForLDAP( void )
{
	CFStringRef idString = NULL;
	
	if ( mReplicaDict != NULL &&
		 CFDictionaryGetValueIfPresent(mReplicaDict, CFSTR(kPWReplicaCurrentServerForLDAPKey), (const void **)&idString) &&
		 CFGetTypeID(idString) != CFStringGetTypeID() )
	{
		CFRetain( idString );
		return idString;
	}
	
	return NULL;
}


CFDictionaryRef
CReplicaFile::GetParent( void )
{
	CFDictionaryRef parentDict = NULL;

	if ( mReplicaDict == NULL )
		return NULL;
	
	if ( ! CFDictionaryGetValueIfPresent( mReplicaDict, CFSTR("Parent"), (const void **)&parentDict ) )
		return NULL;
	
	if ( CFGetTypeID(parentDict) != CFDictionaryGetTypeID() )
		return NULL;

	return parentDict;
}


char *
CReplicaFile::GetXMLData( void )
{
	CFDataRef xmlData = NULL;
	const UInt8 *sourcePtr;
	char *returnString = NULL;
	long length;
	
	if ( mReplicaDict == NULL )
		return NULL;
		
	xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, (CFPropertyListRef)mReplicaDict );
	if ( xmlData == NULL )
		return NULL;
	
	sourcePtr = CFDataGetBytePtr( xmlData );
	length = CFDataGetLength( xmlData );
	if ( sourcePtr != NULL && length > 0 )
	{
		returnString = (char *) malloc( length + 1 );
		if ( returnString != NULL )
		{
			memcpy( returnString, sourcePtr, length );
			returnString[length] = '\0';
		}
	}
	
	CFRelease( xmlData );
	
	return returnString;
}


bool
CReplicaFile::FileHasChanged( void )
{
	bool refresh = false;
	struct timespec modDate;
	
	refresh = ( this->StatReplicaFileAndGetModDate( kPWReplicaFile, &modDate ) != 0 );
	
	if ( !refresh && modDate.tv_sec > mReplicaFileModDate.tv_sec )
		refresh = true;
		
	if ( !refresh && modDate.tv_sec == mReplicaFileModDate.tv_sec && modDate.tv_nsec > mReplicaFileModDate.tv_nsec )
		refresh = true;
	
	return refresh;
}


//----------------------------------------------------------------------------------------------------
//	RefreshIfNeeded
//
//	Returns: void
//
//  Updates either the disk or RAM copy of the replica database to the latest version.
//  Conflicts between the two are resolved and merged.
//  WARNING: This method can flush and reload the CFDictionary. It should not be called if
//			 the calling function has already checked-out or modified elements in the dictionary.
//			 This method is used when processing signals.
//----------------------------------------------------------------------------------------------------

void
CReplicaFile::RefreshIfNeeded( void )
{
	bool refresh = false;
	CFMutableDictionaryRef lastPropertyList = mReplicaDict;
	
	refresh = this->FileHasChanged();
	
	if ( refresh && mDirty )
	{
		// need to resolve conflict between file and memory copies
		// SaveXMLData handles merging
		this->SaveXMLData();
	}
	else
	{
		if ( refresh )
		{
			if ( LoadXMLData( kPWReplicaFile ) == 0 )
			{
				CFRelease( lastPropertyList );
			}
			else
			{
				mReplicaDict = lastPropertyList;
			}
			
			// reset
			mReplicaArray = NULL;
		}
		else
		if ( mDirty )
			this->SaveXMLData();
	}
}


void
CReplicaFile::AllocateIDRange( const char *inReplicaName, UInt32 inCount )
{
	CFMutableDictionaryRef selfDict;
	CFStringRef rangeString;
	char firstID[35];
	char lastID[35];
	char myLastID[35];
	char *myLastIDPtr = NULL;
	int err;
	
	selfDict = this->GetReplicaByName( inReplicaName );
	if ( selfDict == NULL )
		return;
	
	if ( CFDictionaryGetValueIfPresent( selfDict, CFSTR(kPWReplicaIDRangeEndKey), (const void **)&rangeString ) )
		if ( CFStringGetCString( rangeString, myLastID, sizeof(myLastID), kCFStringEncodingUTF8 ) )
			myLastIDPtr = myLastID;
	
	this->GetIDRange( myLastIDPtr, inCount, firstID, lastID );
	err = this->SetIDRange( selfDict, firstID, lastID );
		
	CFRelease( selfDict );
}


void
CReplicaFile::GetIDRangeForReplica( const char *inReplicaName, UInt32 *outStart, UInt32 *outEnd )
{
	CFMutableDictionaryRef replicaDict = this->GetReplicaByName( inReplicaName );
	CFStringRef rangeString;
	PWFileEntry passRec;
	char rangeStr[256];
	
	*outStart = 0;
	*outEnd = 0;
	
	if ( replicaDict == NULL )
		return;
	
	if ( CFDictionaryGetValueIfPresent( replicaDict, CFSTR(kPWReplicaIDRangeBeginKey), (const void **)&rangeString ) &&
		 CFStringGetCString( rangeString, rangeStr, sizeof(rangeStr), kCFStringEncodingUTF8 ) &&
		 pwsf_stringToPasswordRecRef( rangeStr, &passRec ) )
	{
		*outStart = passRec.slot;
	}
	
	if ( CFDictionaryGetValueIfPresent( replicaDict, CFSTR(kPWReplicaIDRangeEndKey), (const void **)&rangeString ) &&
		 CFStringGetCString( rangeString, rangeStr, sizeof(rangeStr), kCFStringEncodingUTF8 ) &&
		 pwsf_stringToPasswordRecRef( rangeStr, &passRec ) )
	{
		*outEnd = passRec.slot;
	}
	
	CFRelease( replicaDict );
}


void
CReplicaFile::SetSyncDate( const char *inReplicaName, CFDateRef inSyncDate )
{
	CFMutableDictionaryRef repDict;
	
	repDict = this->GetReplicaByName( inReplicaName );
	if ( repDict == NULL )
		return;
	
	this->AddOrReplaceValue( repDict, CFSTR(kPWReplicaSyncDateKey), inSyncDate );
	CFRelease( repDict );
	mDirty = true;
}


//----------------------------------------------------------------------------------------------------
//	SetEntryModDate
//
// Should be updated for changes to these keys:
// IP, DNS, ReplicaPolicy, ReplicaStatus, IDRangeBegin, IDRangeEnd
//----------------------------------------------------------------------------------------------------

void
CReplicaFile::SetEntryModDate( const char *inReplicaName )
{
	CFDateRef nowDate = CFDateCreate( kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() );
	if ( nowDate != NULL ) {
		this->SetKeyWithDate( inReplicaName, CFSTR(kPWReplicaEntryModDateKey), nowDate );
		CFRelease( nowDate );
	}
}


void
CReplicaFile::SetEntryModDate( CFMutableDictionaryRef inReplicaDict )
{
	CFDateRef nowDate = CFDateCreate( kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() );
	if ( nowDate != NULL ) {
		AddOrReplaceValueStatic( inReplicaDict, CFSTR(kPWReplicaEntryModDateKey), nowDate );
		CFRelease( nowDate );
	}
}


//----------------------------------------------------------------------------------------------------
//	SetKeyWithDate
//----------------------------------------------------------------------------------------------------

void
CReplicaFile::SetKeyWithDate( const char *inReplicaName, CFStringRef inKeyString, CFDateRef inSyncDate )
{
	CFMutableDictionaryRef repDict;
	
	repDict = this->GetReplicaByName( inReplicaName );
	if ( repDict == NULL )
		return;
	
	this->AddOrReplaceValue( repDict, inKeyString, inSyncDate );
	CFRelease( repDict );
	mDirty = true;
}


//----------------------------------------------------------------------------------------------------
//	GetReplicaByName
//
//	Returns: The dictionary for the replica. The object is retained and must be released by the
//			 caller.
//----------------------------------------------------------------------------------------------------

CFMutableDictionaryRef
CReplicaFile::GetReplicaByName( const char *inReplicaName )
{
	CFMutableDictionaryRef theDict = NULL;
	
	if ( inReplicaName[0] == '\0' || strcmp( inReplicaName, "Parent" ) == 0 )
		theDict = (CFMutableDictionaryRef)this->GetParent();
	else
		theDict = FindMatchToKey( kPWReplicaNameKey, inReplicaName );
	
	if ( theDict != NULL )
		CFRetain( theDict );
	
	return theDict;
}


void
CReplicaFile::GetNameOfReplica( CFMutableDictionaryRef inReplicaDict, char *outReplicaName )
{
	CFStringRef nameString;
	
	if ( inReplicaDict == NULL || outReplicaName == NULL )
		return;
	*outReplicaName = '\0';
	
	if ( ! CFDictionaryGetValueIfPresent( inReplicaDict, CFSTR(kPWReplicaNameKey), (const void **)&nameString ) )
			return;
	
	CFStringGetCString( nameString, outReplicaName, 256, kCFStringEncodingUTF8 );
}


bool
CReplicaFile::GetNameFromIPAddress( const char *inIPAddress, char *outReplicaName )
{
	CFMutableDictionaryRef theReplica = FindMatchToKey( kPWReplicaIPKey, inIPAddress );
	CFStringRef nameString;
	bool result = false;
	
	*outReplicaName = '\0';
	
	if ( theReplica != NULL )
	{
		if ( CFDictionaryGetValueIfPresent( theReplica, CFSTR(kPWReplicaNameKey), (const void **)&nameString ) )
			result = CFStringGetCString( nameString, outReplicaName, 256, kCFStringEncodingUTF8 );
		else
			result = true;		// it's the parent
	}
	
	return result;
}


void
CReplicaFile::CalcServerUniqueID( const char *inRSAPublicKey, char *outHexHash )
{
	MD5_CTX ctx;
	unsigned char pubKeyHash[MD5_DIGEST_LENGTH];
	
	if ( inRSAPublicKey == NULL || outHexHash == NULL )
		return;
	
	MD5_Init( &ctx );
	MD5_Update( &ctx, (unsigned char *)inRSAPublicKey, strlen(inRSAPublicKey) );
	MD5_Final( pubKeyHash, &ctx );
	
	outHexHash[0] = 0;
	ConvertBinaryToHex( pubKeyHash, MD5_DIGEST_LENGTH, outHexHash );
}

		
void
CReplicaFile::AddServerUniqueID( const char *inRSAPublicKey )
{
	char pubKeyHexHash[MD5_DIGEST_LENGTH*2 + 1];
	CFStringRef idString;
	
	if ( inRSAPublicKey == NULL || mReplicaDict == NULL )
		return;
	
	// the ID never changes, so return if present
	if ( CFDictionaryContainsKey( mReplicaDict, CFSTR("ID") ) )
		return;
	
	this->CalcServerUniqueID( inRSAPublicKey, pubKeyHexHash );
	
	idString = CFStringCreateWithCString( kCFAllocatorDefault, pubKeyHexHash, kCFStringEncodingUTF8 );
	if ( idString != NULL )
	{
		CFDictionaryAddValue( mReplicaDict, CFSTR("ID"), idString );
		mDirty = true;
	}
}


void
CReplicaFile::SetParent( const char *inIPStr, const char *inDNSStr )
{
	CFMutableDictionaryRef parentData;
	CFStringRef ipString;
	CFStringRef dnsString = NULL;
	
	if ( inIPStr == NULL )
		return;
	
	parentData = CFDictionaryCreateMutable( kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	if ( parentData != NULL )
	{
		ipString = CFStringCreateWithCString( kCFAllocatorDefault, inIPStr, kCFStringEncodingUTF8 );
		if ( ipString != NULL ) {
			CFDictionaryAddValue( parentData, CFSTR(kPWReplicaIPKey), ipString );
			CFRelease( ipString );
			ipString = NULL;
		}
		
		if ( inDNSStr != NULL )
		{
			dnsString = CFStringCreateWithCString( kCFAllocatorDefault, inDNSStr, kCFStringEncodingUTF8 );
			if ( dnsString != NULL ) {
			CFDictionaryAddValue( parentData, CFSTR(kPWReplicaDNSKey), dnsString );
				CFRelease( dnsString );
				dnsString = NULL;
			}
		}
		
		this->SetParent( parentData );
	}
}


void
CReplicaFile::SetParent( CFDictionaryRef inParentData )
{
	if ( inParentData == NULL )
		return;
	
	this->AddOrReplaceValue( mReplicaDict, CFSTR(kPWReplicaParentKey), inParentData );
	
	// need to have the parent inserted before calling AllocateIDRange()
	this->AllocateIDRange( "", 500 );
	
	mDirty = true;
}


CFMutableDictionaryRef
CReplicaFile::AddReplica( const char *inIPStr, const char *inDNSStr )
{
	CFMutableDictionaryRef replicaData = NULL;
	CFStringRef dnsString = NULL;
	
	if ( inIPStr == NULL )
		return NULL;
	
	replicaData = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	if ( replicaData != NULL )
	{
		this->AddIPAddress( replicaData, inIPStr );
		
		if ( inDNSStr != NULL )
			dnsString = CFStringCreateWithCString( kCFAllocatorDefault, inDNSStr, kCFStringEncodingUTF8 );
		if ( dnsString != NULL ) {
			CFDictionaryAddValue( replicaData, CFSTR(kPWReplicaDNSKey), dnsString );
			CFRelease( dnsString );
			dnsString = NULL;
		}
			
		replicaData = this->AddReplica( replicaData );
	}
	
	return replicaData;
}


CFMutableDictionaryRef
CReplicaFile::AddReplica( CFMutableDictionaryRef inReplicaData )
{
	CFMutableArrayRef replicaArray = NULL;
	CFStringRef ipString = NULL;
	CFStringRef nameString = NULL;
	CFStringRef replicaNameString = NULL;
	char replicaNameStr[256];
	
	if ( inReplicaData == NULL || mReplicaDict == NULL )
		return NULL;
	
	if ( ! CFDictionaryGetValueIfPresent( inReplicaData, CFSTR(kPWReplicaIPKey), (const void **)&ipString ) )
		return NULL;
	if ( ! CFDictionaryGetValueIfPresent( inReplicaData, CFSTR(kPWReplicaNameKey), (const void **)&nameString ) )
		nameString = NULL;
	
	replicaArray = this->GetArrayForKey( CFSTR(kPWReplicaReplicaKey) );
	if ( replicaArray == NULL )
	{
		replicaArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		if ( replicaArray == NULL )
			return NULL;
		
		CFDictionaryAddValue( mReplicaDict, CFSTR(kPWReplicaReplicaKey), replicaArray );
	}
	else
	if ( nameString != NULL )
	{
		// don't add duplicates
		
		CFIndex repIndex, repCount;
		CFDictionaryRef replicaRef;
		
		repCount = CFArrayGetCount( replicaArray );
		for ( repIndex = 0; repIndex < repCount; repIndex++ )
		{
			replicaRef = this->GetReplica( repIndex );
			if ( replicaRef == NULL )
				continue;
			
			if ( CFDictionaryGetValueIfPresent( replicaRef, CFSTR(kPWReplicaNameKey), (const void **)&replicaNameString ) )
			{
				if ( CFStringCompare( nameString, replicaNameString, (CFOptionFlags)0 ) == kCFCompareEqualTo )
				{
					// remove the old copy and refresh
					CFArrayRemoveValueAtIndex( replicaArray, repIndex );
					break;
				}
			}
		}
	}
	
	// has this replica been named?
	if ( nameString == NULL )
	{
		// add the Replica Name
		this->GetNextReplicaName( replicaNameStr );
		replicaNameString = CFStringCreateWithCString( kCFAllocatorDefault, replicaNameStr, kCFStringEncodingUTF8 );
		if ( replicaNameString != NULL )
			CFDictionaryAddValue( inReplicaData, CFSTR(kPWReplicaNameKey), replicaNameString );
	}
	
	// add to the list of replicas
	CFArrayAppendValue( replicaArray, inReplicaData );
	mDirty = true;
	
	// add an ID range
	this->AllocateIDRange( replicaNameStr, 500 );
	
	return (CFMutableDictionaryRef)inReplicaData;
}


//----------------------------------------------------------------------------------------------------
//	AddIPAddress
//
//	Returns: TRUE if the address was added, FALSE if not (duplicate or error)
//----------------------------------------------------------------------------------------------------

bool
CReplicaFile::AddIPAddress( CFMutableDictionaryRef inReplicaData, const char *inIPStr )
{
	CFTypeRef valueRef;
	CFStringRef ipString;
	CFMutableArrayRef arrayRef;
	bool result = false;
	
	if ( inReplicaData == NULL || inIPStr == NULL )
		return result;
	
	ipString = CFStringCreateWithCString( kCFAllocatorDefault, inIPStr, kCFStringEncodingUTF8 );
	if ( ipString == NULL )
		return result;
	
	if ( CFDictionaryGetValueIfPresent( inReplicaData, CFSTR(kPWReplicaIPKey), &valueRef ) )
	{
		if ( CFGetTypeID(valueRef) == CFArrayGetTypeID() )
		{
			arrayRef = (CFMutableArrayRef) valueRef;
			// Note: header says range should be (0,N-1), but reality is (0,N).
			if ( ! CFArrayContainsValue( arrayRef, CFRangeMake(0, CFArrayGetCount(arrayRef)), ipString ) )
			{
				CFArrayAppendValue( arrayRef, ipString );
				result = true;
			}
		}
		else
		if ( CFGetTypeID(valueRef) == CFStringGetTypeID() &&
			 CFStringCompare( (CFStringRef)valueRef, ipString, 0 ) != kCFCompareEqualTo )
		{
			arrayRef = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
			if ( arrayRef != NULL )
			{
				CFArrayAppendValue( arrayRef, valueRef );
				CFArrayAppendValue( arrayRef, ipString );
				CFDictionaryReplaceValue( inReplicaData, CFSTR(kPWReplicaIPKey), arrayRef );
				CFRelease( arrayRef );
				result = true;
			}
		}
	}
	else
	{
		CFDictionaryAddValue( inReplicaData, CFSTR(kPWReplicaIPKey), ipString );
		result = true;
	}
	
	CFRelease( ipString );
	
	if ( result )
		this->SetEntryModDate( inReplicaData );
			
	return result;
}


//----------------------------------------------------------------------------------------------------
//	ReplaceOrAddIPAddress
//
//	Removes an old IP address if found. Always adds the new address whether the old one was present
//	or not.
//----------------------------------------------------------------------------------------------------

void
CReplicaFile::ReplaceOrAddIPAddress( CFMutableDictionaryRef inReplicaData, const char *inOldIPStr, const char *inNewIPStr )
{
	CFTypeRef valueRef;
	CFStringRef oldIPString;
	CFStringRef newIPString;
	CFMutableArrayRef arrayRef;
	CFIndex firstIndex;
	
	if ( inReplicaData == NULL || inOldIPStr == NULL || inNewIPStr == NULL )
		return;
	
	oldIPString = CFStringCreateWithCString( kCFAllocatorDefault, inOldIPStr, kCFStringEncodingUTF8 );
	if ( oldIPString == NULL )
		return;
	
	if ( CFDictionaryGetValueIfPresent( inReplicaData, CFSTR(kPWReplicaIPKey), &valueRef ) )
	{
		if ( CFGetTypeID(valueRef) == CFArrayGetTypeID() )
		{
			arrayRef = (CFMutableArrayRef) valueRef;
			// Note: header says range should be (0,N-1), but reality is (0,N).
			
			firstIndex = CFArrayGetFirstIndexOfValue( arrayRef, CFRangeMake(0, CFArrayGetCount(arrayRef)), oldIPString );
			if ( firstIndex != kCFNotFound )
				CFArrayRemoveValueAtIndex( arrayRef, firstIndex );
			
			this->AddIPAddress( inReplicaData, inNewIPStr );
		}
		else
		if ( CFGetTypeID(valueRef) == CFStringGetTypeID() )
		{
			if ( CFStringCompare( (CFStringRef)valueRef, oldIPString, 0 ) == kCFCompareEqualTo )
			{
				newIPString = CFStringCreateWithCString( kCFAllocatorDefault, inNewIPStr, kCFStringEncodingUTF8 );
				if ( newIPString == NULL )
					return;
				
				CFDictionaryReplaceValue( inReplicaData, CFSTR(kPWReplicaIPKey), newIPString );
				CFRelease( newIPString );
				this->SetEntryModDate( inReplicaData );
			}
			else
			{
				this->AddIPAddress( inReplicaData, inNewIPStr );
			}
		}
	}
	else
	{
		this->AddIPAddress( inReplicaData, inNewIPStr );
	}
	
	CFRelease( oldIPString );
}


CFMutableArrayRef
CReplicaFile::GetIPAddresses( CFMutableDictionaryRef inReplicaData )
{
	CFTypeRef valueRef;
	CFMutableArrayRef arrayRef;
	
	if ( inReplicaData == NULL )
		return NULL;
	
	if ( CFDictionaryGetValueIfPresent( inReplicaData, CFSTR(kPWReplicaIPKey), &valueRef ) )
	{
		if ( CFGetTypeID(valueRef) == CFArrayGetTypeID() )
		{
			CFRetain( (CFMutableArrayRef) valueRef );
			return (CFMutableArrayRef) valueRef;
		}
		else
		if ( CFGetTypeID(valueRef) == CFStringGetTypeID() )
		{
			arrayRef = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
			if ( arrayRef != NULL )
			{
				CFArrayAppendValue( arrayRef, valueRef );
				return arrayRef;
			}
		}
	}
	
	return NULL;
}


bool
CReplicaFile::GetCStringFromDictionary( CFDictionaryRef inDict, CFStringRef inKey, long inMaxLen, char *outString )
{
	CFStringRef idString;
	
	if ( inDict == NULL )
		return false;
	
	*outString = '\0';
	
	if ( ! CFDictionaryGetValueIfPresent( inDict, inKey, (const void **)&idString ) )
		return false;
	
	if ( CFGetTypeID(idString) != CFStringGetTypeID() )
		return false;
	
	return CFStringGetCString( idString, outString, inMaxLen, kCFStringEncodingUTF8 );
}


void
CReplicaFile::AddOrReplaceValue( CFStringRef inKey, CFStringRef inValue )
{
	this->AddOrReplaceValue( mReplicaDict, inKey, inValue );
	mDirty = true;
}


void
CReplicaFile::AddOrReplaceValue( CFMutableDictionaryRef inDict, CFStringRef inKey, CFTypeRef inValue )
{
	AddOrReplaceValueStatic( inDict, inKey, inValue );
}


void
CReplicaFile::AddOrReplaceValueStatic( CFMutableDictionaryRef inDict, CFStringRef inKey, CFTypeRef inValue )
{
	if ( inDict == NULL || inKey == NULL )
		return;
	
	if ( CFDictionaryContainsKey( inDict, inKey ) )
	{
		CFDictionaryReplaceValue( inDict, inKey, inValue );
	}
	else
	{
		CFDictionaryAddValue( inDict, inKey, inValue );
	}
}


int
CReplicaFile::StatReplicaFileAndGetModDate( const char *inFilePath, struct timespec *outModDate )
{
	struct stat sb;
	int result;
	
	if ( outModDate != NULL ) {
		outModDate->tv_sec = 0;
		outModDate->tv_nsec = 0;
	}
	
	result = lstat( inFilePath, &sb );
	if ( result == 0 && outModDate != NULL )
		*outModDate = sb.st_mtimespec;
		
	return result;
}


//----------------------------------------------------------------------------------------------------
//  LoadXMLData
//
//  Returns: -1 = error, 0 = ok.
//----------------------------------------------------------------------------------------------------

int
CReplicaFile::LoadXMLData( const char *inFilePath )
{
	CFStringRef myReplicaDataFilePathRef;
	CFURLRef myReplicaDataFileRef;
	CFReadStreamRef myReadStreamRef;
	CFPropertyListRef myPropertyListRef = NULL;
	CFStringRef errorString = NULL;
	CFPropertyListFormat myPLFormat;
	struct timespec modDate;
	
	if ( this->StatReplicaFileAndGetModDate( inFilePath, &modDate ) != 0 )
		return -1;
	
	myReplicaDataFilePathRef = CFStringCreateWithCString( kCFAllocatorDefault, inFilePath, kCFStringEncodingUTF8 );
	if ( myReplicaDataFilePathRef == NULL )
		return -1;
	
	myReplicaDataFileRef = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, myReplicaDataFilePathRef, kCFURLPOSIXPathStyle, false );
	
	CFRelease( myReplicaDataFilePathRef );
	
	if ( myReplicaDataFileRef == NULL )
		return -1;
	
	myReadStreamRef = CFReadStreamCreateWithFile( kCFAllocatorDefault, myReplicaDataFileRef );
	
	CFRelease( myReplicaDataFileRef );
	
	if ( myReadStreamRef == NULL )
		return -1;
	
	if ( CFReadStreamOpen( myReadStreamRef ) )
	{
		myPLFormat = kCFPropertyListXMLFormat_v1_0;
		myPropertyListRef = CFPropertyListCreateFromStream( kCFAllocatorDefault, myReadStreamRef, 0, kCFPropertyListMutableContainersAndLeaves, &myPLFormat, &errorString );
		CFReadStreamClose( myReadStreamRef );
	}
	CFRelease( myReadStreamRef );
	
	if ( errorString != NULL )
	{
		char errMsg[256];
		
		if ( CFStringGetCString( errorString, errMsg, sizeof(errMsg), kCFStringEncodingUTF8 ) )
			errmsg( "could not load the replica file, error = %s", errMsg );
		CFRelease( errorString );
	}
	
	if ( myPropertyListRef == NULL )
	{
		errmsg( "could not load the replica file." );
		return -1;
	}
	
	if ( CFGetTypeID(myPropertyListRef) != CFDictionaryGetTypeID() )
	{
		CFRelease( myPropertyListRef );
		errmsg( "could not load the replica file because the property list is not a dictionary." );
		return -1;
	}
	
	mReplicaDict = (CFMutableDictionaryRef)myPropertyListRef;
	mReplicaFileModDate = modDate;
	
	return 0;
}


//----------------------------------------------------------------------------------------------------
//  SaveXMLData
//
//  Returns: -1 = error, 0 = ok.
//----------------------------------------------------------------------------------------------------

int
CReplicaFile::SaveXMLData( void )
{
	int result = this->SaveXMLData( (CFPropertyListRef) mReplicaDict, kPWReplicaFile );
	if ( result == 0 )
	{
		this->StatReplicaFileAndGetModDate( kPWReplicaFile, &mReplicaFileModDate );
		mDirty = false;
	}
	
	return result;
}


//----------------------------------------------------------------------------------------------------
//  SaveXMLData
//
//  Returns: -1 = error, 0 = ok.
//
//	Saves the replica file to an alternate location and does not clear the "dirty" flag.
//----------------------------------------------------------------------------------------------------

int
CReplicaFile::SaveXMLData( const char *inSaveFile )
{
	return this->SaveXMLData( (CFPropertyListRef) mReplicaDict, inSaveFile );
}


//----------------------------------------------------------------------------------------------------
//  SaveXMLData
//
//  Returns: -1 = error, 0 = ok.
//----------------------------------------------------------------------------------------------------

int
CReplicaFile::SaveXMLData( CFPropertyListRef inListToWrite, const char *inSaveFile )
{
	CFStringRef myReplicaDataFilePathRef;
	CFURLRef myReplicaDataFileRef;
	CFWriteStreamRef myWriteStreamRef;
	CFStringRef errorString;
	int err;
    struct stat sb;
	
	// make sure the directory path exists
	char *dirPath = strdup( inSaveFile );
	if ( dirPath == NULL )
		return -1;
	
	char *pos = rindex( dirPath, '/' );
	if ( pos != NULL ) 
		*pos = '\0';
	
	err = lstat( dirPath, &sb );
	if ( err != 0 ) {
		err = pwsf_mkdir_p( dirPath, S_IRWXU );
		err = lstat( dirPath, &sb );
	}
	
	free( dirPath );
	
	if ( err != 0 )
		return -1;
	
	// write the file
	myReplicaDataFilePathRef = CFStringCreateWithCString( kCFAllocatorDefault, inSaveFile, kCFStringEncodingUTF8 );
	if ( myReplicaDataFilePathRef == NULL )
		return -1;
	
	myReplicaDataFileRef = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, myReplicaDataFilePathRef, kCFURLPOSIXPathStyle, false );
	
	CFRelease( myReplicaDataFilePathRef );
	
	if ( myReplicaDataFileRef == NULL )
		return -1;
	
	myWriteStreamRef = CFWriteStreamCreateWithFile( kCFAllocatorDefault, myReplicaDataFileRef );
	
	CFRelease( myReplicaDataFileRef );
	
	if ( myWriteStreamRef == NULL )
		return -1;
	
	if ( ! CFWriteStreamOpen( myWriteStreamRef ) )
	{
		CFRelease( myWriteStreamRef );
		return -1;
	}
	
	errorString = NULL;
	CFPropertyListWriteToStream( inListToWrite, myWriteStreamRef, kCFPropertyListXMLFormat_v1_0, &errorString );
	
	CFWriteStreamClose( myWriteStreamRef );
	CFRelease( myWriteStreamRef );
	
	if ( errorString != NULL )
	{
		char errMsg[256];
		
		if ( CFStringGetCString( errorString, errMsg, sizeof(errMsg), kCFStringEncodingUTF8 ) )
			errmsg( "could not save the replica file, error = %s", errMsg );
		CFRelease( errorString );
	}
	
	return 0;
}


void
CReplicaFile::GetNextReplicaName( char *outName )
{
	UInt32 repIndex;
	UInt32 repCount = this->ReplicaCount();
	CFDictionaryRef curReplica;
	CFStringRef curNameString;
	int replicaNameValuePrefixLen;
	int tempReplicaNumber = 0, nextReplicaNumber = 1;
	const int replicaPrefixLen = strlen(kPWReplicaNameValuePrefix);
	char replicaNameStr[256];
	
	replicaNameValuePrefixLen = strlen( kPWReplicaNameValuePrefix );
	
	for ( repIndex = 0; repIndex < repCount; repIndex++ )
	{
		curReplica = this->GetReplica( repIndex );
		if ( curReplica == NULL )
			continue;
			
		if ( ! CFDictionaryGetValueIfPresent( curReplica, CFSTR(kPWReplicaNameKey), (const void **)&curNameString ) )
			continue;
		
		if ( ! CFStringGetCString( curNameString, replicaNameStr, sizeof(replicaNameStr), kCFStringEncodingUTF8 ) )
			continue;
		
		if ( strncmp( replicaNameStr, kPWReplicaNameValuePrefix, replicaNameValuePrefixLen ) != 0 )
			continue;
		
		sscanf( replicaNameStr + replicaNameValuePrefixLen, "%d", &tempReplicaNumber );
		if ( tempReplicaNumber >= nextReplicaNumber )
			nextReplicaNumber = tempReplicaNumber + 1;
	}
	
	sprintf( outName, "%s%d", kPWReplicaNameValuePrefix, nextReplicaNumber );
	
	// if making a replica of a replica, add the other name to avoid collisions
	if ( strncmp( mSelfName, kPWReplicaNameValuePrefix, replicaPrefixLen ) == 0 ) {
		strcat( outName, "." );
		strcat( outName, mSelfName + replicaPrefixLen );
	}
}


CFMutableArrayRef
CReplicaFile::GetArrayForKey( CFStringRef key )
{
	CFMutableArrayRef theArray = NULL;
	
	if ( mReplicaDict == NULL )
		return NULL;
	
	if ( ! CFDictionaryGetValueIfPresent( mReplicaDict, key, (const void **)&theArray ) )
		return NULL;
	
	if ( CFGetTypeID(theArray) != CFArrayGetTypeID() )
		return NULL;
	
	return theArray;
}


void
CReplicaFile::SortReplicas( void )
{
}


//----------------------------------------------------------------------------------------------------
//	StripSyncDates
//----------------------------------------------------------------------------------------------------

void
CReplicaFile::StripSyncDates( void )
{
	UInt32 repIndex;
	UInt32 repCount = this->ReplicaCount();
	CFMutableDictionaryRef curReplica;
	CFDateRef nowDate = CFDateCreate( kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() );
	
	// strip parent
	curReplica = (CFMutableDictionaryRef) this->GetParent();
	if ( curReplica != NULL )
	{
		CFDictionaryRemoveValue( curReplica, CFSTR(kPWReplicaSyncDateKey) );
		this->AddOrReplaceValue( curReplica, CFSTR(kPWReplicaEntryModDateKey), nowDate );
	}
	
	// strip replicas
	for ( repIndex = 0; repIndex < repCount; repIndex++ )
	{
		curReplica = (CFMutableDictionaryRef) this->GetReplica( repIndex );
		if ( curReplica == NULL )
			continue;
		
		CFDictionaryRemoveValue( curReplica, CFSTR(kPWReplicaSyncDateKey) );
		this->AddOrReplaceValue( curReplica, CFSTR(kPWReplicaEntryModDateKey), nowDate );
	}
	
	if ( nowDate != NULL )
		CFRelease( nowDate );
	
	mDirty = true;
}


//----------------------------------------------------------------------------------------------------
//	GetReplicaSyncPolicy
//----------------------------------------------------------------------------------------------------

UInt8
CReplicaFile::GetReplicaSyncPolicy( CFDictionaryRef inReplicaDict )
{
	return this->GetReplicaSyncPolicy( inReplicaDict, kReplicaSyncAnytime );
}


UInt8
CReplicaFile::GetReplicaSyncPolicy( CFDictionaryRef inReplicaDict, UInt8 inDefaultPolicy )
{
	char valueStr[256];
	UInt8 returnValue = inDefaultPolicy;
	
	if ( this->GetCStringFromDictionary( inReplicaDict, CFSTR(kPWReplicaPolicyKey), sizeof(valueStr), valueStr ) )
	{
		if ( strcmp( valueStr, kPWReplicaPolicyNeverKey ) == 0 )
			returnValue = kReplicaSyncNever;
		else
		if ( strcmp( valueStr, kPWReplicaPolicyOnlyIfDesperateKey ) == 0 )
			returnValue = kReplicaSyncOnlyIfDesperate;
		else
		if ( strcmp( valueStr, kPWReplicaPolicyOnScheduleKey ) == 0 )
			returnValue = kReplicaSyncOnSchedule;
		else
		if ( strcmp( valueStr, kPWReplicaPolicyOnDirtyKey ) == 0 )
			returnValue = kReplicaSyncOnDirty;
		else
		if ( strcmp( valueStr, kPWReplicaPolicyAnytimeKey ) == 0 )
			returnValue = kReplicaSyncAnytime;
	}
	
	return returnValue;
}


//----------------------------------------------------------------------------------------------------
//	SetReplicaSyncPolicy
//----------------------------------------------------------------------------------------------------

void
CReplicaFile::SetReplicaSyncPolicy( const char *inReplicaName, UInt8 inPolicy )
{
	CFMutableDictionaryRef repDict;
	CFStringRef policyString = NULL;
	
	repDict = this->GetReplicaByName( inReplicaName );
	if ( repDict == NULL )
		return;
	
	switch( inPolicy )
	{
		case kReplicaSyncNever:
			policyString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaPolicyNeverKey, kCFStringEncodingUTF8 );
			break;
		case kReplicaSyncOnlyIfDesperate:
			policyString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaPolicyOnlyIfDesperateKey, kCFStringEncodingUTF8 );
			break;
		case kReplicaSyncOnSchedule:
			policyString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaPolicyOnScheduleKey, kCFStringEncodingUTF8 );
			break;
		case kReplicaSyncOnDirty:
			policyString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaPolicyOnDirtyKey, kCFStringEncodingUTF8 );
			break;
		case kReplicaSyncAnytime:
			policyString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaPolicyAnytimeKey, kCFStringEncodingUTF8 );
			break;
	}
	
	if ( policyString != NULL )
	{
		bool result = SetReplicaSyncPolicy( repDict, policyString );
		CFRelease( policyString );
		if ( result )
			mDirty = true;
	}
	
	CFRelease( repDict );
}


//----------------------------------------------------------------------------------------------------
//	SetReplicaSyncPolicy
//
//  Returns: TRUE if set
//----------------------------------------------------------------------------------------------------

bool
CReplicaFile::SetReplicaSyncPolicy( CFMutableDictionaryRef inRepDict, CFStringRef inPolicyString )
{
	if ( inRepDict == NULL || inPolicyString == NULL )
		return false;

	this->AddOrReplaceValue( inRepDict, CFSTR(kPWReplicaPolicyKey), inPolicyString );
	this->SetEntryModDate( inRepDict );
	mDirty = true;
	
	return true;
}


//----------------------------------------------------------------------------------------------------
//	GetReplicaStatus
//----------------------------------------------------------------------------------------------------

ReplicaStatus
CReplicaFile::GetReplicaStatus( CFDictionaryRef inReplicaDict )
{
	char valueStr[256];
	UInt8 returnValue = kReplicaActive;
	
	if ( this->GetCStringFromDictionary( inReplicaDict, CFSTR(kPWReplicaStatusKey), sizeof(valueStr), valueStr ) )
	{
		if ( strcmp( valueStr, kPWReplicaStatusActiveValue ) == 0 )
			returnValue = kReplicaActive;
		else
		if ( strcmp( valueStr, kPWReplicaStatusPermDenyValue ) == 0 )
			returnValue = kReplicaPermissionDenied;
		else
		if ( strcmp( valueStr, kPWReplicaStatusNotFoundValue ) == 0 )
			returnValue = kReplicaNotFound;
	}
	
	return returnValue;
}


//----------------------------------------------------------------------------------------------------
//	SetReplicaStatus
//----------------------------------------------------------------------------------------------------

void
CReplicaFile::SetReplicaStatus( CFMutableDictionaryRef repDict, ReplicaStatus inStatus )
{
	CFStringRef statusString = NULL;
	
	if ( repDict == NULL )
		return;
	
	switch( inStatus )
	{
		case kReplicaActive:
			statusString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaStatusActiveValue, kCFStringEncodingUTF8 );
			break;
		case kReplicaPermissionDenied:
			statusString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaStatusPermDenyValue, kCFStringEncodingUTF8 );
			break;
		case kReplicaNotFound:
			statusString = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaStatusNotFoundValue, kCFStringEncodingUTF8 );
			break;
	}
	
	if ( statusString != NULL )
	{
		this->AddOrReplaceValue( repDict, CFSTR(kPWReplicaStatusKey), statusString );
		CFRelease( statusString );
		this->SetEntryModDate( repDict );
		mDirty = true;
	}
}


void
CReplicaFile::GetIDRange( const char *inMyLastID, UInt32 inCount, char *outFirstID, char *outLastID )
{
	PWFileEntry passRec, endPassRec;
	CFDictionaryRef replicaDict;
	CFStringRef rangeString;
	UInt32 repIndex, repCount = this->ReplicaCount();
	char rangeStr[35];
	
	bzero( &passRec, sizeof(PWFileEntry) );
	bzero( &endPassRec, sizeof(PWFileEntry) );
	
	endPassRec.slot = 0;
	
	if ( inMyLastID != NULL )
	{
		pwsf_stringToPasswordRecRef( inMyLastID, &passRec );
		endPassRec.slot = passRec.slot;
	}
	
	for ( repIndex = 0; repIndex < repCount; repIndex++ )
	{
		replicaDict = this->GetReplica( repIndex );
		if ( replicaDict == NULL )
			continue;
		
		if ( ! CFDictionaryGetValueIfPresent( replicaDict, CFSTR(kPWReplicaIDRangeEndKey), (const void **)&rangeString ) )
			continue;
	
		if ( ! CFStringGetCString( rangeString, rangeStr, sizeof(rangeStr), kCFStringEncodingUTF8 ) )
			continue;
		
		if ( pwsf_stringToPasswordRecRef( rangeStr, &passRec ) == 0 )
			continue;
		
		if ( passRec.slot > endPassRec.slot )
			endPassRec.slot = passRec.slot;
	}
	
	replicaDict = this->GetParent();
	if ( replicaDict != NULL )
	{
		if ( CFDictionaryGetValueIfPresent( replicaDict, CFSTR(kPWReplicaIDRangeEndKey), (const void **)&rangeString ) &&
			 CFStringGetCString( rangeString, rangeStr, sizeof(rangeStr), kCFStringEncodingUTF8 ) &&
			 pwsf_stringToPasswordRecRef( rangeStr, &passRec ) == 1
		   )
		{
			if ( passRec.slot > endPassRec.slot )
				endPassRec.slot = passRec.slot;		
		}
	}
	
	endPassRec.slot += (endPassRec.slot > 0) ? 20 : 1;
	pwsf_passwordRecRefToString( &endPassRec, outFirstID );
	
	endPassRec.slot += inCount;
	pwsf_passwordRecRefToString( &endPassRec, outLastID );
}


//----------------------------------------------------------------------------------------------------
//	SetIDRange
//----------------------------------------------------------------------------------------------------

int
CReplicaFile::SetIDRange( CFMutableDictionaryRef inServerDict, const char *inFirstID, const char *inLastID )
{
	CFStringRef rangeString;
	
	rangeString = CFStringCreateWithCString( kCFAllocatorDefault, inFirstID, kCFStringEncodingUTF8 );
	this->AddOrReplaceValue( inServerDict, CFSTR(kPWReplicaIDRangeBeginKey), rangeString );
	CFRelease( rangeString );
	
	rangeString = CFStringCreateWithCString( kCFAllocatorDefault, inLastID, kCFStringEncodingUTF8 );
	this->AddOrReplaceValue( inServerDict, CFSTR(kPWReplicaIDRangeEndKey), rangeString );
	CFRelease( rangeString );
	
	return 0;
}


//----------------------------------------------------------------------------------------------------
//	FindMatchToKey
//
//	Returns the first replica that matches a key/value pair
//----------------------------------------------------------------------------------------------------

CFMutableDictionaryRef
CReplicaFile::FindMatchToKey( const char *inKey, const char *inValue )
{
	CFMutableDictionaryRef theDict = NULL;
	CFStringRef keyString;
	CFStringRef valueString;
	CFTypeRef evalCFType;
	UInt32 repIndex, repCount;
	bool found = false;
	
	keyString = CFStringCreateWithCString( kCFAllocatorDefault, inKey, kCFStringEncodingUTF8 );
	if ( keyString == NULL )
		return NULL;
	
	valueString = CFStringCreateWithCString( kCFAllocatorDefault, inValue, kCFStringEncodingUTF8 );
	if ( valueString == NULL )
	{
		CFRelease( keyString );
		return NULL;
	}
	
	theDict = (CFMutableDictionaryRef)this->GetParent();
	if ( theDict != NULL )
	{
		if ( CFDictionaryGetValueIfPresent( theDict, keyString, (const void **)&evalCFType ) )
		{
			if ( CFGetTypeID(evalCFType) == CFStringGetTypeID() &&
				 CFStringCompare(valueString, (CFStringRef)evalCFType, 0) == kCFCompareEqualTo )
			{
				found = true;
			}
			else
			if ( CFGetTypeID(evalCFType) == CFArrayGetTypeID() )
			{
				if ( CFArrayContainsValue((CFArrayRef)evalCFType, CFRangeMake(0, CFArrayGetCount((CFArrayRef)evalCFType) - 1), valueString) )
				found = true;
			}
		}
	}
	
	if ( !found )
	{
		// clear the parent dictionary
		theDict = NULL;
		
		repCount = this->ReplicaCount();
		for ( repIndex = 0; repIndex < repCount; repIndex++ )
		{
			theDict = (CFMutableDictionaryRef)this->GetReplica( repIndex );
			if ( theDict == NULL )
				break;
			
			if ( ! CFDictionaryGetValueIfPresent( theDict, keyString, (const void **)&evalCFType ) )
				continue;
		
			if ( CFGetTypeID(evalCFType) == CFStringGetTypeID() &&
				 CFStringCompare(valueString, (CFStringRef)evalCFType, 0) == kCFCompareEqualTo )
			{
				break;
			}
			else
			if ( CFGetTypeID(evalCFType) == CFArrayGetTypeID() )
			{
				if ( CFArrayContainsValue((CFArrayRef)evalCFType, CFRangeMake(0, CFArrayGetCount((CFArrayRef)evalCFType) - 1), valueString) )
				break;
			}
			
			theDict = NULL;
		}
	}
	
	CFRelease( keyString );
	CFRelease( valueString );
		
	return theDict;
}


bool CReplicaFile::Dirty( void )
{
	return mDirty;
}


void CReplicaFile::SetDirty( bool inDirty )
{
	mDirty = inDirty;
}


//-----------------------------------------------------------------------------
//	ConvertBinaryToHex
//-----------------------------------------------------------------------------

bool ConvertBinaryToHex( const unsigned char *inData, long len, char *outHexStr )
{
    bool result = true;
	char *tptr = outHexStr;
	char base16table[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
	
    if ( inData == nil || outHexStr == nil )
        return false;
    
	for ( int idx = 0; idx < len; idx++ )
	{
		*tptr++ = base16table[(inData[idx] >> 4) & 0x0F];
		*tptr++ = base16table[(inData[idx] & 0x0F)];
	}
	*tptr = '\0';
		
	return result;
}

#pragma mark -
#pragma mark C API
#pragma mark -

void pwsf_AddReplicaStatus( CReplicaFile *inReplicaFile, CFDictionaryRef inDict, CFMutableDictionaryRef inOutDict )
{
	CFArrayRef ipArray;
	CFStringRef nameString;
	CFStringRef valueString = NULL;
	ReplicaStatus replicaStatus;
	CFIndex index, count;
	
	ipArray = inReplicaFile->GetIPAddresses( (CFMutableDictionaryRef)inDict );
	if ( ipArray == NULL )
		return;
	
	replicaStatus = inReplicaFile->GetReplicaStatus( inDict );
	if ( replicaStatus == kReplicaActive )
	{
		// not so fast, make sure it's syncing too
		CFDateRef lastSyncDate = NULL;
		CFDateRef lastFailedDate = NULL;
		
		bool hasLastSyncDate = ( CFDictionaryGetValueIfPresent( inDict, CFSTR(kPWReplicaSyncDateKey), (const void **)&lastSyncDate ) && CFGetTypeID(lastSyncDate) == CFDateGetTypeID() );
		bool hasFailedSyncDate = ( CFDictionaryGetValueIfPresent( inDict, CFSTR(kPWReplicaSyncAttemptKey), (const void **)&lastFailedDate ) && CFGetTypeID(lastFailedDate) == CFDateGetTypeID() );
		if ( hasFailedSyncDate && hasLastSyncDate )
		{
			if ( CFDateCompare(lastFailedDate, lastSyncDate, NULL) == kCFCompareGreaterThan )
			{
				// last sync was not successful but previous sessions were successful
				valueString = CFStringCreateWithCString( kCFAllocatorDefault, "Warning: The most recent replication failed.", kCFStringEncodingUTF8 );
			}
		}
		else
		if ( hasFailedSyncDate && (!hasLastSyncDate) )
		{
			// has failed all attempts
			valueString = CFStringCreateWithCString( kCFAllocatorDefault, "Warning: Replication has failed all attempts.", kCFStringEncodingUTF8 );
		}
		else if ( (!hasFailedSyncDate) && (!hasLastSyncDate) )
		{
			// has not completed a session yet
			valueString = CFStringCreateWithCString( kCFAllocatorDefault, "Warning: Replication has not completed yet.", kCFStringEncodingUTF8 );
		}
	}
	
	// if no special string, report status.
	if ( valueString == NULL )
	valueString = pwsf_GetReplicaStatusString( replicaStatus );
	
	count = CFArrayGetCount( ipArray );
	for ( index = 0; index < count; index++ )
	{
		nameString = (CFStringRef) CFArrayGetValueAtIndex( ipArray, index );
		if ( nameString == NULL )
			continue;
		
		CFDictionaryAddValue( inOutDict, nameString, valueString );
	}
	
	// add DNS if we have it
	if ( CFDictionaryGetValueIfPresent( inDict, CFSTR(kPWReplicaDNSKey), (const void **)&nameString ) )
		CFDictionaryAddValue( inOutDict, nameString, valueString );
	
	// clean up
	CFRelease( valueString );
	CFRelease( ipArray );
}


CFStringRef pwsf_GetReplicaStatusString( ReplicaStatus replicaStatus )
{
	char *result = "Unknown";
	CFStringRef outString;
	
	switch( replicaStatus )
	{
		case kReplicaActive:
			result = kPWReplicaStatusActiveValue;
			break;
		
		case kReplicaPermissionDenied:
			result = kPWReplicaStatusPermDenyValue;
			break;
			
		case kReplicaNotFound:
			result = kPWReplicaStatusNotFoundValue;
			break;
	}
	
	outString = CFStringCreateWithCString( kCFAllocatorDefault, result, kCFStringEncodingUTF8 );
	return outString;
}

		
