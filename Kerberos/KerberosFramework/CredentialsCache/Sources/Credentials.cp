/*
 * CCICredentials.cp
 *
 * Implementation of credentials cache structures for CCache API.
 *
 * Internally, ccaches are kept in a linked list of cci_ccache_data
 * structures. 
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/Credentials.cp,v 1.28 2004/10/22 20:48:27 lxs Exp $
 */
 
#include "Credentials.h"
#include "Pointer.h"
#include "AbstractFactory.h"

#include <Kerberos/krb.h>  // For krb_time_to_life 

const	cc_credentials_f	CCICredentials::sFunctionTable = {
	CCECredentials::Release,
	CCECredentials::Compare
};

// Release credentials
cc_int32 CCECredentials::Release (
	cc_credentials_t			inCredentials) {
	
	CCIResult	result = ccNoError;
	
	CCIBeginSafeTry_ {
		StCredentials			credentials (inCredentials);
		
		delete credentials.Get ();
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidCredentials)
	            || (result == ccErrServerUnavailable));
	
	return result;
}

// Compare credentials
cc_int32 CCECredentials::Compare (
	cc_credentials_t	inCredentials,
	cc_credentials_t	inCompareTo,
	cc_uint32*			outEqual) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCredentials							credentials (inCredentials);
		StCredentials							compareTo (inCompareTo);
		StPointer <cc_uint32>					equal (outEqual);
		equal = credentials -> Compare (*compareTo.Get ());

	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidContext)
	            || (result == ccErrCredentialsNotFound)
	            || (result == ccErrNoMem)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Create new credentials, initializing data to nil. 
// You have to call Initialize to setup the data properly.
CCICredentials::CCICredentials (
	const CCIUniqueID&	inCredentials,
	CCIInt32			inAPIVersion):
	mCredentialsID (inCredentials),
	mAPIVersion (inAPIVersion) {
	
	GetPublicData ().data = NULL;
}

// Fill out mCredentials appropriately
void CCICredentials::Initialize () {
	GetPublicData ().data = &mCredentials;

	CCIAssert_ ((GetCredentialsVersion () == cc_credentials_v4) ||
		(GetCredentialsVersion () == cc_credentials_v5));
	if (GetCredentialsVersion () == cc_credentials_v4) {
		mCredentials.version = cc_credentials_v4;
		mCredentials.credentials.credentials_v4 =
			new CCICredentialsV4 (this, mAPIVersion);
	} else {
		mCredentials.version = cc_credentials_v5;
		mCredentials.credentials.credentials_v5 = 
			new CCICredentialsV5 (this);
	}
}

// Destroy credentials
CCICredentials::~CCICredentials () {
	if (mCredentials.version == cc_credentials_v4) {
		delete static_cast <CCICredentialsV4*> (mCredentials.credentials.credentials_v4);
	}
	if (mCredentials.version == cc_credentials_v5) {
		delete static_cast <CCICredentialsV5*> (mCredentials.credentials.credentials_v5);
	}
}

// Check integrity of credentials
void CCICredentials::Validate () {

	CCIMagic <CCICredentials>::Validate ();
	
	CCIAssert_ ((CCIInternal <CCICredentials, cc_credentials_d>::Valid ()));
}

#pragma mark -
// Retrieve v4 creds from the cache and copy them to a v4 creds struct
CCICredentialsV4::CCICredentialsV4 (
	CCICredentials*			inCredentials,
	CCIInt32				inAPIVersion) {
	
	CCIAssert_ (inCredentials -> GetCredentialsVersion () == cc_credentials_v4);
	
	inCredentials -> CopyV4Credentials (*this);
	if (inAPIVersion < ccapi_version_4) {
		lifetime = krb_time_to_life (issue_date, lifetime + issue_date);
	}
}

// Destroy v4 creds
CCICredentialsV4::~CCICredentialsV4 () {
}

#pragma mark -
// Retrieve v5 creds from the cache and copy them into a v5 creds struct
CCICredentialsV5::CCICredentialsV5 (
	CCICredentials*			inCredentials) {

	client = NULL;
	server = NULL;
	keyblock.data = NULL;
	ticket.data = NULL;
	second_ticket.data = NULL;
	addresses = NULL;
	authdata = NULL;

	try {
		CCIAssert_ (inCredentials -> GetCredentialsVersion () == cc_credentials_v5);
		
		inCredentials -> CopyV5Credentials (*this);
	} catch (...) {
		Cleanup ();
		throw;
	}
}

// Destroy v5 creds
CCICredentialsV5::~CCICredentialsV5 () {
	Cleanup ();
}

// Destroy all the bits and pieces of v5 creds
void
CCICredentialsV5::Cleanup () {
    delete [] client; 
    delete [] server; 
    delete [] (static_cast <char*> (keyblock.data));
    delete [] (static_cast <char*> (ticket.data));
    delete [] (static_cast <char*> (second_ticket.data));

	if (addresses != NULL) {
		for (cc_data** walker = addresses; *walker != NULL; walker++) {
			delete [] (static_cast <char*> ((*walker) -> data));
			delete (*walker);
		}
        delete [] addresses;
	}
    
	if (authdata != NULL) {
		for (cc_data** walker = authdata; *walker != NULL; walker++) {
            delete [] (static_cast <char*> ((*walker) -> data));
			delete (*walker);
		}
        delete [] authdata;
	}
}
	
#pragma mark -
// Same as CCICredentials, except uses CCAPI v2 structs for creds
#if CCache_v2_compat
CCICompatCredentials::CCICompatCredentials (
	const CCIUniqueID&	inCredentials,
	CCIInt32			inAPIVersion):
	
	mCredentialsID (inCredentials),
	mAPIVersion (inAPIVersion)
{
	mCredentials.reset (
            CCIAbstractFactory::GetTheFactory () -> CreateCredentials (
		inCredentials, inAPIVersion, false));
		
	GetPublicData ().cred.pV4Cred = NULL;
	CCIAssert_ ((GetCredentialsVersion () == cc_credentials_v4) ||
		(GetCredentialsVersion () == cc_credentials_v5));
	if (GetCredentialsVersion () == cc_credentials_v4) {
		GetPublicData ().cred_type = CC_CRED_V4;
		GetPublicData ().cred.pV4Cred =
			new CCICompatCredentialsV4 (mCredentials.get (), mAPIVersion);
	} else {
		GetPublicData ().cred_type = CC_CRED_V5;
		GetPublicData ().cred.pV5Cred = 
			new CCICompatCredentialsV5 (mCredentials.get ());
	}
}

CCICompatCredentials::~CCICompatCredentials () {
	if (GetPublicData ().cred_type == CC_CRED_V4) {
		delete static_cast <CCICompatCredentialsV4*> (GetPublicData ().cred.pV4Cred);
	}
	if (GetPublicData ().cred_type == CC_CRED_V5) {
		delete static_cast <CCICompatCredentialsV5*> (GetPublicData ().cred.pV5Cred);
	}
}

void CCICompatCredentials::Validate () {

	CCIMagic <CCICompatCredentials>::Validate ();
}

#pragma mark -
// Same as CCICredentialsV4, but uses CAPI v2 struct
CCICompatCredentialsV4::CCICompatCredentialsV4 (
	CCICredentials*			inCredentials,
	CCIInt32				inAPIVersion) {
	
	CCIAssert_ (inCredentials -> GetCredentialsVersion () == cc_credentials_v4);
	
	inCredentials -> CompatCopyV4Credentials (*this);
	if (inAPIVersion < ccapi_version_4) {
		lifetime = krb_time_to_life (static_cast <cc_uint32> (issue_date), static_cast <cc_uint32> (lifetime + issue_date));
	}
}

CCICompatCredentialsV4::~CCICompatCredentialsV4 () {
}

#pragma mark -
// Same as CCICredentialsV5, but uses CAPI v2 struct
CCICompatCredentialsV5::CCICompatCredentialsV5 (
	CCICredentials*			inCredentials) {
	
	client = NULL;
	server = NULL;
	keyblock.data = NULL;
	ticket.data = NULL;
	second_ticket.data = NULL;
	addresses = NULL;
	authdata = NULL;

	CCIAssert_ (inCredentials -> GetCredentialsVersion () == cc_credentials_v5);
	
	inCredentials -> CompatCopyV5Credentials (*this);
}

CCICompatCredentialsV5::~CCICompatCredentialsV5 () {
	Cleanup ();
}

// Dispose of everything in v5 creds
void
CCICompatCredentialsV5::Cleanup () {
    delete [] client; 
    delete [] server; 
    delete [] (static_cast <char*> (keyblock.data));
    delete [] (static_cast <char*> (ticket.data));
    delete [] (static_cast <char*> (second_ticket.data));

	if (addresses != NULL) {
		for (cc_data** walker = addresses; *walker != NULL; walker++) {
			delete [] (static_cast <char*> ((*walker) -> data));
			delete (*walker);
		}
        delete [] addresses;
	}
    
	if (authdata != NULL) {
		for (cc_data** walker = authdata; *walker != NULL; walker++) {
            delete [] (static_cast <char*> ((*walker) -> data));
			delete (*walker);
		}
        delete [] authdata;
	}
}

#endif
