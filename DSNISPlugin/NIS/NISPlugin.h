/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 
/*!
 *  @header NISPlugin
 */

#ifndef _NISPlugin_
#define _NISPlugin_	1

#include <CoreFoundation/CoreFoundation.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>

#include <DirectoryServiceCore/CBuff.h>
#include <DirectoryServiceCore/CAttributeList.h>
#include <DirectoryServiceCore/CDSServerModule.h>
#include <DirectoryServiceCore/SharedConsts.h>
#include <DirectoryServiceCore/PluginData.h>

#include <stdio.h>
#include <unistd.h>		// for _POSIX_THREADS
#include <pthread.h>	// for pthread_*_t

#include "NISDebugLog.h"
#include "NISResult.h"

#define kProtocolPrefixPlainStr		"NIS"
#define kProtocolPrefixStr			"/NIS"
#define kProtocolPrefixSlashStr		"/NIS/"
#define kReadNISConfigData			'read'
#define kWriteNISConfigData			'writ'

#define kNoDomainName				"-NO-"

#define kBindErrorString			"Can't bind to server which serves this domain"

#define kMaxTimeForMapCacheToLive	5*60*60		// 5 hours

Boolean ExceptionInResult( const char* resultPtr );
int IsIPAddress(const char* adrsStr, long *ipAdrs);
Boolean IsDNSName(char* theName);

enum eBuffType {
    kRecordListType			= 'RecL',
    kClientSideParsingBuff	= 'StdB'
};

typedef struct NodeData {
    CFStringRef				fNodeName;			// this is what our plugin is going to find
    tDataList*				fDSName;			// this is the converted DS construct
    UInt32					fTimeStamp;			// this is when the node was last "found"
    bool					fIsADefaultNode;	// is this node one that is considered "local" by plugin?
    uInt32					fSignature;			// signature of plugin registering this
    CFMutableDictionaryRef	fServicesRefTable;
} NodeData;

typedef struct sNISContextData {
//	LDAP		   *fHost;				//LDAP session handle
//	uInt32			fConfigTableIndex;	//gConfigTable Hash index
//	char		   *fName;				//LDAP domain name ie. ldap.apple.com
//	int				fPort;				//LDAP port number - default is 389
//	int				fType;				//KW type of reference entry - not used yet
//	int				msgId;				//LDAP session call handle mainly used for searches
//    bool			authCallActive;		//indicates if authentication was made through the API
    									//call and if set means don't use config file auth name/password
//    char		   *authAccountName;	//Account name used in auth
//    char		   *authPassword;		//Password used in auth
//    LDAPMessage	   *pResult;			//LDAP message last result
//    uInt32			fRecNameIndex;		//index used to cycle through all requested Rec Names
//    uInt32			fRecTypeIndex;		//index used to cycle through all requested Rec Types
//    uInt32			fTotalRecCount;		//count of all retrieved records
//    uInt32			fLimitRecSearch;	//client specified limit of number of records to return
//    uInt32			fAttrIndex;			//index used to cycle through all requested Attrs
    uInt32			offset;				//offset into the data buffer
//    uInt32			index;
//    uInt32			attrCnt;
//    char		   *fOpenRecordType;	//record type used to open a record
//    char		   *fOpenRecordName;	//record name used to open a record
} sNISContextData;

typedef struct {
	uInt32				fAuthPass;
	uInt32				fLimitRecSearch;
	uInt32				fRecNameIndex;
	uInt32				fRecTypeIndex;
	uInt32				fAllRecIndex;
	uInt32				fTotalRecCount;
	uInt32				fAttrIndex;
	void			   *fAuthHndl;
	char			   *fAuthAuthorityData;
    tContextData		fPassPlugContinueData;
	CFMutableArrayRef	fResultArrayRef;
	bool				fSearchComplete;
} sNISContinueData;


typedef enum
{
	kNISypcat			= 0,
	kNISdomainname		= 1,
	kNISrpcinfo			= 2,
	kNISportmap			= 3,
	kNISbind			= 4,
	kNISypwhich			= 5,
	kNISypmatch			= 6
} NISLookupType;

#define		kNISypcatPath		"/usr/bin/ypcat"
#define		kNISdomainnamePath	"/bin/domainname"
#define		kNISrpcinfoPath		"/usr/sbin/rpcinfo"
#define		kNISportmapPath		"/usr/sbin/portmap"
#define		kNISbindPath		"/usr/sbin/ypbind"
#define		kNISypwhichPath		"/usr/bin/ypwhich"
#define		kNISypmatchPath		"/usr/bin/ypmatch"

enum {
	kAuthUnknowMethod		= 1220,
	kAuthClearText			= 1221,
	kAuthCrypt				= 1222,
	kAuthSetPasswd			= 1323,
	kAuthSetPasswdAsRoot	= 1224,
	kAuthChangePasswd		= 1225,
	kAuthAPOP				= 1226,
	kAuth2WayRandom			= 1227,
	kAuthNativeClearTextOK	= 1228,
	kAuthNativeNoClearText	= 1229,
	kAuthNIS_NT_Key			= 1230,
	kAuthNIS_LM_Key			= 1231,
	kAuthNativeMethod		= 1232,
	kAuthCRAM_MD5			= 1233
};

class NISDirNodeRep;
class CPlugInRef;
class CDataBuff;

class NISPlugin : public CDSServerModule
{
public:
                                NISPlugin						( void );
    virtual                     ~NISPlugin						( void );
			sInt32				GetNISConfiguration				( void );
			sInt32				ReadInNISMappings				( void );
            
	virtual sInt32				Validate						( const char *inVersionStr, const uInt32 inSignature );
	virtual sInt32				ProcessRequest					( void *inData );
	virtual sInt32				SetPluginState					( const uInt32 inState );

#pragma mark
            void				AddNode							( const char* nodeName, Boolean isLocalNode = false );
            void				RemoveNode						( CFStringRef nodeNameRef );

            void				LockPlugin						( void ) { pthread_mutex_lock( &mPluginLock ); }
            void				UnlockPlugin					( void ) { pthread_mutex_unlock( &mPluginLock ); }
            pthread_mutex_t		mPluginLock;
            
            void				LockPublishedNodes				( void ) { pthread_mutex_lock( &mQueueLock ); }
            void				UnlockPublishedNodes			( void ) { pthread_mutex_unlock( &mQueueLock ); }
            pthread_mutex_t		mQueueLock;

			void				ResetMapCache					( void );
            void				LockMapCache					( void ) { pthread_mutex_lock( &mMapCache ); }
            void				UnlockMapCache					( void ) { pthread_mutex_unlock( &mMapCache ); }
            pthread_mutex_t		mMapCache;

            void				StartNodeLookup					( void );			// this should fire off some threads in the subclass

			const char*			GetProtocolPrefixString			( void );
    
			char*				CreateNISTypeFromRecType		( char *inRecType );
			CFStringRef			CreateNISTypeRefFromRecType		( char *inRecType );
    
protected:
#pragma mark
			sInt32				SaveNewState					( sDoPlugInCustomCall *inData );

			void				ConfigureLookupdIfNeeded		( void );
			Boolean				LookupdIsConfigured				( void );
			void				SaveDefaultLookupdConfiguration	( void );

			sInt32				SetDomain						( CFStringRef domainNameRef );
			sInt32				UpdateHostConfig				( void );
			sInt32				SetNISServers					( CFStringRef nisServersRef );
			CFStringRef			CreateListOfServers				( void );


			sInt32				FillOutCurrentState				( sDoPlugInCustomCall *inData );

			CFMutableDictionaryRef	CopyRecordLookup				( char* mname, char* recordName );
            void					DoRecordsLookup				(	CFMutableArrayRef	resultArrayRef,
																	char*				mname,
																	char*				recordName = NULL,
																	tDirPatternMatch	inAttributePatternMatch = eDSAnyMatch,
																	tDataNodePtr		inAttributePatt2Match = NULL );
																	
			CFMutableDictionaryRef	CreateParseResult					( char *data, const char* mname );
			
            sInt32				HandleRequest 					( void *inData );

			sInt32				DoPlugInCustomCall				( sDoPlugInCustomCall *inData );

			sInt32				GetDirNodeInfo					( sGetDirNodeInfo *inData );

			Boolean				IsOurConfigNode					( char* path );
			Boolean				IsOurDomainNode					( char* path );
            sInt32				OpenDirNode 					( sOpenDirNode *inData );
            sInt32				CloseDirNode 					( sCloseDirNode *inData );
			
			sInt32				ReleaseContinueData				( sNISContinueData* continueData );
			
			sInt32				GetAttributeEntry				( sGetAttributeEntry *inData );
			sInt32				GetAttributeValue				( sGetAttributeValue *inData );
            sInt32				GetRecordList					( sGetRecordList *inData );
            
			sInt32				OpenRecord						( sOpenRecord *inData );
			sInt32				GetRecordAttributeValueByIndex	( sGetRecordAttributeValueByIndex *inData );
			sInt32				DoAttributeValueSearch			( sDoAttrValueSearch* inData );
			sInt32				CloseRecord						( sCloseRecord *inData );
			
			void				AddRecordRecRef					( tRecordReference recRef, CFDictionaryRef resultRef );
			void				DeleteRecordRecRef				( tRecordReference recRef );
			CFDictionaryRef		GetRecordFromRecRef				( tRecordReference recRef );


			sInt32				HandleNetworkTransition			( sHeader *inData );
    
			CFDictionaryRef		CopyRecordResult				( char* mname, char* recordName );
			Boolean				RecordIsAMatch					( CFDictionaryRef recordRef, tDirPatternMatch inAttributePatternMatch, tDataNodePtr inAttributePatt2Match );
			char*				CopyResultOfLookup				( NISLookupType	type, const char* mname = NULL, const char* key = NULL );
			CFDictionaryRef		CopyMapResults					( const char* mname );

			Boolean				ResultMatchesRequestRecordNameCriteria (	CFDictionaryRef		result,
																			tDirPatternMatch	patternMatch,
																			tDataListPtr		inRecordNameList );
																			    
            sInt32				RetrieveResults					(	tDirNodeReference	inNodeRef,
																	tDataBufferPtr		inDataBuff,
																	tDirPatternMatch	inRecordNamePatternMatch,
																	tDataListPtr		inRecordNameList,
																	tDataListPtr		inRecordTypeList,
																	bool				inAttributeInfoOnly,
																	tDataListPtr		inAttributeInfoTypeList,
																	unsigned long*		outRecEntryCount,
																	sNISContinueData*	continueData );

			sInt32				DoAuthentication				( sDoDirNodeAuth *inData );
			sInt32				DoBasicAuth						(	tDirNodeReference	inNodeRef,
																	tDataNodePtr		inAuthMethod, 
																	NISDirNodeRep*		inContext, 
																	sNISContinueData**	inOutContinueData, 
																	tDataBufferPtr		inAuthData,
																	tDataBufferPtr		outAuthData, 
																	bool				inAuthOnly,
																	char*				inAuthAuthorityData );
																	
			sInt32				DoUnixCryptAuth					( NISDirNodeRep *inContext, tDataBuffer *inAuthData, bool inAuthOnly );
			sInt32				GetAuthMethod					( tDataNode *inData, uInt32 *outAuthMethod );
			sInt32				GetUserNameFromAuthBuffer		( tDataBufferPtr inAuthData, unsigned long inUserNameIndex, char **outUserName );
			sInt32				IsValidRecordName				( const char *inRecName, const char	*inRecType, void *inDomain );
			sInt32				VerifyPatternMatch				( const tDirPatternMatch inPatternMatch );
			
        uInt32					mState;
private:
    virtual sInt32				Initialize						( void );
	virtual sInt32				PeriodicTask					( void );
        
		char*					mLocalNodeString;
		CFMutableStringRef		mMetaNodeLocationRef;
		CFDictionaryRef			mMappingTable;
        uInt32					mSignature;
		CFAbsoluteTime			mLastTimeCacheReset;
		Boolean					mWeLaunchedYPBind;
		Boolean					mLookupdIsAlreadyConfigured;

        CFMutableDictionaryRef	mOpenRefTable;
        CFMutableDictionaryRef	mPublishedNodes;		// we keep track of published nodes here
		CFMutableDictionaryRef	mOpenRecordsRef;
		CFMutableDictionaryRef	mCachedMapsRef;
};

NodeData * AllocateNodeData();
void DeallocateNodeData( NodeData *nodeData );


UInt32 GetCurrentTime( void );		// returns current time in seconds

#endif		// #ifndef
