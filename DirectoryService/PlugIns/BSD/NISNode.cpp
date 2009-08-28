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

#include "NISNode.h"
#include "BaseDirectoryPlugin.h"
#include "CLog.h"
#include <syslog.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp.h>
#include <rpcsvc/ypclnt.h>
#include <notify.h>

const char* kDefaultDomainFilePath				= "/etc/defaultdomain";
const char* kOldDefaultDomainFilePath			= "/Library/Preferences/DirectoryService/nisdomain";

struct sNISReachabilityList
{
	struct sNISReachabilityList		*next;
	SCNetworkReachabilityRef		reachabilityRef;
	bool							isReachable;
	char							serverName[256];
};

typedef map<string,string>			NISAttributeMap;
typedef NISAttributeMap::iterator	NISAttributeMapI;

struct sNISRecordMapping
{
	const char		*fRecordType;	// record type we care about
	CFStringRef		fRecordTypeCF;
	bool			fKeyIsName;
	NISAttributeMap	fAttributeMap;	// Standard attribute type, table name
	CFArrayRef		fSuppAttribsCF;
	ParseCallback	fParseCallback;
};

struct sSearchState
{
	bool				fSearchAll;
	CFMutableArrayRef	fPendingResults;
	sNISRecordMapping	*fMapping;
};

// globals (non-member) variables
#pragma mark -
#pragma mark Global (non-member) variables
extern	UInt32			gMaxHandlerThreadCount;

static pthread_t		*gActiveThreads		= NULL;
static int				*gNotifyTokens		= NULL;
static UInt32			gActiveThreadCount	= 0;
static pthread_mutex_t	gActiveThreadMutex	= PTHREAD_MUTEX_INITIALIZER;

// static functions (non-member)
#pragma mark -
#pragma mark Static (non-member) functions
static void CancelNISThreads( void );
static void RemoveFromNISThreads( int inSlot );
static int	AddToNISThreads( void );

// static member variables
#pragma mark -
#pragma mark Static Member variables

char					*NISNode::fNISDomainConfigured		= NULL;
bool					NISNode::fNISAvailable				= false;
sNISReachabilityList	*NISNode::fServerReachability		= NULL;
DSMutexSemaphore		NISNode::fStaticMutex("NISNode::fStaticMutex");
NISRecordConfigDataMap	NISNode::fNISRecordMapTable;
CFAbsoluteTime			NISNode::fLastYPBindLaunchAttempt	= 0;

#pragma mark -
#pragma mark Class Methods

NISNode::NISNode( CFStringRef inNode, const char *inNISDomain, uid_t inUID, uid_t inEffectiveUID ) : FlatFileNode( inNode, inUID, inEffectiveUID )
{
	fNISDomain = strdup( inNISDomain );
}

NISNode::~NISNode( void )
{
	DSFree( fNISDomain );
}

CFMutableDictionaryRef NISNode::CopyNodeInfo( CFArrayRef inAttributes )
{
	CFMutableDictionaryRef	cfNodeInfo		= CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
																		&kCFTypeDictionaryValueCallBacks );
	CFMutableDictionaryRef	cfAttributes	= CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
																		&kCFTypeDictionaryValueCallBacks );
	CFRange					cfAttribRange	= CFRangeMake( 0, CFArrayGetCount(inAttributes) );
	bool					bNeedAll		= CFArrayContainsValue( inAttributes, cfAttribRange, CFSTR(kDSAttributesAll) );
	
	CFDictionarySetValue( cfNodeInfo, kBDPINameKey, CFSTR("DirectoryNodeInfo") );
	CFDictionarySetValue( cfNodeInfo, kBDPITypeKey, CFSTR(kDSStdRecordTypeDirectoryNodeInfo) );
	CFDictionarySetValue( cfNodeInfo, kBDPIAttributeKey, cfAttributes );
	
	if ( bNeedAll || CFArrayContainsValue(inAttributes, cfAttribRange, CFSTR(kDSNAttrNodePath)) )
	{
		CFArrayRef cfNodePath = CFStringCreateArrayBySeparatingStrings( kCFAllocatorDefault, fNodeName, CFSTR("/") );
		
		CFDictionarySetValue( cfAttributes, CFSTR(kDSNAttrNodePath), cfNodePath );
		
		DSCFRelease( cfNodePath );
	}
	
	if ( bNeedAll || CFArrayContainsValue(inAttributes, cfAttribRange, CFSTR(kDS1AttrReadOnlyNode)) )
	{
		CFStringRef	cfReadOnly	= CFSTR("ReadOnly");
		CFArrayRef	cfValue		= CFArrayCreate( kCFAllocatorDefault, (const void **) &cfReadOnly, 1, &kCFTypeArrayCallBacks );
		
		CFDictionarySetValue( cfAttributes, CFSTR(kDS1AttrReadOnlyNode), cfValue );
		
		DSCFRelease( cfValue );
	}
	
	if ( bNeedAll || CFArrayContainsValue(inAttributes, cfAttribRange, CFSTR(kDS1AttrDistinguishedName)) )
	{
		CFStringRef	cfRealName	= CFStringCreateWithCStringNoCopy( kCFAllocatorDefault, fNISDomain, kCFStringEncodingUTF8, kCFAllocatorNull );
		CFArrayRef	cfValue		= CFArrayCreate( kCFAllocatorDefault, (const void **) &cfRealName, 1, &kCFTypeArrayCallBacks );
		
		CFDictionarySetValue( cfAttributes, CFSTR(kDS1AttrDistinguishedName), cfValue );
		
		DSCFRelease( cfValue );
		DSCFRelease( cfRealName );
	}
	
	if ( bNeedAll || CFArrayContainsValue(inAttributes, cfAttribRange, CFSTR(kDSNAttrRecordType)) )
	{
		// we just get our mapping table and see
		fStaticMutex.WaitLock();
		
		NISRecordConfigDataMapI	mapIter = fNISRecordMapTable.begin();
		CFMutableArrayRef		cfValue = CFArrayCreateMutable( kCFAllocatorDefault, fNISRecordMapTable.size(), &kCFTypeArrayCallBacks );
		
		while ( mapIter != fNISRecordMapTable.end() )
		{
			CFArrayAppendValue( cfValue, mapIter->second->fRecordTypeCF );
			
			mapIter++;
		}

		fStaticMutex.SignalLock();
		
		CFDictionarySetValue( cfAttributes, CFSTR(kDSNAttrRecordType), cfValue );
		
		DSCFRelease( cfValue );
	}
	
	if ( bNeedAll || CFArrayContainsValue(inAttributes, cfAttribRange, CFSTR(kDSNAttrAuthMethod)) )
	{
		CFTypeRef	authTypeList[3] = {	CFSTR(kDSStdAuthClearText), CFSTR(kDSStdAuthNodeNativeNoClearText), CFSTR(kDSStdAuthNodeNativeClearTextOK) };
		
		CFArrayRef cfValue = CFArrayCreate( kCFAllocatorDefault, authTypeList, 3, &kCFTypeArrayCallBacks );
		
		CFDictionarySetValue( cfAttributes, CFSTR(kDSNAttrAuthMethod), cfValue );
		
		DSCFRelease( cfValue );
	}
	
	DSCFRelease( cfAttributes );
	
	return cfNodeInfo;
}

tDirStatus NISNode::VerifyCredentials( CFStringRef inRecordType, CFStringRef inRecordName, CFStringRef inPassword )
{
	tDirStatus	siResult	= eDSAuthFailed;
	char		*pTemp1		= NULL;

	if ( DomainNameChanged() == true || IsNISAvailable() == false )
		return eDSCannotAccessSession;
	
	if ( CFStringCompare(inRecordType, CFSTR(kDSStdRecordTypeUsers), 0) != kCFCompareEqualTo )
		return eNotHandledByThisNode;

	const char	*pUsername	= BaseDirectoryPlugin::GetCStringFromCFString( inRecordName, &pTemp1 );
	
	int slot = AddToNISThreads();

	fStaticMutex.WaitLock();
	
	NISRecordConfigDataMapI iter = fNISRecordMapTable.find( string(kDSStdRecordTypeUsers) );
	if ( iter != fNISRecordMapTable.end() )
	{
		sNISRecordMapping	*mapping		= iter->second;
		NISAttributeMapI	mapIter			= mapping->fAttributeMap.find( string(kDSNAttrRecordName) );
		CFStringRef			cfRecordPass	= NULL;
		
		// if we don't have a match, then we need to manually search the table?
		if ( mapIter != mapping->fAttributeMap.end() )
		{		
			char	*match		= NULL;
			int		matchLen	= 0;
			int		usernameLen	= strlen( pUsername );
			
			// first check for shadow.byname, then fallback to the normal
			if ( yp_match(fNISDomain, "master.passwd.byname", pUsername, usernameLen, &match, &matchLen) == 0 ||
				 yp_match(fNISDomain, "shadow.byname", pUsername, usernameLen, &match, &matchLen) == 0 ||
				 yp_match(fNISDomain, mapIter->second.c_str(), pUsername, usernameLen, &match, &matchLen) == 0 )
			{
				match[matchLen] = '\0';
				
				CFMutableDictionaryRef	resultRef = mapping->fParseCallback( NULL, true, 0, match, NULL );
				if ( resultRef != NULL )
				{
					CFDictionaryRef cfAttributes	= (CFDictionaryRef) CFDictionaryGetValue( resultRef, kBDPIAttributeKey );
					CFArrayRef		cfValues		= (CFArrayRef) CFDictionaryGetValue( cfAttributes, CFSTR(kDS1AttrPassword) );
					
					if ( cfValues != NULL && CFArrayGetCount(cfValues) > 0 )
					{
						cfRecordPass = (CFStringRef) CFArrayGetValueAtIndex( cfValues, 0 );
						CFRetain( cfRecordPass ); // need to retain because it will be released below
					}
					else
					{
						cfRecordPass = CFSTR("");
					}
				
					CFRelease( resultRef );
				}
				
				DSFree( match );
			}
		}
		
		if ( cfRecordPass != NULL && CheckPassword(cfRecordPass, inPassword) == true )
			siResult = eDSNoErr;
	}
	
	fStaticMutex.SignalLock();
	
	RemoveFromNISThreads( slot );

	DSFree( pTemp1 );
	
	return siResult;
}

tDirStatus NISNode::SearchRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount)
{
	CFArrayRef	recordTypeList	= inContext->fRecordTypeList;
	tDirStatus	siResult		= eDSNoErr;
	
	if ( DomainNameChanged() == true || IsNISAvailable() == false )
		return eDSCannotAccessSession;

	if ( recordTypeList == NULL )
		return eDSInvalidRecordType;
	
	// check for unsupported searches
	switch ( inContext->fPattMatchType )
	{
		case eDSExact:
		case eDSiExact:
		case eDSStartsWith:
		case eDSiStartsWith:
		case eDSEndsWith:
		case eDSiEndsWith:
		case eDSContains:
		case eDSiContains:
		case eDSAnyMatch:
			break;
		default:
			return eDSUnSupportedMatchType;
	}
	
	if ( inContext->fStateInfo != NULL )
	{
		InternalSearchRecords( inContext, inBuffer, outCount );
	}
	else
	{
		// first time the search has been initiated
		CFRange	cfValueRange	= CFRangeMake( 0, CFArrayGetCount(inContext->fValueList) );
		
		// if we are searching across all record types, we need to fix the type list to contain all the ones we support
		if ( CFArrayContainsValue(inContext->fRecordTypeList, CFRangeMake(0, CFArrayGetCount(recordTypeList)),
								  CFSTR(kDSStdRecordTypeAll)) )
		{
			CFMutableArrayRef		cfRecordTypeList	= CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
			NISRecordConfigDataMapI	iter				= fNISRecordMapTable.begin();
			
			// all record search, let's rebuild the list and put all the record types we support
			while ( iter != fNISRecordMapTable.end() )
			{
				CFArrayAppendValue( cfRecordTypeList, iter->second->fRecordTypeCF );
				iter++;
			}
			
			DSCFRelease( inContext->fRecordTypeList );
			inContext->fRecordTypeList = cfRecordTypeList;
		}
		
		InternalSearchRecords( inContext, inBuffer, outCount );
	}
	
	return siResult;
}

#pragma mark -
#pragma mark Private functions

SInt32 NISNode::InternalSearchRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount )
{
	sSearchState	*stateInfo	= (sSearchState *) inContext->fStateInfo;
	SInt32			siResult	= eDSNoErr;
	
	// we only do this once
	if (stateInfo == NULL)
	{
		stateInfo = (sSearchState *) calloc( 1, sizeof(sSearchState) );
		stateInfo->fPendingResults = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		stateInfo->fSearchAll = CFArrayContainsValue( inContext->fValueList, CFRangeMake(0, CFArrayGetCount(inContext->fValueList)), 
													  CFSTR(kDSRecordsAll) );
		inContext->fStateInfoCallback = FreeSearchState;
		inContext->fStateInfo = stateInfo;
	}
	// put any pending results first and return those before getting more
	else if ( CFArrayGetCount(stateInfo->fPendingResults) != 0 )
	{
		(*outCount) = BaseDirectoryPlugin::FillBuffer( stateInfo->fPendingResults, inBuffer );
		
		// see if we fit anything in the buffer
		if ( (*outCount) == 0 )
			return eDSBufferTooSmall;
	}
	
	// if we have fetched all we wanted
	if ( inContext->fMaxRecCount > 0 && inContext->fIndex >= inContext->fMaxRecCount )
		return eDSNoErr;
	
	// if we don't have pending answers, we have no results and we have more types, go to the next type
	while ( (*outCount) == 0 && inContext->fRecTypeIndex < CFArrayGetCount(inContext->fRecordTypeList) )
	{
		// see if we have a state, or if there is even anything to do..
		CFStringRef	cfRecordType	= (CFStringRef) CFArrayGetValueAtIndex( inContext->fRecordTypeList, inContext->fRecTypeIndex );
		char		*pTemp			= NULL;
		const char	*pRecordType	= BaseDirectoryPlugin::GetCStringFromCFString( cfRecordType, &pTemp );
		
		NISRecordConfigDataMapI iter = fNISRecordMapTable.find( string(pRecordType) );
		
		DSFreeString( pTemp );
		
		if ( iter == fNISRecordMapTable.end() )
		{
			inContext->fRecTypeIndex++;
			siResult = eDSInvalidRecordType; // in case it was our last type
			continue;
		}
		
		stateInfo->fMapping = iter->second;
		
		// now we do the appropriate search
		if ( stateInfo->fSearchAll )
			siResult = FetchAllRecords( inContext, inBuffer, outCount );
		else
			siResult = FetchMatchingRecords( inContext, inBuffer, outCount );
		
		// we always increment here because if the above returned, we either had results or we didn't
		inContext->fRecTypeIndex++;
		
		if ( siResult != eDSNoErr || (*outCount) > 0 )
			break;
	}
	
	return siResult;
}

SInt32 NISNode::FetchAllRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount )
{
	sSearchState			*stateInfo	= (sSearchState *) inContext->fStateInfo;
	sNISRecordMapping		*mapping	= stateInfo->fMapping;
	void					*cbCtx[]	= { this, inContext };
	struct ypall_callback	callback	= { ForEachEntry, (char*) cbCtx };
	SInt32					siResult	= eDSNoErr;
	
	// first get the map we need to use, should be the one with RecordName
	NISAttributeMapI	mapIter = mapping->fAttributeMap.find( string(kDSNAttrRecordName) );
	
	// Note, automount maps cannot be retrieved via the list since there is no way to know what table is an automount table
	if ( mapIter != mapping->fAttributeMap.end() )
	{
		int slot = AddToNISThreads();

		fStaticMutex.WaitLock();
		
		int iError = yp_all( fNISDomain, mapIter->second.c_str(), &callback );
		
		// need to fallback to yp_first/yp_next
		if ( iError == YPERR_VERS )
		{
			const char	*map	= mapIter->second.c_str();
			char	*key		= NULL;
			char	*prevKey	= NULL;
			int		keyLen		= 0;
			char	*val		= NULL;
			int		valLen		= 0;
			
			iError = yp_first( fNISDomain, map, &key, &keyLen, &val, &valLen );
			while ( iError == 0 )
			{
				// explicitly terminate the string here so our parser will work
				val[valLen] = '\0';
				
				// clear any returns or line feeds, we don't care about the return value
				char *tempVal = val;
				val = strsep( &tempVal, "\r\n" );
				
				// parse the string and add it to our list of results
				CFMutableDictionaryRef	resultRef = mapping->fParseCallback( NULL, true, 0, val, (mapping->fKeyIsName ? key : NULL) );
				if ( resultRef != NULL )
				{
					FilterAttributes( resultRef, inContext->fReturnAttribList );
					CFArrayAppendValue( stateInfo->fPendingResults, resultRef );
					DSCFRelease( resultRef );
					
					inContext->fIndex++;
				}

				// free the previous key and the current value
				DSFree( prevKey );
				DSFree( val );

				if ( inContext->fMaxRecCount != 0 && inContext->fIndex >= inContext->fMaxRecCount )
					break;

				// now save the key for next time
				prevKey = key;
				
				// get the next value
				iError = yp_next( fNISDomain, map, prevKey, keyLen, &key, &keyLen, &val, &valLen );
			}
		}

		siResult = MapNISError( iError );

		fStaticMutex.SignalLock();
		
		RemoveFromNISThreads( slot );
	}
	
	if ( siResult == eDSNoErr )
	{
		(*outCount) = BaseDirectoryPlugin::FillBuffer( stateInfo->fPendingResults, inBuffer );
		
		siResult = ((*outCount) == 0 && CFArrayGetCount(stateInfo->fPendingResults) > 0 ? eDSBufferTooSmall : eDSNoErr);
	}
	
	return siResult;
}

SInt32 NISNode::FetchMatchingRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount )
{
	sSearchState		*stateInfo	= (sSearchState *) inContext->fStateInfo;
	sNISRecordMapping	*mapping	= stateInfo->fMapping;
	char				*pTemp		= NULL;
	const char			*attribute	= BaseDirectoryPlugin::GetCStringFromCFString( inContext->fAttributeType, &pTemp );
	SInt32				siResult	= eDSNoErr;
	
	// see if it is supported attribute
	CFArrayRef	cfSuppAttr = stateInfo->fMapping->fSuppAttribsCF;
	if ( CFArrayContainsValue(cfSuppAttr, CFRangeMake(0, CFArrayGetCount(cfSuppAttr)), inContext->fAttributeType) == false )
		return eDSNoStdMappingAvailable;
	
	// we special case Automount and AutomountMaps since they are special
	if ( CFStringCompare(stateInfo->fMapping->fRecordTypeCF, CFSTR(kDSStdRecordTypeAutomount), 0) == kCFCompareEqualTo || 
		 CFStringCompare(stateInfo->fMapping->fRecordTypeCF, CFSTR(kDSStdRecordTypeAutomountMap), 0) == kCFCompareEqualTo )
	{
		siResult = FetchAutomountRecords( inContext, inBuffer, outCount );
	}
	else
	{
		NISAttributeMapI	mapIter = mapping->fAttributeMap.find( string(attribute) );
		
		// change our context data to lowercase if this is an insensitive search
		if ( inContext->fPattMatchType == eDSiStartsWith || inContext->fPattMatchType == eDSiEndsWith )
		{
			CFArrayRef			patternsToMatch		= inContext->fValueList;
			CFMutableArrayRef	lowerCasePatterns	= CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
			
			if ( lowerCasePatterns == NULL )
				return eMemoryAllocError;
			
			// now assign the array to the new one
			inContext->fValueList = lowerCasePatterns;
			
			CFIndex numPatterns = CFArrayGetCount( patternsToMatch );
			for ( CFIndex j = 0; j < numPatterns; j++ )
			{
				CFMutableStringRef	tempPattern	= NULL;
				
				CFStringRef patternToMatch = (CFStringRef) CFArrayGetValueAtIndex( patternsToMatch, j );
				if ( CFGetTypeID(patternToMatch) != CFStringGetTypeID() )
					continue;
				
				tempPattern = CFStringCreateMutableCopy( NULL, 0, patternToMatch );
				if ( tempPattern == NULL )
					continue;
				
				CFStringLowercase( tempPattern, NULL );
				CFArrayAppendValue( lowerCasePatterns, tempPattern );
				DSCFRelease( tempPattern );
			}
			
			DSCFRelease( patternsToMatch );
		}
		
		int slot = AddToNISThreads();

		// if we don't have a match then we'll need to manual match
		if ( mapIter != mapping->fAttributeMap.end() )
		{
			CFIndex	iCount = CFArrayGetCount( inContext->fValueList );
			
			for ( CFIndex ii = 0; ii < iCount && (inContext->fMaxRecCount == 0 || inContext->fIndex < inContext->fMaxRecCount); ii++ )
			{
				CFTypeRef	cfValue		= CFArrayGetValueAtIndex( inContext->fValueList, ii );
				
				// we only accept strings for searching
				if ( CFGetTypeID(cfValue) == CFStringGetTypeID() )
				{
					char		*pTemp2		= NULL;
					const char	*value		= BaseDirectoryPlugin::GetCStringFromCFString( (CFStringRef) cfValue, &pTemp2 );
					char		*match		= NULL;
					int			matchLen	= 0;
					
					fStaticMutex.WaitLock();
					siResult = MapNISError( yp_match(fNISDomain, mapIter->second.c_str(), value, strlen(value), &match, &matchLen) );
					fStaticMutex.SignalLock();
					
					if ( siResult == eDSNoErr && match != NULL )
					{
						match[matchLen] = '\0';
						
						// parse the string and add it to our list of results
						CFMutableDictionaryRef	resultRef = mapping->fParseCallback( NULL, true, 0, match, (mapping->fKeyIsName ? value : NULL) );
						if ( resultRef != NULL )
						{
							if ( RecordMatchesCriteria(inContext, resultRef) )
							{
								FilterAttributes( resultRef, inContext->fReturnAttribList );
								CFArrayAppendValue( stateInfo->fPendingResults, resultRef );
								inContext->fIndex++;
							}
							
							DSCFRelease( resultRef );
						}
					}
					
					DSFree( match );
					DSFree( pTemp2 );
				}
			}
		}
		else if ( (mapIter = mapping->fAttributeMap.find(string(kDSNAttrRecordName))) != mapping->fAttributeMap.end() )
		{
			void					*cbCtx[]	= { this, inContext };
			struct ypall_callback	callback	= { ForEachEntry, (char*) cbCtx };

			fStaticMutex.WaitLock();
			
			int iError = yp_all( fNISDomain, mapIter->second.c_str(), &callback );
			
			// need to fallback to yp_first/yp_next
			if ( iError == YPERR_VERS )
			{
				const char	*map		= mapIter->second.c_str();
				char		*key		= NULL;
				char		*prevKey	= NULL;
				int			keyLen		= 0;
				char		*val		= NULL;
				int			valLen		= 0;
				
				iError = yp_first( fNISDomain, map, &key, &keyLen, &val, &valLen );
				while ( iError == 0 )
				{
					// explicitly terminate the string here so our parser will work
					val[valLen] = '\0';
					
					// clear any returns or line feeds, we don't care about the return value
					char *tempVal = val;
					val = strsep( &tempVal, "\r\n" );
					
					// parse the string and add it to our list of results
					CFMutableDictionaryRef	resultRef = mapping->fParseCallback( NULL, true, 0, val, (mapping->fKeyIsName ? key : NULL) );
					if ( resultRef != NULL )
					{
						if ( RecordMatchesCriteria(inContext, resultRef) )
						{
							FilterAttributes( resultRef, inContext->fReturnAttribList );
							CFArrayAppendValue( stateInfo->fPendingResults, resultRef );
							inContext->fIndex++;
						}
						
						DSCFRelease( resultRef );
					}
					
					// free the previous key and the current value
					DSFree( prevKey );
					DSFree( val );
					
					if ( inContext->fMaxRecCount != 0 && inContext->fIndex >= inContext->fMaxRecCount )
						break;
					
					// now save the key for next time
					prevKey = key;
					
					// get the next value
					iError = yp_next( fNISDomain, map, prevKey, keyLen, &key, &keyLen, &val, &valLen );
				}
			}
			
			fStaticMutex.SignalLock();
		}

		RemoveFromNISThreads( slot );
	}
	
	DSFree( pTemp );
	
	if ( siResult == eDSNoErr )
	{
		(*outCount) = BaseDirectoryPlugin::FillBuffer( stateInfo->fPendingResults, inBuffer );
		
		siResult = ((*outCount) == 0 && CFArrayGetCount(stateInfo->fPendingResults) > 0 ? eDSBufferTooSmall : eDSNoErr);
	}
	
	return siResult;
}

SInt32 NISNode::FetchAutomountRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount )
{
	sSearchState	*stateInfo	= (sSearchState *) inContext->fStateInfo;
	SInt32			siResult	= eDSNoErr;
	bool			bMetaMap	= (CFStringCompare(inContext->fAttributeType, CFSTR(kDS1AttrMetaAutomountMap), 0) == kCFCompareEqualTo);
	bool			bRecName	= (CFStringCompare(inContext->fAttributeType, CFSTR(kDSNAttrRecordName), 0) == kCFCompareEqualTo);
	bool			bAutomount	= (CFStringCompare(stateInfo->fMapping->fRecordTypeCF, CFSTR(kDSStdRecordTypeAutomount), 0) == kCFCompareEqualTo);
	bool			bAutoMap	= (CFStringCompare(stateInfo->fMapping->fRecordTypeCF, CFSTR(kDSStdRecordTypeAutomountMap), 0) == kCFCompareEqualTo);

	int				slot		= AddToNISThreads();

	// iMapIndex is initialized outside of the again
	CFIndex	iCount	= CFArrayGetCount(inContext->fValueList);
	
	for ( CFIndex iMapIndex = 0; iMapIndex < iCount && (inContext->fMaxRecCount == 0 || inContext->fIndex < inContext->fMaxRecCount); iMapIndex++ )
	{
		if ( bAutomount && bRecName )
		{
			CFStringRef	cfValue = (CFStringRef) CFArrayGetValueAtIndex( inContext->fValueList, iMapIndex );
			
			if ( CFGetTypeID(cfValue) == CFStringGetTypeID() )
			{
				CFArrayRef cfValues = CFStringCreateArrayBySeparatingStrings( kCFAllocatorDefault, cfValue, CFSTR(",automountMapName=") );
				
				if ( CFArrayGetCount(cfValues) == 2 )
				{
					char		*pTemp1	= NULL;
					char		*pTemp2	= NULL;
					const char	*pMount = BaseDirectoryPlugin::GetCStringFromCFString( (CFStringRef)CFArrayGetValueAtIndex(cfValues, 0), &pTemp1 );
					const char	*pMap	= BaseDirectoryPlugin::GetCStringFromCFString( (CFStringRef)CFArrayGetValueAtIndex(cfValues, 1), &pTemp2 );
					char		*match	= NULL;
					int			matchLen= 0;
					
					fStaticMutex.WaitLock();
					siResult = MapNISError( yp_match(fNISDomain, pMap, pMount, strlen(pMount), &match, &matchLen) );
					fStaticMutex.SignalLock();
					
					// if it failed, see if there is an underscore, if so try with a dot
					if ( siResult == eDSNoStdMappingAvailable && strchr(pMap, '_') != NULL )
					{
						char	*pMap2	= strdup( pMap );
						
						// replace all _ with .
						for ( char *pLoc = pMap2; (*pLoc) != '\0'; pLoc++ )
						{
							if ( (*pLoc) == '_' ) 
								(*pLoc) = '.';
						}

						fStaticMutex.WaitLock();
						siResult = MapNISError( yp_match(fNISDomain, pMap2, pMount, strlen(pMount), &match, &matchLen) );
						fStaticMutex.SignalLock();
						
						DSFree( pMap2 );
					}
					
					if ( siResult == eDSNoErr && matchLen != 0 )
					{
						CFMutableDictionaryRef cfRecord = CreateRecordDictionary( CFSTR(kDSStdRecordTypeAutomount), pMount );
						
						AddAttributeToRecord( cfRecord, CFSTR(kDSNAttrAutomountInformation), match );
						AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrMetaAutomountMap), pMap );
						
						FilterAttributes( cfRecord, inContext->fReturnAttribList );
						CFArrayAppendValue( stateInfo->fPendingResults, cfRecord );
						CFRelease( cfRecord );
						
						inContext->fIndex++;
					}
					
					DSFree( match );
					DSFree( pTemp2 );
					DSFree( pTemp1 );
				}
				
				CFRelease( cfValues );
			}
		}
		else if ( bMetaMap || (bAutoMap && bRecName) )
		{
			if ( iMapIndex < CFArrayGetCount(inContext->fValueList) )
			{
				CFStringRef	cfValue = (CFStringRef) CFArrayGetValueAtIndex( inContext->fValueList, iMapIndex );
				
				if ( CFGetTypeID(cfValue) == CFStringGetTypeID() )
				{
					char		*pTemp2	= NULL;
					const char	*pMap	= BaseDirectoryPlugin::GetCStringFromCFString( cfValue, &pTemp2 );
					char		*pAlt	= NULL;
					const char	*pSrch	= pMap;
					
					// see if there is an underscore, so we can do the alternate name if needed
					if ( strchr(pMap, '_') != NULL )
					{
						pAlt = strdup( pMap );

						// replace all _ with .
						for ( char *pLoc = pAlt; (*pLoc) != '\0'; pLoc++ )
						{
							if ( (*pLoc) == '_' ) 
								(*pLoc) = '.';
						}
					}
					
					// if we are looking for all maps, we need to enumerate the entire map
					if ( bMetaMap )
					{
						char	*key		= NULL;
						char	*prevKey	= NULL;
						int		keyLen		= 0;
						char	*val		= NULL;
						int		valLen		= 0;
						
						fStaticMutex.WaitLock();
						int iError = yp_first( fNISDomain, pSrch, &key, &keyLen, &val, &valLen );
						if ( iError == YPERR_MAP && pAlt != NULL )
						{
							pSrch = pAlt; // try the alternate
							iError = yp_first( fNISDomain, pSrch, &key, &keyLen, &val, &valLen );
						}
						
						while ( iError == 0 )
						{
							// explicitly terminate the string here so our parser will work
							key[keyLen] = '\0';
							val[valLen] = '\0';
							
							if ( DSIsStringEmpty(key) == false && DSIsStringEmpty(val) == false )
							{
								CFMutableDictionaryRef cfRecord = CreateRecordDictionary( CFSTR(kDSStdRecordTypeAutomount), key );
								
								AddAttributeToRecord( cfRecord, CFSTR(kDSNAttrAutomountInformation), val );
								AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrMetaAutomountMap), pMap );
								
								FilterAttributes( cfRecord, inContext->fReturnAttribList );
								CFArrayAppendValue( stateInfo->fPendingResults, cfRecord );
								CFRelease( cfRecord );
							
								inContext->fIndex++;
							}
							
							// free the previous key and the current value
							DSFree( prevKey );
							DSFree( val );
							
							if ( inContext->fMaxRecCount != 0 && inContext->fIndex >= inContext->fMaxRecCount )
							{
								// need to free the key here since we are breaking the loop
								break;
							}
							
							// now save the key for next time
							prevKey = key;
							key = NULL;
							
							// get the next value
							iError = yp_next( fNISDomain, pSrch, prevKey, keyLen, &key, &keyLen, &val, &valLen );
						}
						
						DSFree( key );
						DSFree( prevKey );
						
						siResult = MapNISError( iError );
						
						fStaticMutex.SignalLock();
					}
					else
					{
						struct ypmaplist	*mapList = NULL;
						
						fStaticMutex.WaitLock();
						siResult = MapNISError( yp_maplist(fNISDomainConfigured, &mapList) );
						fStaticMutex.SignalLock();
						
						// see if we have the map, if so, return a dictionary that it exists
						if ( siResult == eDSNoErr )
						{
							bool	bExists = false;
							
							// we loop through them all and delete as we go too
							while ( mapList != NULL )
							{
								if ( bExists == false && (strcmp(mapList->map, pMap) == 0 || (pAlt != NULL && strcmp(mapList->map, pAlt) == 0)) )
								{
									bExists = true;
								}
								
								struct ypmaplist *delItem = mapList;
								mapList = mapList->next;
								
								DSFree( delItem );
							}
							
							if ( bExists == true )
							{
								CFMutableDictionaryRef cfRecord = CreateRecordDictionary( CFSTR(kDSStdRecordTypeAutomountMap), pMap );
								
								FilterAttributes( cfRecord, inContext->fReturnAttribList );
								CFArrayAppendValue( stateInfo->fPendingResults, cfRecord );
								CFRelease( cfRecord );
								
								inContext->fIndex++;
							}
						}
					}
					
					DSFree( pAlt );
					DSFree( pTemp2 );
				}
			}
		}
	}
	
	RemoveFromNISThreads( slot );

	return siResult;
}

void NISNode::FreeSearchState( void *inState )
{
	sSearchState	*theState = (sSearchState *) inState;
	
	DSCFRelease( theState->fPendingResults );
	DSFree( inState );
}

int NISNode::ForEachEntry( unsigned long inStatus, char *inKey, int inKeyLen, char *inVal, int inValLen, void *inData )
{
	NISNode						*node		= (NISNode *) ((void **)inData)[0];
	sBDPISearchRecordsContext	*context	= (sBDPISearchRecordsContext *) ((void **)inData)[1];
	sSearchState				*stateInfo	= (sSearchState *) context->fStateInfo;
	
	// explicitly terminate the string here so our parser will work
	inVal[inValLen] = '\0';
	
	// clear any returns or line feeds, we don't care about the return value
	char *tempVal = inVal;
	inVal = strsep( &tempVal, "\r\n" );
	
	// parse the string and add it to our list of results
	CFMutableDictionaryRef	resultRef = stateInfo->fMapping->fParseCallback( NULL, true, 0, inVal, (stateInfo->fMapping->fKeyIsName ? inKey : NULL) );
	if ( resultRef != NULL )
	{
		if ( RecordMatchesCriteria(context, resultRef) )
		{
			node->FilterAttributes( resultRef, context->fReturnAttribList );
			CFArrayAppendValue( stateInfo->fPendingResults, resultRef );
			context->fIndex++;
		}
		
		DSCFRelease( resultRef );
	}
	
	// return non-zero if we have reached our max records
	return (context->fMaxRecCount != 0 && context->fIndex >= context->fMaxRecCount);
}

bool NISNode::RecordMatchesCriteria( sBDPISearchRecordsContext *inContext, CFDictionaryRef inRecordDict )
{
	CFArrayRef			patternsToMatch		= inContext->fValueList;
	bool				matches				= false;
	bool				needLowercase		= (inContext->fPattMatchType == eDSiStartsWith || inContext->fPattMatchType == eDSiEndsWith);

	if ( CFArrayContainsValue(patternsToMatch, CFRangeMake(0, CFArrayGetCount(patternsToMatch)), CFSTR(kDSRecordsAll)) || 
		 inContext->fPattMatchType == eDSAnyMatch )
		return true;
	
	CFDictionaryRef cfAttributes = (CFDictionaryRef) CFDictionaryGetValue( inRecordDict, kBDPIAttributeKey );
	if ( cfAttributes == NULL )
		return false;
	
	CFArrayRef attrValues = (CFArrayRef) CFDictionaryGetValue( cfAttributes, inContext->fAttributeType );
	if ( attrValues == NULL )
		return false;
	
	CFIndex numValues = CFArrayGetCount( attrValues );
	for ( CFIndex i = 0; i < numValues && matches == false; i++ )
	{
		CFMutableStringRef	mutableLowercaseValue	= NULL;
		CFStringRef			attributeString			= (CFStringRef) CFArrayGetValueAtIndex( attrValues, i );
		
		if ( CFGetTypeID(attributeString) != CFStringGetTypeID() )
			continue;

		// first lowercase the value
		if ( needLowercase )
		{
			mutableLowercaseValue = CFStringCreateMutableCopy( NULL, 0, attributeString );
			if ( mutableLowercaseValue == NULL )
				continue;
			CFStringLowercase( mutableLowercaseValue, NULL );
		}
		
		CFIndex numPatterns = CFArrayGetCount( patternsToMatch );
		for ( CFIndex j = 0; j < numPatterns && matches == false; j++ )
		{
			CFStringRef patternToMatch = (CFStringRef) CFArrayGetValueAtIndex( patternsToMatch, j );
			
			switch ( inContext->fPattMatchType )
			{
				case eDSExact:
					matches = CFStringCompare(attributeString, patternToMatch, 0) == kCFCompareEqualTo;
					break;
				case eDSiExact:
					matches = (CFStringCompare(attributeString, patternToMatch, kCFCompareCaseInsensitive) == kCFCompareEqualTo);
					break;
				case eDSStartsWith:
					matches = CFStringHasPrefix( attributeString, patternToMatch );
					break;
				case eDSiStartsWith:
					matches = CFStringHasPrefix( mutableLowercaseValue, patternToMatch );
					break;
				case eDSEndsWith:
					matches = CFStringHasSuffix( attributeString, patternToMatch );
					break;
				case eDSiEndsWith:
					matches = CFStringHasSuffix( mutableLowercaseValue, patternToMatch );
					break;
				case eDSContains:
					matches = (CFStringFind(attributeString, patternToMatch, 0).location != kCFNotFound);
					break;
				case eDSiContains:
					matches = (CFStringFind(attributeString, patternToMatch, kCFCompareCaseInsensitive).location != kCFNotFound);
					break;
				default:
					break;
			}
		}
		
		DSCFRelease( mutableLowercaseValue );
	}
	
	return matches;
}

SInt32 NISNode::MapNISError( int inError )
{
	SInt32	siResult	= eDSNoErr;
	
	switch (inError)
	{
		case YPERR_BADDB:
		case YPERR_DOMAIN:
		case YPERR_PMAP:
		case YPERR_RPC:
		case YPERR_YPBIND:
		case YPERR_YPERR:
		case YPERR_YPSERV:
			siResult = eDSCannotAccessSession;
			break;
		case YPERR_RESRC:
			siResult = eMemoryError;
			break;
		case YPERR_KEY:
			siResult = eDSAttributeValueNotFound;
			break;
		case YPERR_MAP:
			siResult = eDSNoStdMappingAvailable;
			break;
		case YPERR_NOMORE:
		default:
			siResult = eDSNoErr;
			break;
	}
	
	return siResult;
}

void NISNode::BuildYPMapTable( void )
{
	struct ypmaplist	*mapList	= NULL;
	
	int					slot		= AddToNISThreads();

	fStaticMutex.WaitLock();

	fNISRecordMapTable.clear();
	
	if ( yp_maplist(fNISDomainConfigured, &mapList) == 0 )
	{
		map<string,string>	typeMap;
		map<string,string>	attrMap;
		
		// first ensure the Flatfile table is built because we use it
		BuildTableMap();
		
		// build us a support map just to map tables to record types
		typeMap[string("passwd")]		= string( kDSStdRecordTypeUsers );
		typeMap[string("group")]		= string( kDSStdRecordTypeGroups );
		typeMap[string("mail")]			= string( kDSStdRecordTypeAliases );
		typeMap[string("bootptab")]		= string( kDSStdRecordTypeBootp );
		typeMap[string("ethers")]		= string( kDSStdRecordTypeEthernets );
		typeMap[string("hosts")]		= string( kDSStdRecordTypeHosts );
		typeMap[string("mounts")]		= string( kDSStdRecordTypeMounts );
		typeMap[string("netgroup")]		= string( kDSStdRecordTypeNetGroups );
		typeMap[string("networks")]		= string( kDSStdRecordTypeNetworks );
		typeMap[string("printcap")]		= string( kDSStdRecordTypePrinters );
		typeMap[string("protocols")]	= string( kDSStdRecordTypeProtocols );
		typeMap[string("rpc")]			= string( kDSStdRecordTypeRPC );
		typeMap[string("services")]		= string( kDSStdRecordTypeServices );
		
		// NOTE: potential maps netgroup.byuser and netgroup.byhost in future
		// but DS doesn't support those type of searches
		attrMap[string("byname")]					= string( kDSNAttrRecordName );
		attrMap[string("byuid")]					= string( kDS1AttrUniqueID );
		attrMap[string("bygid")]					= string( kDS1AttrPrimaryGroupID );
		attrMap[string("bynumber")]					= string( "dsAttrTypeNative:number" );
		attrMap[string("byaddr")]					= string( kDS1AttrENetAddress );
		attrMap[string("mail.aliases")]				= string( kDSNAttrRecordName );
		attrMap[string("netgroup")]					= string( kDSNAttrRecordName );
//		attrMap[string("netgroup.byhost")]			= string( kDSNAttrRecordName );
//		attrMap[string("netgroup.byuser")]			= string( kDSNAttrRecordName );
		attrMap[string("services.bynumber")]		= string( kDS1AttrPort );
		attrMap[string("services.bynp")]			= string( kDS1AttrPort );
		attrMap[string("services.byservicename")]	= string( kDSNAttrRecordName );
		attrMap[string("hosts.byaddr")]				= string( kDSNAttrIPAddress );

		// delete the structure
		while ( mapList != NULL )
		{
			map<string,string>::iterator	mapEntry;
			char							recType[YPMAXMAP + 1];
			
			strlcpy( recType, mapList->map, sizeof(recType) );
			
			char *suffix = strchr( recType, '.' );
			if ( suffix != NULL )
			{
				(*suffix) = '\0';
				suffix++;
			}
			
			mapEntry = typeMap.find( string(recType) );
			if ( mapEntry != typeMap.end() )
			{
				map<string,string>::iterator	attrEntry	= attrMap.find( string(mapList->map) );
				sNISRecordMapping				*recMap		= NULL;
				const char						*dsAttrType = NULL;
				
				// first we check the map name directly for overrides, then fall over to the suffix
				if ( attrEntry != attrMap.end() )
				{
					dsAttrType = attrEntry->second.c_str();
				}
				else if ( suffix != NULL )
				{
					attrEntry = attrMap.find( string(suffix) );
					if ( attrEntry != attrMap.end() )
						dsAttrType = attrEntry->second.c_str();
				}
				
				if ( dsAttrType != NULL )
				{
					NISRecordConfigDataMapI	recMapEntry = fNISRecordMapTable.find( mapEntry->second );
					const char				*dsRecType	= mapEntry->second.c_str();
					
					if ( recMapEntry == fNISRecordMapTable.end() )
					{
						// no need to lock the table is only built once and never changed in flat files
						FlatFileConfigDataMapI iter = fFFRecordTypeTable.find( mapEntry->second );

						// if we don't map it in the flat file plugin, we won't here either
						if ( iter != fFFRecordTypeTable.end() )
						{
							recMap = new sNISRecordMapping;
							
							recMap->fRecordType		= strdup( dsRecType );
							recMap->fRecordTypeCF	= CFStringCreateWithCStringNoCopy( kCFAllocatorDefault, recMap->fRecordType, kCFStringEncodingUTF8, 
																					   kCFAllocatorNull );
							recMap->fKeyIsName		= false;
							recMap->fParseCallback	= iter->second->fParseCallback;
							
							if ( iter->second->fSuppAttribsCF ) {
								CFRetain( iter->second->fSuppAttribsCF );
								recMap->fSuppAttribsCF = iter->second->fSuppAttribsCF;
							}
							else {
								recMap->fSuppAttribsCF = NULL;
							}
							
							// certain tables the key is the name
							if ( strcmp(dsRecType, kDSStdRecordTypeNetGroups) == 0 )
							{
								recMap->fKeyIsName = true;
								DbgLog( kLogPlugin, "NISNode::BuildYPMapTable - setting KeyIsName for table <%s>", dsRecType );
							}
							
							fNISRecordMapTable[mapEntry->second] = recMap;
							
							DbgLog( kLogPlugin, "NISNode::BuildYPMapTable - creating map table for <%s>", dsRecType );
						}
						else
						{
							DbgLog( kLogPlugin, "NISNode::BuildYPMapTable - no map in flat files for <%s>", dsRecType );
						}
					}
					else
					{
						recMap = recMapEntry->second;
					}
					
					if ( recMap != NULL )
					{
						DbgLog( kLogPlugin, "NISNode::BuildYPMapTable - map table <%s> to <%s>:<%s> queries", mapList->map, dsRecType, dsAttrType );
						recMap->fAttributeMap[string(dsAttrType)] = string( mapList->map );
					}
					
					// special case the IP address one because we may also have IPv6 addresses
					if ( recMap != NULL && strcmp(dsAttrType, kDSNAttrIPAddress) == 0 )
					{
						recMap->fAttributeMap[string(kDSNAttrIPv6Address)] = string( mapList->map );
						
						DbgLog( kLogPlugin, "NISNode::BuildYPMapTable - map table <%s> to <%s>:<%s> queries", mapList->map, dsRecType, 
							    kDSNAttrIPv6Address );
					}
				}
			}
			else
			{
				DbgLog( kLogPlugin, "NISNode::BuildYPMapTable - unable to map table <%s> for use", mapList->map );
			}
			
			struct ypmaplist *delItem = mapList;
			mapList = mapList->next;
			
			DSFree( delItem );
		}
		
		// we explicitly add automount map to the list, even though we don't know the name
		FlatFileConfigDataMapI iter = fFFRecordTypeTable.find( string(kDSStdRecordTypeAutomount) );
		if ( iter != fFFRecordTypeTable.end() )
		{
			sNISRecordMapping *recMap = new sNISRecordMapping;
			
			recMap->fRecordType		= strdup( kDSStdRecordTypeAutomount );
			recMap->fRecordTypeCF	= CFStringCreateWithCStringNoCopy( kCFAllocatorDefault, recMap->fRecordType, kCFStringEncodingUTF8, 
																	   kCFAllocatorNull );
			recMap->fKeyIsName		= false;
			recMap->fParseCallback	= iter->second->fParseCallback;
			
			if ( iter->second->fSuppAttribsCF ) {
				CFRetain( iter->second->fSuppAttribsCF );
				recMap->fSuppAttribsCF = iter->second->fSuppAttribsCF;
			}
			else {
				recMap->fSuppAttribsCF = NULL;
			}
			
			fNISRecordMapTable[ string(kDSStdRecordTypeAutomount) ] = recMap;
		}

		// now add the map one too
		iter = fFFRecordTypeTable.find( string(kDSStdRecordTypeAutomountMap) );
		if ( iter != fFFRecordTypeTable.end() )
		{
			sNISRecordMapping *recMap = new sNISRecordMapping;
			
			recMap->fRecordType		= strdup( kDSStdRecordTypeAutomountMap );
			recMap->fRecordTypeCF	= CFStringCreateWithCStringNoCopy( kCFAllocatorDefault, recMap->fRecordType, kCFStringEncodingUTF8, 
																	  kCFAllocatorNull );
			recMap->fParseCallback	= iter->second->fParseCallback;
			recMap->fKeyIsName		= false;
			
			if ( iter->second->fSuppAttribsCF ) {
				CFRetain( iter->second->fSuppAttribsCF );
				recMap->fSuppAttribsCF = iter->second->fSuppAttribsCF;
			}
			else {
				recMap->fSuppAttribsCF = NULL;
			}
			
			fNISRecordMapTable[ string(kDSStdRecordTypeAutomountMap) ] = recMap;
		}
	}
	
	fStaticMutex.SignalLock();

	RemoveFromNISThreads( slot );
}

void NISNode::SetDomainAndServers( const char *inNISDomain, const char *inNISServers )
{
	int slot = AddToNISThreads();
	
	fStaticMutex.WaitLock();

	// if there is an old domain, it needs to be cleared.
	if ( fNISDomainConfigured != NULL && fNISDomainConfigured[0] != '\0' )
	{
		// really need to kill the ypbind process for this to be effective
		yp_unbind( fNISDomainConfigured );
		unlink( kDefaultDomainFilePath );
		setdomainname( "", 0 );
		SetNISServers( fNISDomainConfigured, NULL );
	}

	DSFree( fNISDomainConfigured );
	
	// if there is a new domain name, set it up.  if not, user
	// was disabling NIS so there's nothing more to do.
	if ( inNISDomain != NULL && inNISDomain[0] != '\0' )
	{
		// setting a domain (either existing (i.e. startup) or changing)

		// write the new file (not really needed for the startup case
		// as the existing file would have sufficed).
		FILE *destFP = fopen( kDefaultDomainFilePath, "w+" );
		if ( destFP != NULL )
		{
			fputs( inNISDomain, destFP );
			fclose( destFP );
		}

		fNISDomainConfigured = strdup( inNISDomain );
		
		// on startup inNISServers will be null - in that case skip
		// the call to SetNISServers to prevent clearing the server list.
		if ( inNISServers != NULL )
		{
			SetNISServers( inNISDomain, inNISServers );
		}
			
		setdomainname( inNISDomain, strlen(inNISDomain) );
	}

	fStaticMutex.SignalLock();
	
	RemoveFromNISThreads( slot );
}


char *NISNode::CopyDomainName( void )
{
	struct stat statBlock;
	
	// see if the old file exists and rename to the new location
	if ( lstat(kOldDefaultDomainFilePath, &statBlock) == 0 )
	{
		// if the default file is already there, then just delete the old file
		if ( lstat(kDefaultDomainFilePath, &statBlock) == 0 ) {
			unlink( kOldDefaultDomainFilePath );
		} else {
			rename( kOldDefaultDomainFilePath, kDefaultDomainFilePath );
		}
	}
	
	FILE    *sourceFP       = fopen( kDefaultDomainFilePath, "r" );
	char    *pReturn        = NULL;
    bool    bNameChanged    = false;
	
	if ( sourceFP != NULL )
	{
		char    buf[1024];
		
		if ( fgets(buf, sizeof(buf), sourceFP) != NULL )
		{
			char *tempVal	= buf;
			char *domain	= strsep( &tempVal, "\n\r" );
			
			if ( domain != NULL && domain[0] != '\0' )
				pReturn = strdup( domain );
		}
		
		fclose( sourceFP );
    }
	
	if ( pReturn == NULL )
		pReturn = strdup( "" );
	
	// now we'll check to see if the name copied matches our internal copy, if not... do things
	fStaticMutex.WaitLock();
	
	if ( fNISDomainConfigured == NULL || strcmp(pReturn, fNISDomainConfigured) != 0 )
		bNameChanged = true;
	
	fStaticMutex.SignalLock();
	
	if ( bNameChanged )
		SetDomainAndServers( pReturn, NULL );

	return pReturn;
}

void NISNode::SetNISServers( const char* inNISDomain, const char *inNISServers )
{
	char	fileName[PATH_MAX]	= { 0, };
	
	fStaticMutex.WaitLock();
	
	// assume we aren't available for now	
	fLastYPBindLaunchAttempt = 0;
	fNISAvailable = false;
	
	if ( inNISDomain != NULL && inNISDomain[0] != '\0' )
	{
		snprintf( fileName, sizeof(fileName), "/var/yp/binding/%s.ypservers", inNISDomain );
		
		// now write out new servers if we have them; otherwise
		// erase the old servers
		if ( inNISServers != NULL && inNISServers[0] != '\0' )
		{
			// we attempt to make the directories, we don't care if it errors
			mkdir( "/var/yp", 0664 );
			mkdir( "/var/yp/binding", 0664 );
			
			FILE *destFP = fopen( fileName, "w+" );
			if ( destFP != NULL )
			{
				char *token	= NULL;
				char *nisServers = strdup( inNISServers );
				
				for ( char *server = strtok_r(nisServers, " \r\n", &token); server != NULL; server = strtok_r(NULL, " \r\n", &token) )
				{
					fprintf( destFP, "%s\n", server );
				}
				
				fclose( destFP );
				
				DSFree( nisServers );
			}
		}
		else
		{
			unlink( fileName );
			
			// unlink any version files too
			snprintf( fileName, sizeof(fileName), "/var/yp/binding/%s.2", inNISDomain );
			unlink( fileName );
			
			snprintf( fileName, sizeof(fileName), "/var/yp/binding/%s.1", inNISDomain );
			unlink( fileName );
		}
	}
	
	// delete any existing reachability items
	if ( fServerReachability != NULL )
	{
		sNISReachabilityList *current = fServerReachability;
		
		while ( current != NULL )
		{
			sNISReachabilityList *deleteItem = current;
			current = current->next;
			
			if ( deleteItem->reachabilityRef != NULL )
			{
				SCNetworkReachabilityUnscheduleFromRunLoop( deleteItem->reachabilityRef, CFRunLoopGetMain(), kCFRunLoopDefaultMode );
				DSCFRelease( deleteItem->reachabilityRef );
			}
			
			delete deleteItem;
		}
		
		fServerReachability = NULL;
	}
	
	fStaticMutex.SignalLock();
}

char *NISNode::CopyNISServers( char *inNISDomain )
{
	char	fileName[PATH_MAX]	= { 0, };
	char	*pReturn			= NULL;
	
	snprintf( fileName, sizeof(fileName), "/var/yp/binding/%s.ypservers", inNISDomain );
	
	FILE *fp = fopen( fileName, "r" );
	if ( fp != NULL ) 
	{
		struct stat	fileStat;

		if ( stat(fileName, &fileStat) == 0 )
		{
			pReturn = (char *) calloc( fileStat.st_size, sizeof(char) );
			fread( pReturn, fileStat.st_size, sizeof(char), fp );
		}
		
		fclose( fp );
	}
	
	return pReturn;
}

bool NISNode::DomainNameChanged( void )
{
	bool	bReturn = false;
	
	fStaticMutex.WaitLock();

	if ( strcmp(fNISDomainConfigured, fNISDomain) != 0 )
		bReturn = true;

	fStaticMutex.SignalLock();
	
	return bReturn;
}

void NISNode::NISReachabilityCallback( SCNetworkReachabilityRef target, SCNetworkConnectionFlags flags, void *info )
{
	sNISReachabilityList	*pNISReachList	= ((sNISReachabilityList *) info);
	bool                     isReachable;
	
	if ( (flags & kSCNetworkFlagsReachable) == kSCNetworkFlagsReachable && 
		 (flags & kSCNetworkFlagsConnectionRequired) != kSCNetworkFlagsConnectionRequired )
	{
		isReachable = true;
	}
	else
	{
		isReachable = false;
		CancelNISThreads();
	}
	
	fStaticMutex.WaitLock();

	pNISReachList->isReachable = isReachable;

	DbgLog( kLogPlugin, "NISNode::NISReachabilityCallback reachability set to %d for %s", pNISReachList->isReachable, pNISReachList->serverName );
	
	fStaticMutex.SignalLock();
}

sNISReachabilityList *NISNode::CreateReachability( char *inName )
{
	sNISReachabilityList		*pReturnValue	= NULL;
	uint32_t					tempFamily		= AF_UNSPEC;
	SCNetworkReachabilityRef	scReachRef		= NULL;
	struct addrinfo				hints			= { 0 };
	struct addrinfo				*res			= NULL;
	bool						bIPBased		= false;
	
	hints.ai_family	= tempFamily;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags	= AI_NUMERICHOST;
	
	// we specifically say it is an IP, we don't want to attempt resolution, let SC do that
	if ( getaddrinfo(inName, NULL, &hints, &res) == 0 )
	{
		scReachRef = SCNetworkReachabilityCreateWithAddress( kCFAllocatorDefault, res->ai_addr ); 
		freeaddrinfo( res );
		res = NULL;
		bIPBased = true;
	}
	else
	{
		scReachRef = SCNetworkReachabilityCreateWithName( kCFAllocatorDefault, inName );
	}

	if ( scReachRef != NULL )
	{
		SCNetworkReachabilityContext	reachabilityContext = { 0, NULL, NULL, NULL, NULL };
		sNISReachabilityList			*newReachItem		= new sNISReachabilityList;
		
		newReachItem->next = NULL;
		newReachItem->reachabilityRef = scReachRef;
		newReachItem->isReachable = false;
		strlcpy( newReachItem->serverName, inName, sizeof(newReachItem->serverName) );
		
		// schedule with the run loop now that we are done
		reachabilityContext.info = newReachItem;
		
		if ( SCNetworkReachabilitySetCallback(scReachRef, NISReachabilityCallback, &reachabilityContext) == FALSE )
		{
			DbgLog( kLogPlugin, "NISNode::CreateReachability unable to set callback for SCNetworkReachabilityRef for %s",
				   fNISDomainConfigured );
		}
		else if ( SCNetworkReachabilityScheduleWithRunLoop(scReachRef, CFRunLoopGetMain(), kCFRunLoopDefaultMode) == FALSE )
		{
			DbgLog( kLogPlugin, "NISNode::CreateReachability unable to schedule SCNetworkReachabilityRef with Runloop for %s",
				   fNISDomainConfigured );
		}
		else
		{
			// we only need to check when value is an IP address not a hostname
			if (bIPBased == true) {
				SCNetworkReachabilityFlags flags;
				
				// this does not block because it is an IP address
				if (SCNetworkReachabilityGetFlags(scReachRef, &flags) == true) {
					NISReachabilityCallback(scReachRef, flags, newReachItem);
				}
			}
			
			DbgLog( kLogPlugin, "NISNode::CreateReachability watching %s - %s", fNISDomainConfigured, inName );
			pReturnValue = newReachItem;
			newReachItem = NULL;
			scReachRef = NULL;
		}
		
		DSCFRelease( scReachRef );
		DSDelete( newReachItem );
	}
	
	return pReturnValue;
}

bool NISNode::IsNISAvailable( void )
{
	int slot = AddToNISThreads();

	fStaticMutex.WaitLock();
	
	// see if we need to create our reachability list
	if ( fNISDomainConfigured != NULL && fNISDomainConfigured[0] != '\0' )
	{
		if ( fServerReachability == NULL )
		{
			char *nisServerList = CopyNISServers( fNISDomainConfigured );
			
			if ( nisServerList != NULL )
			{
				char	*state	= NULL;
				char	*server	= NULL;
				
				for ( server = strtok_r(nisServerList, " \n\r", &state); server != NULL; server = strtok_r(NULL, " \n\r", &state) )
				{
					sNISReachabilityList *current = CreateReachability( server );
					if ( current != NULL )
					{
						current->next = fServerReachability;
						fServerReachability = current;
					}
				}
				
				DSFree( nisServerList );
			}
		}
	}
	
	// set to false for now, until we verify below
	bool bReturn = false;
	bool bReachable = (fServerReachability == NULL ? true : false); // if we have no reachability, we assume yes
	
	// check all of our callbacks and see if we have any servers available, and check yp_bind
	for ( sNISReachabilityList *current = fServerReachability; current != NULL; current = current->next )
	{
		if ( current->isReachable == true )
		{
			bReachable = true;
			break;
		}
	}
	
	if ( bReachable == true )
	{
		// attempt to bind to the server
		int error = yp_bind( fNISDomainConfigured );
		if ( error == 0 )
		{
			// if we are available, if we weren't before, build our maps
			if ( fNISAvailable == false )
				BuildYPMapTable();
			
			bReturn = true;
		}
		else
		{
			DbgLog( kLogPlugin, "NISNode::IsNISAvailable - %s (%d)", yperr_string(error), error );
			bReturn = false;
		}
		
	}
	
	// set our new state
	fNISAvailable = bReturn;
	
	fStaticMutex.SignalLock();
	
	RemoveFromNISThreads( slot );

	return bReturn;
}

void NISNode::InitializeGlobals( void )
{
	if (gActiveThreads == NULL)
	{
		gActiveThreads = (pthread_t *) calloc( gMaxHandlerThreadCount, sizeof(pthread_t) );
	}

	if (gNotifyTokens == NULL)
	{
		gNotifyTokens = (int *) calloc( gMaxHandlerThreadCount, sizeof(int) );
	}
}

static int AddToNISThreads( void )
{
	int		slot = -1;
	UInt32	ii;

	pthread_mutex_lock( &gActiveThreadMutex );
	for ( ii = 0; ii < gActiveThreadCount; ii++ )
	{
		if ( gActiveThreads[ii] == NULL )
		{
			gActiveThreads[ii] = pthread_self();
			slot = ii;
			break;
		}
	}                                                                                    
                                                                                         
	// if we passed our active threads, no open slot
	if ( slot == -1 && gActiveThreadCount < gMaxHandlerThreadCount )
	{
		slot = gActiveThreadCount;
		gActiveThreads[slot] = pthread_self();
		gActiveThreadCount++;
	}                                                                                    

	DbgLog( kLogPlugin, "NISNode::AddToNISThreads called added thread %X to slot %d",
			(unsigned long) gActiveThreads[slot], slot );

	pthread_mutex_unlock( &gActiveThreadMutex );                                         

    return slot;
}

static void RemoveFromNISThreads( int inSlot )
{
	if ( inSlot == -1 ) 
	{
        DbgLog( kLogPlugin, "NISNode::RemoveFromNISThreads called for slot with -1" );
        return;
	}
	
    pthread_mutex_lock( &gActiveThreadMutex );

    DbgLog( kLogPlugin, "NISNode::RemoveFromNISThreads called for slot %d = %X", inSlot, (unsigned long) gActiveThreads[inSlot] );

    gActiveThreads[inSlot] = NULL;
    
    if ( gNotifyTokens[inSlot] != 0 )
    {
        notify_cancel( gNotifyTokens[inSlot] );
        DbgLog( kLogPlugin, "NISNode::RemoveFromNISThreads cancelling token %d", gNotifyTokens[inSlot] );
        gNotifyTokens[inSlot] = 0;
    }

    pthread_mutex_unlock( &gActiveThreadMutex );
}

extern "C" uint32_t notify_register_plain( const char *name, int *out_token );

// Defined in ypinternal.h.
#define ThreadStateExitRequested 4

static void CancelNISThreads( void )
{
    pthread_mutex_lock( &gActiveThreadMutex );
	
	DbgLog( kLogPlugin, "NISNode::CancelNISThreads called - %d active threads", gActiveThreadCount );
	
    for ( UInt32 ii = 0; ii < gActiveThreadCount; ii++ )
    {
        if ( gActiveThreads[ii] != NULL && gNotifyTokens[ii] == 0 )
        {
            char notify_name[128];
            int notify_token = 0;
            snprintf(notify_name, sizeof(notify_name), "self.thread.%lu", (unsigned long) gActiveThreads[ii]);

            int status = notify_register_plain(notify_name, &notify_token);
            if (status == NOTIFY_STATUS_OK)
            {
                notify_set_state(notify_token, ThreadStateExitRequested);
                gNotifyTokens[ii] = notify_token;
                DbgLog( kLogPlugin, "NISNode::CancelNISThreads called for slot %d notification '%s'", ii, notify_name );
            }
            
            // send a signal to the thread to to cause it to break out of a select immediately
            pthread_kill( gActiveThreads[ii], SIGURG );
        }
    }
    pthread_mutex_unlock( &gActiveThreadMutex );
}
