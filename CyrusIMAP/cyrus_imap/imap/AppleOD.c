/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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


#include "AppleOD.h"

#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdio.h>
#include <unistd.h>
#include <membershipPriv.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/fcntl.h>
#include <uuid/uuid.h>
#include <sys/errno.h>

#include "imap_err.h"
#include "xmalloc.h"
#include "libconfig.h"

#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPropertyList.h>

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>


static tDirStatus	sOpen_ds			( tDirReference *inOutDirRef );
static tDirStatus	sGet_search_node	( tDirReference inDirRef, tDirNodeReference *outSearchNodeRef );
static tDirStatus	sGet_user_attributes( tDirReference inDirRef, tDirNodeReference inSearchNodeRef, const char *inUserID, struct od_user_opts *inOutOpts );
static tDirStatus	sOpen_user_node		( tDirReference inDirRef, const char *inUserLoc, tDirNodeReference *outUserNodeRef );
static int			sVerify_version		( CFDictionaryRef inCFDictRef );
static void			sGet_mail_values	( char *inMailAttribute, struct od_user_opts *inOutOpts );
static void			sGet_acct_state		( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts );
static void			sGet_auto_forward	( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts );
static void			sGet_IMAP_login		( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts );
static void			sGet_POP3_login		( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts );
static void			sGet_disk_quota		( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts );
static void			sGet_acct_loc		( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts );
static void			sGet_alt_loc		( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts );
static int			sDoCryptAuth		( tDirReference inDirRef, tDirNodeReference inUserNodeRef, const char *inUserID, const char *inPasswd );
static int			sValidateResponse	( const char *inChallenge, const char *inResponse, const char *inAuthType, struct od_user_opts *inOutOpts );
static int			sGetUserOptions		( const char *inUserID, struct od_user_opts *inOutOpts );
static int			sEncodeBase64		( const char *inStr, const int inLen, char *outStr, int outLen );
static int			sDecodeBase64		( const char *inStr, const int inLen, char *outStr, int outLen, int *destLen );
static void			get_random_chars	( char *out_buf, int in_len );
static int			checkServiceACL		( struct od_user_opts *inOutOpts, const char *inGroup );
static int			sDoCRAM_MD5_Auth	( struct protstream *inStreamIn, struct protstream *inStreamOut, struct od_user_opts *inOutOpts );
static int			sDoCRAM_MD5_AuthS	( struct protstream *inStreamIn, struct protstream *inStreamOut, struct od_user_opts *inOutOpts );
static int			sDoPlainAuth		( const char *inCont, const char *inResp, struct protstream *inStreamIn, struct protstream *inStreamOut, struct od_user_opts *inOutOpts );
static int			sDoPlainAuthS		( const char *inCont, const char *inResp, struct protstream *inStreamIn, struct protstream *inStreamOut, struct od_user_opts *inOutOpts );
static int			sDoLoginAuth		( struct protstream *inStreamIn, struct protstream *inStreamOut, struct od_user_opts *inOutOpts );
static int			sDoLoginAuthS		( struct protstream *inStreamIn, struct protstream *inStreamOut, struct od_user_opts *inOutOpts );
static int			sGetClientResponse	( char *inOutBuf, int inBufSize, struct protstream *inStreamIn );

/* -----------------------------------------------------------
	- odGetUserOpts

		Get user options from user record
 * ----------------------------------------------------------- */

int odGetUserOpts ( const char *inUserID, struct od_user_opts *inOutOpts )
{
	int		iResult	= 0;

	if ( inOutOpts != NULL )
	{
		if ( (inUserID != NULL) && (inOutOpts->fUserIDPtr != NULL) )
		{
			if ( strcasecmp( inUserID, inOutOpts->fUserIDPtr ) == 0 )
			{
				if ( (inOutOpts->fRecNamePtr != NULL) && (inOutOpts->fUserLocPtr) )
				{
					syslog( LOG_DEBUG, "AOD: user opts: no-lookup for: %s", inUserID );
					return( eDSNoErr );
				}
			}
		}

		/* clean up */
		odFreeUserOpts( inOutOpts, 0 );

		/* set default settings */
		inOutOpts->fAccountState = eUnknownState;
		inOutOpts->fDiskQuota = 0;

		/* get user options for user record or service acl */
		iResult = sGetUserOptions( inUserID, inOutOpts );

		/* do we have a valid user record name */
		if ( (iResult == eDSNoErr) && (inOutOpts->fRecNamePtr != NULL) )
		{
			/* set the user ID */
			inOutOpts->fUserIDPtr = malloc( strlen( inUserID ) + 1 );
			if ( inOutOpts->fUserIDPtr != NULL )
			{
				strlcpy( inOutOpts->fUserIDPtr, inUserID, strlen( inUserID ) + 1 );
			}
			
			/* are mail service ACL's enabled */
			checkServiceACL( inOutOpts, "mail" );
		}

		/* if we failed to find a user record, set record name to user id and
			mark user opts with unknown user */
		if ( inOutOpts->fRecNamePtr == NULL )
		{
			if ( inUserID != NULL )
			{
				inOutOpts->fRecNamePtr = malloc( strlen( inUserID ) + 1 );
				if ( inOutOpts->fRecNamePtr != NULL )
				{
					strlcpy( inOutOpts->fRecNamePtr, inUserID, strlen( inUserID ) + 1 );
				}
			}
			inOutOpts->fAccountState |= eUnknownUser;
		}
	}

	return( iResult );

} /* odGetUserOpts */


/* -----------------------------------------------------------
	- odFreeUserOpts

		Free user opts memory
 * ----------------------------------------------------------- */

void odFreeUserOpts ( struct od_user_opts *inUserOpts, int inFreeOD )
{
	if ( inUserOpts != NULL )
	{
		syslog( LOG_DEBUG, "AOD: user opts: cleaning up user options structure" );

		inUserOpts->fAccountState = eUnknownState;

		if ( inUserOpts->fUserIDPtr != NULL )
		{
			free( inUserOpts->fUserIDPtr );
			inUserOpts->fUserIDPtr = NULL;
		}

		if ( inUserOpts->fUserUUID != NULL )
		{
			free( inUserOpts->fUserUUID );
			inUserOpts->fUserUUID = NULL;
		}

		if ( inUserOpts->fAuthIDNamePtr != NULL )
		{
			free( inUserOpts->fAuthIDNamePtr );
			inUserOpts->fAuthIDNamePtr = NULL;
		}

		if ( inUserOpts->fRecNamePtr != NULL )
		{
			free( inUserOpts->fRecNamePtr );
			inUserOpts->fRecNamePtr = NULL;
		}

		if ( inUserOpts->fAccountLocPtr != NULL )
		{
			free( inUserOpts->fAccountLocPtr );
			inUserOpts->fAccountLocPtr = NULL;
		}

		if ( inUserOpts->fAltDataLocPtr != NULL )
		{
			free( inUserOpts->fAltDataLocPtr );
			inUserOpts->fAltDataLocPtr = NULL;
		}

		if ( inUserOpts->fAutoFwdPtr != NULL )
		{
			free( inUserOpts->fAutoFwdPtr );
			inUserOpts->fAutoFwdPtr = NULL;
		}

		if ( inUserOpts->fUserLocPtr != NULL )
		{
			free( inUserOpts->fUserLocPtr );
			inUserOpts->fUserLocPtr = NULL;
		}

		if ( inFreeOD )
		{
			if ( inUserOpts->fSearchNodeRef )
			{
				syslog( LOG_DEBUG, "AOD: user opts: releasing search node reference" );
				(void)dsCloseDirNode( inUserOpts->fSearchNodeRef );
				inUserOpts->fSearchNodeRef = 0;
			}
			if ( inUserOpts->fDirRef )
			{
				syslog( LOG_DEBUG, "AOD: user opts: releasing directory reference" );
				(void)dsCloseDirService( inUserOpts->fDirRef );
				inUserOpts->fDirRef = 0;
			}
		}
	}
} /* odFreeUserOpts */


/* -----------------------------------------------------------------
	odCheckPass ()
	
	- inUserOpts must contain:
		- valid directory reference
		- valid user name
		- valid user node meta location
   ----------------------------------------------------------------- */

int odCheckPass ( const char *inPasswd, struct od_user_opts *inUserOpts )
{
	int					iResult			= eAODParamErr;
	tDirStatus			dsStatus		= eDSNoErr;
	tDirReference		dirRef			= 0;
	tDirNodeReference	userNodeRef		= 0;

	if ( (inUserOpts == NULL) || (inUserOpts->fRecNamePtr == NULL) || (inPasswd == NULL) )
	{
		syslog( LOG_ERR, "AOD: check pass: configuration error" );
		return( eAODParamErr );
	}

	dirRef = inUserOpts->fDirRef;

	if ( (inUserOpts != NULL) && (inUserOpts->fUserLocPtr != NULL) )
	{
		dsStatus = sOpen_user_node( dirRef, inUserOpts->fUserLocPtr, &userNodeRef );
		if ( dsStatus == eDSNoErr )
		{
			dsStatus = sDoCryptAuth( dirRef, userNodeRef, inUserOpts->fRecNamePtr, inPasswd );
			switch ( dsStatus )
			{
				case eDSNoErr:
					iResult = eAODNoErr;
					break;

				case eDSAuthNewPasswordRequired:
					syslog( LOG_INFO, "AOD: check pass: new password required for user: %s", inUserOpts->fRecNamePtr );
					iResult = eAODNoErr;
					break;

				case eDSAuthPasswordExpired:
					syslog( LOG_INFO, "AOD: check pass: password expired for user: %s", inUserOpts->fRecNamePtr );
					iResult = eAODNoErr;
					break;

				default:
					syslog( LOG_INFO, "AOD: check pass: %d", dsStatus );
					iResult = eAODAuthFailed;
					break;
			}
			(void)dsCloseDirNode( userNodeRef );
		}
		else
		{
			syslog( LOG_ERR, "AOD: check pass: cannot open user directory node for user: %s (%d)", inUserOpts->fRecNamePtr, dsStatus );
			iResult = eAODCantOpenUserNode;
		}
	}

	return( iResult );

} /* odCheckPass */


/* -----------------------------------------------------------------
	odCheckAPOP ()
	
	- APOP digets apopuser 3ea069c07cb39843c34e9fe7e0dee9ea:
   ----------------------------------------------------------------- */

int odCheckAPOP ( const char *inChallenge, const char *inResponse, struct od_user_opts *inOutOpts )
{
	int		iResult	= eAODParamErr;
	char	*p		= NULL;
	int		len		= 0;
	char	userBuf	[ MAX_USER_BUF_SIZE ];

	/* check for bogus data */
	if ( (inChallenge == NULL) || (inResponse == NULL) )
	{
		syslog( LOG_DEBUG, "AOD: APOP: configuration error: empty challenger or response" );
		return( iResult );
	}

	/* extract the user ID from the response */
	p = strchr( inResponse, ' ' );
	if ( (p == NULL) || (strspn(p + 1, "0123456789abcdef") != 32) )
	{
		syslog( LOG_DEBUG, "AOD: APOP: parameter error: bad digest: %s", inResponse );
		return( eAODParamErr );
	}

	/* user ID length */
	len = (int)(p - inResponse);
	if ( len < MAX_USER_BUF_SIZE )
	{
		memset( userBuf, 0, MAX_USER_BUF_SIZE );
		memcpy( userBuf, inResponse, len );

		/* get user options */
		odGetUserOpts( userBuf, inOutOpts );

		/* consuem any spaces */
		while ( *p == ' ' )
		{
			p++;
		}

		if ( p != NULL )
		{
			/* make the call */
			return( sValidateResponse( inChallenge, p, kDSStdAuthAPOP, inOutOpts ) );
		}
		else
		{
			syslog( LOG_DEBUG, "AOD: APOP: configuration error: bad APOP digest: (%s)", inResponse );
			iResult = eAODConfigError;
		}
	}
	else
	{
		syslog( LOG_DEBUG, "AOD: APOP: username exceeded maximum limit: (%s)", inResponse );
		iResult = eAODAllocError;
	}

	/* something went wrong before we could make the call */

	return( iResult );

} /* odCheckAPOP */


/* -----------------------------------------------------------------
	odCRAM_MD5 ()
   ----------------------------------------------------------------- */

int odCRAM_MD5 ( const char *inChallenge, const char *inResponse, struct od_user_opts *inOutOpts  )
{
	return( sValidateResponse( inChallenge, inResponse, kDSStdAuthCRAM_MD5, inOutOpts ) );
} /* odCRAM_MD5 */


/* -----------------------------------------------------------------
	odDoAuthenticate ()
   ----------------------------------------------------------------- */

int odDoAuthenticate (	const char *inMethod,
						const char *inInitialResp,
						const char *inCont,
						const char *inProtocol,
						struct protstream *inStreamIn,
						struct protstream *inStreamOut,
						struct od_user_opts *inOutOpts )
{
	int result = eAODNoErr;

	if ( strcasecmp( inMethod, "CRAM-MD5" ) == 0 )
	{
		result = sDoCRAM_MD5_Auth( inStreamIn, inStreamOut, inOutOpts );
	}
	else if ( strcasecmp( inMethod, "LOGIN" ) == 0 )
	{
		result = sDoLoginAuth( inStreamIn, inStreamOut, inOutOpts );
	}
	else if ( strcasecmp( inMethod, "PLAIN" ) == 0 )
	{
		result = sDoPlainAuth( inCont, inInitialResp, inStreamIn, inStreamOut, inOutOpts );
	}
	else if ( strcasecmp( inMethod, "SIEVE-CRAM-MD5" ) == 0 )
	{
		result = sDoCRAM_MD5_AuthS( inStreamIn, inStreamOut, inOutOpts );
	}
	else if ( strcasecmp( inMethod, "SIEVE-LOGIN" ) == 0 )
	{
		result = sDoLoginAuthS( inStreamIn, inStreamOut, inOutOpts );
	}
	else if ( strcasecmp( inMethod, "SIEVE-PLAIN" ) == 0 )
	{
		result = sDoPlainAuthS( inCont, inInitialResp, inStreamIn, inStreamOut, inOutOpts );
	}
	else
	{
		result = eAODProtocolError;
	}

    return ( result );

} /* odDoAuthenticate */



/* -----------------------------------------------------------------
	odIsMember ()

		-  Does user (short name) a member of this group (short name)
   ----------------------------------------------------------------- */

int odIsMember ( const char *inUser, const char *inGroup )
{
	int		isMember	= 0;
	uuid_t	userID;
	uuid_t	groupID;

	if ( (inUser == NULL) || (inGroup == NULL) )
	{
		return( 0 );
	}

	if ( mbr_user_name_to_uuid( inUser, userID ) != 0 )
	{
		return( 0 );
	}

	if ( mbr_group_name_to_uuid( inGroup, groupID ) != 0 )
	{
		return( 0 );
	}

	if ( mbr_check_membership( userID, groupID, &isMember ) != 0 )
	{
		return( 0 );
	}

	return( isMember );

} /* odIsMember */


/* -----------------------------------------------------------------
	sDoLoginAuth ()

		C: A1 AUTHENTICATE LOGIN
		S: + VXNlciBOYW1lAA==
		C: bWl0bGlzdEB3d3cuc2FwaG9tZS5jb20=
		S: + UGFzc3dvcmQA
		C: c2piZmxvdw==
		S: A01 OK Success

   ----------------------------------------------------------------- */

int sDoLoginAuth ( 	struct protstream *inStreamIn,
					struct protstream *inStreamOut,
					struct od_user_opts *inOutOpts )
{
	int		iResult	= eAODNoErr;
	int		len		= 0;
	int		respLen	= 0;
	char   *p		= NULL;
	char	userBuf	[ MAX_USER_BUF_SIZE ];
	char	chalBuf	[ MAX_CHAL_BUF_SIZE ];
	char	pwdBuf	[ MAX_USER_BUF_SIZE ];
	char	ioBuf	[ MAX_IO_BUF_SIZE ];

	/* is LOGIN authentication enabled */
	if ( !config_getswitch( IMAPOPT_IMAP_AUTH_LOGIN ) )
	{
		syslog( LOG_DEBUG, "AOD: LOGIN: configuration error: LOGIN authentication not enabled" );
		return ( eAODMethodNotEnabled );
	}

	/* encode the user name prompt and send it */
	strcpy( chalBuf, "Username:" );
	sEncodeBase64( chalBuf, strlen( chalBuf ), ioBuf, MAX_IO_BUF_SIZE );
	prot_printf( inStreamOut, "+ %s\r\n", ioBuf );

	/* reset the buffer */
	memset( ioBuf, 0, MAX_IO_BUF_SIZE );

	/* get the client response */
	if ( prot_fgets( ioBuf, MAX_IO_BUF_SIZE, inStreamIn ) )
	{
		/* trim CRLF */
		p = ioBuf + strlen( ioBuf ) - 1;
		if ( (p >= ioBuf) && (*p == '\n') )
		{
			*p-- = '\0';
		}
		if ( (p >= ioBuf) && (*p == '\r') )
		{
			*p-- = '\0';
		}

		/* check if client cancelled */
		if ( ioBuf[ 0 ] == '*' )
		{
			syslog( LOG_DEBUG, "AOD: LOGIN: user canceled auth attempt" );
			return( eAODAuthCanceled );
		}
	}

	/* reset the buffer */
	memset( userBuf, 0, MAX_USER_BUF_SIZE );

	len = strlen( ioBuf );
	sDecodeBase64( ioBuf, len, userBuf, MAX_USER_BUF_SIZE, &respLen );

	/* get user options */
	odGetUserOpts( userBuf, inOutOpts );

	/* encode the password prompt and send it */
	strcpy( chalBuf, "Password:" );
	sEncodeBase64( chalBuf, strlen( chalBuf ), ioBuf, MAX_IO_BUF_SIZE );
	prot_printf( inStreamOut, "+ %s\r\n", ioBuf );

	/* get the client response */
	if ( prot_fgets( ioBuf, MAX_IO_BUF_SIZE, inStreamIn ) )
	{
		/* trim CRLF */
		p = ioBuf + strlen( ioBuf ) - 1;
		if ( (p >= ioBuf) && (*p == '\n') )
		{
			*p-- = '\0';
		}
		if ( (p >= ioBuf) && (*p == '\r') )
		{
			*p-- = '\0';
		}

		/* check if client cancelled */
		if ( ioBuf[ 0 ] == '*' )
		{
			syslog( LOG_DEBUG, "AOD: LOGIN: user canceled auth attempt" );
			return( eAODAuthCanceled );
		}
	}

	/* reset the buffer */
	memset( pwdBuf, 0, MAX_USER_BUF_SIZE );

	len = strlen( ioBuf );
	sDecodeBase64( ioBuf, len, pwdBuf, MAX_USER_BUF_SIZE, &respLen );

	/* do the auth */
	iResult = odCheckPass( pwdBuf, inOutOpts );

	/* clear stack buffers */
	memset( pwdBuf, 0, MAX_USER_BUF_SIZE );
	memset( ioBuf, 0, MAX_IO_BUF_SIZE );

	return( iResult );

} /* sDoLoginAuth */


/* -----------------------------------------------------------------
	sDoLoginAuthS ()

		C: AUTHENTICATE "LOGIN" 
		S: {12} 
		S: VXNlcm5hbWU6 
		Please enter your password: 
		C: {12+} 
		YHJaY3VeYW== 
		S: {12} 
		S: UGFzc3dvcmQ6 
		C: {12+} 
		YHJaY3VeYW== 

   ----------------------------------------------------------------- */

int sDoLoginAuthS ( struct protstream *inStreamIn,
					struct protstream *inStreamOut,
					struct od_user_opts *inOutOpts )
{
	int		iResult	= eAODNoErr;
	int		len		= 0;
	int		respLen	= 0;
	char	userBuf	[ MAX_USER_BUF_SIZE ];
	char	chalBuf	[ MAX_CHAL_BUF_SIZE ];
	char	pwdBuf	[ MAX_USER_BUF_SIZE ];
	char	ioBuf	[ MAX_IO_BUF_SIZE ];

	/* is LOGIN authentication enabled */
	if ( !config_getswitch( IMAPOPT_IMAP_AUTH_LOGIN ) )
	{
		syslog( LOG_DEBUG, "AOD: LOGIN: configuration error: LOGIN authentication not enabled" );
		return ( eAODMethodNotEnabled );
	}

	/* encode the user name prompt and send it */
	strcpy( chalBuf, "Username:" );
	sEncodeBase64( chalBuf, strlen( chalBuf ), ioBuf, MAX_IO_BUF_SIZE );
	prot_printf( inStreamOut, "{%d}\r\n", (int)strlen( ioBuf ) );
	prot_printf( inStreamOut, "%s\r\n", ioBuf );

	iResult = sGetClientResponse( ioBuf, MAX_IO_BUF_SIZE, inStreamIn );
	if ( iResult != eAODNoErr )
	{
		return( iResult );
	}

	/* reset the buffer */
	memset( userBuf, 0, MAX_USER_BUF_SIZE );

	len = strlen( ioBuf );
	sDecodeBase64( ioBuf, len, userBuf, MAX_USER_BUF_SIZE, &respLen );

	/* get user options */
	odGetUserOpts( userBuf, inOutOpts );

	/* encode the password prompt and send it */
	strcpy( chalBuf, "Password:" );
	sEncodeBase64( chalBuf, strlen( chalBuf ), ioBuf, MAX_IO_BUF_SIZE );
	prot_printf( inStreamOut, "{%d}\r\n", (int)strlen( ioBuf ) );
	prot_printf( inStreamOut, "%s\r\n", ioBuf );

	iResult = sGetClientResponse( ioBuf, MAX_IO_BUF_SIZE, inStreamIn );
	if ( iResult != eAODNoErr )
	{
		return( iResult );
	}

	/* reset the buffer */
	memset( pwdBuf, 0, MAX_USER_BUF_SIZE );

	len = strlen( ioBuf );
	sDecodeBase64( ioBuf, len, pwdBuf, MAX_USER_BUF_SIZE, &respLen );

	/* do the auth */
	iResult = odCheckPass( pwdBuf, inOutOpts );

	/* clear stack buffers */
	memset( pwdBuf, 0, MAX_USER_BUF_SIZE );
	memset( ioBuf, 0, MAX_IO_BUF_SIZE );

	return( iResult );

} /* sDoLoginAuthS */


/* -----------------------------------------------------------------
	sDoPlainAuth ()

		C: A01 AUTHENTICATE PLAIN dGVzdAB0ZXN0AHRlc3Q=
		S: A01 OK Success

	or ...

		C: A01 AUTHENTICATE PLAIN
		(note that there is a space following the "+" in the following line)
		S: +
		C: dGVzdAB0ZXN0AHRlc3Q=
		S: A01 OK Success

	message         = [authorize_id] UTF8NULL userName UTF8NULL passwd
	authenticate-id = 1*UTF8-SAFE      ; MUST accept up to 255 octets
	authorize-id    = 1*UTF8-SAFE      ; MUST accept up to 255 octets
	password        = 1*UTF8-SAFE      ; MUST accept up to 255 octets

   ----------------------------------------------------------------- */

int sDoPlainAuth ( 	const char *inCont,
					const char *inResp,
					struct protstream *inStreamIn,
					struct protstream *inStreamOut,
					struct od_user_opts *inOutOpts )
{
	int		iResult	= eAODProtocolError;
	int		len		= 0;
	int		respLen	= 0;
	char   *p		= NULL;
	char	ioBuf	[ MAX_IO_BUF_SIZE ];
	char	respBuf	[ MAX_IO_BUF_SIZE ];
	char	userBuf [ MAX_USER_BUF_SIZE ];
	char	authName[ MAX_USER_BUF_SIZE ];
	char	passwd	[ MAX_USER_BUF_SIZE ];

	/* is PLAIN authentication enabled */
	if ( !config_getswitch( IMAPOPT_IMAP_AUTH_PLAIN ) )
	{
		syslog( LOG_DEBUG, "AOD: PLAIN: configuration error: PLAIN authentication not enabled" );
		return ( eAODMethodNotEnabled );
	}

	if ( inResp != NULL )
	{
		strlcpy( ioBuf, inResp, MAX_IO_BUF_SIZE );
	}
	else
	{
		prot_printf( inStreamOut, "%s\r\n", inCont );

		/* get the client response */
		if ( prot_fgets( ioBuf, MAX_IO_BUF_SIZE, inStreamIn ) )
		{
			/* trim CRLF */
			p = ioBuf + strlen( ioBuf ) - 1;
			if ( (p >= ioBuf) && (*p == '\n') )
			{
				*p-- = '\0';
			}
			if ( (p >= ioBuf) && (*p == '\r') )
			{
				*p-- = '\0';
			}

			/* check if client cancelled */
			if ( ioBuf[ 0 ] == '*' )
			{
				syslog( LOG_DEBUG, "AOD: PLAIN: user canceled auth attempt" );
				return( eAODAuthCanceled );
			}
		}
	}

	/* reset the buffer */
	memset( respBuf, 0, MAX_IO_BUF_SIZE );

	/* decode the response */
	len = strlen( ioBuf );
	sDecodeBase64( ioBuf, len, respBuf, MAX_IO_BUF_SIZE, &respLen );

	p = respBuf;
	if ( *p != '\0' )
	{
		/* get the authenticate-id */
		strlcpy( authName, p, MAX_USER_BUF_SIZE );

		/* skip past authenticate-id and NUL */
		p = p + (strlen( authName ) + 1 );
	}
	else
	{
		/* no authenticate-id sent, skip first NUL */
		p++;
	}

	/* is response still valid */
	if ( (p != NULL) && (strlen( p ) < MAX_USER_BUF_SIZE) )
	{
		/* get authorize-id (user id) */
		strlcpy( userBuf, p, MAX_USER_BUF_SIZE );

		/* get user options */
		odGetUserOpts( userBuf, inOutOpts );

		/* skip past authorize-id and NUL */
		p = p + (strlen( userBuf ) + 1 );

		/* is response still valid */
		if ( (p != NULL) && (strlen( p ) < MAX_USER_BUF_SIZE) )
		{
			/* get password */
			strlcpy( passwd, p, MAX_USER_BUF_SIZE );

			/* do the auth */
			iResult = odCheckPass( passwd, inOutOpts );
		}
	}

	/* clear stack buffers */
	memset( passwd, 0, MAX_USER_BUF_SIZE );
	memset( respBuf, 0, MAX_IO_BUF_SIZE );

	return( iResult );

} /* sDoPlainAuth */


/* -----------------------------------------------------------------
	sDoPlainAuthS ()

		C: A01 AUTHENTICATE PLAIN dGVzdAB0ZXN0AHRlc3Q=
		S: A01 OK Success

	or ...

		C: A01 AUTHENTICATE PLAIN
		(note that there is a space following the "+" in the following line)
		S: +
		C: dGVzdAB0ZXN0AHRlc3Q=
		S: A01 OK Success

	or ...

		C: AUTHENTICATE "PLAIN" {24+}
		bWlrZWQAbWlrZWQAbWlrZWQ=
		S: A01 OK Success

	or ...

		C: AUTH PLAIN dGVzdAB0ZXN0QHdpei5leGFtcGxlLmNvbQB0RXN0NDI=

	message         = [authName] UTF8NULL userName UTF8NULL passwd
	authenticate-id = 1*UTF8-SAFE      ; MUST accept up to 255 octets
	authorize-id    = 1*UTF8-SAFE      ; MUST accept up to 255 octets
	password        = 1*UTF8-SAFE      ; MUST accept up to 255 octets

   ----------------------------------------------------------------- */

int sDoPlainAuthS ( const char *inCont,
					const char *inResp,
					struct protstream *inStreamIn,
					struct protstream *inStreamOut,
					struct od_user_opts *inOutOpts )
{
	int		iResult	= eAODProtocolError;
	int		len		= 0;
	int		respLen	= 0;
	char   *p		= NULL;
	char   *bufPtr	= NULL;
	char   *bufEnd	= NULL;
	char	ioBuf	[ MAX_IO_BUF_SIZE ];
	char	respBuf	[ MAX_IO_BUF_SIZE ];
	char	authBuf [ MAX_USER_BUF_SIZE ];		//authenticate-id
	char	userBuf [ MAX_USER_BUF_SIZE ];	// user to authorize
	char	passwd	[ MAX_USER_BUF_SIZE ];

	/* is PLAIN authentication enabled */
	if ( !config_getswitch( IMAPOPT_IMAP_AUTH_PLAIN ) )
	{
		syslog( LOG_DEBUG, "AOD: SIEVE-PLAIN: configuration error: PLAIN authentication not enabled" );
		return ( eAODMethodNotEnabled );
	}

	memset( ioBuf, 0, MAX_IO_BUF_SIZE );

	if ( inResp != NULL )
	{
		if ( *inResp == '{' )
		{
			p = (char *)inResp;
			p++;
			while (	isdigit( (int)(*p) ) )
			{
				len = len * 10 + (*p - '0');
				p++;
			}

			if ( *p++ == '+' )
			{
				if ( *p == '}' )
				{
					if ( (len <= 0) || (len > MAX_IO_BUF_SIZE) )
					{
						syslog( LOG_DEBUG, "AOD: SIEVE-PLAIN: out of bounds length. len: %d. buf size: %d", len, MAX_IO_BUF_SIZE );
						return( eAODInvalidDataType );
					}

					bufPtr = ioBuf;
					bufEnd = bufPtr + len;
					while ( bufPtr < bufEnd )
					{
						*bufPtr++ = prot_getc( inStreamIn );
					}
				}
				else
				{
					syslog( LOG_DEBUG, "AOD: SIEVE-PLAIN: bad sequence in {n+}: missing '}'");
					return( eAODInvalidDataType );
				}
			}
			else
			{
				syslog( LOG_DEBUG, "AOD: SIEVE-PLAIN: bad sequence in {n+}: missing '+'");
				return( eAODInvalidDataType );
			}
		}
		else
		{
			strlcpy( ioBuf, inResp, MAX_IO_BUF_SIZE );
		}
	}
	else
	{
		prot_printf( inStreamOut, "%s\r\n", inCont );

		/* get the client response */
		if ( prot_fgets( ioBuf, MAX_IO_BUF_SIZE, inStreamIn ) )
		{
			/* trim CRLF */
			p = ioBuf + strlen( ioBuf ) - 1;
			if ( (p >= ioBuf) && (*p == '\n') )
			{
				*p-- = '\0';
			}
			if ( (p >= ioBuf) && (*p == '\r') )
			{
				*p-- = '\0';
			}

			/* check if client cancelled */
			if ( ioBuf[ 0 ] == '*' )
			{
				syslog( LOG_DEBUG, "AOD: SIEVE-PLAIN: user canceled auth attempt" );
				return( eAODAuthCanceled );
			}
		}
	}

	/* null set buffers */
	memset( respBuf, 0, MAX_IO_BUF_SIZE );
	memset( userBuf, 0, MAX_USER_BUF_SIZE );

	/* decode the response */
	len = strlen( ioBuf );
	sDecodeBase64( ioBuf, len, respBuf, MAX_IO_BUF_SIZE, &respLen );

	p = respBuf;
	if ( *p != '\0' )
	{
		/* get the authenticate-id */
		strlcpy( userBuf, p, MAX_USER_BUF_SIZE );

		/* skip past authenticate-id and NUL */
		p = p + (strlen( userBuf ) + 1 );
	}
	else
	{
		/* no authenticate-id sent, skip first NUL */
		p++;
	}

	/* is response still valid */
	if ( (p != NULL) && (strlen( p ) < MAX_USER_BUF_SIZE) )
	{
		/* get authorize-id (user id) */
		strlcpy( authBuf, p, MAX_USER_BUF_SIZE );
		/* get user options */
		odGetUserOpts( authBuf, inOutOpts );

		/* skip past authorize-id and NUL */
		p = p + (strlen( authBuf ) + 1 );

		/* is response still valid */
		if ( (p != NULL) && (strlen( p ) < MAX_USER_BUF_SIZE) )
		{
			/* get password */
			strlcpy( passwd, p, MAX_USER_BUF_SIZE );

			/* do the auth */
			iResult = odCheckPass( passwd, inOutOpts );
			if ( (iResult == eAODNoErr) && (strlen( userBuf ) != 0) && (strcasecmp( userBuf, authBuf ) != 0) )
			{
				syslog( LOG_DEBUG, "AOD: SIEVE-PLAIN: authorizing user: %s, by: %s", userBuf, authBuf );
				odGetUserOpts( userBuf, inOutOpts );
				if ( inOutOpts->fAuthIDNamePtr != NULL )
				{
					free( inOutOpts->fAuthIDNamePtr );
					inOutOpts->fAuthIDNamePtr = NULL;
				}
				inOutOpts->fAuthIDNamePtr = malloc( strlen( authBuf ) + 1 );
				if ( inOutOpts->fAuthIDNamePtr != NULL )
				{
					strlcpy( inOutOpts->fAuthIDNamePtr, authBuf, strlen( authBuf ) + 1 );
				}
				else
				{
					iResult = eAODAllocError;
				}
			}
		}
	}

	/* clear stack buffers */
	memset( passwd, 0, MAX_USER_BUF_SIZE );
	memset( respBuf, 0, MAX_IO_BUF_SIZE );

	return( iResult );

} /* sDoPlainAuthS */


/* -----------------------------------------------------------------
	sDoCRAM_MD5_AuthS ()
   ----------------------------------------------------------------- */

int sDoCRAM_MD5_AuthS ( struct protstream *inStreamIn,
						struct protstream *inStreamOut,
						struct od_user_opts *inOutOpts )
{
	int				iResult	= eAODAuthFailed;
	int				len		= 0;
	int				respLen	= 0;
	char		   *p		= NULL;
	char			chalBuf	[ MAX_CHAL_BUF_SIZE ];
	char			userBuf [ MAX_USER_BUF_SIZE ];
	char			respBuf	[ MAX_IO_BUF_SIZE ];
	char			ioBuf	[ MAX_IO_BUF_SIZE ];
	char			hostname[ MAXHOSTNAMELEN + 1 ];
	struct timeval	tvChalTime;
	char			randbuf[ 17 ];

	/* is CRAM-MD5 auth enabled */
	if ( !config_getswitch( IMAPOPT_IMAP_AUTH_CRAM_MD5 ) )
	{
		syslog( LOG_DEBUG, "AOD: CRAM-MD5: configuration error: CRAM-MD5 authentication not enabled" );
		return ( eAODMethodNotEnabled );
	}

	/* create the challenge */
	gethostname( hostname, sizeof( hostname ) );
	gettimeofday (&tvChalTime, NULL);

	/* get random data string */
	get_random_chars( randbuf, 17 );

	sprintf( chalBuf, "<%lu.%s.%lu@%s>",
						(unsigned long) getpid(),
						randbuf,
						(unsigned long)time(0),
						hostname );

	/* encode the challenge and send it */
	sEncodeBase64( chalBuf, strlen( chalBuf ), ioBuf, MAX_IO_BUF_SIZE );

	syslog( LOG_DEBUG, "AOD: SIEVE-CRAM-MD5: challenge: %s", ioBuf );
	syslog( LOG_DEBUG, "AOD: SIEVE-CRAM-MD5: challenge: %s", chalBuf );

	prot_printf( inStreamOut, "{%d}\r\n", (int)strlen( ioBuf ) );
	prot_printf( inStreamOut, "%s\r\n", ioBuf );

	iResult = sGetClientResponse( ioBuf, MAX_IO_BUF_SIZE, inStreamIn );
	if ( iResult != eAODNoErr )
	{
		return( iResult );
	}

	/* reset the buffer */
	memset( respBuf, 0, MAX_IO_BUF_SIZE );

	/* decode the response */
	len = strlen( ioBuf );
	if ( len == 0 )
	{
		syslog( LOG_ERR, "AOD: CRAM-MD5: Zero length response" );
		return( eAODParamErr );
	}

	sDecodeBase64( ioBuf, len, respBuf, MAX_IO_BUF_SIZE, &respLen );

	/* get the user name */
	p = strchr( respBuf, ' ' );
	if ( (p == NULL) || (strspn(p + 1, "0123456789abcdef") != 32) )
	{
		syslog( LOG_ERR, "AOD: CRAM-MD5: parameter error: bad digest: %s", respBuf );
		return( eAODParamErr );
	}

	len = p - respBuf;
	if ( len > (MAX_USER_BUF_SIZE - 1) )
	{
		syslog( LOG_ERR, "AOD: CRAM-MD5: username exceeded maximum limit: (%s)", respBuf );
		return( eAODParamErr );
	}

	/* copy user name from response buf */
	memset( userBuf, 0, MAX_USER_BUF_SIZE );
	memcpy( userBuf, respBuf, len );

	syslog( LOG_DEBUG, "AOD: SIEVE-CRAM-MD5: user name: %s", userBuf );

	/* get user options */
	odGetUserOpts( userBuf, inOutOpts );

	/* move past the space */
	if ( ++p != NULL )
	{
		/* validate the response */
		iResult = odCRAM_MD5( chalBuf, p, inOutOpts );
	}
	else
	{
		syslog( LOG_ERR, "AOD: CRAM-MD5: configuration error: bad CRAM-MD5 digest: %s", respBuf );
		iResult = eAODConfigError;
	}

	return( iResult );

} /* sDoCRAM_MD5_AuthS */


/* -----------------------------------------------------------------
	sDoCRAM_MD5_Auth ()
   ----------------------------------------------------------------- */

int sDoCRAM_MD5_Auth ( struct protstream *inStreamIn,
						struct protstream *inStreamOut,
						struct od_user_opts *inOutOpts )
{
	int				iResult	= eAODAuthFailed;
	int				len		= 0;
	int				respLen	= 0;
	char		   *p		= NULL;
	char			chalBuf	[ MAX_CHAL_BUF_SIZE ];
	char			userBuf [ MAX_USER_BUF_SIZE ];
	char			respBuf	[ MAX_IO_BUF_SIZE ];
	char			ioBuf	[ MAX_IO_BUF_SIZE ];
	char			hostname[ MAXHOSTNAMELEN + 1 ];
	struct timeval	tvChalTime;
	char			randbuf[ 17 ];

	/* is CRAM-MD5 auth enabled */
	if ( !config_getswitch( IMAPOPT_IMAP_AUTH_CRAM_MD5 ) )
	{
		syslog( LOG_DEBUG, "AOD: CRAM-MD5: configuration error: CRAM-MD5 authentication not enabled" );
		return ( eAODMethodNotEnabled );
	}

	/* create the challenge */
	gethostname( hostname, sizeof( hostname ) );
	gettimeofday (&tvChalTime, NULL);

	/* get random data string */
	get_random_chars( randbuf, 17 );

	sprintf( chalBuf, "<%lu.%s.%lu@%s>",
						(unsigned long) getpid(),
						randbuf,
						(unsigned long)time(0),
						hostname );

	/* encode the challenge and send it */
	sEncodeBase64( chalBuf, strlen( chalBuf ), ioBuf, MAX_IO_BUF_SIZE );

	prot_printf( inStreamOut, "+ %s\r\n", ioBuf );

	/* reset the buffer */
	memset( ioBuf, 0, MAX_IO_BUF_SIZE );

	/* get the client response */
	if ( prot_fgets( ioBuf, MAX_IO_BUF_SIZE, inStreamIn ) )
	{
		/* trim CRLF */
		p = ioBuf + strlen( ioBuf ) - 1;
		if ( (p >= ioBuf) && (*p == '\n') )
		{
			*p-- = '\0';
		}
		if ( (p >= ioBuf) && (*p == '\r') )
		{
			*p-- = '\0';
		}

		/* check if client cancelled */
		if ( ioBuf[ 0 ] == '*' )
		{
			syslog( LOG_DEBUG, "AOD: CRAM-MD5: user canceled auth attempt" );
			return( eAODAuthCanceled );
		}
	}

	/* reset the buffer */
	memset( respBuf, 0, MAX_IO_BUF_SIZE );

	/* decode the response */
	len = strlen( ioBuf );
	if ( len == 0 )
	{
		syslog( LOG_DEBUG, "AOD: CRAM-MD5: Zero length response" );
		return( eAODParamErr );
	}

	sDecodeBase64( ioBuf, len, respBuf, MAX_IO_BUF_SIZE, &respLen );

	/* get the user name */
	p = strchr( respBuf, ' ' );
	if ( (p == NULL) || (strspn(p + 1, "0123456789abcdef") != 32) )
	{
		syslog( LOG_DEBUG, "AOD: CRAM-MD5: parameter error: bad digest: %s", respBuf );
		return( eAODParamErr );
	}

	len = p - respBuf;
	if ( len > (MAX_USER_BUF_SIZE - 1) )
	{
		syslog( LOG_ERR, "AOD: CRAM-MD5: username exceeded maximum limit: (%s)", respBuf );
		return( eAODParamErr );
	}

	/* copy user name from response buf */
	memset( userBuf, 0, MAX_USER_BUF_SIZE );
	memcpy( userBuf, respBuf, len );

	/* get user options */
	odGetUserOpts( userBuf, inOutOpts );

	/* move past the space */
	if ( ++p != NULL )
	{
		/* validate the response */
		iResult = odCRAM_MD5( chalBuf, p, inOutOpts );
	}
	else
	{
		syslog( LOG_DEBUG, "AOD: CRAM-MD5: configuration error: bad CRAM-MD5 digest: %s", respBuf );
		iResult = eAODConfigError;
	}

	return( iResult );

} /* sDoCRAM_MD5_Auth */


int sGetClientResponse ( char *inOutBuf, int inBufSize, struct protstream *inStreamIn )
{
	int		c			= '\0';
	int		len			= 0;
	char	*bufPtr		= inOutBuf;
	char	*bufEnd		= inOutBuf + inBufSize;

	if ( inOutBuf == NULL )
	{
		return( eAODNullbuffer );
	}

	/* clear out the buffer */
	memset( inOutBuf, 0, inBufSize );

	c = prot_getc( inStreamIn );
	if ( c == EOF )
	{
		return( eAODLostConnection );
	}

	/* is it a quoted string */
	if ( c == '\"' )
	{
		while ( true )
		{
			c = prot_getc( inStreamIn );
			if ( c == EOF )
			{
				return( eAODLostConnection );
			}

			if ( c == '\"' )
			{
				return( eAODNoErr );
			}

			/* illegal characters */
			if ( (c == '\0') || (c == '\r') ||
				 (c == '\n') || (0x7F < ((unsigned char)c)) )
			{
				syslog( LOG_DEBUG, "AOD: GetClientResponse: illegal character: %c", c );
				return( eAODInvalidDataType );
			}

			/* escaped character */
			if ( c == '\\' )
			{
				c = prot_getc( inStreamIn );
				if ( (c != '\"') && (c != '\\') )
				{
					syslog( LOG_DEBUG, "AOD: GetClientResponse: illegal escape character: %c", c );
					return( eAODInvalidDataType );
				}
			}

			/* are we at the end of the buffer */
			if ( bufPtr > bufEnd )
			{
				syslog( LOG_DEBUG, "AOD: GetClientResponse: exceeded buffer" );
				return( eAODInvalidDataType );
			}

			*bufPtr++ = c;
		}
	}
	/* is it a literal */
	else if ( c == '{' )
	{
		/* get the octet count */
		while ( true )
		{
			c = prot_getc( inStreamIn );
			if ( c == EOF )
			{
				return( eAODLostConnection );
			}

			if ( isdigit( (int) c ) )
			{
				len = len * 10 + (c - '0');
			}
			else if ( c == '+' )
			{
				c = prot_getc( inStreamIn );
				if ( c == EOF )
				{
					return( eAODLostConnection );
				}

				if ( c == '}' )
				{
					/* now get the \r\n */
					c = prot_getc( inStreamIn );
					if ( c == EOF )
					{
						return( eAODLostConnection );
					}

					if ( c != '\r' )
					{
						syslog( LOG_DEBUG, "AOD: GetClientResponse: CR not found: %c", c );
						return( eAODInvalidDataType );
					}

					c = prot_getc( inStreamIn );
					if ( c == EOF )
					{
						return( eAODLostConnection );
					}

					if ( c != '\n' )
					{
						syslog( LOG_DEBUG, "AOD: GetClientResponse: LF not found: %c", c );
						return( eAODInvalidDataType );
					}

					break;
				}

				/* all is not well */
				syslog( LOG_DEBUG, "AOD: GetClientResponse: bad sequence in {n+}");
				return( eAODInvalidDataType );
			}
			else
			{
				/* we need to see a +} to be valid */
				syslog( LOG_DEBUG, "AOD: GetClientResponse: bad sequence in {n+}: missing +");
				return( eAODInvalidDataType );
			}
		}

		if ( (len <= 0) || (len > inBufSize) )
		{
			syslog( LOG_DEBUG, "AOD: GetClientResponse: out of bounds length. len: %d. buf size: %d", len, inBufSize );
			return( eAODInvalidDataType );
		}

		/* get the literal */
		bufEnd = inOutBuf + len;
		while ( bufPtr < bufEnd )
		{
			*bufPtr++ = prot_getc( inStreamIn );
		}

		/* now get the \r\n */
		c = prot_getc( inStreamIn );
		if ( c == EOF )
		{
			return( eAODLostConnection );
		}

		if ( c != '\r' )
		{
			syslog( LOG_DEBUG, "AOD: GetClientResponse: CR not found: %c", c );
			return( eAODInvalidDataType );
		}

		c = prot_getc( inStreamIn );
		if ( c == EOF )
		{
			return( eAODLostConnection );
		}

		if ( c != '\n' )
		{
			syslog( LOG_DEBUG, "AOD: GetClientResponse: LF not found: %c", c );
			return( eAODInvalidDataType );
		}

		return( eAODNoErr );
	}

	syslog( LOG_DEBUG, "AOD: GetClientResponse: invalid lead character: %c", c );

	if ( prot_fgets( inOutBuf, MAX_IO_BUF_SIZE, inStreamIn ) )
		syslog( LOG_DEBUG, "AOD: GetClientResponse: inOutBuf: %s", inOutBuf );

	return( eAODInvalidDataType );

} /* sGetClientResponse */


/* -----------------------------------------------------------------
	sGetUserOptions ()
   ----------------------------------------------------------------- */

int sGetUserOptions ( const char *inUserID, struct od_user_opts *inOutOpts )
{
	int					i				= 1;
	tDirStatus			dsStatus		= eDSNoErr;

	if ( (inUserID == NULL) || (inOutOpts == NULL) )
	{
		syslog( LOG_DEBUG, "AOD: System Error: empty user ID or user opts struct" );
		return( -1 );
	}

	/* turns out that DS can fail with invalid ref during _any_ call
		we can recover by trying again.  However, don't retry forever */

	for ( i = 1; i < 4; i++ )
	{
		/* reset to eDSNoErr if dir ref is valid */
		dsStatus = eDSNoErr;

		/* no need to close dir ref if still valid */
		if ( dsVerifyDirRefNum( inOutOpts->fDirRef ) != eDSNoErr )
		{
			syslog( LOG_DEBUG, "AOD: user opts: getting directory reference" );

			dsStatus = sOpen_ds( &inOutOpts->fDirRef );
			if ( dsStatus == eDSNoErr )
			{
				syslog( LOG_DEBUG, "AOD: user opts: getting search node reference" );

				/* open search node */
				dsStatus = sGet_search_node( inOutOpts->fDirRef, &inOutOpts->fSearchNodeRef );
			}
		}

		/* look up the user record */
		if ( dsStatus == eDSNoErr )
		{
			syslog( LOG_DEBUG, "AOD: user opts: looking up user record: %s", inUserID );

			/* get user attributes from applemail attribute */
			dsStatus = sGet_user_attributes( inOutOpts->fDirRef, inOutOpts->fSearchNodeRef, inUserID, inOutOpts );
		}

		/* if ref is invalid, try to get user options again */
		if ( dsStatus != eDSInvalidReference )
		{
			break;
		}

		syslog( LOG_WARNING, "AOD Warning: Directory reference is invalid, retrying (%d)", i );

		/* wait and try again */
		sleep( 1 );
	}

	return( dsStatus );

} /* sGetUserOptions */


/* -----------------------------------------------------------------
	checkServiceACL ()
   ----------------------------------------------------------------- */

int checkServiceACL ( struct od_user_opts *inUserOpts, const char *inGroup )
{
	int			err			= eAODNoErr;
	int			result		= 0;
	uuid_t		userUUID;

	memset( userUUID, 0, sizeof( uuid_t ) );

	/* we should already have this from previous user lookup */
	if ( inUserOpts->fUserUUID != NULL )
	{
		err = mbr_string_to_uuid( (const char *)inUserOpts->fUserUUID, userUUID );
	}
	else
	{
		/* get the uuid for user */
		err = mbr_user_name_to_uuid( inUserOpts->fRecNamePtr, userUUID );
	}

	if ( err != eAODNoErr )
	{
		/* couldn't turn user into uuid settings form user record */
		syslog( LOG_DEBUG, "AOD: service ACL: mbr_user_name_to_uuid failed for user: %s (%s)", inUserOpts->fRecNamePtr, strerror( err ) );
		return( -1 );
	}

	/* check the mail service ACL */
	err = mbr_check_service_membership( userUUID, inGroup, &result );
	if ( err == ENOENT )
	{
		/* look for all services acl */
		syslog( LOG_DEBUG, "AOD: mbr_check_service_membership with access_all_services" );
		err = mbr_check_service_membership( userUUID, "access_all_services", &result );
	}

	/* service ACL is enabled */
	if ( err == eAODNoErr )
	{
		/* check membership */
		if ( result != 0 )
		{
			/* we are a member, enable all mail services */
			/* preserve any auto-forwarding settings */
			if ( inUserOpts->fAccountState & eAutoForwardedEnabled )
			{
				syslog( LOG_DEBUG, "AOD: valid service ACL member: auto-forward" );
				inUserOpts->fAccountState &= ~eIMAPEnabled;
				inUserOpts->fAccountState &= ~ePOPEnabled;
			}
			else
			{
				/* enable all mail services */
				syslog( LOG_DEBUG, "AOD: valid service ACL member: normal access" );
				inUserOpts->fAccountState |= eAccountEnabled;
				inUserOpts->fAccountState |= eIMAPEnabled;
				inUserOpts->fAccountState |= ePOPEnabled;
			}
		}
		else
		{
			/* we are not a member override any settings form user record */
			syslog( LOG_DEBUG, "AOD Warning: mbr_check_service_membership failed with: %d", result );
			inUserOpts->fAccountState |= eACLNotMember;
			inUserOpts->fAccountState &= ~eAccountEnabled;
			inUserOpts->fAccountState &= ~eIMAPEnabled;
			inUserOpts->fAccountState &= ~ePOPEnabled;
		}
	}
	else
	{
		syslog( LOG_DEBUG, "AOD: mail service ACL NOT enabled" );
	}

	return( result );

} /* checkServiceACL */


/* -----------------------------------------------------------------
	sDoesUserBelongToGroup ()
   ----------------------------------------------------------------- */

int sDoesUserBelongToGroup ( const char *inUser, const char *inGroup )
{
	int isMember = 0;

	if ( (inUser != nil) && (inGroup != nil) )
	{
		struct group *passwd_group = getgrnam( inGroup );
		if ( passwd_group != NULL )
		{
			struct passwd *passwd_ent = getpwnam( inUser );
			if ( passwd_ent != NULL )
			{
				isMember = (passwd_ent->pw_gid == passwd_group->gr_gid);
			}
			if ( !isMember )
			{
				char **gr_mem = passwd_group->gr_mem;
				while ( *gr_mem )
				{
					if ( strcmp( inUser, *gr_mem ) == 0 )
					{
						isMember = 1;
						break;
					}
					gr_mem++;
				}
			}
		}
	}

	return( isMember );

} /* sDoesUserBelongToGroup */


/* -----------------------------------------------------------------
	sOpen_ds ()
   ----------------------------------------------------------------- */

tDirStatus sOpen_ds ( tDirReference *inOutDirRef )
{
	tDirStatus		dsStatus	= eDSNoErr;

	dsStatus = dsOpenDirService( inOutDirRef );

	return( dsStatus );

} /* sOpen_ds */


/* -----------------------------------------------------------------
	sGet_search_node ()
   ----------------------------------------------------------------- */

tDirStatus sGet_search_node ( tDirReference inDirRef,
							 tDirNodeReference *outSearchNodeRef )
{
	tDirStatus		dsStatus	= eMemoryAllocError;
	unsigned long	uiCount		= 0;
	tDataBuffer	   *pTDataBuff	= NULL;
	tDataList	   *pDataList	= NULL;

	pTDataBuff = dsDataBufferAllocate( inDirRef, 8192 );
	if ( pTDataBuff != NULL )
	{
		dsStatus = dsFindDirNodes( inDirRef, pTDataBuff, NULL, eDSSearchNodeName, &uiCount, NULL );
		if ( dsStatus == eDSNoErr )
		{
			dsStatus = eDSNodeNotFound;
			if ( uiCount == 1 )
			{
				dsStatus = dsGetDirNodeName( inDirRef, pTDataBuff, 1, &pDataList );
				if ( dsStatus == eDSNoErr )
				{
					dsStatus = dsOpenDirNode( inDirRef, pDataList, outSearchNodeRef );
				}

				if ( pDataList != NULL )
				{
					(void)dsDataListDeAllocate( inDirRef, pDataList, true );

					free( pDataList );
					pDataList = NULL;
				}
			}
		}
		(void)dsDataBufferDeAllocate( inDirRef, pTDataBuff );
		pTDataBuff = NULL;
	}

	return( dsStatus );

} /* sGet_search_node */


/* -----------------------------------------------------------------
	sGet_user_attributes ()
   ----------------------------------------------------------------- */

tDirStatus sGet_user_attributes ( tDirReference inDirRef,
									tDirNodeReference inSearchNodeRef,
									const char *inUserID,
									struct od_user_opts *inOutOpts )
{
	tDirStatus				dsStatus		= eMemoryAllocError;
	int						done			= FALSE;
	int						i				= 0;
	unsigned long			uiRecCount		= 0;
	tDataBuffer			   *pTDataBuff		= NULL;
	tDataList			   *pUserRecType	= NULL;
	tDataList			   *pUserAttrType	= NULL;
	tRecordEntry		   *pRecEntry		= NULL;
	tAttributeEntry		   *pAttrEntry		= NULL;
	tAttributeValueEntry   *pValueEntry		= NULL;
	tAttributeValueListRef	valueRef		= 0;
	tAttributeListRef		attrListRef		= 0;
	tContextData			pContext		= NULL;
	tDataList				tdlRecName;

	memset( &tdlRecName,  0, sizeof( tDataList ) );

	pTDataBuff = dsDataBufferAllocate( inDirRef, 8192 );
	if ( pTDataBuff != NULL )
	{
		dsStatus = dsBuildListFromStringsAlloc( inDirRef, &tdlRecName, inUserID, NULL );
		if ( dsStatus == eDSNoErr )
		{
			dsStatus = eMemoryAllocError;

			pUserRecType = dsBuildListFromStrings( inDirRef, kDSStdRecordTypeUsers, NULL );
			if ( pUserRecType != NULL )
			{
				pUserAttrType = dsBuildListFromStrings( inDirRef,
														kDS1AttrMailAttribute,
														kDSNAttrRecordName,
														kDS1AttrGeneratedUID,
														kDSNAttrMetaNodeLocation,
														NULL );
				if ( pUserAttrType != NULL )
				{
					do {
						/* Get the user record(s) that matches the user id */
						dsStatus = dsGetRecordList( inSearchNodeRef, pTDataBuff, &tdlRecName, eDSiExact, pUserRecType,
													pUserAttrType, FALSE, &uiRecCount, &pContext );

						if ( dsStatus == eDSNoErr )
						{
							dsStatus = eDSInvalidName;
							/* do we have more than 1 match */
							if ( uiRecCount == 1 ) 
							{
								dsStatus = dsGetRecordEntry( inSearchNodeRef, pTDataBuff, 1, &attrListRef, &pRecEntry );
								if ( dsStatus == eDSNoErr )
								{
									/* Get the attributes we care about for the record */
									for ( i = 1; i <= pRecEntry->fRecordAttributeCount; i++ )
									{
										dsStatus = dsGetAttributeEntry( inSearchNodeRef, pTDataBuff, attrListRef, i, &valueRef, &pAttrEntry );
										if ( (dsStatus == eDSNoErr) && (pAttrEntry != NULL) )
										{
											if ( strcasecmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrMailAttribute ) == 0 )
											{
												/* Only get the first attribute value */
												dsStatus = dsGetAttributeValue( inSearchNodeRef, pTDataBuff, 1, valueRef, &pValueEntry );
												if ( dsStatus == eDSNoErr )
												{
													syslog( LOG_DEBUG, "AOD: getting mail attribute for user: %s", inUserID );

													/* Get the individual mail attribute values */
													sGet_mail_values( (char *)pValueEntry->fAttributeValueData.fBufferData, inOutOpts );

													/* If we don't find duplicate users in the same node, we take the first one with
														a valid mail attribute */
													done = true;
												}
											}
											else if ( strcasecmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
											{
												/* Only get the first attribute value */
												dsStatus = dsGetAttributeValue( inSearchNodeRef, pTDataBuff, 1, valueRef, &pValueEntry );
												if ( dsStatus == eDSNoErr )
												{
													/* Get the user record name */
													inOutOpts->fRecNamePtr = malloc( pValueEntry->fAttributeValueData.fBufferLength + 1 );
													if ( inOutOpts->fRecNamePtr != NULL )
													{
														strlcpy( inOutOpts->fRecNamePtr, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength + 1 );
													}
												}
											}
											else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
											{
												/* Only get the first attribute value */
												dsStatus = dsGetAttributeValue( inSearchNodeRef, pTDataBuff, 1, valueRef, &pValueEntry );
												if ( dsStatus == eDSNoErr )
												{
													/* Get the user location */
													inOutOpts->fUserLocPtr = (char *)calloc( pValueEntry->fAttributeValueData.fBufferLength + 1, sizeof( char ) );
													memcpy( inOutOpts->fUserLocPtr, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );
												}
											}
											else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrGeneratedUID ) == 0 )
											{
												/* Only get the first attribute value */
												dsStatus = dsGetAttributeValue( inSearchNodeRef, pTDataBuff, 1, valueRef, &pValueEntry );
												if ( dsStatus == eDSNoErr )
												{
													/* Get the user location */
													inOutOpts->fUserUUID = (uuid_t *)calloc( pValueEntry->fAttributeValueData.fBufferLength + 1, sizeof( char ) );
													memcpy( inOutOpts->fUserUUID, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );
												}
											}
											if ( pValueEntry != NULL )
											{
												(void)dsDeallocAttributeValueEntry( inSearchNodeRef, pValueEntry );
												pValueEntry = NULL;
											}
										}
										else
										{
											syslog( LOG_DEBUG, "AOD Warning: dsGetAttributeEntry failed with: %d for user: %s", dsStatus, inUserID );
										}
										if ( pAttrEntry != NULL )
										{
											(void)dsCloseAttributeValueList( valueRef );
											(void)dsDeallocAttributeEntry( inSearchNodeRef, pAttrEntry );
											pAttrEntry = NULL;
										}
									}

									if ( pRecEntry != NULL )
									{
										(void)dsDeallocRecordEntry( inSearchNodeRef, pRecEntry );
										pRecEntry = NULL;
									}
								}
								else
								{
									syslog( LOG_DEBUG, "AOD Warning: dsGetRecordEntry failed with: %d for user: %s", dsStatus, inUserID );
								}
							}
							else
							{
								done = true;
								if ( uiRecCount > 1 )
								{
									syslog( LOG_NOTICE, "AOD: user attributes: duplicate users found in directory: %s", inUserID );
								}
								dsStatus = eDSUserUnknown;
							}
						}
						else
						{
							syslog( LOG_DEBUG, "AOD Warning: dsGetRecordList failed with: %d for user: %s", dsStatus, inUserID );
						}
					} while ( (pContext != NULL) && (dsStatus == eDSNoErr) && (!done) );

					if ( pContext != NULL )
					{
						(void)dsReleaseContinueData( inSearchNodeRef, pContext );
						pContext = NULL;
					}
					(void)dsDataListDeallocate( inDirRef, pUserAttrType );
					free( pUserAttrType );
					pUserAttrType = NULL;
				}
				(void)dsDataListDeallocate( inDirRef, pUserRecType );
				free( pUserRecType );
				pUserRecType = NULL;
			}
			(void)dsDataListDeAllocate( inDirRef, &tdlRecName, TRUE );
		}
		(void)dsDataBufferDeAllocate( inDirRef, pTDataBuff );
		pTDataBuff = NULL;
	}
	
	return( dsStatus );

} /* sGet_user_attributes */


/* -----------------------------------------------------------------
	sOpen_user_node ()
   ----------------------------------------------------------------- */

tDirStatus sOpen_user_node (  tDirReference inDirRef, const char *inUserLoc, tDirNodeReference *outUserNodeRef )
{
	tDirStatus		dsStatus	= eMemoryAllocError;
	tDataList	   *pUserNode	= NULL;

	pUserNode = dsBuildFromPath( inDirRef, inUserLoc, "/" );
	if ( pUserNode != NULL )
	{
		dsStatus = dsOpenDirNode( inDirRef, pUserNode, outUserNodeRef );

		(void)dsDataListDeAllocate( inDirRef, pUserNode, TRUE );
		free( pUserNode );
		pUserNode = NULL;
	}

	return( dsStatus );

} /* sOpen_user_node */


/* -----------------------------------------------------------------
	sDoCryptAuth ()
   ----------------------------------------------------------------- */

static int sDoCryptAuth ( tDirReference inDirRef,
						  tDirNodeReference inUserNodeRef,
						   const char *inUserID, const char *inPasswd )
{
	tDirStatus				dsStatus		= eAODParamErr;
	long					nameLen			= 0;
	long					passwdLen		= 0;
	unsigned long			curr			= 0;
	unsigned long			len				= 0;
	unsigned long			uiBuffSzie		= 0;
	tDataBuffer			   *pAuthBuff		= NULL;
	tDataBuffer			   *pStepBuff		= NULL;
	tDataNode			   *pAuthType		= NULL;

	if ( (inUserID == NULL) || (inPasswd == NULL) )
	{
		return( eDSAuthParameterError );
	}

	nameLen = strlen( inUserID );
	passwdLen = strlen( inPasswd );

	uiBuffSzie = nameLen + passwdLen + 32;

	pAuthBuff = dsDataBufferAllocate( inDirRef, uiBuffSzie );
	if ( pAuthBuff != NULL )
	{
		/* We don't use this buffer for clear text auth */
		pStepBuff = dsDataBufferAllocate( inDirRef, 256 );
		if ( pStepBuff != NULL )
		{
			pAuthType = dsDataNodeAllocateString( inDirRef, kDSStdAuthNodeNativeClearTextOK );
			if ( pAuthType != NULL )
			{
				/* set user name */
				len = nameLen;
				memcpy( &(pAuthBuff->fBufferData[ curr ]), &len, sizeof( unsigned long ) );
				curr += sizeof( unsigned long );
				memcpy( &(pAuthBuff->fBufferData[ curr ]), inUserID, len );
				curr += len;

				/* set user password */
				len = passwdLen;
				memcpy( &(pAuthBuff->fBufferData[ curr ]), &len, sizeof( unsigned long ) );
				curr += sizeof( unsigned long );
				memcpy( &(pAuthBuff->fBufferData[ curr ]), inPasswd, len );
				curr += len;

				pAuthBuff->fBufferLength = curr;

				dsStatus = dsDoDirNodeAuth( inUserNodeRef, pAuthType, true, pAuthBuff, pStepBuff, NULL );
				if ( dsStatus != eDSNoErr )
				{
					syslog( LOG_ERR, "AOD: crypt authentication error: authentication failed for user: %s (%d)", inUserID, dsStatus );
				}
				(void)dsDataNodeDeAllocate( inDirRef, pAuthType );
				pAuthType = NULL;
			}
			else
			{
				syslog( LOG_ERR, "AOD: crypt authentication error: authentication failed for user: %s (%d)", inUserID, eDSAllocationFailed );
				dsStatus = eDSAllocationFailed;
			}
			(void)dsDataNodeDeAllocate( inDirRef, pStepBuff );
			pStepBuff = NULL;
		}
		else
		{
			syslog( LOG_ERR, "AOD: crypt authentication error: authentication failed for user: %s (%d)", inUserID, eDSAllocationFailed );
			dsStatus = eDSAllocationFailed;
		}
		(void)dsDataNodeDeAllocate( inDirRef, pAuthBuff );
		pAuthBuff = NULL;
	}

	return( dsStatus );

} /* sDoCryptAuth */


/* -----------------------------------------------------------------
	sValidateResponse ()
   ----------------------------------------------------------------- */

int sValidateResponse (	const char *inChallenge,
						const char *inResponse,
						const char *inAuthType,
						struct od_user_opts *inUserOpts )
{
	int					iResult			= eAODParamErr;
	tDirStatus			dsStatus		= eDSNoErr;
	tDirReference		dirRef			= 0;
	tDirNodeReference	userNodeRef		= 0;
	tDataBuffer		   *pAuthBuff		= NULL;
	tDataBuffer		   *pStepBuff		= NULL;
	tDataNode		   *pAuthType		= NULL;
	unsigned long		uiNameLen		= 0;
	unsigned long		uiChalLen		= 0;
	unsigned long		uiRespLen		= 0;
	unsigned long		uiBuffSzie		= 0;
	unsigned long		uiCurr			= 0;
	unsigned long		uiLen			= 0;

	if ( (inUserOpts == NULL) || (inUserOpts->fRecNamePtr == NULL) )
	{
		syslog( LOG_DEBUG, "AOD: validate response: configuration error: empty user" );
		return( eAODParamErr );
	}
	if ( inChallenge == NULL )
	{
		syslog( LOG_DEBUG, "AOD: validate response: configuration error: challenge" );
		return( eAODParamErr );
	}
	if ( inResponse == NULL )
	{
		syslog( LOG_DEBUG, "AOD: validate response: configuration error: empty response" );
		return( eAODParamErr );
	}
	if ( inAuthType == NULL )
	{
		syslog( LOG_DEBUG, "AOD: validate response: configuration error: empty auth type" );
		return( eAODParamErr );
	}

	uiNameLen = strlen( inUserOpts->fRecNamePtr );
	uiChalLen = strlen( inChallenge );
	uiRespLen = strlen( inResponse );

	uiBuffSzie = uiNameLen + uiChalLen + uiRespLen + 32;

	dirRef = inUserOpts->fDirRef;

	if ( inUserOpts->fUserLocPtr != NULL )
	{
		dsStatus = sOpen_user_node( dirRef, inUserOpts->fUserLocPtr, &userNodeRef );
		if ( dsStatus == eDSNoErr )
		{
			pAuthBuff = dsDataBufferAllocate( dirRef, uiBuffSzie );
			if ( pAuthBuff != NULL )
			{
				pStepBuff = dsDataBufferAllocate( dirRef, 256 );
				if ( pStepBuff != NULL )
				{
					pAuthType = dsDataNodeAllocateString( dirRef, inAuthType );
					if ( pAuthType != NULL )
					{
						/* User name */
						uiLen = uiNameLen;
						memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
						uiCurr += sizeof( unsigned long );
						memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), inUserOpts->fRecNamePtr, uiLen );
						uiCurr += uiLen;

						/* Challenge */
						uiLen = uiChalLen;
						memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
						uiCurr += sizeof( unsigned long );
						memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), inChallenge, uiLen );
						uiCurr += uiLen;

						/* Response */
						uiLen = uiRespLen;
						memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
						uiCurr += sizeof( unsigned long );
						memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), inResponse, uiLen );
						uiCurr += uiLen;

						pAuthBuff->fBufferLength = uiCurr;

						dsStatus = dsDoDirNodeAuth( userNodeRef, pAuthType, true, pAuthBuff, pStepBuff, NULL );
						switch ( dsStatus )
						{
							case eDSNoErr:
								iResult = eAODNoErr;
								break;
	
							case eDSAuthNewPasswordRequired:
								syslog( LOG_INFO, "AOD: authentication error: new password required for user: %s", inUserOpts->fRecNamePtr );
								iResult = eAODNoErr;
								break;
	
							case eDSAuthPasswordExpired:
								syslog( LOG_INFO, "AOD: authentication error: password expired for user: %s", inUserOpts->fRecNamePtr );
								iResult = eAODNoErr;
								break;

							default:
								syslog( LOG_INFO, "AOD: authentication error: %d", dsStatus );
								iResult = eAODAuthFailed;
								break;
						}
						(void)dsDataNodeDeAllocate( dirRef, pAuthType );
						pAuthType = NULL;
					}
					else
					{
						syslog( LOG_ERR, "AOD: authentication error: for user: %s.  cannot allocate memory", inUserOpts->fRecNamePtr );
						iResult = eAODAllocError;
					}
					(void)dsDataNodeDeAllocate( dirRef, pStepBuff );
					pStepBuff = NULL;
				}
				else
				{
					syslog( LOG_ERR, "AOD: authentication error: for user: %s.  cannot allocate memory", inUserOpts->fRecNamePtr );
					iResult = eAODAllocError;
				}
				(void)dsDataNodeDeAllocate( dirRef, pAuthBuff );
				pAuthBuff = NULL;
			}
			else
			{
				syslog( LOG_ERR, "AOD: authentication error: for user: %s.  cannot allocate memory", inUserOpts->fRecNamePtr );
				iResult = eAODAllocError;
			}
			(void)dsCloseDirNode( userNodeRef );
			userNodeRef = 0;
		}
		else
		{
			syslog( LOG_ERR, "AOD: authentication error: cannot open user directory node for user: %s (%d)", inUserOpts->fRecNamePtr, dsStatus );
			iResult = eAODCantOpenUserNode;
		}
	}
	else
	{
		syslog( LOG_ERR, "AOD: authentication error: cannot find user: %s (%d)", inUserOpts->fRecNamePtr, dsStatus );
		iResult = eAODUserNotFound;
	}

	return( iResult );

} /* sValidateResponse */


/* -----------------------------------------------------------------
	sGet_mail_values ()
   ----------------------------------------------------------------- */

void sGet_mail_values ( char *inMailAttribute, struct od_user_opts *inOutOpts )
{
	int					iResult 	= 0;
	unsigned long		uiDataLen	= 0;
	CFDataRef			cfDataRef	= NULL;
	CFPropertyListRef	cfPlistRef	= NULL;
	CFDictionaryRef		cfDictRef	= NULL;

	if ( inMailAttribute != NULL )
	{
		uiDataLen = strlen( inMailAttribute );
		cfDataRef = CFDataCreate( NULL, (const UInt8 *)inMailAttribute, uiDataLen );
		if ( cfDataRef != NULL )
		{
			cfPlistRef = CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfDataRef, kCFPropertyListImmutable, NULL );
			if ( cfPlistRef != NULL )
			{
				if ( CFDictionaryGetTypeID() == CFGetTypeID( cfPlistRef ) )
				{
					cfDictRef = (CFDictionaryRef)cfPlistRef;
					iResult = sVerify_version( cfDictRef );
					if ( iResult == eAODNoErr )
					{
						sGet_acct_state( cfDictRef, inOutOpts );
						sGet_IMAP_login( cfDictRef, inOutOpts );
						sGet_POP3_login( cfDictRef, inOutOpts );
						sGet_acct_loc( cfDictRef, inOutOpts );
						sGet_alt_loc( cfDictRef, inOutOpts );
						sGet_disk_quota( cfDictRef, inOutOpts );
					}
				}
				CFRelease( cfPlistRef );
			}
			CFRelease( cfDataRef );
		}
	}
} /* sGet_mail_values */


/* -----------------------------------------------------------------
	sVerify_version ()
   ----------------------------------------------------------------- */

int sVerify_version ( CFDictionaryRef inCFDictRef )
{
	int				iResult 	= eAODInvalidDataType;
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	if ( CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAttrVersion ) ) )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAttrVersion ) );
		if ( cfStringRef != NULL )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				iResult = eAODItemNotFound;
				pValue = (char *)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
				if ( pValue != NULL )
				{
					iResult = eAODWrongVersion;
					if ( strcasecmp( pValue, kXMLValueVersion ) == 0 )
					{
						iResult = eAODNoErr;
					}
				}
			}
		}
	}

	return( iResult );

} /* sVerify_version */


/* -----------------------------------------------------------------
	sGet_acct_state ()
   ----------------------------------------------------------------- */

void sGet_acct_state ( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts )
{
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	/* Default value */
	inOutOpts->fAccountState &= ~eAccountEnabled;

	if ( CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAcctState ) ) )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAcctState ) );
		if ( cfStringRef != NULL )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				pValue = (char *)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
				if ( pValue != NULL )
				{
					if ( strcasecmp( pValue, kXMLValueAcctEnabled ) == 0 )
					{
						inOutOpts->fAccountState |= eAccountEnabled;
						syslog( LOG_DEBUG, "AOD: mail enabled" );
					}
					else if ( strcasecmp( pValue, kXMLValueAcctFwd ) == 0 )
					{
						sGet_auto_forward( inCFDictRef, inOutOpts );
						syslog( LOG_DEBUG, "AOD: mail auto-forwarding enabled" );
					}
					else
					{
						syslog( LOG_DEBUG, "AOD: mail not enabled" );
					}
				}
			}
		}
	}
} /* sGet_acct_state */


/* -----------------------------------------------------------------
	sGet_auto_forward ()
   ----------------------------------------------------------------- */

void sGet_auto_forward ( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts )
{
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	if ( CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAutoFwd ) ) )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAutoFwd ) );
		if ( (cfStringRef != NULL) && (CFGetTypeID( cfStringRef ) == CFStringGetTypeID()) )
		{
			pValue = (char *)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
			if ( (pValue != NULL) && strlen( pValue ) )
			{
				inOutOpts->fAutoFwdPtr = malloc( strlen( pValue ) + 1 );
				if ( inOutOpts->fAutoFwdPtr != NULL )
				{
					strlcpy( inOutOpts->fAutoFwdPtr, pValue, strlen( pValue ) + 1 );
				}

				inOutOpts->fAccountState |= eAutoForwardedEnabled;
				inOutOpts->fAccountState &= ~eIMAPEnabled;
				inOutOpts->fAccountState &= ~ePOPEnabled;
			}
		}
	}
} /* sGet_auto_forward */


/* -----------------------------------------------------------------
	sGet_IMAP_login ()
   ----------------------------------------------------------------- */

void sGet_IMAP_login ( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts )
{
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	/* Default value */
	inOutOpts->fAccountState &= ~eIMAPEnabled;

	if ( CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeykIMAPLoginState ) ) )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeykIMAPLoginState ) );
		if ( (cfStringRef != NULL) && (CFGetTypeID( cfStringRef ) == CFStringGetTypeID()) )
		{
			pValue = (char *)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
			if ( (pValue != NULL)  && (strcasecmp( pValue, kXMLValueIMAPLoginOK ) == 0) )
			{
				inOutOpts->fAccountState |= eIMAPEnabled;
			}
		}
	}
} /* sGet_IMAP_login */


/* -----------------------------------------------------------------
	sGet_POP3_login ()
   ----------------------------------------------------------------- */

void sGet_POP3_login ( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts )
{
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	/* Default value */
	inOutOpts->fAccountState &= ~ePOPEnabled;

	if ( CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyPOP3LoginState ) ) )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyPOP3LoginState ) );
		if ( (cfStringRef != NULL) && (CFGetTypeID( cfStringRef ) == CFStringGetTypeID()) )
		{
			pValue = (char *)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
			if ( (pValue != NULL) && (strcasecmp( pValue, kXMLValuePOP3LoginOK ) == 0) )
			{
				inOutOpts->fAccountState |= ePOPEnabled;
			}
		}
	}
} /* sGet_POP3_login */


/* -----------------------------------------------------------------
	sGet_acct_loc ()
   ----------------------------------------------------------------- */

void sGet_acct_loc ( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts )
{
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	/* don't leak previous value (if any) */
	if ( inOutOpts->fAccountLocPtr != NULL )
	{
		free( inOutOpts->fAccountLocPtr );
		inOutOpts->fAccountLocPtr = NULL;
	}

	if ( CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAcctLoc ) ) )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAcctLoc ) );
		if ( (cfStringRef != NULL) && (CFGetTypeID( cfStringRef ) == CFStringGetTypeID()) )
		{
			pValue = (char*)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
			if ( (pValue != NULL) && strlen( pValue ) )
			{
				inOutOpts->fAccountLocPtr = malloc( strlen( pValue ) + 1 );
				if ( inOutOpts->fAccountLocPtr != NULL )
				{
					strlcpy( inOutOpts->fAccountLocPtr, pValue, strlen( pValue ) + 1 );
				}
			}
		}
	}
} /* sGet_acct_loc */


/* -----------------------------------------------------------------
	sGet_alt_loc ()
   ----------------------------------------------------------------- */

void sGet_alt_loc ( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts )
{
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	/* Default value */
	if ( inOutOpts->fAltDataLocPtr != NULL )
	{
		free( inOutOpts->fAltDataLocPtr );
		inOutOpts->fAltDataLocPtr = NULL;
	}

	if ( CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAltDataStoreLoc ) ) )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAltDataStoreLoc ) );
		if ( cfStringRef != NULL )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				pValue = (char*)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
				if ( (pValue != NULL) && strlen( pValue ) )
				{
					inOutOpts->fAltDataLocPtr = malloc( strlen( pValue ) + 1 );
					if ( inOutOpts->fAltDataLocPtr != NULL )
					{
						strlcpy( inOutOpts->fAltDataLocPtr, pValue, strlen( pValue ) + 1 );
					}
				}
			}
		}
	}
} /* sGet_alt_loc */


/* -----------------------------------------------------------------
	sGet_disk_quota ()
   ----------------------------------------------------------------- */

void sGet_disk_quota ( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts )
{
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	/* Default value */
	inOutOpts->fDiskQuota = 0;

	if ( CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyDiskQuota ) ) )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyDiskQuota ) );
		if ( cfStringRef != NULL )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				pValue = (char *)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
				if ( pValue != NULL )
				{
					inOutOpts->fDiskQuota = atol( pValue );
				}
			}
		}
	}
} /* sGet_disk_quota */


/* -----------------------------------------------------------------
	sEncodeBase64 ()
   ----------------------------------------------------------------- */

int sEncodeBase64 ( const char *inStr, const int inLen, char *outStr, int outLen )
{
	int						i			= 0;
	const unsigned char	   *pStr		= NULL;
	unsigned long			uiInLen		= 0;
	unsigned char			ucVal		= 0;

	if ( (inStr == NULL) || (outStr == NULL) || (inLen <= 0) || (outLen <= 0) )
	{
		return( -1 );
	}

	pStr = (const unsigned char *)inStr;
	uiInLen = inLen;

	memset( outStr, 0, outLen );

	while ( uiInLen >= 3 )
	{
		if ( (i + 4) > outLen )
		{
			return( -1 );
		}
		outStr[ i++ ] = ( basis_64[ pStr[ 0 ] >> 2 ] );
		outStr[ i++ ] = ( basis_64[ ((pStr[ 0 ] << 4) & 0x30) | (pStr[ 1 ] >> 4) ] );
		outStr[ i++ ] = ( basis_64[ ((pStr[ 1 ] << 2) & 0x3c) | (pStr[ 2 ] >> 6) ] );
		outStr[ i++ ] = ( basis_64[ pStr[ 2 ] & 0x3f ] );

		pStr += 3;
		uiInLen -= 3;
	}

	if ( uiInLen > 0 )
	{
		if ( (i + 4) > outLen )
		{
			return( -1 );
		}

		outStr[ i++ ] = ( basis_64[ pStr[0] >> 2] );
		ucVal = (pStr[0] << 4) & 0x30;
		if ( uiInLen > 1 )
		{
			ucVal |= pStr[1] >> 4;
		}
		outStr[ i++ ] = ( basis_64[ ucVal ] );
		outStr[ i++ ] = ( (uiInLen < 2) ? '=' : basis_64[ (pStr[ 1 ] << 2) & 0x3c ] );
		outStr[ i++ ] = ( '=' );
	}

	return( 0 );

} /* sEncodeBase64 */


/* -----------------------------------------------------------------
	sDecodeBase64 ()
   ----------------------------------------------------------------- */

int sDecodeBase64 ( const char *inStr, const int inLen, char *outStr, int outLen, int *destLen )
{
	int				iResult		= 0;
	unsigned long	i			= 0;
	unsigned long	j			= 0;
	unsigned long	c1			= 0;
	unsigned long	c2			= 0;
	unsigned long	c3			= 0;
	unsigned long	c4			= 0;
	const char	   *pStr		= NULL;

	if ( (inStr == NULL) || (outStr == NULL) || (inLen <= 0) || (outLen <= 0) )
	{
		return( -1 );
	}

	pStr = (const unsigned char *)inStr;

	/* Skip past the '+ ' */
	if ( (pStr[ 0 ] == '+') && (pStr[ 1 ] == ' ') )
	{
		pStr += 2;
	}
	if ( *pStr == '\r')
	{
		iResult = -1;
	}
	else
	{
		for ( i = 0; i < inLen / 4; i++ )
		{
			c1 = pStr[ 0 ];
			if ( CHAR64( c1 ) == -1 )
			{
				iResult = -1;
				break;
			}

			c2 = pStr[ 1 ];
			if ( CHAR64( c2 ) == -1 )
			{
				iResult = -1;
				break;
			}

			c3 = pStr[ 2 ];
			if ( (c3 != '=') && (CHAR64( c3 ) == -1) )
			{
				iResult = -1;
				break;
			}

			c4 = pStr[ 3 ];
			if (c4 != '=' && CHAR64( c4 ) == -1)
			{
				iResult = -1;
				break;
			}

			pStr += 4;

			outStr[ j++ ] = ( (CHAR64(c1) << 2) | (CHAR64(c2) >> 4) );

			if ( j >= outLen )
			{
				return( -1 );
			}

			if ( c3 != '=' )
			{
				outStr[ j++ ] = ( ((CHAR64(c2) << 4) & 0xf0) | (CHAR64(c3) >> 2) );
				if ( j >= outLen )
				{
					return( -1 );
				}
				if ( c4 != '=' )
				{
					outStr[ j++ ] = ( ((CHAR64(c3) << 6) & 0xc0) | CHAR64(c4) );
					if ( j >= outLen )
					{
						return( -1 );
					}
				}
			}
		}
		outStr[ j ] = 0;
	}

	if ( destLen )
	{
		*destLen = j;
	}

	return( iResult );

} /* sDecodeBase64 */



/* -----------------------------------------------------------------
	apple_password_callback ()
   ----------------------------------------------------------------- */

int apple_password_callback ( char *inBuf, int inSize, int in_rwflag, void *inUserData )
{
	OSStatus			status		= noErr;
	SecKeychainItemRef	keyChainRef	= NULL;
	void			   *pwdBuf		= NULL;  /* will be allocated and filled in by SecKeychainFindGenericPassword */
	UInt32				pwdLen		= 0;
	char			   *service		= "certificateManager"; /* defined by Apple */
	CallbackUserData   *cbUserData	= (CallbackUserData *)inUserData;

	if ( (cbUserData == NULL) || strlen( cbUserData->key ) == 0 ||
		 (cbUserData->len >= FILENAME_MAX) || (cbUserData->len == 0) || !inBuf )
	{
		syslog( LOG_ERR, "AOD Error: Invalid arguments in callback" );
		return( 0 );
	}

	/* Set the domain to System (daemon) */
	status = SecKeychainSetPreferenceDomain( kSecPreferencesDomainSystem );
	if ( status != noErr )
	{
		syslog( LOG_ERR, "AOD: SSL callback: SecKeychainSetPreferenceDomain returned status: %d", status );
		return( 0 );
	}

	// Passwords created by cert management have the keychain access dialog suppressed.
	status = SecKeychainFindGenericPassword( NULL, strlen( service ), service,
												   cbUserData->len, cbUserData->key,
												   &pwdLen, &pwdBuf,
												   &keyChainRef );

	if ( (status == noErr) && (keyChainRef != NULL) )
	{
		if ( pwdLen > inSize )
		{
			syslog( LOG_ERR, "AOD: SSL callback: invalid buffer size callback size : %d, len : %d", inSize, pwdLen );
			SecKeychainItemFreeContent( NULL, pwdBuf );
			return( 0 );
		}

		memcpy( inBuf, (const void *)pwdBuf, pwdLen );
		if ( inSize > 0 )
		{
			inBuf[ pwdLen ] = 0;
			inBuf[ inSize - 1 ] = 0;
		}

		SecKeychainItemFreeContent( NULL, pwdBuf );

		return( strlen(inBuf ) );
	}
	else if (status == errSecNotAvailable)
	{
		syslog( LOG_ERR, "AOD: SSL callback: SecKeychainSetPreferenceDomain: No keychain is available" );
	}
	else if ( status == errSecItemNotFound )
	{
		syslog( LOG_ERR, "AOD: SSL callback: SecKeychainSetPreferenceDomain: The requested key could not be found in the system keychain");
	}
	else if (status != noErr)
	{
		syslog( LOG_ERR, "AOD: SSL callback: SecKeychainFindGenericPassword returned status: %d", status );
	}

	return( 0 );

} /* apple_password_callback */


/* -----------------------------------------------------------------
	get_random_chars ()
   ----------------------------------------------------------------- */

void get_random_chars ( char *out_buf, int in_len )
{
	int					count = 0;
	int					file;
	unsigned long long	microseconds = 0ULL;
	struct timeval		tv;
	struct timezone		tz;

	memset( out_buf, 0, in_len );

	/* try to open /dev/urandom */
	file = open( "/dev/urandom", O_RDONLY, 0 );
	if ( file == -1 )
	{
		syslog( LOG_ERR, "AOD: random chars: cannot open /dev/urandom" );

		/* try to open /dev/random */
		file = open( "/dev/random", O_RDONLY, 0 );
	}

	if ( file == -1 )
	{
		syslog( LOG_ERR, "AOD: random chars: cannot open /dev/random" );

		gettimeofday( &tv, &tz );

		microseconds = (unsigned long long)tv.tv_sec;
		microseconds *= 1000000ULL;
		microseconds += (unsigned long long)tv.tv_usec;

		snprintf( out_buf, in_len, "%llu", microseconds );
	}
	else
	{
		/* make sure the chars are printable */
		while ( count < (in_len - 1) )
		{
			read( file, &out_buf[ count ], 1 );
			if ( isalnum( out_buf[ count ] ) )
			{
				count++;
			}
		}
		close( file );
	}
} /* get_random_chars */
