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
 * @header CSearchPlugin
 */

#ifndef __CSearchPlugin_h__
#define __CSearchPlugin_h__	1

#include <stdio.h>

#include "DirServicesTypes.h"
#include "PrivateTypes.h"
#include "PluginData.h"
#include "CConfigs.h"
#include "CBuff.h"
#include "CServerPlugin.h"
#include <CoreFoundation/CoreFoundation.h>

#define kXMLSwitchComputersKey				"Switch Computers"
#define kXMLSwitchAllKey					"Switch All"
#define kXMLServerConfigKey					"LDAP Server Config"

class	CDataBuff;

typedef enum {
	keUnknownState		= 0,
	keGetRecordList		= 1,
	keAddDataToBuff		= 2,
	keGetNextNodeRef	= 5,
	keSetContinueData	= 6,
	keDone				= 7,
	keError				= 8,
	keBufferTooSmall	= 9,
    keSearchNodeListEnd	=10
} eSearchState;

typedef struct sSearchConfig {
	sSearchList			   *fSearchNodeList;
	uInt32		  			fSearchPolicy;
	CConfigs			   *pConfigFromXML;
	char				   *fSearchNodeName;
	char				   *fSearchConfigFilePrefix;
	eDirNodeType			fDirNodeType;
	uInt32					fSearchConfigKey;	//KW we now have three options here
												//either eDSAuthenticationSearchNodeName == eDSSearchNodeName or
												//eDSContactsSearchNodeName or eDSNetworkSearchNodeName
	sSearchConfig		   *fNext;
} sSearchConfig;	//KW used to store the essentials for each search policy node collection intended for different use

typedef struct {
	uInt32				fID;
	tDirReference		fDirRef;
	tDirNodeReference	fNodeRef;
	bool				fAttrOnly;
	uInt32				fRecCount;
	uInt32				fRecIndex;
	uInt32				fLimitRecSearch;
	uInt32				fTotalRecCount;
	eSearchState		fState;
	tDataBuffer		   *fDataBuff;
	void			   *fContextData;
	bool				bNodeBuffTooSmall;
} sSearchContinueData;

typedef struct {
	sSearchList		   *fSearchNodeList;
	bool				bListChanged;
	DSMutexSemaphore   *pSearchListMutex;
	uInt32				offset;
	uInt32				fSearchConfigKey;
	void			   *fSearchNode;
	bool				bAutoSearchList;
	bool				bCheckForNIParentNow;
	uid_t				fUID;
	uid_t				fEffectiveUID;
} sSearchContextData;

class CSearchPlugin : public CServerPlugin
{

public:
						CSearchPlugin			( FourCharCode inSig, const char *inName );
	virtual			   ~CSearchPlugin			( void );

	virtual sInt32		Validate				( const char *inVersionStr, const uInt32 inSignature );
	virtual sInt32		Initialize				( void );
	//virtual sInt32		Configure				( void );
	virtual sInt32		SetPluginState			( const uInt32 inState );
	virtual sInt32		PeriodicTask			( void );
	virtual sInt32		ProcessRequest			( void *inData );
	//virtual sInt32		Shutdown				( void );

	static	void		WakeUpRequests			( void );
	static	void		ContinueDeallocProc		( void *inContinueData );
	static	void		ContextDeallocProc		( void* inContextData );
	static	void		ContextSetListChangedProc
												( void* inContextData );
	static	void		ContextSetCheckForNIParentNowProc 
												( void* inContextData );

	sInt32				CleanSearchConfigData	( sSearchConfig *inList );
	sInt32				CleanSearchListData		( sSearchList *inList );
	void				ReDiscoverNetwork		( void );

protected:
	sInt32			SwitchSearchPolicy			(	uInt32 inSearchPolicy,
													sSearchConfig *inSearchConfig );
	sInt32			HandleRequest				(	void *inData );
	sInt32			DoNetInfoDefault			(	sSearchList **inSearchNodeList );
	void			WaitForInit					(	void );
	sInt32			AddLocalNodesAsFirstPaths	(	sSearchList **inSearchNodeList );
	sInt32			AddDefaultLDAPNodesLast		(	sSearchList **inSearchNodeList );
	sSearchConfig  *MakeSearchConfigData		(	sSearchList *inSearchNodeList,
													uInt32 inSearchPolicy,
													CConfigs *inConfigFromXML,
													char *inSearchNodeName,
													char *inSearchConfigFilePrefix,
													eDirNodeType inDirNodeType,
													uInt32 inSearchConfigType );
	sSearchConfig  *FindSearchConfigWithKey		(	uInt32 inSearchConfigKey );
	sInt32			AddSearchConfigToList		(	sSearchConfig *inSearchConfig );
//	sInt32			RemoveSearchConfigWithKey	(	uInt32 inSearchConfigKey );
    static sInt32	CleanContextData			(	sSearchContextData *inContext );
    void			SetSearchPolicyIndicatorFile(	uInt32 inSearchNodeIndex,
													uInt32 inSearchPolicyIndex );
    void			RemoveSearchPolicyIndicatorFile		(	void );
	void			HandleMultipleNetworkTransitions	(	void );

private:
	sInt32			OpenDirNode				( sOpenDirNode *inData );
	sInt32			CloseDirNode			( sCloseDirNode *inData );
	sInt32			GetDirNodeInfo			( sGetDirNodeInfo *inData );
	sInt32			GetRecordList			( sGetRecordList *inData );
	sInt32			GetRecordEntry			( sGetRecordEntry *inData );
	sInt32			GetAttributeEntry		( sGetAttributeEntry *inData );
	sInt32			GetAttributeValue		( sGetAttributeValue *inData );
	sSearchList	   *GetLocalPaths			( char** localNodeName );
	sSearchList	   *GetNetInfoPaths			( bool bFullPath, char** localNodeName );
	sSearchList	   *GetDefaultLDAPPaths		( void );
	sInt32			AttributeValueSearch	( sDoAttrValueSearchWithData *inData );
	sInt32			MultipleAttributeValueSearch
											( sDoMultiAttrValueSearchWithData *inData );
	sInt32			CloseAttributeList		( sCloseAttributeList *inData );
	sInt32			CloseAttributeValueList	( sCloseAttributeValueList *inData );
	sInt32			ReleaseContinueData		( sReleaseContinueData *inData );
	sInt32			DoPlugInCustomCall		( sDoPlugInCustomCall *inData );
	
	void			SystemGoingToSleep		( void );
	void			SystemWillPowerOn			( void );

	sInt32			GetNextNodeRef			(	tDirNodeReference inNodeRef,
												tDirNodeReference *outNodeRef,
												sSearchContextData *inContext );
	tDataList	   *GetNodePath				(	tDirNodeReference inNodeRef,
												sSearchContextData *inContext );

	sInt32			AddDataToOutBuff		(	sSearchContinueData *inContinue,
												CBuff *inOutBuff,
												sSearchContextData *inContext );

	sInt32			CheckSearchPolicyChange	(	sSearchContextData *pContext,
												tDirNodeReference inNodeRef,
												tContextData inContinueData );

	sSearchContextData*	MakeContextData		( void );

	sSearchList*	DupSearchListWithNewRefs( sSearchList *inSearchList );
	sSearchList*	BuildNetworkNodeList			( void );
	sInt32			CheckForNIAutoSwitch			( void );
	CFDictionaryRef FindNIAutoSwitchToLDAPRecord	( void );
	
	sSearchConfig	   *pSearchConfigList;		//would like to block on access to this list
												//KW should make use of CFMutableArray or STL type?
	DSMutexSemaphore	fMutex;

	tDirReference		fDirRef;
	uInt32				fState;
	bool				fPluginInitialized;
	CFRunLoopRef		fServerRunLoop;
	time_t				fTransitionCheckTime;
	CFStringRef			fLZMACAddress;
	CFStringRef			fNLZMACAddress;
	char			   *fAuthSearchPathCheck;
	bool				fSomeNodeFailedToOpen;
};

#endif	// __CSearchPlugin_H__
