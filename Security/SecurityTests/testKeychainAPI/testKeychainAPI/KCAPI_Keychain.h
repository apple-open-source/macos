// ======================================================================
//	File:		KCAPI_Keychain.h
//
//	Operation classes for core KC APIs:
//		- KCMakeKCRefFromFSRef
//		- KCMakeKCRefFromFSSpec
//		- KCMakeKCRefFromAlias
//		- KCMakeAliasFromKCRef
//		- KCReleaseKeychain
//		- KCUnlockNoUI
//		- KCUnlock
//		- KCLogin
//		- KCChangeLoginPassword
//		- KCLogout
//		- KCUnlockWithInfo
//		- KCLock
//		- KCLockNoUI
//		- KCGetDefaultKeychain
//		- KCSetDefaultKeychain
//		- KCCreateKeychain
//		- KCCreateKeychainNoUI
//		- KCGetStatus
//		- KCChangeSettingsNoUI
//		- KCGetKeychain
//		- KCGetKeychainName
//		- KCChangeSettings
//		- KCCountKeychains
//		- KCGetIndKeychain
//		- KCAddCallback
//		- KCRemoveCallback
//		- KCSetInteractionAllowed
//		- KCIsInteractionAllowed
//
//	Copyright:	Copyright (c) 2000,2003 Apple Computer, Inc. All Rights Reserved.
//
//	Change History (most recent first):
//
//		 <1>	2/25/00	em		Created.
// ======================================================================
#ifndef __KCAPI_KEYCHAIN__
#define __KCAPI_KEYCHAIN__
#include "KCOperation.h"
#include "KCOperationID.h"
#include "KCParamUtility.h"

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCMakeKCRefFromFSRef
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCMakeKCRefFromFSRef : public KCOperation
{
public:
OPERATION_ID(KCMakeKCRefFromFSRef)
								COp_KCMakeKCRefFromFSRef();
	virtual	OSStatus			Operate();

protected:
	CParamFSRef					mFSRef;
private:
	OSStatus					KCMakeKCRefFromFSRef(
									FSRef *inKeychainFSRef,
									KCRef *outKeychain)
								{	
									*outKeychain = (KCRef)NULL;
									return noErr;
								}	
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCMakeKCRefFromFSSpec
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCMakeKCRefFromFSSpec : public KCOperation
{
public:
OPERATION_ID(KCMakeKCRefFromFSSpec)
								COp_KCMakeKCRefFromFSSpec();						
	virtual	OSStatus			Operate();
protected:
	CParamFSSpec				mKeychainFile;
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCMakeKCRefFromAlias
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCMakeKCRefFromAlias : public KCOperation
{
public:
OPERATION_ID(KCMakeKCRefFromAlias)
								COp_KCMakeKCRefFromAlias();						
	virtual	OSStatus			Operate();
protected:
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCMakeAliasFromKCRef
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCMakeAliasFromKCRef : public KCOperation
{
public:
OPERATION_ID(KCMakeAliasFromKCRef)
								
								COp_KCMakeAliasFromKCRef();
	virtual	OSStatus			Operate();
protected:
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCReleaseKeychain
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCReleaseKeychain : public KCOperation
{
public:
OPERATION_ID(KCReleaseKeychain)
								
								COp_KCReleaseKeychain();
	virtual	OSStatus			Operate();
protected:
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCUnlockNoUI
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCUnlockNoUI : public KCOperation
{
public:
OPERATION_ID(KCUnlockNoUI)
								
								COp_KCUnlockNoUI();
	virtual	OSStatus			Operate();
    virtual StringPtr			GetPassword(){ return (StringPtr)mPassword; }
protected:
    CParamStringPtr				mPassword;
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCUnlock
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCUnlock : public KCOperation
{
public:
OPERATION_ID(KCUnlock)
								
								COp_KCUnlock();
	virtual	OSStatus			Operate();
protected:
	CParamStringPtr				mPassword;
};	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCChangeLoginPassword
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCChangeLoginPassword : public KCOperation
{
public:
OPERATION_ID(KCChangeLoginPassword)
								
								COp_KCChangeLoginPassword();
	virtual	OSStatus			Operate();
protected:
	CParamStringPtr				mOldPassword;
	CParamStringPtr				mNewPassword;
};	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCLogin
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCLogin : public KCOperation
{
public:
OPERATION_ID(KCLogin)
								
								COp_KCLogin();
	virtual	OSStatus			Operate();
protected:
	CParamStringPtr				mName;
	CParamStringPtr				mPassword;
};	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCLogout
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCLogout : public KCOperation
{
public:
OPERATION_ID(KCLogout)
								
								COp_KCLogout();
	virtual	OSStatus			Operate();
protected:
};	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCUnlockWithInfo
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCUnlockWithInfo : public KCOperation
{
public:
OPERATION_ID(KCUnlockWithInfo)
								COp_KCUnlockWithInfo();
	virtual	OSStatus			Operate();
protected:
	CParamStringPtr				mPassword;
	CParamStringPtr				mMessage;
};										
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCLock
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCLock : public KCOperation
{
public:
OPERATION_ID(KCLock)
								COp_KCLock();
	virtual	OSStatus			Operate();
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCLockNoUI
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
/*
class COp_KCLockNoUI : public KCOperation
{
public:
OPERATION_ID(KCLockNoUI)
								COp_KCLockNoUI();
	virtual	OSStatus			Operate();
protected:
};
*/										
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCGetDefaultKeychain
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCGetDefaultKeychain : public KCOperation
{
public:
OPERATION_ID(KCGetDefaultKeychain)
								COp_KCGetDefaultKeychain();
	virtual	OSStatus			Operate();
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCSetDefaultKeychain
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCSetDefaultKeychain : public KCOperation
{
public:
OPERATION_ID(KCSetDefaultKeychain)
								COp_KCSetDefaultKeychain();
	virtual	OSStatus			Operate();
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCCreateKeychain
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCCreateKeychain : public KCOperation
{
public:
OPERATION_ID(KCCreateKeychain)
								COp_KCCreateKeychain();
	virtual	OSStatus			Operate();
protected:
	CParamStringPtr				mPassword;
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCCreateKeychainNoUI
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCCreateKeychainNoUI : public KCOperation
{
public:
OPERATION_ID(KCCreateKeychainNoUI)
								COp_KCCreateKeychainNoUI();
	virtual	OSStatus			Operate();
    
    virtual StringPtr			GetPassword(){ return (StringPtr)mPassword; }
    virtual	KCRef *				GetKeychainInCallback(){ return &mKeychainInCallback; }
protected:
    CParamStringPtr				mPassword;
	KCRef						mKeychainInCallback;
    static OSStatus				Callback(
                                    KCRef			*outKeychain, 
                                    StringPtr		*outPassword, 
                                    void			*inContext);
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCGetStatus
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCGetStatus : public KCOperation
{
public:
OPERATION_ID(KCGetStatus)
								COp_KCGetStatus();
	virtual	OSStatus			Operate();
protected:
	CParamUInt32				mKeychainStatus;
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCChangeSettingsNoUI
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCChangeSettingsNoUI : public KCOperation
{
public:
OPERATION_ID(KCChangeSettingsNoUI)
									COp_KCChangeSettingsNoUI();
	virtual	OSStatus				Operate();
	
#if TARGET_RT_MAC_MACHO
	virtual KCChangeSettingsInfo	
                *GetChangeSettingsInfoPtr(){ return &mChangeSettingsInfo; }
#endif

protected:
#if TARGET_RT_MAC_MACHO
    static OSStatus				Callback(
                                    KCChangeSettingsInfo	*outSettings, 
                                    void					*inContext);
#endif

	CParamBoolean				mLockOnSleep;
	CParamBoolean				mUseKCGetDataSound;
	CParamBoolean				mUseKCGetDataAlert;
	CParamBoolean				mUseLockInterval;
	CParamUInt32				mLockInterval;
	CParamStringPtr				mNewPassword;
	CParamStringPtr				mOldPassword;

#if TARGET_RT_MAC_MACHO
	KCChangeSettingsInfo		mChangeSettingsInfo;
#endif
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCGetKeychain
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCGetKeychain : public KCItemOperation
{
public:
OPERATION_ID(KCGetKeychain)
								COp_KCGetKeychain();
	virtual	OSStatus			Operate();
protected:
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCGetKeychainName
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCGetKeychainName : public KCOperation
{
public:
OPERATION_ID(KCGetKeychainName)
								COp_KCGetKeychainName();
	virtual	OSStatus			Operate();
protected:
	CParamStringPtr				mKeychainName;
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCChangeSettings
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCChangeSettings : public KCOperation
{
public:
OPERATION_ID(KCChangeSettings)
								COp_KCChangeSettings();
	virtual	OSStatus			Operate();
protected:
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCCountKeychains
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCCountKeychains : public KCOperation
{
public:
OPERATION_ID(KCCountKeychains)
								COp_KCCountKeychains();
	virtual	OSStatus			Operate();

protected:
	CParamUInt16				mCount;
};

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCGetIndKeychain
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCGetIndKeychain : public KCOperation
{
public:
OPERATION_ID(KCGetIndKeychain)
								COp_KCGetIndKeychain();
	virtual	OSStatus			Operate();

protected:
	CParamUInt16				mIndex;
};

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCAddCallback
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCAddCallback : public KCOperation
{
public:
OPERATION_ID(KCAddCallback)
								COp_KCAddCallback();
	virtual	OSStatus			Operate();

protected:
	CParamUInt16				mEvent;
	static UInt32				sCounter[11];
	static KCCallbackUPP		sCallbacks[11];

#define KCADDCALLBACK_DEF(N) \
	static OSStatus				Callback ## N( \
									KCEvent			inKeychainEvent, \
									KCCallbackInfo	*inInfo, \
									void			*inContext)

	KCADDCALLBACK_DEF(0);
	KCADDCALLBACK_DEF(1);
	KCADDCALLBACK_DEF(2);
	KCADDCALLBACK_DEF(3);
	KCADDCALLBACK_DEF(4);
	KCADDCALLBACK_DEF(5);
	KCADDCALLBACK_DEF(6);
	KCADDCALLBACK_DEF(7);
	KCADDCALLBACK_DEF(8);
	KCADDCALLBACK_DEF(9);
	KCADDCALLBACK_DEF(10);
#undef KCADDCALLBACK_DEF


	friend class COp_KCRemoveCallback;
};

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCRemoveCallback
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCRemoveCallback : public KCOperation
{
public:
OPERATION_ID(KCRemoveCallback)
								COp_KCRemoveCallback();
	virtual	OSStatus			Operate();
protected:
	CParamUInt16				mEvent;
	CParamUInt32				mIdleCount;
	CParamUInt32				mLockCount;
	CParamUInt32				mUnlockCount;
	CParamUInt32				mAddCount;
	CParamUInt32				mDeleteCount;
	CParamUInt32				mUpdateCount;
	CParamUInt32				mChangeIdentityCount;
	CParamUInt32				mFindCount;
	CParamUInt32				mSystemCount;
	CParamUInt32				mDefaultChangedCount;
	CParamUInt32				mDataAccessCount;
};

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCSetInteractionAllowed
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCSetInteractionAllowed : public KCOperation
{
public:
OPERATION_ID(KCSetInteractionAllowed)
								COp_KCSetInteractionAllowed();
	virtual	OSStatus			Operate();
protected:
	CParamBoolean				mAllow;
};
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCIsInteractionAllowed
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCIsInteractionAllowed : public KCOperation
{
public:
OPERATION_ID(KCIsInteractionAllowed)
								COp_KCIsInteractionAllowed();
	virtual	OSStatus			Operate();
protected:
	CParamBoolean				mAllow;
};
#endif	// __KCAPI_KEYCHAIN__
