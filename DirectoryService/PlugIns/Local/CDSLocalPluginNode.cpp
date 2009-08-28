/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header CDSLocalPluginNode
 */


#include "CDSLocalPluginNode.h"

#include "AuthHelperUtils.h"
#include "CLog.h"
#include "CDSPluginUtils.h"
#include "DirServicesPriv.h"
#include "DirServicesConstPriv.h"
#include "DirServiceMain.h"
#include "DSUtils.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>
#include <pwd.h>
#include <list>
#include <map>
#include <libkern/OSAtomic.h>
#include <syslog.h>
#include "COSUtils.h"
#include "CDSAuthDefs.h"
#include <PasswordServer/KerberosInterface.h>
#include <PasswordServer/PSUtilitiesDefs.h>
#include <dispatch/dispatch.h>
#include <uuid/uuid.h>
#include <Mbrd_MembershipResolver.h>

#define kGeneratedUIDStr			"generateduid"
#define kAuthAuthorityStr			"authentication_authority"

#if FILE_ACCESS_INDEXING

struct _sIndexMapping
{
	const char		*fRecordType;	// record type we care about
	CFStringRef		fRecordTypeCF;
	char const		**fAttributes;	// NULL terminated
	CFArrayRef		fAttributesCF;
	char			*fRecordNativeType;
};

#define kIndexDir						"/var/db/dslocal/indices/Default/"
#define kIndexPath						"/var/db/dslocal/indices/Default/index"
#define kIndexPrefsPath					"/var/db/dslocal/indices/Default/index.plist"
#define	kIndexPrefsFactoryPath			"/System/Library/DirectoryServices/DefaultLocalDB/index.plist"

extern dsBool		gDSInstallDaemonMode;
extern dsBool		gProperShutdown;
extern dsBool		gSafeBoot;
extern dsBool		gDSDebugMode;
extern dsBool		gDSLocalOnlyMode;

#endif

#define kNonURLCharactersNotToEncode		" "
#define kURLCharactersToEncode				"/"

#pragma mark -
#pragma mark Class Routines

CDSLocalPluginNode::CDSLocalPluginNode( CFStringRef inNodeDirFilePath, CDSLocalPlugin* inPlugin ) :	
	mOpenRecordsLock("CDSLocalPluginNode:mOpenRecordsLock"), 
	mRecordTypeLock("CDSLocalPluginNode:mRecordTypeLock")
	
{
	char	*cStr	= NULL;
	
	mNodeDirFilePath = (CFStringRef) CFRetain( inNodeDirFilePath );
	mPlugin = inPlugin;
	mRecordNameAttrNativeName = mPlugin->AttrNativeTypeForStandardType( CFSTR(kDSNAttrRecordName) );
	mModCounter = 0;
	mNodeDirFilePathCStr = strdup( CStrFromCFString(inNodeDirFilePath, &cStr, NULL, NULL) );
	
	DSFree( cStr );
	
	mUseIndex = false;
	mProperShutdown = gProperShutdown;
	mSafeBoot = gSafeBoot;
	mDatabaseHelper = NULL; // need to default to none since we may not open DB
	
	if ( gDSLocalOnlyMode == false && gDSDebugMode == false && gDSInstallDaemonMode == false )
		EnableTheIndex( inNodeDirFilePath );
}

void CDSLocalPluginNode::EnableTheIndex( CFStringRef inNodeDirFilePath )
{
	CFMutableStringRef	aFilePath	= CFStringCreateMutableCopy( NULL, 0, inNodeDirFilePath );
	
	// replace "nodes" with "indices"
	CFStringFindAndReplace( aFilePath, CFSTR("nodes"), CFSTR("indices"), CFRangeMake(0,CFStringGetLength(aFilePath)), 0 );
	
	if ( CFStringCompare(CFSTR(kIndexDir), aFilePath, 0) == kCFCompareEqualTo )
	{
		if ( EnsureDirs(kIndexDir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, S_IRWXU) == 0 )
		{
			CFDataRef fileData = CreateCFDataFromFile( kIndexPrefsPath );
			if ( fileData == NULL )
			{
				if ( (fileData = CreateCFDataFromFile(kIndexPrefsFactoryPath)) != NULL )
				{
					syslog( LOG_INFO, "Local plugin index preference file '%s' was missing, using factory version", kIndexPrefsPath );
					DbgLog( kLogError, "Local plugin index preference file '%s' was missing, using factory version", kIndexPrefsPath );
					
					int fd = open( kIndexPrefsPath, O_RDWR | O_NOFOLLOW | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR );
					if ( fd != -1 )
					{
						if ( write(fd, CFDataGetBytePtr(fileData), CFDataGetLength(fileData)) == -1 ) {
							const char *errorStr = strerror( errno );
							
							syslog( LOG_INFO, "Could not write factory index preference file '%s' - %s", kIndexPrefsPath, errorStr );
							DbgLog( kLogError, "Could not write factory index preference file '%s' - %s", kIndexPrefsPath, errorStr );
						}
					}
				}
				else {
					syslog( LOG_ERR, "Local plugin index preference file '%s' and factory version were missing, proceeding without index", kIndexPrefsPath );
					DbgLog( kLogError, "Local plugin index preference file '%s' and factory version were missing, proceeding without index", kIndexPrefsPath );
				}
			}
			
			uint32_t expVersion = 0;
			if ( fileData != NULL )
			{
				CFDictionaryRef recordDict	= (CFDictionaryRef) CFPropertyListCreateFromXMLData( NULL, fileData, kCFPropertyListImmutable, NULL );
				
				if ( CFDictionaryGetTypeID() == CFGetTypeID(recordDict) )
				{
					CFIndex dictCount = CFDictionaryGetCount( recordDict );
					if ( dictCount > 0 )
					{
						CFTypeRef	keys[dictCount];
						CFTypeRef	values[dictCount];
						CFTypeID	stringType = CFStringGetTypeID();
						CFTypeID	arrayType = CFArrayGetTypeID();
						
						CFDictionaryGetKeysAndValues( recordDict, keys, values );
						
						for ( CFIndex ii = 0; ii < dictCount; ii++ )
						{
							CFStringRef key = (CFStringRef) keys[ii];
							CFArrayRef value = (CFArrayRef) values[ii];
							
							if ( CFGetTypeID(key) == stringType && CFGetTypeID(value) == arrayType ) {
								AddIndexMapping( key, value );
							}
							else {
								DbgLog( kLogError, "Invalid key or value in '%s'", kIndexPrefsPath );
							}
						}
					}
					
					// we use CRC of the data as a version
					expVersion = CalcCRCWithLength( CFDataGetBytePtr(fileData), CFDataGetLength(fileData) );
				}
				
				DSCFRelease( recordDict );
				DSCFRelease( fileData );
			}
			
			// build the database file path
			mDatabaseHelper = new SQLiteHelper( kIndexPath, expVersion );
		
			mIndexLoaded.ResetEvent();
			
			dispatch_async( dispatch_get_concurrent_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT), 
							^(void) {
								LoadFileAccessIndex();
							} );

			if ( mIndexLoaded.WaitForEvent(10 * kMilliSecsPerSec) == false )
				DbgLog( kLogPlugin, "CDSLocalPluginNode::CDSLocalPluginNode - timed out waiting for index to load continuing without until available" );
		}
	}
	
	DSCFRelease( aFilePath );
}

CDSLocalPluginNode::~CDSLocalPluginNode( void )
{
#if FILE_ACCESS_INDEXING
	CloseDatabase();
#endif

	DSCFRelease( mNodeDirFilePath );
}

#pragma mark -
#pragma mark Public Methods

void CDSLocalPluginNode::CloseDatabase( void )
{
#if FILE_ACCESS_INDEXING
	if ( mDatabaseHelper != NULL )
	{
		char	nodeName[128] = { 0, };
		
		CFStringGetCString( mNodeDirFilePath, nodeName, sizeof(nodeName), kCFStringEncodingUTF8 );
		
		DbgLog( kLogPlugin, "CDSLocalPluginNode::CloseDatabase is shutting down database for %s", nodeName );
		mDatabaseHelper->CloseDatabase();
	}
#endif
}

tDirStatus CDSLocalPluginNode::GetRecords(	CFStringRef			inNativeRecType,
											CFArrayRef			inPatternsToMatch,
											CFStringRef			inAttrTypeToMatch,
											tDirPatternMatch	inPatternMatch,
											bool				inAttrInfoOnly,
											UInt32				maxRecordsToGet,
											CFMutableArrayRef	recordsArray,
											bool				useLongNameAlso,
											CFStringRef*		outRecFilePath)
{
	tDirStatus				siResult					= eDSNoErr;
	CFDataRef				fileData					= NULL;
	CFMutableDictionaryRef	mutableRecordDict			= NULL;
	DIR*					recTypeDir					= NULL;
	UInt32					numRecordsFound				= 0;
	CFIndex					numInPatternsToMatch		= 0;
	bool					bGetAllRecords				= false;
	bool					bCheckUsersType				= false;
	bool					bMatchRealName				= useLongNameAlso;
	bool					lockedRecordType			= false;
	CFStringRef				nativeAttrToMatch			= NULL;
	CFMutableStringRef		nativeType					= NULL;
	char					thisFilePathCStr[768];
	char					thisRecTypeDirPathCStr[512];
	bool					bContinueSearch				= true;
	dirent				   *theDirEnt					= NULL;
#if FILE_ACCESS_INDEXING
	bool					bIsIndexed					= false;
#endif
	
	if ( inNativeRecType == NULL || recordsArray == NULL || inPatternsToMatch == NULL || 
		 (numInPatternsToMatch = CFArrayGetCount(inPatternsToMatch)) == 0 )
	{
		return eDSNullParameter;
	}
	
	try
	{

		if ( CFStringCompare( inNativeRecType, CFSTR( "users" ), 0 ) == kCFCompareEqualTo )
			bCheckUsersType = true;

		for ( CFIndex i = 0; i < numInPatternsToMatch; i++ )
		{
			CFStringRef aPatternToMatch = (CFStringRef)CFArrayGetValueAtIndex( inPatternsToMatch, i );
			if ( aPatternToMatch != NULL &&
				 CFGetTypeID(aPatternToMatch) == CFStringGetTypeID() &&
				 CFStringCompare(aPatternToMatch, CFSTR(kDSRecordsAll), 0) == kCFCompareEqualTo )
			{
				bGetAllRecords = true;
				break;
			}
		}
		
		//get the open records from the plugin first
		if ( CFStringHasPrefix(inAttrTypeToMatch, CFSTR(kDSStdAttrTypePrefix)) == true )
			nativeAttrToMatch = mPlugin->AttrNativeTypeForStandardType( inAttrTypeToMatch );
		else if ( CFStringHasPrefix(inAttrTypeToMatch, CFSTR(kDSNativeAttrTypePrefix)) == true ) {
			nativeAttrToMatch = nativeType = CFStringCreateMutableCopy( kCFAllocatorDefault, 0, inAttrTypeToMatch );
			CFStringTrim( nativeType, CFSTR(kDSNativeAttrTypePrefix) ); // we have to use trim to do the right thing...
		}
		else
			nativeAttrToMatch = inAttrTypeToMatch;
		
		if ( CFStringCompare( nativeAttrToMatch, CFSTR( "name" ), 0 ) == kCFCompareEqualTo )
		{
			if (bCheckUsersType)
				bMatchRealName = true; //check both recordname and realname when searching on user records for recordname
			
			//TODO need to expand on this use of already opened records
			if (maxRecordsToGet == 1) //definitive search on the recordname ie. clearly support auth routine attribute settings
			{
				CFArrayRef openRecordsOfThisType = mPlugin->CreateOpenRecordsOfTypeArray( inNativeRecType );
				CFIndex numOpenRecords = 0;
				if ( openRecordsOfThisType != NULL )
				{
					numOpenRecords = CFArrayGetCount( openRecordsOfThisType );
				}

				//get record data from records if we have already read from file and have them open
				for ( CFIndex i = 0; ( maxRecordsToGet == 0 || numRecordsFound < maxRecordsToGet ) && ( i < numOpenRecords ); i++ )
				{
					CFMutableDictionaryRef openRecordDict = (CFMutableDictionaryRef)CFArrayGetValueAtIndex( openRecordsOfThisType, i );
					CFMutableDictionaryRef aMutableRecordDict = (CFMutableDictionaryRef) CFDictionaryGetValue( openRecordDict, 
																											   CFSTR(kOpenRecordDictAttrsValues) );
					if (	bGetAllRecords ||
							this->RecordMatchesCriteria( aMutableRecordDict, inPatternsToMatch, nativeAttrToMatch, inPatternMatch ) ||
							( bMatchRealName && this->RecordMatchesCriteria( aMutableRecordDict, inPatternsToMatch, CFSTR("realname"), inPatternMatch ) ) )
 					{
						CFMutableDictionaryRef aAttrDictCopy;
						
						aAttrDictCopy = (CFMutableDictionaryRef) CFPropertyListCreateDeepCopy( kCFAllocatorDefault, 
																							   aMutableRecordDict,
																							   kCFPropertyListMutableContainers );
						CFArrayAppendValue( recordsArray, aAttrDictCopy );
						numRecordsFound++;
						CFRelease( aAttrDictCopy );
						
						// if record was changed, let's write it out to disk
						CFBooleanRef wasChanged = (CFBooleanRef) CFDictionaryGetValue( openRecordDict, CFSTR(kOpenRecordDictRecordWasChanged) );
						if ( wasChanged != NULL && CFBooleanGetValue(wasChanged) == TRUE ) {
							// get the path to the record file
							CFStringRef openRecordFilePath = (CFStringRef) CFDictionaryGetValue( openRecordDict,
																								 CFSTR(kOpenRecordDictRecordFile) );
							if ( openRecordFilePath == NULL ) break;
							
							// get the record type
							CFStringRef recordType = (CFStringRef) CFDictionaryGetValue( openRecordDict,
																						 CFSTR(kOpenRecordDictRecordType) );
							if ( recordType == NULL ) break;
							
							DbgLog( kLogInfo, "CDSLocalPlugin::GetRecords - Flushing open record to disk because it was modified" );

							// write any changes to the record
							siResult = FlushRecord( openRecordFilePath, recordType, aMutableRecordDict );
							
							// flush the cache at this point
							mPlugin->FlushCaches( mPlugin->RecordStandardTypeForNativeType(recordType) );
							
							// reset to false because multiple people may have an open record
							CFDictionarySetValue( openRecordDict, CFSTR(kOpenRecordDictRecordWasChanged), kCFBooleanFalse );
						}
						else {
							DbgLog( kLogInfo, "CDSLocalPlugin::GetRecords - Did not flush open record because it was not modified" );
						}
					}
				}
				DSCFRelease( openRecordsOfThisType );
			}
		}

		if (numRecordsFound < maxRecordsToGet)
		{
			mRecordTypeLock.WaitLock();
			lockedRecordType = true;
			CFMutableStringRef mutableRecTypeDirPathCFStr = CFStringCreateMutable( NULL, 0 );
			if ( mutableRecTypeDirPathCFStr == NULL ) throw( eMemoryAllocError );
			
			CFStringAppend( mutableRecTypeDirPathCFStr, mNodeDirFilePath );
			CFStringAppend( mutableRecTypeDirPathCFStr, inNativeRecType );
			
			if ( !CFStringGetCString(mutableRecTypeDirPathCFStr, thisRecTypeDirPathCStr, sizeof(thisRecTypeDirPathCStr), kCFStringEncodingUTF8) )
				throw( eMemoryError );
			
			DSCFRelease( mutableRecTypeDirPathCFStr );
			
#if FILE_ACCESS_INDEXING
			if ( mUseIndex && (inPatternMatch == eDSExact || inPatternMatch == eDSiExact) && bGetAllRecords == false )
			{
				// here we search for each value
				CFIndex	iIndex	= 0;
				CFIndex iCount	= CFArrayGetCount( inPatternsToMatch );
				
				// we look for each value in the index
				while ( iIndex < iCount && numRecordsFound < maxRecordsToGet )
				{
					CFStringRef	cfValue = (CFStringRef) CFArrayGetValueAtIndex( inPatternsToMatch, iIndex );
					
					char **filesToOpen = GetFileAccessIndex( inNativeRecType, inPatternMatch, cfValue, nativeAttrToMatch, &bIsIndexed );
					if ( filesToOpen == NULL && bIsIndexed == true && bMatchRealName == true )
						filesToOpen = GetFileAccessIndex( inNativeRecType, inPatternMatch, cfValue, CFSTR("realname"), 
														  &bIsIndexed );
		
					// if the value we are not searching is not indexed, we'll just break
					if ( bIsIndexed == false )
						break;
					
					// if index gives us the file then let's open
					if ( filesToOpen != NULL )
					{
						for ( int ii = 0; filesToOpen[ii] != NULL && numRecordsFound < maxRecordsToGet; ii++ )
						{
							char *theFile = filesToOpen[ii];
							
							snprintf( thisFilePathCStr, sizeof(thisFilePathCStr), "%s/%s", thisRecTypeDirPathCStr, theFile );
							
							fileData = CreateCFDataFromFile( thisFilePathCStr );
							if ( fileData != NULL )
							{
								mutableRecordDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( NULL, fileData, 
																											  kCFPropertyListMutableContainers, NULL );
								DSCFRelease( fileData );
								
								//here we need to determine if the read file was proper XML and accurately parsed
								//otherwise we DO NOT throw error here but skip this file and keep on going to the other files if they exist
								if ( mutableRecordDict != NULL )
								{
									//process the plist record file if it seems valid
									if ( bGetAllRecords ||
										this->RecordMatchesCriteria( mutableRecordDict, inPatternsToMatch, nativeAttrToMatch, inPatternMatch ) ||
										( bMatchRealName && this->RecordMatchesCriteria( mutableRecordDict, inPatternsToMatch, CFSTR("realname"), inPatternMatch ) ) )
									{
										CFArrayAppendValue( recordsArray, mutableRecordDict );
										numRecordsFound++;
										if ( (maxRecordsToGet == 1) && (outRecFilePath != NULL) )
										{
											*outRecFilePath = CFStringCreateWithCString( NULL, thisFilePathCStr, kCFStringEncodingUTF8 );
										}
									}
									
									DSCFRelease( mutableRecordDict );
								}
							}
							else
							{
								// couldn't stat the file, must be gone
								CFStringRef	cfStdType	= mPlugin->RecordStandardTypeForNativeType( inNativeRecType );
								char		*cStr2		= NULL;
								
								DeleteRecordIndex( CStrFromCFString(cfStdType, &cStr2, NULL, NULL), theFile );
								DSFree( cStr2 );
							}
						}
						
						DSFreeStringList( filesToOpen );
					} // if (fileToOpen != NULL)
					
					iIndex++;
				}
			}//if (mUseIndex)
			
			if ( bIsIndexed == false )
			{
#endif
				//right here we need to try to open the file directly if we are searching on recordname with eDSExact
				//nothing indexed within this code block
				//also exclude common case of asking for all records of a certain type
				// if we are looking for exact match on recordname then we should also make sure that only a single record is asked for right?
				//ie. why keep on looking if we know that we should stop using bContinueSearch
				if (	(numRecordsFound < maxRecordsToGet) &&
						( (CFArrayGetCount( inPatternsToMatch ) == 1) && (inPatternMatch == eDSExact) ) && 
						( CFStringCompare( nativeAttrToMatch, CFSTR( "name" ), 0 ) == kCFCompareEqualTo ) &&
						( CFStringCompare( (CFStringRef)CFArrayGetValueAtIndex(inPatternsToMatch, 0), CFSTR( "dsRecordsAll" ), 0 ) != kCFCompareEqualTo ) )
				{
					char* cStr = NULL;

					const char* aString = CStrFromCFString( (CFStringRef)CFArrayGetValueAtIndex(inPatternsToMatch, 0), &cStr, NULL, NULL );

					if (aString != NULL)
					{
						snprintf( thisFilePathCStr, sizeof(thisFilePathCStr), "%s/%s.plist", thisRecTypeDirPathCStr, aString );

						if ( (fileData = CreateCFDataFromFile(thisFilePathCStr)) != NULL )
						{
							mutableRecordDict = (CFMutableDictionaryRef)::CFPropertyListCreateFromXMLData( NULL, fileData, kCFPropertyListMutableContainers, NULL );
							DSCFRelease( fileData );
								
							//here we need to determine if the read file was proper XML and accurately parsed
							//otherwise we DO NOT throw error here but skip this file and keep on going to the other files if they exist
							if ( mutableRecordDict != NULL )
							{
								//process the plist record file if it seems valid
								if (	bGetAllRecords ||
										this->RecordMatchesCriteria( mutableRecordDict, inPatternsToMatch, nativeAttrToMatch, inPatternMatch ) ||
										( bMatchRealName && this->RecordMatchesCriteria( mutableRecordDict, inPatternsToMatch, CFSTR("realname"), inPatternMatch ) ) )
								{
									CFArrayAppendValue( recordsArray, mutableRecordDict );
									numRecordsFound++;
									bContinueSearch = false;
									if ( (maxRecordsToGet == 1) && (outRecFilePath != NULL) )
									{
										*outRecFilePath = CFStringCreateWithCString( NULL, thisFilePathCStr, kCFStringEncodingUTF8 );
									}
								}
								DSCFRelease( mutableRecordDict );
							}
						}
					}
					DSFreeString(cStr);
				} //direct file access attempt
				
				//here is the actual directory search
				if ( (numRecordsFound < maxRecordsToGet) && bContinueSearch )
				{
					for ( recTypeDir = opendir( thisRecTypeDirPathCStr ); ( recTypeDir != NULL ) &&  ( maxRecordsToGet == 0 || numRecordsFound < maxRecordsToGet ) && ( theDirEnt = readdir( recTypeDir ) ); )
					{
						char	recFileSuffix[]	= ".plist";
						long	suffixOffset	= strlen( theDirEnt->d_name )-strlen( recFileSuffix );
						const char   *fileSuffix		= "<no suffix>";

						if ( suffixOffset > 0 )
						{
							fileSuffix = &theDirEnt->d_name[suffixOffset];
						}

						//if suffix offset is invalid or it is NOT a plist file with a correct suffix then we skip to the next one
						if ( ( suffixOffset <= 0 ) || ( ::strcmp( recFileSuffix, fileSuffix ) != 0 ) ) continue;
					
						snprintf( thisFilePathCStr, sizeof(thisFilePathCStr), "%s/%s", thisRecTypeDirPathCStr, theDirEnt->d_name );
						
						if ( (fileData = CreateCFDataFromFile(thisFilePathCStr)) != NULL )
						{
							mutableRecordDict = (CFMutableDictionaryRef)::CFPropertyListCreateFromXMLData( NULL, fileData, kCFPropertyListMutableContainers, NULL );
							DSCFRelease( fileData );
								
							//here we need to determine if the read file was proper XML and accurately parsed
							//otherwise we DO NOT throw error here but skip this file and keep on going to the other files if they exist
							if ( mutableRecordDict != NULL )
							{
								//process the plist record file if it seems valid
								if (	bGetAllRecords ||
										this->RecordMatchesCriteria( mutableRecordDict, inPatternsToMatch, nativeAttrToMatch, inPatternMatch ) ||
										( bMatchRealName && this->RecordMatchesCriteria( mutableRecordDict, inPatternsToMatch, CFSTR("realname"), inPatternMatch ) ) )
								{
									CFArrayAppendValue( recordsArray, mutableRecordDict );
									numRecordsFound++;
									if ( (maxRecordsToGet == 1) && (outRecFilePath != NULL) )
									{
										*outRecFilePath = CFStringCreateWithCString( NULL, thisFilePathCStr, kCFStringEncodingUTF8 );
									}
								}

								DSCFRelease( mutableRecordDict );
							}
						}
					}
				}//actual directory search

#if FILE_ACCESS_INDEXING
			} //if (!bIsIndexed)
#endif
			DbgLog(  kLogDebug, "CDSLocalPluginNode::GetRecords(): returned %d records from directory %s", numRecordsFound, thisRecTypeDirPathCStr );
			mRecordTypeLock.SignalLock();
			lockedRecordType = false;
		} // if (numRecordsFound < maxRecordsToGet)
	}
	catch( tDirStatus error )
	{
		DbgLog(  kLogPlugin, "CDSLocalPluginNode::GetRecords(): failed with error %d", error );
		siResult = error;
	}
	catch( ... )
	{
		siResult = eUndefinedError;
	}

	if ( lockedRecordType )
		mRecordTypeLock.SignalLock();

	DSCFRelease(mutableRecordDict);
	DSCFRelease(fileData);
	DSCFRelease( nativeType );

	if ( recTypeDir != NULL )
	{
		closedir( recTypeDir );
		recTypeDir = NULL;
	}
	
	return siResult;
}

CFStringRef CDSLocalPluginNode::CreateFilePathForRecord( CFStringRef inNativeRecType, CFStringRef inRecordName )
{
	CFMutableStringRef recordNameForFS = CFStringCreateMutableCopy( NULL, 0, inRecordName );
	CFStringFindAndReplace( recordNameForFS, CFSTR("/"), CFSTR("%2F"), CFRangeMake(0, CFStringGetLength(inRecordName)), 0 );
	
	CFStringRef recFilePath = CFStringCreateWithFormat( NULL, NULL, CFSTR( "%@%@/%@.plist" ), mNodeDirFilePath,
		inNativeRecType, recordNameForFS );
	
	CFRelease( recordNameForFS );
	
	return recFilePath;
}

tDirStatus CDSLocalPluginNode::CreateDictionaryForRecord(  CFStringRef inNativeRecType, CFStringRef inRecordName,
        CFMutableDictionaryRef* outMutableRecordDict, CFStringRef* outRecordFilePath )
{
	tDirStatus siResult = eDSNoErr;
	CFMutableArrayRef recordsArray = NULL;
	CFArrayRef patternsToMatch = NULL;

	//this open SHOULD NOT assume that there is only one shortname or that the proper first shortname is provided

	try
	{
		recordsArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		patternsToMatch = CFArrayCreate( NULL, (const void**)&inRecordName, 1, &kCFTypeArrayCallBacks );
		
		this->GetRecords( inNativeRecType, patternsToMatch, mPlugin->AttrNativeTypeForStandardType( CFSTR( kDSNAttrRecordName ) ), eDSExact, true, 1, recordsArray, false, outRecordFilePath );
		
		*outMutableRecordDict = NULL;
		if ( CFArrayGetCount( recordsArray ) == 1 )
			*outMutableRecordDict = CFDictionaryCreateMutableCopy(NULL, 0, (CFDictionaryRef)CFArrayGetValueAtIndex( recordsArray, 0 ));
		if ( *outMutableRecordDict == NULL )
			throw( eDSRecordNotFound );
	}
	catch( tDirStatus error )
	{
		DbgLog( kLogDebug, "CDSLocalPluginNode::CreateDictionaryForRecord(): failed with error %d", error );
		siResult = error;
	}

	DSCFRelease( patternsToMatch );
	DSCFRelease( recordsArray );

	return siResult;
}

tDirStatus CDSLocalPluginNode::CreateDictionaryForNewRecord( CFStringRef inNativeRecType, CFStringRef inRecordName,
	CFMutableDictionaryRef* outMutableRecordDict, CFStringRef* outRecordFilePath )
{
	tDirStatus siResult = eDSNoErr;
	char* cStr = NULL;
	char* cStr2 = NULL;
	size_t cStrSize = 0;
	CFMutableDictionaryRef mutableAttrsValuesDict = NULL;
	CFStringRef recFilePath = NULL;
	CFMutableArrayRef mutableAttrValues = NULL;
	CFArrayRef automountMapArray = NULL;
	CFStringRef	metaInformation = NULL;	// do not release this it is released with array
	
	if ( inNativeRecType == NULL )
		return eDSInvalidRecordType;
	
	try
	{
		// if this is an automount then the name is encoded as <name>,automountMapName=<mapname>
		// where mapname is the MetaMap information, and we maintain this as the official record name
		if ( CFStringCompare( inNativeRecType, CFSTR("automount"), 0 ) == kCFCompareEqualTo )
		{
			automountMapArray = CFStringCreateArrayBySeparatingStrings( NULL, inRecordName, CFSTR(",automountMapName=") );
			
			if ( automountMapArray == NULL || CFArrayGetCount(automountMapArray) != 2 ) throw eDSInvalidRecordName;
			
			metaInformation = (CFStringRef) CFArrayGetValueAtIndex( automountMapArray, 1 );
			
			if ( CFStringGetLength(metaInformation) == 0 ) throw eDSInvalidRecordName;
		}
		
		// check the name length, we can't accept a file name that exceeds our ability to use temp file and plist extension
		if ( CFStringGetLength(inRecordName) + sizeof(".plist.temp") > NAME_MAX ) {
			CFDebugLog( kLogError, "CDSLocalPluginNode::CreateDictionaryForNewRecord(), file name is too long: %@",
					   inRecordName );
			throw eDSInvalidRecordName;
		}
		
		// build the path to the file for the record
		recFilePath = CreateFilePathForRecord( inNativeRecType, inRecordName );
		if ( recFilePath == NULL )
			throw( eMemoryAllocError );
				
		// check to see if the file already exists
		const char* recFilePathCStr = CStrFromCFString( recFilePath, &cStr, &cStrSize, NULL );
		struct stat statBuffer={};
		int result = lstat( recFilePathCStr, &statBuffer );
		if ( result != 0 )
			result = errno;

		switch ( result )
		{
			case ENOENT:
				break;
			
			case ENAMETOOLONG:
				DbgLog( kLogNotice, "CDSLocalPluginNode::CreateDictionaryForNewRecord(), file name is too long: %s",
						recFilePathCStr );
				throw( eDSInvalidRecordName );
				break;

			default:
				// if file is zero length, we can just remove it
				if ( statBuffer.st_size == 0 ) {
					DbgLog( kLogDebug, "CDSLocalPluginNode::CreateDictionaryForNewRecord(), file at %s was zero length recreating",
						    recFilePathCStr );
					unlink( recFilePathCStr );
					result = ENOENT;
				}
				else {
					DbgLog( kLogDebug, "CDSLocalPluginNode::CreateDictionaryForNewRecord(), file at %s error, stat() result = %d",
						    recFilePathCStr, result );
					throw( eDSRecordAlreadyExists );
				}
				break;
		}
		
		mutableAttrsValuesDict = ::CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks );
		if ( mutableAttrsValuesDict == NULL )
			throw( eMemoryAllocError );
		
		mutableAttrValues = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		if ( mutableAttrValues == NULL )
			throw( eMemoryAllocError );
		
		CFArrayAppendValue( mutableAttrValues, inRecordName );

		::CFDictionaryAddValue( mutableAttrsValuesDict, mRecordNameAttrNativeName, mutableAttrValues );

		::CFRelease( mutableAttrValues );
		mutableAttrValues = NULL;
		
		// if we have metaInformation we need to add that too
		if ( metaInformation != NULL )
		{
			mutableAttrValues = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			if ( mutableAttrValues == NULL )
				throw( eMemoryAllocError );
			
			CFArrayAppendValue( mutableAttrValues, metaInformation );
			CFDictionaryAddValue( mutableAttrsValuesDict, CFSTR("metaautomountmap"), mutableAttrValues );
			
			CFRelease( mutableAttrValues );
			mutableAttrValues = NULL;			
		}

		// write the record out to the file system
		this->FlushRecord( recFilePath, inNativeRecType, mutableAttrsValuesDict );
		
		if ( CFStringCompare(inNativeRecType, CFSTR("users"), 0) == kCFCompareEqualTo )
		{
			const char* recNameCStr = CStrFromCFString( inRecordName, &cStr, &cStrSize, NULL );
			if (recNameCStr == NULL) recNameCStr = "<unknown>";
			CFMutableDictionaryRef eventDict = dsCreateEventLogDict( CFSTR("user.created"), recNameCStr, NULL );
			if ( eventDict != NULL ) {
				dsPostEvent( CFSTR("user.created"), eventDict );
				CFRelease( eventDict );
			}
		}
		
		const char* nativeType = CStrFromCFString( inNativeRecType, &cStr2, NULL, NULL );

		// TODO:  need to be based on the node name, but only one node right now
		dsNotifyUpdatedRecord( "Local", NULL, nativeType );
		
		if ( outMutableRecordDict != NULL )
		{
			*outMutableRecordDict = mutableAttrsValuesDict;
			mutableAttrsValuesDict = NULL;	// we want to return this so mark this NULL so it won't get freed below
		}
		if ( outRecordFilePath!= NULL )
		{
			*outRecordFilePath = recFilePath;
			recFilePath = NULL;				// we want to return this so mark this NULL so it won't get freed below
		}
	}
	catch( tDirStatus error )
	{
		DbgLog(  kLogPlugin, "CDSLocalPluginNode::CreateDictionaryForNewRecord(): failed with error %d", error );
		siResult = error;
	}

	DSFree( cStr2 );
	
	if ( cStr != NULL )
	{
		free( cStr );
		cStr = NULL;
		cStrSize = 0;
	}
	if ( mutableAttrsValuesDict != NULL )
	{
		::CFRelease( mutableAttrsValuesDict );
		mutableAttrsValuesDict = NULL;
	}
	if ( recFilePath != NULL )
	{
		::CFRelease( recFilePath );
		recFilePath = NULL;
	}
	if ( mutableAttrValues != NULL )
	{
		::CFRelease( mutableAttrValues );
		mutableAttrValues = NULL;
	}
	
	if ( automountMapArray != NULL )
	{
		CFRelease( automountMapArray );
		automountMapArray = NULL;
	}

	return siResult;
}


tDirStatus
CDSLocalPluginNode::DeleteRecord( CFStringRef inRecordFilePath, CFStringRef inNativeRecType,
	CFStringRef inRecordName, CFMutableDictionaryRef inMutableRecordAttrsValues )
{
	tDirStatus	siResult		= eDSNoErr;
	char		*cStr			= NULL;
	size_t		cStrSize		= 0;
	bool		isUserRecord	= false;
	const char* recNameCStr		= NULL;
	CFArrayRef	attrArray		= NULL;
	CFStringRef krbCertAA		= NULL;
		
	if ( inNativeRecType == NULL )
		return eDSNullRecType;
	
	if ( inRecordName != NULL )
		CFRetain( inRecordName );
	
	mRecordTypeLock.WaitLock();
	this->UpdateModValue();

	this->RemoveShadowHashFilesIfNecessary( CFSTR(kGeneratedUIDStr), inMutableRecordAttrsValues );
	
	const char* recordFilePathCStr = CStrFromCFString( inRecordFilePath, &cStr, &cStrSize, NULL );
	DbgLog(  kLogPlugin, "CDSLocalPluginNode::DeleteRecord(): deleting file \"%s\"", recordFilePathCStr );

	if ( unlink( recordFilePathCStr ) != 0 )
	{
		DbgLog(  kLogError, "CDSLocalPluginNode::DeleteRecord(): unlink() for \"%s\" failed with error %d", 
			   recordFilePathCStr, errno );
		siResult = ePlugInDataError;
	}
	
	isUserRecord = (CFStringCompare(inNativeRecType, CFSTR("users"), 0) == kCFCompareEqualTo);
	
#if FILE_ACCESS_INDEXING
	if ( mUseIndex && siResult == eDSNoErr )
	{
		char *cStr2 = NULL;
		char *fileName = strrchr( recordFilePathCStr, '/' );
		if ( fileName != NULL )
		{
			CFStringRef	cfStdType = mPlugin->RecordStandardTypeForNativeType( inNativeRecType );
			if ( cfStdType != NULL )
				DeleteRecordIndex( CStrFromCFString(cfStdType, &cStr2, NULL, NULL), ++fileName );
			else
				siResult = eDSInvalidRecordType;
			
			DSFreeString( cStr2 );
		}
	}
#endif
	
	mRecordTypeLock.SignalLock();
	
	if ( isUserRecord )
	{
		recNameCStr = CStrFromCFString( inRecordName, &cStr, &cStrSize, NULL );
		if (recNameCStr == NULL) recNameCStr = "<unknown>";
		CFMutableDictionaryRef eventDict = dsCreateEventLogDict( CFSTR("user.deleted"), recNameCStr, NULL );
		if ( eventDict != NULL ) {
			dsPostEvent( CFSTR("user.deleted"), eventDict );
			CFRelease( eventDict );
		}
		
		// remove the Kerberos principal
		// needs to be after mRecordTypeLock is released 
		if ( inRecordName != NULL )
		{
			char *localKDCRealmStr = GetLocalKDCRealmWithCache( kLocalKDCRealmCacheTimeout );
			if ( localKDCRealmStr != NULL )
			{
				// if we have PasswordServerFramework bundle
				if ( mPlugin->mPWSFrameworkAvailable )
				{
					pwsf_DeletePrincipalInLocalRealm( recNameCStr, localKDCRealmStr );
					
					// delete the certificate hash principal
					attrArray = (CFArrayRef) CFDictionaryGetValue( inMutableRecordAttrsValues, CFSTR(kAuthAuthorityStr) );
					if ( attrArray != NULL )
					{
						krbCertAA = GetTagInArray( attrArray, CFSTR(kDSValueAuthAuthorityKerberosv5Cert) );
						if ( krbCertAA != NULL )
						{
							CAuthAuthority aaStorage;
							if ( aaStorage.AddValue(krbCertAA) )
							{
								char *princStr = aaStorage.GetDataForTag( kDSTagAuthAuthorityKerberosv5Cert, 1 );
								if ( princStr != NULL )
								{
									char *endPtr = strchr( princStr, '@' );
									if ( endPtr != NULL )
									{
										*endPtr = '\0';
										pwsf_DeletePrincipalInLocalRealm( princStr, localKDCRealmStr );
									}
									
									free( princStr );
								}
							}
						}
					}
				}
				else
				{
					syslog( LOG_NOTICE, "Unable to delete principal for record <%s> because PasswordServer.framework is missing", recNameCStr );
					DbgLog( kLogNotice, "Unable to delete principal for record <%s> because PasswordServer.framework is missing", recNameCStr );
				}
				DSFree( localKDCRealmStr );
			}
		}
	}
	
	DSFreeString( cStr );
	
	if ( inRecordName != NULL )
		CFRelease( inRecordName );
	
	return siResult;
}


tDirStatus CDSLocalPluginNode::FlushRecord( CFStringRef inRecordFilePath, CFStringRef inRecordType, CFMutableDictionaryRef inRecordDict )
{
	tDirStatus siResult = eDSNoErr;
	CFMutableStringRef mutableTempFilePath = NULL;
	CFStringRef recordTypeDirPath = NULL;
	CFDataRef recordDictData = NULL;
	CFStringRef changedNameRecordFilePath = NULL;
	FILE* tempFile = NULL;
	char* cStr = NULL;
	size_t cStrSize = 0;
	char* cStr2 = NULL;
	size_t cStrSize2 = 0;
	bool needRecordTypeDirPath = false;
	CFDictionaryRef recordDict = inRecordDict;
	CFMutableDictionaryRef recordMutableDict = NULL;
	
	mOpenRecordsLock.WaitLock();
	mRecordTypeLock.WaitLock();
	
	try
	{
		// remove MetaNodeLocation
		if ( CFDictionaryContainsKey(inRecordDict, CFSTR(kDSNAttrMetaNodeLocation)) )
		{
			recordMutableDict = CFDictionaryCreateMutableCopy( NULL, 0, inRecordDict );
			if ( recordMutableDict == NULL ) throw eMemoryAllocError;
			
			CFDictionaryRemoveValue( recordMutableDict, CFSTR(kDSNAttrMetaNodeLocation) );
			
			recordDict = recordMutableDict;
		}
		
		// normalize behavior through PAM/UNIX paths that only
		// check the Password attribute. If AuthenticationAuthority is present,
		// and does not contain ";basic;" then Password = "********".
		bool zapPassword = false;
		CFArrayRef aaArray = (CFArrayRef) CFDictionaryGetValue( inRecordDict, CFSTR(kAuthAuthorityStr) );
		if ( aaArray != NULL ) 
		{
			zapPassword = true;
			CFIndex aaArrayCount = CFArrayGetCount( aaArray );
			for ( int aaArrayIndex = 0; aaArrayIndex < aaArrayCount; aaArrayIndex++ )
			{
				CFStringRef aaString = (CFStringRef) CFArrayGetValueAtIndex( aaArray, aaArrayIndex );
				if ( aaString != NULL &&
					 CFStringCompare(aaString, CFSTR(kDSValueAuthAuthorityBasic), kCFCompareCaseInsensitive) == kCFCompareEqualTo )
				{
					zapPassword = false;
					break;
				}
			}
		}
		if ( zapPassword )
		{
			if ( recordDict != recordMutableDict )
			{
				recordMutableDict = CFDictionaryCreateMutableCopy( NULL, 0, inRecordDict );
				if ( recordMutableDict == NULL ) throw eMemoryAllocError;
				recordDict = recordMutableDict;
			}
			
			CFTypeRef passVal = (CFTypeRef)CFSTR( kDSValueNonCryptPasswordMarker );
			CFArrayRef passArray = CFArrayCreate( NULL, &passVal, 1, &kCFTypeArrayCallBacks );
			CFDictionarySetValue( recordMutableDict, CFSTR("passwd"), passArray );
			CFRelease( passArray );
		}
		
		// create a CFData for the record dict
		recordDictData = CFPropertyListCreateXMLData( NULL, recordDict );
		if ( recordDictData == NULL )
			throw( eMemoryAllocError );
		
		// build the temp file path
		mutableTempFilePath = CFStringCreateMutableCopy( NULL, 0, inRecordFilePath );
		if ( mutableTempFilePath == NULL )
			throw( eMemoryAllocError );
		CFStringAppend( mutableTempFilePath, CFSTR( ".temp" ) );
		
		// open the temp file for writing
		const char* tempFilePathCStr = CStrFromCFString( mutableTempFilePath, &cStr, &cStrSize, NULL );
		tempFile = ::fopen( tempFilePathCStr, "w" );
		if ( tempFile != NULL )
		{
			int rc = ::chmod( tempFilePathCStr, S_IRUSR | S_IWUSR );
			if ( rc != 0 )
				DbgLog(  kLogError, "CDSLocalPluginNode::FlushRecord() got an error %d while trying to chmod %s",
					errno, tempFilePathCStr );
		}

		// this can happen if the containing directory doesn't exist.
		if ( tempFile == NULL )
			needRecordTypeDirPath = true;

		if ( needRecordTypeDirPath )
		{
			CFRange lastSlashRange = CFStringFind( inRecordFilePath, CFSTR( "/" ), kCFCompareBackwards );
			if ( lastSlashRange.location == kCFNotFound )
			{
				DbgLog(  kLogError, "CDSLocalPluginNode::FlushRecord(): no / found in inRecordFilePath" );
				throw( ePlugInDataError );
			}
			CFRange recordTypeDirRange = CFRangeMake( 0, lastSlashRange.location );
			recordTypeDirPath = CFStringCreateWithSubstring( NULL, inRecordFilePath, recordTypeDirRange );
			if ( recordTypeDirPath == NULL )
				throw( eMemoryAllocError );
		}

		if ( tempFile == NULL )
		{
			const char* recordTypeDirCStr = CStrFromCFString( recordTypeDirPath, &cStr2, &cStrSize2, NULL );
			
			if ( EnsureDirs(recordTypeDirCStr, 0700, 0700) == 0 )
			{
				tempFile = ::fopen( tempFilePathCStr, "w" );
				if ( tempFile == NULL )
				{
					DbgLog( kLogError, "CDSLocalPluginNode::FlushRecord(): fopen() returned NULL while attempting to open \'w\' \"%s\"",
						   tempFilePathCStr );
					throw( ePlugInDataError );
				}
				else
				{
					int rc = ::chmod( tempFilePathCStr, S_IRUSR | S_IWUSR );
					if ( rc != 0 )
						DbgLog(  kLogPlugin, "CDSLocalPluginNode::FlushRecord() got an error %d while trying to chmod %s",
							   errno, tempFilePathCStr );
				}
			}
			else
			{
				DbgLog( kLogError, "CDSLocalPluginNode::FlushRecord() failed to create Directory structure %s", recordTypeDirCStr );
				throw( ePlugInDataError );
			}
		}
		
		// write the bytes to the temp file
		size_t numBlocksWritten = ::fwrite( ::CFDataGetBytePtr( recordDictData ),
			(size_t)::CFDataGetLength( recordDictData ), 1, tempFile );
		::fclose( tempFile );
		tempFile = NULL;
		if ( numBlocksWritten != 1 )
		{
			DbgLog(  kLogPlugin, "CDSLocalPluginNode::FlushRecord(): fwrite() returned unexpected number of blocks written: %d",
				numBlocksWritten );
			throw( ePlugInDataError );
		}
		
		// replace the original file (if it's there) with the temp file
		const char* recFilePathCStr = CStrFromCFString( inRecordFilePath, &cStr2, &cStrSize2, NULL );
		if ( ::rename( tempFilePathCStr, recFilePathCStr ) != 0 )
		{
			DbgLog(  kLogPlugin, "CDSLocalPluginNode::FlushRecord(): rename() failed to move temp file into proper place" );
			throw( ePlugInDataError );
		}
		
		// check to see if the record name was changed, and if so, change the file name as well
		CFArrayRef recordNames = (CFArrayRef)::CFDictionaryGetValue( recordDict, mRecordNameAttrNativeName );
		if ( recordNames == NULL || CFArrayGetCount( recordNames ) == 0 )
			throw( ePlugInDataError );
		CFStringRef recordName = (CFStringRef)CFArrayGetValueAtIndex( recordNames, 0 );
		if ( recordName == NULL )
			throw( ePlugInDataError );

		changedNameRecordFilePath = CreateFilePathForRecord( inRecordType, recordName );
		if ( changedNameRecordFilePath == NULL )
			throw( eMemoryAllocError );
		
		// rename the file if the record name was changed
		bool nameChanged = false;
		const char* newFileName = CStrFromCFString( changedNameRecordFilePath, &cStr, &cStrSize, NULL );
		if ( CFStringCompare( changedNameRecordFilePath, inRecordFilePath, 0 ) != kCFCompareEqualTo )
			nameChanged = true;
		
		if ( nameChanged && rename(recFilePathCStr, newFileName) != 0 )
		{
			DbgLog(  kLogPlugin, "CDSLocalPluginNode::FlushRecord(): rename() failed to rename file for changed record name" );
			throw( ePlugInDataError );
		}
		
#if FILE_ACCESS_INDEXING
		// we don't use the flag here because we could be indexing asynchronously
		if ( mDatabaseHelper != NULL )
		{
			char *pNewName = strrchr( newFileName, '/' );
			if ( pNewName != NULL )
			{
				CFStringRef	cfStdType = mPlugin->RecordStandardTypeForNativeType( inRecordType );
				if ( cfStdType != NULL )
				{
					char		*cStr3		= NULL;
					const char	*stdRecType	= CStrFromCFString( cfStdType, &cStr3, NULL, NULL );

					if ( nameChanged )
					{
						char *pOldName = strrchr( recFilePathCStr, '/' );
						if ( pOldName != NULL )
							DeleteRecordIndex( stdRecType, ++pOldName );
					}

					AddRecordIndex( stdRecType, ++pNewName );
					DSFree( cStr3 );
				}
			}
		}
#endif
	}
	catch( tDirStatus err )
	{
		if ( err != eDSNoErr ) {
			DbgLog(  kLogPlugin, "CDSLocalPluginNode::FlushRecord(): caught error %d", err );
		}
		siResult = err;
	}
	catch( ... )
	{
		siResult = eUndefinedError;
	}
	mRecordTypeLock.SignalLock();
	mOpenRecordsLock.SignalLock();
	
	DSCFRelease( recordMutableDict );
	
	DSFreeString( cStr );
	DSFreeString( cStr2 );
	if ( tempFile != NULL )
	{
		::fclose( tempFile );
		tempFile = NULL;
	}
	if ( recordDictData != NULL )
	{
		::CFRelease( recordDictData );
		recordDictData = NULL;
	}
	if ( mutableTempFilePath != NULL )
	{
		::CFRelease( mutableTempFilePath );
		mutableTempFilePath = NULL;
	}
	if ( recordTypeDirPath != NULL )
	{
		::CFRelease( recordTypeDirPath );
		recordTypeDirPath = NULL;
	}
	if ( changedNameRecordFilePath != NULL )
	{
		::CFRelease( changedNameRecordFilePath );
		changedNameRecordFilePath = NULL;
	}

	return siResult;
}

tDirStatus CDSLocalPluginNode::AddAttributeToRecord( CFStringRef inNativeAttrType, CFStringRef inNativeRecType, CFTypeRef inAttrValue, CFMutableDictionaryRef inMutableRecordAttrsValues )
{
	tDirStatus siResult = eDSNoErr;
	CFMutableArrayRef mutableAttrValues = NULL;

	// check for alias
	if ( inAttrValue != NULL )
	{
		siResult = this->AttributeValueMatchesUserAlias( inNativeRecType, inNativeAttrType, inAttrValue, inMutableRecordAttrsValues );
		if ( siResult != eDSNoErr )
			return siResult;
	}
	
	mOpenRecordsLock.WaitLock();
	this->UpdateModValue();
	try
	{
		mutableAttrValues = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		
		// allowed to have an empty array
		if ( inAttrValue != NULL )
			CFArrayAppendValue( mutableAttrValues, inAttrValue );

		CFDictionarySetValue( inMutableRecordAttrsValues, inNativeAttrType, mutableAttrValues );
	}
	catch( tDirStatus err )
	{
		DbgLog(  kLogPlugin, "CDSLocalPluginNode::AddAttributeToRecord(): caught error %d", err );
		siResult = err;
	}
	mOpenRecordsLock.SignalLock();

	if ( mutableAttrValues != NULL )
	{
		::CFRelease( mutableAttrValues );
		mutableAttrValues = NULL;
	}

	return siResult;
}

tDirStatus CDSLocalPluginNode::AddAttributeValueToRecord( CFStringRef inNativeAttrType, CFStringRef inNativeRecType, CFTypeRef inAttrValue, CFMutableDictionaryRef inMutableRecordAttrsValues )
{
	tDirStatus siResult = eDSNoErr;

	if ( inAttrValue == NULL )
		return siResult;

	// check for aliai
	siResult = this->AttributeValueMatchesUserAlias( inNativeRecType, inNativeAttrType, inAttrValue, inMutableRecordAttrsValues );
	if ( siResult != eDSNoErr )
		return siResult;
	
	mOpenRecordsLock.WaitLock();
	this->UpdateModValue();
	try
	{
		if ( ::CFDictionaryGetCountOfKey( inMutableRecordAttrsValues, inNativeAttrType ) == 1 )
		{
			CFMutableArrayRef mutableAttrValues = (CFMutableArrayRef)::CFDictionaryGetValue( inMutableRecordAttrsValues,
				 inNativeAttrType );
			if ( mutableAttrValues == NULL )
				throw( ePlugInDataError );
			CFArrayAppendValue( mutableAttrValues, inAttrValue );
		}
		else
		{
			CFMutableArrayRef mutableAttrValues = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
			if ( mutableAttrValues == NULL )
				throw( eMemoryAllocError );
			CFArrayAppendValue( mutableAttrValues, inAttrValue );
			::CFDictionaryAddValue( inMutableRecordAttrsValues, inNativeAttrType, mutableAttrValues );
			::CFRelease( mutableAttrValues );
		}
	}
	catch( tDirStatus err )
	{
		DbgLog(  kLogPlugin, "CDSLocalPluginNode::AddAttributeValueToRecord(): caught error %d", err );
		siResult = err;
	}
	mOpenRecordsLock.SignalLock();

	return siResult;
}

tDirStatus CDSLocalPluginNode::RemoveAttributeFromRecord( CFStringRef inNativeAttrType, CFStringRef inNativeRecType,
	CFMutableDictionaryRef inMutableRecordAttrsValues )
{
	tDirStatus siResult = eDSNoErr;
	
	mOpenRecordsLock.WaitLock();
	this->UpdateModValue();
	try
	{
		if ( CFDictionaryGetCountOfKey( inMutableRecordAttrsValues, inNativeAttrType ) == 1 )
		{
			// if the attribute is generateduid, remove the shadowhash file
			this->RemoveShadowHashFilesIfNecessary( inNativeAttrType, inMutableRecordAttrsValues );
			CFDictionaryRemoveValue( inMutableRecordAttrsValues, inNativeAttrType );
		}
	}
	catch( tDirStatus err )
	{
		DbgLog(  kLogPlugin, "CDSLocalPluginNode::RemoveAttributeFromRecord(): caught error %d", err );
		siResult = err;
	}
	mOpenRecordsLock.SignalLock();

	return siResult;
}

tDirStatus CDSLocalPluginNode::RemoveAttributeValueFromRecordByCRC( CFStringRef inNativeAttrType,
	CFStringRef inNativeRecType, CFMutableDictionaryRef inMutableRecordAttrsValues, UInt32 inCRC )
{
	tDirStatus siResult = eDSNoErr;
	char* cStr = NULL;
	CFStringRef* values = NULL;

	mOpenRecordsLock.WaitLock();
	this->UpdateModValue();
	try
	{
		CFMutableArrayRef mutableAttrValues = (CFMutableArrayRef)::CFDictionaryGetValue( inMutableRecordAttrsValues,
			inNativeAttrType );
		if ( mutableAttrValues == NULL )
			throw( eDSAttributeNotFound );
		
		CFIndex numValues = CFArrayGetCount( mutableAttrValues );
		
		values = (CFStringRef*)::malloc( sizeof( CFStringRef ) * numValues );
		if ( values == NULL )
			throw( eMemoryAllocError );
		
		CFArrayGetValues( mutableAttrValues, CFRangeMake( 0, numValues ), (const void**)values );
		
		for ( CFIndex i=0; i<numValues; i++ )
		{
			if ( CalcCRC( CStrFromCFString( values[i], &cStr, NULL, NULL ) ) == inCRC )
			{
				// if the attribute is generateduid, remove the shadowhash file
				// the index doesn't matter because generateduid is a single-valued attribute
				if ( CFStringCompare(inNativeAttrType, CFSTR(kGeneratedUIDStr), 0) == kCFCompareEqualTo )
				{
					this->RemoveShadowHashFilesIfNecessary( inNativeAttrType, inMutableRecordAttrsValues );
				}
				else if ( CFStringCompare(inNativeAttrType, CFSTR(kAuthAuthorityStr), 0) == kCFCompareEqualTo )
				{
					// check the new values
					// If none of them are ;ShadowHash; then delete the shadow hash files.
					if ( !this->ArrayContainsShadowHashOrLocalCachedUser(mutableAttrValues) )
						this->RemoveShadowHashFilesIfNecessary( inNativeAttrType, inMutableRecordAttrsValues );
					
					// If Kerberos is disabled update the local KDC
					this->SetKerberosTicketsEnabledOrDisabled( mutableAttrValues );
				}
				
				CFArrayRemoveValueAtIndex( mutableAttrValues, i );
				break;
			}
		}
	}
	catch( tDirStatus err )
	{
		DbgLog(  kLogPlugin, "CDSLocalPluginNode::RemoveAttributeValueFromRecordByCRC(): caught error %d", err );
		siResult = err;
	}
	mOpenRecordsLock.SignalLock();

	if ( cStr != NULL )
	{
		::free( cStr );
		cStr = NULL;
	}
	if ( values != NULL )
	{
		::free( values );
		values = NULL;
	}

	return siResult;
}

tDirStatus CDSLocalPluginNode::ReplaceAttributeValueInRecordByCRC( CFStringRef inNativeAttrType,
	CFStringRef inNativeRecType, CFMutableDictionaryRef inMutableRecordAttrsValues, UInt32 inCRC,
	CFTypeRef inNewValue )
{
	tDirStatus siResult = eDSNoErr;
	char* cStr = NULL;
	CFTypeRef* values = NULL;
	CFArrayRef automountMapArray = NULL;

	siResult = this->AttributeValueMatchesUserAlias( inNativeRecType, inNativeAttrType, inNewValue, inMutableRecordAttrsValues );
	if ( siResult != eDSNoErr )
		return siResult;
	
	mOpenRecordsLock.WaitLock();
	this->UpdateModValue();
	try
	{
		CFMutableArrayRef mutableAttrValues = (CFMutableArrayRef)::CFDictionaryGetValue( inMutableRecordAttrsValues,
			inNativeAttrType );
		if ( mutableAttrValues == NULL )
			throw( eDSAttributeNotFound );
		
		CFIndex numValues = CFArrayGetCount( mutableAttrValues );
		
		values = (CFTypeRef*)::malloc( sizeof( CFTypeRef ) * numValues );
		if ( values == NULL )
			throw( eMemoryAllocError );
		
		CFArrayGetValues( mutableAttrValues, CFRangeMake( 0, numValues ), (const void**)values );
		
		for ( CFIndex i = 0; i < numValues; i++ )
		{
			size_t attrValueLen = 0;
			UInt32 aCRCValue = 0;
			const void *dataValue = NULL;
			CFTypeID attrValueTypeID = CFGetTypeID( values[i] );
			if ( attrValueTypeID == CFStringGetTypeID() )
			{
				// cStr is not freed because dataValue is freed
				const char* attrValueCStr = CStrFromCFString( (CFStringRef)values[i], &cStr, NULL, NULL );
				if ( attrValueCStr == NULL ) throw( eMemoryAllocError );
				attrValueLen = strlen( attrValueCStr );
				dataValue = attrValueCStr;
				aCRCValue = CalcCRCWithLength( attrValueCStr, attrValueLen );
			}
			else //CFDataRef since we only return this or a CFStringRef
			{
				attrValueLen = (size_t) CFDataGetLength( (CFDataRef)values[i] );
				dataValue = CFDataGetBytePtr( (CFDataRef)values[i] );
				aCRCValue = CalcCRCWithLength( dataValue, attrValueLen );
			}
			
			if ( aCRCValue == inCRC )
			{
				// if this is an automount and the name is being set, we need to verify the name is proper format
				if ( i == 0 && CFStringCompare(CFSTR("automount"), inNativeRecType, 0) == kCFCompareEqualTo &&
					 CFStringCompare(CFSTR("name"), inNativeAttrType, 0) == kCFCompareEqualTo )
				{
					if ( inNewValue == NULL || CFGetTypeID(inNewValue) != CFStringGetTypeID() ) throw eDSInvalidRecordName;
					
					automountMapArray = CFStringCreateArrayBySeparatingStrings( NULL, (CFStringRef) inNewValue, CFSTR(",automountMapName=") );
					
					if ( automountMapArray == NULL || CFArrayGetCount(automountMapArray) != 2 ) throw eDSInvalidRecordName;
					
					CFStringRef metaInformation = (CFStringRef) CFArrayGetValueAtIndex( automountMapArray, 1 );
					
					if ( CFStringGetLength(metaInformation) == 0 ) throw eDSInvalidRecordName;
					
					// since the name has automountMapName as well, we just update the meta information at the same time
					CFArrayRef cfMetaInfoArray = CFArrayCreate( NULL, (const void **) &metaInformation, 1, &kCFTypeArrayCallBacks );
					
					CFDictionarySetValue( inMutableRecordAttrsValues, CFSTR("metaautomountmap"), cfMetaInfoArray );
					
					CFRelease( cfMetaInfoArray );
				}
				
				// if the attribute is generateduid, rename the shadowhash file
				// the index doesn't matter because generateduid is a single-valued attribute
				CFStringRef pathString = this->GetShadowHashFilePath( inNativeAttrType, inMutableRecordAttrsValues );
				if ( CFStringCompare(inNativeAttrType, CFSTR(kGeneratedUIDStr), 0) == kCFCompareEqualTo )
					siResult = this->RenameShadowHashFiles( pathString, (CFStringRef)inNewValue );
				
				if ( siResult == eDSNoErr )
				{
					// if this is an empty value, let's just remove it
					if ( inNewValue == NULL ||
						 (CFGetTypeID(inNewValue) == CFDataGetTypeID() && CFDataGetLength((CFDataRef)inNewValue) == 0) || 
						 (CFGetTypeID(inNewValue) == CFStringGetTypeID() && CFStringGetLength((CFStringRef)inNewValue) == 0) )
					{
						CFArrayRemoveValueAtIndex( mutableAttrValues, i );
					}
					else
					{
						CFArrayReplaceValues( mutableAttrValues, CFRangeMake(i, 1), (const void**)&inNewValue, 1 );
					}
				}
				
				// if the attribute is authentication_authority, check the new values.
				// If none of them are ;ShadowHash; then delete the shadow hash files.
				if ( CFStringCompare(inNativeAttrType, CFSTR(kAuthAuthorityStr), 0) == kCFCompareEqualTo )
				{
					if ( pathString != NULL && !this->ArrayContainsShadowHashOrLocalCachedUser(mutableAttrValues) )
						this->RemoveShadowHashFilesWithPath( pathString );
					
					// If Kerberos is disabled update the local KDC
					this->SetKerberosTicketsEnabledOrDisabled( mutableAttrValues );
				}
				DSCFRelease( pathString );
				break;
			}
		}
	}
	catch( tDirStatus err )
	{
		DbgLog(  kLogPlugin, "CDSLocalPluginNode::ReplaceAttributeValueInRecordByCRC(): caught error %d", err );
		siResult = err;
	}
	mOpenRecordsLock.SignalLock();

	DSFreeString( cStr );
	DSFree( values );	
	DSCFRelease( automountMapArray );
	
	return siResult;
}

tDirStatus CDSLocalPluginNode::ReplaceAttributeValueInRecordByIndex( CFStringRef inNativeAttrType,
	CFStringRef inNativeRecType, CFMutableDictionaryRef inMutableRecordAttrsValues, UInt32 inIndex,
	CFStringRef inNewValue )
{
	tDirStatus siResult = eDSNoErr;
	CFArrayRef automountMapArray = NULL;

	siResult = this->AttributeValueMatchesUserAlias( inNativeRecType, inNativeAttrType, inNewValue, inMutableRecordAttrsValues );
	if ( siResult != eDSNoErr )
		return siResult;

	mOpenRecordsLock.WaitLock();
	this->UpdateModValue();
	try
	{
		CFMutableArrayRef mutableAttrValues = (CFMutableArrayRef)::CFDictionaryGetValue( inMutableRecordAttrsValues,
			inNativeAttrType );
		if ( mutableAttrValues == NULL )
			throw( eDSAttributeNotFound );
		
		if ( inIndex >= (UInt32) CFArrayGetCount( mutableAttrValues ) )
			throw( eDSIndexOutOfRange );

		// if this is an automount and the name is being set, we need to verify the name is proper format
		if ( inIndex == 0 && CFStringCompare(CFSTR("automount"), inNativeRecType, 0) == kCFCompareEqualTo &&
			 CFStringCompare(CFSTR("name"), inNativeAttrType, 0) == kCFCompareEqualTo )
		{
			automountMapArray = CFStringCreateArrayBySeparatingStrings( NULL, inNewValue, CFSTR(",automountMapName=") );
			
			if ( automountMapArray == NULL || CFArrayGetCount(automountMapArray) != 2 ) throw eDSInvalidRecordName;
			
			CFStringRef metaInformation = (CFStringRef) CFArrayGetValueAtIndex( automountMapArray, 1 );
			
			if ( CFStringGetLength(metaInformation) == 0 ) throw eDSInvalidRecordName;
			
			// since the name has automountMapName as well, we just update the meta information at the same time
			CFArrayRef cfMetaInfoArray = CFArrayCreate( NULL, (const void **)&metaInformation, 1, &kCFTypeArrayCallBacks );
			
			CFDictionarySetValue( inMutableRecordAttrsValues, CFSTR("metaautomountmap"), cfMetaInfoArray );
			
			CFRelease( cfMetaInfoArray );
		}
		
		// if the attribute is generateduid, rename the shadowhash file
		// the index doesn't matter because generateduid is a single-valued attribute
		CFStringRef pathString = this->GetShadowHashFilePath( inNativeAttrType, inMutableRecordAttrsValues );
		if ( CFStringCompare(inNativeAttrType, CFSTR(kGeneratedUIDStr), 0) == kCFCompareEqualTo )
			siResult = this->RenameShadowHashFiles( pathString, inNewValue );
		
		if ( siResult == eDSNoErr )
			CFArrayReplaceValues( mutableAttrValues, CFRangeMake( inIndex, 1 ), (const void**)&inNewValue, 1 );
		
		// if the attribute is authentication_authority, check the new values.
		// If none of them are ;ShadowHash; then delete the shadow hash files.
		if ( CFStringCompare(inNativeAttrType, CFSTR(kAuthAuthorityStr), 0) == kCFCompareEqualTo )
		{
			if ( pathString != NULL && !this->ArrayContainsShadowHashOrLocalCachedUser(mutableAttrValues) )
				this->RemoveShadowHashFilesWithPath( pathString );
			
			// If Kerberos is disabled update the local KDC
			this->SetKerberosTicketsEnabledOrDisabled( mutableAttrValues );
		}
		
		DSCFRelease( pathString );
	}
	catch( tDirStatus err )
	{
		DbgLog(  kLogPlugin, "CDSLocalPluginNode::ReplaceAttributeValueInRecordByIndex(): caught error %d", err );
		siResult = err;
	}
	mOpenRecordsLock.SignalLock();
	
	DSCFRelease( automountMapArray );

	return siResult;
}

tDirStatus CDSLocalPluginNode::SetAttributeValuesInRecord( CFStringRef inNativeAttrType, CFStringRef inNativeRecType,
	CFMutableDictionaryRef inMutableRecordAttrsValues, CFMutableArrayRef inMutableAttrValues )
{
	tDirStatus siResult = eDSNoErr;
	CFArrayRef automountMapArray = NULL;

	if ( inNativeRecType == NULL )
		return eDSNullRecType;
	if ( inNativeAttrType == NULL )
		return eDSNullAttributeType;
	
	// pre-check for aliai by iterating through the new values. If there's a
	// match, reject the whole operation.
	if ( inMutableAttrValues != NULL &&
		 CFStringCompare(inNativeRecType, CFSTR("users"), 0) == kCFCompareEqualTo &&
		 CFStringCompare(inNativeAttrType, CFSTR("name"), 0) == kCFCompareEqualTo )
	{
		CFIndex attrValueCount = CFArrayGetCount( inMutableAttrValues );
		for ( CFIndex attrValueIndex = 0; attrValueIndex < attrValueCount; attrValueIndex++ )
		{
			siResult = this->AttributeValueMatchesUserAlias( inNativeRecType, inNativeAttrType,
							CFArrayGetValueAtIndex(inMutableAttrValues, attrValueIndex), inMutableRecordAttrsValues );
			if ( siResult != eDSNoErr )
				return siResult;
		}
	}
	
	mOpenRecordsLock.WaitLock();
	this->UpdateModValue();
	try
	{
		// if this is an automount and the name is being set, we need to verify the name is proper format
		if ( CFStringCompare(CFSTR("automount"), inNativeRecType, 0) == kCFCompareEqualTo &&
			 CFStringCompare(CFSTR("name"), inNativeAttrType, 0) == kCFCompareEqualTo )
		{
			automountMapArray = CFStringCreateArrayBySeparatingStrings( NULL, (CFStringRef) CFArrayGetValueAtIndex(inMutableAttrValues, 0),
																		CFSTR(",automountMapName=") );
			
			if ( automountMapArray == NULL || CFArrayGetCount(automountMapArray) != 2 ) throw eDSInvalidRecordName;
			
			CFStringRef metaInformation = (CFStringRef) CFArrayGetValueAtIndex( automountMapArray, 1 );
			
			if ( CFStringGetLength(metaInformation) == 0 ) throw eDSInvalidRecordName;

			// since the name has automountMapName as well, we just update the meta information at the same time
			CFArrayRef cfMetaInfoArray = CFArrayCreate( NULL, (const void **) &metaInformation, 1, &kCFTypeArrayCallBacks );
			
			CFDictionarySetValue( inMutableRecordAttrsValues, CFSTR("metaautomountmap"), cfMetaInfoArray );
			
			CFRelease( cfMetaInfoArray );
		}
		
		// if the attribute is generateduid, rename the shadowhash file
		// the index doesn't matter because generateduid is a single-valued attribute
		bool isAuthAuthorityAttr = (CFStringCompare(inNativeAttrType, CFSTR(kAuthAuthorityStr), 0) == kCFCompareEqualTo);
		CFStringRef pathString = this->GetShadowHashFilePath( inNativeAttrType, inMutableRecordAttrsValues );
		if ( pathString != NULL )
		{
			if ( CFStringCompare(inNativeAttrType, CFSTR(kGeneratedUIDStr), 0) == kCFCompareEqualTo )
			{
				CFStringRef newGUIDString = (CFStringRef) CFArrayGetValueAtIndex( inMutableAttrValues, 0 );
				siResult = this->RenameShadowHashFiles( pathString, newGUIDString );
			}
			else if ( isAuthAuthorityAttr )
			{
				if ( !this->ArrayContainsShadowHashOrLocalCachedUser(inMutableAttrValues) )
					this->RemoveShadowHashFilesIfNecessary( inNativeAttrType, inMutableRecordAttrsValues );
			}
			
			CFRelease( pathString );
		}
		if ( isAuthAuthorityAttr )
		{
			// If Kerberos is disabled update the local KDC
			this->SetKerberosTicketsEnabledOrDisabled( inMutableAttrValues );
		}
		
		if ( siResult == eDSNoErr )
		{
			CFMutableArrayRef curAttrValues = NULL;
			if ( CFDictionaryGetValueIfPresent(inMutableRecordAttrsValues, inNativeAttrType, (const void **)&curAttrValues) )
			{
				CFArrayRemoveAllValues( curAttrValues );
				CFArrayAppendArray( curAttrValues, inMutableAttrValues, CFRangeMake(0, CFArrayGetCount(inMutableAttrValues)) );
			}
			else
			{
				CFDictionarySetValue( inMutableRecordAttrsValues, inNativeAttrType, inMutableAttrValues );
			}
		}
	}
	catch( tDirStatus err )
	{
		DbgLog(  kLogPlugin, "CDSLocalPluginNode::SetAttributeValuesInRecord(): caught error %d", err );
		siResult = err;
	}
	mOpenRecordsLock.SignalLock();
	
	DSCFRelease( automountMapArray );

	return siResult;
}

tDirStatus CDSLocalPluginNode::GetAttributeValueByCRCFromRecord( CFStringRef inNativeAttrType, CFMutableDictionaryRef inMutableRecordAttrsValues, UInt32 inCRC, CFTypeRef* outAttrValue )
{
	tDirStatus		siResult	= eDSNoErr;
	char		   *cStr		= NULL;

	mOpenRecordsLock.WaitLock();
	try
	{
		CFArrayRef attrValues = (CFArrayRef)::CFDictionaryGetValue( inMutableRecordAttrsValues, inNativeAttrType );
		if ( attrValues == NULL ) throw( eDSAttributeNotFound );
		
		CFIndex numValues = CFArrayGetCount( attrValues );
		
		for ( CFIndex i = 0; i < numValues; i++ )
		{
			CFTypeRef	arrayVal	= CFArrayGetValueAtIndex( attrValues, i );
			if ( arrayVal != NULL )
			{
				CFTypeID	attrTypeID	= CFGetTypeID( arrayVal );
				if ( attrTypeID == CFStringGetTypeID() )
				{
					const char *aStr = CStrFromCFString( (CFStringRef)arrayVal, &cStr, NULL, NULL );
					if ( CalcCRC(aStr) == inCRC )
					{
						*outAttrValue = arrayVal;
						break;
					}
				}
				else if ( attrTypeID == CFDataGetTypeID() )
				{
					CFIndex valueLength = CFDataGetLength( (CFDataRef)arrayVal );
					const UInt8 *valueData = CFDataGetBytePtr( (CFDataRef)arrayVal );
					if ( CalcCRCWithLength(valueData, valueLength) == inCRC )
					{
						*outAttrValue = arrayVal;
						break;
					}
				}
			}
			else
			{
				throw( ePlugInDataError );
			}
		}
		
		if ( *outAttrValue == NULL ) throw( eDSAttributeValueNotFound );
	}
	catch( tDirStatus err )
	{
		DbgLog(  kLogPlugin, "CDSLocalPluginNode::GetAttributeValueByCRCFromRecord(): caught error %d", err );
		siResult = err;
	}
	mOpenRecordsLock.SignalLock();

	DSFreeString( cStr );

	return siResult;
}

tDirStatus CDSLocalPluginNode::GetAttributeValueByIndexFromRecord( CFStringRef inNativeAttrType, CFMutableDictionaryRef inMutableRecordAttrsValues, UInt32 inIndex, CFTypeRef* outAttrValue )
{
	tDirStatus siResult = eDSNoErr;

	if ( inNativeAttrType == NULL )
		return eDSInvalidAttributeType;
	if ( inMutableRecordAttrsValues == NULL )
		return eDSRecordNotFound;
	if ( outAttrValue == NULL )
		return eParameterError;
	
	mOpenRecordsLock.WaitLock();
	try
	{
		CFArrayRef attrValues = (CFArrayRef)::CFDictionaryGetValue( inMutableRecordAttrsValues, inNativeAttrType );
		if ( attrValues == NULL ) throw( eDSAttributeNotFound );
		
		if ( (CFIndex)inIndex >= CFArrayGetCount( attrValues ) ) throw( eDSIndexOutOfRange );
		
		CFTypeRef attrValue = (CFTypeRef)CFArrayGetValueAtIndex( attrValues, (CFIndex)inIndex );
		if ( attrValue == NULL ) throw( eDSIndexOutOfRange );
		
		*outAttrValue = attrValue;
	}
	catch( tDirStatus err )
	{
		DbgLog(  kLogPlugin, "CDSLocalPluginNode::GetAttributeValueByIndexFromRecord(): caught error %d", err );
		siResult = err;
	}
	mOpenRecordsLock.SignalLock();

	return siResult;
}

bool CDSLocalPluginNode::IsValidRecordName( CFStringRef inRecordName, CFStringRef inNativeRecType )
{
	CFMutableArrayRef recordsArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	CFArrayRef patternsToMatch = CFArrayCreate( NULL, (const void**)&inRecordName, 1, &kCFTypeArrayCallBacks );
	this->GetRecords( inNativeRecType, patternsToMatch,
		mPlugin->AttrNativeTypeForStandardType( CFSTR( kDSNAttrRecordName ) ), eDSExact, true, 1, recordsArray );
	CFIndex numRecords = CFArrayGetCount( recordsArray );
	::CFRelease( patternsToMatch );
	::CFRelease( recordsArray );
	
	return numRecords > 0;
}

void CDSLocalPluginNode::FlushRecordCache()
{
}

CFArrayRef CDSLocalPluginNode::CreateAllRecordTypesArray()
{
	char* cStr = NULL;
	DIR* recTypeDir = NULL;
	CFMutableArrayRef mutableAllRecordTypes = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	char thisFilePathCStr[768];

	const char* nodeDirFilePathCStr = CStrFromCFString( mNodeDirFilePath, &cStr, NULL, NULL );
	dirent* theDirEnt = NULL;
	for ( recTypeDir = opendir( nodeDirFilePathCStr ); ( recTypeDir != NULL ) && ( theDirEnt = readdir( recTypeDir ) ); )
	{
		snprintf( thisFilePathCStr, sizeof(thisFilePathCStr), "%s/%s", nodeDirFilePathCStr, theDirEnt->d_name );

		struct stat statBuffer={};
		if ( ::stat( thisFilePathCStr, &statBuffer ) != 0 )
		{
			DbgLog(  kLogPlugin, "CDSLocalPluginNode::CreateAllRecordTypesArray, stat returned nonzero error %d for \"%s\"",
				errno, nodeDirFilePathCStr );
			break;
		}
		
		if ( ( statBuffer.st_mode & S_IFDIR ) != 0 )
		{
			if ( ( ::strcmp( theDirEnt->d_name, "." ) != 0 ) && ( ::strcmp( theDirEnt->d_name, ".." ) != 0 ) )
			{
				DbgLog(  kLogPlugin, "CDSLocalPluginNode::CreateAllRecordTypesArray(): found record type \"%s\"", theDirEnt->d_name );
				CFStringRef nativeRecType = CFStringCreateWithCString( NULL, theDirEnt->d_name, kCFStringEncodingUTF8 );
				CFStringRef stdRecType = mPlugin->RecordStandardTypeForNativeType( nativeRecType );
				if ( stdRecType != NULL )
				{
					CFArrayAppendValue( mutableAllRecordTypes, stdRecType );
				}
				else
				{
					DSCFRelease( nativeRecType );
					nativeRecType = CFStringCreateWithFormat( NULL, NULL, CFSTR(kDSNativeRecordTypePrefix "%s"),
						theDirEnt->d_name );
					CFArrayAppendValue( mutableAllRecordTypes, nativeRecType );
				}
				
				::CFRelease( nativeRecType );
			}
		}
	}
	
	if ( recTypeDir != NULL )
	{
		::closedir( recTypeDir );
		recTypeDir = NULL;
	}
	if ( cStr != NULL )
	{
		::free( cStr );
		cStr = NULL;
	}
	
	return mutableAllRecordTypes;
}

bool
CDSLocalPluginNode::AccessAllowed( CFDictionaryRef inNodeDict, CFStringRef inRecType, CFStringRef inNativeAttribute, eDSAccessMode inAccessMode,
								   CFDictionaryRef inAttributeDict )
{
	CFStringRef authedUserName = (CFStringRef) mPlugin->NodeDictCopyValue( inNodeDict, CFSTR(kNodeAuthenticatedUserName) );
	int euid = 99;
	
	CFNumberRef effectiveUID = (CFNumberRef) mPlugin->NodeDictCopyValue( inNodeDict, CFSTR(kNodeEffectiveUIDKey) );
	if ( effectiveUID != NULL ) {
		CFNumberGetValue( effectiveUID, kCFNumberIntType, &euid );
	}
	
	bool bReturnValue = AccessAllowed( authedUserName, euid, inRecType, inNativeAttribute, inAccessMode, inAttributeDict );
	
	DSCFRelease( authedUserName );
	DSCFRelease( effectiveUID );
	
	return bReturnValue;
}

static
bool __checkACLPermissions( bool *inAccessAllowed, CFStringRef inAuthedUserName, CFStringRef inNativeRecType, CFStringRef inNativeAttribute,
						    CFDictionaryRef inAttributeDict, eDSAccessMode inAccessMode, CFDictionaryRef inGlobalACL )
{
	CFStringRef		cfUUID		= NULL;
	uuid_t			uu;
	uuid_string_t	uuidStr;
	CFStringRef		cfACLMode	= NULL;
	bool			bACLExists	= false;

	switch ( inAccessMode ) {
		case eDSAccessModeReadAttr:
			if ( CFStringCompare(inNativeAttribute, CFSTR("accesscontrolentry"), 0) == kCFCompareEqualTo ) {
				cfACLMode = CFSTR("readsecurity");
			}
			else {
				cfACLMode = CFSTR("readattr");
			}
			break;
		case eDSAccessModeWriteAttr:
			if ( CFStringCompare(inNativeAttribute, CFSTR("accesscontrolentry"), 0) == kCFCompareEqualTo ) {
				cfACLMode = CFSTR("writesecurity");
			}
			else {
				cfACLMode = CFSTR("writeattr");
			}
			break;
		case eDSAccessModeRead:
			if ( CFStringCompare(inNativeAttribute, CFSTR("accesscontrolentry"), 0) == kCFCompareEqualTo ) {
				cfACLMode = CFSTR("readsecurity");
			}
			else {
				cfACLMode = CFSTR("read");
			}
			break;
		case eDSAccessModeWrite:
			if ( CFStringCompare(inNativeAttribute, CFSTR("accesscontrolentry"), 0) == kCFCompareEqualTo ) {
				cfACLMode = CFSTR("writesecurity");
			}
			else {
				cfACLMode = CFSTR("write");
			}
			break;
		case eDSAccessModeDelete:
			cfACLMode = CFSTR("delete");
			break;
		default:
			return false;
	}
	
#ifdef ACL_SUPPORT
	// TODO: deadlock with ourselves because membership APIs come back into local plugin while we hold a lock
	//       need to resolve locking issues while trying to check ACLs

	// don't use external membership calls, only internal ones
	if ( inAuthedUserName == NULL ) {
		if ( Mbrd_ProcessMapIdentifier(ID_TYPE_USERNAME, "_unknown", -1, (guid_t *) uu) == KERN_SUCCESS ) {
			uuid_unparse_upper( uu, uuidStr );
			cfUUID = CFStringCreateWithCString( kCFAllocatorDefault, uuidStr, kCFStringEncodingUTF8 );
		}
	}
	else {
		char username[256];
		
		if ( CFStringGetCString(inAuthedUserName, username, sizeof(username), kCFStringEncodingUTF8) == true ) {
			if ( Mbrd_ProcessMapIdentifier(ID_TYPE_USERNAME, username, -1, (guid_t *) uu) == KERN_SUCCESS ) {
				uuid_unparse_upper( uu, uuidStr );
				cfUUID = CFStringCreateWithCString( kCFAllocatorDefault, uuidStr, kCFStringEncodingUTF8 );
			}
		}
	}
	
	void (^checkACEList)(CFArrayRef) = ^(CFArrayRef aceList) {
		CFIndex iCount = CFArrayGetCount( aceList );
		for ( CFIndex ii = 0; ii < iCount; ii++ )
		{
			CFDictionaryRef aceItem = (CFDictionaryRef) CFArrayGetValueAtIndex( aceList, ii );
			if ( CFGetTypeID(aceItem) == CFDictionaryGetTypeID() )
			{
				CFStringRef tempString = (CFStringRef) CFDictionaryGetValue( aceItem, CFSTR("uuid") );
				if ( tempString != NULL && CFStringGetTypeID() == CFGetTypeID(tempString) )
				{
					bool checkPermission = false;
					
					CFDebugLog( kLogDebug, "CDSLocalPluginNode::AccessAllowed - checking ACE for '%@' and UUID %@ for record type '%@' attribute '%@'", 
								cfACLMode, tempString, inNativeRecType, inNativeAttribute );
					
					if ( CFStringCompare(tempString, cfUUID, kCFCompareCaseInsensitive) == kCFCompareEqualTo ) {
						checkPermission = true;
					}
					// check the owner access now
					else if ( inAuthedUserName != NULL && inAttributeDict != NULL && 
							  CFStringCompare(tempString, CFSTR("ABCDEFAB-CDEF-ABCD-EFAB-CDEF0000000A"), kCFCompareCaseInsensitive) == kCFCompareEqualTo )
					{
						// if this is the owner, see if the UUID matches the person trying
						CFArrayRef guidList = (CFArrayRef) CFDictionaryGetValue( inAttributeDict, CFSTR("generateduid") );
						if ( guidList != NULL && CFArrayGetCount(guidList) > 0 ) {
							CFStringRef userUUID = (CFStringRef) CFArrayGetValueAtIndex( guidList, 0 );
							if ( CFStringCompare(userUUID, cfUUID, kCFCompareCaseInsensitive) == kCFCompareEqualTo ) {
								checkPermission = true;
								CFDebugLog( kLogDebug, "CDSLocalPluginNode::AccessAllowed - ACE contains owner UUID ABCDEFAB-CDEF-ABCD-EFAB-CDEF0000000A mode '%@' for '%@' attribute '%@'", 
											cfACLMode, inNativeRecType, inNativeAttribute );
							}
						}
					}
					
					if ( checkPermission == true ) {
						CFArrayRef permissionList = (CFArrayRef) CFDictionaryGetValue( aceItem, CFSTR("permissions") );
						if ( permissionList != NULL && CFArrayGetTypeID() == CFGetTypeID(permissionList) ) {
							if ( CFArrayContainsValue(permissionList, CFRangeMake(0, CFArrayGetCount(permissionList)), cfACLMode) == true ) {
								(*inAccessAllowed) = true;
								CFDebugLog( kLogInfo, "CDSLocalPluginNode::AccessAllowed - permitted '%@' for '%@' per ACE under UUID %@ for record type '%@' attribute '%@'", 
											inAuthedUserName ?: CFSTR("anonymous"), cfACLMode, tempString, inNativeRecType, inNativeAttribute );
								break;
							}
						}
					}
				}
				else if ( tempString != NULL ) {
					CFDebugLog( kLogError, "CDSLocalPluginNode::AccessAllowed - ACE UUID for record type '%@' attribute '%@' was not a String" );
				}
			}
			else {
				CFDebugLog( kLogError, "CDSLocalPluginNode::AccessAllowed - ACE entry for record type '%@' attribute '%@' was not a Dictionary at index %d", ii );
			}
		}
		
		if ( (*inAccessAllowed) == false ) {
			CFDebugLog( kLogInfo, "CDSLocalPluginNode::AccessAllowed - denied '%@' for '%@' per ACE for record type '%@' attribute '%@'", 
					    inAuthedUserName ?: CFSTR("anonymous"), cfACLMode, inNativeRecType, inNativeAttribute );
		}
	};
	
	bool (^checkACLEntry)(CFDictionaryRef) = ^(CFDictionaryRef aclEntry) {
		// first check specific attribute entry
		if ( CFDictionaryGetTypeID() == CFGetTypeID(aclEntry) ) {
			CFArrayRef aceList = (CFArrayRef) CFDictionaryGetValue( aclEntry, inNativeAttribute );
			if ( aceList != NULL && CFArrayGetTypeID() == CFGetTypeID(aceList) ) {
				// if we have no UUID we can't check the ACL, but it does exist
				if ( cfUUID != NULL ) {
					checkACEList( aceList );
				}
				else {
					CFDebugLog( kLogError, "CDSLocalPluginNode::AccessAllowed - failed to check ACL because no UUID available for user '%@'",
							    inAuthedUserName ? : CFSTR("_unknown") );
				}
				return true;
			}
			
			aceList = (CFArrayRef) CFDictionaryGetValue( aclEntry, CFSTR(kDSAttributesAll) );
			if ( aceList != NULL && CFArrayGetTypeID() == CFGetTypeID(aceList) ) {
				// if we have no UUID we can't check the ACL, but it does exist
				if ( cfUUID != NULL ) {
					checkACEList( aceList );
				}
				else {
					CFDebugLog( kLogError, "CDSLocalPluginNode::AccessAllowed - failed to check ACL because no UUID available for user '%@'",
							    inAuthedUserName ? : CFSTR("_unknown") );
				}
				return true;
			}
		}
		
		return false;
	};
	
	bool (^checkPermissions)(CFDictionaryRef) = ^(CFDictionaryRef permissions) {
		bool bStopChecking = false;

		if ( permissions == inGlobalACL ) {
			CFDebugLog( kLogDebug, "CDSLocalPluginNode::AccessAllowed --- Checking %s ACL for record type '%@' attribute type '%@'", 
					    "global", inNativeRecType, inNativeAttribute );
		}
		else {
			CFDebugLog( kLogDebug, "CDSLocalPluginNode::AccessAllowed --- Checking %s ACL for record type '%@' attribute type '%@'", 
					    "record-level", inNativeRecType, inNativeAttribute );
		}
		
		if ( permissions != NULL && CFDictionaryGetTypeID() == CFGetTypeID(permissions) )
		{
			CFDictionaryRef aclEntry = (CFDictionaryRef) CFDictionaryGetValue( permissions, inNativeRecType );
			if ( aclEntry != NULL ) {
				CFDebugLog( kLogDebug, "CDSLocalPluginNode::AccessAllowed - ACL %s for record type '%@'", 
						    "exists", inNativeRecType );
				bStopChecking = checkACLEntry( aclEntry );
			}
			
			if ( bStopChecking == false ) {
				aclEntry = (CFDictionaryRef) CFDictionaryGetValue( permissions, CFSTR(kDSRecordsAll) );
				if ( aclEntry != NULL ) {
					CFDebugLog( kLogDebug, "CDSLocalPluginNode::AccessAllowed - ACL %s for type 'kDSRecordsAll'", 
							    "exists" );
					bStopChecking = checkACLEntry( aclEntry );
				}
			}
		}
		
		return bStopChecking;
	};
	
	// checkPermissions returns true if there was an ACE so we stop
	// TODO: enable per-record ACLs, we only support the global ACL for now until we are certain we want to support per-record
//	if ( inAttributeDict == NULL ) {
//		bACLExists = checkPermissions( (CFDictionaryRef) CFDictionaryGetValue(inAttributeDict, CFSTR("accesscontrolentry")) );
//	}
	
	if ( bACLExists == false ) {
		bACLExists = checkPermissions( inGlobalACL );
	}
	
	DSCFRelease( cfUUID );
#else
	if ( CFStringCompare(inNativeAttribute, CFSTR("shadowhashdata"), 0) == kCFCompareEqualTo || 
		 CFStringCompare(inNativeAttribute, CFSTR("shadowhashpolicy"), 0) == kCFCompareEqualTo )
	{
		(*inAccessAllowed) = false;
		return true;
	}	
#endif
	
	// fallback to ensure we allow read access if no ACL exists
	if ( bACLExists == false ) {
		
		// if no ACL exists, we always allow read access
		switch ( inAccessMode ) {
			case eDSAccessModeReadAttr:
			case eDSAccessModeRead:
				(*inAccessAllowed) = true;
				bACLExists = true;
				break;
			default:
				break;
		}
	}
	
	// return false means no ACE
	return bACLExists;
}

bool
CDSLocalPluginNode::AccessAllowed( CFStringRef inAuthedUserName, uid_t inEffectiveUID, CFStringRef inNativeRecType, CFStringRef inNativeAttribute,
								   eDSAccessMode inAccessMode, CFDictionaryRef inAttributeDict )
{
	bool			accessAllowed	= false;
	CFStringRef		cfTempName		= NULL;
	
	// return if this is a membership thread lookup
	if ( (inAccessMode == eDSAccessModeRead || inAccessMode == eDSAccessModeReadAttr) && Mbrd_IsMembershipThread() == true ) {
		return true;
	}
	
	// workaround for block annoyances in C++
	if ( __checkACLPermissions(&accessAllowed, inAuthedUserName, inNativeRecType, inNativeAttribute, inAttributeDict,
							   inAccessMode, mPlugin->mPermissions) == true ) {
		return accessAllowed;
	};
	
	// root is always allowed
	if ( inEffectiveUID == 0 || (inAuthedUserName != NULL && CFStringCompare(inAuthedUserName, CFSTR("root"), 0) == kCFCompareEqualTo) ) {
		return true;
	}

	// see if user is an admin as that always allows access
	if ( accessAllowed == false && inAuthedUserName != NULL ) {
		char username[256];
		
		if ( CFStringGetCString(inAuthedUserName, username, sizeof(username), kCFStringEncodingUTF8) == true ) {
			accessAllowed = dsIsUserMemberOfGroup( username, "admin" );
		}
	}
	
	// support legacy style _writers model only after ACE and admin check
	if ( accessAllowed == false && inAttributeDict != NULL )
	{
		CFArrayRef			attribACL		= NULL;
		CFMutableStringRef	cfTempAttrib	= CFStringCreateMutableCopy( NULL, 0, CFSTR("_writers_") );
		CFArrayRef			recordACL		= (CFArrayRef) CFDictionaryGetValue( inAttributeDict, CFSTR("_writers") );
		
		if ( cfTempAttrib != NULL ) {
			CFStringAppend( cfTempAttrib, inNativeAttribute );
			attribACL = (CFArrayRef) CFDictionaryGetValue( inAttributeDict, cfTempAttrib );
			DSCFRelease( cfTempAttrib );
		}
		
		// nothing to do if there is no writers attribute
		if ( attribACL != NULL )
		{
			CFStringRef cfAuthedName = inAuthedUserName;
			
			// if we have no username, we should get it and allow the code below to check _writers
			// since writers allows ubiquitous access based on user session without auth
			if ( cfAuthedName == NULL )
			{
				char	buffer[1024];
				struct	passwd entry_storage;
				struct	passwd *entry;
				
				if ( getpwuid_r(inEffectiveUID, &entry_storage, buffer, sizeof(buffer), &entry) == 0 ) {
					cfTempName = CFStringCreateWithCString( NULL, entry->pw_name, kCFStringEncodingUTF8 );
					cfAuthedName = cfTempName;
				}
			}
			
			if ( cfAuthedName != NULL )
			{
				// check for access based on "_writers" which means record level access
				if ( recordACL != NULL )
				{
					CFIndex iCount = CFArrayGetCount( recordACL );
					for ( CFIndex i = 0; i < iCount; i++ ) {
						if ( CFStringCompare(cfAuthedName, (CFStringRef)CFArrayGetValueAtIndex(recordACL, i), 0) == kCFCompareEqualTo ) {
							accessAllowed = true;
							break;
						}
					}
				}
				
				// check for "_writers_inNativeAttribute" which means attribute level access
				if ( accessAllowed == false && attribACL != NULL )
				{
					CFIndex iCount = CFArrayGetCount( attribACL );
					for ( CFIndex i = 0; i < iCount; i++ ) {
						if ( CFStringCompare(cfAuthedName, (CFStringRef)CFArrayGetValueAtIndex(attribACL, i ), 0) == kCFCompareEqualTo ) {
							accessAllowed = true;
							break;
						}
					}
				}
			}
		}
		
		DSCFRelease( cfTempName );
	}
	
	return accessAllowed;
} // AccessAllowed

bool CDSLocalPluginNode::IsLocalNode()
{
	return true;
}

uint32_t CDSLocalPluginNode::GetModValue( void )
{
	uint32_t	retValue;
	
	mOpenRecordsLock.WaitLock();
	retValue = mModCounter;
	mOpenRecordsLock.SignalLock();
	
	return retValue;
}


#pragma mark -
#pragma mark Private Methods

bool CDSLocalPluginNode::RecordMatchesCriteria( CFDictionaryRef inRecordDict, CFArrayRef inPatternsToMatch,
	CFStringRef inNativeAttrTypeToMatch, tDirPatternMatch inPatternMatch )
{
	// pre-screen for kDSRecordsAll
	if ( CFArrayContainsValue(inPatternsToMatch, CFRangeMake(0, CFArrayGetCount(inPatternsToMatch)), CFSTR(kDSRecordsAll)) )
		return true;
	
	CFArrayRef attrValues = (CFArrayRef)::CFDictionaryGetValue( inRecordDict, inNativeAttrTypeToMatch );
	if ( attrValues == NULL )
		return false;

	bool needLowercase = false;
	switch( inPatternMatch )
	{
		case eDSiStartsWith:
		case eDSiEndsWith:
			needLowercase = true;
			break;
		default:
			break;
	}
	
	CFIndex numValues = CFArrayGetCount( attrValues );
	bool matches = false;
	for ( CFIndex i = 0; i < numValues && !matches; i++ )
	{
		CFTypeRef value = CFArrayGetValueAtIndex( attrValues, i );
		if ( value != NULL )
		{
			if ( CFGetTypeID(value) == CFStringGetTypeID() )
				matches = this->RecordMatchesCriteriaTestString( (CFStringRef)value, inPatternsToMatch, inNativeAttrTypeToMatch,
							inPatternMatch, needLowercase );
			else if ( CFGetTypeID(value) == CFDataGetTypeID() )
				matches = this->RecordMatchesCriteriaTestData( (CFDataRef)value, inPatternsToMatch, inNativeAttrTypeToMatch,
							inPatternMatch, needLowercase );
		}
	}
	
	return matches;
}


bool
CDSLocalPluginNode::RecordMatchesCriteriaTestString(
	CFStringRef inAttributeString,
	CFArrayRef inPatternsToMatch,
	CFStringRef inNativeAttrTypeToMatch,
	tDirPatternMatch inPatternMatch,
	bool inNeedLowercase )
{
	bool matches = false;
	CFMutableStringRef mutableLowercaseValue = NULL;
	CFMutableStringRef mutableLowercasePattern = NULL;

	if ( inNeedLowercase )
	{
		mutableLowercaseValue = CFStringCreateMutableCopy( NULL, 0, inAttributeString );
		if ( mutableLowercaseValue != NULL )
			CFStringLowercase( mutableLowercaseValue, NULL );
	}
	
	CFIndex numPatterns = CFArrayGetCount( inPatternsToMatch );
	for ( CFIndex j = 0; j < numPatterns && !matches; j++ )
	{
		CFStringRef patternToMatch = (CFStringRef)CFArrayGetValueAtIndex( inPatternsToMatch, j );
		if ( patternToMatch == NULL || CFGetTypeID(patternToMatch) != CFStringGetTypeID() )
			continue;
		
		if ( CFStringCompare( patternToMatch, CFSTR( kDSRecordsAll ), 0 ) == kCFCompareEqualTo )
			return true;

		if ( inNeedLowercase )
		{
			mutableLowercasePattern = CFStringCreateMutableCopy( NULL, 0, patternToMatch );
			if ( mutableLowercasePattern == NULL )
				continue;
			CFStringLowercase( mutableLowercasePattern, NULL );
		}

		switch( inPatternMatch )
		{
			case eDSExact:
				if ( CFStringCompare( inAttributeString, patternToMatch, 0 ) == kCFCompareEqualTo )
					matches = true;
				break;
			case eDSiExact:
				if ( CFStringCompare( inAttributeString, patternToMatch, kCFCompareCaseInsensitive ) == kCFCompareEqualTo )
					matches = true;
				break;
			case eDSStartsWith:
				if ( CFStringHasPrefix( inAttributeString, patternToMatch ) )
					matches = true;
				break;
			case eDSiStartsWith:
				if ( CFStringHasPrefix( mutableLowercaseValue, mutableLowercasePattern ) )
					matches = true;
				break;
			case eDSEndsWith:
				if ( CFStringHasSuffix( inAttributeString, patternToMatch ) )
					matches = true;
				break;
			case eDSiEndsWith:
				if ( CFStringHasSuffix( mutableLowercaseValue, mutableLowercasePattern ) )
					matches = true;
				break;
			case eDSContains:
				if ( CFStringFind( inAttributeString, patternToMatch, 0 ).location != kCFNotFound )
					matches = true;
				break;
			case eDSiContains:
				if ( CFStringFind( inAttributeString, patternToMatch, kCFCompareCaseInsensitive ).location != kCFNotFound )
					matches = true;
				break;
			case eDSAnyMatch:
				matches = true;
				break;

			case eDSLessThan:
			case eDSiLessThan:
				break;
			case eDSGreaterThan:
			case eDSiGreaterThan:
				break;
			case eDSLessEqual:
			case eDSiLessEqual:
				break;
			case eDSGreaterEqual:
			case eDSiGreaterEqual:
				break;
			default:	//we don't handle  any of the other pattern matches
				return false;
		}
		
		DSCFRelease( mutableLowercasePattern );
	}

	DSCFRelease( mutableLowercaseValue );
	
	return matches;
}


bool
CDSLocalPluginNode::RecordMatchesCriteriaTestData(
	CFDataRef inAttributeData,
	CFArrayRef inPatternsToMatch,
	CFStringRef inNativeAttrTypeToMatch,
	tDirPatternMatch inPatternMatch,
	bool inNeedLowercase )
{
	bool matches = false;
	const UInt8 *attrData = NULL;
	CFIndex attrDataLen = 0;
	const UInt8 *patternData = NULL;
	CFIndex patternDataLen = 0;
	char *cStr = NULL;
	
	if ( inNeedLowercase )
		return false;
	
	attrDataLen = CFDataGetLength( inAttributeData );
	if ( attrDataLen == 0 )
		return false;
	
	attrData = CFDataGetBytePtr( inAttributeData );
	if ( attrData == NULL )
		return false;
	
	CFIndex numPatterns = CFArrayGetCount( inPatternsToMatch );
	for ( CFIndex j = 0; j < numPatterns && !matches; j++ )
	{
		CFTypeRef patternToMatch = CFArrayGetValueAtIndex( inPatternsToMatch, j );
		if ( patternToMatch != NULL )
		{
			if ( CFGetTypeID(patternToMatch) == CFDataGetTypeID() )
			{
				if ( (patternDataLen = CFDataGetLength((CFDataRef)patternToMatch)) <= 0 ||
					 (patternData = CFDataGetBytePtr((CFDataRef)patternToMatch)) == NULL )
					continue;
			}
			else if ( CFGetTypeID(patternToMatch) == CFStringGetTypeID() )
			{
				if ( (patternData = (const UInt8 *)CStrFromCFString((CFStringRef)patternToMatch, &cStr, NULL, NULL)) == NULL ||
					 (patternDataLen = (CFIndex)strlen((char *)patternData)) == 0 )
					 continue;
			}
			
			switch( inPatternMatch )
			{
				case eDSExact:
					if ( patternDataLen == attrDataLen )
						matches = (memcmp(attrData, patternData, patternDataLen) == 0);
					break;
				
				case eDSStartsWith:
					if ( patternDataLen <= attrDataLen )
						matches = (memcmp(attrData, patternData, patternDataLen) == 0);
					break;
				case eDSEndsWith:
					if ( patternDataLen <= attrDataLen )
						matches = (memcmp(attrData + attrDataLen-patternDataLen, patternData, patternDataLen) == 0);
					break;
				case eDSContains:
					if ( patternDataLen <= attrDataLen )
					{
						int maxpos = attrDataLen-patternDataLen;
						for ( int idx = 0; idx <= maxpos && !matches; idx++ )
							matches = (memcmp(attrData + idx, patternData, patternDataLen) == 0);
					}
					break;
					
				case eDSiExact:
				case eDSiStartsWith:
				case eDSiContains:
				case eDSiEndsWith:
					break;
					
				case eDSLessThan:
				case eDSiLessThan:
					break;
				case eDSGreaterThan:
				case eDSiGreaterThan:
					break;
				case eDSLessEqual:
				case eDSiLessEqual:
					break;
				case eDSGreaterEqual:
				case eDSiGreaterEqual:
					break;
				default:	//we don't handle  any of the other pattern matches
					return false;
			}
		}
		
		DSFreeString( cStr );
	}
		
	return matches;
}


void CDSLocalPluginNode::UpdateModValue()
{
	// protect here, even though it is probably already protected
	mOpenRecordsLock.WaitLock();
	mModCounter++;
	mOpenRecordsLock.SignalLock();
}


void
CDSLocalPluginNode::RemoveShadowHashFilesIfNecessary(
	CFStringRef inNativeAttrType,
	CFMutableDictionaryRef inMutableRecordAttrsValues )
{
	CFStringRef		pathString		= NULL;
	
	if ( (pathString = GetShadowHashFilePath(inNativeAttrType, inMutableRecordAttrsValues)) == NULL )
		return;
	
	this->RemoveShadowHashFilesWithPath( pathString );
	CFRelease( pathString );
}


void
CDSLocalPluginNode::RemoveShadowHashFilesWithPath( CFStringRef inPath )
{
	struct stat		sb;
	char			pathStr[256];
	
	if ( inPath != NULL && CFStringGetCString(inPath, pathStr, sizeof(pathStr), kCFStringEncodingUTF8) )
	{
		if ( lstat(pathStr, &sb) == 0 )
			unlink( pathStr );
		
		strlcat( pathStr, kShadowHashStateFileSuffix, sizeof(pathStr) );
		if ( lstat(pathStr, &sb) == 0 )
			unlink( pathStr );
	}
}


CFStringRef
CDSLocalPluginNode::GetShadowHashFilePath(
	CFStringRef inNativeAttrType,
	CFMutableDictionaryRef inMutableRecordAttrsValues )
{
	CFStringRef		pathString		= NULL;
	CFArrayRef		attrArray		= NULL;
	CFStringRef		guidString		= NULL;
	
	if ( inNativeAttrType == NULL || inMutableRecordAttrsValues == NULL )
		return NULL;
	
	if ( CFStringCompare(inNativeAttrType, CFSTR(kGeneratedUIDStr), 0) != kCFCompareEqualTo &&
		 CFStringCompare(inNativeAttrType, CFSTR(kAuthAuthorityStr), 0) != kCFCompareEqualTo )
		return NULL;
	
	// check for authentication_authority of ShadowHash
	if ( (attrArray = (CFArrayRef) CFDictionaryGetValue(inMutableRecordAttrsValues, CFSTR(kAuthAuthorityStr))) == NULL )
		return NULL;
		
	if ( this->ArrayContainsShadowHashOrLocalCachedUser(attrArray) )
	{
		attrArray = (CFArrayRef) CFDictionaryGetValue( inMutableRecordAttrsValues, CFSTR(kGeneratedUIDStr) );
		if ( attrArray != NULL && CFGetTypeID(attrArray) == CFArrayGetTypeID() )
			guidString = (CFStringRef) CFArrayGetValueAtIndex( attrArray, 0 );
		if ( guidString != NULL && CFGetTypeID(guidString) == CFStringGetTypeID() )
			pathString = CFStringCreateWithFormat( NULL, NULL, CFSTR(kShadowHashDirPath "%@"), guidString );
	}
	
	return pathString;
}


void
CDSLocalPluginNode::GetDataFromEnabledOrDisabledKerberosTag( CFStringRef inAuthAuthority, char **outPrincipal, char **outRealm )
{
	CFMutableDictionaryRef aaDict = NULL;
	CFArrayRef aaDataArray = NULL;
	CFStringRef princString = NULL;
	CFStringRef realmString = NULL;
	bool canGetStrings = false;
	char scratch[1024];
	
	if ( outPrincipal != NULL && outRealm != NULL )
	{
		*outPrincipal = NULL;
		*outRealm = NULL;
		
		// Authentication Authority is of form:
		// ;DisabledUser;;Kerberosv5;;princ@realm;realm;
		CAuthAuthority aaStorage;
		if ( aaStorage.AddValue(inAuthAuthority) )
		{
			if ( (aaDict = aaStorage.GetValueForTagAsCFDict(kDSTagAuthAuthorityDisabledUser)) != NULL &&
				 (aaDataArray = (CFArrayRef)CFDictionaryGetValue(aaDict, CFSTR("data"))) != NULL &&
				 CFArrayGetCount(aaDataArray) >= 5 &&
				 (princString = (CFStringRef)CFArrayGetValueAtIndex(aaDataArray, 3)) != NULL &&
				 (realmString = (CFStringRef)CFArrayGetValueAtIndex(aaDataArray, 4)) != NULL )
			{
				canGetStrings = true;
			}
			else if ( (aaDict = aaStorage.GetValueForTagAsCFDict(kDSTagAuthAuthorityKerberosv5)) != NULL &&
					  (aaDataArray = (CFArrayRef)CFDictionaryGetValue(aaDict, CFSTR("data"))) != NULL &&
					  CFArrayGetCount(aaDataArray) >= 3 &&
					  (princString = (CFStringRef)CFArrayGetValueAtIndex(aaDataArray, 1)) != NULL &&
					  (realmString = (CFStringRef)CFArrayGetValueAtIndex(aaDataArray, 2)) != NULL )
			{
				canGetStrings = true;				
			}
			
			if ( canGetStrings )
			{
				if ( CFStringGetCString(princString, scratch, sizeof(scratch), kCFStringEncodingUTF8) &&
					 (*outPrincipal = strdup(scratch)) != NULL )
				{
					char *atSign = strchr( *outPrincipal, '@' );
					if ( atSign != NULL )
						*atSign = '\0';
				}
				
				if ( CFStringGetCString(realmString, scratch, sizeof(scratch), kCFStringEncodingUTF8) )
					*outRealm = strdup( scratch );
			}
		}
	}
}


CFStringRef
CDSLocalPluginNode::GetKerberosTagIfPresent( CFArrayRef inAttrValues )
{
	return GetTagInArray( inAttrValues, CFSTR(kDSValueAuthAuthorityKerberosv5) );
}


CFStringRef
CDSLocalPluginNode::GetDisabledKerberosTagIfPresent( CFArrayRef inAttrValues )
{
	return GetTagInArray( inAttrValues, CFSTR(kDSValueAuthAuthorityDisabledUser kDSValueAuthAuthorityKerberosv5) );
}


bool
CDSLocalPluginNode::ArrayContainsShadowHashOrLocalCachedUser( CFArrayRef inAttrValues )
{
	if (GetTagInArray(inAttrValues, CFSTR(kDSValueAuthAuthorityShadowHash)) != NULL)
		return true;
	
	return (GetTagInArray(inAttrValues, CFSTR(kDSValueAuthAuthorityLocalCachedUser)) != NULL);
}


CFStringRef
CDSLocalPluginNode::GetTagInArray( CFArrayRef inAttrValues, CFStringRef inTag )
{
	CFStringRef returnString = NULL;
	CFStringRef aaString = NULL;
	CFRange resultRange = { 0, 0 };
	
	// CFArrayContainsValue() doesn't work because ShadowHash can have data appended
	CFIndex attrArrayCount = CFArrayGetCount( inAttrValues );
	for ( CFIndex idx = 0; idx < attrArrayCount; idx++ )
	{
		aaString = (CFStringRef) CFArrayGetValueAtIndex( inAttrValues, idx );
		if ( aaString != NULL &&
			 CFStringFindWithOptions(
				aaString,
				inTag,
				CFRangeMake(0, CFStringGetLength(aaString)),
				kCFCompareCaseInsensitive,
				&resultRange ) )
		{
			returnString = aaString;
			break;
		}
	}
	
	return returnString;
}


tDirStatus
CDSLocalPluginNode::RenameShadowHashFiles( CFStringRef inCurrentPath, CFStringRef inNewGUIDString )
{
	tDirStatus		result = eDSNoErr;
	CFStringRef		newPathString = NULL;
	char			oldpath[PATH_MAX] = {0,};
	char			newpath[PATH_MAX] = {0,};
	struct stat		sb;
	
	if ( inCurrentPath != NULL && inNewGUIDString != NULL )
	{
		newPathString = CFStringCreateWithFormat( NULL, NULL, CFSTR(kShadowHashDirPath "%@"), inNewGUIDString );
		if ( newPathString != NULL )
		{
			if ( CFStringGetCString( inCurrentPath, oldpath, sizeof(oldpath), kCFStringEncodingUTF8 ) &&
				 CFStringGetCString( newPathString, newpath, sizeof(newpath), kCFStringEncodingUTF8 ) )
			{
				if ( lstat(oldpath, &sb) == 0 )
				{
					if ( lstat(newpath, &sb) != 0 )
					{
						rename( oldpath, newpath );
						
						strlcat( oldpath, kShadowHashStateFileSuffix, sizeof(oldpath) );
						strlcat( newpath, kShadowHashStateFileSuffix, sizeof(newpath) );
						
						if ( lstat(oldpath, &sb) == 0 )
						{
							// if we got this far, any existing state file doesn't have an owner
							// and we can delete it
							if ( lstat(newpath, &sb) == 0 )
								unlink( newpath );
							
							rename( oldpath, newpath );
						}
					}
					else
					{
						// not allowed to overwrite another account's password file
						result = eDSInvalidFilePath;
						DbgLog( kLogError, "CDSLocalPluginNode::RenameShadowHashFiles(): Cannot rename %s to %s", oldpath, newpath );
					}
				}					
			}
			
			CFRelease( newPathString );
		}
	}
	
	return result;
}


tDirStatus
CDSLocalPluginNode::AttributeValueMatchesUserAlias(
	CFStringRef inNativeRecType,
	CFStringRef inNativeAttrType, 
	CFTypeRef inAttrValue,
	CFDictionaryRef inRecordDict )
{
	tDirStatus	status			= eDSNoErr;

#if FILE_ACCESS_INDEXING
	char		**filesWithAlias	= NULL;
	
	if ( inNativeRecType != NULL &&
		 inNativeAttrType != NULL && 
		 inAttrValue != NULL && 
		 CFGetTypeID(inAttrValue) == CFStringGetTypeID() &&
		 CFStringCompare(inNativeRecType, CFSTR("users"), 0) == kCFCompareEqualTo &&
		 CFStringCompare(inNativeAttrType, CFSTR("name"), 0) == kCFCompareEqualTo )
	{
		filesWithAlias = GetFileAccessIndex( inNativeRecType, eDSiExact, (CFStringRef) inAttrValue, CFSTR("name"), NULL );

		if ( filesWithAlias != NULL )
		{
			for ( int ii = 0; filesWithAlias[ii] != NULL; ii++ )
			{
				// let's see if the file actually exists
				char	thisFilePathCStr[768] = { 0, };
				char	*theFile = filesWithAlias[ii];
				struct stat statResult;
				
				CFArrayRef recordNames = (CFArrayRef)::CFDictionaryGetValue( inRecordDict, mRecordNameAttrNativeName );
				if ( recordNames == NULL || CFArrayGetCount( recordNames ) == 0 )
					status = ePlugInDataError;
				CFStringRef recordName = (CFStringRef)CFArrayGetValueAtIndex( recordNames, 0 );
				if ( recordName == NULL )
					status = ePlugInDataError;
				
				if ( status == eDSNoErr )
				{
					CFStringRef thisFilePath = CFStringCreateWithFormat( NULL, NULL, CFSTR( "%@%@/%s" ), mNodeDirFilePath,
																		inNativeRecType, theFile );
					CFStringRef filePath = CreateFilePathForRecord( inNativeRecType, recordName );
					
					if ( thisFilePath != NULL && filePath != NULL && CFStringCompare(filePath, thisFilePath, 0) != kCFCompareEqualTo )
					{
						if ( CFStringGetCString( thisFilePath, thisFilePathCStr, sizeof(thisFilePathCStr), kCFStringEncodingUTF8 ) )
						{
							if( stat(thisFilePathCStr, &statResult) == 0 )
							{
								DbgLog( kLogError, "CDSLocalPluginNode::AttributeValueMatchesUserAlias(), alias exists in file %s", thisFilePathCStr );
								status = eDSRecordAlreadyExists;
								break;
							}
						}
						else
						{
							DbgLog( kLogError, "CDSLocalPluginNode::AttributeValueMatchesUserAlias(), alias exists in file %s according to index",
								    theFile );
							status = eDSRecordAlreadyExists;
							break;
						}
					}
					
					DSCFRelease( thisFilePath );
					DSCFRelease( filePath );
				}
			}

			DSFreeStringList( filesWithAlias );
		}
	}
#endif

	return status;
}


void CDSLocalPluginNode::SetKerberosTicketsEnabledOrDisabled( CFMutableArrayRef inMutableAttrValues )
{
	char *princ = NULL;
	char *realm = NULL;
	
	CFStringRef aaString = this->GetDisabledKerberosTagIfPresent( inMutableAttrValues );
	if ( aaString != NULL )
	{
		this->GetDataFromEnabledOrDisabledKerberosTag( aaString, &princ, &realm );
		if ( princ != NULL && realm != NULL )
			SetPrincipalStateInLocalRealm( princ, realm, false );
	}
	else
	{
		aaString = this->GetKerberosTagIfPresent( inMutableAttrValues );
		if ( aaString != NULL )
		{
			this->GetDataFromEnabledOrDisabledKerberosTag( aaString, &princ, &realm );
			if ( princ != NULL && realm != NULL )
				SetPrincipalStateInLocalRealm( princ, realm, true );
		}
	}
	
	DSFreeString( princ );
	DSFreeString( realm );
}

void CDSLocalPluginNode::SetPrincipalStateInLocalRealm(char* principalName, const char *realmName, bool enabled)
{
	const char	*argv[6];
	int			ai = 0;
	char		commandBuf[1024];
	char		stdoutBuff[512];
	char		stderrBuff[512];
	
	if ( principalName == NULL || realmName == NULL || mPlugin->mPWSFrameworkAvailable == false )
		return;
	
	argv[ai++] = "kadmin.local";
	argv[ai++] = "-r";
	argv[ai++] = realmName;
	argv[ai++] = "-q";
	argv[ai++] = commandBuf;
	argv[ai] = NULL;
	
	snprintf(commandBuf, sizeof(commandBuf), "modify_principal %callow_tix %s", enabled ? '+' : '-', principalName);
	pwsf_LaunchTaskWithIO2(kKAdminLocalFilePath, (char * const *)argv, NULL, stdoutBuff, sizeof(stdoutBuff), stderrBuff, sizeof(stderrBuff) );
}

CFDataRef CDSLocalPluginNode::CreateCFDataFromFile( const char *filename, size_t inLength )
{
	CFDataRef	cfData	= NULL;
	struct stat	sb;
	
	if ( filename == NULL )
		return NULL;

	if ( inLength == 0 && lstat(filename, &sb) == 0 )
		inLength = sb.st_size;

	if ( inLength <= 0 )
		return NULL;
	
	int fd = open( filename, O_RDONLY | O_NOFOLLOW );
	if ( fd != -1 )
	{
		void *data = calloc( inLength + 1, sizeof(char) );
	
		read( fd, data, inLength );
		
		cfData = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault, (UInt8 *) data, inLength, kCFAllocatorMalloc );
		if ( cfData == NULL ) {
			DSFree( data );
		}
		
		close( fd );
	}
	
	return cfData;
}

#if FILE_ACCESS_INDEXING

#pragma mark -
#pragma mark Index routines

void CDSLocalPluginNode::AddIndexMapping( CFStringRef inRecordType, CFArrayRef inList )
{
	if ( inRecordType == NULL || inList == NULL ) return;
	
	CFStringRef cfNativeType = mPlugin->RecordNativeTypeForStandardType( inRecordType );
	if ( cfNativeType == NULL ) return;
	
	CFIndex iCount = CFArrayGetCount( inList );
	if ( iCount == 0 ) return;
	
	IndexMapping	*newMap		= (IndexMapping *) calloc( 1, sizeof(IndexMapping) );
	char			*cStr		= NULL;
	size_t			cStrSize	= 0;
	
	newMap->fRecordTypeCF = (CFStringRef) CFRetain( inRecordType );
	newMap->fAttributesCF = CFArrayCreateCopy( kCFAllocatorDefault, inList );
	newMap->fRecordType = strdup( CStrFromCFString(inRecordType, &cStr, &cStrSize, NULL) );
	newMap->fRecordNativeType = strdup( CStrFromCFString(cfNativeType, &cStr, &cStrSize, NULL) );
	newMap->fAttributes = (const char **) calloc( iCount+1, sizeof(const char *) );
	
	for ( CFIndex ii = 0; ii < iCount; ii++ )
	{
		CFStringRef tempString = (CFStringRef) CFArrayGetValueAtIndex( inList, ii );
		newMap->fAttributes[ii] = strdup( CStrFromCFString(tempString, &cStr, &cStrSize, NULL) );
	}

	DSFree( cStr );

	mIndexMap[newMap->fRecordType] = newMap;
}

struct sIndexContext
{
	CDSLocalPluginNode	*pNode;
	sqlite3_stmt		*pStmt;
	const char			*pAttrib;
	const char			*pFileName;
};

void CDSLocalPluginNode::IndexObject( const void *inValue, void *inContext )
{
	sIndexContext	*pContext = (sIndexContext *) inContext;
	
	// if we don't have a pointer, then we are forcing an index rebuild
	if ( pContext->pNode == NULL )
		return;
	
	if ( CFGetTypeID(inValue) == CFStringGetTypeID() )
	{
		char		*cStr	= NULL;
		const char	*pValue = CStrFromCFString( (CFStringRef) inValue, &cStr, NULL, NULL );
		
		int status = sqlite3_reset( pContext->pStmt );
		
		if ( status == SQLITE_OK )
			status = sqlite3_bind_text( pContext->pStmt, 3, pValue, strlen(pValue), SQLITE_TRANSIENT );
		
		if ( status == SQLITE_OK )
			pContext->pNode->mDatabaseHelper->Step( pContext->pStmt );
		
		if ( status == SQLITE_OK ) {
			DbgLog( kLogDebug, "CDSLocalPluginNode::IndexObject - added <%s> to table <%s> for <%s>", pValue, pContext->pAttrib, 
				    pContext->pFileName );
		}
		else if ( status == SQLITE_CORRUPT && pContext->pNode != NULL ) {
			pContext->pNode->DatabaseCorrupt();
			DbgLog( kLogCritical, "CDSLocalPluginNode::IndexObject - failed to add <%s> to table <%s> for <%s> - DB corrupt detected, will rebuild", pValue, 
				    pContext->pAttrib, pContext->pFileName );
			pContext->pNode = NULL; // we NULL this so we know we can't fire off another index thread.
		}
		
		DSFree( cStr );
	}
}

void CDSLocalPluginNode::DatabaseCorrupt( void )
{
	__sync_bool_compare_and_swap( &mUseIndex, true, false );

	mDatabaseHelper->CloseDatabase();
	mDatabaseHelper->RemoveDatabase();
	
	mIndexLoaded.ResetEvent();

	dispatch_async( dispatch_get_concurrent_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT), 
					^(void) {
						LoadFileAccessIndex();
					} );
	
	if ( mIndexLoaded.WaitForEvent(10 * kMilliSecsPerSec) == false )
		DbgLog( kLogPlugin, "CDSLocalPluginNode::DatabaseCorrupt - timed out waiting for index to load continuing without until available" );
}

void CDSLocalPluginNode::AddRecordIndex( const char *inRecordType, const char *inFileName )
{
	LocalNodeIndexMapI iter = mIndexMap.find( inRecordType );
	if ( iter == mIndexMap.end() ) return;
	
	char			sPath[PATH_MAX];
	IndexMapping	*mapping = iter->second;
	struct stat		statBuffer;
	CFDataRef		aFileData	= NULL;

	snprintf( sPath, sizeof(sPath), "%s%s/%s", mNodeDirFilePathCStr, mapping->fRecordNativeType, inFileName );
	
	if ( mDatabaseHelper != NULL && lstat(sPath, &statBuffer) == 0 && (aFileData = CreateCFDataFromFile(sPath, statBuffer.st_size)) != NULL )
	{
		CFDictionaryRef recordDict = (CFDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, aFileData, kCFPropertyListImmutable, NULL );
		DSCFRelease( aFileData );
		
		// only if we got a valid dictionary
		if ( recordDict != NULL )
		{
			CFIndex	iCount		= CFArrayGetCount( mapping->fAttributesCF );
			char	command[256];
			
			mDatabaseHelper->BeginTransaction();
			
			for ( CFIndex ii = 0; ii < iCount; ii++ )
			{
				CFStringRef		cfStdAttrib		= (CFStringRef) CFArrayGetValueAtIndex( mapping->fAttributesCF, ii );
				CFStringRef		cfNativeAttr	= mPlugin->AttrNativeTypeForStandardType( cfStdAttrib );
				const char		*pAttrib		= mapping->fAttributes[ii];
				sqlite3_stmt	*pStmt			= NULL;
				
				// if this didn't map, just continue to he next attribute
				if ( cfNativeAttr == NULL ) continue;
				
				// first delete all entries with this file in it
				snprintf( command, sizeof(command), "DELETE FROM '%s' WHERE filename='%s' AND recordtype='%s';", pAttrib, inFileName, 
						 mapping->fRecordNativeType );
				
				int status = mDatabaseHelper->ExecSync( command );
				if ( status == SQLITE_CORRUPT )
					break;
				
				if ( status != SQLITE_OK )
				{
					snprintf( command, sizeof(command), "CREATE TABLE '%s' ('filename' TEXT, 'recordtype' TEXT, 'value' TEXT);", pAttrib );
					if ( sqlExecSync(command) == SQLITE_CORRUPT )
						break;
					
					snprintf( command, sizeof(command), "CREATE INDEX '%s.index' on '%s' ('value');", pAttrib, pAttrib );
					if ( sqlExecSync(command) == SQLITE_CORRUPT )
						break;
					
					DbgLog( kLogDebug, "CDSLocalPluginNode::AddRecordIndex - created table for attribute %s", pAttrib );
					
					status = SQLITE_OK;
				}
				
				if ( status == SQLITE_OK ) {
					snprintf( command, sizeof(command), "INSERT INTO '%s' ('filename','recordtype','value') VALUES (?,?,?);", pAttrib );
					status = mDatabaseHelper->Prepare( command, -1, &pStmt );
				}
				
				if ( status == SQLITE_OK )
					status = sqlite3_bind_text( pStmt, 1, inFileName, strlen(inFileName), SQLITE_STATIC );
				
				if ( status == SQLITE_OK )
					status = sqlite3_bind_text( pStmt, 2, mapping->fRecordNativeType, strlen(mapping->fRecordNativeType), SQLITE_STATIC );
				
				// now index the attribute
				CFArrayRef cfAttribute	= (CFArrayRef) CFDictionaryGetValue( recordDict, cfNativeAttr );
				if ( cfAttribute != NULL && status == SQLITE_OK )
				{
					CFMutableSetRef	cfSet		= CFSetCreateMutable( kCFAllocatorDefault, 0, &kCFTypeSetCallBacks );
					CFIndex			iAttrCnt	= CFArrayGetCount( cfAttribute );
					sIndexContext	sContext	= { this, pStmt, pAttrib, inFileName };
					
					for ( CFIndex zz = 0; zz < iAttrCnt; zz++ )
						CFSetAddValue( cfSet, CFArrayGetValueAtIndex(cfAttribute, zz) );
					
					CFSetApplyFunction( cfSet, IndexObject, &sContext );
					
					DSCFRelease( cfSet );
				}
				
				mDatabaseHelper->Finalize( pStmt );
			}
			
			snprintf( command, sizeof(command), "REPLACE INTO '%s' ('filetime','filename') VALUES (%d,'%s');", mapping->fRecordNativeType, 
					 (int) statBuffer.st_mtimespec.tv_sec, inFileName );
			sqlExecSync( command );
			
			mDatabaseHelper->EndTransaction();
			
			DSCFRelease( recordDict );
		}
	}
}

void CDSLocalPluginNode::DeleteRecordIndex( const char *inRecordType, const char* inFileName )
{
	LocalNodeIndexMapI iter = mIndexMap.find( inRecordType );
	if ( iter == mIndexMap.end() ) return;
	
	IndexMapping	*mapping	= iter->second;
	const char		**attribs	= mapping->fAttributes;
	char			command[256];
	
	mDatabaseHelper->BeginTransaction();
	
	while ( (*attribs) != NULL )
	{
		// first delete all entries with this file in it
		snprintf( command, sizeof(command), "DELETE FROM '%s' WHERE filename='%s' AND recordtype='%s';", (*attribs), inFileName, 
				  mapping->fRecordNativeType );
		if ( sqlExecSync(command) == SQLITE_CORRUPT )
			break;
		
		snprintf( command, sizeof(command), "DELETE FROM '%s' WHERE filename='%s';", mapping->fRecordNativeType, inFileName );
		if ( sqlExecSync(command) == SQLITE_CORRUPT )
			break;
		
		attribs++;
	}

	DbgLog( kLogPlugin, "CDSLocalPluginNode::DeleteRecordIndex - deleting index for %s type %s", inFileName, inRecordType );

	mDatabaseHelper->EndTransaction();
}

void CDSLocalPluginNode::LoadFileAccessIndex( void )
{
	struct stat statBuffer		= { 0 };
	static bool indexLoading	= false;
	
	// we don't load the index when running debug mode or install only daemon
	if ( gDSDebugMode == true || gDSInstallDaemonMode == true ) {
		return;
	}
	else if ( __sync_bool_compare_and_swap(&indexLoading, false, true) == false ) {
		mIndexLoaded.PostEvent();
		return;
	}

	mDatabaseHelper->LockDatabase();
	
	if ( mSafeBoot == true || mProperShutdown == false ) {
		mProperShutdown = true;
		mSafeBoot = false;
		mDatabaseHelper->RemoveDatabase();
	}
	
TryAgain:
	
	// if it fails to open here, we'll remove the file and attempt to open it again in the next if
	if ( mIndexMap.size() != 0 && mDatabaseHelper->OpenDatabase(true) == true )
	{
		LocalNodeIndexMapI	iter	= mIndexMap.begin();

		while ( iter != mIndexMap.end() )
		{
			IndexMapping	*mapping	= iter->second;
			char			sPath[PATH_MAX];
			char			command[1024];
			
			snprintf( sPath, sizeof(sPath), "%s%s", mNodeDirFilePathCStr, mapping->fRecordNativeType );
			DbgLog( kLogDebug, "CDSLocalPluginNode::LoadFileAccessIndex - Checking index for path %s", sPath );
			
			snprintf( command, sizeof(command), "CREATE TABLE '%s' ('filetime' INTEGER, 'filename' TEXT UNIQUE);", mapping->fRecordNativeType );
			if ( sqlExecSync(command) == SQLITE_CORRUPT ) {
				DbgLog( kLogDebug, "CDSLocalPluginNode::LoadFileAccessIndex - database is corrupted attempting to create table, deleting" );
				mDatabaseHelper->RemoveDatabase();
				goto TryAgain;
			}
			
			DIR *directory	= opendir( sPath );
			if ( directory != NULL )
			{
				dirent			*entry;
				sqlite3_stmt	*pStmt	= NULL;
				
				while ( (entry = readdir(directory)) != NULL )
				{
					bool	bIndex	= true;
					char	sPath2[PATH_MAX];
					
					if ( entry->d_name[0] == '.' ) continue;
					
					snprintf( sPath2, sizeof(sPath2), "%s/%s", sPath, entry->d_name );
					
					if ( stat(sPath2, &statBuffer) == 0 )
					{
						snprintf( command, sizeof(command), "SELECT filetime FROM '%s' WHERE filename='%s';", mapping->fRecordNativeType, entry->d_name );
						
						int status = mDatabaseHelper->Prepare( command, -1, &pStmt );
						if ( status == SQLITE_OK )
							status = mDatabaseHelper->Step( pStmt );

						if ( status == SQLITE_ROW )
						{
							if ( sqlite3_column_int(pStmt, 0) == (int) statBuffer.st_mtimespec.tv_sec )
								bIndex = false;
						}

						mDatabaseHelper->Finalize( pStmt );
					}
					
					if ( bIndex == true )
					{
						// release the lock here because AddRecordIndex grabs the lock and allow updates to occur while the
						// index is checked
						mDatabaseHelper->UnlockDatabase();

						DbgLog( kLogPlugin, "CDSLocalPluginNode::LoadFileAccessIndex - re-indexing type <%s> filename <%s>", mapping->fRecordNativeType, 
							    entry->d_name );
						AddRecordIndex( mapping->fRecordType, entry->d_name );
						
						mDatabaseHelper->LockDatabase();
					}
				}
				
				closedir( directory );
			}
			
			iter++;
		}
		
		__sync_bool_compare_and_swap( &mUseIndex, false, true );
	}
	
	mDatabaseHelper->UnlockDatabase();
	
	mIndexLoaded.PostEvent();
	__sync_bool_compare_and_swap( &indexLoading, true, false );
}

char** CDSLocalPluginNode::GetFileAccessIndex( CFStringRef inNativeRecType, tDirPatternMatch inPatternMatch, CFStringRef inPatternToMatch, 
											   CFStringRef inNativeAttrToMatch, bool *outPreferIndex )
{
	// default to no index
	if ( outPreferIndex != NULL ) 
		(*outPreferIndex) = false;
	
	if ( mUseIndex == false || inPatternToMatch == NULL || (inPatternMatch != eDSExact && inPatternMatch != eDSiExact) )
		return NULL;
	
	CFStringRef cfStdType = mPlugin->RecordStandardTypeForNativeType( inNativeRecType );
	if ( cfStdType == NULL )
		return NULL;
	
	CFStringRef	cfStdAttr = mPlugin->AttrStandardTypeForNativeType( inNativeAttrToMatch );
	if ( cfStdAttr == NULL )
		return NULL;
	
	char				*cStr		= NULL;
	LocalNodeIndexMapI	iter		= mIndexMap.find( CStrFromCFString(cfStdType, &cStr, NULL, NULL) );
	char				**fileNames	= NULL;
	
	DSFree( cStr );
	
	if ( iter == mIndexMap.end() )
		return NULL;
	
	// now see if the attribute is indexed
	IndexMapping	*mapping	= iter->second;
	CFIndex			iIndex		= CFArrayGetFirstIndexOfValue( mapping->fAttributesCF, CFRangeMake(0, CFArrayGetCount(mapping->fAttributesCF)), 
															   cfStdAttr );
	
	if ( iIndex != kCFNotFound )
	{
		char			command[512];
		sqlite3_stmt	*pStmt	= NULL;
		
		// we got this far, the value is indexed, so we'll prefer the index
		if ( outPreferIndex != NULL ) 
			(*outPreferIndex) = true;
		
		if ( inPatternMatch == eDSExact )
			snprintf( command, sizeof(command), "SELECT filename FROM '%s' WHERE value=? AND recordtype=?;", mapping->fAttributes[iIndex] );
		else
			snprintf( command, sizeof(command), "SELECT filename FROM '%s' WHERE value LIKE ? AND recordtype=?;", mapping->fAttributes[iIndex] );

		mDatabaseHelper->LockDatabase();

		int status = mDatabaseHelper->Prepare( command, -1, &pStmt );
		if ( status == SQLITE_OK )
		{
			if ( CFGetTypeID(inPatternToMatch) == CFStringGetTypeID() )
			{
				const char *pPattern = CStrFromCFString( inPatternToMatch, &cStr, NULL, NULL );
				status = sqlite3_bind_text( pStmt, 1, pPattern, strlen(pPattern), SQLITE_TRANSIENT );
				DSFree( cStr );
			}
			else if ( CFGetTypeID(inPatternToMatch) == CFDataGetTypeID() )
			{
				CFDataRef cfData = (CFDataRef) inPatternToMatch;
				status = sqlite3_bind_text( pStmt, 1, (char *) CFDataGetBytePtr(cfData), CFDataGetLength(cfData), SQLITE_TRANSIENT );
			}
		}

		if ( status == SQLITE_OK )
			status = sqlite3_bind_text( pStmt, 2, mapping->fRecordNativeType, strlen(mapping->fRecordNativeType), SQLITE_STATIC );
		
		if ( status == SQLITE_OK )
			status = mDatabaseHelper->Step( pStmt );
		
		int	iCount	= 0;
		int iMax	= -1; // start -1 so that we are one less for comparing index offset
		while ( status == SQLITE_ROW )
		{
			const char *pTemp = (const char *) sqlite3_column_text( pStmt, 0 );
			if ( pTemp != NULL )
			{
				if ( iCount == iMax || fileNames == NULL )
				{
					iMax += 20; // chunks of 20 at at a time
					char **temp = (char **) calloc( iMax + 1, sizeof(char *) );
					if ( iCount != 0 )
						bcopy( fileNames, temp, sizeof(char *) * iCount );
					DSFree( fileNames );
					fileNames = temp;
				}
				
				fileNames[iCount] = strdup( pTemp );
				DbgLog( kLogPlugin, "CDSLocalPluginNode::GetFileAccessIndex - found match in index - type <%s> file <%s>", 
						mapping->fRecordNativeType, fileNames[iCount] );
				
				iCount++;
			}
			
			status = mDatabaseHelper->Step( pStmt );
		}
		
		mDatabaseHelper->Finalize( pStmt );

		mDatabaseHelper->UnlockDatabase();
	}

	return fileNames;
}

int CDSLocalPluginNode::sqlExecSync(const char *command, int length)
{
	int	status	= SQLITE_ERROR;

	if ( mDatabaseHelper != NULL )
	{
		mDatabaseHelper->LockDatabase();
		
		status = mDatabaseHelper->ExecSync( command, length );
		if ( status == SQLITE_CORRUPT ) {
			DbgLog( kLogCritical, "CDSLocalPluginNode::sqlExecSync - SQL database corruption detected, rebuilding" );
			DatabaseCorrupt();
		}
		
		mDatabaseHelper->UnlockDatabase();
	}
	
	return status;
}

int CDSLocalPluginNode::EnsureDirs( const char *inPath, mode_t inPathMode, mode_t inFinalMode )
{
         register const char	*szpSrc = inPath;
         register char			c, *szpPath;
         char					szPath [PATH_MAX];
         struct stat			ssStatBuf;
		 
         if (!inPath)
                 return (errno = EFAULT), -1;
         if (PATH_MAX < strlen (szpSrc))
                 return (errno = ENAMETOOLONG), -1;
         // Try creating the directory: might get lucky!
         if (!mkdir (inPath, inFinalMode))
                 if (!chmod (inPath, inFinalMode))
                         return 0;
         szpPath = szPath;
         // Skip any leading slashes.
         // (We've got WAY more serious problems if root is missing!)
         if (*szpSrc == '/') {
                 *szpPath++ = '/';
                 while (*++szpSrc == '/');
         }

         do {
                 // Copy up to the next slash.
                 while ((c = *szpSrc++) && (c != '/'))
                         *szpPath++ = c;
                 // Remove multiple slashes.
                 while (c && ((*szpSrc == '/') || (*szpSrc == '\0')))
                         c = *szpSrc++;
                 // Terminate working string.
                 *szpPath = '\0';
                 // Check for directory existence.
                 if (stat (szPath, &ssStatBuf)) {
                         // Directory doesn't exist, try to create it.
                         if (errno == ENOENT) {
                                 errno = 0;
                                 if (!mkdir (szPath, (c ? inPathMode : inFinalMode)))
                                         chmod (szPath, (c ? inPathMode : inFinalMode));
                         }
                         // Bail if there was an error.
                         if (errno)
                                 return -1;
                 } else if (!S_ISDIR (ssStatBuf.st_mode)) {
                         // If the given path exists, but is not a directory, it's an error.
                         return (errno = ENOTDIR), -1;
                 }
                 *szpPath++ = '/';
         } while (c);
         return 0;
 }
 
#endif
