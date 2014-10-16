// ======================================================================
//	File:		KCAPI_Password.cpp
//
//	Operation classes for APIs to store and retriev passwords
//		- KCAddAppleSharePassword
//		- KCFindAppleSharePassword
//		- KCAddInternetPassword
//		- KCAddInternetPasswordWithPath
//		- KCFindInternetPassword
//		- KCFindInternetPasswordWithPath
//		- KCAddGenericPassword
//		- KCFindGenericPassword
//
//
//	Copyright:	Copyright (c) 2000,2003,2006 Apple Computer, Inc. All Rights Reserved.
//
//	Change History (most recent first):
//
//		 <1>	3/1/00	em		Created.
// ======================================================================
#include "KCAPI_Password.h"
#include "KCParamUtility.h"

#if TARGET_RT_MAC_MACHO
	#include <OSServices/KeychainCore.h>
	#include <OSServices/KeychainCorePriv.h>
	#include <SecurityHI/KeychainHI.h>
#else
	#include <Keychain.h>
#endif

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCAddAppleSharePassword
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCAddAppleSharePassword::COp_KCAddAppleSharePassword()
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
COp_KCAddAppleSharePassword::Operate()
{
										// ¥¥¥ store fully-specified AFPXVolMountInfo
										// record as the password data

	KCItemRef	aItem = NULL;
	mStatus = ::KCAddAppleSharePassword(
                        &static_cast<AFPServerSignatureStruct>(mServerSignature).data,
                        (StringPtr)mServerAddress,
                        (StringPtr)mServerName,
                        (StringPtr)mVolumeName,
                        (StringPtr)mAccountName,
                        (UInt32)((kcBlob*)mPassword)->length,
                        (const void *)((kcBlob*)mPassword)->data,
                        (KCItemRef*)&aItem);
	AddItem(aItem);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCFindAppleSharePassword
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCFindAppleSharePassword::COp_KCFindAppleSharePassword()
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
COp_KCFindAppleSharePassword::Operate()
{
	KCItemRef	aItem = NULL;
	mStatus = ::KCFindAppleSharePassword(
					&static_cast<AFPServerSignatureStruct>(mServerSignature).data,
					(StringPtr)mServerAddress,
					(StringPtr)mServerName,
					(StringPtr)mVolumeName,
					(StringPtr)mAccountName,
					(UInt32)((kcBlob*)mPassword)->length,
					(void *)((kcBlob*)mPassword)->data,
					(UInt32*)mActualLength,
					(KCItemRef *)&aItem);
	AddItem(aItem);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCAddInternetPassword
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCAddInternetPassword::COp_KCAddInternetPassword()
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
COp_KCAddInternetPassword::Operate()
{
	KCItemRef	aItem = NULL;
	mStatus = ::KCAddInternetPassword(
						(StringPtr)mServerName,
						(StringPtr)mSecurityDomain,
						(StringPtr)mAccountName,
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
// 	¥ COp_KCAddInternetPasswordWithPath
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCAddInternetPasswordWithPath::COp_KCAddInternetPasswordWithPath()
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
COp_KCAddInternetPasswordWithPath::Operate()
{
	KCItemRef	aItem = NULL;
	mStatus = ::KCAddInternetPasswordWithPath(
							(StringPtr)mServerName,
							(StringPtr)mSecurityDomain,
							(StringPtr)mAccountName,
							(StringPtr)mPath,
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
// 	¥ COp_KCFindInternetPassword
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCFindInternetPassword::COp_KCFindInternetPassword()
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
COp_KCFindInternetPassword::Operate()
{
	KCItemRef	aItem = NULL;
	mStatus = ::KCFindInternetPassword(
							(StringPtr)mServerName,
							(StringPtr)mSecurityDomain,
							(StringPtr)mAccountName,
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
// 	¥ COp_KCFindInternetPasswordWithPath
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCFindInternetPasswordWithPath::COp_KCFindInternetPasswordWithPath()
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
COp_KCFindInternetPasswordWithPath::Operate()
{
	KCItemRef	aItem = NULL;
	mStatus = ::KCFindInternetPasswordWithPath(
							(StringPtr)mServerName,
							(StringPtr)mSecurityDomain,
							(StringPtr)mAccountName,
							(StringPtr)mPath,
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
// 	¥ COp_KCAddGenericPassword
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCAddGenericPassword::COp_KCAddGenericPassword()
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
COp_KCAddGenericPassword::Operate()
{
	KCItemRef	aItem = NULL;
	mStatus = ::KCAddGenericPassword(
                        (StringPtr)mServiceName,
                        (StringPtr)mAccountName,
                        (UInt32)((kcBlob*)mPassword)->length,
                        (const void *)((kcBlob*)mPassword)->data,
                        (KCItemRef*)&aItem);
	
	AddItem(aItem);
	return(mStatus);
}
#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCFindGenericPassword
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCFindGenericPassword::COp_KCFindGenericPassword()
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
COp_KCFindGenericPassword::Operate()
{
	KCItemRef	aItem = NULL;
	mStatus = ::KCFindGenericPassword(
                        (StringPtr)mServiceName,
                        (StringPtr)mAccountName,
                        (UInt32)((kcBlob*)mPassword)->length,
                        (void *)((kcBlob*)mPassword)->data,
                        (UInt32*)mActualLength,
                        (KCItemRef*)&aItem);
						
	AddItem(aItem);
	return(mStatus);
}
