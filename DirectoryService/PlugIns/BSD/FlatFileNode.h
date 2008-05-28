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

#ifndef _FLATFILENODE_H
#define _FLATFILENODE_H

#include <sqlite3.h>
#include <map>
#include <string>		//STL string class
#include <DirectoryServiceCore/DSMutexSemaphore.h>
#include "BDPIVirtualNode.h"

using namespace std;

typedef CFMutableDictionaryRef				(*ParseCallback)( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName );

struct sFileMapping
{
	const char		*fRecordType;	// record type we care about
	CFStringRef		fRecordTypeCF;
	const char		*fFileName;		// actual filename on disk
	ParseCallback	fParseCallback;
	char const		**fAttributes;	// NULL terminated
	CFArrayRef		fAttributesCF;
	struct timespec	fTimeChecked;
	int				fKeventFD;
	int				fCacheType;
};

typedef map<string, sFileMapping *>			FlatFileConfigDataMap;
typedef FlatFileConfigDataMap::iterator		FlatFileConfigDataMapI;

class FlatFileNode : public BDPIVirtualNode
{
	public:
										FlatFileNode( CFStringRef inNodeName, uid_t inUID, uid_t inEffectiveUID );
		virtual							~FlatFileNode( void );
		
		virtual CFMutableDictionaryRef	CopyNodeInfo( CFArrayRef cfAttributes );
		
		virtual tDirStatus				VerifyCredentials( CFStringRef inRecordType, CFStringRef inRecordName, CFStringRef inPassword );
		
		virtual tDirStatus				SearchRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount);
		
	protected:
		static FlatFileConfigDataMap	fFFRecordTypeTable;
		static DSMutexSemaphore			fFlatFileLock;

	protected:
		static CFMutableDictionaryRef	parse_user( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName );
		static CFMutableDictionaryRef	parse_group( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName );
		static CFMutableDictionaryRef	parse_host( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName );
		static CFMutableDictionaryRef	parse_network( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName );
		static CFMutableDictionaryRef	parse_service( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName );
		static CFMutableDictionaryRef	parse_protocol( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName );
		static CFMutableDictionaryRef	parse_rpc( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName );
		static CFMutableDictionaryRef	parse_mount( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName );
		static CFMutableDictionaryRef	parse_printer( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName );
		static CFMutableDictionaryRef	parse_bootp( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName );
		static CFMutableDictionaryRef	parse_alias( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName );
		static CFMutableDictionaryRef	parse_ethernet( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName );
		static CFMutableDictionaryRef	parse_netgroup( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName );
		static CFMutableDictionaryRef	parse_automounts( sqlite3 *inDatabase, bool inGenDictionary, int inLineNumber, char *inData, const char *inName );

		static CFMutableDictionaryRef	CreateRecordDictionary( CFStringRef inRecordType, const char *inRecordName );
		static void						AddAttributeToRecord( CFMutableDictionaryRef inRecord, CFStringRef inAttribute, const char *inValue );
		static void						BuildTableMap( void );

		bool							CheckPassword( CFStringRef cfUserPassword, CFStringRef cfCheckPassword );
	
	private:
		static int						fKqueue;
		static sqlite3					*fSQLDatabase;
		static pthread_mutex_t			fWatchFilesLock;
		static DSMutexSemaphore			fSQLDatabaseLock;
		static bool						fProperShutdown;
		static bool						fSafeBoot;
	
	private:
		static int						sqlExecSync( const char *command );
		static int						sqlExecSyncInsert( const char *command, int count, ... );
		static void						FreeSearchState( void *inState );
		static void						FileChangeNotification( CFFileDescriptorRef cfFD, CFOptionFlags callBackTypes, void *info );

		void							OpenOrCreateDatabase( void );
		void							CreateTableForRecordMap( sFileMapping *inFileType );
		void							CheckTimeStamp( sFileMapping *inFileType );
		void							GetDatabaseTimestamps( void );
		static void						InsertFileMapping( const char *inRecordType, const char *inFileName, int inCacheType, 
														   ParseCallback inCallback, ... );
		SInt32							FetchAllRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount );
		SInt32							FetchMatchingRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount );
		SInt32							InternalSearchRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount );
		static void						WatchFiles( void );
};

#endif
