
/*
 * KClient API functions deprecated in KClient API 3.0
 * Provided only for backwards compatibility
 *
 * $Header: /cvs/kfm/KerberosFramework/KClient/Sources/CompatibilityAPI/KClientDeprecated.Shlib.cp,v 1.31 2005/06/14 19:25:32 lxs Exp $
 */

#include <Kerberos/KClientDeprecated.h>
#include "KClientCCacheIntf.h"
#include "KClientProfileIntf.h"

static void PutKrb5DataIntoBuffer (
	krb5_data		inData,
	void*			outBuffer,
	UInt32*			outBufferLength);
	
static OSStatus RemapError (
	const std::bad_alloc&		inException);
static OSStatus RemapError (
	const std::range_error&		inException);
static OSStatus RemapError (
	const UProfileRuntimeError&	inException);
static OSStatus RemapError (
	const UCCacheRuntimeError&	inException,
	OSStatus					inError);
	
#pragma mark ¥ÊRealm configuration ¥

/*
 * The realm configuration section of the deprecated API doesn't get the benefit
 * of a KClientSession, so we can't get a profile handle from there. Instead,
 * we get a profile handle from Kerberos v4 library
 */

// Get default realm
OSStatus
KClientGetLocalRealmDeprecated (
	char*					outRealm )
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientProfileInterface	realmsConfig;
		realmsConfig.GetLocalRealm (outRealm);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapError (e);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbConfigurationErr));
	return err;
}

// Set default realm
OSStatus
KClientSetLocalRealmDeprecated (
	const char*				inRealm )
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientProfileInterface	realmsConfig;
		realmsConfig.SetLocalRealm (inRealm);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = memFullErr;
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapError (e);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbConfigurationErr));
	return err;
}

// Get realm of host
OSStatus
KClientGetRealmDeprecated (
	const char*				inHost,
	char*					outRealm)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientProfileInterface	realmsConfig;
		realmsConfig.GetRealmOfHost (inHost, outRealm);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = memFullErr;
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapError (e);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbConfigurationErr));
	return err;
}

// Add domain -> realm mapping
OSStatus
KClientAddRealmMapDeprecated (
	char*					inHost,
	char*					inRealm )
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientProfileInterface	config;
		config.AddRealmMap (inHost, inRealm);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapError (e);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbConfigurationErr));
	return err;
}

// Delete domain -> realm mapping
OSStatus
KClientDeleteRealmMapDeprecated (
	char*					inHost )
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientProfileInterface	config;
		config.DeleteRealmMap (inHost);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapError (e);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbConfigurationErr) ||
			 (err == cKrbMapDoesntExist));
	return err;
}

// Get domain -> realm mapping by index
OSStatus
KClientGetNthRealmMapDeprecated (
	SInt32					inIndex,
	char*					outHost,
	char*					outRealm )
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientProfileInterface	config;
		config.GetNthRealmMap (inIndex, outHost, outRealm);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (std::range_error& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapError (e);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbConfigurationErr) ||
			 (err == cKrbMapDoesntExist));
	return err;
}

// Get KDC in realm by index
// Only admin servers if inAdmin is true
// All servers if inAdmin is false
OSStatus
KClientGetNthServerDeprecated (
	SInt32					inIndex,
	char*					outHost,
	char*					inRealm,
	Boolean					inAdmin)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientProfileInterface	config;
		config.GetNthServer (inIndex, inRealm, inAdmin, outHost);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (std::range_error& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapError (e);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbConfigurationErr) ||
			 (err == cKrbMapDoesntExist));
	return err;
}

// Add KDC to realm, as admin if inAdmin is true
// If already exists change its status
OSStatus
KClientAddServerMapDeprecated (
	char*					inHost,
	char*					inRealm,
	Boolean					inAdmin)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientProfileInterface	config;
		config.AddServerMap (inHost, inRealm, inAdmin);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapError (e);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbConfigurationErr) ||
			 (err == cKrbMapDoesntExist));
	return err;
}

// Remove KDC from realm
OSStatus
KClientDeleteServerMapDeprecated (
	char*					inHost,
	char*					inRealm)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientProfileInterface	config;
		config.DeleteServerMap (inHost, inRealm);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapError (e);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbConfigurationErr) ||
			 (err == cKrbMapDoesntExist));
	return err;
}

// Lookup KDC and realm by index
// Note that this is a supreme weirdness iterator that spans realms
// and sucks to implement
OSStatus
KClientGetNthServerMapDeprecated (
	SInt32					inIndex,
	char*					outHost,
	char*					outRealm,
	Boolean*				outAdmin)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientProfileInterface	config;
		config.GetNthServerMap (inIndex, outHost, outRealm, *outAdmin);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (std::range_error& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapError (e);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbConfigurationErr) ||
			 (err == cKrbMapDoesntExist));
	return err;
}

// Lookup port by index
// Note that this is a supreme weirdness iterator that spans realms
// and sucks to implement
OSStatus
KClientGetNthServerPortDeprecated (
	SInt32					inIndex,
	UInt16*					outPort)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientProfileInterface	config;
		*outPort = config.GetNthServerPort (inIndex);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (std::range_error& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapError (e);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbConfigurationErr) ||
			 (err == cKrbMapDoesntExist));
	return err;
}

// Set port by index
// Note that this is a supreme weirdness iterator that spans realms
// and sucks to implement
OSStatus
KClientSetNthServerPortDeprecated (
	SInt32					inIndex,
	UInt16					inPort)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientProfileInterface	config;
		config.SetNthServerPort (inIndex, inPort);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (std::range_error& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapError (e);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbConfigurationErr) ||
			 (err == cKrbMapDoesntExist));
	return err;
}

#pragma mark -
#pragma mark ¥ÊCCache manipulation ¥

// Acquire initial ticket (always ask for password)
OSStatus
KClientCacheInitialTicketDeprecated (
	KClientSession*			inSession,
	char*					inService)
{
	// In KClient 3.0, this only works for ticket granting tickets
	char		principal [ANAME_SZ];
	char		instance [INST_SZ];
	char		realm [REALM_SZ];
	
	krb_get_lrealm (realm, 1);
	
	int krb4err = kname_parse (inService, principal, instance, realm);
	if (krb4err != KSUCCESS) {
		return paramErr;
	}

	if ((strcmp (principal, KRB_TICKET_GRANTING_TICKET) != 0) ||
		(strcmp (instance, realm) != 0)) {
		/* This is a not a request for a ticket-granting ticket, fail */
		return paramErr;
	}
	
	/* This is a request for a ticket-granting service. First call Login Library
	   to get fresh tickets */
	
	KLPrincipal	newPrincipal = nil;

	OSStatus err = KLAcquireNewInitialTickets (nil, nil, &newPrincipal, nil);

	char*	principalString = nil;
	if (err == noErr) {
		err = KLGetStringFromPrincipal (newPrincipal, kerberosVersion_V4, &principalString);
	}
	
	KClientPrincipal	kclientPrincipal = nil;
	if (err == noErr) {
		err = KClientV4StringToPrincipal (principalString, &kclientPrincipal);
	}
	
	if (err == noErr) {
		err = KClientSetClientPrincipal (*inSession, kclientPrincipal);
	}
	
	switch (err) {
		case noErr:
			break;

		case klUserCanceledErr:
			return cKrbUserCancelled;
			
		case klMemFullErr:
			return memFullErr;
		
		case klPreferencesReadErr:
		case klV5InitializationFailedErr:
		case klNoRealmsErr:
		case klRealmDoesNotExistErr:
			return cKrbConfigurationErr;
	}
	
	if (newPrincipal != nil) {
		KLDisposePrincipal (newPrincipal);
	}
	
	if (principalString != nil) {
		KLDisposeString (principalString);
	}
	
	if (kclientPrincipal != nil) {
		KClientDisposePrincipal (kclientPrincipal);
	}
	
	return err;
}

// Count number of v4 ccaches
OSStatus
KClientGetNumSessionsDeprecated (
	SInt32*					outSessions)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientCCacheInterface ccache;
		*outSessions = ccache.CountCCaches ();
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UCCacheRuntimeError& e) {
		err = RemapError (e, cKrbSessDoesntExist);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbSessDoesntExist));
	return err;
}

// Get the principal of a ccache (by index)
OSStatus
KClientGetNthSessionDeprecated (
	SInt32					inIndex,
	char*					outName,
	char*					outInstance,
	char*					outRealm)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientCCacheInterface ccache;
		std::string		name;
		std::string		instance;
		std::string		realm;
		ccache.GetNthCCache (inIndex).GetPrincipal(UPrincipal::kerberosV4).GetTriplet (UPrincipal::kerberosV4, name, instance, realm);
		strcpy (outName, name.c_str ());
		strcpy (outInstance, instance.c_str ());
		strcpy (outRealm, realm.c_str ());
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UCCacheRuntimeError& e) {
		err = RemapError (e, cKrbSessDoesntExist);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbSessDoesntExist));
	return err;
}

// Destroy a ccache by principal
OSStatus
KClientDeleteSessionDeprecated (
	char*					inName,
	char*					inInstance,
	char*					inRealm)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientCCacheInterface ccache;
		UPrincipal principal (UPrincipal::kerberosV4, inName, inInstance, inRealm);
		ccache.GetPrincipalCCache (principal).Destroy();
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UCCacheRuntimeError& e) {
		err = RemapError (e, cKrbSessDoesntExist);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbSessDoesntExist));
	return err;
}

// Get credentials
// Client principal is passed in as a triplet
// Service principal comes through the credentials argument
OSStatus
KClientGetCredentialsDeprecated (
	char*					inName,
	char*					inInstance,
	char*					inRealm,
	CREDENTIALS*			ioCred)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientCCacheInterface ccache;
		UPrincipal principal (UPrincipal::kerberosV4, inName, inInstance, inRealm);
		UPrincipal service (UPrincipal::kerberosV4, ioCred -> service, ioCred -> instance, ioCred -> realm);

		UCredentials credentials = ccache.GetPrincipalCCache (principal).GetCredentialsForService (service, UPrincipal::kerberosV4);
		credentials.CopyToV4Credentials (*ioCred);

	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UCCacheRuntimeError& e) {
		err = RemapError (e, cKrbCredsDontExist);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbCredsDontExist));
	return err;
}

// Add credentials
// Client principal is passed in as a triplet
OSStatus
KClientAddCredentialsDeprecated (
	char*					inName,
	char*					inInstance,
	char*					inRealm,
	CREDENTIALS*			inCred)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StCredentialsUnion	credentials (*inCred);
		KClientCCacheInterface ccache;
		UPrincipal principal (UPrincipal::kerberosV4, inName, inInstance, inRealm);
		ccache.GetPrincipalCCache (principal).StoreCredentials (credentials.Get ());
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UCCacheRuntimeError& e) {
		err = RemapError (e, cKrbCredsDontExist);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbCredsDontExist));
	return err;
}

// Delete credentials; client and service come in as triplets
OSStatus
KClientDeleteCredentialsDeprecated (
	char*					inName,
	char*					inInstance,
	char*					inRealm, 
	char*					inSname,
	char*					inSinstance,
	char*					inSrealm)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientCCacheInterface ccache;
		UPrincipal principal (UPrincipal::kerberosV4, inName, inInstance, inRealm);
		UPrincipal service (UPrincipal::kerberosV4, inSname, inSinstance, inSrealm);
		
		ccache.GetPrincipalCCache (principal).DeleteCredentialsForService (service, UPrincipal::kerberosV4);

	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UCCacheRuntimeError& e) {
		err = RemapError (e, cKrbCredsDontExist);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbCredsDontExist));
	return err;
}

// Count credentials in ccache
OSStatus
KClientGetNumCredentialsDeprecated (
	SInt32*					outNumCredentials,
	char*					inName,
	char*					inInstance,
	char*					inRealm)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientCCacheInterface ccache;
		UPrincipal principal (UPrincipal::kerberosV4, inName, inInstance, inRealm);
        UCCache principalCCache = ccache.GetPrincipalCCache (principal);
		*outNumCredentials = ccache.CountCredentials (principalCCache);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UCCacheRuntimeError& e) {
		err = RemapError (e, cKrbCredsDontExist);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbCredsDontExist));
	return err;
}

// Return the service principal of credentials in a cache, by index
OSStatus
KClientGetNthCredentialDeprecated (
	SInt32					inIndex,
	char*					inName,
	char*					inInstance,
	char*					inRealm,
	char*					outSname,
	char*					outSinstance,
	char*					outSrealm)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		KClientCCacheInterface ccache;
		std::string		name;
		std::string		instance;
		std::string		realm;
		UPrincipal principal (UPrincipal::kerberosV4, inName, inInstance, inRealm);
        UCCache principalCCache = ccache.GetPrincipalCCache (principal);
		ccache.GetNthCredentials (principalCCache, inIndex).
			GetServicePrincipal().GetTriplet (UPrincipal::kerberosV4, name, instance, realm);
		strcpy (outSname, name.c_str ());
		strcpy (outSinstance, instance.c_str ());
		strcpy (outSrealm, realm.c_str ());
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UCCacheRuntimeError& e) {
		err = RemapError (e, cKrbCredsDontExist);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
			 (err == cKrbCredsDontExist));
	return err;
}

// Get principal of the default ccache
OSStatus
KClientGetUserNameDeprecated (
	char*					outUserName)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		char	localRealm [REALM_SZ];
		
		KClientCCacheInterface	ccache;
		std::string		name;
		std::string		instance;
		std::string		realm;
		ccache.GetApplicationDefaultCCache ().GetPrincipal(UPrincipal::kerberosV4).GetTriplet (UPrincipal::kerberosV4, name, instance, realm);
		
		KClientProfileInterface	realmsConfig;
		realmsConfig.GetLocalRealm (localRealm);

		const char*			unparseRealm;
		if (realm == localRealm)
			unparseRealm = NULL;
		else
			unparseRealm = realm.c_str ();
		err = kname_unparse (outUserName, name.c_str (), instance.c_str (), unparseRealm);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = RemapError (e);
	} ShieldedCatch_ (UCCacheRuntimeError& e) {
		if (e.Error () == ccErrCCacheNotFound) {
			err = cKrbNotLoggedIn;
		} else {
			throw;
		}
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapError (e);
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr) ||
	         (err == memFullErr) ||
	         (err == cKrbNotLoggedIn) ||
			 (err == cKrbConfigurationErr));
	return err;
}

#pragma mark -
#pragma mark ¥ÊError strings and error remapping ¥

// Get error text
void KClientGetErrorTextDeprecated (
        OSErr                                           inError,
        char*                                           outText)
{
    const char *message = error_message (inError);
    strncpy (outText, message, kKClientMaxErrorTextLength);
    outText[kKClientMaxErrorTextLength - 1] = '\0';
}

static OSStatus RemapError (
	const std::bad_alloc&		/* inException */)
{
	return memFullErr;
}

static OSStatus RemapError (
	const std::range_error&		/* inException */)
{
	return cKrbMapDoesntExist;
}

static OSStatus RemapError (
	const UProfileRuntimeError&	inException)
{
	switch (inException.Error ()) {
		case ENOMEM:
			return memFullErr;
		
		default:
			return cKrbConfigurationErr;
	}
}

static OSStatus RemapError (
	const UCCacheRuntimeError&	/* inException */,
	OSStatus					inError)
{
	return inError;
}

#pragma mark -
#pragma mark ¥ÊK5Client support ¥

static char kK5ClientSendAuthVersion[] = "KRB5_SENDAUTH_V1.0";
static char kK5ClientBogusEudoraApplicationVersion[] = "justjunk";
static char kK5ClientCorrectEudoraApplicationVersion[] = "KPOPV1.0";

OSStatus
K5ClientGetTicketForServiceDeprecated (
	char*			inService,
	void*			outBuffer,
	UInt32*			outBufferLength) {
	
	OSStatus			err = noErr;
	
	krb5_context		context = NULL;
	krb5_ccache			ccache = NULL;
	
	krb5_principal		client = NULL;
	krb5_principal		server = NULL;
	krb5_creds			creds;
	creds.client = NULL;
	creds.server = NULL;
	
	krb5_creds*			credsPtr = NULL;
	krb5_data			krb5Data;
	Boolean				haveData = false;
	krb5_flags			apReqOptions = 0;
	
	krb5_auth_context	auth_context	= NULL;
	
	krb5_error_code		k5err;

	char 				service		[SNAME_SZ]	= "";
	char 				instance	[INST_SZ]	= "";
	char 				realm		[REALM_SZ]	= "";

	int					k4err;
	
	err = KLAcquireInitialTickets (nil, nil, nil, nil);		
	
	if (err == noErr) {
		k5err = krb5_init_context (&context);
		if (k5err != 0) {
			err = cKrbCredsDontExist;
		}
	}
	
	if (err == noErr) {
		k5err = krb5_cc_default (context, &ccache);
		if (k5err != 0) {
			err = cKrbCredsDontExist;
		}
	}
	
	if (err == noErr) {
		k5err = krb5_cc_get_principal (context, ccache, &client);
		if (k5err != 0) {
			err = cKrbCredsDontExist;
		}
	}

	if (err == noErr) {
		k4err = kname_parse (service, instance, realm, inService);
		if (k4err != KSUCCESS) {
			err = cKrbCredsDontExist;
		}
	}
	
	if (err == noErr) {
		k5err = krb5_sname_to_principal (context, instance, service, KRB5_NT_SRV_HST, &server);
		if (k5err != 0) {
			err = cKrbCredsDontExist;
		}
	}

	if (err == noErr) {	
		memset (&creds, 0, sizeof (creds));
		
		k5err = krb5_copy_principal (context, server, &creds.server);
		if (k5err != 0) {
			err = cKrbCredsDontExist;
		}
	}

	if (err == noErr) {	
		k5err = krb5_copy_principal (context, client, &creds.client);
		if (k5err != 0) {
			err = cKrbCredsDontExist;
		}
	}

	if (err == noErr) {	
		/* creds.times.endtime = 0; 	-- memset 0 takes care of this
						   zero means "as long as possible" */
		/* creds.keyblock.enctype = 0; -- as well as this.
						  zero means no session enctype
						  preference */

		k5err = krb5_get_credentials (context, 0, ccache, &creds, &credsPtr);
		if (k5err != 0) {
			err = cKrbCredsDontExist;
		}
	}
	
	if (err == noErr) {
		k5err = krb5_mk_req_extended (context, &auth_context, apReqOptions, 
			NULL, credsPtr, &krb5Data);
		if (k5err != 0) {
			err = cKrbCredsDontExist;
		} else {
			haveData = true;
		}
	}
	
	if (err == noErr) {
		/* 4-byte length, then data */
		PutKrb5DataIntoBuffer (krb5Data, outBuffer, outBufferLength);
	}
	
	if (auth_context != NULL) 
		krb5_auth_con_free (context, auth_context);
	if (client != NULL)
		krb5_free_principal (context, client);
	if (server != NULL)
		krb5_free_principal (context, server);
	if (creds.client != nil)
		krb5_free_principal (context, creds.client);
	if (creds.server != nil)
		krb5_free_principal (context, creds.server);
	if (credsPtr != NULL) 
		krb5_free_creds (context, credsPtr);
	if (haveData)
		krb5_free_data_contents (context, &krb5Data);
	if (ccache != NULL)	
		krb5_cc_close (context, ccache);
	if (context != NULL) 
		krb5_free_context (context);
	
	return err;
}		

OSStatus
K5ClientGetAuthenticatorForServiceDeprecated (
	char*			inService,
	char*			inApplicationVersion,
	void*			outBuffer,
	UInt32*			outBufferLength) {
	
	OSStatus	err;
	UInt32		bufferLength;
	krb5_data	appProtocolName;
	krb5_data	sendAuthProtocolName;
	char*		sendAuthProtocolBuffer;
	char*		appProtocolBuffer;
	char*		ticketBuffer;
	
	sendAuthProtocolBuffer = (char*) outBuffer;
	sendAuthProtocolName.length = (int) strlen (kK5ClientSendAuthVersion) + 1;
	sendAuthProtocolName.data = kK5ClientSendAuthVersion;

	// Than you Steve. Eudora sends "justjunk" as the version.	
	if (strcmp (inApplicationVersion, kK5ClientBogusEudoraApplicationVersion) == 0) {
		inApplicationVersion = kK5ClientCorrectEudoraApplicationVersion;
	}
	
	appProtocolBuffer = sendAuthProtocolBuffer + sendAuthProtocolName.length + sizeof (UInt32);
	appProtocolName.length = (int) strlen (inApplicationVersion) + 1;
	appProtocolName.data = inApplicationVersion;

	ticketBuffer = appProtocolBuffer + appProtocolName.length + sizeof (UInt32);
	err = K5ClientGetTicketForServiceDeprecated (inService, ticketBuffer, &bufferLength);
	if (err != noErr)
		return err;

	*outBufferLength = bufferLength;
	PutKrb5DataIntoBuffer (sendAuthProtocolName, sendAuthProtocolBuffer, &bufferLength);
	*outBufferLength += bufferLength;
	PutKrb5DataIntoBuffer (appProtocolName, appProtocolBuffer, &bufferLength);
	*outBufferLength += bufferLength;
	
	return noErr;
}

void
PutKrb5DataIntoBuffer (
	krb5_data		inData,
	void*			outBuffer,
	UInt32*			outBufferLength)
{
	*(UInt32*)outBuffer = (UInt32) inData.length;
	BlockMoveData (inData.data, (char*) outBuffer + sizeof (UInt32), inData.length);
	*outBufferLength = inData.length + sizeof (UInt32);
}
