/*
 * CCIContextDataCallStubs.cp
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/CallImplementations/ContextDataCallStubs.cp,v 1.8 2001/12/12 23:15:30 meeroh Exp $
 */

#include "CCache.config.h"

#include "CredentialsCacheInternal.h"

#include "CCIContextData.h"
#include "CCIContextDataCallStubs.h"

CCIContextDataCallStub::CCIContextDataCallStub (
	CCIUniqueID			inContextID,
	CCIInt32			inAPIVersion):
	CCIContext (inContextID, inAPIVersion) {
}

CCIContextDataCallStub::CCIContextDataCallStub (
	CCIInt32			inAPIVersion):
	CCIContext (GetGlobalContextID (), inAPIVersion) {
}

CCIContextDataCallStub::~CCIContextDataCallStub () {
}

CCITime
CCIContextDataCallStub::GetChangeTime () {

	return CCIContextDataInterface (GetContextID ()) -> GetChangeTime ();
}


CCICCacheData::UniqueID
CCIContextDataCallStub::OpenCCache (
	const std::string&		inCCacheName) {
	
	return CCIContextDataInterface (GetContextID ()) -> GetCCacheID (inCCacheName);
}

CCICCacheData::UniqueID
CCIContextDataCallStub::OpenDefaultCCache () {
	
	return CCIContextDataInterface (GetContextID ()) -> GetDefaultCCacheID ();
}

std::string
CCIContextDataCallStub::GetDefaultCCacheName () {
	
	return CCIContextDataInterface (GetContextID ()) -> GetDefaultCCacheName ();
}

CCICCacheData::UniqueID
CCIContextDataCallStub::CreateCCache (
	const std::string&		inName,
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {
	
	return CCIContextDataInterface (GetContextID ()) -> CreateCCache (inName, inVersion, inPrincipal);
}

CCICCacheData::UniqueID
CCIContextDataCallStub::CreateDefaultCCache (
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {
	
	return CCIContextDataInterface (GetContextID ()) -> CreateDefaultCCache (inVersion, inPrincipal);
}

CCICCacheData::UniqueID
CCIContextDataCallStub::CreateNewCCache (
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {
	
	return CCIContextDataInterface (GetContextID ()) -> CreateNewCCache (inVersion, inPrincipal);
}

void CCIContextDataCallStub::GetCCacheIDs (
		std::vector <CCIObjectID>&		outCCacheIDs) const {

	CCIContextDataInterface (GetContextID ()) -> GetCCacheIDs (outCCacheIDs);
}

CCILockID
CCIContextDataCallStub::Lock () {
	return CCIContextDataInterface (GetContextID ()) -> Lock ();
}

void
CCIContextDataCallStub::Unlock (
	CCILockID		inLock) {
	CCIContextDataInterface (GetContextID ()) -> Unlock (inLock);
}

bool CCIContextDataCallStub::Compare (
	const CCIContext&		inCompareTo) const {
	return CCIContextDataInterface (GetContextID ()) -> Compare (inCompareTo.GetContextID ());
}

CCIUniqueID
CCIContextDataCallStub::GetGlobalContextID () {
	return CCIContextDataInterface::GetGlobalContext () -> GetGloballyUniqueID ();	
}

#if !CCacheMacOSClassicImplementation
cc_int32 __CredentialsCacheInternalInitiateSyncWithYellowCache (void)
{
	return ccNoError;
}

cc_int32 __CredentialsCacheInternalCompleteSyncWithYellowCache (
	const AppleEvent*	/* inAppleEvent */)
{
	return ccNoError;
}

cc_int32 __CredentialsCacheInternalSyncWithYellowCache (
	AEIdleUPP	/* inIdleProc */)
{
	return ccNoError;
}
#endif