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

#include "FlatFileNode.h"
#include "BaseDirectoryPlugin.h"
#include "CLog.h"
#include "cache.h"
#include "CCachePlugin.h"
#include "crypt-md5.h"
#include <pthread.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/event.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "Mbrd_MembershipResolver.h"

#include <vector>
using std::vector;

#define kSQLBeginTransactionCmd		"BEGIN TRANSACTION"
#define kSQLEndTransactionCmd		"END TRANSACTION"
#define kSQLCommit					"COMMIT"

#define kSQLInsertFileLine			"INSERT INTO '%s' ('filelineno','line') VALUES (?,?);"

#define kSQLInsertTimestamp			"REPLACE INTO 'FileTimestamps' ('filetime','file') VALUES (%d,\"%s\");"
#define kSQLCreateTimestampTable	"CREATE TABLE 'FileTimestamps' ('filetime' INTEGER, 'file' TEXT UNIQUE);"
#define kSQLGetTimestamp			"SELECT filetime FROM 'FileTimestamps' WHERE file=?;"

#define kLineNoRowName				"filelineno"

const char *kDatabaseDirectory		= "/var/db/DirectoryService";
const char *kFlatFileNameDB			= "/var/db/DirectoryService/flatfile.db";
const char *kFlatFileNameDBJournal	= "/var/db/DirectoryService/flatfile.db-journal";

struct sSearchState
{
	bool				fSearchAll;
	bool				fHostSearch;
	int					fFileFD;
	char				*fFileData;
	char				*fSearchContext;
	int					fFileMapSize;
	sqlite3_stmt		*fStmt;
	CFMutableArrayRef	fPendingResults;
	sFileMapping		*fMapping;
};

extern CCachePlugin		*gCacheNode;

FlatFileConfigDataMap	FlatFileNode::fFFRecordTypeTable;
DSMutexSemaphore		FlatFileNode::fFlatFileLock("FlatFileNode::fFlatFileLock");
DSMutexSemaphore		FlatFileNode::fSQLDatabaseLock("FlatFileNode::fSQLDatabaseLock");
pthread_mutex_t			FlatFileNode::fWatchFilesLock			= PTHREAD_MUTEX_INITIALIZER;
sqlite3					*FlatFileNode::fSQLDatabase				= NULL;
bool					FlatFileNode::fProperShutdown			= false;
bool					FlatFileNode::fSafeBoot					= false;
int						FlatFileNode::fKqueue					= 0;
static bool				gUseSQLIndex							= true;

extern dsBool			gDSInstallDaemonMode;
extern dsBool			gDSLocalOnlyMode;
extern dsBool			gProperShutdown;
extern dsBool			gSafeBoot;

extern bool IntegrityCheckDB( sqlite3 *inDatabase );

FlatFileNode::FlatFileNode( CFStringRef inNodeName, uid_t inUID, uid_t inEffectiveUID ) : BDPIVirtualNode( inNodeName, inUID, inEffectiveUID )
{
	BuildTableMap();

	fFlatFileLock.WaitLock();
	
	if ( gDSInstallDaemonMode == false && gDSLocalOnlyMode == false && gUseSQLIndex == true )
	{
		if ( fSQLDatabase == NULL )
		{
			fProperShutdown = gProperShutdown;
			fSafeBoot = gSafeBoot;
			OpenOrCreateDatabase();
		}
	}

	fFlatFileLock.SignalLock();
}

FlatFileNode::~FlatFileNode( void )
{
	
}

CFMutableDictionaryRef FlatFileNode::CopyNodeInfo( CFArrayRef inAttributes )
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
		CFStringRef	cfRealName	= CFSTR("Flat Files in /etc");
		CFArrayRef	cfValue		= CFArrayCreate( kCFAllocatorDefault, (const void **) &cfRealName, 1, &kCFTypeArrayCallBacks );
		
		CFDictionarySetValue( cfAttributes, CFSTR(kDS1AttrDistinguishedName), cfValue );
		
		DSCFRelease( cfValue );
	}
	
	if ( bNeedAll || CFArrayContainsValue(inAttributes, cfAttribRange, CFSTR(kDSNAttrRecordType)) )
	{
		// we just get our mapping table and see
		FlatFileConfigDataMapI	mapIter = fFFRecordTypeTable.begin();
		CFMutableArrayRef		cfValue = CFArrayCreateMutable( kCFAllocatorDefault, fFFRecordTypeTable.size(), &kCFTypeArrayCallBacks );
		
		while ( mapIter != fFFRecordTypeTable.end() )
		{
			CFArrayAppendValue( cfValue, mapIter->second->fRecordTypeCF );
			
			++mapIter;
		}
			
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

tDirStatus FlatFileNode::VerifyCredentials( CFStringRef inRecordType, CFStringRef inRecordName, CFStringRef inPassword )
{
	tDirStatus	siResult	= eNotHandledByThisNode;
	
	if ( CFStringCompare(inRecordType, CFSTR(kDSStdRecordTypeUsers), 0) == kCFCompareEqualTo )
	{
		siResult = eDSAuthFailed;
		
		FlatFileConfigDataMapI iter = fFFRecordTypeTable.find( string(kDSStdRecordTypeUsers) );
		
		if ( iter != fFFRecordTypeTable.end() )
		{
			sFileMapping	*mapping		= iter->second;
			struct stat		statInfo;
			CFStringRef		cfRecordPass	= NULL;
			
			// we explicitly open the master.passwd file here to authenticate only
			int				fileFD			= open( "/etc/master.passwd", O_RDONLY );
			
			if ( fileFD != -1 )
			{
				if ( fstat(fileFD, &statInfo) == 0 && statInfo.st_size > 0 )
				{
					int		pagesize	= getpagesize();
					int		mapSize		= (((statInfo.st_size / pagesize) + 1) * pagesize);
					char	*lnResult	= NULL;
					
					// we add a page so we guarantee we have a NULL at the end of the data
					char *pFileData = (char *) mmap( NULL, mapSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fileFD, 0 );
					
					if ( pFileData != NULL )
					{
						char	*searchContext	= NULL;
						
						lnResult = strtok_r( pFileData, "\n\r", &searchContext );
						
						while ( lnResult != NULL && cfRecordPass == NULL )
						{
							if ( lnResult[0] != '#' )
							{
								CFMutableDictionaryRef	resultRef = mapping->fParseCallback( NULL, true, 0, lnResult, NULL );
								if ( resultRef != NULL )
								{
									CFDictionaryRef cfAttributes	= (CFDictionaryRef) CFDictionaryGetValue( resultRef, kBDPIAttributeKey );
									CFArrayRef		cfRecName		= (CFArrayRef) CFDictionaryGetValue( cfAttributes, CFSTR(kDSNAttrRecordName) );
									
									if ( cfRecName != NULL && CFArrayContainsValue( cfRecName, CFRangeMake(0, CFArrayGetCount(cfRecName)), inRecordName) )
									{
										CFArrayRef	cfValues	= (CFArrayRef) CFDictionaryGetValue( cfAttributes, CFSTR(kDS1AttrPassword) );
										
										if ( cfValues != NULL && CFArrayGetCount(cfValues) > 0 )
										{
											cfRecordPass = (CFStringRef) CFArrayGetValueAtIndex( cfValues, 0 );
											CFRetain( cfRecordPass ); // need to retain because it will be released below
										}
										else
										{
											cfRecordPass = CFSTR("");
										}
									}
									
									CFRelease( resultRef );
								}
							}
							
							lnResult = strtok_r( NULL, "\n\r", &searchContext );
						}
						
						munmap( pFileData, mapSize );
					}
				}
				
				close( fileFD );
			}
			
			if ( cfRecordPass != NULL && CheckPassword(cfRecordPass, inPassword) == true )
				siResult = eDSNoErr;
		}
	}
	
	return siResult;
}

tDirStatus FlatFileNode::SearchRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount)
{
	CFArrayRef	recordTypeList	= inContext->fRecordTypeList;
	tDirStatus	siResult		= eDSNoErr;

	if ( recordTypeList == NULL )
		return eDSInvalidRecordType;
	
	if ( inContext->fStateInfo != NULL )
	{
		InternalSearchRecords( inContext, inBuffer, outCount );
	}
	else
	{
		CFRange	cfValueRange	= CFRangeMake( 0, CFArrayGetCount(inContext->fValueList) );

		// if we are searching across all record types, we need to fix the type list to contain all the ones we support
		if ( CFArrayContainsValue(inContext->fRecordTypeList, CFRangeMake(0, CFArrayGetCount(recordTypeList)),
								  CFSTR(kDSStdRecordTypeAll)) )
		{
			CFMutableArrayRef	cfRecordTypeList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
			
			// all record search, let's rebuild the list and put all the record types we support
			FlatFileConfigDataMapI iter = fFFRecordTypeTable.begin();
			while (iter != fFFRecordTypeTable.end())
			{
				CFArrayAppendValue( cfRecordTypeList, iter->second->fRecordTypeCF );
				iter++;
			}
			
			CFRelease( inContext->fRecordTypeList );
			inContext->fRecordTypeList = cfRecordTypeList;
		}
		
		InternalSearchRecords( inContext, inBuffer, outCount );
	}
	
	return siResult;
}

#pragma mark -
#pragma mark Private functions

void FlatFileNode::WatchFiles( void )
{
	struct kevent			evWatch[fFFRecordTypeTable.size()];
	int						iWatchCount	= 0;
	u_int					events		= (NOTE_DELETE | NOTE_WRITE | NOTE_RENAME);
	FlatFileConfigDataMapI	mapIter		= fFFRecordTypeTable.begin();
	
	// use a local lock here since this is the only place these things are used
	// fFFRecordTypeTable does not need a lock at this point, it's already been created, it's never changed after
	
	pthread_mutex_lock( &fWatchFilesLock );
	
	if ( fKqueue == 0 )
	{
		fKqueue = kqueue();
		
		// we run our kevent on our main runloop
		CFFileDescriptorRef cfKQueue = CFFileDescriptorCreate( kCFAllocatorDefault, fKqueue, false, FileChangeNotification, NULL );
		CFRunLoopSourceRef src = CFFileDescriptorCreateRunLoopSource( kCFAllocatorSystemDefault, cfKQueue, 0 );
		CFFileDescriptorEnableCallBacks( cfKQueue, kCFFileDescriptorReadCallBack );
		CFRunLoopAddSource( CFRunLoopGetMain(), src, kCFRunLoopDefaultMode );
		
		CFRelease( cfKQueue );	
	}
	
	while ( mapIter != fFFRecordTypeTable.end() )
	{
		sFileMapping *mapping = mapIter->second;

		if ( mapping->fKeventFD < 0 && mapping->fFileName != NULL )
		{
			mapping->fKeventFD = open( mapping->fFileName, O_EVTONLY );

			if ( mapping->fKeventFD != -1 )
			{
				EV_SET( &evWatch[iWatchCount], mapping->fKeventFD, EVFILT_VNODE, EV_ADD | EV_ONESHOT, events, 0, (void *) mapping );
				DbgLog( kLogPlugin, "FlatFileNode::WatchFiles watching file: %s at event %d", mapping->fFileName, iWatchCount );
				iWatchCount++;
			}
			else
			{
				DbgLog( kLogPlugin, "FlatFileNode::WatchFiles failed on file: %s errno %d", mapping->fFileName, errno );
			}
			
		}
		
		++mapIter;
	}
	
	if ( iWatchCount != 0 )
	{
		kevent( fKqueue, evWatch, iWatchCount, NULL, 0, NULL );
	}
	
	pthread_mutex_unlock( &fWatchFilesLock );
}

void FlatFileNode::FileChangeNotification( CFFileDescriptorRef cfFD, CFOptionFlags callBackTypes, void *info )
{
	int				iTypeCount				= fFFRecordTypeTable.size();
	struct kevent	evReceive[iTypeCount];
	
	int fd = CFFileDescriptorGetNativeDescriptor( cfFD );
	
	int nevents = kevent( fd, NULL, 0, evReceive, iTypeCount, NULL );
	for (int ii = 0; ii < nevents; ii++)
	{
		sFileMapping	*mapping = (sFileMapping *) evReceive[ii].udata;
		
		// this is a one-shot event, let's just close the event FD
		pthread_mutex_lock( &fWatchFilesLock );

		close( mapping->fKeventFD );
		mapping->fKeventFD = -1;
		mapping->fTimeChecked.tv_sec = 0;	// set time stamp to 0 so we re-read
		
		pthread_mutex_unlock( &fWatchFilesLock );

		// if we haven't looked at the file yet, no reason to do anything
		if ( (evReceive[ii].fflags & NOTE_DELETE) != 0 || (evReceive[ii].fflags & NOTE_RENAME) != 0 )
			DbgLog( kLogPlugin, "FlatFileNode::FileChangeNotification %s renamed or deleted", mapping->fFileName );

		if ( (evReceive[ii].fflags & (NOTE_WRITE | NOTE_DELETE | NOTE_RENAME)) != 0 )
		{
			if ( mapping->fCacheType == CACHE_ENTRY_TYPE_GROUP || mapping->fCacheType == CACHE_ENTRY_TYPE_USER )
			{
				DbgLog( kLogPlugin, "FlatFileNode::FileChangeNotification %s - flushing membership cache", mapping->fFileName );
				Mbrd_ProcessResetCache();
			}
			
			gCacheNode->EmptyCacheEntryType( mapping->fCacheType );
			
			DbgLog( kLogPlugin, "FlatFileNode::FileChangeNotification %s - flushed cache type %d", mapping->fFileName, mapping->fCacheType );
		}
		else
		{
			DbgLog( kLogPlugin, "FlatFileNode::FileChangeNotification %s - unexpected event %d", mapping->fFileName, evReceive[ii].fflags );
		}
	}
	
	if ( nevents == 0 )
	{
		DbgLog( kLogPlugin, "FlatFileNode::FileChangeNotification - No events" );
	}
	
	CFFileDescriptorEnableCallBacks( cfFD, callBackTypes );
}

SInt32 FlatFileNode::InternalSearchRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount )
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
		stateInfo->fFileFD = -1;
		stateInfo->fHostSearch = false;
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
		
		FlatFileConfigDataMapI iter = fFFRecordTypeTable.find( string(pRecordType) );
		
		DSFreeString( pTemp );
		
		if ( iter == fFFRecordTypeTable.end() || iter->second->fFileName == NULL )
		{
			inContext->fRecTypeIndex++;
			siResult = eDSInvalidRecordType; // in case it was our last type
			continue;
		}
		
		stateInfo->fMapping = iter->second;
		stateInfo->fHostSearch = ( strcmp( stateInfo->fMapping->fRecordType, kDSStdRecordTypeHosts) == 0 );
		
		// check the timestamp of the files
		CheckTimeStamp( stateInfo->fMapping );
		
		if ( (stateInfo->fSearchAll || gDSInstallDaemonMode == true || gUseSQLIndex == false || stateInfo->fHostSearch ) && stateInfo->fFileFD == -1 )
		{
			struct stat	statInfo;
			int			fileFD = open( iter->second->fFileName, O_RDONLY );
			if ( fileFD == -1 )
			{
				inContext->fRecTypeIndex++;
				siResult = eDSInvalidRecordType; // in case it was our last type
				continue;
			}
			
			if ( fstat(fileFD, &statInfo) != 0 || statInfo.st_size == 0 )
			{
				close( fileFD );
				inContext->fRecTypeIndex++;
				siResult = eDSInvalidRecordType; // in case it was our last type
				continue;
			}
			
			int pagesize = getpagesize();
			
			// we add a page so we guarantee we have a NULL at the end of the data
			stateInfo->fFileMapSize = (((statInfo.st_size / pagesize) + 1) * pagesize);
			stateInfo->fFileData = (char *) mmap( NULL, stateInfo->fFileMapSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fileFD, 0 );
			stateInfo->fFileFD = fileFD;
			stateInfo->fSearchContext = NULL;
		}
		
		// now we do the appropriate search
		if ( stateInfo->fSearchAll )
			siResult = FetchAllRecords( inContext, inBuffer, outCount );
		else
			siResult = FetchMatchingRecords( inContext, inBuffer, outCount );
		
		// if we got to a NULL context, must be time for a new search next time around
		if ( stateInfo->fSearchContext == NULL )
		{
			munmap( stateInfo->fFileData, stateInfo->fFileMapSize );
			close( stateInfo->fFileFD );
			stateInfo->fFileMapSize = 0;
			stateInfo->fFileFD = -1;
			stateInfo->fFileData = NULL;
			stateInfo->fSearchContext = NULL;

			inContext->fRecTypeIndex++;
		}
		
		if ( siResult != eDSNoErr || (*outCount) > 0 )
			break;
	}
	
	return siResult;
}

SInt32 FlatFileNode::FetchMatchingRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount )
{
	// here we search the DB to find the match if we have an index for it
	sSearchState	*stateInfo	= (sSearchState *) inContext->fStateInfo;

	// first build our querys string, we only search things we have indexed (this is intentional)
	CFArrayRef	cfIndexedAttribs = stateInfo->fMapping->fAttributesCF;

	bool bIsIndexed;
	if ( cfIndexedAttribs == NULL )
	{
		bIsIndexed = false;
	}
	else
	{
		bIsIndexed = CFArrayContainsValue( cfIndexedAttribs, CFRangeMake(0, CFArrayGetCount(cfIndexedAttribs)), inContext->fAttributeType );
	}
	
	// if we are install daemon or not indexed we have to go to the file directly to find the answer
	// and this isn't a host lookup (they go to the files directly due to complexities of the file)
	if ( gDSInstallDaemonMode == false && bIsIndexed == true && gUseSQLIndex == true && stateInfo->fHostSearch == false )
	{
		const char	*queryFmt;
		
		switch ( inContext->fPattMatchType )
		{
			case eDSiExact:
				queryFmt = "SELECT filelineno,\"%s\" FROM \"%s\" WHERE \"%s\" LIKE ?;";
				break;
			case eDSExact:
				queryFmt = "SELECT filelineno,\"%s\" FROM \"%s\" WHERE \"%s\"=?;";
				break;
			default:
				return eDSUnSupportedMatchType;
		}

		char		*pTemp		= NULL;
		char		query[1024];
		const char	*attribute	= BaseDirectoryPlugin::GetCStringFromCFString( inContext->fAttributeType, &pTemp );
		CFIndex		iCount		= CFArrayGetCount( inContext->fValueList );
		
		for( CFIndex ii = 0; ii < iCount; ii++)
		{
			CFTypeRef	cfValue	= CFArrayGetValueAtIndex( inContext->fValueList, ii );

			snprintf( query, sizeof(query), queryFmt, attribute, stateInfo->fMapping->fRecordType, attribute );
			
			vector<int>		filelineno;
			filelineno.reserve(20);

			sqlite3_stmt	*pStmt		= NULL;
			
			fSQLDatabaseLock.WaitLock();
			
			int status = sqlite3_prepare( fSQLDatabase, query, -1, &pStmt, NULL );
			if ( status == SQLITE_OK )
			{
				// it's either CFDataRef or it's CFStringRef
				if ( CFGetTypeID(cfValue) == CFStringGetTypeID() ) {
					char		*pTemp2	= NULL;
					
					const char *value = BaseDirectoryPlugin::GetCStringFromCFString( (CFStringRef) cfValue, &pTemp2 );
					status = sqlite3_bind_text( pStmt, 1, value, -1, SQLITE_TRANSIENT );
					
					DSFreeString( pTemp2 );
				}
				else {
					CFDataRef cfData = (CFDataRef) cfValue;
					
					status = sqlite3_bind_blob( pStmt, 1, CFDataGetBytePtr(cfData), CFDataGetLength(cfData), SQLITE_TRANSIENT );
				}
			}
			
			if ( status == SQLITE_OK )
			{
				do
				{
					status = sqlite3_step( pStmt );
					if ( status == SQLITE_ROW )
					{
						if ( sqlite3_column_type(pStmt, 0) == SQLITE_INTEGER )
						{
							filelineno.push_back( sqlite3_column_int( pStmt, 0 ) );
						}
					}
				} while (status == SQLITE_ROW);

				status = sqlite3_finalize( pStmt );
			}
			
			uint lineCount = filelineno.size();
			for ( uint zz = 0; zz < lineCount && (inContext->fMaxRecCount == 0 || inContext->fIndex < inContext->fMaxRecCount); zz++ )
			{
				const unsigned char *text = NULL;
				
				if ( stateInfo->fMapping->fFileName == NULL )
					continue;
				
				snprintf( query, sizeof(query), "SELECT line FROM \"%s\" WHERE filelineno=\"%d\";", stateInfo->fMapping->fFileName, filelineno[zz] );
				
				status = sqlite3_prepare( fSQLDatabase, query, -1, &pStmt, NULL );
				if ( status == SQLITE_OK )
				{
					do
					{
						status = sqlite3_step( pStmt );
						if ( status == SQLITE_ROW )
						{
							if ( sqlite3_column_type(pStmt, 0) == SQLITE_TEXT )
							{
								text = sqlite3_column_text( pStmt, 0 );
								
								if ( text != NULL )
								{
									char *textLine = strdup( (const char *) text );
									
									CFMutableDictionaryRef cfRecord = stateInfo->fMapping->fParseCallback( NULL, true, 0, textLine, NULL );
									
									if ( cfRecord != NULL )
									{
										FilterAttributes( cfRecord, inContext->fReturnAttribList );
										CFArrayAppendValue( stateInfo->fPendingResults, cfRecord );
										CFRelease( cfRecord );
										
										inContext->fIndex++;
									}
									
									DSFreeString( textLine );

									if ( inContext->fMaxRecCount > 0 && inContext->fIndex >= inContext->fMaxRecCount )
										break;
								}
							}
						}
					} while (status == SQLITE_ROW);
					
					status = sqlite3_finalize( pStmt );
				}
			}
			
			fSQLDatabaseLock.SignalLock();
		}
		
		DSFreeString( pTemp );
	}

	// host searches are special, we have to parse the entire file and return a consolidated answer since entries are by IP not by name
	if ( stateInfo->fFileData != NULL && stateInfo->fHostSearch == true )
	{
		char*	lnResult	= NULL;
		
		if (stateInfo->fSearchContext == NULL)
			lnResult = strtok_r( stateInfo->fFileData, "\n\r", &(stateInfo->fSearchContext) );
		else
			lnResult = strtok_r( NULL, "\n\r", &(stateInfo->fSearchContext) );
		
		CFMutableDictionaryRef	cfParsedAnswers = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
																			 &kCFTypeDictionaryValueCallBacks );
		while ( lnResult != NULL )
		{
			if ( lnResult[0] != '#' )
			{
				CFMutableDictionaryRef	cfRecord = stateInfo->fMapping->fParseCallback( NULL, true, 0, lnResult, NULL );
				if ( cfRecord )
				{
					CFStringRef				cfRecName		= (CFStringRef) CFDictionaryGetValue( cfRecord, kBDPINameKey );
					CFMutableDictionaryRef	cfHostRecord	= (CFMutableDictionaryRef) CFDictionaryGetValue( cfParsedAnswers, cfRecName );
					
					if ( cfHostRecord == NULL )
					{
						CFDictionarySetValue( cfParsedAnswers, cfRecName, cfRecord );
					}
					else
					{
						// here we just add the IP addresses because that is the only difference
						CFMutableDictionaryRef	cfAttribs;
						CFDictionaryRef			cfAddAddribs;
						CFMutableArrayRef		cfValues;
						CFArrayRef				cfAddValues;
						CFStringRef				mergeKeys[] = { CFSTR(kDSNAttrIPAddress), CFSTR(kDSNAttrIPv6Address), NULL };
						int						mergeIndex  = 0;
						
						cfAttribs = (CFMutableDictionaryRef) CFDictionaryGetValue( cfHostRecord, kBDPIAttributeKey );
						cfAddAddribs = (CFDictionaryRef) CFDictionaryGetValue( cfRecord, kBDPIAttributeKey );
						
						while ( mergeKeys[mergeIndex] != NULL )
						{
							cfAddValues = (CFArrayRef) CFDictionaryGetValue( cfAddAddribs, mergeKeys[mergeIndex] );
							if ( cfAddValues != NULL )
							{
								cfValues = (CFMutableArrayRef) CFDictionaryGetValue( cfAttribs, mergeKeys[mergeIndex] );
								if ( cfValues == NULL )
								{
									// just add the values no pre-existing ones
									CFDictionarySetValue( cfAttribs, mergeKeys[mergeIndex], cfAddValues );
								}
								else
								{
									for ( CFIndex ii = 0; ii < CFArrayGetCount( cfAddValues ); ii++ )
									{
										CFTypeRef   cfValue = CFArrayGetValueAtIndex( cfAddValues, ii );
										CFRange     cfRange = CFRangeMake( 0, CFArrayGetCount(cfValues) );
										
										// append it if it doesn't exist
										if ( CFArrayContainsValue(cfValues, cfRange, cfValue) == false )
											CFArrayAppendValue( cfValues, cfValue );
									}
								}
							}
							
							mergeIndex++;
						}
					}
					
					DSCFRelease( cfRecord );
				}
			}
			
			lnResult = strtok_r( NULL, "\n\r", &(stateInfo->fSearchContext) );
		}
		
		// now search check for the one we are looking for
		CFIndex		iAnsCount	= CFDictionaryGetCount( cfParsedAnswers );
		CFTypeRef	*cfValues	= (CFTypeRef *) calloc( CFDictionaryGetCount(cfParsedAnswers), sizeof(CFTypeRef) );
		
		CFDictionaryGetKeysAndValues( cfParsedAnswers, NULL, cfValues );
		
		for ( CFIndex iAns = 0; iAns < iAnsCount && (inContext->fMaxRecCount == 0 || inContext->fIndex < inContext->fMaxRecCount); iAns++ )
		{
			bool					bFound		= false;
			CFMutableDictionaryRef	resultRef	= (CFMutableDictionaryRef) cfValues[iAns];

			CFIndex	iCount	= CFArrayGetCount( inContext->fValueList );
			for (CFIndex ii = 0; ii < iCount && bFound == false; ii++)
			{
				CFTypeRef		cfSearchValue	= CFArrayGetValueAtIndex( inContext->fValueList, ii );
				CFDictionaryRef cfAttributes	= (CFDictionaryRef) CFDictionaryGetValue( resultRef, kBDPIAttributeKey );
				CFArrayRef		cfValueArray	= (CFArrayRef) CFDictionaryGetValue( cfAttributes, inContext->fAttributeType );
				
				if ( cfValueArray != NULL )
				{
					switch (inContext->fPattMatchType)
					{
						case eDSiExact:
							if ( CFStringGetTypeID() == CFGetTypeID(cfSearchValue) )
							{
								CFIndex valCnt = CFArrayGetCount( cfValueArray );
								for ( CFIndex zz = 0; zz < valCnt && bFound == false; zz++ )
								{
									CFTypeRef cfValue = CFArrayGetValueAtIndex( cfValueArray, zz );
									
									if ( CFStringGetTypeID() == CFGetTypeID(cfValue) )
										bFound = (CFStringCompare( (CFStringRef) cfValue, (CFStringRef) cfSearchValue, kCFCompareCaseInsensitive ) == kCFCompareEqualTo);
								}
							}
							break;
						case eDSExact:
							bFound = CFArrayContainsValue( cfValueArray, CFRangeMake(0, CFArrayGetCount(cfValueArray)), cfSearchValue );
							break;
						default:
							break;
					}
				}
			}
			
			if ( bFound )
			{
				FilterAttributes( resultRef, inContext->fReturnAttribList );
				CFArrayAppendValue( stateInfo->fPendingResults, resultRef );
				
				inContext->fIndex++;
			}
		}
		
		DSCFRelease( cfParsedAnswers );
		DSFree( cfValues );
	}
	else if ( stateInfo->fFileData != NULL && (bIsIndexed == false || gDSInstallDaemonMode == true || gUseSQLIndex == false) )
	{
		// if we are not using our index
		char*	lnResult	= NULL;
		bool	bFound		= false;

		if (stateInfo->fSearchContext == NULL)
			lnResult = strtok_r( stateInfo->fFileData, "\n\r", &(stateInfo->fSearchContext) );
		else
			lnResult = strtok_r( NULL, "\n\r", &(stateInfo->fSearchContext) );
		
		while ( lnResult != NULL && (inContext->fMaxRecCount == 0 || inContext->fIndex < inContext->fMaxRecCount) )
		{
			if ( lnResult[0] != '#' )
			{
				CFMutableDictionaryRef	resultRef = stateInfo->fMapping->fParseCallback( NULL, true, 0, lnResult, NULL );
				if ( resultRef )
				{
					CFIndex		iCount		= CFArrayGetCount( inContext->fValueList );
					
					for (CFIndex ii = 0; ii < iCount && bFound == false; ii++)
					{
						CFTypeRef		cfSearchValue	= CFArrayGetValueAtIndex( inContext->fValueList, ii );
						CFDictionaryRef cfAttributes	= (CFDictionaryRef) CFDictionaryGetValue( resultRef, kBDPIAttributeKey );
						CFArrayRef		cfValues		= (CFArrayRef) CFDictionaryGetValue( cfAttributes, inContext->fAttributeType );
						
						if ( cfValues != NULL )
						{
							switch (inContext->fPattMatchType)
							{
								case eDSiExact:
									if ( CFStringGetTypeID() == CFGetTypeID(cfSearchValue) )
									{
										CFIndex valCnt = CFArrayGetCount( cfValues );
										for ( CFIndex zz = 0; zz < valCnt && bFound == false; zz++ )
										{
											CFTypeRef cfValue = CFArrayGetValueAtIndex( cfValues, zz );
											
											if ( CFStringGetTypeID() == CFGetTypeID(cfValue) )
												bFound = (CFStringCompare( (CFStringRef) cfValue, (CFStringRef) cfSearchValue, kCFCompareCaseInsensitive ) == kCFCompareEqualTo);
										}
									}
									break;
								case eDSExact:
									bFound = CFArrayContainsValue( cfValues, CFRangeMake(0, CFArrayGetCount(cfValues)), cfSearchValue );
									break;
								default:
									break;
							}
						}
					}
					
					if ( bFound )
					{
						FilterAttributes( resultRef, inContext->fReturnAttribList );
						CFArrayAppendValue( stateInfo->fPendingResults, resultRef );

						inContext->fIndex++;
					}

					CFRelease( resultRef );
				}
			}
			
			lnResult = strtok_r( NULL, "\n\r", &(stateInfo->fSearchContext) );
		}
	}
	
	(*outCount) = BaseDirectoryPlugin::FillBuffer( stateInfo->fPendingResults, inBuffer );

	return ((*outCount) == 0 && CFArrayGetCount(stateInfo->fPendingResults) > 0 ? eDSBufferTooSmall : eDSNoErr);
}

SInt32 FlatFileNode::FetchAllRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount )
{
	sSearchState	*stateInfo	= (sSearchState *) inContext->fStateInfo;
	char			*lnResult	= NULL;
	
	if ( stateInfo->fFileData == NULL )
		return eDSNoErr;
	
	if ( stateInfo->fSearchContext == NULL )
		lnResult = strtok_r( stateInfo->fFileData, "\n\r", &(stateInfo->fSearchContext) );
	else
		lnResult = strtok_r( NULL, "\n\r", &(stateInfo->fSearchContext) );
	
	while ( lnResult != NULL )
	{
		if ( lnResult[0] != '#' )
		{
			CFMutableDictionaryRef	resultRef = stateInfo->fMapping->fParseCallback( NULL, true, 0, lnResult, NULL );
			if ( resultRef )
			{
				FilterAttributes( resultRef, inContext->fReturnAttribList );
				CFArrayAppendValue( stateInfo->fPendingResults, resultRef );
				CFRelease( resultRef );
				
				inContext->fIndex++;
				
				// we will only do 20 results at a time or when we reached our max
				if ( inContext->fIndex % 20 == 0 || (inContext->fMaxRecCount > 0 && inContext->fIndex >= inContext->fMaxRecCount) )
					break;
			}
		}

		lnResult = strtok_r( NULL, "\n\r", &(stateInfo->fSearchContext) );
	}
	
	(*outCount) = BaseDirectoryPlugin::FillBuffer( stateInfo->fPendingResults, inBuffer );
	
	return ((*outCount) == 0 && CFArrayGetCount(stateInfo->fPendingResults) > 0 ? eDSBufferTooSmall : eDSNoErr);
}

void FlatFileNode::FreeSearchState( void *inState )
{
	sSearchState	*theState = (sSearchState *) inState;
    
    if ( theState == NULL )
    {
        return;
    }
    
    if ( theState->fSearchContext != NULL )
    {
        DbgLog( kLogPlugin, "FlatFileNode::FreeSearchState - called with fSearchContext != NULL - SearchRecords is in progress" );
        theState->fSearchContext = NULL;
    }
	
	if ( theState->fStmt != NULL )
	{
		sqlite3_finalize( theState->fStmt );
        theState->fStmt = NULL;
	}
	
	if ( theState->fFileData != NULL )
	{
        DbgLog( kLogPlugin, "FlatFileNode::FreeSearchState - called with fFileData != NULL - SearchRecords is in progress" );
		munmap( theState->fFileData, theState->fFileMapSize );
        theState->fFileData = NULL;
        theState->fFileMapSize = 0;
	}

	if ( theState->fFileFD != -1 )
	{
		close( theState->fFileFD );
        theState->fFileFD = -1;
	}
	
	DSCFRelease( theState->fPendingResults );
	
	DSFree( inState );
}

#pragma mark -
#pragma mark --- Database creation routines ---

void FlatFileNode::OpenOrCreateDatabase( void )
{
	// we throw out the db on every launch, it's quick to create
	struct stat fileStat;
	bool		bRecreate	= false;
	mode_t		dirPrivs	= S_IRWXU | S_IRWXG;
	
	fSQLDatabaseLock.WaitLock();
	
	// see if path exists
	if ( lstat(kDatabaseDirectory, &fileStat) == 0 )
	{
		// if the path is not a directory, remove it, otherwise check the permissions
		if ( S_ISDIR(fileStat.st_mode) == false )
		{
			remove( kDatabaseDirectory );
			
			// now create it
			if ( mkdir(kDatabaseDirectory, dirPrivs) != 0 )
				return;
		}
		else if ( (fileStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) != dirPrivs ) // ensure we don't have other bits set
			chmod( kDatabaseDirectory, dirPrivs );
	}
	else if ( mkdir(kDatabaseDirectory, dirPrivs) != 0 )
	{
		// if we fail to create it, just bail, we won't use the index
		return;
	}
	
	if ( lstat(kFlatFileNameDB, &fileStat) != 0 || fileStat.st_size == 0 || fSafeBoot == true || fProperShutdown == false )
	{
		unlink( kFlatFileNameDB );
		unlink( kFlatFileNameDBJournal );
		bRecreate = true;

		// we clear the flag so we don't get into a loop
		fProperShutdown = true; 
		fSafeBoot = false;
	}
	
retry:
	
	if ( sqlite3_open(kFlatFileNameDB, &fSQLDatabase) == SQLITE_OK )
	{
		// let's check integrity, if we aren't recreating
		if ( bRecreate == false ) {
			
			// let's change the default cache for the DB to 50 x 1.5k = 75k
			sqlExecSync( "PRAGMA cache_size = 50" );
			
			if ( IntegrityCheckDB(fSQLDatabase) == false ) {
				DbgLog( kLogCritical, "FlatFileNode::OpenOrCreateDatabase - index failed integrity check - rebuilding" );
				sqlite3_close( fSQLDatabase );
				fSQLDatabase = NULL;
				
				unlink( kFlatFileNameDB );
				unlink( kFlatFileNameDBJournal );
				
				bRecreate = true;
				
				goto retry;
			}
			else {
				SrvrLog( kLogApplication, "BSD Plugin - index passed integrity check" );
			}
		}
		
		// let's change the default cache for the DB to 500 x 1.5k = 750k
		sqlExecSync( "PRAGMA cache_size = 500" );

		if ( bRecreate )
			sqlExecSync( kSQLCreateTimestampTable );
		
		GetDatabaseTimestamps();
		
		// create our tables
		FlatFileConfigDataMapI	mapIter = fFFRecordTypeTable.begin();
		
		while (mapIter != fFFRecordTypeTable.end())
		{
			// check our timestamps... if they don't match, recreate the table
			CheckTimeStamp( mapIter->second );

			++mapIter;
		}
	}
	
	fSQLDatabaseLock.SignalLock();
}
		
void FlatFileNode::CreateTableForRecordMap( sFileMapping *inFileType )
{
	CFIndex				iCount			= CFArrayGetCount( inFileType->fAttributesCF );
	char				*tmpStr			= NULL;
	char				*tmpStr2		= NULL;
	CFMutableStringRef	cfCommand		= CFStringCreateMutable( kCFAllocatorDefault, 0 );
	CFMutableStringRef	cfIndex			= CFStringCreateMutable( kCFAllocatorDefault, 0 );
	char				table2cmd[512];
	char				indexcmd[512];
	
	if ( inFileType->fFileName == NULL )
		return;
	
	// first create the table
	CFStringAppendFormat( cfCommand, NULL, CFSTR("CREATE TABLE '%@' ('%s' INTEGER"), inFileType->fRecordTypeCF, kLineNoRowName );
	CFStringAppendFormat( cfIndex, NULL, CFSTR("CREATE INDEX '%@.index' on '%@' ("), inFileType->fRecordTypeCF, inFileType->fRecordTypeCF );

	for (CFIndex ii = 0; ii < iCount; ii++ )
	{
		if ( ii != 0 )
			CFStringAppend( cfIndex, CFSTR(",") );
		
		CFStringRef	cfAttrib = (CFStringRef) CFArrayGetValueAtIndex(inFileType->fAttributesCF, ii);
		
		CFStringAppendFormat( cfCommand, NULL, CFSTR(", '%@' TEXT"), cfAttrib );
		CFStringAppendFormat( cfIndex, NULL, CFSTR("'%@'"), cfAttrib );
	}

	CFStringAppend( cfCommand, CFSTR(");") );
	CFStringAppend( cfIndex, CFSTR(");") );
	
	const char *sqlCommand = BaseDirectoryPlugin::GetCStringFromCFString( cfCommand, &tmpStr );
	const char *index = BaseDirectoryPlugin::GetCStringFromCFString( cfIndex, &tmpStr2 );
	
	snprintf( table2cmd, sizeof(table2cmd), "CREATE TABLE '%s' ('filelineno' INTEGER, 'line' TEXT);", inFileType->fFileName );
	snprintf( indexcmd, sizeof(indexcmd), "CREATE INDEX '%s.index' on '%s' ('filelineno');", inFileType->fFileName, inFileType->fFileName );
	
	fSQLDatabaseLock.WaitLock();

	sqlExecSync( kSQLBeginTransactionCmd );
	sqlExecSync( sqlCommand );
	sqlExecSync( index );
	sqlExecSync( table2cmd );
	sqlExecSync( indexcmd );
	sqlExecSync( kSQLEndTransactionCmd );
	sqlExecSync( kSQLCommit );

	// no go parse the file and insert the entries
	struct stat fileStat;
	if ( stat(inFileType->fFileName, &fileStat) == 0 )
	{
		char	command[1024];
		
		sqlExecSync( kSQLBeginTransactionCmd );

		FILE	*fd = fopen( inFileType->fFileName, "r" );
		if ( fd != NULL )
		{
			char*	lnResult	= NULL;
			int		iLineCount	= 1;
			size_t	lineLen		= 0;

			while ( (lnResult = fgetln(fd, &lineLen)) != NULL )
			{
				// if we haven't reached the end of file
				if ( lineLen == 0 && feof(fd) )
					break;
				
				if (lineLen > 1 && lnResult[0] != '#')
				{
					sqlite3_stmt	*pStmt	= NULL;
					
					lnResult[--lineLen] = '\0';

					snprintf( command, sizeof(command), kSQLInsertFileLine, inFileType->fFileName );

					// there's a table with the filename
					int status = sqlite3_prepare( fSQLDatabase, command, -1, &pStmt, NULL );	
					if ( status == SQLITE_OK )
					{
						// "INSERT INTO '%s' ('filelineno','line') VALUES (?,?);"
						status = sqlite3_bind_int( pStmt, 1, iLineCount );
						if ( status == SQLITE_OK )
						{
							status = sqlite3_bind_text( pStmt, 2, lnResult, lineLen, SQLITE_TRANSIENT );
							if ( status == SQLITE_OK )
							{
								sqlite3_step( pStmt );
							}
						}
						
						sqlite3_finalize( pStmt );
					}
					
					// so let's use our parser to add the line to the database
					inFileType->fParseCallback( fSQLDatabase, false, iLineCount, lnResult, NULL );
				}
				
				iLineCount++;
			}

			fclose( fd );
		}
		
		snprintf( command, sizeof(command), kSQLInsertTimestamp, (int) fileStat.st_mtimespec.tv_sec, inFileType->fFileName );
		sqlExecSync( command );
		
		sqlExecSync( kSQLEndTransactionCmd );
		sqlExecSync( kSQLCommit );
	}

	fSQLDatabaseLock.SignalLock();

	DSCFRelease( cfIndex );
	DSCFRelease( cfCommand );
	DSFreeString( tmpStr );
	DSFreeString( tmpStr2 );
}

void FlatFileNode::CheckTimeStamp( sFileMapping *inFileType )
{
	char		command[512];
	struct stat	fileStat	= { 0 };
	
	if ( inFileType->fFileName == NULL )
		return;
	
	fSQLDatabaseLock.WaitLock();
	
	int statErr = stat( inFileType->fFileName, &fileStat );
	if ( fSQLDatabase != NULL && (statErr != 0 || fileStat.st_mtimespec.tv_sec != inFileType->fTimeChecked.tv_sec) )
	{
		// drop the table so we don't find results since the file is gone or has changed
		snprintf( command, sizeof(command), "DROP TABLE IF EXISTS '%s';", inFileType->fRecordType );
		sqlExecSync( command );
		
		// drop the table so we don't find results since the file is gone or has changed
		snprintf( command, sizeof(command), "DROP TABLE IF EXISTS '%s';", inFileType->fFileName );
		sqlExecSync( command );
		
		snprintf( command, sizeof(command), "DROP INDEX '%s.index';", inFileType->fFileName );
		sqlExecSync( command );

		snprintf( command, sizeof(command), "DROP INDEX '%s.index';", inFileType->fRecordType );
		sqlExecSync( command );

		// if we didn't error doing the stat, then the file must have changed, update it
		if ( statErr == 0 )
		{
			DbgLog( kLogPlugin, "FlatFileNode::CheckTimeStamp rebuilding table for %s", inFileType->fFileName );
			CreateTableForRecordMap( inFileType );
			
			inFileType->fTimeChecked.tv_sec = fileStat.st_mtimespec.tv_sec;

			WatchFiles();
		}
	}

	fSQLDatabaseLock.SignalLock();
}

void FlatFileNode::GetDatabaseTimestamps( void )
{
	FlatFileConfigDataMapI	mapIter = fFFRecordTypeTable.begin();
	
	while (mapIter != fFFRecordTypeTable.end())
	{
		if ( mapIter->second->fFileName == NULL ) {
			++mapIter;
			continue;
		}
		
		sqlite3_stmt	*pStmt	= NULL;
		
		fSQLDatabaseLock.WaitLock();

		int status = sqlite3_prepare( fSQLDatabase, kSQLGetTimestamp, -1, &pStmt, NULL );
		if ( status == SQLITE_OK )
		{
			status = sqlite3_bind_text( pStmt, 1, mapIter->second->fFileName, strlen(mapIter->second->fFileName), SQLITE_TRANSIENT );
			if ( status == SQLITE_OK )
			{
				status = sqlite3_step( pStmt );
				if ( status == SQLITE_ROW )
				{
					if ( sqlite3_column_type(pStmt, 0) == SQLITE_INTEGER )
					{
						mapIter->second->fTimeChecked.tv_sec = sqlite3_column_int( pStmt, 0 );
						DbgLog( kLogPlugin, "FlatFileNode::GetDatabaseTimestamps timestamp %d for %s", (int) mapIter->second->fTimeChecked.tv_sec, 
							   mapIter->second->fFileName );
					}
				}
			}
			
			status = sqlite3_finalize( pStmt );
		}

		fSQLDatabaseLock.SignalLock();
		
		++mapIter;
	}
}

void FlatFileNode::BuildTableMap( void )
{
	fFlatFileLock.WaitLock();
	
	if ( fFFRecordTypeTable.empty() )
	{
		InsertFileMapping( kDSStdRecordTypeUsers, "/etc/passwd", CACHE_ENTRY_TYPE_USER, parse_user, kDSNAttrRecordName, kDS1AttrUniqueID, 
						   kDS1AttrPrimaryGroupID, kDS1AttrDistinguishedName, NULL );
		
		InsertFileMapping( kDSStdRecordTypeGroups, "/etc/group", CACHE_ENTRY_TYPE_GROUP, parse_group, kDSNAttrRecordName, kDS1AttrPrimaryGroupID, 
						   kDSNAttrGroupMembership, NULL );
		
		InsertFileMapping( kDSStdRecordTypeAliases, "/etc/aliases", CACHE_ENTRY_TYPE_ALIAS, parse_alias, kDSNAttrRecordName, NULL );
		
		InsertFileMapping( kDSStdRecordTypeBootp, "/etc/bootptab", 0, parse_bootp, kDSNAttrRecordName, kDS1AttrENetAddress,
						   kDSNAttrIPAddress, NULL );
		
		InsertFileMapping( kDSStdRecordTypeEthernets, "/etc/ethers", CACHE_ENTRY_TYPE_COMPUTER, parse_ethernet, kDS1AttrENetAddress,
						   kDSNAttrRecordName, NULL );
		
		InsertFileMapping( kDSStdRecordTypeHosts, "/etc/hosts", CACHE_ENTRY_TYPE_HOST, parse_host, kDSNAttrIPAddress, kDSNAttrIPv6Address,
						   kDSNAttrRecordName, NULL );
		
		InsertFileMapping( kDSStdRecordTypeMounts, "/etc/fstab", CACHE_ENTRY_TYPE_MOUNT, parse_mount, kDSNAttrRecordName, NULL );
		
		InsertFileMapping( kDSStdRecordTypeNetGroups, "/etc/netgroup", CACHE_ENTRY_TYPE_GROUP, parse_netgroup, kDSNAttrRecordName, NULL );
		
		InsertFileMapping( kDSStdRecordTypeNetworks, "/etc/networks", CACHE_ENTRY_TYPE_NETWORK, parse_network, kDSNAttrRecordName, 
						   "dsAttrTypeNative:address", NULL );
		
		InsertFileMapping( kDSStdRecordTypePrinters, "/etc/printcap", 0, parse_printer, kDSNAttrRecordName, NULL );
		
		InsertFileMapping( kDSStdRecordTypeProtocols, "/etc/protocols", CACHE_ENTRY_TYPE_PROTOCOL, parse_protocol, kDSNAttrRecordName,
						   "dsAttrTypeNative:number", NULL );
		
		InsertFileMapping( kDSStdRecordTypeRPC, "/etc/rpc", CACHE_ENTRY_TYPE_RPC, parse_rpc, kDSNAttrRecordName, "dsAttrTypeNative:number", NULL );
		
		InsertFileMapping( kDSStdRecordTypeServices, "/etc/services", CACHE_ENTRY_TYPE_SERVICE, parse_service, kDSNAttrRecordName, 
						   "dsAttrTypeNative:PortAndProtocol", kDS1AttrPort, NULL );
		
		InsertFileMapping( kDSStdRecordTypeAutomountMap, NULL, CACHE_ENTRY_TYPE_MOUNT, parse_automounts, kDSNAttrRecordName, 
						   NULL );
		InsertFileMapping( kDSStdRecordTypeAutomount, NULL, CACHE_ENTRY_TYPE_MOUNT, parse_automounts, kDSNAttrRecordName,
						   NULL );
		
		WatchFiles();
	}
	
	fFlatFileLock.SignalLock();
}

void FlatFileNode::InsertFileMapping( const char *inRecordType, const char *inFileName, int inCacheType, ParseCallback inCallback, ... )
{
	va_list				args;
	int					iCount	= 0;
	sFileMapping		*newMap = (sFileMapping *) calloc( 1, sizeof(sFileMapping) );
	
	// first count attributes
	va_start( args, inCallback );
	while (va_arg( args, char * ) != NULL)
		iCount++;

	CFMutableArrayRef	cfArray	= CFArrayCreateMutable( kCFAllocatorDefault, iCount, &kCFTypeArrayCallBacks );

	newMap->fRecordType = inRecordType;
	newMap->fRecordTypeCF = CFStringCreateWithCStringNoCopy( kCFAllocatorDefault, inRecordType, kCFStringEncodingUTF8, kCFAllocatorNull );
	newMap->fFileName = inFileName;
	newMap->fAttributes = (const char **) calloc( iCount+1, sizeof(const char *) );
	newMap->fAttributesCF = cfArray;
	newMap->fParseCallback = inCallback;
	newMap->fKeventFD = -1;
	newMap->fCacheType = inCacheType;

	iCount = 0;
	va_start( args, inCallback );
	char *attrib = va_arg( args, char * );
	
	do
	{
		newMap->fAttributes[iCount++] = attrib; // we don't dup these cause we don't delete them

		CFStringRef cfTempString = CFStringCreateWithCStringNoCopy( kCFAllocatorDefault, attrib, kCFStringEncodingUTF8, kCFAllocatorNull );
		CFArrayAppendValue( cfArray, cfTempString );
		CFRelease( cfTempString );
		
		attrib = va_arg( args, char * );
		
	} while( attrib != NULL );
	
	fFFRecordTypeTable[inRecordType] = newMap;
}

int FlatFileNode::sqlExecSync( const char *command )
{
	int				status;
	sqlite3_stmt	*pStmt;
	
	status = sqlite3_prepare( fSQLDatabase, command, -1, &pStmt, NULL );	
	if ( status == SQLITE_OK )
	{
		status = sqlite3_step( pStmt );
		status = sqlite3_finalize( pStmt );
	}
	
	return status;
}

int FlatFileNode::sqlExecSyncInsert( const char *command, int count, ... )
{
	va_list			args;
	sqlite3_stmt	*pStmt	= NULL;
	char			tempCmd[1024];
	
	// use vsnprintf to build the command first
	va_start( args, count );
	vsnprintf( tempCmd, sizeof(tempCmd), command, args );
	va_end( args );
	
	// now get to our offset of the bind values
	va_start( args, count );
	for ( int ii = 0; ii < count; ii++ )
		va_arg( args, const char * );
	
	fSQLDatabaseLock.WaitLock();
	
	int status = sqlite3_prepare( fSQLDatabase, tempCmd, -1, &pStmt, NULL );
	if ( status == SQLITE_OK )
	{
		int			lineNo		= va_arg( args, int );
		const char	*tempPtr	= va_arg( args, const char * );
		int			ii			= 2;
		
		status = sqlite3_bind_int( pStmt, 1, lineNo ); // first argument is always line no
		while ( status == SQLITE_OK && tempPtr != NULL )
		{
			status = sqlite3_bind_text( pStmt, ii, tempPtr, strlen(tempPtr), SQLITE_TRANSIENT );
			tempPtr = va_arg( args, const char * );
			ii++;
		}
		
		if ( status == SQLITE_OK )
			status = sqlite3_step( pStmt );
		
		status = sqlite3_finalize( pStmt );
	}

	fSQLDatabaseLock.SignalLock();

	va_end( args );
	
	return status;
}

#pragma mark -
#pragma mark Utility Functions

bool FlatFileNode::CheckPassword( CFStringRef cfUserPassword, CFStringRef cfCheckPassword )
{
	char		*pTemp1			= NULL;
	char		*pTemp2			= NULL;
	const char	*pRecordPass	= BaseDirectoryPlugin::GetCStringFromCFString( cfUserPassword, &pTemp1 );
	const char	*pPassword		= BaseDirectoryPlugin::GetCStringFromCFString( cfCheckPassword, &pTemp2 );
	bool		bReturn			= false;
	
	if ( DSIsStringEmpty(pPassword) == false )
	{
		char	hashPwd[64] = { 0, };
		
		// if this is an MD5 hash
		if ( strncmp(pRecordPass, "$1$", 3) == 0 )
		{
			fFlatFileLock.WaitLock();
			strlcpy( hashPwd, crypt_md5(pPassword, pRecordPass), sizeof(hashPwd) );
			fFlatFileLock.SignalLock();
		}
		else
		{
			char	salt[9];
			
			salt[0] = pRecordPass[0];
			salt[1] = pRecordPass[1];
			salt[2] = '\0';
			
			fFlatFileLock.WaitLock();
			strlcpy( hashPwd, crypt(pPassword, salt), sizeof(hashPwd) );
			fFlatFileLock.SignalLock();
		}
		
		bReturn = ( strcmp(hashPwd, pRecordPass) == 0 );
	}
	else if ( DSIsStringEmpty(pRecordPass) ) // if the user's password is empty in the record then succeed
	{
		bReturn = true;;
	}
	
	DSFreePassword( pTemp1 );
	DSFreePassword( pTemp2 )
	
	return bReturn;
}

CFMutableDictionaryRef FlatFileNode::CreateRecordDictionary( CFStringRef inRecordType, const char *inRecordName )
{
	CFStringRef				cfRecordName	= CFStringCreateWithCString( kCFAllocatorDefault, inRecordName, kCFStringEncodingUTF8 );
	if ( cfRecordName == NULL )
		return NULL;
	
	CFMutableDictionaryRef	newRecord		= CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
																		 &kCFTypeDictionaryValueCallBacks );
	CFMutableDictionaryRef	cfAttributes	= CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
																		 &kCFTypeDictionaryValueCallBacks );
	CFMutableArrayRef		cfRecordNames	= CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	
	CFArrayAppendValue( cfRecordNames, cfRecordName );
	
	CFDictionarySetValue( cfAttributes, CFSTR(kDSNAttrRecordName), cfRecordNames );

	CFDictionarySetValue( newRecord, kBDPINameKey, cfRecordName );
	CFDictionarySetValue( newRecord, kBDPITypeKey, inRecordType );
	CFDictionarySetValue( newRecord, kBDPIAttributeKey, cfAttributes );
	
	DSCFRelease( cfRecordName );
	DSCFRelease( cfRecordNames );
	DSCFRelease( cfAttributes );
	
	return newRecord;
}

void FlatFileNode::AddAttributeToRecord( CFMutableDictionaryRef inRecord, CFStringRef inAttribute, const char *inValue )
{
	CFMutableDictionaryRef	cfAttributes = (CFMutableDictionaryRef) CFDictionaryGetValue( inRecord, kBDPIAttributeKey );
	
	if ( cfAttributes == NULL )
	{
		cfAttributes = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		CFDictionarySetValue( inRecord, kBDPIAttributeKey, cfAttributes );
		CFRelease( cfAttributes );
	}
	
	CFMutableArrayRef	cfValues = (CFMutableArrayRef) CFDictionaryGetValue( cfAttributes, inAttribute );
	if ( cfValues == NULL )
	{
		cfValues = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		CFDictionarySetValue( cfAttributes, inAttribute, cfValues );
		CFRelease( cfValues );
	}
	
	CFStringRef	cfValue = CFSTR("");
	
	if ( inValue != NULL )
		cfValue = CFStringCreateWithCString( kCFAllocatorDefault, inValue, kCFStringEncodingUTF8 );
	
	if ( cfValue != NULL )
	{
		CFArrayAppendValue( cfValues, cfValue );
		DSCFRelease( cfValue );
	}
}

static int tokenize_line_extended( char ***outTokens, char *inTokenString, char *inData )
{
	int		iMax			= 10;
    int     iTokenMax       = iMax - 1;  // Save room at end for NULL
	char	**tokensTemp	= (char **) calloc( iMax, sizeof(char *) );
	char	*strtokContext	= NULL;
	int		iCount			= 0;
	
	char *tempToken = strtok_r( inData, inTokenString, &strtokContext );
	while (tempToken != NULL)
	{
		// trim whitespace from front
		char *begin = tempToken;
		while ((*begin) == ' ' || (*begin) == '\t' || (*begin) == '\0') 
			(*begin++) = '\0';
		
		// trim whitespace from the end
		char *end = &begin[strlen(begin)-1];
		while ((*end) == ' ' || (*end) == '\t' || (*end) == '\0') 
			(*end++) = '\0';
		
		tokensTemp[iCount++] = begin;
		tempToken = strtok_r( NULL, inTokenString, &strtokContext );
		
		if ( iCount >= iTokenMax )
		{
			iMax += 10;
            iTokenMax = iMax - 1;
			tokensTemp = (char **) reallocf( tokensTemp, iMax * sizeof(char *) );
			if ( tokensTemp == NULL )
			{
				iCount = 0;
				break;
			}
		}
	}
	
	tokensTemp[iCount] = NULL;
	
	(*outTokens) = tokensTemp;
	return iCount;
}

static int tokenize_line( char **inTokens, char *inTokenString, int iMax, char *inData, bool bSkipDuplicates = true )
{
	int         iCount			= 0;
    const int   iTokenMax       = iMax - 1;  // save room at end for NULL.

	if ( bSkipDuplicates )
	{
		char	*strtokContext	= NULL;
		
		char *tempToken = strtok_r( inData, inTokenString, &strtokContext );
		while (tempToken != NULL && iCount < iTokenMax)
		{
			// trim whitespace from front
			char *begin = tempToken;
			char *end	= &begin[strlen(begin)-1];
			
			while ( begin < end && ((*begin) == ' ' || (*begin) == '\t') ) 
			{
				(*begin) = '\0';
				begin++;
			}
			
			// trim whitespace from the end
			while ( end > begin && ((*end) == ' ' || (*end) == '\t') ) 
			{
				(*end) = '\0';
				end--;
			}
			
			inTokens[iCount++] = begin;
			tempToken = strtok_r( NULL, inTokenString, &strtokContext );
		}
	}
	else
	{
		char	*token;
		
        while ((token = strsep(&inData, inTokenString)) != NULL && iCount < iTokenMax)
		{
			inTokens[iCount] = token;
			iCount++;
		}
	}

	inTokens[iCount] = NULL;
	
	return iCount;
}

CFMutableDictionaryRef FlatFileNode::parse_user( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName )
{
	if ( inData == NULL )
		return NULL;
	
    const int   MAX_TOKENS          = 13;  // 12 plus terminating NULL.
	char        *tokens[MAX_TOKENS]	= { NULL };
	int         iCount              = tokenize_line( tokens, ":", MAX_TOKENS, inData, false );
	
	// if we don't have enough tokens or we have + sign for NIS
	if ( iCount < 4 || tokens[0][0] == '+' )
	{
		return NULL;
	}
	
	CFMutableDictionaryRef cfRecord = NULL;
	
	if ( inGenDictionary )
	{
		int	offset = 0;

		// if we have 7 parts, then this is the shadow format, offset to the parts we need is higher
		if ( iCount > 7 )
			offset = 3;

		cfRecord = CreateRecordDictionary( CFSTR(kDSStdRecordTypeUsers), tokens[0] );
		if ( cfRecord != NULL )
		{
			// we'll add the marker if the password field is a * or + or x as it signifies shadow anyway
			char cFirstChar = tokens[1][0];
			if ( (cFirstChar == '*' || cFirstChar == '+' || cFirstChar == 'x') && tokens[1][1] == '\0' )
				AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrPassword), kDSValueNonCryptPasswordMarker );		
			else
				AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrPassword), tokens[1] );		
			
			AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrUniqueID), tokens[2] );
			AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrPrimaryGroupID), tokens[3] );

			if ( offset == 3 )
			{
				AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrChange), tokens[5] );
				AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrExpire), tokens[6] );
			}
			
			char **gecos	= NULL;
			int gecosCnt = tokenize_line_extended( &gecos, ",", tokens[4+offset] );
			
			AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrDistinguishedName), (gecosCnt > 0 ? gecos[0] : NULL) );
			AddAttributeToRecord( cfRecord, CFSTR(kDSNAttrBuilding), (gecosCnt > 1 ? gecos[1] : NULL) );
			AddAttributeToRecord( cfRecord, CFSTR(kDSNAttrPhoneNumber), (gecosCnt > 2 ? gecos[2] : NULL) );
			AddAttributeToRecord( cfRecord, CFSTR(kDSNAttrHomePhoneNumber), (gecosCnt > 3 ? gecos[3] : NULL) );
			
			DSFree( gecos );
			
			AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrNFSHomeDirectory), tokens[5+offset] );
			AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrUserShell), tokens[6+offset] );
		}
	}
	
	// if we have a database, insert the entry into the table
	if ( inDatabase != NULL )
	{
		sqlExecSyncInsert( "INSERT INTO '" kDSStdRecordTypeUsers "' ('%s','%s','%s','%s','%s') VALUES (?,?,?,?,?);", 5,
						   kLineNoRowName, kDSNAttrRecordName, kDS1AttrUniqueID, kDS1AttrPrimaryGroupID, kDS1AttrDistinguishedName, 
						   inLineNumber, tokens[0], tokens[2], tokens[3], (iCount > 7 ? tokens[7] : ""), NULL );
	}
	
	return cfRecord;
}

CFMutableDictionaryRef FlatFileNode::parse_group( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName )
{
	if ( inData == NULL )
		return NULL;
	
    const int   MAX_TOKENS          = 7; // 6 plus terminating NULL
	char        *tokens[MAX_TOKENS]	= { NULL };
	char        **members           = NULL;
	int         mbrCnt              = 0;
	int         iCount              = tokenize_line( tokens, ":", MAX_TOKENS, inData, false );

	// if we don't have enough tokens or we have + sign for NIS
	if ( iCount < 3 || tokens[0][0] == '+' )
	{
		return NULL;
	}
	
	if ( iCount > 3 )
	{
		mbrCnt = tokenize_line_extended( &members, ",", tokens[3] );
	}

	CFMutableDictionaryRef cfRecord = NULL;
	
	if ( inGenDictionary )
	{
		cfRecord = CreateRecordDictionary( CFSTR(kDSStdRecordTypeGroups), tokens[0] );
		if ( cfRecord != NULL )
		{
			AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrPassword), tokens[1] );
			AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrPrimaryGroupID), tokens[2] );
			
			for (int ii = 0; ii < mbrCnt; ii++)
			{
				AddAttributeToRecord( cfRecord, CFSTR(kDSNAttrGroupMembership), members[ii] );
			}
		}
	}
		
	// if we have a database, insert the entry into the table
	if ( inDatabase != NULL )
	{
		int		ii = 0;
		
		do
		{
			sqlExecSyncInsert( "INSERT INTO '" kDSStdRecordTypeGroups "' ('%s','%s','%s','%s') VALUES (?,?,?,?);", 4,
							   kLineNoRowName, kDSNAttrRecordName, kDS1AttrPrimaryGroupID, kDSNAttrGroupMembership, 
							   inLineNumber, tokens[0], tokens[2], (ii < mbrCnt ? members[ii] : ""), NULL );
		} while( ++ii < mbrCnt );
	}

	DSFree( members );

	return cfRecord;
}

CFMutableDictionaryRef FlatFileNode::parse_host( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName )
{
	if ( inData == NULL )
		return NULL;
	
    const int   MAX_TOKEN_CMT               = 4;  // 3 plus terminating NULL
	char        *tokenCmt[MAX_TOKEN_CMT]    = { NULL };
	if ( tokenize_line(tokenCmt, "#", MAX_TOKEN_CMT, inData, true) == 0 )
		return NULL;

	char	**tokens	= NULL;
	int		iCount		= tokenize_line_extended( &tokens, " \t", tokenCmt[0] );
	
	// if we don't have enough tokens or we have + sign for NIS
	if ( iCount < 2 || tokens[0][0] == '+' )
	{
		DSFree( tokens );
		return NULL;
	}
		
	uint32_t    tempFamily  = AF_UNSPEC;
	char        buffer[16];		// IPv6 is 128 bit so max 16 bytes
	char		canonical[INET6_ADDRSTRLEN];

	if( inet_pton(AF_INET6, tokens[0], buffer) == 1 )
		tempFamily = AF_INET6;
	else if( inet_pton(AF_INET, tokens[0], buffer) == 1 )
		tempFamily = AF_INET;
	
	if ( tempFamily == AF_UNSPEC )
	{
		DSFree(tokens);
		return NULL;
	}

	/* We use inet_pton to convert to a canonical form */
	if ( inet_ntop(tempFamily, buffer, canonical, INET6_ADDRSTRLEN) == NULL )
	{
		DSFree(tokens);
		return NULL;
	}
	
	CFMutableDictionaryRef cfRecord = NULL;
	
	if ( inGenDictionary )
	{
		cfRecord = CreateRecordDictionary( CFSTR(kDSStdRecordTypeHosts), tokens[1] );
		if ( cfRecord != NULL )
		{
			AddAttributeToRecord( cfRecord, (tempFamily == AF_INET ? CFSTR(kDSNAttrIPAddress) : CFSTR(kDSNAttrIPv6Address)), canonical );
			
			for (int ii = 2; ii < iCount; ii++)
			{
				AddAttributeToRecord( cfRecord, CFSTR(kDSNAttrRecordName), tokens[ii] );
			}
			
			if ( tokenCmt[1] != NULL )
				AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrComment), tokenCmt[1] );
		}
	}
	
	// if we have a database, insert the entry into the table
	if ( inDatabase != NULL )
	{
		for (int ii = 1; ii < iCount; ii++)
		{
			sqlExecSyncInsert( "INSERT INTO '" kDSStdRecordTypeHosts "' ('%s','%s','%s') VALUES (?,?,?);", 3,
							   kLineNoRowName, kDSNAttrRecordName, (tempFamily == AF_INET ? kDSNAttrIPAddress : kDSNAttrIPv6Address), 
							   inLineNumber, tokens[ii], canonical, NULL );
		}
	}
	
	DSFree( tokens );

	return cfRecord;
}

CFMutableDictionaryRef FlatFileNode::parse_network( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName )
{
	if ( inData == NULL )
		return NULL;
	
    const int   MAX_TOKEN_CMT               = 4;  // 3 plus terminating NULL
	char        *tokenCmt[MAX_TOKEN_CMT]    = { NULL };
	if ( tokenize_line(tokenCmt, "#", MAX_TOKEN_CMT, inData, true) == 0 )
		return NULL;

	char	**tokens	= NULL;
	int		iCount		= tokenize_line_extended( &tokens, " \t", tokenCmt[0] );
	
	// if we don't have enough tokens or we have + sign for NIS
	if ( iCount < 2 || tokens[0][0] == '+' )
	{
		DSFree( tokens );
		return NULL;
	}
	
	CFMutableDictionaryRef cfRecord = NULL;
	
	if ( inGenDictionary )
	{
		cfRecord = CreateRecordDictionary( CFSTR(kDSStdRecordTypeNetworks), tokens[0] );
		if ( cfRecord != NULL )
		{
			AddAttributeToRecord( cfRecord, CFSTR("dsAttrTypeNative:address"), tokens[1] );
			
			for (int ii = 2; ii < iCount; ii++)
			{
				AddAttributeToRecord( cfRecord, CFSTR(kDSNAttrRecordName), tokens[ii] );
			}

			if ( tokenCmt[1] != NULL )
				AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrComment), tokenCmt[1] );
		}
	}
	
	// if we have a database, insert the entry into the table
	if ( inDatabase != NULL )
	{
		for (int ii = 1; ii < iCount; ii++)
		{
			sqlExecSyncInsert( "INSERT INTO '" kDSStdRecordTypeNetworks "' ('%s','%s','%s') VALUES (?,?,?);", 3,
							   kLineNoRowName, kDSNAttrRecordName, "dsAttrTypeNative:address", 
							   inLineNumber, (ii == 1 ? tokens[0] : tokens[ii]), tokens[1], NULL );
		}
	}
	
	DSFree( tokens );
	
	return cfRecord;
}

CFMutableDictionaryRef FlatFileNode::parse_service( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName )
{
	if ( inData == NULL )
		return NULL;
	
	// first split comments
    const int   MAX_TOKEN_CMT               = 4;  // 3 plus terminating NULL
	char        *tokenCmt[MAX_TOKEN_CMT]    = { NULL };
	if ( tokenize_line(tokenCmt, "#", MAX_TOKEN_CMT, inData, true) == 0 )
		return NULL;
	
	char	**tokens	= NULL;
	int		iCount		= tokenize_line_extended( &tokens, " \t", tokenCmt[0] );
	
	// if we don't have enough tokens or we have + sign for NIS
	if ( iCount < 2 || tokens[0][0] == '+' )
	{
		DSFree( tokens );
		return NULL;
	}
	
	char					*portAndProt	= strdup( tokens[1] );
	CFMutableDictionaryRef	cfRecord		= NULL;
	
	// now split port and protocol into separate ones too
	char *slash = strchr( tokens[1], '/' );
	if ( slash == NULL )
		goto failure;

	(*slash) = '\0';
	slash++;

	if ( inGenDictionary )
	{
		cfRecord = CreateRecordDictionary( CFSTR(kDSStdRecordTypeServices), tokens[0] );
		if ( cfRecord != NULL )
		{
			AddAttributeToRecord( cfRecord, CFSTR("dsAttrTypeNative:PortAndProtocol"), portAndProt );
			AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrPort), tokens[1] );
			AddAttributeToRecord( cfRecord, CFSTR(kDSNAttrProtocols), slash );
			
			for (int ii = 2; ii < iCount; ii++)
			{
				AddAttributeToRecord( cfRecord, CFSTR(kDSNAttrRecordName), tokens[ii] );
			}
			
			if ( tokenCmt[1] != NULL )
				AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrComment), tokenCmt[1] );
		}
	}
	
	// if we have a database, insert the entry into the table
	if ( inDatabase != NULL )
	{
		sqlExecSyncInsert( "INSERT INTO '" kDSStdRecordTypeServices "' ('%s','%s','%s','%s') VALUES (?,?,?,?);", 4,
						   kLineNoRowName, kDSNAttrRecordName, "dsAttrTypeNative:PortAndProtocol", kDS1AttrPort,
						   inLineNumber, tokens[0], portAndProt, tokens[1], NULL );
	}
	
failure:
	
	DSFree( tokens );
	DSFreeString( portAndProt );
	
	return cfRecord;
}

CFMutableDictionaryRef FlatFileNode::parse_protocol( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName )
{
	if ( inData == NULL )
		return NULL;
	
	// first split comments
    const int   MAX_TOKEN_CMT               = 4;  // 3 plus terminating NULL
	char        *tokenCmt[MAX_TOKEN_CMT]    = { NULL };
	if ( tokenize_line(tokenCmt, "#", MAX_TOKEN_CMT, inData, true) == 0 )
		return NULL;
	
	char	**tokens	= NULL;
	int		iCount		= tokenize_line_extended( &tokens, " \t", tokenCmt[0] );
	
	// if we don't have enough tokens or we have + sign for NIS
	if ( iCount < 2 || tokens[0][0] == '+' )
	{
		DSFree( tokens );
		return NULL;
	}
	
	CFMutableDictionaryRef cfRecord = NULL;
	
	if ( inGenDictionary )
	{
		cfRecord = CreateRecordDictionary( CFSTR(kDSStdRecordTypeProtocols), tokens[0] );
		if ( cfRecord != NULL )
		{
			AddAttributeToRecord( cfRecord, CFSTR("dsAttrTypeNative:number"), tokens[1] );
			
			if ( tokenCmt[1] != NULL )
				AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrComment), tokenCmt[1] );

			for (int ii = 2; ii < iCount; ii++)
			{
				AddAttributeToRecord( cfRecord, CFSTR(kDSNAttrRecordName), tokens[ii] );
			}
		}
	}
	
	// if we have a database, insert the entry into the table
	if ( inDatabase != NULL )
	{
		for (int ii = 1; ii < iCount; ii++)
		{
			sqlExecSyncInsert( "INSERT INTO '" kDSStdRecordTypeProtocols "' ('%s','%s','%s') VALUES (?,?,?);", 3,
							   kLineNoRowName, kDSNAttrRecordName, "dsAttrTypeNative:number", 
							   inLineNumber, (ii == 1 ? tokens[0] : tokens[ii]), tokens[1], NULL );
		}
	}
	
	DSFree( tokens );
	
	return cfRecord;
}

CFMutableDictionaryRef FlatFileNode::parse_rpc( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName )
{
	if ( inData == NULL )
		return NULL;
	
    const int   MAX_TOKEN_CMT               = 4;  // 3 plus terminating NULL
	char        *tokenCmt[MAX_TOKEN_CMT]    = { NULL };
	if ( tokenize_line(tokenCmt, "#", MAX_TOKEN_CMT, inData, true) == 0 )
		return NULL;

	char	**tokens	= NULL;
	int		iCount		= tokenize_line_extended( &tokens, " \t", tokenCmt[0] );
	
	// if we don't have enough tokens or we have + sign for NIS
	if ( iCount < 2 || tokens[0][0] == '+' )
	{
		DSFree( tokens );
		return NULL;
	}
	
	CFMutableDictionaryRef cfRecord = NULL;
	
	if ( inGenDictionary )
	{
		cfRecord = CreateRecordDictionary( CFSTR(kDSStdRecordTypeRPC), tokens[0] );
		if ( cfRecord != NULL )
		{
			AddAttributeToRecord( cfRecord, CFSTR("dsAttrTypeNative:number"), tokens[1] );
			
			for (int ii = 2; ii < iCount; ii++)
			{
				AddAttributeToRecord( cfRecord, CFSTR(kDSNAttrRecordName), tokens[ii] );
			}
			
			if ( tokenCmt[1] != NULL )
				AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrComment), tokenCmt[1] );
		}
	}
	
	// if we have a database, insert the entry into the table
	if ( inDatabase != NULL )
	{
		for (int ii = 1; ii < iCount; ii++)
		{
			sqlExecSyncInsert( "INSERT INTO '" kDSStdRecordTypeRPC "' ('%s','%s','%s') VALUES (?,?,?);", 3,
							   kLineNoRowName, kDSNAttrRecordName, "dsAttrTypeNative:number", 
							   inLineNumber, (ii == 1 ? tokens[0] : tokens[ii]), tokens[1], NULL );
		}
	}
	
	DSFree( tokens );
	
	return cfRecord;
}

CFMutableDictionaryRef FlatFileNode::parse_mount( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName )
{
	if ( inData == NULL )
		return NULL;
	
    const int   MAX_TOKENS          = 9;  // 8 plus terminating NULL
	char        *tokens[MAX_TOKENS] = { NULL };
	int         iCount              = tokenize_line( tokens, " \t", MAX_TOKENS, inData );
	
	// if we don't have enough tokens or we have + sign for NIS
	if ( iCount < 6 || tokens[0][0] == '+' )
	{
		return NULL;
	}
	
	CFMutableDictionaryRef cfRecord = NULL;
	
	if ( inGenDictionary )
	{
		cfRecord = CreateRecordDictionary( CFSTR(kDSStdRecordTypeMounts), tokens[0] );
		if ( cfRecord != NULL )
		{
			AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrVFSLinkDir), tokens[1] );
			AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrVFSType), tokens[2] );
		
			char *pVFSOpts = strdup( tokens[3] );
			
			char **vfsopts	= NULL;
			int vfsoptCnt = tokenize_line_extended( &vfsopts, ",", tokens[3] );
			for (int ii = 0; ii < vfsoptCnt; ii++)
			{
				AddAttributeToRecord( cfRecord, CFSTR(kDSNAttrVFSOpts), vfsopts[ii] );
			}
			
			DSFree( vfsopts );
			
			AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrVFSDumpFreq), tokens[4] );
			AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrVFSPassNo), tokens[5] );

			DSFreeString( pVFSOpts );
		}
	}
	
	// if we have a database, insert the entry into the table
	if ( inDatabase != NULL )
	{
		sqlExecSyncInsert( "INSERT INTO '" kDSStdRecordTypeMounts "' ('%s','%s') VALUES (?,?);", 2,
						   kLineNoRowName, kDSNAttrRecordName,
						   inLineNumber, tokens[0], NULL );
	}
	
	return cfRecord;
}

CFMutableDictionaryRef FlatFileNode::parse_printer( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName )
{
//	CFMutableDictionaryRef itemRef = FlatFileNode::parse_pb(inData, ':');
//	
//	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSStdRecordTypePrintService) );
//	
//	return itemRef;
	return NULL;
}

CFMutableDictionaryRef FlatFileNode::parse_bootp( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName )
{
//	CFMutableDictionaryRef itemRef = FlatFileNode::parse_pb(inData, ':');
//	
//	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSStdRecordTypeBootp) );
///*	char **tokens;
//
//	if ( inData == NULL ) return NULL;
//
////	tokens = tokens_from_line(inData, " \t", 0);
//	if ( listLength(tokens) < 5 )
//	{
//		DSFree(tokens);
//		return NULL;
//	}
//
//	itemRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
//
//	CFDictionarySetValue( itemRef, CFSTR(kDSNAttrRecordType), CFSTR(kDSStdRecordTypeBootp) );
//	
//	_set_value_for_key(itemRef, tokens[0], kDSNAttrRecordName);
//	_set_value_for_key(itemRef, tokens[1], "dsAttrTypeNative:htype");
//	_set_value_for_key(itemRef, tokens[2], kDS1AttrENetAddress);
//	_set_value_for_key(itemRef, tokens[3], kDSNAttrIPAddress);
//	_set_value_for_key(itemRef, tokens[4], "dsAttrTypeNative:bootfile");
//
//	DSFree(tokens);
//	tokens = NULL;
//*/
//	return itemRef;
	return NULL;
}

CFMutableDictionaryRef FlatFileNode::parse_alias( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName )
{
	if ( inData == NULL )
		return NULL;
	
    const int   MAX_TOKENS               = 4;  // 3 plus terminating NULL
	char        *tokenCmt[MAX_TOKENS]    = { NULL };
	if ( tokenize_line(tokenCmt, "#", MAX_TOKENS, inData, true) == 0 )
		return NULL;

	char	*tokens[MAX_TOKENS]	= { NULL };
	int		iCount              = tokenize_line( tokens, ":", MAX_TOKENS, tokenCmt[0] );
	
	// if we don't have enough tokens or we have + sign for NIS
	if ( iCount < 2 || tokens[0][0] == '+' )
	{
		return NULL;
	}
	
	CFMutableDictionaryRef cfRecord = NULL;
	
	if ( inGenDictionary )
	{
		cfRecord = CreateRecordDictionary( CFSTR(kDSStdRecordTypeAliases), tokens[0] );
		if ( cfRecord != NULL )
		{
			char **aliases	= NULL;
			int aliasCount = tokenize_line_extended( &aliases, ", \t", tokens[1] );
			for (int ii = 0; ii < aliasCount; ii++)
			{
				AddAttributeToRecord( cfRecord, CFSTR("dsAttrTypeNative:members"), aliases[ii] );
			}
			
			if ( tokenCmt[1] != NULL )
				AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrComment), tokenCmt[1] );

			DSFree( aliases );
		}
	}
	
	// if we have a database, insert the entry into the table
	if ( inDatabase != NULL )
	{
		sqlExecSyncInsert( "INSERT INTO '" kDSStdRecordTypeAliases "' ('%s','%s') VALUES (?,?);", 2,
						   kLineNoRowName, kDSNAttrRecordName,
						   inLineNumber, tokens[0], NULL );
	}
	
	return cfRecord;
}

CFMutableDictionaryRef FlatFileNode::parse_ethernet( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName )
{
	if ( inData == NULL )
		return NULL;
	
    const int   MAX_TOKENS          = 4;  // 3 plus terminating NULL
	char        *tokens[MAX_TOKENS] = { NULL };
	int         iCount              = tokenize_line( tokens, " \t", MAX_TOKENS, inData );
	
	// if we don't have enough tokens or we have + sign for NIS
	if ( iCount < 2 || tokens[0][0] == '+' )
	{
		return NULL;
	}
	
	CFMutableDictionaryRef cfRecord = NULL;
	
	if ( inGenDictionary )
	{
		cfRecord = CreateRecordDictionary( CFSTR(kDSStdRecordTypeEthernets), tokens[1] );
		if ( cfRecord != NULL )
		{
			AddAttributeToRecord( cfRecord, CFSTR(kDS1AttrENetAddress), tokens[0] );
		}
	}
	
	// if we have a database, insert the entry into the table
	if ( inDatabase != NULL )
	{
		sqlExecSyncInsert( "INSERT INTO '" kDSNAttrRecordAlias "' ('%s','%s','%s') VALUES (?,?,?);", 3,
						   kLineNoRowName, kDSNAttrRecordName, kDS1AttrENetAddress, 
						   inLineNumber, tokens[1], tokens[0], NULL );
	}
	
	return cfRecord;
}

CFMutableDictionaryRef FlatFileNode::parse_netgroup( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName )
{
	if ( inData == NULL )
		return NULL;
	
	char	**tokens	= NULL;
	int		iCount		= tokenize_line_extended( &tokens, " \t()", inData);
	
	// if we don't have enough tokens or we have + sign for NIS
	if ( iCount < 2 || tokens[0][0] == '+' )
	{
		return NULL;
	}
	
	CFMutableDictionaryRef cfRecord = NULL;
	
	if ( inGenDictionary )
	{
		if ( inName != NULL )
			cfRecord = CreateRecordDictionary( CFSTR(kDSStdRecordTypeNetGroups), inName );
		else
			cfRecord = CreateRecordDictionary( CFSTR(kDSStdRecordTypeNetGroups), tokens[0] );

		if ( cfRecord != NULL )
		{
			for (int ii = (inName != NULL ? 0 : 1); ii < iCount; ii++)
			{
				AddAttributeToRecord( cfRecord, CFSTR("dsAttrTypeNative:triplet"), tokens[ii] );
			}
		}
	}
	
	// if we have a database, insert the entry into the table
	if ( inDatabase != NULL )
	{
		sqlExecSyncInsert( "INSERT INTO '" kDSStdRecordTypeNetGroups "' ('%s','%s') VALUES (?,?);", 2,
						   kLineNoRowName, kDSNAttrRecordName, 
						   inLineNumber, tokens[0], NULL );
	}
	
	return cfRecord;
}

CFMutableDictionaryRef FlatFileNode::parse_automounts( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName ) 
{ 
	if ( inData == NULL )
		return NULL;
	
    const int   MAX_TOKEN_CMT               = 4;  // 3 plus terminating NULL
	char        *tokenCmt[MAX_TOKEN_CMT]    = { NULL };
	if ( tokenize_line(tokenCmt, "#", MAX_TOKEN_CMT, inData, true) == 0 )
		return NULL;

	// we tokenize special here since there are only 2 values
	char	*context	= NULL;
	char	*mapName = strtok_r( tokenCmt[0], "# \t\n\r", &context );
	char	*mapInfo = strtok_r( NULL, "#\n\r", &context );

	if ( mapName == NULL || mapInfo == NULL )
	{
		return NULL;
	}
	
	CFMutableDictionaryRef cfRecord = NULL;

	if ( inGenDictionary )
	{
		cfRecord = CreateRecordDictionary( CFSTR(kDSStdRecordTypeAutomount), mapName );
		if ( cfRecord != NULL )
		{
			AddAttributeToRecord( cfRecord, CFSTR(kDSNAttrAutomountInformation), mapInfo );
			
			if ( tokenCmt[1] != NULL )
				AddAttributeToRecord( cfRecord, CFSTR(kDSNAttrAutomountInformation), tokenCmt[1] );
		}
	}
	
	return cfRecord; 
}
