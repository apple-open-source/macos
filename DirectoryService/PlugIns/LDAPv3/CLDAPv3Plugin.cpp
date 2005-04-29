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
 * @header CLDAPv3Plugin
 * LDAP v3 plugin implementation to interface with Directory Services.
 */

#pragma mark Includes

#include <string.h>		//used for strcpy, etc.
#include <stdlib.h>		//used for malloc
#include <ctype.h>		//use for isprint
#include <openssl/md5.h>
#include <syslog.h>

#include <Security/Authorization.h>
#include <SystemConfiguration/SCDynamicStoreCopyDHCPInfo.h>
#include <CoreFoundation/CFPriv.h>

#include "DirServices.h"
#include "DirServicesUtils.h"
#include "DirServicesConst.h"
#include "DirServicesPriv.h"
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
#include "DSEventSemaphore.h"
#include "CSharedData.h"
#include "DSUtils.h"
#include "PrivateTypes.h"
#include "CLog.h"
#include "GetMacAddress.h"
#include "DSLDAPUtils.h"
#include "buffer_unpackers.h"
#include "CDSPluginUtils.h"

#pragma mark -
#pragma mark Defines and Macros

#define LDAPURLOPT95SEPCHAR			" "
#define LDAPCOMEXPSPECIALCHARS		"()|&"

#pragma mark -
#pragma mark Globals and Structures

// --------------------------------------------------------------------------------
//	Globals
CLDAPNode			CLDAPv3Plugin::fLDAPSessionMgr;
bool				CLDAPv3Plugin::fHandlingNetworkTransition;

CLDAPv3Configs	   *gpConfigFromXML		= NULL;
extern	bool		gServerOS;
extern	bool		gBlockLDAPNetworkChange;

DSMutexSemaphore		   *gLDAPKerberosMutex  = nil;
CContinue				   *gLDAPContinueTable	= nil;
CPlugInRef				   *gLDAPContextTable	= nil;
static DSEventSemaphore	   *gKickLDAPv3Requests	= nil;
CLDAPv3Plugin			   *gLDAPv3Plugin		= nil;


//TODO KW the AuthAuthority definitions need to come from DirectoryServiceCore
struct LDAPv3AuthAuthorityHandler
{
	char* fTag;
	LDAPv3AuthAuthorityHandlerProc fHandler;
};

// used for maps using Tag as key
struct LDAPv3AuthTagStruct
{
	string version;
	string authData;
	
	LDAPv3AuthTagStruct( char *inVersion, char *inAuthData )
	{
		if( inVersion ) version = inVersion;
		if( inAuthData) authData = inAuthData;
	}
};

#define		kLDAPv3AuthAuthorityHandlerProcs		3

static LDAPv3AuthAuthorityHandler sLDAPv3AuthAuthorityHandlerProcs[ kLDAPv3AuthAuthorityHandlerProcs ] =
{
	{ kDSTagAuthAuthorityBasic,				(LDAPv3AuthAuthorityHandlerProc)CLDAPv3Plugin::DoBasicAuth },
	{ kDSTagAuthAuthorityPasswordServer,	(LDAPv3AuthAuthorityHandlerProc)CLDAPv3Plugin::DoPasswordServerAuth },
	{ kDSTagAuthAuthorityKerberosv5,		(LDAPv3AuthAuthorityHandlerProc)CLDAPv3Plugin::DoKerberosAuth }
};

enum
{
	eRecordBlatAvailable =				1000,
	eRecordBlatCreateRecWithAttrs =		1001,
	eRecordBlatSetAttrs =				1002,
	eRecordDeleteAndCredentials =		1003
};

#pragma mark -
#pragma mark Prototypes

// needed for new server binding code
extern int doSASLBindAttemptIfPossible( LDAP *inLDAPHost, sLDAPConfigData *pConfig, char *ldapAcct, char *ldapPasswd, char *kerberosID = NULL );

static void DoLDAPPINetworkChange(CFRunLoopTimerRef timer, void *info);
CFStringRef NetworkChangeLDAPPICopyStringCallback( const void *item );

void DSDoSearch (	char			   *inRecType,
					char			   *inNativeRecType,
					CAttributeList	   *inAttrTypeList,
					sLDAPContextData   *inContext,
					sLDAPContinueData  *inContinue,
					ber_int_t			inScope,
					CLDAPNode		   &inLDAPSessionMgr,
					char			   *inQueryFilter,
					int				   &outLDAPReturnCode,
					int					inSearchTO,
					LDAPMessage		  *&outResult);

sInt32 DSDoRetrieval (	char			   *inNativeRecType,
						char			  **inAttrs,
						sLDAPContextData   *inContext,
						ber_int_t			inScope,
						CLDAPNode		   &inLDAPSessionMgr,
						char			   *inQueryFilter,
						int				   &outLDAPReturnCode,
						int					inSearchTO,
						LDAPMessage		  *&outResult,
						bool			   &outResultFound,
						int				   &ldapMsgId);

sInt32 DSSearchLDAP (	CLDAPNode		   &inLDAPSessionMgr,
						sLDAPContextData   *inContext,
						char			   *inNativeRecType,
						int					scope,
						char			   *queryFilter,
						char			  **attrs,
						int				   &ldapMsgId,
						int				   &ldapReturnCode,
						int					recLimit);

sInt32 DSGetSearchLDAPResult (	CLDAPNode		   &inLDAPSessionMgr,
								sLDAPContextData   *inContext,
								int					inSearchTO,
								LDAPMessage		  *&inResult,
								int					all,
								int				   &inLDAPMsgId,
								int				   &inLDAPReturnCode,
								bool				bErrorOnNoEntry);

void DSGetExtendedLDAPResult (	LDAP			   *inHost,
								int					inSearchTO,
								int				   &inLDAPMsgId,
								int				   &inLDAPReturnCode);
								
static int standard_password_replace( LDAP *ld, char *dn, char *oldPwd, char *newPwd );

static int standard_password_create( LDAP *ld, char *dn, char *newPwd );

static int exop_password_create( LDAP *ld, char *dn, char *newPwd );

static void RemoveKeytabEntry( char *inPrinc );

static int DetermineKVNO( char *inPrinc, char *inPass );

static void AddKeytabEntry( char *inPrinc, char *inPass );

#pragma mark -
#pragma mark Support Functions

void DoLDAPPINetworkChange(CFRunLoopTimerRef timer, void *info)
{
	if ( info != nil )
	{
		((CLDAPv3Plugin *)info)->ReInitForNetworkTransition();
	}
}// DoLDAPPINetworkChange


CFStringRef NetworkChangeLDAPPICopyStringCallback( const void *item )
{
	return CFSTR("NetworkChangeLDAPPlugin");
}

sInt32 DSSearchLDAP (	CLDAPNode		   &inLDAPSessionMgr,
						sLDAPContextData   *inContext,
						char			   *inNativeRecType,
						int					scope,
						char			   *queryFilter,
						char			  **attrs,
						int				   &ldapMsgId,
						int				   &ldapReturnCode,
                                                int                         recLimit )
{
	sInt32			siResult 			= eDSNoErr;
	LDAPControl	  **serverctrls			= nil;
	LDAPControl	  **clientctrls			= nil;
	LDAP		   *aHost				= nil;
	
	aHost = inLDAPSessionMgr.LockSession(inContext);
	if (aHost != nil)
    {
		ldapReturnCode = ldap_search_ext(	aHost,
											inNativeRecType,
											scope,
											queryFilter,
											attrs,
											0,
											serverctrls,
											clientctrls,
											0, recLimit, 
											&ldapMsgId );
    }
    else
    {
        ldapReturnCode = LDAP_LOCAL_ERROR;
    }
	switch(ldapReturnCode)
	{
		case LDAP_SUCCESS:
			break;
		case LDAP_UNAVAILABLE:
		case LDAP_SERVER_DOWN:
		case LDAP_BUSY:
		case LDAP_LOCAL_ERROR:
		//case LDAP_TIMEOUT:
			siResult = eDSCannotAccessSession;
			break;
		default:
			//nothing found? - continue data msgId not set
			siResult = eDSRecordNotFound;
			break;
	}
	
	// CheckFailed thread requires that we not flag the connection as bad, otherwise we will not
	// reconnect if necessary now.  If the reconnect fails, it will get flagged as bad then.
	inLDAPSessionMgr.UnLockSession(inContext, (siResult == eDSRecordNotFound) );
	
	if (serverctrls)  ldap_controls_free( serverctrls );
	if (clientctrls)  ldap_controls_free( clientctrls );

	return(siResult);
} // DSSearchLDAP

sInt32 DSGetSearchLDAPResult (	CLDAPNode		   &inLDAPSessionMgr,
								sLDAPContextData   *inContext,
								int					inSearchTO,
								LDAPMessage		  *&inResult,
								int					all,
								int				   &inLDAPMsgId,
								int				   &inLDAPReturnCode,
								bool				bErrorOnNoEntry)
{
	sInt32			siResult 			= eDSNoErr;
	LDAP		   *aHost				= nil;

	aHost = inLDAPSessionMgr.LockSession(inContext);
	if (aHost != nil)
	{
		double	maxTime = dsTimestamp() + USEC_PER_SEC*((inSearchTO)?inSearchTO:kLDAPDefaultSearchTimeoutInSeconds);
		struct	timeval	tv = {kLDAPDefaultNetworkTimeoutInSeconds,0};

		inLDAPReturnCode = 0;
		
		SetNetworkTimeoutsForHost( aHost, kLDAPDefaultNetworkTimeoutInSeconds );

		while ( inLDAPReturnCode == 0 && dsTimestamp() < maxTime && CLDAPv3Plugin::HandlingNetworkTransition() == false )
		{
			inLDAPReturnCode = ldap_result(aHost, inLDAPMsgId, all, &tv, &inResult);
		}
			
		if ( inLDAPReturnCode == 0 )
		{
			// timed out, let's forget it
			ldap_abandon(aHost, inLDAPMsgId);
			inLDAPReturnCode = -1;
		}
	}
	else
	{
		inLDAPReturnCode = LDAP_LOCAL_ERROR;
	}
	
	// CheckFailed thread requires that we not flag the connection as bad, otherwise we will not
	// reconnect if necessary now.  If the reconnect fails, it will get flagged as bad then.
	inLDAPSessionMgr.UnLockSession(inContext, (inLDAPReturnCode == -1) );
	
	switch(inLDAPReturnCode)
	{
		case LDAP_RES_SEARCH_ENTRY:
			break;
		case LDAP_RES_SEARCH_RESULT:
			{
				int				ldapReturnCode2 = 0;
				int 			err				= 0;
				char		   *matcheddn		= nil;
				char		   *text			= nil;
				char		  **refs			= nil;
				LDAPControl	  **ctrls			= nil;
				aHost = inLDAPSessionMgr.LockSession(inContext);
				if (aHost != nil)
				{
					ldapReturnCode2 = ldap_parse_result(aHost, inResult, &err, &matcheddn, &text, &refs, &ctrls, 0);
				}
				else
				{
					ldapReturnCode2 = LDAP_LOCAL_ERROR;
				}
				inLDAPSessionMgr.UnLockSession(inContext);
				if (ldapReturnCode2 == LDAP_LOCAL_ERROR)
				{
					break;
				}
				else
				{
					if ( (ldapReturnCode2 != LDAP_SUCCESS) && (ldapReturnCode2 != LDAP_MORE_RESULTS_TO_RETURN) )
					{
						aHost = inLDAPSessionMgr.LockSession(inContext);
						ldap_abandon(aHost, inLDAPMsgId);
						inLDAPSessionMgr.UnLockSession(inContext);
					}
					else
					{
						if (ldapReturnCode2 == LDAP_SUCCESS) inLDAPReturnCode = LDAP_SUCCESS;
						if (text)  ber_memfree( text );
						if (matcheddn)  ber_memfree( matcheddn );
						if (refs)  ber_memvfree( (void **) refs );
						if (ctrls)  ldap_controls_free( ctrls );
					}
				}
				//no need to set siResult unless bErrorOnNoEntry
				//since some calls can't handle the return of LDAP_RES_SEARCH_RESULT
				if (bErrorOnNoEntry)
				{
					siResult = eDSRecordNotFound;
				}
				break;
			}
		case LDAP_TIMEOUT:
			siResult = eDSServerTimeout;
			break;
		case -1:
			siResult = eDSCannotAccessSession;
			break;
		default:
			//nothing found? even with LDAP_SUCCESS
			siResult = eDSRecordNotFound;
			break;
	} //switch(ldapReturnCode)

	return (siResult);
}

void DSGetExtendedLDAPResult (	LDAP			   *inHost,
								int					inSearchTO,
								int				   &inLDAPMsgId,
								int				   &inLDAPReturnCode)
{
    LDAPMessage*	res = NULL;
	double			maxTime = dsTimestamp() + USEC_PER_SEC*((inSearchTO)?inSearchTO:kLDAPDefaultSearchTimeoutInSeconds);
	struct	timeval	tv = {kLDAPDefaultNetworkTimeoutInSeconds,0};

	inLDAPReturnCode = 0;
	
	SetNetworkTimeoutsForHost( inHost, kLDAPDefaultNetworkTimeoutInSeconds );

	while ( inLDAPReturnCode == 0 && dsTimestamp() < maxTime && CLDAPv3Plugin::HandlingNetworkTransition() == false )
	{
		inLDAPReturnCode = ldap_result(inHost, inLDAPMsgId, LDAP_MSG_ALL, &tv, &res);
	}

	switch(inLDAPReturnCode)
	{
		case LDAP_RES_SEARCH_ENTRY:
			break;
		case LDAP_RES_EXTENDED:
		case LDAP_RES_SEARCH_RESULT:
			{
				int				ldapReturnCode2 = 0;
				int 			err				= 0;
				char		   *matcheddn		= nil;
				char		   *text			= nil;
				char		  **refs			= nil;
				LDAPControl	  **ctrls			= nil;
				ldapReturnCode2 = ldap_parse_result(inHost, res, &err, &matcheddn, &text, &refs, &ctrls, 0);
				if ( (ldapReturnCode2 != LDAP_SUCCESS) && (ldapReturnCode2 != LDAP_MORE_RESULTS_TO_RETURN) )
				{
					ldap_abandon(inHost, inLDAPMsgId);
                    inLDAPReturnCode = ldapReturnCode2;
				}
				else
				{
					inLDAPReturnCode = err;
					if (text && *text)  ber_memfree( text );
					if (matcheddn && *matcheddn)  ber_memfree( matcheddn );
					if (refs)  ber_memvfree( (void **) refs );
					if (ctrls)  ldap_controls_free( ctrls );
				}
			}
			break;
		case LDAP_TIMEOUT:
		case LDAP_SUCCESS:
		case -1:
		default:
            // the inReturnCode is passed back to the caller in this case
            break;
	} //switch(ldapReturnCode)
}

static int standard_password_replace( LDAP *ld, char *dn, char *oldPwd, char *newPwd )
{
	char *pwdDelete[2] = {};
	pwdDelete[0] = oldPwd;
	pwdDelete[1] = NULL;
	LDAPMod modDelete = {};
	modDelete.mod_op = LDAP_MOD_DELETE;
	modDelete.mod_type = "userPassword";
	modDelete.mod_values = pwdDelete;
	char *pwdCreate[2] = {};
	pwdCreate[0] = newPwd;
	pwdCreate[1] = NULL;
	LDAPMod modAdd = {};
	modAdd.mod_op = LDAP_MOD_ADD;
	modAdd.mod_type = "userPassword";
	modAdd.mod_values = pwdCreate;
	
	LDAPMod *mods[3] = {};
	mods[0] = &modDelete;
	mods[1] = &modAdd;
	mods[2] = NULL;

	return ldap_modify_s( ld, dn, mods );
}

static int standard_password_create( LDAP *ld, char *dn, char *newPwd )
{
	char *pwdCreate[2] = {};
	pwdCreate[0] = newPwd;
	pwdCreate[1] = NULL;
	LDAPMod modReplace = {};
	modReplace.mod_op = LDAP_MOD_REPLACE;
	modReplace.mod_type = "userPassword";
	modReplace.mod_values = pwdCreate;
	
	LDAPMod *mods[2] = {};
	mods[0] = &modReplace;
	mods[1] = NULL;

	return ldap_modify_s( ld, dn, mods );
}

static int exop_password_create( LDAP *ld, char *dn, char *newPwd )
{
	BerElement *ber = ber_alloc_t( LBER_USE_DER );
	if( ber == NULL ) throw( (sInt32) eMemoryError );
	ber_printf( ber, "{" /*}*/ );
	ber_printf( ber, "ts", LDAP_TAG_EXOP_MODIFY_PASSWD_ID, dn );
	ber_printf( ber, "ts", LDAP_TAG_EXOP_MODIFY_PASSWD_NEW, newPwd );
	ber_printf( ber, /*{*/ "N}" );

	struct berval *bv = NULL;
	int rc = ber_flatten( ber, &bv );

	if( rc < 0 ) throw( (sInt32) eMemoryError );

	ber_free( ber, 1 );

	int id;
	rc = ldap_extended_operation( ld, LDAP_EXOP_MODIFY_PASSWD, bv, NULL, NULL, &id );

	ber_bvfree( bv );

	if ( rc == LDAP_SUCCESS ) {
			DSGetExtendedLDAPResult( ld, 0, id, rc);
	}

	return rc;
}

//------------------------------------------------------------------------------------
//	* DSDoSearch
//------------------------------------------------------------------------------------

void DSDoSearch (	char			   *inRecType,
					char			   *inNativeRecType,
					CAttributeList	   *inAttrTypeList,
					sLDAPContextData   *inContext,
					sLDAPContinueData  *inContinue,
					ber_int_t			inScope,
					CLDAPNode		   &inLDAPSessionMgr,
					char			   *inQueryFilter,
					int				   &outLDAPReturnCode,
					int					inSearchTO,
					LDAPMessage		  *&outResult)
{
	sInt32				searchResult	= eDSNoErr;
	char			  **attrs			= nil;
	int					ldapMsgId		= 0;
	int					numRetries		= 2; //two tries to rebind
	
	try
	{
	
		// here we check if there was a LDAP message ID in the context
		// If there was we continue to query, otherwise we search anew
		if (inContinue->msgId == 0)
		{
			attrs = CLDAPv3Plugin::MapAttrListToLDAPTypeArray( inRecType, inAttrTypeList, inContext->fNodeName );
			// here is the call to the LDAP server asynchronously which requires
			// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
			// attribute list (NULL for all), return attrs values flag
			// This returns us the message ID which is used to query the server for the results
			while (true)
			{
				searchResult = DSSearchLDAP(	inLDAPSessionMgr,
												inContext,
												inNativeRecType,
												inScope,
												inQueryFilter,
												attrs,
												ldapMsgId,
												outLDAPReturnCode,
                                                                                                inContinue->fLimitRecSearch );
												
				if (	(searchResult == eDSNoErr) &&
						((inContinue->fTotalRecCount < inContinue->fLimitRecSearch) || (inContinue->fLimitRecSearch == 0)) )
				{
					searchResult = DSGetSearchLDAPResult (	inLDAPSessionMgr,
															inContext,
															inSearchTO,
															outResult,
															LDAP_MSG_ONE,
															ldapMsgId,
															outLDAPReturnCode,
															false);
				}
	
				if (searchResult == eDSRecordNotFound)
				{
					throw( (sInt32)eDSRecordNotFound );
				}
				else if (searchResult == eDSCannotAccessSession)
				{
					if (numRetries == 0)
					{
						throw( (sInt32)eDSCannotAccessSession );
					}
					else
					{
						CLDAPv3Plugin::RebindLDAPSession(inContext, inLDAPSessionMgr );
						numRetries--;
					}
				}
				else if (searchResult == eDSServerTimeout)
				{
					throw( (sInt32)eDSServerTimeout );
				}
				else
				{
					break;
				}
			}//while
			
			inContinue->msgId = ldapMsgId;
		} // msgId == 0
		else
		{
			if ( (inContinue->fTotalRecCount < inContinue->fLimitRecSearch) || (inContinue->fLimitRecSearch == 0) )
			{
				//check if there is a carried LDAP message in the context
				//with a rebind here in between context calls we still have the previous result
				//however, the next call will start right over in the whole context of the ldap_search
				if (inContinue->pResult != nil)
				{
					outResult = inContinue->pResult;
					outLDAPReturnCode = LDAP_RES_SEARCH_ENTRY;
				}
				//retrieve a new LDAP message
				else
				{
					searchResult = DSGetSearchLDAPResult (	inLDAPSessionMgr,
															inContext,
															inSearchTO,
															outResult,
															LDAP_MSG_ONE,
															inContinue->msgId,
															outLDAPReturnCode,
															false);
					if (searchResult == eDSCannotAccessSession)
					{
						throw( (sInt32)eDSCannotAccessSession );
					}
					else if (searchResult == eDSServerTimeout)
					{
						throw( (sInt32)eDSServerTimeout );
					}
				}
			}
		}
	}
	
	catch ( sInt32 err )
	{
		searchResult = err;
	}

	if (attrs != nil)
	{
		int aIndex = 0;
		while (attrs[aIndex] != nil)
		{
			free(attrs[aIndex]);
			attrs[aIndex] = nil;
			aIndex++;
		}
		free(attrs);
		attrs = nil;
	}
	
	//throw out if error and after cleanup of local variables
	if (searchResult != eDSNoErr)
	{
		throw(searchResult);
	}
	
} // DSDoSearch

//------------------------------------------------------------------------------------
//	* DSDoRetrieval
//------------------------------------------------------------------------------------

sInt32 DSDoRetrieval (	char			   *inNativeRecType,
						char			  **inAttrs,
						sLDAPContextData   *inContext,
						ber_int_t			inScope,
						CLDAPNode		   &inLDAPSessionMgr,
						char			   *inQueryFilter,
						int				   &outLDAPReturnCode,
						int					inSearchTO,
						LDAPMessage		  *&outResult,
						bool			   &outResultFound,
						int				   &ldapMsgId)
{
	sInt32				searchResult	= eDSNoErr;
	int					numRetries		= 2; //two tries to rebind

	try
	{
		// here is the call to the LDAP server asynchronously which requires
		// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
		// attribute list (NULL for all), return attrs values flag
		while (true)
		{
			searchResult = DSSearchLDAP(	inLDAPSessionMgr,
											inContext,
											inNativeRecType,
											inScope,
											inQueryFilter,
											inAttrs,
											ldapMsgId,
											outLDAPReturnCode,
                                                                                        1 );
			if (searchResult == eDSNoErr)
			{
				// retrieve the actual LDAP record data for use internally
				// KW should we internally re-read the result after a write?
				searchResult = DSGetSearchLDAPResult (	inLDAPSessionMgr,
														inContext,
														inSearchTO,
														outResult,
														LDAP_MSG_ONE,
														ldapMsgId,
														outLDAPReturnCode,
														true);
			}
			outResultFound = false;
			if (searchResult == eDSRecordNotFound)
			{
				break;
			}
			else if (searchResult == eDSCannotAccessSession)
			{
				if (numRetries == 0)
				{
					throw( (sInt32)eDSCannotAccessSession );
				}
				else
				{
					CLDAPv3Plugin::RebindLDAPSession(inContext, inLDAPSessionMgr );
					numRetries--;
				}
			}
			else if (searchResult == eDSServerTimeout)
			{
				break;
			}
			else
			{
				outResultFound = true;
				break;
			}
		} // while
	}
	
	catch ( sInt32 err )
	{
		searchResult = err;
	}

	//throw out if eDSCannotAccessSession error only after cleanup of local variables
	if (searchResult == eDSCannotAccessSession)
	{
		throw(searchResult);
	}
	
	return(searchResult);
	
} // DSDoRetrieval

void AddKeytabEntry( char *inPrinc, char *inPass )
{
	char				ktname[MAXPATHLEN+sizeof("WRFILE:")+1];
	krb5_keytab			kt			= NULL;
	krb5_context		kContext	= NULL;
	krb5_error_code		retval;
	krb5_keytab_entry   entry;
	krb5_data			password;
	krb5_data			salt;
	long				types[]		= { ENCTYPE_ARCFOUR_HMAC, ENCTYPE_DES_CBC_CRC, ENCTYPE_DES3_CBC_SHA1, 0 };
	long				*keyType	= types;
	
	// let's clear variable contents
	memset( &entry, 0, sizeof(entry) );
	memset( &password, 0, sizeof(password) );
	memset( &salt, 0, sizeof(salt) );
	
	// let's set up the password structure
	password.data = strdup( inPass );
	password.length = strlen( password.data );
	password.magic = KV5M_PWD_DATA;
	
	// information that doesn't change...
	entry.magic = KV5M_KEYTAB;
	
	// before we add the new key, let's remove the old key for this principal
	RemoveKeytabEntry( inPrinc );
	
	gLDAPKerberosMutex->Wait();
	
	retval = krb5_init_context( &kContext );
	if ( 0 == retval )
	{
		// get the keytab, writable
		krb5_kt_default_name( kContext, &ktname[2], sizeof(ktname)-2 );
		ktname[0] = 'W';
		ktname[1] = 'R';
		
		retval = krb5_kt_resolve( kContext, ktname, &kt );
	}
	
	if( 0 == retval )
	{
		// let's parse the principal we got
		retval = krb5_parse_name( kContext, inPrinc, &entry.principal );
	}
	
	if( 0 == retval )
	{
		// let's salt it...
		retval = krb5_principal2salt( kContext, entry.principal, &salt );					
	}
	
	if( 0 == retval )
	{

		// determine the vno for this key
		entry.vno = DetermineKVNO( inPrinc, inPass );
		
		if (entry.vno != 0) {
		
			// now let's put it in the keytab accordingly
			// let's save each key type..
			do
			{
				retval = krb5_c_string_to_key( kContext, *keyType, &password, &salt, &entry.key );
				if( 0 == retval )
				{
					// don't add it if we couldn't get the kvno
					if (entry.vno != 0) {
						
						// we don't care if we can't add it, we'll try the rest of them...
						krb5_kt_add_entry( kContext, kt, &entry );
					}
					
					// need to free the keyblock we just created with string_to_key, we leave the rest cause we are still using it..
					krb5_free_keyblock_contents( kContext, &entry.key );
				}
			} while ( *(++keyType) );
		}
		
		// need to free the salt data cause we are going to use it again..
		krb5_free_data_contents( kContext, &salt );
	}
	
	// need to free contents of entry, we don't have a pointer, this clears principal and anything left
	krb5_free_keytab_entry_contents( kContext, &entry );
	
	if( password.data )
	{
		memset( password.data, 0, password.length );
		free( password.data );
		password.data = NULL;
	}
	
	// let's free stuff up..
	if( kt != NULL )
	{
		krb5_kt_close( kContext, kt );
		kt = NULL;
	}
	
	if( kContext != NULL )
	{
		krb5_free_context( kContext );
		kContext = NULL;
	}
	
	gLDAPKerberosMutex->Signal();
	
} // AddKeytabEntry

void RemoveKeytabEntry( char *inPrinc )
{
	char				ktname[MAXPATHLEN+sizeof("WRFILE:")+1];
	krb5_kt_cursor		cursor;
	krb5_keytab_entry   entry;
	krb5_principal		host_princ	= NULL;
	krb5_keytab			kt			= NULL;
	krb5_context		kContext	= NULL;
	krb5_error_code		retval;
	
	gLDAPKerberosMutex->Wait();
	
	retval = krb5_init_context( &kContext );
	if( 0 == retval )
	{
		krb5_kt_default_name( kContext, &ktname[2], sizeof(ktname)-2 );
		ktname[0] = 'W';
		ktname[1] = 'R';
	}
	
	retval = krb5_kt_resolve( kContext, ktname, &kt );
	if( 0 == retval )
	{
		krb5_parse_name( kContext, inPrinc, &host_princ );
	}
	
	retval = krb5_kt_start_seq_get( kContext, kt, &cursor );
	if( 0 == retval )
	{
		while( retval != KRB5_KT_END && retval != ENOENT )
		{
			if( retval = krb5_kt_next_entry(kContext, kt, &entry, &cursor) )
			{
				break;
			}
			
			if( krb5_principal_compare(kContext, entry.principal, host_princ) )
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
		krb5_kt_end_seq_get( kContext, kt, &cursor );
	}
	
	if( host_princ )
	{
		// need to free the princpal we've been using.
		krb5_free_principal( kContext, host_princ );
	}
	
	if( kt != NULL )
	{
		krb5_kt_close( kContext, kt );
		kt = NULL;
	}
	
	if( kContext )
	{
		krb5_free_context( kContext );
		kContext = NULL;
	}
	
	gLDAPKerberosMutex->Signal();
	
} // RemoveKeytabEntry

int DetermineKVNO( char *inPrinc, char *inPass )
{
	int				kvno		= 0;
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
	
	bzero( &my_creds, sizeof(my_creds) );
	bzero( &kvno_creds, sizeof(kvno_creds) );
	
	if( inPrinc == NULL || inPass == NULL )
		return 0;
	
	gLDAPKerberosMutex->Wait();

	retval = krb5_init_context( &krbContext );
	if ( 0 == retval )
	{
		retval = krb5_parse_name( krbContext, inPrinc, &principal );
	}

	if ( 0 == retval )
	{
		retval = krb5_unparse_name( krbContext, principal, &principalString );
	}

	if ( 0 == retval )
	{
		pCacheName = (char *) malloc( strlen(principalString) + sizeof("MEMORY:") + 1 );	// "MEMORY:" + name + NULL
		strcpy( pCacheName,  "MEMORY:" );
		strcat( pCacheName, principalString );
	}
	
	// let's see if we already have a cache for this user..
	if ( 0 == retval )
	{
		retval = krb5_cc_resolve( krbContext, pCacheName, &krbCache);
	}
	
	if ( 0 == retval )
	{
		retval = krb5_cc_set_default_name( krbContext, pCacheName );
	}

	if ( 0 == retval )
	{
		// let's initialize a cache for ourselves
		retval = krb5_cc_initialize( krbContext, krbCache, principal );
	}
	
	if ( 0 == retval )
	{
		krb5_int32				startTime   = 0;
		krb5_get_init_creds_opt  options;
		
		krb5_get_init_creds_opt_init( &options );
		krb5_get_init_creds_opt_set_forwardable( &options, 1 );
		krb5_get_init_creds_opt_set_proxiable( &options, 1 );
		krb5_get_init_creds_opt_set_address_list( &options, NULL );

		krb5_get_init_creds_opt_set_tkt_life( &options, 300 ); // minimum is 5 minutes

		retval = krb5_get_init_creds_password( krbContext, &my_creds, principal, inPass, NULL, 0, startTime, NULL, &options );
	}
	
	if ( 0 == retval )
	{
		retval = krb5_cc_store_cred( krbContext, krbCache, &my_creds );
	}
	
	if ( 0 == retval )
	{
		// now that we have the TGT, lets get a ticket for ourself
		krb5_copy_principal( krbContext, my_creds.client, &kvno_creds.client );
		krb5_copy_principal( krbContext, my_creds.client, &kvno_creds.server );

		retval = krb5_get_credentials( krbContext, 0, krbCache, &kvno_creds, &my_creds_out );
	}
	
	if ( 0 == retval )
	{
		// extract the kvno
		retval = krb5_decode_ticket( &(my_creds_out->ticket), &ticket );
		if ( 0 == retval )
		{
			kvno = ticket->enc_part.kvno;
			
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
	
	if( krbContext )
	{
		krb5_free_context( krbContext );
		krbContext = NULL;
	}
	
	gLDAPKerberosMutex->Signal();

	return kvno;
}

#pragma mark -
#pragma mark CLDAPv3Plugin Member Functions

// --------------------------------------------------------------------------------
//	* CLDAPv3Plugin ()
// --------------------------------------------------------------------------------

CLDAPv3Plugin::CLDAPv3Plugin ( FourCharCode inSig, const char *inName ) : CServerPlugin(inSig, inName)
{
	
	fState					= kUnknownState;
	fState += kInactive;
	bDoNotInitTwiceAtStartUp= true;
    //ensure that the configXML is nil before initialization
	fDHCPLDAPServersString		= NULL;
	fPrevDHCPLDAPServersString	= NULL;
	fServerRunLoop			= nil;  //could be obtained directly now since a module not plugin
	fTimeToHandleNetworkTransition = 0;
	fHandlingNetworkTransition = false;
	fInitFlag				= false;
    
	gLDAPv3Plugin = this;
	
    if ( gLDAPContinueTable == nil )
    {
		if (gServerOS)
		{
			gLDAPContinueTable = new CContinue( CLDAPv3Plugin::ContinueDeallocProc, 256 );
		}
		else
		{
			gLDAPContinueTable = new CContinue( CLDAPv3Plugin::ContinueDeallocProc, 64 );
		}
    }

    //could pass in a DeleteConfigData method instead of nil so that cleanup code is internal to the list
	//but currently cleaned up outside of the list
    if ( gLDAPContextTable == nil )
    {
		if (gServerOS)
		{
			gLDAPContextTable = new CPlugInRef( CLDAPv3Plugin::ContextDeallocProc, 1024 );
		}
		else
		{
			gLDAPContextTable = new CPlugInRef( CLDAPv3Plugin::ContextDeallocProc, 256 );
		}
    }

	if ( gKickLDAPv3Requests == nil )
	{
		gKickLDAPv3Requests = new DSEventSemaphore();
	}

} // CLDAPv3Plugin


// --------------------------------------------------------------------------------
//	* ~CLDAPv3Plugin ()
// --------------------------------------------------------------------------------

CLDAPv3Plugin::~CLDAPv3Plugin ( void )
{
	//cleanup the mappings and config data here
	//this will clean up the following:
	// 1) gLDAPConfigTable
	// 2) gLDAPConfigTableLen
    if ( gpConfigFromXML != nil)
    {
        delete ( gpConfigFromXML );
        gpConfigFromXML			= nil;
    }

    //no need to clean this up since only one and it exists for the life of this process
    //ie. don't want a race condition on shutdown where it may be in use on another thread
    //if ( gLDAPContinueTable != nil)
    //{
        //delete ( gLDAPContinueTable );
        //gLDAPContinueTable = nil;
    //}
    
    //KW ensure the release of all LDAP session handles eventually
    //but probably NOT through CleanContextData since multiple contexts will
    //have the same session handle
	//TODO is this true?
    
} // ~CLDAPv3Plugin


// --------------------------------------------------------------------------------
//	* Validate ()
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::Validate ( const char *inVersionStr, const uInt32 inSignature )
{
	fPlugInSignature = inSignature;

	return( noErr );

} // Validate


// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::Initialize ( void )
{
    int					countNodes	= 0;
    sInt32				siResult	= eDSNoErr;
    tDataList		   *pldapName	= nil;
    sLDAPConfigData	   *pConfig		= nil;
	char			   *server		= nil;
	char			   *searchBase	= nil;
	int					portNumber	= 389;
	bool				bIsSSL		= false;

	fCheckInitFlag.Wait();
	if (fInitFlag == true)
	{
		//we can do this since all dispatch api calls are blocked if we are in this routine 
		//ie. it has code to look at the Flag and wait until we are done
		//so any other calls should be dismissed
		SRVRLOG( kLogPlugin, "CLDAPv3Plugin: Initialize:: Currently initializing so returning success to caller" );
		fCheckInitFlag.Signal();
		return(siResult);
	}
	fInitFlag = true;
	fCheckInitFlag.Signal();
   
    try
	{
	    if ( gpConfigFromXML == nil )
	    {
	        gpConfigFromXML = new CLDAPv3Configs();
            if ( gpConfigFromXML  == nil ) throw( (sInt32)eDSOpenNodeFailed ); //KW need an eDSPlugInConfigFileError
	    }
		
	    siResult = gpConfigFromXML->Init();
	    if ( siResult != eDSNoErr ) throw( siResult );
		
		//duplicate node in local config file overrides any DHCP obtained config
		
		//our DHCP obtained node has its config as part of the gLDAPConfigTable
		//we register it as the node it really is
		//check if DHCP has changed - if so then we re-discover the nodes else we simply mark the bUpdated flag true
		//go get the DHCP nodes
		CFDictionaryRef		ourDHCPInfo			= NULL;
		CFStringRef			ourDHCPServers		= NULL;
		CFDataRef			ourDHCPServersData	= NULL;
		UInt8			   *byteData			= NULL;
		bool				bNewDHCPConfig		= false;
		
		//save current DHCP server string to compare with possible changes below
		DSCFRelease(fPrevDHCPLDAPServersString);
		fPrevDHCPLDAPServersString = fDHCPLDAPServersString;
		if (fPrevDHCPLDAPServersString != NULL)
		{
			CFRetain(fPrevDHCPLDAPServersString);
		}
		
		ourDHCPInfo = SCDynamicStoreCopyDHCPInfo(NULL, NULL);
		if (ourDHCPInfo != NULL)
		{
			ourDHCPServersData = (CFDataRef) DHCPInfoGetOptionData(ourDHCPInfo, 95);
			if (ourDHCPServersData != NULL)
			{
				byteData = (UInt8 *) calloc(1, CFDataGetLength(ourDHCPServersData));
				if (byteData != NULL)
				{
					CFDataGetBytes(ourDHCPServersData, CFRangeMake(	0,
																	CFDataGetLength(ourDHCPServersData)),
																	byteData);
					if (byteData[0] != '\0')
					{
						ourDHCPServers = CFStringCreateWithBytes(	kCFAllocatorDefault,
																	byteData,
																	CFDataGetLength(ourDHCPServersData),
																	kCFStringEncodingUTF8,
																	false);
					}
					free(byteData);
				} // byteData not NULL
				//CFRelease(ourDHCPServersData); don't release since retrieved with Get
			}
			if (ourDHCPServers != NULL)
			{
				if ( CFGetTypeID( ourDHCPServers ) == CFStringGetTypeID() )
				{
					if ( ( fDHCPLDAPServersString == NULL ) || (CFStringCompare(fDHCPLDAPServersString, ourDHCPServers, 0) != kCFCompareEqualTo) )
					{
						DSCFRelease(fDHCPLDAPServersString);
						fDHCPLDAPServersString = ourDHCPServers;
						bNewDHCPConfig = true;  // set this so we can do a System Config change notification...
					}
					else
					{
						CFRelease(ourDHCPServers);
						ourDHCPServers = nil;
					}

					//always update the config table
					//need to parse the fDHCPLDAPServersString string
					int serverIndex = 1;
					while( ParseNextDHCPLDAPServerString(&server, &searchBase, &portNumber, &bIsSSL, serverIndex) )
					{
						gpConfigFromXML->MakeServerBasedMappingsLDAPConfig( server, 
																			searchBase, 
																			kLDAPDefaultOpenCloseTimeoutInSeconds, 
																			2, 
																			kLDAPDefaultRebindTryTimeoutInSeconds, 
																			kLDAPDefaultSearchTimeoutInSeconds, 
																			portNumber, 
																			bIsSSL, 
																			true, 
																			true,
																			false);

						DSFreeString(server);
						DSFreeString(searchBase);
						portNumber	= 389;
						bIsSSL		= false;
						serverIndex++;
					}
				} // if ( CFGetTypeID( ourDHCPServers ) == CFStringGetTypeID() )
			} // if (ourDHCPServers != NULL)
			else
			{
				DSCFRelease(fDHCPLDAPServersString);
			}
			DSCFRelease(ourDHCPInfo);
		} // if (ourDHCPInfo != NULL)
		else
		{
			DSCFRelease(fDHCPLDAPServersString);
		}
		
	    //Cycle through the LDAPConfigMap
		gpConfigFromXML->ConfigMapWait();   // need to lock here since we are reinitializing an existing config.
		
		list <string> goneDHCPServers;

		//find out if we need to remove DHCP LDAP configs that are no longer received via DHCP
		if ( (fDHCPLDAPServersString != NULL) || (fPrevDHCPLDAPServersString != NULL) )
		{
			char **defaultLDAPNodes = nil;
			uInt32 aDefaultLDAPNodeCount = 0;
			//mutex is already held so this should be okay
			defaultLDAPNodes = gpConfigFromXML->GetDefaultLDAPNodeStrings(aDefaultLDAPNodeCount);
			
			if ( (aDefaultLDAPNodeCount > 0) && (defaultLDAPNodes != nil) )
			{
				int listIndex = 0;
				CFRange foundInPrev;
				CFRange foundInCurr;
				for (listIndex=0; defaultLDAPNodes[listIndex] != nil; listIndex++)
				{
					CFStringRef serverString = CFStringCreateWithCString( NULL, defaultLDAPNodes[listIndex], kCFStringEncodingUTF8 );
					foundInPrev.location = 0;
					foundInCurr.location = 0;
					if (fPrevDHCPLDAPServersString != NULL)
						foundInPrev = CFStringFind(fPrevDHCPLDAPServersString, serverString, kCFCompareCaseInsensitive);
					if (fDHCPLDAPServersString != NULL)
						foundInCurr = CFStringFind(fDHCPLDAPServersString, serverString, kCFCompareCaseInsensitive);
					if ( (foundInPrev.location != kCFNotFound) && (foundInCurr.location == kCFNotFound) )
					{
						//we have now dropped this DHCP LDAP server from our consideration
						goneDHCPServers.push_back(string(defaultLDAPNodes[listIndex]));
					}
					DSCFRelease(serverString);
				}
			}
			DSFreeStringList(defaultLDAPNodes);
		}

		LDAPConfigDataMap configMap = gpConfigFromXML->GetConfigMap();
		
		LDAPConfigDataMapI configIter = configMap.begin();
		
		list <string> deadNodenames;
		
		while( configIter != configMap.end() )
		{
			pConfig = configIter->second;
			
	        if (pConfig != nil)
	        {
				if (pConfig->fNodeName != nil)
				{
					if (pConfig->bUseAsDefaultLDAP) //DHCP LDAP possible node - check if we need to remove it
					{
						for ( list<string>::iterator goneIter = goneDHCPServers.begin(); goneIter != goneDHCPServers.end(); ++goneIter)
						{
							if ( ( pConfig->fServerName != nil) && ( strcasecmp( pConfig->fServerName, (*goneIter).c_str()) == 0) )
								pConfig->bUpdated = false;
						}
					}
					if (pConfig->bUpdated)
					{
						//allow register of nodes that have NOT been verified by ldap_init calls
						countNodes++;
						pConfig->bAvail = true;
						//add standard LDAPv3 prefix to the registered node names here
						pldapName = dsBuildListFromStringsPriv((char *)"LDAPv3", pConfig->fNodeName, nil);
						if (pldapName != nil)
						{
							//same node does not get registered twice
							CServerPlugin::_RegisterNode( fPlugInSignature, pldapName, kDirNodeType );
							dsDataListDeallocatePriv( pldapName);
							free(pldapName);
							pldapName = nil;
						}
						if (pConfig->bServerMappings)
						{
							//reset to get server mappings again if there was update here
							pConfig->bGetServerMappings = true;
						}
						pConfig->bBuildReplicaList = true; //always reset this
					} // Config has been updated OR is new so register the node
					else
					{
						//UN register the node
						//and remove it from the config table
						
		                //add standard LDAPv3 prefix to the registered node names here
		                pldapName = dsBuildListFromStringsPriv((char *)"LDAPv3", pConfig->fNodeName, nil);
		                if (pldapName != nil)
		                {
							//KW what happens when the same node is unregistered twice???
	                        CServerPlugin::_UnregisterNode( fPlugInSignature, pldapName );
	                    	dsDataListDeallocatePriv( pldapName);
							free(pldapName);
	                    	pldapName = nil;
		                }
						
						//let's attempt to force close any non-referenced connections
						deadNodenames.push_back(string(pConfig->fNodeName));
						gpConfigFromXML->DeleteConfigFromMap( pConfig->fNodeName );

						pConfig = nil;
					}
				} //if servername defined
	        } // pConfig != nil
			
			configIter++;
	    } // loop over the LDAP config entries
		
		gpConfigFromXML->ConfigMapSignal();

		for ( list<string>::iterator deadIter = deadNodenames.begin(); deadIter != deadNodenames.end(); ++deadIter)
		{
			fLDAPSessionMgr.ForcedSafeClose( (*deadIter).c_str() );
		}
		// let's signal a network transition causing all nodes to be checked immediately via thread
		// ensures previously failed nodes may now be available...
		fLDAPSessionMgr.NetTransition();

        //need to wait for all nodes to be registered before setting dummy DHCP since
        //don't know which nodes may be DHCP either from DHCP directly or as a configured facade
        //make sure that above in initialization that server mappings are not yet retrieved
        //KW can we optimize this to be called earlier?
        //simply register a dummy DHCP node to let the daemon know that DHCP has been checked
        //note this really doesn't get registered
        pldapName = dsBuildListFromStringsPriv((char *)"LDAPv3", "DHCPChecked", nil);
        if (pldapName != nil)
        {
            CServerPlugin::_RegisterNode( fPlugInSignature, pldapName, kDHCPLDAPv3NodeType );
            dsDataListDeallocatePriv( pldapName);
            free(pldapName);
            pldapName = nil;
        }
        
		// set the initialization flags
		if (!(fState & kInitialized))
		{
			fState += kInitialized;
		}
		if (fState & kFailedToInit)
		{
			fState -= kFailedToInit;
		}
        
        WakeUpRequests();

		// now that we are done, let's notify the SearchPlugin or anyone else that we have updated based on DHCP
		if( bNewDHCPConfig )
		{
			CFStringRef service = CFStringCreateWithCString( NULL, "DirectoryService", kCFStringEncodingUTF8 );
			
			if( service )
			{
				SCDynamicStoreRef   store = SCDynamicStoreCreate(NULL, service, NULL, NULL);
				
				if (store != NULL)
				{   // we don't have to change it we can just cause a notify....
					CFStringRef notify = CFStringCreateWithCString( NULL, "com.apple.DirectoryService.NotifyTypeStandard:DHCP_LDAP_CHANGE", kCFStringEncodingUTF8 );
					
					if( notify ) {
						SCDynamicStoreNotifyValue( store, notify );
						CFRelease( notify );
					}
					CFRelease( store );
				}
				CFRelease( service );
			}
		}
		
	} // try
	catch ( sInt32 err )
	{
		siResult = err;
		// set the inactive and failedtoinit flags
		fState = kUnknownState;
		fState += kFailedToInit;
		fState += kInactive;
	}

	fCheckInitFlag.Wait();
	fInitFlag = false;
	fCheckInitFlag.Signal();
   
	return( siResult );

} // Initialize


//--------------------------------------------------------------------------------------------------
//	* ParseNextDHCPLDAPServerString()
//
//--------------------------------------------------------------------------------------------------

bool CLDAPv3Plugin::ParseNextDHCPLDAPServerString ( char **inServer, char **inSearchBase, int *inPortNumber, bool *inIsSSL, int inServerIndex )
{
	bool	foundNext		= false;
	char   *tmpBuff			= nil;
	char   *aString			= nil;
	uInt32	callocLength	= 0;
	char   *aToken			= nil;
	int		aIndex			= 1;
	uInt32	strLength		= 0;
	char   *ptr				= nil;
	
	if (fDHCPLDAPServersString == NULL)
	{
		return(foundNext);
	}
	
	//parse the fDHCPLDAPServersString string
	callocLength = (uInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(fDHCPLDAPServersString), kCFStringEncodingUTF8) + 1;
	tmpBuff = (char *) calloc(1, callocLength);
	CFStringGetCString( fDHCPLDAPServersString, tmpBuff, callocLength, kCFStringEncodingUTF8 );
	
	//KW might want to add code to skip leading white space?
	aString = tmpBuff;
	aToken = strsep(&aString,LDAPURLOPT95SEPCHAR);
	if (inServerIndex > 1)
	{
		aIndex = inServerIndex;
		while (aIndex != 1)
		{
			aToken = strsep(&aString, LDAPURLOPT95SEPCHAR);
			aIndex--;
		}
	}
	
	if (aToken != nil)
	{
		*inServer		= nil;
		*inSearchBase	= nil;
		
		if ( strncmp(aToken,"ldap://",7) == 0 )
		{
			*inPortNumber	= 389;
			*inIsSSL		= false;
			aToken			= aToken + 7;
		}
		else if ( strncmp(aToken,"ldaps://",8) == 0 )
		{
			*inPortNumber	= 636;
			*inIsSSL		= true;
			aToken			= aToken + 8;
		}
		else
		{
			syslog(LOG_ALERT,"DSLDAPv3PlugIn: DHCP option 95 error since obtained [%s] LDAP server prefix is not of the correct format.", aToken);
			if ( tmpBuff != nil )
			{
				free(tmpBuff);
				tmpBuff = nil;
			}
			return(foundNext);
		}
		
		ptr				= aToken;
		strLength		= 0;
		while ( (*ptr != '\0') && (*ptr != ':') && (*ptr != '/') )
		{
			strLength++;
			ptr++;
		}
		if (strLength != 0)
		{
			*inServer	= (char *) calloc(1, strLength+1);
			strncpy(*inServer, aToken, strLength);
			aToken		= aToken + strLength;
			foundNext	= true;
		}
		else
		{
			syslog(LOG_ALERT,"DSLDAPv3PlugIn: DHCP option 95 error since can't extract LDAP server name from URL.");
			if ( tmpBuff != nil )
			{
				free(tmpBuff);
				tmpBuff = nil;
			}
			return(foundNext);
		}
		
		if (*ptr == ':')
		{
			ptr++;
			aToken++;
			strLength	= 0;
			while ( (*ptr != '\0') && (*ptr != '/') )
			{
				strLength++;
				ptr++;
			}
			if (strLength != 0)
			{
				char *portStr	= nil;
				portStr			= (char *) calloc(1, strLength+1);
				strncpy(portStr, aToken, strLength);
				*inPortNumber	= atoi(portStr);
				free(portStr);
				portStr			= nil;
				aToken			= aToken + strLength;
			}
		}
			
		if (*ptr == '/')
		{
			ptr++;
			aToken++;
			strLength	= 0;
			while ( (*ptr != '\0') && (*ptr != '?') )
			{
				strLength++;
				ptr++;
			}
			if (strLength != 0)
			{
				*inSearchBase	= (char *) calloc(1, strLength+1);
				strncpy(*inSearchBase, aToken, strLength);
				//let's deal with embedded spaces here ie. %20 versus ' '
				ptr = *inSearchBase;
				char *BasePtr = ptr;
				while (*ptr !=  '\0')
				{
					if (*ptr == '%')
					{
						ptr++;
						if (*ptr == '2')
						{
							ptr++;
							if (*ptr == '0')
							{
								*ptr = ' ';
							}
						}
					}
					*BasePtr = *ptr;
					ptr++;
					BasePtr++;
				}
				*BasePtr = *ptr; //add NULL to end of string with replaced spaces
			}
		}
	} // if (aToken != nil)

	if ( tmpBuff != nil )
	{
		free(tmpBuff);
		tmpBuff = nil;
	}
	
	return(foundNext);

} // ParseNextDHCPLDAPServerString


// --------------------------------------------------------------------------------
//	* SetPluginState ()
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::SetPluginState ( const uInt32 inState )
{

    tDataList		   *pldapName	= nil;
    sLDAPConfigData	   *pConfig		= nil;

// don't allow any changes other than active / in-active

	if (inState & kActive) //want to set to active
	{
		if ( (fState & kInactive) && (!(fState & kActive)) )
		{
			//call to initialize again since we need to re-register the nodes and discover DHCP LDAP
			if (bDoNotInitTwiceAtStartUp)
			{
				bDoNotInitTwiceAtStartUp = false;
			}
			else
			{
				Initialize();
			}
			fState += kActive;
			fState -= kInactive;
		}
	}
	if (inState & kInactive) //want to set to in-active
	{
		if ( (fState & kActive) && (!(fState & kInactive)) )
		{
			gpConfigFromXML->ConfigMapWait();   // need to lock here since we are reinitializing an existing config.

			LDAPConfigDataMap configMap = gpConfigFromXML->GetConfigMap();
			
			LDAPConfigDataMapI configIter = configMap.begin();
			
			while( configIter != configMap.end() )
			{
				pConfig = configIter->second;
				if (pConfig != nil)
				{
					if (pConfig->fNodeName != nil)
					{
						//UN register the node
						//but don't remove it from the config table
						
						//add standard LDAPv3 prefix to the registered node names here
						pldapName = dsBuildListFromStringsPriv((char *)"LDAPv3", pConfig->fNodeName, nil);
						if (pldapName != nil)
						{
							//KW what happens when the same node is unregistered twice???
							CServerPlugin::_UnregisterNode( fPlugInSignature, pldapName );
							dsDataListDeallocatePriv( pldapName);
							free(pldapName);
							pldapName = nil;
						}
					} //if servername defined
				} // pConfig != nil
				
				configIter++;
			} // loop over the LDAP config entries

            fState += kInactive;
            fState -= kActive;

			gpConfigFromXML->ConfigMapSignal();
		}
    }

	return( eDSNoErr );

} // SetPluginState


//--------------------------------------------------------------------------------------------------
//	* WakeUpRequests() (static)
//
//--------------------------------------------------------------------------------------------------

void CLDAPv3Plugin::WakeUpRequests ( void )
{
	gKickLDAPv3Requests->Signal();
} // WakeUpRequests


// ---------------------------------------------------------------------------
//	* WaitForInit
//
// ---------------------------------------------------------------------------

void CLDAPv3Plugin::WaitForInit ( void )
{
	volatile	uInt32		uiAttempts	= 0;

	if (!(fState & kActive))
	{
		while ( !(fState & kInitialized) &&
				!(fState & kFailedToInit) )
		{
			// Try for 2 minutes before giving up
			if ( uiAttempts++ >= 240 )
			{
				return;
			}
	
			// Now wait until we are told that there is work to do or
			//	we wake up on our own and we will look for ourselves
	
			gKickLDAPv3Requests->Wait( (uInt32)(.5 * kMilliSecsPerSec) );
	
			gKickLDAPv3Requests->Reset();
		}
	}//NOT already Active
} // WaitForInit


// ---------------------------------------------------------------------------
//	* ProcessRequest
//
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::ProcessRequest ( void *inData )
{
	sInt32		siResult	= 0;
	char	   *pathStr		= nil;

	if ( inData == nil )
	{
		return( ePlugInDataError );
	}
    
	if (((sHeader *)inData)->fType == kOpenDirNode)
	{
		if (((sOpenDirNode *)inData)->fInDirNodeName != nil)
		{
			pathStr = ::dsGetPathFromListPriv( ((sOpenDirNode *)inData)->fInDirNodeName, "/" );
			if (pathStr != nil)
			{
				if (strncmp(pathStr,"/LDAPv3",7) != 0)
				{
					free(pathStr);
					pathStr = nil;
					return(eDSOpenNodeFailed);
				}
				free(pathStr);
				pathStr = nil;
			}
		}
	}
	
	if ( ((sHeader *)inData)->fType == kServerRunLoop )
	{
		if ( (((sHeader *)inData)->fContextData) != nil )
		{
			fServerRunLoop = (CFRunLoopRef)(((sHeader *)inData)->fContextData);
			return (siResult);
		}
	}
	
	if ( ((sHeader *)inData)->fType == kKerberosMutex )
	{
		if( (((sHeader *)inData)->fContextData) != nil )
		{
			gLDAPKerberosMutex = (DSMutexSemaphore *)(((sHeader *)inData)->fContextData);
			return siResult;
		}

	}

    WaitForInit();

	if (fState == kUnknownState)
	{
		return( (sInt32)ePlugInCallTimedOut );
	}

	if ( (fState & kFailedToInit) || !(fState & kInitialized) )
	{
		return( (sInt32)ePlugInFailedToInitialize );
	}

	if ( ((fState & kInactive) || !(fState & kActive))
		  && (((sHeader *)inData)->fType != kDoPlugInCustomCall)
		  && (((sHeader *)inData)->fType != kOpenDirNode) )
	{
        return( ePlugInNotActive );
	}
    
	if ( ((sHeader *)inData)->fType == kHandleNetworkTransition )
	{
		if (!gBlockLDAPNetworkChange)
		{
			HandleMultipleNetworkTransitionsForLDAP();
		}
	}
	else
	{
		siResult = HandleRequest( inData );
	}

	return( siResult );

} // ProcessRequest



// ---------------------------------------------------------------------------
//	* HandleRequest
//
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::HandleRequest ( void *inData )
{
	sInt32				siResult	= 0;
	sHeader			   *pMsgHdr		= nil;
	bool				bWaitForInit= true;
	DSSemaphore			timedWait;

	//block new requests while a re-initialization is on-going
	while(bWaitForInit)
	{
		fCheckInitFlag.Wait();
		if (fInitFlag)
		{
			fCheckInitFlag.Signal();
			//wait one tenth of a second to check again
			timedWait.Wait( (uInt32)(.1 * kMilliSecsPerSec) );
		}
		else
		{
			fCheckInitFlag.Signal();
			bWaitForInit = false;
		}
	}

	if ( inData == nil )
	{
		return( -8088 );
	}

	pMsgHdr = (sHeader *)inData;

	//use of LDAP connections will be tracked with LockSession / UnlockSession
	
	switch ( pMsgHdr->fType )
	{
		case kOpenDirNode:
			siResult = OpenDirNode( (sOpenDirNode *)inData );
			break;
			
		case kCloseDirNode:
			siResult = CloseDirNode( (sCloseDirNode *)inData );
			break;
			
		case kGetDirNodeInfo:
			siResult = GetDirNodeInfo( (sGetDirNodeInfo *)inData );
			break;
			
		case kGetRecordList:
			siResult = GetRecordList( (sGetRecordList *)inData );
			break;
			
		case kGetRecordEntry:
            siResult = GetRecordEntry( (sGetRecordEntry *)inData );
			break;
			
		case kGetAttributeEntry:
			siResult = GetAttributeEntry( (sGetAttributeEntry *)inData );
			break;
			
		case kGetAttributeValue:
			siResult = GetAttributeValue( (sGetAttributeValue *)inData );
			break;
			
		case kOpenRecord:
			siResult = OpenRecord( (sOpenRecord *)inData );
			break;
			
		case kGetRecordReferenceInfo:
			siResult = GetRecRefInfo( (sGetRecRefInfo *)inData );
			break;
			
		case kGetRecordAttributeInfo:
			siResult = GetRecAttribInfo( (sGetRecAttribInfo *)inData );
			break;
			
		case kGetRecordAttributeValueByIndex:
			siResult = GetRecAttrValueByIndex( (sGetRecordAttributeValueByIndex *)inData );
			break;
			
		case kGetRecordAttributeValueByValue:
			siResult = GetRecAttrValueByValue( (sGetRecordAttributeValueByValue *)inData );
			break;
			
		case kGetRecordAttributeValueByID:
			siResult = GetRecordAttributeValueByID( (sGetRecordAttributeValueByID *)inData );
			break;

		case kFlushRecord:
			siResult = FlushRecord( (sFlushRecord *)inData );
			break;
			
		case kCloseAttributeList:
			siResult = CloseAttributeList( (sCloseAttributeList *)inData );
			break;

		case kCloseAttributeValueList:
			siResult = CloseAttributeValueList( (sCloseAttributeValueList *)inData );
			break;

		case kCloseRecord:
			siResult = CloseRecord( (sCloseRecord *)inData );
			break;
			
		case kReleaseContinueData:
			siResult = ReleaseContinueData( (sReleaseContinueData *)inData );
			break;

		case kSetRecordName:
			siResult = SetRecordName( (sSetRecordName *)inData );
			break;
			
		case kSetRecordType:
			siResult = eNotYetImplemented; //KW not to be implemented
			break;
			
		case kDeleteRecord:
			siResult = DeleteRecord( (sDeleteRecord *)inData );
			break;

		case kCreateRecord:
		case kCreateRecordAndOpen:
			siResult = CreateRecord( (sCreateRecord *)inData );
			break;

		case kAddAttribute:
			siResult = AddAttribute( (sAddAttribute *)inData );
			break;

		case kRemoveAttribute:
			siResult = RemoveAttribute( (sRemoveAttribute *)inData );
			break;
			
		case kAddAttributeValue:
			siResult = AddAttributeValue( (sAddAttributeValue *)inData );
			break;
			
		case kRemoveAttributeValue:
			siResult = RemoveAttributeValue( (sRemoveAttributeValue *)inData );
			break;
			
		case kSetAttributeValue:
			siResult = SetAttributeValue( (sSetAttributeValue *)inData );
			break;
			
		case kSetAttributeValues:
			siResult = SetAttributeValues( (sSetAttributeValues *)inData );
			break;
			
		case kDoDirNodeAuth:
			siResult = DoAuthentication( (sDoDirNodeAuth *)inData );
			break;
			
		case kDoDirNodeAuthOnRecordType:
			if (	( ((sDoDirNodeAuthOnRecordType *)inData)->fInRecordType != NULL) &&
					( ((sDoDirNodeAuthOnRecordType *)inData)->fInRecordType->fBufferData != NULL) )
					{
						siResult = DoAuthenticationOnRecordType(	(sDoDirNodeAuthOnRecordType *)inData,
																	((sDoDirNodeAuthOnRecordType *)inData)->fInRecordType->fBufferData );
					}
			break;
			
		case kDoAttributeValueSearch:
		case kDoAttributeValueSearchWithData:
			siResult = DoAttributeValueSearch( (sDoAttrValueSearchWithData *)inData );
			break;

		case kDoMultipleAttributeValueSearch:
		case kDoMultipleAttributeValueSearchWithData:
			siResult = DoMultipleAttributeValueSearch( (sDoMultiAttrValueSearchWithData *)inData );
			break;

		case kDoPlugInCustomCall:
			siResult = DoPlugInCustomCall( (sDoPlugInCustomCall *)inData );
			break;
		
		case kHandleSystemWillSleep:
			siResult = eDSNoErr;
			fLDAPSessionMgr.SystemGoingToSleep();
			break;

		case kHandleSystemWillPowerOn:
			siResult = eDSNoErr;
			fLDAPSessionMgr.SystemWillPowerOn();
			break;

		default:
			siResult = eNotHandledByThisNode;
			break;
	}

	pMsgHdr->fResult = siResult;
	return( siResult );

} // HandleRequest

//------------------------------------------------------------------------------------
//	* OpenDirNode
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::OpenDirNode ( sOpenDirNode *inData )
{
    char			   *ldapName		= nil;
	char			   *pathStr			= nil;
    char			   *subStr			= nil;
	sLDAPContextData	   *pContext		= nil;
	sInt32				siResult		= eDSNoErr;
    tDataListPtr		pNodeList		= nil;

    pNodeList	=	inData->fInDirNodeName;
    
	try
	{
		if ( inData != nil )
		{
			pathStr = dsGetPathFromListPriv( pNodeList, (char *)"/" );
			if ( pathStr == nil ) throw( (sInt32)eDSNullNodeName );

			//special case for the configure LDAPv3 node
			if (::strcmp(pathStr,"/LDAPv3") == 0)
			{
				// set up the context data now with the relevant parameters for the configure LDAPv3 node
				// DS API reference number is used to access the reference table
				pContext = new sLDAPContextData;
				pContext->fNodeName = strdup("LDAPv3 Configure");
				pContext->fLDAPNodeStruct = new sLDAPNodeStruct;
				pContext->fUID = inData->fInUID;
				pContext->fEffectiveUID = inData->fInEffectiveUID;
				
				//generic hash index
				// add the item to the reference table
				gLDAPContextTable->AddItem( inData->fOutNodeRef, pContext );
			}
			// check that there is something after the delimiter or prefix
			// strip off the LDAPv3 prefix here
			else if ( (strlen(pathStr) > 8) && (::strncmp(pathStr,"/LDAPv3/",8) == 0) )
			{
				subStr = pathStr + 8;

				ldapName = strdup( subStr );
				if ( ldapName == nil ) throw( (sInt32)eDSNullNodeName );
				
				::strcpy(ldapName,subStr);
				pContext = new sLDAPContextData;
				pContext->fNodeName = ldapName;
				pContext->fUID = inData->fInUID;
				pContext->fEffectiveUID = inData->fInEffectiveUID;
				
				siResult = fLDAPSessionMgr.SafeOpen( (char *)ldapName, &(pContext->fLDAPNodeStruct) );
				if ( siResult != eDSNoErr)
				{
					delete pContext;
					pContext = nil;
					throw( (sInt32)eDSOpenNodeFailed );
				}
				
				//set the bLDAPv2ReadOnly flag in the context
				if (gpConfigFromXML != nil)
				{
					//retrieve the config data
					sLDAPConfigData *pConfig = gpConfigFromXML->ConfigWithNodeNameLock( ldapName );
					if (pConfig != nil)
					{
						pContext->bLDAPv2ReadOnly = pConfig->bLDAPv2ReadOnly;

						gpConfigFromXML->ConfigUnlock( pConfig );
						pConfig = NULL;
					}
				}
				
				// add the item to the reference table
				gLDAPContextTable->AddItem( inData->fOutNodeRef, pContext );
			} // there was some name passed in here ie. length > 1
			else
			{
				siResult = eDSOpenNodeFailed;
			}
		} // inData != nil
		else
		{
			siResult = eDSNullParameter;
		}
	} // try
	catch ( sInt32 err )
	{
		siResult = err;
		if (pContext != nil)
		{
			gLDAPContextTable->RemoveItem( inData->fOutNodeRef );
		}
	}
	
	if (pathStr != nil)
	{
		delete( pathStr );
		pathStr = nil;
	}

	return( siResult );

} // OpenDirNode

//------------------------------------------------------------------------------------
//	* CloseDirNode
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::CloseDirNode ( sCloseDirNode *inData )
{
	sInt32				siResult	= eDSNoErr;
	sLDAPContextData   *pContext	= nil;
	sLDAPNodeStruct    *pNodeStruct = nil;

	try
	{
		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		// we need to special case this one so we delete the node struct too
		if( strcmp(pContext->fNodeName, "LDAPv3 Configure") == 0 )
		{
			pNodeStruct = pContext->fLDAPNodeStruct;
		}
	
		// our last chance to clean up anything we missed for that node 
		gLDAPContinueTable->RemoveItems( inData->fInNodeRef ); // clean up continues before we remove the first context itself..

		gLDAPContextTable->RemoveItem( inData->fInNodeRef );

		// we have to clean this up ourselves if we were the configure node
		DSDelete( pNodeStruct );
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // CloseDirNode


//------------------------------------------------------------------------------------
//	* GetRecordList
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetRecordList ( sGetRecordList *inData )
{
    sInt32					siResult			= eDSNoErr;
    uInt32					uiTotal				= 0;
    uInt32					uiCount				= 0;
    char				   *pRecType			= nil;
    char				   *pLDAPRecType		= nil;
    bool					bAttribOnly			= false;
    tDirPatternMatch		pattMatch			= eDSNoMatch1;
    char				  **pRecNames			= nil;
    CAttributeList		   *cpRecTypeList		= nil;
    CAttributeList		   *cpAttrTypeList 		= nil;
    sLDAPContextData	   *pContext			= nil;
    sLDAPContinueData	   *pLDAPContinue		= nil;
    CBuff				   *outBuff				= nil;
    int						numRecTypes			= 1;
    bool					bBuffFull			= false;
    bool					separateRecTypes	= false;
    uInt32					countDownRecTypes	= 0;
	bool					bOCANDGroup			= false;
	CFArrayRef				OCSearchList		= nil;
	ber_int_t				scope				= LDAP_SCOPE_SUBTREE;

    try
    {
        // Verify all the parameters
        if ( inData  == nil ) throw( (sInt32) eMemoryError );
        if ( inData->fInDataBuff  == nil ) throw( (sInt32)eDSEmptyBuffer );
        if (inData->fInDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

        if ( inData->fInRecNameList  == nil ) throw( (sInt32)eDSEmptyRecordNameList );
        if ( inData->fInRecTypeList  == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
        if ( inData->fInAttribTypeList  == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );

        // Node context data
        pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
        if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		if ( inData->fIOContinueData == nil )
		{
			pLDAPContinue = (sLDAPContinueData *)::calloc( 1, sizeof( sLDAPContinueData ) );

			gLDAPContinueTable->AddItem( pLDAPContinue, inData->fInNodeRef );

            //parameters used for data buffering
            pLDAPContinue->fRecNameIndex = 1;
            pLDAPContinue->fRecTypeIndex = 1;
            pLDAPContinue->fTotalRecCount = 0;
            pLDAPContinue->fLimitRecSearch = 0;
			
			//check if the client has requested a limit on the number of records to return
			//we only do this the first call into this context for pLDAPContinue
			if (inData->fOutRecEntryCount >= 0)
			{
				pLDAPContinue->fLimitRecSearch = inData->fOutRecEntryCount;
			}
		}
		else
		{
			pLDAPContinue = (sLDAPContinueData *)inData->fIOContinueData;
			if ( gLDAPContinueTable->VerifyItem( pLDAPContinue ) == false )
			{
				throw( (sInt32)eDSInvalidContinueData );
			}
		}

        // start with the continue set to nil until buffer gets full and there is more data
        //OR we have more record types to look through
        inData->fIOContinueData		= nil;
		//return zero if nothing found here
		inData->fOutRecEntryCount	= 0;

        // copy the buffer data into a more manageable form
        outBuff = new CBuff();
        if ( outBuff  == nil ) throw( (sInt32) eMemoryError );

        siResult = outBuff->Initialize( inData->fInDataBuff, true );
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = outBuff->GetBuffStatus();
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = outBuff->SetBuffType( 'StdA' );
        if ( siResult != eDSNoErr ) throw( siResult );

        // Get the attribute strings to match
		pRecNames = dsAllocStringsFromList(0, inData->fInRecNameList);
		if ( pRecNames == nil ) throw( (sInt32)eDSEmptyRecordNameList );

        // Get the record name pattern match type
        pattMatch = inData->fInPatternMatch;

        // Get the record type list
        // Record type mapping for LDAP to DS API is dealt with below
        cpRecTypeList = new CAttributeList( inData->fInRecTypeList );
        if ( cpRecTypeList  == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
        //save the number of rec types here to use in separating the buffer data
        countDownRecTypes = cpRecTypeList->GetCount() - pLDAPContinue->fRecTypeIndex + 1;
        if (cpRecTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyRecordTypeList );

        // Get the attribute list
        //KW? at this time would like to simply dump all attributes
        //would expect to do this always since this is where the databuffer is built
        cpAttrTypeList = new CAttributeList( inData->fInAttribTypeList );
        if ( cpAttrTypeList  == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
        if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );

        // Get the attribute info only flag
        bAttribOnly = inData->fInAttribInfoOnly;

        // get records of these types
        while ((( cpRecTypeList->GetAttribute( pLDAPContinue->fRecTypeIndex, &pRecType ) == eDSNoErr ) && (!bBuffFull)) && (!separateRecTypes))
        {
        	//mapping rec types - if std to native
        	numRecTypes = 1;
			bOCANDGroup = false;
			if (OCSearchList != nil)
			{
				CFRelease(OCSearchList);
				OCSearchList = nil;
			}
            pLDAPRecType = MapRecToLDAPType( (const char *)pRecType, pContext->fNodeName, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
            //throw on first nil only if last passed in record type
            if ( ( pLDAPRecType == nil ) && (countDownRecTypes == 1) )
				throw( (sInt32)eDSInvalidRecordType );
            while (( pLDAPRecType != nil ) && (!bBuffFull))
            {
				bBuffFull = false;
				if ( ::strcmp( pRecNames[0], kDSRecordsAll ) == 0 ) //should be only a single value of kDSRecordsAll in this case
				{
					siResult = GetAllRecords( pRecType, pLDAPRecType, cpAttrTypeList, pContext, pLDAPContinue, bAttribOnly, outBuff, uiCount, bOCANDGroup, OCSearchList, scope );
				}
				else
				{
					siResult = GetTheseRecords( (char *)kDSNAttrRecordName, pRecNames, pRecType, pLDAPRecType, pattMatch, cpAttrTypeList, pContext, pLDAPContinue, bAttribOnly, outBuff, uiCount, bOCANDGroup, OCSearchList, scope );
				}

				//outBuff->GetDataBlockCount( &uiCount );
				//cannot use this as there may be records added from different record names
				//at which point the first name will fill the buffer with n records and
				//uiCount is reported as n but as the second name fills the buffer with m MORE records
				//the statement above will return the total of n+m and add it to the previous n
				//so that the total erroneously becomes 2n+m and not what it should be as n+m
				//therefore uiCount is extracted directly out of the GetxxxRecord(s) calls

				if ( siResult == CBuff::kBuffFull )
				{
					bBuffFull = true;
					//set continue if there is more data available
					inData->fIOContinueData = pLDAPContinue;
					
					// check to see if buffer is empty and no entries added
					// which implies that the buffer is too small
					if ( ( uiCount == 0 ) && ( uiTotal == 0 ) )
					{
						throw( (sInt32)eDSBufferTooSmall );
					}

					uiTotal += uiCount;
					inData->fOutRecEntryCount = uiTotal;
					outBuff->SetLengthToSize();
					siResult = eDSNoErr;
				}
				else if ( siResult == eDSNoErr )
				{
					uiTotal += uiCount;
					pLDAPContinue->fRecNameIndex++;
					pLDAPContinue->msgId = 0;
				}

                if (pLDAPRecType != nil)
                {
                    delete( pLDAPRecType );
                    pLDAPRecType = nil;
                }
                
                if (!bBuffFull)
                {
	                numRecTypes++;
					bOCANDGroup = false;
					if (OCSearchList != nil)
					{
						CFRelease(OCSearchList);
						OCSearchList = nil;
					}
					pLDAPContinue->fRecNameIndex = 1;
	                //get the next mapping
	                pLDAPRecType = MapRecToLDAPType( (const char *)pRecType, pContext->fNodeName, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
                }
                
            } // while mapped Rec Type != nil
            
            if (!bBuffFull)
            {
	            pRecType = nil;
	            pLDAPContinue->fRecTypeIndex++;
	            pLDAPContinue->fRecNameIndex = 1;
	            //reset the LDAP message ID to zero since now going to go after a new type
	            pLDAPContinue->msgId = 0;
	            
	            //KW? here we decide to exit with data full of the current type of records
	            // and force a good exit with the data we have so we can come back for the next rec type
            	separateRecTypes = true;
                //set continue since there may be more data available
                inData->fIOContinueData = pLDAPContinue;
				siResult = eDSNoErr;
				
				//however if this was the last rec type then there will be no more data
	            // check the number of rec types left
	            countDownRecTypes--;
	            if (countDownRecTypes == 0)
	            {
	                inData->fIOContinueData = nil;
				}
            }
            
        } // while loop over record types

        if (( siResult == eDSNoErr ) & (!bBuffFull))
        {
            if ( uiTotal == 0 )
            {
                outBuff->ClearBuff();
            }
            else
            {
                outBuff->SetLengthToSize();
            }

            inData->fOutRecEntryCount = uiTotal;
        }
    } // try
    
    catch ( sInt32 err )
    {
		siResult = err;
    }

	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	
	if ( (inData->fIOContinueData == nil) && (pLDAPContinue != nil) )
	{
		// we've decided not to return continue data, so we should clean up
		gLDAPContinueTable->RemoveItem( pLDAPContinue );
		pLDAPContinue = nil;
	}

	if (pLDAPRecType != nil)
	{
		delete( pLDAPRecType );
		pLDAPRecType = nil;
	}

	if ( cpRecTypeList != nil )
	{
		delete( cpRecTypeList );
		cpRecTypeList = nil;
	}

	if ( cpAttrTypeList != nil )
	{
		delete( cpAttrTypeList );
		cpAttrTypeList = nil;
	}

	if ( outBuff != nil )
	{
		delete( outBuff );
		outBuff = nil;
	}

	if (pRecNames != nil)
	{
		uInt32 strCnt = 0;
		while(pRecNames[strCnt] != nil)
		{
			free(pRecNames[strCnt]);
			pRecNames[strCnt] = nil;
			strCnt++;
		}
		free(pRecNames);
		pRecNames = nil;
	}

    return( siResult );

} // GetRecordList


// ---------------------------------------------------------------------------
//	* MapRecToLDAPType
// ---------------------------------------------------------------------------

char* CLDAPv3Plugin::MapRecToLDAPType ( const char *inRecType, char *inConfigName, int inIndex, bool *outOCGroup, CFArrayRef *outOCListCFArray, ber_int_t *outScope )
{
	char				   *outResult	= nil;
	uInt32					uiStrLen	= 0;
	uInt32					uiNativeLen	= ::strlen( kDSNativeRecordTypePrefix );
	uInt32					uiStdLen	= ::strlen( kDSStdRecordTypePrefix );
	
	//idea here is to use the inIndex to request a specific map
	//if inIndex is 1 then the first map will be returned
	//if inIndex is >= 1 and <= totalCount then that map will be returned
	//if inIndex is <= 0 then nil will be returned
	//if inIndex is > totalCount nil will be returned
	//note the inIndex will reference the inIndexth entry ie. start at 1
	//caller can increment the inIndex starting at one and get maps until nil is returned
	
	if ( ( inRecType != nil ) && (inIndex > 0) )
	{
		uiStrLen = ::strlen( inRecType );

		// First look for native record type, ensure it is long enough to compare
		if ( uiStrLen >= uiNativeLen && ::strncmp( inRecType, kDSNativeRecordTypePrefix, uiNativeLen ) == 0 )
		{
			// Make sure we have data past the prefix
			if ( ( uiStrLen > uiNativeLen ) && (inIndex == 1) )
			{
				uiStrLen = uiStrLen - uiNativeLen;
				outResult = (char *) calloc(1, uiStrLen + 2);
				::strcpy( outResult, inRecType + uiNativeLen );
				DBGLOG1( kLogPlugin, "CLDAPv3Plugin: MapRecToLDAPType:: Warning:Native record type <%s> is being used", outResult );
			}
		}//native maps
		//now deal with the standard mappings
		else if ( uiStrLen >= uiStdLen && ::strncmp( inRecType, kDSStdRecordTypePrefix, uiStdLen ) == 0 )
		{
			sLDAPConfigData *pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inConfigName );
			if (pConfig != nil)
			{
				if ( pConfig->fRecordAttrMapDict )
				{
					outResult = gpConfigFromXML->ExtractRecMap(inRecType, pConfig->fRecordAttrMapDict, inIndex, outOCGroup, outOCListCFArray, outScope );
				}
				else
				{
					//TODO need to "try" to get a default here if no mappings
				}
				gpConfigFromXML->ConfigUnlock( pConfig );
			}

		}//standard maps
		//passthrough since we don't know what to do with it
		//and we assume the DS API client knows that the LDAP server
		//can handle this record type
		else if( inIndex == 1 )
		{
			outResult = (char *) calloc( 1, 1 + ::strlen( inRecType ) );
			::strcpy( outResult, inRecType );
			DBGLOG1( kLogPlugin, "CLDAPv3Plugin: MapRecToLDAPType:: Warning:Native record type with no provided prefix <%s> is being used", outResult );
		}//passthrough map
		
	}// ( ( inRecType != nil ) && (inIndex > 0) )

	return( outResult );

} // MapRecToLDAPType

//------------------------------------------------------------------------------------
//	* GetAllRecords
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetAllRecords (	char			   *inRecType,
										char			   *inNativeRecType,
										CAttributeList	   *inAttrTypeList,
										sLDAPContextData   *inContext,
										sLDAPContinueData  *inContinue,
										bool				inAttrOnly,
										CBuff			   *inBuff,
										uInt32			   &outRecCount,
										bool				inbOCANDGroup,
										CFArrayRef			inOCSearchList,
										ber_int_t			inScope )
{
    sInt32				siResult		= eDSNoErr;
    sInt32				siValCnt		= 0;
    int					ldapReturnCode 	= 0;
    bool				bufferFull		= false;
    LDAPMessage		   *result			= nil;
    char			   *recName			= nil;
    sLDAPConfigData	   *pConfig			= nil;
    int					searchTO		= 0;
	CDataBuff		   *aRecData		= nil;
	CDataBuff		   *aAttrData		= nil;
    char			   *queryFilter		= nil;


	//TODO why not optimize and save the queryFilter in the continue data for the next call in?
	//not that easy over continue data
	
	//build the record query string
	queryFilter = BuildLDAPQueryFilter(	nil,
										nil,
										eDSAnyMatch,
										inContext->fNodeName,
										false,
										(const char *)inRecType,
										inNativeRecType,
										inbOCANDGroup,
										inOCSearchList );
	    
	outRecCount = 0; //need to track how many records were found by this call to GetAllRecords
	
    try
    {
    	if (inContext == nil ) throw( (sInt32)eDSInvalidContext);
    	if (inContinue == nil ) throw( (sInt32)eDSInvalidContinueData);
    	
    	// check to make sure the queryFilter is not nil
    	if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );
    	
		aRecData = new CDataBuff();
		if ( aRecData  == nil ) throw( (sInt32) eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData  == nil ) throw( (sInt32) eMemoryError );
		
		//retrieve the config data
		pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inContext->fNodeName );
		if (pConfig != nil)
		{
			searchTO	= pConfig->fSearchTimeout;

			gpConfigFromXML->ConfigUnlock( pConfig );
			pConfig = NULL;
		}
		
		//LDAP search with rebind capability
		DSDoSearch (	inRecType,
						inNativeRecType,
						inAttrTypeList,
						inContext,
						inContinue,
						inScope,
						fLDAPSessionMgr,
						queryFilter,
						ldapReturnCode,
						searchTO,
						result);

		while ( 	( (ldapReturnCode == LDAP_RES_SEARCH_ENTRY) || (ldapReturnCode == LDAP_RES_SEARCH_RESULT) )
					&& !(bufferFull)
					&& (	(inContinue->fTotalRecCount < inContinue->fLimitRecSearch) ||
							(inContinue->fLimitRecSearch == 0) ) )
        {
			if (ldapReturnCode == LDAP_RES_SEARCH_ENTRY)
			{
				// package the record into the DS format into the buffer
				// steps to add an entry record to the buffer
				// build the fRecData header
				// build the fAttrData
				// append the fAttrData to the fRecData
				// add the fRecData to the buffer inBuff
	
				aAttrData->Clear();
				aRecData->Clear();
	
				if ( inRecType != nil )
				{
					aRecData->AppendShort( ::strlen( inRecType ) );
					aRecData->AppendString( inRecType );
				} // what to do if the inRecType is nil? - never get here then
				else
				{
					aRecData->AppendShort( ::strlen( "Record Type Unknown" ) );
					aRecData->AppendString( "Record Type Unknown" );
				}
	
				// need to get the record name
				recName = GetRecordName( inRecType, result, inContext, siResult );
				if ( siResult != eDSNoErr ) throw( siResult );
				if ( recName != nil )
				{
					aRecData->AppendShort( ::strlen( recName ) );
					aRecData->AppendString( recName );
	
					delete ( recName );
					recName = nil;
				}
				else
				{
					aRecData->AppendShort( ::strlen( "Record Name Unknown" ) );
					aRecData->AppendString( "Record Name Unknown" );
				}
	
				// need to calculate the number of attribute types ie. siValCnt
				// also need to extract the attributes and place them into fAttrData
				//siValCnt = 0;
	
				aAttrData->Clear();
				siResult = GetTheseAttributes( inRecType, inAttrTypeList, result, inAttrOnly, inContext, siValCnt, aAttrData );
				if ( siResult != eDSNoErr ) throw( siResult );
				
				//add the attribute info to the fRecData
				if ( siValCnt == 0 )
				{
					// Attribute count
					aRecData->AppendShort( 0 );
				}
				else
				{
					// Attribute count
					aRecData->AppendShort( siValCnt );
					aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
				}
	
				// add the fRecData now to the inBuff
				siResult = inBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
			}
            // need to check if the buffer is full
			// need to handle full buffer and keep the result alive for the next call in
            if (siResult == CBuff::kBuffFull)
            {
                bufferFull = true;
                
                //save the result if buffer is full
                inContinue->pResult = result;
            }
            else if ( siResult == eDSNoErr )
            {
                ldap_msgfree( result );
				result = nil;
                
				if (ldapReturnCode == LDAP_RES_SEARCH_ENTRY)
				{
					outRecCount++; //another record added
					inContinue->fTotalRecCount++;
				}
				
                //make sure no result is carried in the context
                inContinue->pResult = nil;

				//only get next result if buffer is not full
				DSGetSearchLDAPResult (	fLDAPSessionMgr,
										inContext,
										searchTO,
										result,
										LDAP_MSG_ONE,
										inContinue->msgId,
										ldapReturnCode,
										false);
            }
            else
            {
                //make sure no result is carried in the context
                inContinue->pResult = nil;
                throw( (sInt32)eDSInvalidBuffFormat );
            }

        } // while loop over entries

		if ( (result != inContinue->pResult) && (result != nil) )
		{
			ldap_msgfree( result );
			result = nil;
		}

		//here we attempt to cleanup the response list of responses that have already been deleted
		//if siResult is eDSNoErr then the msgId will be removed from inContinue in the calling routine GetRecordList
		if ( (siResult == eDSNoErr) && (inContinue != NULL) && (inContinue->msgId != 0) )
		{
			LDAP *aHost = nil;
			aHost = fLDAPSessionMgr.LockSession(inContext);
			if (aHost != nil)
			{
				DSSearchCleanUp(aHost, inContinue->msgId);
			}
			fLDAPSessionMgr.UnLockSession(inContext);
		}

    } // try block

    catch ( sInt32 err )
    {
        siResult = err;
    }

	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}

	if (aRecData != nil)
	{
		delete (aRecData);
		aRecData = nil;
	}
	if (aAttrData != nil)
	{
		delete (aAttrData);
		aAttrData = nil;
	}

    return( siResult );

} // GetAllRecords


//------------------------------------------------------------------------------------
//	* GetRecordEntry
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetRecordEntry ( sGetRecordEntry *inData )
{
    sInt32					siResult		= eDSNoErr;
    uInt32					uiIndex			= 0;
    uInt32					uiCount			= 0;
    uInt32					uiOffset		= 0;
	uInt32					uberOffset		= 0;
    char 				   *pData			= nil;
    tRecordEntryPtr			pRecEntry		= nil;
    sLDAPContextData		   *pContext		= nil;
    CBuff					inBuff;
	uInt32					offset			= 0;
	uInt16					usTypeLen		= 0;
	char				   *pRecType		= nil;
	uInt16					usNameLen		= 0;
	char				   *pRecName		= nil;
	uInt16					usAttrCnt		= 0;
	uInt32					buffLen			= 0;

    try
    {
        if ( inData  == nil ) throw( (sInt32) eMemoryError );
        if ( inData->fInOutDataBuff  == nil ) throw( (sInt32)eDSEmptyBuffer );
        if (inData->fInOutDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

        siResult = inBuff.Initialize( inData->fInOutDataBuff );
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = inBuff.GetDataBlockCount( &uiCount );
        if ( siResult != eDSNoErr ) throw( siResult );

        uiIndex = inData->fInRecEntryIndex;
        if ((uiIndex > uiCount) || (uiIndex == 0)) throw( (sInt32)eDSInvalidIndex );

        pData = inBuff.GetDataBlock( uiIndex, &uberOffset );
        if ( pData  == nil ) throw( (sInt32)eDSCorruptBuffer );

		//assume that the length retrieved is valid
		buffLen = inBuff.GetDataBlockLength( uiIndex );
		
		// Skip past this same record length obtained from GetDataBlockLength
		pData	+= 4;
		offset	= 0; //buffLen does not include first four bytes

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the record type
		::memcpy( &usTypeLen, pData, 2 );
		
		pData	+= 2;
		offset	+= 2;

		pRecType = pData;
		
		pData	+= usTypeLen;
		offset	+= usTypeLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the record name
		::memcpy( &usNameLen, pData, 2 );
		
		pData	+= 2;
		offset	+= 2;

		pRecName = pData;
		
		pData	+= usNameLen;
		offset	+= usNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the attribute count
		::memcpy( &usAttrCnt, pData, 2 );

		pRecEntry = (tRecordEntry *)::calloc( 1, sizeof( tRecordEntry ) + usNameLen + usTypeLen + 4 + kBuffPad );

		pRecEntry->fRecordNameAndType.fBufferSize	= usNameLen + usTypeLen + 4 + kBuffPad;
		pRecEntry->fRecordNameAndType.fBufferLength	= usNameLen + usTypeLen + 4;

		// Add the record name length
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData, &usNameLen, 2 );
		uiOffset += 2;

		// Add the record name
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, pRecName, usNameLen );
		uiOffset += usNameLen;

		// Add the record type length
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, &usTypeLen, 2 );

		// Add the record type
		uiOffset += 2;
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, pRecType, usTypeLen );

		pRecEntry->fRecordAttributeCount = usAttrCnt;

        pContext = new sLDAPContextData;
        if ( pContext  == nil ) throw( (sInt32) eMemoryAllocError );

        pContext->offset = uberOffset + offset + 4; // context used by next calls of GetAttributeEntry
													// include the four bytes of the buffLen

        gLDAPContextTable->AddItem( inData->fOutAttrListRef, pContext );

        inData->fOutRecEntryPtr = pRecEntry;
    }

    catch ( sInt32 err )
    {
        siResult = err;
    }

    return( siResult );

} // GetRecordEntry


//------------------------------------------------------------------------------------
//	* GetTheseAttributes
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetTheseAttributes (	char			   *inRecType,
											CAttributeList	   *inAttrTypeList,
											LDAPMessage		   *inResult,
											bool				inAttrOnly,
											sLDAPContextData   *inContext,
											sInt32			   &outCount,
											CDataBuff		   *inDataBuff )
{
	sInt32				siResult				= eDSNoErr;
	sInt32				attrTypeIndex			= 1;
	char			   *pLDAPAttrType			= nil;
	char			   *pAttrType				= nil;
	char			   *pAttr					= nil;
	BerElement		   *ber						= nil;
	struct berval	  **bValues;
	int					numAttributes			= 1;
	int					numStdAttributes		= 1;
	bool				bTypeFound				= false;
	int					valCount				= 0;
	char			   *pStdAttrType			= nil;
	bool				bAtLeastOneTypeValid	= false;
	CDataBuff		   *aTmpData				= nil;
	CDataBuff		   *aTmp2Data				= nil;
	bool				bStripCryptPrefix		= false;
	bool				bUsePlus				= true;
	char			   *pLDAPPasswdAttrType		= nil;
	uInt32				literalLength			= 0;
	LDAP			   *aHost					= nil;

	try
	{
		if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );
		
		outCount = 0;
		aTmpData = new CDataBuff();
		if ( aTmpData  == nil ) throw( (sInt32) eMemoryError );
		aTmp2Data = new CDataBuff();
		if ( aTmp2Data  == nil ) throw( (sInt32) eMemoryError );
		inDataBuff->Clear();

		// Get the record attributes with/without the values
		while ( inAttrTypeList->GetAttribute( attrTypeIndex++, &pAttrType ) == eDSNoErr )
		{
			siResult = eDSNoErr;
			if ( ::strcmp( kDSNAttrMetaNodeLocation, pAttrType ) == 0 )
			{
			// we look at kDSNAttrMetaNodeLocation with NO mapping
			// since we have special code to deal with it and we always place the
			// node name into it
				aTmpData->Clear();
				
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( pAttrType ) );
				aTmpData->AppendString( pAttrType );

				if ( inAttrOnly == false )
				{
					// Append the attribute value count
					aTmpData->AppendShort( 1 );

					char *tmpStr = nil;

					//extract name from the context data
					//need to add prefix of /LDAPv3/ here since the fName does not contain it in the context
					if ( inContext->fNodeName != nil )
					{
		        		tmpStr = (char *) calloc(1, 1+8+::strlen(inContext->fNodeName));
		        		::strcpy( tmpStr, "/LDAPv3/" );
		        		::strcat( tmpStr, inContext->fNodeName );
					}
					else
					{
						tmpStr = (char *) calloc(1, 1+::strlen("Unknown Node Location"));
						::strcpy( tmpStr, "Unknown Node Location" );
					}

					// Append attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );
				}// if ( inAttrOnly == false )
				else
				{
					aTmpData->AppendShort( 0 );
				}

				// Add the attribute length
				outCount++;
				inDataBuff->AppendLong( aTmpData->GetLength() );
				inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();

			} // if ( ::strcmp( kDSNAttrMetaNodeLocation, pAttrType ) == 0 )
			else if ( ::strcmp( kDSNAttrRecordType, pAttrType ) == 0 )
			{
				// we simply use the input argument inRecType
				aTmpData->Clear();
				
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( pAttrType ) );
				aTmpData->AppendString( pAttrType );

				if ( inAttrOnly == false )
				{
					// Append the attribute value count
					aTmpData->AppendShort( 1 );

					// Append attribute value
					aTmpData->AppendLong( ::strlen( inRecType ) );
					aTmpData->AppendString( inRecType );

				}// if ( inAttrOnly == false )
				else
				{
					aTmpData->AppendShort( 0 );
				}

				// Add the attribute length
				outCount++;
				inDataBuff->AppendLong( aTmpData->GetLength() );
				inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();

			} // if ( ::strcmp( kDSNAttrRecordType, pAttrType ) == 0 )
			else if ((::strcmp(pAttrType,kDSAttributesAll) == 0) || (::strcmp(pAttrType,kDSAttributesStandardAll) == 0) || (::strcmp(pAttrType,kDSAttributesNativeAll) == 0))
			{
				if ((::strcmp(pAttrType,kDSAttributesAll) == 0) || (::strcmp(pAttrType,kDSAttributesStandardAll) == 0))
				{
					//attrs always added here
					//kDSNAttrMetaNodeLocation
					//kDSNAttrRecordType
					
					// we look at kDSNAttrMetaNodeLocation with NO mapping
					// since we have special code to deal with it and we always place the
					// node name into it AND we output it here since ALL or ALL Std was asked for
					aTmpData->Clear();

					// Append the attribute name
					aTmpData->AppendShort( ::strlen( kDSNAttrMetaNodeLocation ) );
					aTmpData->AppendString( kDSNAttrMetaNodeLocation );

					if ( inAttrOnly == false )
					{
						// Append the attribute value count
						aTmpData->AppendShort( 1 );

						char *tmpStr = nil;

						//extract name from the context data
						//need to add prefix of /LDAPv3/ here since the fName does not contain it in the context
						if ( inContext->fNodeName != nil )
						{
							tmpStr = (char *) calloc(1, 1+8+::strlen(inContext->fNodeName));
							::strcpy( tmpStr, "/LDAPv3/" );
							::strcat( tmpStr, inContext->fNodeName );
						}
						else
						{
							tmpStr = (char *) calloc(1, 1+::strlen("Unknown Node Location"));
							::strcpy( tmpStr, "Unknown Node Location" );
						}

						// Append attribute value
						aTmpData->AppendLong( ::strlen( tmpStr ) );
						aTmpData->AppendString( tmpStr );

						delete( tmpStr );
					}
					else
					{
						aTmpData->AppendShort( 0 );
					}
	
					// Add the attribute length
					outCount++;
					inDataBuff->AppendLong( aTmpData->GetLength() );
					inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
	
					// we simply use the input argument inRecType
					aTmpData->Clear();
					
					// Append the attribute name
					aTmpData->AppendShort( ::strlen( kDSNAttrRecordType ) );
					aTmpData->AppendString( kDSNAttrRecordType );

					if ( inAttrOnly == false )
					{
						// Append the attribute value count
						aTmpData->AppendShort( 1 );

						// Append attribute value
						aTmpData->AppendLong( ::strlen( inRecType ) );
						aTmpData->AppendString( inRecType );

					}// if ( inAttrOnly == false )
					else
					{
						aTmpData->AppendShort( 0 );
					}

					// Add the attribute length
					outCount++;
					inDataBuff->AppendLong( aTmpData->GetLength() );
					inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

					// Clear the temp block
					aTmpData->Clear();
						
					//Get the mapping for kDS1AttrPassword
					//If it exists AND it is mapped to something that exists IN LDAP then we will use it
					//otherwise we set bUsePlus and use the kDS1AttrPasswordPlus value of "*******"
					//Don't forget to strip off the {crypt} prefix from kDS1AttrPassword as well
					pLDAPPasswdAttrType = MapAttrToLDAPType( (const char *)inRecType, kDS1AttrPassword, inContext->fNodeName, 1, true );
					
					//plan is to output both standard and native attributes if request ALL ie. kDSAttributesAll
					// ie. all the attributes even if they are duplicated
					
					// std attributes go first
					numStdAttributes = 1;
					pStdAttrType = GetNextStdAttrType( inRecType, inContext->fNodeName, numStdAttributes );
					while ( pStdAttrType != nil )
					{
						//get the first mapping
						numAttributes = 1;
						pLDAPAttrType = MapAttrToLDAPType( (const char *)inRecType, pStdAttrType, inContext->fNodeName, numAttributes );
						//throw if first nil since no more will be found otherwise proceed until nil
											//can't throw since that wipes out retrieval of all the following requested attributes
						//if (pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttrListRef); //KW would like a eDSNoMappingAvailable
						
						//set the indicator to check whether at least one of the native maps
						//form given standard type is correct
						//KW need to provide client with info on failed mappings??
						bAtLeastOneTypeValid = false;
						//set indicator of multiple native to standard mappings
						bTypeFound = false;
						while ( pLDAPAttrType != nil )
						{
							if (pLDAPAttrType[0] == '#') //special case where attr is mapped to a literal string
							{
								if (!bTypeFound)
								{
									aTmp2Data->Clear();
		
									//use given type in the output NOT mapped to type
									aTmp2Data->AppendShort( ::strlen( pStdAttrType ) );
									aTmp2Data->AppendString( pStdAttrType );
									
									//set indicator so that multiple values from multiple mapped to types
									//can be added to the given type
									bTypeFound = true;
									
									//set attribute value count to zero
									valCount = 0;
		
									// Clear the temp block
									aTmpData->Clear();
								}
								
								//set the flag indicating that we got a match at least once
								bAtLeastOneTypeValid = true;
	
								char *vsReturnStr = nil;
								vsReturnStr = MappingNativeVariableSubstitution( pLDAPAttrType, inContext, inResult, siResult );
								
								if (vsReturnStr != nil)
								{
									// we found a wildcard so reset the literalLength
									literalLength = strlen(vsReturnStr) - 1;
								}
								else
								{
									//if parsing error returned then we throw an error
									if (siResult != eDSNoErr) throw (siResult);
									// no wildcard found so get length of pLDAPAttrType
									literalLength = strlen(pLDAPAttrType) - 1;
								}
									
								if ( (inAttrOnly == false) && (literalLength > 0) )
								{
									valCount++;
										
									if (vsReturnStr != nil)
									{
										// If we found a wildcard then copy it here
										aTmpData->AppendLong( literalLength );
										aTmpData->AppendBlock( vsReturnStr + 1, literalLength );
									}
									else
									{
										// Append attribute value
										aTmpData->AppendLong( literalLength );
										aTmpData->AppendBlock( pLDAPAttrType + 1, literalLength );
									}
								} // if ( (inAttrOnly == false) && (strlen(pLDAPAttrType) > 1) ) 
								if (vsReturnStr != nil)
								{
									free( vsReturnStr );
									vsReturnStr = nil;
								}
							}
							else
							{
								aHost = fLDAPSessionMgr.LockSession(inContext);
								if (aHost != nil)
								{
									for (	pAttr = ldap_first_attribute (aHost, inResult, &ber );
											pAttr != NULL; pAttr = ldap_next_attribute(aHost, inResult, ber ) )
									{
										//TODO can likely optimize use of ldap_get_values_len() below for Std types
										bStripCryptPrefix = false;
										if (::strcasecmp( pAttr, pLDAPAttrType ) == 0)
										{
											if (pLDAPPasswdAttrType != nil )
											{
												if ( ( ::strcasecmp( pAttr, pLDAPPasswdAttrType ) == 0 ) &&
													( ::strcmp( pStdAttrType, kDS1AttrPassword ) == 0 ) )
												{
													//want to remove leading "{crypt}" prefix from password if it exists
													bStripCryptPrefix = true;
													//don't need to use the "********" passwdplus
													bUsePlus = false;
													//cleanup
													free(pLDAPPasswdAttrType);
													pLDAPPasswdAttrType = nil;
												}
											}
											//set the flag indicating that we got a match at least once
											bAtLeastOneTypeValid = true;
											//note that if standard type is incorrectly mapped ie. not found here
											//then the output will not contain any data on that std type
											if (!bTypeFound)
											{
												aTmp2Data->Clear();
			
												//use given type in the output NOT mapped to type
												aTmp2Data->AppendShort( ::strlen( pStdAttrType ) );
												aTmp2Data->AppendString( pStdAttrType );
												//set indicator so that multiple values from multiple mapped to types
												//can be added to the given type
												bTypeFound = true;
												
												//set attribute value count to zero
												valCount = 0;
												
												// Clear the temp block
												aTmpData->Clear();
											}
			
											if (( inAttrOnly == false ) &&
												(( bValues = ldap_get_values_len (aHost, inResult, pAttr )) != NULL) )
											{
												if (bStripCryptPrefix)
												{
													bStripCryptPrefix = false; //only attempted once here
													// add to the number of values for this attribute
													for (int ii = 0; bValues[ii] != NULL; ii++ )
													valCount++;
													
													// for each value of the attribute
													for (int i = 0; bValues[i] != NULL; i++ )
													{
														//case insensitive compare with "crypt" string
														if ( ( bValues[i]->bv_len > 7) &&
															(strncasecmp(bValues[i]->bv_val,"{crypt}",7) == 0) )
														{
															// Append attribute value without "{crypt}" prefix
															aTmpData->AppendLong( bValues[i]->bv_len - 7 );
															aTmpData->AppendBlock( (bValues[i]->bv_val) + 7, bValues[i]->bv_len - 7 );
														}
														else
														{
															// Append attribute value
															aTmpData->AppendLong( bValues[i]->bv_len );
															aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
														}
													} // for each bValues[i]
													ldap_value_free_len(bValues);
													bValues = NULL;
												}
												else
												{
													// add to the number of values for this attribute
													for (int ii = 0; bValues[ii] != NULL; ii++ )
													valCount++;
													
													// for each value of the attribute
													for (int i = 0; bValues[i] != NULL; i++ )
													{
														// Append attribute value
														aTmpData->AppendLong( bValues[i]->bv_len );
														aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
													} // for each bValues[i]
													ldap_value_free_len(bValues);
													bValues = NULL;
												}
											} // if ( inAttrOnly == false ) && bValues = ldap_get_values_len ...
											
											if (pAttr != nil)
											{
												ldap_memfree( pAttr );
											}
											//found the right attr so go to the next one
											break;
										} // if (::strcmp( pAttr, pLDAPAttrType ) == 0) || 
										if (pAttr != nil)
										{
											ldap_memfree( pAttr );
										}
									} // for ( loop over ldap_next_attribute )
									
									if (ber != nil)
									{
										ber_free( ber, 0 );
										ber = nil;
									}
								} //aHost != nil
								fLDAPSessionMgr.UnLockSession(inContext);
							}
							
							//cleanup pLDAPAttrType if needed
							if (pLDAPAttrType != nil)
							{
								delete (pLDAPAttrType);
								pLDAPAttrType = nil;
							}
							numAttributes++;
							//get the next mapping
							pLDAPAttrType = MapAttrToLDAPType( (const char *)inRecType, pStdAttrType, inContext->fNodeName, numAttributes );				
						} // while ( pLDAPAttrType != nil )
						
						if (bAtLeastOneTypeValid)
						{
							// Append the attribute value count
							aTmp2Data->AppendShort( valCount );
							
							if (valCount > 0)
							{
								// Add the attribute values to the attribute type
								aTmp2Data->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
								valCount = 0;
							}
	
							// Add the attribute data to the attribute data buffer
							outCount++;
							inDataBuff->AppendLong( aTmp2Data->GetLength() );
							inDataBuff->AppendBlock( aTmp2Data->GetData(), aTmp2Data->GetLength() );
						}
						
						//cleanup pStdAttrType if needed
						if (pStdAttrType != nil)
						{
							delete (pStdAttrType);
							pStdAttrType = nil;
						}
						numStdAttributes++;
						//get the next std attribute
						pStdAttrType = GetNextStdAttrType( inRecType, inContext->fNodeName, numStdAttributes );
					}// while ( pStdAttrType != nil )
					
					if (bUsePlus)
					{
						// we add kDS1AttrPasswordPlus here
						aTmpData->Clear();
		
						// Append the attribute name
						aTmpData->AppendShort( ::strlen( kDS1AttrPasswordPlus ) );
						aTmpData->AppendString( kDS1AttrPasswordPlus );
	
						if ( inAttrOnly == false )
						{
							// Append the attribute value count
							aTmpData->AppendShort( 1 );
	
							// Append attribute value
							aTmpData->AppendLong( 8 );
							aTmpData->AppendString( "********" );
						}
						else
						{
							aTmpData->AppendShort( 0 );
						}
	
						// Add the attribute length
						outCount++;
						inDataBuff->AppendLong( aTmpData->GetLength() );
						inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

						// Clear the temp block
						aTmpData->Clear();
					} //if (bUsePlus)
					
				}// Std and all
				if ((::strcmp(pAttrType,kDSAttributesAll) == 0) || (::strcmp(pAttrType,kDSAttributesNativeAll) == 0))
				{
					aHost = fLDAPSessionMgr.LockSession(inContext);
					if (aHost != nil)
					{
						//now we output the native attributes
						for (	pAttr = ldap_first_attribute (aHost, inResult, &ber );
								pAttr != NULL; pAttr = ldap_next_attribute(aHost, inResult, ber ) )
						{
							aTmpData->Clear();
									
							if ( pAttr != nil )
							{
								aTmpData->AppendShort( ::strlen( pAttr ) + ::strlen( kDSNativeAttrTypePrefix ) );
								aTmpData->AppendString( (char *)kDSNativeAttrTypePrefix );
								aTmpData->AppendString( pAttr );
		
								if (( inAttrOnly == false ) &&
									(( bValues = ldap_get_values_len(aHost, inResult, pAttr )) != NULL) )
								{
								
									// calculate the number of values for this attribute
									valCount = 0;
									for (int ii = 0; bValues[ii] != NULL; ii++ )
									valCount++;
									
									// Append the attribute value count
									aTmpData->AppendShort( valCount );
			
									// for each value of the attribute
									for (int i = 0; bValues[i] != NULL; i++ )
									{
										// Append attribute value
										aTmpData->AppendLong( bValues[i]->bv_len );
										aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
									} // for each bValues[i]
									ldap_value_free_len(bValues);
									bValues = NULL;
								} // if ( inAttrOnly == false ) && bValues = ldap_get_values_len ...
								else
								{
									aTmpData->AppendShort( 0 );
								}
							
							}
							// Add the attribute length
							outCount++;
							inDataBuff->AppendLong( aTmpData->GetLength() );
							inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
		
							// Clear the temp block
							aTmpData->Clear();
							
							if (pAttr != nil)
							{
								ldap_memfree( pAttr );
							}
						} // for ( loop over ldap_next_attribute )
					
						if (ber != nil)
						{
							ber_free( ber, 0 );
							ber = nil;
						}
					} //if aHost != nil
					fLDAPSessionMgr.UnLockSession(inContext);
				}//Native and all
			}
			else //we have a specific attribute in mind
			{
				bStripCryptPrefix = false;
				//here we first check for the request for kDS1AttrPasswordPlus
				//ie. we see if kDS1AttrPassword is mapped and return that value if it is -- otherwise
				//we return the special value of "********" which is apparently never a valid crypt password
				//but this signals the requestor of kDS1AttrPasswordPlus to attempt to auth against the LDAP
				//server through doAuthentication via dsDoDirNodeAuth
				//get the first mapping
				numAttributes = 1;
				if (::strcmp( kDS1AttrPasswordPlus, pAttrType ) == 0)
				{
					//want to remove leading "{crypt}" prefix from password if it exists
					bStripCryptPrefix = true;
					pLDAPAttrType = MapAttrToLDAPType( (const char *)inRecType, kDS1AttrPassword, inContext->fNodeName, numAttributes );
					//set the indicator to check whether at least one of the native maps
					//form given standard type is correct
					//KW need to provide client with info on failed mappings??
					bAtLeastOneTypeValid = false;
					//set indicator of multiple native to standard mappings
					bTypeFound = false;
					if (pLDAPAttrType == nil)
					{
						bAtLeastOneTypeValid = true;
						
						//here we fill the value with "*******"
						aTmp2Data->Clear();
						//use given type in the output NOT mapped to type
						aTmp2Data->AppendShort( ::strlen( pAttrType ) );
						aTmp2Data->AppendString( pAttrType );
						//set indicator so that multiple values from multiple mapped to types
						//can be added to the given type
						bTypeFound = true;
						
						if (inAttrOnly == false)
						{
							//set attribute value count to one
							valCount = 1;
									
							// Clear the temp block
							aTmpData->Clear();
							// Append attribute value
							aTmpData->AppendLong( 8 );
							aTmpData->AppendString( "********" );
						}
					}
				}
				else
				{
					pLDAPAttrType = MapAttrToLDAPType( (const char *)inRecType, pAttrType, inContext->fNodeName, numAttributes );
					//throw if first nil since no more will be found otherwise proceed until nil
					// can't throw since that wipes out retrieval of all the following requested attributes
					//if (pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttrListRef); //KW would like a eDSNoMappingAvailable

					//set the indicator to check whether at least one of the native maps
					//form given standard type is correct
					//KW need to provide client with info on failed mappings??
					bAtLeastOneTypeValid = false;
					//set indicator of multiple native to standard mappings
					bTypeFound = false;
				}
				
				while ( pLDAPAttrType != nil )
				{
					//note that if standard type is incorrectly mapped ie. not found here
					//then the output will not contain any data on that std type
					if (!bTypeFound)
					{
						aTmp2Data->Clear();

						//use given type in the output NOT mapped to type
						aTmp2Data->AppendShort( ::strlen( pAttrType ) );
						aTmp2Data->AppendString( pAttrType );
						//set indicator so that multiple values from multiple mapped to types
						//can be added to the given type
						bTypeFound = true;
						
						//set attribute value count to zero
						valCount = 0;
						
						// Clear the temp block
						aTmpData->Clear();
					}

					if (pLDAPAttrType[0] == '#') //special case where attr is mapped to a literal string
					{
						bAtLeastOneTypeValid = true;

						char *vsReturnStr = nil;
						vsReturnStr = MappingNativeVariableSubstitution( pLDAPAttrType, inContext, inResult, siResult );
						
						if (vsReturnStr != nil)
						{
							// If we found a wildcard then set literalLength to the tmpStr
							literalLength = strlen(vsReturnStr) - 1;
						}
						else
						{
							//if parsing error returned then we throw an error
							if (siResult != eDSNoErr) throw (siResult);
							literalLength = strlen(pLDAPAttrType) - 1;
						}

						if ( (inAttrOnly == false) && (literalLength > 0) )
						{
							valCount++;
								
							if(vsReturnStr != nil)
							{
								// we found a wildcard then copy it here
								aTmpData->AppendLong( literalLength );
								aTmpData->AppendBlock( vsReturnStr + 1, literalLength );
							}
							else
							{
								// append attribute value
								aTmpData->AppendLong( literalLength );
								aTmpData->AppendBlock( pLDAPAttrType + 1, literalLength );
							}
						} // if ( (inAttrOnly == false) && (strlen(pLDAPAttrType) > 1) ) 
						if (vsReturnStr != nil)
						{
							free( vsReturnStr );
							vsReturnStr = nil;
						}
					}
					else
					{
						aHost = fLDAPSessionMgr.LockSession(inContext);
						if (aHost != nil)
						{
							bValues = ldap_get_values_len (aHost, inResult, pLDAPAttrType );
							if (bValues != NULL && bValues[0] != NULL)
							{
								bAtLeastOneTypeValid = true;
							}
							
							if ( ( inAttrOnly == false ) && (bValues != NULL) )
							{
							
								if (bStripCryptPrefix)
								{
									// add to the number of values for this attribute
									for (int ii = 0; bValues[ii] != NULL; ii++ )
									valCount++;
									
									if (valCount == 0)
									{
										valCount = 1;
										//no value found or returned for the mapped password attr
										// Append attribute value
										aTmpData->AppendLong( 8 );
										aTmpData->AppendString( "********" );
									}
									
									// for each value of the attribute
									for (int i = 0; bValues[i] != NULL; i++ )
									{
										//case insensitive compare with "crypt" string
										if ( ( bValues[i]->bv_len > 7) &&
											(strncasecmp(bValues[i]->bv_val,"{crypt}",7) == 0) )
										{
											// Append attribute value without "{crypt}" prefix
											aTmpData->AppendLong( bValues[i]->bv_len - 7 );
											aTmpData->AppendBlock( (bValues[i]->bv_val) + 7, bValues[i]->bv_len - 7 );
										}
										else
										{
											// Append attribute value
											aTmpData->AppendLong( bValues[i]->bv_len );
											aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
										}
									} // for each bValues[i]
									ldap_value_free_len(bValues);
									bValues = NULL;
								}
								else
								{
									// add to the number of values for this attribute
									for (int ii = 0; bValues[ii] != NULL; ii++ )
									valCount++;
									
									// for each value of the attribute
									for (int i = 0; bValues[i] != NULL; i++ )
									{
										// Append attribute value
										aTmpData->AppendLong( bValues[i]->bv_len );
										aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
									} // for each bValues[i]
									ldap_value_free_len(bValues);
									bValues = NULL;
								}
							} // if (aHost != nil) && ( inAttrOnly == false ) && bValues = ldap_get_values_len ...
							else if ( (valCount == 0) && (bValues == NULL) && bStripCryptPrefix)
							{
								valCount = 1;
								//no value found or returned for the mapped password attr
								// Append attribute value
								aTmpData->AppendLong( 8 );
								aTmpData->AppendString( "********" );
							}
							if (bValues != NULL)
							{
								ldap_value_free_len(bValues);
								bValues = NULL;
							}
						}
						fLDAPSessionMgr.UnLockSession(inContext);
					}
							
					//cleanup pLDAPAttrType if needed
					if (pLDAPAttrType != nil)
					{
						delete (pLDAPAttrType);
						pLDAPAttrType = nil;
					}
					numAttributes++;
					//get the next mapping
					pLDAPAttrType = MapAttrToLDAPType( (const char *)inRecType, pAttrType, inContext->fNodeName, numAttributes );				
				} // while ( pLDAPAttrType != nil )
				
				if ((bAtLeastOneTypeValid && inAttrOnly) || (valCount > 0))
				{
					// Append the attribute value count
					aTmp2Data->AppendShort( valCount );
					
					if (valCount > 0) 
					{
						// Add the attribute values to the attribute type
						aTmp2Data->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
						valCount = 0;
					}

					// Add the attribute data to the attribute data buffer
					outCount++;
					inDataBuff->AppendLong( aTmp2Data->GetLength() );
					inDataBuff->AppendBlock( aTmp2Data->GetData(), aTmp2Data->GetLength() );
				}
			} // else specific attr in mind
			
		} // while ( inAttrTypeList->GetAttribute( attrTypeIndex++, &pAttrType ) == eDSNoErr )
		
	} // try

	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	if ( pLDAPPasswdAttrType != nil )
	{
		free(pLDAPPasswdAttrType);
		pLDAPPasswdAttrType = nil;
	}
	if ( aTmpData != nil )
	{
		delete( aTmpData );
		aTmpData = nil;
	}
	if ( aTmp2Data != nil )
	{
		delete( aTmp2Data );
		aTmp2Data = nil;
	}

	return( siResult );

} // GetTheseAttributes


//------------------------------------------------------------------------------------
//	* GetRecordName
//------------------------------------------------------------------------------------

char *CLDAPv3Plugin::GetRecordName (	char			   *inRecType,
										LDAPMessage		   *inResult,
                                        sLDAPContextData   *inContext,
                                        sInt32			   &errResult )
{
	char		       *recName			= nil;
	char		       *pLDAPAttrType	= nil;
	struct berval	  **bValues;
	int					numAttributes	= 1;
	bool				bTypeFound		= false;
	LDAP			   *aHost			= nil;

	try
	{
		if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );

		errResult = eDSNoErr;
            
		//get the first mapping
		numAttributes = 1;
		pLDAPAttrType = MapAttrToLDAPType( (const char *)inRecType, kDSNAttrRecordName, inContext->fNodeName, numAttributes, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if (pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttrListRef); //KW would like a eDSNoMappingAvailable
            
		//set indicator of multiple native to standard mappings
		bTypeFound = false;
		aHost = fLDAPSessionMgr.LockSession(inContext);
		while ( (aHost != nil) && ( pLDAPAttrType != nil ) && (!bTypeFound) )
		{
			if ( ( bValues = ldap_get_values_len(aHost, inResult, pLDAPAttrType )) != NULL )
			{
				// for first value of the attribute
				recName = (char *) calloc(1, 1 + bValues[0]->bv_len);
				::strcpy( recName, bValues[0]->bv_val );
				//we found a value so stop looking
				bTypeFound = true;
				ldap_value_free_len(bValues);
				bValues = NULL;
			} // if ( bValues = ldap_get_values_len ...)
						
			//cleanup pLDAPAttrType if needed
			if (pLDAPAttrType != nil)
			{
					delete (pLDAPAttrType);
					pLDAPAttrType = nil;
			}
			numAttributes++;
			//get the next mapping
			pLDAPAttrType = MapAttrToLDAPType( (const char *)inRecType, kDSNAttrRecordName, inContext->fNodeName, numAttributes, true );				
		} // while ( pLDAPAttrType != nil )
		fLDAPSessionMgr.UnLockSession(inContext);
		//cleanup pLDAPAttrType if needed
		if (pLDAPAttrType != nil)
		{
			delete (pLDAPAttrType);
			pLDAPAttrType = nil;
		}
           
	} // try

	catch ( sInt32 err )
	{
		errResult = err;
	}

	return( recName );

} // GetRecordName


// ---------------------------------------------------------------------------
//	* MapAttrToLDAPType
// ---------------------------------------------------------------------------

char* CLDAPv3Plugin::MapAttrToLDAPType ( const char *inRecType, const char *inAttrType, char *inConfigName, int inIndex, bool bSkipLiteralMappings )
{
	char				   *outResult	= nil;
	uInt32					uiStrLen	= 0;
	uInt32					uiNativeLen	= ::strlen( kDSNativeAttrTypePrefix );
	uInt32					uiStdLen	= ::strlen( kDSStdAttrTypePrefix );
	int						aIndex		= inIndex;

	//idea here is to use the inIndex to request a specific map
	//if inIndex is 1 then the first map will be returned
	//if inIndex is >= 1 and <= totalCount then that map will be returned
	//if inIndex is <= 0 then nil will be returned
	//if inIndex is > totalCount nil will be returned
	//note the inIndex will reference the inIndexth entry ie. start at 1
	//caller can increment the inIndex starting at one and get maps until nil is returned

	if (( inAttrType != nil ) && (inIndex > 0))
	{
		uiStrLen = ::strlen( inAttrType );

		// First look for native attribute type
		if ( ::strncmp( inAttrType, kDSNativeAttrTypePrefix, uiNativeLen ) == 0 )
		{
			// Make sure we have data past the prefix
			if (( uiStrLen > uiNativeLen ) && (inIndex == 1))
			{
				uiStrLen = uiStrLen - uiNativeLen;
				outResult = (char *) calloc(1, uiStrLen + 1 );
				::strcpy( outResult, inAttrType + uiNativeLen );
				DBGLOG1( kLogPlugin, "CLDAPv3Plugin: MapAttrToLDAPType:: Warning:Native attribute type <%s> is being used", outResult );
			}
		} // native maps
		else if ( ::strncmp( inAttrType, kDSStdAttrTypePrefix, uiStdLen ) == 0 )
		{
			sLDAPConfigData *pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inConfigName );
			if (pConfig != nil)
			{
				//TODO need to "try" to get a default here if no mappings
				//KW maybe NOT ie. directed open can work with native types
				if ( pConfig->fRecordAttrMapDict == NULL )
				{
				}
				else
				{
					if (bSkipLiteralMappings)
					{
						bool bKeepLooking = false;
						do
						{
							bKeepLooking = false;
							outResult = gpConfigFromXML->ExtractAttrMap(inRecType, inAttrType, pConfig->fRecordAttrMapDict, aIndex );
							//if not used, ie. static, then free outResult
							if ( (outResult != nil) && (outResult[0] == '#') )
							{
								free(outResult);
								outResult = nil;
								bKeepLooking = true;
							}
							aIndex++;
						}
						while ( bKeepLooking );
					}
					else
					{
						outResult = gpConfigFromXML->ExtractAttrMap(inRecType, inAttrType, pConfig->fRecordAttrMapDict, inIndex );
					}
				}
				gpConfigFromXML->ConfigUnlock( pConfig );
			}

		}//standard maps
        //passthrough since we don't know what to do with it
        //and we assume the DS API client knows that the LDAP server
        //can handle this attribute type
		else
		{
			if ( inIndex == 1 )
			{
				outResult = (char *) calloc(1, 1 + ::strlen( inAttrType ) );
				::strcpy( outResult, inAttrType );
				DBGLOG1( kLogPlugin, "CLDAPv3Plugin: MapAttrToLDAPType:: Warning:Native attribute type with no provided prefix <%s> is being used", outResult );
			}
		}// passthrough map
	}// if (( inAttrType != nil ) && (inIndex > 0))

	return( outResult );

} // MapAttrToLDAPType


// ---------------------------------------------------------------------------
//	* MapAttrToLDAPTypeArray
// ---------------------------------------------------------------------------

char** CLDAPv3Plugin::MapAttrToLDAPTypeArray ( const char *inRecType, const char *inAttrType, char *inConfigName )
{
	char				  **outResult	= nil;
	uInt32					uiStrLen	= 0;
	uInt32					uiNativeLen	= ::strlen( kDSNativeAttrTypePrefix );
	uInt32					uiStdLen	= ::strlen( kDSStdAttrTypePrefix );
	int						countNative	= 0;

	if ( inAttrType != nil )
	{
		uiStrLen = ::strlen( inAttrType );

		// First look for native attribute type
		if ( ::strncmp( inAttrType, kDSNativeAttrTypePrefix, uiNativeLen ) == 0 )
		{
			// Make sure we have data past the prefix
			if ( uiStrLen > uiNativeLen )
			{
				uiStrLen = uiStrLen - uiNativeLen;
				outResult = (char **)::calloc( 2, sizeof( char * ) );
				*outResult = (char *) calloc(1, uiStrLen + 1 );
				::strcpy( *outResult, inAttrType + uiNativeLen );
				DBGLOG1( kLogPlugin, "CLDAPv3Plugin: MapAttrToLDAPType:: Warning:Native attribute type <%s> is being used", *outResult );
			}
		} // native maps
		else if ( ::strncmp( inAttrType, kDSStdAttrTypePrefix, uiStdLen ) == 0 )
		{
			sLDAPConfigData *pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inConfigName );
			if (pConfig != nil)
			{
				//TODO need to "try" to get a default here if no mappings
				//KW maybe NOT ie. directed open can work with native types
				
				char   *singleMap	= nil;
				int		aIndex		= 1;
				int		usedIndex	= 0;

				if ( pConfig->fRecordAttrMapDict == NULL )
				{
				}
				else
				{
					//need to know number of native maps first to alloc the outResult
					countNative = gpConfigFromXML->AttrMapsCount(inRecType, inAttrType, pConfig->fRecordAttrMapDict);
					if (countNative > 0)
					{
						outResult = (char **)::calloc( countNative + 1, sizeof(char *) );
						do
						{
							singleMap = gpConfigFromXML->ExtractAttrMap(inRecType, inAttrType, pConfig->fRecordAttrMapDict, aIndex );
							if (singleMap != nil)
							{
								if (singleMap[0] != '#')
								{
									outResult[usedIndex] = singleMap; //usedIndex is zero based
									usedIndex++;
								}
								else //don't use the literal mapping
								{
									free(singleMap);
									//singleMap = nil; //don't reset since needed for while condition below
								}
							}
							aIndex++;
						}
						while (singleMap != nil);
					}
				}
				gpConfigFromXML->ConfigUnlock( pConfig );
			}
			
		}//standard maps
        //passthrough since we don't know what to do with it
        //and we assume the DS API client knows that the LDAP server
        //can handle this attribute type
		else
		{
			outResult = (char **)::calloc( 2, sizeof(char *) );
			*outResult = (char *) calloc(1, 1 + ::strlen( inAttrType ) );
			::strcpy( *outResult, inAttrType );
			DBGLOG1( kLogPlugin, "CLDAPv3Plugin: MapAttrToLDAPTypeArray:: Warning:Native attribute type with no provided prefix <%s> is being used", *outResult );
		}// passthrough map
	}// if ( inAttrType != nil )

	return( outResult );

} // MapAttrToLDAPTypeArray


// ---------------------------------------------------------------------------
//	* MapAttrListToLDAPTypeArray
// ---------------------------------------------------------------------------

char** CLDAPv3Plugin::MapAttrListToLDAPTypeArray ( char *inRecType, CAttributeList *inAttrTypeList, char *inConfigName )
{
	char				  **outResult		= nil;
	char				  **mvResult		= nil;
	uInt32					uiStrLen		= 0;
	uInt32					uiNativeLen		= ::strlen( kDSNativeAttrTypePrefix );
	uInt32					uiStdLen		= ::strlen( kDSStdAttrTypePrefix );
	int						countNative		= 0;
	char				   *pAttrType		= nil;
	sInt32					attrTypeIndex	= 1;
	bool					bAddRecordName  = true;
	bool					bRecordNameGiven= false;
	bool					bCleanUp		= false;

	//TODO can we optimize allocs using a STL set and then creating the char** at the end
	//add in the recordname as the last entry if not already present or ALL not requested
	inAttrTypeList->GetAttribute( attrTypeIndex++, &pAttrType );
	while ( ( pAttrType != nil ) || bAddRecordName)
	{
		if ( pAttrType == nil )
		{
			if ( bRecordNameGiven )
			{
				break;
			}
			else
			{
				bAddRecordName  = false;
				pAttrType		= (char *)kDSNAttrRecordName;
				bCleanUp		= true;
			}
		}
		//deal with the special requests for all attrs here
		//if any of kDSAttributesAll, kDSAttributesNativeAll, or kDSAttributesStandardAll
		//are found anywhere in the list then we retrieve everything in the ldap_search call
		else if (	( strcmp(pAttrType,kDSAttributesAll) == 0 ) ||
					( strcmp(pAttrType,kDSAttributesNativeAll) == 0 ) ||
					( strcmp(pAttrType,kDSAttributesStandardAll) == 0 ) )
		{
			if (outResult != nil)
			{
				for (int ourIndex=0; ourIndex < countNative; ourIndex++) //remove existing
				{
					free(outResult[ourIndex]);
					outResult[ourIndex] = nil;
				}
				free(outResult);
			}
			return(nil);
		}
		
		uiStrLen = ::strlen( pAttrType );

		// First look for native attribute type
		if ( ::strncmp( pAttrType, kDSNativeAttrTypePrefix, uiNativeLen ) == 0 )
		{
			// Make sure we have data past the prefix
			if ( uiStrLen > uiNativeLen )
			{
				uiStrLen = uiStrLen - uiNativeLen;
				if (outResult != nil)
				{
					mvResult = outResult;
					outResult = (char **)::calloc( countNative + 2, sizeof( char * ) );
					for (int oldIndex=0; oldIndex < countNative; oldIndex++) //transfer existing
					{
						outResult[oldIndex] = mvResult[oldIndex];
					}
				}
				else
				{
					outResult = (char **)::calloc( 2, sizeof( char * ) );
				}
				outResult[countNative] = (char *) calloc(1, uiStrLen + 1 );
				::strcpy( outResult[countNative], pAttrType + uiNativeLen );
				DBGLOG1( kLogPlugin, "CLDAPv3Plugin: MapAttrListToLDAPTypeArray:: Warning:Native attribute type <%s> is being used", outResult[countNative] );
				countNative++;
				if (mvResult != nil)
				{
					free(mvResult);
					mvResult = nil;
				}
			}
		} // native maps
		else if ( ::strncmp( pAttrType, kDSStdAttrTypePrefix, uiStdLen ) == 0 )
		{
			if ( ::strcmp( pAttrType, (char *)kDSNAttrRecordName ) == 0 )
			{
				bRecordNameGiven = true;
			}

			//find the attr map that we need using inConfigName
			sLDAPConfigData *pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inConfigName );
			if (pConfig != nil)
			{
				//TODO need to "try" to get a default here if no mappings
				//KW maybe NOT ie. directed open can work with native types
				
				char   *singleMap	= nil;
				int		aIndex		= 1;
				int		mapCount	= 0;
				int		usedMapCount= 0;

				if ( pConfig->fRecordAttrMapDict == NULL )
				{
				}
				else
				{
					//need to know number of native maps first to alloc the outResult
					mapCount = gpConfigFromXML->AttrMapsCount(inRecType, pAttrType, pConfig->fRecordAttrMapDict);
					if (mapCount > 0)
					{
						if (outResult != nil)
						{
							mvResult = outResult;
							outResult = (char **)::calloc( countNative + mapCount + 1, sizeof( char * ) );
							for (int oldIndex=0; oldIndex < countNative; oldIndex++) //transfer existing
							{
								outResult[oldIndex] = mvResult[oldIndex];
							}
						}
						else
						{
							outResult = (char **)::calloc( mapCount + 1, sizeof( char * ) );
						}
						do
						{
							singleMap = gpConfigFromXML->ExtractAttrMap(inRecType, pAttrType, pConfig->fRecordAttrMapDict, aIndex );
							if (singleMap != nil)
							{
								if (singleMap[0] != '#')
								{
									outResult[countNative + usedMapCount] = singleMap; //usedMapCount is zero based
									usedMapCount++;
								}
								else //don't use the literal mapping
								{
									free(singleMap);
									//singleMap = nil; //don't reset since needed for while condition below
								}
							}
							aIndex++;
						}
						while (singleMap != nil);
						countNative += usedMapCount;
						if (mvResult != nil)
						{
							free(mvResult);
							mvResult = nil;
						}
					}
				}
				gpConfigFromXML->ConfigUnlock( pConfig );
			}
			
		}//standard maps
        //passthrough since we don't know what to do with it
        //and we assume the DS API client knows that the LDAP server
        //can handle this attribute type
		else
		{
			if (outResult != nil)
			{
				mvResult = outResult;
				outResult = (char **)::calloc( countNative + 2, sizeof( char * ) );
				for (int oldIndex=0; oldIndex < countNative; oldIndex++) //transfer existing
				{
					outResult[oldIndex] = mvResult[oldIndex];
				}
			}
			else
			{
				outResult = (char **)::calloc( 2, sizeof( char * ) );
			}
			outResult[countNative] = (char *) calloc(1, strlen( pAttrType ) + 1 );
			::strcpy( outResult[countNative], pAttrType );
			DBGLOG1( kLogPlugin, "CLDAPv3Plugin: MapAttrListToLDAPTypeArray:: Warning:Native attribute type with no provided prefix <%s> is being used", outResult[countNative] );
			countNative++;
			if (mvResult != nil)
			{
				free(mvResult);
				mvResult = nil;
			}
		}// passthrough map
		if (bCleanUp)
		{
			pAttrType = nil;
		}
		else
		{
			pAttrType = nil;
			inAttrTypeList->GetAttribute( attrTypeIndex++, &pAttrType );
		}
	}// while ( inAttrTypeList->GetAttribute( attrTypeIndex++, &pAttrType ) == eDSNoErr )

	return( outResult );

} // MapAttrListToLDAPTypeArray


// ---------------------------------------------------------------------------
//	* MapLDAPWriteErrorToDS
//
//		- convert LDAP error to DS error
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::MapLDAPWriteErrorToDS ( sInt32 inLDAPError, sInt32 inDefaultError )
{
	sInt32		siOutError	= 0;

	switch ( inLDAPError )
	{
		case LDAP_SUCCESS:
			siOutError = eDSNoErr;
			break;
		case LDAP_AUTH_UNKNOWN:
		case LDAP_AUTH_METHOD_NOT_SUPPORTED:
			siOutError = eDSAuthMethodNotSupported;
			break;
		case LDAP_INAPPROPRIATE_AUTH:
		case LDAP_INVALID_CREDENTIALS:
		case LDAP_INSUFFICIENT_ACCESS:
		case LDAP_STRONG_AUTH_REQUIRED:
			siOutError = eDSPermissionError;
			break;
		case LDAP_NO_SUCH_ATTRIBUTE:
			siOutError = eDSAttributeNotFound;
			break;
		case LDAP_NO_SUCH_OBJECT:
			siOutError = eDSRecordNotFound;
			break;
		case LDAP_NO_MEMORY:
			siOutError = eMemoryError;
			break;
		case LDAP_TIMEOUT:
			siOutError = eDSServerTimeout;
			break;
		case LDAP_NAMING_VIOLATION:
		case LDAP_OBJECT_CLASS_VIOLATION:
		case LDAP_CONSTRAINT_VIOLATION:
		case LDAP_INVALID_SYNTAX:
			siOutError = eDSSchemaError;
			break;
		case LDAP_SERVER_DOWN:
			siOutError = eDSCannotAccessSession;
			break;
		case LDAP_UNDEFINED_TYPE:
			siOutError = eDSInvalidAttributeType;
			break;
		case LDAP_INVALID_DN_SYNTAX:
			siOutError = eDSInvalidName;
			break;
			
		default:
		/*
		Remaining errors not yet mapped
		case LDAP_INAPPROPRIATE_MATCHING:
		case LDAP_TYPE_OR_VALUE_EXISTS:
		case LDAP_OPERATIONS_ERROR:
		case LDAP_PROTOCOL_ERROR:
		case LDAP_TIMELIMIT_EXCEEDED:
		case LDAP_SIZELIMIT_EXCEEDED:
		case LDAP_COMPARE_FALSE:
		case LDAP_COMPARE_TRUE:
		case LDAP_PARTIAL_RESULTS:
		case LDAP_REFERRAL:
		case LDAP_ADMINLIMIT_EXCEEDED
		case LDAP_UNAVAILABLE_CRITICAL_EXTENSION
		case LDAP_CONFIDENTIALITY_REQUIRED
		case LDAP_SASL_BIND_IN_PROGRESS
		case LDAP_ALIAS_PROBLEM
		case LDAP_IS_LEAF
		case LDAP_ALIAS_DEREF_PROBLEM

		case LDAP_BUSY
		case LDAP_UNAVAILABLE
		case LDAP_UNWILLING_TO_PERFORM
		case LDAP_LOOP_DETECT

		case LDAP_NOT_ALLOWED_ON_NONLEAF
		case LDAP_NOT_ALLOWED_ON_RDN
		case LDAP_ALREADY_EXISTS
		case LDAP_NO_OBJECT_CLASS_MODS
		case LDAP_RESULTS_TOO_LARGE
		case LDAP_AFFECTS_MULTIPLE_DSAS

		case LDAP_OTHER

		case LDAP_LOCAL_ERROR
		case LDAP_ENCODING_ERROR
		case LDAP_DECODING_ERROR
		case LDAP_FILTER_ERROR
		case LDAP_USER_CANCELLED
		case LDAP_PARAM_ERROR

		case LDAP_CONNECT_ERROR
		case LDAP_NOT_SUPPORTED
		case LDAP_CONTROL_NOT_FOUND
		case LDAP_NO_RESULTS_RETURNED
		case LDAP_MORE_RESULTS_TO_RETURN
		case LDAP_CLIENT_LOOP
		case LDAP_REFERRAL_LIMIT_EXCEEDED*/
			siOutError = inDefaultError;
			break;
	}
	DBGLOG1( kLogPlugin, "CLDAPv3Plugin: error code %d returned by LDAP framework", inLDAPError );

	return( siOutError );

} // MapLDAPWriteErrorToDS


//------------------------------------------------------------------------------------
//	* GetAttributeEntry
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetAttributeEntry ( sGetAttributeEntry *inData )
{
	sInt32					siResult			= eDSNoErr;
	uInt16					usAttrTypeLen		= 0;
	uInt16					usAttrCnt			= 0;
	uInt32					usAttrLen			= 0;
	uInt16					usValueCnt			= 0;
	uInt32					usValueLen			= 0;
	uInt32					i					= 0;
	uInt32					uiIndex				= 0;
	uInt32					uiAttrEntrySize		= 0;
	uInt32					uiOffset			= 0;
	uInt32					uiTotalValueSize	= 0;
	uInt32					offset				= 4;
	uInt32					buffSize			= 0;
	uInt32					buffLen				= 0;
	char				   *p			   		= nil;
	char				   *pAttrType	   		= nil;
	tDataBufferPtr			pDataBuff			= nil;
	tAttributeEntryPtr		pAttribInfo			= nil;
	sLDAPContextData		   *pAttrContext		= nil;
	sLDAPContextData		   *pValueContext		= nil;

	try
	{
		if ( inData  == nil ) throw( (sInt32) eMemoryError );

		pAttrContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInAttrListRef );
		if ( pAttrContext  == nil ) throw( (sInt32)eDSBadContextData );

		uiIndex = inData->fInAttrInfoIndex;
		if (uiIndex == 0) throw( (sInt32)eDSInvalidIndex );
				
		pDataBuff = inData->fInOutDataBuff;
		if ( pDataBuff  == nil ) throw( (sInt32)eDSNullDataBuff );
		
		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pAttrContext->offset;
		offset	= pAttrContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
				
		// Get the attribute count
		::memcpy( &usAttrCnt, p, 2 );
		if (uiIndex > usAttrCnt) throw( (sInt32)eDSInvalidIndex );

		// Move 2 bytes
		p		+= 2;
		offset	+= 2;

		// Skip to the attribute that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (offset + 4 > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
		
			// Get the length for the attribute
			::memcpy( &usAttrLen, p, 4 );

			// Move the offset past the length word and the length of the data
			p		+= 4 + usAttrLen;
			offset	+= 4 + usAttrLen;
		}

		// Get the attribute offset
		uiOffset = offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 4 > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute block
		::memcpy( &usAttrLen, p, 4 );

		// Skip past the attribute length
		p		+= 4;
		offset	+= 4;

		//set the bufLen to stricter range
		buffLen = offset + usAttrLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute type
		::memcpy( &usAttrTypeLen, p, 2 );
		
		pAttrType = p + 2;
		p		+= 2 + usAttrTypeLen;
		offset	+= 2 + usAttrTypeLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get number of values for this attribute
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;
		
		for ( i = 0; i < usValueCnt; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (offset + 4 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
			// Get the length for the value
			::memcpy( &usValueLen, p, 4 );
			
			p		+= 4 + usValueLen;
			offset	+= 4 + usValueLen;
			
			uiTotalValueSize += usValueLen;
		}

		uiAttrEntrySize = sizeof( tAttributeEntry ) + usAttrTypeLen + kBuffPad;
		pAttribInfo = (tAttributeEntry *)::calloc( 1, uiAttrEntrySize );

		pAttribInfo->fAttributeValueCount				= usValueCnt;
		pAttribInfo->fAttributeDataSize					= uiTotalValueSize;
		pAttribInfo->fAttributeValueMaxSize				= 512;				// KW this is not used anywhere
		pAttribInfo->fAttributeSignature.fBufferSize	= usAttrTypeLen + kBuffPad;
		pAttribInfo->fAttributeSignature.fBufferLength	= usAttrTypeLen;
		::memcpy( pAttribInfo->fAttributeSignature.fBufferData, pAttrType, usAttrTypeLen );

		pValueContext = new sLDAPContextData;
		if ( pValueContext  == nil ) throw( (sInt32) eMemoryAllocError );

		pValueContext->offset = uiOffset;

		gLDAPContextTable->AddItem( inData->fOutAttrValueListRef, pValueContext );

		inData->fOutAttrInfoPtr = pAttribInfo;
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeEntry


//------------------------------------------------------------------------------------
//	* GetAttributeValue
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetAttributeValue ( sGetAttributeValue *inData )
{
	sInt32						siResult		= eDSNoErr;
	uInt16						usValueCnt		= 0;
	uInt32						usValueLen		= 0;
	uInt16						usAttrNameLen	= 0;
	uInt32						i				= 0;
	uInt32						uiIndex			= 0;
	uInt32						offset			= 0;
	char					   *p				= nil;
	tDataBuffer				   *pDataBuff		= nil;
	tAttributeValueEntry	   *pAttrValue		= nil;
	sLDAPContextData		   *pValueContext	= nil;
	uInt32						buffSize		= 0;
	uInt32						buffLen			= 0;
	uInt32						attrLen			= 0;

	try
	{
		pValueContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInAttrValueListRef );
		if ( pValueContext  == nil ) throw( (sInt32)eDSBadContextData );

		uiIndex = inData->fInAttrValueIndex;
		if (uiIndex == 0) throw( (sInt32)eDSInvalidIndex );
		
		pDataBuff = inData->fInOutDataBuff;
		if ( pDataBuff  == nil ) throw( (sInt32)eDSNullDataBuff );

		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pValueContext->offset;
		offset	= pValueContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 4 > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
				
		// Get the buffer length
		::memcpy( &attrLen, p, 4 );

		//now add the offset to the attr length for the value of buffLen to be used to check for buffer overruns
		//AND add the length of the buffer length var as stored ie. 4 bytes
		buffLen		= attrLen + pValueContext->offset + 4;
		if (buffLen > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );

		// Skip past the attribute length
		p		+= 4;
		offset	+= 4;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the attribute name length
		::memcpy( &usAttrNameLen, p, 2 );
		
		p		+= 2 + usAttrNameLen;
		offset	+= 2 + usAttrNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the value count
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;

		if (uiIndex > usValueCnt)  throw( (sInt32)eDSInvalidIndex );

		// Skip to the value that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (offset + 4 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
			// Get the length for the value
			::memcpy( &usValueLen, p, 4 );
			
			p		+= 4 + usValueLen;
			offset	+= 4 + usValueLen;
		}

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 4 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		::memcpy( &usValueLen, p, 4 );
		
		p		+= 4;
		offset	+= 4;

		//if (usValueLen == 0)  throw( (sInt32)eDSInvalidBuffFormat ); //if zero is it okay?

		pAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + usValueLen + kBuffPad );

		pAttrValue->fAttributeValueData.fBufferSize		= usValueLen + kBuffPad;
		pAttrValue->fAttributeValueData.fBufferLength	= usValueLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if ( offset + usValueLen > buffLen )throw( (sInt32)eDSInvalidBuffFormat );
		
		::memcpy( pAttrValue->fAttributeValueData.fBufferData, p, usValueLen );

		// Set the attribute value ID
		pAttrValue->fAttributeValueID = CalcCRC( pAttrValue->fAttributeValueData.fBufferData );

		inData->fOutAttrValue = pAttrValue;
			
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeValue



//------------------------------------------------------------------------------------
//	* GetTheseRecords
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetTheseRecords (	char			   *inConstAttrType,
										char			  **inAttrNames,
										char			   *inRecType,
										char			   *inNativeRecType,
										tDirPatternMatch	patternMatch,
										CAttributeList	   *inAttrTypeList,
										sLDAPContextData   *inContext,
										sLDAPContinueData  *inContinue,
										bool				inAttrOnly,
										CBuff			   *inBuff,
										uInt32			   &outRecCount,
										bool				inbOCANDGroup,
										CFArrayRef			inOCSearchList,
										ber_int_t			inScope )
{
	sInt32					siResult		= eDSNoErr;
    sInt32					siValCnt		= 0;
    int						ldapReturnCode 	= 0;
    bool					bufferFull		= false;
    LDAPMessage			   *result			= nil;
    char				   *recName			= nil;
    char				   *queryFilter		= nil;
    sLDAPConfigData		   *pConfig			= nil;
    int						searchTO		= 0;
	CDataBuff			   *aRecData		= nil;
	CDataBuff			   *aAttrData		= nil;
	bool					bFoundMatch		= false;

	//TODO why not optimize and save the queryFilter in the continue data for the next call in?
	//not that easy over continue data
	
	//code maintenance savings by combining FindAllRecords behavior within GetTheseRecords
	//ie. only difference was in building the queryfilter
	if (inConstAttrType == nil)
	{
		//case of FindAllRecords behavior
		//build the record query string
		queryFilter = BuildLDAPQueryFilter(	inConstAttrType,
											nil,
											eDSAnyMatch,
											inContext->fNodeName,
											false,
											(const char *)inRecType,
											inNativeRecType,
											inbOCANDGroup,
											inOCSearchList );
	}
	else
	{
		//build the record query string
		queryFilter = BuildLDAPQueryMultiFilter
										(	inConstAttrType,
											inAttrNames,
											patternMatch,
											inContext->fNodeName,
											false,
											(const char *)inRecType,
											inNativeRecType,
											inbOCANDGroup,
											inOCSearchList );
	}
	    
	outRecCount = 0; //need to track how many records were found by this call to GetTheseRecords
	
    try
    {
    
    	if (inContext == nil ) throw( (sInt32)eDSInvalidContext);
    	if (inContinue == nil ) throw( (sInt32)eDSInvalidContinueData);
    	
    	// check to make sure the queryFilter is not nil
    	if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );
    	
		aRecData = new CDataBuff();
		if ( aRecData  == nil ) throw( (sInt32) eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData  == nil ) throw( (sInt32) eMemoryError );
		
		pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inContext->fNodeName );
		if (pConfig != nil)
		{
			searchTO	= pConfig->fSearchTimeout;
			
			gpConfigFromXML->ConfigUnlock( pConfig );
			pConfig = NULL; // so we don't use it accidentally
		}
		
		//LDAP search with rebind capability
		DSDoSearch (	inRecType,
						inNativeRecType,
						inAttrTypeList,
						inContext,
						inContinue,
						inScope,
						fLDAPSessionMgr,
						queryFilter,
						ldapReturnCode,
						searchTO,
						result);

		while ( 	( (ldapReturnCode == LDAP_RES_SEARCH_ENTRY) || (ldapReturnCode == LDAP_RES_SEARCH_RESULT) )
					&& !(bufferFull)
					&& (	(inContinue->fTotalRecCount < inContinue->fLimitRecSearch) ||
							(inContinue->fLimitRecSearch == 0) ) )
        {
			// check to see if there is a match
            // package the record into the DS format into the buffer
            // steps to add an entry record to the buffer
            // build the aRecData header
            // build the aAttrData
            // append the aAttrData to the aRecData
            // add the aRecData to the buffer inBuff

			if (ldapReturnCode == LDAP_RES_SEARCH_ENTRY)
			{
				bFoundMatch = false;
				if ( DoAnyOfTheseAttributesMatch(inContext, inAttrNames, patternMatch, result) )
				{
					bFoundMatch = true;
	
					aRecData->Clear();
		
					if ( inRecType != nil )
					{
						aRecData->AppendShort( ::strlen( inRecType ) );
						aRecData->AppendString( inRecType );
					}
					else
					{
						aRecData->AppendShort( ::strlen( "Record Type Unknown" ) );
						aRecData->AppendString( "Record Type Unknown" );
					}
		
					// need to get the record name
					recName = GetRecordName( inRecType, result, inContext, siResult );
					if ( siResult != eDSNoErr ) throw( siResult );
					if ( recName != nil )
					{
						aRecData->AppendShort( ::strlen( recName ) );
						aRecData->AppendString( recName );
		
						delete ( recName );
						recName = nil;
					}
					else
					{
						aRecData->AppendShort( ::strlen( "Record Name Unknown" ) );
						aRecData->AppendString( "Record Name Unknown" );
					}
		
					// need to calculate the number of attribute types ie. siValCnt
					// also need to extract the attributes and place them into fAttrData
					//siValCnt = 0;
		
					aAttrData->Clear();
					siResult = GetTheseAttributes( inRecType, inAttrTypeList, result, inAttrOnly, inContext, siValCnt, aAttrData );
					if ( siResult != eDSNoErr ) throw( siResult );
					
					//add the attribute info to the aRecData
					if ( siValCnt == 0 )
					{
						// Attribute count
						aRecData->AppendShort( 0 );
					}
					else
					{
						// Attribute count
						aRecData->AppendShort( siValCnt );
						aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
					}
		
					// add the aRecData now to the inBuff
					siResult = inBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
	
				} // DoTheseAttributesMatch?
			}
            // need to check if the buffer is full
			// need to handle full buffer and keep the result alive for the next call in
            if (siResult == CBuff::kBuffFull)
            {
                bufferFull = true;
                
                //save the result if buffer is full
                inContinue->pResult = result;
            }
            else if ( siResult == eDSNoErr )
            {
				//only get next result if buffer is not full
                ldap_msgfree( result );
				result = nil;
                
				if (bFoundMatch)
				{
					outRecCount++; //another record added
					inContinue->fTotalRecCount++;
				}
				
                //make sure no result is carried in the context
                inContinue->pResult = nil;

				//only get next result if buffer is not full
				DSGetSearchLDAPResult (	fLDAPSessionMgr,
										inContext,
										searchTO,
										result,
										LDAP_MSG_ONE,
										inContinue->msgId,
										ldapReturnCode,
										false);
            }
            else
            {
                //make sure no result is carried in the context
                inContinue->pResult = nil;
                throw( (sInt32)eDSInvalidBuffFormat );
            }
            
        } // while loop over entries

		if ( (result != inContinue->pResult) && (result != nil) )
		{
			ldap_msgfree( result );
			result = nil;
		}

		//here we attempt to cleanup the response list of responses that have already been deleted
		//if siResult is eDSNoErr then the msgId will be removed from inContinue in the calling routine GetRecordList
		if ( (siResult == eDSNoErr) && (inContinue != NULL) && (inContinue->msgId != 0) )
		{
			LDAP *aHost = nil;
			aHost = fLDAPSessionMgr.LockSession(inContext);
			if (aHost != nil)
			{
				DSSearchCleanUp(aHost, inContinue->msgId);
			}
			fLDAPSessionMgr.UnLockSession(inContext);
		}

    } // try block

    catch ( sInt32 err )
    {
        siResult = err;
    }

	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}

	if (aRecData != nil)
	{
		delete (aRecData);
		aRecData = nil;
	}
	if (aAttrData != nil)
	{
		delete (aAttrData);
		aAttrData = nil;
	}
	
    return( siResult );

} // GetTheseRecords


//------------------------------------------------------------------------------------
//	* BuildLDAPQueryFilter
//------------------------------------------------------------------------------------

char *CLDAPv3Plugin::BuildLDAPQueryFilter (	char			   *inConstAttrType,
											const char		   *inConstAttrName,
											tDirPatternMatch	patternMatch,
											char			   *inConfigName,
											bool				useWellKnownRecType,
											const char		   *inRecType,
											char			   *inNativeRecType,
											bool				inbOCANDGroup,
											CFArrayRef			inOCSearchList )
{
    char				   *queryFilter			= nil;
	unsigned long			matchType			= eDSExact;
	char				   *nativeAttrType		= nil;
	uInt32					recNameLen			= 0;
	int						numAttributes		= 1;
	CFMutableStringRef		cfStringRef			= nil;
	CFMutableStringRef		cfQueryStringRef	= nil;
	char				   *escapedName			= nil;
	uInt32					escapedIndex		= 0;
	uInt32					originalIndex		= 0;
	bool					bOnceThru			= false;
	uInt32					offset				= 3;
	uInt32					callocLength		= 0;
	bool					objClassAdded		= false;
	bool					bGetAllDueToLiteralMapping = false;
	int						aOCSearchListCount	= 0;
	
	cfQueryStringRef = CFStringCreateMutable(kCFAllocatorDefault, 0);
	
	//build the objectclass filter prefix condition if available and then set objClassAdded
	//before the original filter on name and native attr types
	//use the inConfigTableIndex to access the mapping config

	//check for nil and then check if this is a standard type so we have a chance there is an objectclass mapping
	if ( (inRecType != nil) && (inNativeRecType != nil) )
	{
		if (strncmp(inRecType,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
		{
			//determine here whether or not there are any objectclass mappings to include
			if (inOCSearchList != nil)
			{
				//here we extract the object class strings
				//combine using "&" if inbOCANDGroup otherwise use "|"
				aOCSearchListCount = CFArrayGetCount(inOCSearchList);
				for ( int iOCIndex = 0; iOCIndex < aOCSearchListCount; iOCIndex++ )
				{
					CFStringRef	ocString;
					ocString = (CFStringRef)::CFArrayGetValueAtIndex( inOCSearchList, iOCIndex );
					if (ocString != nil)
					{
						if (!objClassAdded)
						{
							objClassAdded = true;
							if (inbOCANDGroup)
							{
								CFStringAppendCString(cfQueryStringRef,"(&", kCFStringEncodingUTF8);
							}
							else
							{
								CFStringAppendCString(cfQueryStringRef,"(&(|", kCFStringEncodingUTF8);
							}
						}
						
						CFStringAppendCString(cfQueryStringRef, "(objectclass=", kCFStringEncodingUTF8);

						// do we need to escape any of the characters internal to the CFString??? like before
						// NO since "*, "(", and ")" are not legal characters for objectclass names
						CFStringAppend(cfQueryStringRef, ocString);
						
						CFStringAppendCString(cfQueryStringRef, ")", kCFStringEncodingUTF8);
					}
				}// loop over the objectclasses CFArray
				if (CFStringGetLength(cfQueryStringRef) != 0)
				{
					if (!inbOCANDGroup)
					{
						//( so PB bracket completion works
						CFStringAppendCString(cfQueryStringRef,")", kCFStringEncodingUTF8);
					}
				}
			}
		}// if (strncmp(inRecType,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
	}// if ( (inRecType != nil) && (inNativeRecType != nil) )
	
	//check for case of NO objectclass mapping BUT also no inConstAttrName meaning we want to have
	//the result of (objectclass=*)
	if ( (CFStringGetLength(cfQueryStringRef) == 0) && (inConstAttrName == nil) )
	{
		CFStringAppendCString(cfQueryStringRef,"(&(objectclass=*)", kCFStringEncodingUTF8);
		objClassAdded = true;
	}
	
	//here we decide if this is eDSCompoundExpression or eDSiCompoundExpression so that we special case this
	if (	(patternMatch == eDSCompoundExpression) ||
			(patternMatch == eDSiCompoundExpression) )
	{ //KW right now it is always case insensitive
		cfStringRef = ParseCompoundExpression(inConstAttrName, inRecType, inConfigName );

		if (cfStringRef != nil)
		{
			CFStringAppend(cfQueryStringRef, cfStringRef);
			CFRelease(cfStringRef);
			cfStringRef = nil;
		}
			
	}
	else
	{
		//first check to see if input not nil
		if (inConstAttrName != nil)
		{
			recNameLen = strlen(inConstAttrName);
			escapedName = (char *)::calloc(1, 2 * recNameLen + 1);
			// assume at most all characters will be escaped
			while (originalIndex < recNameLen)
			{
				switch (inConstAttrName[originalIndex])
				{
					case '*':
					case '(':
					case ')':
						escapedName[escapedIndex] = '\\';
						++escapedIndex;
						// add \ escape character then fall through and pick up the original character
					default:
						escapedName[escapedIndex] = inConstAttrName[originalIndex];
						++escapedIndex;
						break;
				}
				++originalIndex;
			}
			
			//assume that the query is "OR" based ie. meet any of the criteria
			cfStringRef = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("(|"));
			
			//get the first mapping
			numAttributes = 1;
			//KW Note that kDS1RecordName is single valued so we are using kDSNAttrRecordName
			//as a multi-mapped std type which will easily lead to multiple values
			nativeAttrType = MapAttrToLDAPType( inRecType, inConstAttrType, inConfigName, numAttributes, false );
			//would throw if first nil since no more will be found otherwise proceed until nil
			//however simply set to default LDAP native in this case
			//ie. we are trying regardless if kDSNAttrRecordName is mapped or not
			//whether or not "cn" is a good choice is a different story
			if (nativeAttrType == nil) //only for first mapping
			{
				nativeAttrType = strdup("cn");
			}
	
			while ( nativeAttrType != nil )
			{
				if (nativeAttrType[0] == '#') //literal mapping
				{
					if (strlen(nativeAttrType) > 1)
					{
						if (DoesThisMatch(escapedName, nativeAttrType+1, patternMatch))
						{
							if (CFStringGetLength(cfQueryStringRef) == 0)
							{
								CFStringAppendCString(cfQueryStringRef,"(&(objectclass=*)", kCFStringEncodingUTF8);
								objClassAdded = true;
							}
							bGetAllDueToLiteralMapping = true;
							free(nativeAttrType);
							nativeAttrType = nil;
							continue;
						}
					}
				}
				else
				{
					if (bOnceThru)
					{
						if (useWellKnownRecType)
						{
							//need to allow for condition that we want only a single query
							//that uses only a single native type - use the first one - 
							//for perhaps an open record or a write to a specific record
							bOnceThru = false;
							break;
						}
						offset = 0;
					}
					matchType = (unsigned long) (patternMatch);
					switch (matchType)
					{
				//		case eDSAnyMatch:
				//			cout << "Pattern match type of <eDSAnyMatch>" << endl;
				//			break;
						case eDSStartsWith:
						case eDSiStartsWith:
							CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"=", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, escapedName, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"*)", kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
						case eDSEndsWith:
						case eDSiEndsWith:
							CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"=*", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, escapedName, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
						case eDSContains:
						case eDSiContains:
							CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"=*", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, escapedName, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"*)", kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
						case eDSWildCardPattern:
						case eDSiWildCardPattern:
							//assume the inConstAttrName is wild
							CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"=", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, inConstAttrName, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
						case eDSRegularExpression:
						case eDSiRegularExpression:
							//assume inConstAttrName replaces entire wild expression
							CFStringAppendCString(cfStringRef, inConstAttrName, kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
						case eDSExact:
						case eDSiExact:
						default:
							CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"=", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, escapedName, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
					} // switch on matchType
				}
				//cleanup nativeAttrType if needed
				if (nativeAttrType != nil)
				{
					free(nativeAttrType);
					nativeAttrType = nil;
				}
				numAttributes++;
				//get the next mapping
				nativeAttrType = MapAttrToLDAPType( inRecType, inConstAttrType, inConfigName, numAttributes, false );
			} // while ( nativeAttrType != nil )
	
			if (!bGetAllDueToLiteralMapping)
			{
				if (cfStringRef != nil)
				{
					// building search like "sn=name"
					if (offset == 3)
					{
						CFRange	aRangeToDelete;
						aRangeToDelete.location = 1;
						aRangeToDelete.length = 2;			
						CFStringDelete(cfStringRef, aRangeToDelete);
					}
					//building search like "(|(sn=name)(cn=name))"
					else
					{
						CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
					}
					
					CFStringAppend(cfQueryStringRef, cfStringRef);
				}
			}
			
			if (cfStringRef != nil)
			{
				CFRelease(cfStringRef);
				cfStringRef = nil;
			}

			if (escapedName != nil)
			{
				free(escapedName);
				escapedName = nil;
			}
	
		} // if inConstAttrName not nil
	}
	if (objClassAdded)
	{
		CFStringAppendCString(cfQueryStringRef,")", kCFStringEncodingUTF8);
	}
	
	//here we make the char * output in queryfilter
	if (CFStringGetLength(cfQueryStringRef) != 0)
	{
		callocLength = (uInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfQueryStringRef), kCFStringEncodingUTF8) + 1;
		queryFilter = (char *) calloc(1, callocLength);
		CFStringGetCString( cfQueryStringRef, queryFilter, callocLength, kCFStringEncodingUTF8 );
	}

	if (cfQueryStringRef != nil)
	{
		CFRelease(cfQueryStringRef);
		cfQueryStringRef = nil;
	}

	return (queryFilter);
	
} // BuildLDAPQueryFilter


//------------------------------------------------------------------------------------
//	* BuildLDAPQueryMultiFilter
//------------------------------------------------------------------------------------

char *CLDAPv3Plugin::BuildLDAPQueryMultiFilter
										(	char			   *inConstAttrType,
											char			  **inAttrNames,
											tDirPatternMatch	patternMatch,
											char			   *inConfigName,
											bool				useWellKnownRecType,
											const char		   *inRecType,
											char			   *inNativeRecType,
											bool				inbOCANDGroup,
											CFArrayRef			inOCSearchList )
{
    char				   *queryFilter			= nil;
	unsigned long			matchType			= eDSExact;
	char				   *nativeAttrType		= nil;
	uInt32					recNameLen			= 0;
	int						numAttributes		= 1;
	CFMutableStringRef		cfStringRef			= nil;
	CFMutableStringRef		cfQueryStringRef	= nil;
	char				  **escapedStrings		= nil;
	uInt32					escapedIndex		= 0;
	uInt32					originalIndex		= 0;
	bool					bOnceThru			= false;
	uInt32					offset				= 3;
	uInt32					callocLength		= 0;
	bool					objClassAdded		= false;
	bool					bGetAllDueToLiteralMapping = false;
	int						aOCSearchListCount	= 0;
	
	cfQueryStringRef = CFStringCreateMutable(kCFAllocatorDefault, 0);
	
	//build the objectclass filter prefix condition if available and then set objClassAdded
	//before the original filter on name and native attr types
	//use the inConfigTableIndex to access the mapping config

	//check for nil and then check if this is a standard type so we have a chance there is an objectclass mapping
	if ( (inRecType != nil) && (inNativeRecType != nil) )
	{
		if (strncmp(inRecType,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
		{
			//determine here whether or not there are any objectclass mappings to include
			if (inOCSearchList != nil)
			{
				//here we extract the object class strings
				//combine using "&" if inbOCANDGroup otherwise use "|"
				aOCSearchListCount = CFArrayGetCount(inOCSearchList);
				for ( int iOCIndex = 0; iOCIndex < aOCSearchListCount; iOCIndex++ )
				{
					CFStringRef	ocString;
					ocString = (CFStringRef)::CFArrayGetValueAtIndex( inOCSearchList, iOCIndex );
					if (ocString != nil)
					{
						if (!objClassAdded)
						{
							objClassAdded = true;
							if (inbOCANDGroup)
							{
								CFStringAppendCString(cfQueryStringRef,"(&", kCFStringEncodingUTF8);
							}
							else
							{
								CFStringAppendCString(cfQueryStringRef,"(&(|", kCFStringEncodingUTF8);
							}
						}
						
						CFStringAppendCString(cfQueryStringRef, "(objectclass=", kCFStringEncodingUTF8);

						// do we need to escape any of the characters internal to the CFString??? like before
						// NO since "*, "(", and ")" are not legal characters for objectclass names
						CFStringAppend(cfQueryStringRef, ocString);
						
						CFStringAppendCString(cfQueryStringRef, ")", kCFStringEncodingUTF8);
					}
				}// loop over the objectclasses CFArray
				if (CFStringGetLength(cfQueryStringRef) != 0)
				{
					if (!inbOCANDGroup)
					{
						//( so PB bracket completion works
						CFStringAppendCString(cfQueryStringRef,")", kCFStringEncodingUTF8);
					}
				}
			}
		}// if (strncmp(inRecType,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
	}// if ( (inRecType != nil) && (inNativeRecType != nil) )
	
	//check for case of NO objectclass mapping BUT also no inAttrNames meaning we want to have
	//the result of (objectclass=*)
	if ( (CFStringGetLength(cfQueryStringRef) == 0) && (inAttrNames == nil) )
	{
		CFStringAppendCString(cfQueryStringRef,"(&(objectclass=*)", kCFStringEncodingUTF8);
		objClassAdded = true;
	}
	
	//here we decide if this is eDSCompoundExpression or eDSiCompoundExpression so that we special case this
	if (	((patternMatch == eDSCompoundExpression) ||
			(patternMatch == eDSiCompoundExpression)) && inAttrNames )
	{ //KW right now it is always case insensitive
		
		//if this is a multi-search, we will assume that inAttrNames[0] is the compound expression
		//multiple inAttrNames is not supported
		cfStringRef = ParseCompoundExpression( inAttrNames[0], inRecType, inConfigName );
		
		if (cfStringRef != nil)
		{
			CFStringAppend(cfQueryStringRef, cfStringRef);
			CFRelease(cfStringRef);
			cfStringRef = nil;
		}
	}
	else
	{
		//first check to see if input not nil
		if (inAttrNames != nil)
		{
			uInt32 strCount = 0; //get number of strings to search for
			while(inAttrNames[strCount] != nil)
			{
				strCount++;
			}
			escapedStrings = (char **) calloc( strCount + 1, sizeof (char *));
			strCount = 0;
			while(inAttrNames[strCount] != nil)
			{
				recNameLen = strlen(inAttrNames[strCount]);
				escapedStrings[strCount] = (char *)::calloc(1, 2 * recNameLen + 1);
				//assume at most all characters will be escaped
				escapedIndex	= 0;
				originalIndex   = 0;
				while (originalIndex < recNameLen)
				{
					switch (inAttrNames[strCount][originalIndex])
					{
						case '*':
						case '(':
						case ')':
							escapedStrings[strCount][escapedIndex] = '\\';
							++escapedIndex;
							// add \ escape character then fall through and pick up the original character
						default:
							escapedStrings[strCount][escapedIndex] = inAttrNames[strCount][originalIndex];
							++escapedIndex;
							break;
					}//switch (inAttrNames[strCount][originalIndex])
					++originalIndex;
				}//while (originalIndex < recNameLen)
				strCount++;
			}//while(inAttrNames[strCount] != nil)

			//assume that the query is "OR" based ie. meet any of the criteria
			cfStringRef = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("(|"));
			
			//get the first mapping
			numAttributes = 1;
			//KW Note that kDS1RecordName is single valued so we are using kDSNAttrRecordName
			//as a multi-mapped std type which will easily lead to multiple values
			nativeAttrType = MapAttrToLDAPType( inRecType, inConstAttrType, inConfigName, numAttributes, false );
			//would throw if first nil since no more will be found otherwise proceed until nil
			//however simply set to default LDAP native in this case
			//ie. we are trying regardless if kDSNAttrRecordName is mapped or not
			//whether or not "cn" is a good choice is a different story
			if (nativeAttrType == nil) //only for first mapping
			{
				nativeAttrType = strdup("cn");
			}
	
			matchType = (unsigned long) (patternMatch);
			while ( nativeAttrType != nil )
			{
				if (nativeAttrType[0] == '#') //literal mapping
				{
					if (strlen(nativeAttrType) > 1)
					{
						if (DoAnyMatch((const char *)(nativeAttrType+1), escapedStrings, patternMatch))
						{
							if (CFStringGetLength(cfQueryStringRef) == 0)
							{
								CFStringAppendCString(cfQueryStringRef,"(&(objectclass=*)", kCFStringEncodingUTF8);
								objClassAdded = true;
							}
							bGetAllDueToLiteralMapping = true;
							free(nativeAttrType);
							nativeAttrType = nil;
							continue;
						}
					}
				}
				else
				{
					if (bOnceThru)
					{
						if (useWellKnownRecType)
						{
							//need to allow for condition that we want only a single query
							//that uses only a single native type - use the first one - 
							//for perhaps an open record or a write to a specific record
							bOnceThru = false;
							break;
						}
						offset = 0;
					}
					//the multiple values are OR'ed in this search
					CFStringAppendCString(cfStringRef,"(|", kCFStringEncodingUTF8);
					strCount = 0;
					//for each search pattern to match to
					while(escapedStrings[strCount] != nil)
					{
						switch (matchType)
						{
					//		case eDSAnyMatch:
					//			cout << "Pattern match type of <eDSAnyMatch>" << endl;
					//			break;
							case eDSStartsWith:
							case eDSiStartsWith:
								CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,"=", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, escapedStrings[strCount], kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,"*)", kCFStringEncodingUTF8);
								bOnceThru = true;
								break;
							case eDSEndsWith:
							case eDSiEndsWith:
								CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,"=*", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, escapedStrings[strCount], kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
								bOnceThru = true;
								break;
							case eDSContains:
							case eDSiContains:
								CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,"=*", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, escapedStrings[strCount], kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,"*)", kCFStringEncodingUTF8);
								bOnceThru = true;
								break;
							case eDSWildCardPattern:
							case eDSiWildCardPattern:
								//assume the inConstAttrName is wild
								CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,"=", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, inAttrNames[strCount], kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
								bOnceThru = true;
								break;
							case eDSRegularExpression:
							case eDSiRegularExpression:
								//assume inConstAttrName replaces entire wild expression
								CFStringAppendCString(cfStringRef, inAttrNames[strCount], kCFStringEncodingUTF8);
								bOnceThru = true;
								break;
							case eDSExact:
							case eDSiExact:
							default:
								CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,"=", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, escapedStrings[strCount], kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
								bOnceThru = true;
								break;
						} // switch on matchType
						strCount++;
					}//while(escapedStrings[strCount] != nil)
					//the multiple values are OR'ed in this search
					CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
				}
				//cleanup nativeAttrType if needed
				if (nativeAttrType != nil)
				{
					free(nativeAttrType);
					nativeAttrType = nil;
				}
				numAttributes++;
				//get the next mapping
				nativeAttrType = MapAttrToLDAPType( inRecType, inConstAttrType, inConfigName, numAttributes, false );
			} // while ( nativeAttrType != nil )
	
			if (!bGetAllDueToLiteralMapping)
			{
				if (cfStringRef != nil)
				{
					// building search like "sn=name"
					if (offset == 3)
					{
						CFRange	aRangeToDelete;
						aRangeToDelete.location = 1;
						aRangeToDelete.length = 2;			
						CFStringDelete(cfStringRef, aRangeToDelete);
					}
					//building search like "(|(sn=name)(cn=name))"
					else
					{
						CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
					}
					
					CFStringAppend(cfQueryStringRef, cfStringRef);
				}
			}
			
			if (cfStringRef != nil)
			{
				CFRelease(cfStringRef);
				cfStringRef = nil;
			}

			if (escapedStrings != nil)
			{
				strCount = 0;
				while(escapedStrings[strCount] != nil)
				{
					free(escapedStrings[strCount]);
					escapedStrings[strCount] = nil;
					strCount++;
				}
				free(escapedStrings);
				escapedStrings = nil;
			}
	
		} // if (inAttrNames != nil)
	}
	if (objClassAdded)
	{
		CFStringAppendCString(cfQueryStringRef,")", kCFStringEncodingUTF8);
	}
	
	//here we make the char * output in queryfilter
	if (CFStringGetLength(cfQueryStringRef) != 0)
	{
		callocLength = (uInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfQueryStringRef), kCFStringEncodingUTF8) + 1;
		queryFilter = (char *) calloc(1, callocLength);
		CFStringGetCString( cfQueryStringRef, queryFilter, callocLength, kCFStringEncodingUTF8 );
	}

	if (cfQueryStringRef != nil)
	{
		CFRelease(cfQueryStringRef);
		cfQueryStringRef = nil;
	}

	return (queryFilter);
	
} // BuildLDAPQueryMultiFilter


// ---------------------------------------------------------------------------
//	* ParseCompoundExpression
// ---------------------------------------------------------------------------

CFMutableStringRef CLDAPv3Plugin::ParseCompoundExpression ( const char *inConstAttrName, const char *inRecType, char *inConfigName )
{
	CFMutableStringRef	outExpression	= NULL;
	char			   *ourExpressionPtr= nil;
	char			   *ourExp			= nil;
	char			   *attrExpression	= nil;
	uInt32				numChars		= 0;
	bool				bNotDone		= true;
	char			   *attrType		= nil;
	char			   *pattMatch		= nil;
	bool				bMultiMap		= false;
	
	//TODO if there is a search on a single attr type with a single literal mapping - how do we find anything?
	//transfer the special chars "(", ")", "|", "&" directly to the outExpression
	//extract the attr type and replace with the correct mappings
	//if std attr type use ALL native attr maps; if native type use alone; if not prefixed assume native attr type
	//extract the pattern match and use it with all the native maps used for a standard attr type
	
	//NOTE many comments with ( or ) have been added solely to get block completion in PB to work

	if ( ( inConstAttrName != nil ) && ( inRecType != nil ) )
	{
		ourExpressionPtr = (char *) calloc(1, strlen(inConstAttrName) + 1);
		if (ourExpressionPtr != nil)
		{
			ourExp = ourExpressionPtr;
			strcpy(ourExpressionPtr, inConstAttrName);
			outExpression = CFStringCreateMutable(kCFAllocatorDefault, 0);
			//get special chars to start
			numChars = strspn(ourExpressionPtr,LDAPCOMEXPSPECIALCHARS); //number of special chars at start
			if (numChars > 0)
			{
				attrExpression = (char *) calloc(1, numChars + 1);
				if (attrExpression != nil)
				{
					strncpy(attrExpression, ourExpressionPtr, numChars);
					CFStringAppendCString(outExpression, attrExpression, kCFStringEncodingUTF8);
					ourExpressionPtr = ourExpressionPtr + numChars;
					free(attrExpression);
					attrExpression = nil;
				}
			}
			
			//loop over the attr types with their pattern match
			while (bNotDone)
			{
				bMultiMap = false;
				numChars = 0;
				numChars = strcspn(ourExpressionPtr,LDAPCOMEXPSPECIALCHARS); //number of chars in "attrtype=pattmatch"
				if (numChars > 0) //another attr Expression found
				{
					attrExpression = (char *) calloc(1, numChars + 1);
					if (attrExpression != nil)
					{
						strncpy(attrExpression, ourExpressionPtr, numChars);
						ourExpressionPtr = ourExpressionPtr + numChars;
						attrType = strsep(&attrExpression,"=");
						pattMatch= strsep(&attrExpression,"=");
						if ((attrType != nil) && (pattMatch != nil) ) //we have something here
						{
							char *pLDAPAttrType = nil;
							int numAttributes = 1;
							pLDAPAttrType = MapAttrToLDAPType( inRecType, attrType, inConfigName, numAttributes, true );
							if ( pLDAPAttrType != nil)
							{
								if ( (strcasecmp(attrType, pLDAPAttrType) == 0) //no mappings really so reuse input
									|| (strncmp(attrType, kDSNativeAttrTypePrefix, strlen(kDSNativeAttrTypePrefix)) == 0) ) //native type input
								{
									CFStringAppendCString(outExpression, pLDAPAttrType, kCFStringEncodingUTF8);
									CFStringAppendCString(outExpression, "=", kCFStringEncodingUTF8);
									CFStringAppendCString(outExpression, pattMatch, kCFStringEncodingUTF8);
									DSFreeString(pLDAPAttrType);
								}
								else
								{
									CFStringAppendCString(outExpression, "|", kCFStringEncodingUTF8); //)
									bMultiMap = true; //possible multi map that needs to be accounted for - not always the case ie. could be a single map
								}
							}
							while(pLDAPAttrType != nil)
							{
								CFStringAppendCString(outExpression, "(", kCFStringEncodingUTF8); //)
								CFStringAppendCString(outExpression, pLDAPAttrType, kCFStringEncodingUTF8);
								CFStringAppendCString(outExpression, "=", kCFStringEncodingUTF8);
								CFStringAppendCString(outExpression, pattMatch, kCFStringEncodingUTF8); //(
								numAttributes++;
								DSFreeString(pLDAPAttrType);
								pLDAPAttrType = MapAttrToLDAPType( inRecType, attrType, inConfigName, numAttributes, true );
								if (pLDAPAttrType != nil)
								{
									CFStringAppendCString(outExpression, ")", kCFStringEncodingUTF8);
								}
							}
							if (bMultiMap)
							{
								//(
								CFStringAppendCString(outExpression, ")", kCFStringEncodingUTF8);
							}
						} // if ((attrType != nil) && (pattMatch != nil) )
						free(attrExpression);
						attrExpression = nil;
					} //attrExpression != nil
				} //another attr Expression found
				else
				{
					bNotDone = false;
				}
				//get more following special chars
				numChars = 0;
				numChars = strspn(ourExpressionPtr,LDAPCOMEXPSPECIALCHARS); //number of special chars following
				if (numChars > 0)
				{
					attrExpression = (char *) calloc(1, numChars + 1);
					if (attrExpression != nil)
					{
						strncpy(attrExpression, ourExpressionPtr, numChars);
						CFStringAppendCString(outExpression, attrExpression, kCFStringEncodingUTF8);
						ourExpressionPtr = ourExpressionPtr + numChars;
						free(attrExpression);
						attrExpression = nil;
					}
				}
			} // while (bNotDone)
			
			free(ourExp);
			ourExp = nil;
		} //ourExpressionPtr != nil
	}

	if (outExpression != NULL)
	{
		if (CFStringGetLength(outExpression) < 3) //if only "()" then don't return anything
		{
			CFRelease(outExpression);
			outExpression = NULL;
		}
	}

	return( outExpression );

} // ParseCompoundExpression


//------------------------------------------------------------------------------------
//	* GetDirNodeInfo
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetDirNodeInfo ( sGetDirNodeInfo *inData )
{
	sInt32				siResult		= eDSNoErr;
	uInt32				uiOffset		= 0;
	uInt32				uiCntr			= 1;
	uInt32				uiAttrCnt		= 0;
	CAttributeList	   *inAttrList		= nil;
	char			   *pAttrName		= nil;
	char			   *pData			= nil;
	sLDAPContextData	   *pContext		= nil;
	sLDAPContextData	   *pAttrContext	= nil;
	CBuff				outBuff;
	CDataBuff		   *aRecData		= nil;
	CDataBuff		   *aAttrData		= nil;
	CDataBuff		   *aTmpData		= nil;
	sLDAPConfigData	   *pConfig			= nil;

// Can extract here the following:
// kDSAttributesAll
// kDSNAttrNodePath
// kDS1AttrReadOnlyNode
// kDSNAttrAuthMethod
// dsAttrTypeStandard:AcountName
// kDSNAttrDefaultLDAPPaths
// kDS1AttrDistinguishedName
//KW need to add mappings info next

	try
	{
		if ( inData  == nil ) throw( (sInt32) eMemoryError );

		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		inAttrList = new CAttributeList( inData->fInDirNodeInfoTypeList );
		if ( inAttrList == nil ) throw( (sInt32)eDSNullNodeInfoTypeList );
		if (inAttrList->GetCount() == 0) throw( (sInt32)eDSEmptyNodeInfoTypeList );

		siResult = outBuff.Initialize( inData->fOutDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = outBuff.SetBuffType( 'Gdni' );  //can't use 'StdB' since a tRecordEntry is not returned
		if ( siResult != eDSNoErr ) throw( siResult );

		aRecData = new CDataBuff();
		if ( aRecData  == nil ) throw( (sInt32) eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData  == nil ) throw( (sInt32) eMemoryError );
		aTmpData = new CDataBuff();
		if ( aTmpData  == nil ) throw( (sInt32) eMemoryError );
		
		// Set the record name and type
		aRecData->AppendShort( ::strlen( "dsAttrTypeStandard:DirectoryNodeInfo" ) );
		aRecData->AppendString( (char *)"dsAttrTypeStandard:DirectoryNodeInfo" );
		aRecData->AppendShort( ::strlen( "DirectoryNodeInfo" ) );
		aRecData->AppendString( (char *)"DirectoryNodeInfo" );

		while ( inAttrList->GetAttribute( uiCntr++, &pAttrName ) == eDSNoErr )
		{
			//package up all the dir node attributes dependant upon what was asked for
			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, kDSNAttrNodePath ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrNodePath ) );
				aTmpData->AppendString( kDSNAttrNodePath );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count always two
					aTmpData->AppendShort( 2 );
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( "LDAPv3" ) );
					aTmpData->AppendString( (char *)"LDAPv3" );

					char *tmpStr = nil;
					//don't need to retrieve for the case of "generic unknown" so don't check index 0
					// simply always use the pContext->fNodeName since case of registered it is identical to
					if (pContext->fNodeName != nil)
					{
						tmpStr = pContext->fNodeName;
					}
					else
					{
						tmpStr = "Unknown Node Location";
					}
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

				} // fInAttrInfoOnly is false
				else
				{
					aTmpData->AppendShort( 0 );
				}
				
				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			} // kDSAttributesAll or kDSNAttrNodePath
			
			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDS1AttrReadOnlyNode ) == 0) )
			{
				aTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDS1AttrReadOnlyNode ) );
				aTmpData->AppendString( kDS1AttrReadOnlyNode );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count
					aTmpData->AppendShort( 1 );

					//possible for a node to be ReadOnly, ReadWrite, WriteOnly
					//note that ReadWrite does not imply fully readable or writable
					
					if (pContext->bLDAPv2ReadOnly)
					{
						aTmpData->AppendLong( ::strlen( "ReadOnly" ) );
						aTmpData->AppendString( "ReadOnly" );
					}
					else
					{
						aTmpData->AppendLong( ::strlen( "ReadWrite" ) );
						aTmpData->AppendString( "ReadWrite" );
					}
				}
				else
				{
					aTmpData->AppendShort( 0 );
				}

				// Add the attribute length and data
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();
			}
				 
			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, kDSNAttrAuthMethod ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				//KW at some point need to retrieve SASL auth methods from LDAP server if they are available
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrAuthMethod ) );
				aTmpData->AppendString( kDSNAttrAuthMethod );

				if ( inData->fInAttrInfoOnly == false )
				{
					char *tmpStr = nil;
					
					// Attribute value count
					aTmpData->AppendShort( 4 );
					
					tmpStr = (char *) calloc(1, 1+::strlen( kDSStdAuthCrypt ));
					::strcpy( tmpStr, kDSStdAuthCrypt );
					
					// Append first attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );
					tmpStr = nil;

					tmpStr = (char *) calloc(1, 1+::strlen( kDSStdAuthClearText ));
					::strcpy( tmpStr, kDSStdAuthClearText );
					
					// Append second attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );
					tmpStr = nil;
                                        
					tmpStr = (char *) calloc(1, 1+::strlen( kDSStdAuthNodeNativeClearTextOK ));
					::strcpy( tmpStr, kDSStdAuthNodeNativeClearTextOK );
					
					// Append third attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );
					tmpStr = nil;
					
					tmpStr = (char *) calloc(1, 1+::strlen( kDSStdAuthNodeNativeNoClearText ));
					::strcpy( tmpStr, kDSStdAuthNodeNativeNoClearText );
					
					// Append fourth attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );
					tmpStr = nil;
				} // fInAttrInfoOnly is false
				else
				{
					aTmpData->AppendShort( 0 );
				}

				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			
			} // kDSAttributesAll or kDSNAttrAuthMethod

			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, "dsAttrTypeStandard:AccountName" ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( "dsAttrTypeStandard:AccountName" ) );
				aTmpData->AppendString( (char *)"dsAttrTypeStandard:AccountName" );

				if ( inData->fInAttrInfoOnly == false )
				{
					const char *tmpStr = (pContext->fAuthUserName ? pContext->fAuthUserName : "No Account Name");
					
					// Attribute value count
					aTmpData->AppendShort( 1 );
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

				} // fInAttrInfoOnly is false
				else
				{
					aTmpData->AppendShort( 0 );
				}
				
				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			
			} // kDSAttributesAll or dsAttrTypeStandard:AccountName

			if ( ::strcmp( pAttrName, kDSNAttrDefaultLDAPPaths ) == 0 )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrDefaultLDAPPaths ) );
				aTmpData->AppendString( (char *)kDSNAttrDefaultLDAPPaths );

				if ( inData->fInAttrInfoOnly == false )
				{
					char **defaultLDAPNodes = nil;
					uInt32 aDefaultLDAPNodeCount = 0;
					defaultLDAPNodes = gpConfigFromXML->GetDefaultLDAPNodeStrings(aDefaultLDAPNodeCount);
					
					// Attribute value count
					aTmpData->AppendShort( aDefaultLDAPNodeCount );

					if ( (aDefaultLDAPNodeCount > 0) && (defaultLDAPNodes != nil) )
					{
						int listIndex = 0;
						for (listIndex=0; defaultLDAPNodes[listIndex] != nil; listIndex++)
						{
							// Append attribute value
							aTmpData->AppendLong( strlen( defaultLDAPNodes[listIndex] ) );
							aTmpData->AppendString( defaultLDAPNodes[listIndex] );
						}
					}
					DSFreeStringList(defaultLDAPNodes);
				} // fInAttrInfoOnly is false
				else
				{
					aTmpData->AppendShort( 0 );
				}
				
				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			
			} // kDSNAttrDefaultLDAPPaths

			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, kDS1AttrDistinguishedName ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDS1AttrDistinguishedName ) );
				aTmpData->AppendString( (char *)kDS1AttrDistinguishedName );

				if ( inData->fInAttrInfoOnly == false )
				{
					pConfig = gpConfigFromXML->ConfigWithNodeNameLock( pContext->fNodeName );

					char *tmpStr = nil;
					if (pConfig != nil && pConfig->fUIName )
					{
						tmpStr = pConfig->fUIName;
					}
					else
					{
						tmpStr = "No Display Name";
					}
					
					// Attribute value count
					aTmpData->AppendShort( 1 );
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					if( pConfig )
					{
						gpConfigFromXML->ConfigUnlock( pConfig );
					}
				} // fInAttrInfoOnly is false
				else
				{
					aTmpData->AppendShort( 0 );
				}
				
				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			
			} // kDSAttributesAll or kDS1AttrDistinguishedName

			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDSNAttrRecordType ) == 0) )
			{
				aTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrRecordType ) );
				aTmpData->AppendString( kDSNAttrRecordType );

				if ( inData->fInAttrInfoOnly == false )
				{
					int valueCount = 0;
					int i = 0;

					pConfig = gpConfigFromXML->ConfigWithNodeNameLock( pContext->fNodeName );

					if (pConfig != nil && pConfig->fRecordAttrMapDict != nil )
					{
						valueCount = CFDictionaryGetCount( pConfig->fRecordAttrMapDict );
					}
					
					aTmpData->AppendShort( valueCount );

					if( valueCount != 0 )
					{
						CFStringRef *keys = (CFStringRef *)calloc( valueCount, sizeof(CFStringRef) );
						
						CFDictionaryGetKeysAndValues( pConfig->fRecordAttrMapDict, (const void **)keys, NULL );
						for ( i = 0; i < valueCount; i++ )
						{
							char	tempBuffer[1024];

							// just get the string in to the temp buffer and put it in the list
							CFStringGetCString( keys[i], tempBuffer, 1024, kCFStringEncodingUTF8 );
							
							aTmpData->AppendLong( ::strlen( tempBuffer ) );
							aTmpData->AppendString( tempBuffer );
						}
						
						DSFree( keys );
					}
					if( pConfig )
					{
						gpConfigFromXML->ConfigUnlock( pConfig );
					}
				}
				else
				{
					aTmpData->AppendShort( 0 );
				}

				// Add the attribute length and data
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();
			}
			
			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, "dsAttrTypeStandard:TrustInformation" ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
				
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( "dsAttrTypeStandard:TrustInformation" ) );
				aTmpData->AppendString( "dsAttrTypeStandard:TrustInformation" );
				
				if ( inData->fInAttrInfoOnly == false )
				{
					pConfig = gpConfigFromXML->ConfigWithNodeNameLock( pContext->fNodeName );
					
					if (pConfig != nil)
					{
						// use sets of true & false, don't rely on compiler setting 1 for comparisons
						bool bAuthenticated = (pConfig->bSecureUse && pConfig->fServerAccount != NULL && pConfig->fServerPassword != NULL);
						bool bAnonymous = (bAuthenticated ? false : true); // we can't do ! because we need a 1 or 0
						bool bFullTrust = false;
						bool bPartialTrust = false;
						bool bDHCP = pConfig->bUseAsDefaultLDAP;
						bool bEncryption = (pConfig->bIsSSL || (pConfig->fSecurityLevel & kSecPacketEncryption));
						
						if( bAuthenticated )
						{
							bFullTrust = ((pConfig->fSecurityLevel & (kSecManInMiddle | kSecPacketSigning)) == (kSecManInMiddle | kSecPacketSigning));
							
							// let's see if we have a partial trust flags in place..
							if( bFullTrust == false )
							{
								bPartialTrust = ((pConfig->fSecurityLevel & kSecManInMiddle) == kSecManInMiddle);
							}
						}
						
						gpConfigFromXML->ConfigUnlock( pConfig );

						aTmpData->AppendShort( bAuthenticated + bAnonymous + bFullTrust + bPartialTrust + bDHCP + bEncryption );
						
						if( bAuthenticated )
						{
							aTmpData->AppendLong( ::strlen( "Authenticated" ) );
							aTmpData->AppendString( "Authenticated" );
						}
						else if( bAnonymous )
						{
							aTmpData->AppendLong( ::strlen( "Anonymous" ) );
							aTmpData->AppendString( "Anonymous" );
						}

						if( bFullTrust )
						{
							aTmpData->AppendLong( ::strlen( "FullTrust" ) );
							aTmpData->AppendString( "FullTrust" );
						}
						else if( bPartialTrust )
						{
							aTmpData->AppendLong( ::strlen( "PartialTrust" ) );
							aTmpData->AppendString( "PartialTrust" );
						}
						
						if( bEncryption )
						{
							aTmpData->AppendLong( ::strlen( "Encryption" ) );
							aTmpData->AppendString( "Encryption" );
						}

						if( bDHCP )
						{
							aTmpData->AppendLong( ::strlen( "DHCP" ) );
							aTmpData->AppendString( "DHCP" );
						}
					}
					else
					{
						aTmpData->AppendShort( 0 );
					}
				}
				else
				{
					aTmpData->AppendShort( 0 );
				}
				
				// Add the attribute length and data
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
				
				// Clear the temp block
				aTmpData->Clear();
			}
		} // while

		aRecData->AppendShort( uiAttrCnt );
		if (uiAttrCnt > 0)
		{
			aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
		}

		outBuff.AddData( aRecData->GetData(), aRecData->GetLength() );
		inData->fOutAttrInfoCount = uiAttrCnt;

		pData = outBuff.GetDataBlock( 1, &uiOffset );
		if ( pData != nil )
		{
			pAttrContext = new sLDAPContextData;
			if ( pAttrContext  == nil ) throw( (sInt32) eMemoryAllocError );
			
			pAttrContext->fNodeName = strdup( pContext->fNodeName );

		//add to the offset for the attr list the length of the GetDirNodeInfo fixed record labels
//		record length = 4
//		aRecData->AppendShort( ::strlen( "dsAttrTypeStandard:DirectoryNodeInfo" ) ); = 2
//		aRecData->AppendString( "dsAttrTypeStandard:DirectoryNodeInfo" ); = 36
//		aRecData->AppendShort( ::strlen( "DirectoryNodeInfo" ) ); = 2
//		aRecData->AppendString( "DirectoryNodeInfo" ); = 17
//		total adjustment = 4 + 2 + 36 + 2 + 17 = 61

			pAttrContext->offset = uiOffset + 61;

			gLDAPContextTable->AddItem( inData->fOutAttrListRef, pAttrContext );
		}
        else
        {
            siResult = eDSBufferTooSmall;
        }
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if ( inAttrList != nil )
	{
		delete( inAttrList );
		inAttrList = nil;
	}
	if ( aRecData != nil )
	{
		delete( aRecData );
		aRecData = nil;
	}
	if ( aAttrData != nil )
	{
		delete( aAttrData );
		aAttrData = nil;
	}
	if ( aTmpData != nil )
	{
		delete( aTmpData );
		aTmpData = nil;
	}

	return( siResult );

} // GetDirNodeInfo

//------------------------------------------------------------------------------------
//	* OpenRecord
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::OpenRecord ( sOpenRecord *inData )
{
	sInt32				siResult		= eDSNoErr;
	tDataNodePtr		pRecName		= nil;
	tDataNodePtr		pRecType		= nil;
	char			   *pLDAPRecType	= nil;
	sLDAPContextData   *pContext		= nil;
	sLDAPContextData   *pRecContext		= nil;
    char			   *queryFilter		= nil;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	int					numRecTypes		= 1;
	bool				bResultFound	= false;
    sLDAPConfigData	   *pConfig			= nil;
    int					searchTO   		= 0;
	bool				bOCANDGroup		= false;
	CFArrayRef			OCSearchList	= nil;
	ber_int_t			scope			= LDAP_SCOPE_SUBTREE;
	sInt32				searchResult	= eDSNoErr;
	char			  **attrs			= nil;
	int					ldapMsgId		= 0;


	try
	{
		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		pRecType = inData->fInRecType;
		if ( pRecType  == nil ) throw( (sInt32)eDSNullRecType );

		pRecName = inData->fInRecName;
		if ( pRecName  == nil ) throw( (sInt32)eDSNullRecName );

		if (pRecName->fBufferLength == 0) throw( (sInt32)eDSEmptyRecordNameList );
		if (pRecType->fBufferLength == 0) throw( (sInt32)eDSEmptyRecordTypeList );

		//search for the specific LDAP record now
		
		//retrieve the config data
		pConfig = gpConfigFromXML->ConfigWithNodeNameLock( pContext->fNodeName );
		if (pConfig != nil)
		{
			searchTO	= pConfig->fSearchTimeout;
			
			gpConfigFromXML->ConfigUnlock( pConfig );
			pConfig = NULL;
		}
		
		//let us only ask for the record name in this search for the record
		attrs = MapAttrToLDAPTypeArray( pRecType->fBufferData, kDSNAttrRecordName, pContext->fNodeName );
				
        // we will search over all the rectype mappings until we find the first
        // result for the search criteria in the queryfilter
        numRecTypes = 1;
		bOCANDGroup = false;
		if (OCSearchList != nil)
		{
			CFRelease(OCSearchList);
			OCSearchList = nil;
		}
		pLDAPRecType = MapRecToLDAPType( (const char *)(pRecType->fBufferData), pContext->fNodeName, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
		//only throw this for first time since we need at least one map
		if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
		
		while ( (pLDAPRecType != nil) && (!bResultFound) )
		{

			//build the record query string
			//removed the use well known map only condition ie. true to false
			queryFilter = BuildLDAPQueryFilter(	(char *)kDSNAttrRecordName,
												pRecName->fBufferData,
												eDSExact,
												pContext->fNodeName,
												false,
												(const char *)(pRecType->fBufferData),
												pLDAPRecType,
												bOCANDGroup,
												OCSearchList );
			if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );

			searchResult = DSDoRetrieval (	pLDAPRecType,
											attrs,
											pContext,
											scope,
											fLDAPSessionMgr,
											queryFilter,
											ldapReturnCode,
											searchTO,
											result,
											bResultFound,
											ldapMsgId );

			if ( queryFilter != nil )
			{
				delete( queryFilter );
				queryFilter = nil;
			}

			if (pLDAPRecType != nil)
			{
				delete (pLDAPRecType);
				pLDAPRecType = nil;
			}
			numRecTypes++;
			bOCANDGroup = false;
			if (OCSearchList != nil)
			{
				CFRelease(OCSearchList);
				OCSearchList = nil;
			}
			pLDAPRecType = MapRecToLDAPType( (const char *)(pRecType->fBufferData), pContext->fNodeName, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
		} // while ( (pLDAPRecType != nil) && (!bResultFound) )

		if (	(bResultFound) &&
				( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
		{
			fLDAPSessionMgr.SessionMapMutexLock();
			pRecContext = new sLDAPContextData( *pContext );
			fLDAPSessionMgr.SessionMapMutexUnlock();
			if ( pRecContext  == nil ) throw( (sInt32) eMemoryAllocError );
	        
			pRecContext->fType = 2;
			
			if (pRecType->fBufferData != nil)
			{
				pRecContext->fOpenRecordType = strdup( pRecType->fBufferData );
			}
			
			if (pRecName->fBufferData != nil)
			{
				pRecContext->fOpenRecordName = strdup( pRecName->fBufferData );
			}
			
			LDAP *aHost = fLDAPSessionMgr.LockSession(pRecContext);
			if (aHost != nil)
			{
				//get the ldapDN here
				pRecContext->fOpenRecordDN = ldap_get_dn(aHost, result);
				DSSearchCleanUp(aHost, ldapMsgId);
			}
			fLDAPSessionMgr.UnLockSession(pRecContext);
		
			gLDAPContextTable->AddItem( inData->fOutRecRef, pRecContext );
		} // if bResultFound and ldapReturnCode okay
		else
		{
	     	siResult = searchResult;
		}
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	
	if ( result != nil )
	{
		ldap_msgfree( result );
		result = nil;
	}
	
	if ( pLDAPRecType != nil )
	{
		delete( pLDAPRecType );
		pLDAPRecType = nil;
	}

	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}
	if (attrs != nil)
	{
		int aIndex = 0;
		while (attrs[aIndex] != nil)
		{
			free(attrs[aIndex]);
			attrs[aIndex] = nil;
			aIndex++;
		}
		free(attrs);
		attrs = nil;
	}

	return( siResult );

} // OpenRecord


//------------------------------------------------------------------------------------
//	* CloseRecord
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::CloseRecord ( sCloseRecord *inData )
{
	sInt32				siResult	=	eDSNoErr;
	sLDAPContextData   *pContext	=	nil;

	try
	{
		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );
		
		gLDAPContextTable->RemoveItem( inData->fInRecRef );
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // CloseRecord


//------------------------------------------------------------------------------------
//	* FlushRecord
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::FlushRecord ( sFlushRecord *inData )
{
	sInt32				siResult	=	eDSNoErr;
	sLDAPContextData   *pContext	=	nil;

	try
	{
		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( pContext->bLDAPv2ReadOnly ) throw( (sInt32)eDSReadOnly);
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // FlushRecord


//------------------------------------------------------------------------------------
//	* DeleteRecord
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DeleteRecord ( sDeleteRecord *inData, bool inDeleteCredentials )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPContextData	   *pRecContext		= nil;
	char				   *pUserID			= NULL;
	char				   *pAdminID		= NULL;
	char				   *pServerAddress	= NULL;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( pRecContext->bLDAPv2ReadOnly ) throw( (sInt32)eDSReadOnly);

		if ( pRecContext->fOpenRecordDN  == nil ) throw( (sInt32)eDSNullRecName );
		
		// we need to prep before we delete the account cause we need some information in advance
		if ( inDeleteCredentials && pRecContext->fOpenRecordName != NULL && pRecContext->fOpenRecordType != NULL && pRecContext->fLDAPNodeStruct->fLDAPUserName )
		{
			char **dn = ldap_explode_dn( pRecContext->fLDAPNodeStruct->fLDAPUserName, 1 );

			pUserID = GetPWSIDforRecord( pRecContext, pRecContext->fOpenRecordName, pRecContext->fOpenRecordType );
			if ( pUserID != NULL )
			{
				// now lets stop at the ","
				char *pComma = strchr( pUserID, ',' );
				if ( pComma != NULL )
				{
					*pComma = '\0';
				}
			}
			pAdminID = GetPWSIDforRecord( pRecContext, dn[0], kDSStdRecordTypeUsers );
			
			if ( pAdminID != NULL )
			{
				pServerAddress = strchr( pAdminID, ':' );
				if ( pServerAddress != NULL )
				{
					pServerAddress++; // skip past the ':'
				}
			}
			
			ldap_value_free( dn );
			dn = NULL;
		}

		//KW revisit for what degree of error return we need to provide
		//if LDAP_NOT_ALLOWED_ON_NONLEAF then this is not a leaf in the hierarchy ie. leaves need to go first
		//if LDAP_INVALID_CREDENTIALS or ??? then don't have authority so use eDSPermissionError
		//if LDAP_NO_SUCH_OBJECT then eDSRecordNotFound
		//so for now we return simply  eDSPermissionError if ANY error
		LDAP *aHost = fLDAPSessionMgr.LockSession(pRecContext);
		if ( (aHost != nil) && ( ldap_delete_s( aHost, pRecContext->fOpenRecordDN) != LDAP_SUCCESS ) )
		{
			siResult = eDSPermissionError;
		}
		fLDAPSessionMgr.UnLockSession(pRecContext);
		
		// if we want to delete the credentials too for PWS
		if ( siResult == eDSNoErr && pUserID != NULL && pAdminID != NULL && pServerAddress != NULL )
		{
			tDirReference		dsRef = 0;
			tDirNodeReference	nodeRef	= 0;

			int error = ::dsOpenDirService( &dsRef );
			if ( error == eDSNoErr )
			{
				char *nodeName = (char *)calloc(1,strlen(pServerAddress)+sizeof("/PasswordServer/")+1);

				if( nodeName != NULL )
				{
					sprintf( nodeName, "/PasswordServer/%s", pServerAddress );
					error = PWOpenDirNode( dsRef, nodeName, &nodeRef );
					
					free( nodeName );
					nodeName = NULL;
				}
				else
				{
					error = eMemoryAllocError;
				}
			}
            
			if( error == eDSNoErr )
			{
				int				iUserIDLen	= strlen( pUserID ) + 1; // include the NULL terminator
				int				iAdminIDLen	= strlen( pAdminID ) + 1; // include the NULL terminator
				tDataBufferPtr	pAuthMethod = dsDataNodeAllocateString( nodeRef, kDSStdAuthDeleteUser );
				tDataBufferPtr	pStepData	= dsDataBufferAllocatePriv( sizeof(long) + iAdminIDLen + 
																		sizeof(long) + pRecContext->fLDAPNodeStruct->fLDAPCredentialsLen +
																		sizeof(long) + iUserIDLen );
				tDataBufferPtr	pStepDataResp = dsDataBufferAllocatePriv( 1024 ); // shouldn't be needed, but just in case
				
				char	*pWorkingBuff = pStepData->fBufferData;
				
				// put the admin ID in first
				*((long *)pWorkingBuff) = iAdminIDLen;
				pWorkingBuff += sizeof(long);
				bcopy( pAdminID, pWorkingBuff, iAdminIDLen );
				pWorkingBuff += iAdminIDLen;
				
				// now put the credentials in there
				*((long *)pWorkingBuff) = pRecContext->fLDAPNodeStruct->fLDAPCredentialsLen;
				pWorkingBuff += sizeof(long);
				bcopy( pRecContext->fLDAPNodeStruct->fLDAPCredentials, pWorkingBuff, pRecContext->fLDAPNodeStruct->fLDAPCredentialsLen );
				pWorkingBuff += pRecContext->fLDAPNodeStruct->fLDAPCredentialsLen;
				
				// now put the user we're deleting in place
				*((long *)pWorkingBuff) = iUserIDLen;
				pWorkingBuff += sizeof(long);
				bcopy( pUserID, pWorkingBuff, iUserIDLen );
				pWorkingBuff += iUserIDLen;
				
				// set the length to the size
				pStepData->fBufferLength = pWorkingBuff - pStepData->fBufferData;
				
				// let's attempt the delete now
				int result = dsDoDirNodeAuth( nodeRef, pAuthMethod, true, pStepData, pStepDataResp, NULL );
				
				DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Attempt to delete PWS ID associated with record returned status %d", result );
				
				dsDataNodeDeAllocate( nodeRef, pAuthMethod );
				dsDataNodeDeAllocate( nodeRef, pStepData );
				dsDataNodeDeAllocate( nodeRef, pStepDataResp );
			}
			
			if ( nodeRef )
			{
				dsCloseDirNode( nodeRef );
			}
			
			if ( dsRef )
			{
				dsCloseDirService( dsRef );
			}
		}
		
		gLDAPContextTable->RemoveItem( inData->fInRecRef );
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	DSFreeString( pAdminID );
	DSFreeString( pUserID );

	return( siResult );
} // DeleteRecord


//------------------------------------------------------------------------------------
//	* AddAttribute
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::AddAttribute( sAddAttribute *inData )
{
	sInt32					siResult		= eDSNoErr;

	siResult = AddValue( inData->fInRecRef, inData->fInNewAttr, inData->fInFirstAttrValue );

	return( siResult );

} // AddAttribute


//------------------------------------------------------------------------------------
//	* AddAttributeValue
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::AddAttributeValue( sAddAttributeValue *inData )
{
	sInt32					siResult		= eDSNoErr;

	siResult = AddValue( inData->fInRecRef, inData->fInAttrType, inData->fInAttrValue );

	return( siResult );

} // AddAttributeValue


//------------------------------------------------------------------------------------
//	* SetAttributes
//  to be used by custom plugin calls
//  caller owns the CFDictionary and needs to clean it up
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::SetAttributes ( unsigned long inRecRef, CFDictionaryRef inDict )
{
	sInt32					siResult		= eDSNoErr;
	char				   *attrTypeStr		= nil;
	char				   *attrTypeLDAPStr	= nil;
	sLDAPContextData	   *pRecContext		= nil;
	uInt32					attrCount		= 0;
	uInt32					attrIndex		= 0;
	uInt32					valCount		= 0;
	uInt32					valIndex		= 0;
	struct berval		  **newValues		= nil;
	LDAPMod				   *mods[128];				//pick 128 since it seems unlikely we will ever get to 126 required attrs
	int						ldapReturnCode	= 0;
	char				   *attrValue		= nil;
	uInt32					attrLength		= 0;
	LDAP				   *aHost			= nil;
	uInt32					callocLength	= 0;
	bool					bGotIt			= false;
	CFStringRef				keyCFString		= NULL;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( pRecContext->bLDAPv2ReadOnly ) throw( (sInt32)eDSReadOnly);

		if ( pRecContext->fOpenRecordDN == nil ) throw( (sInt32)eDSNullRecName );
		if ( CFGetTypeID(inDict) != CFDictionaryGetTypeID() ) throw( (sInt32)eDSInvalidBuffFormat );

		for (uInt32 modsIndex=0; modsIndex < 128; modsIndex++)
		{
			mods[modsIndex] = NULL;
		}

		//find out how many attrs need to be set
		attrCount = (uInt32) CFDictionaryGetCount(inDict);
		char** keys = (char**)calloc( attrCount + 1, sizeof( void* ) );
		//get a linked list of the attrs so we can iterate over them
		CFDictionaryGetKeysAndValues( inDict, (const void**)keys, NULL );
		
		//loop over attrs in the dictionary
		attrIndex = 0;
		for( uInt32 keyIndex = 0; keyIndex < attrCount; keyIndex++ )
		{
			keyCFString = NULL;
			if ( !(CFGetTypeID((CFStringRef)keys[keyIndex]) == CFStringGetTypeID() ) )
			{
				continue; //skip this one and do not increment attrIndex
			}
			else
			{
				keyCFString = (CFStringRef)keys[keyIndex];
			}

			attrTypeStr = (char *)CFStringGetCStringPtr( keyCFString, kCFStringEncodingUTF8 );
			if (attrTypeStr == nil)
			{
				callocLength = (uInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(keyCFString), kCFStringEncodingUTF8) + 1;
				attrTypeStr = (char *)::calloc( 1, callocLength );
				if ( attrTypeStr == nil ) throw( (sInt32)eMemoryError );

				// Convert it to a regular 'C' string 
				bGotIt = CFStringGetCString( keyCFString, attrTypeStr, callocLength, kCFStringEncodingUTF8 );
				if (bGotIt == false) throw( (sInt32)eMemoryError );
			}

			//get the first mapping
			attrTypeLDAPStr = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), attrTypeStr, pRecContext->fNodeName, 1, true );
			//throw if first nil since we only use the first native type to write to
			//skip everything if a single one is incorrect?
			if ( attrTypeLDAPStr == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			CFArrayRef valuesCFArray = (CFArrayRef)CFDictionaryGetValue( inDict, keyCFString );
			if ( !(CFGetTypeID(valuesCFArray) == CFArrayGetTypeID() ) )
			{
				if (bGotIt)
				{
					DSFreeString(attrTypeStr);
				}
				continue; //skip this one and free up the attr string if required
			}
			valCount	= (uInt32) CFArrayGetCount( valuesCFArray );
			newValues	= (struct berval**) calloc(1, (valCount + 1) *sizeof(struct berval *) );
			
			valIndex = 0;
			for (uInt32 i = 0; i < valCount; i++ ) //extract the values out of the valuesCFArray
			{
				//need to determine whether the value is either string or data
				CFTypeRef valueCFType = CFArrayGetValueAtIndex( valuesCFArray, (CFIndex)i );
				
				if ( CFGetTypeID(valueCFType) == CFStringGetTypeID() )
				{
					CFStringRef valueCFString = (CFStringRef)valueCFType;
					
					callocLength = (uInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(valueCFString), kCFStringEncodingUTF8) + 1;
					attrValue = (char *)::calloc( 1, callocLength );
					if ( attrValue == nil ) throw( (sInt32)eMemoryError );

					// Convert it to a regular 'C' string 
					bool bGotValue = CFStringGetCString( valueCFString, attrValue, callocLength, kCFStringEncodingUTF8 );
					if (bGotValue == false) throw( (sInt32)eMemoryError );

					attrLength = strlen(attrValue);
					newValues[valIndex] = (struct berval*) calloc(1, sizeof(struct berval) );
					newValues[valIndex]->bv_val = attrValue;
					newValues[valIndex]->bv_len = attrLength;
					valIndex++;
					attrValue = nil;
				}
				else if ( CFGetTypeID(valueCFType) == CFDataGetTypeID() )
				{
					CFDataRef valueCFData = (CFDataRef)valueCFType;
					
					attrLength = (uInt32) CFDataGetLength(valueCFData);
					attrValue = (char *)::calloc( 1, attrLength + 1 );
					if ( attrValue == nil ) throw( (sInt32)eMemoryError );
					
					CFDataGetBytes( valueCFData, CFRangeMake(0,attrLength), (UInt8*)attrValue );

					newValues[valIndex] = (struct berval*) calloc(1, sizeof(struct berval) );
					newValues[valIndex]->bv_val = attrValue;
					newValues[valIndex]->bv_len = attrLength;
					valIndex++;
					attrValue = nil;
				}
			} // for each value provided
			
			if (valIndex > 0) //means we actually have something to add
			{
				//create this mods entry
				mods[attrIndex] = (LDAPMod *) calloc( 1, sizeof(LDAPMod *) );
				mods[attrIndex]->mod_op		= LDAP_MOD_REPLACE | LDAP_MOD_BVALUES;
				mods[attrIndex]->mod_type	= attrTypeLDAPStr;
				mods[attrIndex]->mod_bvalues= newValues;
				attrTypeLDAPStr = nil;
				attrIndex++;
				if (attrIndex == 127) //we need terminating NULL for the list
				{
					if (bGotIt)
					{
						DSFreeString(attrTypeStr);
					}
					break; //this is all we modify ie. first 126 attrs in this set - we will never hit this
				}
			}
			if (bGotIt)
			{
				DSFreeString(attrTypeStr);
			}
		} //for( uInt32 keyIndex = 0; keyIndex < attrCount; keyIndex++ )

		aHost = fLDAPSessionMgr.LockSession(pRecContext);
		if (aHost != nil)
		{
			ldapReturnCode = ldap_modify_s( aHost, pRecContext->fOpenRecordDN, mods);
		}
		else
		{
			ldapReturnCode = LDAP_LOCAL_ERROR;
		}
		fLDAPSessionMgr.UnLockSession(pRecContext);
		if ( ldapReturnCode != LDAP_SUCCESS )
		{
			siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSAttributeNotFound );
		}
		
		for (uInt32 modsIndex=0; ((modsIndex < 128) && (mods[modsIndex] != NULL)); modsIndex++)
		{
			DSFreeString(mods[modsIndex]->mod_type)
			newValues = mods[modsIndex]->mod_bvalues;
			if (newValues != NULL)
			{
				ldap_value_free_len(newValues);
				newValues = NULL;
			}
			mods[modsIndex] = NULL;
		}
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	DSFreeString( attrTypeLDAPStr );
		
	return( siResult );

} // SetAttributes


//------------------------------------------------------------------------------------
//	* RemoveAttribute
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::RemoveAttribute ( sRemoveAttribute *inData )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPContextData	   *pRecContext		= nil;
	int						numAttributes	= 1;
	char				   *pLDAPAttrType	= nil;
	LDAPMod				   *mods[2];
	int						ldapReturnCode	= 0;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( pRecContext->bLDAPv2ReadOnly ) throw( (sInt32)eDSReadOnly);

		if ( pRecContext->fOpenRecordDN == nil ) throw( (sInt32)eDSNullRecName );
		if ( inData->fInAttribute->fBufferData == nil ) throw( (sInt32)eDSNullAttributeType );

		//get the first mapping
		numAttributes = 1;
		pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), inData->fInAttribute->fBufferData, pRecContext->fNodeName, numAttributes, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

		mods[1] = NULL;
		//do all the mapped native types and simply collect the last error if there is one
		while ( pLDAPAttrType != nil )
		{
			//create this mods entry
			{
				LDAPMod	mod;
				mod.mod_op		= LDAP_MOD_DELETE;
				mod.mod_type	= pLDAPAttrType;
				mod.mod_values	= NULL;
				mods[0]			= &mod;
				
				LDAP *aHost = fLDAPSessionMgr.LockSession(pRecContext);
				if (aHost != nil)
				{
					ldapReturnCode = ldap_modify_s( aHost, pRecContext->fOpenRecordDN, mods);
				}
				else
				{
					ldapReturnCode = LDAP_LOCAL_ERROR;
				}
				fLDAPSessionMgr.UnLockSession(pRecContext);
				if ( ( ldapReturnCode == LDAP_NO_SUCH_ATTRIBUTE ) || ( ldapReturnCode == LDAP_UNDEFINED_TYPE ) )
				{
					siResult = eDSNoErr;
				}
				else if ( ldapReturnCode != LDAP_SUCCESS )
				{
					siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSAttributeNotFound );
				}
			}

			//cleanup pLDAPAttrType if needed
			DSFreeString(pLDAPAttrType);
			numAttributes++;
			//get the next mapping
			pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), inData->fInAttribute->fBufferData, pRecContext->fNodeName, numAttributes, true );
		} // while ( pLDAPAttrType != nil )

	}

	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	return( siResult );

} // RemoveAttribute


//------------------------------------------------------------------------------------
//	* AddValue
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::AddValue ( uInt32 inRecRef, tDataNodePtr inAttrType, tDataNodePtr inAttrValue )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPContextData	   *pRecContext		= nil;
	char				   *attrValue		= nil;
	uInt32					attrLength		= 0;
	struct berval			bval;
	struct berval			*bvals[2];
	int						numAttributes	= 1;
	char				   *pLDAPAttrType	= nil;
	LDAPMod				   *mods[2];
	int						ldapReturnCode	= 0;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( pRecContext->bLDAPv2ReadOnly ) throw( (sInt32)eDSReadOnly);

		if ( pRecContext->fOpenRecordDN == nil ) throw( (sInt32)eDSNullRecName );
		if ( inAttrType->fBufferData == nil ) throw( (sInt32)eDSNullAttributeType );

		//build the mods entry to pass into ldap_modify_s
		if ( (inAttrValue == nil) || (inAttrValue->fBufferLength < 1) )
		{
			//don't allow empty values since the ldap call will fail
			throw( (sInt32)eDSEmptyAttributeValue );
		}
		else
		{
			attrLength = inAttrValue->fBufferLength;
			attrValue = (char *) calloc(1, 1 + attrLength);
			memcpy(attrValue, inAttrValue->fBufferData, attrLength);
			
		}
		bval.bv_val = attrValue;
		bval.bv_len = attrLength;
		bvals[0]	= &bval;
		bvals[1]	= NULL;

		//get the first mapping
		numAttributes = 1;
		pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), inAttrType->fBufferData, pRecContext->fNodeName, numAttributes, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

		mods[1] = NULL;
		//ONLY add to the FIRST mapped native type
		if ( pLDAPAttrType != nil )
		{
			//create this mods entry
			{
				LDAPMod	mod;
				mod.mod_op		= LDAP_MOD_ADD | LDAP_MOD_BVALUES;
				mod.mod_type	= pLDAPAttrType;
				mod.mod_bvalues	= bvals;
				mods[0]			= &mod;
				
				LDAP *aHost = fLDAPSessionMgr.LockSession(pRecContext);
				if (aHost != nil)
				{
					ldapReturnCode = ldap_modify_s( aHost, pRecContext->fOpenRecordDN, mods);
				}
				else
				{
					ldapReturnCode = LDAP_LOCAL_ERROR;
				}
				fLDAPSessionMgr.UnLockSession(pRecContext);
				if ( ldapReturnCode != LDAP_SUCCESS )
				{
					siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSAttributeNotFound );
				}
			}

			//cleanup pLDAPAttrType if needed
			if (pLDAPAttrType != nil)
			{
				delete (pLDAPAttrType);
				pLDAPAttrType = nil;
			}
		} // if ( pLDAPAttrType != nil )
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	DSFree(attrValue);
		
	return( siResult );

} // AddValue


//------------------------------------------------------------------------------------
//	* SetRecordName
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::SetRecordName ( sSetRecordName *inData )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPContextData	   *pRecContext		= nil;
	char				   *pLDAPAttrType	= nil;
	int						ldapReturnCode	= 0;
	tDataNodePtr			pRecName		= nil;
	char				   *ldapRDNString	= nil;
	uInt32					ldapRDNLength	= 0;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( pRecContext->bLDAPv2ReadOnly ) throw( (sInt32)eDSReadOnly);

		//here we are going to assume that the name given to create this record
		//will fill out the DN with the first recordname native type
		pRecName = inData->fInNewRecName;
		if ( pRecName  == nil ) throw( (sInt32)eDSNullRecName );

		if (pRecName->fBufferLength == 0) throw( (sInt32)eDSEmptyRecordNameList );

		//get ONLY the first record name mapping
		pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), kDSNAttrRecordName, pRecContext->fNodeName, 1, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable
		
		
		//if first native map for kDSNAttrRecordName is "cn" then the DN will be built as follows:
		//"cn = pRecName->fBufferData, pLDAPRecType"
		//special cars in ldapRDNString need to be escaped
		ldapRDNLength = strlen(pLDAPAttrType) + 1 + 2*pRecName->fBufferLength;
		ldapRDNString = (char *)calloc(1, ldapRDNLength + 1);
		strcpy(ldapRDNString,pLDAPAttrType);
		strcat(ldapRDNString,"=");
		char *escapedString = BuildEscapedRDN(pRecName->fBufferData);
		strcat(ldapRDNString,escapedString);
		DSFreeString(escapedString);
		
		//KW looks like for v3 we must use ldap_rename API instead of ldap_modrdn2_s for v2

//		ldapReturnCode = ldap_modrdn2_s( pRecContext->fHost, pRecContext->fOpenRecordDN, ldapRDNString, 1);
		LDAP *aHost = fLDAPSessionMgr.LockSession(pRecContext);
		if (aHost != nil)
		{
			ldapReturnCode = ldap_rename_s( aHost, pRecContext->fOpenRecordDN, ldapRDNString, NULL, 1, NULL, NULL);
		}
		else
		{
			ldapReturnCode = LDAP_LOCAL_ERROR;
		}
		fLDAPSessionMgr.UnLockSession(pRecContext);
		if ( ldapReturnCode == LDAP_ALREADY_EXISTS )
		{
			siResult = eDSNoErr; // already has this name
		}
		else if ( ldapReturnCode != LDAP_SUCCESS )
		{
			siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSRecordNotFound );
		}
		else //let's update our context data since we succeeded
		{
			DSFreeString(pRecContext->fOpenRecordName);
			pRecContext->fOpenRecordName = (char *)calloc(1, 1+::strlen(pRecName->fBufferData));
			::strcpy( pRecContext->fOpenRecordName, pRecName->fBufferData );
			
			char *newldapDN		= nil;
			char *pLDAPRecType	= nil;
			pLDAPRecType = MapRecToLDAPType( (const char *)(pRecContext->fOpenRecordType), pRecContext->fNodeName, 1, nil, nil, nil );
			if (pLDAPRecType != nil)
			{
				newldapDN = (char *) calloc(1, 1 + strlen(ldapRDNString) + 2 + strlen(pLDAPRecType));
				strcpy(newldapDN,ldapRDNString);
				strcat(newldapDN,", ");
				strcat(newldapDN,pLDAPRecType);
				DSFreeString(pRecContext->fOpenRecordDN);
				pRecContext->fOpenRecordDN = newldapDN;
				DSFreeString(pLDAPRecType);
			}
		}

	}

	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	//cleanup pLDAPAttrType if needed
	DSFreeString(pLDAPAttrType);

	//cleanup ldapRDNString if needed
	DSFreeString(ldapRDNString);

	return( siResult );

} // SetRecordName


//------------------------------------------------------------------------------------
//	* CreateRecord
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::CreateRecord ( sCreateRecord *inData )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPContextData	   *pContext		= nil;
	sLDAPContextData	   *pRecContext		= nil;
	char				   *pLDAPAttrType	= nil;
	LDAPMod				   *mods[128];				//pick 128 since it seems unlikely we will ever get to 126 required attrs
	uInt32					modIndex		= 0;
	int						ldapReturnCode	= 0;
    sLDAPConfigData		   *pConfig			= nil;
	tDataNodePtr			pRecType		= nil;
	tDataNodePtr			pRecName		= nil;
	char				   *pLDAPRecType	= nil;
	char				   *ldapDNString	= nil;
	uInt32					ldapDNLength	= 0;
	uInt32					ocCount			= 0;
	uInt32					raCount			= 0;
	LDAPMod					ocmod;
	LDAPMod					rnmod;
	char				  **ocvals			= nil;
	char				   *rnvals[2];
	char				   *ocString		= nil;
	listOfStrings			objectClassList;
	listOfStrings			reqAttrsList;
	char				  **needsValueMarker= nil;
	bool					bOCANDGroup		= false;
	CFArrayRef				OCSearchList	= nil;
	char				   *tmpBuff			= nil;
	CFIndex					cfBuffSize		= 1024;
	int						aOCSearchListCount	= 0;
	bool					bOCSchemaNil	= false;
	bool					bOCSchemaBuilt	= false;


	try
	{
		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( pContext->bLDAPv2ReadOnly ) throw( (sInt32)eDSReadOnly);

		pRecType = inData->fInRecType;
		if ( pRecType  == nil ) throw( (sInt32)eDSNullRecType );

		//here we are going to assume that the name given to create this record
		//will fill out the DN with the first recordname native type
		pRecName = inData->fInRecName;
		if ( pRecName  == nil ) throw( (sInt32)eDSNullRecName );

		if (pRecName->fBufferLength == 0) throw( (sInt32)eDSEmptyRecordNameList );
		if (pRecType->fBufferLength == 0) throw( (sInt32)eDSEmptyRecordTypeList );

		//get ONLY the first record name mapping
		pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecType->fBufferData), kDSNAttrRecordName, pContext->fNodeName, 1, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable
		
		//get ONLY the first record type mapping
		pLDAPRecType = MapRecToLDAPType( (const char *)(pRecType->fBufferData), pContext->fNodeName, 1, &bOCANDGroup, &OCSearchList, nil );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );  //KW would like a eDSNoMappingAvailable
		
		//if first native map for kDSNAttrRecordName is "cn" then the DN will be built as follows:
		//"cn = pRecName->fBufferData, pLDAPRecType"
		//might need to escape RDN chars
		ldapDNLength = strlen(pLDAPAttrType) + 1 + 2*pRecName->fBufferLength + 2 + strlen(pLDAPRecType);
		ldapDNString = (char *)calloc(1, ldapDNLength + 1);
		strcpy(ldapDNString,pLDAPAttrType);
		strcat(ldapDNString,"=");
		char *escapedString = BuildEscapedRDN(pRecName->fBufferData);
		strcat(ldapDNString,escapedString);
		DSFreeString(escapedString);
		strcat(ldapDNString,", ");
		strcat(ldapDNString,pLDAPRecType);
		
		rnvals[0] = pRecName->fBufferData;
		rnvals[1] = NULL;
		rnmod.mod_op = 0;
		rnmod.mod_type = pLDAPAttrType;
		rnmod.mod_values = rnvals;

		pConfig = gpConfigFromXML->ConfigWithNodeNameLock( pContext->fNodeName );
		if( pConfig != nil )
		{
			if (pConfig->fObjectClassSchema == nil)
			{
				bOCSchemaNil = true;
			}
			if (pConfig->bOCBuilt)
			{
				bOCSchemaBuilt = true;
			}
			gpConfigFromXML->ConfigUnlock( pConfig );
			pConfig = nil;
		}

		if ( (pRecType->fBufferData != nil) && (pLDAPRecType != nil) )
		{
			if (strncmp(pRecType->fBufferData,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
			{
				if (OCSearchList != nil)
				{
					CFStringRef	ocString = nil;
					// assume that the extracted objectclass strings will be significantly less than 1024 characters
					tmpBuff = (char *)::calloc(1, 1024);
					
					// here we extract the object class strings
					// do we need to escape any of the characters internal to the CFString??? like before
					// NO since "*, "(", and ")" are not legal characters for objectclass names
					
					// if OR then we only use the first one
					if (!bOCANDGroup)
					{
						if (CFArrayGetCount(OCSearchList) >= 1)
						{
							ocString = (CFStringRef)::CFArrayGetValueAtIndex( OCSearchList, 0 );
						}
						if (ocString != nil)
						{
							CFStringGetCString(ocString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8);
							string ourString(tmpBuff);
							objectClassList.push_back(ourString);
						}
					}
					else
					{
						aOCSearchListCount = CFArrayGetCount(OCSearchList);
						for ( int iOCIndex = 0; iOCIndex < aOCSearchListCount; iOCIndex++ )
						{
							::memset(tmpBuff,0,1024);
							ocString = (CFStringRef)::CFArrayGetValueAtIndex( OCSearchList, iOCIndex );
							if (ocString != nil)
							{		
								CFStringGetCString(ocString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8);
								string ourString(tmpBuff);
								objectClassList.push_back(ourString);
							}
						}// loop over the objectclasses CFArray
					}
				}//OCSearchList != nil
			}// if (strncmp(pRecType->fBufferData,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
		}// if ( (pRecType->fBufferData != nil) && (pLDAPRecType != nil) )

		mods[0] = &rnmod;
//		mods[1] = &snmod;
		for (uInt32 modsIndex=1; modsIndex < 128; modsIndex++)
		{
			mods[modsIndex] = NULL;
		}
		modIndex = 1;
		
		//here we check if we have the object class schema -- otherwise we try to retrieve it
		//ie. don't have it and haven't already tried to retrieve it
		if ( bOCSchemaNil && (!bOCSchemaBuilt) )
		{
			fLDAPSessionMgr.GetSchema( pContext );
		}
		
		if (OCSearchList != nil)
		{
			pConfig = gpConfigFromXML->ConfigWithNodeNameLock( pContext->fNodeName );
			if (pConfig && pConfig->fObjectClassSchema != nil) //if there is a hierarchy to compare to then do it
			{
				//now we look at the objectclass list provided by the user and expand it fully with the hierarchy info
				for (listOfStringsCI iter = objectClassList.begin(); iter != objectClassList.end(); ++iter)
				{
					//const char *aString = (*iter).c_str();
					if (pConfig->fObjectClassSchema->count(*iter) != 0)
					{
						ObjectClassMap::iterator mapIter = pConfig->fObjectClassSchema->find(*iter);
						for (AttrSetCI parentIter = mapIter->second->fParentOCs.begin(); parentIter != mapIter->second->fParentOCs.end(); ++parentIter)
						{
							bool addObjectClassName = true;
							for (listOfStringsCI dupIter = objectClassList.begin(); dupIter != objectClassList.end(); ++dupIter)
							{
								if (*dupIter == *parentIter) //already in the list
								{
									addObjectClassName = false;
									break;
								}
							}
							if (addObjectClassName)
							{
								objectClassList.push_back(*parentIter);
							}
						}
					}
				}
				
				//now that we have the objectclass list we can build a similar list of the required creation attributes
				for (listOfStringsCI iter = objectClassList.begin(); iter != objectClassList.end(); ++iter)
				{
					if (pConfig->fObjectClassSchema->count(*iter) != 0)
					{
						ObjectClassMap::iterator mapIter = pConfig->fObjectClassSchema->find(*iter);
						for (AttrSetCI reqIter = mapIter->second->fRequiredAttrs.begin(); reqIter != mapIter->second->fRequiredAttrs.end(); ++reqIter)
						{
							if (	(*reqIter != "objectClass") &&					// explicitly added already
									(*reqIter != "nTSecurityDescriptor") &&			// exclude nTSecurityDescriptor special AD attr type
									(*reqIter != "objectCategory") &&				// exclude objectCategory special AD attr type
									(*reqIter != "instanceType") )					// exclude instanceType special AD attr type
							{
								bool addReqAttr = true;
								for (listOfStringsCI dupIter = reqAttrsList.begin(); dupIter != reqAttrsList.end(); ++dupIter)
								{
									if (*dupIter == *reqIter) //already in the list
									{
										addReqAttr = false;
										break;
									}
								}
								if (addReqAttr)
								{
									reqAttrsList.push_back(*reqIter);
								}
							}
							if (*reqIter == "nTSecurityDescriptor")		//For AD LDAP we force the addition of the sAMAccountName
							{
								string nameString("sAMAccountName");
								reqAttrsList.push_back(nameString);
							}
						}
					}
				}
			}
			if (pConfig != nil)
			{
				gpConfigFromXML->ConfigUnlock( pConfig );
			}
			raCount = reqAttrsList.size();
			ocCount = objectClassList.size();
			ocString = (char *)calloc(1,12);
			strcpy(ocString,"objectClass");
			ocmod.mod_op = 0;
			ocmod.mod_type = ocString;
			ocmod.mod_values = nil;
			ocvals = (char **)calloc(1,(ocCount+1)*sizeof(char **));
			//build the ocvals here
			uInt32 ocValIndex = 0;
			for (listOfStringsCI iter = objectClassList.begin(); iter != objectClassList.end(); ++iter)
			{
				const char *aString = (*iter).c_str();
				ocvals[ocValIndex] = (char *)aString;  //TODO recheck for leaks
				ocValIndex++;
			}
			ocvals[ocCount] = nil;
			ocmod.mod_values = ocvals;

			mods[1] = &ocmod;
			modIndex = 2;
		}
		
		needsValueMarker = (char **)calloc(1,2*sizeof(char *));
		needsValueMarker[0] = "99";
		needsValueMarker[1] = NULL;
		
		//check if we have determined what attrs need to be added
		if (raCount != 0)
		{
			for (listOfStringsCI addIter = reqAttrsList.begin(); addIter != reqAttrsList.end(); ++addIter)
			{
				if (modIndex == 127)
				{
					//unlikely to get here as noted above but nonetheless check for it and just drop the rest of the req attrs
					break;
				}
				if (	(strcasecmp((*addIter).c_str(), pLDAPAttrType) != 0) ||		//if this is not the record name then we can add a default value
						(strcasecmp("sAMAccountName", pLDAPAttrType) == 0) )
				{
					LDAPMod *aLDAPMod = (LDAPMod *)calloc(1,sizeof(LDAPMod));
					aLDAPMod->mod_op = 0;
					const char *aString = (*addIter).c_str();
					aLDAPMod->mod_type = (char *)aString;  //TODO recheck for leaks
					aLDAPMod->mod_values = needsValueMarker; //TODO KW really need syntax specific default value added here
					mods[modIndex] = aLDAPMod;
					modIndex++;
				}
			}
		}
		mods[modIndex] = NULL;

		LDAP *aHost = fLDAPSessionMgr.LockSession(pContext);
		if (aHost != nil)
		{
			ldapReturnCode = ldap_add_ext_s( aHost, ldapDNString, mods, NULL, NULL);
		}
		else
		{
			ldapReturnCode = LDAP_LOCAL_ERROR;
		}
		fLDAPSessionMgr.UnLockSession(pContext);
		if ( ldapReturnCode == LDAP_ALREADY_EXISTS )
		{
			siResult = eDSRecordAlreadyExists;
		}
		else if ( ldapReturnCode != LDAP_SUCCESS )
		{
			siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSRecordNotFound );
		}

		if ( (inData->fInOpen == true) && (siResult == eDSNoErr) )
		{
			fLDAPSessionMgr.SessionMapMutexLock();
			pRecContext = new sLDAPContextData( *pContext );
			fLDAPSessionMgr.SessionMapMutexUnlock();
			if ( pRecContext  == nil ) throw( (sInt32) eMemoryAllocError );
	        
			pRecContext->fType = 2;

			if (pRecType->fBufferData != nil)
			{
				pRecContext->fOpenRecordType = strdup( pRecType->fBufferData );
			}
			if (pRecName->fBufferData != nil)
			{
				pRecContext->fOpenRecordName = strdup( pRecName->fBufferData );
			}
			
			//get the ldapDN here
			pRecContext->fOpenRecordDN = ldapDNString;
			ldapDNString = nil;
		
			gLDAPContextTable->AddItem( inData->fOutRecRef, pRecContext );
		}

	}

	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	//cleanup if needed
	DSFreeString(pLDAPAttrType);
	DSFreeString(pLDAPRecType);
	DSFreeString(ldapDNString);
	DSFree(needsValueMarker);
	DSFreeString(ocString);
	DSFree(ocvals);
	
	uInt32 startIndex = 1;
	if (OCSearchList != nil)
	{
		startIndex = 2;
	}
	for (uInt32 anIndex = startIndex; anIndex < modIndex; anIndex++)
	{
		free(mods[anIndex]);
		mods[anIndex] = NULL;
	}
	
	DSCFRelease(OCSearchList);
	DSFreeString(tmpBuff);

	return( siResult );

} // CreateRecord


//------------------------------------------------------------------------------------
//	* CreateRecordWithAttributes
//  to be used by custom plugin calls
//  caller owns the CFDictionary and needs to clean it up
//  Record name is ONLY singled valued ie. multiple values are ignored if provided
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::CreateRecordWithAttributes ( tDirNodeReference inNodeRef, const char* inRecType, const char* inRecName, CFDictionaryRef inDict )
{
	sInt32					siResult		= eDSNoErr;
	char				   *attrTypeStr		= nil;
	char				   *attrTypeLDAPStr	= nil;
	sLDAPContextData	   *pContext		= nil;
	uInt32					attrCount		= 0;
	uInt32					valCount		= 0;
	uInt32					valIndex		= 0;
	struct berval		  **newValues		= nil;
	LDAPMod				   *mods[128];				//pick 128 since it seems unlikely we will ever get to 126 required attrs
	int						ldapReturnCode	= 0;
	char				   *attrValue		= nil;
	uInt32					attrLength		= 0;
	LDAP				   *aHost			= nil;
	uInt32					callocLength	= 0;
	bool					bGotIt			= false;
	CFStringRef				keyCFString		= NULL;
	char				   *pLDAPRecType	= nil;
	char				   *recNameAttrType	= nil;
    sLDAPConfigData		   *pConfig			= nil;
	char				   *ldapDNString	= nil;
	uInt32					ldapDNLength	= 0;
	uInt32					ocCount			= 0;
	uInt32					raCount			= 0;
	LDAPMod				   *ocmod;
	LDAPMod				   *rnmod;
	char				  **ocvals			= nil;
	char				   *rnvals[2];
	listOfStrings			objectClassList;
	listOfStrings			reqAttrsList;
	bool					bOCANDGroup		= false;
	CFArrayRef				OCSearchList	= nil;
	char				   *tmpBuff			= nil;
	CFIndex					cfBuffSize		= 1024;
	int						aOCSearchListCount	= 0;
	bool					bOCSchemaNil	= false;
	bool					bOCSchemaBuilt	= false;
	uInt32					keyIndex		= 0;
	uInt32					modIndex		= 0;
	uInt32					bValueStartIndex= 0;
	CFMutableDictionaryRef	mergedDict		= NULL;

	try
	{
		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inNodeRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( pContext->bLDAPv2ReadOnly ) throw( (sInt32)eDSReadOnly);

		if ( CFGetTypeID(inDict) != CFDictionaryGetTypeID() ) throw( (sInt32)eDSInvalidBuffFormat );

		if ( inRecType  == nil ) throw( (sInt32)eDSNullRecType );
		//here we are going to assume that the name given to create this record
		//will fill out the DN with the first recordname native type
		if ( inRecName  == nil ) throw( (sInt32)eDSNullRecName );
		
		for (uInt32 modsIndex=0; modsIndex < 128; modsIndex++)
		{
			mods[modsIndex] = NULL;
		}

		//get ONLY the first record name mapping
		recNameAttrType = MapAttrToLDAPType( (const char *)inRecType, kDSNAttrRecordName, pContext->fNodeName, 1, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( recNameAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable
		
		//get ONLY the first record type mapping
		pLDAPRecType = MapRecToLDAPType( (const char *)inRecType, pContext->fNodeName, 1, &bOCANDGroup, &OCSearchList, nil );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );  //KW would like a eDSNoMappingAvailable
		
		//if first native map for kDSNAttrRecordName is "cn" then the DN will be built as follows:
		//"cn = inRecName, pLDAPRecType"
		//RDN chars might need escaping
		ldapDNLength = strlen(recNameAttrType) + 1 + 2*strlen(inRecName) + 2 + strlen(pLDAPRecType);
		ldapDNString = (char *)calloc(1, ldapDNLength + 1);
		strcpy(ldapDNString,recNameAttrType);
		strcat(ldapDNString,"=");
		char *escapedString = BuildEscapedRDN(inRecName);
		strcat(ldapDNString,escapedString);
		DSFreeString(escapedString);
		strcat(ldapDNString,", ");
		strcat(ldapDNString,pLDAPRecType);
		
		rnvals[0] = (char *)inRecName;
		rnvals[1] = NULL;
		rnmod = (LDAPMod *) calloc( 1, sizeof(LDAPMod *) );
		rnmod->mod_op = LDAP_MOD_ADD;
		rnmod->mod_type = strdup(recNameAttrType);
		rnmod->mod_values = rnvals; //not freed below

		pConfig = gpConfigFromXML->ConfigWithNodeNameLock( pContext->fNodeName );
		if( pConfig != nil )
		{
			if (pConfig->fObjectClassSchema == nil)
			{
				bOCSchemaNil = true;
			}
			if (pConfig->bOCBuilt)
			{
				bOCSchemaBuilt = true;
			}
			gpConfigFromXML->ConfigUnlock( pConfig );
			pConfig = nil;
		}

		if ( (inRecType != nil) && (pLDAPRecType != nil) )
		{
			if (strncmp(inRecType,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
			{
				if (OCSearchList != nil)
				{
					CFStringRef	ocString = nil;
					// assume that the extracted objectclass strings will be significantly less than 1024 characters
					tmpBuff = (char *)::calloc(1, 1024);
					
					// here we extract the object class strings
					// do we need to escape any of the characters internal to the CFString??? like before
					// NO since "*, "(", and ")" are not legal characters for objectclass names
					
					// if OR then we only use the first one
					if (!bOCANDGroup)
					{
						if (CFArrayGetCount(OCSearchList) >= 1)
						{
							ocString = (CFStringRef)::CFArrayGetValueAtIndex( OCSearchList, 0 );
						}
						if (ocString != nil)
						{
							CFStringGetCString(ocString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8);
							string ourString(tmpBuff);
							objectClassList.push_back(ourString);
						}
					}
					else
					{
						aOCSearchListCount = CFArrayGetCount(OCSearchList);
						for ( int iOCIndex = 0; iOCIndex < aOCSearchListCount; iOCIndex++ )
						{
							::memset(tmpBuff,0,1024);
							ocString = (CFStringRef)::CFArrayGetValueAtIndex( OCSearchList, iOCIndex );
							if (ocString != nil)
							{		
								CFStringGetCString(ocString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8);
								string ourString(tmpBuff);
								objectClassList.push_back(ourString);
							}
						}// loop over the objectclasses CFArray
					}
				}//OCSearchList != nil
			}// if (strncmp(pRecType->fBufferData,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
		}// if ( (pRecType->fBufferData != nil) && (pLDAPRecType != nil) )

		//first entry is the name of the record to create
		mods[0] = rnmod;
		modIndex = 1;
		bValueStartIndex = 1;
		
		//here we check if we have the object class schema -- otherwise we try to retrieve it
		//ie. don't have it and haven't already tried to retrieve it
		if ( bOCSchemaNil && (!bOCSchemaBuilt) )
		{
			fLDAPSessionMgr.GetSchema( pContext );
		}
		
		if (OCSearchList != nil)
		{
			pConfig = gpConfigFromXML->ConfigWithNodeNameLock( pContext->fNodeName );
			if (pConfig && pConfig->fObjectClassSchema != nil) //if there is a hierarchy to compare to then do it
			{
				//now we look at the objectclass list provided by the user and expand it fully with the hierarchy info
				for (listOfStringsCI iter = objectClassList.begin(); iter != objectClassList.end(); ++iter)
				{
					//const char *aString = (*iter).c_str();
					if (pConfig->fObjectClassSchema->count(*iter) != 0)
					{
						ObjectClassMap::iterator mapIter = pConfig->fObjectClassSchema->find(*iter);
						for (AttrSetCI parentIter = mapIter->second->fParentOCs.begin(); parentIter != mapIter->second->fParentOCs.end(); ++parentIter)
						{
							bool addObjectClassName = true;
							for (listOfStringsCI dupIter = objectClassList.begin(); dupIter != objectClassList.end(); ++dupIter)
							{
								if (*dupIter == *parentIter) //already in the list
								{
									addObjectClassName = false;
									break;
								}
							}
							if (addObjectClassName)
							{
								objectClassList.push_back(*parentIter);
							}
						}
					}
				}
				
				//now that we have the objectclass list we can build a similar list of the required creation attributes
				for (listOfStringsCI iter = objectClassList.begin(); iter != objectClassList.end(); ++iter)
				{
					if (pConfig->fObjectClassSchema->count(*iter) != 0)
					{
						ObjectClassMap::iterator mapIter = pConfig->fObjectClassSchema->find(*iter);
						for (AttrSetCI reqIter = mapIter->second->fRequiredAttrs.begin(); reqIter != mapIter->second->fRequiredAttrs.end(); ++reqIter)
						{
							if (	(*reqIter != "objectClass") &&					// explicitly added already
									(*reqIter != "nTSecurityDescriptor") &&			// exclude nTSecurityDescriptor special AD attr type
									(*reqIter != "objectCategory") &&				// exclude objectCategory special AD attr type
									(*reqIter != "instanceType") )					// exclude instanceType special AD attr type
							{
								bool addReqAttr = true;
								for (listOfStringsCI dupIter = reqAttrsList.begin(); dupIter != reqAttrsList.end(); ++dupIter)
								{
									if (*dupIter == *reqIter) //already in the list
									{
										addReqAttr = false;
										break;
									}
								}
								if (addReqAttr)
								{
									reqAttrsList.push_back(*reqIter);
								}
							}
							if (*reqIter == "nTSecurityDescriptor")		//For AD LDAP we force the addition of the sAMAccountName
							{
								string nameString("sAMAccountName");
								reqAttrsList.push_back(nameString);
							}
						}
					}
				}
			}
			if (pConfig != nil)
			{
				gpConfigFromXML->ConfigUnlock( pConfig );
			}
			
			//set the count of required attrs
			raCount = reqAttrsList.size();
			
			//set the count of object classes
			ocCount = objectClassList.size();
			
			ocmod = (LDAPMod *) calloc( 1, sizeof(LDAPMod *) );
			ocmod->mod_op = LDAP_MOD_ADD;
			ocmod->mod_type = strdup("objectClass");
			ocmod->mod_values = nil;
			ocvals = (char **)calloc(1,(ocCount+1)*sizeof(char **));
			//build the ocvals here
			uInt32 ocValIndex = 0;
			for (listOfStringsCI iter = objectClassList.begin(); iter != objectClassList.end(); ++iter)
			{
				const char *aString = (*iter).c_str();
				ocvals[ocValIndex] = (char *)aString;  //TODO recheck for leaks
				ocValIndex++;
			}
			ocvals[ocCount] = nil;
			ocmod->mod_values = ocvals; // freed outside of mods below

			mods[1] = ocmod;
			modIndex = 2;
			bValueStartIndex = 2;
		}
		
		//NEED to reconcile the two separate attr lists
		//ie. one of required attrs and one of user defined attrs to set
		//build a Dict of the required attrs and then add in or replcae with the defined attrs
		
		mergedDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFStringRef singleDefaultValue = CFStringCreateWithCString( kCFAllocatorDefault, "99", kCFStringEncodingUTF8 ); //TODO KW really need syntax specific default value added here
		CFMutableArrayRef valueDefaultArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		CFArrayAppendValue( valueDefaultArray, singleDefaultValue );
		
		//check if we have determined what attrs need to be added
		if (raCount != 0)
		{
			for (listOfStringsCI addIter = reqAttrsList.begin(); addIter != reqAttrsList.end(); ++addIter)
			{
				if (	(strcasecmp((*addIter).c_str(), recNameAttrType) != 0) ||		//if this is not the record name then we can add a default value
						(strcasecmp("sAMAccountName", recNameAttrType) == 0) )
				{
					CFStringRef aString = CFStringCreateWithCString(kCFAllocatorDefault, (*addIter).c_str(), kCFStringEncodingUTF8);
					CFDictionarySetValue( mergedDict, aString, valueDefaultArray );
					CFRelease(aString);
				}
			}
		}
		CFRelease(singleDefaultValue);
		CFRelease(valueDefaultArray);
		
		//now add in the defined attrs
		attrCount = (uInt32) CFDictionaryGetCount(inDict);
		char** keys = (char**)calloc( attrCount + 1, sizeof( void* ) );
		//get a linked list of the attrs so we can iterate over them
		CFDictionaryGetKeysAndValues( inDict, (const void**)keys, NULL );
		CFArrayRef valuesCFArray = NULL;
		for( keyIndex = 0; keyIndex < attrCount; keyIndex++ )
		{
			keyCFString = NULL;
			if ( !(CFGetTypeID((CFStringRef)keys[keyIndex]) == CFStringGetTypeID() ) )
			{
				continue; //skip this one and do not increment attrIndex
			}
			else
			{
				keyCFString = (CFStringRef)keys[keyIndex];
			}
			attrTypeStr = (char *)CFStringGetCStringPtr( keyCFString, kCFStringEncodingUTF8 );
			if (attrTypeStr == nil)
			{
				callocLength = (uInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(keyCFString), kCFStringEncodingUTF8) + 1;
				attrTypeStr = (char *)::calloc( 1, callocLength );
				if ( attrTypeStr == nil ) throw( (sInt32)eMemoryError );

				// Convert it to a regular 'C' string 
				bGotIt = CFStringGetCString( keyCFString, attrTypeStr, callocLength, kCFStringEncodingUTF8 );
				if (bGotIt == false) throw( (sInt32)eMemoryError );
			}

			//get the first mapping
			attrTypeLDAPStr = MapAttrToLDAPType( (const char *)inRecType, attrTypeStr, pContext->fNodeName, 1, true );
			//throw if first nil since we only use the first native type to write to
			//skip everything if a single one is incorrect?
			if ( attrTypeLDAPStr == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			valuesCFArray = (CFArrayRef)CFDictionaryGetValue( inDict, keyCFString );
			
			//TODO KW should we deal with multiple values for a record name?
			if (strcasecmp(attrTypeLDAPStr, recNameAttrType) != 0) //if this is not the record name then we can carry it forward
			{
				CFStringRef aString = CFStringCreateWithCString(kCFAllocatorDefault, attrTypeLDAPStr, kCFStringEncodingUTF8);
				CFDictionarySetValue( mergedDict, aString, valuesCFArray );
				CFRelease(aString);
			}
			
			if (bGotIt)
			{
				DSFreeString(attrTypeStr);
			}
			DSFreeString(attrTypeLDAPStr);
		}
		
		//find out how many attrs need to be set
		attrCount = (uInt32) CFDictionaryGetCount(mergedDict);
		keys = (char**)calloc( attrCount + 1, sizeof( void* ) );
		//get a linked list of the attrs so we can iterate over them
		CFDictionaryGetKeysAndValues( mergedDict, (const void**)keys, NULL );
		
		//loop over attrs in the dictionary
		for( keyIndex = 0; keyIndex < attrCount; keyIndex++ )
		{
			keyCFString = NULL;
			if ( !(CFGetTypeID((CFStringRef)keys[keyIndex]) == CFStringGetTypeID() ) )
			{
				continue; //skip this one and do not increment modIndex
			}
			else
			{
				keyCFString = (CFStringRef)keys[keyIndex];
			}

			attrTypeStr = (char *)CFStringGetCStringPtr( keyCFString, kCFStringEncodingUTF8 );
			if (attrTypeStr == nil)
			{
				callocLength = (uInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(keyCFString), kCFStringEncodingUTF8) + 1;
				attrTypeStr = (char *)::calloc( 1, callocLength );
				if ( attrTypeStr == nil ) throw( (sInt32)eMemoryError );

				// Convert it to a regular 'C' string 
				bGotIt = CFStringGetCString( keyCFString, attrTypeStr, callocLength, kCFStringEncodingUTF8 );
				if (bGotIt == false) throw( (sInt32)eMemoryError );
			}

			CFArrayRef valuesCFArray = (CFArrayRef)CFDictionaryGetValue( mergedDict, keyCFString );
			if ( !(CFGetTypeID(valuesCFArray) == CFArrayGetTypeID() ) )
			{
				if (bGotIt)
				{
					DSFreeString(attrTypeStr);
				}
				continue; //skip this one and free up the attr string if required
			}
			valCount	= (uInt32) CFArrayGetCount( valuesCFArray );
			newValues	= (struct berval**) calloc(1, (valCount + 1) *sizeof(struct berval *) );
			
			valIndex = 0;
			for (uInt32 i = 0; i < valCount; i++ ) //extract the values out of the valuesCFArray
			{
				//need to determine whether the value is either string or data
				CFTypeRef valueCFType = CFArrayGetValueAtIndex( valuesCFArray, (CFIndex)i );
				
				if ( CFGetTypeID(valueCFType) == CFStringGetTypeID() )
				{
					CFStringRef valueCFString = (CFStringRef)valueCFType;
					
					callocLength = (uInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(valueCFString), kCFStringEncodingUTF8) + 1;
					attrValue = (char *)::calloc( 1, callocLength );
					if ( attrValue == nil ) throw( (sInt32)eMemoryError );

					// Convert it to a regular 'C' string 
					bool bGotValue = CFStringGetCString( valueCFString, attrValue, callocLength, kCFStringEncodingUTF8 );
					if (bGotValue == false) throw( (sInt32)eMemoryError );

					attrLength = strlen(attrValue);
					newValues[valIndex] = (struct berval*) calloc(1, sizeof(struct berval) );
					newValues[valIndex]->bv_val = attrValue;
					newValues[valIndex]->bv_len = attrLength;
					valIndex++;
					attrValue = nil;
				}
				else if ( CFGetTypeID(valueCFType) == CFDataGetTypeID() )
				{
					CFDataRef valueCFData = (CFDataRef)valueCFType;
					
					attrLength = (uInt32) CFDataGetLength(valueCFData);
					attrValue = (char *)::calloc( 1, attrLength + 1 );
					if ( attrValue == nil ) throw( (sInt32)eMemoryError );
					
					CFDataGetBytes( valueCFData, CFRangeMake(0,attrLength), (UInt8*)attrValue );

					newValues[valIndex] = (struct berval*) calloc(1, sizeof(struct berval) );
					newValues[valIndex]->bv_val = attrValue;
					newValues[valIndex]->bv_len = attrLength;
					valIndex++;
					attrValue = nil;
				}
			} // for each value provided
			
			if (valIndex > 0) //means we actually have something to add
			{
				//create this mods entry
				mods[modIndex] = (LDAPMod *) calloc( 1, sizeof(LDAPMod *) );
				mods[modIndex]->mod_op		= LDAP_MOD_ADD | LDAP_MOD_BVALUES;
				mods[modIndex]->mod_type	= strdup(attrTypeStr);
				mods[modIndex]->mod_bvalues= newValues;
				modIndex++;
				if (modIndex == 127) //we need terminating NULL for the list
				{
					if (bGotIt)
					{
						DSFreeString(attrTypeStr);
					}
					break; //this is all we modify ie. first 126 attrs in this set - we will never hit this
				}
			}
			if (bGotIt)
			{
				DSFreeString(attrTypeStr);
			}
			attrTypeStr = nil;
		} //for( uInt32 keyIndex = 0; keyIndex < attrCount; keyIndex++ )

		aHost = fLDAPSessionMgr.LockSession(pContext);
		if (aHost != nil)
		{
			ldapReturnCode = ldap_add_ext_s( aHost, ldapDNString, mods, NULL, NULL);
		}
		else
		{
			ldapReturnCode = LDAP_LOCAL_ERROR;
		}
		fLDAPSessionMgr.UnLockSession(pContext);
		if ( ldapReturnCode == LDAP_ALREADY_EXISTS )
		{
			siResult = eDSRecordAlreadyExists;
		}
		else if ( ldapReturnCode != LDAP_SUCCESS )
		{
			siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSRecordNotFound );
		}

		for (uInt32 modsIndex=0; ((modsIndex < 128) && (mods[modsIndex] != NULL)); modsIndex++)
		{
			DSFreeString(mods[modsIndex]->mod_type);
			if (modsIndex >= bValueStartIndex)
			{
				newValues = mods[modsIndex]->mod_bvalues;
				if (newValues != NULL)
				{
					ldap_value_free_len(newValues);
					newValues = NULL;
				}
			}
			mods[modsIndex] = NULL;
		}
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	DSCFRelease(OCSearchList);
	DSFree(ocvals);
	DSFreeString( attrTypeLDAPStr );
	DSFreeString( pLDAPRecType );
	DSFreeString( recNameAttrType );
	
	DSCFRelease(mergedDict);
		
	return( siResult );

} // CreateRecordWithAttributes


//------------------------------------------------------------------------------------
//	* RemoveAttributeValue
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::RemoveAttributeValue ( sRemoveAttributeValue *inData )
{
	sInt32					siResult		= eDSNoErr;
	tDataNodePtr			pAttrType		= nil;
	char				   *pLDAPAttrType	= nil;
	sLDAPContextData	   *pRecContext		= nil;
	LDAPMessage			   *result			= nil;
	struct berval		  **bValues			= nil;
	struct berval		  **newValues		= nil;
	unsigned long			valCount		= 0;
	bool					bFoundIt		= false;
	int						numAttributes	= 1;
	uInt32					crcVal			= 0;
	LDAPMod				   *mods[2];
	int						ldapReturnCode	= 0;
	LDAP				   *aHost			= nil;
	char				  **attrs			= nil;
	int						ldapMsgId		= 0;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( pRecContext->bLDAPv2ReadOnly ) throw( (sInt32)eDSReadOnly);

		if ( pRecContext->fOpenRecordDN == nil ) throw( (sInt32)eDSNullRecName );

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );
		if ( pAttrType->fBufferData == nil ) throw( (sInt32)eDSEmptyAttributeType );

		attrs = MapAttrToLDAPTypeArray( pRecContext->fOpenRecordType, pAttrType->fBufferData, pRecContext->fNodeName );
		if ( attrs == nil ) throw( (sInt32)eDSInvalidAttributeType );
		
		siResult = GetRecRefLDAPMessage( pRecContext, ldapMsgId, attrs, &result );
		if ( siResult != eDSNoErr ) throw( siResult );

        if ( result != nil )
        {
        
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fNodeName, numAttributes, true );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			//set indicator of multiple native to standard mappings
			bFoundIt = false;
			while ( ( pLDAPAttrType != nil ) && (!bFoundIt) )
			{
				valCount = 0; //separate count for each bvalues
				
				aHost = fLDAPSessionMgr.LockSession(pRecContext);
				if ( (aHost != nil) && ( ( bValues = ldap_get_values_len (aHost, result, pLDAPAttrType )) != NULL ) )
				{
					// calc length of bvalues
					for (int i = 0; bValues[i] != NULL; i++ )
					{
						valCount++;
					}
					
					newValues = (struct berval**) calloc(1, (valCount + 1) *sizeof(struct berval *) );
					// for each value of the attribute
					uInt32 newIndex = 0;
					for (int i = 0; bValues[i] != NULL; i++ )
					{

						//use CRC here - WITF we assume string??
						crcVal = CalcCRC( bValues[i]->bv_val );
						if ( crcVal == inData->fInAttrValueID )
						{
							bFoundIt = true;
							//add the bvalues to the newValues
							newValues[newIndex] = bValues[i];
							newIndex++; 
						}
						
					} // for each bValues[i]
					
				} // if bValues = ldap_get_values_len ...
				fLDAPSessionMgr.UnLockSession(pRecContext);
						
				if (bFoundIt)
				{
					//here we set the newValues ie. remove the found one
					//create this mods entry
					{
						LDAPMod	mod;
						mod.mod_op		= LDAP_MOD_DELETE | LDAP_MOD_BVALUES;
						mod.mod_type	= pLDAPAttrType;
						mod.mod_bvalues	= newValues;
						mods[0]			= &mod;
						mods[1]			= NULL;

						//KW revisit for what degree of error return we need to provide
						//if LDAP_INVALID_CREDENTIALS or LDAP_INSUFFICIENT_ACCESS then don't have authority so use eDSPermissionError
						//if LDAP_NO_SUCH_OBJECT then eDSAttributeNotFound
						//if LDAP_TYPE_OR_VALUE_EXISTS then ???
						//so for now we return simply eDSSchemaError if ANY other error
						aHost = fLDAPSessionMgr.LockSession(pRecContext);
						if (aHost != nil)
						{
							ldapReturnCode = ldap_modify_s( aHost, pRecContext->fOpenRecordDN, mods);
						}
						else
						{
							ldapReturnCode = LDAP_LOCAL_ERROR;
						}
						fLDAPSessionMgr.UnLockSession(pRecContext);
						if ( ldapReturnCode != LDAP_SUCCESS )
						{
							siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSAttributeNotFound );
						}
					}
				}
				
				if (bValues != NULL)
				{
					ldap_value_free_len(bValues);
					bValues = NULL;
				}
				DSFree(newValues); //since newValues points to bValues
				
				//KW here we decide to opt out since we have removed one value already
				//ie. we could continue on to the next native mapping to find more
				//CRC ID matches and remove them as well by resetting bFoundIt
				//and allowing the stop condition to be the number of native types
								
				//cleanup pLDAPAttrType if needed
				DSFreeString(pLDAPAttrType);
				numAttributes++;
				//get the next mapping
				pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fNodeName, numAttributes, true );
			} // while ( pLDAPAttrType != nil )
			
        } // retrieve the result from the LDAP server
        else
        {
        	throw( (sInt32)eDSRecordNotFound ); //KW???
        }

	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	DSFreeString( pLDAPAttrType );
	DSFreeStringList( attrs );
	
	if (result != nil)
	{
		ldap_msgfree( result );
		result = nil;
	}
				
	aHost = fLDAPSessionMgr.LockSession(pRecContext);
	if (aHost != nil)
	{
		DSSearchCleanUp(aHost, ldapMsgId);
	}
	fLDAPSessionMgr.UnLockSession(pRecContext);
			
	return( siResult );

} // RemoveAttributeValue


//------------------------------------------------------------------------------------
//	* SetAttributeValues
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::SetAttributeValues ( sSetAttributeValues *inData )
{
	sInt32					siResult		= eDSNoErr;
	tDataNodePtr			pAttrType		= nil;
	char				   *pLDAPAttrType	= nil;
	sLDAPContextData	   *pRecContext		= nil;
	unsigned long			valCount		= 0;
	struct berval		   *replaceValue	= nil;
	struct berval		  **newValues		= nil;
	LDAPMod				   *mods[2];
	int						ldapReturnCode	= 0;
	char				   *attrValue		= nil;
	uInt32					attrLength		= 0;
	LDAP				   *aHost			= nil;
	tDataNodePtr			valuePtr		= nil;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( pRecContext->bLDAPv2ReadOnly ) throw( (sInt32)eDSReadOnly);

		if ( pRecContext->fOpenRecordDN == nil ) throw( (sInt32)eDSNullRecName );
		if ( inData->fInAttrType == nil ) throw( (sInt32)eDSNullAttributeType );
		if ( inData->fInAttrValueList == nil ) throw( (sInt32)eDSNullAttributeValue ); //would like a plural constant for this
		if ( inData->fInAttrValueList->fDataNodeCount <= 0 ) throw( (sInt32)eDSNullAttributeValue ); //would like a plural constant for this
		
		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );
		if ( pAttrType->fBufferData == nil ) throw( (sInt32)eDSEmptyAttributeType );

		//get the first mapping
		pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fNodeName, 1, true );
		//throw if first nil since we only use the first native type to write to
		if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

		valCount	= inData->fInAttrValueList->fDataNodeCount;
		newValues	= (struct berval**) calloc(1, (valCount + 1) *sizeof(struct berval *) );
		
		uInt32 jIndex = 0;
		for (uInt32 i = 0; i < valCount; i++ ) //extract the values out of the tDataList
		{
			dsDataListGetNodeAlloc( 0, inData->fInAttrValueList, i+1, &valuePtr );
			if ( (valuePtr != nil) && (valuePtr->fBufferLength > 0) )
			{
				attrLength = valuePtr->fBufferLength;
				attrValue = (char *) calloc(1, 1 + attrLength);
				memcpy(attrValue, valuePtr->fBufferData, attrLength);
				replaceValue = (struct berval*) calloc(1, sizeof(struct berval) );
				replaceValue->bv_val = attrValue;
				replaceValue->bv_len = attrLength;
				newValues[jIndex] = replaceValue;
				jIndex++;
				attrValue = nil;
				replaceValue = nil;
			}
			if (valuePtr != NULL)
			{
				dsDataBufferDeallocatePriv(valuePtr);
				valuePtr = NULL;
			}
		} // for each value provided
		
		if (jIndex > 0) //means we actually have something to add
		{
			//here we set the newValues ie. remove the found one
			//create this mods entry
			LDAPMod	mod;
			mod.mod_op		= LDAP_MOD_REPLACE | LDAP_MOD_BVALUES;
			mod.mod_type	= pLDAPAttrType;
			mod.mod_bvalues	= newValues;
			mods[0]			= &mod;
			mods[1]			= NULL;

			aHost = fLDAPSessionMgr.LockSession(pRecContext);
			if (aHost != nil)
			{
				ldapReturnCode = ldap_modify_s( aHost, pRecContext->fOpenRecordDN, mods);
			}
			else
			{
				ldapReturnCode = LDAP_LOCAL_ERROR;
			}
			fLDAPSessionMgr.UnLockSession(pRecContext);
			if ( ldapReturnCode != LDAP_SUCCESS )
			{
				siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSAttributeNotFound );
			}
			
			if (newValues != NULL)
			{
				ldap_value_free_len(newValues);
				newValues = NULL;
			}
		}
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	DSFreeString( pLDAPAttrType );
		
	return( siResult );

} // SetAttributeValues


//------------------------------------------------------------------------------------
//	* SetAttributeValue
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::SetAttributeValue ( sSetAttributeValue *inData )
{
	sInt32					siResult		= eDSNoErr;
	tDataNodePtr			pAttrType		= nil;
	char				   *pLDAPAttrType	= nil;
	sLDAPContextData	   *pRecContext		= nil;
	LDAPMessage			   *result			= nil;
	unsigned long			valCount		= 0;
	struct berval		  **bValues			= nil;
	struct berval			replaceValue;
	struct berval		  **newValues		= nil;
	bool					bFoundIt		= false;
	int						numAttributes	= 1;
	uInt32					crcVal			= 0;
	LDAPMod				   *mods[2];
	int						ldapReturnCode	= 0;
	char				   *attrValue		= nil;
	uInt32					attrLength		= 0;
	LDAP				   *aHost			= nil;
	char				  **attrs			= nil;
	int						ldapMsgId		= 0;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( pRecContext->bLDAPv2ReadOnly ) throw( (sInt32)eDSReadOnly);

		if ( pRecContext->fOpenRecordDN == nil ) throw( (sInt32)eDSNullRecName );
		if ( inData->fInAttrValueEntry == nil ) throw( (sInt32)eDSNullAttributeValue );
		
		//CRC ID is inData->fInAttrValueEntry->fAttributeValueID as used below

		//build the mods data entry to pass into replaceValue
		if (	(inData->fInAttrValueEntry->fAttributeValueData.fBufferData == NULL) ||
				(inData->fInAttrValueEntry->fAttributeValueData.fBufferLength < 1) )
		{
			//don't allow empty values since the ldap call will fail
			throw( (sInt32)eDSEmptyAttributeValue );
		}
		else
		{
			attrLength = inData->fInAttrValueEntry->fAttributeValueData.fBufferLength;
			attrValue = (char *) calloc(1, 1 + attrLength);
			memcpy(attrValue, inData->fInAttrValueEntry->fAttributeValueData.fBufferData, attrLength);
			
		}

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );
		if ( pAttrType->fBufferData == nil ) throw( (sInt32)eDSEmptyAttributeType );

		attrs = MapAttrToLDAPTypeArray( pRecContext->fOpenRecordType, pAttrType->fBufferData, pRecContext->fNodeName );
		if ( attrs == nil ) throw( (sInt32)eDSInvalidAttributeType );
		
		siResult = GetRecRefLDAPMessage( pRecContext, ldapMsgId, attrs, &result );
		if ( siResult != eDSNoErr ) throw( siResult );

        if ( result != nil )
        {
        
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fNodeName, numAttributes, true );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			//set indicator of multiple native to standard mappings
			bFoundIt = false;
			while ( ( pLDAPAttrType != nil ) && (!bFoundIt) )
			{
				valCount = 0; //separate count for each bvalues
				
				aHost = fLDAPSessionMgr.LockSession(pRecContext);
				if ( (aHost != nil) && ( ( bValues = ldap_get_values_len (aHost, result, pLDAPAttrType )) != NULL ) )
				{
				
					// calc length of bvalues
					for (int i = 0; bValues[i] != NULL; i++ )
					{
						valCount++;
					}
					
					newValues = (struct berval**) calloc(1, (valCount + 1) *sizeof(struct berval *) );
					
					for (int i = 0; bValues[i] != NULL; i++ )
					{

						//use CRC here - WITF we assume string??
						crcVal = CalcCRC( bValues[i]->bv_val );
						if ( crcVal == inData->fInAttrValueEntry->fAttributeValueID )
						{
							bFoundIt = true;
							replaceValue.bv_val = attrValue;
							replaceValue.bv_len = attrLength;
							newValues[i] = &replaceValue;

						}
						else
						{
							//add the bvalues to the newValues
							newValues[i] = bValues[i];
						}
						
					} // for each bValues[i]
					
					if (!bFoundIt) //this means that attr type was searched but current value is not present so nothing to change
					{
						if (bValues != NULL)
						{
							ldap_value_free_len(bValues);
							bValues = NULL;
						}
						if (newValues != NULL)
						{
							free(newValues); //since newValues points to bValues
							newValues = NULL;
						}
						siResult = eDSAttributeValueNotFound;
					}
				} // if bValues = ldap_get_values_len ...
				fLDAPSessionMgr.UnLockSession(pRecContext);
						
				if (bFoundIt)
				{
					//here we set the newValues ie. remove the found one
					//create this mods entry
					{
						LDAPMod	mod;
						mod.mod_op		= LDAP_MOD_REPLACE | LDAP_MOD_BVALUES;
						mod.mod_type	= pLDAPAttrType;
						mod.mod_bvalues	= newValues;
						mods[0]			= &mod;
						mods[1]			= NULL;

						aHost = fLDAPSessionMgr.LockSession(pRecContext);
						if (aHost != nil)
						{
							ldapReturnCode = ldap_modify_s( aHost, pRecContext->fOpenRecordDN, mods);
						}
						else
						{
							ldapReturnCode = LDAP_LOCAL_ERROR;
						}
						fLDAPSessionMgr.UnLockSession(pRecContext);
						if ( ldapReturnCode != LDAP_SUCCESS )
						{
							siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSAttributeNotFound );
						}
					}
				}
				
				if (bValues != NULL)
				{
					ldap_value_free_len(bValues);
					bValues = NULL;
				}
				if (newValues != NULL)
				{
					free(newValues); //since newValues points to bValues
					newValues = NULL;
				}
				
				//KW here we decide to opt out since we have removed one value already
				//ie. we could continue on to the next native mapping to find more
				//CRC ID matches and remove them as well by resetting bFoundIt
				//and allowing the stop condition to be the number of native types
								
				//cleanup pLDAPAttrType if needed
				if (pLDAPAttrType != nil)
				{
					delete (pLDAPAttrType);
					pLDAPAttrType = nil;
				}
				numAttributes++;
				//get the next mapping
				pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fNodeName, numAttributes, true );
			} // while ( pLDAPAttrType != nil )
			
        } // retrieve the result from the LDAP server
        else
        {
        	throw( (sInt32)eDSRecordNotFound ); //KW???
        }

	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if ( result != nil )
	{
		ldap_msgfree( result );
		result = nil;
	}
				
	aHost = fLDAPSessionMgr.LockSession(pRecContext);
	if (aHost != nil)
	{
		DSSearchCleanUp(aHost, ldapMsgId);
	}
	fLDAPSessionMgr.UnLockSession(pRecContext);
	
	DSFreeString(pLDAPAttrType);
	DSFree(attrValue);
	DSFreeStringList(attrs);
	
	return( siResult );

} // SetAttributeValue


//------------------------------------------------------------------------------------
//	* ReleaseContinueData
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::ReleaseContinueData ( sReleaseContinueData *inData )
{
	sInt32	siResult	= eDSNoErr;
	
	// RemoveItem calls our ContinueDeallocProc to clean up
	if ( gLDAPContinueTable->RemoveItem( inData->fInContinueData ) != eDSNoErr )
	{
		siResult = eDSInvalidContext;
	}

	return( siResult );

} // ReleaseContinueData


//------------------------------------------------------------------------------------
//	* CloseAttributeList
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::CloseAttributeList ( sCloseAttributeList *inData )
{
	sInt32				siResult		= eDSNoErr;
	sLDAPContextData	   *pContext		= nil;

	pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInAttributeListRef );
	if ( pContext != nil )
	{
		//only "offset" should have been used in the Context
		gLDAPContextTable->RemoveItem( inData->fInAttributeListRef );
	}
	else
	{
		siResult = eDSInvalidAttrListRef;
	}

	return( siResult );

} // CloseAttributeList


//------------------------------------------------------------------------------------
//	* CloseAttributeValueList
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::CloseAttributeValueList ( sCloseAttributeValueList *inData )
{
	sInt32				siResult		= eDSNoErr;
	sLDAPContextData	   *pContext		= nil;

	pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInAttributeValueListRef );
	if ( pContext != nil )
	{
		//only "offset" should have been used in the Context
		gLDAPContextTable->RemoveItem( inData->fInAttributeValueListRef );
	}
	else
	{
		siResult = eDSInvalidAttrValueRef;
	}

	return( siResult );

} // CloseAttributeValueList


//------------------------------------------------------------------------------------
//	* GetRecRefInfo
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetRecRefInfo ( sGetRecRefInfo *inData )
{
	sInt32			siResult	= eDSNoErr;
	uInt32			uiRecSize	= 0;
	tRecordEntry   *pRecEntry	= nil;
	sLDAPContextData   *pContext	= nil;
	char		   *refType		= nil;
    uInt32			uiOffset	= 0;
    char		   *refName		= nil;

	try
	{
		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		//place in the record type from the context data of an OpenRecord
		if ( pContext->fOpenRecordType != nil)
		{
			refType = strdup( pContext->fOpenRecordType );
		}
		else //assume Record type of "Record Type Unknown"
		{
			refType = strdup( "Record Type Unknown" );
		}
		
		//place in the record name from the context data of an OpenRecord
		if ( pContext->fOpenRecordName != nil)
		{
			refName = strdup( pContext->fOpenRecordName );
		}
		else //assume Record name of "Record Name Unknown"
		{
			refName = strdup( "Record Name Unknown" );
		}
		
		uiRecSize = sizeof( tRecordEntry ) + ::strlen( refType ) + ::strlen( refName ) + 4 + kBuffPad;
		pRecEntry = (tRecordEntry *)::calloc( 1, uiRecSize );
		
		pRecEntry->fRecordNameAndType.fBufferSize	= ::strlen( refType ) + ::strlen( refName ) + 4 + kBuffPad;
		pRecEntry->fRecordNameAndType.fBufferLength	= ::strlen( refType ) + ::strlen( refName ) + 4;
		
		uiOffset = 0;
		uInt16 strLen = 0;
		// Add the record name length and name itself
		strLen = ::strlen( refName );
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, &strLen, 2);
		uiOffset += 2;
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, refName, strLen);
		uiOffset += strLen;
		
		// Add the record type length and type itself
		strLen = ::strlen( refType );
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, &strLen, 2);
		uiOffset += 2;
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, refType, strLen);
		uiOffset += strLen;

		inData->fOutRecInfo = pRecEntry;
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}
		
	if (refType != nil)
	{
		delete( refType );
		refType = nil;
	}
	if (refName != nil)
	{
		delete( refName );
		refName = nil;
	}

	return( siResult );

} // GetRecRefInfo


//------------------------------------------------------------------------------------
//	* GetRecAttribInfo
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetRecAttribInfo ( sGetRecAttribInfo *inData )
{
	sInt32					siResult			= eDSNoErr;
	uInt32					uiTypeLen			= 0;
	uInt32					uiDataLen			= 0;
	tDataNodePtr			pAttrType			= nil;
	char				   *pLDAPAttrType		= nil;
	tAttributeEntryPtr		pOutAttrEntry		= nil;
	sLDAPContextData	   *pRecContext			= nil;
	LDAPMessage			   *result				= nil;
	struct berval		  **bValues;
	int						numAttributes		= 1;
	bool					bTypeFound			= false;
	int						valCount			= 0;
	LDAP			   		*aHost				= nil;
	char				  **attrs				= nil;
	int						ldapMsgId			= 0;
	
	try
	{

		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );
		if ( pAttrType->fBufferData == nil ) throw( (sInt32)eDSEmptyAttributeType );

		attrs = MapAttrToLDAPTypeArray( pRecContext->fOpenRecordType, pAttrType->fBufferData, pRecContext->fNodeName );
		if ( attrs == nil ) throw( (sInt32)eDSInvalidAttributeType );
		
		siResult = GetRecRefLDAPMessage( pRecContext, ldapMsgId, attrs, &result );
		if ( siResult != eDSNoErr ) throw( siResult);

        if ( result != nil )
        {
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fNodeName, numAttributes );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			//set indicator of multiple native to standard mappings
			bTypeFound = false;
			while ( pLDAPAttrType != nil )
			{
				if (!bTypeFound)
				{
					//set up the length of the attribute type
					uiTypeLen = ::strlen( pAttrType->fBufferData );
					pOutAttrEntry = (tAttributeEntry *)::calloc( 1, sizeof( tAttributeEntry ) + uiTypeLen + kBuffPad );

					pOutAttrEntry->fAttributeSignature.fBufferSize		= uiTypeLen;
					pOutAttrEntry->fAttributeSignature.fBufferLength	= uiTypeLen;
					::memcpy( pOutAttrEntry->fAttributeSignature.fBufferData, pAttrType->fBufferData, uiTypeLen ); 
					bTypeFound = true;
					valCount = 0;
					uiDataLen = 0;
				}
				
				if ( (pLDAPAttrType[0] == '#') && (strlen(pLDAPAttrType) > 1) )
				{
					char *vsReturnStr = nil;
					vsReturnStr = MappingNativeVariableSubstitution( pLDAPAttrType, pRecContext, result, siResult );
					
					if (vsReturnStr != nil)
					{
						valCount++;
						uiDataLen += (strlen(vsReturnStr) - 1);
						free( vsReturnStr );
						vsReturnStr = nil;
					}
					else
					{
						//if parsing error returned then we throw an error
						if (siResult != eDSNoErr) throw (siResult);
						valCount++;
						uiDataLen += (strlen(pLDAPAttrType) - 1);
					}
				}
				else
				{
					aHost = fLDAPSessionMgr.LockSession(pRecContext);
					if ( (aHost != nil) && ( ( bValues = ldap_get_values_len(aHost, result, pLDAPAttrType )) != NULL ) )
					{
					
						// calculate the number of values for this attribute
						for (int ii = 0; bValues[ii] != NULL; ii++ )
							valCount++;
							
						// for each value of the attribute
						for (int i = 0; bValues[i] != NULL; i++ )
						{
							// Append attribute value
							uiDataLen += bValues[i]->bv_len;
						} // for each bValues[i]
						if (bValues != NULL)
						{
							ldap_value_free_len(bValues);
							bValues = NULL;
						}
					} // if ( aHost != nil ) && bValues = ldap_get_values_len ...
					fLDAPSessionMgr.UnLockSession(pRecContext);
				}

				//cleanup pLDAPAttrType if needed
				if (pLDAPAttrType != nil)
				{
					delete (pLDAPAttrType);
					pLDAPAttrType = nil;
				}
				numAttributes++;
				//get the next mapping
				pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fNodeName, numAttributes );
			} // while ( pLDAPAttrType != nil )

			if ( pOutAttrEntry == nil )
			{
				inData->fOutAttrInfoPtr = nil;
				throw( (sInt32)eDSAttributeNotFound );
			}
			// Number of attribute values
			if ( valCount > 0 )
			{
				pOutAttrEntry->fAttributeValueCount = valCount;
				//KW seems arbitrary max length
				pOutAttrEntry->fAttributeValueMaxSize = 255;
				//set the total length of all the attribute data
				pOutAttrEntry->fAttributeDataSize = uiDataLen;
				//assign the result out
				inData->fOutAttrInfoPtr = pOutAttrEntry;
			}
			else
			{
				free( pOutAttrEntry );
				pOutAttrEntry = nil;
				inData->fOutAttrInfoPtr = nil;
				siResult = eDSAttributeNotFound;
			}
			
        } // retrieve the result from the LDAP server
        else
        {
        	siResult = eDSRecordNotFound;
        }
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if ( result != nil )
	{
		ldap_msgfree( result );
		result = nil;
	}
				
	aHost = fLDAPSessionMgr.LockSession(pRecContext);
	if (aHost != nil)
	{
		DSSearchCleanUp(aHost, ldapMsgId);
	}
	fLDAPSessionMgr.UnLockSession(pRecContext);
	
	DSFreeString(pLDAPAttrType);
	DSFreeStringList(attrs);
	
	return( siResult );

} // GetRecAttribInfo


//------------------------------------------------------------------------------------
//	* GetRecAttrValueByIndex
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetRecAttrValueByIndex ( sGetRecordAttributeValueByIndex *inData )
{
	sInt32					siResult				= eDSNoErr;
	uInt32					uiDataLen				= 0;
	tDataNodePtr			pAttrType				= nil;
	char				   *pLDAPAttrType			= nil;
	tAttributeValueEntryPtr	pOutAttrValue			= nil;
	sLDAPContextData	   *pRecContext				= nil;
	LDAPMessage			   *result					= nil;
	struct berval		  **bValues;
	unsigned long			valCount				= 0;
	bool					bFoundIt				= false;
	int						numAttributes			= 1;
	uInt32					literalLength			= 0;
	bool					bStripCryptPrefix		= false;
	LDAP				   *aHost					= nil;
	char				  **attrs					= nil;
	int						ldapMsgId				= 0;

	try
	{
		if (inData->fInAttrValueIndex == 0) throw( (sInt32)eDSIndexOutOfRange );
		
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );
		if ( pAttrType->fBufferData == nil ) throw( (sInt32)eDSEmptyAttributeType );

		attrs = MapAttrToLDAPTypeArray( pRecContext->fOpenRecordType, pAttrType->fBufferData, pRecContext->fNodeName );
		if ( attrs == nil ) throw( (sInt32)eDSInvalidAttributeType );
		
		siResult = GetRecRefLDAPMessage( pRecContext, ldapMsgId, attrs, &result );
		if ( siResult != eDSNoErr ) throw( siResult);

        if ( result != nil )
        {
			if (::strcmp( kDS1AttrPasswordPlus, pAttrType->fBufferData ) == 0)
			{
				//want to remove leading "{crypt}" prefix from password if it exists
				//iff the request came in for the DS std password type ie. not native
				bStripCryptPrefix = true;
			}
			
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fNodeName, numAttributes );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			//set indicator of multiple native to standard mappings
			bFoundIt = false;
			while ( ( pLDAPAttrType != nil ) && (!bFoundIt) )
			{
				if (pLDAPAttrType[0] == '#')
				{
					valCount++;
					literalLength = strlen(pLDAPAttrType + 1);
					if ( (valCount == inData->fInAttrValueIndex) && (literalLength > 0) )
					{
						char *vsReturnStr = nil;
						vsReturnStr = MappingNativeVariableSubstitution( pLDAPAttrType, pRecContext, result, siResult );
					
						if (vsReturnStr != nil)
						{
							uiDataLen = strlen(vsReturnStr + 1);
							
							pOutAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );

							pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
							pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
							pOutAttrValue->fAttributeValueID = CalcCRC( pLDAPAttrType + 1 );
							::memcpy( pOutAttrValue->fAttributeValueData.fBufferData, vsReturnStr + 1, uiDataLen );
							free(vsReturnStr);
							vsReturnStr = nil;
						}
						else
						{
							//if parsing error returned then we throw an error
							if (siResult != eDSNoErr) throw (siResult);
							// Append attribute value
							uiDataLen = literalLength;
							
							pOutAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
	
							pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
							pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
							pOutAttrValue->fAttributeValueID = CalcCRC( pLDAPAttrType + 1 );
							::memcpy( pOutAttrValue->fAttributeValueData.fBufferData, pLDAPAttrType + 1, uiDataLen );
						}
						
						bFoundIt = true;
					}
				}
				else
				{
					aHost = fLDAPSessionMgr.LockSession(pRecContext);
					if ( (aHost != nil) && ( ( bValues = ldap_get_values_len (aHost, result, pLDAPAttrType )) != NULL ) )
					{
					
						// for each value of the attribute
						for (int i = 0; bValues[i] != NULL; i++ )
						{

							valCount++;
							if (valCount == inData->fInAttrValueIndex)
							{
								uInt32 anOffset = 0;
								if (bStripCryptPrefix)
								{
									//case insensitive compare with "crypt" string
									if ( ( bValues[i]->bv_len > 6) &&
										(strncasecmp(bValues[i]->bv_val,"{crypt}",7) == 0) )
									{
										// use the value without "{crypt}" prefix
										anOffset = 7;
									}
								}
								// Append attribute value
								uiDataLen = bValues[i]->bv_len - anOffset;
								
								pOutAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
	
								pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
								pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
								if ( bValues[i]->bv_val != nil )
								{
									pOutAttrValue->fAttributeValueID = CalcCRC( bValues[i]->bv_val ); //no offset for CRC
									::memcpy( pOutAttrValue->fAttributeValueData.fBufferData, bValues[i]->bv_val + anOffset, uiDataLen );
								}
	
								bFoundIt = true;
								break;
							} // if valCount correct one
						} // for each bValues[i]
						if (bValues != NULL)
						{
							ldap_value_free_len(bValues);
							bValues = NULL;
						}
					} // if bValues = ldap_get_values_len ...
					fLDAPSessionMgr.UnLockSession(pRecContext);
				}
						
				if (bFoundIt)
				{
					inData->fOutEntryPtr = pOutAttrValue;				
				}
						
				//cleanup pLDAPAttrType if needed
				if (pLDAPAttrType != nil)
				{
					delete (pLDAPAttrType);
					pLDAPAttrType = nil;
				}
				numAttributes++;
				//get the next mapping
				pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fNodeName, numAttributes );
			} // while ( pLDAPAttrType != nil )
			
	        if (!bFoundIt)
	        {
				if (valCount < inData->fInAttrValueIndex)
				{
					siResult = eDSIndexOutOfRange;
				}
				else
				{
					siResult = eDSAttributeNotFound;
				}
	        }
        } // retrieve the result from the LDAP server
        else
        {
        	siResult = eDSRecordNotFound;
        }

		if (pLDAPAttrType != nil)
		{
			delete( pLDAPAttrType );
			pLDAPAttrType = nil;
		}				
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if (attrs != nil)
	{
		int aIndex = 0;
		while (attrs[aIndex] != nil)
		{
			free(attrs[aIndex]);
			attrs[aIndex] = nil;
			aIndex++;
		}
		free(attrs);
		attrs = nil;
	}
	
	if ( result != nil )
	{
		ldap_msgfree( result );
		result = nil;
	}
	
	aHost = fLDAPSessionMgr.LockSession(pRecContext);
	if (aHost != nil)
	{
		DSSearchCleanUp(aHost, ldapMsgId);
	}
	fLDAPSessionMgr.UnLockSession(pRecContext);

	return( siResult );

} // GetRecAttrValueByIndex


//------------------------------------------------------------------------------------
//	* GetRecAttrValueByValue
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetRecAttrValueByValue ( sGetRecordAttributeValueByValue *inData )
{
	sInt32					siResult				= eDSNoErr;
	uInt32					uiDataLen				= 0;
	tDataNodePtr			pAttrType				= nil;
	char				   *pLDAPAttrType			= nil;
	tAttributeValueEntryPtr	pOutAttrValue			= nil;
	sLDAPContextData	   *pRecContext				= nil;
	LDAPMessage			   *result					= nil;
	struct berval		  **bValues;
	bool					bFoundIt				= false;
	int						numAttributes			= 1;
	uInt32					literalLength			= 0;
	bool					bStripCryptPrefix		= false;
	LDAP			   		*aHost					= nil;
	char				  **attrs					= nil;
	char				   *attrValue				= nil;
	uInt32					attrLength				= 0;
	int						ldapMsgId				= 0;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );
		if ( pAttrType->fBufferData == nil ) throw( (sInt32)eDSEmptyAttributeType );

		attrLength = inData->fInAttrValue->fBufferLength;
		attrValue = (char *) calloc(1, 1 + attrLength);
		memcpy(attrValue, inData->fInAttrValue->fBufferData, attrLength);

		attrs = MapAttrToLDAPTypeArray( pRecContext->fOpenRecordType, pAttrType->fBufferData, pRecContext->fNodeName );
		if ( attrs == nil ) throw( (sInt32)eDSInvalidAttributeType );
		
//call for each ldap attr type below and do not bother with this GetRecRefLDAPMessage call at all
// however, does not handle binary data
//       int ldap_compare_s(ld, dn, attr, value)
//       LDAP *ld;
//       char *dn, *attr, *value;
// success means LDAP_COMPARE_TRUE

		siResult = GetRecRefLDAPMessage( pRecContext, ldapMsgId, attrs, &result );
		if ( siResult != eDSNoErr ) throw( siResult);

        if ( result != nil )
        {
			if (::strcmp( kDS1AttrPasswordPlus, pAttrType->fBufferData ) == 0)
			{
				//want to remove leading "{crypt}" prefix from password if it exists
				//iff the request came in for the DS std password type ie. not native
				bStripCryptPrefix = true;
			}
			
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fNodeName, numAttributes );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			//set indicator of multiple native to standard mappings
			bFoundIt = false;
			while ( ( pLDAPAttrType != nil ) && (!bFoundIt) )
			{
				if (pLDAPAttrType[0] == '#')
				{
					literalLength = strlen(pLDAPAttrType + 1);
					if ( literalLength > 0 )
					{
						char *vsReturnStr = nil;
						vsReturnStr = MappingNativeVariableSubstitution( pLDAPAttrType, pRecContext, result, siResult );
					
						if ( (vsReturnStr != nil)  && (memcmp(vsReturnStr, attrValue, attrLength) == 0 ) )
						{
							bFoundIt = true;
							uiDataLen = strlen(vsReturnStr + 1);
							
							pOutAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );

							pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
							pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
							pOutAttrValue->fAttributeValueID = CalcCRC( pLDAPAttrType + 1 );
							::memcpy( pOutAttrValue->fAttributeValueData.fBufferData, vsReturnStr + 1, uiDataLen );
							free(vsReturnStr);
							vsReturnStr = nil;
						}
					}
				}
				else
				{
					aHost = fLDAPSessionMgr.LockSession(pRecContext);
					if ( (aHost != nil) && ( ( bValues = ldap_get_values_len (aHost, result, pLDAPAttrType )) != NULL ) )
					{
						// for each value of the attribute
						for (int i = 0; bValues[i] != NULL; i++ )
						{
							uInt32 anOffset = 0;
							if (bStripCryptPrefix)
							{
								//case insensitive compare with "crypt" string
								if ( ( bValues[i]->bv_len > 6) &&
									(strncasecmp(bValues[i]->bv_val,"{crypt}",7) == 0) )
								{
									// use the value without "{crypt}" prefix
									anOffset = 7;
								}
							}
							if (memcmp((bValues[i]->bv_val)+anOffset, attrValue, attrLength-anOffset) == 0 )
							{
								// Append attribute value
								uiDataLen = bValues[i]->bv_len - anOffset;
								
								pOutAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
	
								pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
								pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
								if ( bValues[i]->bv_val != nil )
								{
									pOutAttrValue->fAttributeValueID = CalcCRC( bValues[i]->bv_val ); //no offset for CRC
									::memcpy( pOutAttrValue->fAttributeValueData.fBufferData, bValues[i]->bv_val + anOffset, uiDataLen );
								}
	
								bFoundIt = true;
								break;
							}
						} // for each bValues[i]
						if (bValues != NULL)
						{
							ldap_value_free_len(bValues);
							bValues = NULL;
						}
					} // if bValues = ldap_get_values_len ...
					fLDAPSessionMgr.UnLockSession(pRecContext);
				}
						
				if (bFoundIt)
				{
					inData->fOutEntryPtr = pOutAttrValue;				
				}
						
				//cleanup pLDAPAttrType if needed
				if (pLDAPAttrType != nil)
				{
					delete (pLDAPAttrType);
					pLDAPAttrType = nil;
				}
				numAttributes++;
				//get the next mapping
				pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fNodeName, numAttributes );
			} // while ( pLDAPAttrType != nil )
			
	        if (!bFoundIt)
	        {
				siResult = eDSAttributeNotFound;
	        }
        } // retrieve the result from the LDAP server
        else
        {
        	siResult = eDSRecordNotFound;
        }

		if (pLDAPAttrType != nil)
		{
			delete( pLDAPAttrType );
			pLDAPAttrType = nil;
		}				
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if (attrs != nil)
	{
		int aIndex = 0;
		while (attrs[aIndex] != nil)
		{
			free(attrs[aIndex]);
			attrs[aIndex] = nil;
			aIndex++;
		}
		free(attrs);
		attrs = nil;
	}
	if (attrValue != nil)
	{
		free(attrValue);
		attrValue = nil;
	}
	
	if ( result != nil )
	{
		ldap_msgfree( result );
		result = nil;
	}
	
	aHost = fLDAPSessionMgr.LockSession(pRecContext);
	if (aHost != nil)
	{
		DSSearchCleanUp(aHost, ldapMsgId);
	}
	fLDAPSessionMgr.UnLockSession(pRecContext);

	return( siResult );

} // GetRecAttrValueByValue


//------------------------------------------------------------------------------------
//	* GetRecordAttributeValueByID
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetRecordAttributeValueByID ( sGetRecordAttributeValueByID *inData )
{
	sInt32					siResult			= eDSAttributeNotFound;
	uInt32					uiDataLen			= 0;
	tDataNodePtr			pAttrType			= nil;
	char				   *pLDAPAttrType		= nil;
	tAttributeValueEntryPtr	pOutAttrValue		= nil;
	sLDAPContextData	   *pRecContext			= nil;
	LDAPMessage			   *result				= nil;
	struct berval		  **bValues;
	bool					bFoundIt			= false;
	int						numAttributes		= 1;
	uInt32					crcVal				= 0;
	uInt32					literalLength		= 0;
	bool					bStripCryptPrefix   = false;
	LDAP			   		*aHost				= nil;
	char				  **attrs				= nil;
	int						ldapMsgId			= 0;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );
		if ( pAttrType->fBufferData == nil ) throw( (sInt32)eDSEmptyAttributeType );

		attrs = MapAttrToLDAPTypeArray( pRecContext->fOpenRecordType, pAttrType->fBufferData, pRecContext->fNodeName );
		if ( attrs == nil ) throw( (sInt32)eDSInvalidAttributeType );
		
		siResult = GetRecRefLDAPMessage( pRecContext, ldapMsgId, attrs, &result );
		if ( siResult != eDSNoErr ) throw( siResult);

        if ( result != nil )
        {
			if (::strcmp( kDS1AttrPasswordPlus, pAttrType->fBufferData ) == 0)
			{
				//want to remove leading "{crypt}" prefix from password if it exists
				//iff the request came in for the DS std password type ie. not native
				bStripCryptPrefix = true;
			}
			
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fNodeName, numAttributes );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			siResult = eDSAttributeValueNotFound;
			//set indicator of multiple native to standard mappings
			bFoundIt = false;
			while ( ( pLDAPAttrType != nil ) && (!bFoundIt) )
			{
				if (pLDAPAttrType[0] == '#')
				{
					literalLength = strlen(pLDAPAttrType + 1);
					if (literalLength > 0)
					{
						crcVal = CalcCRC( pLDAPAttrType + 1 );
						if ( crcVal == inData->fInValueID )
						{
							char *vsReturnStr = nil;
							vsReturnStr = MappingNativeVariableSubstitution( pLDAPAttrType, pRecContext, result, siResult );
					
							if (vsReturnStr != nil)
							{
								uiDataLen = strlen(vsReturnStr + 1);
								pOutAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
		
								pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
								pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
								pOutAttrValue->fAttributeValueID = crcVal;
								::memcpy( pOutAttrValue->fAttributeValueData.fBufferData, vsReturnStr + 1, uiDataLen );
								free(vsReturnStr);
								vsReturnStr = nil;
							}
							else
							{
								//if parsing error returned then we throw an error
								if (siResult != eDSNoErr) throw (siResult);
								// Append attribute value
								uiDataLen = literalLength;
								
								pOutAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
		
								pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
								pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
								pOutAttrValue->fAttributeValueID = crcVal;
								::memcpy( pOutAttrValue->fAttributeValueData.fBufferData, pLDAPAttrType + 1, uiDataLen );
							}
							
							bFoundIt = true;
							siResult = eDSNoErr;
						}
					}
				}
				else
				{
					aHost = fLDAPSessionMgr.LockSession(pRecContext);
					if ( (aHost != nil) && ( ( bValues = ldap_get_values_len(aHost, result, pLDAPAttrType )) != NULL ) )
					{
					
						// for each value of the attribute
						for (int i = 0; bValues[i] != NULL; i++ )
						{
							//use CRC here - WITF we assume string??
							crcVal = CalcCRC( bValues[i]->bv_val );
							if ( crcVal == inData->fInValueID )
							{
								uInt32 anOffset = 0;
								if (bStripCryptPrefix)
								{
									//case insensitive compare with "crypt" string
									if ( ( bValues[i]->bv_len > 6) &&
										(strncasecmp(bValues[i]->bv_val,"{crypt}",7) == 0) )
									{
										// use the value without "{crypt}" prefix
										anOffset = 7;
									}
								}
								// Append attribute value
								uiDataLen = bValues[i]->bv_len - anOffset;
								
								pOutAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
								pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
								pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
								if ( bValues[i]->bv_val != nil )
								{
									pOutAttrValue->fAttributeValueID = crcVal;
									::memcpy( pOutAttrValue->fAttributeValueData.fBufferData, bValues[i]->bv_val + anOffset, uiDataLen );
								}
	
								bFoundIt = true;
								siResult = eDSNoErr;
								break;
							} // if ( crcVal == inData->fInValueID )
						} // for each bValues[i]
						ldap_value_free_len(bValues);
						bValues = NULL;
					} // if bValues = ldap_get_values_len ...
					fLDAPSessionMgr.UnLockSession(pRecContext);
				}
				if (bFoundIt)
				{
					inData->fOutEntryPtr = pOutAttrValue;				
				}
						
				//cleanup pLDAPAttrType if needed
				if (pLDAPAttrType != nil)
				{
					delete (pLDAPAttrType);
					pLDAPAttrType = nil;
				}
				numAttributes++;
				//get the next mapping
				pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fNodeName, numAttributes );
			} // while ( pLDAPAttrType != nil )
			
        } // retrieve the result from the LDAP server

		if (pLDAPAttrType != nil)
		{
			delete( pLDAPAttrType );
			pLDAPAttrType = nil;
		}				
			
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if (attrs != nil)
	{
		int aIndex = 0;
		while (attrs[aIndex] != nil)
		{
			free(attrs[aIndex]);
			attrs[aIndex] = nil;
			aIndex++;
		}
		free(attrs);
		attrs = nil;
	}
	
	if ( result != nil )
	{
		ldap_msgfree( result );
		result = nil;
	}
	
	aHost = fLDAPSessionMgr.LockSession(pRecContext);
	if (aHost != nil)
	{
		DSSearchCleanUp(aHost, ldapMsgId);
	}
	fLDAPSessionMgr.UnLockSession(pRecContext);
	
	return( siResult );

} // GetRecordAttributeValueByID


// ---------------------------------------------------------------------------
//	* GetNextStdAttrType
// ---------------------------------------------------------------------------

char* CLDAPv3Plugin::GetNextStdAttrType ( char *inRecType, char *inConfigName, int &inputIndex )
{
	char				   *outResult	= nil;
	sLDAPConfigData		   *pConfig		= nil;

	//idea here is to use the inIndex to request a specific std Attr Type
	//if inIndex is 1 then the first std attr type will be returned
	//if inIndex is >= 1 and <= totalCount then that std attr type will be returned
	//if inIndex is <= 0 then nil will be returned
	//if inIndex is > totalCount nil will be returned
	//note the inIndex will reference the inIndexth entry ie. start at 1
	//caller can increment the inIndex starting at one and get std attr types until nil is returned

	if (inputIndex > 0)
	{
		//if no std attr type is found then NIL will be returned
		
		pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inConfigName );
		if (pConfig != nil)
		{
			//TODO need to "try" to get a default here if no mappings
			//KW maybe NOT ie. directed open can work with native types
			if ( pConfig->fRecordAttrMapDict == NULL )
			{
			}
			else
			{
				outResult = gpConfigFromXML->ExtractStdAttrName( inRecType, pConfig->fRecordAttrMapDict, inputIndex );
			}
			gpConfigFromXML->ConfigUnlock( pConfig );
		}
	}// if (inIndex > 0)

	return( outResult );

} // GetNextStdAttrType


//------------------------------------------------------------------------------------
//	* DoAttributeValueSearch
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoAttributeValueSearch ( sDoAttrValueSearchWithData *inData )
{
    sInt32					siResult			= eDSNoErr;
    bool					bAttribOnly			= false;
    uInt32					uiCount				= 0;
    uInt32					uiTotal				= 0;
    char				  **pRecTypes			= nil;
    CAttributeList		   *cpAttrTypeList 		= nil;
    char				  **pSearchStrings		= nil;
    char				   *pAttrType			= nil;
    char				   *pLDAPRecType		= nil;
    tDirPatternMatch		pattMatch			= eDSExact;
    sLDAPContextData	   *pContext			= nil;
    sLDAPContinueData	   *pLDAPContinue		= nil;
    CBuff				   *outBuff				= nil;
	tDataList			   *pTmpDataList		= nil;
    int						numRecTypes			= 1;
    bool					bBuffFull			= false;
    bool					separateRecTypes	= false;
    uInt32					countDownRecTypes	= 0;
	bool					bOCANDGroup			= false;
	CFArrayRef				OCSearchList		= nil;
	char				   *aCompoundExp		= " "; //added fake string since pAttrType is used in strcmp below
	ber_int_t				scope				= LDAP_SCOPE_SUBTREE;

    try
    {

        // Verify all the parameters
        if ( inData  == nil ) throw( (sInt32) eMemoryError );
        if ( inData->fOutDataBuff  == nil ) throw( (sInt32)eDSEmptyBuffer );
        if (inData->fOutDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

        if ( inData->fInRecTypeList  == nil ) throw( (sInt32)eDSEmptyRecordTypeList );

        // Node context data
        pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
        if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		if ( inData->fIOContinueData == nil )
		{
			pLDAPContinue = (sLDAPContinueData *)::calloc( 1, sizeof( sLDAPContinueData ) );

			gLDAPContinueTable->AddItem( pLDAPContinue, inData->fInNodeRef );

            //parameters used for data buffering
            pLDAPContinue->fRecTypeIndex = 1;
            pLDAPContinue->fTotalRecCount = 0;
            pLDAPContinue->fLimitRecSearch = 0;
			
			//check if the client has requested a limit on the number of records to return
			//we only do this the first call into this context for pLDAPContinue
			if (inData->fOutMatchRecordCount >= 0)
			{
				pLDAPContinue->fLimitRecSearch = inData->fOutMatchRecordCount;
			}
		}
		else
		{
			pLDAPContinue = (sLDAPContinueData *)inData->fIOContinueData;
			if ( gLDAPContinueTable->VerifyItem( pLDAPContinue ) == false )
			{
				throw( (sInt32)eDSInvalidContinueData );
			}
		}

        // start with the continue set to nil until buffer gets full and there is more data
        //OR we have more record types to look through
        inData->fIOContinueData			= nil;
		//return zero here if nothing found
		inData->fOutMatchRecordCount	= 0;

        // copy the buffer data into a more manageable form
        outBuff = new CBuff();
        if ( outBuff  == nil ) throw( (sInt32) eMemoryError );

        siResult = outBuff->Initialize( inData->fOutDataBuff, true );
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = outBuff->GetBuffStatus();
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = outBuff->SetBuffType( 'StdA' );
        if ( siResult != eDSNoErr ) throw( siResult );

        // Get the record type list
		pRecTypes = dsAllocStringsFromList(0, inData->fInRecTypeList);
		if ( pRecTypes == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
		uInt32 strCnt = 0;
		bool bResetRecTypes = false;
		while(pRecTypes[strCnt] != nil)
		{
			if (strcmp(pRecTypes[strCnt], kDSStdRecordTypeAll) == 0)
			{
				bResetRecTypes = true;
			}
			strCnt++;
		}
        if (strCnt == 0) throw( (sInt32)eDSEmptyRecordTypeList );
		if (bResetRecTypes)
		{
			//in this case we now do the following:
			//A:TODO try to retrieve the native record type from the search base suffix in the config
			//B:TODO if not present then we compare suffixes of the native record types for users, groups, computers and use that
			//C:otherwise we fall back on brute force search through the remaining std record types
			//TODO we only build this list of native record types once for a query and save it in the continue data
			
			//C:
			//mapping rec types - if std to native
			DSFreeStringList(pRecTypes);
			strCnt = 0;
			uInt32 i = 0;
			sLDAPConfigData *pConfig = gpConfigFromXML->ConfigWithNodeNameLock( pContext->fNodeName );
			
			// this is really easy with the new normalized map dictionary, only add the ones we know about
			if (pConfig != nil )
			{
				if( pConfig->fRecordAttrMapDict != nil )
				{
					strCnt = CFDictionaryGetCount( pConfig->fRecordAttrMapDict );
					if( strCnt )
					{
						CFStringRef	*keys = (CFStringRef *) calloc( strCnt, sizeof(CFStringRef) );
						pRecTypes = (char **) calloc(strCnt+1, sizeof(char *));
						
						CFDictionaryGetKeysAndValues( pConfig->fRecordAttrMapDict, (const void **)keys, NULL );
						
						for( i = 0; i < strCnt; i++ )
						{
							CFIndex uiLength = CFStringGetMaximumSizeForEncoding( CFStringGetLength(keys[i]), kCFStringEncodingUTF8 );
							pRecTypes[i] = (char *) calloc( uiLength, sizeof(char) );
							CFStringGetCString( keys[i], pRecTypes[i], uiLength, kCFStringEncodingUTF8 );
						}
						
						DSFree( keys );
					}
				}
				gpConfigFromXML->ConfigUnlock( pConfig );
			}
			// if we didn't end up with a list, then let's throw an error
			if (strCnt == 0) throw( (sInt32)eDSEmptyRecordTypeList );
		}
		
        //save the number of rec types here to use in separating the buffer data
        countDownRecTypes = strCnt - pLDAPContinue->fRecTypeIndex + 1;

        // Get the attribute pattern match type
        pattMatch = inData->fInPattMatchType;

        // Get the attribute type
		pAttrType = inData->fInAttrType->fBufferData;
		if (	(pattMatch == eDSCompoundExpression) || 
				(pattMatch == eDSiCompoundExpression) )
		{
			pAttrType = aCompoundExp; //used fake string since pAttrType is used in strcmp below
		}
		else
		{
			if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );
		}

        // Get the attribute string match
		pSearchStrings = (char **) calloc(2, sizeof(char *));
		pSearchStrings[0] = strdup(inData->fInPatt2Match->fBufferData);
		if ( pSearchStrings == nil ) throw( (sInt32)eDSEmptyPattern2Match );

		if ( inData->fType == kDoAttributeValueSearchWithData )
		{
			// Get the attribute list
			cpAttrTypeList = new CAttributeList( inData->fInAttrTypeRequestList );
			if ( cpAttrTypeList == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
			if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );

			// Get the attribute info only flag
			bAttribOnly = inData->fInAttrInfoOnly;
		}
		else
		{
			pTmpDataList = dsBuildListFromStringsPriv( kDSAttributesAll, nil );
			if ( pTmpDataList != nil )
			{
				cpAttrTypeList = new CAttributeList( pTmpDataList );
				if ( cpAttrTypeList == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
				if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );
			}
		}

        // get records of these types
        while ((( pRecTypes[pLDAPContinue->fRecTypeIndex-1] != nil ) && (!bBuffFull)) && (!separateRecTypes))
        {
        	//mapping rec types - if std to native
        	numRecTypes = 1;
			bOCANDGroup = false;
			if (OCSearchList != nil)
			{
				CFRelease(OCSearchList);
				OCSearchList = nil;
			}
            pLDAPRecType = MapRecToLDAPType( (const char *)pRecTypes[pLDAPContinue->fRecTypeIndex-1], pContext->fNodeName, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
            //throw on first nil only if last passed in record type
            if ( ( pLDAPRecType == nil ) && (countDownRecTypes == 1) )
				throw( (sInt32)eDSInvalidRecordType );
            while (( pLDAPRecType != nil ) && (!bBuffFull))
            {
				bBuffFull = false;
				if (	(::strcmp( pAttrType, kDSAttributesAll ) == 0 ) ||
						(::strcmp( pAttrType, kDSAttributesStandardAll ) == 0 ) ||
						(::strcmp( pAttrType, kDSAttributesNativeAll ) == 0 ) )
				{
					//go get me all records that have any attribute equal to pSearchStrings with pattMatch constraint
					//KW this is a very difficult search to do
					//approach A: set up a very complex search filter to pass to the LDAP server
					//need to be able to handle all standard types that are mapped
					//CHOSE THIS approach B: get each record and parse it completely using native attr types
					//approach C: just like A but concentrate on a selected subset of attr types
					siResult = GetTheseRecords( nil, pSearchStrings, pRecTypes[pLDAPContinue->fRecTypeIndex-1], pLDAPRecType, pattMatch, cpAttrTypeList, pContext, pLDAPContinue, bAttribOnly, outBuff, uiCount, bOCANDGroup, OCSearchList, scope );
				}
				else
				{
					//go get me all records that have pAttrType equal to pSearchStrings with pattMatch constraint
					siResult = FindTheseRecords( pAttrType, pSearchStrings, pRecTypes[pLDAPContinue->fRecTypeIndex-1], pLDAPRecType, pattMatch, cpAttrTypeList, pContext, pLDAPContinue, bAttribOnly, outBuff, uiCount, bOCANDGroup, OCSearchList, scope );
				}

				//outBuff->GetDataBlockCount( &uiCount );
				//cannot use this as there may be records added from different record names
				//at which point the first name will fill the buffer with n records and
				//uiCount is reported as n but as the second name fills the buffer with m MORE records
				//the statement above will return the total of n+m and add it to the previous n
				//so that the total erroneously becomes 2n+m and not what it should be as n+m
				//therefore uiCount is extracted directly out of the FindxxxRecord(s) calls

				if ( siResult == CBuff::kBuffFull )
				{
					bBuffFull = true;
					//set continue if there is more data available
					inData->fIOContinueData = pLDAPContinue;
					
					// check to see if buffer is empty and no entries added
					// which implies that the buffer is too small
					if ( ( uiCount == 0 ) && ( uiTotal == 0 ) )
					{
						throw( (sInt32)eDSBufferTooSmall );
					}

					uiTotal += uiCount;
					inData->fOutMatchRecordCount = uiTotal;
					outBuff->SetLengthToSize();
					siResult = eDSNoErr;
				}
				else if ( siResult == eDSNoErr )
				{
					uiTotal += uiCount;
//	                pContext->fRecNameIndex++;
					pLDAPContinue->msgId = 0;
				}

                if (pLDAPRecType != nil)
                {
                    delete( pLDAPRecType );
                    pLDAPRecType = nil;
                }
                
                if (!bBuffFull)
                {
	                numRecTypes++;
					bOCANDGroup = false;
					if (OCSearchList != nil)
					{
						CFRelease(OCSearchList);
						OCSearchList = nil;
					}
	                //get the next mapping
	                pLDAPRecType = MapRecToLDAPType( (const char *)pRecTypes[pLDAPContinue->fRecTypeIndex-1], pContext->fNodeName, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
                }
                
            } // while mapped Rec Type != nil
            
            if (!bBuffFull)
            {
	            pLDAPContinue->fRecTypeIndex++;
//	            pContext->fRecNameIndex = 1;
	            //reset the LDAP message ID to zero since now going to go after a new type
	            pLDAPContinue->msgId = 0;
	            
	            //KW? here we decide to exit with data full of the current type of records
	            // and force a good exit with the data we have so we can come back for the next rec type
            	separateRecTypes = true;
                //set continue since there may be more data available
                inData->fIOContinueData = pLDAPContinue;
				siResult = eDSNoErr;
				
				//however if this was the last rec type then there will be no more data
	            // check the number of rec types left
	            countDownRecTypes--;
	            if (countDownRecTypes == 0)
	            {
	                inData->fIOContinueData = nil;
				}
            }
            
        } // while loop over record types

        if (( siResult == eDSNoErr ) & (!bBuffFull))
        {
            if ( uiTotal == 0 )
            {
                outBuff->ClearBuff();
            }
            else
            {
                outBuff->SetLengthToSize();
            }

            inData->fOutMatchRecordCount = uiTotal;
        }
    } // try
    
    catch ( sInt32 err )
    {
		siResult = err;
    }

	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	
	if ( (inData->fIOContinueData == nil) && (pLDAPContinue != nil) )
	{
		// we've decided not to return continue data, so we should clean up
		gLDAPContinueTable->RemoveItem( pLDAPContinue );
		pLDAPContinue = nil;
	}

	if (pLDAPRecType != nil)
	{
		delete( pLDAPRecType );
		pLDAPRecType = nil;
	}

    if ( cpAttrTypeList != nil )
    {
		delete( cpAttrTypeList );
		cpAttrTypeList = nil;
    }

	if ( pTmpDataList != nil )
	{
		dsDataListDeallocatePriv( pTmpDataList );
		free(pTmpDataList);
		pTmpDataList = nil;
	}

    if ( outBuff != nil )
    {
		delete( outBuff );
		outBuff = nil;
    }
	
	DSFreeStringList(pSearchStrings);
	
	DSFreeStringList(pRecTypes);

    return( siResult );

} // DoAttributeValueSearch


//------------------------------------------------------------------------------------
//	* DoMultipleAttributeValueSearch
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoMultipleAttributeValueSearch ( sDoMultiAttrValueSearchWithData *inData )
{
    sInt32					siResult			= eDSNoErr;
    bool					bAttribOnly			= false;
    uInt32					uiCount				= 0;
    uInt32					uiTotal				= 0;
    char				  **pRecTypes			= nil;
    CAttributeList		   *cpAttrTypeList 		= nil;
    char				  **pSearchStrings		= nil;
    char				   *pAttrType			= nil;
    char				   *pLDAPRecType		= nil;
    tDirPatternMatch		pattMatch			= eDSExact;
    sLDAPContextData	   *pContext			= nil;
    sLDAPContinueData	   *pLDAPContinue		= nil;
    CBuff				   *outBuff				= nil;
	tDataList			   *pTmpDataList		= nil;
    int						numRecTypes			= 1;
    bool					bBuffFull			= false;
    bool					separateRecTypes	= false;
    uInt32					countDownRecTypes	= 0;
	bool					bOCANDGroup			= false;
	CFArrayRef				OCSearchList		= nil;
	char				   *aCompoundExp		= " "; //added fake string since pAttrType is used in strcmp below
	ber_int_t				scope				= LDAP_SCOPE_SUBTREE;

    try
    {

        // Verify all the parameters
        if ( inData  == nil ) throw( (sInt32) eMemoryError );
        if ( inData->fOutDataBuff  == nil ) throw( (sInt32)eDSEmptyBuffer );
        if (inData->fOutDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

        if ( inData->fInRecTypeList  == nil ) throw( (sInt32)eDSEmptyRecordTypeList );

        // Node context data
        pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
        if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		if ( inData->fIOContinueData == nil )
		{
			pLDAPContinue = (sLDAPContinueData *)::calloc( 1, sizeof( sLDAPContinueData ) );

			gLDAPContinueTable->AddItem( pLDAPContinue, inData->fInNodeRef );

            //parameters used for data buffering
            pLDAPContinue->fRecTypeIndex = 1;
            pLDAPContinue->fTotalRecCount = 0;
            pLDAPContinue->fLimitRecSearch = 0;
			
			//check if the client has requested a limit on the number of records to return
			//we only do this the first call into this context for pLDAPContinue
			if (inData->fOutMatchRecordCount >= 0)
			{
				pLDAPContinue->fLimitRecSearch = inData->fOutMatchRecordCount;
			}
		}
		else
		{
			pLDAPContinue = (sLDAPContinueData *)inData->fIOContinueData;
			if ( gLDAPContinueTable->VerifyItem( pLDAPContinue ) == false )
			{
				throw( (sInt32)eDSInvalidContinueData );
			}
		}

        // start with the continue set to nil until buffer gets full and there is more data
        //OR we have more record types to look through
        inData->fIOContinueData			= nil;
		//return zero here if nothing found
		inData->fOutMatchRecordCount	= 0;

        // copy the buffer data into a more manageable form
        outBuff = new CBuff();
        if ( outBuff  == nil ) throw( (sInt32) eMemoryError );

        siResult = outBuff->Initialize( inData->fOutDataBuff, true );
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = outBuff->GetBuffStatus();
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = outBuff->SetBuffType( 'StdA' );
        if ( siResult != eDSNoErr ) throw( siResult );

        // Get the record type list
		pRecTypes = dsAllocStringsFromList(0, inData->fInRecTypeList);
		if ( pRecTypes == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
		uInt32 strCnt = 0;
		bool bResetRecTypes = false;
		while(pRecTypes[strCnt] != nil)
		{
			if (strcmp(pRecTypes[strCnt], kDSStdRecordTypeAll) == 0)
			{
				bResetRecTypes = true;
			}
			strCnt++;
		}
        if (strCnt == 0) throw( (sInt32)eDSEmptyRecordTypeList );
		if (bResetRecTypes)
		{
			//in this case we now do the following:
			//A:TODO try to retrieve the native record type from the search base suffix in the config
			//B:TODO if not present then we compare suffixes of the native record types for users, groups, computers and use that
			//C:otherwise we fall back on brute force search through the remaining std record types
			//TODO we only build this list of native record types once for a query and save it in the continue data
			
			//C:
			//mapping rec types - if std to native
			DSFreeStringList(pRecTypes);
			strCnt = 0;
			uInt32 i = 0;
			sLDAPConfigData *pConfig = gpConfigFromXML->ConfigWithNodeNameLock( pContext->fNodeName );
			
			// this is really easy with the new normalized map dictionary, only add the ones we know about
			if (pConfig != nil )
			{
				if( pConfig->fRecordAttrMapDict != nil )
				{
					strCnt = CFDictionaryGetCount( pConfig->fRecordAttrMapDict );
					if( strCnt )
					{
						CFStringRef	*keys = (CFStringRef *) calloc( strCnt, sizeof(CFStringRef) );
						pRecTypes = (char **) calloc(strCnt+1, sizeof(char *));
						
						CFDictionaryGetKeysAndValues( pConfig->fRecordAttrMapDict, (const void **)keys, NULL );
						
						for( i = 0; i < strCnt; i++ )
						{
							CFIndex uiLength = CFStringGetMaximumSizeForEncoding( CFStringGetLength(keys[i]), kCFStringEncodingUTF8 );
							pRecTypes[i] = (char *) calloc( uiLength, sizeof(char) );
							CFStringGetCString( keys[i], pRecTypes[i], uiLength, kCFStringEncodingUTF8 );
						}
						
						DSFree( keys );
					}
				}
				gpConfigFromXML->ConfigUnlock( pConfig );
			}
			// if we didn't end up with a list, then let's throw an error
			if (strCnt == 0) throw( (sInt32)eDSEmptyRecordTypeList );
		}
        //save the number of rec types here to use in separating the buffer data
        countDownRecTypes = strCnt - pLDAPContinue->fRecTypeIndex + 1;

        // Get the attribute pattern match type
        pattMatch = inData->fInPattMatchType;

        // Get the attribute type
		pAttrType = inData->fInAttrType->fBufferData;
		if (	(pattMatch == eDSCompoundExpression) || 
				(pattMatch == eDSiCompoundExpression) )
		{
			pAttrType = aCompoundExp; //used fake string since pAttrType is used in strcmp below
		}
		else
		{
			if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );
		}

        // Get the attribute strings list to match
		pSearchStrings = dsAllocStringsFromList(0, inData->fInPatterns2MatchList);
		if ( pSearchStrings == nil ) throw( (sInt32)eDSEmptyPattern2Match );

		if ( inData->fType == kDoMultipleAttributeValueSearchWithData )
		{
			// Get the attribute list
			cpAttrTypeList = new CAttributeList( inData->fInAttrTypeRequestList );
			if ( cpAttrTypeList == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
			if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );

			// Get the attribute info only flag
			bAttribOnly = inData->fInAttrInfoOnly;
		}
		else
		{
			pTmpDataList = dsBuildListFromStringsPriv( kDSAttributesAll, nil );
			if ( pTmpDataList != nil )
			{
				cpAttrTypeList = new CAttributeList( pTmpDataList );
				if ( cpAttrTypeList == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
				if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );
			}
		}

        // get records of these types
        while ((( pRecTypes[pLDAPContinue->fRecTypeIndex-1] != nil ) && (!bBuffFull)) && (!separateRecTypes))
        {
        	//mapping rec types - if std to native
        	numRecTypes = 1;
			bOCANDGroup = false;
			if (OCSearchList != nil)
			{
				CFRelease(OCSearchList);
				OCSearchList = nil;
			}
            pLDAPRecType = MapRecToLDAPType( (const char *)pRecTypes[pLDAPContinue->fRecTypeIndex-1], pContext->fNodeName, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
            if ( ( pLDAPRecType == nil ) && (countDownRecTypes == 1) )
				throw( (sInt32)eDSInvalidRecordType );
            while (( pLDAPRecType != nil ) && (!bBuffFull))
            {
				bBuffFull = false;
				if (	(::strcmp( pAttrType, kDSAttributesAll ) == 0 ) ||
						(::strcmp( pAttrType, kDSAttributesStandardAll ) == 0 ) ||
						(::strcmp( pAttrType, kDSAttributesNativeAll ) == 0 ) )
				{
					//go get me all records that have any attribute equal to pSearchStrings with pattMatch constraint
					//KW this is a very difficult search to do
					//approach A: set up a very complex search filter to pass to the LDAP server
					//need to be able to handle all standard types that are mapped
					//CHOSE THIS approach B: get each record and parse it completely using native attr types
					//approach C: just like A but concentrate on a selected subset of attr types
					siResult = GetTheseRecords( nil, pSearchStrings, pRecTypes[pLDAPContinue->fRecTypeIndex-1], pLDAPRecType, pattMatch, cpAttrTypeList, pContext, pLDAPContinue, bAttribOnly, outBuff, uiCount, bOCANDGroup, OCSearchList, scope );
				}
				else
				{
					//go get me all records that have pAttrType equal to pSearchStrings with pattMatch constraint
					siResult = FindTheseRecords( pAttrType, pSearchStrings, pRecTypes[pLDAPContinue->fRecTypeIndex-1], pLDAPRecType, pattMatch, cpAttrTypeList, pContext, pLDAPContinue, bAttribOnly, outBuff, uiCount, bOCANDGroup, OCSearchList, scope );
				}

				//outBuff->GetDataBlockCount( &uiCount );
				//cannot use this as there may be records added from different record names
				//at which point the first name will fill the buffer with n records and
				//uiCount is reported as n but as the second name fills the buffer with m MORE records
				//the statement above will return the total of n+m and add it to the previous n
				//so that the total erroneously becomes 2n+m and not what it should be as n+m
				//therefore uiCount is extracted directly out of the FindxxxRecord(s) calls

				if ( siResult == CBuff::kBuffFull )
				{
					bBuffFull = true;
					//set continue if there is more data available
					inData->fIOContinueData = pLDAPContinue;
					
					// check to see if buffer is empty and no entries added
					// which implies that the buffer is too small
					if ( ( uiCount == 0 ) && ( uiTotal == 0 ) )
					{
						throw( (sInt32)eDSBufferTooSmall );
					}

					uiTotal += uiCount;
					inData->fOutMatchRecordCount = uiTotal;
					outBuff->SetLengthToSize();
					siResult = eDSNoErr;
				}
				else if ( siResult == eDSNoErr )
				{
					uiTotal += uiCount;
//	                pContext->fRecNameIndex++;
					pLDAPContinue->msgId = 0;
				}

                if (pLDAPRecType != nil)
                {
                    delete( pLDAPRecType );
                    pLDAPRecType = nil;
                }
                
                if (!bBuffFull)
                {
	                numRecTypes++;
					bOCANDGroup = false;
					if (OCSearchList != nil)
					{
						CFRelease(OCSearchList);
						OCSearchList = nil;
					}
	                //get the next mapping
	                pLDAPRecType = MapRecToLDAPType( (const char *)pRecTypes[pLDAPContinue->fRecTypeIndex-1], pContext->fNodeName, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
                }
                
            } // while mapped Rec Type != nil
            
            if (!bBuffFull)
            {
	            pLDAPContinue->fRecTypeIndex++;
//	            pContext->fRecNameIndex = 1;
	            //reset the LDAP message ID to zero since now going to go after a new type
	            pLDAPContinue->msgId = 0;
	            
	            //KW? here we decide to exit with data full of the current type of records
	            // and force a good exit with the data we have so we can come back for the next rec type
            	separateRecTypes = true;
                //set continue since there may be more data available
                inData->fIOContinueData = pLDAPContinue;
				siResult = eDSNoErr;
				
				//however if this was the last rec type then there will be no more data
	            // check the number of rec types left
	            countDownRecTypes--;
	            if (countDownRecTypes == 0)
	            {
	                inData->fIOContinueData = nil;
				}
            }
            
        } // while loop over record types

        if (( siResult == eDSNoErr ) & (!bBuffFull))
        {
            if ( uiTotal == 0 )
            {
                outBuff->ClearBuff();
            }
            else
            {
                outBuff->SetLengthToSize();
            }

            inData->fOutMatchRecordCount = uiTotal;
        }
    } // try
    
    catch ( sInt32 err )
    {
		siResult = err;
    }

	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	
	if ( (inData->fIOContinueData == nil) && (pLDAPContinue != nil) )
	{
		// we've decided not to return continue data, so we should clean up
		gLDAPContinueTable->RemoveItem( pLDAPContinue );
		pLDAPContinue = nil;
	}

	if (pLDAPRecType != nil)
	{
		delete( pLDAPRecType );
		pLDAPRecType = nil;
	}

    if ( cpAttrTypeList != nil )
    {
		delete( cpAttrTypeList );
		cpAttrTypeList = nil;
    }

	if ( pTmpDataList != nil )
	{
		dsDataListDeallocatePriv( pTmpDataList );
		free(pTmpDataList);
		pTmpDataList = nil;
	}

    if ( outBuff != nil )
    {
		delete( outBuff );
		outBuff = nil;
    }

	DSFreeStringList(pSearchStrings);
	
	DSFreeStringList(pRecTypes);

    return( siResult );

} // DoMultipleAttributeValueSearch


//------------------------------------------------------------------------------------
//	* DoTheseAttributesMatch
//------------------------------------------------------------------------------------

bool CLDAPv3Plugin::DoTheseAttributesMatch(	sLDAPContextData	   *inContext,
											char				   *inAttrName,
											tDirPatternMatch		pattMatch,
											LDAPMessage			   *inResult)
{
	char			   *pAttr					= nil;
	char			   *pVal					= nil;
	BerElement		   *ber						= nil;
	struct berval	  **bValues;
	bool				bFoundMatch				= false;
	LDAP			   *aHost					= nil;

	//let's check all the attribute values for a match on the input name
	//with the given patt match constraint - first match found we stop and
	//then go get it all
	//TODO - room for optimization here
	aHost = fLDAPSessionMgr.LockSession(inContext);
	if (aHost != nil)
	{
		for (	pAttr = ldap_first_attribute(aHost, inResult, &ber );
				pAttr != NULL; pAttr = ldap_next_attribute(aHost, inResult, ber ) )
		{
			if (( bValues = ldap_get_values_len(aHost, inResult, pAttr )) != NULL)
			{
				// for each value of the attribute
				for (int i = 0; bValues[i] != NULL; i++ )
				{
					//need this since bValues might be binary data with no NULL terminator
					pVal = (char *) calloc(1,bValues[i]->bv_len + 1);
					memcpy(pVal, bValues[i]->bv_val, bValues[i]->bv_len);
					if (DoesThisMatch(pVal, inAttrName, pattMatch))
					{
						bFoundMatch = true;
						free(pVal);
						pVal = nil;
						break;
					}
					else
					{
						free(pVal);
						pVal = nil;
					}
				} // for each bValues[i]
				ldap_value_free_len(bValues);
				bValues = NULL;
				
				if (bFoundMatch)
				{
					if (pAttr != nil)
					{
						ldap_memfree( pAttr );
					}
					break;
				}
			} // if bValues = ldap_get_values_len ...
				
			if (pAttr != nil)
			{
				ldap_memfree( pAttr );
			}
		} // for ( loop over ldap_next_attribute )
	} // if aHost != nil
	fLDAPSessionMgr.UnLockSession(inContext);
	
	if (ber != nil)
	{
		ber_free( ber, 0 );
	}

	return( bFoundMatch );

} // DoTheseAttributesMatch


//------------------------------------------------------------------------------------
//	* DoAnyOfTheseAttributesMatch
//------------------------------------------------------------------------------------

bool CLDAPv3Plugin::DoAnyOfTheseAttributesMatch(	sLDAPContextData	   *inContext,
													char				  **inAttrNames,
													tDirPatternMatch		pattMatch,
													LDAPMessage			   *inResult)
{
	char			   *pAttr					= nil;
	char			   *pVal					= nil;
	BerElement		   *ber						= nil;
	struct berval	  **bValues;
	bool				bFoundMatch				= false;
	LDAP			   *aHost					= nil;

	//let's check all the attribute values for a match on the input name
	//with the given patt match constraint - first match found we stop and
	//then go get it all
	//TODO - room for optimization here
	aHost = fLDAPSessionMgr.LockSession(inContext);
	if (aHost != nil)
	{
		for (	pAttr = ldap_first_attribute(aHost, inResult, &ber );
				pAttr != NULL; pAttr = ldap_next_attribute(aHost, inResult, ber ) )
		{
			if (( bValues = ldap_get_values_len(aHost, inResult, pAttr )) != NULL)
			{
				// for each value of the attribute
				for (int i = 0; bValues[i] != NULL; i++ )
				{
					//need this since bValues might be binary data with no NULL terminator
					pVal = (char *) calloc(1,bValues[i]->bv_len + 1);
					memcpy(pVal, bValues[i]->bv_val, bValues[i]->bv_len);
					if (DoAnyMatch(pVal, inAttrNames, pattMatch))
					{
						bFoundMatch = true;
						free(pVal);
						pVal = nil;
						break;
					}
					else
					{
						free(pVal);
						pVal = nil;
					}
				} // for each bValues[i]
				ldap_value_free_len(bValues);
				bValues = NULL;
				
				if (bFoundMatch)
				{
					if (pAttr != nil)
					{
						ldap_memfree( pAttr );
					}
					break;
				}
			} // if bValues = ldap_get_values_len ...
				
			if (pAttr != nil)
			{
				ldap_memfree( pAttr );
			}
		} // for ( loop over ldap_next_attribute )
	} // if aHost != nil
	fLDAPSessionMgr.UnLockSession(inContext);
	
	if (ber != nil)
	{
		ber_free( ber, 0 );
	}

	return( bFoundMatch );

} // DoAnyOfTheseAttributesMatch


// ---------------------------------------------------------------------------
//	* DoAnyMatch
// ---------------------------------------------------------------------------

bool CLDAPv3Plugin::DoAnyMatch (	const char		   *inString,
									char			  **inPatterns,
									tDirPatternMatch	inPattMatch )
{
	const char	   *p			= nil;
	bool			bMatched	= false;
	char		   *string1;
	char		   *string2;
	uInt32			length1		= 0;
	uInt32			length2		= 0;
	uInt32			uMatch		= 0;
	uInt16			usIndex		= 0;

	if ( (inString == nil) || (inPatterns == nil) )
	{
		return( false );
	}

	uMatch = (uInt32) inPattMatch;

	length1 = strlen(inString);
	string1 = (char *) calloc(1, length1 + 1);
	if ( (inPattMatch >= eDSExact) && (inPattMatch <= eDSRegularExpression) )
	{
		strcpy(string1,inString);
	}
	else
	{
		p = inString;
		for ( usIndex = 0; usIndex < length1; usIndex++  )
		{
			string1[usIndex] = toupper( *p );
			p++;
		}
	}


	uInt32 strCount = 0;
	while(inPatterns[strCount] != nil)
	{
		length2 = strlen(inPatterns[strCount]);
		string2 = (char *) calloc(1, length2 + 1);
		
		if ( (inPattMatch >= eDSExact) && (inPattMatch <= eDSRegularExpression) )
		{
			strcpy(string2,inPatterns[strCount]);
		}
		else
		{
			p = inPatterns[strCount];
			for ( usIndex = 0; usIndex < length2; usIndex++  )
			{
				string2[usIndex] = toupper( *p );
				p++;
			}
		}

		switch ( uMatch )
		{
			case eDSExact:
			case eDSiExact:
			{
				if ( strcmp( string1, string2 ) == 0 )
				{
					bMatched = true;
				}
			}
			break;

			case eDSStartsWith:
			case eDSiStartsWith:
			{
				if ( strncmp( string1, string2, length2 ) == 0 )
				{
					bMatched = true;
				}
			}
			break;

			case eDSEndsWith:
			case eDSiEndsWith:
			{
				if ( length1 >= length2 )
				{
					if ( strcmp( string1 + length1 - length2, string2 ) == 0 )
					{
						bMatched = true;
					}
				}
			}
			break;

			case eDSContains:
			case eDSiContains:
			{
				if ( length1 >= length2 )
				{
					if ( strstr( string1, string2 ) != nil )
					{
						bMatched = true;
					}
				}
			}
			break;

			case eDSLessThan:
			case eDSiLessThan:
			{
				if ( strcmp( string1, string2 ) < 0 )
				{
					bMatched = true;
				}
			}
			break;

			case eDSGreaterThan:
			case eDSiGreaterThan:
			{
				if ( strcmp( string1, string2 ) > 0 )
				{
					bMatched = true;
				}
			}
			break;

			case eDSLessEqual:
			case eDSiLessEqual:
			{
				if ( strcmp( string1, string2 ) <= 0 )
				{
					bMatched = true;
				}
			}
			break;

			case eDSGreaterEqual:
			case eDSiGreaterEqual:
			{
				if ( strcmp( string1, string2 ) >= 0 )
				{
					bMatched = true;
				}
			}
			break;

			case eDSAnyMatch:
			default:
				break;
		}
		
		if (string2 != nil)
		{
			delete(string2);
		}
		
		if (bMatched)
		{
			break;
		}
		strCount++;
	}//while(inPatterns[strCount] != nil)

	if (string1 != nil)
	{
		delete(string1);
	}

	return( bMatched );

} // DoAnyMatch


//------------------------------------------------------------------------------------
//	* FindTheseRecords
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::FindTheseRecords (	char			   *inConstAttrType,
										char			  **inAttrNames,
										char			   *inRecType,
										char			   *inNativeRecType,
										tDirPatternMatch	patternMatch,
										CAttributeList	   *inAttrTypeList,
										sLDAPContextData   *inContext,
										sLDAPContinueData  *inContinue,
										bool				inAttrOnly,
										CBuff			   *inBuff,
										uInt32			   &outRecCount,
										bool				inbOCANDGroup,
										CFArrayRef			inOCSearchList,
										ber_int_t			inScope )
{
	sInt32					siResult		= eDSNoErr;
    sInt32					siValCnt		= 0;
    int						ldapReturnCode 	= 0;
    bool					bufferFull		= false;
    LDAPMessage			   *result			= nil;
    char				   *recName			= nil;
    char				   *queryFilter		= nil;
    sLDAPConfigData		   *pConfig			= nil;
    int						searchTO		= 0;
	CDataBuff			   *aRecData		= nil;
	CDataBuff			   *aAttrData		= nil;

	//TODO why not optimize and save the queryFilter in the continue data for the next call in?
	//not that easy over continue data
	
	//build the record query string
	queryFilter = BuildLDAPQueryMultiFilter
									(	inConstAttrType,
										inAttrNames,
										patternMatch,
										inContext->fNodeName,
										false,
										(const char *)inRecType,
										inNativeRecType,
										inbOCANDGroup,
										inOCSearchList );
	    
	outRecCount = 0; //need to track how many records were found by this call to FindTheseRecords
	
    try
    {
    
    	if (inContext == nil ) throw( (sInt32)eDSInvalidContext);
    	if (inContinue == nil ) throw( (sInt32)eDSInvalidContinueData);
    	
    	// check to make sure the queryFilter is not nil
    	if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );
    	
 		aRecData = new CDataBuff();
		if ( aRecData  == nil ) throw( (sInt32) eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData  == nil ) throw( (sInt32) eMemoryError );

		//retrieve the config data
		pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inContext->fNodeName );
		if (pConfig != nil)
		{
			searchTO	= pConfig->fSearchTimeout;
			gpConfigFromXML->ConfigUnlock( pConfig );
			pConfig = NULL;
		}
		
		//LDAP search with rebind capability
		DSDoSearch (	inRecType,
						inNativeRecType,
						inAttrTypeList,
						inContext,
						inContinue,
						inScope,
						fLDAPSessionMgr,
						queryFilter,
						ldapReturnCode,
						searchTO,
						result );

		while ( 	( (ldapReturnCode == LDAP_RES_SEARCH_ENTRY) || (ldapReturnCode == LDAP_RES_SEARCH_RESULT) )
					&& !(bufferFull)
					&& (	(inContinue->fTotalRecCount < inContinue->fLimitRecSearch) ||
							(inContinue->fLimitRecSearch == 0) ) )
        {
            // package the record into the DS format into the buffer
            // steps to add an entry record to the buffer
            // build the fRecData header
            // build the fAttrData
            // append the fAttrData to the fRecData
            // add the fRecData to the buffer inBuff
			if (ldapReturnCode == LDAP_RES_SEARCH_ENTRY)
			{
				aRecData->Clear();
	
				if ( inRecType != nil )
				{
					aRecData->AppendShort( ::strlen( inRecType ) );
					aRecData->AppendString( inRecType );
				}
				else
				{
					aRecData->AppendShort( ::strlen( "Record Type Unknown" ) );
					aRecData->AppendString( "Record Type Unknown" );
				}
	
				// need to get the record name
				recName = GetRecordName( inRecType, result, inContext, siResult );
				if ( siResult != eDSNoErr ) throw( siResult );
				if ( recName != nil )
				{
					aRecData->AppendShort( ::strlen( recName ) );
					aRecData->AppendString( recName );
	
					delete ( recName );
					recName = nil;
				}
				else
				{
					aRecData->AppendShort( ::strlen( "Record Name Unknown" ) );
					aRecData->AppendString( "Record Name Unknown" );
				}
	
				// need to calculate the number of attribute types ie. siValCnt
				// also need to extract the attributes and place them into fAttrData
				//siValCnt = 0;
	
				aAttrData->Clear();
				siResult = GetTheseAttributes( inRecType, inAttrTypeList, result, inAttrOnly, inContext, siValCnt, aAttrData );
				if ( siResult != eDSNoErr ) throw( siResult );
				
				//add the attribute info to the fRecData
				if ( siValCnt == 0 )
				{
					// Attribute count
					aRecData->AppendShort( 0 );
				}
				else
				{
					// Attribute count
					aRecData->AppendShort( siValCnt );
					aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
				}
	
				// add the fRecData now to the inBuff
				siResult = inBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
			}
            // need to check if the buffer is full
			// need to handle full buffer and keep the result alive for the next call in
            if (siResult == CBuff::kBuffFull)
            {
                bufferFull = true;
                
                //save the result if buffer is full
                inContinue->pResult = result;
            }
            else if ( siResult == eDSNoErr )
            {
                ldap_msgfree( result );
				result = nil;
                
				if (ldapReturnCode == LDAP_RES_SEARCH_ENTRY)
				{
					outRecCount++; //another record added
					inContinue->fTotalRecCount++;
				}
				
                //make sure no result is carried in the context
                inContinue->pResult = nil;

				//only get next result if buffer is not full
				DSGetSearchLDAPResult (	fLDAPSessionMgr,
										inContext,
										searchTO,
										result,
										LDAP_MSG_ONE,
										inContinue->msgId,
										ldapReturnCode,
										false);
            }
            else
            {
                //make sure no result is carried in the context
                inContinue->pResult = nil;
                throw( (sInt32)eDSInvalidBuffFormat );
            }
            
        } // while loop over entries

		if ( (result != inContinue->pResult) && (result != nil) )
		{
			ldap_msgfree( result );
			result = nil;
		}

		//here we attempt to cleanup the response list of responses that have already been deleted
		//if siResult is eDSNoErr then the msgId will be removed from inContinue in the calling routine DoAttributeValueSearch
		if ( (siResult == eDSNoErr) && (inContinue != NULL) && (inContinue->msgId != 0) )
		{
			LDAP *aHost = nil;
			aHost = fLDAPSessionMgr.LockSession(inContext);
			if (aHost != nil)
			{
				DSSearchCleanUp(aHost, inContinue->msgId);
			}
			fLDAPSessionMgr.UnLockSession(inContext);
		}

    } // try block

    catch ( sInt32 err )
    {
        siResult = err;
    }
	
	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}

	if ( aRecData != nil )
	{
		delete( aRecData );
		aRecData = nil;
	}
	if ( aAttrData != nil )
	{
		delete( aAttrData );
		aAttrData = nil;
	}

    return( siResult );

} // FindTheseRecords


//------------------------------------------------------------------------------------
//	* SetAttributeValueForDN
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::SetAttributeValueForDN( sLDAPContextData *pContext, char *inDN, const char *inRecordType, char *inAttrType, char **inValues )
{
	sInt32		siResult		= eDSNoErr;
	char		*pLDAPAttrType	= NULL;
	
	// if we have values we can continue..
	if( inValues != NULL && *inValues != NULL )
	{
		//get the first mapping
		pLDAPAttrType = MapAttrToLDAPType( (const char *)inRecordType, inAttrType, pContext->fNodeName, 1, true );

		// if we have a mapping and we have values
		if( pLDAPAttrType != NULL )
		{
			LDAPMod		stMod			= { 0 };
			int			ldapReturnCode	= 0;
			berval		**bvals			= NULL;
			LDAPMod		*pMods[2];

			// first let's build our berEntries
			while( *inValues )
			{
				berval *bval = (berval *) calloc( sizeof(berval), 1 );
				if( bval != NULL )  // if a calloc fails we have bigger issues, we'll just stop here.. set siResult
				{
					bval->bv_val = *inValues;
					bval->bv_len = strlen( *inValues );
					ber_bvecadd( &bvals, bval );
				}
				else
				{
					siResult = eMemoryAllocError;
					break;
				}
				inValues++;
			};
			
			// if we have bvals and we didn't get a memoryallocation error
			if( bvals != NULL && siResult == eDSNoErr )
			{
				// now, create the mod entry
				stMod.mod_op		= LDAP_MOD_REPLACE | LDAP_MOD_BVALUES;
				stMod.mod_type		= pLDAPAttrType;
				stMod.mod_bvalues   = bvals;
				
				pMods[0]			= &stMod;
				pMods[1]			= NULL;
				
				// now let's get our LDAP session and do the work..
				LDAP *aHost = fLDAPSessionMgr.LockSession( pContext );
				if( aHost != NULL )
				{
					ldapReturnCode = ldap_modify_s( aHost, inDN, pMods );
				}
				else
				{
					ldapReturnCode = LDAP_LOCAL_ERROR;
				}
				fLDAPSessionMgr.UnLockSession( pContext );
				
				if( ldapReturnCode != LDAP_SUCCESS )
				{
					siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSAttributeNotFound );
				}
			}
			
			delete( pLDAPAttrType );
			pLDAPAttrType = NULL;
			
			if( bvals )
			{
				ber_memvfree( (void **) bvals );
				bvals = NULL;
			}			
		} // if( pLDAPAttrType != NULL )
		else
		{
			siResult = eDSNoStdMappingAvailable;
		}
	}
	else
	{
		siResult = eDSNullParameter;
	}
	
	return( siResult );
} // SetAttributeValueForDN


//------------------------------------------------------------------------------------
//	* TryPWSPasswordSet
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::TryPWSPasswordSet( tDirNodeReference inNodeRef, uInt32 inAuthMethodCode, sLDAPContextData *pContext, tDataBufferPtr inAuthBuffer, const char *inRecordType )
{
	tDataBufferPtr  newAuthMethod   = NULL;
	tDataBufferPtr  authResult		= dsDataBufferAllocatePriv( 2048 );
	tContextData	continueData	= NULL;
	sInt32			siResult		= eDSAuthFailed;
	char			**dn			= NULL;
	char			*pUsername		= NULL;
	int				userBufferItem	= 1;
	
	// pick an auth method
	switch( inAuthMethodCode )
	{
		case kAuthNewUserWithPolicy:
		case kAuthNewUser:
			// already added to password server
			userBufferItem = 3;
			break;
		default:
			newAuthMethod = dsDataNodeAllocateString( inNodeRef, kDSStdAuthNewUser );
	}
	
	// if our buffer was allocated, we can continue..
	if( authResult != NULL )
	{
		// if we we are authenticated as a user and have a username coming in in the buffer
		if( pContext->fLDAPNodeStruct->bAuthCallActive && GetUserNameFromAuthBuffer(inAuthBuffer, userBufferItem, &pUsername) == eDSNoErr )
		{
			// we need to parse out the username from the authed user so we can stuff it in the buffer
			dn = ldap_explode_dn( pContext->fLDAPNodeStruct->fLDAPUserName, 1 );
		}
	}
	
	// If we have a dn, let's stuff a buffer with the right information..
	if( dn != NULL )
	{
		tDataBufferPtr  authData	= NULL;
		char			*pUserID	= NULL;
		char			**aaArray   = NULL;
		unsigned long   aaCount		= 0;
		
		// Let's locate the user and get the Auth Authority, then we can continue
		siResult = GetAuthAuthority( pContext, dn[0], fLDAPSessionMgr, &aaCount, &aaArray, kDSStdRecordTypeUsers );
		if ( siResult == eDSNoErr ) 
		{
			unsigned long   idx			= 0;

			// loop through all possibilities and find the password server authority
			while ( idx < aaCount )
			{
				char	*aaVersion  = NULL;
				char	*aaTag		= NULL;
				char	*aaData		= NULL;
				
				//parse this value of auth authority
				siResult = ParseAuthAuthority( aaArray[idx], &aaVersion, &aaTag, &aaData );
				if ( aaArray[idx] )
				{
					free( aaArray[idx] );
					aaArray[idx] = NULL;
				}
				
				if( strcasecmp( aaTag, kDSTagAuthAuthorityPasswordServer ) == 0 )
				{
					// we don't free this cause we're going to use it later..
					pUserID = aaData;
					aaData = NULL;
				}
				
				if( aaVersion != NULL )
				{
					free( aaVersion );
					aaVersion = NULL;
				}
				
				if( aaTag != NULL )
				{
					free( aaTag );
					aaTag = NULL;
				}
				
				if( aaData != NULL )
				{
					free( aaData );
					aaData = NULL;
				}
				++idx;
			}
			
			if( aaArray != NULL )
			{
				free( aaArray );
				aaArray = NULL;
			}
		}
		
		if( pUserID != NULL && (authData = dsDataBufferAllocatePriv(1024)) != NULL )
		{
			char			*pPtr = authData->fBufferData;
			long			lTempLength;

			// auth'ed username
			lTempLength = strlen( pUserID ) + 1;
			*((long *) pPtr) = lTempLength;
			pPtr += sizeof(long);
			bcopy( pUserID, pPtr, lTempLength );
			pPtr += lTempLength;
			
			// auth'ed password
			lTempLength = pContext->fLDAPNodeStruct->fLDAPCredentialsLen+1;
			*((long *) pPtr) = lTempLength;
			pPtr += sizeof(long);
			bcopy( pContext->fLDAPNodeStruct->fLDAPCredentials, pPtr, lTempLength );
			pPtr += lTempLength;
			
			// buffer that was passed to us
			bcopy( inAuthBuffer->fBufferData, pPtr, inAuthBuffer->fBufferLength );
			pPtr += inAuthBuffer->fBufferLength;
			
			authData->fBufferLength = pPtr - authData->fBufferData;
			
			// since we don't know the actual IP of PasswordServer, we'll pull it from the userID we are signed in as.
			char *ipAddress = strchr( pUserID, ':' ) + 1;
			if( ipAddress != NULL && strlen(ipAddress) )
			{
				char			*newAuthority		= NULL;
				char			*newKerbAuthority   = NULL;
				char			*newPWSid			= NULL;
				tDataBufferPtr  pAuthType			= NULL;
				unsigned long	newPWSlength		= 0;
				int				iLength				= 0;
				
				if ( newAuthMethod != NULL )
				{
					siResult = dsDoDirNodeAuth( inNodeRef, newAuthMethod, true, authData, authResult, &continueData );
								
					// now let's parse the buffer for the response information and build the auth authority string
					if( siResult == eDSNoErr )
					{
						newPWSlength = *((unsigned long *)authResult->fBufferData);
						newPWSid = (char *) calloc( sizeof(char), newPWSlength + 1 );
						strncpy( newPWSid, authResult->fBufferData + sizeof(newPWSlength), newPWSlength );
					}
				}
				else
				{
					if ( pContext->fPWSUserID != NULL )
					{
						// fetch the ID we got from the preceding password server call
						newPWSlength = pContext->fPWSUserIDLength;
						newPWSid = pContext->fPWSUserID;
						// hand-off ownership of the memory
						pContext->fPWSUserID = NULL;
					}
					else
					{
						siResult = eDSBadContextData;	// appropriate error?
					}
				}
				
				if( siResult == eDSNoErr )
				{
					iLength = strlen( kDSValueAuthAuthorityPasswordServerPrefix ) + newPWSlength + strlen( ipAddress ) + 2;
					newAuthority = (char *) calloc( sizeof(char), iLength );
					strcpy( newAuthority, kDSValueAuthAuthorityPasswordServerPrefix );
					strcat( newAuthority, newPWSid );
					strcat( newAuthority, ":" );
					strcat( newAuthority, ipAddress );
				}
				
				// now let's parse out the KerberosPrincipal
				if( siResult == eDSNoErr && newPWSid && (pAuthType = dsDataNodeAllocateString(inNodeRef, kDSStdAuthGetKerberosPrincipal)) != NULL )
				{
					bcopy( &newPWSlength, authData->fBufferData, sizeof(newPWSlength) );
					bcopy( newPWSid, authData->fBufferData+sizeof(newPWSlength), newPWSlength );
					authData->fBufferLength = sizeof(newPWSlength) + newPWSlength;
					
					if( dsDoDirNodeAuth(inNodeRef, pAuthType, true, authData, authResult, NULL) == eDSNoErr )
					{
						long	lLength		= *((long *)authResult->fBufferData);
						char	*kerbPrinc  = (char *) calloc( sizeof(char), lLength );
						char	*cr			= NULL;
						char	*comma		= strchr( newPWSid, ',' );
						int		iTmpLen		= comma - newPWSid;

						newKerbAuthority = (char *) calloc( sizeof(char), 1024 );

						if( kerbPrinc != NULL && newKerbAuthority != NULL )
						{
							bcopy( authResult->fBufferData+sizeof(long), kerbPrinc, lLength );
							
							// if we have a CR/LF let's clear it..
							if( cr = strstr( kerbPrinc, "\r\n" ) )
							{
								*cr = '\0';
							}
							
							char *atSign = strchr( kerbPrinc, '@' );
							
							if( atSign )
							{
								// first add Kerberos Tag
								strcpy( newKerbAuthority, kDSValueAuthAuthorityKerberosv5 );
								strncat( newKerbAuthority, newPWSid, iTmpLen );
								strcat( newKerbAuthority, ";" );
								strcat( newKerbAuthority, kerbPrinc );
								strcat( newKerbAuthority, ";" );
								strcat( newKerbAuthority, atSign+1 );
								strcat( newKerbAuthority, ";" );
								strcat( newKerbAuthority, comma+1 );
								strcat( newKerbAuthority, ":" );
								strcat( newKerbAuthority, ipAddress );
							}
						}
						
						if( kerbPrinc )
						{
							free( kerbPrinc );
							kerbPrinc = NULL;
						}
					}
					
					// if we have a new auth authority, let's set it for the record
					if( newAuthority )
					{
						// let's get the DN for the record that we're updating
						char *pUserDN = GetDNForRecordName( pUsername, pContext, fLDAPSessionMgr, inRecordType );

						if( pUserDN )
						{
							// Let's set the authentication authority now...  We use a special internal routine so we don't
							//   get a different record on a different node and for speed.
							char *pValues[] = { newAuthority, newKerbAuthority, NULL };
							siResult = SetAttributeValueForDN( pContext, pUserDN, inRecordType, kDSNAttrAuthenticationAuthority, pValues );
							if( siResult == eDSNoErr )
							{
								// let's put the password marker too...
								char *pValues[] = { kDSValueNonCryptPasswordMarker, NULL };
								SetAttributeValueForDN( pContext, pUserDN, inRecordType, kDS1AttrPassword, pValues );
							}
							
							free( pUserDN );
							pUserDN = NULL;
						}
							
					}
				}
				
				if( newKerbAuthority != NULL )
				{
					free( newKerbAuthority );
					newKerbAuthority = NULL;
				}				

				if( newAuthority )
				{
					free( newAuthority );
					newAuthority = NULL;
				}
				
				if( newPWSid != NULL )
				{
					free( newPWSid );
					newPWSid = NULL;
				}
				
				if( pAuthType != NULL ) 
				{
					dsDataBufferDeAllocate( inNodeRef, pAuthType );
					pAuthType = NULL;
				}
			}
			free( pUserID );
			pUserID = NULL;
		}
		
		if( authData != NULL )
		{
			dsDataBufferDeAllocate( inNodeRef, authData );
			authData = NULL;
		}

		// we already checked this one..
		ldap_value_free( dn );
		dn = NULL;
	} // if( dn != NULL )
	
	if( pUsername != NULL )
	{
		free( pUsername );
		pUsername = NULL;
	}
	
	if( authResult )
	{
		dsDataBufferDeAllocate( inNodeRef, authResult );
		authResult = NULL;
	}

	if( newAuthMethod )
	{
		dsDataBufferDeAllocate( inNodeRef, newAuthMethod );
		newAuthMethod = NULL;
	}
	
	return siResult;
} // TryPWSPasswordSet

//------------------------------------------------------------------------------------
//	* DoAuthentication
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoAuthentication ( sDoDirNodeAuth *inData )
{
	sInt32 siResult = eDSAuthFailed;

	siResult = DoAuthenticationOnRecordType( (sDoDirNodeAuthOnRecordType *)inData, (const char *)kDSStdRecordTypeUsers );

	return( siResult );

} // DoAuthentication


//------------------------------------------------------------------------------------
//	* DoAuthenticationOnRecordType
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoAuthenticationOnRecordType ( sDoDirNodeAuthOnRecordType *inData, const char* inRecordType )
{
	sInt32						siResult			= eDSAuthFailed;
	uInt32						uiAuthMethod		= 0;
	sLDAPContextData			*pContext			= nil;
	sLDAPContinueData   		*pContinueData		= NULL;
	char*						userName			= NULL;
	LDAPv3AuthAuthorityHandlerProc	handlerProc 	= NULL;
	
	try
	{
		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidNodeRef );
		if ( inData->fInAuthStepData == nil ) throw( (sInt32)eDSNullAuthStepData );
		if ( inRecordType == nil ) throw( (sInt32)eDSNullRecType );
		
		if ( inData->fIOContinueData != NULL )
		{
			// get info from continue
			pContinueData = (sLDAPContinueData *)inData->fIOContinueData;
			if ( gLDAPContinueTable->VerifyItem( pContinueData ) == false )
			{
				throw( (sInt32)eDSInvalidContinueData );
			}
			
			if ( eDSInvalidContinueData == nil ) throw( (sInt32)(pContinueData->fAuthHandlerProc) );
			handlerProc = (LDAPv3AuthAuthorityHandlerProc)(pContinueData->fAuthHandlerProc);
			if (handlerProc != NULL)
			{
				siResult = (handlerProc)(inData->fInNodeRef, inData->fInAuthMethod, pContext,
										 &pContinueData, inData->fInAuthStepData, 
										 inData->fOutAuthStepDataResponse, 
										 inData->fInDirNodeAuthOnlyFlag, 
										 pContinueData->fAuthAuthorityData, NULL,
                                         fLDAPSessionMgr, inRecordType);
			}
		}
		else
		{
			unsigned long idx = 0;
			unsigned long aaCount = 0;
			char **aaArray;
                
			// first call
			// note: if GetAuthMethod() returns eDSAuthMethodNotSupported we want to keep going
			// because password server users may still be able to use the method with PSPlugin
			if (inData->fInAuthMethod != nil)
			{
				DBGLOG1( kLogPlugin, "CLDAPv3Plugin: DoAuthenticationOnRecordType - Attempting use of authentication method %s", inData->fInAuthMethod->fBufferData );
			}
			siResult = dsGetAuthMethodEnumValue( inData->fInAuthMethod, &uiAuthMethod );
			if ( ( siResult != eDSNoErr ) && ( siResult != eDSAuthMethodNotSupported ) )
				throw(siResult);

			if ( ( siResult == eDSNoErr) && ( uiAuthMethod == kAuth2WayRandom ) )
			{
				// for 2way random the first buffer is the username
				if ( inData->fInAuthStepData->fBufferLength > inData->fInAuthStepData->fBufferSize ) throw( (sInt32)eDSInvalidBuffFormat );
				userName = (char*)calloc( inData->fInAuthStepData->fBufferLength + 1, 1 );
				strncpy( userName, inData->fInAuthStepData->fBufferData, inData->fInAuthStepData->fBufferLength );
			}
			else if ( ( siResult == eDSNoErr) && ( uiAuthMethod == kAuthGetPolicy ) )
			{
				siResult = GetUserNameFromAuthBuffer( inData->fInAuthStepData, 3, &userName );
				if ( siResult != eDSNoErr ) throw( siResult );
			}
			else
			{
				siResult = GetUserNameFromAuthBuffer( inData->fInAuthStepData, 1, &userName );
				if ( siResult != eDSNoErr ) throw( siResult );
			}
			// get the auth authority
			siResult = GetAuthAuthority( pContext,
										userName,
										fLDAPSessionMgr,
										&aaCount,
										&aaArray, inRecordType );
			
			if ( siResult == eDSNoErr ) 
			{
				// loop through all possibilities for set
				// do first auth authority that supports the method for check password
				bool bLoopAll = IsWriteAuthRequest( uiAuthMethod );
				bool bIsSecondary = false;
				char* aaVersion = NULL;
				char* aaTag = NULL;
				char* aaData = NULL;
				siResult = eDSAuthMethodNotSupported;
				
				// let's parse all of the authorities and put them into a map so we can just iterate through
				map<string, LDAPv3AuthTagStruct> authMethods;
				
				for( idx = 0; idx < aaCount; idx++ )
				{
					if( aaArray[idx] )
					{
						//parse this value of auth authority
						if( ParseAuthAuthority(aaArray[idx], &aaVersion, &aaTag, &aaData) == eDSNoErr )
						{
							if( aaTag )
							{
								authMethods.insert( make_pair(string(aaTag), LDAPv3AuthTagStruct(aaVersion,aaData)) );
								
								free( aaTag );
								aaTag = NULL;
							}
							
							if( aaVersion )
							{
								free( aaVersion );
								aaVersion = NULL;
							}
							
							if( aaData )
							{
								free( aaData );
								aaData = NULL;
							}
						}
						free( aaArray[idx] );
						aaArray[idx] = NULL;
					}
				}
				
				map<string,LDAPv3AuthTagStruct>::iterator authIterator = authMethods.begin();
				
				LDAPv3AuthTagStruct *kerbAuthAuthority = NULL; 

				map<string,LDAPv3AuthTagStruct>::iterator kerbIterator = authMethods.find(string(kDSTagAuthAuthorityKerberosv5));
				if( kerbIterator != authMethods.end() )
				{
					kerbAuthAuthority = &(kerbIterator->second);
				}
				
				while ( authIterator != authMethods.end() &&
						(siResult == eDSAuthMethodNotSupported || (bLoopAll && siResult == eDSNoErr)) )
				{
					LDAPv3AuthTagStruct authData = authIterator->second;

					handlerProc = GetLDAPv3AuthAuthorityHandler( authIterator->first.c_str() );
					if (handlerProc != NULL)
					{
						siResult = (handlerProc)(inData->fInNodeRef, inData->fInAuthMethod, pContext, 
												&pContinueData, 
												inData->fInAuthStepData, 
												inData->fOutAuthStepDataResponse,
												inData->fInDirNodeAuthOnlyFlag,(char *) authData.authData.c_str(), (char *)(kerbAuthAuthority ? kerbAuthAuthority->authData.c_str() : NULL),
												fLDAPSessionMgr, inRecordType);
						if ( siResult == eDSNoErr )
						{
							if ( pContinueData != NULL )
							{
								// we are supposed to return continue data
								// remember the proc we used
								pContinueData->fAuthHandlerProc = (void*)handlerProc;
								pContinueData->fAuthAuthorityData = (char *)authData.authData.c_str();
								break;
							}
							else
							{
								bIsSecondary = true;
							}
						}
					}
					else
					{
						siResult = eDSAuthMethodNotSupported;
					}
					++authIterator;
				}
				
				// If we are a doing a kAuthNewUser or kAuthNewUserWithPolicy, add an auth-authority for
				// the new user.
				if( pContext->fPWSNodeRef != 0 &&
					(uiAuthMethod == kAuthNewUser || uiAuthMethod == kAuthNewUserWithPolicy ) )
				{
					TryPWSPasswordSet( pContext->fPWSNodeRef, uiAuthMethod, pContext, inData->fInAuthStepData, inRecordType );
				}
				
				// need to free remaining attributes
				if ( bIsSecondary && siResult == eDSAuthMethodNotSupported )
					siResult = eDSNoErr;
				
				if (aaArray != NULL) {
					free( aaArray );
					aaArray = NULL;
				}
			}
			else
			{
				// If we are a doing a SetPassword or SetPasswordAsRoot, let's see if we have Password Server available first
				// If so, we'll try to create a password slot for the user and make them a PasswordServer User
				// If we have a fPWSNodeRef, we already have a password server available... 
				//		because the user that authenticated is a passwordServer user.. which means
				//		they may have rights to set the password
				
				if( pContext->fPWSNodeRef != 0 &&
					(uiAuthMethod == kAuthSetPasswd || uiAuthMethod == kAuthSetPasswdAsRoot) )
				{
					siResult = TryPWSPasswordSet( pContext->fPWSNodeRef, uiAuthMethod, pContext, inData->fInAuthStepData, inRecordType );
				}
				
				// well, the PasswordServer Set failed, let's try to do a normal one..
				if( siResult != eDSNoErr )
				{
					//revert to basic
					siResult = DoBasicAuth(inData->fInNodeRef,inData->fInAuthMethod, pContext, 
										&pContinueData, inData->fInAuthStepData,
										inData->fOutAuthStepDataResponse,
										inData->fInDirNodeAuthOnlyFlag, NULL, NULL,
										fLDAPSessionMgr, inRecordType);
					if (pContinueData != NULL && siResult == eDSNoErr)
					{
						// we are supposed to return continue data
						// remember the proc we used
						pContinueData->fAuthHandlerProc = (void*)CLDAPv3Plugin::DoBasicAuth;
					}
				}
			}
		}
	}
    
	catch( sInt32 err )
	{
		siResult = err;
	}
	
	// if this was a set password let's see if it was on an auth'd connection
	if ( siResult == eDSNoErr && uiAuthMethod == kAuthSetPasswdAsRoot && pContext->fLDAPNodeStruct->bAuthCallActive )
	{
		char *accountId = GetDNForRecordName ( userName, pContext, fLDAPSessionMgr, inRecordType );
		if( accountId && strcmp( accountId, (char *)pContext->fLDAPNodeStruct->fLDAPUserName) == 0 )
		{
			char	*password = NULL;
			GetUserNameFromAuthBuffer( inData->fInAuthStepData, 2, &password );
			
			// let's update the node struct otherwise we'll have issues on reconnect
			pContext->fLDAPNodeStruct->updateCredentials( password, strlen(password), kDSStdAuthClearText );
			fLDAPSessionMgr.CredentialChange( pContext->fLDAPNodeStruct, accountId );
			DBGLOG( kLogPlugin, "CLDAPv3Plugin: DoAuthenticationOnRecordType - Updated session password" );
			
			DSFreePassword( password );
		}
		
		DSFree( accountId );
	}
	
	DSFree(userName);

	inData->fResult = siResult;
	inData->fIOContinueData = pContinueData;

	return( siResult );

} // DoAuthenticationOnRecordType


//------------------------------------------------------------------------------------
//	* IsWriteAuthRequest
//------------------------------------------------------------------------------------

bool CLDAPv3Plugin::IsWriteAuthRequest ( uInt32 uiAuthMethod )
{
	switch ( uiAuthMethod )
	{
		case kAuthSetPasswd:
		case kAuthSetPasswdAsRoot:
		case kAuthChangePasswd:
			return true;
			
		default:
			return false;
	}
} // IsWriteAuthRequest


//------------------------------------------------------------------------------------
//	* DoKerberosAuth
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoKerberosAuth(	tDirNodeReference inNodeRef,
										tDataNodePtr inAuthMethod,
										sLDAPContextData* inContext,
										sLDAPContinueData** inOutContinueData,
										tDataBufferPtr inAuthData,
										tDataBufferPtr outAuthData,
										bool inAuthOnly,
										char* inAuthAuthorityData,
										char* inKerberosId,
										CLDAPNode& inLDAPSessionMgr,
										const char *inRecordType )
{
    sInt32					result				= eDSAuthFailed;
	krb5_creds				credentials;
	krb5_context			krbContext			= NULL;
	krb5_principal			principal			= NULL;
	krb5_get_init_creds_opt	options;
	char					*pKerberosID		= NULL;
	char					*pPassword			= NULL;
	unsigned long			uiAuthMethod		= 0;
	
	DBGLOG( kLogPlugin, "CLDAPv3Plugin: DoKerberosAuth::" );
	
	if ( inAuthOnly == false )
	{
		result = eDSAuthMethodNotSupported;
	} 
	else if ( inAuthData->fBufferData != NULL )
	{
		DBGLOG1( kLogPlugin, "CLDAPv3Plugin: DoKerberosAuth - Attempting use of authentication method %s", inAuthMethod->fBufferData );
	
		// if we don't have a kerberos ID, let's get the name from the buffer instead
		if( NULL == pKerberosID )
			GetUserNameFromAuthBuffer( inAuthData, 1, &pKerberosID );
		else
			pKerberosID = strdup( inKerberosId );
		
		GetUserNameFromAuthBuffer( inAuthData, 2, &pPassword );
		
		if( pKerberosID != NULL && pPassword != NULL )
		{
			gLDAPKerberosMutex->Wait();
			
			result = dsGetAuthMethodEnumValue( inAuthMethod, &uiAuthMethod );
			if ( eDSNoErr == result )
			{
				switch( uiAuthMethod )
				{
					case kAuthNativeClearTextOK:
					case kAuthClearText:
					case kAuthNativeNoClearText:
					case kAuthCrypt:
						result = krb5_init_context( &krbContext );
						if( 0 == result )
							result = krb5_parse_name( krbContext, inKerberosId, &principal );

						bzero( &credentials, sizeof(credentials) );

						if( 0 == result )
						{
							krb5_get_init_creds_opt_init( &options );
							krb5_get_init_creds_opt_set_tkt_life( &options, 30 );
							krb5_get_init_creds_opt_set_address_list( &options, NULL );
							
							result = krb5_get_init_creds_password( krbContext, &credentials, principal, pPassword, NULL, 0, 0, NULL, &options );
						}
							
						// if had an error or it wasn't pre-authenticated, we can't confirm the validity
						if( 0 != result ||
							(0 == result && (credentials.ticket_flags & TKT_FLG_PRE_AUTH) == 0) )
						{
							result = eDSAuthFailed;
						}
						
						krb5_free_principal( krbContext, principal );
						krb5_free_cred_contents( krbContext, &credentials );
						krb5_free_context( krbContext );
						break;
					case kAuthChangePasswd:
						// we could support old and new password sets, but not for now
					default:
						result = eDSAuthMethodNotSupported;
						break;
				}
			}	
			gLDAPKerberosMutex->Signal();
		}
		else
		{
			result = eDSAuthParameterError;
		}
	}
	else
	{
		result = eDSNullDataBuff;
	}
	
	DSFree( pKerberosID );
	DSFreePassword( pPassword );
	
	return result;
} // DoKerberosAuth

//------------------------------------------------------------------------------------
//	* DoPasswordServerAuth
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoPasswordServerAuth(
    tDirNodeReference inNodeRef,
    tDataNodePtr inAuthMethod, 
    sLDAPContextData* inContext, 
    sLDAPContinueData** inOutContinueData, 
    tDataBufferPtr inAuthData, 
    tDataBufferPtr outAuthData, 
    bool inAuthOnly,
    char* inAuthAuthorityData,
	char* inKerberosId,
    CLDAPNode& inLDAPSessionMgr,
	const char *inRecordType )
{
    sInt32					result				= eDSAuthFailed;
    sInt32					error;
    uInt32					authMethod;
    char				   *serverAddr;
    char				   *uidStr				= NULL;
    long					uidStrLen;
    tDataBufferPtr			authDataBuff		= NULL;
    tDataBufferPtr			authDataBuffTemp	= NULL;
    char				   *nodeName			= NULL;
    char				   *userName			= NULL;
    char				   *accountId			= NULL;
	char				   *password			= NULL;
    sLDAPContinueData      *pContinue			= NULL;
    tContextData			continueData		= NULL;
    unsigned long			aaCount				= 0;
	char				  **aaArray;

    if ( !inAuthAuthorityData || *inAuthAuthorityData == '\0' )
        return eDSAuthParameterError;
    
    try
    {        
        serverAddr = strchr( inAuthAuthorityData, ':' );
        if ( serverAddr )
        {
            uidStrLen = serverAddr - inAuthAuthorityData;
            uidStr = (char *) calloc(1, uidStrLen+1);
            if ( uidStr == nil ) throw( (sInt32)eMemoryError );
            strncpy( uidStr, inAuthAuthorityData, uidStrLen );
            
            // advance past the colon
            serverAddr++;
            
			DBGLOG( kLogPlugin, "CLDAPv3Plugin: DoPasswordServerAuth::" );
			if (inAuthMethod != nil)
			{
				DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Attempting use of authentication method %s", inAuthMethod->fBufferData );
			}
            error = dsGetAuthMethodEnumValue( inAuthMethod, &authMethod );
			switch( authMethod )
			{
				case kAuth2WayRandom:
					if ( inOutContinueData == nil )
						throw( (sInt32)eDSNullParameter );
					
					if ( *inOutContinueData == nil )
					{
						pContinue = (sLDAPContinueData *)::calloc( 1, sizeof( sLDAPContinueData ) );
						gLDAPContinueTable->AddItem( pContinue, inNodeRef );
						
						// make a buffer for the user ID
						authDataBuff = ::dsDataBufferAllocatePriv( uidStrLen + 1 );
						if ( authDataBuff == nil ) throw ( (sInt32)eMemoryError );
						
						// fill
						strcpy( authDataBuff->fBufferData, uidStr );
						authDataBuff->fBufferLength = uidStrLen;
					}
					else
					{
						pContinue = *inOutContinueData;
						if ( gLDAPContinueTable->VerifyItem( pContinue ) == false )
							throw( (sInt32)eDSInvalidContinueData );
					}
					break;
					
				case kAuthSetPasswd:
				case kAuthSetPolicy:
					{
						char* aaVersion = NULL;
						char* aaTag = NULL;
						char* aaData = NULL;
						unsigned int idx;
						sInt32 lookupResult;
						char* endPtr = NULL;
						
						// lookup the user that wasn't passed to us
						error = GetUserNameFromAuthBuffer( inAuthData, 3, &userName );
						if ( error != eDSNoErr ) throw( error );
						
						// lookup authauthority attribute
						error = GetAuthAuthority( inContext,
										userName,
										inLDAPSessionMgr,
										&aaCount,
										&aaArray,
										inRecordType );
						
						if ( error != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
						
						// don't break or throw to guarantee cleanup
						lookupResult = eDSAuthFailed;
						for ( idx = 0; idx < aaCount && lookupResult == eDSAuthFailed; idx++ )
						{
							//parse this value of auth authority
							error = ParseAuthAuthority( aaArray[idx], &aaVersion, &aaTag, &aaData );
							if ( aaArray[idx] ) {
								free( aaArray[idx] );
								aaArray[idx] = NULL;
							}
							
							// need to check version
							if ( error != eDSNoErr )
								lookupResult = eParameterError;
							
							if ( error == eDSNoErr && strcmp(aaTag, kDSTagAuthAuthorityPasswordServer) == 0 )
							{
								endPtr = strchr( aaData, ':' );
								if ( endPtr == NULL )
								{
									lookupResult = eParameterError;
								}
								else
								{
									*endPtr = '\0';
									lookupResult = eDSNoErr;
								}
							}
							
							DSFreeString(aaVersion);
							DSFreeString(aaTag);
							if (lookupResult != eDSNoErr) {
								DSFreeString(aaData);
							}
						}
						
						if ( lookupResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
						
						// do the usual
						error = RepackBufferForPWServer( inAuthData, uidStr, 1, &authDataBuffTemp );
						if ( error != eDSNoErr ) throw( error );
						
						// put the admin user ID in slot 3
						error = RepackBufferForPWServer( authDataBuffTemp, aaData, 3, &authDataBuff );
						DSFreeString(aaData);
						if ( error != eDSNoErr ) throw( error );
					}
					break;
					
				case kAuthGetPolicy:
					{
						char* aaVersion = NULL;
						char* aaTag = NULL;
						char* aaData = NULL;
						unsigned int idx;
						sInt32 lookupResult;
						char* endPtr = NULL;
						
						// lookup the user that wasn't passed to us
						error = GetUserNameFromAuthBuffer( inAuthData, 1, &userName );
						if ( error != eDSNoErr ) throw( error );
						
						// put the user ID in slot 3
						error = RepackBufferForPWServer( inAuthData, uidStr, 3, &authDataBuffTemp );
						if ( error != eDSNoErr ) throw( error );

						// lookup authauthority attribute
						if ( userName[0] != '\0' )
						{
							error = GetAuthAuthority( inContext,
											userName,
											inLDAPSessionMgr,
											&aaCount,
											&aaArray,
											inRecordType );
							if ( error != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
							
							// don't break or throw to guarantee cleanup
							lookupResult = eDSAuthFailed;
							for ( idx = 0; idx < aaCount && lookupResult == eDSAuthFailed; idx++ )
							{
								//parse this value of auth authority
								error = ParseAuthAuthority( aaArray[idx], &aaVersion, &aaTag, &aaData );
								if ( aaArray[idx] ) {
									free( aaArray[idx] );
									aaArray[idx] = NULL;
								}
								
								// need to check version
								if ( error != eDSNoErr )
									lookupResult = eParameterError;
								
								if ( error == eDSNoErr && strcmp(aaTag, kDSTagAuthAuthorityPasswordServer) == 0 )
								{
									endPtr = strchr( aaData, ':' );
									if ( endPtr == NULL )
									{
										lookupResult = eParameterError;
									}
									else
									{
										*endPtr = '\0';
										lookupResult = eDSNoErr;
									}
								}
								
								DSFreeString(aaVersion);
								DSFreeString(aaTag);
								
								if (lookupResult != eDSNoErr) {
									DSFreeString(aaData);
								}
							}
							
							if ( lookupResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
							
							// do the usual
							error = RepackBufferForPWServer( authDataBuffTemp, aaData, 1, &authDataBuff );
							if ( error != eDSNoErr ) throw( error );
							DSFreeString(aaData);
						}
						else
						{
							error = RepackBufferForPWServer( authDataBuffTemp, uidStr, 1, &authDataBuff );
							if ( error != eDSNoErr ) throw( error );
						}
					}
					break;

				case kAuthClearText:
				case kAuthNativeClearTextOK:
				case kAuthNativeNoClearText:
				case kAuthNewUser:
				case kAuthNewUserWithPolicy:
					{
						tDataListPtr dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
						if (dataList != NULL)
						{
							userName = dsDataListGetNodeStringPriv(dataList, 1);
							password = dsDataListGetNodeStringPriv(dataList, 2);
							// this allocates a copy of the string
							
							dsDataListDeallocatePriv(dataList);
							free(dataList);
							dataList = NULL;
						}
					}
					//fall through
				
				default:
					error = RepackBufferForPWServer( inAuthData, uidStr, 1, &authDataBuff );
					if ( error != eDSNoErr ) throw( error );
			}
            
			if ( inContext->fPWSRef == 0 )
            {
                error = ::dsOpenDirService( &inContext->fPWSRef );
                if ( error != eDSNoErr ) throw( error );
            }
            
            // JT we should only use the saved reference if the operation
            // requires prior authentication
            // otherwise we should open a new ref each time
            if ( inContext->fPWSNodeRef == 0 )
            {
                nodeName = (char *)calloc(1,strlen(serverAddr)+17);
                if ( nodeName == nil ) throw ( (sInt32)eMemoryError );
                
                sprintf( nodeName, "/PasswordServer/%s", serverAddr );
                error = PWOpenDirNode( inContext->fPWSRef, nodeName, &inContext->fPWSNodeRef );
                if ( error != eDSNoErr ) throw( (sInt32)eDSAuthServerError );
                
            }
            
            if ( pContinue )
                continueData = pContinue->fPassPlugContinueData;
            
            result = dsDoDirNodeAuth( inContext->fPWSNodeRef, inAuthMethod, inAuthOnly,
                                        authDataBuff, outAuthData, &continueData );
            
			//DBGLOG1( kLogPlugin, "CLDAPv3Plugin: result = %l", result );
			
			if ( result == eDSAuthNoAuthServerFound || result == eDSAuthServerError )
			{
				result = PWSetReplicaData( inContext, uidStr, inLDAPSessionMgr );
				if ( result == eDSNoErr )
					result = dsDoDirNodeAuth( inContext->fPWSNodeRef, inAuthMethod, inAuthOnly,
												authDataBuff, outAuthData, &continueData );
			}
			
            if ( pContinue )
                pContinue->fPassPlugContinueData = continueData;
			
			if ( authMethod == kAuthNewUser || authMethod == kAuthNewUserWithPolicy )
			{
				if ( outAuthData->fBufferLength > sizeof(unsigned long) )
				{
					if ( inContext->fPWSUserID != NULL ) {
						free( inContext->fPWSUserID );
						inContext->fPWSUserID = NULL;
					}
					inContext->fPWSUserIDLength = *((unsigned long *)outAuthData->fBufferData);
					inContext->fPWSUserID = (char *) malloc( inContext->fPWSUserIDLength + 1 );
					if ( inContext->fPWSUserID == NULL ) throw( (sInt32)eMemoryError );
					strlcpy( inContext->fPWSUserID, outAuthData->fBufferData + sizeof(unsigned long), inContext->fPWSUserIDLength + 1 );
				}
			}
			
            if ( (result == eDSNoErr) && (inAuthOnly == false) && (userName != NULL) && (password != NULL) )
			{
				accountId = GetDNForRecordName ( userName, inContext, inLDAPSessionMgr, inRecordType );
				
				if( accountId ) // just to be safe... we could fail in the middle of looking for DN
				{
					// Here is the bind to the LDAP server, no need to lock session, AuthOpen will handle
					result = inLDAPSessionMgr.AuthOpen(	accountId,
														inKerberosId,
														password,
														strlen(password),
														kDSStdAuthClearText,
														&(inContext->fLDAPNodeStruct) );

					//the LDAP master wasn't reachable..
					if( result == eDSCannotAccessSession )
					{
						result = eDSAuthMasterUnreachable;
					}
				}
            }
        }
    }
    
    catch(sInt32 err )
    {
        result = err;
    }
    catch( ... )
	{
		result = eDSAuthFailed;
	}
	
	if ( nodeName )
        free( nodeName );

	if ( uidStr != NULL )
	{
		free( uidStr );
		uidStr = NULL;
	}
    if ( userName != NULL )
	{
		free( userName );
		userName = NULL;
	}
    if ( password != NULL )
	{
		bzero(password, strlen(password));
		free( password );
		password = NULL;
	}
    if ( accountId != NULL )
	{
		free( accountId );
		accountId = NULL;
	}
  
    if ( authDataBuff )
        dsDataBufferDeallocatePriv( authDataBuff );
    if ( authDataBuffTemp )
        dsDataBufferDeallocatePriv( authDataBuffTemp );
                        
	return( result );
}

    
//------------------------------------------------------------------------------------
//	* DoBasicAuth
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoBasicAuth(
    tDirNodeReference inNodeRef,
    tDataNodePtr inAuthMethod,
    sLDAPContextData* inContext,
    sLDAPContinueData** inOutContinueData,
    tDataBufferPtr inAuthData,
    tDataBufferPtr outAuthData,
    bool inAuthOnly,
    char* inAuthAuthorityData,
	char* inKerberosId,
    CLDAPNode& inLDAPSessionMgr,
	const char *inRecordType )
{
	sInt32					siResult		= noErr;
	UInt32					uiAuthMethod	= 0;
    
	try
	{
		DBGLOG( kLogPlugin, "CLDAPv3Plugin: DoBasicAuth::" );
		if (inAuthMethod != nil)
		{
			DBGLOG1( kLogPlugin, "CLDAPv3Plugin: DoBasicAuth - Attempting use of authentication method %s", inAuthMethod->fBufferData );
		}
		siResult = dsGetAuthMethodEnumValue( inAuthMethod, &uiAuthMethod );
		if ( siResult == noErr )
		{
			switch( uiAuthMethod )
			{
				case kAuthSetPolicy:
					siResult = eDSAuthMethodNotSupported;
					break;
				
				case kAuthCrypt:
				case kAuthNativeNoClearText:
					siResult = DoUnixCryptAuth( inContext, inAuthData, inLDAPSessionMgr, inRecordType );
					if( siResult == eDSAuthFailedClearTextOnly )
					{
						sLDAPConfigData *pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inContext->fNodeName );
						// eDSAuthFailedRequiring Clear Text only, so let's do a bind check if we have SSL enabled
						if( pConfig != nil )
						{
							if( pConfig->bIsSSL )
							{
								siResult = DoClearTextAuth( inContext, inAuthData, inKerberosId, inAuthOnly, inLDAPSessionMgr, inRecordType );
							}
							gpConfigFromXML->ConfigUnlock( pConfig );
						}
					}
					break;

				case kAuthNativeClearTextOK:
					if ( inAuthOnly == true )
					{
						// auth only
						siResult = DoUnixCryptAuth( inContext, inAuthData, inLDAPSessionMgr, inRecordType );
						if ( siResult == eDSNoErr )
						{
							if ( outAuthData->fBufferSize > ::strlen( kDSStdAuthCrypt ) )
							{
								::strcpy( outAuthData->fBufferData, kDSStdAuthCrypt );
							}
						}
					}
					if ( (siResult != eDSNoErr) || (inAuthOnly == false) )
					{
						siResult = DoClearTextAuth( inContext, inAuthData, inKerberosId, inAuthOnly, inLDAPSessionMgr, inRecordType );
						if ( siResult == eDSNoErr )
						{
							if ( outAuthData->fBufferSize > ::strlen( kDSStdAuthClearText ) )
							{
								::strcpy( outAuthData->fBufferData, kDSStdAuthClearText );
							}
						}
					}
					break;

				case kAuthClearText:
					siResult = DoClearTextAuth( inContext, inAuthData, inKerberosId, inAuthOnly, inLDAPSessionMgr, inRecordType );
					if ( siResult == eDSNoErr )
					{
						if ( outAuthData->fBufferSize > ::strlen( kDSStdAuthClearText ) )
						{
							::strcpy( outAuthData->fBufferData, kDSStdAuthClearText );
						}
					}
					break;
                
                case kAuthSetPasswd:
					siResult = DoSetPassword( inContext, inAuthData, inKerberosId, inLDAPSessionMgr, inRecordType );
					break;

				case kAuthSetPasswdAsRoot:
					siResult = DoSetPasswordAsRoot( inContext, inAuthData, inKerberosId, inLDAPSessionMgr, inRecordType );
					break;

				case kAuthChangePasswd:
					siResult = DoChangePassword( inContext, inAuthData, inKerberosId, inLDAPSessionMgr, inRecordType );
					break;
                
				default:
					siResult = eDSAuthMethodNotSupported;
					break;
			}
		}
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // DoBasicAuth


//------------------------------------------------------------------------------------
//	* DoSetPassword
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoSetPassword ( sLDAPContextData *inContext, tDataBuffer *inAuthData, char *inKerberosId,
                                      CLDAPNode& inLDAPSessionMgr, const char *inRecordType )
{
	sInt32				siResult		= eDSAuthFailed;
	char			   *pData			= nil;
	char			   *userName		= nil;
	uInt32				userNameLen		= 0;
	char			   *userPwd			= nil;
	uInt32				userPwdLen		= 0;
	char			   *rootName		= nil;
	uInt32				rootNameLen		= 0;
	char			   *rootPwd			= nil;
	uInt32				rootPwdLen		= 0;
	uInt32				offset			= 0;
	uInt32				buffSize		= 0;
	uInt32				buffLen			= 0;
	char			   *accountId		= nil;

	try
	{
		if ( inContext == nil ) throw( (sInt32)eDSAuthFailed );
		if ( inAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );

		pData = inAuthData->fBufferData;
		buffSize = inAuthData->fBufferSize;
		buffLen = inAuthData->fBufferLength;

		if ( buffLen > buffSize ) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the length of the first data block and verify that it
		//	is greater than 0 and doesn't go past the end of the buffer
		//	(user name)

		if ( buffLen < 4 * sizeof( unsigned long ) + 2 ) throw( (sInt32)eDSInvalidBuffFormat );
		// need length for username, password, agent username, and agent password.
		// both usernames must be at least one character
		::memcpy( &userNameLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (userNameLen == 0) throw( (sInt32)eDSInvalidBuffFormat );
		if (offset + userNameLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		userName = (char *)::calloc( 1, userNameLen + 1 );
		::memcpy( userName, pData, userNameLen );
		pData += userNameLen;
		offset += userNameLen;
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the user's new password
		::memcpy( &userPwdLen, pData, sizeof( unsigned long ) );
  		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (offset + userPwdLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );
		
		//do not allow a password of zero length since LDAP poorly deals with this
		if (userPwdLen < 1) throw( (sInt32)eDSAuthPasswordTooShort);

		userPwd = (char *)::calloc( 1, userPwdLen + 1 );
		::memcpy( userPwd, pData, userPwdLen );
		pData += userPwdLen;
		offset += userPwdLen;
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the agent user's name
		::memcpy( &rootNameLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (rootNameLen == 0) throw( (sInt32)eDSInvalidBuffFormat );
		if (rootNameLen > (buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );

		rootName = (char *)::calloc( 1, rootNameLen + 1 );
		::memcpy( rootName, pData, rootNameLen );
		pData += rootNameLen;
		offset += rootNameLen;
		if (sizeof( unsigned long ) > (buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the agent user's password
		::memcpy( &rootPwdLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (rootPwdLen > (buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );

		rootPwd = (char *)::calloc( 1, rootPwdLen + 1 );
		::memcpy( rootPwd, pData, rootPwdLen );
		pData += rootPwdLen;
		offset += rootPwdLen;

		if (rootName)
		{
			//here again we choose the first match
			accountId = GetDNForRecordName ( rootName, inContext, inLDAPSessionMgr, inRecordType );
		}

		// we need to save the old NodeStruct so that we can put it back..
		sLDAPNodeStruct *origNodeStruct = inContext->fLDAPNodeStruct;
		
		// increment node count by one so it doesn't get cleaned up while we are doing the auth
		fLDAPSessionMgr.LockSession( inContext );
		origNodeStruct->ChangeRefCountBy( 1 );
		fLDAPSessionMgr.UnLockSession( inContext, false );
		
		// Here is the bind to the LDAP server as the agent
		siResult = inLDAPSessionMgr.AuthOpen(   accountId,
												inKerberosId,
												rootPwd,
												strlen(rootPwd),
												kDSStdAuthClearText,
												&(inContext->fLDAPNodeStruct) );
		if( siResult == eDSNoErr )
		{
			// we need to wait here cause the node is available to everyone else..
			LDAP *aHost = inLDAPSessionMgr.LockSession(inContext);

			int rc = exop_password_create( aHost, accountId, userPwd );
			if( rc != LDAP_SUCCESS ) {
				rc = standard_password_create( aHost, accountId, userPwd );
			}

			// let's update the node struct otherwise we'll have issues on reconnect
			if( rc == LDAP_SUCCESS && strcmp(accountId, (char *)inContext->fLDAPNodeStruct->fLDAPUserName) == 0 )
			{
				inContext->fLDAPNodeStruct->updateCredentials( userPwd, strlen(userPwd), kDSStdAuthClearText );
				inLDAPSessionMgr.CredentialChange( inContext->fLDAPNodeStruct, accountId );
				DBGLOG( kLogPlugin, "CLDAPv3Plugin: DoSetPassword - Updated session password" );
			}
			
			// *** gbv not sure what error codes to check for
			if ( ( rc == LDAP_INSUFFICIENT_ACCESS ) || ( rc == LDAP_INVALID_CREDENTIALS ) ) {
				siResult = eDSPermissionError;
			}
			else if ( rc == LDAP_NOT_SUPPORTED ) {
				siResult = eDSAuthMethodNotSupported;
			}
			else if ( rc == LDAP_SUCCESS ) {
				DBGLOG1( kLogPlugin, "CLDAPv3Plugin: DoSetPassword succeeded for %s", accountId );
				siResult = eDSNoErr;
			}
			else
			{
				siResult = eDSAuthFailed;
			}

			inContext->fLDAPNodeStruct->ChangeRefCountBy( -1 ); // was temporary need to unref count it
			inContext->fLDAPNodeStruct = origNodeStruct; // already incremented above and decremented by Authopen
			inLDAPSessionMgr.UnLockSession(inContext);
		}
		else
		{
			inLDAPSessionMgr.LockSession( inContext );
			origNodeStruct->ChangeRefCountBy( -1 ); // need to restore our ref count
			inLDAPSessionMgr.UnLockSession( inContext, false );
			siResult = eDSAuthFailed;
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( userName != nil )
	{
		free( userName );
		userName = nil;
	}

	if ( userPwd != nil )
	{
		bzero(userPwd, strlen(userPwd));
		free( userPwd );
		userPwd = nil;
	}

	if ( rootName != nil )
	{
		free( rootName );
		rootName = nil;
	}

	if ( rootPwd != nil )
	{
		bzero(rootPwd, strlen(rootPwd));
		free( rootPwd );
		rootPwd = nil;
	}

	return( siResult );

} // DoSetPassword


//------------------------------------------------------------------------------------
//	* DoSetPasswordAsRoot
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoSetPasswordAsRoot ( sLDAPContextData *inContext, tDataBuffer *inAuthData, char *inKerberosId,
                                      CLDAPNode& inLDAPSessionMgr, const char *inRecordType )
{
	sInt32				siResult		= eDSAuthFailed;
	char			   *pData			= nil;
	char			   *userName		= nil;
	uInt32				userNameLen		= 0;
	char			   *newPasswd		= nil;
	uInt32				newPwdLen		= 0;
	uInt32				offset			= 0;
	uInt32				buffSize		= 0;
	uInt32				buffLen			= 0;
	char			   *accountId		= nil;
	int					rc				= LDAP_LOCAL_ERROR;

	try
	{
		if ( inContext == nil ) throw( (sInt32)eDSAuthFailed );
		if ( inAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );

		pData = inAuthData->fBufferData;
		buffSize = inAuthData->fBufferSize;
		buffLen = inAuthData->fBufferLength;

   		if ( buffLen > buffSize ) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the length of the first data block and verify that it
		//	is greater than 0 and doesn't go past the end of the buffer
		//	(user name)

		if ( buffLen < 2 * sizeof( unsigned long ) + 1 ) throw( (sInt32)eDSInvalidBuffFormat );
		// need length for both username and password, plus username must be at least one character
		::memcpy( &userNameLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (userNameLen == 0) throw( (sInt32)eDSInvalidBuffFormat );
		if (userNameLen > (buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );

		userName = (char *)::calloc( 1, userNameLen + 1 );
		::memcpy( userName, pData, userNameLen );
		pData += userNameLen;
		offset += userNameLen;
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the users new password
		::memcpy( &newPwdLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (newPwdLen > (buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );

		//do not allow a password of zero length since LDAP poorly deals with this
		if (newPwdLen < 1) throw( (sInt32)eDSAuthPasswordTooShort);

		newPasswd = (char *)::calloc( 1, newPwdLen + 1 );
		::memcpy( newPasswd, pData, newPwdLen );
		pData += newPwdLen;
		offset += newPwdLen;

		if (userName)
		{
			//here again we choose the first match
			accountId = GetDNForRecordName ( userName, inContext, inLDAPSessionMgr, inRecordType );
		}

		LDAP *aHost = inLDAPSessionMgr.LockSession(inContext);
		if (aHost != nil)
		{
			rc = exop_password_create( aHost, accountId, newPasswd );
			if( rc != LDAP_SUCCESS )
					rc = standard_password_create( aHost, accountId, newPasswd );
			
			// let's update the node struct otherwise we'll have issues on reconnect
			if( rc == LDAP_SUCCESS && strcmp( accountId, (char *)inContext->fLDAPNodeStruct->fLDAPUserName) == 0 )
			{
				inContext->fLDAPNodeStruct->updateCredentials( newPasswd, strlen(newPasswd), kDSStdAuthClearText );
				inLDAPSessionMgr.CredentialChange( inContext->fLDAPNodeStruct, accountId );
				DBGLOG( kLogPlugin, "CLDAPv3Plugin: DoSetPasswordAsRoot - Updated session password" );
			}
		}
		else
		{
			rc = LDAP_LOCAL_ERROR;
		}
        inLDAPSessionMgr.UnLockSession(inContext);
    
        // *** gbv not sure what error codes to check for
        if ( ( rc == LDAP_INSUFFICIENT_ACCESS ) || ( rc == LDAP_INVALID_CREDENTIALS ) )
        {
            siResult = eDSPermissionError;
        }
        else if ( rc == LDAP_NOT_SUPPORTED )
        {
            siResult = eDSAuthMethodNotSupported;
        }
        else if ( rc == LDAP_SUCCESS )
        {
			DBGLOG1( kLogPlugin, "CLDAPv3Plugin: DoSetPasswordAsRoot succeeded for %s", accountId );
            siResult = eDSNoErr;
        }
        else
        {
            siResult = eDSAuthFailed;
        }
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( userName != nil )
	{
		free( userName );
		userName = nil;
	}

	if ( newPasswd != nil )
	{
		bzero(newPasswd, strlen(newPasswd));
		free( newPasswd );
		newPasswd = nil;
	}

	return( siResult );

} // DoSetPasswordAsRoot


//------------------------------------------------------------------------------------
//	* DoChangePassword
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoChangePassword ( sLDAPContextData *inContext, tDataBuffer *inAuthData, char *inKerberosId,
                                      CLDAPNode& inLDAPSessionMgr, const char *inRecordType )
{
	sInt32			siResult		= eDSAuthFailed;
	char		   *pData			= nil;
	char		   *name			= nil;
	uInt32			nameLen			= 0;
	char		   *oldPwd			= nil;
	uInt32			OldPwdLen		= 0;
	char		   *newPwd			= nil;
	uInt32			newPwdLen		= 0;
	uInt32			offset			= 0;
	uInt32			buffSize		= 0;
	uInt32			buffLen			= 0;
	char		   *pathStr			= nil;
	char		   *accountId		= nil;

	try
	{
		if ( inContext == nil ) throw( (sInt32)eDSAuthFailed );

		if ( inAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );

		pData = inAuthData->fBufferData;
		buffSize = inAuthData->fBufferSize;
		buffLen = inAuthData->fBufferLength;

		if ( buffLen > buffSize ) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the length of the first data block and verify that it
		//	is greater than 0 and doesn't go past the end of the buffer

		if ( buffLen < 3 * sizeof( unsigned long ) + 1 ) throw( (sInt32)eDSInvalidBuffFormat );
		// we need at least 3 x 4 bytes for lengths of three strings,
		// and username must be at least 1 long
		::memcpy( &nameLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (nameLen == 0) throw( (sInt32)eDSInvalidBuffFormat );
		if (offset + nameLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		name = (char *)::calloc( 1, nameLen + 1 );
		::memcpy( name, pData, nameLen );
		pData += nameLen;
		offset += nameLen;
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		::memcpy( &OldPwdLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (offset + OldPwdLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		oldPwd = (char *)::calloc( 1, OldPwdLen + 1 );
		::memcpy( oldPwd, pData, OldPwdLen );
		pData += OldPwdLen;
		offset += OldPwdLen;
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		::memcpy( &newPwdLen, pData, sizeof( unsigned long ) );
   		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (offset + newPwdLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		//do not allow a password of zero length since LDAP poorly deals with this
		if (newPwdLen < 1) throw( (sInt32)eDSAuthPasswordTooShort);

		newPwd = (char *)::calloc( 1, newPwdLen + 1 );
		::memcpy( newPwd, pData, newPwdLen );
		pData += newPwdLen;
		offset += newPwdLen;

        // Set up a new connection to LDAP for this user

		if (name)
		{
			//here again we choose the first match
			accountId = GetDNForRecordName ( name, inContext, inLDAPSessionMgr, inRecordType );
		}

		//if username did not garner an accountId then fail authentication
		if (accountId == nil)
		{
			throw( (sInt32)eDSAuthFailed );
		}

		// we need to save the old NodeStruct so that we can put it back..
		sLDAPNodeStruct *origNodeStruct = inContext->fLDAPNodeStruct;
		
		// need to increment by one as a safety because AuthOpen will decrement it if successful
		inLDAPSessionMgr.LockSession( inContext );
		origNodeStruct->ChangeRefCountBy( 1 ); 
		inLDAPSessionMgr.UnLockSession( inContext, false );
		
		// Here is the bind to the LDAP server as the user in question
		siResult = inLDAPSessionMgr.AuthOpen(   accountId,
												inKerberosId,
												oldPwd,
												strlen(oldPwd),
												kDSStdAuthClearText,
												&(inContext->fLDAPNodeStruct) );
		
		if( siResult == eDSNoErr ) {
		
			// we need to wait here cause the node is available to everyone else..
			LDAP *aHost = inLDAPSessionMgr.LockSession(inContext);
			
			// password change algorithm: first attempt the extended operation,
			// if that fails try to change the userPassword field directly
			int rc = exop_password_create( aHost, accountId, newPwd );
			if( rc != LDAP_SUCCESS ) {
				rc = standard_password_create( aHost, accountId, newPwd );
				if ( rc != LDAP_SUCCESS ) {
					rc = standard_password_replace( aHost, accountId, oldPwd, newPwd );\
				}
			}
			
			// let's update the node struct password otherwise we'll have issues on reconnect
			if( rc == LDAP_SUCCESS && strcmp( accountId, (char *)inContext->fLDAPNodeStruct->fLDAPUserName) == 0 )
			{
				inContext->fLDAPNodeStruct->updateCredentials( newPwd, strlen(newPwd), kDSStdAuthClearText );
				inLDAPSessionMgr.CredentialChange( inContext->fLDAPNodeStruct, accountId );
				DBGLOG( kLogPlugin, "CLDAPv3Plugin: DoChangePassword - Updated session password" );
			}
			
			// *** gbv not sure what error codes to check for
			if ( ( rc == LDAP_INSUFFICIENT_ACCESS ) || ( rc == LDAP_INVALID_CREDENTIALS ) )
			{
				siResult = eDSPermissionError;
			}
			else if ( rc == LDAP_NOT_SUPPORTED )
			{
				siResult = eDSAuthMethodNotSupported;
			}
			else if ( rc == LDAP_SUCCESS )
			{
				DBGLOG1( kLogPlugin, "CLDAPv3Plugin: DoChangePassword succeeded for %s", accountId );
				siResult = eDSNoErr;
			}
			else
			{
				siResult = eDSAuthFailed;
			}

			inContext->fLDAPNodeStruct->ChangeRefCountBy( -1 ); // was temporary need to unref count it
			inContext->fLDAPNodeStruct = origNodeStruct; // already incremented above and decremented by Authopen
			inLDAPSessionMgr.UnLockSession(inContext);
		}
		else
		{
			fLDAPSessionMgr.LockSession( inContext );
			origNodeStruct->ChangeRefCountBy( -1 ); // need to restore our ref count
			fLDAPSessionMgr.UnLockSession( inContext, false );
			siResult = eDSAuthFailed;
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( pathStr != nil )
	{
		free( pathStr );
		pathStr = nil;
	}

	if ( name != nil )
	{
		free( name );
		name = nil;
	}

	if ( newPwd != nil )
	{
		bzero(newPwd, strlen(newPwd));
		free( newPwd );
		newPwd = nil;
	}

	if ( oldPwd != nil )
	{
		bzero(oldPwd, strlen(oldPwd));
		free( oldPwd );
		oldPwd = nil;
	}

	return( siResult );

} // DoChangePassword


//--------------------------------------------------------------------------------------------------
// * PWSetReplicaData ()
//
//	Note:	inAuthorityData is the UserID + RSA_key, but the IP address should be pre-stripped by
//			the calling function.
//--------------------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::PWSetReplicaData( sLDAPContextData *inContext, const char *inAuthorityData, CLDAPNode& inLDAPSessionMgr )
{
	sInt32				error					= eDSNoErr;
	bool				bFoundWithHash			= false;
	long				replicaListLen			= 0;
	char				*rsaKeyPtr				= NULL;
	tDataBufferPtr		replicaBuffer			= NULL;
    tDataBufferPtr		replyBuffer				= NULL;
	unsigned long		valueCount				= 0;
	char				**valueData				= NULL;
	char				recordName[64];
	char				hashStr[34];
	
	// get /config/passwordserver_HEXHASH
	rsaKeyPtr = strchr( inAuthorityData, ',' );
	if ( rsaKeyPtr != NULL )
	{
		MD5_CTX ctx;
		unsigned char pubKeyHash[MD5_DIGEST_LENGTH];
		
		MD5_Init( &ctx );
		rsaKeyPtr++;
		MD5_Update( &ctx, rsaKeyPtr, strlen(rsaKeyPtr) );
		MD5_Final( pubKeyHash, &ctx );
		
		BinaryToHexConversion( pubKeyHash, MD5_DIGEST_LENGTH, hashStr );
		sprintf( recordName, "passwordserver_%s", hashStr );
		
		error = LookupAttribute( inContext, kDSStdRecordTypeConfig, recordName, kDS1AttrPasswordServerList, inLDAPSessionMgr, &valueCount, &valueData );
		if ( error == eDSNoErr && valueCount > 0 )
			bFoundWithHash = true;
	}
	
	if ( ! bFoundWithHash )
	{
		error = LookupAttribute( inContext, kDSStdRecordTypeConfig, "passwordserver", kDS1AttrPasswordServerList, inLDAPSessionMgr, &valueCount, &valueData );
		if ( error != eDSNoErr ) 
			return error;
	}
	
	try
	{
		if ( valueCount >= 1 )
		{
			replicaListLen = strlen( valueData[0] );
			replicaBuffer = ::dsDataBufferAllocatePriv( replicaListLen + 1 );
			if ( replicaBuffer == nil ) throw( (sInt32)eMemoryError );
			
			replyBuffer = ::dsDataBufferAllocatePriv( 1 );
			if ( replyBuffer == nil ) throw( (sInt32)eMemoryError );
			
			replicaBuffer->fBufferLength = replicaListLen;
			memcpy( replicaBuffer->fBufferData, valueData[0], replicaListLen );
			
			error = dsDoPlugInCustomCall( inContext->fPWSNodeRef, 1, replicaBuffer, replyBuffer );
			
			::dsDataBufferDeallocatePriv( replicaBuffer );
			::dsDataBufferDeallocatePriv( replyBuffer );
		}
	}
	catch( sInt32 catchErr )
	{
		error = catchErr;
	}
	
	// clean up
	if ( valueData != NULL )
	{
		for ( unsigned long index = 0; index < valueCount; index++ )
			if ( valueData[index] != NULL )
				free( valueData[index] );
		
		free( valueData );
	}
	
	return error;
}


//------------------------------------------------------------------------------------
//	* GetAuthAuthority
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetAuthAuthority ( sLDAPContextData *inContext, const char *userName, CLDAPNode& inLDAPSessionMgr, unsigned long *outAuthCount, char **outAuthAuthority[], const char *inRecordType )
{
	return LookupAttribute( inContext, inRecordType, userName, kDSNAttrAuthenticationAuthority, inLDAPSessionMgr, outAuthCount, outAuthAuthority );
}


sInt32 CLDAPv3Plugin::LookupAttribute (	sLDAPContextData *inContext,
										const char *inRecordType,
										const char *inRecordName,
										const char *inAttribute,
										CLDAPNode& inLDAPSessionMgr,
										unsigned long *outCount,
										char **outData[] )
{
	sInt32			siResult			= eDSAuthFailed;
	char		   *valueData			= nil;
	char		   *pLDAPRecType		= nil;
    char		   *queryFilter			= nil;
	LDAPMessage	   *result				= nil;
	LDAPMessage	   *entry				= nil;
	char		   *attr				= nil;
	BerElement	   *ber					= nil;
	int				ldapReturnCode 		= 0;
	int				numRecTypes			= 1;
	bool			bResultFound		= false;
	char		  **attrs				= nil;
    int				searchTO			= 0;
	bool			bOCANDGroup			= false;
	CFArrayRef		OCSearchList		= nil;
	struct berval **berVal				= nil;
	ber_int_t		scope				= LDAP_SCOPE_SUBTREE;
	sInt32			searchResult		= eDSNoErr;
	char		   *pLDAPAttrType		= nil;
	int				ldapMsgId			= 0;
    
	try
	{
		if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( inRecordName == nil ) throw( (sInt32)eDSNullDataBuff );
        if ( outData == nil ) throw( (sInt32)eDSNullParameter );
        *outCount = 0;
        *outData = nil;
        
		attrs = MapAttrToLDAPTypeArray( inRecordType, inAttribute, inContext->fNodeName );
		if ( attrs == nil ) throw( (sInt32)eDSInvalidAttributeType );
		
		if (*attrs == nil) //no values returned so maybe this is a static mapping
		{
			pLDAPAttrType = MapAttrToLDAPType( inRecordType, inAttribute, inContext->fNodeName, 1, false );
			if (pLDAPAttrType != nil)
			{
				*outData = (char **)calloc( 2, sizeof(char *) );
				(*outData)[0] = pLDAPAttrType;
				siResult = eDSNoErr;
			}
		}
		else //search for auth authority in LDAP server
		{
        
			DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Attempting to get %s", inAttribute );
			
			//retrieve the config data
			sLDAPConfigData *pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inContext->fNodeName );
			if (pConfig != nil)
			{
				searchTO = pConfig->fSearchTimeout;
				
				gpConfigFromXML->ConfigUnlock( pConfig );
				pConfig = NULL;
			}
			
			// we will search over all the rectype mappings until we find the first
			// result for the search criteria in the queryfilter
			numRecTypes = 1;
			bOCANDGroup = false;
			if (OCSearchList != nil)
			{
				CFRelease(OCSearchList);
				OCSearchList = nil;
			}
			pLDAPRecType = MapRecToLDAPType( inRecordType, inContext->fNodeName, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
			//only throw this for first time since we need at least one map
			if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
			
			while ( (pLDAPRecType != nil) && (!bResultFound) )
			{
				queryFilter = BuildLDAPQueryFilter(	(char *)kDSNAttrRecordName,
												inRecordName,
												eDSExact,
												inContext->fNodeName,
												false,
												inRecordType,
												pLDAPRecType,
												bOCANDGroup,
												OCSearchList );
				
				if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );
	
				searchResult = DSDoRetrieval (	pLDAPRecType,
												attrs,
												inContext,
												scope,
												inLDAPSessionMgr,
												queryFilter,
												ldapReturnCode,
												searchTO,
												result,
												bResultFound,
												ldapMsgId );
	
				if (queryFilter != nil)
				{
					delete (queryFilter);
					queryFilter = nil;
				}
	
				if (pLDAPRecType != nil)
				{
					delete (pLDAPRecType);
					pLDAPRecType = nil;
				}
				numRecTypes++;
				bOCANDGroup = false;
				if (OCSearchList != nil)
				{
					CFRelease(OCSearchList);
					OCSearchList = nil;
				}
				pLDAPRecType = MapRecToLDAPType( inRecordType, inContext->fNodeName, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
			} // while ( (pLDAPRecType != nil) && (!bResultFound) )
	
			if (	(bResultFound) &&
					( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
			{
				LDAP *aHost = inLDAPSessionMgr.LockSession(inContext);
				if (aHost != nil)
				{
					//get the authAuthority attribute here
					entry = ldap_first_entry( aHost, result );
					if ( entry != nil )
					{
						attr = ldap_first_attribute( aHost, entry, &ber );
						if ( attr != nil )
						{
							int idx;
							int numValues = 0;
							
							berVal = ldap_get_values_len( aHost, entry, attr );
							if ( berVal != nil )
							{
								numValues = ldap_count_values_len( berVal );
								if ( numValues > 0 )
								{
									*outCount = numValues;
									*outData = (char **)calloc( numValues+1, sizeof(char *) );
								}
								
								for ( idx = 0; idx < numValues; idx++ )
								{
									valueData = (char *)malloc( berVal[idx]->bv_len + 1 );
									if ( valueData == nil ) throw ( eMemoryError );
									
									strncpy( valueData, berVal[idx]->bv_val, berVal[idx]->bv_len );
									valueData[berVal[idx]->bv_len] = '\0';
									
									// TODO: return the right string
									DBGLOG1( kLogPlugin, "CLDAPv3Plugin: LookupAttribute value found %s\n", valueData ); 
									
									(*outData)[idx] = valueData;
								}
								siResult = eDSNoErr;
								
								ldap_value_free_len( berVal );
								berVal = nil;
							}
							ldap_memfree( attr );
							attr = nil;
						}
						if ( ber != nil )
						{
							ber_free( ber, 0 );
						}
						//need to be smart and not call abandon unless the search is continuing
						//ldap_abandon( aHost, ldapMsgId ); // we don't care about the other results, just the first
					}		
					DSSearchCleanUp(aHost, ldapMsgId);
				}// if aHost != nil
				inLDAPSessionMgr.UnLockSession(inContext);
			} // if bResultFound and ldapReturnCode okay
			else
			{
				siResult = searchResult;
			}
		} //search for auth authority in LDAP server
	}

	catch ( sInt32 err )
	{
		DBGLOG1( kLogPlugin, "CLDAPv3Plugin: LookupAttribute error %l", err );
		siResult = err;
	}
	
	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	
	if ( attrs != nil )
	{
		for ( int i = 0; attrs[i] != nil; ++i )
		{
			free( attrs[i] );
		}
		free( attrs );
		attrs = nil;
	}

	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}
	
	if ( result != nil )
	{
		ldap_msgfree( result );
		result = nil;
	}
	
	return( siResult );
}


// ---------------------------------------------------------------------------
//	* ParseAuthAuthority
//    retrieve version, tag, and data from authauthority
//    format is version;tag;data
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::ParseAuthAuthority ( const char	 * inAuthAuthority,
                                            char			** outVersion,
                                            char			** outAuthTag,
                                            char			** outAuthData )
{
	char* authAuthority = NULL;
	char* current = NULL;
	char* tempPtr = NULL;
	sInt32 result = eDSAuthFailed;
	if ( inAuthAuthority == NULL || outVersion == NULL 
		 || outAuthTag == NULL || outAuthData == NULL )
	{
		return eDSAuthFailed;
	}
	authAuthority = strdup(inAuthAuthority);
	if (authAuthority == NULL)
	{
		return eDSAuthFailed;
	}
	current = authAuthority;
	do {
		tempPtr = strsep(&current, ";");
		if (tempPtr == NULL) break;
		*outVersion = strdup(tempPtr);
		
		tempPtr = strsep(&current, ";");
		if (tempPtr == NULL) break;
		*outAuthTag = strdup(tempPtr);
		
		// if this is kerberos auth authority, we care about principal, aaData for auths, so skip authData
		if( strcmp(tempPtr, kDSTagAuthAuthorityKerberosv5) == 0 )
		{
			tempPtr = strsep(&current, ";");
		}
		
		tempPtr = strsep(&current, ";");
		if (tempPtr == NULL) break;
		*outAuthData = strdup(tempPtr);
		
		result = eDSNoErr;
	} while (false);
	
	free(authAuthority);
	authAuthority = NULL;
	if (result != eDSNoErr)
	{
		DSFreeString(*outVersion);
		DSFreeString(*outAuthTag);
		DSFreeString(*outAuthData);
	}
	return result;
}


//------------------------------------------------------------------------------------
//	* DoUnixCryptAuth
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoUnixCryptAuth ( sLDAPContextData *inContext, tDataBuffer *inAuthData, CLDAPNode& inLDAPSessionMgr, const char *inRecordType )
{
	sInt32			siResult			= eDSAuthFailed;
	char		   *pData				= nil;
	uInt32			offset				= 0;
	uInt32			buffSize			= 0;
	uInt32			buffLen				= 0;
	char		   *userName			= nil;
	sInt32			nameLen				= 0;
	char		   *pwd					= nil;
	sInt32			pwdLen				= 0;
	char		   *cryptPwd			= nil;
	char		   *pLDAPRecType		= nil;
    char		   *queryFilter			= nil;
	LDAPMessage	   *result				= nil;
	LDAPMessage	   *entry				= nil;
	char		   *attr				= nil;
	char		  **vals				= nil;
	BerElement	   *ber					= nil;
	int				ldapReturnCode 		= 0;
	int				numRecTypes			= 1;
	bool			bResultFound		= false;
	char		  **attrs				= nil;
    int				searchTO			= 0;
	bool			bOCANDGroup			= false;
	CFArrayRef		OCSearchList		= nil;
	ber_int_t		scope				= LDAP_SCOPE_SUBTREE;
	sInt32			searchResult		= eDSNoErr;
	int				ldapMsgId			= 0;
	
	try
	{
		if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( inAuthData == nil ) throw( (sInt32)eDSNullDataBuff );

		pData = inAuthData->fBufferData;
		if ( pData == nil ) throw( (sInt32)eDSNullDataBuff );
		
		buffSize	= inAuthData->fBufferSize;
		buffLen		= inAuthData->fBufferLength;
		if (buffLen > buffSize) throw( (sInt32)eDSInvalidBuffFormat );
	
		if ( offset + (2 * sizeof( unsigned long ) + 1) > buffLen ) throw( (sInt32)eDSInvalidBuffFormat );
		// need username length, password length, and username must be at least 1 character

		// Get the length of the user name
		::memcpy( &nameLen, pData, sizeof( unsigned long ) );
		if (nameLen == 0) throw( (sInt32)eDSInvalidBuffFormat );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );

		userName = (char *)::calloc(1, nameLen + 1);
		if ( userName == nil ) throw( (sInt32)eMemoryError );

		if ( offset + nameLen > buffLen ) throw( (sInt32)eDSInvalidBuffFormat );
		// Copy the user name
		::memcpy( userName, pData, nameLen );
		
		pData += nameLen;
		offset += nameLen;

		if ( offset + sizeof( unsigned long ) > buffLen ) throw( (sInt32)eDSInvalidBuffFormat );
		// Get the length of the user password
		::memcpy( &pwdLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );

		pwd = (char *)::calloc(1, pwdLen + 1);
		if ( pwd == nil ) throw( (sInt32)eMemoryError );

		if ( offset + pwdLen > buffLen ) throw( (sInt32)eDSInvalidBuffFormat );
		// Copy the user password
		::memcpy( pwd, pData, pwdLen );

		attrs = MapAttrToLDAPTypeArray( inRecordType, kDS1AttrPassword, inContext->fNodeName );
		
		//No Mappings, so we need to return an error that only a clear text password would work instead of no mapping/failure
		if( attrs == nil ) throw ( (sInt32)eDSAuthFailedClearTextOnly );

		DBGLOG( kLogPlugin, "CLDAPv3Plugin: Attempting Crypt Authentication" );

		//retrieve the config data
		sLDAPConfigData *pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inContext->fNodeName );
		if (pConfig != nil)
		{
			searchTO	= pConfig->fSearchTimeout;
			gpConfigFromXML->ConfigUnlock( pConfig );
			pConfig = NULL;
		}
		
        // we will search over all the rectype mappings until we find the first
        // result for the search criteria in the queryfilter
        numRecTypes = 1;
		bOCANDGroup = false;
		if (OCSearchList != nil)
		{
			CFRelease(OCSearchList);
			OCSearchList = nil;
		}
		pLDAPRecType = MapRecToLDAPType( inRecordType, inContext->fNodeName, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
		//only throw this for first time since we need at least one map
		if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
		
		while ( (pLDAPRecType != nil) && (!bResultFound) )
		{
			queryFilter = BuildLDAPQueryFilter(	(char *)kDSNAttrRecordName,
												userName,
												eDSExact,
												inContext->fNodeName,
												false,
												inRecordType,
												pLDAPRecType,
												bOCANDGroup,
												OCSearchList );
			if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );

			searchResult = DSDoRetrieval (	pLDAPRecType,
											attrs,
											inContext,
											scope,
											inLDAPSessionMgr,
											queryFilter,
											ldapReturnCode,
											searchTO,
											result,
											bResultFound,
											ldapMsgId );

			if (queryFilter != nil)
			{
				delete (queryFilter);
				queryFilter = nil;
			}

			if (pLDAPRecType != nil)
			{
				delete (pLDAPRecType);
				pLDAPRecType = nil;
			}
			numRecTypes++;
			bOCANDGroup = false;
			if (OCSearchList != nil)
			{
				CFRelease(OCSearchList);
				OCSearchList = nil;
			}
			pLDAPRecType = MapRecToLDAPType( inRecordType, inContext->fNodeName, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
		} // while ( (pLDAPRecType != nil) && (!bResultFound) )

		if (	(bResultFound) &&
				( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
		{
			LDAP *aHost = gLDAPv3Plugin->fLDAPSessionMgr.LockSession(inContext);
			if (aHost != nil)
			{
				//get the passwd attribute here
				//we are only going to look at the first attribute, first value
				entry = ldap_first_entry( aHost, result );
				if ( entry != nil )
				{
					attr = ldap_first_attribute( aHost, entry, &ber );
					if ( attr != nil )
					{
						vals = ldap_get_values( aHost, entry, attr );
						if ( vals != nil )
						{
							if ( vals[0] != nil )
							{
								cryptPwd = vals[0];
							}
							else
							{
								cryptPwd = (char *)""; //don't free this
							}
							if (::strncasecmp(cryptPwd,"{SMD5}",6) == 0)
							{
								cryptPwd = cryptPwd + 6;
								
								if ( *cryptPwd != '\0' )
								{
									unsigned outlen;
									int result;
									unsigned char hashResult[MD5_DIGEST_LENGTH];
									MD5_CTX ctx;
									char targetStr[128];
									
									result = sasl_decode64( cryptPwd, strlen(cryptPwd), targetStr, sizeof(targetStr), &outlen );
									if ( result == SASL_OK )
									{
										MD5_Init( &ctx );
										MD5_Update( &ctx, pwd, strlen( pwd ) );
										MD5_Update( &ctx, targetStr + MD5_DIGEST_LENGTH, outlen - MD5_DIGEST_LENGTH );
										MD5_Final( hashResult, &ctx );
										
										if ( memcmp( hashResult, targetStr, MD5_DIGEST_LENGTH ) == 0 )
										{
											siResult = eDSNoErr;
										}
										bzero(hashResult, MD5_DIGEST_LENGTH);
									}
								}
							}
							else
							{
								//case insensitive compare with "crypt" string
								if (::strncasecmp(cryptPwd,"{crypt}",7) == 0)
								{
									// special case for OpenLDAP's crypt password attribute
									// advance past {crypt} to the actual crypt password we want to compare against
									cryptPwd = cryptPwd + 7;
								}
								//account for the case where cryptPwd == "" such that we will auth if pwdLen is 0
								if (::strcmp(cryptPwd,"") != 0)
								{
									char salt[ 9 ];
									char hashPwd[ 32 ];
									salt[ 0 ] = cryptPwd[0];
									salt[ 1 ] = cryptPwd[1];
									salt[ 2 ] = '\0';
						
									::memset( hashPwd, 0, 32 );
									::strcpy( hashPwd, ::crypt( pwd, salt ) );
						
									siResult = eDSAuthFailed;
									if ( ::strcmp( hashPwd, cryptPwd ) == 0 )
									{
										siResult = eDSNoErr;
									}
									bzero(hashPwd, 32);
								}
								else // cryptPwd is == ""
								{
									if ( ::strcmp(pwd,"") == 0 )
									{
										siResult = eDSNoErr;
									}
								}
							}
							ldap_value_free( vals );
							vals = nil;
						}
						ldap_memfree( attr );
						attr = nil;
					}
					if ( ber != nil )
					{
						ber_free( ber, 0 );
					}
					//need to be smart and not call abandon unless the search is continuing
					//ldap_abandon( aHost, ldapMsgId ); // we don't care about the other results, just the first
				}
				DSSearchCleanUp(aHost, ldapMsgId);
			}//if aHost != nil
			gLDAPv3Plugin->fLDAPSessionMgr.UnLockSession(inContext);
		} // if bResultFound and ldapReturnCode okay

		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}

	catch ( sInt32 err )
	{
		DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Crypt authentication error %l", err );
		siResult = err;
	}
	
	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	
	if ( attrs != nil )
	{
		for ( int i = 0; attrs[i] != nil; ++i )
		{
			free( attrs[i] );
		}
		free( attrs );
		attrs = nil;
	}

	if ( userName != nil )
	{
		free( userName );
		userName = nil;
	}

	if ( pwd != nil )
	{
		bzero(pwd, strlen(pwd));
		free( pwd );
		pwd = nil;
	}

	if ( queryFilter != nil )
	{
		free( queryFilter );
		queryFilter = nil;
	}
	
	if ( cryptPwd == nil )
	{
		// couldn't read a crypt password, only option left is a cleartext authentication
		siResult = eDSAuthFailedClearTextOnly;
	}
	
	return( siResult );

} // DoUnixCryptAuth


//------------------------------------------------------------------------------------
//	* DoClearTextAuth
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoClearTextAuth ( sLDAPContextData *inContext, tDataBuffer *inAuthData, char *inKerberosId, bool authCheckOnly, CLDAPNode& inLDAPSessionMgr, const char *inRecordType )
{
	sInt32				siResult			= eDSAuthFailed;
	char			   *pData				= nil;
	char			   *userName			= nil;
	char			   *accountId			= nil;
	sInt32				nameLen				= 0;
	char			   *pwd					= nil;
	sInt32				pwdLen				= 0;

	try
	{
	//check the authCheckOnly if true only test name and password
	//ie. need to retain current credentials
		if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( inAuthData == nil ) throw( (sInt32)eDSNullDataBuff );

		pData = inAuthData->fBufferData;
		if ( pData == nil ) throw( (sInt32)eDSNullDataBuff );
		
		// Get the length of the user name
		::memcpy( &nameLen, pData, sizeof( long ) );
		//accept the case of a NO given name and NO password
		//LDAP servers use this to reset the credentials
		//if (nameLen == 0) throw( (sInt32)eDSAuthInBuffFormatError );

		if (nameLen > 0)
		{
			userName = (char *) calloc(1, nameLen + 1);
			if ( userName == nil ) throw( (sInt32) eMemoryError );

			// Copy the user name
			pData += sizeof( long );
			::memcpy( userName, pData, nameLen );
			pData += nameLen;
		}

		// Get the length of the user password
		::memcpy( &pwdLen, pData, sizeof( long ) );
		//accept the case of a given name and NO password
		//LDAP servers use this as tracking info when no password is required
		//if (pwdLen == 0) throw( (sInt32)eDSAuthInBuffFormatError );

		if (pwdLen > 0)
		{
			pwd = (char *) calloc(1, pwdLen + 1);
			if ( pwd == nil ) throw( (sInt32) eMemoryError );

			// Copy the user password
			pData += sizeof( long );
			::memcpy( pwd, pData, pwdLen );
			pData += pwdLen;
		}

		DBGLOG( kLogPlugin, "CLDAPv3Plugin: Attempting Cleartext Authentication" );

		//get the correct account id
		//we assume that the DN is always used for this
		if (userName)
		{
			//here again we choose the first match
			accountId = GetDNForRecordName ( userName, inContext, inLDAPSessionMgr, inRecordType );
		}
                
		//if username did not garner an accountId then fail authentication
		if (accountId == nil)
		{
			throw( (sInt32)eDSAuthFailed );
		}
		
		if ((pwd != NULL) && (pwd[0] != '\0') && (nameLen != 0))
		{
			if (!authCheckOnly)
			{
				//no need to lock session cause the AuthOpen will do it
				siResult = inLDAPSessionMgr.AuthOpen(	accountId,
														inKerberosId,
														pwd,
														strlen(pwd),
														kDSStdAuthClearText,
														&(inContext->fLDAPNodeStruct) );
				
				if( siResult == eDSCannotAccessSession )
				{
					siResult = eDSAuthMasterUnreachable;
				}
			}
			else //no session ie. authCheckOnly
			{
				siResult = inLDAPSessionMgr.SimpleAuth(	inContext->fLDAPNodeStruct,
														accountId,
														pwd, strlen(pwd), inKerberosId );
			}
			
		}

	}

	catch ( sInt32 err )
	{
		DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Cleartext authentication error %l", err );
		siResult = err;
	}

	if ( accountId != nil )
	{
		free( accountId );
		accountId = nil;
	}

	if ( userName != nil )
	{
		free( userName );
		userName = nil;
	}

	if ( pwd != nil )
	{
		bzero(pwd, strlen(pwd));
		free( pwd );
		pwd = nil;
	}

	return( siResult );

} // DoClearTextAuth

//------------------------------------------------------------------------------------
//	* GetDNForRecordName
//------------------------------------------------------------------------------------

char* CLDAPv3Plugin::GetDNForRecordName ( char* inRecName, sLDAPContextData *inContext, CLDAPNode& inLDAPSessionMgr, const char *inRecordType )
{
	char			   *ldapDN			= nil;	
	char			   *pLDAPRecType	= nil;
    char			   *queryFilter		= nil;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	int					numRecTypes		= 1;
	bool				bResultFound	= false;
    int					searchTO		= 0;
	bool				bOCANDGroup		= false;
	CFArrayRef			OCSearchList	= nil;
	ber_int_t			scope			= LDAP_SCOPE_SUBTREE;
	sInt32				searchResult	= eDSNoErr;
	int					ldapMsgId		= 0;


	try
	{
		if ( inRecName  == nil ) throw( (sInt32)eDSNullRecName );
		if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );

		//search for the specific LDAP record now
		
		//retrieve the config data
		sLDAPConfigData *pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inContext->fNodeName );
		if (pConfig != nil)
		{
			searchTO	= pConfig->fSearchTimeout;
			gpConfigFromXML->ConfigUnlock( pConfig );
			pConfig = NULL;
		}
		
        // we will search over all the rectype mappings until we find the first
        // result for the search criteria in the queryfilter
        numRecTypes = 1;
		bOCANDGroup = false;
		if (OCSearchList != nil)
		{
			CFRelease(OCSearchList);
			OCSearchList = nil;
		}
		pLDAPRecType = MapRecToLDAPType( inRecordType, inContext->fNodeName, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
		//only throw this for first time since we need at least one map
		if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
		
		while ( (pLDAPRecType != nil) && (!bResultFound) )
		{

			//build the record query string
			queryFilter = BuildLDAPQueryFilter(	(char *)kDSNAttrRecordName,
												inRecName,
												eDSExact,
												inContext->fNodeName,
												false,
												inRecordType,
												pLDAPRecType,
												bOCANDGroup,
												OCSearchList );
			if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );

			searchResult = DSDoRetrieval (	pLDAPRecType,
											NULL,
											inContext,
											scope,
											inLDAPSessionMgr,
											queryFilter,
											ldapReturnCode,
											searchTO,
											result,
											bResultFound,
											ldapMsgId );

			if (queryFilter != nil)
			{
				delete (queryFilter);
				queryFilter = nil;
			}

			if (pLDAPRecType != nil)
			{
				delete (pLDAPRecType);
				pLDAPRecType = nil;
			}
			numRecTypes++;
			bOCANDGroup = false;
			if (OCSearchList != nil)
			{
				CFRelease(OCSearchList);
				OCSearchList = nil;
			}
			pLDAPRecType = MapRecToLDAPType( inRecordType, inContext->fNodeName, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
		} // while ( (pLDAPRecType != nil) && (!bResultFound) )

		if (	(bResultFound) &&
				( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
		{
			LDAP *aHost = gLDAPv3Plugin->fLDAPSessionMgr.LockSession(inContext);
			if (aHost != nil)
			{
				//get the ldapDN here
				ldapDN = ldap_get_dn(aHost, result);
				DSSearchCleanUp(aHost, ldapMsgId);
			}
			gLDAPv3Plugin->fLDAPSessionMgr.UnLockSession(inContext);
		
		} // if bResultFound and ldapReturnCode okay
		
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}

	catch ( sInt32 err )
	{
		ldapDN = nil;
	}

	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	
	if ( pLDAPRecType != nil )
	{
		delete( pLDAPRecType );
		pLDAPRecType = nil;
	}

	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}

	return( ldapDN );

} // GetDNForRecordName

//------------------------------------------------------------------------------------
//      * DoPlugInCustomCall
//------------------------------------------------------------------------------------ 

sInt32 CLDAPv3Plugin::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
	sInt32			siResult	= eDSNoErr;
	unsigned long	aRequest	= 0;
	sLDAPContextData	   *pContext		= nil;
	sInt32				xmlDataLength	= 0;
	CFDataRef   		xmlData			= nil;
	unsigned long		bufLen			= 0;
	AuthorizationRef	authRef			= 0;
	tDataListPtr		dataList		= NULL;
	char*				userName		= NULL;
	char*				password		= NULL;
	char*				xmlString		= NULL;
	AuthorizationExternalForm blankExtForm;
	bool				verifyAuthRef	= true;

//seems that the client needs to have a tDirNodeReference 
//to make the custom call even though it will likely be non-dirnode specific related

	try
	{
		if ( inData == nil ) throw( (sInt32)eDSNullParameter );
		if ( inData->fInRequestData == nil ) throw( (sInt32)eDSNullDataBuff );
		if ( inData->fInRequestData->fBufferData == nil ) throw( (sInt32)eDSEmptyBuffer );

		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );
		
		if ( strcmp(pContext->fNodeName,"LDAPv3 Configure") == 0 )
		{
			aRequest = inData->fInRequestCode;
			bufLen = inData->fInRequestData->fBufferLength;
			if ( aRequest != 55 )
			{
				if ( bufLen < sizeof( AuthorizationExternalForm ) ) throw( (sInt32)eDSInvalidBuffFormat );

				if ( pContext->fEffectiveUID == 0 ) {
					bzero(&blankExtForm,sizeof(AuthorizationExternalForm));
					if (memcmp(inData->fInRequestData->fBufferData,&blankExtForm,
							   sizeof(AuthorizationExternalForm)) == 0) {
						verifyAuthRef = false;
					}
				}
				if (verifyAuthRef) {
					siResult = AuthorizationCreateFromExternalForm((AuthorizationExternalForm *)inData->fInRequestData->fBufferData,
						&authRef);
					if (siResult != errAuthorizationSuccess)
					{
						DBGLOG1( kLogPlugin, "CLDAPv3Plugin: AuthorizationCreateFromExternalForm returned error %d", siResult );
						syslog( LOG_ALERT, "AuthorizationCreateFromExternalForm returned error %d", siResult );
						throw( (sInt32)eDSPermissionError );
					}
		
					AuthorizationItem rights[] = { {"system.services.directory.configure", 0, 0, 0} };
					AuthorizationItemSet rightSet = { sizeof(rights)/ sizeof(*rights), rights };
				
					siResult = AuthorizationCopyRights(authRef, &rightSet, NULL,
						kAuthorizationFlagExtendRights, NULL);

					if (siResult != errAuthorizationSuccess)
					{
						DBGLOG1( kLogPlugin, "CLDAPv3Plugin: AuthorizationCopyRights returned error %d", siResult );
						syslog( LOG_ALERT, "AuthorizationCopyRights returned error %d", siResult );
						throw( (sInt32)eDSPermissionError );
					}
				}
			}
			switch( aRequest )
			{
				case 55:
					// parse input buffer
					dataList = dsAuthBufferGetDataListAllocPriv(inData->fInRequestData);
					if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( dsDataListGetNodeCountPriv(dataList) != 3 ) throw( (sInt32)eDSInvalidBuffFormat );

					// this allocates a copy of the string
					userName = dsDataListGetNodeStringPriv(dataList, 1);
					if ( userName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( strlen(userName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );

					password = dsDataListGetNodeStringPriv(dataList, 2);
					if ( password == nil ) throw( (sInt32)eDSInvalidBuffFormat );

					xmlString = dsDataListGetNodeStringPriv(dataList, 3);
					if ( xmlString == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					xmlData = CFDataCreate(NULL,(UInt8*)xmlString,strlen(xmlString));
					
					siResult = gpConfigFromXML->WriteServerMappings( userName, password, xmlData );
					if (userName != nil)
					{
						free(userName);
						userName = nil;
					}
					if (password != nil)
					{
						free(password);
						password = nil;
					}
					if (xmlString != nil)
					{
						free(xmlString);
						xmlString = nil;
					}
					break;
					/*
					//ReadServerMappings will accept the partial XML config data that comes out of the local config file so that
					//it can return the proper XML config data that defines all the mappings
					CFDataRef gpConfigFromXML->ReadServerMappings ( LDAP *serverHost = pContext->fHost, CFDataRef inMappings = xmlData );
					*/
					 
				case 66:
					// get length of XML file
						
					if ( inData->fOutRequestResponse == nil ) throw( (sInt32)eDSNullDataBuff );
					if ( inData->fOutRequestResponse->fBufferData == nil ) throw( (sInt32)eDSEmptyBuffer );
					if ( inData->fOutRequestResponse->fBufferSize < sizeof( CFIndex ) ) throw( (sInt32)eDSInvalidBuffFormat );
					if (gpConfigFromXML)
					{
						// need four bytes for size
						xmlData = gpConfigFromXML->CopyXMLConfig();
						if (xmlData != 0)
						{
							*(CFIndex*)(inData->fOutRequestResponse->fBufferData) = CFDataGetLength(xmlData);
							inData->fOutRequestResponse->fBufferLength = sizeof( CFIndex );
							CFRelease( xmlData );
							xmlData = 0;
						}
					}
					break;
					
				case 77:
					// read xml config
					CFRange	aRange;
						
					if ( inData->fOutRequestResponse == nil ) throw( (sInt32)eDSNullDataBuff );
					if ( inData->fOutRequestResponse->fBufferData == nil ) throw( (sInt32)eDSEmptyBuffer );
					if (gpConfigFromXML)
					{
						xmlData = gpConfigFromXML->CopyXMLConfig();
						if (xmlData != 0)
						{
							aRange.location = 0;
							aRange.length = CFDataGetLength(xmlData);
							if ( inData->fOutRequestResponse->fBufferSize < (unsigned int)aRange.length ) throw( (sInt32)eDSBufferTooSmall );
							CFDataGetBytes( xmlData, aRange, (UInt8*)(inData->fOutRequestResponse->fBufferData) );
							inData->fOutRequestResponse->fBufferLength = aRange.length;
							CFRelease(xmlData);
							xmlData = 0;
						}
					}
					break;
					
				case 88:
				{
					CFPropertyListRef		configPropertyList = NULL;
					CFStringRef				errorString = NULL;
					
					//here we accept an XML blob to replace the current config file
					//need to make xmlData large enough to receive the data
					xmlDataLength = (sInt32) bufLen - sizeof( AuthorizationExternalForm );
					if ( xmlDataLength <= 0 ) throw( (sInt32)eDSInvalidBuffFormat );
					xmlData = CFDataCreate(NULL,(UInt8 *)(inData->fInRequestData->fBufferData + sizeof( AuthorizationExternalForm )),
						xmlDataLength);
					
					if (xmlData != nil)
					{
						// extract the config dictionary from the XML data.
						configPropertyList = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
												xmlData,
												kCFPropertyListMutableContainers, 	// in case we have to strip out DHCP data
											&errorString);
						if (configPropertyList != nil )
						{
							//make the propertylist a dict
							if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) && CFDictionaryContainsValue( (CFDictionaryRef)configPropertyList, CFSTR(kXMLDHCPConfigArrayKey) ) )
							{
								// check to see if this has the DHCP config data.  If so strip it.
								CFDictionaryRemoveValue( (CFMutableDictionaryRef)configPropertyList, CFSTR(kXMLDHCPConfigArrayKey) );
								CFRelease( xmlData );
								xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configPropertyList);
							}
							CFRelease(configPropertyList);
							configPropertyList = NULL;
						}
					}
					
					if (gpConfigFromXML)
					{
						// refresh registered nodes
						siResult = gpConfigFromXML->SetXMLConfig(xmlData);
						Initialize();
					}
					CFRelease(xmlData);
					break;
				}
					
				case 99:
					Initialize();
					break;

				case 111:
					//here we accept an XML blob to add a Server Mappings LDAP node to the search policy
					//need to make xmlData large enough to accomodate the data
					xmlDataLength = (sInt32) bufLen - sizeof(AuthorizationExternalForm);
					if ( xmlDataLength <= 0 ) throw( (sInt32)eDSInvalidBuffFormat );
					xmlData = CFDataCreate(NULL, (UInt8 *)(inData->fInRequestData->fBufferData + sizeof(AuthorizationExternalForm)), xmlDataLength);
					if (gpConfigFromXML)
					{
						// add to registered nodes as a "forced" DHCP type node
						siResult = gpConfigFromXML->AddToConfig(xmlData);
						if (siResult == eDSNoErr)
						{
							Initialize();
						}
					}
					CFRelease(xmlData);
					break;
					
				case 200:
				case 201:
					// Verify server can be contacted, get basic information from RootDSE
					// this includes determining the name context and look for server mappings
					// Will return:
					//		eDSNoErr					= Everything okay, continue to next step
					//		eDSBogusServer				= Could not contact a server at that address
					//		eDSSchemaError				= Could not detect Schema Mappings from config
					//		eDSRecordAlreadyExists		= The server is already in the configuration
					siResult = DoNewServerDiscovery( inData );
					break;
					
				case 202:
					// Verify configuration from server or user works (i.e., mappings) or requires authentication
					// Will return:
					//		eDSNoErr					= Everything okay, continue to next step
					//		eDSBogusServer				= Could not contact a server at that address
					//		eDSInvalidNativeMapping		= Could not use mappings supplied to query directory
					//		eDSAuthParameterError		= Requires Authentication, can't query Directory
					siResult = DoNewServerVerifySettings( inData );
					break;
					
				case 203:
					// Determine if server configuration (i.e., Directory Binding)
					// Will return:
					//		eDSNoErr					= Everything okay, continue next step
					//		eDSBogusServer				= Could not contact a server at that address
					//		eDSInvalidNativeMapping		= Could not use mappings supplied to query directory
					//		eDSAuthParameterError		= Requires Authentication, can't query Directory
					siResult = DoNewServerGetConfig( inData );
					break;
					
				case 204:		// Do not join, check fail with eDSRecordAlreadyExists if already there
				case 205:		// Join existing account
					// Bind to server
					// Will return:
					//		eDSNoErr					= Everything okay, configuration complete
					//		eDSBogusServer				= Could not contact a server at that address
					//		eDSInvalidNativeMapping		= Could not use mappings supplied to query directory
					//		eDSNoStdMappingAvailable	= No Computer Mapping available for this process
					//		eDSInvalidRecordName		= Missing a valid name for the Computer
					//		eDSAuthMethodNotSupported   = Method not supported, see key "Supported Security Level"
					//		eDSRecordAlreadyExists		= Computer already exists, send 205 to override
					siResult = DoNewServerBind( inData );
					break;
					
				case 206:
					// Setup and non-binded server
					// Will return:
					//		eDSNoErr					= Everything okay, configuration complete
					//		eDSBogusServer				= Could not contact a server at that address
					//		eDSInvalidNativeMapping		= Could not use mappings supplied to query directory
					siResult = DoNewServerSetup( inData );
					break;
					
				case 207:   // normal unbind
				case 208:   // force unbind, leaving computer account if it exists
				case 209:   // remove server from configuration
					// Unbind request
					// Will return:
					//		eDSNoErr					= Everything okay, removal complete
					//		eDSBogusServer				= No such server configured
					//		eDSAuthParameterError		= Must be bound, needs authentication to unbind
					siResult = DoRemoveServer( inData );
					break;
				
				case 210:   // do a bind, but not for ourselves
				case 211:   // do a force bind, but not for ourselves
					// Bind to server
					// Will return:
					//		eDSNoErr					= Everything okay, configuration complete
					//		eDSBogusServer				= Could not contact a server at that address
					//		eDSInvalidNativeMapping		= Could not use mappings supplied to query directory
					//		eDSNoStdMappingAvailable	= No Computer Mapping available for this process
					//		eDSInvalidRecordName		= Missing a valid name for the Computer
					//		eDSAuthMethodNotSupported   = Method not supported, see key "Supported Security Level"
					//		eDSRecordAlreadyExists		= Computer already exists, send 205 to override
					siResult = DoNewServerBind2( inData );
					break;
					
				default:
					break;
			}
		}
		else if( inData->fInRequestCode == eRecordDeleteAndCredentials )
		{
			// this is a special delete function that will use existing credentials to delete a record
			// and it's PWS information cleanly
			sDeleteRecord	tempStruct;
			
			tempStruct.fInRecRef = *((tRecordReference *)inData->fInRequestData->fBufferData);
			siResult = DeleteRecord( &tempStruct, true );
		}
#define LDAPV3_PLUGIN_RECORD_BLAT		1
#if LDAPV3_PLUGIN_RECORD_BLAT
		else if( inData->fInRequestCode == eRecordBlatAvailable )	// lets clients see  if record blat operations are available
		{
			if( inData->fOutRequestResponse->fBufferSize < 1 )
				throw( (sInt32)eDSInvalidBuffFormat );
			inData->fOutRequestResponse->fBufferLength = 1;
			inData->fOutRequestResponse->fBufferData[0] = 1;
		}
		else if( inData->fInRequestCode == eRecordBlatCreateRecWithAttrs )	// create and set the attribute values in one op
		{
			dsBool recordTypeAllocated = false;
			dsBool recordNameAllocated = false;
			const char* recordType = NULL;
			const char* recordName = NULL;
			CFDictionaryRef recordDict = 0;
			{	// get the record ref and desired record dict out of the buffer
				CFDataRef bufferCFData = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault, (const UInt8*)inData->fInRequestData->fBufferData,
					inData->fInRequestData->fBufferLength, kCFAllocatorNull );
				if( bufferCFData == NULL )
					throw( (sInt32)eDSInvalidBuffFormat );
				CFStringRef errorString;
				CFDictionaryRef bufferCFDict = (CFDictionaryRef)CFPropertyListCreateFromXMLData( kCFAllocatorDefault, bufferCFData,
					kCFPropertyListImmutable, &errorString );
				CFRelease( bufferCFData );	//don't need the bufferCFData anymore
				bufferCFData = 0;

				CFStringRef recordTypeCFString = (CFStringRef)CFDictionaryGetValue( bufferCFDict, CFSTR( "Record Type" ) );
				if( recordTypeCFString != 0 )
					recordType = CFStringGetCStringPtr( recordTypeCFString, kCFStringEncodingUTF8 );
				if( recordType == NULL )
				{
					recordTypeAllocated = true;
					CFIndex bufferSize = CFStringGetLength( recordTypeCFString ) * 2;
					char* recordTypeCStr = (char*)calloc( bufferSize, sizeof( char ) );
					if( !CFStringGetCString( recordTypeCFString, recordTypeCStr, bufferSize, kCFStringEncodingUTF8 ) )
						throw( (sInt32)eDSInvalidBuffFormat );
					recordType = recordTypeCStr;
				}
				recordDict = (CFDictionaryRef)CFDictionaryGetValue( bufferCFDict, CFSTR( "Attributes and Values" ) );
				if( recordDict == NULL )
					throw( (sInt32)eDSInvalidBuffFormat );

				CFArrayRef recordNames = (CFArrayRef)CFDictionaryGetValue( recordDict, CFSTR( kDSNAttrRecordName ) );
				if( recordNames == 0 )
					throw( (sInt32)eDSInvalidBuffFormat );
				
				CFStringRef recordNameCFString = (CFStringRef)CFArrayGetValueAtIndex( recordNames, 0 );
				if( recordNameCFString == 0 )
					throw( (sInt32)eDSInvalidBuffFormat );

				if( recordNameCFString != 0 )
					recordName = CFStringGetCStringPtr( recordNameCFString, kCFStringEncodingUTF8 );
				if( recordName == NULL )
				{
					recordNameAllocated = true;
					CFIndex bufferSize = CFStringGetLength( recordNameCFString ) * 2;
					char* recordNameCStr = (char*)calloc( bufferSize, sizeof( char ) );
					if( !CFStringGetCString( recordNameCFString, recordNameCStr, bufferSize, kCFStringEncodingUTF8 ) )
						throw( (sInt32)eDSInvalidBuffFormat );
					recordName = recordNameCStr;
				}
				
				CFRetain( recordDict );

				CFRelease( bufferCFDict );	//don't need the bufferCFDict anymore
				bufferCFDict = NULL;
			}
			if( ( recordType != NULL ) &&( recordName != NULL ) && ( recordDict != 0 ) )
			{
				siResult = CreateRecordWithAttributes( inData->fInNodeRef, recordType, recordName, recordDict );
				if( siResult != eDSNoErr )
					throw( (sInt32)siResult );
				
			}
			if( recordTypeAllocated )
				free( (char*)recordType );
			
			if( recordNameAllocated )
				free( (char*)recordName );
		}
		else if( inData->fInRequestCode == eRecordBlatSetAttrs )	// set all of the records attr values
		{
			tRecordReference recordRef = 0;
			CFDictionaryRef recordDict = 0;
			{	// get the record ref and desired record dict out of the buffer
				CFDataRef bufferCFData = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault, (const UInt8*)inData->fInRequestData->fBufferData,
					inData->fInRequestData->fBufferLength, kCFAllocatorNull );
				if( bufferCFData == NULL )
					throw( (sInt32)eDSInvalidBuffFormat );
				CFStringRef errorString;
				CFDictionaryRef bufferCFDict = (CFDictionaryRef)CFPropertyListCreateFromXMLData( kCFAllocatorDefault, bufferCFData,
					kCFPropertyListImmutable, &errorString );
				CFRelease( bufferCFData );	//don't need the bufferCFData anymore
				bufferCFData = 0;

				CFNumberRef recordRefNumber = (CFNumberRef)CFDictionaryGetValue( bufferCFDict, CFSTR( "Record Reference" ) );
				if( !CFNumberGetValue( recordRefNumber, kCFNumberLongType, &recordRef ) )
					throw( (sInt32)eDSInvalidBuffFormat );
				
				recordDict = (CFDictionaryRef)CFDictionaryGetValue( bufferCFDict, CFSTR( "Attributes and Values" ) );
				if( recordDict == NULL )
					throw( (sInt32)eDSInvalidBuffFormat );

				
				CFRetain( recordDict );

				CFRelease( bufferCFDict );	//don't need the bufferCFDict anymore
				bufferCFDict = NULL;
			}
			if( ( recordRef != 0 ) && ( recordDict != 0 ) )
			{
				siResult = SetAttributes( recordRef, recordDict );
				if( siResult != eDSNoErr )
					throw( (sInt32)siResult );
			}
			
			CFRelease( recordDict );	//done with the recordDict
		}
#endif	// LDAPV3_PLUGIN_RECORD_BLAT
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if (authRef != 0)
	{
		AuthorizationFree(authRef, 0);
		authRef = 0;
	}

	return( siResult );

} // DoPlugInCustomCall

#pragma mark -
#pragma mark New Server Setup Utility Functions

// ---------------------------------------------------------------------------
//	* DoSimpleLDAPBind
// ---------------------------------------------------------------------------

LDAP *CLDAPv3Plugin::DoSimpleLDAPBind( char *pServer, bool bSSL, bool bLDAPv2ReadOnly, char *pUsername, char *pPassword, bool bNoCleartext, sInt32 *outFailureCode )
{
	int			iVersion		= LDAP_VERSION3;
	timeval		stTimeout		= { kLDAPDefaultOpenCloseTimeoutInSeconds, 0 };  // let's wait a whole kLDAPDefaultNetworkTimeoutInSeconds seconds
	sInt32		siResult		= eDSAuthServerError;
	LDAP		*pLD			= NULL;
	int			ldapReturnCode  = 0;
	
	pLD = ldap_init( pServer, (bSSL ? LDAPS_PORT : LDAP_PORT) );

	if (!bLDAPv2ReadOnly)
	{
		ldap_set_option( pLD, LDAP_OPT_PROTOCOL_VERSION, &iVersion );
	}
	ldap_set_option( pLD, LDAP_OPT_REFERRALS, LDAP_OPT_OFF );   // we don't want to follow referrals..
	SetNetworkTimeoutsForHost( pLD, kLDAPDefaultNetworkTimeoutInSeconds );

	if( bSSL && !bLDAPv2ReadOnly )
	{
		int ldapOptVal = LDAP_OPT_X_TLS_HARD;
		ldap_set_option( pLD, LDAP_OPT_X_TLS, &ldapOptVal );
	}
	
	// we always try a SASL bind first if possible.. when credentials are present, unless we are SSL
	if( !bSSL && pUsername != NULL && strlen(pUsername) && pPassword != NULL && strlen(pPassword) && !bLDAPv2ReadOnly )
	{
		sLDAPConfigData stConfig;
		
		stConfig.fSASLmethods = (CFMutableArrayRef) GetSASLMethods( pLD );
		
		// If we have SASLMethods
		if( stConfig.fSASLmethods != NULL ) 
		{
			ldapReturnCode = doSASLBindAttemptIfPossible( pLD, &stConfig, pUsername, pPassword );
			
			if( ldapReturnCode == LDAP_SUCCESS )
			{
				siResult = eDSNoErr;
			}
			else if( ldapReturnCode == LDAP_TIMEOUT )
			{
				siResult = eDSServerTimeout;
			}
			else if( ldapReturnCode == LDAP_LOCAL_ERROR || ldapReturnCode == LDAP_LOCAL_ERROR )
			{   // SASL couldn't negotiate for some reason
				siResult = eDSAuthServerError;
			}
			else
			{
				siResult = eDSAuthFailed;
			}
		}

		if( stConfig.fSASLmethods != NULL )
		{
			CFRelease( stConfig.fSASLmethods );
			stConfig.fSASLmethods = NULL;
		}
	}
	
	// if (Cleartext is OK or SSL is in place or no username and password) and we have an authServerError,
	// meaning either nothing has been done, or SASL failed
	if( (!bNoCleartext || bSSL || pUsername == NULL || pPassword == NULL) && siResult == eDSAuthServerError )
	{
		LDAPMessage *pLdapResult	= NULL;
		int siBindMsgID = ldap_simple_bind( pLD, pUsername, pPassword );
		ldapReturnCode = ldap_result( pLD, siBindMsgID, 0, &stTimeout, &pLdapResult );

		if( ldapReturnCode == 0 || ldapReturnCode == -1  )
		{
			siResult = eDSServerTimeout;
		}
		else if( pLdapResult )
		{
			if( ldap_parse_result( pLD, pLdapResult, &ldapReturnCode, NULL, NULL, NULL, NULL, 0 ) == LDAP_SUCCESS )
			{
				if( ldapReturnCode == LDAP_SUCCESS )
				{
					siResult = eDSNoErr;
				}
				else if( ldapReturnCode == LDAP_TIMEOUT )
				{
					siResult = eDSServerTimeout;
				}
				else if( ldapReturnCode == LDAP_PROTOCOL_ERROR )
				{
					siResult = eDSReadOnly;
				}
			}
			else
			{
				siResult = eDSAuthFailed;
			}
		}
		
		if( pLdapResult )
		{
			ldap_msgfree( pLdapResult );
			pLdapResult = NULL;
		}
	}
	
	if( siResult != eDSNoErr )
	{
		ldap_unbind_ext_s( pLD, NULL, NULL );
		pLD = NULL;
	}
	
	if( outFailureCode )
	{
		*outFailureCode = siResult;
	}
		
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
	
	if( ldap_search_ext_s( pLD, LDAP_ROOT_DSE, LDAP_SCOPE_BASE, "(objectclass=*)", pAttributes, false, NULL, NULL, &stTimeout, 0, &pLDAPResult ) == LDAP_SUCCESS )
	{
		cfSASLMechs = GetLDAPAttributeFromResult( pLD, pLDAPResult, "supportedSASLMechanisms" );
	}
	
	if( pLDAPResult )
	{
		ldap_msgfree( pLDAPResult );
		pLDAPResult = NULL;
	}
	
	return cfSASLMechs;
}

// ---------------------------------------------------------------------------
//	* GetLDAPAttributeFromResult
// ---------------------------------------------------------------------------

CFMutableArrayRef CLDAPv3Plugin::GetLDAPAttributeFromResult( LDAP *pLD, LDAPMessage *pMessage, char *pAttribute )
{
	berval				**pBValues  = NULL;
	CFMutableArrayRef   cfResult	= CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

	if( (pBValues = ldap_get_values_len(pLD, pMessage, pAttribute)) != NULL )
	{
		uInt32 ii = 0;
		
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
	
	if( CFArrayGetCount(cfResult) == 0 )
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
	CFStringRef cfGroupStyle	= (CFStringRef) CFDictionaryGetValue( inRecordDict, CFSTR(kXMLGroupObjectClasses) );
	CFArrayRef  cfObjectClasses = (CFArrayRef) CFDictionaryGetValue( inRecordDict, CFSTR(kXMLObjectClasses) );
	char		*pSearchString  = NULL;
	
	if( cfObjectClasses != NULL && CFArrayGetCount(cfObjectClasses) != 0 )
	{
		pSearchString = (char *) calloc( sizeof(char), 256 );
		if( cfGroupStyle != NULL && CFStringCompare(cfGroupStyle, CFSTR("AND"), 0) == kCFCompareEqualTo )
		{
			strcpy( pSearchString, "(&" );
		} else {
			strcpy( pSearchString, "(|" );
		}
		CFIndex iCount = CFArrayGetCount( cfObjectClasses );
		for( CFIndex ii = 0; ii < iCount; ii++ )
		{
			CFStringRef cfObjectClass = (CFStringRef) CFArrayGetValueAtIndex( cfObjectClasses, ii );
			if( cfObjectClass != NULL && CFGetTypeID(cfObjectClass) == CFStringGetTypeID() )
			{
				strcat( pSearchString, "(objectclass=" );
				uInt32 iLength = strlen(pSearchString);
				
				// let's be sure not to go beyond our 256 chars
				CFStringGetCString( cfObjectClass, pSearchString+iLength, 256-iLength, kCFStringEncodingUTF8 );
				strcat( pSearchString, ")" );
			}
		}
		// need to finish off our search string..
		strcat( pSearchString, ")" );
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
	
	if( cfRecordMap != NULL && CFGetTypeID(cfRecordMap) == CFArrayGetTypeID() )
	{
		CFIndex					iCount			= CFArrayGetCount( cfRecordMap );
		CFDictionaryRef			cfRecordDict	= NULL;
		
		for( CFIndex ii = 0; ii < iCount; ii++ )
		{
			CFDictionaryRef cfMapDict = (CFDictionaryRef) CFArrayGetValueAtIndex( cfRecordMap, ii );
			
			if( CFGetTypeID(cfMapDict) == CFDictionaryGetTypeID() )
			{
				CFStringRef cfMapName = (CFStringRef) CFDictionaryGetValue( cfMapDict, CFSTR(kXMLStdNameKey) );
				
				if( cfMapName != NULL && CFStringCompare( cfMapName, inRecordType, 0) == kCFCompareEqualTo )
				{
					cfRecordDict = cfMapDict;
					break;
				}
			}
		}
		
		if( cfRecordDict != NULL )
		{
			// Now let's read the newly found map and find out where to look for config records..
			CFArrayRef cfNativeMap = (CFArrayRef) CFDictionaryGetValue( cfRecordDict, CFSTR(kXMLNativeMapArrayKey) );
			
			if( cfNativeMap != NULL && CFGetTypeID(cfNativeMap) == CFArrayGetTypeID() )
			{
				if( CFArrayGetCount(cfNativeMap) > 0 )
				{
					// let's assume we have mappings at this point...
					cfReturnDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
					
					CFDictionaryRef cfNativeDict = (CFDictionaryRef) CFArrayGetValueAtIndex( cfNativeMap, 0 );
					if( cfNativeDict != NULL && CFGetTypeID(cfNativeDict) == CFDictionaryGetTypeID() )
					{
						CFStringRef cfSearchbase = (CFStringRef) CFDictionaryGetValue( cfNativeDict, CFSTR(kXMLSearchBase) );
						if( cfSearchbase != NULL && CFGetTypeID(cfSearchbase) == CFStringGetTypeID() )
							CFDictionarySetValue( cfReturnDict, CFSTR(kXMLSearchBase), cfSearchbase );
						
						CFStringRef cfGroupStyle = (CFStringRef) CFDictionaryGetValue( cfNativeDict, CFSTR(kXMLGroupObjectClasses) );
						if( cfGroupStyle != NULL && CFGetTypeID(cfGroupStyle) == CFStringGetTypeID() )
							CFDictionarySetValue( cfReturnDict, CFSTR(kXMLGroupObjectClasses), cfGroupStyle );
						
						CFArrayRef cfObjectClasses = (CFArrayRef) CFDictionaryGetValue( cfNativeDict, CFSTR(kXMLObjectClasses) );
						if( cfObjectClasses != NULL && CFGetTypeID(cfObjectClasses) == CFArrayGetTypeID() )
							CFDictionarySetValue( cfReturnDict, CFSTR(kXMLObjectClasses), cfObjectClasses );
					}
				}
			}
			
			// Now let's read the attribute map
			CFArrayRef cfAttribMap = (CFArrayRef) CFDictionaryGetValue( cfRecordDict, CFSTR(kXMLAttrTypeMapArrayKey) );
			if( cfAttribMap != NULL && CFGetTypeID(cfAttribMap) == CFArrayGetTypeID() )
			{
				CFMutableDictionaryRef  cfAttribDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
				
				CFDictionarySetValue( cfReturnDict, CFSTR("Attributes"), cfAttribDict );
				CFRelease( cfAttribDict ); // since we just added it to the dictionary we can release it now.
				
				CFIndex		iCount = CFArrayGetCount( cfAttribMap );
				for( CFIndex ii = 0; ii < iCount; ii++ )
				{
					CFDictionaryRef cfMapDict = (CFDictionaryRef) CFArrayGetValueAtIndex( cfAttribMap, ii );
					if( cfMapDict != NULL && CFGetTypeID(cfMapDict) == CFDictionaryGetTypeID() )
					{
						CFStringRef cfName = (CFStringRef) CFDictionaryGetValue( cfMapDict, CFSTR(kXMLStdNameKey) );
						
						if( cfName != NULL && CFGetTypeID(cfName) == CFStringGetTypeID())
						{
							CFArrayRef cfNativeMap = (CFArrayRef) CFDictionaryGetValue( cfMapDict, CFSTR(kXMLNativeMapArrayKey) );
							
							if( cfNativeMap != NULL && CFGetTypeID(cfNativeMap) == CFArrayGetTypeID() )
							{
								// set the key to the name and array to the value
								CFDictionarySetValue( cfAttribDict, cfName, cfNativeMap );
							}
						}
					}
				}
			}
			
			// now let's go through the Attribute Type Map and see if there are any mappings we don't already have
			cfAttribMap = (CFArrayRef) CFDictionaryGetValue( inDict, CFSTR(kXMLAttrTypeMapArrayKey) );
			if( cfAttribMap != NULL && CFGetTypeID(cfAttribMap) == CFArrayGetTypeID() )
			{
				CFMutableDictionaryRef  cfAttribDict = (CFMutableDictionaryRef) CFDictionaryGetValue( cfReturnDict, CFSTR("Attributes") );
				if( cfAttribDict == NULL )
				{
					cfAttribDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
					CFDictionarySetValue( cfReturnDict, CFSTR("Attributes"), cfAttribDict );
					CFRelease( cfAttribDict ); // since we just added it to the dictionary we can release it now.
				}
				
				CFIndex		iCount = CFArrayGetCount( cfAttribMap );
				for( CFIndex ii = 0; ii < iCount; ii++ )
				{
					CFDictionaryRef cfMapDict = (CFDictionaryRef) CFArrayGetValueAtIndex( cfAttribMap, ii );
					if( cfMapDict != NULL && CFGetTypeID(cfMapDict) == CFDictionaryGetTypeID() )
					{
						CFStringRef cfName = (CFStringRef) CFDictionaryGetValue( cfMapDict, CFSTR(kXMLStdNameKey) );
						
						if( cfName != NULL && CFGetTypeID(cfName) == CFStringGetTypeID())
						{
							CFArrayRef cfNativeMap = (CFArrayRef) CFDictionaryGetValue( cfMapDict, CFSTR(kXMLNativeMapArrayKey) );
							
							if( cfNativeMap != NULL && CFGetTypeID(cfNativeMap) == CFArrayGetTypeID() )
							{
								// if the key doesn't already exist, let's go ahead and put the high level one in there..
								// it's not additive, only if it is missing
								if( CFDictionaryGetValue(cfAttribDict, cfName) == NULL )
								{
									// set the key to the name and array to the value
									CFDictionarySetValue( cfAttribDict, cfName, cfNativeMap );
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
	
	if( cfAttribMap != NULL )
	{
		cfAttributeArray = (CFArrayRef) CFDictionaryGetValue( cfAttribMap, inAttribute );
		if( (outFirstAttribute != NULL || outCFFirstAttribute != NULL) && cfAttributeArray != NULL && CFGetTypeID(cfAttributeArray) == CFArrayGetTypeID() )
		{
			CFStringRef cfAttribute = (CFStringRef) CFArrayGetValueAtIndex( cfAttributeArray, 0 );

			if( cfAttribute != NULL && CFGetTypeID(cfAttribute) == CFStringGetTypeID() )
			{
				if( outFirstAttribute != NULL )
				{
					uInt32 iLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfAttribute), kCFStringEncodingUTF8) + 1;
					*outFirstAttribute = (char *) calloc( sizeof(char), iLength );
					CFStringGetCString( cfAttribute, *outFirstAttribute, iLength, kCFStringEncodingUTF8 );
				}
				
				if( outCFFirstAttribute != NULL )
				{
					*outCFFirstAttribute = cfAttribute;
				}
			}
		}
	} else if( outCFFirstAttribute ) {
		*outCFFirstAttribute = NULL;
	}
	return cfAttributeArray;
}// GetAttribFromDict


// ---------------------------------------------------------------------------
//	* IsServerInConfig
// ---------------------------------------------------------------------------

bool CLDAPv3Plugin::IsServerInConfig( CFDictionaryRef inConfig, CFStringRef inServerName, CFIndex *outIndex, CFMutableDictionaryRef *outConfig )
{
	bool	bReturn			= false;
	bool	bFreeInConfig   = false;

	// if we weren't passed a config, let's get our current configuration
	if( inConfig == NULL )
	{
		CFDataRef cfTempData = gpConfigFromXML->CopyXMLConfig();
		
		inConfig = (CFDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfTempData, kCFPropertyListImmutable, NULL );
		
		// we're done with the TempData at this point
		CFRelease( cfTempData );
		cfTempData = NULL;
		
		bFreeInConfig = true;
	}
	
	// let's grab the configured list of servers...
	CFMutableArrayRef cfConfigList = (CFMutableArrayRef) CFDictionaryGetValue( inConfig, CFSTR(kXMLConfigArrayKey) );
	if( cfConfigList != NULL )
	{
		CFIndex iCount = CFArrayGetCount( cfConfigList );
		for( CFIndex ii = 0; ii < iCount; ii++ )
		{
			CFMutableDictionaryRef cfConfigServerDict = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( cfConfigList, ii );
			CFStringRef		cfTempServer = (CFStringRef) CFDictionaryGetValue( cfConfigServerDict, CFSTR(kXMLServerKey) );
			
			if( cfTempServer && CFStringCompare( cfTempServer, inServerName, kCFCompareCaseInsensitive) == kCFCompareEqualTo )
			{
				if( outConfig )
				{
					*outConfig = cfConfigServerDict;
					CFRetain( cfConfigServerDict ); // we need to retain cause we're going to release below
				}
				if( outIndex )
				{
					*outIndex = ii;
				}
				bReturn = true;
				break;
			}
		}
	}
	
	if( bFreeInConfig )
	{
		CFRelease( inConfig );
		inConfig = NULL;
	}
	
	return bReturn;
} // IsServerInConfig

// ---------------------------------------------------------------------------
//	* GetServerInfoFromConfig
// ---------------------------------------------------------------------------

CFStringRef CLDAPv3Plugin::GetServerInfoFromConfig( CFDictionaryRef inDict, char **outServer, bool *outSSL, bool *outLDAPv2ReadOnly, char **pUsername, char **pPassword )
{
	// automatically released, just a Get.
	CFStringRef cfServer = (CFStringRef) CFDictionaryGetValue( inDict, CFSTR(kXMLServerKey) );
	if( cfServer != NULL && CFGetTypeID(cfServer) == CFStringGetTypeID() && outServer != NULL )
	{
		uInt32 iLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfServer), kCFStringEncodingUTF8) + 1;
		*outServer = (char *) calloc( sizeof(char), iLength );
		CFStringGetCString( cfServer, *outServer, iLength, kCFStringEncodingUTF8 );
	}
	
	if( outSSL )
	{
		CFBooleanRef cfBool = (CFBooleanRef) CFDictionaryGetValue( inDict, CFSTR(kXMLIsSSLFlagKey) );
		if( cfBool != NULL && CFGetTypeID(cfBool) == CFBooleanGetTypeID() )
		{
			*outSSL = CFBooleanGetValue( cfBool );
		}
	}

	if( outLDAPv2ReadOnly )
	{
		CFBooleanRef cfBool = (CFBooleanRef) CFDictionaryGetValue( inDict, CFSTR(kXMLLDAPv2ReadOnlyKey) );
		if( cfBool != NULL && CFGetTypeID(cfBool) == CFBooleanGetTypeID() )
		{
			*outLDAPv2ReadOnly = CFBooleanGetValue( cfBool );
		}
	}

	if( pUsername )
	{
		CFStringRef cfUsername = (CFStringRef) CFDictionaryGetValue( inDict, CFSTR(kXMLServerAccountKey) );
		
		if( cfUsername != NULL && CFGetTypeID(cfUsername) == CFStringGetTypeID() && CFStringGetLength(cfUsername) != 0 )
		{
			uInt32 iLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfUsername), kCFStringEncodingUTF8) + 1;
			*pUsername = (char *) calloc( sizeof(char), iLength );
			CFStringGetCString( cfUsername, *pUsername, iLength, kCFStringEncodingUTF8 );
		}
	}

	if( pPassword )
	{
		CFStringRef cfPassword = (CFStringRef) CFDictionaryGetValue( inDict, CFSTR(kXMLServerPasswordKey) );
		
		if( cfPassword != NULL && CFGetTypeID(cfPassword) == CFStringGetTypeID() && CFStringGetLength(cfPassword) != 0 )
		{
			uInt32 iLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfPassword), kCFStringEncodingUTF8) + 1;
			*pPassword = (char *) calloc( sizeof(char), iLength );
			CFStringGetCString( cfPassword, *pPassword, iLength, kCFStringEncodingUTF8 );
		}
	}
	
	return cfServer;
} // GetServerInfoFromConfig

// ---------------------------------------------------------------------------
//	* GetXMLFromBuffer
// ---------------------------------------------------------------------------

CFMutableDictionaryRef CLDAPv3Plugin::GetXMLFromBuffer( tDataBufferPtr inBuffer )
{
	sInt32					iBufLen		= inBuffer->fBufferLength;
	CFMutableDictionaryRef  cfXMLDict   = NULL;

	// we always get an XML blob, so let's parse the blob so we can do something with it
	sInt32 iXMLDataLength = iBufLen - sizeof( AuthorizationExternalForm );
	
	if( iXMLDataLength > 0 )
	{
		CFDataRef cfXMLData = CFDataCreate( kCFAllocatorDefault, (UInt8 *)(inBuffer->fBufferData + sizeof(AuthorizationExternalForm)), iXMLDataLength );
		
		if( cfXMLData != NULL )
		{
			// extract the config dictionary from the XML data.
			cfXMLDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfXMLData, kCFPropertyListMutableContainersAndLeaves, NULL );
			
			CFRelease( cfXMLData ); // let's release it, we're done with it
			cfXMLData = NULL;
		}
	}
	
	return cfXMLDict;
} // GetXMLFromBuffer

// ---------------------------------------------------------------------------
//	* PutXMLInBuffer
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::PutXMLInBuffer( CFDictionaryRef inXMLDict, tDataBufferPtr outBuffer )
{
	CFDataRef   cfReturnData = (CFDataRef) CFPropertyListCreateXMLData( kCFAllocatorDefault, inXMLDict );
	sInt32		siResult = eDSNoErr;
	
	if( cfReturnData )
	{
		CFRange stRange = CFRangeMake( 0, CFDataGetLength(cfReturnData) );
		if ( outBuffer->fBufferSize < (unsigned int) stRange.length ) 
		{
			siResult = eDSBufferTooSmall;
		}
		else
		{
			CFDataGetBytes( cfReturnData, stRange, (UInt8*)(outBuffer->fBufferData) );
			outBuffer->fBufferLength = stRange.length;
		}
		CFRelease( cfReturnData );
		cfReturnData = 0;
	}
	return siResult;
} // PutXMLInBuffer

#pragma mark -
#pragma mark New Server Setup Functions

// ---------------------------------------------------------------------------
//	* DoNewServerConfig
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoNewServerDiscovery( sDoPlugInCustomCall *inData )
{
	sInt32					siResult		= eDSBogusServer;
	bool					bSSL			= false;
	bool					bLDAPv2ReadOnly	= false;
	CFMutableDictionaryRef  cfXMLDict		= NULL;
	LDAP					*pLD			= NULL;
	char					*pServer		= NULL;
	
	try {
		// we should always have XML data for this process, if we don't, throw an error
		cfXMLDict = GetXMLFromBuffer( inData->fInRequestData );
		if( cfXMLDict == NULL ) {
			throw( (sInt32) eDSInvalidBuffFormat );
		}
		
		// automatically released, just a Get.
		CFStringRef cfServer = GetServerInfoFromConfig( cfXMLDict, &pServer, &bSSL, &bLDAPv2ReadOnly, NULL, NULL );
		if( cfServer == NULL )
		{
			throw( (sInt32) eDSInvalidBuffFormat );
		}
		
		// see if server is already in our config, return error if so
		if( inData->fInRequestCode == 201 && IsServerInConfig(NULL, cfServer, NULL, NULL) )
		{
			throw( (sInt32) eDSRecordAlreadyExists );
		}
		
		// this is an attempt to determine the server, see if we can contact it, and get some basic information

		// first let's get the root DSE and see what we can figure out..
		char			*pAttributes[]  = { "namingContexts", "defaultNamingContext", NULL };
		timeval			stTimeout		= { kLDAPDefaultOpenCloseTimeoutInSeconds, 0 }; // default kLDAPDefaultOpenCloseTimeoutInSeconds seconds
		LDAPMessage		*pLDAPResult	= NULL;
		sInt32			bindResult		= eDSNoErr;
		
		// let's do a bind... if we can't bind anonymous to get the root DSE, then we error back
		if( (pLD = DoSimpleLDAPBind(pServer, bSSL, bLDAPv2ReadOnly, NULL, NULL, false, &bindResult)) == NULL )
		{
			if( bindResult == eDSReadOnly)
			{
				bLDAPv2ReadOnly = true;
				if( (pLD = DoSimpleLDAPBind(pServer, bSSL, bLDAPv2ReadOnly, NULL, NULL)) == NULL ) {
					// eDSBogusServer means no server responding at that address
					throw( (sInt32) eDSBogusServer );
				}
			}
			// if we aren't already SSL, let's try it just to be sure..
			else if( !bSSL && !bLDAPv2ReadOnly )
			{
				bSSL = true;
				if( (pLD = DoSimpleLDAPBind(pServer, bSSL, bLDAPv2ReadOnly, NULL, NULL)) == NULL ) {
					// eDSBogusServer means no server responding at that address
					throw( (sInt32) eDSBogusServer );
				}
			}
			else
			{
				// eDSBogusServer means no server responding at that address
				throw( (sInt32) eDSBogusServer );
			}
		}
		
		// now let's continue our discovery process
		int iLDAPRetCode = ldap_search_ext_s( pLD, LDAP_ROOT_DSE, LDAP_SCOPE_BASE, "(objectclass=*)", pAttributes, false, NULL, NULL, &stTimeout, 0, &pLDAPResult );
		
		// This should never fail, unless we aren't really talking to an LDAP server
		if( iLDAPRetCode != LDAP_SUCCESS && !bLDAPv2ReadOnly )
		{
			// eDSBogusServer means we couldn't really talk to the server.. so let's consider it bogus??
			throw( (sInt32) eDSBogusServer );
		}
		
		// let's see if this server has a defaultNamingContext (iPlanet & Others).
		CFArrayRef cfNameContext = GetLDAPAttributeFromResult( pLD, pLDAPResult, "defaultNamingContext" );
		if( cfNameContext == NULL )
		{
			// if no defaultNamingContext, let's get the normal attribute.  UI will handle more than 1 context
			cfNameContext = GetLDAPAttributeFromResult( pLD, pLDAPResult, "namingContexts" );
		}
		
		if( pLDAPResult )
		{
			ldap_msgfree( pLDAPResult );
			pLDAPResult = NULL;
		}
		
		// at this point the server has responded...
		siResult = eDSNoErr;
		
		// Now let's search for Server mappings to see if they exist, cause hopefully it will be an OD server
		if( cfNameContext )
		{
			CFIndex iNumContexts = CFArrayGetCount( cfNameContext );
			bool	bFound = false;
			char	*pAttributes[]  = { "description", NULL };
			berval  **pBValues  = NULL;
			
			for( CFIndex ii = 0; ii < iNumContexts && !bFound; ii++ )
			{
				CFStringRef cfSearchbase = (CFStringRef) CFArrayGetValueAtIndex( cfNameContext, ii );
				
				// if we only have 1 naming context, let's go ahead and set the Template Search Base,in case we don't get Server Mappings
				if( iNumContexts == 1 )
				{
					CFDictionarySetValue( cfXMLDict, CFSTR("Template Search Base Suffix"), cfSearchbase );
				}
				
				uInt32 iLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfSearchbase), kCFStringEncodingUTF8) + 1;
				char *pSearchbase = (char *) calloc( sizeof(char), iLength );
				CFStringGetCString( cfSearchbase, pSearchbase, iLength, kCFStringEncodingUTF8 );
				
				int iLDAPRetCode = ldap_search_ext_s( pLD, pSearchbase, LDAP_SCOPE_SUBTREE, "(&(objectclass=organizationalUnit)(ou=macosxodconfig))", pAttributes, false, NULL, NULL, &stTimeout, 0, &pLDAPResult );
				
				if( iLDAPRetCode == LDAP_SUCCESS )
				{
					if( (pBValues = ldap_get_values_len(pLD, pLDAPResult, "description")) != NULL )
					{
						// good, we have a server mappings config...  Let's use the config we just got from the server
						CFDataRef cfXMLData = CFDataCreate( kCFAllocatorDefault, (UInt8 *)(pBValues[0]->bv_val), pBValues[0]->bv_len );
						
						if( cfXMLData != NULL )
						{
							// extract the config dictionary from the XML data and this is our new configuration
							CFMutableDictionaryRef cfTempDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfXMLData, kCFPropertyListMutableContainersAndLeaves, NULL );
							
							// make sure we got a dictionary and it has something in it...
							if( cfTempDict && CFGetTypeID(cfTempDict) == CFDictionaryGetTypeID() && CFDictionaryGetCount(cfTempDict) )
							{
								CFArrayRef  cfRecordMap = (CFArrayRef) CFDictionaryGetValue( cfTempDict, CFSTR(kXMLRecordTypeMapArrayKey) );
								
								// let's see if we have a Record Map, if not, we won't consider this a valid server config
								if( cfRecordMap && CFArrayGetCount(cfRecordMap) != 0 )
								{
									// let's reset server setting to the server the user entered
									CFDictionarySetValue( cfTempDict, CFSTR(kXMLServerKey), cfServer );
									
									// set server mappings again, since the ones we got don't have the flag on
									CFDictionarySetValue( cfTempDict, CFSTR(kXMLServerMappingsFlagKey), kCFBooleanTrue );
									
									// let's release the old config, since we are exchanging them..
									CFRelease( cfXMLDict );

									// let's exchange them and NULL cfTempDict otherwise we'll release it accidently
									cfXMLDict = cfTempDict;
									cfTempDict = NULL;
									
									// we found it...
									bFound = true;
								}
							}
							
							if( cfTempDict != NULL )
							{
								CFRelease( cfTempDict );
								cfTempDict = NULL;
							}
							
							CFRelease( cfXMLData ); // let's release it, we're done with it
							cfXMLData = NULL;
						}
						
						ldap_value_free_len( pBValues );
						pBValues = NULL;
					} 
					
					ldap_msgfree( pLDAPResult );
					pLDAPResult = NULL;
				}
				
				if( pSearchbase )
				{
					free( pSearchbase );
					pSearchbase = NULL;
				}
			}

			CFRelease( cfNameContext );
			cfNameContext = NULL;
		}
		
		// let's set the key for when we send it back...
		int iPortNumber = (bSSL ? LDAPS_PORT : LDAP_PORT);
		CFNumberRef cfPortNumber = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &iPortNumber ); 
		
		CFDictionarySetValue( cfXMLDict, CFSTR(kXMLPortNumberKey), cfPortNumber );
		CFDictionarySetValue( cfXMLDict, CFSTR(kXMLIsSSLFlagKey), (bSSL ? kCFBooleanTrue : kCFBooleanFalse) );
		CFDictionarySetValue( cfXMLDict, CFSTR(kXMLLDAPv2ReadOnlyKey), (bLDAPv2ReadOnly ? kCFBooleanTrue : kCFBooleanFalse) );
		
		CFRelease( cfPortNumber );
		cfPortNumber = NULL;
		
	} catch( sInt32 err ) {
		siResult = err;
	} catch( ... ) {
		// catch all for miss throws...
		siResult = eUndefinedError;
	}
	
	if( pServer != NULL )
	{
		free( pServer );
		pServer = NULL;
	}
	
	if( pLD != NULL )
	{
		ldap_unbind_ext_s( pLD, NULL, NULL );
		pLD = NULL;
	}
	
	if( cfXMLDict != NULL )
	{
		if( siResult == eDSNoErr )
		{
			sInt32 iError = PutXMLInBuffer( cfXMLDict, inData->fOutRequestResponse );
			if( iError != eDSNoErr ) {
				siResult = iError;
			}
		}

		CFRelease( cfXMLDict );
		cfXMLDict = NULL;
	}
	
	return siResult;
} // DoNewServerDiscovery

// ---------------------------------------------------------------------------
//	* DoNewServerVerifySettings
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoNewServerVerifySettings( sDoPlugInCustomCall *inData )
{
	sInt32					siResult				= eDSInvalidNativeMapping;
	sInt32					siBindResult			= eDSAuthFailed;
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
		if( cfXMLDict == NULL ) {
			throw( (sInt32) eDSInvalidBuffFormat );
		}
		
		// Automatically released, just a Get.
		CFStringRef cfServer = GetServerInfoFromConfig( cfXMLDict, &pServer, &bSSL, &bLDAPv2ReadOnly, &pUsername, &pPassword );
		if( cfServer == NULL )
		{
			throw( (sInt32) eDSInvalidBuffFormat );
		}
		
		// let's attempt to query something from the server... users are the safest, but we'll try groups, then computers if previous do not exist
		cfRecordMap = CreateMappingFromConfig( cfXMLDict, CFSTR(kDSStdRecordTypeUsers) );
		if( cfRecordMap == NULL )
		{
			cfRecordMap = CreateMappingFromConfig( cfXMLDict, CFSTR(kDSStdRecordTypeGroups) );
			if( cfRecordMap == NULL )
			{
				cfRecordMap = CreateMappingFromConfig( cfXMLDict, CFSTR(kDSStdRecordTypeComputers) );
				if( cfRecordMap == NULL )
				{
					throw( (sInt32) eDSInvalidNativeMapping );
				}
			}
		}
		
		// Now continue to try a query to the server after building a search string appropriately
		if( (pLD = DoSimpleLDAPBind(pServer, bSSL, bLDAPv2ReadOnly, pUsername, pPassword, false, &siBindResult)) == NULL )
		{
			throw( siBindResult );
		}
		
		// First let's get Searchbase from config
		CFStringRef cfSearchbase = (CFStringRef) CFDictionaryGetValue( cfRecordMap, CFSTR(kXMLSearchBase) );
		if( cfSearchbase != NULL)
		{
			uInt32 iLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfSearchbase), kCFStringEncodingUTF8) + 1;
			pSearchbase = (char *) calloc( sizeof(char), iLength );
			CFStringGetCString( cfSearchbase, pSearchbase, iLength, kCFStringEncodingUTF8 );
		}
		
		// Let's start our search string with a Parenthesis
		pObjectSearchString = GenerateSearchString( cfRecordMap );
		if( pObjectSearchString == NULL )
		{
			throw( (sInt32) eDSInvalidNativeMapping );
		}
		
		// Don't care what attributes, let's get them all, since all we want are the names
		int iLDAPRetCode = ldap_search_ext_s( pLD, pSearchbase, LDAP_SCOPE_SUBTREE, pObjectSearchString, NULL, true, NULL, NULL, &stTimeout, 1, &pLDAPResult );

		// we might get a size limit exceeded message since we're only asking for 1
		if( iLDAPRetCode == LDAP_SUCCESS || iLDAPRetCode == LDAP_SIZELIMIT_EXCEEDED )
		{
			int iCount = ldap_count_entries( pLD, pLDAPResult );

			// well if we didn't get any results, must not have access..
			if( iCount == 0 )
			{
				siResult = eDSAuthParameterError;
			}
			else
			{
				siResult = eDSNoErr;
			}
		}
		
		if( pLDAPResult != NULL )
		{
			ldap_msgfree( pLDAPResult );
			pLDAPResult = NULL;
		}
		
	} catch( sInt32 iError ) {
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
	
	if( pPassword != NULL )
	{
		free( pPassword );
		pPassword = NULL;
	}
	
	if( pLD != NULL )
	{
		ldap_unbind_ext_s( pLD, NULL, NULL );
		pLD = NULL;
	}
	
	if( cfXMLDict != NULL )
	{
		if( siResult == eDSNoErr )
		{
			sInt32 iError = PutXMLInBuffer( cfXMLDict, inData->fOutRequestResponse );
			if( iError != eDSNoErr ) {
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

sInt32 CLDAPv3Plugin::DoNewServerGetConfig( sDoPlugInCustomCall *inData )
{
	bool					bSSL			= false;
	sInt32					siResult			= eDSInvalidNativeMapping;
	sInt32					siBindResult		= eDSNoErr;
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
		if( cfXMLDict == NULL )
		{
			throw( (sInt32) eDSInvalidBuffFormat );
		}
		
		// automatically released, just a Get.
		CFStringRef cfServer = GetServerInfoFromConfig( cfXMLDict, &pServer, &bSSL, &bLDAPv2ReadOnly, &pUsername, &pPassword );
		if( cfServer == NULL )
		{
			throw( (sInt32) eDSInvalidBuffFormat );
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
		if( (pLD = DoSimpleLDAPBind(pServer, bSSL, bLDAPv2ReadOnly, pUsername, pPassword, false, &siBindResult)) == NULL )
		{
			throw( siBindResult );
		}

		// Start the dictionary for the Security Levels supported
		cfSupportedSecLevel = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		
		// Store the dictionary in the configuration..
		CFDictionarySetValue( cfXMLDict, CFSTR(kXMLSupportedSecurityKey), cfSupportedSecLevel );
		
		// if we have SSL, we have some capabilities already..
		if( bSSL )
		{
			CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityNoClearTextAuths), kCFBooleanTrue );
			CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityPacketEncryption), kCFBooleanTrue );
		}
		
		// get sasl mechanisms for later use...  TBD --- Do we need to discover this dynamically from ourselves / LDAP Framework????
		CFArrayRef cfSASLMechs = GetSASLMethods( pLD );
		if( cfSASLMechs )
		{
			CFRange		stRange = CFRangeMake( 0, CFArrayGetCount(cfSASLMechs) );
			
			if( CFArrayContainsValue( cfSASLMechs, stRange, CFSTR("CRAM-MD5")) && fLDAPSessionMgr.isSASLMethodSupported(CFSTR("CRAM-MD5")) )
			{
				CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityNoClearTextAuths), kCFBooleanTrue );
			}
			
			if( CFArrayContainsValue( cfSASLMechs, stRange, CFSTR("GSSAPI")) && fLDAPSessionMgr.isSASLMethodSupported(CFSTR("GSSAPI")) )
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
		if( cfConfigMap == NULL )
		{
			throw( (sInt32) eDSNoErr );
		}
		
		// Next let's get Searchbase from config
		CFStringRef cfSearchbase = (CFStringRef) CFDictionaryGetValue( cfConfigMap, CFSTR(kXMLSearchBase) );
		if( cfSearchbase != NULL)
		{
			uInt32 iLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfSearchbase), kCFStringEncodingUTF8) + 1;
			pSearchbase = (char *) calloc( sizeof(char), iLength );
			CFStringGetCString( cfSearchbase, pSearchbase, iLength, kCFStringEncodingUTF8 );
		}
		
		// Let's start our search string with a Parenthesis
		char *pObjectSearchString = GenerateSearchString( cfConfigMap );
		if( pObjectSearchString != NULL )
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
			throw( (sInt32) eDSNoErr );
		}
		
		CFArrayRef cfAttributeArray = GetAttribFromRecordDict( cfConfigMap, CFSTR(kDS1AttrXMLPlist), &pAttribute );
		if( cfAttributeArray == NULL )
		{
			throw( (sInt32) eDSNoErr );
		}
		
		LDAPMessage		*pLDAPResult		= NULL;

		int iLDAPRetCode = ldap_search_ext_s( pLD, pSearchbase, LDAP_SCOPE_SUBTREE, pSearchString, NULL, false, NULL, NULL, &stTimeout, 0, &pLDAPResult );
		
		if( iLDAPRetCode == LDAP_SUCCESS )
		{
			berval  **pBValues  = NULL;
			
			if( (pBValues = ldap_get_values_len(pLD, pLDAPResult, pAttribute)) != NULL )
			{
				// good, we have a policy config record...
				CFDataRef cfXMLData = CFDataCreate( kCFAllocatorDefault, (UInt8 *)(pBValues[0]->bv_val), pBValues[0]->bv_len );
				
				if( cfXMLData != NULL )
				{
					// extract the config dictionary from the XML data and this is our new configuration
					CFDictionaryRef cfTempDict = (CFDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfXMLData, kCFPropertyListImmutable, NULL );
					
					// make sure we got a dictionary and it has something in it...
					if( cfTempDict )
					{
						CFDictionaryRef cfSecurityLevel = (CFDictionaryRef) CFDictionaryGetValue( cfTempDict, CFSTR(kXMLConfiguredSecurityKey) );
						if( cfSecurityLevel != NULL && CFGetTypeID(cfSecurityLevel) == CFDictionaryGetTypeID() )
						{
							CFDictionarySetValue( cfXMLDict, CFSTR(kXMLConfiguredSecurityKey), cfSecurityLevel );
						}
						
						CFBooleanRef cfBindingActive = (CFBooleanRef) CFDictionaryGetValue( cfTempDict, CFSTR(kXMLDirectoryBindingKey) );
						if( cfBindingActive != NULL && CFGetTypeID(cfBindingActive) == CFBooleanGetTypeID() )
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

		if( pLDAPResult )
		{
			ldap_msgfree( pLDAPResult );
			pLDAPResult = NULL;
		}
		
		//
		// now let's figure out the realm of the server so that we can create a temporary kerberos if necessary
		iLDAPRetCode = ldap_search_ext_s( pLD, pSearchbase, LDAP_SCOPE_SUBTREE, pKerbSearchString, NULL, false, NULL, NULL, &stTimeout, 0, &pLDAPResult );
		
		if( iLDAPRetCode == LDAP_SUCCESS )
		{
			berval  **pBValues  = NULL;
			
			if( (pBValues = ldap_get_values_len(pLD, pLDAPResult, pAttribute)) != NULL )
			{
				// good, we have a policy config record...
				CFDataRef cfXMLData = CFDataCreate( kCFAllocatorDefault, (UInt8 *)(pBValues[0]->bv_val), pBValues[0]->bv_len );
				
				if( cfXMLData != NULL )
				{
					// extract the config dictionary from the XML data and this is our new configuration
					CFDictionaryRef cfTempDict = (CFDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfXMLData, kCFPropertyListImmutable, NULL );
					
					// make sure we got a dictionary and it has something in it...
					if( cfTempDict != NULL )
					{
						CFDictionaryRef cfWorkingDict;
						
						cfWorkingDict = (CFDictionaryRef) CFDictionaryGetValue( cfTempDict, CFSTR("edu.mit.kerberos") );
						if( cfWorkingDict != NULL )
						{
							cfWorkingDict = (CFDictionaryRef) CFDictionaryGetValue( cfWorkingDict, CFSTR("libdefaults") );
							if( cfWorkingDict != NULL )
							{
								CFStringRef cfString = (CFStringRef) CFDictionaryGetValue( cfWorkingDict, CFSTR("default_realm") );
								
								if( cfString != NULL )
								{
									uInt32 iLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfString), kCFStringEncodingUTF8) + 1;
									char *pKerberosRealm = (char *) calloc( sizeof(char), iLength );
									CFStringGetCString( cfString, pKerberosRealm, iLength, kCFStringEncodingUTF8 );
									
									gpConfigFromXML->VerifyKerberosForRealm( pKerberosRealm, pServer );
									
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
		
		if( pLDAPResult )
		{
			ldap_msgfree( pLDAPResult );
			pLDAPResult = NULL;
		}
				
		siResult = eDSNoErr;
	} catch( sInt32 iError ) {
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
	
	if( pPassword != NULL )
	{
		free( pPassword );
		pPassword = NULL;
	}
	
	if( pLD != NULL )
	{
		ldap_unbind_ext_s( pLD, NULL, NULL );
		pLD = NULL;
	}
	
	if( cfXMLDict != NULL )
	{
		if( siResult == eDSNoErr )
		{
			sInt32 iError = PutXMLInBuffer( cfXMLDict, inData->fOutRequestResponse );
			if( iError != eDSNoErr ) {
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

sInt32 CLDAPv3Plugin::DoNewServerBind( sDoPlugInCustomCall *inData )
{
	sInt32					siResult				= eDSInvalidNativeMapping;
	CFMutableDictionaryRef  cfXMLDict				= NULL;
	char					*pUsername				= NULL;
	char					*pPassword				= NULL;
	CFDataRef				cfCurrentConfigData		= NULL;

	try
	{
		// we should always have XML data for this process, if we don't, throw an error
		cfXMLDict = GetXMLFromBuffer( inData->fInRequestData );
		if( cfXMLDict == NULL ) {
			throw( (sInt32) eDSInvalidBuffFormat );
		}
		
		// automatically released, just a Get.
		CFStringRef cfServer = GetServerInfoFromConfig( cfXMLDict, NULL, NULL, NULL, &pUsername, &pPassword );
		if( cfServer == NULL )
		{
			throw( (sInt32) eDSInvalidBuffFormat );
		}
		
		// we need credentials at this point, if we don't have them throw an error
		if( pUsername == NULL || pPassword == NULL )
		{
			throw( (sInt32) eDSAuthParameterError );
		}
		
		// let's be sure the Auth flag is not set otherwise we will use the credentials instead of dsDoDirNodeAuth
		CFDictionarySetValue( cfXMLDict, CFSTR(kXMLSecureUseFlagKey), kCFBooleanFalse );
		
		// now let's use internal ways of doing things...  We'll add the new config to the config dictionary, and update it later...
		// this is so we can use all existing code to do the rest of the work...
		cfCurrentConfigData = gpConfigFromXML->CopyXMLConfig();
		
		CFMutableDictionaryRef cfConfigXML = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfCurrentConfigData, kCFPropertyListMutableContainersAndLeaves, NULL );

		// let's grab the configured list of servers...
		CFMutableArrayRef  cfConfigList = (CFMutableArrayRef) CFDictionaryGetValue( cfConfigXML, CFSTR(kXMLConfigArrayKey) );

		// let's see if our server is in our config, we could be binding with existing... if so, let's swap them out, otherwise add it to the end.
		CFIndex iIndex = 0;
		if( IsServerInConfig(cfConfigXML, cfServer, &iIndex, NULL) )
		{
			// let's swap the new config for the old one..
			CFArraySetValueAtIndex( cfConfigList, iIndex, cfXMLDict );
		}
		else
		{
			// let's be sure we have an array list, in particular for a fresh configuration
			if( cfConfigList == NULL )
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
		gpConfigFromXML->SetXMLConfig( cfNewConfig );
		
		CFRelease( cfNewConfig );
		cfNewConfig = NULL;
		
		CFRelease( cfConfigXML );
		cfConfigXML = NULL;
		
		Initialize();

		// this is to re-use code for multiple bind scenarios
		siResult = DoNewServerBind2( inData );
		
	} catch( sInt32 iError ) {
		siResult = iError;
	} catch( ... ) {
		// catch all for miss throws...
		siResult = eUndefinedError;
	}
	
	if( pUsername != NULL )
	{
		free( pUsername );
		pUsername = NULL;
	}
	
	if( pPassword != NULL )
	{
		memset( pPassword, 0, strlen(pPassword) );
		free( pPassword );
		pPassword = NULL;
	}
	
	if( cfXMLDict )
	{
		CFRelease( cfXMLDict );
		cfXMLDict = NULL;
	}
	
	// we need to revert the configuration...
	if( cfCurrentConfigData != NULL )
	{
		gpConfigFromXML->SetXMLConfig( cfCurrentConfigData );
		
		Initialize();

		CFRelease( cfCurrentConfigData );
		cfCurrentConfigData = NULL;
	}
	
	return siResult;
} // DoNewServerBind

// ---------------------------------------------------------------------------
//	* DoNewServerBind2
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoNewServerBind2( sDoPlugInCustomCall *inData )
{
	sInt32					siResult				= eDSInvalidNativeMapping;
	CFMutableDictionaryRef  cfXMLDict				= NULL;
	char					*pServer				= NULL;
	char					*pUsername				= NULL;
	char					*pPassword				= NULL;
	char					*pComputerID			= NULL;
	CFDictionaryRef			cfComputerMap			= NULL;
	tDirReference			dsRef					= 0;
	tDirNodeReference		dsNodeRef				= 0;
	tDataNodePtr			pRecType				= ::dsDataNodeAllocateString( dsRef, kDSStdRecordTypeComputers );;
	tDataBufferPtr			responseDataBufPtr		= ::dsDataBufferAllocatePriv( 1024 );
	tDataBufferPtr			sendDataBufPtr			= ::dsDataBufferAllocatePriv( 1024 );
	tDataNodePtr			pAuthType				= NULL;
	tDataNodePtr			pRecName				= NULL;
	tDataList				dataList				= { 0 };

	try
	{
		// we should always have XML data for this process, if we don't, throw an error
		cfXMLDict = GetXMLFromBuffer( inData->fInRequestData );
		if( cfXMLDict == NULL ) {
			throw( (sInt32) eDSInvalidBuffFormat );
		}
		
		// automatically released, just a Get.
		CFStringRef cfServer = GetServerInfoFromConfig( cfXMLDict, &pServer, NULL, NULL, &pUsername, &pPassword );
		if( cfServer == NULL )
		{
			throw( (sInt32) eDSInvalidBuffFormat );
		}
		
		// we need credentials at this point, if we don't have them throw an error
		if( pUsername == NULL || pPassword == NULL )
		{
			throw( (sInt32) eDSAuthParameterError );
		}
		
		// let's be sure the Auth flag is not set otherwise we will use the credentials instead of dsDoDirNodeAuth
		CFDictionarySetValue( cfXMLDict, CFSTR(kXMLSecureUseFlagKey), kCFBooleanFalse );
		
		// first let's verify we have a Computer Record mapping, if not thow eDSInvalidNativeMapping error
		cfComputerMap = CreateMappingFromConfig( cfXMLDict, CFSTR(kDSStdRecordTypeComputers) );
		if( cfComputerMap == NULL )
		{
			throw( (sInt32) eDSNoStdMappingAvailable );
		}

		// let's grab the computer name cause we'll need it..
		CFStringRef cfComputerName = (CFStringRef) CFDictionaryGetValue( cfXMLDict, CFSTR(kXMLUserDefinedNameKey) );
		if( cfComputerName != NULL && CFGetTypeID(cfComputerName) == CFStringGetTypeID() )
		{
			uInt32 iLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfComputerName), kCFStringEncodingUTF8) + 1;
			pComputerID = (char *) calloc( sizeof(char), iLength );
			CFStringGetCString( cfComputerName, pComputerID, iLength, kCFStringEncodingUTF8 );
		} else {
			throw( (sInt32) eDSInvalidRecordName );
		}
		
		// let's build our path....
		siResult = ::dsOpenDirService( &dsRef );
		if( siResult != eDSNoErr )
		{
			throw( (sInt32) siResult );
		}

		char		pNodeName[255];
		
		strcpy( pNodeName, "/LDAPv3/" );
		strcat( pNodeName, pServer );
		
		tDataList	*nodeString = ::dsBuildFromPathPriv( pNodeName, "/" );
		
		siResult = ::dsOpenDirNode( dsRef, nodeString, &dsNodeRef );

		::dsDataListDeallocatePriv( nodeString );
		free( nodeString );

		if( siResult != eDSNoErr )
		{
			DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Bind Request - Unable to open Directory Node %s", pNodeName );
			throw( (sInt32) siResult );
		}
		
		uInt32			iNameLen		= strlen( pUsername );
		uInt32			iPassLen		= strlen( pPassword );
		uInt32			authDataSize	= iNameLen + iPassLen + (2 * sizeof(uInt32));
		
		pAuthType   = ::dsDataNodeAllocateString( dsRef, kDSStdAuthNodeNativeNoClearText );

		sendDataBufPtr->fBufferLength = authDataSize;
		
		/* store user name and password into the auth buffer in the correct format */
		char *ptr = (char*)(sendDataBufPtr->fBufferData);
		
		// 4 byte length & user name
		*((uInt32 *)ptr) = iNameLen;
		ptr += sizeof(uInt32);
		bcopy( pUsername, ptr, iNameLen );
		ptr += iNameLen;
		
		// 4 byte length & password
		*((uInt32 *)ptr) = iPassLen;
		ptr += sizeof(uInt32);
		bcopy( pPassword, ptr, iPassLen );
	
		DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to authenticate as provided user - %s", pUsername );

		if( (siResult = dsDoDirNodeAuth( dsNodeRef, pAuthType, false, sendDataBufPtr, responseDataBufPtr, 0)) != eDSNoErr )
		{
			DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Bind Request - Credentials for %s failed", pUsername );
			throw( (sInt32) siResult );
		}
		
		if( pAuthType != NULL )
		{
			::dsDataNodeDeAllocate( dsRef, pAuthType );
			pAuthType = NULL;
		}
		
		// let's try to open the Record
		tRecordReference	recRef = 0;
		
		pRecName = ::dsDataNodeAllocateString( dsRef, pComputerID );
		
		siResult = dsOpenRecord( dsNodeRef, pRecType, pRecName, &recRef );

		// if we didn't get an error, then we were able to open the record
		if( siResult == eDSNoErr )
		{
			if( inData->fInRequestCode == 204 || inData->fInRequestCode == 210 )
			{
				DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Bind Request - Existing computer record %s", pComputerID );
				throw( (sInt32) eDSRecordAlreadyExists );
			}
		}
		
		if( recRef == 0 )
		{
			// first let's create the computer record --- should computername have a "$" at the end?
			DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to Create computer record and open - %s", pComputerID );
			
			siResult = dsCreateRecordAndOpen( dsNodeRef, pRecType, pRecName, &recRef );
			
			if( siResult != eDSNoErr )
			{
				DBGLOG2( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d attempting to Create computer record and open - %s", siResult, pComputerID );
				throw( (sInt32) siResult );
			}
		}

		// let's set the Distinguished name
		tDataNodePtr	pAttrName = ::dsDataNodeAllocateString( dsRef, kDS1AttrDistinguishedName );

		DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to set Distinguished Name value - %s", pComputerID );
		
		::dsBuildListFromNodesAlloc( dsRef, &dataList, pRecName, NULL );

		siResult = ::dsSetAttributeValues( recRef, pAttrName, &dataList );
		
		::dsDataListDeallocatePriv( &dataList );
		::dsDataNodeDeAllocate( dsRef, pAttrName );
		pAttrName = NULL;
		
		if( siResult != eDSNoErr && siResult != eDSNoStdMappingAvailable )
		{
			DBGLOG2( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d attempting to set DistinguishedName value - %s", siResult, pComputerID );
			throw( (sInt32) siResult );
		}

		// Now let's set the OSVersion in the record... TBD
//		CFStringRef cfVersion = CFCopySystemVersionString();
//		if( cfVersion != NULL )
//		{
//			uInt32 iLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfVersion), kCFStringEncodingUTF8) + 1;
//			char *pVersion = (char *) calloc( sizeof(char), iLength );
//			CFStringGetCString( cfVersion, pVersion, iLength, kCFStringEncodingUTF8 );

//			CFRelease( cfVersion );
//			cfVersion = NULL;
						
//			pAttrValue = ::dsDataNodeAllocateString( dsRef, pVersion );
//			pAttrName = ::dsDataNodeAllocateString( dsRef, TBD );
			
//			siResult = ::dsAddAttributeValue( recRef, pAttrName, pAttrValue );
			
//			free( pVersion );
//			pVersion = NULL;

//			::dsDataNodeDeAllocate( dsRef, pAttrName );
//			pAttrName = NULL;
			
//			::dsDataNodeDeAllocate( dsRef, pAttrValue );
//			pAttrValue = NULL;
			
//			if( siResult != eDSNoErr && siResult != eDSNoStdMappingAvailable )
//			{
//				throw( (sInt32) siResult );
//			}			
//		}
		
		// now let's add a UUID to the record
		CFUUIDRef   cfUUID = CFUUIDCreate( kCFAllocatorDefault );
		if( cfUUID )
		{
			CFStringRef cfUUIDStr = CFUUIDCreateString( kCFAllocatorDefault, cfUUID );

			uInt32 iLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfUUIDStr), kCFStringEncodingUTF8) + 1;
			char *pUUID = (char *) calloc( sizeof(char), iLength );
			CFStringGetCString( cfUUIDStr, pUUID, iLength, kCFStringEncodingUTF8 );
			
			tDataNodePtr	pAttrName = ::dsDataNodeAllocateString( dsRef, kDS1AttrGeneratedUID );

			::dsBuildListFromStringsAlloc( dsRef, &dataList, pUUID, NULL );
			
			DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to set GeneratedUID value - %s", pUUID );
			
			siResult = ::dsSetAttributeValues( recRef, pAttrName, &dataList );
			
			::dsDataListDeallocatePriv( &dataList );
			
			::dsDataNodeDeAllocate( dsRef, pAttrName );
			pAttrName = NULL;
			
			CFRelease( cfUUID );
			cfUUID = NULL;
			
			CFRelease( cfUUIDStr );
			cfUUIDStr = NULL;
			
			free( pUUID );
			pUUID = NULL;

			if( siResult != eDSNoErr && siResult != eDSNoStdMappingAvailable )
			{
				DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d attempting to set GeneratedUID", siResult );
				throw( (sInt32) siResult );
			}
		}
		
		CFStringRef cfEnetAddr = NULL;
		if( inData->fInRequestCode == 210 || inData->fInRequestCode == 211 )
		{
			CFStringRef cfTempAddr = (CFStringRef) CFDictionaryGetValue( cfXMLDict, CFSTR(kDS1AttrENetAddress) );
			if( cfTempAddr != NULL && CFGetTypeID(cfTempAddr) == CFStringGetTypeID() )
			{
				CFRetain( cfTempAddr ); // need to retain cause we release it in a little while
				cfEnetAddr = cfTempAddr;
				DBGLOG( kLogPlugin, "CLDAPv3Plugin: Bind Request - Received MAC Address for bind other request" );
			}
			else
			{
				DBGLOG( kLogPlugin, "CLDAPv3Plugin: Bind Request - Did not receive MAC Address for bind other request" );
			}
			// remove it from the dictionary cause this isn't normally in the config...
			CFDictionaryRemoveValue( cfXMLDict, CFSTR(kDS1AttrENetAddress) );
		}
		else
		{
			GetMACAddress( &cfEnetAddr, NULL );
			DBGLOG( kLogPlugin, "CLDAPv3Plugin: Bind Request - Determined MAC Address from local host information" );
		}

		// Set the macAddress - CFSTR(kDS1AttrENetAddress) -- needs to be en0 - for Managed Client Settings
		if( cfEnetAddr )
		{
			uInt32 iLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfEnetAddr), kCFStringEncodingUTF8) + 1;
			char *pLinkAddr = (char *) calloc( sizeof(char), iLength );
			CFStringGetCString( cfEnetAddr, pLinkAddr, iLength, kCFStringEncodingUTF8 );

			tDataNodePtr	pAttrName = ::dsDataNodeAllocateString( dsRef, kDS1AttrENetAddress );

			::dsBuildListFromStringsAlloc( dsRef, &dataList, pLinkAddr, NULL );

			DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to set MAC Address - %s", pLinkAddr );

			siResult = ::dsSetAttributeValues( recRef, pAttrName, &dataList );
			
			::dsDataNodeDeAllocate( dsRef, pAttrName );
			pAttrName = NULL;

			::dsDataListDeallocatePriv( &dataList );

			DSFreeString( pLinkAddr );
			DSCFRelease( cfEnetAddr );
			
			if( siResult != eDSNoErr && siResult != eDSNoStdMappingAvailable )
			{
				DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d attempting to set MAC Address", siResult );
				throw( (sInt32) siResult );
			}
		}
		
		// Set the Password for the record - 20 characters long.. will be complex..
		const int iPassLength = 20;
		char	*pCompPassword = (char *) calloc( sizeof(char), iPassLength+1 );
		int		ii = 0;

		srandomdev();   // seed random from dev
		while( ii < iPassLength )
		{
			char cTemp = (char) (random() % 0x7f);
			
			// accept printable characters, but no spaces...
			if( isprint(cTemp) && !isspace(cTemp) )
			{
				pCompPassword[ii] = cTemp;
				ii++;
			}
		}
		
		pAuthType = ::dsDataNodeAllocateString( dsRef, kDSStdAuthSetPasswdAsRoot );
		
		char *pPtr = sendDataBufPtr->fBufferData;
		
		int len = strlen( pComputerID );
		*((long*)pPtr) = len;
		pPtr += sizeof(long);
		bcopy( pComputerID, pPtr, len );
		pPtr += len;
		
		len = strlen( pCompPassword );
		*((long*)pPtr) = len;
		pPtr += sizeof(long);
		bcopy( pCompPassword, pPtr, len );
		pPtr += len;
		
		sendDataBufPtr->fBufferLength = pPtr - sendDataBufPtr->fBufferData;
		
		DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Bind Request - Attempting to set Password for Computer account - %s", pComputerID );

		siResult = dsDoDirNodeAuthOnRecordType( dsNodeRef, pAuthType, true, sendDataBufPtr, responseDataBufPtr, 0, pRecType );
		if( siResult == eDSNoErr )
		{
			// make password CFDataRef from the beginning
			CFDataRef   cfPassword = CFDataCreate( kCFAllocatorDefault, (UInt8*)pCompPassword, strlen(pCompPassword) );
			CFStringRef cfQualifier = NULL;
			
			CFStringRef cfMapBase = (CFStringRef) CFDictionaryGetValue( cfComputerMap, CFSTR(kXMLSearchBase) );
			
			// let's get the record name qualifier..
			GetAttribFromRecordDict( cfComputerMap, CFSTR(kDSNAttrRecordName), NULL, &cfQualifier );
			
			// let's compose the fully qualified DN for the computerAccount
			CFStringRef cfDN = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%@=%@,%@"), cfQualifier, cfComputerName, cfMapBase );
			
			if( cfDN != NULL )
			{
				// success.. let's store the password in the configuration now..
				CFDictionarySetValue( cfXMLDict, CFSTR(kXMLSecureUseFlagKey), kCFBooleanTrue );
				CFDictionarySetValue( cfXMLDict, CFSTR(kXMLServerAccountKey), cfDN );
				CFDictionarySetValue( cfXMLDict, CFSTR(kXMLServerPasswordKey), cfPassword );
				CFDictionarySetValue( cfXMLDict, CFSTR(kXMLBoundDirectoryKey), kCFBooleanTrue );
				
				if( dsOpenRecord(dsNodeRef, pRecType, pRecName, &recRef) == eDSNoErr )
				{
					tDataNodePtr		pAuthAuthority = ::dsDataNodeAllocateString( dsRef, kDSNAttrAuthenticationAuthority );
					tAttributeEntryPtr  attribInfo = NULL;

					// let's get the KerberosID if one is present and set that as well
					if( dsGetRecordAttributeInfo(recRef, pAuthAuthority, &attribInfo) == eDSNoErr )
					{
						for( unsigned int ii = 1; ii <= attribInfo->fAttributeValueCount; ii++ )
						{
							tAttributeValueEntryPtr	authEntry = NULL;
							
							if( dsGetRecordAttributeValueByIndex( recRef, pAuthAuthority, ii, &authEntry ) == eDSNoErr )
							{
								if( strstr( authEntry->fAttributeValueData.fBufferData, kDSValueAuthAuthorityKerberosv5 ) )
								{
									char *pAuth = authEntry->fAttributeValueData.fBufferData;
									int ii = 0;
									
									while( strsep(&pAuth, ";") && ii++ < 2 );
									
									if( pAuth )
									{
										// one more time so we get the KerberosID itself..
										char *pKerbIDTemp = strsep( &pAuth, ";" );
										if( pKerbIDTemp && *pKerbIDTemp )
										{
											CFStringRef cfKerberosID = CFStringCreateWithCString( kCFAllocatorDefault, pKerbIDTemp, kCFStringEncodingUTF8 );
											CFDictionarySetValue( cfXMLDict, CFSTR(kXMLKerberosId), cfKerberosID );
											
											AddKeytabEntry( pKerbIDTemp, pCompPassword );

											CFRelease( cfKerberosID );
										}
									}
								}
								dsDeallocAttributeValueEntry( dsRef, authEntry );
								authEntry = NULL;
							}
						}
						dsDeallocAttributeEntry( dsRef, attribInfo );
						attribInfo = NULL;
					}
					dsDataNodeDeAllocate( dsRef, pAuthAuthority );
					pAuthAuthority = NULL;
				}
				
				CFRelease( cfDN );
				cfDN = NULL;
			}
			
			CFRelease( cfPassword );
			cfPassword = NULL;
		}
		else
		{
			DBGLOG2( kLogPlugin, "CLDAPv3Plugin: Bind Request - Error %d attempting to set Password for Computer account - %s", siResult, pComputerID );
		}
		
		if( pCompPassword != NULL )
		{
			free( pCompPassword );
			pCompPassword = NULL;
		}
		
		dsCloseRecord( recRef );
		
	} catch( sInt32 iError ) {
		siResult = iError;
	} catch( ... ) {
		// catch all for miss throws...
		siResult = eUndefinedError;
	}
	
	if( pRecName != NULL )
	{
		::dsDataNodeDeAllocate( dsRef, pRecName );
		pRecName = NULL;
	}
	
	if( pAuthType != NULL )
	{
		::dsDataNodeDeAllocate( dsRef, pAuthType );
		pAuthType = NULL;
	}
		
	if( sendDataBufPtr != NULL )
	{
		::dsDataBufferDeallocatePriv( sendDataBufPtr );
		sendDataBufPtr = NULL;
	}
	
	if( responseDataBufPtr != NULL )
	{
		::dsDataBufferDeallocatePriv( responseDataBufPtr );
		responseDataBufPtr = NULL;
	}
	
	DSCFRelease( cfComputerMap );
	
	if( pServer != NULL )
	{
		free( pServer );
		pServer = NULL;
	}
	
	DSFreeString( pUsername );
	
	if( pPassword != NULL )
	{
		memset( pPassword, 0, strlen(pPassword) );
		free( pPassword );
		pPassword = NULL;
	}
	
	DSFreeString( pComputerID );

	if( pRecType != NULL )
	{
		::dsDataNodeDeAllocate( dsRef, pRecType );
		pRecType = NULL;
	}

	if( dsNodeRef )
	{
		dsCloseDirNode( dsNodeRef );
		dsNodeRef = 0;
	}
	
	if( dsRef )
	{
		dsCloseDirService( dsRef );
		dsRef = 0;
	}

	if( cfXMLDict != NULL )
	{
		if( siResult == eDSNoErr )
		{
			sInt32 iError = PutXMLInBuffer( cfXMLDict, inData->fOutRequestResponse );
			if( iError != eDSNoErr ) {
				siResult = iError;
			}
		}
		
		CFRelease( cfXMLDict );
		cfXMLDict = NULL;
	}
	
	return siResult;
} // DoNewServerBind2

// ---------------------------------------------------------------------------
//	* DoNewServerSetup
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoNewServerSetup( sDoPlugInCustomCall *inData )
{
	sInt32					siResult			= eDSInvalidBuffFormat;
	CFMutableDictionaryRef  cfXMLDict			= NULL;
	CFMutableDictionaryRef  cfConfigXML			= NULL;
	CFDataRef				cfTempData			= NULL;
	CFIndex					iIndex				= 0;
	
	// we should always have XML data for this process, if we don't, throw an error
	cfXMLDict = GetXMLFromBuffer( inData->fInRequestData );
	if( cfXMLDict != NULL )
	{
		// We'll add the new config to the config dictionary
		cfTempData = gpConfigFromXML->CopyXMLConfig();
		
		cfConfigXML = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfTempData, kCFPropertyListMutableContainersAndLeaves, NULL );
		
		CFRelease( cfTempData );
		cfTempData = NULL;
		
		// let's grab the configured list of servers...
		CFMutableArrayRef cfConfigList = (CFMutableArrayRef) CFDictionaryGetValue( cfConfigXML, CFSTR(kXMLConfigArrayKey) );
		
		CFStringRef cfServer = GetServerInfoFromConfig( cfXMLDict, NULL, NULL, NULL, NULL, NULL );
		if( IsServerInConfig(cfConfigXML, cfServer, &iIndex, NULL) )
		{
			// let's swap the new config for the old one..
			CFArraySetValueAtIndex( cfConfigList, iIndex, cfXMLDict );
		}
		else
		{
			// let's be sure we have an array list, in particular for a fresh configuration
			if( cfConfigList == NULL )
			{
				cfConfigList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
				CFDictionarySetValue( cfConfigXML, CFSTR(kXMLConfigArrayKey), cfConfigList );
				CFRelease( cfConfigList ); // we can release since it was retained by the dictionary
			}
			// let's add this config to the configured servers..  if we complete, we'll
			CFArrayAppendValue( cfConfigList, cfXMLDict );
		}
		
		// first let's convert the password to a CFData type so it isn't in cleartext
		CFStringRef cfPassword = (CFStringRef) CFDictionaryGetValue( cfXMLDict, CFSTR(kXMLServerPasswordKey) );
		
		// if the password is a string convert it to a Data type
		if( cfPassword && CFGetTypeID(cfPassword) == CFStringGetTypeID() )
		{
			CFDataRef   cfPassData = CFStringCreateExternalRepresentation( kCFAllocatorDefault, cfPassword, kCFStringEncodingUTF8, 0 );
			CFDictionarySetValue( cfXMLDict, CFSTR(kXMLServerPasswordKey), cfPassData );
			CFRelease( cfPassData );
		}
		
		// now let's convert back to Data blob and save the new config...
		cfTempData = (CFDataRef) CFPropertyListCreateXMLData( kCFAllocatorDefault, cfConfigXML );

		// we're done with cfConfigXML now..
		CFRelease( cfConfigXML );
		cfConfigXML = NULL;
		
		// Technically the username already has a password, even though it is admin, no one else knows about this temporary node
		siResult = gpConfigFromXML->SetXMLConfig( cfTempData );

		// we're done with cfTempData
		CFRelease( cfTempData );
		cfTempData = NULL;
		
		// let's re-initialize ourselves to add the new config
		Initialize();
	}

	// let's return it because we may have updated it
	if( cfXMLDict != NULL )
	{
		if( siResult == eDSNoErr )
		{
			sInt32 iError = PutXMLInBuffer( cfXMLDict, inData->fOutRequestResponse );
			if( iError != eDSNoErr ) {
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

sInt32 CLDAPv3Plugin::DoRemoveServer( sDoPlugInCustomCall *inData )
{
	sInt32					siResult				= eDSNoErr;
	CFDictionaryRef			cfXMLDict				= NULL; // this is the inbound XML dictionary
	CFDictionaryRef			cfConfigDict			= NULL; // this is our current configuration XML data
	CFMutableDictionaryRef  cfServerDict			= NULL; // this is the actual server we're trying to unconfigure
	CFIndex					iConfigIndex			= 0;
	char					*pServer				= NULL;
	char					*pUsername				= NULL;
	char					*pPassword				= NULL;
	char					*pComputerID			= NULL;
	tDirReference			dsRef					= 0;
	tDirNodeReference		dsNodeRef				= 0;
	tDataNodePtr			pRecType				= ::dsDataNodeAllocateString( dsRef, kDSStdRecordTypeComputers );;
	tDataBufferPtr			responseDataBufPtr		= ::dsDataBufferAllocatePriv( 1024 );
	tDataBufferPtr			sendDataBufPtr			= ::dsDataBufferAllocatePriv( 1024 );
	tDataNodePtr			pAuthType				= NULL;
	tDataNodePtr			pRecName				= NULL;
	CFMutableArrayRef		cfConfigList			= NULL; // no need to release, temporary variable
	
	try
	{
		// we should always have XML data for this process, if we don't, throw an error
		cfXMLDict = GetXMLFromBuffer( inData->fInRequestData );
		if( cfXMLDict == NULL ) {
			throw( (sInt32) eDSInvalidBuffFormat );
		}
		
		// automatically released with cfXMLDict release, just a Get.
		CFStringRef cfServer = GetServerInfoFromConfig( cfXMLDict, &pServer, NULL, NULL, &pUsername, &pPassword );
		if( cfServer == NULL )
		{
			throw( (sInt32) eDSInvalidBuffFormat );
		}

		// now let's find the server we're trying to unbind from in the config
		CFDataRef cfTempData = gpConfigFromXML->CopyXMLConfig();
		
		cfConfigDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfTempData, kCFPropertyListMutableContainersAndLeaves, NULL );

		cfConfigList = (CFMutableArrayRef) CFDictionaryGetValue( cfConfigDict, CFSTR(kXMLConfigArrayKey) );

		// we're done with the TempData at this point
		CFRelease( cfTempData );
		cfTempData = NULL;
		
		if( IsServerInConfig( cfConfigDict, cfServer, &iConfigIndex, &cfServerDict ) == false )
		{
			throw( (sInt32) eDSBogusServer );
		}

		CFBooleanRef	cfBound = (CFBooleanRef) CFDictionaryGetValue( cfServerDict, CFSTR(kXMLBoundDirectoryKey) );
		bool			bBound = false;
		
		if( cfBound != NULL && CFGetTypeID(cfBound) == CFBooleanGetTypeID() )
		{
			bBound = CFBooleanGetValue( cfBound );
		}
		
		// if we are bound, we need to unbind first, then we can delete the config and re-initialize
		if( bBound )
		{
			// well, if we don't have credentials, we won't be able to continue here..
			if( pUsername == NULL || pPassword == NULL )
			{
				throw( (sInt32) eDSAuthParameterError );
			}
			
			char *pComputerDN = NULL;
			GetServerInfoFromConfig( cfServerDict, NULL, NULL, NULL, &pComputerDN, NULL );
			
			// shouldn't happen but a safety
			if( pComputerDN == NULL )
			{
				throw( (sInt32) eDSBogusServer );
			}

			char **pDN = ldap_explode_dn( pComputerDN, 1 );
			
			if( pDN != NULL )
			{
				pComputerID = strdup( pDN[0] );
				ldap_value_free( pDN );
				pDN = NULL;
			}
			
			free( pComputerDN );
			pComputerDN = NULL;
			
			// let's build our path....
			siResult = ::dsOpenDirService( &dsRef );
			if( siResult != eDSNoErr )
			{
				throw( siResult );
			}
			
			char	*pNodeName = (char *) calloc(1, strlen(pServer)+10);  // add 10 characters for null and 1 pad for "/LDAPv3/"
			
			strcpy( pNodeName, "/LDAPv3/" );
			strcat( pNodeName, pServer );
			
			tDataList	*nodeString = ::dsBuildFromPathPriv( pNodeName, "/" );
			
			delete pNodeName;
			pNodeName = NULL;
			
			siResult = ::dsOpenDirNode( dsRef, nodeString, &dsNodeRef );
			
			::dsDataListDeallocatePriv( nodeString );
			free( nodeString );
			nodeString = NULL;
			
			// if we are forcibly unbinding and we have a failure, throw noError and just delete it
			if( siResult == eDSOpenNodeFailed && inData->fInRequestCode == 208 )
			{
				throw( (sInt32) eDSNoErr );
			}
			else if( siResult != eDSNoErr )
			{
				throw( siResult );
			}
			
			uInt32			iNameLen		= strlen( pUsername );
			uInt32			iPassLen		= strlen( pPassword );
			uInt32			authDataSize	= iNameLen + iPassLen + (2 * sizeof(uInt32));
			
			pAuthType   = ::dsDataNodeAllocateString( dsRef, kDSStdAuthNodeNativeNoClearText );
			
			sendDataBufPtr->fBufferLength = authDataSize;
			
			/* store user name and password into the auth buffer in the correct format */
			char *ptr = (char*)(sendDataBufPtr->fBufferData);
			
			// 4 byte length & user name
			*((uInt32 *)ptr) = iNameLen;
			ptr += sizeof(uInt32);
			bcopy( pUsername, ptr, iNameLen );
			ptr += iNameLen;
			
			// 4 byte length & password
			*((uInt32 *)ptr) = iPassLen;
			ptr += sizeof(uInt32);
			bcopy( pPassword, ptr, iPassLen );
			
			if( (siResult = dsDoDirNodeAuth( dsNodeRef, pAuthType, false, sendDataBufPtr, responseDataBufPtr, 0)) != eDSNoErr )
			{
				throw( (sInt32) siResult );
			}
			
			if( pAuthType != NULL )
			{
				::dsDataNodeDeAllocate( dsRef, pAuthType );
				pAuthType = NULL;
			}
			
			// let's try to open the Record
			tRecordReference	recRef = 0;
			
			pRecName = ::dsDataNodeAllocateString( dsRef, pComputerID );
			
			siResult = dsOpenRecord( dsNodeRef, pRecType, pRecName, &recRef );
			
			// if we didn't get an error, then we were able to open the record
			if( siResult == eDSNoErr )
			{
				// let's delete the existing cause we are doing Join/replace
				sendDataBufPtr->fBufferLength = sizeof( recRef );
				bcopy( &recRef, sendDataBufPtr->fBufferData, sizeof(recRef) );

				// we use a custom call instead of doing a normal delete to ensure any PWS information is deleted as well
				siResult = ::dsDoPlugInCustomCall( dsNodeRef, eRecordDeleteAndCredentials, sendDataBufPtr, responseDataBufPtr );
				
				if( siResult != eDSNoErr )
				{
					if( inData->fInRequestCode == 207 ) // not a force unbind, so throw the appropriate error
					{
						throw( (sInt32) siResult );
					}
				}
			}
			
			if( inData->fInRequestCode == 208 ) // if this is force unbind, we will reset to no error
			{
				siResult = eDSNoErr;
			}
			else
			{
				throw( siResult );
			}
		}
		
		siResult = eDSNoErr;
		
	} catch( sInt32 iError ) {
		siResult = iError;
	} catch( ... ) {
		// catch all for miss throws...
		siResult = eUndefinedError;
	}
	
	// if we are removing the configuration, just remove it here and update our configuration.
	if( siResult == eDSNoErr )
	{
		// remove the kerberos principal from the keytab
		CFStringRef kerberosID = (CFStringRef) CFDictionaryGetValue( cfServerDict, CFSTR(kXMLKerberosId) );
		char *pKerberosID = NULL;
		
		if (kerberosID != NULL && CFStringGetLength(kerberosID) > 0)
		{
			uInt32 callocLength = (uInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(kerberosID), kCFStringEncodingUTF8) + 1;
			pKerberosID = (char *) calloc( 1, callocLength );
			CFStringGetCString( kerberosID, pKerberosID, callocLength, kCFStringEncodingUTF8 );
		}
		
		if( pKerberosID != NULL )
		{
			krb5_context	krbContext  = NULL;
			krb5_principal	principal	= NULL;
			krb5_ccache		krbCache	= NULL;
			char			*principalString = NULL;
			char			*pCacheName	= NULL;
			
			RemoveKeytabEntry( pKerberosID );

			int retval = krb5_init_context( &krbContext );
			if( 0 == retval )
				retval = krb5_parse_name( krbContext, pKerberosID, &principal );

			if( 0 == retval )
				retval = krb5_unparse_name( krbContext, principal, &principalString );
			
			if( 0 == retval )
			{
				pCacheName = (char *) malloc( strlen(principalString) + sizeof("MEMORY:") + 1 );
				strcpy( pCacheName, "MEMORY:" );
				strcat( pCacheName, principalString );

				retval = krb5_cc_resolve( krbContext, pCacheName, &krbCache );
			}

			if( NULL != principal )
			{
				krb5_free_principal( krbContext, principal );
				principal = NULL;
			}
			
			// now let's destroy any cache that may exist, clearing memory
			if( NULL != krbCache )
			{
				krb5_cc_destroy( krbContext, krbCache );
				krbCache = NULL;
				DBGLOG1( kLogPlugin, "CLDAPv3Plugin: Destroying kerberos Memory cache for %s", principalString );
			}
			
			if( NULL != krbContext )
			{
				krb5_free_context( krbContext );
				krbContext = NULL;
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
		if( inData->fInRequestCode == 209 )
		{
			// remove the value at the index we saved.. then stuff it back and reset the configuration.
			CFArrayRemoveValueAtIndex( cfConfigList, iConfigIndex );
		}
		
		CFDataRef cfTempData = (CFDataRef) CFPropertyListCreateXMLData( kCFAllocatorDefault, cfConfigDict );
		
		// Technically the username already has a password, even though it is admin, no one else knows about this temporary node
		siResult = gpConfigFromXML->SetXMLConfig( cfTempData );
		
		// we're done with cfTempData
		CFRelease( cfTempData );
		cfTempData = NULL;
		
		// let's re-initialize ourselves to add the new config
		Initialize();		
	}
	
	if( pRecName != NULL )
	{
		::dsDataNodeDeAllocate( dsRef, pRecName );
		pRecName = NULL;
	}
	
	if( pAuthType != NULL )
	{
		::dsDataNodeDeAllocate( dsRef, pAuthType );
		pAuthType = NULL;
	}
	
	if( sendDataBufPtr != NULL )
	{
		::dsDataBufferDeallocatePriv( sendDataBufPtr );
		sendDataBufPtr = NULL;
	}
	
	if( responseDataBufPtr != NULL )
	{
		::dsDataBufferDeallocatePriv( responseDataBufPtr );
		responseDataBufPtr = NULL;
	}
	
	DSFreeString( pServer );
	DSFreeString( pUsername );
	
	if( pPassword != NULL )
	{
		memset( pPassword, 0, strlen(pPassword) );
		free( pPassword );
		pPassword = NULL;
	}
	
	DSFreeString( pComputerID );
	
	if( pRecType != NULL )
	{
		::dsDataNodeDeAllocate( dsRef, pRecType );
		pRecType = NULL;
	}
	
	if( dsNodeRef )
	{
		dsCloseDirNode( dsNodeRef );
		dsNodeRef = 0;
	}
	
	if( dsRef )
	{
		dsCloseDirService( dsRef );
		dsRef = 0;
	}
	
	if( cfServerDict != NULL )
	{
		// let's put the new cfServerDictionary in the return buffer..
		if( siResult == eDSNoErr )
		{
			sInt32 iError = PutXMLInBuffer( cfServerDict, inData->fOutRequestResponse );
			if( iError != eDSNoErr ) {
				siResult = iError;
			}
		}

		CFRelease( cfServerDict );
		cfServerDict = NULL;
	}
	
	DSCFRelease( cfConfigDict );
	DSCFRelease( cfXMLDict );
	
	return siResult;
} // DoRemoveServer

#pragma mark -
#pragma mark Other functions

// ---------------------------------------------------------------------------
//	* ContextDeallocProc
// ---------------------------------------------------------------------------

void CLDAPv3Plugin::ContextDeallocProc ( void* inContextData )
{
	sLDAPContextData *pContext = (sLDAPContextData *) inContextData;

	if ( pContext != nil )
	{
		delete pContext;
		pContext = nil;
	}
} // ContextDeallocProc

// ---------------------------------------------------------------------------
//	* ContinueDeallocProc
// ---------------------------------------------------------------------------

void CLDAPv3Plugin::ContinueDeallocProc ( void* inContinueData )
{
	sLDAPContinueData *pLDAPContinue = (sLDAPContinueData *)inContinueData;
	uInt32 refNum = 0;
	sLDAPContextData *pContext = nil;
	LDAP *aHost = nil;
	int rc = 0;

	if ( pLDAPContinue != nil )
	{
        // remember ldap_msgfree( pLDAPContinue->pResult ) will remove the LDAPMessage
        if (pLDAPContinue->pResult != nil)
        {
        	ldap_msgfree( pLDAPContinue->pResult );
	        pLDAPContinue->pResult = nil;
        }
		
		if (pLDAPContinue->msgId != 0)
		{
			refNum = gLDAPContinueTable->GetRefNumForItem( inContinueData );
			pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( refNum );
			if (pContext != nil) 
			{
				aHost = fLDAPSessionMgr.LockSession(pContext);
				if (aHost != nil)
				{
					rc = ldap_abandon(aHost,pLDAPContinue->msgId);
					pLDAPContinue->msgId = 0;
				}
				fLDAPSessionMgr.UnLockSession(pContext,false);
			}
		}

		free( pLDAPContinue );
		pLDAPContinue = nil;
	}
} // ContinueDeallocProc


// ---------------------------------------------------------------------------
//	* RebindLDAPSession
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin:: RebindLDAPSession ( sLDAPContextData *inContext, CLDAPNode& inLDAPSessionMgr )
{
    sInt32			siResult	= eDSNoErr;

	if (inContext != nil)
	{
		siResult = inLDAPSessionMgr.RebindSession( inContext->fLDAPNodeStruct );
	}
	else
	{
		siResult = eDSNullParameter;
	}

	return (siResult);
	
}// RebindLDAPSession

//------------------------------------------------------------------------------------
//	* GetRecRefLDAPMessage
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetRecRefLDAPMessage ( sLDAPContextData *inRecContext, int &ldapMsgId, char **inAttrs, LDAPMessage **outResultMsg )
{
	sInt32				siResult		= eDSNoErr;
    int					searchTO   		= 0;
	int					numRecTypes		= 1;
	char			   *pLDAPRecType	= nil;
	bool				bResultFound	= false;
    char			   *queryFilter		= nil;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	bool				bOCANDGroup		= false;
	CFArrayRef			OCSearchList	= nil;
	ber_int_t			scope			= LDAP_SCOPE_SUBTREE;
	sInt32				searchResult	= eDSNoErr;

	try
	{
	
		//TODO would actually like to use the fOpenRecordDN instead here as it might reduce the effort in the search?
		//ie. we will likely already have it
		if ( inRecContext->fOpenRecordName  == nil ) throw( (sInt32)eDSNullRecName );
		if ( inRecContext->fOpenRecordType  == nil ) throw( (sInt32)eDSNullRecType );

		//search for the specific LDAP record now
		
		//retrieve the config data
		sLDAPConfigData *pConfig = gpConfigFromXML->ConfigWithNodeNameLock( inRecContext->fNodeName );
		if (pConfig != nil)
		{
			searchTO	= pConfig->fSearchTimeout;
			gpConfigFromXML->ConfigUnlock( pConfig );
			pConfig = NULL;
		}
		
        // we will search over all the rectype mappings until we find the first
        // result for the search criteria in the queryfilter
        numRecTypes = 1;
		bOCANDGroup = false;
		if (OCSearchList != nil)
		{
			CFRelease(OCSearchList);
			OCSearchList = nil;
		}
		pLDAPRecType = MapRecToLDAPType( (const char *)(inRecContext->fOpenRecordType), inRecContext->fNodeName, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
		//only throw this for first time since we need at least one map
		if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
		
		while ( (pLDAPRecType != nil) && (!bResultFound) )
		{

			//build the record query string
			//removed the use well known map only condition ie. true to false
			queryFilter = BuildLDAPQueryFilter(	(char *)kDSNAttrRecordName, //TODO can we use dn here ie native type
												inRecContext->fOpenRecordName,
												eDSExact,
												inRecContext->fNodeName,
												false,
												(const char *)(inRecContext->fOpenRecordType),
												pLDAPRecType,
												bOCANDGroup,
												OCSearchList );
			if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );

			searchResult = DSDoRetrieval (	pLDAPRecType,
											inAttrs,
											inRecContext,
											scope,
											fLDAPSessionMgr,
											queryFilter,
											ldapReturnCode,
											searchTO,
											result,
											bResultFound,
											ldapMsgId );

			if ( queryFilter != nil )
			{
				delete( queryFilter );
				queryFilter = nil;
			}

			if (pLDAPRecType != nil)
			{
				delete (pLDAPRecType);
				pLDAPRecType = nil;
			}
			numRecTypes++;
			bOCANDGroup = false;
			if (OCSearchList != nil)
			{
				CFRelease(OCSearchList);
				OCSearchList = nil;
			}
			pLDAPRecType = MapRecToLDAPType( (const char *)(inRecContext->fOpenRecordType), inRecContext->fNodeName, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
		} // while ( (pLDAPRecType != nil) && (!bResultFound) )

		if (	(bResultFound) &&
				( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
		{
			siResult = eDSNoErr;
		} // if bResultFound and ldapReturnCode okay
		else
		{
	     	siResult = searchResult;
			if ( result != nil )
			{
				ldap_msgfree( result );
				result = nil;
			}
		}
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	DSCFRelease(OCSearchList);
	DSFreeString(pLDAPRecType);
	DSFreeString(queryFilter);
	
	if (result != nil)
	{
		*outResultMsg = result;
	}

	return( siResult );

} // GetRecRefLDAPMessage


// ---------------------------------------------------------------------------
//	* GetLDAPv3AuthAuthorityHandler
// ---------------------------------------------------------------------------

LDAPv3AuthAuthorityHandlerProc CLDAPv3Plugin::GetLDAPv3AuthAuthorityHandler ( const char* inTag )
{
	if (inTag == NULL)
	{
		return NULL;
	}
	for (unsigned int i = 0; i < kLDAPv3AuthAuthorityHandlerProcs; ++i)
	{
		if (strcasecmp(inTag,sLDAPv3AuthAuthorityHandlerProcs[i].fTag) == 0)
		{
			// found it
			return sLDAPv3AuthAuthorityHandlerProcs[i].fHandler;
		}
	}
	return NULL;
}


// --------------------------------------------------------------------------------
//	* PeriodicTask ()
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::PeriodicTask ( void )
{
	if ( HandlingNetworkTransition() == false )
		fLDAPSessionMgr.CheckIdles();
	
	return( eDSNoErr );
} // PeriodicTask

//------------------------------------------------------------------------------------
//	* HandleMultipleNetworkTransitionsForLDAP
//------------------------------------------------------------------------------------

void CLDAPv3Plugin::HandleMultipleNetworkTransitionsForLDAP ( void )
{
	void	   *ptInfo		= nil;
	
	//let us be smart about doing the check
	//we would like to wait a short period for the Network transitions to subside
	//since we don't want to re-init multiple times during this wait period
	//however we do go ahead and fire off timers each time
	//each call in here we update the delay time by 5 seconds
	fTimeToHandleNetworkTransition = dsTimestamp() + USEC_PER_SEC*5;
	DBGLOG1( kLogPlugin, "T[%X] CLDAPv3Plugin::HandleMultipleNetworkTransitionsForLDAP, setting fHandlingNetworkTransition to true", pthread_self() );
	fHandlingNetworkTransition = true;
	
	if (fServerRunLoop != nil)
	{
		ptInfo = (void *)this;
		CFRunLoopTimerContext c = {0, (void*)ptInfo, NULL, NULL, NetworkChangeLDAPPICopyStringCallback};
	
		CFRunLoopTimerRef timer = CFRunLoopTimerCreate(	NULL,
														CFAbsoluteTimeGetCurrent() + 5,
														0,
														0,
														0,
														DoLDAPPINetworkChange,
														(CFRunLoopTimerContext*)&c);
	
		CFRunLoopAddTimer(fServerRunLoop, timer, kCFRunLoopDefaultMode);
		if (timer) CFRelease(timer);
	}
} // HandleMultipleNetworkTransitionsForLDAP


//------------------------------------------------------------------------------------
//	* ReInitForNetworkTransition
//------------------------------------------------------------------------------------

void CLDAPv3Plugin::ReInitForNetworkTransition ( void )
{
	if ( dsTimestamp() >= fTimeToHandleNetworkTransition && fHandlingNetworkTransition )
	{
		Initialize();
		//NetTransistion is already called within Initialize so no need to call it here
		//fLDAPSessionMgr.NetTransition();
		DBGLOG1( kLogPlugin, "T[%X] CLDAPv3Plugin::ReInitForNetworkTransition, setting fHandlingNetworkTransition to false", pthread_self() );
		fHandlingNetworkTransition = false;
		
	}

}// ReInitForNetworkTransition

//------------------------------------------------------------------------------------
//	* WaitForNetworkTransitionToFinishWithFunctionName
//------------------------------------------------------------------------------------

void CLDAPv3Plugin::WaitForNetworkTransitionToFinishWithFunctionName( const char* callerFunction )
{
	if ( fHandlingNetworkTransition )
	{
		DSSemaphore				timedWait;
		double					waitTime	= dsTimestamp() + USEC_PER_SEC*30;

		DBGLOG2( kLogPlugin, "T[%X] %s called, waiting for WaitForNetworkTransitionToFinish", pthread_self(), callerFunction );
		while( fHandlingNetworkTransition )
		{
			// Check every .5 seconds
			timedWait.Wait( (uInt32)(.5 * kMilliSecsPerSec) );

			// check over max
			if ( dsTimestamp() > waitTime )
			{
				// We have waited as long as we are going to at this time
				DBGLOG2( kLogPlugin, "T[%X] %s called, waited as long as we are going to for WaitForNetworkTransitionToFinish", pthread_self(), callerFunction );
				break;
			}
		}
		DBGLOG2( kLogPlugin, "T[%X] %s called, finished WaitForNetworkTransitionToFinish", pthread_self(), callerFunction );
	}
}// WaitForNetworkTransitionToFinish

//------------------------------------------------------------------------------------
//	* MappingNativeVariableSubstitution
//------------------------------------------------------------------------------------

char* CLDAPv3Plugin::MappingNativeVariableSubstitution(	char			   *inLDAPAttrType,
														sLDAPContextData   *inContext,
														LDAPMessage		   *inResult,
														sInt32&				outResult)
{
	char			   *returnStr					= nil;
	char			   *tmpStr						= nil;
	char			   *vsPtr						= nil;
	char			  **tokens						= nil;
	char			  **tokenValues					= nil;
	struct berval     **vsBValues;
	int					tokenCount					= 0;
	int					returnStrCount				= 0;
	int					vsShift						= 0;
	LDAP			   *aHost						= nil;
	int					tokenStep					= 0;
	int					index						= 0;
	char			   *lastPtr						= nil;
	const char			delimiterChar				= '$';
	const char			literalChar					= '#';
	const char			nullChar					= '\0';
	int					tokensMax					= 0;
	
	
	outResult = eDSNoErr;
	
	if ( (inLDAPAttrType != nil) && (*inLDAPAttrType == literalChar) && ( strchr(inLDAPAttrType, delimiterChar) != nil) )
	{
		// found a variable substitution marker...
		
		returnStrCount = strlen(inLDAPAttrType);
		
		// copy the literal mapping since we do destructive parsing
		vsPtr = (char *)calloc(1, 1+returnStrCount);
		memcpy(vsPtr, inLDAPAttrType, returnStrCount);
		
		tmpStr = vsPtr; //needed to free the memory below
		
		//setup the max size for the tokens arrays
		while (vsPtr != nil)
		{
			tokensMax++;
			vsPtr = strchr(vsPtr, delimiterChar);
			if (vsPtr != nil)
			{
				vsPtr = vsPtr + 1;
			}
		}
		tokens		= (char **)calloc(sizeof(char *), tokensMax+1);
		tokenValues = (char **)calloc(sizeof(char *), tokensMax+1);
		
		vsPtr = tmpStr; //reset the ptr

		while ( (vsPtr != nil) && (*vsPtr != nullChar) )
		{
			if ( *vsPtr == delimiterChar )
			{
				//this should be the start of a variable substitution
				*vsPtr = nullChar;
				vsPtr = vsPtr + 1;
				if (*vsPtr != nullChar)
				{
					lastPtr = strchr(vsPtr, delimiterChar);
					//attribute value must be alphanumeric or - or ; according to RFC2252
					//also check for consecutive delimiters which represents a single delimiter
					if (lastPtr != nil)
					{
						tokens[tokenCount] = vsPtr;
						if (vsPtr != lastPtr)
						{
							//case of true variable substitution
							*lastPtr = nullChar;
						}
						else
						{
							tokenValues[tokenCount] = strdup("$");
						}
						tokenCount++;
						vsPtr = lastPtr + 1;
					}
					else
					{
						//invalid mapping format ie. no matching delimiter pair
						outResult = eDSInvalidNativeMapping;
						break;
					}
				}
				else
				{
					//invalid mapping format ie. nothing following delimiter
					outResult = eDSInvalidNativeMapping;
					break;
				}
			}
			else
			{
				//we have literal text so we leave it alone
				tokens[tokenCount] = vsPtr;
				tokenValues[tokenCount] = vsPtr;
				tokenCount++;
				while ( ( *vsPtr != nullChar ) && ( *vsPtr != delimiterChar ) )
				{
					vsPtr = vsPtr + 1;
				}
			}
		}
		
		if (outResult == eDSNoErr)
		{
			aHost = fLDAPSessionMgr.LockSession(inContext);
			
			if (aHost != nil)
			{
				for ( tokenStep = 0; tokenStep < tokenCount; tokenStep++)
				{
					if (tokenValues[tokenStep] == nil)
					{
						//choose first value only for the substitution
						vsBValues = ldap_get_values_len(aHost, inResult, tokens[tokenStep]);
						if (vsBValues != nil)
						{
							returnStrCount += vsBValues[0]->bv_len; //a little extra since variable name was already included
							tokenValues[tokenStep] = (char *) ::calloc(vsBValues[0]->bv_len, sizeof(char));
							::memcpy(tokenValues[tokenStep], vsBValues[0]->bv_val, vsBValues[0]->bv_len);
							ldap_value_free_len(vsBValues);
							vsBValues = nil;
						}
					} //not a delimiter char
				} // for step through tokens
			}
			
			fLDAPSessionMgr.UnLockSession(inContext);
			
			returnStr = (char *) ::calloc(1+returnStrCount, sizeof(char));
			
			for ( tokenStep = 0; tokenStep < tokenCount; tokenStep++)
			{
				if (tokenValues[tokenStep] != nil)
				{
					memcpy((returnStr + vsShift), tokenValues[tokenStep], (::strlen(tokenValues[tokenStep])));
					vsShift += strlen(tokenValues[tokenStep]);
				}
			}
			
			for (index = 0; index < tokensMax; index++)
			{
				if ( (tokenValues[index] != nil) && ( tokenValues[index] != tokens[index] ) )
				{
					free(tokenValues[index]);
					tokenValues[index] = nil;
				}
			}
		}
		free(tmpStr);
		tmpStr = nil;
		free(tokens);
		free(tokenValues);
	}
	
	return(returnStr);
	
} // MappingNativeVariableSubstitution

//------------------------------------------------------------------------------------
//	* BuildEscapedRDN
//------------------------------------------------------------------------------------

char* CLDAPv3Plugin::BuildEscapedRDN( const char *inLDAPRDN )
{
	char	   *outLDAPRDN		= nil;
	uInt32		recNameLen		= 0;
	uInt32		originalIndex	= 0;
	uInt32		escapedIndex	= 0;

	if (inLDAPRDN != nil)
	{
		recNameLen = strlen(inLDAPRDN);
		outLDAPRDN = (char *)::calloc(1, 2 * recNameLen + 1);
		// assume at most all characters will be escaped
		while (originalIndex < recNameLen)
		{
			switch (inLDAPRDN[originalIndex])
			{
				case ' ':
					outLDAPRDN[escapedIndex] = '\\';
					++escapedIndex;
					outLDAPRDN[escapedIndex] = inLDAPRDN[originalIndex];
					++escapedIndex;
					break;
				case '#':
					if (originalIndex == 0 )
					{
						outLDAPRDN[escapedIndex] = '\\';
						++escapedIndex;
						outLDAPRDN[escapedIndex] = inLDAPRDN[originalIndex];
						++escapedIndex;
					}
					break;
				case ',':
				case '+':
				case '"':
				case '\\':
				case '<':
				case '>':
				case ';':
					outLDAPRDN[escapedIndex] = '\\';
					++escapedIndex;
					//fall thru to complete these escaped cases
				default:
					outLDAPRDN[escapedIndex] = inLDAPRDN[originalIndex];
					++escapedIndex;
					break;
			}
			++originalIndex;
		}
	}
	return(outLDAPRDN);
} //BuildEscapedRDN


//------------------------------------------------------------------------------------
//	* GetPWSIDforRecord
//------------------------------------------------------------------------------------

char *CLDAPv3Plugin::GetPWSIDforRecord( sLDAPContextData *pContext, char *inRecName, char *inRecType )
{
	char			**aaArray   = NULL;
	unsigned long   aaCount		= 0;
	char			*pUserID	= NULL;
	
	int siResult = GetAuthAuthority( pContext, inRecName, fLDAPSessionMgr, &aaCount, &aaArray, inRecType );
	
	if ( siResult == eDSNoErr ) 
	{
		// loop through all possibilities and find the password server authority
		for ( unsigned long idx = 0; idx < aaCount; idx++ )
		{
			char	*aaVersion  = NULL;
			char	*aaTag		= NULL;
			char	*aaData		= NULL;
			
			//parse this value of auth authority
			siResult = ParseAuthAuthority( aaArray[idx], &aaVersion, &aaTag, &aaData );
			
			if( aaTag != NULL && strcasecmp( aaTag, kDSTagAuthAuthorityPasswordServer ) == 0 )
			{
				// we don't free this cause we're going to use it later..
				pUserID = aaData;
				aaData = NULL;
			}
			
			DSFreeString( aaArray[idx] )
			DSFreeString( aaVersion );
			DSFreeString( aaTag );
			DSFreeString( aaData );
		}
		
		DSFree( aaArray );
	}
	
	return pUserID;
} // GetPWSIDforRecord
