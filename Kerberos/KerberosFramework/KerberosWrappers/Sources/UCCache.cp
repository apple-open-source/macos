#include <Kerberos/UCCache.h>
#include <Kerberos/UKerberos5Context.h>
#include <Kerberos/KerberosDebug.h>
#include <string.h>

#include "ThrowUtils.h"

#pragma mark -

// Wrappers for ccache functions. Most of them are pretty straigtforward wrappers which
// call the function and throw errors

UCCacheContext::UCCacheContext ():
	UCCacheContextAutoPtr () {

	cc_context_t context;
	cc_int32 ccErr = cc_initialize (&context, ccapi_version_4, NULL, NULL);
	ThrowIfCCacheError (ccErr);
		
	Reset (context);
}

cc_time_t
UCCacheContext::GetChangeTime ()
{
	ThrowIfInvalid (*this);
	cc_time_t	changeTime;
	cc_int32 ccErr = cc_context_get_change_time (Get (), &changeTime);
	ThrowIfCCacheError (ccErr);
	
	return changeTime;
}

UCCacheString
UCCacheContext::GetDefaultCCacheName ()
{
	ThrowIfInvalid (*this);
	cc_string_t		name;
	cc_int32 ccErr = cc_context_get_default_ccache_name (Get (), &name);
	ThrowIfCCacheError (ccErr);
	
	return UCCacheString (name);
}

UCCache
UCCacheContext::OpenCCache (
	const char*			inName)
{
	ThrowIfInvalid (*this);
	cc_ccache_t ccache;
	cc_int32 ccErr = cc_context_open_ccache (Get (), inName, &ccache);
	ThrowIfCCacheError (ccErr);
	
	return UCCache (ccache);
}

UCCache
UCCacheContext::OpenDefaultCCache ()
{
	ThrowIfInvalid (*this);
	cc_ccache_t	ccache;
	cc_int32 ccErr = cc_context_open_default_ccache (Get (), &ccache);
	ThrowIfCCacheError (ccErr);
	
	return UCCache (ccache);
}

UCCache
UCCacheContext::CreateCCache (
	const char*				inName,
	UPrincipal::EVersion	inVersion,
	const char*				inPrincipal)
{
	ThrowIfInvalid (*this);
	cc_ccache_t ccache;
	cc_int32 ccErr = cc_context_create_ccache (Get(), inName, inVersion, inPrincipal, &ccache);
	ThrowIfCCacheError (ccErr);
	
	return UCCache (ccache);
}

UCCache
UCCacheContext::CreateDefaultCCache (
	UPrincipal::EVersion	inVersion,
	const char*				inPrincipal)
{
	ThrowIfInvalid (*this);
	cc_ccache_t ccache;
	cc_int32 ccErr = cc_context_create_default_ccache (Get(), inVersion, inPrincipal, &ccache);
	ThrowIfCCacheError (ccErr);
	
	return UCCache (ccache);
}

UCCache
UCCacheContext::CreateNewCCache (
	UPrincipal::EVersion	inVersion,
	const char*				inPrincipal)
{
	ThrowIfInvalid (*this);
	cc_ccache_t ccache;
	cc_int32 ccErr = cc_context_create_new_ccache (Get(), inVersion, inPrincipal, &ccache);
	ThrowIfCCacheError (ccErr);
	
	return UCCache (ccache);
}

UCCacheIterator
UCCacheContext::NewCCacheIterator ()
{
	ThrowIfInvalid (*this);
	cc_ccache_iterator_t iterator = NULL;
	cc_int32 ccErr = cc_context_new_ccache_iterator (Get (), &iterator);
	ThrowIfCCacheError (ccErr);
	
	return UCCacheIterator (iterator);
}

// Search all ccaches for a principal
UCCache
UCCacheContext::OpenCCacheForPrincipal (
	const UPrincipal&		inPrincipal) {
	
	UCCacheIterator iterator = NewCCacheIterator ();
	UCCache 		ccache;

	while (iterator.Next (ccache)) {
		if (ccache.GetCredentialsVersion () == UPrincipal::kerberosV4) {
			if (ccache.GetPrincipal (UPrincipal::kerberosV4) == inPrincipal)
				return ccache;
		} else {
			if (ccache.GetPrincipal (UPrincipal::kerberosV5) == inPrincipal)
				return ccache;
		}
	}

	DebugThrow_ (UCCacheLogicError (ccErrCCacheNotFound));
	return UCCache (); // silence the warning
}	

bool UCCacheContext::operator == (
	const UCCacheContext& inCompareTo) {
	
	cc_uint32	equal;
	cc_int32	err = cc_context_compare (Get (), inCompareTo.Get (), &equal);
	ThrowIfCCacheError (err);
	return (equal != 0);
}

#pragma mark -

void
UCCache::Destroy () {
	ThrowIfInvalid (*this);
	
	// ccache releases the cc_ccache_t on destroy
	cc_ccache_destroy (Get ());
	Release ();
}

void
UCCache::MoveTo (UCCache&	destination)
{
	ThrowIfInvalid (*this);
	ThrowIfInvalid (destination);
	
	// ccache releases the cc_ccache_t on move
	cc_ccache_move (Get (), destination.Get ());
	Release ();
}

void
UCCache::SetDefault ()
{
	ThrowIfInvalid (*this);
	cc_int32 ccErr = cc_ccache_set_default (Get ());
	ThrowIfCCacheError (ccErr);
}

UPrincipal::EVersion
UCCache::GetCredentialsVersion () const
{
	ThrowIfInvalid (*this);
	cc_uint32	version;
	cc_int32 ccErr = cc_ccache_get_credentials_version (Get (), &version);
	ThrowIfCCacheError (ccErr);
		
	return static_cast <UPrincipal::EVersion> (version);
}

UCCacheString
UCCache::GetName () const
{
	ThrowIfInvalid (*this);
	cc_string_t	name;
	cc_int32 ccErr = cc_ccache_get_name (Get (), &name);
	ThrowIfCCacheError (ccErr);
	
	return UCCacheString (name);
}

UPrincipal
UCCache::GetPrincipal (
	UPrincipal::EVersion				inVersion) const
{
	ThrowIfInvalid (*this);
	cc_string_t principal;
	cc_int32 ccErr = cc_ccache_get_principal (Get (), inVersion, &principal);
	ThrowIfCCacheError (ccErr);
	
	UCCacheString	string (principal);
	return UPrincipal (static_cast <UPrincipal::EVersion> (inVersion), principal -> data);
}

void
UCCache::SetPrincipal (
	UPrincipal::EVersion	inVersion,
	const char*				inPrincipal)
{
	ThrowIfInvalid (*this);
	cc_uint32 ccErr = cc_ccache_set_principal (Get (), inVersion, inPrincipal);
	ThrowIfCCacheError (ccErr);
}

// Set the principal for specified versions -- can be both v4 and v5
void
UCCache::SetPrincipal (
	UPrincipal::EVersion	inVersion,
	UPrincipal&				inPrincipal)
{
	cc_uint32 ccErr;
	
	ThrowIfInvalid (*this);
	if (inVersion == UPrincipal::kerberosV4And5) {
		ccErr = cc_ccache_set_principal (Get (), UPrincipal::kerberosV4, inPrincipal.GetString (UPrincipal::kerberosV4).c_str ());
		ThrowIfCCacheError (ccErr);
		ccErr = cc_ccache_set_principal (Get (), UPrincipal::kerberosV5, inPrincipal.GetString (UPrincipal::kerberosV5).c_str ());
		ThrowIfCCacheError (ccErr);
	} else {
		ccErr = cc_ccache_set_principal (Get (), inVersion, inPrincipal.GetString (inVersion).c_str ());
		ThrowIfCCacheError (ccErr);
	}
}


void
UCCache::StoreCredentials (
	const cc_credentials_union* inCredentials)
{
	ThrowIfInvalid (*this);
	cc_uint32 ccErr = cc_ccache_store_credentials (Get (), inCredentials);
	ThrowIfCCacheError (ccErr);
}

void
UCCache::StoreCredentials (
	const CREDENTIALS*	inCredentials)
{
	ThrowIfInvalid (*this);
	
	StCredentialsUnion credentials(*inCredentials);	
	cc_int32 ccErr = cc_ccache_store_credentials (Get (), credentials.Get ());

	ThrowIfCCacheError (ccErr);
}

void
UCCache::StoreCredentials (
	const krb5_creds*	inCredentials)
{
	ThrowIfInvalid (*this);
		
	StCredentialsUnion credentials(*inCredentials);	
	cc_int32 ccErr = cc_ccache_store_credentials (Get (), credentials.Get ());

	ThrowIfCCacheError (ccErr);
}


void
UCCache::RemoveCredentials (
	UCredentials&	inCredentials)
{
	ThrowIfInvalid (*this);
	cc_uint32 ccErr = cc_ccache_remove_credentials (Get (), inCredentials.Get ());
	ThrowIfCCacheError (ccErr);
}

UCredentialsIterator
UCCache::NewCredentialsIterator (
	UPrincipal::EVersion			inVersion) const
{
	ThrowIfInvalid (*this);
	cc_credentials_iterator_t iterator = NULL;
	cc_int32 ccErr = cc_ccache_new_credentials_iterator (Get (), &iterator);
	ThrowIfCCacheError (ccErr);
	
	return UCredentialsIterator (iterator, inVersion);
}

// Search a ccache for credentials for a given service
UCredentials
UCCache::GetCredentialsForService (
	const	UPrincipal&					inPrincipal,
	UPrincipal::EVersion				inVersion) const
{
	ThrowIfInvalid (*this);

	UCredentialsIterator iterator = NewCredentialsIterator (inVersion);
	
	UCredentials credentials;
	
	while (iterator.Next (credentials)) {
		if (inPrincipal == credentials.GetServicePrincipal ())
			return credentials;
	}
	
	DebugThrow_ (UCCacheRuntimeError (ccErrCredentialsNotFound));
	return UCredentials (); // silence the warning
}

// Delete credentials for a given service from the ccache
void
UCCache::DeleteCredentialsForService (
	const	UPrincipal&					inPrincipal,
	UPrincipal::EVersion				inVersion)
{
	ThrowIfInvalid (*this);

	UCredentialsIterator iterator = NewCredentialsIterator (inVersion);
	
	UCredentials theCredentials;
	
	while (iterator.Next (theCredentials)) {
		if (inPrincipal == theCredentials.GetServicePrincipal ()) {
			cc_int32 ccErr = cc_ccache_remove_credentials (Get (), theCredentials.Get ());
			ThrowIfCCacheError (ccErr);
		}
	}

	DebugThrow_ (UCCacheLogicError (ccErrCredentialsNotFound));
}

bool UCCache::operator == (
	const UCCache& inCompareTo) {
	
	cc_uint32	equal;
	cc_int32	err = cc_ccache_compare (Get (), inCompareTo.Get (), &equal);
	ThrowIfCCacheError (err);
	return (equal != 0);
}

#pragma mark -

bool
UCCacheIterator::Next (
	UCCache&				ioCCache)
{
	ThrowIfInvalid (*this);
	cc_ccache_t ccache;
	cc_int32 ccErr = cc_ccache_iterator_next (Get (), &ccache);
	if (ccErr == ccIteratorEnd)
		return false;
	ThrowIfCCacheError (ccErr);
	
	ioCCache = ccache;
	return true;
}

#pragma mark -

// Copy ccache credentials to v4 CREDENTIALS struct
void
UCredentials::CopyToV4Credentials (
	CREDENTIALS&				outCredentials) const
{	
	cc_credentials_v4_t*	v4creds = GetV4Credentials ();
	
	strcpy (outCredentials.service, v4creds -> service);
	strcpy (outCredentials.instance, v4creds -> service_instance);
	strcpy (outCredentials.realm, v4creds -> realm);
	memmove (&outCredentials.session, &v4creds -> session_key, sizeof (des_cblock));
	outCredentials.kvno = v4creds -> kvno;
	outCredentials.ticket_st.length = v4creds -> ticket_size;
	memmove (outCredentials.ticket_st.dat, v4creds -> ticket, (u_int32_t) v4creds -> ticket_size);
	outCredentials.issue_date = (int32_t) v4creds -> issue_date;
    // Fix the lifetime from CCAPI v4 to Kerberos 4 long lifetimes:
	outCredentials.lifetime = krb_time_to_life (v4creds->issue_date, 
                                v4creds->lifetime + v4creds->issue_date);
	strcpy (outCredentials.pname, v4creds -> principal);
	strcpy (outCredentials.pinst, v4creds -> principal_instance);
	outCredentials.address = v4creds -> address;
	outCredentials.stk_type = (u_int32_t) v4creds -> string_to_key_type;
}

// Copy ccache credentials to v4 CREDENTIALS struct
void
UCredentials::CopyToV5Credentials (
	krb5_creds&				outCredentials) const
{
    UKerberos5Context		context;
    cc_credentials_v5_t*	v5creds = GetV5Credentials ();
    krb5_int32 offset_seconds = 0, offset_microseconds = 0;
    krb5_error_code v5err;

    /*
     * allocate and copy
     * copy all of those damn fields back
     */
    v5err = krb5_parse_name(context.Get (), v5creds->client, &(outCredentials.client));
    if (v5err) {
        if (v5err == ENOMEM)  {
            std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
        } else {
            DebugThrow_ (std::logic_error ("UCredentials: Principal is not valid"));
        }
    }

    v5err = krb5_parse_name(context.Get (), v5creds->server, &(outCredentials.server));
    if (v5err) {
        if (v5err == ENOMEM)  {
            std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
        } else {
            DebugThrow_ (std::logic_error ("UCredentials: Principal is not valid"));
        }
    }

    /* copy keyblock */
    outCredentials.keyblock.enctype = v5creds->keyblock.type;
    outCredentials.keyblock.length = v5creds->keyblock.length;
    outCredentials.keyblock.contents = (krb5_octet *) malloc (outCredentials.keyblock.length);
    if (outCredentials.keyblock.contents == NULL) {
        std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
        DebugThrow_ (foo);
    }
    memcpy (outCredentials.keyblock.contents, v5creds->keyblock.data, outCredentials.keyblock.length);

    /* copy times */
    v5err = krb5_get_time_offsets (context.Get (), &offset_seconds, &offset_microseconds);
    if (v5err) {
        if (v5err == ENOMEM)  {
            std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
        } else {
            DebugThrow_ (std::logic_error ("UCredentials: Can't get time offsets"));
        }
    }

    outCredentials.times.authtime   = v5creds->authtime     + offset_seconds;
    outCredentials.times.starttime  = v5creds->starttime    + offset_seconds;
    outCredentials.times.endtime    = v5creds->endtime      + offset_seconds;
    outCredentials.times.renew_till = v5creds->renew_till   + offset_seconds;
    outCredentials.is_skey          = v5creds->is_skey;
    outCredentials.ticket_flags     = v5creds->ticket_flags;

    /* more branching fields */
    if (v5creds->addresses == NULL) {
	    outCredentials.addresses = NULL;
	} else {
	    krb5_address 	**addrPtr, *addr;
	    cc_data			**dataPtr, *data;
	    unsigned int		numRecords = 0;

	    /* Allocate the array of pointers: */
	    for (dataPtr = v5creds->addresses; *dataPtr != NULL; numRecords++, dataPtr++) {}

	    outCredentials.addresses = (krb5_address **) malloc (sizeof(krb5_address *) * (numRecords + 1));
	    if (outCredentials.addresses == NULL) {
            std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
        }

	    /* Fill in the array, allocating the address structures: */
	    for (dataPtr = v5creds->addresses, addrPtr = outCredentials.addresses; *dataPtr != NULL; addrPtr++, dataPtr++) {
            *addrPtr = (krb5_address *) malloc (sizeof(krb5_address));
            if (*addrPtr == NULL) {
                std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
                DebugThrow_ (foo);
            }
            data = *dataPtr;
            addr = *addrPtr;
    
            addr->addrtype = data->type;
            addr->magic    = KV5M_ADDRESS;
            addr->length   = data->length;
            addr->contents = (krb5_octet *) malloc (sizeof(krb5_octet) * addr->length);
            if (addr->contents == NULL) {
                std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
                DebugThrow_ (foo);
            }
            memmove(addr->contents, data->data, addr->length); /* copy contents */
	    }

	    /* Write terminator: */
	    *addrPtr = NULL;
	}    
    
    outCredentials.ticket.length = v5creds->ticket.length;
    outCredentials.ticket.data = (char *) malloc (v5creds->ticket.length);
    if (outCredentials.ticket.data == NULL) {
        std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
        DebugThrow_ (foo);
    }
    memcpy(outCredentials.ticket.data, v5creds->ticket.data, v5creds->ticket.length);
    
    outCredentials.second_ticket.length = v5creds->second_ticket.length;
    outCredentials.second_ticket.data = (char *) malloc (v5creds->second_ticket.length);
    if (outCredentials.second_ticket.data == NULL) {
        std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
        DebugThrow_ (foo);
    }
    memcpy (outCredentials.second_ticket.data, v5creds->second_ticket.data, v5creds->second_ticket.length);

    /* zero out magic number */
    outCredentials.magic = 0;

    /* authdata */
	if (v5creds->authdata == NULL) {
	    outCredentials.authdata = NULL;
	} else {
	    krb5_authdata 	**authPtr, *auth;
	    cc_data			**dataPtr, *data;
	    unsigned int		numRecords = 0;

	    /* Allocate the array of pointers: */
	    for (dataPtr = v5creds->authdata; *dataPtr != NULL; numRecords++, dataPtr++) {}

	    outCredentials.authdata = (krb5_authdata **) malloc (sizeof(krb5_authdata *) * (numRecords + 1));
	    if (outCredentials.authdata == NULL) {
            std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
            DebugThrow_ (foo);
        }
	    
        /* Fill in the array, allocating the address structures: */
	    for (dataPtr = v5creds->authdata, authPtr = outCredentials.authdata; *dataPtr != NULL; authPtr++, dataPtr++) {
            *authPtr = (krb5_authdata *) malloc (sizeof(krb5_authdata));
            if (*authPtr == NULL) {
                std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
                DebugThrow_ (foo);
            }
            data = *dataPtr;
            auth = *authPtr;
    
            auth->ad_type  = data->type;
            auth->magic    = KV5M_AUTHDATA;
            auth->length   = data->length;
            auth->contents = (krb5_octet *) malloc (sizeof(krb5_octet) * auth->length);
            if (auth->contents == NULL) {
                std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
                DebugThrow_ (foo);
            }
            memmove(auth->contents, data->data, auth->length); /* copy contents */
        }
    
        /* Write terminator: */
        *authPtr = NULL;
    }

    return;
}

// Get the appropriate service principal based on the ticket version
UPrincipal
UCredentials::GetServicePrincipal () const
{
	if (GetVersion () == UPrincipal::kerberosV4) {
		return UPrincipal (
			UPrincipal::kerberosV4,
			GetV4Credentials () -> service,
			GetV4Credentials () -> service_instance,
			GetV4Credentials () -> realm);
	} else {
		return UPrincipal (
			UPrincipal::kerberosV5,
			GetV5Credentials () -> server);
	}
}

// Get the appropriate client principal based on the ticket version
UPrincipal
UCredentials::GetClientPrincipal () const
{
	if (GetVersion () == UPrincipal::kerberosV4) {
		return UPrincipal (
			UPrincipal::kerberosV4,
			GetV4Credentials () -> principal,
			GetV4Credentials () -> principal_instance,
			GetV4Credentials () -> realm);
	} else {
		return UPrincipal (
			UPrincipal::kerberosV5,
			GetV5Credentials () -> client);
	}
}

// Copy out the v4 session key
des_cblock&
UCredentials::GetV4SessionKey (
	des_cblock&			outSessionKey) const
{
	memmove (&outSessionKey, &GetV4Credentials () -> session_key, sizeof (des_cblock));
	return outSessionKey;
}

// Get the v4 credentials if appropriate
cc_credentials_v4_t*
UCredentials::GetV4Credentials () const
{
	ThrowIfInvalid (*this);
	if (GetVersion () != UPrincipal::kerberosV4)
		ThrowIfCCacheError (ccErrBadCredentialsVersion);
	return Get () -> data -> credentials.credentials_v4;
}

// Get the v5 credentials if appropriate
cc_credentials_v5_t*
UCredentials::GetV5Credentials () const
{
	ThrowIfInvalid (*this);
	if (GetVersion () != UPrincipal::kerberosV5)
		ThrowIfCCacheError (ccErrBadCredentialsVersion);
	return Get () -> data -> credentials.credentials_v5;
}

UPrincipal::EVersion 
UCredentials::GetVersion () const
{
	ThrowIfInvalid (*this);
	return static_cast <UPrincipal::EVersion> (Get () -> data -> version);
}

bool UCredentials::operator == (
	const UCredentials& inCompareTo) {
	
	cc_uint32	equal;
	cc_int32	err = cc_credentials_compare (Get (), inCompareTo.Get (), &equal);
	ThrowIfCCacheError (err);
	return (equal != 0);
}

#pragma mark -

// Initialize from v4 CREDENTIALS struct
StCredentialsUnion::StCredentialsUnion (
	const CREDENTIALS&	inCreds)
{
	mCredentialsUnion.version = cc_credentials_v4;
	mCredentialsUnion.credentials.credentials_v4 = &mV4Creds;
	
	::strcpy (mV4Creds.principal, 			inCreds.pname);
	::strcpy (mV4Creds.principal_instance,	inCreds.pinst);
	::strcpy (mV4Creds.service, 			inCreds.service);
	::strcpy (mV4Creds.service_instance, 	inCreds.instance);
	::strcpy (mV4Creds.realm, 				inCreds.realm);
	::memmove (mV4Creds.session_key, 		inCreds.session, sizeof (des_cblock));
	mV4Creds.kvno = 						inCreds.kvno;
	mV4Creds.string_to_key_type = 			inCreds.stk_type;
	mV4Creds.issue_date = 					inCreds.issue_date;
	mV4Creds.address = 						inCreds.address;
    // Fix the lifetime to something CCAPI v4 likes:
	mV4Creds.lifetime = (int) (krb_life_to_time ((unsigned long) inCreds.issue_date,
                                                 inCreds.lifetime) 
                               - inCreds.issue_date);
	mV4Creds.ticket_size = 					inCreds.ticket_st.length;
	::memmove (mV4Creds.ticket, 			inCreds.ticket_st.dat, mV4Creds.ticket_size);
}


// Initialize from v5 creds
StCredentialsUnion::StCredentialsUnion (
	const krb5_creds&	inCreds)
{
	mCredentialsUnion.version = cc_credentials_v5;
	mCredentialsUnion.credentials.credentials_v5 = &mV5Creds;

	UKerberos5Context		context;
	char*					tempString;
	krb5_error_code			v5err;
	krb5_int32 				offset_seconds, offset_microseconds;

	// Client:
	v5err = ::krb5_unparse_name (context.Get (), inCreds.client, &tempString);
	Assert_ ((v5err == KRB5_PARSE_MALFORMED) || (v5err == ENOMEM) || (v5err == 0));
	if (v5err == ENOMEM)  {
                        std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
                }
		else if (v5err == KRB5_PARSE_MALFORMED) DebugThrow_ (std::logic_error ("StCredentialsUnion: Principal is not valid"));
	mV5Creds.client = new char [::strlen(tempString) + 1];
	::strcpy (mV5Creds.client, tempString);
	::krb5_free_unparsed_name (context.Get (), tempString);
	
	// Server:
	v5err = ::krb5_unparse_name (context.Get (), inCreds.server, &tempString);
	Assert_ ((v5err == KRB5_PARSE_MALFORMED) || (v5err == ENOMEM) || (v5err == 0));
	if (v5err == ENOMEM)  {
                        std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
                }
		else if (v5err == KRB5_PARSE_MALFORMED) DebugThrow_ (std::logic_error ("StCredentialsUnion: Principal is not valid"));
	mV5Creds.server = new char [::strlen(tempString) + 1];
	::strcpy (mV5Creds.server, tempString);
	::krb5_free_unparsed_name (context.Get (), tempString);
	
	// Keyblock:
	mV5Creds.keyblock.type =			inCreds.keyblock.enctype;
	mV5Creds.keyblock.length =			inCreds.keyblock.length;
	
	mV5Creds.keyblock.data = new char [mV5Creds.keyblock.length];
	::memmove(mV5Creds.keyblock.data, inCreds.keyblock.contents, mV5Creds.keyblock.length);
	
	// Times:
	v5err = ::krb5_get_time_offsets(context.Get(), &offset_seconds, &offset_microseconds);
	Assert_ (v5err == 0);
	if (v5err != 0)
		offset_seconds = 0;
	
	mV5Creds.authtime =					inCreds.times.authtime 		+ offset_seconds;
	mV5Creds.starttime =				inCreds.times.starttime 	+ offset_seconds;
	mV5Creds.endtime =					inCreds.times.endtime 		+ offset_seconds;
	mV5Creds.renew_till =				inCreds.times.renew_till 	+ offset_seconds;
	
	// Flags:
	mV5Creds.is_skey =					inCreds.is_skey;
	mV5Creds.ticket_flags =				inCreds.ticket_flags;
	
	// Addresses:
	if (inCreds.addresses == NULL) {
		mV5Creds.addresses = NULL;
	} else {
		krb5_address 	**addrPtr, *addr;
		cc_data			**dataPtr, *data;
		u_int32_t		numRecords = 0;
		
		// Allocate the array of pointers:
		for (addrPtr = inCreds.addresses; *addrPtr != NULL; numRecords++, addrPtr++) {}
		mV5Creds.addresses = new cc_data *[numRecords + 1];
		
		// Fill in the array, allocating the address structures:
		for (dataPtr = mV5Creds.addresses, addrPtr = inCreds.addresses; *addrPtr != NULL; addrPtr++, dataPtr++) {
			
			*dataPtr = new cc_data;
			data = *dataPtr;
			addr = *addrPtr;
			
			data->type = 		addr->addrtype;
			data->length = 		addr->length;
			data->data =		new char [data->length];
			::memmove(data->data, addr->contents, data->length); // copy pointer
		}
		
		// Write terminator:
		*dataPtr = NULL;
	}

	// Ticket:
	mV5Creds.ticket.length = inCreds.ticket.length;
	mV5Creds.ticket.data = new char [mV5Creds.ticket.length];
	::memmove(mV5Creds.ticket.data, inCreds.ticket.data, mV5Creds.ticket.length);
	
	// Second Ticket:
	mV5Creds.second_ticket.length = inCreds.second_ticket.length;
	mV5Creds.second_ticket.data = new char [mV5Creds.second_ticket.length];
	::memmove(mV5Creds.second_ticket.data, inCreds.second_ticket.data, mV5Creds.second_ticket.length);

	// Authdata:
	if (inCreds.authdata == NULL) {
		mV5Creds.authdata = NULL;
	} else {
		krb5_authdata 	**authPtr, *auth;
		cc_data			**dataPtr, *data;
		u_int32_t		numRecords = 0;
		
		// Allocate the array of pointers:
		for (authPtr = inCreds.authdata; *authPtr != NULL; numRecords++, authPtr++) {}
		mV5Creds.authdata = new cc_data* [numRecords + 1];
		
		// Fill in the array, allocating the address structures:
		for (dataPtr = mV5Creds.authdata, authPtr = inCreds.authdata; *authPtr != NULL; authPtr++, dataPtr++) {
			
			*dataPtr = new cc_data;
			data = *dataPtr;
			auth = *authPtr;
			
			data->type = 		auth->ad_type;
			data->length = 		auth->length;
			data->data =		new char [data->length];
			::memmove(data->data, auth->contents, data->length); // copy pointer
		}
		
		// Write terminator:
		*dataPtr = NULL;
	}
}

StCredentialsUnion::~StCredentialsUnion ()
{
	if (mCredentialsUnion.version == cc_credentials_v5) {
		// Free the client and server strings:
		delete [] mV5Creds.client;
		delete [] mV5Creds.server;
		
		// Free the data structures:
		delete [] static_cast<char *>(mV5Creds.keyblock.data);
		delete [] static_cast<char *>(mV5Creds.ticket.data);
		delete [] static_cast<char *>(mV5Creds.second_ticket.data);
		
		// Free the addresses:
		if (mV5Creds.addresses != NULL) {
			cc_data **dataPtr, *data;
			for (dataPtr = mV5Creds.addresses; *dataPtr != NULL; dataPtr++) {
				data = *dataPtr;
				
				delete [] static_cast<char *>(data->data);
				delete data;
			}
			delete [] mV5Creds.addresses;
		}
		
		// Free the authdata:
		if (mV5Creds.authdata != NULL) {
			cc_data **dataPtr, *data;
			for (dataPtr = mV5Creds.authdata; *dataPtr != NULL; dataPtr++) {
				data = *dataPtr;
				
				delete [] static_cast<char *>(data->data);
			    delete data;
			}
			delete [] mV5Creds.authdata;
		}
	}
}


#pragma mark -

// Iterate over credentials of a specific version
bool
UCredentialsIterator::Next (
	UCredentials&				ioCredentials)
{
	ThrowIfInvalid (*this);
	cc_credentials_t credentials;
	// Must find credentials of the appropriate version
	for (;;) {
		cc_int32 ccErr = cc_credentials_iterator_next (Get (), &credentials);
		if (ccErr == ccIteratorEnd)
			return false;
			ThrowIfCCacheError (ccErr);
			
		if ((mVersion == UPrincipal::kerberosV4And5) ||
		    ((mVersion == UPrincipal::kerberosV4) && (credentials -> data -> version == cc_credentials_v4)) ||
		    ((mVersion == UPrincipal::kerberosV5) && (credentials -> data -> version == cc_credentials_v5))) {
			// Version matches, return
			UCredentials		result (credentials);
			ioCredentials = result;
			return true;
		} else {
			// Version doesn't match, try again.
			cc_credentials_release (credentials);
		}
	}
}