// ======================================================================
//	File:		KCAPI_CString.cpp
//
//	Operation classes for KC Manager APIs that use "C" strings
//		- kcunlock
//		- kccreatekeychain
//		- kcgetkeychainname
//		- kcaddapplesharepassword
//		- kcfindapplesharepassword
//		- kcaddinternetpassword
//		- kcaddinternetpasswordwithpath
//		- kcfindinternetpassword
//		- kcfindinternetpasswordwithpath
//		- kcaddgenericpassword
//		- kcfindgenericpassword
//
//
//	Copyright:	Copyright (c) 2000,2003,2006 Apple Computer, Inc. All Rights Reserved.
//
//	Change History (most recent first):
//
//		 <1>	3/1/00	em		Created.
// ======================================================================
#include "KCAPI_CString.h"
#include "KCParamUtility.h"

#if TARGET_RT_MAC_MACHO
	#include <OSServices/KeychainCore.h>
	#include <OSServices/KeychainCorePriv.h>
	#include <SecurityHI/KeychainHI.h>
#else
	#include <Keychain.h>
#endif
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_kcunlock
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_kcunlock::COp_kcunlock()
    :mPassword("Password")
{
	AddParam(mKeychainIndex);
	AddParam(mPassword);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_kcunlock::Operate()
{
	mStatus = ::kcunlock(
					GetKeychain(), 
					(const char *)((StringPtr)mPassword+1));
	return(mStatus);
}

#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_kccreatekeychain
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_kccreatekeychain::COp_kccreatekeychain()
	:mPassword("Password")
{
	AddParam(mPassword);
	AddResult(mKeychainIndex);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_kccreatekeychain::Operate()
{
	KCRef	aKeychain = NULL;
	mStatus = ::kccreatekeychain(
					(const char *)((StringPtr)mPassword+1),
					(KCRef*)&aKeychain);
	AddKeychain(aKeychain);
	return(mStatus);
}

#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_kcgetkeychainname
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_kcgetkeychainname::COp_kcgetkeychainname()
	:mKeychainName("KeychainName")
{
	AddParam(mKeychainIndex);
	AddResult(mKeychainName);
}
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_kcgetkeychainname::Operate()
{	
	mStatus = ::kcgetkeychainname(
					GetKeychain(),
					(char*)((StringPtr)mKeychainName+1));
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_kcaddapplesharepassword
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_kcaddapplesharepassword::COp_kcaddapplesharepassword()
    :mServerSignature("ServerSignature"), 
    mServerAddress("ServerAddress"),
    mServerName("ServerName"), 
    mVolumeName("VolumeName"), 
    mAccountName("AccountName"), 
    mPassword("Password") 
{
    AddParam(mServerSignature);
    AddParam(mServerAddress);
    AddParam(mServerName);
    AddParam(mVolumeName);
    AddParam(mAccountName);
    AddParam(mPassword);
    
    AddResult(mItemIndex);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_kcaddapplesharepassword::Operate()
{
	KCItemRef				aItem = NULL;
	mStatus = ::kcaddapplesharepassword(
                        &static_cast<AFPServerSignatureStruct>(mServerSignature).data,
						(const char *)((StringPtr)mServerAddress+1),
						(const char *)((StringPtr)mServerName+1),
						(const char *)((StringPtr)mVolumeName+1),
						(const char *)((StringPtr)mAccountName+1),
                        (UInt32)((kcBlob*)mPassword)->length,
                        (const void *)((kcBlob*)mPassword)->data,
                        (KCItemRef*)&aItem);
	AddItem(aItem);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_kcfindapplesharepassword
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_kcfindapplesharepassword::COp_kcfindapplesharepassword()
    :mServerSignature("ServerSignature"), 
    mServerAddress("ServerAddress"),
    mServerName("ServerName"), 
    mVolumeName("VolumeName"), 
    mAccountName("AccountName"), 
    mPassword("Password"), 
    mActualLength("ActualLength")
{
    AddParam(mServerSignature);
    AddParam(mServerAddress);
    AddParam(mServerName);
    AddParam(mVolumeName);
    AddParam(mAccountName);
    AddParam(mPassword);
	
    AddResult(mPassword);
    AddResult(mActualLength);
    AddResult(mItemIndex);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_kcfindapplesharepassword::Operate()
{
	KCItemRef				aItem = NULL;
	mStatus = ::kcfindapplesharepassword(
					&static_cast<AFPServerSignatureStruct>(mServerSignature).data,
					(const char *)((StringPtr)mServerAddress+1),
					(const char *)((StringPtr)mServerName+1),
					(const char *)((StringPtr)mVolumeName+1),
					(const char *)((StringPtr)mAccountName+1),
					(UInt32)((kcBlob*)mPassword)->length,
					(void *)((kcBlob*)mPassword)->data,
					(UInt32*)mActualLength,
					(KCItemRef *)&aItem);
	AddItem(aItem);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_kcaddinternetpassword
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_kcaddinternetpassword::COp_kcaddinternetpassword()
	:mServerName("ServerName"),
	mSecurityDomain("SecurityDomain"),
	mAccountName("AccountName"),
	mPort("Port"),
	mProtocol("Protocol"),
	mAuthType("AuthType"),
	mPassword("Password")
{
    AddParam(mServerName);
    AddParam(mSecurityDomain);
    AddParam(mAccountName);
    AddParam(mPort);
    AddParam(mProtocol);
    AddParam(mAuthType);
    AddParam(mPassword);

    AddResult(mItemIndex);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_kcaddinternetpassword::Operate()
{
	KCItemRef	aItem = NULL;
	mStatus = ::kcaddinternetpassword(
						(const char *)((StringPtr)mServerName+1),
						(const char *)((StringPtr)mSecurityDomain+1),
						(const char *)((StringPtr)mAccountName+1),
						(UInt16)mPort,
						(OSType)mProtocol,
						(OSType)mAuthType,
						(UInt32)((kcBlob*)mPassword)->length,
						(const void *)((kcBlob*)mPassword)->data,
						(KCItemRef *)&aItem);

	AddItem(aItem);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_kcaddinternetpasswordwithpath
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_kcaddinternetpasswordwithpath::COp_kcaddinternetpasswordwithpath()
	:mServerName("ServerName"),
	mSecurityDomain("SecurityDomain"),
	mAccountName("AccountName"),
	mPath("Path"),
	mPort("Port"),
	mProtocol("Protocol"),
	mAuthType("AuthType"),
	mPassword("Password")
{
    AddParam(mServerName);
    AddParam(mSecurityDomain);
    AddParam(mAccountName);
    AddParam(mPath);
    AddParam(mPort);
    AddParam(mProtocol);
    AddParam(mAuthType);
    AddParam(mPassword);

    AddResult(mItemIndex);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_kcaddinternetpasswordwithpath::Operate()
{
	KCItemRef	aItem = NULL;
	mStatus = ::kcaddinternetpasswordwithpath(
							(const char *)((StringPtr)mServerName+1),
							(const char *)((StringPtr)mSecurityDomain+1),
							(const char *)((StringPtr)mAccountName+1),
							(const char *)((StringPtr)mPath+1),
							(UInt16)mPort,
							(OSType)mProtocol,
							(OSType)mAuthType,
							(UInt32)((kcBlob*)mPassword)->length,
							(const void *)((kcBlob*)mPassword)->data,
							(KCItemRef *)&aItem);

	AddItem(aItem);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_kcfindinternetpassword
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_kcfindinternetpassword::COp_kcfindinternetpassword()
	:mServerName("ServerName"),
	mSecurityDomain("SecurityDomain"),
	mAccountName("AccountName"),
	mPort("Port"),
	mProtocol("Protocol"),
	mAuthType("AuthType"),
	mPassword("Password"),
	mActualLength("ActualLength")
{
    AddParam(mServerName);
    AddParam(mSecurityDomain);
    AddParam(mAccountName);
    AddParam(mPort);
    AddParam(mProtocol);
    AddParam(mAuthType);
    AddParam(mPassword);
	
    AddResult(mPassword);
    AddResult(mActualLength);
    AddResult(mItemIndex);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_kcfindinternetpassword::Operate()
{
	KCItemRef	aItem = NULL;
	mStatus = ::kcfindinternetpassword(
							(const char *)((StringPtr)mServerName+1),
							(const char *)((StringPtr)mSecurityDomain+1),
							(const char *)((StringPtr)mAccountName+1),
							(UInt16)mPort,
							(OSType)mProtocol,
							(OSType)mAuthType,
							(UInt32)((kcBlob*)mPassword)->length,
							(void *)((kcBlob*)mPassword)->data,
							(UInt32*)mActualLength,
							(KCItemRef*)&aItem);
	AddItem(aItem);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_kcfindinternetpasswordwithpath
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_kcfindinternetpasswordwithpath::COp_kcfindinternetpasswordwithpath()
	:mServerName("ServerName"),
	mSecurityDomain("SecurityDomain"),
	mAccountName("AccountName"),
	mPath("Path"),
	mPort("Port"),
	mProtocol("Protocol"),
	mAuthType("AuthType"),
	mPassword("Password"),
	mActualLength("ActualLength")
{
    AddParam(mServerName);
    AddParam(mSecurityDomain);
    AddParam(mAccountName);
    AddParam(mPath);
    AddParam(mPort);
    AddParam(mProtocol);
    AddParam(mAuthType);
    AddParam(mPassword);
	
    AddResult(mPassword);
    AddResult(mActualLength);
    AddResult(mItemIndex);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_kcfindinternetpasswordwithpath::Operate()
{
	KCItemRef	aItem = NULL;
	mStatus = ::kcfindinternetpasswordwithpath(
							(const char *)((StringPtr)mServerName+1),
							(const char *)((StringPtr)mSecurityDomain+1),
							(const char *)((StringPtr)mAccountName+1),
							(const char *)((StringPtr)mPath+1),
							(UInt16)mPort,
							(OSType)mProtocol,
							(OSType)mAuthType,
							(UInt32)((kcBlob*)mPassword)->length,
							(void *)((kcBlob*)mPassword)->data,
							(UInt32*)mActualLength,
							(KCItemRef*)&aItem);
	AddItem(aItem);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_kcaddgenericpassword
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_kcaddgenericpassword::COp_kcaddgenericpassword()
	:mServiceName("ServiceName"),
	mAccountName("AccountName"),
	mPassword("Password")
{
    AddParam(mServiceName);
    AddParam(mAccountName);
    AddParam(mPassword);
	
	AddResult(mItemIndex);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_kcaddgenericpassword::Operate()
{
	KCItemRef	aItem = NULL;
	mStatus = ::kcaddgenericpassword(
						(const char *)((StringPtr)mServiceName+1),
						(const char *)((StringPtr)mAccountName+1),
						(UInt32)((kcBlob*)mPassword)->length,
						(const void *)((kcBlob*)mPassword)->data,
						(KCItemRef*)&aItem);
	AddItem(aItem);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_kcfindgenericpassword
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_kcfindgenericpassword::COp_kcfindgenericpassword()
	:mServiceName("ServiceName"),
	mAccountName("AccountName"),
	mPassword("Password"),
	mActualLength("ActualLength")
{
    AddParam(mServiceName);
    AddParam(mAccountName);
    AddParam(mPassword);
	
    AddResult(mPassword);
    AddResult(mActualLength);
	AddResult(mItemIndex);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_kcfindgenericpassword::Operate()
{
	KCItemRef	aItem = NULL;
	mStatus = ::kcfindgenericpassword(
						(const char *)((StringPtr)mServiceName+1),
						(const char *)((StringPtr)mAccountName+1),
                        (UInt32)((kcBlob*)mPassword)->length,
                        (void *)((kcBlob*)mPassword)->data,
                        (UInt32*)mActualLength,
                        (KCItemRef*)&aItem);
	AddItem(aItem);
	return(mStatus);
}
