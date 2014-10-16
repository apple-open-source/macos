// ======================================================================
//	File:		KCAPI_Keychain.cpp
//
//	Operation classes for core KC APIs:
//		- KCMakeKCRefFromFSRef
//		- KCMakeKCRefFromFSSpec
//		- KCMakeKCRefFromAlias
//		- KCMakeAliasFromKCRef
//		- KCReleaseKeychain
//		- KCUnlockNoUI
//		- KCUnlock
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
#include "KCAPI_Keychain.h"

#if TARGET_RT_MAC_MACHO
	#include <OSServices/KeychainCorePriv.h>
#endif

#include <Carbon/Carbon.h>
#undef check
UInt32	COp_KCAddCallback::sCounter[] = {0,0,0,0,0,0,0,0,0,0,0};

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCMakeKCRefFromFSRef
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCMakeKCRefFromFSRef::COp_KCMakeKCRefFromFSRef()
	:mFSRef("FSRef")
{
	AddParam(mFSRef);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCMakeKCRefFromFSRef::Operate()
{
	throw("KCMakeKCRefFromFSRef is not implemented");
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCMakeKCRefFromFSSpec
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCMakeKCRefFromFSSpec::COp_KCMakeKCRefFromFSSpec()
    :mKeychainFile("KeychainFile")
{
	AddParam(mKeychainFile);
	AddResult(mKeychainIndex);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCMakeKCRefFromFSSpec::Operate()
{
	KCRef	aKeychain = NULL;

	mStatus = ::KCMakeKCRefFromFSSpec(
					(FSSpec*)mKeychainFile, 
					&aKeychain);

	AddKeychain(aKeychain);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCMakeKCRefFromAlias
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCMakeKCRefFromAlias::COp_KCMakeKCRefFromAlias()
{
	AddParam(mAliasIndex);
	AddResult(mKeychainIndex);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCMakeKCRefFromAlias::Operate()
{
	KCRef	aKeychain = NULL;
    AliasHandle alias = GetAlias();
    
	mStatus = ::KCMakeKCRefFromAlias(alias, &aKeychain);
	AddKeychain(aKeychain);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCMakeAliasFromKCRef
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCMakeAliasFromKCRef::COp_KCMakeAliasFromKCRef()
{
	AddParam(mKeychainIndex);
	AddResult(mAliasIndex);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCMakeAliasFromKCRef::Operate()
{
	AliasHandle	alias = GetAlias();
	mStatus = ::KCMakeAliasFromKCRef(GetKeychain(), (AliasHandle*)&alias);
    AddAlias(alias);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCReleaseKeychain
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCReleaseKeychain::COp_KCReleaseKeychain()
{
	AddParam(mKeychainIndex);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCReleaseKeychain::Operate()
{
	KCRef	aKeychain = GetKeychain();
	mStatus = ::KCReleaseKeychain(&aKeychain);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCLogout
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCLogout::COp_KCLogout()
{
}

OSStatus
COp_KCLogout::Operate()
{
    mStatus = ::KCLogout();

	return(mStatus);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCLogin
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCLogin::COp_KCLogin()
	:mName("Name"),
	mPassword("Password")
{
	AddParam(mName);
	AddParam(mPassword);
}

OSStatus
COp_KCLogin::Operate()
{
    mStatus = ::KCLogin( (StringPtr)mName, (StringPtr)mPassword );

	return(mStatus);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCChangeLoginPassword
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCChangeLoginPassword::COp_KCChangeLoginPassword()
	:mOldPassword("OldPassword"),
	mNewPassword("NewPassword")
{
	AddParam(mOldPassword);
	AddParam(mNewPassword);
}

OSStatus
COp_KCChangeLoginPassword::Operate()
{
    mStatus = ::KCChangeLoginPassword( (StringPtr)mOldPassword, (StringPtr)mNewPassword );

	return(mStatus);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCUnlockNoUI
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCUnlockNoUI::COp_KCUnlockNoUI()
	:mPassword("Password")
{
	AddParam(mKeychainIndex);
	AddParam(mPassword);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCUnlockNoUI::Operate()
{
//#if TARGET_RT_MAC_MACHO
//%%%¥¥¥¥¥¥¥	
    KCRef	aKeychain = GetKeychain();
    mStatus = ::KCUnlockNoUI( aKeychain,  (StringPtr)mPassword);
//	throw("KCUnlockNoUI not implemented");
//#else
//	throw("KCUnlockNoUI not implemented");
//#endif
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCUnlock
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCUnlock::COp_KCUnlock()
	:mPassword("Password")
{
	AddParam(mKeychainIndex);
	AddParam(mPassword);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCUnlock::Operate()
{
	mStatus = ::KCUnlock(GetKeychain(), (StringPtr)mPassword);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCUnlockWithInfo
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCUnlockWithInfo::COp_KCUnlockWithInfo()
	:mPassword("Password"), mMessage("Mesage")
{
	AddParam(mKeychainIndex);
	AddParam(mPassword);
	AddParam(mMessage);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCUnlockWithInfo::Operate()
{
#if 0 //TARGET_RT_MAC_MACHO
	mStatus = ::KCUnlockWithInfo(GetKeychain(), (StringPtr)mPassword, (StringPtr)mMessage);
#else
	return unimpErr;
	//throw("KCUnlockWithInfo is not implemented");
#endif
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCLock
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCLock::COp_KCLock()
{
	AddParam(mKeychainIndex);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCLock::Operate()
{
	mStatus = ::KCLock(GetKeychain());
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCLockNoUI
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
/*
COp_KCLockNoUI::COp_KCLockNoUI()
{
	AddParam(mKeychainIndex);
}
*/
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
/*
OSStatus
COp_KCLockNoUI::Operate()
{
#if TARGET_RT_MAC_MACHO
	mStatus = ::KCLockNoUI(GetKeychain());
#else
	throw("KCLockNoUI not implemented");
#endif
	return(mStatus);
}
#pragma mark -
*/
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCGetDefaultKeychain
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCGetDefaultKeychain::COp_KCGetDefaultKeychain()
{
	AddResult(mKeychainIndex);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCGetDefaultKeychain::Operate()
{
	KCRef	aKeychain = NULL;
	mStatus = ::KCGetDefaultKeychain(&aKeychain);
	AddKeychain(aKeychain);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCSetDefaultKeychain
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCSetDefaultKeychain::COp_KCSetDefaultKeychain()
{
	AddParam(mKeychainIndex);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCSetDefaultKeychain::Operate()
{
	mStatus = ::KCSetDefaultKeychain(GetKeychain());
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCCreateKeychain
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCCreateKeychain::COp_KCCreateKeychain()
	:mPassword("Password")
{
	AddParam(mKeychainIndex);
	AddParam(mPassword);
	AddResult(mKeychainIndex);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCCreateKeychain::Operate()
{
	KCRef	aKeychain = GetKeychain();
	mStatus = ::KCCreateKeychain((StringPtr)mPassword, &aKeychain);
	AddKeychain(aKeychain);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCCreateKeychainNoUI
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCCreateKeychainNoUI::COp_KCCreateKeychainNoUI()
    : mPassword("Password")
{
	AddParam(mKeychainIndex);
    AddParam(mPassword);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCCreateKeychainNoUI::Operate()
{
//#if TARGET_RT_MAC_MACHO

	KCRef	aKeychain = GetKeychain();

	mStatus = ::KCCreateKeychainNoUI(aKeychain, (StringPtr)mPassword);
//#else
//	throw("KCCreateKeychainNoUI not implemented");
//#endif
	return(mStatus);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Callback
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCCreateKeychainNoUI::Callback(
    KCRef			*outKeychain, 
    StringPtr		*outPassword, 
    void			*inContext)
{    										
/*    COp_KCCreateKeychainNoUI	*thisObject = static_cast<COp_KCCreateKeychainNoUI*>(inContext);
    if(thisObject == NULL) return -1;
    
//    FSSpec		*aFileSpec = thisObject->GetKeychainFile();
    KCRef		*aKeychain = thisObject->GetKeychainInCallback();
    StringPtr	aPassword = thisObject->GetPassword();
 //   OSStatus	aStatus = ::KCMakeKCRefFromFSSpec(aFileSpec, aKeychain);
    if(aStatus == noErr){
        *outKeychain = *aKeychain;
        *outPassword = aPassword;
    }
	else{
        *outKeychain = NULL;
        *outPassword = NULL;
	}
			
    return aStatus;
*/
return noErr;
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCGetStatus
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCGetStatus::COp_KCGetStatus()
	:mKeychainStatus("KeychainStatus")
{
	AddParam(mKeychainIndex);
	AddResult(mKeychainStatus);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCGetStatus::Operate()
{
	mStatus = ::KCGetStatus(
					GetKeychain(), 
					(UInt32*)mKeychainStatus);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCChangeSettingsNoUI
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCChangeSettingsNoUI::COp_KCChangeSettingsNoUI()
	:mLockOnSleep("LockOnSleep"),
	mUseKCGetDataSound("UseKCGetDataSound"),
	mUseKCGetDataAlert("UseKCGetDataAlert"),
	mUseLockInterval("UseLockInterval"),
	mLockInterval("LockInterval"),
	mNewPassword("NewPassword"),
	mOldPassword("OldPassword")
{
	AddParam(mLockOnSleep);
	AddParam(mUseKCGetDataSound);
	AddParam(mUseKCGetDataAlert);
	AddParam(mUseLockInterval);
	AddParam(mLockInterval);
	AddParam(mNewPassword);
	AddParam(mOldPassword);
	AddParam(mKeychainIndex);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCChangeSettingsNoUI::Operate()
{
#if TARGET_RT_MAC_MACHO

	mChangeSettingsInfo.lockOnSleep = (Boolean)mLockOnSleep;
//	mChangeSettingsInfo.useKCGetDataSound = (Boolean)mUseKCGetDataSound;
//	mChangeSettingsInfo.useKCGetDataAlert = (Boolean)mUseKCGetDataAlert;
	mChangeSettingsInfo.useLockInterval = (Boolean)mUseLockInterval;
	mChangeSettingsInfo.lockInterval = (UInt32)mLockInterval;
//	mChangeSettingsInfo.newPassword = (StringPtr)mNewPassword;
//	mChangeSettingsInfo.oldPassword = (StringPtr)mOldPassword;
	mChangeSettingsInfo.keychain = GetKeychain();
	
//	mStatus = ::KCChangeSettingsNoUI(
//					COp_KCChangeSettingsNoUI::Callback, 
//					this);
	throw("KCChangeSettingsNoUI not implemented");
#else
	throw("KCChangeSettingsNoUI not implemented");
#endif
	return(mStatus);
}

#if TARGET_RT_MAC_MACHO
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Callback
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCChangeSettingsNoUI::Callback(
    KCChangeSettingsInfo	* outSettings, 
    void					* inContext)
{
    COp_KCChangeSettingsNoUI	*thisObject = static_cast<COp_KCChangeSettingsNoUI*>(inContext);
    if(thisObject == NULL) return -1;

										// #2462430 - this should be :
										// 		*outSettings = thisObject->GetSettingsInfoPtr();
										// where KCChangeSettingsInfo **outSettings
										//
	outSettings = thisObject->GetChangeSettingsInfoPtr();
    return noErr;
}
#endif

#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCGetKeychain
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCGetKeychain::COp_KCGetKeychain()
{
	AddParam(mItemIndex);
	AddResult(mKeychainIndex);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCGetKeychain::Operate()
{
	KCRef	aKeychain;
	mStatus = ::KCGetKeychain(
					GetItem(), 
					&aKeychain);
	AddKeychain(aKeychain);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCGetKeychainName
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCGetKeychainName::COp_KCGetKeychainName()
	:mKeychainName("KeychainName")
{
	AddParam(mKeychainIndex);
	AddResult(mKeychainName);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCGetKeychainName::Operate()
{
	mStatus = ::KCGetKeychainName(
					GetKeychain(), 
					(StringPtr)mKeychainName);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCChangeSettings
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCChangeSettings::COp_KCChangeSettings()
{
	AddParam(mKeychainIndex);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCChangeSettings::Operate()
{
	mStatus = ::KCChangeSettings(GetKeychain());
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCCountKeychains
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCCountKeychains::COp_KCCountKeychains()
	:mCount("Count")
{
	AddResult(mCount);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCCountKeychains::Operate()
{
	mStatus = noErr;
	mCount = ::KCCountKeychains();
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCGetIndKeychain
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCGetIndKeychain::COp_KCGetIndKeychain()
	:mIndex("Index")
{
	AddParam(mIndex);
	AddResult(mKeychainIndex);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCGetIndKeychain::Operate()
{
	KCRef	aKeychain = NULL;
	mStatus = ::KCGetIndKeychain(mIndex, &aKeychain);
	AddKeychain(aKeychain);
	return(mStatus);
}
#pragma mark -
KCCallbackUPP	COp_KCAddCallback::sCallbacks[] =
	{	NewKCCallbackUPP(COp_KCAddCallback::Callback0),
		NewKCCallbackUPP(COp_KCAddCallback::Callback1),
		NewKCCallbackUPP(COp_KCAddCallback::Callback2),
		NewKCCallbackUPP(COp_KCAddCallback::Callback3),
		NewKCCallbackUPP(COp_KCAddCallback::Callback4),
		NewKCCallbackUPP(COp_KCAddCallback::Callback5),
		NewKCCallbackUPP(COp_KCAddCallback::Callback6),
		NewKCCallbackUPP(COp_KCAddCallback::Callback7),
		NewKCCallbackUPP(COp_KCAddCallback::Callback8),
		NewKCCallbackUPP(COp_KCAddCallback::Callback9),
		NewKCCallbackUPP(COp_KCAddCallback::Callback10) };
		
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCAddCallback
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCAddCallback::COp_KCAddCallback()
	:mEvent("KCEvent")
{
	AddParam(mEvent);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCAddCallback::Operate()
{
	mStatus = ::KCAddCallback(
					COp_KCAddCallback::sCallbacks[mEvent],
					(KCEventMask)(1 << (KCEvent)mEvent),
					(void *)this);
	return(mStatus);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
#define KCADDCALLBACK(N) \
OSStatus \
COp_KCAddCallback::Callback ## N( \
	KCEvent			inKeychainEvent, \
	KCCallbackInfo	*inInfo, \
	void			*inContext) \
{ \
	COp_KCAddCallback::sCounter[inKeychainEvent]++; \
	return noErr; \
}

KCADDCALLBACK(0)
KCADDCALLBACK(1)
KCADDCALLBACK(2)
KCADDCALLBACK(3)
KCADDCALLBACK(4)
KCADDCALLBACK(5)
KCADDCALLBACK(6)
KCADDCALLBACK(7)
KCADDCALLBACK(8)
KCADDCALLBACK(9)
KCADDCALLBACK(10)

#undef KCADDCALLBACK

#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCRemoveCallback
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCRemoveCallback::COp_KCRemoveCallback()
	:mEvent("KCEvent"),
	mIdleCount("IdleCount"),
	mLockCount("LockCount"),
	mUnlockCount("UnlockCount"),
	mAddCount("AddCount"),
	mDeleteCount("DeleteCount"),
	mUpdateCount("UpdateCount"),
	mChangeIdentityCount("ChangeIdentityCount"),
	mFindCount("FindCount"),
	mSystemCount("SystemCount"),
	mDefaultChangedCount("DefaultChangedCount"),
	mDataAccessCount("DataAccessCount")
{
	AddParam(mEvent);
	
	AddResult(mIdleCount);
	AddResult(mLockCount);
	AddResult(mUnlockCount);
	AddResult(mAddCount);
	AddResult(mDeleteCount);
	AddResult(mUpdateCount);
	AddResult(mChangeIdentityCount);
	AddResult(mFindCount);
	AddResult(mSystemCount);
	AddResult(mDefaultChangedCount);
	AddResult(mDataAccessCount);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCRemoveCallback::Operate()
{
	// Receive a few events so we make sure we get all pending notifications (callbacks)
	EventRecord er;
	for (int ix = 0; ix < 142; ix++)
		GetNextEvent(0, &er);

	mStatus = ::KCRemoveCallback(
					COp_KCAddCallback::sCallbacks[mEvent]);

										// Copy the current results
	UInt16						i = 0;
	tParamList::iterator		aIterator = mResultList.begin();
    CParamUInt32 *				aParam = static_cast<CParamUInt32 *>(*aIterator);
    while(aIterator != mResultList.end()){
    	if(aParam){
			*aParam = COp_KCAddCallback::sCounter[i];
			i++;
		}
        aParam = static_cast<CParamUInt32 *>(*(++aIterator));
    }

										// reset the counter
	COp_KCAddCallback::sCounter[mEvent] = 0;

	return(mStatus);
}

#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCSetInteractionAllowed
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCSetInteractionAllowed::COp_KCSetInteractionAllowed()
	:mAllow("AllowInteraction")
{
	AddParam(mAllow);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCSetInteractionAllowed::Operate()
{
	mStatus = ::KCSetInteractionAllowed(mAllow);
	return(mStatus);
}

#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCIsInteractionAllowed
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCIsInteractionAllowed::COp_KCIsInteractionAllowed()
	:mAllow("AllowInteraction")
{
	AddResult(mAllow);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCIsInteractionAllowed::Operate()
{
	mStatus = noErr;
	mAllow = ::KCIsInteractionAllowed();
	return(mStatus);
}
