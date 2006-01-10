/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header CLDAPNode
 * LDAP node management class.
 * Mutex ordering to prevent deadlocks:
 * <top to bottom>
 * fLDAPNodeOpenMutex
 * fLDAPSessionMutex
 * ConfigWithNodeNameLock and ConfigUnlock
 */

#include <stdio.h>
#include <string.h>		//used for strcpy, etc.
#include <stdlib.h>		//used for malloc
#include <ctype.h>		//use for isprint
#include <syslog.h>		//error logging
#include <arpa/inet.h>	// inet_ntop
#include <netinet/in.h>	// struct sockaddr_in
#include <ifaddrs.h>
#include <fcntl.h>
#include <sasl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <Kerberos/Kerberos.h>
#include <stack>

#include "CLDAPNode.h"
#include "CLDAPv3Plugin.h"
#include "CLog.h"
#include "DNSLookups.h"
#include "DSLDAPUtils.h"
#include "DSUtils.h"
#include "ServerControl.h"

#pragma mark Defines and Macros

#define OCSEPCHARS			" '()$"
#define IsOnlyBitSet(a,b)   (((a) & (b)) && ((a) ^ (b)) == 0) 

#pragma mark -
#pragma mark Globals, TypeDefs and Static Member Variables

extern  bool				gServerOS;
extern  DSMutexSemaphore	*gLDAPKerberosMutex;
extern  CLDAPv3Configs		*gpConfigFromXML;
extern	sInt32				gProcessPID;

struct saslDefaults
{
	char *authcid;
	char *password;
	char *authzid;
};

bool CLDAPNode::fCheckThreadActive	= false;
bool gBlockLDAPNetworkChange		= false;

class CLDAPv3Plugin;

#pragma mark -
#pragma mark Prototypes

bool checkReachability(struct sockaddr *destAddr);
int sasl_interact( LDAP *ld, unsigned flags, void *inDefaults, void *inInteract );
bool getUserTGTIfNecessaryAndStoreInCache( char *inName, char *inPassword );
int doSASLBindAttemptIfPossible( LDAP *inLDAPHost, sLDAPConfigData *pConfig, char *ldapAcct, char *ldapPasswd, char *kerberosID = NULL );
void *checkFailedServers( void *data );
void LogFailedConnection(const char *inTag, const char *inServerName, int inDisabledIntervalDuration);
void SetSockList(int *inSockList, int inSockCount, bool inClose);

#pragma mark -
#pragma mark Struct sLDAPNodeStruct Functions

sLDAPNodeStruct::sLDAPNodeStruct( void )
{
	fNodeName = NULL;
	fHost = NULL;
	fLDAPSessionMutex = new DSMutexSemaphore;
	
	fRefCount = 1;				// this is the number of references to this structure, must be 0 to delete
	fConnectionActiveCount = 0; // this is the number of active users using the fHost
	
	fLDAPServer = NULL;
	fDirectServerName = NULL;
	fDirectLDAPPort = 0;

	bAuthCallActive = false;
	bBadSession = false;
	
	fKerberosId = fLDAPUserName = fLDAPAuthType = NULL;
	fLDAPCredentials = NULL;
	fLDAPCredentialsLen = 0;
	
	fConnectionStatus = kConnectionUnknown;
	fDelayedBindTime = 0;
	
	fIdleTOCount = 0;
	fIdleTO = 2;	// let's default to 60 seconds..
	fDelayRebindTry = kLDAPDefaultRebindTryTimeoutInSeconds;
}

sLDAPNodeStruct::sLDAPNodeStruct( const sLDAPNodeStruct &inLDAPNodeStruct)
{
	fNodeName = (inLDAPNodeStruct.fNodeName ? strdup( inLDAPNodeStruct.fNodeName ) : NULL);
	
	fHost = NULL;
	fLDAPSessionMutex = new DSMutexSemaphore;
	
	fRefCount = 1;
	fConnectionActiveCount = 0;

	fLDAPServer = (inLDAPNodeStruct.fLDAPServer ? strdup(inLDAPNodeStruct.fLDAPServer) : NULL);
	
	fDirectServerName = (inLDAPNodeStruct.fDirectServerName ? strdup(inLDAPNodeStruct.fDirectServerName) : NULL );
	
	bAuthCallActive = false;
	bBadSession = false;
	
	fKerberosId = fLDAPUserName = fLDAPAuthType = NULL;
	fLDAPCredentials = NULL;
	fLDAPCredentialsLen = 0;
	
	fDirectLDAPPort = inLDAPNodeStruct.fDirectLDAPPort;
	
	fConnectionStatus = inLDAPNodeStruct.fConnectionStatus;
	fDelayedBindTime = inLDAPNodeStruct.fDelayedBindTime;
	
	fIdleTOCount = 0;
	fIdleTO = inLDAPNodeStruct.fIdleTO;
	fDelayRebindTry = inLDAPNodeStruct.fDelayRebindTry;
}

sLDAPNodeStruct::~sLDAPNodeStruct( void )
{
	if( fHost )
	{
		ldap_unbind_ext( fHost, NULL, NULL );
	}
	
	DSDelete( fNodeName );
	DSDelete( fLDAPServer );
	DSDelete( fDirectServerName );
	DSDelete( fLDAPUserName );
	DSDelete( fKerberosId );
	DSDelete( fLDAPAuthType );
	DSFree( fLDAPCredentials );
	DSDelete( fLDAPSessionMutex );
}

void sLDAPNodeStruct::updateCredentials( void *inLDAPCredentials, uInt32 inLDAPCredentialLen, char *inAuthType )
{
	DSFree( fLDAPCredentials );
	DSDelete( fLDAPAuthType );
	
	fLDAPAuthType = (inAuthType ? strdup(inAuthType) : NULL);

	if ( inLDAPCredentialLen )
	{
		fLDAPCredentials = calloc( inLDAPCredentialLen + 1, 1 );
		bcopy( inLDAPCredentials, fLDAPCredentials, inLDAPCredentialLen );
	}
}

void sLDAPNodeStruct::SessionMutexWaitWithFunctionName( const char* callingFunction )
{
	double		startTime, callTime;
	
#ifdef LOG_LDAPSessionMutex_Attempts
	DBGLOG3( kLogPlugin, "T[%X] %s called, waiting for fLDAPSessionMutex [%X]", pthread_self(), callingFunction, fLDAPSessionMutex );
#endif
	startTime = dsTimestamp();

	fLDAPSessionMutex->Wait();

	fLDAPSessionMutexStartTime = dsTimestamp();
	callTime = (fLDAPSessionMutexStartTime - startTime)/USEC_PER_SEC;

#ifdef LOG_LDAPSessionMutex_Attempts
	DBGLOG4( kLogPlugin, "T[%X] %s, got fLDAPSessionMutex [%X] after %f seconds", pthread_self(), callingFunction, fLDAPSessionMutex, callTime );
#endif
}

void sLDAPNodeStruct::SessionMutexSignalWithFunctionName( const char* callingFunction )
{
	double		callTime;
	
	callTime = (dsTimestamp() - fLDAPSessionMutexStartTime)/USEC_PER_SEC;

	if ( callTime > kMinTimeToLogLockBeingHeld )
	{
		DBGLOG4( kLogPlugin, "T[%X] %s, held fLDAPSessionMutex [%X] for %f seconds", pthread_self(), callingFunction, fLDAPSessionMutex, callTime );
	}
	
	fLDAPSessionMutex->Signal();
}

#pragma mark -
#pragma mark Struct sLDAPContextData Functions

sLDAPContextData::sLDAPContextData( const sLDAPContextData& inContextData )
{
	fType = 0;
	offset = 0;
	index = 1;
	
	fNodeName = ( inContextData.fNodeName ? strdup(inContextData.fNodeName) : NULL );

	// we don't dupe the fOpenRecordType = fOpenRecordName = fOpenRecordDN
	fOpenRecordType = fOpenRecordName = fOpenRecordDN = NULL;

	fUID = inContextData.fUID;
	fEffectiveUID = inContextData.fEffectiveUID;
	bLDAPv2ReadOnly = inContextData.bLDAPv2ReadOnly;
	
	fAuthUserName = (inContextData.fAuthUserName ? strdup(inContextData.fAuthUserName) : NULL);

	fAuthType = (inContextData.fAuthType ? strdup(inContextData.fAuthType) : NULL);
	
	if( inContextData.fAuthCredential != NULL )
	{
		fAuthCredential = calloc( inContextData.fAuthCredentialLen + 1, 1 );
		bcopy( inContextData.fAuthCredential, fAuthCredential, inContextData.fAuthCredentialLen );
		fAuthCredentialLen = inContextData.fAuthCredentialLen;
	}
	else
	{
		fAuthCredential = NULL;
		fAuthCredentialLen = 0;
	}
	
	// we don't copy fPWSRef and fPWSNodeRef
	fPWSRef = 0;
	fPWSNodeRef = 0;
	fPWSUserIDLength = 0;
	fPWSUserID = NULL;

	// session is already locked if we are here.. so no need to lock..
	fLDAPNodeStruct = inContextData.fLDAPNodeStruct;
	
	// we need to increase the refcount since we just referenced the existing fLDAPNodeStruct
	fLDAPNodeStruct->ChangeRefCountBy( 1 );
}

sLDAPContextData::sLDAPContextData( void )
{
	fNodeName = NULL;
	fType = 0;
	offset = 0;
	index = 1;
	
	bLDAPv2ReadOnly	= false;
	
	fOpenRecordType = fOpenRecordName = fOpenRecordDN = NULL;
	fAuthUserName = fAuthType = NULL;
	fAuthCredential = NULL;
	fAuthCredentialLen = 0;
	
	fPWSRef = 0;
	fPWSNodeRef = 0;
	fPWSUserIDLength = 0;
	fPWSUserID = NULL;
	fUID = fEffectiveUID = 0xffffffff;  // this is -1 (nobody)
	fLDAPNodeStruct = NULL;
}

sLDAPContextData::~sLDAPContextData( void )
{
	DSDelete( fNodeName );
	DSDelete( fOpenRecordType );
	DSDelete( fOpenRecordName );
	DSDelete( fOpenRecordDN );
	DSDelete( fAuthUserName );
	DSFree( fAuthCredential );
	DSDelete( fAuthType );
	
	if ( fPWSNodeRef != 0 ) {
		dsCloseDirNode( fPWSNodeRef );
		fPWSNodeRef = 0;
	}
	if ( fPWSRef != 0 ) {
		dsCloseDirService( fPWSRef );
		fPWSRef = 0;
	}
	DSFree( fPWSUserID );
	
	// should always be here but just in case
	if( fLDAPNodeStruct )
		fLDAPNodeStruct->ChangeRefCountBy( -1 );
}

void sLDAPContextData::setCredentials( char *inUserName, void *inCredential, uInt32 inCredentialLen, char *inAuthType )
{
	DSDelete( fAuthUserName );
	
	// void pointer can't use macro
	DSFree( fAuthCredential );
	
	DSDelete( fAuthType );
	
	fAuthUserName = (inUserName ? strdup(inUserName) : NULL);
	
	if( inCredential && inCredentialLen )
	{
		fAuthCredential = calloc( inCredentialLen + 1, 1 ); // add a null terminator
		bcopy( inCredential, fAuthCredential, inCredentialLen );
		fAuthCredentialLen = inCredentialLen;
	}
	else
	{
		fAuthCredential = NULL;
		fAuthCredentialLen = 0;
	}
	
	fAuthType = (inAuthType ? strdup(inAuthType) : NULL);
}

#pragma mark -
#pragma mark Support Routines

bool checkReachability(struct sockaddr *destAddr)
{
	SCNetworkReachabilityRef	target	= NULL;
	SCNetworkConnectionFlags	flags;
	bool						ok		= false;

	target = SCNetworkReachabilityCreateWithAddress(NULL, destAddr);
	if (target == NULL) {
		// can't determine the reachability
		goto done;
	}

	if (!SCNetworkReachabilityGetFlags(target, &flags)) {
		// can't get the reachability flags
		goto done;
	}

	if (!(flags & kSCNetworkFlagsReachable)) {
		// the destination address is not reachable with the current network config
		goto done;
	}

	if (flags & kSCNetworkFlagsConnectionRequired) {
		// a connection must first be established to reach the destination address
		goto done;
	}

	ok = true;	// the destination is reachable!

	done :
 
	if (target) CFRelease(target);
	return(ok);
}


int sasl_interact( LDAP *ld, unsigned flags, void *inDefaults, void *inInteract )
{
	sasl_interact_t *interact = (sasl_interact_t *)inInteract;
	saslDefaults *defaults = (saslDefaults *) inDefaults;
	
	if( ld == NULL ) return LDAP_PARAM_ERROR;
	
	while( interact->id != SASL_CB_LIST_END )
	{
		const char *dflt = interact->defresult;
		
		switch( interact->id )
		{
			case SASL_CB_AUTHNAME:
				if( defaults ) dflt = defaults->authcid;
				break;
			case SASL_CB_PASS:
				if( defaults ) dflt = defaults->password;
				break;
			case SASL_CB_USER:
				if( defaults ) dflt = defaults->authzid;
				break;
		}
		
		// if we don't have a value NULL it...
		if( dflt && !(*dflt) ) 
			dflt = NULL;
		
		// if we have a return value or SASL_CB_USER
		if( dflt || interact->id == SASL_CB_USER )
		{
			// we must either return something or an empty value otherwise....
			interact->result = ((dflt && *dflt) ? dflt : "");
			interact->len = strlen( (char *)interact->result );
		} else {
			return LDAP_OTHER;
		}
		
		interact++;
	}
	return LDAP_SUCCESS;
} 

bool getUserTGTIfNecessaryAndStoreInCache( char *inName, char *inPassword )
{
   int				siResult 	= eDSAuthFailed;
   krb5_creds		my_creds;
   krb5_context		krbContext  = NULL;
   krb5_principal	principal	= NULL;
   krb5_principal	cachePrinc	= NULL;
   krb5_ccache		krbCache	= NULL;
   krb5_error_code	retval;
   krb5_creds 		mcreds;
   bool				bNeedNewCred = true;
   char				*principalString = NULL;
   char				*pCacheName = NULL;
   
   try
   {
	   if( inName == NULL || inPassword == NULL ) throw( (int) __LINE__ );
	   
	   retval = krb5_init_context( &krbContext );
	   if( retval ) throw((int) __LINE__ );
	   
	   retval = krb5_parse_name( krbContext, inName, &principal );
	   if( retval ) throw((int) __LINE__ );
	   
	   retval = krb5_unparse_name( krbContext, principal, &principalString );
	   if( retval ) throw((int) __LINE__ );
	   
	   pCacheName = (char *) malloc( strlen(principalString) + 7 + 1 );	// "MEMORY:" + name + NULL
	   strcpy( pCacheName, "MEMORY:" );
	   strcat( pCacheName, principalString );
	   
	   // let's see if we already have a cache for this user..
	   retval = krb5_cc_resolve( krbContext, pCacheName, &krbCache);
	   if( retval ) throw((int) __LINE__ );
	   
	   retval = krb5_cc_set_default_name( krbContext, pCacheName );
	   if( retval ) throw((int) __LINE__ );
	   
	   // Lets get the cache principal...
	   if( krb5_cc_get_principal(krbContext, krbCache, &cachePrinc) == 0 )
	   {
		   // Now let's compare the principals to see if they match....
		   if( krb5_principal_compare(krbContext, principal, cachePrinc) )
		   {
			   // now let's retrieve the TGT and see if it is expired yet..
			   memset( &mcreds, 0, sizeof(mcreds) );
			   
			   retval = krb5_copy_principal( krbContext, principal, &mcreds.client );
			   if( retval ) throw((int) __LINE__ );
			   
			   // let's build the principal for the TGT so we can pull it from the cache...
			   retval = krb5_build_principal_ext( krbContext, &mcreds.server, principal->realm.length, principal->realm.data, KRB5_TGS_NAME_SIZE, KRB5_TGS_NAME, principal->realm.length, principal->realm.data, 0 );
			   
			   // this retrieves the TGT from the cache if it exists, if it doesn't, then we will get a new credential below
			   if( retval == 0 && krb5_cc_retrieve_cred( krbContext, krbCache, KRB5_TC_SUPPORTED_KTYPES, &mcreds, &my_creds) == 0 )
			   {
				   krb5_int32 timeret = 0;
				   
				   krb5_timeofday( krbContext, &timeret );
				   
				   // let's check for 5 minutes ahead.. so we don't expire in the middle of doing something....
				   timeret += 600;
				   
				   // if TGT is about to expire, let's just re-initialize the cache, and get a new one
				   if( timeret > my_creds.times.endtime )
				   {
					   krb5_cc_initialize( krbContext, krbCache, principal );
					   DBGLOG1( kLogPlugin, "CLDAPNode: Existing TGT expires shortly, initializing cache for user %s", principalString );
				   }
				   else
				   {
					   // otherwise, our credentials are fine, no need to get new ones..
					   bNeedNewCred = false;
					   DBGLOG1( kLogPlugin, "CLDAPNode: Existing TGT for user %s is Valid", principalString );
				   }
				   
				   krb5_free_cred_contents( krbContext, &my_creds );
			   }
			   krb5_free_cred_contents( krbContext, &mcreds );
		   }
		   else
		   {
			   DBGLOG1( kLogPlugin, "CLDAPNode: Cache for user %s being initialized", principalString );

			   retval = krb5_cc_initialize( krbContext, krbCache, principal );
			   if( retval ) throw((int) __LINE__ );
		   }
	   }
	   else
	   {
		   DBGLOG1( kLogPlugin, "CLDAPNode: Uninitialized cache available, initializing for user %s", principalString );

		   retval = krb5_cc_initialize( krbContext, krbCache, principal );
		   if( retval ) throw((int) __LINE__ );
	   }
	   
	   // GSSAPI's cache name needs to be set to match if we are getting ready to use GSSAPI
	   OM_uint32	minor_status;
	   
	   gss_krb5_ccache_name( &minor_status, pCacheName, NULL );
	   
//	   krb5_address				**addresses = NULL;
	   krb5_int32				startTime   = 0;
	   krb5_get_init_creds_opt  options;
	   
	   memset( &my_creds, 0, sizeof(my_creds) );
	   
	   krb5_get_init_creds_opt_init( &options );
	   krb5_get_init_creds_opt_set_forwardable( &options, 1 );
	   krb5_get_init_creds_opt_set_proxiable( &options, 1 );
	   
//	   krb5_os_localaddr( krbContext, &addresses );
	   krb5_get_init_creds_opt_set_address_list( &options, NULL );
//	   krb5_free_addresses( krbContext, addresses );
	   
	   // if we don't need new credentials, lets set the ticket life really short
	   if( bNeedNewCred == false )
	   {
		   krb5_get_init_creds_opt_set_tkt_life( &options, 300 ); // minimum is 5 minutes
		   DBGLOG1( kLogPlugin, "CLDAPNode: Getting TGT with short ticket life for verification only for user %s", principalString );
	   }

	   // we need to verify the password anyway...
	   retval = krb5_get_init_creds_password( krbContext, &my_creds, principal, inPassword, NULL, 0, startTime, NULL, &options );
	   if( retval )
	   {
		   DBGLOG1( kLogPlugin, "CLDAPNode: Error %d getting TGT", retval );
		   throw((int) __LINE__ );
	   }

	   // if we needed new credentials, then we need to store them..
	   if( bNeedNewCred )
	   {
		   DBGLOG1( kLogPlugin, "CLDAPNode: Storing credentials in Kerberos cache for user %s", principalString );

		   retval = krb5_cc_store_cred( krbContext, krbCache, &my_creds);
	   }
	   else 
	   {
		   DBGLOG1( kLogPlugin, "CLDAPNode: Valid credentials in Kerberos cache for user %s", principalString );
	   }
	   
	   // No need to hold onto credentials here..
	   krb5_free_cred_contents( krbContext, &my_creds );

	   siResult = eDSNoErr;
   }
   catch (int err )
   {
	   // err actually contains the line number we errored on so we can debug later if needed
	   DBGLOG2( kLogPlugin, "CLDAPNode: Error getting TGT for user line %d in %s", err, __FILE__ );
	   siResult = eDSAuthFailed;
   }
   
   DSDelete( pCacheName );
   
   if( principalString )
   {
	   krb5_free_unparsed_name( krbContext, principalString );
	   principalString = NULL;
   }
   
   if( principal )
   {
	   krb5_free_principal( krbContext, principal );
	   principal = NULL;
   }
   
   if( cachePrinc )
   {
	   krb5_free_principal( krbContext, cachePrinc );
	   cachePrinc = NULL;
   }
   
   if( krbCache )
   {
	   // if the auth failed, let's destroy the cache instead..
	   if( siResult == eDSAuthFailed )
	   {
		   krb5_cc_destroy( krbContext, krbCache );
		   krbCache = NULL;
	   }
	   else 
	   {
		   krb5_cc_close( krbContext, krbCache );
		   krbCache = NULL;
	   }
   }
   
   if( krbContext )
   {
	   krb5_free_context( krbContext );
	   krbContext = NULL;
   }
   
   return (siResult == eDSNoErr);
}
				   
int doSASLBindAttemptIfPossible( LDAP *inLDAPHost, sLDAPConfigData *pConfig, char *ldapAcct, char *ldapPasswd, char *kerberosID )
{
	int				siResult		= LDAP_OTHER;
	saslDefaults	defaults		= { 0 };
	char			**dn			= NULL;

	// let's set up locals so we can see if we ended up with missing required methods...
	uInt32  iReqSecurity		= kSecNoSecurity;
	uInt32  iFinalSecurity		= kSecNoSecurity;

	if( pConfig != NULL && pConfig->fSASLmethods != NULL )
	{
		CFMutableArrayRef   cfSupportedMethods  = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		CFRange				saslMethodRange		= CFRangeMake( 0, CFArrayGetCount( pConfig->fSASLmethods ) );

		iReqSecurity = pConfig->fSecurityLevel & kSecSecurityMask;
		
		// if we don't have an account... we don't care about cleartext
		if( DSIsStringEmpty(ldapAcct) == false && DSIsStringEmpty( ldapPasswd ) == false )
		{
			dn = ldap_explode_dn( ldapAcct, 1 );
			
			// if this is a DN then let's use the DN... otherwise just use the ldapAcct, cause it isn't a DN
			if( dn ) {
				defaults.authcid	= dn[0];
				defaults.authzid	= (char *)calloc( sizeof(char), strlen(ldapAcct) + 3 + 1 );
				strcpy( defaults.authzid, "dn:" );
				strcat( defaults.authzid, ldapAcct );
			} else {
				defaults.authcid	= ldapAcct;
				defaults.authzid	= (char *)calloc( sizeof(char), strlen(ldapAcct) + 2 + 1 );
				strcpy( defaults.authzid, "u:" );
				strcat( defaults.authzid, ldapAcct );
			}
			
			defaults.password   = ldapPasswd;
		}
		else
		{
			// if we don't have a username or password, let's turn off the kSecDisallowCleartext bit, cause there aren't credentials
			iReqSecurity &= (~kSecDisallowCleartext);
		}
		
		// if we have SSL, we already support non-clear text and PacketEncryption..
		if( pConfig->bIsSSL )
		{
			iFinalSecurity |= (kSecDisallowCleartext | kSecPacketEncryption);
		}
		
		// if we support GSSAPI and if any of the individual bits are requested or no security at all
		if( CFArrayContainsValue(pConfig->fSASLmethods, saslMethodRange, CFSTR("GSSAPI")) && gLDAPKerberosMutex &&
			( (iReqSecurity & kSecDisallowCleartext) || 
			  (iReqSecurity & kSecManInMiddle) || 
			  (iReqSecurity & kSecPacketSigning) || 
			  (iReqSecurity & kSecPacketEncryption) || 
			  (iReqSecurity & kSecSecurityMask) == 0 ) )
		{
			CFArrayAppendValue( cfSupportedMethods, CFSTR("GSSAPI") );
			iFinalSecurity |= (kSecDisallowCleartext | kSecManInMiddle | kSecPacketSigning | kSecPacketEncryption);
		}
		
		// we only add CRAM-MD5 if security requirements can be met with it.. cause it only solves cleartext
		//   -- if it is the only bit set
		//   -- no security requirements specified
		if( CFArrayContainsValue(pConfig->fSASLmethods, saslMethodRange, CFSTR("CRAM-MD5")) && 
			( IsOnlyBitSet(iReqSecurity,kSecDisallowCleartext) ||
			  (iReqSecurity & kSecSecurityMask) == 0) )
		{
			CFArrayAppendValue( cfSupportedMethods, CFSTR("CRAM-MD5") );
			iFinalSecurity |= kSecDisallowCleartext;
		}
		
		// we disable DIGEST-MD5 cause improper configured servers seem to have a problem
//		if( CFArrayContainsValue(pConfig->fSASLmethods, CFSTR("DIGEST-MD5")) && 
//			( IsOnlyBitSet(iReqSecurity,kSecDisallowCleartext) || (iReqSecurity & kSecSecurityMask) == 0) )
//		{
//			CFSetAddValue( cfSupportedMethods, CFSTR("DIGEST-MD5") );
//			iFinalSecurity |= kSecDisallowCleartext;
//		}
		
		// if we have satisfied all of the requirements
		if( (iReqSecurity & iFinalSecurity) == iReqSecurity && 
			DSIsStringEmpty(ldapAcct) == false && DSIsStringEmpty(ldapPasswd) == false )
		{
			CFIndex iCount = CFArrayGetCount( cfSupportedMethods );

			// cfSupportedMethods contains only methods that are supported by the node
			if( iCount )
			{
				for( CFIndex ii = 0; ii < iCount && (siResult == LDAP_OTHER || siResult == LDAP_LOCAL_ERROR); ii++ )
				{
					CFStringRef cfMethod = (CFStringRef) CFArrayGetValueAtIndex( cfSupportedMethods, ii );
					
					uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfMethod), kCFStringEncodingUTF8 ) + 1;
					char *pMethod = (char *) calloc( sizeof(char), uiLength );
					CFStringGetCString( cfMethod, pMethod, uiLength, kCFStringEncodingUTF8 );
					
					DBGLOG1( kLogPlugin, "CLDAPNode: Attempting %s Authentication", pMethod );
					
					// we handle GSSAPI special since it 
					if( strcmp(pMethod, "GSSAPI") == 0 )
					{
						saslDefaults	gssapiDefaults	= { 0 };
						char			*username		= NULL;
						
						// if the authzid == the fServerAccount, let's see if we have a kerberos ID too
						if( pConfig->fServerAccount && strcmp(pConfig->fServerAccount, ldapAcct) == 0 && pConfig->fKerberosId )
						{
							username = pConfig->fKerberosId;
						}
						else
						{
							username = (kerberosID ? kerberosID : defaults.authcid);
						}

						// if encryption is required, we need to set the ssf flag
						if( (iReqSecurity & kSecPacketEncryption) != 0 )
						{
							ldap_set_option( inLDAPHost, LDAP_OPT_X_SASL_SECPROPS, (void *) "minssf=56" );
						}
						else if( (iReqSecurity & kSecPacketSigning) != 0 )
						{
							ldap_set_option( inLDAPHost, LDAP_OPT_X_SASL_SECPROPS, (void *) "minssf=1" );
						}

						if ( username != NULL )
						{
							char *pRealm = strchr( username, '@' );
							if ( pRealm != NULL )
							{
								gpConfigFromXML->VerifyKerberosForRealm( pRealm+1, pConfig->fServerName );
							}
						}
						
						gLDAPKerberosMutex->Wait();

						// let's get a TGT if possible or ensure we have one, if we can't we really shouldn't attempt sasl_bind
						if( getUserTGTIfNecessaryAndStoreInCache(username, ldapPasswd) )
						{
							siResult = ldap_sasl_interactive_bind_s( inLDAPHost, NULL, pMethod, NULL, NULL, LDAP_SASL_QUIET, sasl_interact, &gssapiDefaults );
						}
						else
						{
							DBGLOG( kLogPlugin, "CLDAPNode: Couldn't get Kerberos credentials for GSSAPI" );
							siResult = LDAP_LOCAL_ERROR;
						}

						if( siResult == LDAP_LOCAL_ERROR || siResult == LDAP_OTHER )
						{
							ldap_set_option( inLDAPHost, LDAP_OPT_X_SASL_SECPROPS, (void *) "" );   // reset minSSF
						}
						
						gLDAPKerberosMutex->Signal();
					}
					else
					{
						siResult = ldap_sasl_interactive_bind_s( inLDAPHost, NULL, pMethod, NULL, NULL, LDAP_SASL_QUIET, sasl_interact, &defaults );
					}
					
					if( siResult == LDAP_AUTH_METHOD_NOT_SUPPORTED )
					{
						DBGLOG1( kLogPlugin, "CLDAPNode: Failed %s Authentication, not supported", pMethod );
						siResult = LDAP_OTHER;
					}
					else if( siResult == LDAP_OTHER || siResult == LDAP_LOCAL_ERROR )
					{
						DBGLOG2( kLogPlugin, "CLDAPNode: Failed %s Authentication for %s", pMethod, (defaults.authzid ? defaults.authzid : defaults.authcid) );
					}
					
					if( siResult == eDSNoErr )
					{
						DBGLOG2( kLogPlugin, "CLDAPNode: Successful %s Authentication for %s", pMethod, (defaults.authzid ? defaults.authzid : defaults.authcid) );
					}
					
					DSFree( pMethod );
				}
			}
			else
			{
				// if we don't have SASL methods
				DBGLOG( kLogPlugin, "CLDAPNode: No SASL methods found for server." );
			}
			
			// if the final result was LDAP_OTHER, we couldn't negotiate something secure
			if( siResult == LDAP_OTHER || siResult == LDAP_LOCAL_ERROR )
			{
				// if we had SSL, let's turn off the bits for NoCleartext and Encryption, cause they are covered..
				if( pConfig->bIsSSL )
				{
					iReqSecurity &= (kSecManInMiddle | kSecPacketSigning);
				}
				
				// if there are flags set that we were expecting to negotiate, then we need to set the flags so we can error on it.
				// we cheat by flipping all bits
				iFinalSecurity = (~iFinalSecurity);
			}
		}
		else	// skipping sasl methods cause there's no name and password involved...
		{
			DBGLOG( kLogPlugin, "CLDAPNode: Skipping SASL methods for server." );
		}
		
		if( dn != NULL )
		{
			ldap_value_free( dn );
			dn = NULL;
		}
		
		DSFree( defaults.authzid );
		
		DSCFRelease( cfSupportedMethods );
	}
	
	// if any of the requirements were not met and we have a username and password, let's print an error accordingly
	if( (iReqSecurity & iFinalSecurity) != iReqSecurity && 
		DSIsStringEmpty(ldapAcct) == false && DSIsStringEmpty(ldapPasswd) == false )
	{
		// flip all the bits we're missing, then AND it with what we wanted
		uInt32  iMissing = (~iFinalSecurity) & iReqSecurity;
		char	pRequirements[128] = { 0 };

		if( (iMissing & kSecDisallowCleartext) == kSecDisallowCleartext )
		{
			strcat( pRequirements, "No ClearText, " );
		}
		
		if( (iMissing & kSecManInMiddle) == kSecManInMiddle )
		{
			strcat( pRequirements, "Man-In-The-Middle, " );
		}
		
		if( (iMissing & kSecPacketSigning) == kSecPacketSigning )
		{
			strcat( pRequirements, "Packet Signing, " );
		}
		
		if( (iMissing & kSecPacketEncryption) == kSecPacketEncryption )
		{
			strcat( pRequirements, "Packet Encryption" );
		}
		else if( strlen(pRequirements) )
		{
			pRequirements[strlen(pRequirements)-2] = 0; // let's null out the comma otherwise
		}
		
		DBGLOG2( kLogPlugin, "CLDAPNode: Required Policies not Supported: %s.  LDAP Connection for Node %s denied.", pRequirements, pConfig->fNodeName );
		syslog( LOG_ALERT,"DSLDAPv3PlugIn: Required Policies not Supported: %s.  LDAP Connection for Node %s denied.", pRequirements, pConfig->fNodeName );
		siResult = LDAP_STRONG_AUTH_REQUIRED;
	}
	
	return siResult;
}// doSASLBindAttemptIfPossible

void *checkFailedServers( void *data )
{
	CLDAPNode   *ldapNode = (CLDAPNode *)data;
	
	ldapNode->CheckFailed();
	CLDAPNode::fCheckThreadActive = false;
	return NULL;
}

void LogFailedConnection(const char *inTag, const char *inServerName, int inDisabledIntervalDuration)
{
	if ((inTag != nil) && (inServerName != nil))
	{
		//log this timed out connection
		syslog(LOG_ALERT,"%s: During an attempt to bind to [%s] LDAP server.", inTag, inServerName);
		syslog(LOG_ALERT,"%s: Disabled future attempts to bind to [%s] LDAP server for next %d seconds.", inTag, inServerName, inDisabledIntervalDuration);
		DBGLOG2( kLogPlugin, "CLDAPNode: Disabled future attempts to bind to LDAP server %s for %d seconds", inServerName, inDisabledIntervalDuration );
	}
	else
	{
		syslog(LOG_ALERT,"%s: Logging Failed LDAP connection with incomplete data", (inTag ? inTag : "unknown"));
		DBGLOG( kLogPlugin, "CLDAPNode: Failed LDAP connection" );
	}
}

void SetSockList(int *inSockList, int inSockCount, bool inClose)
{
	for (int iCount = 0; iCount < inSockCount; iCount++)
	{
		if ( (inClose) && (inSockList[iCount] >= 0) )
		{
			close(inSockList[iCount]);
		}
		inSockList[iCount] = -1;
	}
}

#pragma mark -
#pragma mark CLDAPNode Functions

// --------------------------------------------------------------------------------
//	* CLDAPNode ()
// --------------------------------------------------------------------------------

CLDAPNode::CLDAPNode ( void )
{
	// let's build the methods we support with LDAP
	fSupportedSASLMethods = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	
	CFArrayAppendValue( fSupportedSASLMethods, CFSTR("CRAM-MD5") );
	CFArrayAppendValue( fSupportedSASLMethods, CFSTR("GSSAPI") );
	
	fInStartupState = ( gProcessPID < 100 );        // only want to set this if we are booting up
	
} // CLDAPNode


// --------------------------------------------------------------------------------
//	* ~CLDAPNode ()
// --------------------------------------------------------------------------------

CLDAPNode::~CLDAPNode ( void )
{
	CFRelease( fSupportedSASLMethods );
	fSupportedSASLMethods = NULL;
} // ~CLDAPNode

double	gNodeOpenMutexHeldStartTime;

void CLDAPNode::NodeOpenMutexWaitWithFunctionName( const char* callingFunction, bool waitForCheckFailedThreadToComplete )
{
	#ifdef LOG_LDAPNodeOpenMutex_Attempts
	double		startTime, callTime;
	
	startTime = dsTimestamp();
	
	DBGLOG3( kLogPlugin, "T[%X] %s called, waiting for fLDAPNodeOpenMutex [%X]", pthread_self(), callingFunction, &fLDAPNodeOpenMutex );
	#endif

	if ( waitForCheckFailedThreadToComplete && fCheckThreadActive )
	{
		DSSemaphore		timedWait;
		while(fCheckThreadActive)
		{
			// Check every .5 seconds
			timedWait.Wait( (uInt32)(.5 * kMilliSecsPerSec) );
		}
	}
	
	fLDAPNodeOpenMutex.Wait();

	gNodeOpenMutexHeldStartTime = dsTimestamp();

	#ifdef LOG_LDAPNodeOpenMutex_Attempts
	callTime = (gNodeOpenMutexHeldStartTime - startTime)/USEC_PER_SEC;
	DBGLOG4( kLogPlugin, "T[%X] %s, got fLDAPNodeOpenMutex [%X] after %f seconds", pthread_self(), callingFunction, &fLDAPNodeOpenMutex, callTime );
	#endif
}

void CLDAPNode::NodeOpenMutexSignalWithFunctionName( const char* callingFunction )
{
	double		callTime;
	
	callTime = (dsTimestamp() - gNodeOpenMutexHeldStartTime)/USEC_PER_SEC;

	if ( callTime > kMinTimeToLogLockBeingHeld )
	{
		DBGLOG4( kLogPlugin, "T[%X] %s, held fLDAPNodeOpenMutex [%X] for %f seconds", pthread_self(), callingFunction, &fLDAPNodeOpenMutex, callTime );
	}
	#ifdef LOG_LDAPNodeOpenMutex_Attempts
	else
	{
		DBGLOG4( kLogPlugin, "T[%X] %s, held fLDAPNodeOpenMutex [%X] for %f seconds", pthread_self(), callingFunction, &fLDAPNodeOpenMutex, callTime );
	}
	#endif
	
	fLDAPNodeOpenMutex.Signal();
}

// ---------------------------------------------------------------------------
//	* SafeOpen
// ---------------------------------------------------------------------------

sInt32 CLDAPNode::SafeOpen		(	char			*inNodeName,
									sLDAPNodeStruct **outLDAPNodeStruct )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPNodeStruct		   *pLDAPNodeStruct	= nil;
	sLDAPConfigData		   *pConfig			= nil;
    int						ldapPort		= LDAP_PORT;
	LDAPNodeMapI			aLDAPNodeMapI;
	uInt32					openTO			= kLDAPDefaultOpenCloseTimeoutInSeconds;
	uInt32					idleTO			= 0;
	uInt32					rebindRetry		= 0;
	string					aNodeName(inNodeName);
	bool					bGetServerMappings		= false;
	bool					bGetSecuritySettings	= false;
    bool                    bBuildReplicaList       = false;
	char					*configNodeName	= NULL; // do not release only a pointer to existing data
	
//if already open then just get host
//if not open then bind to get host and search for entry for host
//called from OpenDirNode

//allow the inNodeName to have a suffixed ":portNumber" for directed open
	CLDAPv3Plugin::WaitForNetworkTransitionToFinish();

	NodeOpenMutexWait();
	
	// let's see if there's a config for the node in question
	pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inNodeName );
	
	if (pConfig != nil)
	{
		openTO					= pConfig->fOpenCloseTimeout;
		idleTO					= pConfig->fIdleTimeout;
		rebindRetry				= pConfig->fDelayRebindTry;
		ldapPort				= pConfig->fServerPort;
		bGetServerMappings		= pConfig->bGetServerMappings;
		bGetSecuritySettings	= pConfig->bGetSecuritySettings;
        bBuildReplicaList       = pConfig->bBuildReplicaList;
		configNodeName			= strdup(pConfig->fNodeName);

		gpConfigFromXML->ConfigUnlock( pConfig );
	}
	
	aLDAPNodeMapI	= fLDAPNodeMap.find(aNodeName);	

	// if it wasn't found, then let's create a new one..
	if (aLDAPNodeMapI == fLDAPNodeMap.end())
	{
		pLDAPNodeStruct = new sLDAPNodeStruct;

		// don't care if this was originally in the config file or not
		// ie. allow non-configured connections if possible
		// however, they need to use the standard LDAP PORT if no config entry exists
		// search now for possible LDAP port entry

		// we aren't using pConfig, just seeing if it was found..
		if (pConfig != nil)
		{
			if (configNodeName != nil)
			{
				pLDAPNodeStruct->fNodeName = configNodeName;
				configNodeName	= NULL;

				//add the idle connection TO value here based on user defined minutes and 30 sec periodic task
				pLDAPNodeStruct->fIdleTO = idleTO;
				
				//add in the delay rebind try after failed bind time
				pLDAPNodeStruct->fDelayRebindTry = rebindRetry;
				
			} // if name not nil
			else
			{
				pLDAPNodeStruct->fNodeName = strdup( inNodeName );
			}
		}// if config entry not nil
		
		if (pConfig == nil)
		{
			char	*aLDAPName		= nil;

			//here we have not found a configuration but will allow the open
			//first check if there is a suffixed ':' port number on the inNodeName
			siResult = ParseLDAPNodeName( inNodeName, &aLDAPName, &ldapPort );
			if (siResult == eDSNoErr)
			{
				pLDAPNodeStruct->fDirectServerName		= aLDAPName;
				pLDAPNodeStruct->fDirectLDAPPort		= ldapPort;
				//TODO need to access the LDAP server for possible mapping configuration that can be added
				//thus fNodeName will be NULL
			}
		}
		
		//add this to the fLDAPNodeMap
		fLDAPNodeMap[aNodeName] = pLDAPNodeStruct;
		//fLDAPNodeMap.insert(pair<string, sLDAPNodeStruct*>(aNodeName, pLDAPNodeStruct));
	}
	else
	{
		pLDAPNodeStruct = aLDAPNodeMapI->second;
		pLDAPNodeStruct->ChangeRefCountBy( 1 ); // we are using existing, increment it accordingly
	}
	
	// if we have an existing, we need to decrement our refcount..
	if( *outLDAPNodeStruct )
	{
		(*outLDAPNodeStruct)->ChangeRefCountBy( -1 );
	}
	
	// assign our outgoing since we just created or used existing a node struct
	*outLDAPNodeStruct = pLDAPNodeStruct;
	
	// we're done accessing the NodeMutex
	NodeOpenMutexSignal();

	if (siResult == eDSNoErr)
	{
		if (pLDAPNodeStruct->fConnectionStatus == kConnectionUnknown)
		{
			//first lets spawn our checking thread if this has not already been done
			EnsureCheckFailedConnectionsThreadIsRunning();
			
			// let's go to sleep over the designated open/close timeout value
			// and check every .5 secs if it becomes available.
			DSSemaphore		timedWait;
			double			waitTime	= dsTimestamp() + USEC_PER_SEC*openTO;
			while(pLDAPNodeStruct->fConnectionStatus == kConnectionUnknown)
			{
				// Check every .5 seconds
				timedWait.Wait( (uInt32)(.5 * kMilliSecsPerSec) );

				// check over max
				if ( dsTimestamp() > waitTime )
				{
					// We have waited as long as we are going to at this time
					break;
				}
			}
		}
		
		CLDAPv3Plugin::WaitForNetworkTransitionToFinish();
		if (pLDAPNodeStruct->fConnectionStatus != kConnectionSafe)
		{
			siResult = eDSCannotAccessSession;
		}
		
		if (siResult == eDSNoErr)
		{
			pLDAPNodeStruct->SessionMutexWait();

			// SASLMethods are first so that we can do non-clear text auth methods
			CheckSASLMethods( pLDAPNodeStruct );
	
            // if bBuildReplicaList is active, but we need serverMappings, we need to get them first..
            if( bBuildReplicaList && bGetServerMappings )
            {
                // let's turn buildReplicaList and reset it later..
                pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inNodeName );
                pConfig->bBuildReplicaList = false;
                gpConfigFromXML->ConfigUnlock( pConfig );
            }

			//call to bind here
			siResult = BindProc( pLDAPNodeStruct );

            // turn the flag back on
            if( bBuildReplicaList && bGetServerMappings )
            {
                // let's turn buildReplicaList and reset it later..
                pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inNodeName );
                pConfig->bBuildReplicaList = true;
                gpConfigFromXML->ConfigUnlock( pConfig );
            }
            
			// we have mutex, no need to call activeConnectionChange
			pLDAPNodeStruct->fConnectionActiveCount++;
		
			if( pLDAPNodeStruct->fHost )
			{
                bool    bDoRebind = false;
                
				// we need to preflight ServerMappings here... then rebind again...
				if( bGetServerMappings )
				{
					RetrieveServerMappingsIfRequired( pLDAPNodeStruct );
                    bDoRebind = true; // if we needed server mappings, then we need to rebind when we are done..
				}
				
				// if we were successful at binding, then let's retrieve our latest security settings if available
				// we won't do this unless we have already met our previous security requirements during the first bind
				if( bGetSecuritySettings )
				{
					pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inNodeName );
					if (pConfig != nil)
					{
                        if( gpConfigFromXML->UpdateConfigWithSecuritySettings( pLDAPNodeStruct->fNodeName, pConfig, pLDAPNodeStruct->fHost ) == eDSNoErr )
                        {
                            // if we updated our security settings then we need to rebind
                            bDoRebind = true;
                        }
						gpConfigFromXML->ConfigUnlock( pConfig );
					}
				}
                
                if( bDoRebind )
                {
                    ldap_unbind_ext( pLDAPNodeStruct->fHost, NULL, NULL );
                    pLDAPNodeStruct->fHost = nil;
                    
                    // we need to rebind to see if we found replicas now....
                    siResult = BindProc( pLDAPNodeStruct );
                }
			}
			
			// we have mutex, no need to call activeConnectionChange
			pLDAPNodeStruct->fConnectionActiveCount--;
			
			pLDAPNodeStruct->SessionMutexSignal();
		}
	}
	
	DSFreeString( configNodeName );
	
	return(siResult);
} // SafeOpen

// ---------------------------------------------------------------------------
//	* AuthOpen
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::AuthOpen		(	char			*inLDAPUserName,
									char			*inKerberosId,
									void			*inLDAPCredentials,
									uInt32			 inLDAPCredentialsLen,
									char			*inLDAPAuthType,
									sLDAPNodeStruct **outLDAPNodeStruct )
{
	sInt32			siResult				= eDSAuthFailed;
	sLDAPNodeStruct *pLDAPAuthNodeStruct	= nil;
	sLDAPNodeStruct *inLDAPNodeStruct		= *outLDAPNodeStruct; // we should always have one since we had to open the first time
	char			timestamp[128]			= { 0, };
	
	// here we are looking for existing node before we decide to create a new one
	// if we have a configName, then it is the server, otherwise, we use DirectServerName

	// in form of "server:cn=user,cn=blah:timestamp"
	string  aNodeName(inLDAPNodeStruct->fNodeName ? inLDAPNodeStruct->fNodeName : inLDAPNodeStruct->fDirectServerName );
	
	// let's put a stamp on this one and add it to our lists..
	snprintf( timestamp, sizeof(timestamp), "%lld", mach_absolute_time() );

	aNodeName += ":";
	aNodeName += inLDAPUserName;
	aNodeName += ":";
	aNodeName += timestamp;
	
	// let's wait to make sure we have control of the struct
	inLDAPNodeStruct->SessionMutexWait();

	pLDAPAuthNodeStruct	= new sLDAPNodeStruct( *inLDAPNodeStruct );
	
	inLDAPNodeStruct->SessionMutexSignal();
	
	pLDAPAuthNodeStruct->bAuthCallActive	= true;
	pLDAPAuthNodeStruct->fLDAPUserName		= (inLDAPUserName ? strdup( inLDAPUserName ) : nil);
	pLDAPAuthNodeStruct->fLDAPAuthType		= (inLDAPAuthType ? strdup( inLDAPAuthType ) : nil);
	pLDAPAuthNodeStruct->fKerberosId		= (inKerberosId ? strdup( inKerberosId ) : nil);
	
	if( inLDAPCredentials && inLDAPCredentialsLen )
	{
		pLDAPAuthNodeStruct->fLDAPCredentials	= calloc( inLDAPCredentialsLen + 1, 1 );
		bcopy( inLDAPCredentials, pLDAPAuthNodeStruct->fLDAPCredentials, inLDAPCredentialsLen );
		pLDAPAuthNodeStruct->fLDAPCredentialsLen = inLDAPCredentialsLen;
	}

	// all we have to do is bind and return, since we always keep the nodeStructs
	siResult = BindProc( pLDAPAuthNodeStruct, false, false, true );

	if( eDSNoErr == siResult )
	{
		// let's decrement the Refcount of the original since we are detaching from original
		inLDAPNodeStruct->ChangeRefCountBy( -1 );

		// set the outgoing pointer
		*outLDAPNodeStruct = pLDAPAuthNodeStruct;
		
		NodeOpenMutexWait();
		fLDAPNodeMap[aNodeName] = pLDAPAuthNodeStruct;
		NodeOpenMutexSignal();
	}
	else // delete the one we created because it isn't used
	{
		delete pLDAPAuthNodeStruct;
		pLDAPAuthNodeStruct = nil;
	}
	
	return( siResult );

}// AuthOpen

// ---------------------------------------------------------------------------
//	* ForcedSafeClose
// ---------------------------------------------------------------------------

void CLDAPNode::ForcedSafeClose( const char *inNodeName )
{
	// we need to iterate through all nodes that start with "inNodeName" and close them if no references
	sLDAPNodeStruct	   *pLDAPNodeStruct = nil;
	LDAPNodeMapI		aLDAPNodeMapI;
	LDAPNodeMap			aLDAPNodeMap;
	stack<string>		unusedStack;

	NodeOpenMutexWait();

	for (aLDAPNodeMapI = fLDAPNodeMap.begin(); aLDAPNodeMapI != fLDAPNodeMap.end(); ++aLDAPNodeMapI)
	{
		pLDAPNodeStruct = aLDAPNodeMapI->second;

		// need to grab the mutex, since we don't go to the MapTable everytime anymore...
		pLDAPNodeStruct->SessionMutexWait();

		// let's compare the node name and refCount so we can remove if necessary
		if( (inNodeName || strcmp(inNodeName, pLDAPNodeStruct->fNodeName) == 0) && pLDAPNodeStruct->fRefCount == 0 )
		{
			// first, let's unbind...
			if( pLDAPNodeStruct->fHost )
			{
				ldap_unbind( pLDAPNodeStruct->fHost ); //unbind the connection and nil out
				pLDAPNodeStruct->fHost = NULL;
			}
			unusedStack.push( aLDAPNodeMapI->first );
		}
		
		pLDAPNodeStruct->SessionMutexSignal();
	}

	// we go through our stack to remove connections..
	while( unusedStack.empty() == false )
	{
		// no need for mutex now, once we got the mutex above and new references cannot be opened without fLDAPNodeOpenMutex
		aLDAPNodeMapI = fLDAPNodeMap.find( unusedStack.top() );

		if( aLDAPNodeMapI != fLDAPNodeMap.end() )
		{
			DBGLOG1( kLogPlugin, "CLDAPNode: Force removing Node: %s -- References: 0 -- from table", aLDAPNodeMapI->first.c_str() );
			
			DSDelete( aLDAPNodeMapI->second );
			fLDAPNodeMap.erase( unusedStack.top() );
		}
		unusedStack.pop();
	}
	NodeOpenMutexSignal();	
	
} //ForcedSafeClose

// ---------------------------------------------------------------------------
//	* RebindSession
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::RebindSession( sLDAPNodeStruct *pLDAPNodeStruct  )
{
	sInt32					siResult		= eDSNoErr;
	
	CLDAPv3Plugin::WaitForNetworkTransitionToFinish();
	NodeOpenMutexWait();	// make sure the node mutex is free 
							// so we don't prematurely grab the session mutex
	NodeOpenMutexSignal();
		
	pLDAPNodeStruct->SessionMutexWait();
	
	if (pLDAPNodeStruct->fHost != nil)
	{
		ldap_unbind(pLDAPNodeStruct->fHost);
		pLDAPNodeStruct->fHost = nil;
	}
		
	//call to bind here
	//TODO How many retries do we do?
	siResult = BindProc( pLDAPNodeStruct, false, false, pLDAPNodeStruct->bAuthCallActive );
		
	if (siResult == eDSNoErr)
	{
	   // if we weren't safe before, we need to notify that the search policy may have changed.
	   if ( pLDAPNodeStruct->fConnectionStatus != kConnectionSafe )
			   gSrvrCntl->NodeSearchPolicyChanged();

		//set the out parameters now
		pLDAPNodeStruct->fConnectionStatus = kConnectionSafe;
		RetrieveServerMappingsIfRequired( pLDAPNodeStruct );
	}

	pLDAPNodeStruct->SessionMutexSignal();
	
	return(siResult);

}// RebindSession

// ---------------------------------------------------------------------------
//	* SimpleAuth
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::SimpleAuth(	sLDAPNodeStruct *inLDAPNodeStruct,
								char			*inLDAPUserName,
								void			*inLDAPCredentials,
								uInt32			 inLDAPCredentialsLen,
								char			*inKerberosId )
{
	sInt32					siResult			= eDSNoErr;

	// we are going to setup a temporary struct based on the existing to do the auth..
	inLDAPNodeStruct->SessionMutexWait();
	
	sLDAPNodeStruct *pLDAPAuthNodeStruct	= new sLDAPNodeStruct( *inLDAPNodeStruct );
	
	inLDAPNodeStruct->SessionMutexSignal();
	
	pLDAPAuthNodeStruct->bAuthCallActive	= true;
	pLDAPAuthNodeStruct->fLDAPUserName		= (inLDAPUserName ? strdup( inLDAPUserName ) : nil);
	pLDAPAuthNodeStruct->fKerberosId		= (inKerberosId ? strdup( inKerberosId ) : nil);
	
	if( inLDAPCredentials && inLDAPCredentialsLen )
	{
		pLDAPAuthNodeStruct->fLDAPCredentials	= calloc( inLDAPCredentialsLen + 1, 1 );
		bcopy( inLDAPCredentials, pLDAPAuthNodeStruct->fLDAPCredentials, inLDAPCredentialsLen );
		pLDAPAuthNodeStruct->fLDAPCredentialsLen = inLDAPCredentialsLen;
	}

	// now try to bind
	siResult = BindProc( pLDAPAuthNodeStruct, false, true, false );
	
	if (siResult != eDSNoErr)
	{
		siResult = eDSAuthFailed;
	}

	// all we have to do is delete it.. in does unbind and everything
	DSDelete( pLDAPAuthNodeStruct );
	
	return(siResult);

}// SimpleAuth

// ---------------------------------------------------------------------------
//	* GetSchema
// ---------------------------------------------------------------------------

void	CLDAPNode::GetSchema	( sLDAPContextData *inContext )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPConfigData		   *pConfig			= nil;
	LDAPMessage			   *LDAPResult		= nil;
	BerElement			   *ber				= nil;
	struct berval		  **bValues			= nil;
	char				   *pAttr			= nil;
	sObjectClassSchema	   *aOCSchema		= nil;
	bool					bSkipToTag		= true;
	char				   *lineEntry		= nil;
	char				   *strtokContext	= nil;
	LDAP				   *aHost			= nil;
	
	if ( inContext != nil )
	{
		aHost = LockSession(inContext);
		if ( aHost != nil ) //valid LDAP handle
		{
			pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inContext->fNodeName );
			if (pConfig != nil && !(pConfig->bOCBuilt)) // if valid pConfig and we haven't build the schema yet
			{
				//at this point we can make the call to the LDAP server to determine the object class schema
				//then after building the ObjectClassMap we can assign it to the pConfig->fObjectClassSchema
				//in either case we set the pConfig->bOCBuilt since we made the attempt
				siResult = GetSchemaMessage( aHost, pConfig->fSearchTimeout, &LDAPResult);
				
				if (siResult == eDSNoErr)
				{
					//parse the attributes in the LDAPResult - should only be one ie. objectclass
					for (	pAttr = ldap_first_attribute (aHost, LDAPResult, &ber );
							pAttr != NULL; pAttr = ldap_next_attribute(aHost, LDAPResult, ber ) )
					{
						if (( bValues = ldap_get_values_len (aHost, LDAPResult, pAttr )) != NULL)
						{
							ObjectClassMap *aOCClassMap = new(ObjectClassMap);
						
							// for each value of the attribute we need to parse and add as an entry to the objectclass schema map
							for (int i = 0; bValues[i] != NULL; i++ )
							{
								aOCSchema = nil;
								if (lineEntry != nil) //delimiter chars will be overwritten by NULLs
								{
									DSFree(lineEntry);
								}
								
								//here we actually parse the values
								lineEntry = (char *)calloc(1,bValues[i]->bv_len+1);
								strcpy(lineEntry, bValues[i]->bv_val);
								
								char	   *aToken			= nil;
								
								//find the objectclass name
								aToken = strtok_r(lineEntry,OCSEPCHARS, &strtokContext);
								while ( (aToken != nil) && (strcmp(aToken,"NAME") != 0) )
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
								}
								if (aToken != nil)
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken != nil)
									{
										//now use the NAME to create an entry
										//first check if that NAME is already present - unlikely
										if (aOCClassMap->find(aToken) == aOCClassMap->end())
										{
											aOCSchema = new(sObjectClassSchema);
											(*aOCClassMap)[aToken] = aOCSchema;
										}
									}
								}
								
								if (aOCSchema == nil)
								{
									continue;
								}
								if (aToken == nil)
								{
									continue;
								}
								//here we have the NAME - at least one of them
								//now check if there are any more NAME values
								bSkipToTag = true;
								while (bSkipToTag)
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										break;
									}
									bSkipToTag = IsTokenNotATag(aToken);
									if (bSkipToTag)
									{
										aOCSchema->fOtherNames.insert(aOCSchema->fOtherNames.begin(),aToken);
									}
								}
								if (aToken == nil)
								{
									continue;
								}
								
								if (strcmp(aToken,"DESC") == 0)
								{
									bSkipToTag = true;
									while (bSkipToTag)
									{
										aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
										if (aToken == nil)
										{
											break;
										}
										bSkipToTag = IsTokenNotATag(aToken);
									}
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"OBSOLETE") == 0)
								{
									bSkipToTag = true;
									while (bSkipToTag)
									{
										aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
										if (aToken == nil)
										{
											break;
										}
										bSkipToTag = IsTokenNotATag(aToken);
									}
									if (aToken == nil)
									{
										continue;
									}
								}
		
								if (strcmp(aToken,"SUP") == 0)
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
									aOCSchema->fParentOCs.insert(aOCSchema->fParentOCs.begin(),aToken);
									//get the other SUP entries
									bSkipToTag = true;
									while (bSkipToTag)
									{
										aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
										if (aToken == nil)
										{
											break;
										}
										bSkipToTag = IsTokenNotATag(aToken);
										if (bSkipToTag)
										{
											aOCSchema->fParentOCs.insert(aOCSchema->fParentOCs.begin(),aToken);
										}
									}
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"ABSTRACT") == 0)
								{
									aOCSchema->fType = 0;
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"STRUCTURAL") == 0)
								{
									aOCSchema->fType = 1;
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"AUXILIARY") == 0)
								{
									aOCSchema->fType = 2;
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"MUST") == 0)
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
									aOCSchema->fRequiredAttrs.insert(aOCSchema->fRequiredAttrs.begin(),aToken);
									//get the other MUST entries
									bSkipToTag = true;
									while (bSkipToTag)
									{
										aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
										if (aToken == nil)
										{
											break;
										}
										bSkipToTag = IsTokenNotATag(aToken);
										if (bSkipToTag)
										{
											aOCSchema->fRequiredAttrs.insert(aOCSchema->fRequiredAttrs.begin(),aToken);
										}
									}
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"MAY") == 0)
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
									aOCSchema->fAllowedAttrs.insert(aOCSchema->fAllowedAttrs.begin(),aToken);
									//get the other MAY entries
									bSkipToTag = true;
									while (bSkipToTag)
									{
										aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
										if (aToken == nil)
										{
											break;
										}
										bSkipToTag = IsTokenNotATag(aToken);
										if (bSkipToTag)
										{
											aOCSchema->fAllowedAttrs.insert(aOCSchema->fAllowedAttrs.begin(),aToken);
										}
									}
									if (aToken == nil)
									{
										continue;
									}
								}
								
							} // for each bValues[i]
							
							if (lineEntry != nil) //delimiter chars will be overwritten by NULLs
							{
								DSFree(lineEntry);
							}
							
							ldap_value_free_len(bValues);
							
							pConfig->fObjectClassSchema = aOCClassMap;
							
						} // if bValues = ldap_get_values_len ...
												
						if (pAttr != nil)
						{
							ldap_memfree( pAttr );
						}
						
					} // for ( loop over ldap_next_attribute )
					
					if (ber != nil)
					{
						ber_free( ber, 0 );
					}
					
					ldap_msgfree( LDAPResult );
					pConfig->bOCBuilt = true;
				}
			}
			if( pConfig != nil )
			{
				gpConfigFromXML->ConfigUnlock( pConfig );
			}
			UnLockSession(inContext);
		}
	}

} // GetSchema

// ---------------------------------------------------------------------------
//	* ParseLDAPNodeName
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::ParseLDAPNodeName(	char	   *inNodeName,
										char	  **outLDAPName,
										int		   *outLDAPPort )
{
	sInt32			siResult	= eDSNoErr;
	char		   *portPos		= nil;
	uInt32			inLength	= 0;
	int				ldapPort	= LDAP_PORT;
	char		   *ldapName	= nil;
	
//parse a string with a name and possibly a suffix of ':' followed by a port number

	if (inNodeName != nil)
	{
		inLength	= strlen(inNodeName);
		portPos		= strchr(inNodeName, ':');
		//check if ':' found
		if (portPos != nil)
		{
			portPos++;
			//check if nothing after ':'
			if (portPos != nil)
			{
				ldapPort = strtoul(portPos,NULL,0);
				//if error in conversion set back to default
				if (ldapPort == 0)
				{
					ldapPort = LDAP_PORT;
				}
				
				inLength = inLength - strlen(portPos);					
			}
			//strip off the suffix ':???'
			ldapName = (char *) calloc(1, inLength);
			strncpy(ldapName, inNodeName, inLength-1);
		}
		else
		{
			ldapName = (char *) calloc(1, inLength+1);
			strncpy(ldapName, inNodeName, inLength);
		}
		
		*outLDAPName	= ldapName;
		*outLDAPPort	= ldapPort;
	}
	else
	{
		siResult = eDSNullParameter;
	}

	return(siResult);

}// ParseLDAPNodeName


void CLDAPNode::RereadDefinedReplicas( sLDAPNodeStruct *inLDAPNodeStruct )
{
	sLDAPConfigData		*pConfig	= nil;

	if (inLDAPNodeStruct != NULL)
	{
		//check if we need to retrieve the server mappings
		pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inLDAPNodeStruct->fNodeName );
		
		if( pConfig != nil && !(pConfig->bLDAPv2ReadOnly) )
			pConfig->bBuildReplicaList = true;
		
		if( pConfig != NULL )
			gpConfigFromXML->ConfigUnlock( pConfig );
	}
}// RereadDefinedReplicas


// ---------------------------------------------------------------------------
//	* BindProc
// ---------------------------------------------------------------------------

sInt32 CLDAPNode::BindProc ( sLDAPNodeStruct *inLDAPNodeStruct, bool bForceBind, bool bCheckPasswordOnly, bool bNeedWriteable )
{
    sInt32				siResult		= eDSNoErr;
    int					bindMsgId		= 0;
	int					version			= -1;
	char			   *ldapPasswd		= nil;
    sLDAPConfigData	   *pConfig			= nil;
    int					openTO			= kLDAPDefaultOpenCloseTimeoutInSeconds;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	bool				bLDAPv2ReadOnly	= false;
	
	try
	{
		if ( inLDAPNodeStruct == nil ) throw( (sInt32)eDSNullParameter );
		
		if ( inLDAPNodeStruct->bBadSession ) throw( (sInt32)eDSCannotAccessSession );

		if ( inLDAPNodeStruct->fConnectionStatus == kConnectionUnsafe && bForceBind == false ) throw( (sInt32)eDSCannotAccessSession );

		if ( inLDAPNodeStruct->fConnectionStatus == kConnectionUnknown && bForceBind == false && fInStartupState == true )
		{
			DSSemaphore timedWait;
			timedWait.Wait( (uInt32)(1 * kMilliSecsPerSec) );       // wait one second just to make sure we let the CheckFailed thread
																	// get a chance to make the connection

			if ( inLDAPNodeStruct->fConnectionStatus == kConnectionUnknown && fInStartupState == true )
				throw( (sInt32)eDSCannotAccessSession );
		}

		CLDAPv3Plugin::WaitForNetworkTransitionToFinish();
		inLDAPNodeStruct->SessionMutexWait();

        // Here is the bind to the LDAP server
		// Note that there may be stored name/password in the config table
		// ie. always use the config table data if authentication has not explicitly been set
		// use LDAPAuthNodeMap if inLDAPNodeStruct contains a username
		
		//check that we were already here
		if (inLDAPNodeStruct->fHost == NULL)
		{
			//retrieve the config data
			pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inLDAPNodeStruct->fNodeName );
			if (pConfig != nil)
			{
				// if no authCall active, then we should use the config settings
				if( !inLDAPNodeStruct->bAuthCallActive )
				{
					// if we have a config, let's just erase the credentials and re-read them
					// into the NodeStruct.  This allows us rebind, even if we have no config
					DSDelete( inLDAPNodeStruct->fLDAPUserName );
					DSDelete( inLDAPNodeStruct->fKerberosId );
					DSDelete( inLDAPNodeStruct->fLDAPAuthType );
					
					DSFree( inLDAPNodeStruct->fLDAPCredentials );

					if ( pConfig->bSecureUse )
					{
						inLDAPNodeStruct->fLDAPUserName = (pConfig->fServerAccount ? strdup(pConfig->fServerAccount) : nil);
						inLDAPNodeStruct->fKerberosId = (pConfig->fKerberosId ? strdup(pConfig->fKerberosId) : nil);
						inLDAPNodeStruct->fLDAPCredentials = (void *) (pConfig->fServerPassword ? strdup( pConfig->fServerPassword ) : NULL);
						inLDAPNodeStruct->fLDAPCredentialsLen = (pConfig->fServerPassword ? strlen( pConfig->fServerPassword ) : 0);
					}
				}				
				openTO = pConfig->fOpenCloseTimeout;
			}
			
			if (inLDAPNodeStruct->fLDAPCredentials != nil)
			{
				//auth type of clear text means char * password
				// if nil we dup the password, or if it is AuthClearText.
				if ( inLDAPNodeStruct->fLDAPCredentials && (inLDAPNodeStruct->fLDAPAuthType == nil || strcmp(inLDAPNodeStruct->fLDAPAuthType,kDSStdAuthClearText) == 0) )
				{
					ldapPasswd = strdup( (char *) inLDAPNodeStruct->fLDAPCredentials );
				}
			}

			// if we are in an unknown state, and we are not forcebinding (i.e., checkFailed), let's wait
			if( !bForceBind && inLDAPNodeStruct->fConnectionStatus == kConnectionUnknown )
			{
				//first lets spawn our checking thread if this has not already been done
				EnsureCheckFailedConnectionsThreadIsRunning();

				// we need to unlock this session so the thread can check it out of band..
				inLDAPNodeStruct->SessionMutexSignal();
				
				// let's go to sleep over the designated open/close timeout value
				// and check every .5 secs if it becomes available.
				DSSemaphore		timedWait;
				double			waitTime	= dsTimestamp() + USEC_PER_SEC*openTO;
				while(inLDAPNodeStruct->fConnectionStatus == kConnectionUnknown)
				{
					// Check every .5 seconds
					timedWait.Wait( (uInt32)(.5 * kMilliSecsPerSec) );

					// check over max
					if ( dsTimestamp() > waitTime )
					{
						// We have waited as long as we are going to at this time
						break;
					}
				}
	
				CLDAPv3Plugin::WaitForNetworkTransitionToFinish();
				inLDAPNodeStruct->SessionMutexWaitWithFunctionName("CLDAPNode::BindProc(2)");

				// let's also reset the last used one so we try the full suite of replicas up front
				if (pConfig != nil && pConfig->fReplicaHosts)
					pConfig->fReplicaHosts->resetLastUsed();
			}

			if( !bForceBind && inLDAPNodeStruct->fConnectionStatus != kConnectionSafe )
			{
				throw( (sInt32)eDSCannotAccessSession );
			}
			
			// if configName not NULL, then we were based on a config originally
			if (inLDAPNodeStruct->fNodeName != NULL )
			{
				inLDAPNodeStruct->fHost = InitLDAPConnection( inLDAPNodeStruct, pConfig, bNeedWriteable ); // pass, even if NULL
			}
			else if( inLDAPNodeStruct->fDirectServerName )
			{
				//directed open with no configuration will not support replica picking				
				inLDAPNodeStruct->fHost = ldap_init( inLDAPNodeStruct->fDirectServerName, inLDAPNodeStruct->fDirectLDAPPort );
				inLDAPNodeStruct->setLastLDAPServer( inLDAPNodeStruct->fDirectServerName );
				SetNetworkTimeoutsForHost( inLDAPNodeStruct->fHost, kLDAPDefaultNetworkTimeoutInSeconds );
			}
			
			// since we did an InitLDAPConnection, which does Establish, we can safely assume we've failed all together
			if ( inLDAPNodeStruct->fHost == nil )
			{
				// if we are authing an existing connection, AuthOpen, then we don't fail due to a bind cause it could be a bad password
				if( !bCheckPasswordOnly )
				{
					//log this failed connection, if we aren't force binding.. if force binding, no need to log this detail
					if (bForceBind)
					{
						inLDAPNodeStruct->fConnectionStatus = kConnectionUnsafe;
						DBGLOG( kLogPlugin, "CLDAPNode: BindProc SETTING kConnectionUnsafe 1" );
					}
					else
					{
						LogFailedConnection("InitLDAPConnection or ldap_init failure", inLDAPNodeStruct->fLDAPServer, inLDAPNodeStruct->fDelayRebindTry);
						inLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
					}

					inLDAPNodeStruct->fDelayedBindTime = time( nil ) + inLDAPNodeStruct->fDelayRebindTry;
				}
				throw( (sInt32)eDSCannotAccessSession );
			}
			else
			{
				if (pConfig != nil)
				{
					if ( pConfig->bIsSSL && !(pConfig->bLDAPv2ReadOnly) )
					{
						int ldapOptVal = LDAP_OPT_X_TLS_HARD;
						ldap_set_option(inLDAPNodeStruct->fHost, LDAP_OPT_X_TLS, &ldapOptVal);
					}
					
					if ( pConfig->bLDAPv2ReadOnly ) bLDAPv2ReadOnly = true;
					
					ldap_set_option(inLDAPNodeStruct->fHost, LDAP_OPT_REFERRALS, (pConfig->bReferrals ? LDAP_OPT_ON : LDAP_OPT_OFF) );
				}
				
				if (!bLDAPv2ReadOnly)
				{
					// SASLMethods are first so that we can do non-clear text auth methods
					CheckSASLMethods( inLDAPNodeStruct );

					/* LDAPv3 only */
					version = LDAP_VERSION3;
					ldap_set_option( inLDAPNodeStruct->fHost, LDAP_OPT_PROTOCOL_VERSION, &version );
		
					// let's do a SASLbind if possible
					ldapReturnCode = doSASLBindAttemptIfPossible( inLDAPNodeStruct->fHost, pConfig, inLDAPNodeStruct->fLDAPUserName, ldapPasswd, inLDAPNodeStruct->fKerberosId );
				}
				
				// if we didn't have a local error or LDAP_OTHER, then we either failed or succeeded so no need to continue
				if( ldapReturnCode != LDAP_LOCAL_ERROR && ldapReturnCode != LDAP_OTHER && !bLDAPv2ReadOnly )
				{
					// if STRONG_AUTH_REQUIRED, means policy violation during SASLBind process, we handle special
					if( ldapReturnCode == LDAP_STRONG_AUTH_REQUIRED )
					{
						// if we're just checking password no need to do anything but return eDSAuthFailed
						// all other cases, whether bound or using config should return eDSCannotAccessSession 
						if( bCheckPasswordOnly )
						{
							DBGLOG( kLogPlugin, "CLDAPNode: Node Authentication failed" );
							throw( (sInt32) eDSAuthFailed );
						}

						// log if we don't have an auth call active
						if( inLDAPNodeStruct->bAuthCallActive == false )
						{
							inLDAPNodeStruct->fConnectionStatus = kConnectionUnsafe;
							DBGLOG( kLogPlugin, "CLDAPNode: BindProc SETTING kConnectionUnsafe 2" );
							inLDAPNodeStruct->fDelayedBindTime = time( nil ) + 3600;

							syslog(LOG_ALERT,"DSLDAPv3PlugIn: Policy Violation.  Disabled future attempts to bind to [%s] for 1 hour.", inLDAPNodeStruct->fLDAPServer );
							DBGLOG1( kLogPlugin, "CLDAPNode: Policy Violation.  Disabled future attempts to bind to [%s] for 1 hour.", inLDAPNodeStruct->fLDAPServer );
						}
						throw( (sInt32)eDSCannotAccessSession );
					}
					// if we didn't have a success then we failed for some reason
					else if( ldapReturnCode != LDAP_SUCCESS )
					{
						if( bCheckPasswordOnly )
						{
							DBGLOG( kLogPlugin, "CLDAPNode: Node Authentication failed" );
							throw( (sInt32) eDSAuthFailed );
						}

						if( ldapReturnCode == LDAP_INVALID_CREDENTIALS )
						{
							inLDAPNodeStruct->bBadSession = true;
							DBGLOG( kLogPlugin, "CLDAPNode: Failed doing SASL Authentication - bad credentials" );
						}
						else
						{
							DBGLOG( kLogPlugin, "CLDAPNode: Failed doing SASL Authentication" );
						}

						if (bForceBind)
						{
							inLDAPNodeStruct->fConnectionStatus = kConnectionUnsafe;
							DBGLOG( kLogPlugin, "CLDAPNode: BindProc SETTING kConnectionUnsafe 3" );
						}
						else
							inLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;

						inLDAPNodeStruct->fDelayedBindTime = time( nil ) + inLDAPNodeStruct->fDelayRebindTry;
						throw( (sInt32)eDSCannotAccessSession );
					}
				}
				// if we don't have pConfig, we're ok..
				// if we are in SSL mode
				// if we allow clear text binds
				// if we don't allow clear text binds but don't have any credentials, we're doing anonymous it's OK
				else if( pConfig == NULL || pConfig->bIsSSL || (pConfig->fSecurityLevel & kSecDisallowCleartext) == 0 || 
						 (pConfig->fSecurityLevel & kSecDisallowCleartext == kSecDisallowCleartext && (DSIsStringEmpty(inLDAPNodeStruct->fLDAPUserName) || inLDAPNodeStruct->fLDAPCredentialsLen == 0) ) ) 
				{
					timeval	tv = { 0 };
					char	*pErrorString = NULL;
				
					if (bLDAPv2ReadOnly)
					{
						DBGLOG( kLogPlugin, "CLDAPNode: SASL Authentication not attempted with LDAPv2, failing through to bind" );
					}
					else if ( DSIsStringEmpty(inLDAPNodeStruct->fLDAPUserName) == false && inLDAPNodeStruct->fLDAPCredentialsLen )
					{
						// if we couldn't do a SASL, let's fallback to normal bind.
						DBGLOG( kLogPlugin, "CLDAPNode: SASL Authentication didn't work, failing through to bind" );
					}

					if (inLDAPNodeStruct->fHost != nil)
					{
						//this is our and only our LDAP session for now
						//need to use our timeout so we don't hang indefinitely
						SetNetworkTimeoutsForHost( inLDAPNodeStruct->fHost, kLDAPDefaultNetworkTimeoutInSeconds );

						bindMsgId = ldap_simple_bind( inLDAPNodeStruct->fHost, inLDAPNodeStruct->fLDAPUserName, ldapPasswd );

						tv.tv_sec		= openTO;
						ldapReturnCode	= ldap_result( inLDAPNodeStruct->fHost, bindMsgId, 0, &tv, &result );
						
						if ( ldapReturnCode == -1 )
						{
							pErrorString = "Bind failure - Server Down";
						}
						else if ( ldapReturnCode == 0 )
						{
							// timeout no need to abandon bindmessage as binds don't require abandon's per docs
							pErrorString = "Bind timeout";
						}
						else if ( ldap_result2error(inLDAPNodeStruct->fHost, result, 1) != LDAP_SUCCESS )
						{
							pErrorString = "Bind failure";
						}
					}
					else
					{
						pErrorString = "Unable to obtain a LDAP handle";
					}
					
					if( pErrorString )
					{
						// if we are authing and existing connection, AuthOpen, then we don't fail due to a bind cause it could be a bad password
						if( bCheckPasswordOnly )
						{
							DBGLOG( kLogPlugin, "CLDAPNode: Node Authentication failed" );
						}
						else
						{
							//log this failed connection, unless we are force binding.. no reason to log, it is already failed
							if (bForceBind)
							{
								inLDAPNodeStruct->fConnectionStatus = kConnectionUnsafe;
								DBGLOG( kLogPlugin, "CLDAPNode: BindProc SETTING kConnectionUnsafe 4" );
								if( DSIsStringEmpty(inLDAPNodeStruct->fLDAPUserName) || inLDAPNodeStruct->fLDAPCredentialsLen == 0 )
								{
									DBGLOG1( kLogPlugin, "CLDAPNode: Anonymous Bind Unsuccessful - Retry in %d seconds", inLDAPNodeStruct->fDelayRebindTry );
								}
								else
								{
									DBGLOG1( kLogPlugin, "CLDAPNode: Cleartext Bind Authentication Unsuccessful - Retry in %d seconds", inLDAPNodeStruct->fDelayRebindTry );
								}
							}
							else
							{
								LogFailedConnection( pErrorString, inLDAPNodeStruct->fLDAPServer, inLDAPNodeStruct->fDelayRebindTry );
								inLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
							}

							inLDAPNodeStruct->fDelayedBindTime = time( nil ) + inLDAPNodeStruct->fDelayRebindTry;
						}
						
						throw( (sInt32)eDSCannotAccessSession );
					}
					if( DSIsStringEmpty(inLDAPNodeStruct->fLDAPUserName) || inLDAPNodeStruct->fLDAPCredentialsLen == 0 )
					{
						DBGLOG( kLogPlugin, "CLDAPNode: Anonymous Bind Successful" );
					}
					else
					{
						DBGLOG( kLogPlugin, "CLDAPNode: Cleartext Bind Authentication Successful" );
					}
				}
				else
				{
					DBGLOG1( kLogPlugin, "CLDAPNode: ClearText binds disallowed by Policy.  LDAP Connection to server %s denied.", inLDAPNodeStruct->fLDAPServer );
					syslog( LOG_ALERT,"DSLDAPv3PlugIn: ClearText binds disallowed by Policy.  LDAP Connection to server %s denied.", inLDAPNodeStruct->fLDAPServer );

					if( !bCheckPasswordOnly )
					{
						inLDAPNodeStruct->fConnectionStatus = kConnectionUnsafe;
						DBGLOG( kLogPlugin, "CLDAPNode: BindProc SETTING kConnectionUnsafe 5" );
						inLDAPNodeStruct->fDelayedBindTime = time( nil ) + (inLDAPNodeStruct->fDelayRebindTry ? inLDAPNodeStruct->fDelayRebindTry : kLDAPDefaultRebindTryTimeoutInSeconds);
					}
					
					throw( (sInt32)eDSCannotAccessSession );
				}

				// if we weren't safe before, we need to notify that the search policy may have changed.
				if ( inLDAPNodeStruct->fConnectionStatus != kConnectionSafe )
					   gSrvrCntl->NodeSearchPolicyChanged();

				inLDAPNodeStruct->fConnectionStatus = kConnectionSafe;
			}

			//result is consumed above within ldap_result2error
			result = nil;
		}
		
	} // try
	
	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	if( siResult != eDSNoErr && inLDAPNodeStruct && inLDAPNodeStruct->fHost )
	{
		// we need to unbind and throw this connection away, otherwise we won't rebind correctly
		ldap_unbind_ext( inLDAPNodeStruct->fHost, NULL, NULL );
		inLDAPNodeStruct->fHost = NULL;
	}
	
	DSDelete( ldapPasswd );

	if( pConfig )
	{
		gpConfigFromXML->ConfigUnlock( pConfig );
	}

	if( inLDAPNodeStruct )
	{
		inLDAPNodeStruct->SessionMutexSignalWithFunctionName("CLDAPNode::BindProc(2)");
	}
	
	return (siResult);
	
}// BindProc


//------------------------------------------------------------------------------------
//	* GetNamingContexts
//------------------------------------------------------------------------------------

char** CLDAPNode::GetNamingContexts( LDAP *inHost, int inSearchTO, uInt32 *outCount )
{
	sInt32				siResult			= eDSRecordNotFound;
	bool				bResultFound		= false;
    int					ldapMsgId			= 0;
	LDAPMessage		   *result				= nil;
	int					ldapReturnCode		= 0;
	char			   *attrs[2]			= {"namingContexts",NULL};
	BerElement		   *ber					= nil;
	struct berval	  **bValues				= nil;
	char			   *pAttr				= nil;
	char			  **outMapSearchBases	= nil;

	//search for the specific LDAP record namingContexts at the rootDSE which may contain
	//the list of LDAP server search bases
	
	// here is the call to the LDAP server asynchronously which requires
	// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
	// attribute list (NULL for all), return attrs values flag
	// Note: asynchronous call is made so that a MsgId can be used for future calls
	// This returns us the message ID which is used to query the server for the results
	
	*outCount = 0;
	
	ldapReturnCode = ldap_search_ext(	inHost,
										"",
										LDAP_SCOPE_BASE,
										"(objectclass=*)",
										attrs,
										0,
										NULL,
										NULL,
										0, 0, 
										&ldapMsgId );

	if (ldapReturnCode == LDAP_SUCCESS)
	{
		bResultFound = true;
		//retrieve the actual LDAP record data for use internally
		//useful only from the read-only perspective
		struct	timeval	tv;
		tv.tv_usec	= 0;
		if (inSearchTO == 0)
		{
			tv.tv_sec	= kLDAPDefaultOpenCloseTimeoutInSeconds; //since this may be an implicit bind and don't want to block forever
		}
		else
		{
			tv.tv_sec	= inSearchTO;
		}

		SetNetworkTimeoutsForHost( inHost, kLDAPDefaultNetworkTimeoutInSeconds );

		ldapReturnCode = ldap_result(inHost, ldapMsgId, 0, &tv, &result);
	}

	if (	(bResultFound) &&
			( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
	{
		//get the search base list here
		//parse the attributes in the result - should only be one ie. namingContexts
		pAttr = ldap_first_attribute (inHost, result, &ber );
		if (pAttr != nil)
		{
			if (( bValues = ldap_get_values_len (inHost, result, pAttr )) != NULL)
			{
				// calculate the number of values for this attribute
				uInt32 valCount = 0;
				for (int ii = 0; bValues[ii] != nil; ii++ )
				{
					valCount++;
				}
				outMapSearchBases = (char **) calloc( valCount+1, sizeof(char *));
				// for each value of the attribute we need to parse and add as an entry to the outList
				for (int i = 0; (bValues[i] != nil) && (bValues[i]->bv_val != nil); i++ )
				{
					outMapSearchBases[i] = strdup(bValues[i]->bv_val);
					(*outCount)++;
					siResult = eDSNoErr;
				}
				ldap_value_free_len(bValues);
			} // if bValues = ldap_get_values_len ...
			ldap_memfree( pAttr );
		} // if pAttr != nil
			
		if (ber != nil)
		{
			ber_free( ber, 0 );
		}
			
		ldap_msgfree( result );
		result = nil;

	} // if bResultFound and ldapReturnCode okay
	else if (ldapReturnCode == LDAP_TIMEOUT)
	{
		siResult = eDSServerTimeout;
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
			outMapSearchBases = (char **) -1;   // this signifies a bad server/no response so we don't hang
		}
	}
	else
	{
		siResult = eDSRecordNotFound;
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}
	
	DSSearchCleanUp(inHost, ldapMsgId);
	
	return( outMapSearchBases );

} // GetNamingContexts


//------------------------------------------------------------------------------------
//	* GetSchemaMessage
//------------------------------------------------------------------------------------

sInt32 CLDAPNode::GetSchemaMessage ( LDAP *inHost, int inSearchTO, LDAPMessage **outResultMsg )
{
	sInt32				siResult		= eDSNoErr;
	bool				bResultFound	= false;
    int					ldapMsgId		= 0;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	char			   *sattrs[2]		= {"subschemasubentry",NULL};
	char			   *attrs[2]		= {"objectclasses",NULL};
	char			   *subschemaDN		= nil;
	BerElement		   *ber				= nil;
	struct berval	  **bValues			= nil;
	char			   *pAttr			= nil;

	try
	{
		//search for the specific LDAP record subschemasubentry at the rootDSE which contains
		//the "dn" of the subschema record
		
		// here is the call to the LDAP server asynchronously which requires
		// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
		// attribute list (NULL for all), return attrs values flag
		// Note: asynchronous call is made so that a MsgId can be used for future calls
		// This returns us the message ID which is used to query the server for the results
		if ( (ldapMsgId = ldap_search( inHost, "", LDAP_SCOPE_BASE, "(objectclass=*)", sattrs, 0) ) == -1 )
		{
			bResultFound = false;
		}
		else
		{
			bResultFound = true;
			//retrieve the actual LDAP record data for use internally
			//useful only from the read-only perspective
			struct	timeval	tv;
			tv.tv_usec	= 0;
			if (inSearchTO == 0)
			{
				tv.tv_sec	= kLDAPDefaultSearchTimeoutInSeconds; // don't want to block forever
			}
			else
			{
				tv.tv_sec	= inSearchTO;
			}

			SetNetworkTimeoutsForHost( inHost, kLDAPDefaultNetworkTimeoutInSeconds );
			
			ldapReturnCode = ldap_result(inHost, ldapMsgId, 0, &tv, &result);
		}
	
		if (	(bResultFound) &&
				( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
		{
			siResult = eDSNoErr;
			//get the subschemaDN here
			//parse the attributes in the result - should only be one ie. subschemasubentry
			for (	pAttr = ldap_first_attribute (inHost, result, &ber );
						pAttr != NULL; pAttr = ldap_next_attribute(inHost, result, ber ) )
			{
				if (( bValues = ldap_get_values_len (inHost, result, pAttr )) != NULL)
				{					
					// should be only one value of the attribute
					if ( bValues[0] != NULL )
					{
						subschemaDN = (char *) calloc(1, bValues[0]->bv_len + 1);
						strcpy(subschemaDN,bValues[0]->bv_val);
					}
					
					ldap_value_free_len(bValues);
				} // if bValues = ldap_get_values_len ...
											
				if (pAttr != nil)
				{
					ldap_memfree( pAttr );
				}
					
			} // for ( loop over ldap_next_attribute )
				
			if (ber != nil)
			{
				ber_free( ber, 0 );
			}
				
			ldap_msgfree( result );
			result = nil;

		} // if bResultFound and ldapReturnCode okay
		else if (ldapReturnCode == LDAP_TIMEOUT)
		{
	     	siResult = eDSServerTimeout;
			if ( result != nil )
			{
				ldap_msgfree( result );
				result = nil;
			}
		}
		else
		{
	     	siResult = eDSRecordNotFound;
			if ( result != nil )
			{
				ldap_msgfree( result );
				result = nil;
			}
		}
		
		DSSearchCleanUp(inHost, ldapMsgId);
	
		if (subschemaDN != nil)
		{
			//here we call to get the actual subschema record
			
			//here is the call to the LDAP server asynchronously which requires
			// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
			// attribute list (NULL for all), return attrs values flag
			// Note: asynchronous call is made so that a MsgId can be used for future calls
			// This returns us the message ID which is used to query the server for the results
			if ( (ldapMsgId = ldap_search( inHost, subschemaDN, LDAP_SCOPE_BASE, "(objectclass=subSchema)", attrs, 0) ) == -1 )
			{
				bResultFound = false;
			}
			else
			{
				bResultFound = true;
				//retrieve the actual LDAP record data for use internally
				//useful only from the read-only perspective
				//KW when write capability is added, we will need to re-read the result after a write
				struct	timeval	tv;
				tv.tv_usec	= 0;
				if (inSearchTO == 0)
				{
					tv.tv_sec	= kLDAPDefaultSearchTimeoutInSeconds; // don't want to block forever
				}
				else
				{
					tv.tv_sec	= inSearchTO;
				}
				ldapReturnCode = ldap_result(inHost, ldapMsgId, 0, &tv, &result);
			}
			
			DSFree( subschemaDN );
		
			if (	(bResultFound) &&
					( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
			{
				siResult = eDSNoErr;
			} // if bResultFound and ldapReturnCode okay
			else if (ldapReturnCode == LDAP_TIMEOUT)
			{
				siResult = eDSServerTimeout;
				if ( result != nil )
				{
					ldap_msgfree( result );
					result = nil;
				}
			}
			else
			{
				siResult = eDSRecordNotFound;
				if ( result != nil )
				{
					ldap_msgfree( result );
					result = nil;
				}
			}

			DSSearchCleanUp(inHost, ldapMsgId);
	
		}
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if (result != nil)
	{
		*outResultMsg = result;
	}

	return( siResult );

} // GetSchemaMessage

//------------------------------------------------------------------------------------
//	* IsTokenNotATag
//------------------------------------------------------------------------------------

bool CLDAPNode::IsTokenNotATag ( char *inToken )
{
	
	if (inToken == nil)
	{
		return true;
	}
	
	//check for first char in inToken as an uppercase letter in the following set
	//"NDOSAMX" since that will cover the following tags
	//NAME,DESC,OBSOLETE,SUP,ABSTRACT,STRUCTURAL,AUXILIARY,MUST,MAY,X-ORIGIN

	switch(*inToken)
	{
		case 'N':
		case 'D':
		case 'O':
		case 'S':
		case 'A':
		case 'M':
		case 'X':
		
			if (strcmp(inToken,"DESC") == 0)
			{
				return false;
			}
		
			if (strcmp(inToken,"SUP") == 0)
			{
				return false;
			}
			
			if (strlen(inToken) > 7)
			{
				if (strcmp(inToken,"OBSOLETE") == 0)
				{
					return false;
				}
		
				if (strcmp(inToken,"ABSTRACT") == 0)
				{
					return false;
				}
			
				if (strcmp(inToken,"STRUCTURAL") == 0)
				{
					return false;
				}
			
				if (strcmp(inToken,"AUXILIARY") == 0)
				{
					return false;
				}

				if (strcmp(inToken,"X-ORIGIN") == 0) //appears that iPlanet uses a non-standard tag ie. post RFC 2252
				{
					return false;
				}
			}
		
			if (strcmp(inToken,"MUST") == 0)
			{
				return false;
			}
		
			if (strcmp(inToken,"MAY") == 0)
			{
				return false;
			}
		
			if (strcmp(inToken,"NAME") == 0)
			{
				return false;
			}
			break;
		default:
			break;
	}

	return( true );

} // IsTokenNotATag

// ---------------------------------------------------------------------------
//	* Lock Session
// ---------------------------------------------------------------------------

LDAP* CLDAPNode::LockSession( sLDAPContextData *inContext )
{
	LDAP	*returnValue			= nil;

	if (inContext != nil)
	{
		// let's get a pointer so we don't keep double-dereferencing
		sLDAPNodeStruct *pLDAPNodeStruct = inContext->fLDAPNodeStruct;
		
		// Now let's wait for the pLDAPNode host to become available and lock it..
		pLDAPNodeStruct->SessionMutexWait();
		
		// we don't increment RefCount, cause we did that when it was added to the sLDAPContextData
		// we have mutex so we can change things as we please
		pLDAPNodeStruct->fConnectionActiveCount++;
		pLDAPNodeStruct->fIdleTOCount = 0;  // reset idleCount since we just did something...
		
		// if pLDAPNodeStruct->fHost == NULL, let's rebind now..
		if( pLDAPNodeStruct->fHost == NULL )
		{
			BindProc( pLDAPNodeStruct, false, false, pLDAPNodeStruct->bAuthCallActive );
		}
		
		// now let's set the return value..
		returnValue = pLDAPNodeStruct->fHost;
	}
	return returnValue;
} // LockSession

// ---------------------------------------------------------------------------
//	* UnLock Session
// ---------------------------------------------------------------------------

void CLDAPNode::UnLockSession( sLDAPContextData *inContext, bool inHasFailed  )
{
	if (inContext != nil)
	{
		// let's get a pointer so we don't keep double-dereferencing
		sLDAPNodeStruct  *pLDAPNodeStruct = inContext->fLDAPNodeStruct;

		// we have mutex so we can change things as we please
		pLDAPNodeStruct->fConnectionActiveCount--;
		
		if (inHasFailed)
		{
			//log this failed connection in a search operation
			LogFailedConnection( "Search connection failure", pLDAPNodeStruct->fLDAPServer, pLDAPNodeStruct->fDelayRebindTry );
			pLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
			pLDAPNodeStruct->fDelayedBindTime = time( nil ) + inContext->fLDAPNodeStruct->fDelayRebindTry;
			
			// let's unbind and so that we bind next time...
			ldap_unbind_ext( pLDAPNodeStruct->fHost, NULL, NULL );
			pLDAPNodeStruct->fHost = NULL;
		}
			
		// Now let's Signal that the pLDAPNode is available...
		pLDAPNodeStruct->SessionMutexSignal();
	}
} //UnLockSession

//------------------------------------------------------------------------------------
//	* CredentialChange
//------------------------------------------------------------------------------------

void CLDAPNode::CredentialChange( sLDAPNodeStruct *inLDAPNodeStruct, char *inUserDN )
{
	LDAPNodeMapI	aLDAPNodeMapI;
	LDAPNodeMap		aLDAPNodeMap;
	string			nodeString = inLDAPNodeStruct->fNodeName;
	int				length = 0;
	char			*pNodeName = NULL;
	
	nodeString += ":";
	nodeString += inUserDN;
	
	length = nodeString.length();
	pNodeName = (char *)nodeString.c_str();
	
	NodeOpenMutexWait();
	
	for (aLDAPNodeMapI = fLDAPNodeMap.begin(); aLDAPNodeMapI != fLDAPNodeMap.end(); ++aLDAPNodeMapI)
	{
		// if it isn't the node we passed in and it has same user, let's invalidate it
		if( inLDAPNodeStruct != aLDAPNodeMapI->second && 
			strncmp( pNodeName, aLDAPNodeMapI->first.c_str(), length ) == 0 )
		{
			aLDAPNodeMapI->second->bBadSession = true;
			DBGLOG1( kLogPlugin, "CLDAPNode: Invalidated session %s due to credential change", aLDAPNodeMapI->first.c_str() );
		}
	}
	
	NodeOpenMutexSignal();
} //CredentialChange

// ---------------------------------------------------------------------------
//     * Check Idles
// ---------------------------------------------------------------------------

void CLDAPNode::CheckIdles( void )
{
	sLDAPNodeStruct	   *pLDAPNodeStruct = nil;
	LDAPNodeMapI		aLDAPNodeMapI;
	bool				bShouldWeCheck  = false;
	stack<string>		unusedStack;

	NodeOpenMutexWait();
	
	for (aLDAPNodeMapI = fLDAPNodeMap.begin(); CLDAPv3Plugin::HandlingNetworkTransition() == false && aLDAPNodeMapI != fLDAPNodeMap.end(); ++aLDAPNodeMapI)
	{
		pLDAPNodeStruct = aLDAPNodeMapI->second;
		
		if (pLDAPNodeStruct->fConnectionStatus != kConnectionSafe)
		{
			bShouldWeCheck = true;
		}
		
		// need to grab the mutex, since we don't go to the MapTable everytime anymore...
		pLDAPNodeStruct->SessionMutexWait();

		if (pLDAPNodeStruct->fConnectionActiveCount == 0 && pLDAPNodeStruct->fHost) //no active connections and an fHost
		{
			if (pLDAPNodeStruct->fIdleTOCount >= pLDAPNodeStruct->fIdleTO) //idle timeout has expired
			{
				ldap_unbind( pLDAPNodeStruct->fHost ); //unbind the connection and nil out
				pLDAPNodeStruct->fHost = nil;
				pLDAPNodeStruct->fIdleTOCount = 0;
				
				DBGLOG2( kLogPlugin, "CLDAPNode: Status Node: %s -- Server: %s -- Idle Disconnected", aLDAPNodeMapI->first.c_str(), pLDAPNodeStruct->fLDAPServer );
				
			}
			else
			{
				pLDAPNodeStruct->fIdleTOCount++;
				DBGLOG3( kLogPlugin, "CLDAPNode: Status Node: %s -- Server: %s - Time: %d sec -- Idle", aLDAPNodeMapI->first.c_str(), pLDAPNodeStruct->fLDAPServer, pLDAPNodeStruct->fIdleTOCount * 30 );
			}
		}
		// if we have no references to the structure, let's delete the structure too
		else if( pLDAPNodeStruct->fRefCount == 0  )
		{
			// we only delete safe connections or authed connections, otherwise we don't know there is a problem for future connections..
			if( pLDAPNodeStruct->fConnectionStatus == kConnectionSafe && strchr(aLDAPNodeMapI->first.c_str(), ':') == NULL )
			{
				unusedStack.push( aLDAPNodeMapI->first );
			}
			else 
			{
				// we need to see if there is a config still, if not we'll remove anyway..
				sLDAPConfigData *pConfig = gpConfigFromXML->ConfigWithNodeNameLock( pLDAPNodeStruct->fNodeName );
				if( pConfig )
				{
					// still a config, do nothing... but need to unlock for other people
					gpConfigFromXML->ConfigUnlock( pConfig );
				}
				else
				{
					// well no config, push onto our remove list...
					unusedStack.push( aLDAPNodeMapI->first );
				}
			}
		}
		else 
		{
			pLDAPNodeStruct->fIdleTOCount = 0;
			// Not sure we want to spew the following message every 30 seconds...
			//DBGLOG3( kLogPlugin, "CLDAPNode: Status Node: %s -- Active: %d -- References: %d", aLDAPNodeMapI->first.c_str(), pLDAPNodeStruct->fConnectionActiveCount, pLDAPNodeStruct->fRefCount );
		}
		
		pLDAPNodeStruct->SessionMutexSignal();
	}

	// we go through our stack to remove connections..
	while( unusedStack.empty() == false )
	{
		aLDAPNodeMapI = fLDAPNodeMap.find( unusedStack.top() );

		if( aLDAPNodeMapI != fLDAPNodeMap.end() )
		{
			DBGLOG1( kLogPlugin, "CLDAPNode: Status Node: %s -- References: 0 -- removing from table", aLDAPNodeMapI->first.c_str() );
			
			DSDelete( aLDAPNodeMapI->second );
			fLDAPNodeMap.erase( unusedStack.top() );
		}
		unusedStack.pop();
	}

	NodeOpenMutexSignal();

	//check that there is actually at least one entry in the table that needs to be checked
	if (bShouldWeCheck)
	{
		// while we are here, let's also kick off the thread for checking failed..
		EnsureCheckFailedConnectionsThreadIsRunning();
	}
	
} //CheckIdles

// ---------------------------------------------------------------------------
//     * CheckFailed
// ---------------------------------------------------------------------------

void CLDAPNode::CheckFailed( void )
{
	// This function is called by the thread only...
	sLDAPNodeStruct	   *pLDAPNodeStruct = nil;
	LDAPNodeMapI		aLDAPNodeMapI;
	LDAPNodeMap			aLDAPNodeMap;

	//DBGLOG( kLogPlugin, "CLDAPNode: CheckFailed" );

	// we don't want to process failed connections right after a network transition,
	// let the active connections go first
	CLDAPv3Plugin::WaitForNetworkTransitionToFinish();
	
	// let's copy all the failed connections to a new map table....
	NodeOpenMutexWaitButNotForCheckFailedThread();

	for (aLDAPNodeMapI = fLDAPNodeMap.begin(); CLDAPv3Plugin::HandlingNetworkTransition() == false && aLDAPNodeMapI != fLDAPNodeMap.end(); ++aLDAPNodeMapI)
	{
		pLDAPNodeStruct = aLDAPNodeMapI->second;

		pLDAPNodeStruct->SessionMutexWait();
		
		if( pLDAPNodeStruct->fConnectionStatus != kConnectionSafe ) {

			// if the failed have a host, we should unbind from them and clear it
			if( pLDAPNodeStruct->fHost != nil )
			{
				ldap_unbind_ext( pLDAPNodeStruct->fHost, NULL, NULL );
				pLDAPNodeStruct->fHost = NULL;
			}
			
			if( pLDAPNodeStruct->bBadSession == false )
				aLDAPNodeMap[aLDAPNodeMapI->first] = pLDAPNodeStruct;
		}
		
		pLDAPNodeStruct->SessionMutexSignal();
	}

	NodeOpenMutexSignal();	

	CLDAPv3Plugin::WaitForNetworkTransitionToFinish();
	for (aLDAPNodeMapI = aLDAPNodeMap.begin(); CLDAPv3Plugin::HandlingNetworkTransition() == false && aLDAPNodeMapI != aLDAPNodeMap.end(); ++aLDAPNodeMapI)
	{
		pLDAPNodeStruct = aLDAPNodeMapI->second;
		
		// if we have an Unknown state or a failed connection and it is time to retry....
		if( pLDAPNodeStruct->fConnectionStatus == kConnectionUnknown || time( nil ) > pLDAPNodeStruct->fDelayedBindTime ) {
			
			DBGLOG1( kLogPlugin, "CLDAPNode: Checking failed Node: %s", aLDAPNodeMapI->first.c_str() );

			// Let's attempt a bind, with the override, since no one else will use it right now...
			sInt32	bindStatus = BindProc( pLDAPNodeStruct, true );
			
			if ( bindStatus )
				DBGLOG2( kLogPlugin, "CLDAPNode: Checking failed Node: %s returned bindStatus: %d", aLDAPNodeMapI->first.c_str(), bindStatus );
		}
	}
	
	fInStartupState = false;        // once we've done this once, we are no longer starting up

} //CheckFailed

// ---------------------------------------------------------------------------
//     * SystemGoingToSleep
// ---------------------------------------------------------------------------

void CLDAPNode::SystemGoingToSleep( void )
{
	//set a network change blocking flag at sleep
	gBlockLDAPNetworkChange = true;

	// let's flag all replicas to be rebuilt on network transition AFTER Wake... seems like the right thing to do...
	NodeOpenMutexWait();
	
	gpConfigFromXML->SetAllConfigBuildReplicaFlagTrue();
	
	// even if thread is going, let's flag all connections as unknown and reset time
	if( fLDAPNodeMap.size() > 0 )
	{
		LDAPNodeMapI	aLDAPNodeMapI;
		
		// let's loop through all connections and flag them all as unsafe until we get a network transition
		for (aLDAPNodeMapI = fLDAPNodeMap.begin(); aLDAPNodeMapI != fLDAPNodeMap.end(); ++aLDAPNodeMapI)
		{
			sLDAPNodeStruct *pLDAPNodeStruct = aLDAPNodeMapI->second;
			
			// we want connections to bail as quickly as possible so set status to unsafe.
			// on system wake, we will get a network transition that will set all the
			// connection statuses to unknown
			pLDAPNodeStruct->fConnectionStatus = kConnectionUnsafe;
			DBGLOG( kLogPlugin, "CLDAPNode: SystemGoingToSleep SETTING kConnectionUnsafe 1" );
			pLDAPNodeStruct->fDelayedBindTime = 0;
			
			pLDAPNodeStruct->SessionMutexWait();

			if ( pLDAPNodeStruct->fHost != nil ) //no active connections
			{
				ldap_unbind( pLDAPNodeStruct->fHost ); //unbind the connection and nil out
				pLDAPNodeStruct->fHost = nil;
			}
			pLDAPNodeStruct->SessionMutexSignal();
		}
	}

	NodeOpenMutexSignal();
	
} // SystemGoingToSleep


//------------------------------------------------------------------------------------
//	SystemWillPowerOn
//------------------------------------------------------------------------------------

void CLDAPNode::SystemWillPowerOn( void )
{
	//reset a network change blocking flag at wake
	gBlockLDAPNetworkChange = false;
}//SystemWillPowerOn


// ---------------------------------------------------------------------------
//     * NetTransition
// ---------------------------------------------------------------------------

void CLDAPNode::NetTransition( void )
{
	// let's flag all replicas to be rebuilt on network transition... seems like the right thing to do...
	NodeOpenMutexWaitButNotForCheckFailedThread();
	
	gpConfigFromXML->SetAllConfigBuildReplicaFlagTrue();
	
	// even if thread is going, let's flag all connections as unknown and reset time, otherwise
	// we may not recover from bad Security or for the reason, rebind time hasn't come up yet
	if( fLDAPNodeMap.size() > 0 )
	{
		LDAPNodeMapI	aLDAPNodeMapI;
		
		// let's loop through all connections and flag them all as unknown
		for (aLDAPNodeMapI = fLDAPNodeMap.begin(); aLDAPNodeMapI != fLDAPNodeMap.end(); ++aLDAPNodeMapI)
		{
			sLDAPNodeStruct *pLDAPNodeStruct = aLDAPNodeMapI->second;
			
			pLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
			pLDAPNodeStruct->fDelayedBindTime = 0;

			pLDAPNodeStruct->SessionMutexWait();

			if ( pLDAPNodeStruct->fHost != nil )
			{
				ldap_unbind( pLDAPNodeStruct->fHost ); //unbind the connection and nil out
				pLDAPNodeStruct->fHost = nil;
			}			
			pLDAPNodeStruct->SessionMutexSignal();
		}
		
		// while we are here, let's also kick off the thread for checking failed..
		EnsureCheckFailedConnectionsThreadIsRunning();
	}

	NodeOpenMutexSignal();
} //NetTransition

// ---------------------------------------------------------------------------
//     * InitLDAPConnection
// ---------------------------------------------------------------------------

LDAP* CLDAPNode::InitLDAPConnection( sLDAPNodeStruct *inLDAPNodeStruct, sLDAPConfigData *inConfig, bool bInNeedWriteable )
{
    LDAP               *outHost                 = NULL;
	sReplicaInfo	   *inOutList				= nil;
	sInt32				replicaSearchResult		= eDSNoErr;
	
//use 		inConfig->bBuildReplicaList 	as indicator
//note		struct addrinfo* fReplicaHosts	inside inConfig is the built list
//using 	char *fServerName				 from inConfig was the old way

//assumptions:
//- we have a config
//- need to check if we have built a replica list
//- if not then we try to build it now
//-	check if we already have a replica host list that we can use to get the replica list
//- if yes then we start to use it
//- we retain last used replica inside the config struct

	if (inConfig == NULL)
		return NULL;

	if (inConfig->bBuildReplicaList)
	{
		//if we don't have a list of replicas, let's start one
		if (inConfig->fReplicaHostnames == NULL)
		{
			inConfig->fReplicaHostnames = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		}

		// if we have a list, but it is empty... let's add the main host to the list...
		if( CFArrayGetCount(inConfig->fReplicaHostnames) == 0 )
		{
            CFStringRef cfServerString = CFStringCreateWithCString( NULL, inConfig->fServerName, kCFStringEncodingUTF8 );

			CFArrayAppendValue(inConfig->fReplicaHostnames, cfServerString);
            
            CFRelease( cfServerString );
            cfServerString = NULL;
		}

		//use the known replicas to fill out the addrinfo list, we don't care about writable yet
        //if we don't have a writeable list, we'll pass in the readable list so all replicas are flagged as writable as safety
        BuildReplicaInfoLinkList( &inOutList, inConfig->fReplicaHostnames, (inConfig->fWriteableHostnames ? inConfig->fWriteableHostnames : inConfig->fReplicaHostnames), inConfig->fServerPort );

        //now let's go look for current replica lists with what we already knew
        //config record starting point, altServer from rootDSE, then DNS
        replicaSearchResult = RetrieveDefinedReplicas(inLDAPNodeStruct, inConfig->fReplicaHostnames, inConfig->fWriteableHostnames, inConfig->fServerPort, &inOutList);
		
		// if we were server mappings we may not have had mappings so use the list we have for now
		if ( replicaSearchResult == eDSNoErr || replicaSearchResult == eDSNoStdMappingAvailable || inConfig->fReplicaHosts == nil )
		{
			// we should always have one, but just in case...
			if( inOutList )
			{
				// need to clean up the old list before we replace it...
                DSDelete( inConfig->fReplicaHosts );
				inConfig->fReplicaHosts = inOutList;
			}
		}
		
		if ( inOutList == NULL )
		{
			inConfig->bBuildReplicaList = true;
			if (inConfig->fReplicaHosts != NULL)
			{
                DSDelete( inConfig->fReplicaHosts );
			}
		}
		else if (inOutList->fAddrInfo == NULL)
		{
			sReplicaInfo *aPtr = inOutList->fNext;
			bool bAtLeastOneAddr = false;
			while (aPtr != NULL)
			{
				if (aPtr->fAddrInfo != NULL)
				{
					bAtLeastOneAddr = true;
					break;
				}
				aPtr = aPtr->fNext;
			}
			if (bAtLeastOneAddr)
			{
				inConfig->bBuildReplicaList = false;
			}
			else
			{
				inConfig->bBuildReplicaList = true;
				if (inConfig->fReplicaHosts != NULL)
				{
					// if our replica list is the same as the inOutList, then we need to NULL the pointer
					// since we are freeing the replicalist
					if( inConfig->fReplicaHosts == inOutList ) {
						inOutList = NULL;
					}
					DSDelete( inConfig->fReplicaHosts );
				}
			}
		}
		else
		{
			inConfig->bBuildReplicaList = false;
		}
		
		//here we need to save the hostnames of the replica list in the config file
		gpConfigFromXML->UpdateReplicaList(inConfig->fNodeName, inConfig->fReplicaHostnames, inConfig->fWriteableHostnames);
	}
    
	//try to catch case where AddrInfo list for some reason did not get updated although the config did when adding replicas
    if( inConfig->fReplicaHosts == NULL && inConfig->fReplicaHostnames != NULL )
    {
        BuildReplicaInfoLinkList( &inOutList, inConfig->fReplicaHostnames, inConfig->fWriteableHostnames, inConfig->fServerPort );
    }
	
	//case where inLDAPNodeStruct->fHost != nil means that connection was established within Replica Searching methods above
	if ( (replicaSearchResult != eDSCannotAccessSession) && (inLDAPNodeStruct->fHost == nil) )
	{
		//establish a connection using replicas OR
		//simply ldap_init
		if (inConfig->fReplicaHosts != nil)
		{
			//use the writeable hostnames to establish a connection if required
			outHost = EstablishConnection( inLDAPNodeStruct, inConfig->fReplicaHosts, inConfig->fServerPort, inConfig->fOpenCloseTimeout, bInNeedWriteable );
			//provide fallback to try local loopback address if the following 
			//conditions are met
			//ie. if ip address change occurred we need to be able to write the
			//change to ourselves
			//1-failed to connect to any of the replicas
			//2-we are running on server
			//3-looking for a writable replica
			if( outHost == nil && bInNeedWriteable )
			{
				// two cases to consider:
				// using loopback, check if this is a replica or not
				// or client needs to connect to the provided IP
				if ((strcmp( inConfig->fServerName, "127.0.0.1" ) == 0)
					 || (strcmp( inConfig->fServerName, "localhost" ) == 0))
				{
					if (gServerOS && !LocalServerIsLDAPReplica())
					{
						outHost = ldap_init( "127.0.0.1", inConfig->fServerPort );
						inLDAPNodeStruct->setLastLDAPServer( "127.0.0.1" );
					}
				}
				else
				{
					outHost = ldap_init( inConfig->fServerName, inConfig->fServerPort );
					inLDAPNodeStruct->setLastLDAPServer( inConfig->fServerName );
				}
			}
		}
		else //if no list built we fallback to original call
		{
			outHost = ldap_init( inConfig->fServerName, inConfig->fServerPort );
			inLDAPNodeStruct->setLastLDAPServer( inConfig->fServerName );

			// set some network level timeouts here
			SetNetworkTimeoutsForHost( outHost, kLDAPDefaultNetworkTimeoutInSeconds );
		}
	}
	if (inLDAPNodeStruct->fHost != nil)
	{
		outHost = inLDAPNodeStruct->fHost;
	}
	if (inConfig->fReplicaHosts != inOutList)	// if we aren't using inOutList, free it
	{
		DSDelete(inOutList);
	}
	
	return(outHost);
} //InitLDAPConnection
 

// ---------------------------------------------------------------------------
//     * ResolveHostName
// ---------------------------------------------------------------------------

struct addrinfo* CLDAPNode::ResolveHostName( CFStringRef inServerNameRef, int inPortNumber )
{
	struct addrinfo		hints;
	struct addrinfo	   *res				= nil;
	char				portString[32]	= {0};
	char				serverName[512]	= {0};
	

	if (CFStringGetCString(inServerNameRef, serverName, 512, kCFStringEncodingUTF8))
	{
		//retrieve the addrinfo for this server
		memset(&hints, 0, sizeof(hints));
		hints.ai_family		= PF_UNSPEC; //IPV4 or IPV6
		hints.ai_socktype	= SOCK_STREAM;
		
		sprintf( portString, "%d", inPortNumber );
		if ( getaddrinfo(serverName, portString, &hints, &res) != 0 )
		{
			res = nil;
		}
	}
	return(res);
} //ResolveHostName
 
 
// ---------------------------------------------------------------------------
//     * EstablishConnection
// ---------------------------------------------------------------------------

LDAP* CLDAPNode::EstablishConnection( sLDAPNodeStruct *inLDAPNodeStruct, sReplicaInfo *inList, int inPort, int inOpenTimeout, bool bInNeedWriteable )
{
	const int			maxSockets				= 512;   // lets do half of our max FD_SETSIZE = 512
	LDAP			   *outHost					= nil;
	sReplicaInfo	   *lastUsedReplica			= nil;
	sReplicaInfo	   *resolvedRepIter			= nil;
	struct addrinfo	   *resolvedAddrIter		= nil;
	int					val						= 1;
	int					len						= sizeof(val);
	struct timeval		recvTimeoutVal			= { inOpenTimeout, 0 };
	struct timeval		recheckTimeoutVal		= { 1, 0 };
	struct timeval		quickCheck				= { 0, 3000 };   // 3 milliseconds
	struct timeval		pollValue				= { 0, 0 };
	int					sockCount				= 0;
	fd_set				fdset, fdwrite, fdread;
	int					fcntlFlags				= 0;
	char			   *goodHostAddress			= NULL;
	int				   *sockList				= nil;
	sReplicaInfo	  **replicaPointers			= nil;
	struct addrinfo   **addrinfoPointers		= nil;
	int					sockIter;
	bool				bReachableAddresses		= false;
	bool				bTrySelect				= false;
	
	if (inPort == 0)
	{
		inPort = 389; //default
	}

	//establish a connection
	//simple sequential approach testing connect reachability first
	//TODO KW better scheme needed here
	//use the last used one first and wait say 5 seconds for it to establish
	//then if not try the others in a concurrent manner
	//reuse the passwordserver code for this

	if( inList == NULL )
		return NULL;
	
	// We can't select on more than FD_SETSIZE, so let's limit it there..
	sockList = (int *)calloc( maxSockets, sizeof(int) );
	replicaPointers = (sReplicaInfo **)calloc( maxSockets, sizeof(sReplicaInfo *) );
	addrinfoPointers = (struct addrinfo **)calloc( maxSockets, sizeof(struct addrinfo *) );

	SetSockList( sockList, maxSockets, false );
	FD_ZERO( &fdset );

	for (resolvedRepIter = inList; resolvedRepIter != nil; resolvedRepIter = resolvedRepIter->fNext)
	{
		if ( !bInNeedWriteable || ( bInNeedWriteable && resolvedRepIter->bWriteable ))
		{
			for (resolvedAddrIter = resolvedRepIter->fAddrInfo; resolvedAddrIter != nil && sockCount < maxSockets; resolvedAddrIter = resolvedAddrIter->ai_next)
			{
				// if we haven't found a reachable address yet.
				if( bReachableAddresses == false )
				{
					bReachableAddresses = ReachableAddress( resolvedAddrIter );
				}
				
				if( resolvedRepIter->bUsedLast == true )
				{
					resolvedRepIter->bUsedLast = false;
					lastUsedReplica = resolvedRepIter;
				}
				else
				{
					replicaPointers[sockCount] = resolvedRepIter;
					addrinfoPointers[sockCount] = resolvedAddrIter;
					sockCount++;
				}
			}
		}
	}
	
	// we are worried about bootstrap issues here, so if we don't have a last used, either we were just configured
	// or we just booted, so let's go through some extra effort but not if we have no addrinfo values ie. check that sockCount > 0
	if( lastUsedReplica == NULL && bReachableAddresses == false && sockCount > 0 )
	{
		// if we didn't have a reachable address, let's wait a little just in case we are in bootup or waking up before we try sockets..
		struct mach_timebase_info       timeBaseInfo;
		
		mach_timebase_info( &timeBaseInfo );
		
		int			iReachableCount = 0;
		uint64_t	delay = (((uint64_t)NSEC_PER_SEC * (uint64_t)timeBaseInfo.denom) / (uint64_t)timeBaseInfo.numer);

		// Let's timeout in 50% of the configured time if we don't have an address....
		while( bReachableAddresses == false && iReachableCount < (inOpenTimeout >> 1) )
		{
			// let's wait 1 second... and check again...
			mach_wait_until( mach_absolute_time() + delay );
			iReachableCount++;
			
			// let's check if we any addresses would be reachable
			int iCount;
			for( iCount = 0; iCount < sockCount && bReachableAddresses == false; iCount++ )
			{
				bReachableAddresses = ReachableAddress( addrinfoPointers[iCount] );
			}
		}
	}

	// if we don't have any reachable addresses, there's no reason to try.. and hang the user unnecessarily..
	if( bReachableAddresses )
	{
		//try the last used one first
		if (lastUsedReplica != nil)
		{
			for (resolvedAddrIter = lastUsedReplica->fAddrInfo; resolvedAddrIter != nil && goodHostAddress == nil; resolvedAddrIter = resolvedAddrIter->ai_next)
			{
				if( ReachableAddress(resolvedAddrIter) )
				{
					goodHostAddress = LDAPWithBlockingSocket( resolvedAddrIter, inOpenTimeout );
					if( goodHostAddress )
					{
						DBGLOG2( kLogPlugin, "CLDAPNode: EstablishConnection - Previous replica with IP Address = %s responded for %s", goodHostAddress, (bInNeedWriteable ? "write" : "read") );
					}
				}
			}
		}
	
		// so let's go through all sockets until we finish and timeout and don't have an IP address
		for( sockIter = 0; sockIter < sockCount && goodHostAddress == nil; sockIter++ )
		{
			struct addrinfo *tmpAddress = addrinfoPointers[sockIter];
			
			if( ReachableAddress(tmpAddress) )
			{
				// if it is local, we should try it blocking because it will fail immediately..
				if( IsLocalAddress( tmpAddress ) )
				{
					goodHostAddress = LDAPWithBlockingSocket( tmpAddress, inOpenTimeout );
					if( goodHostAddress )
					{
						lastUsedReplica = replicaPointers[sockIter];
						DBGLOG2( kLogPlugin, "CLDAPNode: EstablishConnection - Attempting to use local address = %s for %s", goodHostAddress, (bInNeedWriteable ? "write" : "read") );
					} else {
						char *tempaddress = ConvertToIPAddress( addrinfoPointers[sockIter] );
						if( tempaddress ) {
							DBGLOG1( kLogPlugin, "CLDAPNode: EstablishConnection - Failed local address connect to = %s", tempaddress );
							DSFree( tempaddress );
						}
					}
				}
				else
				{
					// otherwise we should do a non-blocking socket...
					int aSock = socket( tmpAddress->ai_family, tmpAddress->ai_socktype, tmpAddress->ai_protocol );
					if( aSock != -1 )
					{
						setsockopt( aSock, SOL_SOCKET, SO_NOSIGPIPE, &val, len );
						setsockopt( aSock, SOL_SOCKET, SO_RCVTIMEO, &recvTimeoutVal, sizeof(recvTimeoutVal) );
						
						//non-blocking now
						fcntlFlags = fcntl( aSock, F_GETFL, 0 );
						if( fcntlFlags != -1 )
						{
							if( fcntl(aSock, F_SETFL, fcntlFlags | O_NONBLOCK) != -1 )
							{
								sockList[sockIter] = aSock;
								
								// if this is a -1, then we add it to our select poll...
								if (connect(aSock, tmpAddress->ai_addr, tmpAddress->ai_addrlen) == -1)
								{
									FD_SET( aSock, &fdset );
									bTrySelect = true;
									
									char *tempaddress = ConvertToIPAddress( addrinfoPointers[sockIter] );
									if( tempaddress ) {
										DBGLOG1( kLogPlugin, "CLDAPNode: EstablishConnection - Attempting Replica connect to = %s", tempaddress );
										DSFree( tempaddress );
									}
								}
								else
								{
									goodHostAddress = ConvertToIPAddress( tmpAddress );
									if( goodHostAddress ) {
										lastUsedReplica = replicaPointers[sockIter];
										DBGLOG1( kLogPlugin, "CLDAPNode: EstablishConnection - Immediate Response to = %s", goodHostAddress );
									}
								}
							}
							else
							{
								close( aSock );
								DBGLOG1( kLogPlugin, "CLDAPNode: EstablishConnection - Unable to do non-blocking connect for socket = %d", aSock );
							}
						}
						else
						{
							close( aSock );
							DBGLOG1( kLogPlugin, "CLDAPNode: EstablishConnection - Unable to do get GETFL = %d", aSock );
						}
					}
				}
			}
			else
			{
				char *tempaddress = ConvertToIPAddress( addrinfoPointers[sockIter] );
				if( tempaddress )
				{
					DBGLOG1( kLogPlugin, "CLDAPNode: EstablishConnection - Address not reachable %s", tempaddress );
					DSFree( tempaddress );
				}
			}
			
			if( bTrySelect )
			{
				// let's do our select to see if anything responded....
				FD_COPY( &fdset, &fdwrite );
				FD_COPY( &fdset, &fdread );
				
				// let's do a quick check to see if we've already gotten a response
				if( select( FD_SETSIZE, NULL, &fdwrite, NULL, &quickCheck ) > 0 )
				{
					select( FD_SETSIZE, &fdread, NULL, NULL, &pollValue );   // let's check the read too...

					int checkIter;
					for( checkIter = 0; checkIter <= sockIter; checkIter++ )
					{
						// if we have write, but no read
						int aSock = sockList[checkIter];
						if( aSock != -1 && FD_ISSET(aSock, &fdwrite) && !FD_ISSET(aSock, &fdread) )
						{
							goodHostAddress = ConvertToIPAddress( addrinfoPointers[checkIter] );
							if( goodHostAddress )
							{
								lastUsedReplica = replicaPointers[checkIter];
								break;
							}
						}
					else if( aSock != -1 && FD_ISSET(aSock, &fdwrite) && FD_ISSET(aSock, &fdread) )
					{
						// if we have a bad socket, we will always get an immediate response
						// so let's remove it from the poll
						FD_CLR( aSock, &fdset );
						char *tmpHostAddr = ConvertToIPAddress( addrinfoPointers[checkIter] );
						if( tmpHostAddr ) {
							DBGLOG1( kLogPlugin, "CLDAPNode: EstablishConnection - Quick Check Bad socket to host %s clearing from poll", tmpHostAddr );
							break;
						}
					}					
					}
				}
			}
		}

		// let's do our polling.....
		int iterTry = 0;
		while( goodHostAddress == NULL && iterTry++ < inOpenTimeout && bTrySelect ) 
		{
			FD_COPY( &fdset, &fdwrite ); // we need to copy the fdset, cause it get's zero'd
			FD_COPY( &fdset, &fdread ); // we need to copy the fdset, cause it get's zero'd
			
			//here were need to select on the sockets
			recheckTimeoutVal.tv_sec = 1;
			if( select(FD_SETSIZE, NULL, &fdwrite, NULL, &recheckTimeoutVal) > 0 )
			{
				int checkIter;

				select( FD_SETSIZE, &fdread, NULL, NULL, &pollValue );
			
				for( checkIter = 0; checkIter < sockCount; checkIter++ )
				{
					// if we have write, but no read
					int aSock = sockList[checkIter];
					if( aSock != -1 && FD_ISSET(aSock, &fdwrite) && !FD_ISSET(aSock, &fdread) )
					{
						goodHostAddress = ConvertToIPAddress( addrinfoPointers[checkIter] );
						if( goodHostAddress ) {
							lastUsedReplica = replicaPointers[checkIter];
							break;
						}
					}
					else if( aSock != -1 && FD_ISSET(aSock, &fdwrite) && FD_ISSET(aSock, &fdread) )
					{
						// if we have a bad socket, we will always get an immediate response
						// so let's remove it from the poll
						FD_CLR( aSock, &fdset );
						char *tmpHostAddr = ConvertToIPAddress( addrinfoPointers[checkIter] );
						if( tmpHostAddr ) {
							DBGLOG1( kLogPlugin, "CLDAPNode: EstablishConnection - Bad socket to host %s clearing from poll", tmpHostAddr );
							break;
						}
					}
				}
			}
		}
		
		// we have an address, so let's do the ldap_init...
		if( goodHostAddress )
		{
			//call ldap_init
			outHost = ldap_init( goodHostAddress, inPort );

			//if success set bUsedLast
			if (outHost != nil)
			{
				// set some network level timeouts here
				SetNetworkTimeoutsForHost( outHost, kLDAPDefaultNetworkTimeoutInSeconds );

				if( lastUsedReplica )
				{
					lastUsedReplica->bUsedLast = true;
				}
				DBGLOG2( kLogPlugin, "CLDAPNode: EstablishConnection - Using replica with IP Address = %s for %s", goodHostAddress, (bInNeedWriteable ? "write" : "read") );
				inLDAPNodeStruct->setLastLDAPServer( goodHostAddress );
			}
			else
			{
				DBGLOG1( kLogPlugin, "CLDAPNode: EstablishConnection - ldap_init failed for %s", goodHostAddress );
				inLDAPNodeStruct->setLastLDAPServer( NULL );
			}
			
			DSFree( goodHostAddress );
		}
		else
		{
			DBGLOG1( kLogPlugin, "CLDAPNode: EstablishConnection - Could not establish connection for %s", (bInNeedWriteable ? "write" : "read") );
			inLDAPNodeStruct->setLastLDAPServer( NULL );
		}
	}
	else
	{
		DBGLOG( kLogPlugin, "CLDAPNode: EstablishConnection - No reachable addresses, possibly no IP addresses" );
		inLDAPNodeStruct->setLastLDAPServer( NULL );
	}
	
	// lets close all of our sockets
	SetSockList( sockList, sockCount, true );

	DSFree( sockList );
	DSFree( replicaPointers )
	DSFree( addrinfoPointers );
	
	return(outHost);
} //EstablishConnection

//------------------------------------------------------------------------------------
//	* BuildReplicaInfoLinkList
//------------------------------------------------------------------------------------

void CLDAPNode::BuildReplicaInfoLinkList( sReplicaInfo **inOutList, CFArrayRef inRepList, CFArrayRef inWriteableList, int inPort )
{
    // if we have a RepList and inOutList
    if( inRepList != NULL && inOutList != NULL )
    {
        CFStringRef replicaStrRef	= NULL;
        
        //use the known replicas to fill out the sReplicaInfo list
        //the writeable replicas are already contained in this list as well
        //so we need only one list of replica info structs
        CFIndex numReps = CFArrayGetCount(inRepList);
        
        // let's make this easy... let's use the readable replicas, as there should be more of those than writable
        if ( numReps > 0)
        {
            sReplicaInfo *tailItem = NULL;
            sReplicaInfo *aNewList = NULL;
            CFRange rangeOfWrites = CFRangeMake( 0, (inWriteableList ? CFArrayGetCount(inWriteableList) : 0) );
            
            for (CFIndex indexToRep=0; indexToRep < numReps; indexToRep++ )
            {
                replicaStrRef = (CFStringRef)::CFArrayGetValueAtIndex( inRepList, indexToRep );
                struct addrinfo *addrList = ResolveHostName(replicaStrRef, inPort);
                sReplicaInfo* newInfo = (sReplicaInfo *)calloc(1, sizeof(sReplicaInfo));
                
                // if we have a tail item, which will branch less
                if( tailItem != NULL )
                {
                    tailItem->fNext = newInfo; // set the next pointer
                    tailItem = newInfo; // the reset the tail to the one we just added..
                }
                else
                {
                    // otherwise we must be the first entry
                    aNewList    = newInfo;
                    tailItem	= newInfo;
                }

                newInfo->fAddrInfo = addrList;
                newInfo->hostname = CFStringCreateCopy( kCFAllocatorDefault, replicaStrRef );
                
                // see if it is in the writeable list and flag it accordingly
                if( inWriteableList != nil && CFArrayContainsValue(inWriteableList, rangeOfWrites, replicaStrRef) )
                {
                    newInfo->bWriteable = true;
                }
            }
            
            if (aNewList != NULL) //there is a new rep list that was built
            {
                DSDelete( *inOutList );
                
                // now lets set the new one..
                *inOutList = aNewList;
            }
        }
    }
    
} // BuildReplicaInfoLinkList

//------------------------------------------------------------------------------------
//	* MergeArraysRemovingDuplicates
//------------------------------------------------------------------------------------

void CLDAPNode::MergeArraysRemovingDuplicates( CFMutableArrayRef cfPrimaryArray, CFArrayRef cfArrayToAdd )
{
    CFIndex addCount    = CFArrayGetCount( cfArrayToAdd );
    CFRange cfRange     = CFRangeMake( 0, CFArrayGetCount(cfPrimaryArray) );
    
    // if the incoming add array has loopback, let's add it to the beginning first before we loop through
    if( CFArrayContainsValue(cfArrayToAdd, CFRangeMake(0,addCount), CFSTR("127.0.0.1")) ||
        CFArrayContainsValue(cfArrayToAdd, CFRangeMake(0,addCount), CFSTR("localhost")) )
    {
        CFArrayInsertValueAtIndex( cfPrimaryArray, 0, CFSTR("127.0.0.1") );
        cfRange.length++; // increase range cause we just added one
    }

    for( CFIndex ii = 0; ii < addCount; ii++ )
    {
        CFStringRef cfString    = (CFStringRef) CFArrayGetValueAtIndex( cfArrayToAdd, ii );
        
        if( CFArrayContainsValue(cfPrimaryArray, cfRange, cfString) == false )
        {
            CFArrayAppendValue( cfPrimaryArray, cfString );
            cfRange.length++; // increase range cause we just added one
        }
    }
} //MergeArraysRemovingDuplicates

//------------------------------------------------------------------------------------
//	* GetReplicaListFromDNS
//------------------------------------------------------------------------------------

sInt32 CLDAPNode::GetReplicaListFromDNS( sLDAPNodeStruct *inLDAPNodeStruct, CFMutableArrayRef inOutRepList )
{
	sInt32				siResult		= eDSNoStdMappingAvailable;
	CFMutableArrayRef	serviceRecords  = NULL;
	CFMutableArrayRef	aRepList		= NULL; //no writeable replicas obtained here
	CFStringRef			aString			= NULL;
	
    // let's see if it is a hostname and not an IP address
	if ( (inLDAPNodeStruct != nil) && (inLDAPNodeStruct->fNodeName != nil) )
	{
        CFStringRef cfServerName = CFStringCreateWithCString( kCFAllocatorDefault, inLDAPNodeStruct->fNodeName, kCFStringEncodingUTF8 );
        CFArrayRef  cfComponents = CFStringCreateArrayBySeparatingStrings( kCFAllocatorDefault, cfServerName, CFSTR(".") );
        bool        bIsDNSname  = true;
        int         namePartCount = 1; // default to 1
        CFStringRef cfDomainSuffix = NULL;

        if( cfComponents != NULL )
        {
            namePartCount = CFArrayGetCount( cfComponents );
            
            // if we have 4 components, let's see if the last one is all numbers
            if( namePartCount == 4 && CFStringGetIntValue((CFStringRef) CFArrayGetValueAtIndex(cfComponents, 3)) != 0 )
            {
                bIsDNSname = false;
            }
            
            // if we are a DNS name, let's compose the suffix for this domain for later use..
            if( bIsDNSname && namePartCount > 2 )
            {
                cfDomainSuffix = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%@.%@"), CFArrayGetValueAtIndex(cfComponents, namePartCount-2), CFArrayGetValueAtIndex(cfComponents, namePartCount-1) );
            }
        }

        DSCFRelease( cfServerName );
        DSCFRelease( cfComponents );
        
        // if we have a DNS name and not a dotted IP address then there is something to do
        if( bIsDNSname )
        {
            // if we are using credentials we will use any server that is available from our primary DNS domain
            // if we have more than 2 parts to the server name and we have a domain suffix (i.e., server.domain.com vs. domain.com)
            if( inLDAPNodeStruct->fLDAPUserName && inLDAPNodeStruct->fLDAPCredentialsLen && namePartCount > 2 && cfDomainSuffix != NULL )
            {
                CFStringRef         cfKey  = SCDynamicStoreKeyCreateNetworkGlobalEntity( kCFAllocatorDefault, kSCDynamicStoreDomainState, kSCEntNetDNS );
                SCDynamicStoreRef   cfStore = SCDynamicStoreCreate( kCFAllocatorDefault, CFSTR("DirectoryService"), NULL, NULL );
                CFDictionaryRef     cfDNSdomainsDict = (CFDictionaryRef) SCDynamicStoreCopyValue( cfStore, cfKey );
                
                DSCFRelease( cfKey );
                DSCFRelease( cfStore );
                
                CFArrayRef cfSearchArray = (CFArrayRef) CFDictionaryGetValue( cfDNSdomainsDict, kSCPropNetDNSSearchDomains );
                if( cfSearchArray != NULL && CFArrayGetCount( cfSearchArray ) )
                {
                    CFStringRef cfFirstDomain = (CFStringRef) CFArrayGetValueAtIndex( cfSearchArray, 0 );
                    
                    // if the suffix of the server matches the configured search domain.. then we will use this search domain to find servers
                    if( CFStringHasSuffix( cfFirstDomain, cfDomainSuffix ) )
                    {
						uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfFirstDomain), kCFStringEncodingUTF8 ) + 1;
						char *domain = (char *) calloc( sizeof(char), uiLength );
						CFStringGetCString( cfFirstDomain, domain, uiLength, kCFStringEncodingUTF8 );

                        DBGLOG1( kLogPlugin, "CLDAPNode: GetReplicaListFromDNS - Checking computer's primary domain %s", domain );

                        serviceRecords = getDNSServiceRecs( "ldap", domain );
                        
                        DSFreeString( domain );
                    }
                }
                
                DSCFRelease( cfDNSdomainsDict );
            }

            DSCFRelease( cfDomainSuffix );

            // searchRecords is still NULL, we will search the _ldap._tcp.configuredserver.domain.com
            if( serviceRecords == NULL )
            {
                DBGLOG1( kLogPlugin, "CLDAPNode: GetReplicaListFromDNS - Checking domain %s", inLDAPNodeStruct->fNodeName );
                serviceRecords = getDNSServiceRecs( "ldap", inLDAPNodeStruct->fNodeName );
            }

            // if we are using authentication and there are more than 2 pieces to the domain name
            //    let's search the subdomain under the server so "_ldap._tcp.domain.com"
            if( inLDAPNodeStruct->fLDAPUserName && inLDAPNodeStruct->fLDAPCredentialsLen && namePartCount > 2 && serviceRecords == NULL )
            {
                char *subDomain = strchr( inLDAPNodeStruct->fNodeName, '.' ) + 1; // plus 1 to skip the dot itself
                
                if( subDomain )
                {
                    DBGLOG1( kLogPlugin, "CLDAPNode: GetReplicaListFromDNS - Checking domain %s", subDomain );
                    serviceRecords = getDNSServiceRecs( "ldap", subDomain );
                }
            }
        }
	}
	
	if ( serviceRecords != NULL )
	{
		aRepList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		CFIndex totalCount = CFArrayGetCount(serviceRecords);
		for (CFIndex indexToCnt=0; indexToCnt < totalCount; indexToCnt++ )
		{
			aString = (CFStringRef)CFDictionaryGetValue( (CFDictionaryRef)::CFArrayGetValueAtIndex( serviceRecords, indexToCnt ), CFSTR("Host"));
			if ( aString != NULL );
			{
				CFArrayAppendValue(aRepList, aString);
			}
		}
		CFRelease(serviceRecords);
	}
	
	if ( aRepList != NULL )
	{
        // if new data found
        if( CFArrayGetCount(aRepList) > 0 )
        {
            DBGLOG( kLogPlugin, "CLDAPNode: GetReplicaListFromDNS - Found ldap replica servers in DNS service records." );
            
            // we merge the replica list to the DNS list, DNS is has priority
            MergeArraysRemovingDuplicates( aRepList, inOutRepList ); 
            
            CFArrayRemoveAllValues( inOutRepList );
            CFArrayAppendArray( inOutRepList, aRepList, CFRangeMake(0,CFArrayGetCount(aRepList)) );

            siResult = eDSNoErr;
        }
        else
        {
            DBGLOG( kLogPlugin, "CLDAPNode: GetReplicaListFromDNS - No ldap replica servers in DNS service records." );
        }
        
        CFRelease( aRepList );
        aRepList = NULL;
	} 
    else
    {
        DBGLOG( kLogPlugin, "CLDAPNode: GetReplicaListFromDNS - No ldap replica servers in DNS service records." );
    }
    
	return(siResult);
}//GetReplicaListFromDNS

//------------------------------------------------------------------------------------
//	* RetrieveDefinedReplicas
//------------------------------------------------------------------------------------

sInt32 CLDAPNode::RetrieveDefinedReplicas( sLDAPNodeStruct *inLDAPNodeStruct, CFMutableArrayRef &inOutRepList, CFMutableArrayRef &inOutWriteableList, int inPort, sReplicaInfo **inOutList )
{
	LDAP			   *outHost			= nil;
	CFMutableArrayRef	aRepList		= NULL;
	CFMutableArrayRef	aWriteableList	= NULL;
	bool				bMessageFound	= false;
	sInt32				foundResult		= eDSRecordNotFound;
	sLDAPConfigData    *pConfig			= nil;
	int					openTimeout		= kLDAPDefaultOpenCloseTimeoutInSeconds;
	int					searchTimeout   = 30; //default value for replica info extraction
	int					version			= -1;
    int					bindMsgId		= 0;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	bool				bIsSSL			= false;
	bool				bReferrals		= true;
	uInt32				aSecurityLevel	= 0;
	bool				bNoConfig		= true;
	bool				bDNSReplicas	= false;
	bool				bLDAPv2ReadOnly	= false;

	try
	{
		CLDAPv3Plugin::WaitForNetworkTransitionToFinish();
		
		inLDAPNodeStruct->SessionMutexWait();
		
		inLDAPNodeStruct->fConnectionActiveCount++;
		
		pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inLDAPNodeStruct->fNodeName );
		if (pConfig != nil)
		{
			bNoConfig = false;
			// if no authCall active, then we should use the config settings
			if( !inLDAPNodeStruct->bAuthCallActive )
			{
				// if we have a config, let's just erase the credentials and re-read them
				// into the NodeStruct.  This allows us rebind, even if we have no config
				DSDelete( inLDAPNodeStruct->fLDAPUserName );
				DSDelete( inLDAPNodeStruct->fKerberosId );
				DSDelete( inLDAPNodeStruct->fLDAPAuthType );
				
				DSFree( inLDAPNodeStruct->fLDAPCredentials );
				
				if ( pConfig->bSecureUse )
				{
					inLDAPNodeStruct->fLDAPUserName = (pConfig->fServerAccount ? strdup(pConfig->fServerAccount) : nil);
					inLDAPNodeStruct->fKerberosId = (pConfig->fKerberosId ? strdup(pConfig->fKerberosId) : nil);
					inLDAPNodeStruct->fLDAPCredentials = (void *) (pConfig->fServerPassword ? strdup( pConfig->fServerPassword ) : NULL);
					inLDAPNodeStruct->fLDAPCredentialsLen = (pConfig->fServerPassword ? strlen( pConfig->fServerPassword ) : 0 );
				}
			}				
			
			openTimeout		= pConfig->fOpenCloseTimeout;
			searchTimeout   = pConfig->fSearchTimeout;
			bIsSSL			= pConfig->bIsSSL;
			bLDAPv2ReadOnly = pConfig->bLDAPv2ReadOnly;
			bReferrals		= pConfig->bReferrals;
			bDNSReplicas	= pConfig->bDNSReplicas;
			aSecurityLevel	= pConfig->fSecurityLevel;
			gpConfigFromXML->ConfigUnlock( pConfig );
		}

		inLDAPNodeStruct->fConnectionActiveCount--;

		inLDAPNodeStruct->SessionMutexSignal();
		
		//establish a connection using the addrinfo list
		outHost = EstablishConnection( inLDAPNodeStruct, *inOutList, inPort, openTimeout, false );
		
		//retrieve the current replica list
		//if we find something then we replace the current replica list and
		//rebuild the addrinfo list
		if (outHost != nil)
		{
			if (bIsSSL && !bLDAPv2ReadOnly)
			{
				int ldapOptVal = LDAP_OPT_X_TLS_HARD;
				ldap_set_option(outHost, LDAP_OPT_X_TLS, &ldapOptVal);
			}
			ldap_set_option(outHost, LDAP_OPT_REFERRALS, (bReferrals ? LDAP_OPT_ON : LDAP_OPT_OFF) );
			
			if (!bLDAPv2ReadOnly)
			{
				/* LDAPv3 only */
				version = LDAP_VERSION3;
				ldap_set_option( outHost, LDAP_OPT_PROTOCOL_VERSION, &version );
			}
	
			pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inLDAPNodeStruct->fNodeName );
			if ( pConfig != nil)
			{
				if (!bLDAPv2ReadOnly)
				{
					// SASLMethods are first so that we can do non-clear text auth methods
					CheckSASLMethods( inLDAPNodeStruct );

					// let's do a SASLbind if possible
					ldapReturnCode = doSASLBindAttemptIfPossible( outHost, pConfig, inLDAPNodeStruct->fLDAPUserName, (char *)inLDAPNodeStruct->fLDAPCredentials, inLDAPNodeStruct->fKerberosId );
				}
				gpConfigFromXML->ConfigUnlock( pConfig );
			}
			
			// if we didn't have a local error or LDAP_OTHER, then we either failed or succeeded so no need to continue
			if( ldapReturnCode != LDAP_LOCAL_ERROR && ldapReturnCode != LDAP_OTHER && !bLDAPv2ReadOnly )
			{
				// if we didn't have a success then we failed for some reason
				if( ldapReturnCode != LDAP_SUCCESS )
				{
					DBGLOG( kLogPlugin, "CLDAPNode: Failed doing SASL Authentication in Replica retrieval" );
					inLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
					inLDAPNodeStruct->fDelayedBindTime = time( nil ) + inLDAPNodeStruct->fDelayRebindTry;
					throw( (sInt32)eDSCannotAccessSession );
				}
			}
			// if we don't have pConfig, we're ok..
			// if we are SSL mode
			// if cleartext binds are allowed
			// if we don't allow clear text binds but don't have any credentials, we're doing anonymous it's OK
			else if( bNoConfig || bIsSSL || (aSecurityLevel & kSecDisallowCleartext) == 0 || 
					 (aSecurityLevel & kSecDisallowCleartext == kSecDisallowCleartext && (DSIsStringEmpty(inLDAPNodeStruct->fLDAPUserName) || inLDAPNodeStruct->fLDAPCredentialsLen == 0)) ) 
			{
				timeval	tv = { 0 };
			
				//this is our and only our LDAP session for now
				//need to use our timeout so we don't hang indefinitely
				bindMsgId = ldap_simple_bind( outHost, inLDAPNodeStruct->fLDAPUserName, (char *)inLDAPNodeStruct->fLDAPCredentials );

				tv.tv_sec		= (openTimeout ? openTimeout : kLDAPDefaultOpenCloseTimeoutInSeconds);
				ldapReturnCode	= ldap_result( outHost, bindMsgId, 0, &tv, &result );
				
				if ( ldapReturnCode == -1 )
				{
					throw( (sInt32)eDSCannotAccessSession );
				}
				else if ( ldapReturnCode == 0 )
				{
					//log this timed out connection
					LogFailedConnection("Bind timeout in Replica retrieval", inLDAPNodeStruct->fLDAPServer, inLDAPNodeStruct->fDelayRebindTry);

					inLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
					inLDAPNodeStruct->fDelayedBindTime = time( nil ) + inLDAPNodeStruct->fDelayRebindTry;
					throw( (sInt32)eDSCannotAccessSession );
				}
				else if ( ldap_result2error(outHost, result, 1) != LDAP_SUCCESS )
				{
					//log this failed connection
					LogFailedConnection("Bind failure in Replica retrieval", inLDAPNodeStruct->fLDAPServer, inLDAPNodeStruct->fDelayRebindTry);

					inLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
					inLDAPNodeStruct->fDelayedBindTime = time( nil ) + inLDAPNodeStruct->fDelayRebindTry;
					inLDAPNodeStruct->fHost = outHost;
					throw( (sInt32)eDSCannotAccessSession );
				}
			} else {
				DBGLOG1( kLogPlugin, "CLDAPNode: ClearText binds disallowed by Policy.  LDAP Connection to server %s denied", inLDAPNodeStruct->fLDAPServer );
				syslog( LOG_ALERT,"DSLDAPv3PlugIn: ClearText binds disallowed by Policy.  LDAP Connection to server %s denied.", inLDAPNodeStruct->fLDAPServer );
				
				inLDAPNodeStruct->fConnectionStatus = kConnectionUnsafe;
				inLDAPNodeStruct->fDelayedBindTime = time( nil ) + (inLDAPNodeStruct->fDelayRebindTry ? inLDAPNodeStruct->fDelayRebindTry : kLDAPDefaultRebindTryTimeoutInSeconds);

				// we need to unbind and throw this connection away, otherwise we won't rebind correctly with security
				ldap_unbind_ext( outHost, NULL, NULL );
				outHost = NULL;
				
				throw( (sInt32)eDSCannotAccessSession );
			}

			// if we weren't safe before, we need to notify that the search policy may have changed.
			if ( inLDAPNodeStruct->fConnectionStatus != kConnectionSafe )
				   gSrvrCntl->NodeSearchPolicyChanged();
			
			//if we make it here the bind succeeded so lets flag the connection as safe
			inLDAPNodeStruct->fConnectionStatus = kConnectionSafe;

			//if we haven't bound above then we don't go looking for the replica info
			
			aRepList		= CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
			aWriteableList	= CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
            
            // let's add the configured IP first, then we'll re-order from there..
            if( inLDAPNodeStruct != NULL && inLDAPNodeStruct->fNodeName != NULL )
            {
                CFStringRef cfString = CFStringCreateWithCString( kCFAllocatorDefault, inLDAPNodeStruct->fNodeName, kCFStringEncodingUTF8 );
                CFArrayAppendValue( aRepList, cfString );
                DSCFRelease( cfString );
            }

            // look at config record first
			if ( GetReplicaListFromConfigRecord(outHost, searchTimeout, inLDAPNodeStruct, aRepList, aWriteableList) == eDSNoErr )
			{
				bMessageFound = true;
			}
            
            // look at altServer Entries
			if ( GetReplicaListFromAltServer(outHost, searchTimeout, aRepList) == eDSNoErr )
			{
				bMessageFound = true;
			}
            
            // look at DNS entries last, DNS will make order of entries based on DNS results
            if( bDNSReplicas )
            {
				if( GetReplicaListFromDNS(inLDAPNodeStruct, aRepList) == eDSNoErr )
					bMessageFound = true;
            }
			else
			{
				DBGLOG( kLogPlugin, "CLDAPNode: GetReplicaListFromDNS - Skipped - disabled in configuration" );
			}
            
			if (bMessageFound)
			{
                // if we have a new list, let's put the new in place
                if (CFArrayGetCount(aRepList) > 0)
                {
                    // if the writeable list is empty, but we have a readable list, let's copy the readable to the writable
                    // this may not always be the right thing, but for iPlanet, eDirectory and AD it is
                    // not appropriate for OpenLDAP, but it is the minority for this case
                    if( CFArrayGetCount(aWriteableList) == 0 )
                    {
                        CFArrayAppendArray( aWriteableList, aRepList, CFRangeMake(0,CFArrayGetCount(aRepList)) );
                    }
                    
                    // if we had a list sent to us, let's clean it, otherwise just set it
                    if( inOutRepList )
                    {
                        CFArrayRemoveAllValues( inOutRepList );
                        CFArrayAppendArray( inOutRepList, aRepList, CFRangeMake(0,CFArrayGetCount(aRepList)) );
                    }
                    else
                    {
                        // if we didn't have one, let's just subtitute.
                        inOutRepList = aRepList;
                        aRepList = NULL; // NULL out so we don't release later
                    }
                    foundResult = eDSNoErr;
                }
				
                // if we have a new list, let's put the new in place
                if (CFArrayGetCount(aWriteableList) > 0)
                {
                    if( inOutWriteableList )
                    {
                        CFArrayRemoveAllValues( inOutWriteableList );
                        CFArrayAppendArray(inOutWriteableList, aWriteableList, CFRangeMake(0,CFArrayGetCount(aWriteableList)));
                    }
                    else
                    {
                        inOutWriteableList = aWriteableList;
                        aWriteableList = NULL; // NULL out so we don't release later
                    }
                    foundResult = eDSNoErr;
                }
			} //new data found via bMessageFound
			
			// always rework the list since need to know which are writable
            BuildReplicaInfoLinkList( inOutList, inOutRepList, inOutWriteableList, inPort );
            
            //don't use the connection we had because it may not be the closest, re-establish the connection with new list
            ldap_unbind(outHost);
            
            // let's log the information for people to see if necessary
            if( inOutRepList != NULL && CFArrayGetCount( inOutRepList ) )
            {
                CFStringRef listString  = CFStringCreateByCombiningStrings( kCFAllocatorDefault, inOutRepList, CFSTR(", ") );
                uInt32      uiLength    = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(listString), kCFStringEncodingUTF8 ) + 1;
                char        *logLine = (char *) calloc( sizeof(char), uiLength );
                
                CFStringGetCString( listString, logLine, uiLength, kCFStringEncodingUTF8 );
                
                DBGLOG1( kLogPlugin, "CLDAPNode: Readable Replica List - %s", logLine );
                
                DSFreeString( logLine );
                DSCFRelease( listString );
            }
            
            // let's log the information for people to see if necessary
            if( inOutWriteableList != NULL && CFArrayGetCount( inOutWriteableList ) )
            {
                CFStringRef listString  = CFStringCreateByCombiningStrings( kCFAllocatorDefault, inOutWriteableList, CFSTR(", ") );
                uInt32      uiLength    = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(listString), kCFStringEncodingUTF8 ) + 1;
                char        *logLine = (char *) calloc( sizeof(char), uiLength );
                
                CFStringGetCString( listString, logLine, uiLength, kCFStringEncodingUTF8 );
                
                DBGLOG1( kLogPlugin, "CLDAPNode: Writeable Replica List - %s", logLine );
                
                DSFreeString( logLine );
                DSCFRelease( listString );
            }
		}
		else
		{
			foundResult = eDSCannotAccessSession;
		}
	} // try
	
	catch ( sInt32 err )
	{
		foundResult = err;
	}
	
    DSCFRelease( aRepList );
    DSCFRelease( aWriteableList );
	
	return(foundResult);
}// RetrieveDefinedReplicas


//------------------------------------------------------------------------------------
//	* GetReplicaListFromAltServer
//------------------------------------------------------------------------------------

sInt32 CLDAPNode::GetReplicaListFromAltServer( LDAP *inHost, int inSearchTO, CFMutableArrayRef inOutRepList )
{
	sInt32				siResult		= eDSRecordNotFound;
	bool				bResultFound	= false;
    int					ldapMsgId		= 0;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	char			   *attrs[2]		= {"altserver",NULL};
	BerElement		   *ber				= nil;
	struct berval	  **bValues			= nil;
	char			   *pAttr			= nil;
    CFMutableArrayRef   aRepList        = NULL;

	//search for the specific LDAP record altserver at the rootDSE which may contain
	//the list of LDAP replica urls
	
	// here is the call to the LDAP server asynchronously which requires
	// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
	// attribute list (NULL for all), return attrs values flag
	// Note: asynchronous call is made so that a MsgId can be used for future calls
	// This returns us the message ID which is used to query the server for the results
	if ( (ldapMsgId = ldap_search( inHost, "", LDAP_SCOPE_BASE, "(objectclass=*)", attrs, 0) ) == -1 )
	{
		bResultFound = false;
	}
	else
	{
		bResultFound = true;
		//retrieve the actual LDAP record data for use internally
		//useful only from the read-only perspective
		struct	timeval	tv;
		tv.tv_usec	= 0;
		if (inSearchTO == 0)
		{
			tv.tv_sec	= kLDAPDefaultSearchTimeoutInSeconds;
		}
		else
		{
			tv.tv_sec	= inSearchTO;
		}
			ldapReturnCode = ldap_result(inHost, ldapMsgId, 0, &tv, &result);
		}

	if (	(bResultFound) &&
			( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
	{
        aRepList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
        
		//get the replica list here
		//parse the attributes in the result - should only be one ie. altserver
		pAttr = ldap_first_attribute (inHost, result, &ber );
		if (pAttr != nil)
		{
			if (( bValues = ldap_get_values_len (inHost, result, pAttr )) != NULL)
			{					
				// for each value of the attribute we need to parse and add as an entry to the outList
				for (int i = 0; bValues[i] != NULL; i++ )
				{
					if ( bValues[i] != NULL )
					{
						//need to strip off any leading characters since this should be an url format
						//ldap:// or ldaps://
						int offset = 0;
						char *strPtr = bValues[i]->bv_val;
						if (strPtr != NULL && strlen(strPtr) >= 9) //don't bother trying to strip if string not even long enough to have a prefix
						{
							if (strncmp(strPtr,"ldaps://",8) == 0)
							{
								offset = 8;
							}
							else if (strncmp(strPtr,"ldap://",7) == 0)
							{
								offset = 7;
							}
						}
						//try to stop at end of server name and don't include port number or search base
						char *strEnd = nil;
						strEnd = strchr(strPtr+offset,':');
						if (strEnd != nil)
						{
							strEnd[0] = '\0';
						}
						else
						{
							strEnd = strchr(strPtr+offset,'/');
							if (strEnd != nil)
							{
								strEnd[0] = '\0';
							}
						}
						CFStringRef aCFString = CFStringCreateWithCString( NULL, strPtr+offset, kCFStringEncodingUTF8 );
						CFArrayAppendValue(aRepList, aCFString);
						//TODO KW which of these are writeable?
						//CFArrayAppendValue(outRoutWriteableListepList, aCFString);
						CFRelease(aCFString);
						siResult = eDSNoErr;
					}
				}
				ldap_value_free_len(bValues);
			} // if bValues = ldap_get_values_len ...
			ldap_memfree( pAttr );
		} // if pAttr != nil
			
		if (ber != nil)
		{
			ber_free( ber, 0 );
		}
			
		ldap_msgfree( result );
		result = nil;

	} // if bResultFound and ldapReturnCode okay
	else if (ldapReturnCode == LDAP_TIMEOUT)
	{
		siResult = eDSServerTimeout;
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}
	else
	{
		siResult = eDSRecordNotFound;
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}
	
	DSSearchCleanUp(inHost, ldapMsgId);
    
    if ( aRepList != NULL )
	{
        // if new data found
        if( CFArrayGetCount(aRepList) > 0 )
        {
            DBGLOG( kLogPlugin, "CLDAPNode: GetReplicaListFromAltServer - Found some ldap replica servers in rootDSE altServer." );

            // merge the new list into the existing list
            MergeArraysRemovingDuplicates( inOutRepList, aRepList ); 
            siResult = eDSNoErr;
        }
        else
        {
            DBGLOG( kLogPlugin, "CLDAPNode: GetReplicaListFromAltServer - No ldap replica servers in rootDSE altServer." );
        }
        
        CFRelease( aRepList );
        aRepList = NULL;
	} 
    else
    {
        DBGLOG( kLogPlugin, "CLDAPNode: GetReplicaListFromAltServer - No ldap replica servers in rootDSE altServer." );
    }
    	
	return( siResult );

} // GetReplicaListFromAltServer


//------------------------------------------------------------------------------------
//	* GetReplicaListFromConfigRecord
//------------------------------------------------------------------------------------

sInt32 CLDAPNode::GetReplicaListFromConfigRecord( LDAP *inHost, int inSearchTO, sLDAPNodeStruct *inLDAPNodeStruct, CFMutableArrayRef outRepList, CFMutableArrayRef outWriteableList )
{
	sInt32				siResult		= eDSRecordNotFound;
	bool				bResultFound	= false;
    int					ldapMsgId		= 0;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	BerElement		   *ber				= nil;
	struct berval	  **bValues			= nil;
	char			   *pAttr			= nil;
	LDAPControl		  **serverctrls		= nil;
	LDAPControl		  **clientctrls		= nil;
	char			   *nativeRecType	= nil;
	bool				bOCANDGroup		= false;
	CFArrayRef			OCSearchList	= nil;
	ber_int_t			scope			= LDAP_SCOPE_BASE;
	char			   *queryFilter		= nil;
	char			   *repListAttr		= nil;
	char			   *writeListAttr	= nil;
	int					whichAttr		= 0;

	if (inLDAPNodeStruct == nil)
	{
		return(siResult);
	}

	//search for the specific LDAP config record which may contain
	//the list of both read and write LDAP replica urls
	
	nativeRecType = CLDAPv3Plugin::MapRecToLDAPType(	kDSStdRecordTypeConfig,
														inLDAPNodeStruct->fNodeName,
														1,
														&bOCANDGroup,
														&OCSearchList,
														&scope );
	if (nativeRecType == nil)
	{
        DBGLOG( kLogPlugin, "CLDAPNode: GetReplicaListFromConfigRecord - No Config Record mapping to retrieve replica list." );
		return(eDSNoStdMappingAvailable);
	}
	
	queryFilter = CLDAPv3Plugin::BuildLDAPQueryFilter(	kDSNAttrRecordName,
														"ldapreplicas",
														eDSExact,
														inLDAPNodeStruct->fNodeName,
														false,
														kDSStdRecordTypeConfig,
														nativeRecType,
														bOCANDGroup,
														OCSearchList );
	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	if (queryFilter == nil)
	{
		DSFree( nativeRecType );
		return(siResult);
	}
	
	// here is the call to the LDAP server asynchronously which requires
	// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
	// attribute list (NULL for all), return attrs values flag
	// Note: asynchronous call is made so that a MsgId can be used for future calls
	// This returns us the message ID which is used to query the server for the results

	ldapReturnCode = ldap_search_ext(	inHost,
										nativeRecType,
										scope,
										queryFilter,
										NULL,
										0,
										serverctrls,
										clientctrls,
										0, 0, 
										&ldapMsgId );
	if (ldapReturnCode == LDAP_SUCCESS)
	{
		bResultFound = true;
		//retrieve the actual LDAP record data for use internally
		//useful only from the read-only perspective
		struct	timeval	tv;
		tv.tv_usec	= 0;
		if (inSearchTO == 0)
		{
			tv.tv_sec	= kLDAPDefaultSearchTimeoutInSeconds;
		}
		else
		{
			tv.tv_sec	= inSearchTO;
		}
			ldapReturnCode = ldap_result(inHost, ldapMsgId, 0, &tv, &result);
		}

	DSFree( nativeRecType );
	
	DSFree( queryFilter );
	
	if (serverctrls)  ldap_controls_free( serverctrls );
	if (clientctrls)  ldap_controls_free( clientctrls );

	if (	(bResultFound) &&
			( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
	{
		siResult = eDSNoErr;
		//get the replica list here
		//parse the attributes in the result - should only be two
		repListAttr		= CLDAPv3Plugin::MapAttrToLDAPType(	kDSStdRecordTypeConfig,
															kDSNAttrLDAPReadReplicas,
															inLDAPNodeStruct->fNodeName,
															1 );
		writeListAttr	= CLDAPv3Plugin::MapAttrToLDAPType(	kDSStdRecordTypeConfig,
															kDSNAttrLDAPWriteReplicas,
															inLDAPNodeStruct->fNodeName,
															1 );

				
		for (	pAttr = ldap_first_attribute (inHost, result, &ber );
					pAttr != NULL; pAttr = ldap_next_attribute(inHost, result, ber ) )
		{
			whichAttr = 0;
			if ( ( repListAttr != nil ) && ( strcmp(pAttr, repListAttr) == 0 ) )
			{
				whichAttr = 1;
			}
			if ( ( writeListAttr != nil ) && ( strcmp(pAttr, writeListAttr) == 0 ) )
			{
				whichAttr = 2;
			}
			if ( ( whichAttr != 0 ) && (( bValues = ldap_get_values_len (inHost, result, pAttr )) != NULL) )
			{					
				// for each value of the attribute we need to parse and add as an entry to the outList
				for (int i = 0; bValues[i] != NULL; i++ )
				{
					if ( bValues[i] != NULL )
					{
						//need to strip off any leading characters since this should be an url format
						//ldap:// or ldaps://
						int offset = 0;
						char *strPtr = bValues[i]->bv_val;
						if (strPtr != NULL && strlen(strPtr) >= 9) //don't bother trying to strip if string not even long enough to have a prefix
						{
							if (strncmp(strPtr,"ldaps://",8) == 0)
							{
								offset = 8;
							}
							else if (strncmp(strPtr,"ldap://",7) == 0)
							{
								offset = 7;
							}
						}
						//try to stop at end of server name and don't include port number or search base
						char *strEnd = nil;
						strEnd = strchr(strPtr+offset,':');
						if (strEnd != nil)
						{
							strEnd[0] = '\0';
						}
						else
						{
							strEnd = strchr(strPtr+offset,'/');
							if (strEnd != nil)
							{
								strEnd[0] = '\0';
							}
						}
                        
                        CFStringRef aCFString = CFStringCreateWithCString( NULL, strPtr+offset, kCFStringEncodingUTF8 );
						if (whichAttr == 1)
						{
                            if( CFArrayContainsValue(outRepList, CFRangeMake(0,CFArrayGetCount(outRepList)), aCFString) == false )
                            {
                                CFArrayAppendValue(outRepList, aCFString);
                            }
						}
						else
						{
                            if( CFArrayContainsValue(outWriteableList, CFRangeMake(0,CFArrayGetCount(outWriteableList)), aCFString) == false )
                            {
                                CFArrayAppendValue(outWriteableList, aCFString);
                            }
						}
                        CFRelease(aCFString);
					}
				}
				ldap_value_free_len(bValues);
			} // if bValues = ldap_get_values_len ...
			ldap_memfree( pAttr );
		} // for ( loop over ldap_next_attribute )
			
		if (ber != nil)
		{
			ber_free( ber, 0 );
		}
			
		ldap_msgfree( result );
		result = nil;

	} // if bResultFound and ldapReturnCode okay
	else if (ldapReturnCode == LDAP_TIMEOUT)
	{
		siResult = eDSServerTimeout;
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}
	else
	{
		siResult = eDSRecordNotFound;
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}
	
	DSSearchCleanUp(inHost, ldapMsgId);
	
	DSFree( repListAttr );
	DSFree( writeListAttr );
    
    if( CFArrayGetCount(outRepList) > 0 )
    {
        DBGLOG( kLogPlugin, "CLDAPNode: GetReplicaListFromConfigRecord - Found Config Record \"ldapreplicas\" that had Replica information." );
    }
    else
    {
        DBGLOG( kLogPlugin, "CLDAPNode: GetReplicaListFromConfigRecord - No Config Record \"ldapreplicas\" with Replica information." );
    }
    
	return( siResult );

} // GetReplicaListFromConfigRecord


// ---------------------------------------------------------------------------
//	* RetrieveServerMappingsIfRequired
// ---------------------------------------------------------------------------

void CLDAPNode::RetrieveServerMappingsIfRequired(sLDAPNodeStruct *inLDAPNodeStruct)
{
	sLDAPConfigData		   *pConfig			= nil;

	if (inLDAPNodeStruct != nil)
	{
		//check if we need to retrieve the server mappings
		pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inLDAPNodeStruct->fNodeName );
		if ( (inLDAPNodeStruct->fHost != nil) && (pConfig != nil) && (pConfig->bGetServerMappings) )
		{
			char **aMapSearchBase	= nil;
			uInt32	numberBases		= 0;
			//here we check if there is a provided mappings search base
			//otherwise we will attempt to retrieve possible candidates from the namingContexts of the LDAP server
			if ( (pConfig->fMapSearchBase == nil) || ( strcmp(pConfig->fMapSearchBase,"") == 0 ) )
			{
				//use the fOpenCloseTimeout here since we don't guarantee an explicit bind occurred at this point
				aMapSearchBase = GetNamingContexts( inLDAPNodeStruct->fHost, pConfig->fOpenCloseTimeout, &numberBases );
				if( aMapSearchBase == (char **) -1 )
				{
					aMapSearchBase = nil;
					//log this failed connection
					LogFailedConnection("GetNamingContexts failure", inLDAPNodeStruct->fLDAPServer, inLDAPNodeStruct->fDelayRebindTry);
					inLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
					inLDAPNodeStruct->fDelayedBindTime = time( nil ) + inLDAPNodeStruct->fDelayRebindTry;
				}
			}
			else
			{
				numberBases = 1;
				aMapSearchBase = (char **)calloc(numberBases+1, sizeof (char *));
				aMapSearchBase[0] = strdup(pConfig->fMapSearchBase);
			}
			
			for (uInt32 baseIndex = 0; (baseIndex < numberBases) && (aMapSearchBase[baseIndex] != nil); baseIndex++)
			{
				if ( (gpConfigFromXML->UpdateLDAPConfigWithServerMappings( pConfig->fNodeName, aMapSearchBase[baseIndex], pConfig->fServerPort, pConfig->bIsSSL, pConfig->bLDAPv2ReadOnly, pConfig->bUseAsDefaultLDAP, inLDAPNodeStruct->fHost )) == eDSNoErr )
				{
                    DBGLOG( kLogPlugin, "CLDAPNode: SafeOpen retrieved server mappings." );
					pConfig->bGetServerMappings = false;
					break;
				}
				else
				{
					syslog(LOG_ALERT,"LDAPv3: SafeOpen Can't retrieve server mappings from search base of <%s>.", aMapSearchBase[baseIndex] );
				}
			}
			
			if (pConfig->bGetServerMappings == true)
			{
                DBGLOG( kLogPlugin, "CLDAPNode: SafeOpen Cannot retrieve server mappings at this time." );
				syslog(LOG_ALERT,"LDAPv3: SafeOpen Cannot retrieve server mappings at this time.");
			}
			
			if (aMapSearchBase != nil)
			{
				for (uInt32 bIndex = 0; bIndex < numberBases; bIndex++)
				{
					DSFree( aMapSearchBase[bIndex] );
				}
				free(aMapSearchBase);
				aMapSearchBase = nil;
			}
		}
		
		if( pConfig )
		{
			gpConfigFromXML->ConfigUnlock( pConfig );
		}
	}
} // RetrieveServerMappingsIfRequired

// ---------------------------------------------------------------------------
//	* LDAPWithBlockingSocket
// ---------------------------------------------------------------------------

char *CLDAPNode::LDAPWithBlockingSocket( struct addrinfo *addrInfo, int seconds )
{
	int					aSock;
	int					val						= 1;
	int					len						= sizeof(val);
	struct timeval		recvTimeoutVal			= { seconds, 0 };
	char				*returnHostAddress		= NULL;

	aSock = socket( addrInfo->ai_family, addrInfo->ai_socktype, addrInfo->ai_protocol );
	if (aSock != -1)
	{
		setsockopt( aSock, SOL_SOCKET, SO_NOSIGPIPE, &val, len );
		setsockopt( aSock, SOL_SOCKET, SO_RCVTIMEO, &recvTimeoutVal, sizeof(recvTimeoutVal) );
		
		//block for recvTimeoutVal
		if( connect(aSock, addrInfo->ai_addr, addrInfo->ai_addrlen) == 0 )
		{
			returnHostAddress = ConvertToIPAddress( addrInfo );
		}
		close(aSock);
	}
	return returnHostAddress;
} //LDAPWithBlockingSocket

char *CLDAPNode::ConvertToIPAddress( struct addrinfo *addrInfo )
{
	//translate to the IP address
	char *returnHostAddress = NULL;
	
	if (addrInfo->ai_family == AF_INET)
	{
		returnHostAddress = (char *) calloc( 129, 1 );
		if( inet_ntop( AF_INET, (const void *)&(((struct sockaddr_in*)(addrInfo->ai_addr))->sin_addr), returnHostAddress, 129 ) == NULL )
		{
			free( returnHostAddress );
			returnHostAddress = NULL;
		}
	}
	else if (addrInfo->ai_family == AF_INET6)
	{
		returnHostAddress = (char *) calloc( 129, 1 );
		if( inet_ntop( AF_INET6, (const void *)&(((struct sockaddr_in6*)(addrInfo->ai_addr))->sin6_addr), returnHostAddress, 129 ) == NULL ) 
		{
			free( returnHostAddress );
			returnHostAddress = NULL;
		}
	}
	
	return returnHostAddress;
}

bool CLDAPNode::IsLocalAddress( struct addrinfo *addrInfo )
{
    struct ifaddrs *ifa_list = nil, *ifa = nil;
	bool			bReturn = false;
    
    if( getifaddrs(&ifa_list) != -1 )
	{
		for( ifa = ifa_list; ifa; ifa = ifa->ifa_next )
		{
			if( ifa->ifa_addr->sa_family == addrInfo->ai_addr->sa_family )
			{
				if( ifa->ifa_addr->sa_family == AF_INET )
				{
					struct sockaddr_in *interface = (struct sockaddr_in *)ifa->ifa_addr;
					struct sockaddr_in *check = (struct sockaddr_in *) addrInfo->ai_addr;
					
					if( interface->sin_addr.s_addr == check->sin_addr.s_addr )
					{
						bReturn = true;
						break;
					}
				}
				if( ifa->ifa_addr->sa_family == AF_INET6 )
				{
					struct sockaddr_in6 *interface = (struct sockaddr_in6 *)ifa->ifa_addr;
					struct sockaddr_in6 *check = (struct sockaddr_in6 *)addrInfo->ai_addr;
					
					if( memcmp( &interface->sin6_addr, &check->sin6_addr, sizeof(struct in6_addr) ) == 0 )
					{
						bReturn = true;
						break;
					}
				}
			}
		}
		freeifaddrs(ifa_list);
	}
	return bReturn;
}

bool CLDAPNode::ReachableAddress( struct addrinfo *addrInfo )
{
	bool	bReturn = IsLocalAddress( addrInfo );
	
	// if it wasn't local
	if( bReturn == false )
	{
		bReturn = checkReachability(addrInfo->ai_addr);
/*
		struct ifaddrs *ifa_list = nil, *ifa = nil;
		
		if( getifaddrs(&ifa_list) != -1 )
		{
			for( ifa = ifa_list; ifa; ifa = ifa->ifa_next )
			{
				// if either AF_INET or AF_INET6 then we have an address
				if( ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6 )
				{
					if( strcmp( ifa->ifa_name, "lo0" ) != 0 )  // if we aren't using loopback only....
					{
						bReturn = true;
						break;
					}
				}
			}
			freeifaddrs(ifa_list);
		}
*/
	}
	return bReturn;
}

void CLDAPNode::CheckSASLMethods( sLDAPNodeStruct *inLDAPNodeStruct )
{
	sLDAPConfigData		*pConfig	= nil;

	if (inLDAPNodeStruct != nil )
	{
		//check if we need to retrieve the server mappings
		pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inLDAPNodeStruct->fNodeName );
		
		if( pConfig != nil && pConfig->fSASLmethods == NULL && !(pConfig->bLDAPv2ReadOnly) )
		{
			LDAPMessage		   *result				= nil;
			int					ldapReturnCode		= 0;
			char			   *attrs[2]			= { "supportedSASLMechanisms",NULL };
			BerElement		   *ber					= nil;
			struct berval	  **bValues				= nil;
			char			   *pAttr				= nil;
			struct timeval		tv					= { 0, 0 };

			// we are in bootstrap, so let's don't try to get replicas, otherwise we will have a problem
			pConfig->bBuildReplicaList = false;
			
			LDAP *aHost = InitLDAPConnection( inLDAPNodeStruct, pConfig );

			// now we can go look for replicas
			pConfig->bBuildReplicaList = true;

			DBGLOG( kLogPlugin, "CLDAPNode: Getting SASL Methods" );

			if( aHost )
			{
				if ( pConfig->bIsSSL )
				{
					int ldapOptVal = LDAP_OPT_X_TLS_HARD;
					ldap_set_option( aHost, LDAP_OPT_X_TLS, &ldapOptVal );
				}

				ldap_set_option(aHost, LDAP_OPT_REFERRALS, (pConfig->bReferrals ? LDAP_OPT_ON : LDAP_OPT_OFF) );

				/* LDAPv3 only */
				int version = LDAP_VERSION3;
				ldap_set_option( aHost, LDAP_OPT_PROTOCOL_VERSION, &version );
				
				//this is our and only our LDAP session for now
				//need to use our timeout so we don't hang indefinitely
				int bindMsgId = ldap_simple_bind( aHost, NULL, NULL );
				
				tv.tv_sec		= (pConfig->fOpenCloseTimeout ? pConfig->fOpenCloseTimeout : kLDAPDefaultOpenCloseTimeoutInSeconds);
				ldapReturnCode	= ldap_result( aHost, bindMsgId, 0, &tv, &result );
				
				if( ldapReturnCode != 0 && ldapReturnCode != -1 && ldap_result2error(aHost, result, 1) == LDAP_SUCCESS )
				{
					tv.tv_sec	= (pConfig->fSearchTimeout ? pConfig->fSearchTimeout : kLDAPDefaultOpenCloseTimeoutInSeconds);
					
					pConfig->fSASLmethods = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
					
					ldapReturnCode = ldap_search_ext_s(	aHost,
										 "",
										 LDAP_SCOPE_BASE,
										 "(objectclass=*)",
										 attrs,
										 false,
										 NULL,
										 NULL,
										 &tv, 0, 
										 &result );
					
					if (ldapReturnCode == LDAP_SUCCESS)
					{
						pAttr = ldap_first_attribute (aHost, result, &ber );
						if (pAttr != nil)
						{
							if( (bValues = ldap_get_values_len (aHost, result, pAttr)) != NULL )
							{
								uInt32 ii = 0;
								
								while( bValues[ii] != NULL )
								{
									CFStringRef value = CFStringCreateWithCString( NULL, bValues[ii]->bv_val, kCFStringEncodingUTF8 ); 
									CFArrayAppendValue( pConfig->fSASLmethods, value );
									CFRelease( value );
									ii++;
								}
								ldap_value_free_len( bValues );
							} // if bValues = ldap_get_values_len ...
							ldap_memfree( pAttr );
						} // if pAttr != nil
						
						if (ber != nil)
						{
							ber_free( ber, 0 );
						}
						
						ldap_msgfree( result );
						result = nil;
						
						DBGLOG( kLogPlugin, "CLDAPNode: Successful SASL Method retrieval" );
					}
				}
				
				// if for some reason we ended up with a host that is different than structure, clean up the local one
				if ( aHost != inLDAPNodeStruct->fHost )
				{
					ldap_unbind_ext( aHost, NULL, NULL );
					aHost = NULL;
				}
			}
		}
		if( pConfig != nil )
		{
			gpConfigFromXML->ConfigUnlock( pConfig );
		}
	}
}// CheckSASLMethods


// this routine determines if a particular method can be handled by the internal SASL calls
bool CLDAPNode::isSASLMethodSupported ( CFStringRef inMethod )
{
	CFRange aRange = CFRangeMake( 0, CFArrayGetCount(fSupportedSASLMethods) );
	return CFArrayContainsValue( fSupportedSASLMethods, aRange, inMethod );
}

bool CLDAPNode::LocalServerIsLDAPReplica(  )
{
	bool bResult = false;
	char* fileContents = NULL;

	try
	{
		CFile slapdConf("/etc/openldap/slapd.conf");
		if ( !slapdConf.is_open() )
			throw(-1);
		
		CFile slapdMacOSXConf;
		fileContents = (char*)calloc( 1, slapdConf.FileSize() + 1 );
		if ( fileContents != NULL )
		{
			slapdConf.Read( fileContents, slapdConf.FileSize() );
			if ((strncmp( fileContents, "updatedn", sizeof("updatedn") - 1 ) == 0)
				|| (strstr( fileContents, "\nupdatedn" ) != NULL))
			{
				bResult = true;
			}
			free( fileContents );
			fileContents = NULL;
		}
		if ( !bResult )
		{
			slapdMacOSXConf.open("/etc/openldap/slapd_macosxserver.conf");
			fileContents = (char*)calloc( 1, slapdMacOSXConf.FileSize() + 1 );
		}
		if (fileContents != NULL)
		{
			slapdMacOSXConf.Read( fileContents, slapdMacOSXConf.FileSize() );
			if ((strncmp( fileContents, "updatedn", sizeof("updatedn") - 1 ) == 0)
				|| (strstr( fileContents, "\nupdatedn" ) != NULL))
			{
				bResult = true;
			}
			free( fileContents );
			fileContents = NULL;
		}
		
	}
	catch ( ... )
	{
	}

	DSFree( fileContents );
	
	return bResult;
}

// ---------------------------------------------------------------------------
//	* EnsureCheckFailedConnectionsThreadIsRunning
// ---------------------------------------------------------------------------

void CLDAPNode::EnsureCheckFailedConnectionsThreadIsRunning( void )
{
	if( fCheckThreadActive == false )
	{
		fCheckThreadActive = true;
				
		pthread_t       checkThread;
		pthread_attr_t	_DefaultAttrs;
				
		::pthread_attr_init( &_DefaultAttrs );
	
		::pthread_attr_setdetachstate( &_DefaultAttrs, PTHREAD_CREATE_DETACHED);
		pthread_create( &checkThread, &_DefaultAttrs, checkFailedServers, (void *)this );
	}
} // EnsureCheckFailedConnectionsThreadIsRunning

