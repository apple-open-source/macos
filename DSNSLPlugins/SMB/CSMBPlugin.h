/*
 *  CSMBPlugin.h
 *
 *  Created by imlucid on Wed Aug 15 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

#ifndef _CSMBPlugin_

#include "CNSLPlugin.h"

#define	__APPLE_NMBLOOKUP_HACK_2987131	// this will ask nmblookup to give us raw bytes

#define kProtocolPrefixPlainStr		"SMB"
#define kProtocolPrefixStr			"/SMB"
#define kProtocolPrefixSlashStr		"/SMB/"
#define kScopePrefixStr				"Network Neighborhood(DS)"
#define kScopePrefixSlashStr		"Network Neighborhood(DS)/"
#define kReadSMBConfigData			'read'
#define kWriteSMBConfigData			'writ'

Boolean ExceptionInResult( const char* resultPtr );
int IsIPAddress(const char* adrsStr, long *ipAdrs);
Boolean IsDNSName(char* theName);

class CSMBPlugin : public CNSLPlugin
{
public:
                                CSMBPlugin				( void );
	virtual                     ~CSMBPlugin				( void );
    
    virtual sInt32				InitPlugin				( void );
	virtual sInt32				GetDirNodeInfo			( sGetDirNodeInfo *inData );

            Boolean				IsScopeInReturnList		( const char* scope );
            void				AddResult				( const char* url );
    
            uInt32				fSignature;
			const char*			GetWinsServer			( void ) { return mWINSServer; }
			void				AddWINSWorkgroup		( const char* workgroup );
			Boolean				IsWINSWorkgroup			( const char* workgroup );
			void				NodeLookupIsCurrent		( void ) { mNodeListIsCurrent = true; }

			const char*			GetBroadcastAdddress	( void );
protected: 
			void				WriteWorkgroupToFile	( FILE* fp );
			void				WriteWINSToFile			( FILE* fp );
			void				ReadConfigFile			( void );
			void				WriteToConfigFile		( void );
			
			
	virtual	sInt32				DoPlugInCustomCall		( sDoPlugInCustomCall *inData );
	virtual sInt32				HandleNetworkTransition	( sHeader *inData );
	
			sInt32				GetPrimaryInterfaceBroadcastAdrs( char** broadcastAddr );
	
			sInt32				FillOutCurrentState		( sDoPlugInCustomCall *inData );
			void*				MakeDataBufferOfWorkgroups( UInt32* dataLen );

    virtual CFStringRef			GetBundleIdentifier		( void );
    virtual const char*			GetProtocolPrefixString	( void );		// this is used for top of the node's path "NSL"
    virtual const char*			GetLocalNodeString		( void );		// this is the user's "Local" location
    virtual Boolean 			IsLocalNode				( const char *inNode );
    
    virtual void				NewNodeLookup			( void );		// this should fire off some threads in the subclass
    virtual	void				NewServiceLookup		( char* serviceType, CNSLDirNodeRep* nodeDirRep );
    virtual Boolean				OKToOpenUnPublishedNode	( const char* parentNodeName );    

private:
			Boolean					mNodeListIsCurrent;
            char*					mLocalNodeString;		
            char*					mServiceTypeString;
			char*					mTemplateConfFilePath;
			char*					mConfFilePath;
            char*					mNMBLookupToolPath;
			char*					mWINSServer;
			CFMutableDictionaryRef	mWINSWorkgroups;
			char*					mBroadcastAddr;
};

#endif


