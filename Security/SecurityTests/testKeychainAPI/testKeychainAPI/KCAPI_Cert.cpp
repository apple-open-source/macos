// ======================================================================
//	File:		KCAPI_Cert.cpp
//
//	Operation classes for APIs for working with Certificates
//			- KCFindX509Certificates
//			- KCChooseCertificate
//
//
//	Copyright:	Copyright (c) 2000,2003,2008 Apple Inc. All Rights Reserved.
//
//	Change History (most recent first):
//
//		 <1>	3/1/00	em		Created.
// ======================================================================

#include "KCAPI_Cert.h"
#include "KCParamUtility.h"

#if TARGET_RT_MAC_MACHO
	#include <OSServices/KeychainCore.h>
	#include <OSServices/KeychainCorePriv.h>
	#include <SecurityHI/KeychainHI.h>
#else
	#include <Keychain.h>
#endif

#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCFindX509Certificates
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCFindX509Certificates::COp_KCFindX509Certificates()
{
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCFindX509Certificates::Operate()
{
#if TARGET_RT_MAC_MACHO
	throw("KCGetDataNoUI is not implemented");
#else
	KCRef					mKeychain = NULL;
	CFStringRef				mName = NULL;
	CFStringRef				mEmailAddress = NULL;
	KCCertSearchOptions		mOptions;
	CFMutableArrayRef		mCertificateItems;

	mStatus = ::KCFindX509Certificates(
					(KCRef)mKeychain,
					(CFStringRef)mName,
					(CFStringRef)mEmailAddress,
					(KCCertSearchOptions)mOptions,
					(CFMutableArrayRef *)&mCertificateItems);
#endif
	return(mStatus);
}

#pragma mark -
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCChooseCertificate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
COp_KCChooseCertificate::COp_KCChooseCertificate()
{
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Operate
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
OSStatus
COp_KCChooseCertificate::Operate()
{
/*
	CFArrayRef				mItems = NULL;
	KCItemRef				mCertificate = NULL;
	CFArrayRef				mPolicyOIDs = NULL;
	KCVerifyStopOn			mStopOn;

	mStatus = ::KCChooseCertificate(
					(CFArrayRef)mItems,
					(KCItemRef *)&mCertificate,
					(CFArrayRef)mPolicyOIDs,
					(KCVerifyStopOn)mStopOn);
*/
printf("WARNING : ChooseCertificate cannot be linked\n");
	return(mStatus);
}
