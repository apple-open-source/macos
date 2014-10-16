// ======================================================================
//	File:		KCOperation.h
//
//	pure virtual base class for performing operations in KeychainLib
//  (based on Dave Akhond's Operation for CDSA
//
//	Copyright:	Copyright (c) 2000,2003,2008 Apple Inc. All Rights Reserved.
//
//	Change History (most recent first):
//
//		 <1>	2/22/00	em		Created.
// ======================================================================

#ifndef __KC_OPERATION__
#define __KC_OPERATION__

#ifdef _CPP_UTILITIES
#pragma export on
#endif

#include <stdio.h>
#include <Carbon/Carbon.h>
#include <vector.h>
#undef check
#if TARGET_RT_MAC_MACHO
	#include <OSServices/KeychainCore.h>
	#include <OSServices/KeychainCorePriv.h>
	#include <SecurityHI/KeychainHI.h>
#else
	#include <Keychain.h>
#endif

#include "testKeychainAPI.h"
#include "KCParamUtility.h"
#include "KCOperationID.h"


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operation
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class Operation {
public:
											// Birth & Death
								Operation():mStatus(noErr){}
	virtual						~Operation(){}
	
	virtual void				SetClient(void *inClient);
	virtual	OSStatus			Operate() = 0;
	virtual eKCOperationID		GetID() = 0;
	
	virtual void				ReadArguments(
                                    FILE			*inFile);
                                    
	virtual void				WriteArguments(
                                    FILE			*inFile);
                                    
	virtual void				WriteResults(
                                    FILE			*inFile);
                                    
	static int					ReadScript(
                                    FILE			*inFile, 
                                    eKCOperationID	&outID);
                                    
	virtual void				ReadScript(
                                    FILE 			*inFile);
                                    
	virtual void				WriteScript(
                                    FILE			*inFile);
                                    
	virtual void				GenerateScript(
                                    FILE			*inFile);

	virtual bool				RunScript(
                                    FILE			*inFile);
                                    
	virtual bool				CompareResults(
									FILE			*inFile);
protected:
	CTestApp					*mClient;
	OSStatus					mStatus;
	tParamList					mParamList;
	tParamList					mResultList;

	virtual void				AddParam(CParam &inParam){ mParamList.push_back(&inParam);}
	virtual void				AddResult(CParam &inParam){ mResultList.push_back(&inParam);}

	    
};

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ KCOperation
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class KCOperation : public Operation {
public:
								KCOperation():mKeychainIndex("KeychainIndex"), mAliasIndex("AliasIndex") {}
	virtual						~KCOperation(){}

	virtual void				AddAlias(AliasHandle alias)
                                { 								
                                    sAliasList.push_back(alias); 
									mAliasIndex = (UInt32)(sAliasList.size()-1);
                                }

	virtual void				AddKeychain(KCRef &inKeychain)
                                { 								
                                    sKCRefList.push_back(inKeychain); 
                                    mKeychainIndex = (UInt32)(sKCRefList.size()-1);
                                }
                                
	virtual AliasHandle			GetAlias()
                                {								
									if((UInt32)mAliasIndex < sAliasList.size())
										return sAliasList[(UInt32)mAliasIndex];
									else
										return NULL;
                                }

	virtual KCRef				GetKeychain()
                                {								
									if((UInt32)mKeychainIndex < sKCRefList.size())
										return sKCRefList[(UInt32)mKeychainIndex];
									else
										return NULL;
                                }

	static	void				Cleanup()
								{
										// ¥¥¥ need to release each keychain first
									sAliasList.clear();
									sKCRefList.clear();
								}
protected:
	class CKeychainIndex : public CUInt32
	{
	public:
		CKeychainIndex &		operator = (const UInt32 &inData){ data = inData; return *this; }
		bool					operator == (const UInt32 inData) const
								{ 
									return (CKCRef(KCOperation::sKCRefList[data]) == KCOperation::sKCRefList[inData]);
								}				
	}; 
	typedef TParam<CKeychainIndex, POD_STRUCT(UInt32), UInt32> CParamKeychainIndex;

	class CAliasIndex : public CUInt32
	{
	public:
		CAliasIndex &		operator = (const UInt32 &inData){ data = inData; return *this; }
		bool					operator == (const UInt32 inData) const
								{ 
									return (CAliasHandle(KCOperation::sAliasList[data]) == KCOperation::sAliasList[inData]);
								}				
	}; 
	typedef TParam<CAliasIndex, POD_STRUCT(UInt32), UInt32> CParamAliasIndex;
	
	CParamKeychainIndex			mKeychainIndex;
	CParamAliasIndex			mAliasIndex;
	static vector<KCRef>		sKCRefList;
	static vector<AliasHandle>	sAliasList;

	friend class CKeychainIndex;
	friend class CAliasIndex;
};


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ KCItemOperation
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class KCItemOperation : public KCOperation {
public:
                                KCItemOperation():mItemIndex("ItemIndex"){}
	virtual						~KCItemOperation(){}

	virtual void				AddItem(KCItemRef &inItem)
                                {
                                    sKCItemRefList.push_back(inItem);
									mItemIndex = (UInt32)(sKCItemRefList.size()-1);
                                }
                                
	virtual KCItemRef			GetItem()
                                {
									if((UInt32)mItemIndex < sKCItemRefList.size())
										return sKCItemRefList[(UInt32)mItemIndex];
									else
										return NULL;
                                }

	static	void				Cleanup()
								{
										// ¥¥¥ need to release each item first
									sKCItemRefList.clear();
								}
protected:
	class CItemIndex : public CUInt32
	{
	public:
		CItemIndex &			operator = (const UInt32 &inData){ data = inData; return *this; }
		bool					operator == (const UInt32 inData) const
								{ 
									return(CKCItemRef(KCItemOperation::sKCItemRefList[data]) == KCItemOperation::sKCItemRefList[inData]);
								}				
	}; 
	typedef TParam<CItemIndex, POD_STRUCT(UInt32), UInt32> CParamItemIndex;
	CParamItemIndex				mItemIndex;
	
	static vector<KCItemRef>	sKCItemRefList;

	friend class CItemIndex;
};

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ KCSearchOperation
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class KCSearchOperation : public KCItemOperation {
public:
                                KCSearchOperation():mSearchIndex("SearchIndex"){}
	virtual						~KCSearchOperation(){}

	virtual void				AddSearch(KCSearchRef &inSearch)
                                { 
                                    sKCSearchRefList.push_back(inSearch); mSearchIndex = (UInt32)(sKCSearchRefList.size()-1); 
                                }
                            
	virtual KCSearchRef			GetSearch()
                                {
									if((UInt32)mSearchIndex < sKCSearchRefList.size())
										return sKCSearchRefList[(UInt32)mSearchIndex];
									else
										return NULL;
                                }

	static	void				Cleanup()
								{
										// ¥¥¥ need to release each ref first
									sKCSearchRefList.clear();
								}
protected:
	class CSearchIndex : public CUInt32
	{
	public:
		CSearchIndex &			operator = (const UInt32 &inData){ data = inData; return *this; }
		bool					operator == (const UInt32 inData) const
								{ 
									return (data == inData) || 
										(KCSearchOperation::sKCSearchRefList[data] == KCSearchOperation::sKCSearchRefList[inData]);
								}				
	}; 
	typedef TParam<CSearchIndex, POD_STRUCT(UInt32), UInt32> CParamSearchIndex;
	CParamSearchIndex			mSearchIndex;
	
	static vector<KCSearchRef>	sKCSearchRefList;

	friend class CSearchIndex;
};

#ifdef _CPP_UTILITIES
#pragma export off
#endif

#endif // __KC_OPERATION__
