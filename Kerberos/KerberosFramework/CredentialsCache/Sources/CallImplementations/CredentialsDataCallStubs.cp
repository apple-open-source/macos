/*
 * CCICredentialsDataCallStubs.cp
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/CallImplementations/CredentialsDataCallStubs.cp,v 1.3 2000/12/09 21:34:01 meeroh Exp $
 */

#include "CCache.config.h"

#include "CCICredentialsDataCallStubs.h"
#include "CCICredentialsData.h"

CCICredentialsDataCallStub::CCICredentialsDataCallStub (
	CCIUniqueID	inCredentials,
	CCIInt32	inAPIVersion,
	bool		inInitialize):
	CCICredentials (inCredentials, inAPIVersion) {
	
	if (inInitialize)
		Initialize ();
}

CCICredentialsDataCallStub::~CCICredentialsDataCallStub () {
}

bool CCICredentialsDataCallStub::Compare (
	const CCICredentials&		inCompareTo) const {
	return CCICredentialsDataInterface (CCICredentials::GetCredentialsID ()) -> Compare (inCompareTo.GetCredentialsID ());
}

CCIUInt32
CCICredentialsDataCallStub::GetCredentialsVersion () {
	return CCICredentialsDataInterface (CCICredentials::GetCredentialsID ()) -> GetVersion ();
}

void
CCICredentialsDataCallStub::CopyV4Credentials (
	cc_credentials_v4_t&		outCredentials) const {
	CCICredentialsDataInterface (CCICredentials::GetCredentialsID ()) -> CopyV4Credentials (outCredentials);
}

void
CCICredentialsDataCallStub::CopyV5Credentials (
	cc_credentials_v5_t&		outCredentials) const {
	CCICredentialsDataInterface (CCICredentials::GetCredentialsID ()) -> CopyV5Credentials (outCredentials);
}

#if CCache_v2_compat
void
CCICredentialsDataCallStub::CompatCopyV4Credentials (
	cc_credentials_v4_compat&		outCredentials) const {
	CCICredentialsDataInterface (CCICredentials::GetCredentialsID ()) -> CompatCopyV4Credentials (outCredentials);
}

void
CCICredentialsDataCallStub::CompatCopyV5Credentials (
	cc_credentials_v5_compat&		outCredentials) const {
	CCICredentialsDataInterface (CCICredentials::GetCredentialsID ()) -> CompatCopyV5Credentials (outCredentials);
}
#endif
