/*
 * CCICCacheDataCallStubs.cp
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/CallImplementations/CCacheDataCallStubs.cp,v 1.5 2001/09/24 16:32:38 meeroh Exp $
 */

#include "CCache.config.h"

#include "CCICCacheDataCallStubs.h"
#include "CCICCacheData.h"

CCICCacheDataCallStub::CCICCacheDataCallStub (
	CCIUniqueID	inCCache,
	CCIInt32	inAPIVersion):
	CCICCache (inCCache, inAPIVersion) {
}

CCICCacheDataCallStub::~CCICCacheDataCallStub () {
}

void
CCICCacheDataCallStub::Destroy () {
	CCICCacheDataInterface (GetCCacheID ()) -> Destroy ();
}

void
CCICCacheDataCallStub::SetDefault () {
	CCICCacheDataInterface (GetCCacheID ()) -> SetDefault ();
}

CCIUInt32
CCICCacheDataCallStub::GetCredentialsVersion () {
	return CCICCacheDataInterface (GetCCacheID ()) -> GetCredentialsVersion ();
}

std::string
CCICCacheDataCallStub::GetPrincipal (
	CCIUInt32				inVersion) {
	
	return CCICCacheDataInterface (GetCCacheID ()) -> GetPrincipal (inVersion);
}
	
std::string
CCICCacheDataCallStub::GetName () {
	
	return CCICCacheDataInterface (GetCCacheID ()) -> GetName ();
}
	
void
CCICCacheDataCallStub::SetPrincipal (
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {
	
	CCICCacheDataInterface (GetCCacheID ()) -> SetPrincipal (inVersion, inPrincipal);
}
	
#if CCache_v2_compat
void
CCICCacheDataCallStub::CompatSetPrincipal (
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {
	
	CCICCacheDataInterface (GetCCacheID ()) -> CompatSetPrincipal (inVersion, inPrincipal);
}
#endif	

void
CCICCacheDataCallStub::StoreConvertedCredentials (
	const cc_credentials_union*		inCredentials) {
	
	CCICCacheDataInterface (GetCCacheID ()) -> StoreCredentials (inCredentials);
}

void
CCICCacheDataCallStub::StoreFlattenedCredentials (
	std::strstream&					inCredentials) {
	
	CCICCacheDataInterface (GetCCacheID ()) -> StoreCredentials (new CCICredentialsData (inCredentials));
}


#if CCache_v2_compat
void
CCICCacheDataCallStub::CompatStoreConvertedCredentials (
	const cred_union&		inCredentials) {
	
	CCICCacheDataInterface (GetCCacheID ()) -> CompatStoreCredentials (inCredentials);
}

void
CCICCacheDataCallStub::CompatStoreFlattenedCredentials (
	std::strstream&					inCredentials) {
	
	CCICCacheDataInterface (GetCCacheID ()) -> StoreCredentials (new CCICredentialsData (inCredentials));
}


#endif
	
void		
CCICCacheDataCallStub::RemoveCredentials (
	const CCICredentials&	inCredentials) {
	
	CCICCacheDataInterface (GetCCacheID ()) -> RemoveCredentials (inCredentials.GetCredentialsID ());
}
	
CCITime		
CCICCacheDataCallStub::GetLastDefaultTime () {
	
	return CCICCacheDataInterface (GetCCacheID ()) -> GetLastDefaultTime ();
}

CCITime		
CCICCacheDataCallStub::GetChangeTime () {
	
	return CCICCacheDataInterface (GetCCacheID ()) -> GetChangeTime ();
}

void		
CCICCacheDataCallStub::Move (
	CCICCache&		inCCache) {
	
	CCICCacheDataInterface (GetCCacheID ()) -> Move (inCCache.GetCCacheID ());
}

CCILockID		
CCICCacheDataCallStub::Lock () {
	
	return CCICCacheDataInterface (GetCCacheID ()) -> Lock ();
}

void		
CCICCacheDataCallStub::Unlock (
	CCILockID					inLock) {
	
	CCICCacheDataInterface (GetCCacheID ()) -> Unlock (inLock);
}

bool CCICCacheDataCallStub::Compare (
	const CCICCache&		inCompareTo) const {
	return CCICCacheDataInterface (GetCCacheID ()) -> Compare (inCompareTo.GetCCacheID ());
}

void		
CCICCacheDataCallStub::GetCredentialsIDs (
	std::vector <CCIObjectID>&	outCredenitalsIDs) const {
	
	CCICCacheDataInterface (GetCCacheID ()) -> GetCredentialsIDs (outCredenitalsIDs);
}