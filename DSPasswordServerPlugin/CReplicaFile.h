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
 *  CReplicaFile.h
 *  PasswordServer
 *
 */

#ifndef __CREPLICAFILE__
#define __CREPLICAFILE__

#include <Carbon/Carbon.h>
#include "AuthFile.h"

#define kPWReplicaFile						"/var/db/authserver/authserverreplicas"
#define kPWReplicaDir						"/var/db/authserver"

#define kPWReplicaParentKey					"Parent"
#define kPWReplicaReplicaKey				"Replicas"
#define kPWReplicaIPKey						"IP"
#define kPWReplicaDNSKey					"DNS"
#define kPWReplicaNameKey					"ReplicaName"
#define kPWReplicaPolicyKey					"ReplicaPolicy"
#define kPWReplicaStatusKey					"ReplicaStatus"
#define kPWReplicaNameValuePrefix			"Replica"
#define kPWReplicaSyncDateKey				"LastSyncDate"
#define kPWReplicaSyncServerKey				"LastSyncServer"
#define kPWReplicaStatusAllow				"AllowReplication"
#define kPWReplicaStatusUseACL				"UseACL"
#define kPWReplicaIDRangeBeginKey			"IDRangeBegin"
#define kPWReplicaIDRangeEndKey				"IDRangeEnd"

// values
#define kPWReplicaPolicyNeverKey			"SyncNever"
#define kPWReplicaPolicyOnlyIfDesperateKey	"SyncOnlyIfDesperate"
#define kPWReplicaPolicyOnScheduleKey		"SyncOnSchedule"
#define kPWReplicaPolicyOnDirtyKey			"SyncOnDirty"
#define kPWReplicaPolicyAnytimeKey			"SyncAnytime"

#define kPWReplicaStatusActiveValue			"Active"
#define kPWReplicaStatusPermDenyValue		"PermissionDenied"
#define kPWReplicaStatusNotFoundValue		"NotFound"


typedef UInt8 ReplicaPolicy;
enum {
	kReplicaNone,
	kReplicaAllowAll,
	kReplicaUseACL
};

typedef UInt8 ReplicaSyncPolicy;
enum {
	kReplicaSyncNever,
	kReplicaSyncOnlyIfDesperate,
	kReplicaSyncOnSchedule,
	kReplicaSyncOnDirty,
	kReplicaSyncAnytime
};

typedef UInt8 ReplicaStatus;
enum {
	kReplicaActive,
	kReplicaPermissionDenied,
	kReplicaNotFound
};

class CReplicaFile
{
	public:
	
												CReplicaFile();
												CReplicaFile( const char *xmlDataStr );
												CReplicaFile( bool inLoadCustomFile, const char *inFilePath );
		virtual									~CReplicaFile();
		
		static bool								MergeReplicaLists( CReplicaFile *inList1, CReplicaFile *inOutList2 );
		
		// top level
		virtual ReplicaPolicy					GetReplicaPolicy( void );
		virtual void							SetReplicaPolicy( ReplicaPolicy inPolicy );
		virtual bool							IPAddressIsInACL( UInt32 inIPAddress );
		virtual UInt32							ReplicaCount( void );
		virtual CFDictionaryRef					GetReplica( UInt32 index );
		virtual bool							IsActive( void );
		virtual bool							GetUniqueID( char *outIDStr );
		virtual CFDictionaryRef					GetParent( void );
		virtual char *							GetXMLData( void );
		virtual bool							FileHasChanged( void );
		virtual void							RefreshIfNeeded( void );
		virtual void							CalcServerUniqueID( const char *inRSAPublicKey, char *outHexHash );
		virtual void							AddServerUniqueID( const char *inRSAPublicKey );
		virtual void							SetParent( const char *inIPStr, const char *inDNSStr );
		virtual void							SetParent( CFDictionaryRef inParentData );
		virtual CFMutableDictionaryRef			AddReplica( const char *inIPStr, const char *inDNSStr );
		virtual CFMutableDictionaryRef			AddReplica( CFMutableDictionaryRef inReplicaData );
		virtual bool							AddIPAddress( CFMutableDictionaryRef inReplicaData, const char *inIPStr );
		virtual void							ReplaceOrAddIPAddress( CFMutableDictionaryRef inReplicaData, const char *inOldIPStr, const char *inNewIPStr );
		virtual CFMutableArrayRef				GetIPAddresses( CFMutableDictionaryRef inReplicaData );
		
		virtual int								SaveXMLData( void );
		virtual void							SortReplicas( void );
		virtual void							StripSyncDates( void );
		
		// per replica
		virtual void							AllocateIDRange( const char *inReplicaName, UInt32 inCount );
		virtual void							GetIDRangeForReplica( const char *inReplicaName, UInt32 *outStart, UInt32 *outEnd );
		virtual void							SetSyncDate( const char *inReplicaName, CFDateRef inSyncDate );
		virtual CFMutableDictionaryRef			GetReplicaByName( const char *inReplicaName );
		virtual void							GetNameOfReplica( CFMutableDictionaryRef inReplicaDict, char *outReplicaName );
		virtual bool							GetNameFromIPAddress( const char *inIPAddress, char *outReplicaName );
		virtual UInt8							GetReplicaSyncPolicy( CFDictionaryRef inReplicaDict );
		virtual void							SetReplicaSyncPolicy( const char *inReplicaName, UInt8 inPolicy );
		virtual bool							SetReplicaSyncPolicy( CFMutableDictionaryRef inRepDict, CFStringRef inPolicyString );
		virtual ReplicaStatus					GetReplicaStatus( CFDictionaryRef inReplicaDict );
		virtual void							SetReplicaStatus( CFMutableDictionaryRef repDict, ReplicaStatus inStatus );
		
		// utilities
		virtual void							GetSelfName( char *outName );
		virtual void							SetSelfName( const char *inSelfName );
		virtual bool							GetCStringFromDictionary( CFDictionaryRef inDict, CFStringRef inKey, long inMaxLen, char *outString );
		virtual bool							Dirty( void ) { return mDirty; };
		virtual void							SetDirty( bool inDirty ) { mDirty = inDirty; };
		
		static int								SaveXMLData( CFPropertyListRef inListToWrite, const char *inSaveFile );

	protected:
		
		virtual void							AddOrReplaceValue( CFStringRef inKey, CFStringRef inValue );
		virtual void							AddOrReplaceValue( CFMutableDictionaryRef inDict, CFStringRef inKey, CFTypeRef inValue );
		static void								AddOrReplaceValueStatic( CFMutableDictionaryRef inDict, CFStringRef inKey, CFTypeRef inValue );
		
		virtual void							GetNextReplicaName( char *outName );
		virtual int								StatReplicaFileAndGetModDate( const char *inFilePath, struct timespec *outModDate );
		virtual int								LoadXMLData( const char *inFilePath );
		virtual CFMutableArrayRef				GetArrayForKey( CFStringRef key );
		virtual void							GetIDRange( const char *inMyLastID, UInt32 inCount, char *outFirstID, char *outLastID );
		virtual int								SetIDRange( CFMutableDictionaryRef inServerDict, const char *inFirstID, const char *inLastID );
		virtual CFMutableDictionaryRef			FindMatchToKey( const char *inKey, const char *inValue );
		virtual void							passwordRecRefToString(PWFileEntry *inPasswordRec, char *outRefStr);
		virtual int								stringToPasswordRecRef(const char *inRefStr, PWFileEntry *outPasswordRec);
		
		CFMutableDictionaryRef					mReplicaDict;
		CFArrayRef 								mReplicaArray;
		bool									mDirty;
		struct timespec							mReplicaFileModDate;
		char									mSelfName[20];
};

bool ConvertBinaryToHex( const unsigned char *inData, long len, char *outHexStr );

#endif
