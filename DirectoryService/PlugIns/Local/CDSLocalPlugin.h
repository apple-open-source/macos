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
 * @header CLocalPlugin
 */

#ifndef _CDSLocalPlugin_
#define _CDSLocalPlugin_	1

#include <CoreFoundation/CoreFoundation.h>
#include "DirServicesTypes.h"
#include "DirServicesConst.h"
#include "CDSServerModule.h"
#include "SharedConsts.h"
#include "PluginData.h"
#include "BaseDirectoryPlugin.h"
#include "CDSAuthParams.h"

#define LOG_REQUEST_TIMES		0
#define LOG_MAPPINGS			0
#define FILE_ACCESS_INDEXING	1

// node dict keys
#define kNodeFilePathkey			"Node File Path"
#define kNodePathkey				"Node Path"
#define kNodeNamekey				"Node Name"
#define kNodeObjectkey				"Node Object"
#define kNodeUIDKey					"Node UID"
#define kNodeEffectiveUIDKey		"Node Effective UID"
#define kNodeAuthenticatedUserName	"Node Authenticated User Name"
#define kNodePWSDirRef				"Node PWS Dir Ref"
#define kNodePWSNodeRef				"Node PWS Node Ref"
#define kNodeLDAPDirRef				"Node LDAP Dir Ref"
#define kNodeLDAPNodeRef			"Node LDAP Node Ref"

// dsDoDirNodeAuth continue data keys
#define kAuthContinueDataHandlerTag				"Handler Tag"
#define kAuthContinueDataAuthAuthority			"Auth Authority"
#define kAuthContinueDataMutableRecordDict		"Record Dict"
#define kAuthContinueDataAuthedUserIsAdmin		"Authed User Is Admin"
#define kAuthCOntinueDataPassPluginContData		"Password Plugin Continue Data"

//auth type tags
#define kHashNameListPrefix				"HASHLIST:"
#define kHashNameNT						"SMB-NT"
#define kHashNameLM						"SMB-LAN-MANAGER"
#define kHashNameCRAM_MD5				"CRAM-MD5"
#define kHashNameSHA1					"SALTED-SHA1"
#define kHashNameRecoverable			"RECOVERABLE"
#define kHashNameSecure					"SECURE"

#define kDBPath				"/var/db/dslocal/"
#define kDBNodesPath		"/var/db/dslocal/nodes"
#define kNodesDir			"nodes"
#define kDictionaryType		"Dict Type"

// continue data keys
#define kContinueDataRecordsArrayKey			"Records Array"
#define kContinueDataNumRecordsReturnedKey		"Num Records Returned"
#define kContinueDataDesiredAttributesArrayKey	"Desired Attributes Array"

// node info dict for use in GetDirNodeInfo
#define kNodeInfoDictType						"Node Info"
#define kNodeInfoAttributes						"Attributes"

// attrValueListRef dict
#define kAttrValueListRefAttrName				"Attribute"

// open record dict
#define kOpenRecordDictRecordFile				"Record File"
#define kOpenRecordDictRecordType				"Record Type"
#define kOpenRecordDictAttrsValues				"Attributes Values"
#define kOpenRecordDictNodeDict					"Node Dict"
#define kOpenRecordDictIsDeleted				"Is Deleted"
#define kOpenRecordDictRecordWasChanged			"Record Was Changed"

#define kDefaultNodeName		"Default"
#define kTargetNodeName			"Target"
#define kTopLevelNodeName		"Local"
#define kLocalNodePrefix		"/Local"
#define kLocalNodePrefixRoot	"/Local/"
#define kLocalNodeDefault		"/Local/Default"
#define kLocalNodeDefaultRoot	"/Local/Default/"

enum {
	ePluginHashLM							= 0x0001,
	ePluginHashNT							= 0x0002,
	ePluginHashSHA1							= 0x0004,		/* deprecated */
	ePluginHashCRAM_MD5						= 0x0008,
	ePluginHashSaltedSHA1					= 0x0010,
	ePluginHashRecoverable					= 0x0020,
	ePluginHashSecurityTeamFavorite			= 0x0040,
	
	ePluginHashDefaultSet			= ePluginHashSaltedSHA1,
	ePluginHashWindowsSet			= ePluginHashNT | ePluginHashSaltedSHA1 | ePluginHashLM,
	ePluginHashDefaultServerSet		= ePluginHashNT | ePluginHashSaltedSHA1 | ePluginHashLM |
										ePluginHashCRAM_MD5 | ePluginHashRecoverable,
	ePluginHashAll					= 0x007F,
	ePluginHashHasReadConfig		= 0x8000
};

#define kCustomCallFlushRecordCache			FOUR_CHAR_CODE( 'FCCH' )

class CDSLocalPluginNode;

class CDSLocalPlugin : public BaseDirectoryPlugin
{
	public:
		bool					mPWSFrameworkAvailable;
	
								CDSLocalPlugin( FourCharCode inSig, const char *inName );
		virtual					~CDSLocalPlugin( void );

		// required pure virtuals for BaseDirectoryPlugin
		virtual SInt32			Initialize				( void );
		virtual SInt32			SetPluginState			( const UInt32 inState );
		virtual SInt32			PeriodicTask			( void );
		virtual CDSAuthParams*	NewAuthParamObject		( void );
	
		static SInt32			FillBuffer				( CFMutableArrayRef inRecordList, BDPIOpaqueBuffer inData );
		static const char		*GetCStringFromCFString	( CFStringRef inCFString, char **outCString );
		static void				FilterAttributes		( CFMutableDictionaryRef inRecord, CFArrayRef inRequestedAttribs, CFStringRef inNodeName );
		
		void					CloseDatabases( void );
	
		// from CDSServerModule:
		virtual SInt32			Validate( const char *inVersionStr, const UInt32 inSignature );
		virtual SInt32			ProcessRequest( void *inData );

		CFStringRef				AttrNativeTypeForStandardType( CFStringRef inStdType );
		CFStringRef				AttrStandardTypeForNativeType( CFStringRef inNativeType );
		CFStringRef				AttrPrefixedNativeTypeForNativeType( CFStringRef inNativeType );
		CFStringRef				RecordNativeTypeForStandardType( CFStringRef inStdType );
		CFStringRef				RecordStandardTypeForNativeType( CFStringRef inNativeType );
		CFStringRef				RecordPrefixedNativeTypeForNativeType( CFStringRef inNativeType );

		CFStringRef				GetUserGUID( CFStringRef inUserName, CDSLocalPluginNode* inNode );
		
		bool					SearchAllRecordTypes( CFArrayRef inRecTypesArray );
		
		// internal wrappers for task handlers
		tDirNodeReference		GetInternalNodeRef( void );
		tDirStatus				CreateRecord( CFStringRef inStdRecType, CFStringRef inRecName,
									bool inOpenRecord, tRecordReference* outOpenRecordRef = NULL );
		tDirStatus				OpenRecord( CFStringRef inStdRecType, CFStringRef inRecName,
									tRecordReference* outOpenRecordRef );
		tDirStatus				CloseRecord( tRecordReference inRecordRef );
		tDirStatus				AddAttribute( tRecordReference inRecordRef, CFStringRef inAttributeName,
									CFStringRef inAttrValue );
		tDirStatus				RemoveAttribute( tRecordReference inRecordRef, CFStringRef inAttributeName );		
		tDirStatus				GetRecAttrValueByIndex( tRecordReference inRecordRef, const char *inAttributeName,
									UInt32 inIndex, tAttributeValueEntryPtr *outEntryPtr );	
		tDirStatus				AddAttributeValue( tRecordReference inRecordRef, const char *inAttributeType,
									const char *inAttributeValue );
		tDirStatus				GetRecAttribInfo( tRecordReference inRecordRef, const char *inAttributeType,
									tAttributeEntryPtr *outAttributeInfo );
		
		// auth stuff
		tDirStatus				AuthOpen( tDirNodeReference inNodeRef, const char* inUserName, const char* inPassword,
									bool inUserIsAdmin, bool inIsEffectiveRoot=false );
		UInt32					DelayFailedLocalAuthReturnsDeltaInSeconds()
									{ return mDelayFailedLocalAuthReturnsDeltaInSeconds; }

		void					AddContinueData( tDirNodeReference inNodeRef, CFMutableDictionaryRef inContinueData, tContextData *inOutAttachReference );
		CFMutableDictionaryRef	RecordDictForRecordRef( tRecordReference inRecordRef );
		CFMutableDictionaryRef	CopyNodeDictForNodeRef( tDirNodeReference inNodeRef );
		CDSLocalPluginNode*		NodeObjectFromNodeDict( CFDictionaryRef inNodeDict );
		CDSLocalPluginNode*		NodeObjectForNodeRef( tDirNodeReference inNodeRef );
		CFArrayRef				CreateOpenRecordsOfTypeArray( CFStringRef inNativeRecType );

		static void					ContextDeallocProc( void* inContextData );
		static const char*			CreateCStrFromCFString( CFStringRef inCFStr, char **ioCStr );
		static CFMutableArrayRef	CFArrayCreateFromDataList( tDataListPtr inDataList );
		static void					LogCFDictionary( CFDictionaryRef inDict, CFStringRef inPrefixMessage );
		static tDirStatus			OpenDirNodeFromPath( CFStringRef inPath, tDirReference inDSRef,
										tDirNodeReference* outNodeRef );
		static	void				WakeUpRequests( void );

		tDirStatus					GetDirServiceRef( tDirReference *outDSRef );
	
		CFTypeRef					NodeDictCopyValue( CFDictionaryRef inNodeDict, const void *inKey );
		void						NodeDictSetValue( CFMutableDictionaryRef inNodeDict, const void *inKey, const void *inValue );
	
		CFArrayRef					CopyKerberosServiceList( void );
	
		void						FlushCaches( CFStringRef inStdType );
		
		CFMutableDictionaryRef		mOpenRecordRefs;
		CFDictionaryRef				mPermissions;

	protected:

		// required pure virtuals for BaseDirectoryPlugin
		virtual CFDataRef			CopyConfiguration		( void );
		virtual bool				NewConfiguration		( const char *inData, UInt32 inLength );
		virtual bool				CheckConfiguration		( const char *inData, UInt32 inLength );
		virtual tDirStatus			HandleCustomCall		( sBDPINodeContext *pContext, sDoPlugInCustomCall *inData );
		virtual bool				IsConfigureNodeName		( CFStringRef inNodeName );
		virtual BDPIVirtualNode		*CreateNodeForPath		( CFStringRef inPath, uid_t inUID, uid_t inEffectiveUID );

		// subclass
		void						WaitForInit( void );

	private:

		// task handlers
		virtual tDirStatus		OpenDirNode( sOpenDirNode* inData );
		virtual tDirStatus		CloseDirNode( sCloseDirNode* inData );
		virtual tDirStatus		GetDirNodeInfo( sGetDirNodeInfo* inData );
		virtual tDirStatus		GetRecordList( sGetRecordList* inData );
		virtual tDirStatus		DoAttributeValueSearch( sDoAttrValueSearch* inData );
		virtual tDirStatus		DoAttributeValueSearchWithData( sDoAttrValueSearchWithData* inData );
		virtual tDirStatus		DoMultipleAttributeValueSearch( sDoMultiAttrValueSearch* inData );
		virtual tDirStatus		DoMultipleAttributeValueSearchWithData( sDoMultiAttrValueSearchWithData* inData );
		virtual tDirStatus		ReleaseContinueData( sReleaseContinueData* inData );
		virtual tDirStatus		CloseAttributeValueList( sCloseAttributeValueList* inData );
		virtual tDirStatus		OpenRecord( sOpenRecord* inData );
		virtual tDirStatus		CloseRecord( sCloseRecord* inData );

	public:
		virtual tDirStatus		FlushRecord( sFlushRecord* inData );
		virtual tDirStatus		SetAttributeValues( sSetAttributeValues* inData, const char *inRecTypeStr );

	private:
		virtual tDirStatus		SetRecordName( sSetRecordName* inData );
		virtual tDirStatus		DeleteRecord( sDeleteRecord* inData );
		virtual tDirStatus		CreateRecord( sCreateRecord* inData );
		virtual tDirStatus		AddAttribute( sAddAttribute* inData, const char *inRecTypeStr );
		virtual tDirStatus		AddAttributeValue( sAddAttributeValue* inData, const char *inRecTypeStr );
		virtual tDirStatus		RemoveAttribute( sRemoveAttribute* inData, const char *inRecTypeStr );
		virtual tDirStatus		RemoveAttributeValue( sRemoveAttributeValue* inData, const char *inRecTypeStr );
		virtual tDirStatus		SetAttributeValue( sSetAttributeValue* inData, const char *inRecTypeStr );
		virtual tDirStatus		GetRecAttrValueByID( sGetRecordAttributeValueByID* inData );
		virtual tDirStatus		GetRecAttrValueByIndex( sGetRecordAttributeValueByIndex* inData );
		virtual tDirStatus		GetRecAttrValueByValue( sGetRecordAttributeValueByValue* inData );
		virtual tDirStatus		GetRecRefInfo( sGetRecRefInfo* inData );
		virtual tDirStatus		GetRecAttribInfo( sGetRecAttribInfo* inData );
		virtual tDirStatus		DoAuthentication( sDoDirNodeAuth *inData, const char *inRecTypeStr,
													CDSAuthParams &inParams );
		
		bool					CheckForShellAndValidate( CFDictionaryRef nodeDict, CDSLocalPluginNode *node, CFStringRef nativeAttrType,
														 const char *buffer, UInt32 bufferLen );

		bool					BufferFirstTwoItemsEmpty( tDataBufferPtr inAuthBuffer );
		void					AuthenticateRoot( CFMutableDictionaryRef nodeDict, bool *inAuthedUserIsAdmin, CFStringRef *inOutAuthedUserName );
		virtual tDirStatus		DoPlugInCustomCall( sDoPlugInCustomCall* inData );
		virtual tDirStatus		HandleNetworkTransition( sHeader* inData );

		// internal-use methods
		void					FreeExternalNodesInDict( CFMutableDictionaryRef inNodeDict );
		bool					CreateLocalDefaultNodeDirectory( void );
		CFMutableArrayRef		FindSubDirsInDirectory( const char* inDir );
		const char*				GetProtocolPrefixString();
		bool					LoadMappings();
		void					LoadSettings();

		tDirStatus				PackRecordsIntoBuffer( CFDictionaryRef inNodeDict, UInt32 inFirstRecordToReturnIndex,
									CFArrayRef inRecordsArray, CFArrayRef inDesiredAttributes, tDataBufferPtr inBuff,
									bool inAttrInfoOnly, UInt32* outNumRecordsPacked );
		void					AddRecordTypeToRecords( CFStringRef inStdType, CFArrayRef inRecordsArray );
		tDirStatus				AddMetanodeToRecords( CFArrayRef inRecords, CFStringRef inNode );
		void					RemoveUndesiredAttributes( CFArrayRef inRecords, CFArrayRef inDesiredAttributes );
		tDirStatus				ReadHashConfig( CDSLocalPluginNode* inNode );

		CFDictionaryRef			CreateDictionariesFromFiles( CFArrayRef inFilePaths );

		CFDictionaryRef			CreateNodeInfoDict( CFArrayRef inDesiredAttrs, CFDictionaryRef inNodeDict );

		tDirStatus				GetRetainedRecordDict( CFStringRef inRecordName, CFStringRef inNativeRecType,
									CDSLocalPluginNode* inNode, CFMutableDictionaryRef* outRecordDict );
		bool					RecurseUserIsMemberOfGroup( CFStringRef inUserRecordName, CFStringRef inUserGUID,
									CFStringRef inUserGID, CFDictionaryRef inGroupDict, CDSLocalPluginNode* inNode );

		void					LockOpenAttrValueListRefs( bool inWriting );
		void					UnlockOpenAttrValueListRefs();
		CFTypeRef				GetAttrValueFromInput( const char* inData, UInt32 inDataLength );
		CFMutableArrayRef		CreateCFArrayFromGenericDataList( tDataListPtr inDataList );
		CFDictionaryRef			CopyNodeDictandNodeObject( CFDictionaryRef inOpenRecordDict, CDSLocalPluginNode **outPluginNode );
		static void				ContinueDeallocProc( void *inContinueData );
		
	private:
		DSMutexSemaphore			mOpenNodeRefsLock;
		DSMutexSemaphore			mOpenRecordRefsLock;
		DSMutexSemaphore			mOpenAttrValueListRefsLock;
		DSMutexSemaphore			mGeneralPurposeLock;

		UInt32						mSignature;
        CFMutableDictionaryRef		mOpenNodeRefs;
		CFMutableDictionaryRef		mOpenAttrValueListRefs;
		UInt32						mHashList;
		UInt32						mDelayFailedLocalAuthReturnsDeltaInSeconds;

		CFDictionaryRef				mAttrNativeToStdMappings;
		CFDictionaryRef				mAttrStdToNativeMappings;
		CFMutableDictionaryRef		mAttrPrefixedNativeToNativeMappings;
		CFMutableDictionaryRef		mAttrNativeToPrefixedNativeMappings;
		CFDictionaryRef				mRecNativeToStdMappings;
		CFDictionaryRef				mRecStdToNativeMappings;
		CFMutableDictionaryRef		mRecPrefixedNativeToNativeMappings;
		tDirReference				mDSRef;
		tDirReference				mInternalDirRef;
		tDirNodeReference			mInternalNodeRef;
};

#endif
