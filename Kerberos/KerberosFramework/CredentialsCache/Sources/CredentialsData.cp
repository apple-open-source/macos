/*
 * CCICCacheData.cp
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/CredentialsData.cp,v 1.17 2003/03/17 20:47:53 lxs Exp $
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/CredentialsData.cp,v 1.17 2003/03/17 20:47:53 lxs Exp $
 */

#include "CredentialsData.h"
#include "FlattenCredentials.h"

// Create new credentials from cres structure
CCICredentialsData::CCICredentialsData (
	const cc_credentials_union*		inCredentials):
	CCIUniqueGlobally <CCICredentialsData> (),
	mCredentialsV5 (NULL),
	mCredentialsV4 (NULL) {
	
	CCIAssert_ ((inCredentials -> version == cc_credentials_v4) ||
		(inCredentials -> version == cc_credentials_v5));
	
	if (inCredentials -> version == cc_credentials_v4) {
		mCredentialsV4 = new CCICredentialsV4Data (inCredentials -> credentials.credentials_v4);
	} else {
		mCredentialsV5 = new CCICredentialsV5Data (inCredentials -> credentials.credentials_v5);
	}
}

// Create new credentials from streamed data
CCICredentialsData::CCICredentialsData (
        std::istream&				inStream):
	CCIUniqueGlobally <CCICredentialsData> (),
	mCredentialsV5 (NULL),
	mCredentialsV4 (NULL) {
        
        CCIUInt32	version;
        inStream >> version;
        
	CCIAssert_ ((version == cc_credentials_v4) ||
		(version == cc_credentials_v5));
	
	if (version == cc_credentials_v4) {
		mCredentialsV4 = new CCICredentialsV4Data (inStream);
	} else {
		mCredentialsV5 = new CCICredentialsV5Data (inStream);
	}
}

// Destroy
CCICredentialsData::~CCICredentialsData () {
	if (mCredentialsV4)
		delete mCredentialsV4;
	if (mCredentialsV5)
		delete mCredentialsV5;
}

// Compare for identity (not equality) with other creds
bool
CCICredentialsData::Compare (
	const CCICredentialsData::UniqueID&		inCompareTo) {
	return (GetGloballyUniqueID () == inCompareTo);
}

#if CCache_v2_compat
// Create from v2-style struct
CCICredentialsData::CCICredentialsData (
	const cred_union&				inCredentials):
	CCIUniqueGlobally <CCICredentialsData> (),
	mCredentialsV5 (NULL),
	mCredentialsV4 (NULL) {
	
	CCIAssert_ ((inCredentials.cred_type == CC_CRED_V4) ||
		(inCredentials.cred_type == CC_CRED_V5));
	
	if (inCredentials.cred_type == CC_CRED_V4) {
		mCredentialsV4 = new CCICredentialsV4Data (inCredentials.cred.pV4Cred);
	} else {
		mCredentialsV5 = new CCICredentialsV5Data (inCredentials.cred.pV5Cred);
	}
}
#endif

// Get the creds version
CCIUInt32
CCICredentialsData::GetVersion () const {
	CCIAssert_ (((mCredentialsV4 == NULL) && (mCredentialsV5 != NULL)) || 
		(mCredentialsV4 != NULL) && (mCredentialsV5 == NULL));
	
	if (mCredentialsV4 != NULL) {
		return cc_credentials_v4;
	} else {
		return cc_credentials_v5;
	}
}

// Copy to v4 creds struct
void
CCICredentialsData::CopyV4Credentials (
	cc_credentials_v4_t&		outCredentials) const {
	CCIAssert_ ((mCredentialsV4 != NULL) && (mCredentialsV5 == NULL));
	
	mCredentialsV4 -> CopyCredentials (outCredentials);
}

// Copy to v5 creds struct
void
CCICredentialsData::CopyV5Credentials (
	cc_credentials_v5_t&		outCredentials) const {
	CCIAssert_ ((mCredentialsV5 != NULL) && (mCredentialsV4 == NULL));
	
	mCredentialsV5 -> CopyCredentials (outCredentials);
}

#if CCache_v2_compat
// Copy to v2-style v4 struct
void
CCICredentialsData::CompatCopyV4Credentials (
	cc_credentials_v4_compat&		outCredentials) const {
	CCIAssert_ ((mCredentialsV4 != NULL) && (mCredentialsV5 == NULL));
	
	mCredentialsV4 -> CompatCopyCredentials (outCredentials);
}

// Copy to v2-style v5 struct
void
CCICredentialsData::CompatCopyV5Credentials (
	cc_credentials_v5_compat&		outCredentials) const {
	CCIAssert_ ((mCredentialsV5 != NULL) && (mCredentialsV4 == NULL));
	
	mCredentialsV5 -> CompatCopyCredentials (outCredentials);
}
#endif

#if PRAGMA_MARK
#pragma mark -
#endif

// Create from v4 struct
CCICredentialsData::CCICredentialsV4Data::CCICredentialsV4Data (
	const cc_credentials_v4_t*		inCredentials):
	
	mCredentials (*inCredentials) {
}

// Create from stream
CCICredentialsData::CCICredentialsV4Data::CCICredentialsV4Data (
	std::istream&				inStream) {
        
        inStream >> mCredentials;
}

#if CCache_v2_compat
// Create from v2-style v4 struct
CCICredentialsData::CCICredentialsV4Data::CCICredentialsV4Data (
	const cc_credentials_v4_compat*		inCredentials) {
	
	mCredentials.version = 4;
	strcpy (mCredentials.principal, inCredentials -> principal);
	strcpy (mCredentials.principal_instance, inCredentials -> principal_instance);
	strcpy (mCredentials.service, inCredentials -> service);
	strcpy (mCredentials.service_instance, inCredentials -> service_instance);
	strcpy (mCredentials.realm, inCredentials -> realm);
	memcpy (mCredentials.session_key, inCredentials -> session_key, sizeof (inCredentials -> session_key));
	mCredentials.kvno = inCredentials -> kvno;
	mCredentials.string_to_key_type = inCredentials -> str_to_key;
	mCredentials.issue_date = static_cast <cc_uint32> (inCredentials -> issue_date);
	mCredentials.lifetime = inCredentials -> lifetime;
	mCredentials.address = inCredentials -> address;
	mCredentials.ticket_size = inCredentials -> ticket_sz;
	CCIAssert_ (inCredentials -> ticket_sz <= static_cast <CCIInt32> (sizeof(mCredentials.ticket)));
#pragma message (CCIMessage_Warning_ "CCICredentialsData::CCICredentialsV4Data::CCICredentialsV4Data This should be a bad param error instead of an assertion")
	memcpy (mCredentials.ticket, inCredentials -> ticket, static_cast <cc_uint32> (inCredentials -> ticket_sz));
}
#endif

// Copy to v4 struct
void
CCICredentialsData::CCICredentialsV4Data::CopyCredentials (
	cc_credentials_v4_t&		outCredentials) const {
	outCredentials = mCredentials;
}

#if CCache_v2_compat
// Copy to v2-style v4 struct
void
CCICredentialsData::CCICredentialsV4Data::CompatCopyCredentials (
	cc_credentials_v4_compat&		outCredentials) const {
	outCredentials.kversion = 4;
	strcpy (outCredentials.principal, mCredentials.principal);
	strcpy (outCredentials.principal_instance, mCredentials.principal_instance);
	strcpy (outCredentials.service, mCredentials.service);
	strcpy (outCredentials.service_instance, mCredentials.service_instance);
	strcpy (outCredentials.realm, mCredentials.realm);
	memcpy (outCredentials.session_key, mCredentials.session_key, sizeof (mCredentials.session_key));
	outCredentials.kvno = mCredentials.kvno;
	outCredentials.str_to_key = mCredentials.string_to_key_type;
	outCredentials.issue_date = static_cast <cc_int32> (mCredentials.issue_date);
	outCredentials.lifetime = mCredentials.lifetime;
	outCredentials.address = mCredentials.address;
	outCredentials.ticket_sz = mCredentials.ticket_size;
	memcpy (outCredentials.ticket, mCredentials.ticket, static_cast <cc_uint32> (mCredentials.ticket_size));
	outCredentials.oops = 0xDEADBEEF;
}
#endif

#if PRAGMA_MARK
#pragma mark -
#endif

// Create from v5 struct
CCICredentialsData::CCICredentialsV5Data::CCICredentialsV5Data (
	const cc_credentials_v5_t*		inCredentials):
	
	mClient (inCredentials -> client),
	mServer (inCredentials -> server),
	mKeyblock (inCredentials -> keyblock),
	mAuthTime (inCredentials -> authtime),
	mStartTime (inCredentials -> starttime),
	mEndTime (inCredentials -> endtime),
	mRenewTill (inCredentials -> renew_till),
	mIsSKey (inCredentials -> is_skey),
	mTicketFlags (inCredentials -> ticket_flags),
	mAddresses (inCredentials -> addresses),
	mTicket (inCredentials -> ticket),
	mSecondTicket (inCredentials -> second_ticket),
	mAuthData (inCredentials -> authdata) {
}

// Create from stream
CCICredentialsData::CCICredentialsV5Data::CCICredentialsV5Data (
	std::istream&				inStream) {

    inStream >> mClient;
    inStream >> mServer;
    inStream >> mKeyblock;
    inStream >> mAuthTime;
    inStream >> mStartTime;
    inStream >> mEndTime;
    inStream >> mRenewTill;
    inStream >> mIsSKey;
    inStream >> mTicketFlags;
    inStream >> mAddresses;
    inStream >> mTicket;
    inStream >> mSecondTicket;
    inStream >> mAuthData;
}

#if CCache_v2_compat
// Create from v2-style v5 struct
CCICredentialsData::CCICredentialsV5Data::CCICredentialsV5Data (
	const cc_credentials_v5_compat*		inCredentials):
	
	mClient (inCredentials -> client),
	mServer (inCredentials -> server),
	mKeyblock (inCredentials -> keyblock),
	mAuthTime (inCredentials -> authtime),
	mStartTime (inCredentials -> starttime),
	mEndTime (inCredentials -> endtime),
	mRenewTill (inCredentials -> renew_till),
	mIsSKey (inCredentials -> is_skey),
	mTicketFlags (inCredentials -> ticket_flags),
	mAddresses (inCredentials -> addresses),
	mTicket (inCredentials -> ticket),
	mSecondTicket (inCredentials -> second_ticket),
	mAuthData (inCredentials -> authdata) {
}
#endif

// Copy to v5 struct
void
CCICredentialsData::CCICredentialsV5Data::CopyCredentials (
	cc_credentials_v5_t&		outCredentials) const {
	
	CopyString (mClient, outCredentials.client);
	CopyString (mServer, outCredentials.server);
	CopyCCData (mKeyblock, outCredentials.keyblock);
	CopyCCDataArray (mAddresses, outCredentials.addresses);
	CopyCCData (mTicket, outCredentials.ticket);
	CopyCCData (mSecondTicket, outCredentials.second_ticket);
	CopyCCDataArray (mAuthData, outCredentials.authdata);
	
	outCredentials.authtime = mAuthTime;
	outCredentials.starttime = mStartTime;
	outCredentials.endtime = mEndTime;
	outCredentials.renew_till = mRenewTill;
	outCredentials.is_skey = mIsSKey;
	outCredentials.ticket_flags = mTicketFlags;
}

#if CCache_v2_compat
// Copy to v2-style v5 struct
void
CCICredentialsData::CCICredentialsV5Data::CompatCopyCredentials (
	cc_credentials_v5_compat&		outCredentials) const {
	
	CopyString (mClient, outCredentials.client);
	CopyString (mServer, outCredentials.server);
	CopyCCData (mKeyblock, outCredentials.keyblock);
	CopyCCDataArray (mAddresses, outCredentials.addresses);
	CopyCCData (mTicket, outCredentials.ticket);
	CopyCCData (mSecondTicket, outCredentials.second_ticket);
	CopyCCDataArray (mAuthData, outCredentials.authdata);
	
	outCredentials.authtime = mAuthTime;
	outCredentials.starttime = mStartTime;
	outCredentials.endtime = mEndTime;
	outCredentials.renew_till = mRenewTill;
	outCredentials.is_skey = mIsSKey;
	outCredentials.ticket_flags = mTicketFlags;
}
#endif

// Utility: copy a string to char*
void
CCICredentialsData::CCICredentialsV5Data::CopyString (
	const SharedString&			inSource,
	char*&						outDestination) const {
	
	outDestination = new char [inSource.length () + 1];
	inSource.copy (outDestination, inSource.length ());
	outDestination [inSource.length ()] = '\0';
}

// Utility: copy a data blob to cc_data
void
CCICredentialsData::CCICredentialsV5Data::CopyCCData (
	const SharedCCData&			inSource,
	cc_data&					outDestination) const {
	
	outDestination.type = inSource.GetType ();
	outDestination.length = inSource.GetSize ();
	unsigned char* data = new unsigned char [inSource.GetSize ()];
	SharedCCData::const_iterator	iterator = inSource.begin ();
	for (; iterator < inSource.end (); iterator++) {
		data [iterator - inSource.begin ()] = *iterator;
	}
	outDestination.data = data;
}

// Utility: copy a data blob array to cc_data**
void
CCICredentialsData::CCICredentialsV5Data::CopyCCDataArray (
	const SharedCCDataArray&	inSource,
	cc_data**&					outDestination) const {
	
	// Special case 0-length, because that's how it is in Kerberos...
	if (inSource.GetSize () == 0) {
		outDestination = NULL;
		return;
	}
	
	outDestination = new cc_data* [inSource.GetSize () + 1];
	// NULL out the entire array, so that we can bail safely
	for (CCIUInt32 i = 0; i < inSource.GetSize () + 1; i++) {
		outDestination [i] = NULL;
	}

	SharedCCDataArray::const_iterator		iterator = inSource.begin ();
	for (; iterator < inSource.end (); iterator++) {
		outDestination [iterator - inSource.begin ()] =
			new cc_data ();
		outDestination [iterator - inSource.begin ()] -> data = NULL;
		CopyCCData (*(*iterator), *outDestination [iterator - inSource.begin ()]);
	}
}

// Send creds to a stream
std::ostream& operator << (std::ostream& ioStream, const CCICredentialsData& inCredentials)
{
    CCIUInt32	version = inCredentials.GetVersion ();
    ioStream << version << std::endl;
    switch (version) {
        case cc_credentials_v4:
            ioStream << *inCredentials.mCredentialsV4 << std::endl;
            break;
        
        case cc_credentials_v5:
            ioStream << *inCredentials.mCredentialsV5 << std::endl;
            break;

        default:
            throw CCIException (ccErrBadCredentialsVersion);
    }
    return ioStream;
}

// Send v4 creds to a stream
std::ostream& operator << (std::ostream& ioStream, const CCICredentialsData::CCICredentialsV4Data& inCredentials)
{
    return ioStream << inCredentials.mCredentials;
}

// Send v5 creds to a stream
std::ostream& operator << (std::ostream& ioStream, const CCICredentialsData::CCICredentialsV5Data& inCredentials)
{
    ioStream << inCredentials.mClient << std::endl;
    ioStream << inCredentials.mServer << std::endl;
    ioStream << inCredentials.mKeyblock << std::endl;
    ioStream << inCredentials.mAuthTime << std::endl;
    ioStream << inCredentials.mStartTime << std::endl;
    ioStream << inCredentials.mEndTime << std::endl;
    ioStream << inCredentials.mRenewTill << std::endl;
    ioStream << inCredentials.mIsSKey << std::endl;
    ioStream << inCredentials.mTicketFlags << std::endl;
    ioStream << inCredentials.mAddresses << std::endl;
    ioStream << inCredentials.mTicket << std::endl;
    ioStream << inCredentials.mSecondTicket << std::endl;
    ioStream << inCredentials.mAuthData << std::endl;
    return ioStream;
}

// Send a data blob to a stream
std::ostream& operator << (std::ostream& ioStream, const Implementations::CCISharedCCData& inData)
{
    ioStream << inData.GetType () << std::endl;
    ioStream << inData.GetSize () << std::endl;
    Implementations::CCISharedCCData::const_iterator	iterator = inData.begin ();
    for (; iterator < inData.end (); iterator++) {
        ioStream << (int) (*iterator) << " ";
    }
    
    return ioStream;
}
    
// Send a data blob array to a stream
std::ostream& operator << (std::ostream& ioStream, const Implementations::CCISharedCCDataArray& inDataArray)
{
    ioStream << inDataArray.GetSize () << std::endl;
    Implementations::CCISharedCCDataArray::const_iterator	iterator = inDataArray.begin ();
    for (; iterator < inDataArray.end (); iterator++) {
        ioStream << **iterator;
    }
    return ioStream;
}
