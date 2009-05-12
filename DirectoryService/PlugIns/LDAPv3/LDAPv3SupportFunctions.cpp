/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#include <syslog.h>
#include <DirectoryServiceCore/CSharedData.h>
#include "LDAPv3SupportFunctions.h"
#include "DirServicesUtilsPriv.h"
#include "CDSPluginUtils.h"
#include "CLDAPConnection.h"
#include "CLDAPNodeConfig.h"
#include "DSLDAPUtils.h"
#include "DSUtils.h"

#define kKerberosPrefsFilePath			"/Library/Preferences/edu.mit.Kerberos"

#define kKerberosPrefsRecordTemplate	"\
<?xml version=\"1.0\" encoding=\"UTF-8\"?>	\
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">	\
<plist version=\"1.0\">		\
<dict>						\
	<key>KADM_List</key>	\
	<array>					\
		<string>%s</string>	\
	</array>				\
	<key>KDC_List</key>		\
	<array>					\
		<string>%s</string>	\
	</array>				\
</dict>						\
</plist>"


//------------------------------------------------------------------------------------
//	* DSCheckForLDAPResult
//------------------------------------------------------------------------------------

static tDirStatus DSCheckForLDAPResult( LDAP			*inHost,
									    int32_t			inSearchTimeout, 
									    int				inHowMany,
									    int				inLDAPMsgId,
									    LDAPMessage		**outResult )
{
	tDirStatus	siResult	= eDSCannotAccessSession;
	
	if ( inHost != NULL )
	{
		struct	timeval	tv = { inSearchTimeout, 0 };
		
		int rc = ldap_result( inHost, inLDAPMsgId, inHowMany, &tv, outResult );
		switch ( rc )
		{
			case LDAP_RES_SEARCH_ENTRY:
				// we have an entry, let's just break out because means we have something
				siResult = eDSNoErr;
				break;
			case LDAP_RES_SEARCH_RESULT:
			{
				int		ldapErr		= LDAP_SUCCESS;
				char*	ldapErrMsg	= NULL;

				// check the final result block to see if there are any server errors.
				rc = ldap_parse_result( inHost, *outResult, &ldapErr, NULL, &ldapErrMsg, NULL, NULL, 0);
				if ( rc == LDAP_SUCCESS )
				{
					switch( ldapErr )
					{
						case LDAP_ADMINLIMIT_EXCEEDED:
						case LDAP_SIZELIMIT_EXCEEDED:
							DbgLog( kLogNotice, "CLDAPv3::DSCheckForLDAPResult - LDAP request limit exceeded" );
						case LDAP_SUCCESS:
						case LDAP_NO_SUCH_OBJECT:
							siResult = eDSRecordNotFound;  // normal successful return code
							break;
						case LDAP_TIMEOUT:
						case LDAP_TIMELIMIT_EXCEEDED:
							DbgLog( kLogNotice, "CLDAPv3::DSCheckForLDAPResult - LDAP request timed out" );
						default:
							siResult = eDSOperationFailed;
							DbgLog( kLogPlugin, "CLDAPv3::DSCheckForLDAPResult - LDAP server search result error %d: %s",
								    ldapErr, (ldapErrMsg == NULL || ldapErrMsg[0] == '\0') ? "Unknown error" : ldapErrMsg );
							break;
					}
				}
				else
				{
					siResult = eDSOperationFailed;
					DbgLog( kLogPlugin, "CLDAPv3::DSCheckForLDAPResult - ldap_parse_result() failed, return code = %d", rc );
				}
				
				DSFree( ldapErrMsg );
 				break;
			}
			case 0:
				// we timed out during the search we'll assume the server is bad
			case -1:
				siResult = eDSCannotAccessSession;
				break;
			default:
				DbgLog( kLogPlugin, "CLDAPv3::DSCheckForLDAPResult - unexpected result from LDAP (result=%d).", rc );
				siResult = eUndefinedError;
				break;
		}
	}
	
	if ( siResult != eDSNoErr && (*outResult) != NULL )
	{
		ldap_msgfree( (*outResult) );
		(*outResult) = NULL;
	}
	
	return siResult;
}

tDirStatus DSInitiateOrContinueSearch( sLDAPContextData		*inContext,
									   sLDAPContinueData	*inContinue,
									   char					*inSearchBase,
									   char					**inAttrList,
									   ber_int_t			inScope,
									   char					*inQueryFilter,
									   LDAPMessage			**outResult )
{
	tDirStatus	siResult	= eDSCannotAccessSession;
	
	(*outResult) = NULL;
	
	LDAP *aHost = inContext->fLDAPConnection->LockLDAPSession();
	if ( aHost != NULL )
	{
		// if we have no Msg ID then this search was never initiated
		if ( inContinue->fLDAPMsgId == 0 )
		{
			int		iRecCount	= 0;
			int		numRetries	= 2;
			
			// only request what we can use
			if ( inContinue->fLimitRecSearch != 0 )
				iRecCount = inContinue->fLimitRecSearch - inContinue->fTotalRecCount;
			
			do 
			{
				int rc = ldap_search_ext( aHost, inSearchBase, inScope, inQueryFilter, inAttrList, 0, NULL, NULL, 0,
										  iRecCount, &(inContinue->fLDAPMsgId) );
				
				// we check for errors right off the bat
				switch ( rc )
				{
					case LDAP_FILTER_ERROR:
						siResult = eParameterError;
						break;
					case LDAP_SUCCESS:
						siResult = eDSNoErr;
						break;
					case LDAP_UNAVAILABLE:
					case LDAP_SERVER_DOWN:
					case LDAP_BUSY:
					case LDAP_LOCAL_ERROR:
					case LDAP_TIMEOUT:
						siResult = eDSCannotAccessSession;
						break;
					default:
						siResult = eDSRecordNotFound;
						break;
				}
				
				// here we look for the first result
				if ( siResult == eDSNoErr )
				{
					inContinue->fRefLD = aHost;
					siResult = DSCheckForLDAPResult( aHost, inContext->fLDAPConnection->fNodeConfig->fSearchTimeout, LDAP_MSG_ONE,
													 inContinue->fLDAPMsgId, outResult );
				}
				
				if ( siResult != eDSNoErr && inContinue->fLDAPMsgId > 0 )
				{
					ldap_abandon_ext( aHost, inContinue->fLDAPMsgId, NULL, NULL );
					inContinue->fLDAPMsgId = 0;
					inContinue->fRefLD = NULL;
				}
				
				if ( siResult == eDSCannotAccessSession )
				{
					DbgLog( kLogPlugin, "CLDAPv3::DSInitiateOrContinueSearch - search initiate failed trying %d more times", numRetries );
					
					// if we abandon or close this session the outResult could be invalid
					if ( (*outResult) != NULL )
					{
						ldap_msgfree( *outResult );
						(*outResult) = NULL;
					}
					
					// first abandon the message ID so we can try to initiate it again
					if ( inContinue->fLDAPMsgId > 0 )
					{
						ldap_abandon_ext( aHost, inContinue->fLDAPMsgId, NULL, NULL );
						inContinue->fLDAPMsgId = 0;
						inContinue->fRefLD = NULL;
					}
					
					// if we're out of retries let's break out of the loop
					if ( numRetries-- <= 0 )
						break;
					
					// unlock our last session
					inContext->fLDAPConnection->UnlockLDAPSession( aHost, true );
					
					// let's try to get a host handle again, if we can't, we'll exit loop and the 2nd unlock will not
					// do anything if aHost is NULL
					aHost = inContext->fLDAPConnection->LockLDAPSession();
				}
				
			} while ( siResult == eDSCannotAccessSession && aHost != NULL );
		}
		else
		{
			// if we have a result in our continue, just return that, otherwise, we check for a new one
			if ( inContinue->fResult != NULL )
			{
				(*outResult) = inContinue->fResult;
				inContinue->fResult = NULL;
				siResult = eDSNoErr;
			}
			else
			{
				siResult = DSCheckForLDAPResult( aHost, inContext->fLDAPConnection->fNodeConfig->fSearchTimeout, LDAP_MSG_ONE,
												 inContinue->fLDAPMsgId, outResult );
			}
		}
		
		// if we lost our connection or we are done, let's clean up
		if ( siResult == eDSCannotAccessSession || siResult == eDSRecordNotFound )
		{
			// if we abandon or close this session the outResult will be invalid
			if ( (*outResult) != NULL )
			{
				ldap_msgfree( *outResult );
				(*outResult) = NULL;
			}
			
			if ( inContinue->fLDAPMsgId > 0 )
			{
				if ( aHost == inContinue->fRefLD )
					ldap_abandon_ext( aHost, inContinue->fLDAPMsgId, NULL, NULL );
				
				inContinue->fLDAPMsgId = 0;
				inContinue->fRefLD = NULL;
			}
		}
		
		inContext->fLDAPConnection->UnlockLDAPSession( aHost, (siResult == eDSCannotAccessSession) );
	}
	
	return siResult;
}

tDirStatus DSRetrieveSynchronous( char				*inSearchBase, 
								  char				**inAttrs, 
								  sLDAPContextData	*inContext, 
								  ber_int_t			inScope,
								  char				*inQueryFilter,
								  LDAPMessage		**outResult,
								  char				**outDN )
{
	tDirStatus	siResult	= eDSRecordNotFound;
	int			numRetries	= 2;
	
	(*outResult) = NULL;
	
	LDAP *aHost = inContext->fLDAPConnection->LockLDAPSession();
	if ( aHost != NULL )
	{
		struct timeval tv = { inContext->fLDAPConnection->fNodeConfig->fSearchTimeout, 0 };
		
		do
		{
			int rc = ldap_search_ext_s( aHost, inSearchBase, inScope, inQueryFilter, inAttrs, false, NULL, NULL, &tv, 0, outResult );
			if ( rc == LDAP_SUCCESS && (*outResult) != NULL )
			{
				if ( ldap_msgtype(*outResult) == LDAP_RES_SEARCH_ENTRY )
				{
					// if we had no error and we have a result return eDSNoErr;
					siResult = eDSNoErr;
					if ( outDN != NULL )
						(*outDN) = ldap_get_dn( aHost, (*outResult) );
				}
				else
				{
					// otherwise just free the result
					ldap_msgfree( *outResult );
					(*outResult) = NULL;
				}
			}
			else if ( IsFatalLDAPError(rc) )
			{
				siResult = eDSCannotAccessSession;
				inContext->fLDAPConnection->UnlockLDAPSession( aHost, true );
				
				// now try to get a handle again
				aHost = inContext->fLDAPConnection->LockLDAPSession();
			}
		} while ( siResult == eDSCannotAccessSession && aHost != NULL && numRetries-- > 0 );
		
		inContext->fLDAPConnection->UnlockLDAPSession( aHost, (siResult == eDSCannotAccessSession) );
	}
	else
	{
		siResult = eDSCannotAccessSession;
	}
	
	return siResult;
}

//------------------------------------------------------------------------------------
//	* GetDNForRecordName
//------------------------------------------------------------------------------------

char *GetDNForRecordName( char				*inRecName,
						  sLDAPContextData	*inContext,
						  const char		*inRecordType )
{
	char			   *ldapDN			= NULL;	
	char			   *pLDAPSearchBase	= NULL;
	char			   *queryFilter		= NULL;
	LDAPMessage		   *result			= NULL;
	bool				bOCANDGroup		= false;
	CFArrayRef			OCSearchList	= NULL;
	ber_int_t			scope			= LDAP_SCOPE_SUBTREE;
	tDirStatus			searchResult	= eDSNoErr;
	
	if ( inRecName == nil ) return NULL;
	if ( inContext == nil ) return NULL;
	
	CLDAPNodeConfig *nodeConfig = inContext->fLDAPConnection->fNodeConfig;
	if ( nodeConfig == NULL ) return NULL;
	
	pLDAPSearchBase = nodeConfig->MapRecToSearchBase( inRecordType, 1, &bOCANDGroup, &OCSearchList, &scope );
	if ( pLDAPSearchBase == nil ) return NULL;
	
	queryFilter = nodeConfig->BuildLDAPQueryFilter(	(char *)kDSNAttrRecordName, inRecName, eDSExact, false, inRecordType,
												    pLDAPSearchBase, bOCANDGroup, OCSearchList );
	
	if ( queryFilter != NULL )
		searchResult = DSRetrieveSynchronous( pLDAPSearchBase, NULL, inContext, scope, queryFilter, &result, &ldapDN );
	
	if ( result != NULL )
	{
		ldap_msgfree( result );
		result = NULL;
	}
	
	DSCFRelease( OCSearchList );
	DSDelete( pLDAPSearchBase );
	DSDelete( queryFilter );
	
	return( ldapDN );
	
} // GetDNForRecordName

// ---------------------------------------------------------------------------
//	* ldapParseAuthAuthority
//
//	@discussion:
//	retrieve version, tag, and data from authauthority
//	format is version;tag;data
//	This method is a local wrapper that returns segment
//	#4 instead of #3 for the data if the authentication authority is
//	Kerberosv5.
// ---------------------------------------------------------------------------

tDirStatus ldapParseAuthAuthority( const char	*inAuthAuthority,
								   char			**outVersion,
								   char			**outAuthTag,
								   char			**outAuthData )
{
	char **authDataArray = NULL;
	int	keepIndex = 0;
	
	tDirStatus status = dsParseAuthAuthorityExtended( inAuthAuthority, outVersion, outAuthTag, &authDataArray );
	if ( status != eDSNoErr )
		return status;
	
	// if this is kerberos auth authority, we care about principal, aaData for auths, so skip authData
	if( *outAuthTag && strcmp(*outAuthTag, kDSTagAuthAuthorityKerberosv5) == 0 )
		keepIndex = 1;
	
	for ( int idx = 0; authDataArray[idx] != NULL; idx++ )
	{
		if ( idx == keepIndex )
			*outAuthData = authDataArray[idx];
		else
			DSFreeString( authDataArray[idx] );
	}
	
	DSFree( authDataArray );
	
	return eDSNoErr;
}


// ---------------------------------------------------------------------------
//	* VerifyKerberosForRealm
//
//	@discussion:
//	This function ensures the requested realm exists in the kerberos file.
// ---------------------------------------------------------------------------

void VerifyKerberosForRealm( const char *inRealmName, const char *inServer )
{
	tDirStatus				siResult				= eDSNoErr;
	tDirReference			dsRef					= 0;
	tDataListPtr			nodeDL					= NULL;
	tDirNodeReference		dsNodeRef				= 0;
	tDataNodePtr			pRecType				= NULL;
	tRecordReference		recRef					= 0;
	tDataNodePtr			pRecName				= NULL;
	tDataNodePtr			pAttrName				= NULL;
	tDataList				dataList				= { 0 };
	int						error_num				= 0;
	struct hostent			*hostEntryTemp			= NULL;
	struct hostent			*hostEntry				= NULL;
	char					recNameStr[256]			= {0,};
	char					*kprefTemplateStr		= NULL;
	size_t					kprefTemplateStrLen		= 0;
	
	if ( inServer == NULL )
	{
		DbgLog( kLogPlugin, "VerifyKerberosForRealm - inServer == NULL" );
		return;
	}
	
	// let's resolve the incoming name into a host name in case it is a dotted ip address
	hostEntryTemp = getipnodebyname(inServer, AF_INET, AI_DEFAULT, &error_num);
	if ( hostEntryTemp == NULL )
	{
		DbgLog( kLogPlugin, "VerifyKerberosForRealm - getipnodebyname returned NULL for server %s", inServer );
		return;
	}
	
	hostEntry = getipnodebyaddr( hostEntryTemp->h_addr_list[0], hostEntryTemp->h_length, hostEntryTemp->h_addrtype, &error_num );
	freehostent( hostEntryTemp );
	hostEntryTemp = NULL;
	
	if ( hostEntry == NULL )
	{
		DbgLog( kLogPlugin, "VerifyKerberosForRealm - gethostbyaddr returned NULL for server %s", inServer );
		return;
	}
		
	siResult = dsOpenDirService( &dsRef );
	if ( siResult != eDSNoErr )
		goto cleanup;
	
	nodeDL = dsBuildFromPathPriv( kstrDefaultLocalNodeName, "/" );
	siResult = dsOpenDirNode( dsRef, nodeDL, &dsNodeRef );
	dsDataListDeallocatePriv( nodeDL );
	free( nodeDL );
	
	if ( siResult != eDSNoErr )
	{
		DbgLog( kLogPlugin, "VerifyKerberosForRealm - Error %d while opening Directory Node " kstrDefaultLocalNodeName, siResult );
		goto cleanup;
	}
	
	pRecType = dsDataNodeAllocateString( dsRef, kDSStdRecordTypeConfig );
	snprintf( recNameStr, sizeof(recNameStr), "Kerberos:%s", inRealmName );
	pRecName = dsDataNodeAllocateString( dsRef, recNameStr );
	siResult = dsOpenRecord( dsNodeRef, pRecType, pRecName, &recRef );
	switch ( siResult )
	{
		case eDSNoErr:
			// record exists, don't need to do anything
			goto cleanup;
		
		case eDSRecordNotFound:
			// add
			break;
		
		default:
			DbgLog( kLogPlugin, "VerifyKerberosForRealm - Error %d while retrieving record /Config/%s", siResult, recNameStr );
			goto cleanup;
	}
	
	// eDSRecordNotFound case
	
	siResult = dsCreateRecordAndOpen( dsNodeRef, pRecType, pRecName, &recRef );
	if ( siResult != eDSNoErr )
	{
		DbgLog( kLogPlugin, "VerifyKerberosForRealm - Error %d attempting to create and open record /Config/%s",
				siResult, recNameStr );
		goto cleanup;
	}
	
	pAttrName = dsDataNodeAllocateString( dsRef, kDS1AttrXMLPlist );
	
	kprefTemplateStr = (char *)malloc( (kprefTemplateStrLen = sizeof(kKerberosPrefsRecordTemplate) + strlen(hostEntry->h_name)*2) );
	snprintf( kprefTemplateStr, kprefTemplateStrLen, kKerberosPrefsRecordTemplate, hostEntry->h_name, hostEntry->h_name );
	dsBuildListFromStringsAlloc( dsRef, &dataList, kprefTemplateStr, NULL );
	siResult = dsSetAttributeValues( recRef, pAttrName, &dataList );
	if ( siResult != eDSNoErr )
	{
		DbgLog( kLogPlugin, "VerifyKerberosForRealm - Error %d attempting to set XMLPlist in record /Config/%s",
				siResult, recNameStr );
	}
	
	dsDataListDeallocatePriv( &dataList );
	
cleanup:
	DSFreeString( kprefTemplateStr );
	if ( pAttrName != NULL )
		dsDataNodeDeAllocate( dsRef, pAttrName );
	if ( pRecName != NULL )
		dsDataNodeDeAllocate( dsRef, pRecName );
	if ( pRecType != NULL )
		dsDataNodeDeAllocate( dsRef, pRecType );
	if ( recRef != 0 )
		dsCloseRecord( recRef );
	if ( dsNodeRef != 0 )
		dsCloseDirNode( dsNodeRef );
	if ( dsRef != 0 )
		dsCloseDirService( dsRef );
	if ( hostEntry != NULL )
		freehostent( hostEntry );
}

