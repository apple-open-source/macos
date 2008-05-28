/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
 * @header CLDAPv3Plugin
 * LDAP v3 plugin implementation to interface with Directory Services.
 */

#pragma mark Includes

#include <string.h>		//used for strcpy, etc.
#include <stdlib.h>		//used for malloc
#include <ctype.h>		//use for isprint
#include <syslog.h>
#include <arpa/inet.h>
#include <spawn.h>
#include <grp.h>
#include <CommonCrypto/CommonDigest.h>

#include <Security/Authorization.h>
#include <SystemConfiguration/SCDynamicStoreCopyDHCPInfo.h>
#include <CoreFoundation/CFPriv.h>
#include <PasswordServer/PSUtilitiesDefs.h>
#include <PasswordServer/KerberosInterface.h>
#include <PasswordServer/KerberosServiceSetup.h>

#include "DirServices.h"
#include "DirServicesUtils.h"
#include "DirServicesConst.h"
#include "DirServicesConstPriv.h"
#include "DirServicesPriv.h"
#include "CLDAPPlugInPrefs.h"
#include "LDAPv3SupportFunctions.h"

extern "C" {
	#include "saslutil.h"
};

#include "CLDAPv3Plugin.h"
#include <krb5.h>

using namespace std;

#include "ServerModuleLib.h"
#include "CRCCalc.h"
#include "CPlugInRef.h"
#include "CContinue.h"
#include "DSCThread.h"
#include "CSharedData.h"
#include "DSUtils.h"
#include "PrivateTypes.h"
#include "CLog.h"
#include "GetMACAddress.h"
#include "DSLDAPUtils.h"
#include "buffer_unpackers.h"
#include "CDSPluginUtils.h"

#pragma mark -
#pragma mark Defines and Macros

#define LDAPURLOPT95SEPCHAR			" "
#define LDAPCOMEXPSPECIALCHARS		"()|&"

#define kKerberosConfigureServices			true
#define kKerberosDoNotConfigureServices		false

#pragma mark -
#pragma mark Globals and Structures

extern bool					gServerOS;
extern DSMutexSemaphore		*gKerberosMutex;

#pragma mark -
#pragma mark Prototypes

static void RemoveKeytabEntry( const char *inPrinc );
static krb5_error_code DetermineKVNO( const char *inPrinc, char *inPass, krb5_kvno *outKVNO );
static krb5_error_code AddKeytabEntry( const char *inPrinc, char *inPass );

// service table
typedef CFErrorRef (*PrincipalHandler1)(CFStringRef inPrincipal);
typedef CFErrorRef (*PrincipalHandler2)(CFStringRef inPrincipal, CFStringRef inAdminName, const char *inPassword);

typedef union PrincipalHandler {
	PrincipalHandler1 h1;
	PrincipalHandler2 h2;
} PrincipalHandler;

typedef struct Service {
	const char *serviceType;
	PrincipalHandler handler;
} Service;

static Service gServiceTable[] = {
	{ "afpserver", {SetAFPPrincipal} },
	{ "ftp", {SetFTPPrincipal} },
	{ "imap", {SetIMAPPrincipal} },
	{ "pop", {SetPOPPrincipal} },
	{ "smtp", {SetSMTPPrincipal} },
	{ "CIFS", {(PrincipalHandler1)SetSMBPrincipal} },	// cast function defniition to
	{ "smb", {(PrincipalHandler1)SetSMBPrincipal} },	// avoid warnings
	{ "ldap", {SetLDAPPrincipal} },
	{ "ssh", {SetSSHPrincipal} },
	{ "host", {SetSSHPrincipal} },
	{ "xgrid", {SetXGridPrincipal} },
	{ "http", {SetHTTPPrincipal} },
	{ "ipp", {SetIPPPrincipal} },
	{ "nfs", {SetNFSPrincipal} },
	{ "xmpp", {SetJABBERPrincipal} },
	{ "vpn", {SetVPNPrincipal} },
	
	// NULL terminate the list
	{ NULL, {NULL} }
};

#pragma mark -
#pragma mark Support Functions

krb5_error_code AddKeytabEntry( const char *inPrinc, char *inPass )
{
	const size_t		ktPrefixSize = sizeof("WRFILE:") - 1;
	char				ktname[MAXPATHLEN + ktPrefixSize + 2];
	krb5_keytab			kt			= NULL;
	krb5_context		kContext	= NULL;
	krb5_error_code		retval;
	krb5_keytab_entry   entry;
	krb5_data			password;
	krb5_data			salt;
	SInt32				types[]		= { ENCTYPE_ARCFOUR_HMAC, ENCTYPE_DES_CBC_CRC, ENCTYPE_DES3_CBC_SHA1, 0 };
	SInt32				*keyType	= types;
	
	// let's clear variable contents
	bzero( &entry, sizeof(entry) );
	bzero( &password, sizeof(password) );
	bzero( &salt, sizeof(salt) );
	
	// let's set up the password structure
	password.data = strdup( inPass );
	password.length = strlen( password.data );
	password.magic = KV5M_PWD_DATA;
	
	// information that doesn't change...
	entry.magic = KV5M_KEYTAB;
	
	// before we add the new key, let's remove the old key for this principal
	RemoveKeytabEntry( inPrinc );
	
	gKerberosMutex->WaitLock();
	
	retval = krb5_init_context( &kContext );
	if ( retval == 0 )
	{
		// get the keytab, writable
		krb5_kt_default_name( kContext, &ktname[2], sizeof(ktname)-2 );
		ktname[0] = 'W';
		ktname[1] = 'R';
		
		retval = krb5_kt_resolve( kContext, ktname, &kt );
	}
	
	if ( retval == 0 )
	{
		// let's parse the principal we got
		retval = krb5_parse_name( kContext, inPrinc, &entry.principal );
	}
	
	if ( retval == 0 )
	{
		// let's salt it...
		retval = krb5_principal2salt( kContext, entry.principal, &salt );					
	}
	
	if ( retval == 0 )
	{
		// determine the vno for this key
		retval = DetermineKVNO( inPrinc, inPass, &entry.vno );
		if ( retval == 0 && entry.vno != 0 )
		{
			// now let's put it in the keytab accordingly
			// let's save each key type..
			do
			{
				retval = krb5_c_string_to_key( kContext, *keyType, &password, &salt, &entry.key );
				if ( retval == 0 )
				{
					// don't add it if we couldn't get the kvno
					if (entry.vno != 0) {
						
						// we don't care if we can't add it, we'll try the rest of them...
						krb5_error_code logErr = krb5_kt_add_entry( kContext, kt, &entry );
						if ( logErr != 0 )
							DbgLog( kLogPlugin, "CLDAPv3PluginNewServerSetup.cpp:AddKeytabEntry: krb5_kt_add_entry = %d", logErr );
					}
					
					// need to free the keyblock we just created with string_to_key, we leave the rest cause we are still using it..
					krb5_free_keyblock_contents( kContext, &entry.key );
				}
			} while ( *(++keyType) );
			
			// set keytab file permissions
			if ( strlen(ktname) > ktPrefixSize )
			{
				const char *filePath = ktname + ktPrefixSize;
				struct group *result = NULL;
				struct group grp = {0};
				char buffer[1024];
				int err = getgrnam_r( "_keytabusers", &grp, buffer, sizeof(buffer), &result );
				if ( err == 0 ) {
					chown( filePath, 0, grp.gr_gid );
					chmod( filePath, (S_IRUSR | S_IWUSR | S_IRGRP) );
				}
			}
		}
		else {
			DbgLog( kLogPlugin,
				"CLDAPv3PluginNewServerSetup.cpp:AddKeytabEntry: could not determine kvno for %s, error = %d",
				inPrinc, retval );
		}
		
		// need to free the salt data cause we are going to use it again..
		krb5_free_data_contents( kContext, &salt );
	}
	
	// need to free contents of entry, we don't have a pointer, this clears principal and anything left
	krb5_free_keytab_entry_contents( kContext, &entry );
	
	if ( password.data )
	{
		bzero( password.data, password.length );
		free( password.data );
		password.data = NULL;
	}
	
	// let's free stuff up..
	if ( kt != NULL )
	{
		krb5_kt_close( kContext, kt );
		kt = NULL;
	}
	
	if ( kContext != NULL )
	{
		krb5_free_context( kContext );
		kContext = NULL;
	}
	
	gKerberosMutex->SignalLock();
	
	return retval;
} // AddKeytabEntry

void RemoveKeytabEntry( const char *inPrinc )
{
	char				ktname[MAXPATHLEN+sizeof("WRFILE:")+1];
	krb5_kt_cursor		cursor		= NULL;
	krb5_keytab_entry   entry;
	krb5_principal		host_princ	= NULL;
	krb5_keytab			kt			= NULL;
	krb5_context		kContext	= NULL;
	krb5_error_code		retval;
	
	gKerberosMutex->WaitLock();
	
	bzero( &entry, sizeof(entry) );
	
	retval = krb5_init_context( &kContext );
	if ( retval == 0 )
	{
		krb5_kt_default_name( kContext, &ktname[2], sizeof(ktname)-2 );
		ktname[0] = 'W';
		ktname[1] = 'R';
	}
	
	retval = krb5_kt_resolve( kContext, ktname, &kt );
	if ( retval == 0 )
	{
		krb5_parse_name( kContext, inPrinc, &host_princ );
	}
	
	retval = krb5_kt_start_seq_get( kContext, kt, &cursor );
	if ( retval == 0 )
	{
		while( retval != KRB5_KT_END && retval != ENOENT )
		{
			if ( (retval = krb5_kt_next_entry(kContext, kt, &entry, &cursor)) )
			{
				break;
			}
			
			if ( krb5_principal_compare(kContext, entry.principal, host_princ) )
			{
				// we have to end the sequence here and start over because MIT kerberos doesn't allow
				// removal while cursoring through
				krb5_kt_end_seq_get( kContext, kt, &cursor );
				
				krb5_kt_remove_entry( kContext, kt, &entry );
				
				retval = krb5_kt_start_seq_get( kContext, kt, &cursor );
			}
			
			// need to free the contents so we don't leak
			krb5_free_keytab_entry_contents( kContext, &entry );
		}
		if ( cursor != NULL )
		{
			krb5_kt_end_seq_get( kContext, kt, &cursor );
		}
	}
	
	if ( host_princ )
	{
		// need to free the princpal we've been using.
		krb5_free_principal( kContext, host_princ );
	}
	
	if ( kt != NULL )
	{
		krb5_kt_close( kContext, kt );
		kt = NULL;
	}
	
	if ( kContext )
	{
		krb5_free_context( kContext );
		kContext = NULL;
	}
	
	gKerberosMutex->SignalLock();
	
} // RemoveKeytabEntry

#define log_on_krb5_error(A)	\
if ((A) != 0) DbgLog(kLogDebug, "DetermineKVNO error: File: %s. Line: %d", __FILE__, __LINE__)

static krb5_error_code DetermineKVNO( const char *inPrinc, char *inPass, krb5_kvno *outKVNO )
{
    krb5_ticket		*ticket		= NULL;
	krb5_creds		my_creds;
	krb5_creds		kvno_creds;
	krb5_creds 		*my_creds_out = NULL;
	krb5_principal	principal	= NULL;
	krb5_principal	cachePrinc	= NULL;
	krb5_ccache		krbCache	= NULL;
	krb5_error_code	retval;
	char			*principalString = NULL;
	char			*pCacheName = NULL;
	krb5_context	krbContext	= NULL;
	
	if ( outKVNO == NULL )
		return KRB5KDC_ERR_NULL_KEY;
	*outKVNO = 0;
	
	if ( inPrinc == NULL || inPass == NULL )
		return KRB5KDC_ERR_NULL_KEY;

	bzero( &my_creds, sizeof(my_creds) );
	bzero( &kvno_creds, sizeof(kvno_creds) );
		
	gKerberosMutex->WaitLock();

	retval = krb5_init_context( &krbContext );
	log_on_krb5_error( retval );
	if ( retval == 0 )
	{
		retval = krb5_parse_name( krbContext, inPrinc, &principal );
		log_on_krb5_error( retval );
	}
	
	if ( retval == 0 )
	{
		retval = krb5_unparse_name( krbContext, principal, &principalString );
		log_on_krb5_error( retval );
	}
	if ( retval == 0 )
	{
		pCacheName = (char *) malloc( strlen(principalString) + sizeof("MEMORY:") + 1 );	// "MEMORY:" + name + NULL
		strcpy( pCacheName,  "MEMORY:" );
		strcat( pCacheName, principalString );
	
		// let's see if we already have a cache for this user..
		retval = krb5_cc_resolve( krbContext, pCacheName, &krbCache );
		log_on_krb5_error( retval );
	}
	
	if ( retval == 0 )
	{
		retval = krb5_cc_set_default_name( krbContext, pCacheName );
		log_on_krb5_error( retval );
	}
	
	if ( retval == 0 )
	{
		// let's initialize a cache for ourselves
		retval = krb5_cc_initialize( krbContext, krbCache, principal );
		log_on_krb5_error( retval );
	}
	
	if ( retval == 0 )
	{
		krb5_int32				startTime   = 0;
		krb5_get_init_creds_opt  options;
		
		krb5_get_init_creds_opt_init( &options );
		krb5_get_init_creds_opt_set_forwardable( &options, 1 );
		krb5_get_init_creds_opt_set_proxiable( &options, 1 );
		krb5_get_init_creds_opt_set_address_list( &options, NULL );

		krb5_get_init_creds_opt_set_tkt_life( &options, 300 ); // minimum is 5 minutes

		retval = krb5_get_init_creds_password( krbContext, &my_creds, principal, inPass, NULL, 0, startTime, NULL, &options );
		log_on_krb5_error( retval );
	}
	
	if ( retval == 0 )
	{
		retval = krb5_cc_store_cred( krbContext, krbCache, &my_creds );
		log_on_krb5_error( retval );
	}
	
	if ( retval == 0 )
	{
		// now that we have the TGT, lets get a ticket for ourself
		krb5_copy_principal( krbContext, my_creds.client, &kvno_creds.client );
		krb5_copy_principal( krbContext, my_creds.client, &kvno_creds.server );

		retval = krb5_get_credentials( krbContext, 0, krbCache, &kvno_creds, &my_creds_out );
		log_on_krb5_error( retval );
	}
	
	if ( retval == 0 )
	{
		// extract the kvno
		retval = krb5_decode_ticket( &(my_creds_out->ticket), &ticket );
		log_on_krb5_error( retval );
		if ( retval == 0 )
		{
			*outKVNO = ticket->enc_part.kvno;
			
			krb5_free_ticket( krbContext, ticket );
			ticket = NULL;
		}
	}

	DSDelete( pCacheName );
	
	krb5_free_cred_contents( krbContext, &my_creds );
	krb5_free_cred_contents( krbContext, &kvno_creds );
	
	if ( my_creds_out != NULL )
	{
		krb5_free_creds( krbContext, my_creds_out );
		my_creds_out = NULL;
	}

	if ( principalString )
	{
		krb5_free_unparsed_name( krbContext, principalString );
		principalString = NULL;
	}
	
	if ( principal )
	{
		krb5_free_principal( krbContext, principal );
		principal = NULL;
	}
	
	if ( cachePrinc )
	{
		krb5_free_principal( krbContext, cachePrinc );
		cachePrinc = NULL;
	}
	
	if ( krbCache )
	{
		krb5_cc_destroy( krbContext, krbCache );
		krbCache = NULL;
	}
	
	if ( krbContext )
	{
		krb5_free_context( krbContext );
		krbContext = NULL;
	}
	
	gKerberosMutex->SignalLock();

	return retval;
}


//-----------------------------------------------------------------------------
//	ConfigureService
//
//	@discussion
//		Loop through the services and set the principal names in various
//		config files.
//-----------------------------------------------------------------------------

OSStatus ConfigureService(const char *inService, const char *inPrinc, const char *inAdmin, const char *inPassword)
{
    CFStringRef			servicePrincipal = NULL;
    CFStringRef			errorMessage = NULL;
	CFStringRef			adminString = NULL;
	CFErrorRef			theCFError = NULL;
    OSStatus			theError = noErr;
	int					tableIndex = 0;
    bool				serviceNotHandled = true;
	
	if ( inService == NULL || inPrinc == NULL )
		return -1;
	
	servicePrincipal = CFStringCreateWithCString( NULL, inPrinc, kCFStringEncodingUTF8 );
	if ( servicePrincipal == NULL )
		return -1;
	
	for ( tableIndex = 0; gServiceTable[tableIndex].serviceType != NULL; tableIndex++ )
	{
		if ( strcasecmp(inService, gServiceTable[tableIndex].serviceType) == 0 )
		{
			serviceNotHandled = false;
			
			if ( inAdmin != NULL )
				adminString = CFStringCreateWithCString( NULL, inAdmin, kCFStringEncodingUTF8 );
			
			// Note: for the h1 handlers, the extra 2 items on the stack are ignored
			theCFError = (*(gServiceTable[tableIndex].handler.h2))( servicePrincipal, adminString, inPassword );
			if ( theCFError != NULL )
				theError = CFErrorGetCode( theCFError );
			DSCFRelease( adminString );
			break;
		}
	}
	
	if ( serviceNotHandled )
	{
		DbgLog( kLogPlugin, "Don't know how to configure %s. You will need to set the principal of %s to %s",
						inService, inService, inPrinc);
		theError = noErr;
	}
	
	if ( theError != noErr )
	{
		DbgLog( kLogPlugin, "Unable to configure service %s error = %d\n", inService, (int) theError);

		if ( theCFError != NULL )
		{
			errorMessage = CFErrorCopyDescription( theCFError );
			if ( errorMessage != NULL ) {
				CFDebugLog( kLogPlugin, "Description: %@", errorMessage );
				CFRelease( errorMessage );
			}
		}
	}
	
	DSCFRelease( servicePrincipal );
	DSCFRelease( theCFError );
	
    return theError;
}


//-----------------------------------------------------------------------------
//	GetHostFromSystemConfiguration
//-----------------------------------------------------------------------------

static int GetHostFromSystemConfiguration( char *inOutHostStr, size_t maxHostStrLen )
{
	int result = -1;
	SCPreferencesRef scpRef = NULL;
	
	do
	{		
		scpRef = SCPreferencesCreate( NULL, CFSTR("DirectoryService"), 0 );
		if ( scpRef == NULL )
			break;
		
		CFDictionaryRef sysDict = (CFDictionaryRef) SCPreferencesGetValue( scpRef, CFSTR("System") );
		if ( sysDict == NULL )
			break;
		
		CFDictionaryRef sys2Dict = (CFDictionaryRef) CFDictionaryGetValue( sysDict, CFSTR("System") );
		if ( sys2Dict == NULL )
			break;
		
		CFStringRef hostString = (CFStringRef) CFDictionaryGetValue( sys2Dict, CFSTR("HostName") );
		if ( hostString == NULL )
			break;
		
		if ( CFStringGetCString(hostString, inOutHostStr, maxHostStrLen, kCFStringEncodingUTF8) )
			result = 0;
	}
	while (0);
	
	if ( scpRef != NULL )
		CFRelease( scpRef );
	
	return result;
}


//-----------------------------------------------------------------------------
//	GetHostFromAnywhere
//-----------------------------------------------------------------------------

static int GetHostFromAnywhere( char *inOutHostStr, size_t maxHostStrLen )
{
	int result = GetHostFromSystemConfiguration( inOutHostStr, maxHostStrLen );
	if ( result != 0 )
	{
		// try DNS
		unsigned long *ipList = NULL;
		struct hostent *hostEnt = NULL;
		struct sockaddr_in addr = { sizeof(struct sockaddr_in), AF_INET, 0 };
		int error_num = 0;
		
		if ( pwsf_LocalIPList(&ipList) == kCPSUtilOK )
		{
			inOutHostStr[0] = 0;
			for ( int idx = 0; ipList[idx] != 0 && inOutHostStr[0] == 0; idx++ )
			{
				addr.sin_addr.s_addr = htonl( ipList[idx] );
				hostEnt = getipnodebyaddr( &addr, sizeof(struct sockaddr_in), AF_INET, &error_num );
				if ( hostEnt != NULL ) {
					if ( hostEnt->h_name != NULL ) {
						strlcpy( inOutHostStr, hostEnt->h_name, maxHostStrLen );
						result = 0;
					}
					freehostent( hostEnt );
				}
			}
		}
	}
	
	// last resort
	if ( result != 0 )
		result = gethostname( inOutHostStr, maxHostStrLen );
	
	return result;
}


// ---------------------------------------------------------------------------
//	AddServicePrincipalToKeytab
// ---------------------------------------------------------------------------

static char *
AddServicePrincipalToKeytab(
	const char *inService,
	const char *inHost,
	const char *inRealm,
	const char *inComputerPassword )
{
	char *servPrincStr = NULL;
	size_t servPrincStrLen = 0;
	krb5_error_code retval = 0;
	char servPass[CC_SHA1_DIGEST_LENGTH + 1];
	
	if ( inService != NULL )
	{
		servPrincStrLen = strlen(inService) + strlen(inHost) + strlen(inRealm) + sizeof("/@");
		servPrincStr = (char *) malloc( servPrincStrLen );
		if ( servPrincStr != NULL )
		{
			snprintf( servPrincStr, servPrincStrLen, "%s/%s", inService, inHost );
DbgLog(kLogPlugin, "AddServicePrincipalToKeytab attempting to add service %s", servPrincStr);
			pwsf_GeneratePasswordForPrincipal( inComputerPassword, servPrincStr, servPass );
			strlcat( servPrincStr, "@", servPrincStrLen );
			strlcat( servPrincStr, inRealm, servPrincStrLen );
			retval = AddKeytabEntry( servPrincStr, servPass );
DbgLog(kLogPlugin, "AddServicePrincipalToKeytab: AddKeytabEntry = %d", retval);
			bzero( servPass, sizeof(servPass) );
		}
	}
	
	return servPrincStr;
}


// ---------------------------------------------------------------------------
//	AddServicePrincipalListToKeytab
//
//	@discussion
//		inServiceList is a comma-separated list of services
// ---------------------------------------------------------------------------

static void
AddServicePrincipalListToKeytab(
	const char *inServiceList,
	const char *inHost,
	const char *inRealm,
	const char *inComputerPassword,
	bool inConfigureServices,
	const char *inUserName,
	const char *inUserPassword )
{
	char *curService = NULL;
	char *princStr = NULL;
	char *serviceListCopy = strdup( inServiceList );
	char *nextService = serviceListCopy;
	
	do
	{
		curService = strsep( &nextService, "," );						
		princStr = AddServicePrincipalToKeytab( curService, inHost, inRealm, inComputerPassword );
		if ( inConfigureServices )
			ConfigureService( curService, princStr, inUserName, inUserPassword );
		DSFreeString( princStr );
	}
	while ( nextService != NULL );
	
	DSFreeString( serviceListCopy );
}


// ---------------------------------------------------------------------------
//	NodeNameWithHost
// ---------------------------------------------------------------------------

static tDataListPtr NodeNameWithHost( const char *inHost, int inNodeNameMax, char *outNodeName )
{
	char pNodeName[256] = {0,};
	
	if ( inHost != NULL )
	{
		strcpy( pNodeName, "/LDAPv3/" );
		strlcat( pNodeName, inHost, sizeof(pNodeName) );
		if ( outNodeName != NULL && inNodeNameMax > 0 )
			strlcpy( outNodeName, pNodeName, inNodeNameMax );
		return dsBuildFromPathPriv( pNodeName, "/" );
	}
	
	return NULL;
}


// ---------------------------------------------------------------------------
//	EnsureDollarSuffix
// ---------------------------------------------------------------------------

static void EnsureDollarSuffix( const char *inComputerIDStr, size_t inMaxLen, char *outCopyWithDollarStr )
{
	if ( inComputerIDStr != NULL && inMaxLen > 0 && outCopyWithDollarStr != NULL )
	{
		size_t len = strlcpy( outCopyWithDollarStr, inComputerIDStr, inMaxLen );
		if ( len > 0 && inComputerIDStr[len - 1] != '$' )
			strlcat( outCopyWithDollarStr, "$", inMaxLen );
	}
}


// ---------------------------------------------------------------------------
//	NewUUID
// ---------------------------------------------------------------------------

static char *NewUUID( void )
{
	char		*pUUID	= NULL;
	CFUUIDRef	cfUUID	= CFUUIDCreate( kCFAllocatorDefault );
	
	if ( cfUUID != NULL )
	{
		CFStringRef cfUUIDStr = CFUUIDCreateString( kCFAllocatorDefault, cfUUID );
		UInt32 iLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfUUIDStr), kCFStringEncodingUTF8) + 1;
		pUUID = (char *) calloc( sizeof(char), iLength );
		CFStringGetCString( cfUUIDStr, pUUID, iLength, kCFStringEncodingUTF8 );
		
		DSCFRelease( cfUUIDStr );
		CFRelease( cfUUID );	
	}
	
	return pUUID;
}


#pragma mark -
#pragma mark New Server Setup Utility Functions

// ---------------------------------------------------------------------------
//	* DoSimpleLDAPBind
// ---------------------------------------------------------------------------

LDAP *CLDAPv3Plugin::DoSimpleLDAPBind( const char *pServer, bool bSSL, bool bLDAPv2ReadOnly, char *pUsername, char *pPassword, bool bNoCleartext, 
									   SInt32 *outFailureCode )
{
	LDAP				*pLD			= NULL;
	CLDAPReplicaInfo	*replica		= NULL;
	char				ldapURL[256];
	tDirStatus			dsStatus;
	
	// create a URL and create a config and let it do the work
	snprintf( ldapURL, sizeof(ldapURL), "%s://%s", (bSSL ? "ldaps" : "ldap"), pServer );
	
	CLDAPNodeConfig *tempConfig = new CLDAPNodeConfig( fConfigFromXML, ldapURL, false );
	
	tempConfig->fLDAPv2ReadOnly = bLDAPv2ReadOnly;
	tempConfig->fIsSSL = bSSL;
	
	pLD = tempConfig->EstablishConnection( &replica, false, NULL, NULL, &dsStatus );
	if ( pLD != NULL )
	{
		if ( (replica->fSupportedSecurity & kSecDisallowCleartext) == 0 && bNoCleartext == true )
		{
			if ( outFailureCode != NULL )
				(*outFailureCode) = eDSAuthServerError;
			
			ldap_unbind_ext_s( pLD, NULL, NULL );
			pLD = NULL;
		}
		else if ( DSIsStringEmpty(pUsername) == false && DSIsStringEmpty(pPassword) == false )
		{
			if ( bNoCleartext )
				tempConfig->fSecurityLevel |= kSecDisallowCleartext;

			LDAP *pTempLD = tempConfig->EstablishConnection( &replica, false, pUsername, NULL, pPassword, NULL, NULL, &dsStatus );
			if ( pTempLD != NULL )
			{
				ldap_unbind_ext_s( pLD, NULL, NULL );
				pLD = pTempLD;
			}
			else
			{
				ldap_unbind_ext_s( pLD, NULL, NULL );
			}
			
			if ( outFailureCode != NULL )
				(*outFailureCode) = dsStatus;
		}
	}
	else
	{
		if ( outFailureCode != NULL )
			(*outFailureCode) = eDSServerTimeout;
	}
	
	DSRelease( tempConfig );
		
	return pLD;
} // DoSimpleLDAPBind


// ---------------------------------------------------------------------------
//	* GetSASLMethods
// ---------------------------------------------------------------------------

CFArrayRef CLDAPv3Plugin::GetSASLMethods( LDAP *pLD )
{
	char		*pAttributes[]  = { "supportedSASLMechanisms", NULL };
	timeval		stTimeout		= { 30, 0 }; // default 30 seconds
	CFArrayRef  cfSASLMechs		= NULL;
	LDAPMessage *pLDAPResult	= NULL;
	
	if ( ldap_search_ext_s( pLD, LDAP_ROOT_DSE, LDAP_SCOPE_BASE, "(objectclass=*)", pAttributes, false, NULL, NULL, &stTimeout, 0, &pLDAPResult ) == LDAP_SUCCESS )
	{
		cfSASLMechs = GetLDAPAttributeFromResult( pLD, pLDAPResult, "supportedSASLMechanisms" );
	}
	
	if ( pLDAPResult )
	{
		ldap_msgfree( pLDAPResult );
		pLDAPResult = NULL;
	}
	
	return cfSASLMechs;
}


// ---------------------------------------------------------------------------
//	* GetOverlayCapabilities
// ---------------------------------------------------------------------------

bool CLDAPv3Plugin::OverlaySupportsUniqueNameEnforcement( const char *inServer, bool inSSL )
{
	bool		result					= false;
	char		*pOverlayAttributes[]	= { "olcOverlay", NULL };
	timeval		stTimeout				= { 30, 0 }; // default 30 seconds
	LDAPMessage *pLDAPResult			= NULL;
	int			ldapResult				= 0;
	CFArrayRef  overlay					= NULL;
	LDAP		*pLD					= NULL;
	SInt32		bindResult				= eDSNoErr;
	
	// let's do a bind... if we can't bind anonymous to get the root DSE, then we error back
	if ( (pLD = DoSimpleLDAPBind(inServer, inSSL, false, NULL, NULL, false, &bindResult)) == NULL )
	{
		// double-check if SSL required
		if ( !inSSL )
			pLD = DoSimpleLDAPBind( inServer, true, false, NULL, NULL );
	}
	
	if ( pLD != NULL )
	{
		ldapResult = ldap_search_ext_s( pLD, "cn=config", LDAP_SCOPE_SUB, "(objectclass=olcUniqueConfig)", pOverlayAttributes,
										false, NULL, NULL, &stTimeout, 0, &pLDAPResult );
		if ( ldapResult == LDAP_SUCCESS )
		{
			overlay = GetLDAPAttributeFromResult( pLD, pLDAPResult, "olcOverlay" );
			result = (overlay != NULL && CFArrayGetCount(overlay) > 0);
			DSCFRelease( overlay );
		}
		
		if ( pLDAPResult )
		{
			ldap_msgfree( pLDAPResult );
			pLDAPResult = NULL;
		}
		
		ldap_unbind_ext_s( pLD, NULL, NULL );
		pLD = NULL;
	}
	
	return result;
}


// ---------------------------------------------------------------------------
//	* GetLDAPAttributeFromResult
// ---------------------------------------------------------------------------

CFMutableArrayRef CLDAPv3Plugin::GetLDAPAttributeFromResult( LDAP *pLD, LDAPMessage *pMessage, char *pAttribute )
{
	berval				**pBValues  = NULL;
	CFMutableArrayRef   cfResult	= CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

	if ( (pBValues = ldap_get_values_len(pLD, pMessage, pAttribute)) != NULL )
	{
		UInt32 ii = 0;
		
		while( pBValues[ii] != NULL )
		{
			CFStringRef cfAttrib = CFStringCreateWithBytes( kCFAllocatorDefault, (UInt8 *)pBValues[ii]->bv_val, pBValues[ii]->bv_len, kCFStringEncodingUTF8, false ); 
			CFArrayAppendValue( cfResult, cfAttrib );
			CFRelease( cfAttrib );
			ii++;
		}
		ldap_value_free_len( pBValues );
		pBValues = NULL;
	} 
	
	if ( CFArrayGetCount(cfResult) == 0 )
	{
		CFRelease( cfResult );
		cfResult = NULL;
	}
	
	return cfResult;
} // GetLDAPAttributeFromResult

// ---------------------------------------------------------------------------
//	* GenerateSearchString
// ---------------------------------------------------------------------------

char *CLDAPv3Plugin::GenerateSearchString( CFDictionaryRef inRecordDict )
{
	CFStringRef		cfGroupStyle	= (CFStringRef) CFDictionaryGetValue( inRecordDict, CFSTR(kXMLGroupObjectClasses) );
	CFArrayRef		cfObjectClasses = (CFArrayRef) CFDictionaryGetValue( inRecordDict, CFSTR(kXMLObjectClasses) );
	char		*pSearchString  = NULL;
	const size_t	pSearchStringSize = 256;
	
	if ( cfObjectClasses != NULL && CFArrayGetCount(cfObjectClasses) != 0 )
	{
		pSearchString = (char *) calloc( sizeof(char), pSearchStringSize );
		if ( cfGroupStyle != NULL && CFStringCompare(cfGroupStyle, CFSTR("AND"), 0) == kCFCompareEqualTo )
		{
			strcpy( pSearchString, "(&" );
		}
		else
		{
			strcpy( pSearchString, "(|" );
		}
		CFIndex iCount = CFArrayGetCount( cfObjectClasses );
		for( CFIndex ii = 0; ii < iCount; ii++ )
		{
			CFStringRef cfObjectClass = (CFStringRef) CFArrayGetValueAtIndex( cfObjectClasses, ii );
			if ( cfObjectClass != NULL && CFGetTypeID(cfObjectClass) == CFStringGetTypeID() )
			{
				strlcat( pSearchString, "(objectclass=", pSearchStringSize );
				UInt32 iLength = strlen( pSearchString );
				
				if ( iLength >= pSearchStringSize ) {
					free( pSearchString );
					return NULL;
				}
				
				// let's be sure not to go beyond our 256 chars
				CFStringGetCString( cfObjectClass, pSearchString+iLength, pSearchStringSize - iLength, kCFStringEncodingUTF8 );
				strlcat( pSearchString, ")", pSearchStringSize );
			}
		}
		// need to finish off our search string..
		strlcat( pSearchString, ")", pSearchStringSize );
	}
	
	return pSearchString;
} // GenerateSearchString

// ---------------------------------------------------------------------------
//	* CreateMappingFromConfig
// ---------------------------------------------------------------------------

CFDictionaryRef CLDAPv3Plugin::CreateMappingFromConfig( CFDictionaryRef inDict, CFStringRef inRecordType )
{
	CFArrayRef				cfRecordMap		= (CFArrayRef) CFDictionaryGetValue( inDict, CFSTR(kXMLRecordTypeMapArrayKey) );
	CFMutableDictionaryRef  cfReturnDict	= NULL;
	
	if ( cfRecordMap != NULL && CFGetTypeID(cfRecordMap) == CFArrayGetTypeID() )
	{
		CFIndex					iCount			= CFArrayGetCount( cfRecordMap );
		CFDictionaryRef			cfRecordDict	= NULL;
		
		for( CFIndex ii = 0; ii < iCount; ii++ )
		{
			CFDictionaryRef cfMapDict = (CFDictionaryRef) CFArrayGetValueAtIndex( cfRecordMap, ii );
			
			if ( CFGetTypeID(cfMapDict) == CFDictionaryGetTypeID() )
			{
				CFStringRef cfMapName = (CFStringRef) CFDictionaryGetValue( cfMapDict, CFSTR(kXMLStdNameKey) );
				
				if ( cfMapName != NULL && CFStringCompare( cfMapName, inRecordType, 0) == kCFCompareEqualTo )
				{
					cfRecordDict = cfMapDict;
					break;
				}
			}
		}
		
		if ( cfRecordDict != NULL )
		{
			// Now let's read the newly found map and find out where to look for config records..
			CFArrayRef cfNativeMap = (CFArrayRef) CFDictionaryGetValue( cfRecordDict, CFSTR(kXMLNativeMapArrayKey) );
			
			if ( cfNativeMap != NULL && CFGetTypeID(cfNativeMap) == CFArrayGetTypeID() )
			{
				if ( CFArrayGetCount(cfNativeMap) > 0 )
				{
					// let's assume we have mappings at this point...
					cfReturnDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
					
					CFDictionaryRef cfNativeDict = (CFDictionaryRef) CFArrayGetValueAtIndex( cfNativeMap, 0 );
					if ( cfNativeDict != NULL && CFGetTypeID(cfNativeDict) == CFDictionaryGetTypeID() )
					{
						CFStringRef cfSearchbase = (CFStringRef) CFDictionaryGetValue( cfNativeDict, CFSTR(kXMLSearchBase) );
						if ( cfSearchbase != NULL && CFGetTypeID(cfSearchbase) == CFStringGetTypeID() )
							CFDictionarySetValue( cfReturnDict, CFSTR(kXMLSearchBase), cfSearchbase );
						
						CFStringRef cfGroupStyle = (CFStringRef) CFDictionaryGetValue( cfNativeDict, CFSTR(kXMLGroupObjectClasses) );
						if ( cfGroupStyle != NULL && CFGetTypeID(cfGroupStyle) == CFStringGetTypeID() )
							CFDictionarySetValue( cfReturnDict, CFSTR(kXMLGroupObjectClasses), cfGroupStyle );
						
						CFArrayRef cfObjectClasses = (CFArrayRef) CFDictionaryGetValue( cfNativeDict, CFSTR(kXMLObjectClasses) );
						if ( cfObjectClasses != NULL && CFGetTypeID(cfObjectClasses) == CFArrayGetTypeID() )
							CFDictionarySetValue( cfReturnDict, CFSTR(kXMLObjectClasses), cfObjectClasses );
					}
				}
			}
			
			// Now let's read the attribute map
			CFArrayRef cfAttribMap = (CFArrayRef) CFDictionaryGetValue( cfRecordDict, CFSTR(kXMLAttrTypeMapArrayKey) );
			if ( cfAttribMap != NULL && CFGetTypeID(cfAttribMap) == CFArrayGetTypeID() )
			{
				CFMutableDictionaryRef  cfAttribDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
				
				CFDictionarySetValue( cfReturnDict, CFSTR("Attributes"), cfAttribDict );
				CFRelease( cfAttribDict ); // since we just added it to the dictionary we can release it now.
				
				iCount = CFArrayGetCount( cfAttribMap );
				for( CFIndex ii = 0; ii < iCount; ii++ )
				{
					CFDictionaryRef cfMapDict = (CFDictionaryRef) CFArrayGetValueAtIndex( cfAttribMap, ii );
					if ( cfMapDict != NULL && CFGetTypeID(cfMapDict) == CFDictionaryGetTypeID() )
					{
						CFStringRef cfName = (CFStringRef) CFDictionaryGetValue( cfMapDict, CFSTR(kXMLStdNameKey) );
						
						if ( cfName != NULL && CFGetTypeID(cfName) == CFStringGetTypeID())
						{
							CFArrayRef cfNativeMapTemp = (CFArrayRef) CFDictionaryGetValue( cfMapDict, CFSTR(kXMLNativeMapArrayKey) );
							
							if ( cfNativeMap != NULL && CFGetTypeID(cfNativeMapTemp) == CFArrayGetTypeID() )
							{
								// set the key to the name and array to the value
								CFDictionarySetValue( cfAttribDict, cfName, cfNativeMapTemp );
							}
						}
					}
				}
			}
			
			// now let's go through the Attribute Type Map and see if there are any mappings we don't already have
			cfAttribMap = (CFArrayRef) CFDictionaryGetValue( inDict, CFSTR(kXMLAttrTypeMapArrayKey) );
			if ( cfAttribMap != NULL && CFGetTypeID(cfAttribMap) == CFArrayGetTypeID() )
			{
				CFMutableDictionaryRef  cfAttribDict = (CFMutableDictionaryRef) CFDictionaryGetValue( cfReturnDict, CFSTR("Attributes") );
				if ( cfAttribDict == NULL )
				{
					cfAttribDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
					CFDictionarySetValue( cfReturnDict, CFSTR("Attributes"), cfAttribDict );
					CFRelease( cfAttribDict ); // since we just added it to the dictionary we can release it now.
				}
				
				iCount = CFArrayGetCount( cfAttribMap );
				for( CFIndex ii = 0; ii < iCount; ii++ )
				{
					CFDictionaryRef cfMapDict = (CFDictionaryRef) CFArrayGetValueAtIndex( cfAttribMap, ii );
					if ( cfMapDict != NULL && CFGetTypeID(cfMapDict) == CFDictionaryGetTypeID() )
					{
						CFStringRef cfName = (CFStringRef) CFDictionaryGetValue( cfMapDict, CFSTR(kXMLStdNameKey) );
						
						if ( cfName != NULL && CFGetTypeID(cfName) == CFStringGetTypeID())
						{
							CFArrayRef cfNativeMapTemp = (CFArrayRef) CFDictionaryGetValue( cfMapDict, CFSTR(kXMLNativeMapArrayKey) );
							
							if ( cfNativeMapTemp != NULL && CFGetTypeID(cfNativeMapTemp) == CFArrayGetTypeID() )
							{
								// if the key doesn't already exist, let's go ahead and put the high level one in there..
								// it's not additive, only if it is missing
								if ( CFDictionaryGetValue(cfAttribDict, cfName) == NULL )
								{
									// set the key to the name and array to the value
									CFDictionarySetValue( cfAttribDict, cfName, cfNativeMapTemp );
								}
							}
						}
					}
				}
			}
		}
	}
	
	return cfReturnDict;
} // CreateMappingFromConfig

// ---------------------------------------------------------------------------
//	* GetAttribFromDict
// ---------------------------------------------------------------------------

// Puts first mapping from attribute into outFirstAttribute...
CFArrayRef CLDAPv3Plugin::GetAttribFromRecordDict( CFDictionaryRef inDict, CFStringRef inAttribute, char **outFirstAttribute, CFStringRef *outCFFirstAttribute )
{
	CFArrayRef		cfAttributeArray	= NULL;
	CFDictionaryRef cfAttribMap			= (CFDictionaryRef) CFDictionaryGetValue( inDict, CFSTR("Attributes") );
	
	if ( cfAttribMap != NULL )
	{
		cfAttributeArray = (CFArrayRef) CFDictionaryGetValue( cfAttribMap, inAttribute );
		if ( (outFirstAttribute != NULL || outCFFirstAttribute != NULL) && cfAttributeArray != NULL && CFGetTypeID(cfAttributeArray) == CFArrayGetTypeID() && CFArrayGetCount( cfAttributeArray ) > 0 )
		{
			CFStringRef cfAttribute = (CFStringRef) CFArrayGetValueAtIndex( cfAttributeArray, 0 );

			if ( cfAttribute != NULL && CFGetTypeID(cfAttribute) == CFStringGetTypeID() )
			{
				if ( outFirstAttribute != NULL )
				{
					UInt32 iLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfAttribute), kCFStringEncodingUTF8) + 1;
					*outFirstAttribute = (char *) calloc( sizeof(char), iLength );
					CFStringGetCString( cfAttribute, *outFirstAttribute, iLength, kCFStringEncodingUTF8 );
				}
				
				if ( outCFFirstAttribute != NULL )
				{
					*outCFFirstAttribute = cfAttribute;
				}
			}
		}
	}
	else if ( outCFFirstAttribute ) {
		*outCFFirstAttribute = NULL;
	}
	return cfAttributeArray;
}// GetAttribFromDict


// ---------------------------------------------------------------------------
//	* IsServerInConfig
// ---------------------------------------------------------------------------

bool CLDAPv3Plugin::IsServerInConfig( CFDictionaryRef inConfig, CFStringRef inServerName, CFIndex *outIndex, CFMutableDictionaryRef *outConfig )
{
	bool				bReturn			= false;
	bool				bFreeInConfig   = false;
	CFMutableArrayRef	cfConfigList	= NULL;
	
	// if we weren't passed a config, let's get our current configuration
	if ( inConfig == NULL )
	{
		CFDataRef cfTempData = fConfigFromXML->CopyLiveXMLConfig();
		if ( cfTempData != NULL )
		{
			inConfig = (CFDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfTempData, kCFPropertyListImmutable, NULL );
			
			// we're done with the TempData at this point
			DSCFRelease( cfTempData );
		}
				
		bFreeInConfig = true;
	}
	
	// let's grab the configured list of servers...
	if ( inConfig != NULL )
		cfConfigList = (CFMutableArrayRef) CFDictionaryGetValue( inConfig, CFSTR(kXMLConfigArrayKey) );
	if ( cfConfigList != NULL )
	{
		CFIndex iCount = CFArrayGetCount( cfConfigList );
		for( CFIndex ii = 0; ii < iCount; ii++ )
		{
			CFMutableDictionaryRef cfConfigServerDict = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( cfConfigList, ii );
			CFStringRef		cfTempServer = (CFStringRef) CFDictionaryGetValue( cfConfigServerDict, CFSTR(kXMLServerKey) );
			
			if ( cfTempServer && CFStringCompare( cfTempServer, inServerName, kCFCompareCaseInsensitive) == kCFCompareEqualTo )
			{
				if ( outConfig )
				{
					*outConfig = cfConfigServerDict;
					CFRetain( cfConfigServerDict ); // we need to retain cause we're going to release below
				}
				if ( outIndex )
				{
					*outIndex = ii;
				}
				bReturn = true;
				break;
			}
		}
	}
	
	if ( bFreeInConfig )
		DSCFRelease( inConfig );
	
	return bReturn;
} // IsServerInConfig

// ---------------------------------------------------------------------------
//	* GetServerInfoFromConfig
// ---------------------------------------------------------------------------

CFStringRef CLDAPv3Plugin::GetServerInfoFromConfig( CFDictionaryRef inDict, char **outServer, bool *outSSL, bool *outLDAPv2ReadOnly, char **pUsername, char **pPassword )
{
	// automatically released, just a Get.
	CFStringRef cfServer = (CFStringRef) CFDictionaryGetValue( inDict, CFSTR(kXMLServerKey) );
	if ( cfServer != NULL && CFGetTypeID(cfServer) == CFStringGetTypeID() && outServer != NULL )
	{
		UInt32 iLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfServer), kCFStringEncodingUTF8) + 1;
		*outServer = (char *) calloc( sizeof(char), iLength );
		CFStringGetCString( cfServer, *outServer, iLength, kCFStringEncodingUTF8 );
	}
	
	if ( outSSL )
	{
		CFBooleanRef cfBool = (CFBooleanRef) CFDictionaryGetValue( inDict, CFSTR(kXMLIsSSLFlagKey) );
		if ( cfBool != NULL && CFGetTypeID(cfBool) == CFBooleanGetTypeID() )
		{
			*outSSL = CFBooleanGetValue( cfBool );
		}
	}

	if ( outLDAPv2ReadOnly )
	{
		CFBooleanRef cfBool = (CFBooleanRef) CFDictionaryGetValue( inDict, CFSTR(kXMLLDAPv2ReadOnlyKey) );
		if ( cfBool != NULL && CFGetTypeID(cfBool) == CFBooleanGetTypeID() )
		{
			*outLDAPv2ReadOnly = CFBooleanGetValue( cfBool );
		}
	}

	if ( pUsername )
	{
		CFStringRef cfUsername = (CFStringRef) CFDictionaryGetValue( inDict, CFSTR(kXMLServerAccountKey) );
		
		if ( cfUsername != NULL && CFGetTypeID(cfUsername) == CFStringGetTypeID() && CFStringGetLength(cfUsername) != 0 )
		{
			UInt32 iLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfUsername), kCFStringEncodingUTF8) + 1;
			*pUsername = (char *) calloc( sizeof(char), iLength );
			CFStringGetCString( cfUsername, *pUsername, iLength, kCFStringEncodingUTF8 );
		}
	}

	if ( pPassword )
	{
		CFStringRef cfPassword = (CFStringRef) CFDictionaryGetValue( inDict, CFSTR(kXMLServerPasswordKey) );
		
		if ( cfPassword != NULL && CFGetTypeID(cfPassword) == CFStringGetTypeID() && CFStringGetLength(cfPassword) != 0 )
		{
			UInt32 iLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfPassword), kCFStringEncodingUTF8) + 1;
			*pPassword = (char *) calloc( sizeof(char), iLength );
			CFStringGetCString( cfPassword, *pPassword, iLength, kCFStringEncodingUTF8 );
		}
	}
	
	return cfServer;
} // GetServerInfoFromConfig


#pragma mark -
#pragma mark New Server Setup Functions

// ---------------------------------------------------------------------------
//	* DoNewServerConfig
// ---------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::DoNewServerDiscovery( sDoPlugInCustomCall *inData )
{
	SInt32					siResult		= eDSBogusServer;
	bool					bSSL			= false;
	bool					bLDAPv2ReadOnly	= false;
	CFMutableDictionaryRef  cfXMLDict		= NULL;
	LDAP					*pLD			= NULL;
	char					*pServer		= NULL;
	
	try {
		// we should always have XML data for this process, if we don't, throw an error
		cfXMLDict = GetXMLFromBuffer( inData->fInRequestData );
		if ( cfXMLDict == NULL ) {
			throw( (SInt32) eDSInvalidBuffFormat );
		}
		
		// automatically released, just a Get.
		CFStringRef cfServer = GetServerInfoFromConfig( cfXMLDict, &pServer, &bSSL, &bLDAPv2ReadOnly, NULL, NULL );
		if ( cfServer == NULL )
		{
			throw( (SInt32) eDSInvalidBuffFormat );
		}
		
		// see if server is already in our config, return error if so
		if ( inData->fInRequestCode == eDSCustomCallLDAPv3NewServerDiscoveryNoDupes && IsServerInConfig(NULL, cfServer, NULL, NULL) )
		{
			throw( (SInt32) eDSRecordAlreadyExists );
		}
		
		// this is an attempt to determine the server, see if we can contact it, and get some basic information

		// first let's get the root DSE and see what we can figure out..
		char			*pAttributes[]  = { "namingContexts", "defaultNamingContext", NULL };
		timeval			stTimeout		= { kLDAPDefaultOpenCloseTimeoutInSeconds, 0 }; // default kLDAPDefaultOpenCloseTimeoutInSeconds seconds
		LDAPMessage		*pLDAPResult	= NULL;
		SInt32			bindResult		= eDSNoErr;
		
		// let's do a bind... if we can't bind anonymous to get the root DSE, then we error back
		if ( (pLD = DoSimpleLDAPBind(pServer, bSSL, bLDAPv2ReadOnly, NULL, NULL, false, &bindResult)) == NULL )
		{
			if ( bindResult == eDSReadOnly)
			{
				bLDAPv2ReadOnly = true;
				if ( (pLD = DoSimpleLDAPBind(pServer, bSSL, bLDAPv2ReadOnly, NULL, NULL)) == NULL ) {
					// eDSBogusServer means no server responding at that address
					throw( (SInt32) eDSBogusServer );
				}
			}
			// if we aren't already SSL, let's try it just to be sure..
			else if ( !bSSL && !bLDAPv2ReadOnly )
			{
				bSSL = true;
				if ( (pLD = DoSimpleLDAPBind(pServer, bSSL, bLDAPv2ReadOnly, NULL, NULL)) == NULL ) {
					// eDSBogusServer means no server responding at that address
					throw( (SInt32) eDSBogusServer );
				}
			}
			else if ( bSSL )
			{
				// try without SSL so we can notify the user server does not support SSL
				bSSL = false;
				if ( (pLD = DoSimpleLDAPBind(pServer, bSSL, bLDAPv2ReadOnly, NULL, NULL)) != NULL ) {
					// eNotHandledByThisNode means server does not support SSL
					throw( (SInt32) eNotHandledByThisNode );
				}
			}
			else
			{
				// eDSBogusServer means no server responding at that address
				throw( (SInt32) eDSBogusServer );
			}
		}
		
		// now let's continue our discovery process
		int iLDAPRetCode = ldap_search_ext_s( pLD, LDAP_ROOT_DSE, LDAP_SCOPE_BASE, "(objectclass=*)", pAttributes, false, NULL, NULL, &stTimeout, 0, &pLDAPResult );
		
		// This should never fail, unless we aren't really talking to an LDAP server
		if ( iLDAPRetCode != LDAP_SUCCESS && !bLDAPv2ReadOnly )
		{
			// eDSBogusServer means we couldn't really talk to the server.. so let's consider it bogus??
			throw( (SInt32) eDSBogusServer );
		}
		
		// let's see if this server has a defaultNamingContext (iPlanet & Others).
		CFArrayRef cfNameContext = GetLDAPAttributeFromResult( pLD, pLDAPResult, "defaultNamingContext" );
		if ( cfNameContext == NULL )
		{
			// if no defaultNamingContext, let's get the normal attribute.  UI will handle more than 1 context
			cfNameContext = GetLDAPAttributeFromResult( pLD, pLDAPResult, "namingContexts" );
		}
		
		if ( pLDAPResult )
		{
			ldap_msgfree( pLDAPResult );
			pLDAPResult = NULL;
		}
		
		// at this point the server has responded...
		siResult = eDSNoErr;
		
		// Now let's search for Server mappings to see if they exist, cause hopefully it will be an OD server
		if ( cfNameContext )
		{
			CFIndex iNumContexts = CFArrayGetCount( cfNameContext );
			bool	bFound = false;
			char	*pAttributes2[]  = { "description", NULL };
			berval  **pBValues  = NULL;
			
			for( CFIndex ii = 0; ii < iNumContexts && !bFound; ii++ )
			{
				CFStringRef cfSearchbase = (CFStringRef) CFArrayGetValueAtIndex( cfNameContext, ii );
				
				// if we only have 1 naming context, let's go ahead and set the Template Search Base,in case we don't get Server Mappings
				if ( iNumContexts == 1 )
				{
					CFDictionarySetValue( cfXMLDict, CFSTR(kXMLTemplateSearchBaseSuffix), cfSearchbase );
				}
				
				UInt32 iLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfSearchbase), kCFStringEncodingUTF8) + 1;
				char *pSearchbase = (char *) calloc( sizeof(char), iLength );
				CFStringGetCString( cfSearchbase, pSearchbase, iLength, kCFStringEncodingUTF8 );
				
				iLDAPRetCode = ldap_search_ext_s( pLD, pSearchbase, LDAP_SCOPE_SUBTREE, "(&(objectclass=organizationalUnit)(ou=macosxodconfig))", pAttributes2, false, NULL, NULL, &stTimeout, 0, &pLDAPResult );
				
				if ( iLDAPRetCode == LDAP_SUCCESS )
				{
					if ( (pBValues = ldap_get_values_len(pLD, pLDAPResult, "description")) != NULL )
					{
						// good, we have a server mappings config...  Let's use the config we just got from the server
						CFDataRef cfXMLData = CFDataCreate( kCFAllocatorDefault, (UInt8 *)(pBValues[0]->bv_val), pBValues[0]->bv_len );
						
						if ( cfXMLData != NULL )
						{
							// extract the config dictionary from the XML data and this is our new configuration
							CFMutableDictionaryRef cfTempDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfXMLData, kCFPropertyListMutableContainersAndLeaves, NULL );
							
							// make sure we got a dictionary and it has something in it...
							if ( cfTempDict && CFGetTypeID(cfTempDict) == CFDictionaryGetTypeID() && CFDictionaryGetCount(cfTempDict) )
							{
								CFArrayRef  cfRecordMap = (CFArrayRef) CFDictionaryGetValue( cfTempDict, CFSTR(kXMLRecordTypeMapArrayKey) );
								
								// let's see if we have a Record Map, if not, we won't consider this a valid server config
								if ( cfRecordMap && CFArrayGetCount(cfRecordMap) != 0 )
								{
									// let's reset server setting to the server the user entered
									CFDictionarySetValue( cfTempDict, CFSTR(kXMLServerKey), cfServer );
									
									// set server mappings again, since the ones we got don't have the flag on
									CFDictionarySetValue( cfTempDict, CFSTR(kXMLServerMappingsFlagKey), kCFBooleanTrue );
									
									// let's release the old config, since we are exchanging them..
									DSCFRelease( cfXMLDict );

									// let's exchange them and NULL cfTempDict otherwise we'll release it accidently
									cfXMLDict = cfTempDict;
									cfTempDict = NULL;
									
									// we found it...
									bFound = true;
								}
							}
							
							DSCFRelease( cfTempDict );
							DSCFRelease( cfXMLData ); // let's release it, we're done with it
						}
						
						ldap_value_free_len( pBValues );
						pBValues = NULL;
					} 
					
					ldap_msgfree( pLDAPResult );
					pLDAPResult = NULL;
				}
				
				DSFreeString( pSearchbase );
			}

			DSCFRelease( cfNameContext );
		}
		
		// let's set the key for when we send it back...
		int iPortNumber = (bSSL ? LDAPS_PORT : LDAP_PORT);
		CFNumberRef cfPortNumber = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &iPortNumber ); 
		
		CFDictionarySetValue( cfXMLDict, CFSTR(kXMLPortNumberKey), cfPortNumber );
		CFDictionarySetValue( cfXMLDict, CFSTR(kXMLIsSSLFlagKey), (bSSL ? kCFBooleanTrue : kCFBooleanFalse) );
		CFDictionarySetValue( cfXMLDict, CFSTR(kXMLLDAPv2ReadOnlyKey), (bLDAPv2ReadOnly ? kCFBooleanTrue : kCFBooleanFalse) );
		
		DSCFRelease( cfPortNumber );
		
	} catch( SInt32 err ) {
		siResult = err;
	} catch( ... ) {
		// catch all for miss throws...
		siResult = eUndefinedError;
	}
	
	if ( pServer != NULL )
	{
		free( pServer );
		pServer = NULL;
	}
	
	if ( pLD != NULL )
	{
		ldap_unbind_ext_s( pLD, NULL, NULL );
		pLD = NULL;
	}
	
	if ( cfXMLDict != NULL )
	{
		if ( siResult == eDSNoErr )
		{
			SInt32 iError = PutXMLInBuffer( cfXMLDict, inData->fOutRequestResponse );
			if ( iError != eDSNoErr ) {
				siResult = iError;
			}
		}

		DSCFRelease( cfXMLDict );
	}
	
	return siResult;
} // DoNewServerDiscovery

// ---------------------------------------------------------------------------
//	* DoNewServerVerifySettings
// ---------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::DoNewServerVerifySettings( sDoPlugInCustomCall *inData )
{
	SInt32					siResult				= eDSInvalidNativeMapping;
	SInt32					siBindResult			= eDSAuthFailed;
	LDAP					*pLD					= NULL;
	CFMutableDictionaryRef  cfXMLDict				= NULL;
	bool					bSSL					= false;
	char					*pServer				= NULL;
	char					*pObjectSearchString	= NULL;
	char					*pSearchbase			= NULL;
	LDAPMessage				*pLDAPResult			= NULL;
	char					*pUsername				= NULL;
	char					*pPassword				= NULL;
	timeval					stTimeout				= { 30, 0 }; // default 30 seconds
	CFDictionaryRef			cfRecordMap				= NULL;
	bool					bLDAPv2ReadOnly			= false;
	
	try
	{
		// we should always have XML data for this process, if we don't, throw an error
		cfXMLDict = GetXMLFromBuffer( inData->fInRequestData );
		if ( cfXMLDict == NULL ) {
			throw( (SInt32) eDSInvalidBuffFormat );
		}
		
		// Automatically released, just a Get.
		CFStringRef cfServer = GetServerInfoFromConfig( cfXMLDict, &pServer, &bSSL, &bLDAPv2ReadOnly, &pUsername, &pPassword );
		if ( cfServer == NULL )
		{
			throw( (SInt32) eDSInvalidBuffFormat );
		}
		
		// let's attempt to query something from the server... users are the safest, but we'll try groups, then computers if previous do not exist
		cfRecordMap = CreateMappingFromConfig( cfXMLDict, CFSTR(kDSStdRecordTypeUsers) );
		if ( cfRecordMap == NULL )
		{
			cfRecordMap = CreateMappingFromConfig( cfXMLDict, CFSTR(kDSStdRecordTypeGroups) );
			if ( cfRecordMap == NULL )
			{
				cfRecordMap = CreateMappingFromConfig( cfXMLDict, CFSTR(kDSStdRecordTypeComputers) );
				if ( cfRecordMap == NULL )
				{
					throw( (SInt32) eDSInvalidNativeMapping );
				}
			}
		}
		
		// Now continue to try a query to the server after building a search string appropriately
		if ( (pLD = DoSimpleLDAPBind(pServer, bSSL, bLDAPv2ReadOnly, pUsername, pPassword, false, &siBindResult)) == NULL )
		{
			throw( siBindResult );
		}
		
		// First let's get Searchbase from config
		CFStringRef cfSearchbase = (CFStringRef) CFDictionaryGetValue( cfRecordMap, CFSTR(kXMLSearchBase) );
		if ( cfSearchbase != NULL)
		{
			UInt32 iLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfSearchbase), kCFStringEncodingUTF8) + 1;
			pSearchbase = (char *) calloc( sizeof(char), iLength );
			CFStringGetCString( cfSearchbase, pSearchbase, iLength, kCFStringEncodingUTF8 );
		}
		
		// Let's start our search string with a Parenthesis
		pObjectSearchString = GenerateSearchString( cfRecordMap );
		if ( pObjectSearchString == NULL )
		{
			throw( (SInt32) eDSInvalidNativeMapping );
		}
		
		// Don't care what attributes, let's get them all, since all we want are the names
		int iLDAPRetCode = ldap_search_ext_s( pLD, pSearchbase, LDAP_SCOPE_SUBTREE, pObjectSearchString, NULL, true, NULL, NULL, &stTimeout, 1, &pLDAPResult );

		// we might get a size limit exceeded message since we're only asking for 1
		if ( iLDAPRetCode == LDAP_SUCCESS || iLDAPRetCode == LDAP_SIZELIMIT_EXCEEDED )
		{
			int iCount = ldap_count_entries( pLD, pLDAPResult );

			// well if we didn't get any results, must not have access..
			if ( iCount == 0 )
			{
				siResult = eDSAuthParameterError;
			}
			else
			{
				siResult = eDSNoErr;
			}
		}
		
		if ( pLDAPResult != NULL )
		{
			ldap_msgfree( pLDAPResult );
			pLDAPResult = NULL;
		}
		
	} catch( SInt32 iError ) {
		siResult = iError;
	} catch( ... ) {
		// catch all for miss throws...
		siResult = eUndefinedError;
	}
	
	DSCFRelease( cfRecordMap );
	DSFreeString( pObjectSearchString );
	DSFreeString( pSearchbase );
	DSFreeString( pServer );
	DSFreeString( pUsername );
	
	if ( pPassword != NULL )
	{
		free( pPassword );
		pPassword = NULL;
	}
	
	if ( pLD != NULL )
	{
		ldap_unbind_ext_s( pLD, NULL, NULL );
		pLD = NULL;
	}
	
	if ( cfXMLDict != NULL )
	{
		if ( siResult == eDSNoErr )
		{
			SInt32 iError = PutXMLInBuffer( cfXMLDict, inData->fOutRequestResponse );
			if ( iError != eDSNoErr ) {
				siResult = iError;
			}
		}
		
		CFRelease( cfXMLDict );
		cfXMLDict = NULL;
	}
	
	return siResult;
} // DoNewServerVerifySettings

// ---------------------------------------------------------------------------
//	* DoNewServerGetConfig
// ---------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::DoNewServerGetConfig( sDoPlugInCustomCall *inData )
{
	bool					bSSL			= false;
	SInt32					siResult			= eDSInvalidNativeMapping;
	SInt32					siBindResult		= eDSNoErr;
	timeval					stTimeout			= { 30, 0 };	// 30 second timeout
	CFMutableDictionaryRef  cfXMLDict			= NULL;
	CFDictionaryRef			cfConfigMap			= NULL;
	CFMutableDictionaryRef  cfSupportedSecLevel = NULL;
	LDAP					*pLD				= NULL;
	char					*pServer			= NULL;
	char					*pSearchString		= NULL;
	char					*pKerbSearchString	= NULL;
	char					*pSearchbase		= NULL;
	char					*pAttribute			= NULL;
	char					*pUsername			= NULL;
	char					*pPassword			= NULL;
	bool					bLDAPv2ReadOnly		= false;
	
	try {
		
		// we should always have XML data for this process, if we don't, throw an error
		cfXMLDict = GetXMLFromBuffer( inData->fInRequestData );
		if ( cfXMLDict == NULL )
		{
			throw( (SInt32) eDSInvalidBuffFormat );
		}
		
		// automatically released, just a Get.
		CFStringRef cfServer = GetServerInfoFromConfig( cfXMLDict, &pServer, &bSSL, &bLDAPv2ReadOnly, &pUsername, &pPassword );
		if ( cfServer == NULL )
		{
			throw( (SInt32) eDSInvalidBuffFormat );
		}
		
		// next we are going to get the Policy...  "macosxodpolicy"
		// Keys -   kXMLDirectoryBindingKey		-   true or false			= Whether Directory Binding is allowed
		//			kXMLConfiguredSecurityKey and kXMLSupportedSecurityKey	-   Dictionary with keys:
		//
		//					on = required, otherwise optional
		//						kXMLSecurityBindingRequired		= Directory Binding
		//						kXMLSecurityNoClearTextAuths	= SASL or encryption
		//						kXMLSecurityManInTheMiddle		= Man in the Middle
		//						kXMLSecurityPacketSigning		= Packet Signing
		//						kXMLSecurityPacketEncryption	= Encryption
		//
		//				By default, if Mutual Auth or Encryption can be done, it will be attempted, if possible
		
		// First, let's parse our SASL mechanisms and set what the server is capable of
		if ( (pLD = DoSimpleLDAPBind(pServer, bSSL, bLDAPv2ReadOnly, pUsername, pPassword, false, &siBindResult)) == NULL )
		{
			throw( siBindResult );
		}

		// Start the dictionary for the Security Levels supported
		cfSupportedSecLevel = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		
		// Store the dictionary in the configuration..
		CFDictionarySetValue( cfXMLDict, CFSTR(kXMLSupportedSecurityKey), cfSupportedSecLevel );
		
		// if we have SSL, we have some capabilities already..
		if ( bSSL )
		{
			CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityNoClearTextAuths), kCFBooleanTrue );
			CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityPacketEncryption), kCFBooleanTrue );
		}
		
		// get sasl mechanisms for later use...  TBD --- Do we need to discover this dynamically from ourselves / LDAP Framework????
		CFArrayRef cfSASLMechs = GetSASLMethods( pLD );
		if ( cfSASLMechs )
		{
			CFRange		stRange = CFRangeMake( 0, CFArrayGetCount(cfSASLMechs) );
			
			if ( CFArrayContainsValue( cfSASLMechs, stRange, CFSTR("CRAM-MD5")) && fLDAPConnectionMgr->IsSASLMethodSupported(CFSTR("CRAM-MD5")) )
			{
				CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityNoClearTextAuths), kCFBooleanTrue );
			}
			
			if ( CFArrayContainsValue( cfSASLMechs, stRange, CFSTR("GSSAPI")) && fLDAPConnectionMgr->IsSASLMethodSupported(CFSTR("GSSAPI")) )
			{
				CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityNoClearTextAuths), kCFBooleanTrue );
				CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityManInTheMiddle), kCFBooleanTrue );
				CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityPacketSigning), kCFBooleanTrue );
				CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityPacketEncryption), kCFBooleanTrue );
			}
			
			CFRelease( cfSASLMechs );
			cfSASLMechs = NULL;
		}
				
		// First let's see if we have a Mapping for the Config container, if not, we're done here..
		cfConfigMap = CreateMappingFromConfig( cfXMLDict, CFSTR(kDSStdRecordTypeConfig) );
		if ( cfConfigMap == NULL )
		{
			throw( (SInt32) eDSNoErr );
		}
		
		// Next let's get Searchbase from config
		CFStringRef cfSearchbase = (CFStringRef) CFDictionaryGetValue( cfConfigMap, CFSTR(kXMLSearchBase) );
		if ( cfSearchbase != NULL)
		{
			UInt32 iLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfSearchbase), kCFStringEncodingUTF8) + 1;
			pSearchbase = (char *) calloc( sizeof(char), iLength );
			CFStringGetCString( cfSearchbase, pSearchbase, iLength, kCFStringEncodingUTF8 );
		}
		
		// Let's start our search string with a Parenthesis
		char *pObjectSearchString = GenerateSearchString( cfConfigMap );
		if ( pObjectSearchString != NULL )
		{
			pSearchString  = (char *) calloc( sizeof(char), strlen(pObjectSearchString) + 22 + 1 ); // for the rest + NULL
			
			strcpy( pSearchString, "(&" );					// we want all of these to be true
			strcat( pSearchString, pObjectSearchString );   // add the objectlist
			strcat( pSearchString, "(cn=macosxodpolicy)" ); // add the name we're looking for
			strcat( pSearchString, ")" );   // close off the list
			
			pKerbSearchString = (char *) calloc( sizeof(char), 2 + strlen(pObjectSearchString) + 19 + 1 );
			strcpy( pKerbSearchString, "(&" );					// we want all of these to be true
			strcat( pKerbSearchString, pObjectSearchString );   // add the objectlist
			strcat( pKerbSearchString, "(cn=KerberosClient)" ); // add the name we're looking for
			strcat( pKerbSearchString, ")" );   // close off the list
			
			free( pObjectSearchString );
			pObjectSearchString = NULL;
		}
		else
		{
			throw( (SInt32) eDSNoErr );
		}
		
		CFArrayRef cfAttributeArray = GetAttribFromRecordDict( cfConfigMap, CFSTR(kDS1AttrXMLPlist), &pAttribute );
		if ( cfAttributeArray == NULL )
		{
			throw( (SInt32) eDSNoErr );
		}
		
		LDAPMessage		*pLDAPResult		= NULL;

		int iLDAPRetCode = ldap_search_ext_s( pLD, pSearchbase, LDAP_SCOPE_SUBTREE, pSearchString, NULL, false, NULL, NULL, &stTimeout, 0, &pLDAPResult );
		
		if ( iLDAPRetCode == LDAP_SUCCESS )
		{
			berval  **pBValues  = NULL;
			
			if ( (pBValues = ldap_get_values_len(pLD, pLDAPResult, pAttribute)) != NULL )
			{
				// good, we have a policy config record...
				CFDataRef cfXMLData = CFDataCreate( kCFAllocatorDefault, (UInt8 *)(pBValues[0]->bv_val), pBValues[0]->bv_len );
				
				if ( cfXMLData != NULL )
				{
					// extract the config dictionary from the XML data and this is our new configuration
					CFDictionaryRef cfTempDict = (CFDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfXMLData, kCFPropertyListImmutable, NULL );
					
					// make sure we got a dictionary and it has something in it...
					if ( cfTempDict )
					{
						CFDictionaryRef cfSecurityLevel = (CFDictionaryRef) CFDictionaryGetValue( cfTempDict, CFSTR(kXMLConfiguredSecurityKey) );
						if ( cfSecurityLevel != NULL && CFGetTypeID(cfSecurityLevel) == CFDictionaryGetTypeID() )
						{
							CFDictionarySetValue( cfXMLDict, CFSTR(kXMLConfiguredSecurityKey), cfSecurityLevel );
						}
						
						CFBooleanRef cfBindingActive = (CFBooleanRef) CFDictionaryGetValue( cfTempDict, CFSTR(kXMLDirectoryBindingKey) );
						if ( cfBindingActive != NULL && CFGetTypeID(cfBindingActive) == CFBooleanGetTypeID() )
						{
							CFDictionarySetValue( cfXMLDict, CFSTR(kXMLDirectoryBindingKey), cfBindingActive );
						}
						
						CFRelease( cfTempDict );
						cfTempDict = NULL;
					}
					
					CFRelease( cfXMLData ); // let's release it, we're done with it
					cfXMLData = NULL;
				}
				
				ldap_value_free_len( pBValues );
				pBValues = NULL;
			} 
		}

		if ( pLDAPResult )
		{
			ldap_msgfree( pLDAPResult );
			pLDAPResult = NULL;
		}
		
		//
		// now let's figure out the realm of the server so that we can create a temporary kerberos if necessary
		iLDAPRetCode = ldap_search_ext_s( pLD, pSearchbase, LDAP_SCOPE_SUBTREE, pKerbSearchString, NULL, false, NULL, NULL, &stTimeout, 0, &pLDAPResult );
		
		if ( iLDAPRetCode == LDAP_SUCCESS )
		{
			berval  **pBValues  = NULL;
			
			if ( (pBValues = ldap_get_values_len(pLD, pLDAPResult, pAttribute)) != NULL )
			{
				// good, we have a policy config record...
				CFDataRef cfXMLData = CFDataCreate( kCFAllocatorDefault, (UInt8 *)(pBValues[0]->bv_val), pBValues[0]->bv_len );
				
				if ( cfXMLData != NULL )
				{
					// extract the config dictionary from the XML data and this is our new configuration
					CFDictionaryRef cfTempDict = (CFDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfXMLData, kCFPropertyListImmutable, NULL );
					
					// make sure we got a dictionary and it has something in it...
					if ( cfTempDict != NULL )
					{
						CFDictionaryRef cfWorkingDict;
						
						cfWorkingDict = (CFDictionaryRef) CFDictionaryGetValue( cfTempDict, CFSTR("edu.mit.kerberos") );
						if ( cfWorkingDict != NULL )
						{
							cfWorkingDict = (CFDictionaryRef) CFDictionaryGetValue( cfWorkingDict, CFSTR("libdefaults") );
							if ( cfWorkingDict != NULL )
							{
								CFStringRef cfString = (CFStringRef) CFDictionaryGetValue( cfWorkingDict, CFSTR("default_realm") );
								
								if ( cfString != NULL )
								{
									UInt32 iLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfString), kCFStringEncodingUTF8) + 1;
									char *pKerberosRealm = (char *) calloc( sizeof(char), iLength );
									CFStringGetCString( cfString, pKerberosRealm, iLength, kCFStringEncodingUTF8 );
									
									VerifyKerberosForRealm( pKerberosRealm, pServer );
									
									// we will cheat here and set the kerberosID so that it uses the right realm
									// we'll fill in the name later..
									CFStringRef cfKerbID = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("@%@"), cfString );
									CFDictionarySetValue( cfXMLDict, CFSTR(kXMLKerberosId), cfKerbID );
									CFRelease( cfKerbID );
									
									free( pKerberosRealm );
									pKerberosRealm = NULL;
								}
							}
						}
						
						CFRelease( cfTempDict );
						cfTempDict = NULL;
					}
					
					CFRelease( cfXMLData ); // let's release it, we're done with it
					cfXMLData = NULL;
				}
				
				ldap_value_free_len( pBValues );
				pBValues = NULL;
			} 
		}
		
		if ( pLDAPResult )
		{
			ldap_msgfree( pLDAPResult );
			pLDAPResult = NULL;
		}
				
		siResult = eDSNoErr;
	} catch( SInt32 iError ) {
		siResult = iError;
	} catch( ... ) {
		// catch all for miss throws...
		siResult = eUndefinedError;
	}
	
	DSCFRelease( cfSupportedSecLevel );
	DSCFRelease( cfConfigMap );
	DSFreeString( pServer );
	DSFreeString( pKerbSearchString );
	DSFreeString( pSearchString );
	DSFreeString( pSearchbase );
	DSFreeString( pAttribute );
	DSFreeString( pUsername );
	
	if ( pPassword != NULL )
	{
		free( pPassword );
		pPassword = NULL;
	}
	
	if ( pLD != NULL )
	{
		ldap_unbind_ext_s( pLD, NULL, NULL );
		pLD = NULL;
	}
	
	if ( cfXMLDict != NULL )
	{
		if ( siResult == eDSNoErr )
		{
			SInt32 iError = PutXMLInBuffer( cfXMLDict, inData->fOutRequestResponse );
			if ( iError != eDSNoErr ) {
				siResult = iError;
			}
		}
		
		CFRelease( cfXMLDict );
		cfXMLDict = NULL;
	}
	
	return siResult;
} // DoNewServerGetConfig

// ---------------------------------------------------------------------------
//	* DoNewServerBind
// ---------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::DoNewServerBind( sDoPlugInCustomCall *inData )
{
	SInt32					siResult				= eDSInvalidNativeMapping;
	CFMutableDictionaryRef  cfXMLDict				= NULL;
	char					*pUsername				= NULL;
	char					*pPassword				= NULL;
	CFDataRef				cfCurrentConfigData		= NULL;

	try
	{
		// we should always have XML data for this process, if we don't, throw an error
		cfXMLDict = GetXMLFromBuffer( inData->fInRequestData );
		if ( cfXMLDict == NULL ) {
			throw( (SInt32) eDSInvalidBuffFormat );
		}
		
		// automatically released, just a Get.
		CFStringRef cfServer = GetServerInfoFromConfig( cfXMLDict, NULL, NULL, NULL, &pUsername, &pPassword );
		if ( cfServer == NULL )
		{
			throw( (SInt32) eDSInvalidBuffFormat );
		}
		
		// we need credentials at this point, if we don't have them throw an error
		if ( pUsername == NULL || pPassword == NULL )
		{
			throw( (SInt32) eDSAuthParameterError );
		}
		
		// let's be sure the Auth flag is not set otherwise we will use the credentials instead of dsDoDirNodeAuth
		CFDictionarySetValue( cfXMLDict, CFSTR(kXMLSecureUseFlagKey), kCFBooleanFalse );
		
		// now let's use internal ways of doing things...  We'll add the new config to the config dictionary, and update it later...
		// this is so we can use all existing code to do the rest of the work...
		cfCurrentConfigData = fConfigFromXML->CopyLiveXMLConfig();
		
		CFMutableDictionaryRef cfConfigXML = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfCurrentConfigData, kCFPropertyListMutableContainersAndLeaves, NULL );
		if ( cfConfigXML == NULL )
			cfConfigXML = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		
		// let's grab the configured list of servers...
		CFMutableArrayRef  cfConfigList = (CFMutableArrayRef) CFDictionaryGetValue( cfConfigXML, CFSTR(kXMLConfigArrayKey) );
		if ( cfConfigList == NULL )
			cfConfigList = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		
		// let's see if our server is in our config, we could be binding with existing... if so, let's swap them out, otherwise add it to the end.
		CFIndex iIndex = 0;
		if ( IsServerInConfig(cfConfigXML, cfServer, &iIndex, NULL) )
		{
			// let's swap the new config for the old one..
			CFArraySetValueAtIndex( cfConfigList, iIndex, cfXMLDict );
		}
		else
		{
			// let's be sure we have an array list, in particular for a fresh configuration
			if ( cfConfigList == NULL )
			{
				cfConfigList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
				CFDictionarySetValue( cfConfigXML, CFSTR(kXMLConfigArrayKey), cfConfigList );
				CFRelease( cfConfigList ); // we can release since it was retained by the dictionary
			}
			// let's add this config to the configured servers..  if we complete, we'll
			CFArrayAppendValue( cfConfigList, cfXMLDict );
		}
		
		// now let's convert back to Data blob and save the new config...
		CFDataRef cfNewConfig = (CFDataRef) CFPropertyListCreateXMLData( kCFAllocatorDefault, cfConfigXML );
		
		// Technically the username already has a password, even though it is admin, no one else knows about this temporary node
		fConfigFromXML->NewXMLConfig( cfNewConfig );
		
		CFRelease( cfNewConfig );
		cfNewConfig = NULL;
		
		CFRelease( cfConfigXML );
		cfConfigXML = NULL;
		
		Initialize();

		// this is to re-use code for multiple bind scenarios
		siResult = DoNewServerBind2( inData );
		
	} catch( SInt32 iError ) {
		siResult = iError;
	} catch( ... ) {
		// catch all for miss throws...
		siResult = eUndefinedError;
	}
	
	if ( pUsername != NULL )
	{
		free( pUsername );
		pUsername = NULL;
	}
	
	if ( pPassword != NULL )
	{
		memset( pPassword, 0, strlen(pPassword) );
		free( pPassword );
		pPassword = NULL;
	}
	
	if ( cfXMLDict )
	{
		CFRelease( cfXMLDict );
		cfXMLDict = NULL;
	}
	
	// we need to revert the configuration...
	if ( cfCurrentConfigData != NULL )
	{
		fConfigFromXML->NewXMLConfig( cfCurrentConfigData );
		
		Initialize();

		CFRelease( cfCurrentConfigData );
		cfCurrentConfigData = NULL;
	}
	
	return siResult;
} // DoNewServerBind

// ---------------------------------------------------------------------------
//	* DoNewServerBind2
// ---------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::DoNewServerBind2( sDoPlugInCustomCall *inData )
{
	tDirStatus				siResult					= eDSInvalidNativeMapping;
	tDirStatus				siResultHost				= eDSRecordNotFound;
	tDirStatus				siResultLKDC				= eDSRecordNotFound;
	tDirStatus				siResultFromGetHost			= eDSNoErr;
	CFMutableDictionaryRef  cfXMLDict					= NULL;
	char					*pComputerID				= NULL;
	char					*localKDCRealmStr			= NULL;
	tDirReference			dsRef						= 0;
	tDirNodeReference		dsNodeRef					= 0;
	tRecordReference		recRef						= 0;
	tRecordReference		recRefHost					= 0;
	tRecordReference		recRefLKDC					= 0;
	tDataBufferPtr			responseDataBufPtr			= dsDataBufferAllocatePriv( 1024 );
	tDataBufferPtr			sendDataBufPtr				= dsDataBufferAllocatePriv( 1024 );
	bool					bOverwriteAllowed			= false;
	char					*kerbIDStr					= NULL;
	char					*pCompPassword				= NULL;
	char					pNodeName[255]				= { 0 };
	char					hostname[512]				= { 0 };
	char					hostnameList[512]			= { 0 };
	char					hostnameDollar[sizeof(hostname) + 1] = { 0 };
	char					localKDCRealmDollarStr[256]	= { 0 };
	DSAPIWrapper			dsWrapper;
	
	CLDAPBindData serverInfo( inData->fInRequestData, &cfXMLDict );
	if ( (siResult = serverInfo.DataValidForBind()) != eDSNoErr )
		return siResult;
	
	try
	{
		// we need credentials at this point, if we don't have them throw an error
		if ( serverInfo.UserName() == NULL || serverInfo.Password() == NULL )
			throw( eDSAuthParameterError );
		
		// let's be sure the Auth flag is not set otherwise we will use the credentials instead of dsDoDirNodeAuth
		CFDictionarySetValue( cfXMLDict, CFSTR(kXMLSecureUseFlagKey), kCFBooleanFalse );
		
		// let's grab the computer name because we'll need it..
		const char *pComputerIDNoDollar = serverInfo.ComputerName();
		if ( pComputerIDNoDollar != NULL )
		{
			size_t pComputerIDLen = strlen(pComputerIDNoDollar) + 2;
			pComputerID = (char *) calloc( sizeof(char), pComputerIDLen );
			if ( pComputerID == NULL )
				throw( eMemoryError );
			
			EnsureDollarSuffix( pComputerIDNoDollar, pComputerIDLen, pComputerID );
		}
		else
		{
			throw( eDSInvalidBuffFormat );
		}
		
		tDataListPtr nodeString = NodeNameWithHost( serverInfo.Server(), sizeof(pNodeName), pNodeName );
		if ( nodeString == NULL )
			throw( eDSInvalidBuffFormat );
		
		siResult = dsWrapper.OpenNodeByName( nodeString, serverInfo.UserName(), serverInfo.Password() );
		
		dsDataListDeallocatePriv( nodeString );
		free( nodeString );
		
		dsRef = dsWrapper.GetDSRef();
		dsNodeRef = dsWrapper.GetCurrentNodeRef();
		
		if ( siResult != eDSNoErr )
		{
			if ( dsNodeRef == 0 )
			{
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Unable to open Directory Node %s", pNodeName );
			}
			else
			{
				if ( siResult == eNotHandledByThisNode ) {
					siResult = eDSNoErr;
					throw( eDSNoErr );
				}
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Unable to authenticate as provided user - %s", serverInfo.UserName() );
			}
			
			throw( siResult );
		}
		
		// let's try to open the 3 Computer Records
		bOverwriteAllowed = (inData->fInRequestCode != eDSCustomCallLDAPv3NewServerBind &&
							 inData->fInRequestCode != eDSCustomCallLDAPv3NewServerBindOther);
		
		siResult = dsWrapper.OpenRecord( kDSStdRecordTypeComputers, pComputerID, &recRef, false );
		if ( siResult == eDSNoErr && !bOverwriteAllowed ) {
			DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Existing computer record %s", pComputerID );
			throw( eDSRecordAlreadyExists );
		}
		
		// To protect the Kerberos service principals associated with
		// the Computer record, the computer record's name must be
		// canonical.
		if ( GetHostFromAnywhere(hostname, sizeof(hostname)) != 0 )
			siResultFromGetHost = siResult = eDSUnknownHost;
		
		if ( siResultFromGetHost != eDSUnknownHost )
		{
			EnsureDollarSuffix( hostname, sizeof(hostnameDollar), hostnameDollar );
			siResultHost = dsWrapper.OpenRecord( kDSStdRecordTypeComputers, hostnameDollar, &recRefHost, false );
			if ( siResultHost == eDSNoErr && !bOverwriteAllowed ) {
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Existing computer record %s", hostnameDollar );
				throw( eDSRecordAlreadyExists );
			}
		}
		
		localKDCRealmStr = GetLocalKDCRealm();
		if ( localKDCRealmStr != NULL )
		{
			EnsureDollarSuffix( localKDCRealmStr, sizeof(localKDCRealmDollarStr), localKDCRealmDollarStr );
			siResultLKDC = dsWrapper.OpenRecord( kDSStdRecordTypeComputers, localKDCRealmDollarStr, &recRefLKDC, false );
			if ( siResultLKDC == eDSNoErr && !bOverwriteAllowed ) {
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Existing computer record %s", localKDCRealmDollarStr );
				throw( eDSRecordAlreadyExists );
			}
		}
		
		// before any computer records are created, make sure the existing ones
		// have the same owner.
		if ( !OwnerGUIDsMatch( dsRef, recRef, recRefHost, recRefLKDC ) )
		{
			DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Owner GUIDs in Computer records don't match" );
			throw( eDSPermissionError );
		}
		
		// Set the Password for the record - 20 characters long.. will be complex..
		pCompPassword = GenerateRandomComputerPassword();
		
		bool bNeedsMultipleComputerRecords = !OverlaySupportsUniqueNameEnforcement( serverInfo.Server(), serverInfo.SSL() );
		DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - LDAP Server supports single computer record: %s", 
				bNeedsMultipleComputerRecords ? "FALSE" : "TRUE" );
		if ( bNeedsMultipleComputerRecords )
		{
			siResult = DoNewServerBind2a( inData, dsWrapper, serverInfo, recRef, recRefHost, recRefLKDC, pComputerID,
							pCompPassword, hostnameDollar, localKDCRealmDollarStr );
		}
		else
		{
			siResult = DoNewServerBind2b( inData, dsWrapper, serverInfo, recRef, recRefHost, recRefLKDC, pComputerID,
							pCompPassword, hostnameDollar, localKDCRealmDollarStr );
		}
		
		if ( inData->fInRequestCode == eDSCustomCallLDAPv3NewServerBindOther ||
			 inData->fInRequestCode == eDSCustomCallLDAPv3NewServerForceBindOther )
		{
			if ( serverInfo.EnetAddress() != NULL )
			{
				// remove it from the dictionary cause this isn't normally in the config...
				CFDictionaryRemoveValue( cfXMLDict, CFSTR(kDS1AttrENetAddress) );
			}
		}
		
		// Kerberosv5 Authentication Authority
		if ( siResult == eDSNoErr )
		{
			// make password CFDataRef from the beginning
			CFDataRef   cfPassword = CFDataCreate( kCFAllocatorDefault, (UInt8*)pCompPassword, strlen(pCompPassword) );
			CFStringRef cfQualifier = NULL;
			
			CFStringRef cfMapBase = (CFStringRef) CFDictionaryGetValue( serverInfo.ComputerMap(), CFSTR(kXMLSearchBase) );
			
			// let's get the record name qualifier..
			GetAttribFromRecordDict( serverInfo.ComputerMap(), CFSTR(kDSNAttrRecordName), NULL, &cfQualifier );
			
			// let's compose the fully qualified DN for the computerAccount
			CFStringRef cfDN = NULL;
			CFStringRef computerID = CFStringCreateWithCString( kCFAllocatorDefault, pComputerID, kCFStringEncodingUTF8 );
			if ( computerID != NULL ) {
				cfDN = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%@=%@,%@"), cfQualifier, computerID, cfMapBase );
				CFRelease( computerID );
			}
			
			if ( cfDN != NULL )
			{
				// success.. let's store the password in the configuration now..
				CFDictionarySetValue( cfXMLDict, CFSTR(kXMLSecureUseFlagKey), kCFBooleanTrue );
				CFDictionarySetValue( cfXMLDict, CFSTR(kXMLServerAccountKey), cfDN );
				CFDictionarySetValue( cfXMLDict, CFSTR(kXMLServerPasswordKey), cfPassword );
				CFDictionarySetValue( cfXMLDict, CFSTR(kXMLBoundDirectoryKey), kCFBooleanTrue );
				
				SetComputerRecordKerberosAuthority( dsWrapper, pCompPassword, kDSStdRecordTypeComputers, pComputerID, cfXMLDict, &kerbIDStr );
				
				if ( bNeedsMultipleComputerRecords )
				{
					SetComputerRecordKerberosAuthority( dsWrapper, pCompPassword, kDSStdRecordTypeComputers, hostnameDollar, NULL, NULL );
					SetComputerRecordKerberosAuthority( dsWrapper, pCompPassword, kDSStdRecordTypeComputers, localKDCRealmDollarStr, NULL, NULL );
				}
				
				CFRelease( cfDN );
				cfDN = NULL;
			}
			
			CFRelease( cfPassword );
			cfPassword = NULL;
			
			// Create our service principals in our directory's KDC
			CLDAPPlugInPrefs prefsFile;
			DSPrefs prefs = {0};
			prefsFile.GetPrefs( &prefs );
			
			char *realm = kerbIDStr ? rindex(kerbIDStr, '@') : NULL;
			if ( siResultFromGetHost != eDSUnknownHost && realm != NULL )
			{
				realm++;
				
				VerifyKerberosForRealm( realm, realm );
				
				strlcpy( hostnameList, hostname, sizeof(hostnameList) );
				
				// Create Desktop KDC service principals if there is a KDC
				if ( localKDCRealmStr != NULL )
				{
					strlcat( hostnameList, ",", sizeof(hostnameList) );
					strlcat( hostnameList, localKDCRealmStr, sizeof(hostnameList) );
				}
				
				siResult = dsFillAuthBuffer(
								sendDataBufPtr, 5,
								strlen(hostnameDollar), hostnameDollar,
								strlen(pCompPassword), pCompPassword,
								strlen(prefs.services), prefs.services,
								strlen(hostnameList), hostnameList,
								strlen(realm), realm );
				
				responseDataBufPtr->fBufferLength = 0;
				
				siResult = dsWrapper.DoDirNodeAuthOnRecordType( kDSStdAuthSetComputerAcctPasswdAsRoot, true,
											sendDataBufPtr, responseDataBufPtr, 0, kDSStdRecordTypeComputers );
				if ( siResult == eDSNoErr )
				{
					AddServicePrincipalListToKeytab(
							prefs.services,
							hostname, realm, pCompPassword,
							kKerberosConfigureServices,
							serverInfo.UserName(), serverInfo.Password() );
					
					if ( localKDCRealmStr != NULL )
					{
						AddServicePrincipalListToKeytab(
								prefs.services,
								localKDCRealmStr, realm, pCompPassword,
								kKerberosDoNotConfigureServices,
								NULL, NULL );
						
						// xgrid can handle more than one service principal
						if ( strstr(prefs.services, "xgrid") != NULL )
						{
							CFMutableStringRef princString = CFStringCreateMutable( kCFAllocatorDefault, 0 );
							if ( princString != NULL ) {
								CFStringAppend( princString, CFSTR("xgrid/") );
								CFStringAppendCString( princString, localKDCRealmStr, kCFStringEncodingUTF8 );
								CFStringAppend( princString, CFSTR("@") );
								CFStringAppendCString( princString, realm, kCFStringEncodingUTF8 );
								
								AddXGridPrincipal( princString );
								CFRelease( princString );
							}
						}
					}
				}
				else
				{
					DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d attempting to create service principals for Computer account - %s", siResult, pComputerID );
				}
			}
			else
			{
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - could not create service principals due to missing data. Hostname = %s, realm = %s",
						hostname[0] ? hostname : "<empty>", realm ? realm : "<empty>" );
			}
			
			// now if we have a realm, let's spawn slapconfig
			struct stat sb;
			if ( realm != NULL && lstat("/usr/sbin/slapconfig", &sb) == 0 && sb.st_uid == 0 && (sb.st_mode & S_IFREG) == S_IFREG )
			{
				register pid_t childPID = -1;
				char *argv[] = { "/usr/sbin/slapconfig", "-enableproxyusers", realm, NULL };

				// we don't care about status
				if ( posix_spawn(&childPID, argv[0], NULL, NULL, argv, NULL) == 0 && childPID != -1 )
				{
					int nStatus;

					while ( waitpid(childPID, &nStatus, 0) == -1 && errno != ECHILD );
				}
			}
			
			siResult = eDSNoErr;
		}
		else
		{
			DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d attempting to set Password for Computer account - %s",
					siResult, pComputerID );
		}
	}
	catch( tDirStatus iError ) {
		siResult = iError;
	}
	catch( ... ) {
		// catch all for miss throws...
		siResult = eUndefinedError;
	}
	
	DSFreePassword( pCompPassword );
	
	if ( recRef != 0 )
	{
		dsCloseRecord( recRef );
		recRef = 0;
	}
	if ( recRefHost != 0 )
	{
		dsCloseRecord( recRefHost );
		recRefHost = 0;
	}
	if ( recRefLKDC != 0 )
	{
		dsCloseRecord( recRefLKDC );
		recRefLKDC = 0;
	}
	
	DSFreeString( kerbIDStr );
				
	if ( sendDataBufPtr != NULL )
	{
		dsDataBufferDeallocatePriv( sendDataBufPtr );
		sendDataBufPtr = NULL;
	}
	
	if ( responseDataBufPtr != NULL )
	{
		dsDataBufferDeallocatePriv( responseDataBufPtr );
		responseDataBufPtr = NULL;
	}
	
	DSFreeString( pComputerID );
	DSFreeString( localKDCRealmStr );
	
	if ( cfXMLDict != NULL )
	{
		if ( siResult == eDSNoErr )
		{
			tDirStatus iError = (tDirStatus)PutXMLInBuffer( cfXMLDict, inData->fOutRequestResponse );
			if ( iError != eDSNoErr ) {
				siResult = iError;
			}
		}
		
		CFRelease( cfXMLDict );
		cfXMLDict = NULL;
	}
	
	return (SInt32)siResult;
} // DoNewServerBind2


// ---------------------------------------------------------------------------
//	* DoNewServerBind2a
//
//	Returns: tDirStatus
//
//	Performs the original binding style with three computer records to
//	protect the Kerberos service principal namespace.
// ---------------------------------------------------------------------------

tDirStatus
CLDAPv3Plugin::DoNewServerBind2a(
	sDoPlugInCustomCall *inData,
	DSAPIWrapper &dsWrapper,
	CLDAPBindData &serverInfo,
	tRecordReference recRef,
	tRecordReference recRefHost,
	tRecordReference recRefLKDC,
	const char *pComputerID,
	const char *pCompPassword,
	const char *hostnameDollar,
	const char *localKDCRealmDollarStr )
{
	tDirStatus				siResult					= eDSInvalidNativeMapping;
	tDirReference			dsRef						= dsWrapper.GetDSRef();
	tDataBufferPtr			responseDataBufPtr			= dsDataBufferAllocatePriv( 1024 );
	tDataBufferPtr			sendDataBufPtr				= dsDataBufferAllocatePriv( 1024 );
	tDataNodePtr			pAttrName					= NULL;
	tAttributeEntryPtr		pAttrEntry					= NULL;
	tDataList				dataList					= { 0 };
	
	try
	{		
		if ( recRef == 0 )
		{
			// create the computer record
			DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to Create computer record and open - %s",
					pComputerID );
			
			siResult = dsWrapper.OpenRecord( kDSStdRecordTypeComputers, pComputerID, &recRef, true );
			if ( siResult != eDSNoErr )
			{
				DbgLog( kLogPlugin,
						"CLDAPv3Plugin: Bind Request - Error %d attempting to Create computer record and open - %s",
						siResult, pComputerID );
				throw( siResult );
			}
		}
		
		if ( !DSIsStringEmpty(hostnameDollar) )
		{
			if ( recRefHost == 0 && strcmp(pComputerID, hostnameDollar) != 0 )
			{
				// create the computer record
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to Create computer record and open - %s",
						hostnameDollar );
				
				siResult = dsWrapper.OpenRecord( kDSStdRecordTypeComputers, hostnameDollar, &recRefHost, true );
				if ( siResult != eDSNoErr )
				{
					DbgLog( kLogPlugin,
							"CLDAPv3Plugin: Bind Request - Error %d attempting to Create computer record and open - %s",
							siResult, hostnameDollar );
					throw( siResult );
				}
			}
			else if ( recRefHost != 0 && strcmp(pComputerID, hostnameDollar) == 0 )
			{
				dsCloseRecord( recRefHost );
				recRefHost = 0;
			}
		}
		
		if ( !DSIsStringEmpty(localKDCRealmDollarStr) && recRefLKDC == 0 &&
			 strcmp(pComputerID, localKDCRealmDollarStr) != 0 )
		{
			// create the computer record
			DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to Create computer record and open - %s",
					localKDCRealmDollarStr );
			
			siResult = dsWrapper.OpenRecord( kDSStdRecordTypeComputers, localKDCRealmDollarStr, &recRefLKDC, true );
			if ( siResult != eDSNoErr )
			{
				DbgLog( kLogPlugin,
						"CLDAPv3Plugin: Bind Request - Error %d attempting to Create computer record and open - %s",
						siResult, localKDCRealmDollarStr );
				throw( siResult );
			}
		}
		
		// put other record names in the pComputerID record so we can
		// clean up if the server is unbound
		pAttrName = dsDataNodeAllocateString( dsRef, kDS1AttrComment );
		
		DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to set a comment in %s with other record names", pComputerID );
		
		{
			char otherRecNames[1024] = {0,};
			if ( hostnameDollar[0] != '\0' )
				strlcpy( otherRecNames, hostnameDollar, sizeof(otherRecNames) );
			if ( localKDCRealmDollarStr[0] != '\0' )
			{
				if ( otherRecNames[0] != '\0' )
					strlcat( otherRecNames, ",", sizeof(otherRecNames) );
				strlcat( otherRecNames, localKDCRealmDollarStr, sizeof(otherRecNames) );
			}
			
			dsBuildListFromStringsAlloc( dsRef, &dataList, otherRecNames, NULL );
		
			siResult = dsSetAttributeValues( recRef, pAttrName, &dataList );
			
			dsDataListDeallocatePriv( &dataList );
			dsDataNodeDeAllocate( dsRef, pAttrName );
			pAttrName = NULL;
			
			if ( siResult != eDSNoErr && siResult != eDSNoStdMappingAvailable )
			{
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d attempting to set Comment value - %s", siResult, otherRecNames );
				throw( siResult );
			}
		}
		
		// Now let's set the OSVersion in the record... TBD
//		CFStringRef cfVersion = CFCopySystemVersionString();
//		if ( cfVersion != NULL )
//		{
//			UInt32 iLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfVersion), kCFStringEncodingUTF8) + 1;
//			char *pVersion = (char *) calloc( sizeof(char), iLength );
//			CFStringGetCString( cfVersion, pVersion, iLength, kCFStringEncodingUTF8 );

//			CFRelease( cfVersion );
//			cfVersion = NULL;
						
//			pAttrValue = dsDataNodeAllocateString( dsRef, pVersion );
//			pAttrName = dsDataNodeAllocateString( dsRef, TBD );
			
//			siResult = dsAddAttributeValue( recRef, pAttrName, pAttrValue );
			
//			free( pVersion );
//			pVersion = NULL;

//			dsDataNodeDeAllocate( dsRef, pAttrName );
//			pAttrName = NULL;
			
//			dsDataNodeDeAllocate( dsRef, pAttrValue );
//			pAttrValue = NULL;
			
//			if ( siResult != eDSNoErr && siResult != eDSNoStdMappingAvailable )
//			{
//				throw( siResult );
//			}			
//		}
		
		// now let's add a UUID to the record if there isn't one (Tiger)
		pAttrName = dsDataNodeAllocateString( dsRef, kDS1AttrGeneratedUID );
		siResult = dsGetRecordAttributeInfo( recRef, pAttrName, &pAttrEntry );
		if ( siResult != eDSNoErr || pAttrEntry->fAttributeValueCount == 0 )
		{
			char *pUUID = NewUUID();
			if ( pUUID )
			{
				dsBuildListFromStringsAlloc( dsRef, &dataList, pUUID, NULL );
				
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to set GeneratedUID value - %s", pUUID );
				
				siResult = dsSetAttributeValues( recRef, pAttrName, &dataList );
				
				dsDataListDeallocatePriv( &dataList );
				
				DSFreeString( pUUID );

				if ( siResult != eDSNoErr && siResult != eDSNoStdMappingAvailable )
				{
					DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d attempting to set GeneratedUID", siResult );
					throw( siResult );
				}
			}
		}
		
		dsDataNodeDeAllocate( dsRef, pAttrName );
		pAttrName = NULL;
		
		if ( pAttrEntry != NULL ) {
			dsDeallocAttributeEntry( dsRef, pAttrEntry );
			pAttrEntry = NULL;
		}
		
		char *pLinkAddr = NULL;
		
		if ( inData->fInRequestCode == eDSCustomCallLDAPv3NewServerBindOther ||
			 inData->fInRequestCode == eDSCustomCallLDAPv3NewServerForceBindOther )
		{
			if ( serverInfo.EnetAddress() != NULL )
			{
				pLinkAddr = strdup( serverInfo.EnetAddress() );
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Received MAC Address for bind other request" );
			}
			else
			{
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Did not receive MAC Address for bind other request" );
			}
		}
		else
		{
			CFStringRef cfEnetAddr = NULL;
			GetMACAddress( &cfEnetAddr, NULL, true );
			if ( cfEnetAddr != NULL )
			{
				UInt32 iLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfEnetAddr), kCFStringEncodingUTF8 ) + 1;
				pLinkAddr = (char *) calloc( sizeof(char), iLength );
				if ( pLinkAddr != NULL )
					CFStringGetCString( cfEnetAddr, pLinkAddr, iLength, kCFStringEncodingUTF8 );
				DSCFRelease( cfEnetAddr );
			}
			
			DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Determined MAC Address from local host information" );
		}
		
		// Set the macAddress - CFSTR(kDS1AttrENetAddress) -- needs to be en0 - for Managed Client Settings
		if ( pLinkAddr != NULL )
		{
			pAttrName = dsDataNodeAllocateString( dsRef, kDS1AttrENetAddress );

			dsBuildListFromStringsAlloc( dsRef, &dataList, pLinkAddr, NULL );
			
			// remove potential ENetAddr conflicts
			tDirStatus remove_result;
			if ( recRefHost != 0 )
			{
				remove_result = dsRemoveAttribute( recRefHost, pAttrName );
				if ( remove_result != eDSNoErr )
					DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d while removing ENetAddr from FQDN record", remove_result );
			}
			if ( recRefLKDC != 0 )
			{
				remove_result = dsRemoveAttribute( recRefLKDC, pAttrName );
				if ( remove_result != eDSNoErr )
					DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d while removing ENetAddr from LKDC record", remove_result );
			}
			
			DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to set MAC Address - %s", pLinkAddr );
			
			siResult = dsSetAttributeValues( recRef, pAttrName, &dataList );			
			dsDataNodeDeAllocate( dsRef, pAttrName );
			pAttrName = NULL;
			
			dsDataListDeallocatePriv( &dataList );

			DSFreeString( pLinkAddr );
			
			if ( siResult != eDSNoErr && siResult != eDSNoStdMappingAvailable )
			{
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d attempting to set MAC Address", siResult );
				throw( siResult );
			}
		}
		
		siResult = dsFillAuthBuffer(
						sendDataBufPtr, 2,
						strlen(pComputerID), pComputerID,
						strlen(pCompPassword), pCompPassword );
		
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to set Password for Computer account - %s", pComputerID );
		siResult = dsWrapper.DoDirNodeAuthOnRecordType( kDSStdAuthSetPasswdAsRoot, true, sendDataBufPtr, responseDataBufPtr, 0,
						kDSStdRecordTypeComputers );	
		if ( siResult == eDSNoErr )
		{
			// Set again for the Host record
			siResult = dsFillAuthBuffer(
							sendDataBufPtr, 2,
							strlen(hostnameDollar), hostnameDollar,
							strlen(pCompPassword), pCompPassword );
			
			if ( siResult != eDSNoErr )
				throw( siResult );
			
			DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to set Password for Computer account - %s", hostnameDollar );
			siResult = dsWrapper.DoDirNodeAuthOnRecordType( kDSStdAuthSetPasswdAsRoot, true, sendDataBufPtr, responseDataBufPtr, 0,
						kDSStdRecordTypeComputers );
			
			// Set again for the LKDC record
			if ( !DSIsStringEmpty(localKDCRealmDollarStr) )
			{
				siResult = dsFillAuthBuffer(
								sendDataBufPtr, 2,
								strlen(localKDCRealmDollarStr), localKDCRealmDollarStr,
								strlen(pCompPassword), pCompPassword );
				
				if ( siResult == eDSNoErr )
				{
					DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to set Password for Computer account - %s", localKDCRealmDollarStr );
					siResult = dsWrapper.DoDirNodeAuthOnRecordType( kDSStdAuthSetPasswdAsRoot, true, sendDataBufPtr, responseDataBufPtr, 0,
						kDSStdRecordTypeComputers );
				}
			}
		}
	}
	catch( tDirStatus iError ) {
		siResult = iError;
	}
	catch( ... ) {
		// catch all for miss throws...
		siResult = eUndefinedError;
	}
	
	if ( pAttrName != NULL )
	{
		dsDataNodeDeAllocate( dsRef, pAttrName );
		pAttrName = NULL;
	}
	
	if ( sendDataBufPtr != NULL )
	{
		dsDataBufferDeallocatePriv( sendDataBufPtr );
		sendDataBufPtr = NULL;
	}
	
	if ( responseDataBufPtr != NULL )
	{
		dsDataBufferDeallocatePriv( responseDataBufPtr );
		responseDataBufPtr = NULL;
	}
	
	return siResult;
} // DoNewServerBind2a


// ---------------------------------------------------------------------------
//	* DoNewServerBind2b
//
//	Returns: tDirStatus
//
//	Performs the updated binding style with three record names in a single
//	record to protect the Kerberos service principal namespace.
// ---------------------------------------------------------------------------

tDirStatus
CLDAPv3Plugin::DoNewServerBind2b(
	sDoPlugInCustomCall *inData,
	DSAPIWrapper &dsWrapper,
	CLDAPBindData &serverInfo,
	tRecordReference recRef,
	tRecordReference recRefHost,
	tRecordReference recRefLKDC,
	const char *pComputerID,
	const char *pCompPassword,
	const char *hostnameDollar,
	const char *localKDCRealmDollarStr )
{
	tDirStatus				siResult					= eDSInvalidNativeMapping;
	tDirReference			dsRef						= 0;
	tDataBufferPtr			responseDataBufPtr			= dsDataBufferAllocatePriv( 1024 );
	tDataBufferPtr			sendDataBufPtr				= dsDataBufferAllocatePriv( 1024 );
	tDataNodePtr			pAttrName					= NULL;
	tAttributeEntryPtr		pAttrEntry					= NULL;
	tDataList				dataList					= { 0 };

	try
	{		
		if ( recRef == 0 )
		{
			// create the computer record
			DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to Create computer record and open - %s", pComputerID );
			
			siResult = dsWrapper.OpenRecord( kDSStdRecordTypeComputers, pComputerID, &recRef, true );
			if ( siResult != eDSNoErr )
			{
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d attempting to Create computer record and open - %s",
						siResult, pComputerID );
				throw( siResult );
			}
		}
		
		if ( !DSIsStringEmpty(hostnameDollar) )
		{
			if ( strcmp(pComputerID, hostnameDollar) != 0 )
			{
				// add hostnameDollar as a short-name to the computer record
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to add secondary name - %s",
						hostnameDollar );
				
				siResult = dsWrapper.AddShortName( recRef, hostnameDollar );
				if ( siResult == eDSNoErr )
				{
					if ( recRefHost != 0 )
					{
						tDirStatus deleteResult = dsDeleteRecord( recRefHost );
						if ( deleteResult == eDSNoErr )
						{
							recRefHost = 0;
						}
						else
						{
							DbgLog( kLogPlugin,
								"CLDAPv3Plugin: Bind Request - Warning %d attempting to remove secondary computer record - %s",
								deleteResult, hostnameDollar );
						}
					}
				}
				else
				{
					DbgLog( kLogPlugin,
							"CLDAPv3Plugin: Bind Request - Error %d attempting to add secondary name - %s",
							siResult, hostnameDollar );
					throw( siResult );
				}
			}
			else if ( recRefHost != 0 && strcmp(pComputerID, hostnameDollar) == 0 )
			{
				dsCloseRecord( recRefHost );
				recRefHost = 0;
			}
		}
		
		if ( !DSIsStringEmpty(localKDCRealmDollarStr) &&
			 strcmp(pComputerID, localKDCRealmDollarStr) != 0 )
		{
			// create the computer record
			DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to add secondary name - %s", localKDCRealmDollarStr );
			
			siResult = dsWrapper.AddShortName( recRef, localKDCRealmDollarStr );
			if ( siResult == eDSNoErr )
			{
				if ( recRefLKDC != 0 )
				{
					tDirStatus deleteResult = dsDeleteRecord( recRefLKDC );
					if ( deleteResult == eDSNoErr )
					{
						recRefLKDC = 0;
					}
					else
					{
						DbgLog( kLogPlugin,
							"CLDAPv3Plugin: Bind Request - Warning %d attempting to remove secondary computer record - %s",
							deleteResult, localKDCRealmDollarStr );
					}
				}
			}
			else
			{
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d attempting to add secondary name - %s",
						siResult, localKDCRealmDollarStr );
				throw( siResult );
			}
		}
		
		// put other record names in the pComputerID record so we can
		// clean up if the server is unbound
		pAttrName = dsDataNodeAllocateString( dsRef, kDS1AttrComment );
		
		DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to set a comment in %s with other record names", pComputerID );
		
		{
			char otherRecNames[1024] = {0,};
			if ( hostnameDollar[0] != '\0' )
				strlcpy( otherRecNames, hostnameDollar, sizeof(otherRecNames) );
			if ( localKDCRealmDollarStr[0] != '\0' )
			{
				if ( otherRecNames[0] != '\0' )
					strlcat( otherRecNames, ",", sizeof(otherRecNames) );
				strlcat( otherRecNames, localKDCRealmDollarStr, sizeof(otherRecNames) );
			}
			
			dsBuildListFromStringsAlloc( dsRef, &dataList, otherRecNames, NULL );
		
			siResult = dsSetAttributeValues( recRef, pAttrName, &dataList );
			
			dsDataListDeallocatePriv( &dataList );
			dsDataNodeDeAllocate( dsRef, pAttrName );
			pAttrName = NULL;
			
			if ( siResult != eDSNoErr && siResult != eDSNoStdMappingAvailable )
			{
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d attempting to set Comment value - %s", siResult, otherRecNames );
				throw( siResult );
			}
		}
		
		// Now let's set the OSVersion in the record... TBD
//		CFStringRef cfVersion = CFCopySystemVersionString();
//		if ( cfVersion != NULL )
//		{
//			UInt32 iLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfVersion), kCFStringEncodingUTF8) + 1;
//			char *pVersion = (char *) calloc( sizeof(char), iLength );
//			CFStringGetCString( cfVersion, pVersion, iLength, kCFStringEncodingUTF8 );

//			CFRelease( cfVersion );
//			cfVersion = NULL;
						
//			pAttrValue = dsDataNodeAllocateString( dsRef, pVersion );
//			pAttrName = dsDataNodeAllocateString( dsRef, TBD );
			
//			siResult = dsAddAttributeValue( recRef, pAttrName, pAttrValue );
			
//			free( pVersion );
//			pVersion = NULL;

//			dsDataNodeDeAllocate( dsRef, pAttrName );
//			pAttrName = NULL;
			
//			dsDataNodeDeAllocate( dsRef, pAttrValue );
//			pAttrValue = NULL;
			
//			if ( siResult != eDSNoErr && siResult != eDSNoStdMappingAvailable )
//			{
//				throw( siResult );
//			}			
//		}
		
		// now let's add a UUID to the record if there isn't one (Tiger)
		pAttrName = dsDataNodeAllocateString( dsRef, kDS1AttrGeneratedUID );
		siResult = dsGetRecordAttributeInfo( recRef, pAttrName, &pAttrEntry );
		if ( siResult != eDSNoErr || pAttrEntry->fAttributeValueCount == 0 )
		{
			char *pUUID = NewUUID();
			if ( pUUID )
			{
				dsBuildListFromStringsAlloc( dsRef, &dataList, pUUID, NULL );
				
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to set GeneratedUID value - %s", pUUID );
				
				siResult = dsSetAttributeValues( recRef, pAttrName, &dataList );
				
				dsDataListDeallocatePriv( &dataList );
				
				DSFreeString( pUUID );

				if ( siResult != eDSNoErr && siResult != eDSNoStdMappingAvailable )
				{
					DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d attempting to set GeneratedUID", siResult );
					throw( siResult );
				}
			}
		}
		
		dsDataNodeDeAllocate( dsRef, pAttrName );
		pAttrName = NULL;
		
		if ( pAttrEntry != NULL ) {
			dsDeallocAttributeEntry( dsRef, pAttrEntry );
			pAttrEntry = NULL;
		}
		
		char *pLinkAddr = NULL;
		
		if ( inData->fInRequestCode == eDSCustomCallLDAPv3NewServerBindOther ||
			 inData->fInRequestCode == eDSCustomCallLDAPv3NewServerForceBindOther )
		{
			if ( serverInfo.EnetAddress() != NULL )
			{
				pLinkAddr = strdup( serverInfo.EnetAddress() );
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Received MAC Address for bind other request" );
			}
			else
			{
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Did not receive MAC Address for bind other request" );
			}
		}
		else
		{
			CFStringRef cfEnetAddr = NULL;
			GetMACAddress( &cfEnetAddr, NULL, true );
			if ( cfEnetAddr != NULL )
			{
				UInt32 iLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfEnetAddr), kCFStringEncodingUTF8 ) + 1;
				pLinkAddr = (char *) calloc( sizeof(char), iLength );
				if ( pLinkAddr != NULL )
					CFStringGetCString( cfEnetAddr, pLinkAddr, iLength, kCFStringEncodingUTF8 );
				DSCFRelease( cfEnetAddr );
			}
			
			DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Determined MAC Address from local host information" );
		}
		
		// Set the macAddress - CFSTR(kDS1AttrENetAddress) -- needs to be en0 - for Managed Client Settings
		if ( pLinkAddr != NULL )
		{
			pAttrName = dsDataNodeAllocateString( dsRef, kDS1AttrENetAddress );

			dsBuildListFromStringsAlloc( dsRef, &dataList, pLinkAddr, NULL );
			
			// remove potential ENetAddr conflicts
			tDirStatus remove_result;
			if ( recRefHost != 0 )
			{
				remove_result = dsRemoveAttribute( recRefHost, pAttrName );
				if ( remove_result != eDSNoErr )
					DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d while removing ENetAddr from FQDN record", remove_result );
			}
			if ( recRefLKDC != 0 )
			{
				remove_result = dsRemoveAttribute( recRefLKDC, pAttrName );
				if ( remove_result != eDSNoErr )
					DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d while removing ENetAddr from LKDC record", remove_result );
			}
			
			DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to set MAC Address - %s", pLinkAddr );
			
			siResult = dsSetAttributeValues( recRef, pAttrName, &dataList );			
			dsDataNodeDeAllocate( dsRef, pAttrName );
			pAttrName = NULL;
			
			dsDataListDeallocatePriv( &dataList );

			DSFreeString( pLinkAddr );
						
			if ( siResult != eDSNoErr && siResult != eDSNoStdMappingAvailable )
			{
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d attempting to set MAC Address", siResult );
				throw( siResult );
			}
		}
		
		siResult = dsFillAuthBuffer(
						sendDataBufPtr, 2,
						strlen(pComputerID), pComputerID,
						strlen(pCompPassword), pCompPassword );
		
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		DbgLog( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to set Password for Computer account - %s", pComputerID );
		siResult = dsWrapper.DoDirNodeAuthOnRecordType( kDSStdAuthSetPasswdAsRoot, true, sendDataBufPtr, responseDataBufPtr, 0,
						kDSStdRecordTypeComputers );	
	}
	catch( tDirStatus iError ) {
		siResult = iError;
	}
	catch( ... ) {
		// catch all for miss throws...
		siResult = eUndefinedError;
	}
	
	if ( pAttrName != NULL )
	{
		dsDataNodeDeAllocate( dsRef, pAttrName );
		pAttrName = NULL;
	}
	
	if ( sendDataBufPtr != NULL )
	{
		dsDataBufferDeallocatePriv( sendDataBufPtr );
		sendDataBufPtr = NULL;
	}
	
	if ( responseDataBufPtr != NULL )
	{
		dsDataBufferDeallocatePriv( responseDataBufPtr );
		responseDataBufPtr = NULL;
	}
		
	return siResult;
} // DoNewServerBind2b


// ---------------------------------------------------------------------------
//	* DoNewServerSetup
// ---------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::DoNewServerSetup( sDoPlugInCustomCall *inData )
{
	SInt32					siResult			= eDSInvalidBuffFormat;
	CFMutableDictionaryRef  cfXMLDict			= NULL;
	CFMutableDictionaryRef  cfConfigXML			= NULL;
	CFDataRef				cfTempData			= NULL;
	CFIndex					iIndex				= 0;
	CFMutableArrayRef		cfConfigList		= NULL;
	
	// we should always have XML data for this process, if we don't, throw an error
	cfXMLDict = GetXMLFromBuffer( inData->fInRequestData );
	if ( cfXMLDict != NULL )
	{
		// We'll add the new config to the config dictionary
		cfTempData = fConfigFromXML->CopyLiveXMLConfig();
		if ( cfTempData != NULL )
		{
			cfConfigXML = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfTempData, kCFPropertyListMutableContainersAndLeaves, NULL );
			
			CFRelease( cfTempData );
			cfTempData = NULL;
			
			// let's grab the configured list of servers...
			cfConfigList = (CFMutableArrayRef) CFDictionaryGetValue( cfConfigXML, CFSTR(kXMLConfigArrayKey) );
			if ( cfConfigList != NULL )
				CFRetain( cfConfigList );
		}
		
		// ensure we have these
		if ( cfConfigXML == NULL )
			cfConfigXML = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		if ( cfConfigList == NULL ) {
			cfConfigList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
			if ( cfConfigList != NULL ) {
				CFDictionaryAddValue( cfConfigXML, CFSTR(kXMLConfigArrayKey), cfConfigList );
				// keep a retained copy of cfConfigList
			}
		}
		
		CFStringRef cfServer = GetServerInfoFromConfig( cfXMLDict, NULL, NULL, NULL, NULL, NULL );
		if ( IsServerInConfig(cfConfigXML, cfServer, &iIndex, NULL) )
		{
			// let's swap the new config for the old one..
			CFArraySetValueAtIndex( cfConfigList, iIndex, cfXMLDict );
		}
		else
		{
			// let's be sure we have an array list, in particular for a fresh configuration
			CFDictionarySetValue( cfConfigXML, CFSTR(kXMLConfigArrayKey), cfConfigList );
			
			// let's add this config to the configured servers..  if we complete, we'll
			CFArrayAppendValue( cfConfigList, cfXMLDict );
		}
		
		// we're done with cfConfigList now..
		CFRelease( cfConfigList );
		cfConfigList = NULL;
		
		// first let's convert the password to a CFData type so it isn't in cleartext
		CFStringRef cfPassword = (CFStringRef) CFDictionaryGetValue( cfXMLDict, CFSTR(kXMLServerPasswordKey) );
		
		// if the password is a string convert it to a Data type
		if ( cfPassword && CFGetTypeID(cfPassword) == CFStringGetTypeID() )
		{
			CFDataRef cfPassData = CFStringCreateExternalRepresentation( kCFAllocatorDefault, cfPassword, kCFStringEncodingUTF8, 0 );
			if ( cfPassData != NULL ) {
				CFDictionarySetValue( cfXMLDict, CFSTR(kXMLServerPasswordKey), cfPassData );
				CFRelease( cfPassData );
			}
		}
		
		// now let's convert back to Data blob and save the new config...
		cfTempData = (CFDataRef) CFPropertyListCreateXMLData( kCFAllocatorDefault, cfConfigXML );
		if ( cfTempData != NULL )
		{			
			// Technically the username already has a password, even though it is admin, no one else knows about this temporary node
			siResult = fConfigFromXML->NewXMLConfig( cfTempData );

			// we're done with cfTempData
			CFRelease( cfTempData );
			cfTempData = NULL;
		}
		
		// we're done with cfConfigXML now..
		CFRelease( cfConfigXML );
		cfConfigXML = NULL;
		
		// let's re-initialize ourselves to add the new config
		Initialize();
	}
	
	// let's return it because we may have updated it
	if ( cfXMLDict != NULL )
	{
		if ( siResult == eDSNoErr )
		{
			SInt32 iError = PutXMLInBuffer( cfXMLDict, inData->fOutRequestResponse );
			if ( iError != eDSNoErr ) {
				siResult = iError;
			}
		}
		
		CFRelease( cfXMLDict );
		cfXMLDict = NULL;
	}
		
	return siResult;
} // DoNewServerSetup

// ---------------------------------------------------------------------------
//	* DoRemoveServer
// ---------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::DoRemoveServer( sDoPlugInCustomCall *inData )
{
	tDirStatus				siResult				= eDSNoErr;
	CFMutableDictionaryRef	cfXMLDict				= NULL; // this is the inbound XML dictionary
	CFDictionaryRef			cfConfigDict			= NULL; // this is our current configuration XML data
	CFMutableDictionaryRef  cfServerDict			= NULL; // this is the actual server we're trying to unconfigure
	CFIndex					iConfigIndex			= 0;
	char					*pComputerID			= NULL;
	tDirReference			dsRef					= 0;
	tDirNodeReference		dsNodeRef				= 0;
	tDataNodePtr			pRecType				= dsDataNodeAllocateString( dsRef, kDSStdRecordTypeComputers );
	tDataBufferPtr			responseDataBufPtr		= dsDataBufferAllocatePriv( 1024 );
	tDataBufferPtr			sendDataBufPtr			= dsDataBufferAllocatePriv( 1024 );
	tDataNodePtr			pAuthType				= NULL;
	tDataNodePtr			pRecName				= NULL;
	tDataNodePtr			pRecName2				= NULL;
	CFMutableArrayRef		cfConfigList			= NULL; // no need to release, temporary variable
	DSAPIWrapper			dsWrapper;
	
	CLDAPBindData serverInfo( inData->fInRequestData, &cfXMLDict );
	if ( (siResult = serverInfo.DataValidForRemove()) != eDSNoErr )
	{
		DbgLog( kLogPlugin, "CLDAPv3Plugin::DoRemoveServer: Error reading server information = %d", siResult );
		return siResult;
	}
	
	try
	{
		// now let's find the server we're trying to unbind from in the config
		CFDataRef cfTempData = fConfigFromXML->CopyLiveXMLConfig();
		
		cfConfigDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfTempData, kCFPropertyListMutableContainersAndLeaves, NULL );

		cfConfigList = (CFMutableArrayRef) CFDictionaryGetValue( cfConfigDict, CFSTR(kXMLConfigArrayKey) );

		// we're done with the TempData at this point
		CFRelease( cfTempData );
		cfTempData = NULL;
		
		if ( IsServerInConfig( cfConfigDict, serverInfo.ServerCFString(), &iConfigIndex, &cfServerDict ) == false )
		{
			DbgLog( kLogPlugin, "CLDAPv3Plugin::DoRemoveServer: the server is not in the configuration." );
			throw( eDSBogusServer );
		}
		
		CFBooleanRef	cfBound = (CFBooleanRef) CFDictionaryGetValue( cfServerDict, CFSTR(kXMLBoundDirectoryKey) );
		bool			bBound = false;
		
		if ( cfBound != NULL && CFGetTypeID(cfBound) == CFBooleanGetTypeID() )
		{
			bBound = CFBooleanGetValue( cfBound );
		}
		
		// if we are bound, we need to unbind first, then we can delete the config and re-initialize
		if ( bBound )
		{
			// well, if we don't have credentials, we won't be able to continue here..
			if ( serverInfo.UserName() == NULL || serverInfo.Password() == NULL )
			{
				DbgLog( kLogPlugin, "CLDAPv3Plugin::DoRemoveServer: No credentials for username = %s.",
					serverInfo.UserName() ? serverInfo.UserName() : "(null)" );
				throw( eDSAuthParameterError );
			}
			
			char *pComputerDN = NULL;
			GetServerInfoFromConfig( cfServerDict, NULL, NULL, NULL, &pComputerDN, NULL );
			
			// shouldn't happen but a safety
			if ( pComputerDN == NULL )
			{
				DbgLog( kLogPlugin, "CLDAPv3Plugin::DoRemoveServer: unable to retrieve the computer name." );
				throw( eDSBogusServer );
			}
			
			char **pDN = ldap_explode_dn( pComputerDN, 1 );
			
			if ( pDN != NULL )
			{
				pComputerID = strdup( pDN[0] );
				ldap_value_free( pDN );
				pDN = NULL;
			}
			
			free( pComputerDN );
			pComputerDN = NULL;
			
			// let's build our path....
			tDataListPtr nodeString = NodeNameWithHost( serverInfo.Server(), 0, NULL );
			if ( nodeString == NULL )
				throw( eMemoryError );
			
			siResult = dsWrapper.OpenNodeByName( nodeString, serverInfo.UserName(), serverInfo.Password() );
			dsDataListDeallocatePriv( nodeString );
			free( nodeString );
			nodeString = NULL;
			
			dsRef = dsWrapper.GetDSRef();
			dsNodeRef = dsWrapper.GetCurrentNodeRef();
			
			if ( siResult != eDSNoErr )
			{
				DbgLog( kLogPlugin, "CLDAPv3Plugin::DoRemoveServer: Error opening node = %d", serverInfo.Server(), siResult );
				throw( siResult );
			}
			
			// let's try to open the Record
			tRecordReference recRef = 0;
			siResult = dsWrapper.OpenRecord( kDSStdRecordTypeComputers, pComputerID, &recRef, false );
			
			// if we didn't get an error, then we were able to open the record
			if ( siResult == eDSNoErr )
			{
				// check for any other Computer record names that need to be
				// deleted along with this one.
				tDataNodePtr pAttrType = dsDataNodeAllocateString( dsRef, kDS1AttrComment );
				tAttributeValueEntryPtr valueEntryPtr = NULL;
				tDirStatus siResult2 = eDSNoErr;
				tRecordReference recRef2 = 0;
				char *otherRecNamesHeadPtr = NULL;
				char *otherRecNames = NULL;
				char *recName = NULL;
				
				siResult2 = dsGetRecordAttributeValueByIndex( recRef, pAttrType, 1, &valueEntryPtr );
				if ( siResult2 == eDSNoErr && valueEntryPtr != NULL )
				{
					otherRecNamesHeadPtr = otherRecNames = dsCStrFromCharacters(
										valueEntryPtr->fAttributeValueData.fBufferData,
										valueEntryPtr->fAttributeValueData.fBufferLength );
					
					while ( (recName = strsep(&otherRecNames, ",")) != NULL )
					{
						siResult2 = dsWrapper.OpenRecord( kDSStdRecordTypeComputers, recName, &recRef2, false );
						if ( siResult2 == eDSNoErr )
						{
							sendDataBufPtr->fBufferLength = sizeof( recRef2 );
							bcopy( &recRef2, sendDataBufPtr->fBufferData, sizeof(recRef2) );
							
							// we use a custom call instead of doing a normal delete to ensure
							// any PWS information is deleted as well
							DbgLog( kLogPlugin, "CLDAPv3Plugin::DoRemoveServer: deleting computer record - %s", recName );
							dsDoPlugInCustomCall( dsNodeRef, eDSCustomCallDeleteRecordAndCredentials, sendDataBufPtr,
													responseDataBufPtr );
						}
						if ( pRecName2 != NULL ) {
							dsDataNodeDeAllocate( dsRef, pRecName2 );
							pRecName2 = NULL;
						}
					}
					
					DSFreeString( otherRecNamesHeadPtr );
					
					dsDeallocAttributeValueEntry( dsRef, valueEntryPtr );
					valueEntryPtr = NULL;
				}
				
				dsDataNodeDeAllocate( dsRef, pAttrType );
				pAttrType = NULL;
				
				// let's delete the existing cause we are doing Join/replace
				sendDataBufPtr->fBufferLength = sizeof( recRef );
				bcopy( &recRef, sendDataBufPtr->fBufferData, sizeof(recRef) );

				// we use a custom call instead of doing a normal delete to ensure any PWS information is deleted as well
				DbgLog( kLogPlugin, "CLDAPv3Plugin::DoRemoveServer: deleting computer record - %s", pComputerID );
				dsDoPlugInCustomCall( dsNodeRef, eDSCustomCallDeleteRecordAndCredentials, sendDataBufPtr, responseDataBufPtr );
			}
		}
		else
		{
			siResult = eDSNoErr;
		}
		
	} catch( tDirStatus iError ) {
		siResult = iError;
	} catch( ... ) {
		// catch all for miss throws...
		siResult = eUndefinedError;
	}
	
	// if this is force unbind, we don't care, just set to eDSNoErr
	if ( inData->fInRequestCode == eDSCustomCallLDAPv3ForceUnbindServerConfig )
		siResult = eDSNoErr;
	else if ( siResult != eDSBogusServer && siResult != eDSAuthParameterError && siResult != eDSNoErr )
		siResult = eDSOpenNodeFailed; // only the above errors and eDSOpenNodeFailed are allowed back from this routine
	
	// if we are removing the configuration, just remove it here and update our configuration.
	if ( siResult == eDSNoErr )
	{
		// remove the kerberos principal from the keytab
		CFStringRef kerberosID = (CFStringRef) CFDictionaryGetValue( cfServerDict, CFSTR(kXMLKerberosId) );
		char *pKerberosID = NULL;
		
		if (kerberosID != NULL && CFStringGetLength(kerberosID) > 0)
		{
			UInt32 callocLength = (UInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(kerberosID), kCFStringEncodingUTF8) + 1;
			pKerberosID = (char *) calloc( 1, callocLength );
			CFStringGetCString( kerberosID, pKerberosID, callocLength, kCFStringEncodingUTF8 );
		}
		
		if ( pKerberosID != NULL )
		{
			krb5_context	krbContext  = NULL;
			krb5_principal	principal	= NULL;
			krb5_ccache		krbCache	= NULL;
			char			*principalString = NULL;
			char			*pCacheName	= NULL;
			
			RemoveKeytabEntry( pKerberosID );
			
			gKerberosMutex->WaitLock();

			int retval = krb5_init_context( &krbContext );
			if ( retval == 0 )
				retval = krb5_parse_name( krbContext, pKerberosID, &principal );

			if ( retval == 0 )
				retval = krb5_unparse_name( krbContext, principal, &principalString );
			
			if ( retval == 0 )
			{
				pCacheName = (char *) malloc( strlen(principalString) + sizeof("MEMORY:") + 1 );
				strcpy( pCacheName, "MEMORY:" );
				strcat( pCacheName, principalString );

				retval = krb5_cc_resolve( krbContext, pCacheName, &krbCache );
			}

			if ( NULL != principal )
			{
				krb5_free_principal( krbContext, principal );
				principal = NULL;
			}
			
			// now let's destroy any cache that may exist, clearing memory
			if ( NULL != krbCache )
			{
				krb5_cc_destroy( krbContext, krbCache );
				krbCache = NULL;
				DbgLog( kLogPlugin, "CLDAPv3Plugin: Destroying kerberos Memory cache for %s", principalString );
			}
			
			if ( NULL != krbContext )
			{
				krb5_free_context( krbContext );
				krbContext = NULL;
			}
			
			gKerberosMutex->SignalLock();
			
			// let's spawn slapconfig to disable proxy users if necessary
			struct stat sb;
			char *realm = rindex( pKerberosID, '@' );
			if ( realm != NULL && lstat("/usr/sbin/slapconfig", &sb) == 0 && sb.st_uid == 0 && (sb.st_mode & S_IFREG) == S_IFREG )
			{
				pid_t childPID;
				int nStatus;
				char *argv[] = { "/usr/sbin/slapconfig", "-disableproxyusers", realm, NULL };
				
				// we don't care about status, but we need to reap the child
				if ( posix_spawn( &childPID, argv[0], NULL, NULL, argv, NULL ) == 0 )
					while ( waitpid(childPID, &nStatus, 0) == -1 && errno != ECHILD );
			}

			DSFreeString( pCacheName );
			DSFreeString( principalString );
		}
		
		DSFreeString( pKerberosID );
		
		// here let's reset all keys if they are set since we just unbound
		CFDictionarySetValue( cfServerDict, CFSTR(kXMLBoundDirectoryKey), kCFBooleanFalse );
		CFDictionarySetValue( cfServerDict, CFSTR(kXMLSecureUseFlagKey), kCFBooleanFalse );
		CFDictionarySetValue( cfServerDict, CFSTR(kXMLServerAccountKey), CFSTR("") );
		CFDictionarySetValue( cfServerDict, CFSTR(kXMLServerPasswordKey), CFSTR("") );
		CFDictionarySetValue( cfServerDict, CFSTR(kXMLKerberosId), CFSTR("") );
		
		// if we are removing, let's remove the index
		if ( inData->fInRequestCode == eDSCustomCallLDAPv3RemoveServerConfig )
		{
			// remove the value at the index we saved.. then stuff it back and reset the configuration.
			CFArrayRemoveValueAtIndex( cfConfigList, iConfigIndex );
		}
		
		CFDataRef cfTempData = (CFDataRef) CFPropertyListCreateXMLData( kCFAllocatorDefault, cfConfigDict );
		
		// Technically the username already has a password, even though it is admin, no one else knows about this temporary node
		siResult = (tDirStatus)fConfigFromXML->NewXMLConfig( cfTempData );
		
		// we're done with cfTempData
		CFRelease( cfTempData );
		cfTempData = NULL;
		
		// let's re-initialize ourselves to add the new config
		Initialize();		
	}
	
	if ( pRecName != NULL )
	{
		dsDataNodeDeAllocate( dsRef, pRecName );
		pRecName = NULL;
	}
	
	if ( pAuthType != NULL )
	{
		dsDataNodeDeAllocate( dsRef, pAuthType );
		pAuthType = NULL;
	}
	
	if ( sendDataBufPtr != NULL )
	{
		dsDataBufferDeallocatePriv( sendDataBufPtr );
		sendDataBufPtr = NULL;
	}
	
	if ( responseDataBufPtr != NULL )
	{
		dsDataBufferDeallocatePriv( responseDataBufPtr );
		responseDataBufPtr = NULL;
	}
	
	DSFreeString( pComputerID );
	
	if ( pRecType != NULL )
	{
		dsDataNodeDeAllocate( dsRef, pRecType );
		pRecType = NULL;
	}
	
	if ( dsNodeRef )
	{
		dsCloseDirNode( dsNodeRef );
		dsNodeRef = 0;
	}
	
	if ( dsRef )
	{
		dsCloseDirService( dsRef );
		dsRef = 0;
	}
	
	if ( cfServerDict != NULL )
	{
		// let's put the new cfServerDictionary in the return buffer..
		if ( siResult == eDSNoErr )
		{
			SInt32 iError = PutXMLInBuffer( cfServerDict, inData->fOutRequestResponse );
			if ( iError != eDSNoErr ) {
				siResult = (tDirStatus)iError;
			}
		}

		CFRelease( cfServerDict );
		cfServerDict = NULL;
	}
	
	DSCFRelease( cfConfigDict );
	DSCFRelease( cfXMLDict );
	
	return siResult;
} // DoRemoveServer

// ---------------------------------------------------------------------------
//	* GetOwnerGUID
//		Extracts the Owner GUID attribute from the record.
// ---------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::GetOwnerGUID( tDirReference			inDSRef,
                                        tRecordReference		inRecRef,
                                        const char				*refName,
                                        tAttributeValueEntryPtr	*outAttrValEntryPtr )
{
	tDirStatus			siResult	= eDSNoErr;
	tDataNodePtr		pAttrName	= NULL;
	tAttributeEntryPtr	pAttrEntry	= NULL;
	
	// no mapping for this attribute - have to use native.
	pAttrName = dsDataNodeAllocateString( inDSRef, "dsAttrTypeNative:apple-ownerguid" );
	siResult  = dsGetRecordAttributeInfo( inRecRef, pAttrName, &pAttrEntry );
	if ( siResult != eDSNoErr )
	{
		DbgLog( kLogPlugin, "CLDAPv3Plugin::GetOwnerGUID - Error %d attempting to get OwnerGUID in %s", siResult, refName );
	}
	else if ( pAttrEntry->fAttributeValueCount != 1 )
	{
		DbgLog( kLogPlugin, "CLDAPv3Plugin::GetOwnerGUID - Incorrect number of OwnerGUIDs in %s - should be 1, was %d",
				refName, pAttrEntry->fAttributeValueCount );

		siResult = eDSInvalidAttrValueRef;
	}
	else
	{
		siResult = dsGetRecordAttributeValueByIndex( inRecRef, pAttrName, 1, outAttrValEntryPtr );
		if ( siResult != eDSNoErr )
		{
			DbgLog( kLogPlugin, "CLDAPv3Plugin::GetOwnerGUID - Error %d attempting to get OwnerGUID attribute entry in %s", siResult, refName );
		}
	}
	
	dsDeallocAttributeEntry( inDSRef, pAttrEntry );
	dsDataNodeDeAllocate( inDSRef, pAttrName );

	return siResult;
} // GetOwnerGUID

// ---------------------------------------------------------------------------
//	* OwnerGUIDsMatch
//		Compares up to 3 Owner GUIDs (computer, host, LKDC).  Returns true if
//      all are the same.
// ---------------------------------------------------------------------------

bool CLDAPv3Plugin::OwnerGUIDsMatch( tDirReference		inDSRef,
                                     tRecordReference	inCompRecRef,
                                     tRecordReference	inHostCompRecRef,
                                     tRecordReference	inLDKCCompRecRef )
{
	bool					bResult				= true;
	static const char		*recNames[]			= { "Computer Rec", "Host Computer Rec", "LKDC Computer Rec", NULL };
	tRecordReference		recRefs[]			= { inCompRecRef, inHostCompRecRef, inLDKCCompRecRef, 0 };
	tAttributeValueEntryPtr guids[]				= { NULL, NULL, NULL, NULL };
	int						numGuids			= 0;
	
	for ( int i = 0;  recNames[i] != NULL;  i++ )
	{
		// if the ref == 0, the record isn't there, so skip it.
		if ( recRefs[i] != 0 )
		{
			if ( GetOwnerGUID( inDSRef, recRefs[i], recNames[i], &guids[numGuids] ) != eDSNoErr )
			{
				bResult = false;
				break;
			}
			
			++numGuids;
		}
	}

	// compare whatever guids we found.  start loop at 1 because we're using
	// guids[0] as the base for the comparisions.
	if ( bResult )
	{
		for ( int i = 1;  i < numGuids;  i++ )
		{
			if ( ::strcmp( guids[0]->fAttributeValueData.fBufferData, guids[i]->fAttributeValueData.fBufferData ) != 0 )
			{
				bResult = false;
				break;
			}
		}
	}
	
	for ( int i = 0;  i < numGuids;  i++ )
	{
		dsDeallocAttributeValueEntry( inDSRef, guids[i] );
	}

	return bResult;
} // OwnerGUIDsMatch


// ---------------------------------------------------------------------------
//	* SetComputerRecordKerberosAuthority
//
//	Sets a Kerberos Authentication Authority for a Computer record
// ---------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::SetComputerRecordKerberosAuthority(
	DSAPIWrapper &dsWrapper,
	char *inComputerPassword,
	const char *inRecType,
	const char *inRecName,
	CFMutableDictionaryRef inCFDict,
	char **outKerbIDStr )
{
	tDirStatus				siResult				= eDSNoErr;
	tRecordReference		recRef					= 0;
	tAttributeEntryPtr		attribInfo				= NULL;
	tAttributeValueEntryPtr	authEntry				= NULL;
	tDirReference			dsRef					= dsWrapper.GetDSRef();
	tDataNodePtr			pAuthAuthority			= dsDataNodeAllocateString( dsRef, kDSNAttrAuthenticationAuthority );
	
	if ( outKerbIDStr != NULL )
		*outKerbIDStr = NULL;
	
	if ( inRecName == NULL )
		return eDSNoErr;
	
	if ( (siResult = dsWrapper.OpenRecord(inRecType, inRecName, &recRef)) == eDSNoErr )
	{
		// let's get the KerberosID if one is present and set that as well
		if ( dsGetRecordAttributeInfo(recRef, pAuthAuthority, &attribInfo) == eDSNoErr )
		{
			for ( unsigned int ii = 1; ii <= attribInfo->fAttributeValueCount; ii++ )
			{
				if ( dsGetRecordAttributeValueByIndex( recRef, pAuthAuthority, ii, &authEntry ) == eDSNoErr )
				{
					if ( strstr(authEntry->fAttributeValueData.fBufferData, kDSValueAuthAuthorityKerberosv5) )
					{
						char *pAuth = authEntry->fAttributeValueData.fBufferData;
						int zz = 0;
						
						while ( strsep(&pAuth, ";") && zz++ < 2 );
						
						if ( pAuth )
						{
							// one more time so we get the KerberosID itself..
							char *pKerbIDTemp = strsep( &pAuth, ";" );
							if ( pKerbIDTemp && *pKerbIDTemp )
							{
								if ( outKerbIDStr != NULL )
									*outKerbIDStr = strdup( pKerbIDTemp );
									
								if ( inCFDict != NULL )
								{
									CFStringRef cfKerberosID = CFStringCreateWithCString( kCFAllocatorDefault, pKerbIDTemp, kCFStringEncodingUTF8 );
									CFDictionarySetValue( inCFDict, CFSTR(kXMLKerberosId), cfKerberosID );
									CFRelease( cfKerberosID );
								}
								
								AddKeytabEntry( pKerbIDTemp, inComputerPassword );
							}
						}
					}
					
					dsDeallocAttributeValueEntry( dsRef, authEntry );
				}
			}
			dsDeallocAttributeEntry( dsRef, attribInfo );
		}
		
		dsCloseRecord( recRef );
	}
	
	dsDataNodeDeAllocate( dsRef, pAuthAuthority );
	
	return siResult;			
}

