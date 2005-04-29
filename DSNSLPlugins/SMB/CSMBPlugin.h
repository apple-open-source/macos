/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 *  @header CSMBPlugin
 */

#ifndef _CSMBPlugin_

#include "LMBDiscoverer.h"
#include "CNSLPlugin.h"

//#define	__APPLE_NMBLOOKUP_HACK_2987131	// this will ask nmblookup to give us raw bytes

#define kProtocolPrefixPlainStr		"SMB"
#define kProtocolPrefixStr			"/SMB"
#define kProtocolPrefixSlashStr		"/SMB/"
#define kScopePrefixStr				"Network Neighborhood(DS)"
#define kScopePrefixSlashStr		"Network Neighborhood(DS)/"
#define kReadSMBConfigData			'read'
#define kWriteSMBConfigData			'writ'
#define kReadSMBConfigXMLDataSize	'xmls'
#define kReadSMBConfigXMLData		'rxml'
#define kWriteSMBConfigXMLData		'wxml'

#define kNMBLookupToolPath			"/usr/bin/nmblookup"
#define kServiceTypeString			"smb"
#define kTemplateConfFilePath		"/etc/smb.conf.template"
#define kTempConfFilePath			"/var/run/smb.conf.temp"
#define kConfFilePath				"/etc/smb.conf"
#define kBrowsingConfFilePath		"/var/run/smbbrowsing.conf"

#define kUseWINSURLFilePath			"/etc/smb_use_wins_url.conf"

class CSMBPlugin : public CNSLPlugin
{
public:
                                CSMBPlugin							( void );
	virtual                     ~CSMBPlugin							( void );
    
    virtual sInt32				InitPlugin							( void );
			Boolean				AreWeRunningOnXServer				( void );
			
	virtual	void				ActivateSelf						( void );
	virtual	void				DeActivateSelf						( void );

	virtual Boolean				PluginSupportsServiceType			( const char* serviceType );

			void				RegisterForComputerNameChanges		( void );
			void				DeregisterForComputerNameChanges	( void );

			void				HandleComputerNameChange			( void );
			
            uInt32				fSignature;
			const char*			GetWinsServer						( void ) { return mWINSServer; }
			void				NodeLookupIsCurrent					( void );
			
			LMBDiscoverer*		OurLMBDiscoverer					( void ) { return mLMBDiscoverer; }
			void				ClearLMBForWorkgroup				( CFStringRef workgroupRef, CFStringRef lmbNameRef );

			CFStringRef			GetComputerNameChangeKey			( void ) { return mComputerNameChangeKey; }
		
			void				ResetBroadcastThrottle						( void );
			Boolean				OKToDoServiceLookupInWorkgroup				( CFStringRef workgroupRef );
			void				BroadcastServiceLookupFailedInWorkgroup		( CFStringRef workgroupRef );
			void				BroadcastServiceLookupSucceededInWorkgroup	( CFStringRef workgroupRef );
protected: 
			
			void				WriteWorkgroupToFile				( FILE* fp );
			void				WriteNetBIOSTrackingCommentToFile	( FILE* fp );
			void				WriteNetBIOSNameToFile				( FILE* fp );			
			void				WriteCommentToFile					( FILE* fp );
			void				WriteWINSToFile						( FILE* fp );
			void				WriteCodePageToFile					( FILE* fp );
			void				WriteUnixCharsetToFile				( FILE* fp );
			void				WriteDisplayCharsetToFile			( FILE* fp );
			void				ReadConfigFile						( void );
			void				WriteToConfigFile					( const char* pathToConfigFile );
			void				CheckAndHandleIfConfigFileChanged	( void );
			
			void				SaveKnownLMBsToDisk					( void );
			void				ReadKnownLMBsFromDisk				( void );
			
	virtual	sInt32				DoPlugInCustomCall					( sDoPlugInCustomCall *inData );
	virtual sInt32				HandleNetworkTransition				( sHeader *inData );
    virtual void				ClearOutStaleNodes					( void ) {}		// do nothing for stale nodes		

// xml based config data handling
			sInt32				FillOutCurrentStateWithXML			( sDoPlugInCustomCall *inData, Boolean sizeOnly );
			sInt32				SaveNewStateFromXML					( sDoPlugInCustomCall *inData );

// legacy config data handling
			sInt32				FillOutCurrentState					( sDoPlugInCustomCall *inData );
			sInt32				SaveNewState						( sDoPlugInCustomCall *inData );

			CFArrayRef			CreateListOfWorkgroups				( void );
			void*				MakeDataBufferOfWorkgroups			( UInt32* dataLen );

    virtual CFStringRef			GetBundleIdentifier					( void );
    virtual const char*			GetProtocolPrefixString				( void );		// this is used for top of the node's path "NSL"
    virtual const char*			GetLocalNodeString					( void );		// this is the user's "Local" location
    virtual Boolean 			IsLocalNode							( const char *inNode );
    
	virtual	UInt32				GetTimeBetweenNodeLookups			( void );
    virtual void				NewNodeLookup						( void );		// this should fire off some threads in the subclass
    virtual	void				NewServiceLookup					( char* serviceType, CNSLDirNodeRep* nodeDirRep );
    virtual Boolean				OKToOpenUnPublishedNode				( const char* parentNodeName );   

private:
            void					LockNodeState					( void ) { pthread_mutex_lock( &mNodeStateLock ); }
            void					UnLockNodeState					( void ) { pthread_mutex_unlock( &mNodeStateLock ); }
            pthread_mutex_t			mNodeStateLock;
			
            void					LockBroadcastThrottler			( void ) { pthread_mutex_lock( &mBroadcastThrottlerLock ); }
            void					UnLockBroadcastThrottler		( void ) { pthread_mutex_unlock( &mBroadcastThrottlerLock ); }
            pthread_mutex_t			mBroadcastThrottlerLock;

			Boolean					mNodeListIsCurrent;
			Boolean					mNodeSearchInProgress;
			Boolean					mUseWINSURL;
            char*					mLocalNodeString;		
			char*					mWINSServer;
			char*					mNetBIOSName;
			char*					mCommentFieldString;
			Boolean					mUseComputerNameTracking;
			CFMutableDictionaryRef	mBroadcastThrottler;
			Boolean					mConfFileCodePageAlreadyModifiedByDS;
			Boolean					mInitialSearch;
			Boolean					mNeedFreshLookup;
			Boolean					mCurrentSearchCanceled;
			UInt32					mTimeBetweenLookups;
			time_t					mConfFileModTime;
			LMBDiscoverer*			mLMBDiscoverer;
			SCDynamicStoreRef		mSCRef;
			CFStringRef				mComputerNameChangeKey;
			Boolean					mRunningOnXServer;
};

#endif


