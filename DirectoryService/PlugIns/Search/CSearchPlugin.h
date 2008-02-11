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
#include "CInternalDispatchThread.h"
#include <CoreFoundation/CoreFoundation.h>
#include "DSEventSemaphore.h"

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
	UInt32		  			fSearchNodePolicy;
	CConfigs			   *pConfigFromXML;
	char				   *fSearchNodeName;
	char				   *fSearchConfigFilePrefix;
	eDirNodeType			fDirNodeType;
	UInt32					fSearchConfigKey;	//KW we now have three options here
												//either eDSAuthenticationSearchNodeName == eDSSearchNodeName or
												//eDSContactsSearchNodeName or eDSNetworkSearchNodeName
	sSearchConfig		   *fNext;
} sSearchConfig;	//KW used to store the essentials for each search policy node collection intended for different use

typedef struct {
	UInt32				fID;
	tDirReference		fDirRef;
	tDirNodeReference	fNodeRef;
	bool				fAttrOnly;
	UInt32				fRecCount;
	UInt32				fRecIndex;
	UInt32				fLimitRecSearch;
	UInt32				fTotalRecCount;
	eSearchState		fState;
	tDataBuffer		   *fDataBuff;
	void			   *fContextData;
	bool				bNodeBuffTooSmall;
	bool				bIsAugmented;
	tDataListPtr		fAugmentReqAttribs;
} sSearchContinueData;

typedef struct {
	sSearchList		   *fSearchNodeList;
	int32_t				bListChanged;   // int32_t because it's used in OSAtomic calls
	DSMutexSemaphore   *pSearchListMutex;
	UInt32				offset;
	UInt32				fSearchConfigKey;
	void			   *fSearchNode;
	bool				bAutoSearchList;
	bool				bCheckForNIParentNow;
	uid_t				fUID;
	uid_t				fEffectiveUID;
#if AUGMENT_RECORDS
	CConfigs		   *pConfigFromXML;
#endif
} sSearchContextData;

class CSearchPluginHandlerThread : public CInternalDispatchThread
{
public:
					CSearchPluginHandlerThread			( void );
					CSearchPluginHandlerThread			( const FourCharCode inThreadSignature, int inWhichFunction, void *inNeededClass );
	virtual		   ~CSearchPluginHandlerThread			( void );
	
	virtual	long	ThreadMain			( void );		// we manage our own thread top level
	virtual	void	StartThread			( void );
	virtual	void	StopThread			( void );

protected:
	virtual	void	LastChance			( void );
			
private:
	int				fWhichFunction;
	void		   *fNeededClass;
};

__BEGIN_DECLS
int ShouldRegisterWorkstation(void);
__END_DECLS

class CSearchPlugin : public CServerPlugin
{

public:
	static DSEventSemaphore	fAuthPolicyChangeEvent;
	static DSEventSemaphore	fContactPolicyChangeEvent;
	static int32_t			fAuthCheckNodeThreadActive;
	static int32_t			fContactCheckNodeThreadActive;
	
						CSearchPlugin			( FourCharCode inSig, const char *inName );
	virtual			   ~CSearchPlugin			( void );

	virtual SInt32		Validate				( const char *inVersionStr, const UInt32 inSignature );
	virtual SInt32		Initialize				( void );
	//virtual SInt32		Configure				( void );
	virtual SInt32		SetPluginState			( const UInt32 inState );
	virtual SInt32		PeriodicTask			( void );
	virtual SInt32		ProcessRequest			( void *inData );
	//virtual SInt32		Shutdown				( void );

	static	void		WakeUpRequests			( void );
	static	void		ContinueDeallocProc		( void *inContinueData );
	static	void		ContextDeallocProc		( void* inContextData );
	static	void		ContextSetListChangedProc
												( void* inContextData );
	static	void		ContextSetCheckForNIParentNowProc 
												( void* inContextData );

	SInt32				CleanSearchConfigData	( sSearchConfig *inList );
	SInt32				CleanSearchListData		( sSearchList *inList );
	void				EnsureCheckNodesThreadIsRunning	( tDirPatternMatch policyToCheck );
	void				CheckNodes				( tDirPatternMatch policyToCheck, int32_t *threadFlag, DSEventSemaphore *eventSemaphore );

	bool				fRegisterWorkstation;

protected:
	bool			SwitchSearchPolicy			(	UInt32 inSearchPolicy,
													sSearchConfig *inSearchConfig );
	SInt32			HandleRequest				(	void *inData );
	void			WaitForInit					(	void );
	SInt32			AddLocalNodesAsFirstPaths	(	sSearchList **inSearchNodeList );
	SInt32			AddDefaultLDAPNodesLast		(	sSearchList **inSearchNodeList );
	sSearchConfig  *MakeSearchConfigData		(	sSearchList *inSearchNodeList,
													UInt32 inSearchPolicy,
													CConfigs *inConfigFromXML,
													char *inSearchNodeName,
													char *inSearchConfigFilePrefix,
													eDirNodeType inDirNodeType,
													UInt32 inSearchConfigType );
	sSearchConfig  *FindSearchConfigWithKey		(	UInt32 inSearchConfigKey );
	SInt32			AddSearchConfigToList		(	sSearchConfig *inSearchConfig );
//	SInt32			RemoveSearchConfigWithKey	(	UInt32 inSearchConfigKey );
    static SInt32	CleanContextData			(	sSearchContextData *inContext );
    void			SetSearchPolicyIndicatorFile(	UInt32 inSearchNodeIndex,
													UInt32 inSearchPolicyIndex );
    void			RemoveSearchPolicyIndicatorFile		(	void );
	void			HandleMultipleNetworkTransitions	(	void );

private:
	SInt32			OpenDirNode				( sOpenDirNode *inData );
	SInt32			CloseDirNode			( sCloseDirNode *inData );
	SInt32			GetDirNodeInfo			( sGetDirNodeInfo *inData );
	SInt32			GetRecordList			( sGetRecordList *inData );
	SInt32			GetRecordEntry			( sGetRecordEntry *inData );
	SInt32			GetAttributeEntry		( sGetAttributeEntry *inData );
	SInt32			GetAttributeValue		( sGetAttributeValue *inData );
	sSearchList	   *GetDefaultLocalPath		( void );
	sSearchList	   *GetBSDLocalPath			( void );
	sSearchList	   *GetDefaultLDAPPaths		( void );
	SInt32			AttributeValueSearch	( sDoAttrValueSearchWithData *inData );
	SInt32			MultipleAttributeValueSearch
											( sDoMultiAttrValueSearchWithData *inData );
	SInt32			CloseAttributeList		( sCloseAttributeList *inData );
	SInt32			CloseAttributeValueList	( sCloseAttributeValueList *inData );
	SInt32			ReleaseContinueData		( sReleaseContinueData *inData );
	SInt32			DoPlugInCustomCall		( sDoPlugInCustomCall *inData );
	
	void			SystemGoingToSleep		( void );
	void			SystemWillPowerOn			( void );

	SInt32			GetNextNodeRef			(	tDirNodeReference inNodeRef,
												tDirNodeReference *outNodeRef,
												sSearchContextData *inContext );
	tDataList	   *GetNodePath				(	tDirNodeReference inNodeRef,
												sSearchContextData *inContext );

	SInt32			AddDataToOutBuff		(	sSearchContinueData *inContinue,
												CBuff *inOutBuff,
												sSearchContextData *inContext,
												tDataListPtr inRequestedAttrList );

	SInt32			CheckSearchPolicyChange	(	sSearchContextData *pContext,
												tDirNodeReference inNodeRef,
												tContextData inContinueData );
	bool			IsAugmented				(	sSearchContextData *inContext, 
												tDirNodeReference inNodeRef );
	void			UpdateContinueForAugmented	(	sSearchContextData *inContext,
													sSearchContinueData *inContinue,
													tDataListPtr inAttrTypeRequestList );

	sSearchContextData*	MakeContextData		( void );

	sSearchList*	DupSearchListWithNewRefs( sSearchList *inSearchList );
	sSearchList*	BuildNetworkNodeList			( void );
#if AUGMENT_RECORDS
	SInt32			CheckForAugmentConfig			( tDirPatternMatch policyToCheck );
	CFDictionaryRef FindAugmentConfigRecord			( tDirPatternMatch nodeType );
#endif
	
	sSearchConfig	   *pSearchConfigList;		//would like to block on access to this list
												//KW should make use of CFMutableArray or STL type?
	DSMutexSemaphore	fMutex;

	tDirReference		fDirRef;
	UInt32				fState;
	CFStringRef			fLZMACAddress;
	CFStringRef			fNLZMACAddress;
	char			   *fAuthSearchPathCheck;
	bool				fSomeNodeFailedToOpen;
	
#if AUGMENT_RECORDS
	tDirNodeReference	fAugmentNodeRef;
#endif
};

#endif	// __CSearchPlugin_H__
