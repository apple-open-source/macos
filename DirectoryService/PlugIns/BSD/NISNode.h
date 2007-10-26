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

#ifndef _NISNODE_H
#define _NISNODE_H

#include "FlatFileNode.h"
#include <SystemConfiguration/SystemConfiguration.h>
#include <DirectoryServiceCore/DSMutexSemaphore.h>

struct sNISReachabilityList;

typedef struct sNISRecordMapping;

typedef map<string, sNISRecordMapping *>	NISRecordConfigDataMap;
typedef NISRecordConfigDataMap::iterator	NISRecordConfigDataMapI;

class NISNode : public FlatFileNode
{
	public:
										NISNode( CFStringRef inNode, const char *inNISDomain, uid_t inUID, uid_t inEffectiveUID );
		virtual							~NISNode( void );
		
		virtual CFMutableDictionaryRef	CopyNodeInfo( CFArrayRef cfAttributes );
		
		virtual tDirStatus				VerifyCredentials( CFStringRef inRecordType, CFStringRef inRecordName, CFStringRef inPassword );
		
		virtual tDirStatus				SearchRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount);
		
		// public statics
		static	char					*CopyDomainName( void );
		static	char					*CopyNISServers( char *inNISDomain );
	
		static	void					SetDomainAndServers( const char *inNISDomain, const char *inNISServers );
	
		static	bool					IsNISAvailable( void );
		static	void					InitializeGlobals( void );
	
	private:
		static char						*fNISDomainConfigured;
		static DSMutexSemaphore			fStaticMutex;
		static bool						fNISAvailable;
		static sNISReachabilityList		*fServerReachability;
		static NISRecordConfigDataMap	fNISRecordMapTable;
		static CFAbsoluteTime			fLastYPBindLaunchAttempt;

		char							*fNISDomain;
	
	private:
		static	void					SetNISServers( const char* inNISDomain, const char *inNISServers );
		static	void					NISReachabilityCallback( SCNetworkReachabilityRef target, SCNetworkConnectionFlags flags, void *info );
		static	sNISReachabilityList	*CreateReachability( char *inName );
		static	void					BuildYPMapTable( void );
		static	void					FreeSearchState( void *inState );
		static	int						ForEachEntry( unsigned long inStatus, char *inKey, int inKeyLen, char *inVal, int inValLen, void *inData );
		static	bool					RecordMatchesCriteria( sBDPISearchRecordsContext *inContext, CFDictionaryRef inRecord );

				bool					DomainNameChanged( void );
				SInt32					InternalSearchRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount );
				SInt32					FetchAllRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount );
				SInt32					FetchMatchingRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount );
				SInt32					FetchAutomountRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount );
				SInt32					MapNISError( int error );
};

#endif
