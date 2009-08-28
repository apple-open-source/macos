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
#include <CommonCrypto/CommonDigest.h>

#include <Security/Authorization.h>
#include <PasswordServer/AuthFile.h>
#include <SystemConfiguration/SCDynamicStoreCopyDHCPInfo.h>
#include <CoreFoundation/CFPriv.h>

#include "DirServices.h"
#include "DirServicesUtils.h"
#include "DirServicesConst.h"
#include "DirServicesConstPriv.h"
#include "DirServicesPriv.h"
#include "LDAPv3SupportFunctions.h"

extern "C" {
	#include <sasl/saslutil.h>
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
#include "CPlugInObjectRef.h"

#pragma mark -
#pragma mark Defines and Macros

#define LDAPURLOPT95SEPCHAR			" "
#define LDAPCOMEXPSPECIALCHARS		"()|&"

#define kSpaceForPreferredKeyValuePair	1024

#define kKDCRecordRealmToken			"[realms]"
#define kKDCEndOfRealmNameMarker		"= {"

#pragma mark -
#pragma mark Globals and Structures

extern bool					gServerOS;
extern DSMutexSemaphore		*gKerberosMutex;
extern CContinue			*gLDAPContinueTable;
extern CPlugInObjectRef<sLDAPContextData *>	*gLDAPContextTable;
extern int					gPWSReplicaListLoadedSeqNumber;
extern int					gConfigRecordChangeSeqNumber;
extern char					*gPWSReplicaListLastRSAStr;
extern tDataBufferPtr		gPWSReplicaListXMLBuffer;
extern DSMutexSemaphore		gPWSReplicaListXMLBufferMutex;

#pragma mark -
#pragma mark Prototypes

static int standard_password_replace( LDAP *ld, char *dn, char *oldPwd, char *newPwd );
static int standard_password_create( LDAP *ld, char *dn, char *newPwd );
static int exop_password_create( LDAP *ld, char *dn, char *newPwd );

void DSGetExtendedLDAPResult (	LDAP *inHost,
								int inSearchTO,
								int &inLDAPMsgId,
								int &inLDAPReturnCode);

#pragma mark -
#pragma mark Support Functions

static int standard_password_replace( LDAP *ld, char *dn, char *oldPwd, char *newPwd )
{
	char *pwdDelete[2] = {};
	pwdDelete[0] = oldPwd;
	pwdDelete[1] = NULL;
	LDAPMod modDelete = {};
	modDelete.mod_op = LDAP_MOD_DELETE;
	modDelete.mod_type = (char *)"userPassword";
	modDelete.mod_values = pwdDelete;
	char *pwdCreate[2] = {};
	pwdCreate[0] = newPwd;
	pwdCreate[1] = NULL;
	LDAPMod modAdd = {};
	modAdd.mod_op = LDAP_MOD_ADD;
	modAdd.mod_type = (char *)"userPassword";
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
	modReplace.mod_type = (char *)"userPassword";
	modReplace.mod_values = pwdCreate;
	
	LDAPMod *mods[2] = {};
	mods[0] = &modReplace;
	mods[1] = NULL;

	return ldap_modify_s( ld, dn, mods );
}

static int exop_password_create( LDAP *ld, char *dn, char *newPwd )
{
	BerElement *ber = ber_alloc_t( LBER_USE_DER );
	if ( ber == NULL ) return( (SInt32) eMemoryError );
	ber_printf( ber, "{" /*}*/ );
	ber_printf( ber, "ts", LDAP_TAG_EXOP_MODIFY_PASSWD_ID, dn );
	ber_printf( ber, "ts", LDAP_TAG_EXOP_MODIFY_PASSWD_NEW, newPwd );
	ber_printf( ber, /*{*/ "N}" );

	struct berval *bv = NULL;
	int rc = ber_flatten( ber, &bv );

	if ( rc < 0 ) return( (SInt32) eMemoryError );

	ber_free( ber, 1 );

	int id;
	rc = ldap_extended_operation( ld, LDAP_EXOP_MODIFY_PASSWD, bv, NULL, NULL, &id );

	ber_bvfree( bv );

	if ( rc == LDAP_SUCCESS ) {
			DSGetExtendedLDAPResult( ld, 0, id, rc);
	}

	return rc;
}


void DSGetExtendedLDAPResult (	LDAP *inHost,
								int inSearchTO,
								int &inLDAPMsgId,
								int &inLDAPReturnCode)
{
    LDAPMessage*	res = NULL;
	struct	timeval	tv = {kLDAPDefaultNetworkTimeoutInSeconds,0};

	inLDAPReturnCode = 0;
	
    CLDAPv3Plugin::WaitForNetworkTransitionToFinish();

    inLDAPReturnCode = ldap_result(inHost, inLDAPMsgId, LDAP_MSG_ALL, &tv, &res);

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


#pragma mark -
#pragma mark Authentication Class Methods

//------------------------------------------------------------------------------------
//	* TryPWSPasswordSet
//------------------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::TryPWSPasswordSet( tDirNodeReference inNodeRef, UInt32 inAuthMethodCode, sLDAPContextData *pContext, tDataBufferPtr inAuthBuffer, const char *inRecordType )
{
	tDataBufferPtr  newAuthMethod			= NULL;
	tDataBufferPtr  authResult		= dsDataBufferAllocatePriv( 2048 );
	tContextData	continueData	= NULL;
	SInt32			siResult		= eDSAuthFailed;
	char			**dn			= NULL;
	char			*pUsername		= NULL;
	int				userBufferItem	= 1;
	bool			addingComputerRecord	= false;
	const char		*authMethod		= kDSStdAuthNewUser;
	
	// pick an auth method
	switch( inAuthMethodCode )
	{
		case kAuthNewUserWithPolicy:
		case kAuthNewUser:
			// already added to password server
			userBufferItem = 3;
			break;
		default:
			addingComputerRecord = (inRecordType && strcmp(inRecordType, kDSStdRecordTypeComputers) == 0);
			if ( addingComputerRecord )
				authMethod = kDSStdAuthNewComputer;
			
			newAuthMethod = dsDataNodeAllocateString( inNodeRef, authMethod );
	}
	
	// if our buffer was allocated, we can continue..
	if ( authResult != NULL )
	{
		// if we we are authenticated as a user and have a username coming in in the buffer
		if ( pContext->fLDAPConnection->fbAuthenticated && GetUserNameFromAuthBuffer(inAuthBuffer, userBufferItem, &pUsername) == eDSNoErr )
		{
			// we need to parse out the username from the authed user so we can stuff it in the buffer
			dn = ldap_explode_dn( pContext->fLDAPConnection->fLDAPUsername, 1 );
		}
	}
	
	// If we have a dn, let's stuff a buffer with the right information..
	if ( dn != NULL )
	{
		tDataBufferPtr  authData	= NULL;
		char			*pUserID	= NULL;
		char			**aaArray   = NULL;
		UInt32			aaCount		= 0;
		char			*recType	= pContext->fLDAPConnection->fLDAPRecordType;
				
		// Let's locate the user and get the Auth Authority, then we can continue
		siResult = GetAuthAuthority( pContext, dn[0], 0, &aaCount, &aaArray, recType ? recType : kDSStdRecordTypeUsers );
		if ( siResult == eDSNoErr )
		{
			pUserID = GetPWSAuthData( aaArray, aaCount );
			DSFree( aaArray );
		}
		
		if ( pUserID != NULL && (authData = dsDataBufferAllocatePriv(4096)) != NULL )
		{
			char *ipAddress = NULL;
			char *endOfIDPtr = strchr( pUserID, ':' );
			if ( endOfIDPtr != NULL ) {
				*endOfIDPtr = '\0';
				ipAddress = endOfIDPtr + 1;
			}
			
			siResult = dsFillAuthBuffer(
							authData, 2,
							strlen(pUserID), pUserID,
							strlen(pContext->fLDAPConnection->fLDAPPassword), pContext->fLDAPConnection->fLDAPPassword );
			
			// append the buffer that was passed to us
			bcopy( inAuthBuffer->fBufferData, authData->fBufferData + authData->fBufferLength, inAuthBuffer->fBufferLength );
			authData->fBufferLength += inAuthBuffer->fBufferLength;
			
			if ( addingComputerRecord )
				siResult = dsAppendAuthBuffer( authData, 1, 34, pUserID );
			
			// since we don't know the actual IP of PasswordServer, we'll pull it from the userID we are signed in as.
			if ( ipAddress != NULL && !DSIsStringEmpty(ipAddress) )
			{
				char			*newAuthority		= NULL;
				char			*newKerbAuthority   = NULL;
				char			*newPWSid			= NULL;
				tDataBufferPtr  pAuthType			= NULL;
				UInt32			newPWSlength		= 0;
				int				iLength				= 0;
				char			*pUserDN			= NULL;

				if ( newAuthMethod != NULL )
				{
					siResult = dsDoDirNodeAuth( inNodeRef, newAuthMethod, true, authData, authResult, &continueData );
								
					// now let's parse the buffer for the response information and build the auth authority string
					if ( siResult == eDSNoErr )
					{
						newPWSlength = *((UInt32 *)authResult->fBufferData);
						
						newPWSid = dsCStrFromCharacters( authResult->fBufferData + sizeof(newPWSlength), newPWSlength );
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
				
				if ( siResult == eDSNoErr )
				{
					iLength = strlen( kDSValueAuthAuthorityPasswordServerPrefix ) + newPWSlength + strlen( ipAddress ) + 2;
					newAuthority = (char *) calloc( sizeof(char), iLength );
					strcpy( newAuthority, kDSValueAuthAuthorityPasswordServerPrefix );
					strcat( newAuthority, newPWSid );
					strcat( newAuthority, ":" );
					strcat( newAuthority, ipAddress );
				}
				
				// now let's parse out the KerberosPrincipal
				if ( siResult == eDSNoErr && newPWSid && KDCHasNonLocalRealm(pContext) &&
					(pAuthType = dsDataNodeAllocateString(inNodeRef, kDSStdAuthGetKerberosPrincipal)) != NULL )
				{
					bcopy( &newPWSlength, authData->fBufferData, sizeof(newPWSlength) );
					bcopy( newPWSid, authData->fBufferData+sizeof(newPWSlength), newPWSlength );
					authData->fBufferLength = sizeof(newPWSlength) + newPWSlength;
					
					if ( dsDoDirNodeAuth(inNodeRef, pAuthType, true, authData, authResult, NULL) == eDSNoErr )
					{
						SInt32			lLength		= *((SInt32 *)authResult->fBufferData);
						char			*kerbPrinc  = (char *) calloc( sizeof(char), lLength + 1 );
						char			*cr			= NULL;
						char			*comma		= strchr( newPWSid, ',' );
						int				iTmpLen		= comma ? (comma - newPWSid) : 34;
						const size_t	newKerbAuthoritySize = sizeof(kDSValueAuthAuthorityKerberosv5) + kPWFileMaxPublicKeyBytes;
						
						newKerbAuthority = (char *) calloc( sizeof(char), newKerbAuthoritySize );

						if ( kerbPrinc != NULL && newKerbAuthority != NULL )
						{
							bcopy( authResult->fBufferData+sizeof(SInt32), kerbPrinc, lLength );
							
							// if we have a CR/LF let's clear it..
							if ( (cr = strstr(kerbPrinc, "\r\n")) )
							{
								*cr = '\0';
							}
							
							char *atSign = strchr( kerbPrinc, '@' );
							
							if ( atSign )
							{
								// first add Kerberos Tag
								strcpy( newKerbAuthority, kDSValueAuthAuthorityKerberosv5 );
								strncat( newKerbAuthority, newPWSid, iTmpLen );
								strcat( newKerbAuthority, ";" );
								strlcat( newKerbAuthority, kerbPrinc, newKerbAuthoritySize );
								strlcat( newKerbAuthority, ";", newKerbAuthoritySize );
								strlcat( newKerbAuthority, atSign+1, newKerbAuthoritySize );
								strlcat( newKerbAuthority, ";", newKerbAuthoritySize );
								strlcat( newKerbAuthority, comma+1, newKerbAuthoritySize );
								strlcat( newKerbAuthority, ":", newKerbAuthoritySize );
								strlcat( newKerbAuthority, ipAddress, newKerbAuthoritySize );
							}
						}
						
						if ( siResult == eDSNoErr && (pUserDN = GetDNForRecordName(pUsername, pContext, inRecordType)) != NULL )
						{
							// now lets set the attribute
							size_t buffLen = sizeof("Kerberos:") + strlen( kerbPrinc );
							char *altID = (char *) malloc( buffLen );
							
							strlcpy( altID, "Kerberos:", buffLen );
							strlcat( altID, kerbPrinc, buffLen );
							
							// now lets see if it already exists
							UInt32	count		= 0;
							char	**values	= NULL;
							bool	bFound		= false;
							
							if ( LookupAttribute(pContext, inRecordType, pUsername, kDSNAttrAltSecurityIdentities, &count, &values) == eDSNoErr )
							{
								for ( UInt32 ii = 0; ii < count; ii++ )
								{
									char *temp = values[ii];
									
									if ( temp != NULL && strcmp(temp, altID) == 0 ) {
										bFound = true;
									}
								}
							}
							
							// if it wasn't found add it to the list, using a single op that includes old values
							if ( bFound == false )
							{
								values = (char **) reallocf( values, (count + 2) * sizeof(char *) );
								values[count++] = altID;
								values[count] = NULL;
								
								tDirStatus tempResult = SetAttributeValueForDN( pContext, pUserDN, inRecordType, 
																			    kDSNAttrAltSecurityIdentities, (const char **) values );
								DbgLog( kLogInfo, "CLDAPv3Plugin::TryPWSPasswordSet - Adding '%s' to AltSecurityIdentities - %s (%d)", 
									    altID, (tempResult == eDSNoErr ? "success" : "failed"), tempResult );
								
								altID = NULL;
							}
							
							DSFree( altID );
							DSFreeStringList( values );
						}
						
						DSFreeString( kerbPrinc );
					}
				}
				
				// if we have a new auth authority, let's set it for the record
				if ( siResult == eDSNoErr && newAuthority )
				{
					// let's get the DN for the record that we're updating
					if ( pUserDN == NULL )
						pUserDN = GetDNForRecordName( pUsername, pContext, inRecordType );
					
					if ( pUserDN )
					{
						// Let's set the authentication authority now...  We use a special internal routine so we don't
						// get a different record on a different node and for speed.
						const char *pValues[] = { newAuthority, newKerbAuthority, NULL };
						siResult = SetAttributeValueForDN( pContext, pUserDN, inRecordType, kDSNAttrAuthenticationAuthority, pValues );
						if ( siResult == eDSNoErr )
						{
							// let's put the password marker too...
							const char *pValues2[] = { kDSValueNonCryptPasswordMarker, NULL };
							SetAttributeValueForDN( pContext, pUserDN, inRecordType, kDS1AttrPassword, pValues2 );
						}
					}
				}

				DSFreeString( newKerbAuthority );
				DSFreeString( newAuthority );
				DSFreeString( newPWSid );
				DSFree( pUserDN );
				
				if ( pAuthType != NULL ) 
				{
					dsDataBufferDeAllocate( inNodeRef, pAuthType );
					pAuthType = NULL;
				}
			}
			
			DSFree( pUserID );
		}
		
		if ( authData != NULL )
		{
			dsDataBufferDeAllocate( inNodeRef, authData );
			authData = NULL;
		}

		// we already checked this one..
		ldap_value_free( dn );
		dn = NULL;
	} // if ( dn != NULL )
	
	DSFreeString( pUsername );
	
	if ( authResult )
	{
		dsDataBufferDeAllocate( inNodeRef, authResult );
		authResult = NULL;
	}

	if ( newAuthMethod )
	{
		dsDataBufferDeAllocate( inNodeRef, newAuthMethod );
		newAuthMethod = NULL;
	}
	
	return siResult;
} // TryPWSPasswordSet


//------------------------------------------------------------------------------------
//	* GetPWSAuthData
//------------------------------------------------------------------------------------

char *CLDAPv3Plugin::GetPWSAuthData( char **inAAArray, UInt32 inAACount )
{
	char		*pUserID	= NULL;
	char		*aaVersion  = NULL;
	char		*aaTag		= NULL;
	char		*aaData		= NULL;
	UInt32		idx			= 0;
	
	// loop through all possibilities and find the password server authority
	for ( idx = 0; idx < inAACount; idx++ )
	{
		// parse this value of auth authority
		ldapParseAuthAuthority( inAAArray[idx], &aaVersion, &aaTag, &aaData );
		DSFreeString( inAAArray[idx] )
		
		if ( strcasecmp(aaTag, kDSTagAuthAuthorityPasswordServer) == 0 )
		{
			// we don't free this because we're going to use it later..
			pUserID = aaData;
			aaData = NULL;
		}
		
		DSFreeString( aaVersion );
		DSFreeString( aaTag );
		DSFreeString( aaData );
	}
	
	return pUserID;
}


//------------------------------------------------------------------------------------
//	* DoAuthentication
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::DoAuthentication ( sDoDirNodeAuth *inData, const char *inRecTypeStr,
	CDSAuthParams &inParams )
{
	return (tDirStatus)DoAuthenticationOnRecordType( (sDoDirNodeAuthOnRecordType *)inData, inRecTypeStr );
} // DoAuthentication


//------------------------------------------------------------------------------------
//	* DoAuthenticationOnRecordType
//------------------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::DoAuthenticationOnRecordType ( sDoDirNodeAuthOnRecordType *inData, const char* inRecordType )
{
	SInt32						siResult			= eDSAuthFailed;
	UInt32						uiAuthMethod		= 0;
	sLDAPContextData			*pContext			= nil;
	sLDAPContinueData   		*pContinueData		= NULL;
	char*						userName			= NULL;
	int							userNameBuffLen		= 0;
	LDAPv3AuthAuthorityHandlerProc	handlerProc 	= NULL;
	tContextData				uiContinue			= 0;
		
	pContext = gLDAPContextTable->GetObjectForRefNum( inData->fInNodeRef );
	if ( pContext == NULL )
		return( (SInt32)eDSInvalidNodeRef );
	if ( inData->fInAuthStepData == NULL )
	{
		DSRelease( pContext );
		return( (SInt32)eDSNullAuthStepData );
	}
	if ( inRecordType == NULL )
	{
		DSRelease( pContext );
		return( (SInt32)eDSNullRecType );
	}

	do
	{		
		if ( inData->fIOContinueData != 0 )
		{
			// get info from continue
			pContinueData = (sLDAPContinueData *) gLDAPContinueTable->GetPointer( inData->fIOContinueData );
			if ( pContinueData == NULL ) {
				siResult = eDSInvalidContinueData;
				break;
			}
			
			if ( pContinueData->fAuthHandlerProc == NULL ) {
				siResult = eDSInvalidContinueData;
				break;
			}
			
			uiContinue = inData->fIOContinueData;
			pContinueData->Retain();
			
			handlerProc = (LDAPv3AuthAuthorityHandlerProc)(pContinueData->fAuthHandlerProc);
			if (handlerProc != NULL)
			{
				siResult = (handlerProc)(inData->fInNodeRef, inData->fInAuthMethod, pContext,
										 &pContinueData, inData->fInAuthStepData, 
										 inData->fOutAuthStepDataResponse, 
										 inData->fInDirNodeAuthOnlyFlag, 
										 pContinueData->fAuthAuthorityData, NULL,
                                         fLDAPConnectionMgr, inRecordType);
			}
			
			pContinueData->Release();
		}
		else
		{
			UInt32 idx = 0;
			UInt32 aaCount = 0;
			char **aaArray;
                
			// first call
			// note: if GetAuthMethod() returns eDSAuthMethodNotSupported we want to keep going
			// because password server users may still be able to use the method with PSPlugin
			if (inData->fInAuthMethod != nil)
			{
				DbgLog( kLogPlugin, "CLDAPv3Plugin: DoAuthenticationOnRecordType - Attempting use of authentication method %s", inData->fInAuthMethod->fBufferData );
			}
			siResult = dsGetAuthMethodEnumValue( inData->fInAuthMethod, &uiAuthMethod );
			if ( ( siResult != eDSNoErr ) && ( siResult != eDSAuthMethodNotSupported ) )
				break;
			
			if ( ( siResult == eDSNoErr) && ( uiAuthMethod == kAuthKerberosTickets ) )
			{
				siResult = DoKerberosAuth( inData->fInNodeRef, inData->fInAuthMethod,
											pContext, &pContinueData, 
											inData->fInAuthStepData, inData->fOutAuthStepDataResponse,
											inData->fInDirNodeAuthOnlyFlag, NULL, NULL,
											fLDAPConnectionMgr, inRecordType );
				
				if ( pContinueData != NULL ) {
					pContinueData->fAuthHandlerProc = (void *)DoKerberosAuth;
					inData->fIOContinueData = gLDAPContinueTable->AddPointer( pContinueData, inData->fInNodeRef );
				}
				
				inData->fResult = siResult;
				
				DSRelease( pContext );
				return( siResult );
			}
			else if ( ( siResult == eDSNoErr) && ( uiAuthMethod == kAuth2WayRandom ) )
			{
				// for 2way random the first buffer is the username
				if ( inData->fInAuthStepData->fBufferLength > inData->fInAuthStepData->fBufferSize ) {
					siResult = eDSInvalidBuffFormat;
					break;
				}
				userName = dsCStrFromCharacters( inData->fInAuthStepData->fBufferData, inData->fInAuthStepData->fBufferLength );
			}
			else if ( ( siResult == eDSNoErr) && ( uiAuthMethod == kAuthGetPolicy ) )
			{
				siResult = GetUserNameFromAuthBuffer( inData->fInAuthStepData, 3, &userName, &userNameBuffLen );
				if ( siResult != eDSNoErr )
					break;
			}
			else if ( uiAuthMethod == kAuthNativeMethod && 
					  strcmp("dsAuthMethodNative:dsAuthRootProcessWrite", inData->fInAuthMethod->fBufferData) == 0 )
			{
				siResult = eDSAuthFailed;
				
				// only allow this if the root process is running as root
				if ( pContext->fUID == 0 && pContext->fEffectiveUID == 0 )
				{
					char	*pUsername		= NULL;
					char	*pPassword		= NULL;
					char	*pKerberosID	= NULL;

					CLDAPNodeConfig *nodeConfig = pContext->fLDAPConnection->fNodeConfig;
					if ( nodeConfig != NULL && nodeConfig->CopyCredentials(&pUsername, &pKerberosID, &pPassword) == true )
					{
						siResult = fLDAPConnectionMgr->AuthConnection( &(pContext->fLDAPConnection), 
																	   pUsername,
																	   inRecordType,
																	   pKerberosID,
																	   pPassword );
					}
					else
					{
						siResult = eDSAuthMethodNotSupported;
					}
					
					DSFree( pUsername );
					DSFreePassword( pPassword );
					DSFree( pKerberosID );
				}
				else
				{
					DbgLog( kLogPlugin, "CLDAPv3Plugin::DoAuthenticationOnRecordType unable to do rootAuth process is not running as root" );
				}
				
				DSRelease( pContext );
				return siResult;
			}
			else
			{
				siResult = GetUserNameFromAuthBuffer( inData->fInAuthStepData, 1, &userName, &userNameBuffLen );
				if ( siResult != eDSNoErr )
					break;
			}
			// get the auth authority
			siResult = GetAuthAuthority( pContext,
										userName,
										userNameBuffLen,
										&aaCount,
										&aaArray, inRecordType );
			
			// check for users this ldap server is augmenting
			if ( siResult == eDSRecordNotFound && strcmp(inRecordType, kDSStdRecordTypeUsers) == 0 )
			{
				int augUserNameLen = sizeof( "Users:" ) + strlen( userName );
				char *augUserName = (char *) malloc( augUserNameLen );
				snprintf( augUserName, augUserNameLen, "Users:%s", userName );
				
				SInt32 siAugmentResult = GetAuthAuthority(
											pContext,
											augUserName,
											augUserNameLen,
											&aaCount,
											&aaArray,
											kDSStdRecordTypeAugments );
				
				DSFreeString( augUserName );
				
				if ( siAugmentResult == eDSAttributeNotFound )
				{
					siResult = eNotHandledByThisNode;
					break;
				}
				else if ( siAugmentResult == eDSNoErr )
					siResult = eDSNoErr;
			}
			
			if ( siResult == eDSNoErr || siResult == eDSAuthMethodNotSupported )
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
				
				for ( idx = 0; idx < aaCount; idx++ )
				{
					if ( aaArray[idx] )
					{
						//parse this value of auth authority
						if ( ldapParseAuthAuthority(aaArray[idx], &aaVersion, &aaTag, &aaData) == eDSNoErr )
						{
							if ( aaTag )
							{
								authMethods.insert( make_pair(string(aaTag), LDAPv3AuthTagStruct(aaVersion,aaData)) );
								
								free( aaTag );
								aaTag = NULL;
							}
							
							DSFreeString( aaVersion );
							DSFreeString( aaData );
						}
						free( aaArray[idx] );
						aaArray[idx] = NULL;
					}
				}
				
				map<string,LDAPv3AuthTagStruct>::iterator authIterator = authMethods.begin();
				
				LDAPv3AuthTagStruct *kerbAuthAuthority = NULL; 

				map<string,LDAPv3AuthTagStruct>::iterator kerbIterator = authMethods.find(string(kDSTagAuthAuthorityKerberosv5));
				if ( kerbIterator != authMethods.end() )
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
												inData->fInDirNodeAuthOnlyFlag,
												(char *) authData.authData.c_str(),
												(char *)(kerbAuthAuthority ? kerbAuthAuthority->authData.c_str() : NULL),
												fLDAPConnectionMgr,
												inRecordType);
						if ( siResult == eDSNoErr )
						{
							if ( pContinueData != NULL )
							{
								// we are supposed to return continue data
								// remember the proc we used
								pContinueData->fAuthHandlerProc = (void*)handlerProc;
								pContinueData->fAuthAuthorityData = (char *)strdup(authData.authData.c_str());
								uiContinue = gLDAPContinueTable->AddPointer( pContinueData, inData->fInNodeRef );
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
				if ( pContext->fPWSNodeRef != 0 &&
					(uiAuthMethod == kAuthNewUser || uiAuthMethod == kAuthNewUserWithPolicy ) )
				{
					TryPWSPasswordSet( pContext->fPWSNodeRef, uiAuthMethod, pContext, inData->fInAuthStepData, inRecordType );
				}
				
				// need to free remaining attributes
				if ( bIsSecondary && siResult == eDSAuthMethodNotSupported )
					siResult = eDSNoErr;
				
				DSFree( aaArray );
			}
			else
			{
				if ( siResult == eDSRecordNotFound )
				{
					siResult = eDSAuthUnknownUser;
				}
				else if ( siResult != eDSCannotAccessSession )
				{
					// If we are a doing a SetPassword or SetPasswordAsRoot, let's see if we have Password Server available first
					// If so, we'll try to create a password slot for the user and make them a PasswordServer User
					// If we have a fPWSNodeRef, we already have a password server available... 
					//		because the user that authenticated is a passwordServer user.. which means
					//		they may have rights to set the password
					
					if ( pContext->fPWSNodeRef != 0 &&
						(uiAuthMethod == kAuthSetPasswd || uiAuthMethod == kAuthSetPasswdAsRoot) )
					{
						siResult = TryPWSPasswordSet( pContext->fPWSNodeRef, uiAuthMethod, pContext, inData->fInAuthStepData, inRecordType );
					}
					
					// well, the PasswordServer Set failed, let's try to do a normal one..
					if ( siResult != eDSNoErr )
					{
						//revert to basic
						siResult = DoBasicAuth(inData->fInNodeRef,inData->fInAuthMethod, pContext, 
											&pContinueData, inData->fInAuthStepData,
											inData->fOutAuthStepDataResponse,
											inData->fInDirNodeAuthOnlyFlag, NULL, NULL,
											fLDAPConnectionMgr, inRecordType);
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
	}
	while ( 0 );
	
	// if this was a set password let's see if it was on an auth'd connection
	if ( siResult == eDSNoErr && uiAuthMethod == kAuthSetPasswdAsRoot && pContext->fLDAPConnection->fbAuthenticated )
	{
		char *accountId = GetDNForRecordName( userName, pContext, inRecordType );
		if ( accountId && strcmp( accountId, (char *)pContext->fLDAPConnection->fLDAPUsername) == 0 )
		{
			char	*password = NULL;
			GetUserNameFromAuthBuffer( inData->fInAuthStepData, 2, &password );
			
			// let's update the node struct otherwise we'll have issues on reconnect
			pContext->fLDAPConnection->UpdateCredentials( password );
			DbgLog( kLogPlugin, "CLDAPv3Plugin: DoAuthenticationOnRecordType - Updated session password" );
			
			DSFreePassword( password );
		}
		
		DSFree( accountId );
	}
	
	DSFree(userName);

	inData->fResult = siResult;
	inData->fIOContinueData = uiContinue;
	
	if ( uiContinue != 0 && pContinueData == NULL )
		gLDAPContinueTable->RemoveContext( uiContinue );
	
	DSRelease( pContext );

	return( siResult );

} // DoAuthenticationOnRecordType


//------------------------------------------------------------------------------------
//	* IsWriteAuthRequest
//------------------------------------------------------------------------------------

bool CLDAPv3Plugin::IsWriteAuthRequest ( UInt32 uiAuthMethod )
{
	switch ( uiAuthMethod )
	{
		case kAuthSetPasswd:
		case kAuthSetPasswdAsRoot:
		case kAuthChangePasswd:
		case kAuthSetCertificateHashAsRoot:
			return true;
			
		default:
			return false;
	}
} // IsWriteAuthRequest


//------------------------------------------------------------------------------------
//	* DoKerberosAuth
//------------------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::DoKerberosAuth(	tDirNodeReference inNodeRef,
										tDataNodePtr inAuthMethod,
										sLDAPContextData* inContext,
										sLDAPContinueData** inOutContinueData,
										tDataBufferPtr inAuthData,
										tDataBufferPtr outAuthData,
										bool inAuthOnly,
										char* inAuthAuthorityData,
										char* inKerberosId,
										CLDAPConnectionManager *inLDAPSessionMgr,
										const char *inRecordType )
{
    SInt32					result				= eDSAuthFailed;
	krb5_creds				credentials;
	krb5_creds				*creds				= NULL;
	krb5_context			krbContext			= NULL;
	krb5_principal			principal			= NULL;
	krb5_get_init_creds_opt	options;
	char					*userName			= NULL;
	char					*pKerberosID		= NULL;
	char					*pPassword			= NULL;
	UInt32					uiAuthMethod		= 0;
	
	DbgLog( kLogPlugin, "CLDAPv3Plugin: DoKerberosAuth::" );
	
	if ( dsGetAuthMethodEnumValue(inAuthMethod, &uiAuthMethod) == eDSNoErr &&
		 uiAuthMethod == kAuthKerberosTickets )
	{
		if ( inAuthOnly )
			return eDSAuthMethodNotSupported;
		if ( inOutContinueData == NULL || inContext->fLDAPConnection == NULL )
			return eDSNullParameter;
		
		// eDSAuthParameterError means that a replica is available, but no credentials supplied
		if ( *inOutContinueData == 0 && 
			 inLDAPSessionMgr->AuthConnectionKerberos(&(inContext->fLDAPConnection), NULL, NULL, NULL, NULL) == eDSAuthParameterError )
		{
			// The result gets cleared on success
			result = eDSAuthFailed;
			
			char *serverPrincipal = inContext->fLDAPConnection->CopyReplicaServicePrincipal();
			if ( serverPrincipal != NULL )
			{
				DbgLog( kLogPlugin, "CLDAPv3Plugin::DoKerberosAuth - principal for server is %s", serverPrincipal );
				result = dsFillAuthBuffer( outAuthData, 1, strlen(serverPrincipal) + 1, serverPrincipal );
				DSFreeString( serverPrincipal );
			}
			else
			{
				DbgLog( kLogPlugin, "CLDAPv3Plugin::DoKerberosAuth - could not determine principal for server" );
			}
			
			if ( result == eDSNoErr )
			{
				*inOutContinueData = new sLDAPContinueData;
				if ( *inOutContinueData == NULL )
					return eMemoryError;
			}
		}
		else
		{
			*inOutContinueData = NULL;
			
			gKerberosMutex->WaitLock();
			
			result = GetKrbCredentialFromAuthBuffer( inAuthData, &userName, &pKerberosID, &creds );
			if ( result == eDSNoErr )
			{
				result = inLDAPSessionMgr->AuthConnectionKerberos( &(inContext->fLDAPConnection), userName, NULL, creds, pKerberosID );
			}
			
			gKerberosMutex->SignalLock();
		}
	}
	else if ( inAuthOnly == false )
	{
		result = eDSAuthMethodNotSupported;
	} 
	else if ( inAuthData->fBufferData != NULL )
	{
		DbgLog( kLogPlugin, "CLDAPv3Plugin: DoKerberosAuth - Attempting use of authentication method %s", inAuthMethod->fBufferData );
	
		// if we don't have a kerberos ID, let's get the name from the buffer instead
		if ( NULL == inKerberosId )
			GetUserNameFromAuthBuffer( inAuthData, 1, &pKerberosID );
		else
			pKerberosID = strdup( inKerberosId );
		
		GetUserNameFromAuthBuffer( inAuthData, 2, &pPassword );
		
		if ( pKerberosID != NULL && pPassword != NULL )
		{
			gKerberosMutex->WaitLock();
			
			result = dsGetAuthMethodEnumValue( inAuthMethod, &uiAuthMethod );
			if ( eDSNoErr == result )
			{
				switch( uiAuthMethod )
				{
					case kAuthNativeClearTextOK:
					case kAuthClearText:
					case kAuthNativeNoClearText:
					case kAuthCrypt:
					case kAuthNativeRetainCredential:
						result = krb5_init_context( &krbContext );
						if ( 0 == result )
							result = krb5_parse_name( krbContext, inKerberosId, &principal );

						bzero( &credentials, sizeof(credentials) );

						if ( 0 == result )
						{
							krb5_get_init_creds_opt_init( &options );
							krb5_get_init_creds_opt_set_tkt_life( &options, 30 );
							
							result = krb5_get_init_creds_password( krbContext, &credentials, principal, pPassword, NULL, 0, 0, NULL, &options );
						}
							
						// if had an error or it wasn't pre-authenticated, we can't confirm the validity
						if ( 0 != result ||
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
			gKerberosMutex->SignalLock();
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
	
	DSFreeString( userName );
	DSFreeString( pKerberosID );
	DSFreePassword( pPassword );
	
	return result;
} // DoKerberosAuth

//------------------------------------------------------------------------------------
//	* DoPasswordServerAuth
//------------------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::DoPasswordServerAuth(
    tDirNodeReference inNodeRef,
    tDataNodePtr inAuthMethod, 
    sLDAPContextData* inContext, 
    sLDAPContinueData** inOutContinueData, 
    tDataBufferPtr inAuthData, 
    tDataBufferPtr outAuthData, 
    bool inAuthOnly,
    char* inAuthAuthorityData,
	char* inKerberosId,
    CLDAPConnectionManager *inLDAPSessionMgr,
	const char *inRecordType )
{
    SInt32					returnValue			= eDSAuthFailed;
    SInt32					error				= eDSAuthFailed;
    UInt32					authMethod;
    char				   *serverAddr;
    char				   *uidStr				= NULL;
    SInt32					uidStrLen;
    tDataBufferPtr			authDataBuff		= NULL;
    tDataBufferPtr			authDataBuffTemp	= NULL;
    char				   *nodeName			= NULL;
    char				   *userName			= NULL;
    char				   *accountId			= NULL;
	char				   *password			= NULL;
    sLDAPContinueData      *pContinue			= NULL;
    tContextData			continueData		= NULL;
    UInt32					aaCount				= 0;
	char				  **aaArray;

    if ( !inAuthAuthorityData || *inAuthAuthorityData == '\0' ) {
		DBGLOG( kLogPlugin, "CLDAPv3Plugin::DoPasswordServerAuth: no auth data" );
        return eDSAuthParameterError;
    }
    
	serverAddr = strchr( inAuthAuthorityData, ':' );
	if ( serverAddr )
	{
		uidStrLen = serverAddr - inAuthAuthorityData;
		
		uidStr = dsCStrFromCharacters( inAuthAuthorityData, uidStrLen );
		if ( uidStr == nil ) return( (SInt32)eMemoryError );
		
		// advance past the colon
		serverAddr++;
		
		DbgLog( kLogPlugin, "CLDAPv3Plugin::DoPasswordServerAuth:" );
		if (inAuthMethod != nil)
		{
			DbgLog( kLogPlugin, "CLDAPv3Plugin: Attempting use of authentication method %s", inAuthMethod->fBufferData );
		}
		error = dsGetAuthMethodEnumValue( inAuthMethod, &authMethod );
		switch( authMethod )
		{
			case kAuthKerberosTickets:
				returnValue = eDSAuthMethodNotSupported;
				goto cleanup;
				break;
				
			case kAuth2WayRandom:
				if ( inOutContinueData == nil ) {
					returnValue = eDSNullParameter;
					goto cleanup;
				}
				if ( *inOutContinueData == nil )
				{
					pContinue = new sLDAPContinueData;
					
					// make a buffer for the user ID
					authDataBuff = dsDataBufferAllocatePriv( uidStrLen + 1 );
					if ( authDataBuff == nil ) {
						returnValue = eMemoryError;
						break;
					}
					
					// fill
					strcpy( authDataBuff->fBufferData, uidStr );
					authDataBuff->fBufferLength = uidStrLen;
				}
				else
				{
					pContinue = *inOutContinueData;
				}
				break;
				
			case kAuthSetPasswd:
			case kAuthSetPolicy:
			case kAuthSetUserName:
			case kAuthSetUserData:
			case kAuthDeleteUser:
				{
					char* aaVersion = NULL;
					char* aaTag = NULL;
					char* aaData = NULL;
					unsigned int idx;
					SInt32 lookupResult;
					char* endPtr = NULL;
					
					// lookup the user that wasn't passed to us
					error = GetUserNameFromAuthBuffer( inAuthData, 3, &userName );
					if ( error != eDSNoErr ) {
						returnValue = error;
						goto cleanup;
					}
					
					// lookup authauthority attribute
					error = GetAuthAuthority(
									inContext,
									userName,
									0,
									&aaCount,
									&aaArray,
									inRecordType );
					
					if ( error != eDSNoErr ) {
						returnValue = eDSAuthFailed;
						goto cleanup;
					}
					
					// don't break or throw to guarantee cleanup
					lookupResult = eDSAuthFailed;
					for ( idx = 0; idx < aaCount && lookupResult == eDSAuthFailed; idx++ )
					{
						//parse this value of auth authority
						error = ldapParseAuthAuthority( aaArray[idx], &aaVersion, &aaTag, &aaData );
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
					
					if ( lookupResult != eDSNoErr ) {
						returnValue = eDSAuthFailed;
						goto cleanup;
					}
					
					// do the usual
					error = RepackBufferForPWServer( inAuthData, uidStr, 1, &authDataBuffTemp );
					if ( error != eDSNoErr ) {
						returnValue = error;
						goto cleanup;
					}
					
					// put the admin user ID in slot 3
					error = RepackBufferForPWServer( authDataBuffTemp, aaData, 3, &authDataBuff );
					DSFreeString(aaData);
					if ( error != eDSNoErr ) {
						returnValue = error;
						goto cleanup;
					}
				}
				break;

			case kAuthNTSessionKey:
			case kAuthNTLMv2WithSessionKey:
				{
					char* aaVersion = NULL;
					char* aaTag = NULL;
					char* aaData = NULL;
					unsigned int idx;
					SInt32 lookupResult;
					char* endPtr = NULL;
					int indexOfSecondUser = (authMethod == kAuthNTSessionKey) ? 4 : 6;
					
					// lookup the user that wasn't passed to us
					error = GetUserNameFromAuthBuffer( inAuthData, indexOfSecondUser, &userName );
					if ( error != eDSNoErr && error != eDSNullParameter ) {
						returnValue = error;
						goto cleanup;
					}
					
					if ( !DSIsStringEmpty(userName) )
					{
						// lookup authauthority attribute
						error = GetAuthAuthority( inContext,
										userName,
										0,
										&aaCount,
										&aaArray,
										inRecordType );
						
						if ( error != eDSNoErr ) {
							returnValue = eDSAuthFailed;
							goto cleanup;
						}
						
						// don't break or throw to guarantee cleanup
						lookupResult = eDSAuthFailed;
						for ( idx = 0; idx < aaCount && lookupResult == eDSAuthFailed; idx++ )
						{
							//parse this value of auth authority
							error = ldapParseAuthAuthority( aaArray[idx], &aaVersion, &aaTag, &aaData );
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
						
						if ( lookupResult != eDSNoErr ) {
							returnValue = eDSAuthFailed;
							goto cleanup;
						}
						
						// do the usual
						error = RepackBufferForPWServer( inAuthData, uidStr, 1, &authDataBuffTemp );
						if ( error != eDSNoErr ) {
							returnValue = error;
							goto cleanup;
						}
						
						// put the admin user ID in slot 4 or 6 depending on the auth method
						error = RepackBufferForPWServer( authDataBuffTemp, aaData, indexOfSecondUser, &authDataBuff );
						DSFreeString(aaData);
						if ( error != eDSNoErr ) {
							returnValue = error;
							goto cleanup;
						}
					}
					else
					{
						// do the usual
						error = RepackBufferForPWServer( inAuthData, uidStr, 1, &authDataBuff );
						if ( error != eDSNoErr ) {
							returnValue = error;
							goto cleanup;
						}
					}
				}
				break;
			
			case kAuthGetPolicy:
				{
					char* aaVersion = NULL;
					char* aaTag = NULL;
					char* aaData = NULL;
					unsigned int idx;
					SInt32 lookupResult;
					char* endPtr = NULL;
					
					// lookup the user that wasn't passed to us
					// GetPolicy doesn't require an authenticator
					error = GetUserNameFromAuthBuffer( inAuthData, 1, &userName );
					if ( error != eDSNoErr )
						userName = NULL;
					
					// put the user ID in slot 3
					error = RepackBufferForPWServer( inAuthData, uidStr, 3, &authDataBuffTemp );
					if ( error != eDSNoErr ) {
						returnValue = error;
						goto cleanup;
					}

					// lookup authauthority attribute
					if ( !DSIsStringEmpty(userName) )
					{
						error = GetAuthAuthority( inContext,
										userName,
										0,
										&aaCount,
										&aaArray,
										inRecordType );
						
						if ( error == eDSNoErr )
						{
							// don't break or throw to guarantee cleanup
							lookupResult = eDSAuthFailed;
							for ( idx = 0; idx < aaCount && lookupResult == eDSAuthFailed; idx++ )
							{
								//parse this value of auth authority
								error = ldapParseAuthAuthority( aaArray[idx], &aaVersion, &aaTag, &aaData );
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
							
							if ( lookupResult != eDSNoErr ) {
								returnValue = eDSAuthFailed;
								goto cleanup;
							}
							
							// do the usual
							error = RepackBufferForPWServer( authDataBuffTemp, aaData, 1, &authDataBuff );
							if ( error != eDSNoErr ) {
								returnValue = error;
								goto cleanup;
							}
							DSFreeString(aaData);
						}
						else
						{
							// GetPolicy does not require an authenticator,
							// so just log that the supplied authenticator was invalid for
							// the target user.
							DbgLog( kLogPlugin, "CLDAPv3Plugin::DoPasswordServerAuth: could not retrieve the "
										"authentication authority for %s", userName );
							
							error = RepackBufferForPWServer( authDataBuffTemp, uidStr, 1, &authDataBuff );
							if ( error != eDSNoErr ) {
								returnValue = error;
								goto cleanup;
							}
						}
					}
					else
					{
						error = RepackBufferForPWServer( authDataBuffTemp, uidStr, 1, &authDataBuff );
						if ( error != eDSNoErr ) {
							returnValue = error;
							goto cleanup;
						}
					}
				}
				break;

			case kAuthClearText:
			case kAuthNativeClearTextOK:
			case kAuthNativeNoClearText:
			case kAuthNewUser:
			case kAuthNewUserWithPolicy:
			case kAuthNativeRetainCredential:
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
				if ( error != eDSNoErr ) {
					returnValue = error;
					goto cleanup;
				}
				if ( authMethod == kAuthPPS )
				{
					if ( inOutContinueData == nil ) {
						returnValue = eDSNullParameter;
						goto cleanup;
					}
					
					if ( *inOutContinueData == nil )
					{
						pContinue = new sLDAPContinueData;
					}
					else
					{
						pContinue = *inOutContinueData;
					}
				}
		}
		
		*inOutContinueData = pContinue;
		
		if ( inContext->fPWSRef == 0 )
		{
			error = dsOpenDirService( &inContext->fPWSRef );
			if ( error != eDSNoErr ) {
				returnValue = error;
				goto cleanup;
			}
		}
		
		// JT we should only use the saved reference if the operation
		// requires prior authentication
		// otherwise we should open a new ref each time
		if ( inContext->fPWSNodeRef == 0 )
		{
			nodeName = (char *)calloc(1,strlen(serverAddr)+17);
			if ( nodeName == nil ) {
				returnValue = eMemoryError;
				goto cleanup;
			}
			
			sprintf( nodeName, "/PasswordServer/%s", serverAddr );
			error = PWOpenDirNode( inContext->fPWSRef, nodeName, &inContext->fPWSNodeRef );
			if ( error != eDSNoErr ) {
				returnValue = eDSAuthServerError;
				goto cleanup;
			}
		}
		
		if ( pContinue )
			continueData = pContinue->fPassPlugContinueData;
	
		returnValue = PWSetReplicaData( inContext, uidStr, inLDAPSessionMgr );
		if ( returnValue == eDSNoErr )
			returnValue = dsDoDirNodeAuth( inContext->fPWSNodeRef, inAuthMethod, inAuthOnly,
										authDataBuff, outAuthData, &continueData );
		
		if ( pContinue )
			pContinue->fPassPlugContinueData = continueData;
		
		if ( authMethod == kAuthNewUser || authMethod == kAuthNewUserWithPolicy )
		{
			if ( outAuthData->fBufferLength > sizeof(UInt32) )
			{
				if ( inContext->fPWSUserID != NULL ) {
					free( inContext->fPWSUserID );
					inContext->fPWSUserID = NULL;
				}
				inContext->fPWSUserIDLength = *((UInt32 *)outAuthData->fBufferData);
				inContext->fPWSUserID = (char *) malloc( inContext->fPWSUserIDLength + 1 );
				if ( inContext->fPWSUserID == NULL ) {
					returnValue = eMemoryError;
					goto cleanup;
				}
				strlcpy( inContext->fPWSUserID, outAuthData->fBufferData + sizeof(UInt32), inContext->fPWSUserIDLength + 1 );
			}
		}
		
		if ( (returnValue == eDSNoErr) && (inAuthOnly == false) && (userName != NULL) && (password != NULL) )
		{
			accountId = GetDNForRecordName ( userName, inContext, inRecordType );
			
			if ( accountId ) // just to be safe... we could fail in the middle of looking for DN
			{
				returnValue = inLDAPSessionMgr->AuthConnection( &(inContext->fLDAPConnection),
																accountId,
																inRecordType,
																inKerberosId,
																password );

				//the LDAP master wasn't reachable..
				if ( returnValue == eDSCannotAccessSession )
				{
					returnValue = eDSAuthMasterUnreachable;
				}
			}
		}
	}

cleanup:
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
                        
	return( returnValue );
}

    
//------------------------------------------------------------------------------------
//	* DoBasicAuth
//------------------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::DoBasicAuth(
    tDirNodeReference inNodeRef,
    tDataNodePtr inAuthMethod,
    sLDAPContextData* inContext,
    sLDAPContinueData** inOutContinueData,
    tDataBufferPtr inAuthData,
    tDataBufferPtr outAuthData,
    bool inAuthOnly,
    char* inAuthAuthorityData,
	char* inKerberosId,
    CLDAPConnectionManager *inLDAPSessionMgr,
	const char *inRecordType )
{
	SInt32					siResult		= eDSNoErr;
	UInt32					uiAuthMethod	= 0;
    
	DbgLog( kLogPlugin, "CLDAPv3Plugin: DoBasicAuth::" );
	if (inAuthMethod != nil)
	{
		DbgLog( kLogPlugin, "CLDAPv3Plugin: DoBasicAuth - Attempting use of authentication method %s", inAuthMethod->fBufferData );
	}
	siResult = dsGetAuthMethodEnumValue( inAuthMethod, &uiAuthMethod );
	if ( siResult == eDSNoErr )
	{
		switch( uiAuthMethod )
		{
			case kAuthSetPolicy:
				siResult = eDSAuthMethodNotSupported;
				break;
			
			case kAuthCrypt:
			case kAuthNativeNoClearText:
				siResult = DoUnixCryptAuth( inContext, inAuthData, inLDAPSessionMgr, inRecordType );
				if ( siResult == eDSAuthFailedClearTextOnly )
				{
					// eDSAuthFailedRequiring Clear Text only, so let's do a bind check if we have SSL enabled
					if ( inContext->fLDAPConnection->fNodeConfig->fIsSSL )
						siResult = DoClearTextAuth( inContext, inAuthData, inKerberosId, inAuthOnly, inLDAPSessionMgr, inRecordType );
				}
				break;

			case kAuthNativeClearTextOK:
			case kAuthNativeRetainCredential:
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
	
	return( siResult );
} // DoBasicAuth


//------------------------------------------------------------------------------------
//	* DoSetPassword
//------------------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::DoSetPassword ( sLDAPContextData *inContext, tDataBuffer *inAuthData, char *inKerberosId,
                                      CLDAPConnectionManager *inLDAPSessionMgr, const char *inRecordType )
{
	SInt32				siResult		= eDSAuthFailed;
	char			   *pData			= nil;
	char			   *userName		= nil;
	UInt32				userNameLen		= 0;
	char			   *userPwd			= nil;
	UInt32				userPwdLen		= 0;
	char			   *rootName		= nil;
	UInt32				rootNameLen		= 0;
	char			   *rootPwd			= nil;
	UInt32				rootPwdLen		= 0;
	UInt32				offset			= 0;
	UInt32				buffSize		= 0;
	UInt32				buffLen			= 0;
	char			   *accountId		= nil;

	if ( inContext == nil ) return( (SInt32)eDSAuthFailed );
	if ( inAuthData == nil ) return( (SInt32)eDSNullAuthStepData );
	
	pData = inAuthData->fBufferData;
	buffSize = inAuthData->fBufferSize;
	buffLen = inAuthData->fBufferLength;

	if ( buffLen > buffSize ) return( (SInt32)eDSInvalidBuffFormat );

	// Get the length of the first data block and verify that it
	//	is greater than 0 and doesn't go past the end of the buffer
	//	(user name)

	if ( buffLen < 4 * sizeof( UInt32 ) + 2 ) return( (SInt32)eDSInvalidBuffFormat );
	// need length for username, password, agent username, and agent password.
	// both usernames must be at least one character
	memcpy( &userNameLen, pData, sizeof( UInt32 ) );
	pData += sizeof( UInt32 );
	offset += sizeof( UInt32 );
	if (userNameLen == 0) return( (SInt32)eDSInvalidBuffFormat );
	if (offset + userNameLen > buffLen) return( (SInt32)eDSInvalidBuffFormat );
	
	do
	{
		userName = (char *)calloc( 1, userNameLen + 1 );
		memcpy( userName, pData, userNameLen );
		pData += userNameLen;
		offset += userNameLen;
		if (offset + sizeof( UInt32 ) > buffLen) {
			siResult = eDSInvalidBuffFormat;
			break;
		}
		
		// Get the user's new password
		memcpy( &userPwdLen, pData, sizeof( UInt32 ) );
  		pData += sizeof( UInt32 );
		offset += sizeof( UInt32 );
		if (offset + userPwdLen > buffLen) {
			siResult = eDSInvalidBuffFormat;
			break;
		}
		
		//do not allow a password of zero length since LDAP poorly deals with this
		if (userPwdLen < 1) {
			siResult = eDSAuthPasswordTooShort;
			break;
		}

		userPwd = (char *)calloc( 1, userPwdLen + 1 );
		memcpy( userPwd, pData, userPwdLen );
		pData += userPwdLen;
		offset += userPwdLen;
		if (offset + sizeof( UInt32 ) > buffLen) {
			siResult = eDSInvalidBuffFormat;
			break;
		}

		// Get the agent user's name
		memcpy( &rootNameLen, pData, sizeof( UInt32 ) );
		pData += sizeof( UInt32 );
		offset += sizeof( UInt32 );
		if (rootNameLen == 0 || rootNameLen > (buffSize - offset)) {
			siResult = eDSInvalidBuffFormat;
			break;
		}

		rootName = (char *)calloc( 1, rootNameLen + 1 );
		memcpy( rootName, pData, rootNameLen );
		pData += rootNameLen;
		offset += rootNameLen;
		if (sizeof( UInt32 ) > (buffSize - offset)) {
			siResult = eDSInvalidBuffFormat;
			break;
		}

		// Get the agent user's password
		memcpy( &rootPwdLen, pData, sizeof( UInt32 ) );
		pData += sizeof( UInt32 );
		offset += sizeof( UInt32 );
		if (rootPwdLen > (buffSize - offset)) {
			siResult = eDSInvalidBuffFormat;
			break;
		}

		rootPwd = (char *)calloc( 1, rootPwdLen + 1 );
		memcpy( rootPwd, pData, rootPwdLen );
		pData += rootPwdLen;
		offset += rootPwdLen;

		if (rootName)
		{
			//here again we choose the first match
			accountId = GetDNForRecordName ( rootName, inContext, inRecordType );
		}

		CLDAPConnection *originalConnect = inContext->fLDAPConnection->Retain();
		
		// Here is the bind to the LDAP server as the agent
		siResult = inLDAPSessionMgr->AuthConnection( &(inContext->fLDAPConnection), accountId, inRecordType, inKerberosId, rootPwd );
		if ( siResult == eDSNoErr )
		{
			// we need to wait here cause the node is available to everyone else..
			LDAP *aHost = inContext->fLDAPConnection->LockLDAPSession();
			if ( aHost != NULL )
			{
				int rc = exop_password_create( aHost, accountId, userPwd );
				if ( rc != LDAP_SUCCESS ) {
					rc = standard_password_create( aHost, accountId, userPwd );
				}

				// let's update the node struct otherwise we'll have issues on reconnect
				if ( rc == LDAP_SUCCESS && strcmp(accountId, inContext->fLDAPConnection->fLDAPUsername) == 0 )
				{
					inContext->fLDAPConnection->UpdateCredentials( userPwd );
					DbgLog( kLogPlugin, "CLDAPv3Plugin: DoSetPassword - Updated session password" );
				}
				
				// *** gbv not sure what error codes to check for
				if ( ( rc == LDAP_INSUFFICIENT_ACCESS ) || ( rc == LDAP_INVALID_CREDENTIALS ) ) {
					siResult = eDSPermissionError;
				}
				else if ( rc == LDAP_NOT_SUPPORTED ) {
					siResult = eDSAuthMethodNotSupported;
				}
				else if ( rc == LDAP_SUCCESS ) {
					DbgLog( kLogPlugin, "CLDAPv3Plugin: DoSetPassword succeeded for %s", accountId );
					siResult = eDSNoErr;
				}
				else
				{
					siResult = eDSAuthFailed;
				}
				
				inContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
				inContext->fLDAPConnection->Release();
				inContext->fLDAPConnection = originalConnect; // already retained above
			}
			else
			{
				siResult = eDSCannotAccessSession;
			}
		}
		else
		{
			inContext->fLDAPConnection->Release();
			inContext->fLDAPConnection = originalConnect; // already retained above
			siResult = eDSAuthFailed;
		}
	}
	while (0);
	
	DSFreeString( userName );
	DSFreePassword( userPwd );
	DSFreeString( rootName );
	DSFreePassword( rootPwd );
	
	return( siResult );

} // DoSetPassword


//------------------------------------------------------------------------------------
//	* DoSetPasswordAsRoot
//------------------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::DoSetPasswordAsRoot ( sLDAPContextData *inContext, tDataBuffer *inAuthData, char *inKerberosId,
                                      CLDAPConnectionManager *inLDAPSessionMgr, const char *inRecordType )
{
	SInt32				siResult		= eDSAuthFailed;
	char			   *pData			= nil;
	char			   *userName		= nil;
	UInt32				userNameLen		= 0;
	char			   *newPasswd		= nil;
	UInt32				newPwdLen		= 0;
	UInt32				offset			= 0;
	UInt32				buffSize		= 0;
	UInt32				buffLen			= 0;
	char			   *accountId		= nil;
	int					rc				= LDAP_LOCAL_ERROR;

	if ( inContext == nil ) return( (SInt32)eDSAuthFailed );
	if ( inAuthData == nil ) return( (SInt32)eDSNullAuthStepData );
	
	pData = inAuthData->fBufferData;
	buffSize = inAuthData->fBufferSize;
	buffLen = inAuthData->fBufferLength;

	if ( buffLen > buffSize ) return( (SInt32)eDSInvalidBuffFormat );
	
	// Get the length of the first data block and verify that it
	//	is greater than 0 and doesn't go past the end of the buffer
	//	(user name)

	if ( buffLen < 2 * sizeof( UInt32 ) + 1 ) return( (SInt32)eDSInvalidBuffFormat );
	// need length for both username and password, plus username must be at least one character
	memcpy( &userNameLen, pData, sizeof( UInt32 ) );
	pData += sizeof( UInt32 );
	offset += sizeof( UInt32 );
	if (userNameLen == 0) return( (SInt32)eDSInvalidBuffFormat );
	if (userNameLen > (buffSize - offset)) return( (SInt32)eDSInvalidBuffFormat );
	
	do
	{
		userName = (char *)calloc( 1, userNameLen + 1 );
		memcpy( userName, pData, userNameLen );
		pData += userNameLen;
		offset += userNameLen;
		if (offset + sizeof( UInt32 ) > buffLen) {
			siResult = eDSInvalidBuffFormat;
			break;
		}

		// Get the users new password
		memcpy( &newPwdLen, pData, sizeof( UInt32 ) );
		pData += sizeof( UInt32 );
		offset += sizeof( UInt32 );
		if (newPwdLen > (buffSize - offset)) {
			siResult = eDSInvalidBuffFormat;
			break;
		}

		//do not allow a password of zero length since LDAP poorly deals with this
		if (newPwdLen < 1) {
			siResult = eDSAuthPasswordTooShort;
			break;
		}

		newPasswd = (char *)calloc( 1, newPwdLen + 1 );
		memcpy( newPasswd, pData, newPwdLen );
		pData += newPwdLen;
		offset += newPwdLen;

		if (userName)
		{
			//here again we choose the first match
			accountId = GetDNForRecordName ( userName, inContext, inRecordType );
		}

		LDAP *aHost = inContext->fLDAPConnection->LockLDAPSession();
		if (aHost != nil)
		{
			rc = exop_password_create( aHost, accountId, newPasswd );
			if ( rc != LDAP_SUCCESS )
					rc = standard_password_create( aHost, accountId, newPasswd );
			
			// let's update the node struct otherwise we'll have issues on reconnect
			if ( rc == LDAP_SUCCESS && inContext->fLDAPConnection->fLDAPUsername != NULL &&
			    strcmp( accountId, inContext->fLDAPConnection->fLDAPUsername) == 0 )
			{
				inContext->fLDAPConnection->UpdateCredentials( newPasswd );
				DbgLog( kLogPlugin, "CLDAPv3Plugin: DoSetPasswordAsRoot - Updated session password" );
			}
			
			inContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
		}
		else
		{
			rc = LDAP_LOCAL_ERROR;
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
			DbgLog( kLogPlugin, "CLDAPv3Plugin: DoSetPasswordAsRoot succeeded for %s", accountId );
            siResult = eDSNoErr;
        }
        else
        {
            siResult = eDSAuthFailed;
        }
	}
	while (0);
	
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

SInt32 CLDAPv3Plugin::DoChangePassword ( sLDAPContextData *inContext, tDataBuffer *inAuthData, char *inKerberosId,
                                      CLDAPConnectionManager *inLDAPSessionMgr, const char *inRecordType )
{
	SInt32			siResult		= eDSAuthFailed;
	char		   *pData			= nil;
	char		   *name			= nil;
	UInt32			nameLen			= 0;
	char		   *oldPwd			= nil;
	UInt32			OldPwdLen		= 0;
	char		   *newPwd			= nil;
	UInt32			newPwdLen		= 0;
	UInt32			offset			= 0;
	UInt32			buffSize		= 0;
	UInt32			buffLen			= 0;
	char		   *pathStr			= nil;
	char		   *accountId		= nil;

	if ( inContext == nil ) return( (SInt32)eDSAuthFailed );
	if ( inAuthData == nil ) return( (SInt32)eDSNullAuthStepData );

	pData = inAuthData->fBufferData;
	buffSize = inAuthData->fBufferSize;
	buffLen = inAuthData->fBufferLength;

	if ( buffLen > buffSize ) return( (SInt32)eDSInvalidBuffFormat );

	// Get the length of the first data block and verify that it
	//	is greater than 0 and doesn't go past the end of the buffer

	if ( buffLen < 3 * sizeof( UInt32 ) + 1 ) return( (SInt32)eDSInvalidBuffFormat );
	// we need at least 3 x 4 bytes for lengths of three strings,
	// and username must be at least 1 long
	memcpy( &nameLen, pData, sizeof( UInt32 ) );
	pData += sizeof( UInt32 );
	offset += sizeof( UInt32 );
	if (nameLen == 0) return( (SInt32)eDSInvalidBuffFormat );
	if (offset + nameLen > buffLen) return( (SInt32)eDSInvalidBuffFormat );

	do
	{
		name = (char *)calloc( 1, nameLen + 1 );
		memcpy( name, pData, nameLen );
		pData += nameLen;
		offset += nameLen;
		if (offset + sizeof( UInt32 ) > buffLen) {
			siResult = eDSInvalidBuffFormat;
			break;
		}

		memcpy( &OldPwdLen, pData, sizeof( UInt32 ) );
		pData += sizeof( UInt32 );
		offset += sizeof( UInt32 );
		if (offset + OldPwdLen > buffLen) {
			siResult = eDSInvalidBuffFormat;
			break;
		}

		oldPwd = (char *)calloc( 1, OldPwdLen + 1 );
		memcpy( oldPwd, pData, OldPwdLen );
		pData += OldPwdLen;
		offset += OldPwdLen;
		if (offset + sizeof( UInt32 ) > buffLen) {
			siResult = eDSInvalidBuffFormat;
			break;
		}
		
		memcpy( &newPwdLen, pData, sizeof( UInt32 ) );
   		pData += sizeof( UInt32 );
		offset += sizeof( UInt32 );
		if (offset + newPwdLen > buffLen) {
			siResult = eDSInvalidBuffFormat;
			break;
		}

		//do not allow a password of zero length since LDAP poorly deals with this
		if (newPwdLen < 1) {
			siResult = eDSAuthPasswordTooShort;
			break;
		}

		newPwd = (char *)calloc( 1, newPwdLen + 1 );
		memcpy( newPwd, pData, newPwdLen );
		pData += newPwdLen;
		offset += newPwdLen;

        // Set up a new connection to LDAP for this user

		if (name)
		{
			//here again we choose the first match
			accountId = GetDNForRecordName ( name, inContext, inRecordType );
		}

		//if username did not garner an accountId then fail authentication
		if (accountId == nil) {
			siResult = eDSAuthFailed;
			break;
		}

		// we need to save the old NodeStruct so that we can put it back..
		CLDAPConnection *origConnection = inContext->fLDAPConnection->Retain();
		
		// Here is the bind to the LDAP server as the user in question
		siResult = inLDAPSessionMgr->AuthConnection( &inContext->fLDAPConnection, accountId, inRecordType, inKerberosId, oldPwd );
		
		if ( siResult == eDSNoErr )
		{
			// we need to wait here cause the node is available to everyone else..
			LDAP *aHost = inContext->fLDAPConnection->LockLDAPSession();
			if ( aHost != NULL )
			{
				// password change algorithm: first attempt the extended operation,
				// if that fails try to change the userPassword field directly
				int rc = exop_password_create( aHost, accountId, newPwd );
				if ( rc != LDAP_SUCCESS ) {
					rc = standard_password_create( aHost, accountId, newPwd );
					if ( rc != LDAP_SUCCESS ) {
						rc = standard_password_replace( aHost, accountId, oldPwd, newPwd );\
					}
				}
				
				// let's update the node struct password otherwise we'll have issues on reconnect
				if ( rc == LDAP_SUCCESS && strcmp( accountId, inContext->fLDAPConnection->fLDAPUsername) == 0 )
				{
					inContext->fLDAPConnection->UpdateCredentials( newPwd );
					DbgLog( kLogPlugin, "CLDAPv3Plugin: DoChangePassword - Updated session password" );
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
					DbgLog( kLogPlugin, "CLDAPv3Plugin: DoChangePassword succeeded for %s", accountId );
					siResult = eDSNoErr;
				}
				else
				{
					siResult = eDSAuthFailed;
				}

				inContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
				inContext->fLDAPConnection->Release();
				inContext->fLDAPConnection = origConnection;
			}
		}
		else
		{
			inContext->fLDAPConnection->Release();
			inContext->fLDAPConnection = origConnection;
			siResult = eDSAuthFailed;
		}
	}
	while (0);
	
	DSFreeString( pathStr );
	DSFreeString( name );
	DSFreePassword( newPwd );
	DSFreePassword( oldPwd );
	
	return( siResult );

} // DoChangePassword


//--------------------------------------------------------------------------------------------------
// * ConvertXMLStrToCFDictionary
//--------------------------------------------------------------------------------------------------

CFMutableDictionaryRef CLDAPv3Plugin::ConvertXMLStrToCFDictionary( const char *xmlStr )
{
	CFDataRef				xmlData			= NULL;
	CFStringRef				errorString		= NULL;
	CFMutableDictionaryRef	cfDict			= NULL;
	
	if ( xmlStr != NULL )
	{
		xmlData = CFDataCreate( kCFAllocatorDefault, (const unsigned char *)xmlStr, strlen(xmlStr) );
		if ( xmlData != NULL )
		{
			cfDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, xmlData,
							kCFPropertyListMutableContainersAndLeaves, &errorString );
			DSCFRelease( xmlData );
		}
	}
	
	return cfDict;
}


//--------------------------------------------------------------------------------------------------
// * ConvertCFDictionaryToXMLStr
//--------------------------------------------------------------------------------------------------

char *CLDAPv3Plugin::ConvertCFDictionaryToXMLStr( CFDictionaryRef inDict, size_t *outLength )
{
	CFDataRef		xmlData			= NULL;
	const UInt8		*sourcePtr		= NULL;
	char			*returnString	= NULL;
	long			length			= 0;
	
	if ( inDict == NULL )
		return NULL;
	
	xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, (CFPropertyListRef)inDict );
	if ( xmlData == NULL )
		return NULL;
	
	sourcePtr = CFDataGetBytePtr( xmlData );
	length = CFDataGetLength( xmlData );
	if ( sourcePtr != NULL && length > 0 )
	{
		returnString = (char *) calloc( length + 1, sizeof(char) );
		if ( returnString != NULL )
			bcopy( sourcePtr, returnString, length );
	}
	
	DSCFRelease( xmlData );
	
	*outLength = length;
	
	return returnString;
}


//--------------------------------------------------------------------------------------------------
// * PWSetReplicaData ()
//
//	Note:	inAuthorityData is the UserID + RSA_key, but the IP address should be pre-stripped by
//			the calling function.
//--------------------------------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::PWSetReplicaData( sLDAPContextData *inContext, const char *inAuthorityData, CLDAPConnectionManager *inLDAPSessionMgr )
{
	SInt32					error					= eDSNoErr;
	bool					bFoundWithHash			= false;
	SInt32					replicaListLen			= 0;
	char					*rsaKeyPtr				= NULL;
    tDataBufferPtr			replyBuffer				= NULL;
	UInt32					valueCount				= 0;
	char					**valueData				= NULL;
	CFMutableDictionaryRef	replicaListDict			= NULL;
	CFStringRef				currentServerString		= NULL;
	char					*xmlStr					= NULL;
	size_t					xmlStrLen				= 0;
	char					recordName[64]			= {0};
	char					hashStr[34]				= {0};
	
	gPWSReplicaListXMLBufferMutex.WaitLock();
		
	// get /config/passwordserver_HEXHASH
	rsaKeyPtr = strchr( inAuthorityData, ',' );
	if ( rsaKeyPtr != NULL )
	{
		CC_MD5_CTX ctx;
		unsigned char pubKeyHash[CC_MD5_DIGEST_LENGTH];
	
		rsaKeyPtr++;
		CC_MD5_Init( &ctx );
		CC_MD5_Update( &ctx, rsaKeyPtr, strlen(rsaKeyPtr) );
		CC_MD5_Final( pubKeyHash, &ctx );
	
		BinaryToHexConversion( pubKeyHash, CC_MD5_DIGEST_LENGTH, hashStr );
	}
	
	if ( gPWSReplicaListXMLBuffer == NULL ||
		 gPWSReplicaListLoadedSeqNumber == 0 ||
		 gPWSReplicaListLoadedSeqNumber < gConfigRecordChangeSeqNumber ||
		 gPWSReplicaListLastRSAStr == NULL ||
		 hashStr[0] == '\0' ||
		 strcmp(gPWSReplicaListLastRSAStr, hashStr) != 0 )
	{
		// get /config/passwordserver_HEXHASH
		if ( rsaKeyPtr != NULL )
		{
			sprintf( recordName, "passwordserver_%s", hashStr );
			error = LookupAttribute( inContext, kDSStdRecordTypeConfig, recordName, kDS1AttrPasswordServerList, &valueCount, &valueData );
			if ( error == eDSNoErr && valueCount > 0 )
				bFoundWithHash = true;
		}
		
		if ( ! bFoundWithHash )
		{
			error = LookupAttribute( inContext, kDSStdRecordTypeConfig, "passwordserver", kDS1AttrPasswordServerList,
									 &valueCount, &valueData );
			if ( error != eDSNoErr ) {
				gPWSReplicaListXMLBufferMutex.SignalLock();
				
				// return eDSNoErr. We don't want to fail the auth because of a missing config record.
				return eDSNoErr;
			}
		}
		
		if ( valueCount >= 1 )
		{
			DSFreeString( gPWSReplicaListLastRSAStr );
			gPWSReplicaListLastRSAStr = strdup( hashStr );
			
			if ( gPWSReplicaListXMLBuffer != NULL ) {
				dsDataBufferDeallocatePriv( gPWSReplicaListXMLBuffer );
				gPWSReplicaListXMLBuffer = NULL;
			}
			
			replicaListLen = strlen( valueData[0] );
			gPWSReplicaListXMLBuffer = dsDataBufferAllocatePriv( replicaListLen + kSpaceForPreferredKeyValuePair );
			if ( gPWSReplicaListXMLBuffer != NULL )
			{
				gPWSReplicaListXMLBuffer->fBufferLength = replicaListLen;
				strlcpy( gPWSReplicaListXMLBuffer->fBufferData, valueData[0], replicaListLen + 1 );
				gPWSReplicaListLoadedSeqNumber = gConfigRecordChangeSeqNumber;
			}
			else
			{
				error = eMemoryError;
			}
		}
	}
	
	if ( error == eDSNoErr )
	{
		replyBuffer = dsDataBufferAllocatePriv( 1 );
		if ( replyBuffer != NULL )
		{
			char *replicaAddress = inContext->fLDAPConnection->CopyReplicaIPAddress();
			if ( replicaAddress != NULL )
			{
				// convert XML str to CFDictionary and add the preferred server
				replicaListDict = ConvertXMLStrToCFDictionary( gPWSReplicaListXMLBuffer->fBufferData );
				if ( replicaListDict != NULL )
				{
					currentServerString = CFStringCreateWithCString( kCFAllocatorDefault, replicaAddress, kCFStringEncodingUTF8 );
					if ( currentServerString != NULL ) {
						CFDictionaryAddValue( replicaListDict, CFSTR("CurrentServerForLDAP"), currentServerString );
						DSCFRelease( currentServerString );
					}
					
					xmlStr = ConvertCFDictionaryToXMLStr( replicaListDict, &xmlStrLen );
					if ( xmlStr != NULL )
					{
						bool rewriteHintFile = true;

						if ( xmlStrLen < gPWSReplicaListXMLBuffer->fBufferSize )
						{
							struct stat hintFileStat;

							// If file exists, and new data appears to match what we've already cached/written,
							// don't bother rewriting the hint file.
							if ( stat(kLDAPReplicaHintFilePath, &hintFileStat) == 0 &&
								hintFileStat.st_size == xmlStrLen &&
								gPWSReplicaListXMLBuffer->fBufferLength == xmlStrLen &&
								strncmp(gPWSReplicaListXMLBuffer->fBufferData, xmlStr, xmlStrLen) == 0 )
							{
								rewriteHintFile = false;
							}

							if ( rewriteHintFile ) {
								gPWSReplicaListXMLBuffer->fBufferLength = xmlStrLen;
								strlcpy( gPWSReplicaListXMLBuffer->fBufferData, xmlStr, xmlStrLen + 1 );
							}
						}

						if ( rewriteHintFile ) {
							bool rewriteOk = false;

							// leave a breadcrumb for the KDC realm plug-in
							unlink( kLDAPReplicaHintFilePath ); // we remove the old file and recreate
							
							// needs to be world readable because any user needs to read this file
							int fd = open( kLDAPReplicaHintFilePath, O_NOFOLLOW | O_CREAT | O_EXCL | O_WRONLY | O_EXLOCK, 0644 );
							if ( fd != -1 ) {
								ssize_t bytesWritten = write( fd, xmlStr, xmlStrLen );
								if ( bytesWritten == xmlStrLen ) {
									rewriteOk = true;
								}
								close( fd );
							}

							if ( !rewriteOk ) {
								DbgLog( kLogError, "Unable to rewrite LDAP hint file %s - %d", kLDAPReplicaHintFilePath, errno );
								unlink( kLDAPReplicaHintFilePath );
							}
						}
						
						DSFreeString( xmlStr );
					}
					
					DSCFRelease( replicaListDict );
				}
				
				DSFree( replicaAddress );
			}
			
			error = dsDoPlugInCustomCall( inContext->fPWSNodeRef, 1, gPWSReplicaListXMLBuffer, replyBuffer );
			
			dsDataBufferDeallocatePriv( replyBuffer );
		}
		else
		{
			error = eMemoryError;
		}
	}
	
	gPWSReplicaListXMLBufferMutex.SignalLock();
	
	// clean up
	if ( valueData != NULL )
	{
		for ( UInt32 index = 0; index < valueCount; index++ )
			if ( valueData[index] != NULL )
				free( valueData[index] );
		
		free( valueData );
	}
	
	return error;
}


//------------------------------------------------------------------------------------
//	* DoUnixCryptAuth
//------------------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::DoUnixCryptAuth ( sLDAPContextData *inContext, tDataBuffer *inAuthData, CLDAPConnectionManager *inLDAPSessionMgr,
		const char *inRecordType )
{
	SInt32			siResult			= eDSAuthFailed;
	char		   *pData				= nil;
	UInt32			offset				= 0;
	UInt32			buffSize			= 0;
	UInt32			buffLen				= 0;
	char		   *userName			= nil;
	SInt32			nameLen				= 0;
	char		   *pwd					= nil;
	SInt32			pwdLen				= 0;
	char		   *cryptPwd			= nil;
	char		   *pLDAPSearchBase		= nil;
    char		   *queryFilter			= nil;
	LDAPMessage	   *result				= nil;
	LDAPMessage	   *entry				= nil;
	char		   *attr				= nil;
	char		  **vals				= nil;
	BerElement	   *ber					= nil;
	int				numRecTypes			= 1;
	bool			bResultFound		= false;
	char		  **attrs				= nil;
	bool			bOCANDGroup			= false;
	CFArrayRef		OCSearchList		= nil;
	ber_int_t		scope				= LDAP_SCOPE_SUBTREE;
	SInt32			searchResult		= eDSNoErr;
	
	if ( inContext  == nil ) return( (SInt32)eDSBadContextData );
	if ( inAuthData == nil ) return( (SInt32)eDSNullDataBuff );

	pData = inAuthData->fBufferData;
	if ( pData == nil ) return( (SInt32)eDSNullDataBuff );
	
	buffSize	= inAuthData->fBufferSize;
	buffLen		= inAuthData->fBufferLength;
	if (buffLen > buffSize) return( (SInt32)eDSInvalidBuffFormat );

	if ( offset + (2 * sizeof( UInt32 ) + 1) > buffLen ) return( (SInt32)eDSInvalidBuffFormat );
	// need username length, password length, and username must be at least 1 character

	// Get the length of the user name
	memcpy( &nameLen, pData, sizeof( UInt32 ) );
	if (nameLen == 0) return( (SInt32)eDSInvalidBuffFormat );
	pData += sizeof( UInt32 );
	offset += sizeof( UInt32 );

	userName = (char *)calloc(1, nameLen + 1);
	if ( userName == nil ) return( (SInt32)eMemoryError );
	
	do
	{
		if ( offset + nameLen > buffLen ) {
			siResult = eDSInvalidBuffFormat;
			break;
		}
		
		// Copy the user name
		memcpy( userName, pData, nameLen );
		
		pData += nameLen;
		offset += nameLen;

		if ( offset + sizeof( UInt32 ) > buffLen ) {
			siResult = eDSInvalidBuffFormat;
			break;
		}
		// Get the length of the user password
		memcpy( &pwdLen, pData, sizeof( UInt32 ) );
		pData += sizeof( UInt32 );
		offset += sizeof( UInt32 );

		pwd = (char *)calloc(1, pwdLen + 1);
		if ( pwd == nil ) {
			siResult = eMemoryError;
			break;
		}

		if ( offset + pwdLen > buffLen ) {
			siResult = eDSInvalidBuffFormat;
			break;
		}
		
		// Copy the user password
		memcpy( pwd, pData, pwdLen );

		attrs = MapAttrToLDAPTypeArray( inContext, inRecordType, kDS1AttrPassword );
		
		//No Mappings, so we need to return an error that only a clear text password would work instead of no mapping/failure
		if ( attrs == nil ) {
			siResult = eDSAuthFailedClearTextOnly;
			break;
		}

		DbgLog( kLogPlugin, "CLDAPv3Plugin: Attempting Crypt Authentication" );

        // we will search over all the rectype mappings until we find the first
        // result for the search criteria in the queryfilter
        numRecTypes = 1;
		bOCANDGroup = false;
		if (OCSearchList != nil)
		{
			CFRelease(OCSearchList);
			OCSearchList = nil;
		}
		pLDAPSearchBase = MapRecToSearchBase( inContext, inRecordType, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
		//only throw this for first time since we need at least one map
		if ( pLDAPSearchBase == nil ) {
			siResult = eDSInvalidRecordType;
			break;
		}
		
		while ( (pLDAPSearchBase != nil) && (!bResultFound) )
		{
			queryFilter = BuildLDAPQueryFilter(	inContext,
												(char *)kDSNAttrRecordName,
												userName,
												eDSExact,
												false,
												inRecordType,
												pLDAPSearchBase,
												bOCANDGroup,
												OCSearchList );
			if ( queryFilter == nil ) {
				siResult = eDSNullParameter;
				break;
			}

			searchResult = DSRetrieveSynchronous( pLDAPSearchBase, attrs, inContext, scope, queryFilter, &result, NULL );

			DSDelete( queryFilter );
			DSDelete( pLDAPSearchBase );
			DSCFRelease(OCSearchList);

			numRecTypes++;
			bOCANDGroup = false;

			pLDAPSearchBase = MapRecToSearchBase( inContext, inRecordType, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
		} // while ( (pLDAPSearchBase != nil) && (!bResultFound) )

		if ( searchResult == eDSNoErr && result != NULL )
		{
			LDAP *aHost = inContext->fLDAPConnection->LockLDAPSession();
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
									int err;
									unsigned char hashResult[CC_MD5_DIGEST_LENGTH];
									CC_MD5_CTX ctx;
									char targetStr[128];
									
									err = sasl_decode64( cryptPwd, strlen(cryptPwd), targetStr, sizeof(targetStr), &outlen );
									if ( err == SASL_OK )
									{
										CC_MD5_Init( &ctx );
										CC_MD5_Update( &ctx, pwd, strlen( pwd ) );
										CC_MD5_Update( &ctx, targetStr + CC_MD5_DIGEST_LENGTH, outlen - CC_MD5_DIGEST_LENGTH );
										CC_MD5_Final( hashResult, &ctx );
										
										if ( memcmp( hashResult, targetStr, CC_MD5_DIGEST_LENGTH ) == 0 )
										{
											siResult = eDSNoErr;
										}
										bzero(hashResult, CC_MD5_DIGEST_LENGTH);
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
				}
				
				inContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
			}//if aHost != nil
		} // if bResultFound and ldapReturnCode okay

		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}
	while (0);
	
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

SInt32 CLDAPv3Plugin::DoClearTextAuth ( sLDAPContextData *inContext, tDataBuffer *inAuthData, char *inKerberosId,
		bool authCheckOnly, CLDAPConnectionManager *inLDAPSessionMgr, const char *inRecordType )
{
	SInt32				siResult			= eDSAuthFailed;
	char			   *pData				= nil;
	char			   *userName			= nil;
	char			   *accountId			= nil;
	SInt32				nameLen				= 0;
	char			   *pwd					= nil;
	SInt32				pwdLen				= 0;

	//check the authCheckOnly if true only test name and password
	//ie. need to retain current credentials
	if ( inContext  == nil ) return( (SInt32)eDSBadContextData );
	if ( inAuthData == nil ) return( (SInt32)eDSNullDataBuff );

	pData = inAuthData->fBufferData;
	if ( pData == nil ) return( (SInt32)eDSNullDataBuff );
	
	// Get the length of the user name
	memcpy( &nameLen, pData, sizeof( SInt32 ) );
	//accept the case of a NO given name and NO password
	//LDAP servers use this to reset the credentials
	//if (nameLen == 0) return( (SInt32)eDSAuthInBuffFormatError );

	do
	{
		if (nameLen > 0)
		{
			userName = (char *) calloc(1, nameLen + 1);
			if ( userName == nil ) {
				siResult = eMemoryError;
				break;
			}

			// Copy the user name
			pData += sizeof( SInt32 );
			memcpy( userName, pData, nameLen );
			pData += nameLen;
		}

		// Get the length of the user password
		memcpy( &pwdLen, pData, sizeof( SInt32 ) );
		//accept the case of a given name and NO password
		//LDAP servers use this as tracking info when no password is required
		//if (pwdLen == 0) {
		//	siResult = eDSAuthInBuffFormatError;
		//	break;
		//}
		
		if (pwdLen > 0)
		{
			pwd = (char *) calloc(1, pwdLen + 1);
			if ( pwd == nil ) {
				siResult = eMemoryError;
				break;
			}

			// Copy the user password
			pData += sizeof( SInt32 );
			memcpy( pwd, pData, pwdLen );
			pData += pwdLen;
		}

		DbgLog( kLogPlugin, "CLDAPv3Plugin: Attempting Cleartext Authentication" );

		//get the correct account id
		//we assume that the DN is always used for this
		if (userName)
		{
			//here again we choose the first match
			accountId = GetDNForRecordName ( userName, inContext, inRecordType );
		}
                
		//if username did not garner an accountId then fail authentication
		if (accountId == nil) {
			siResult = eDSAuthFailed;
			break;
		}
		
		if ((pwd != NULL) && (pwd[0] != '\0') && (nameLen != 0))
		{
			if (!authCheckOnly)
			{
				//no need to lock session cause the AuthOpen will do it
				siResult = inLDAPSessionMgr->AuthConnection( &(inContext->fLDAPConnection),
															 accountId,
															 inRecordType,
															 inKerberosId,
															 pwd );
				
				if ( siResult == eDSCannotAccessSession )
				{
					siResult = eDSAuthMasterUnreachable;
				}
			}
			else //no session ie. authCheckOnly
			{
				siResult = inLDAPSessionMgr->VerifyCredentials( inContext->fLDAPConnection,
																accountId,
																inRecordType, 
																inKerberosId, 
																pwd );
			}
		}
	}
	while ( 0 );

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


bool CLDAPv3Plugin::KDCHasNonLocalRealm( sLDAPContextData *inContext )
{
	bool hasNonLocalRealm = false;
	UInt32 attrIndex = 0;
	UInt32 attrCount = 0;
	char **attrValues = NULL;
	tDirStatus status = LookupAttribute( inContext, kDSStdRecordTypeConfig, "KerberosKDC", kDS1AttrKDCConfigData, &attrCount, &attrValues );
	if ( status == eDSNoErr )
	{
		for ( attrIndex = 0; attrIndex < attrCount; attrIndex++ )
		{
			// This attribute should be single-valued. If there are 
			// additional attributes, they are ignored. Iteration
			// continues for all values to free memory.
			
			if ( attrIndex == 0 )
			{
				char *tptr = strstr( attrValues[0], kKDCRecordRealmToken );
				if ( tptr != NULL )
				{
					tptr += sizeof(kKDCRecordRealmToken) - 1;
					
					// terminate before next section
					char *endPtr = strchr( tptr, '[' );
					if ( endPtr != NULL )
						*endPtr = '\0';
					
					// skip whitespace
					while ( *tptr && isspace(*tptr) )
						tptr++;
					
					if ( *tptr != '\0' )
					{
						for ( endPtr = tptr; endPtr != NULL; )
						{
							endPtr = strstr( tptr, kKDCEndOfRealmNameMarker );
							if ( endPtr != NULL )
							{
								*endPtr = '\0';
								endPtr += sizeof(kKDCEndOfRealmNameMarker) - 1;
							
								if ( tptr != NULL )
								{
									if ( strstr(tptr, "LKDC") == NULL )
										hasNonLocalRealm = true;
									
									// skip definition, enforce syntax
									endPtr = strchr( endPtr, '}' );
									if ( endPtr == NULL )
										break;
									
									tptr = ++endPtr;
									
									// skip whitespace
									while ( *tptr && isspace(*tptr) )
										tptr++;
								}
							}
						}
					}
				}
			}
			
			DSFreeString( attrValues[attrIndex] );
		}
		
		DSFree( attrValues );
	}
	
	return hasNonLocalRealm;
}
