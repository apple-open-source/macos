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
#include <string.h>
#include <stdbool.h>
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

#include <Kerberos/Kerberos.h>
#include <Kerberos/gssapi_krb5.h>
#include <Kerberos/gssapi.h>

static tDirStatus	sOpen_ds			( tDirReference *inOutDirRef );
static tDirStatus	sGet_search_node	( tDirReference inDirRef, tDirNodeReference *outSearchNodeRef );
static tDirStatus	sLook_up_user		( tDirReference inDirRef, tDirNodeReference inSearchNodeRef, const char *inUserID, char **outUserLocation );
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
static int			sValidateResponse	( const char *inUserID, const char *inChallenge, const char *inResponse, const char *inAuthType );
static void			sSetErrorText		( eErrorType inErrType, int inError, const char *inStr );
static int			sGetUserOptions		( const char *inUserID, struct od_user_opts *inOutOpts );
static int			sGSS_Init			( const char *inProtocol );
static int			sEncodeBase64		( const char *inStr, const int inLen, char *outStr, int outLen );
static int			sDecodeBase64		( const char *inStr, const int inLen, char *outStr, int outLen, int *destLen );
static void			sLogErrors			( char *inName, OM_uint32 inMajor, OM_uint32 inMinor );
static char *		sGetServerPrincipal ( const char *inServerKey );
static int			sGetPrincipalStr	( char *inOutBuf, int inSize );
static void			get_random_chars	( char *out_buf, int in_len );
static int			checkServiceACL		( struct od_user_opts *inOutOpts, const char *inGroup );
static int			sDoCRAM_MD5_Auth	( struct protstream *inStreamIn, struct protstream *inStreamOut, char **inOutCannonUser );
static int			sDoPlainAuth		( const char *inCont, struct protstream *inStreamIn, struct protstream *inStreamOut, char **inOutCannonUser );
static int			sDoLoginAuth		( struct protstream *inStreamIn, struct protstream *inStreamOut, char **inOutCannonUser );
static int			sDoGSS_Auth			( const char *inProtocol, struct protstream *inStreamIn, struct protstream *inStreamOut, char **inOutCannonUser );

static	gss_cred_id_t	stCredentials;

extern char *auth_canonifyid(const char *identifier, size_t len);

/* ---- Globals ---------------------------------------------- */

char	gErrStr[ kONE_K_BUF ];

/*  --- Public Routines -------------------------------------- */

/* -----------------------------------------------------------
	- odGetUserOpts

		Get user options from user record
 * ----------------------------------------------------------- */

int odGetUserOpts ( const char *inUserID, struct od_user_opts *inOutOpts )
{
	int		r = 0;

	if ( inOutOpts != NULL )
	{
		/* clear struct */
		memset( inOutOpts, 0, sizeof( struct od_user_opts ) );

		/* set default settings */
		inOutOpts->fAcctState = eAcctDisabled;
		inOutOpts->fIMAPLogin = eAcctDisabled;
		inOutOpts->fPOP3Login = eAcctDisabled;

		/* get user options for user record or service acl */
		r = sGetUserOptions( inUserID, inOutOpts );

		/* do we have a user record name */
		if ( inOutOpts->fRecName[0] != '\0' )
		{
			/* are mail service ACL's enabled */
			checkServiceACL( inOutOpts, "mail" );
		}
	}

	return( r );

} /* odGetUserOpts */


/* -----------------------------------------------------------------
	aodCheckPass ()
   ----------------------------------------------------------------- */

int odCheckPass ( const char *inUserID, const char *inPasswd )
{
	int					iResult			= eAODNoErr;
	tDirStatus			dsStatus		= eDSNoErr;
	tDirReference		dirRef			= 0;
	tDirNodeReference	searchNodeRef	= 0;
	tDirNodeReference	userNodeRef		= 0;
	char			   *userLoc			= NULL;

	if ( (inUserID == NULL) || (inPasswd == NULL) )
	{
		sSetErrorText( eTypeParamErr, (inUserID == NULL ? 1 : 2 ), NULL );
		return( eAODParamErr );
	}

	dsStatus = sOpen_ds( &dirRef );
	if ( dsStatus == eDSNoErr )
	{
		dsStatus = sGet_search_node( dirRef, &searchNodeRef );
		if ( dsStatus == eDSNoErr )
		{
			dsStatus = sLook_up_user( dirRef, searchNodeRef, inUserID, &userLoc );
			if ( dsStatus == eDSNoErr )
			{
				dsStatus = sOpen_user_node( dirRef, userLoc, &userNodeRef );
				if ( dsStatus == eDSNoErr )
				{
					dsStatus = sDoCryptAuth( dirRef, userNodeRef, inUserID, inPasswd );
					switch ( dsStatus )
					{
						case eDSNoErr:
							iResult = eAODNoErr;
							break;

						case eDSAuthNewPasswordRequired:
							sSetErrorText( eTypeAuthWarnNewPW, dsStatus, inUserID );
							iResult = eAODNoErr;
							break;

						case eDSAuthPasswordExpired:
							sSetErrorText( eTypeAuthWarnExpirePW, dsStatus, inUserID );
							iResult = eAODNoErr;
							break;

						default:
							sSetErrorText( eTypeAuthFailed, dsStatus, inUserID );
							iResult = eAODAuthFailed;
							break;
					}
					(void)dsCloseDirNode( userNodeRef );
				}
				else
				{
					sSetErrorText( eTypeCantOpenUserNode, dsStatus, inUserID );
					iResult = eAODCantOpenUserNode;
				}
			}
			else
			{
				sSetErrorText( eTypeUserNotFound, dsStatus, inUserID );
				iResult = eAODUserNotFound;
			}
			(void)dsCloseDirNode( searchNodeRef );

			if ( userLoc != NULL )
			{
				free( userLoc );
				userLoc = NULL;
			}
		}
		else
		{
			sSetErrorText( eTypeOpenSearchFailed, dsStatus, NULL );
			iResult = eAODOpenSearchFailed;
		}
		(void)dsCloseDirService( dirRef );
	}
	else
	{
		sSetErrorText( eTypeOpenDSFailed, dsStatus, NULL );
		iResult = eAODOpenDSFailed;
	}

	return( iResult );

} /* aodCheckPass */


/* -----------------------------------------------------------------
	odCheckAPOP ()
   ----------------------------------------------------------------- */

int odCheckAPOP ( char **inOutUserID, const char *inChallenge, const char *inResponse  )
{
	char	*p		= NULL;
	int		len		= 0;
	char	tmp[ 2046 ];

	/* check for bogus data */
	if ( (inChallenge == NULL) || (inResponse == NULL) )
	{
		return( eAODConfigError );
	}

	/* get the user id */
	p = strchr( inResponse, ' ' );
	if ( p != NULL )
	{
		/* make a copy */
		len = (p - inResponse);

		/* create user id container */
		*inOutUserID = (char *)malloc( len + 1 );
		if ( *inOutUserID != NULL )
		{
			memset( *inOutUserID, 0, len + 1 );
			strncpy( *inOutUserID, inResponse, len );

			if ( strlen( inResponse ) + 1 < 2048 );
			{
				/* consuem any spaces */
				while ( *p == ' ' )
				{
					p++;
				}
	
				if ( p != NULL )
				{
					strcpy( tmp, p );

					/* make the call */
					return( sValidateResponse( *inOutUserID, inChallenge, tmp, kDSStdAuthAPOP ) );
				}
			}
		}
	}

	/* something went wrong before we could make the call */

	return( eAODConfigError );

} /* odCheckAPOP */


/* -----------------------------------------------------------------
	odCRAM_MD5 ()
   ----------------------------------------------------------------- */

int odCRAM_MD5 ( const char *inUserID, const char *inChallenge, const char *inResponse  )
{
	return( sValidateResponse( inUserID, inChallenge, inResponse, kDSStdAuthCRAM_MD5 ) );
} /* aodCRAM_MD5 */


/* -----------------------------------------------------------------
	odDoAuthenticate ()
   ----------------------------------------------------------------- */

int odDoAuthenticate ( const char *inMethod,
						const char *inDigest,
						const char *inCont,
						const char *inProtocol,
						struct protstream *inStreamIn,
						struct protstream *inStreamOut,
						char **inOutCannonUser )
{
	int				result	= ODA_AUTH_FAILED;

	if ( strcasecmp( inMethod, "cram-md5" ) == 0 )
	{
		result = sDoCRAM_MD5_Auth( inStreamIn, inStreamOut, inOutCannonUser );
	}
	else if ( strcasecmp( inMethod, "PLAIN" ) == 0 )
	{
		result = sDoPlainAuth( inCont, inStreamIn, inStreamOut, inOutCannonUser );
	}
	else if ( strcasecmp( inMethod, "LOGIN" ) == 0 )
	{
		result = sDoLoginAuth( inStreamIn, inStreamOut, inOutCannonUser );
	}
	else if ( strcasecmp( inMethod, "GSSAPI" ) == 0 )
	{
		result = sDoGSS_Auth( inProtocol, inStreamIn, inStreamOut, inOutCannonUser );
	}
	else
	{
		result = ODA_PROTOCOL_ERROR;
	}

    return ( result );

} /* odDoAuthenticate */


/* -----------------------------------------------------------------
   -----------------------------------------------------------------
   -----------------------------------------------------------------
	Static functions
   -----------------------------------------------------------------
   -----------------------------------------------------------------
   ----------------------------------------------------------------- */

/* -----------------------------------------------------------------
	sDoGSS_Auth ()
   ----------------------------------------------------------------- */

int sDoGSS_Auth (	const char *inProtocol,
					struct protstream *inStreamIn,
					struct protstream *inStreamOut,
					char **inOutCannonUser )
{
	int				result	= ODA_AUTH_FAILED;
	int				length	= 0;
	int				respLen	= 0;
	char		   *ptr		= NULL;
	char			ioBuf	[ MAX_IO_BUF_SIZE ];
	char			userBuf	[ MAX_USER_BUF_SIZE ];
	char			respBuf	[ MAX_IO_BUF_SIZE ];
	char			tmpBuf	[ MAX_IO_BUF_SIZE ];
	char			prinUser[ MAX_USER_NAME_SIZE + 1 ];
	gss_buffer_desc	in_token;
	gss_buffer_desc	out_token;
	gss_buffer_desc	disp_name;
	gss_OID			mech_type;
	OM_uint32		minStatus	= 0;
	OM_uint32		majStatus	= 0;
	gss_ctx_id_t	context		= GSS_C_NO_CONTEXT;
	OM_uint32		ret_flags	= 0;
	unsigned long	maxsize		= htonl( 8192 );
	gss_name_t		clientName;
	krb5_context	krb5Context;
	krb5_principal	krb5Principal;

	/* is GSSAPI authentication enabled */
	if ( !config_getswitch( IMAPOPT_IMAP_AUTH_GSSAPI ) )
	{
		return ( ODA_METHOD_NOT_ENABLDE );
	}

	/* get the service principal and the credentials */
	result = sGSS_Init( inProtocol );
	if ( result != GSS_S_COMPLETE )
	{
		return( result );
	}

	/* set default error result */
	result = ODA_AUTH_FAILED;

	/* notify the client and get response */
	prot_printf( inStreamOut, "+ \r\n" );
	if ( prot_fgets( ioBuf, MAX_IO_BUF_SIZE, inStreamIn ) )
	{
		/* trim CRLF */
		ptr = ioBuf + strlen( ioBuf ) - 1;
		if ( (ptr >= ioBuf) && (*ptr == '\n') )
		{
			*ptr-- = '\0';
		}
		if ( (ptr >= ioBuf) && (*ptr == '\r') )
		{
			*ptr-- = '\0';
		}

		/* check if client cancelled */
		if ( ioBuf[ 0 ] == '*' )
		{
			return( ODA_AUTH_CANCEL );
		}
	}

	/* clear response buffer */
	memset( respBuf, 0, MAX_IO_BUF_SIZE );

	length = strlen( ioBuf );
	sDecodeBase64( ioBuf, length, respBuf, MAX_IO_BUF_SIZE, &respLen );

	in_token.value  = respBuf;
	in_token.length = respLen;

	do {
		// negotiate authentication
		majStatus = gss_accept_sec_context(	&minStatus,
											&context,
											stCredentials,
											&in_token,
											GSS_C_NO_CHANNEL_BINDINGS,
											&clientName,
											&mech_type,
											&out_token,
											&ret_flags,
											NULL,	/* ignore time?*/
											NULL );

		switch ( majStatus )
		{
			case GSS_S_COMPLETE:		// successful
			case GSS_S_CONTINUE_NEEDED:
			{
				if ( out_token.value )
				{
					// Encode the challenge and send it
					sEncodeBase64( (char *)out_token.value, out_token.length, ioBuf, MAX_IO_BUF_SIZE );

					prot_printf( inStreamOut, "+ %s\r\n", ioBuf );
					if ( prot_fgets( ioBuf, MAX_IO_BUF_SIZE, inStreamIn ) )
					{
						/* trim CRLF */
						ptr = ioBuf + strlen( ioBuf ) - 1;
						if ( (ptr >= ioBuf) && (*ptr == '\n') )
						{
							*ptr-- = '\0';
						}
						if ( (ptr >= ioBuf) && (*ptr == '\r') )
						{
							*ptr-- = '\0';
						}

						/* check if client cancelled */
						if ( ioBuf[ 0 ] == '*' )
						{
							return( ODA_AUTH_CANCEL );
						}
					}

					memset( respBuf, 0, MAX_IO_BUF_SIZE );

					// Decode the response
					length = strlen( ioBuf );
					sDecodeBase64( ioBuf, length, respBuf, MAX_IO_BUF_SIZE, &respLen );

					in_token.value  = respBuf;
					in_token.length = respLen;

					gss_release_buffer( &minStatus, &out_token );
				}
				break;
			}

			default:
				sLogErrors( "gss_accept_sec_context", majStatus, minStatus );
				break;
		}
	} while ( in_token.value && in_token.length && (majStatus == GSS_S_CONTINUE_NEEDED) );

	if ( majStatus == GSS_S_COMPLETE )
	{
		gss_buffer_desc		inToken;
		gss_buffer_desc		outToken;

		memcpy( tmpBuf, (void *)&maxsize, 4 );
		inToken.value	= tmpBuf;
		inToken.length	= 4;

		tmpBuf[ 0 ] = 1;

		majStatus = gss_wrap( &minStatus, context, 0, GSS_C_QOP_DEFAULT, &inToken, NULL, &outToken );
		if ( majStatus == GSS_S_COMPLETE )
		{
			// Encode the challenge and send it
			sEncodeBase64( (char *)outToken.value, outToken.length, ioBuf, MAX_IO_BUF_SIZE );

			prot_printf( inStreamOut, "+ %s\r\n", ioBuf );
			memset( ioBuf, 0, MAX_IO_BUF_SIZE );
			if ( prot_fgets( ioBuf, MAX_IO_BUF_SIZE, inStreamIn ) )
			{
				/* trim CRLF */
				ptr = ioBuf + strlen( ioBuf ) - 1;
				if ( (ptr >= ioBuf) && (*ptr == '\n') )
				{
					*ptr-- = '\0';
				}
				if ( (ptr >= ioBuf) && (*ptr == '\r') )
				{
					*ptr-- = '\0';
				}

				/* check if client cancelled */
				if ( ioBuf[ 0 ] == '*' )
				{
					return( ODA_AUTH_CANCEL );
				}
			}

			// Decode the response
			respLen = 0;
			memset( respBuf, 0, MAX_IO_BUF_SIZE );
			length = strlen( ioBuf );

			sDecodeBase64( ioBuf, length, respBuf, MAX_IO_BUF_SIZE, &respLen );

			inToken.value  = respBuf;
			inToken.length = respLen;

			gss_release_buffer( &minStatus, &outToken );

			majStatus = gss_unwrap( &minStatus, context, &inToken, &outToken, NULL, NULL );
			if ( majStatus == GSS_S_COMPLETE )
			{
				if ( (outToken.value != NULL) &&
					 (outToken.length > 4)	  &&
					 (outToken.length < MAX_USER_BUF_SIZE) )
				{
					memcpy( userBuf, outToken.value, outToken.length );
					if ( userBuf[0] & 1 )
					{
						userBuf[ outToken.length ] = '\0';
						if ( gss_display_name( &minStatus, clientName, &disp_name, &mech_type ) == GSS_S_COMPLETE  )
						{
							if ( !krb5_init_context( &krb5Context ) )
							{
								if ( !krb5_parse_name( krb5Context, disp_name.value, &krb5Principal ) )
								{
									if ( !krb5_aname_to_localname( krb5Context, krb5Principal, MAXHOSTNAMELEN-1, prinUser ) )
									{
										const char *p = userBuf+4;
										if ( !strcmp( p, prinUser ) )
										{
											*inOutCannonUser = auth_canonifyid( p, 0 );
											result = kSGSSSuccess;
										}
										else
										{
											syslog( LOG_NOTICE, "AOD Error: badlogin from: %s attempted to login with ticket for %s.", p, prinUser );
										}
									}
									krb5_free_principal( krb5Context, krb5Principal );
								}
								krb5_free_context( krb5Context );	/* finished with context */
							}
						}
						else
						{
							sLogErrors( "gss_display_name", majStatus, minStatus );
						}
					}
				}
			}
			else
			{
				sLogErrors( "gss_unwrap", majStatus, minStatus );
			}
			gss_release_buffer( &minStatus, &outToken );
		}
		else
		{
			sLogErrors( "gss_wrap", majStatus, minStatus );
		}
	}
	else
	{
		sLogErrors( "gss_accept_sec_context", majStatus, minStatus );
	}

	return( result );

} /* sDoGSS_Auth */


/* -----------------------------------------------------------------
	sDoLoginAuth ()
   ----------------------------------------------------------------- */

int sDoLoginAuth ( 	struct protstream *inStreamIn,
					struct protstream *inStreamOut,
					char **inOutCannonUser )
{
	int				result	= ODA_AUTH_FAILED;
	int				length	= 0;
	int				respLen	= 0;
	char		   *ptr		= NULL;
	char			userBuf	[ MAX_USER_BUF_SIZE ];
	char			chalBuf	[ MAX_CHAL_BUF_SIZE ];
	char			pwdBuf	[ MAX_USER_BUF_SIZE ];
	char			ioBuf	[ MAX_IO_BUF_SIZE ];

	/* is LOGIN authentication enabled */
	if ( !config_getswitch( IMAPOPT_IMAP_AUTH_LOGIN ) )
	{
		return ( ODA_METHOD_NOT_ENABLDE );
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
		ptr = ioBuf + strlen( ioBuf ) - 1;
		if ( (ptr >= ioBuf) && (*ptr == '\n') )
		{
			*ptr-- = '\0';
		}
		if ( (ptr >= ioBuf) && (*ptr == '\r') )
		{
			*ptr-- = '\0';
		}

		/* check if client cancelled */
		if ( ioBuf[ 0 ] == '*' )
		{
			return( ODA_AUTH_CANCEL );
		}
	}

	/* set default error result */
	result = ODA_PROTOCOL_ERROR;

	/* reset the buffer */
	memset( userBuf, 0, MAX_USER_BUF_SIZE );

	length = strlen( ioBuf );
	sDecodeBase64( ioBuf, length, userBuf, MAX_USER_BUF_SIZE, &respLen );

	/* encode the password prompt and send it */
	strcpy( chalBuf, "Password:" );
	sEncodeBase64( chalBuf, strlen( chalBuf ), ioBuf, MAX_IO_BUF_SIZE );
	prot_printf( inStreamOut, "+ %s\r\n", ioBuf );

	/* get the client response */
	if ( prot_fgets( ioBuf, MAX_IO_BUF_SIZE, inStreamIn ) )
	{
		/* trim CRLF */
		ptr = ioBuf + strlen( ioBuf ) - 1;
		if ( (ptr >= ioBuf) && (*ptr == '\n') )
		{
			*ptr-- = '\0';
		}
		if ( (ptr >= ioBuf) && (*ptr == '\r') )
		{
			*ptr-- = '\0';
		}

		/* check if client cancelled */
		if ( ioBuf[ 0 ] == '*' )
		{
			return( ODA_AUTH_CANCEL );
		}
	}

	/* reset the buffer */
	memset( pwdBuf, 0, MAX_USER_BUF_SIZE );

	length = strlen( ioBuf );
	sDecodeBase64( ioBuf, length, pwdBuf, MAX_USER_BUF_SIZE, &respLen );

	/* do the auth */
	result = odCheckPass( userBuf, pwdBuf );
	if ( result == eAODNoErr )
	{
		*inOutCannonUser = auth_canonifyid( userBuf, 0 );
	}

	/* nuke the password buf */
	memset( pwdBuf, 0, MAX_USER_BUF_SIZE );

	/* nuke the io buf */
	memset( ioBuf, 0, MAX_IO_BUF_SIZE );

	return( result );

} /* sDoLoginAuth */


/* -----------------------------------------------------------------
	sDoPlainAuth ()
   ----------------------------------------------------------------- */

int sDoPlainAuth ( 	const char *inCont,
					struct protstream *inStreamIn,
					struct protstream *inStreamOut,
					char **inOutCannonUser )
{
	int				result	= ODA_AUTH_FAILED;
	int				length	= 0;
	int				respLen	= 0;
	char		   *ptr		= NULL;
	char			ioBuf	[ MAX_IO_BUF_SIZE ];
	char			respBuf	[ MAX_IO_BUF_SIZE ];
	char			userBuf	[ MAX_USER_BUF_SIZE ];
	char			pwdBuf	[ MAX_USER_BUF_SIZE ];

	/* is PLAIN authentication enabled */
	if ( !config_getswitch( IMAPOPT_IMAP_AUTH_PLAIN ) )
	{
		return ( ODA_METHOD_NOT_ENABLDE );
	}

	prot_printf( inStreamOut, "%s\r\n", inCont );

	/* get the client response */
	if ( prot_fgets( ioBuf, MAX_IO_BUF_SIZE, inStreamIn ) )
	{
		/* trim CRLF */
		ptr = ioBuf + strlen( ioBuf ) - 1;
		if ( (ptr >= ioBuf) && (*ptr == '\n') )
		{
			*ptr-- = '\0';
		}
		if ( (ptr >= ioBuf) && (*ptr == '\r') )
		{
			*ptr-- = '\0';
		}

		/* check if client cancelled */
		if ( ioBuf[ 0 ] == '*' )
		{
			return( ODA_AUTH_CANCEL );
		}
	}

	/* reset the buffer */
	memset( respBuf, 0, MAX_IO_BUF_SIZE );

	/* decode the response */
	length = strlen( ioBuf );
	sDecodeBase64( ioBuf, length, respBuf, MAX_IO_BUF_SIZE, &respLen );

	/* set default error result */
	result = ODA_PROTOCOL_ERROR;

	ptr = respBuf;
	if ( *ptr == '\0' )
	{
		ptr++;
	}

	if ( ptr != NULL )
	{
		if ( strlen( ptr ) < MAX_USER_BUF_SIZE )
		{
			strcpy( userBuf, ptr );

			ptr = ptr + (strlen( userBuf ) + 1 );
			if ( ptr != NULL )
			{
				if ( strlen( ptr ) < MAX_USER_BUF_SIZE )
				{
					strcpy( pwdBuf, ptr );

					/* do the auth */
					result = odCheckPass( userBuf, pwdBuf );
					if ( result == eAODNoErr )
					{
						*inOutCannonUser = auth_canonifyid( userBuf, 0 );
					}
				}
			}
		}
	}

	/* nuke the password buf */
	memset( pwdBuf, 0, MAX_USER_BUF_SIZE );

	/* nuke the response buf */
	memset( respBuf, 0, MAX_IO_BUF_SIZE );

	return( result );

} /* sDoPlainAuth */


/* -----------------------------------------------------------------
	sDoCRAM_MD5_Auth ()
   ----------------------------------------------------------------- */

int sDoCRAM_MD5_Auth ( struct protstream *inStreamIn,
						struct protstream *inStreamOut,
						char **inOutCannonUser )
{
	int				result	= ODA_AUTH_FAILED;
	int				length	= 0;
	int				respLen	= 0;
	char		   *ptr		= NULL;
	char			userBuf	[ MAX_USER_BUF_SIZE ];
	char			chalBuf	[ MAX_CHAL_BUF_SIZE ];
	char			respBuf	[ MAX_IO_BUF_SIZE ];
	char			ioBuf	[ MAX_IO_BUF_SIZE ];
	char			hostname[ MAXHOSTNAMELEN + 1 ];
	struct timeval	tvChalTime;
	char			randbuf[ 17 ];
    struct od_user_opts	useropts;

	/* is CRAM-MD5 auth enabled */
	if ( !config_getswitch( IMAPOPT_IMAP_AUTH_CRAM_MD5 ) )
	{
		return ( ODA_METHOD_NOT_ENABLDE );
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
		ptr = ioBuf + strlen( ioBuf ) - 1;
		if ( (ptr >= ioBuf) && (*ptr == '\n') )
		{
			*ptr-- = '\0';
		}
		if ( (ptr >= ioBuf) && (*ptr == '\r') )
		{
			*ptr-- = '\0';
		}

		/* check if client cancelled */
		if ( ioBuf[ 0 ] == '*' )
		{
			return( ODA_AUTH_CANCEL );
		}
	}

	/* reset the buffer */
	memset( respBuf, 0, MAX_IO_BUF_SIZE );

	/* decode the response */
	length = strlen( ioBuf );
	sDecodeBase64( ioBuf, length, respBuf, MAX_IO_BUF_SIZE, &respLen );

	/* set default error result */
	result = ODA_PROTOCOL_ERROR;

	/* get the user name */
	ptr = strchr( respBuf, ' ' );
	if ( ptr != NULL )
	{

		length = ptr - respBuf;
		if ( length < MAX_USER_BUF_SIZE )
		{
			/* copy user name */
			memset( userBuf, 0, MAX_USER_BUF_SIZE );
			strncpy( userBuf, respBuf, length );

			/* move past the space */
			ptr++;
			if ( ptr != NULL )
			{
				/* validate the response */
				result = odCRAM_MD5( userBuf, chalBuf, ptr );
				if ( result == ODA_NO_ERROR )
				{
					odGetUserOpts( userBuf, &useropts );
					if ( useropts.fRecName[ 0 ] == '\0' )
					{
						*inOutCannonUser = auth_canonifyid( userBuf, 0 );
					}
					else
					{
						*inOutCannonUser = auth_canonifyid( useropts.fRecName, 0 );
					}
				}
			}
		}
	}

	return( result );

} /* 905_MD5_Auth */

/* -----------------------------------------------------------------
	sGetUserOptions ()
   ----------------------------------------------------------------- */

int sGetUserOptions ( const char *inUserID, struct od_user_opts *inOutOpts )
{
	int					i				= 1;
	tDirStatus			dsStatus		= eDSNoErr;
	tDirReference		dirRef			= 0;
	tDirNodeReference	searchNodeRef	= 0;

	if ( (inUserID == NULL) || (inOutOpts == NULL) )
	{
		return( -1 );
	}

	/* turns out that DS can fail with invalid ref during _any_ call
		we can recover by trying again.  However, don't retry forever */

	for ( i = 1; i < 4; i++ )
	{
		/* reset to eDSNoErr if dir ref is valid */
		dsStatus = eDSNoErr;

		/* No need to close dir ref if still valid */
		if ( dsVerifyDirRefNum( dirRef ) != eDSNoErr )
		{
			dsStatus = sOpen_ds( &dirRef );
		}

		/* we have a valid dir ref, try to get the search node */
		if ( dsStatus == eDSNoErr )
		{
			/* open search node */
			dsStatus = sGet_search_node( dirRef, &searchNodeRef );
			if ( dsStatus == eDSNoErr )
			{
				/* get user attributes from mail attribute */
				dsStatus = sGet_user_attributes( dirRef, searchNodeRef, inUserID, inOutOpts );
				(void)dsCloseDirNode( searchNodeRef );
			}
			(void)dsCloseDirService( dirRef );
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

int checkServiceACL ( struct od_user_opts *inOutOpts, const char *inGroup )
{
	int			err			= eTypeNoErr;
	int			result		= 0;
	uuid_t		userUuid;

	/* get the uuid for user */
	err = mbr_user_name_to_uuid( inOutOpts->fRecName, userUuid );
	if ( err != eTypeNoErr )
	{
		/* couldn't turn user into uuid settings form user record */
		syslog( LOG_DEBUG, "AOD: mbr_user_name_to_uuid failed for user: %s (%s)", inOutOpts->fRecName, strerror( err ) );
		return( -1 );
	}

	/* check the mail service ACL */
	err = mbr_check_service_membership( userUuid, inGroup, &result );
	if ( err == ENOENT )
	{
		/* look for all services acl */
		err = mbr_check_service_membership( userUuid, "access_all_services", &result );
	}

	if ( err == eTypeNoErr )
	{
		/* service ACL is enabled, check membership */
		if ( result != 0 )
		{
			/* we are a member, enable all mail services */
			if ( inOutOpts->fAcctState == eAcctForwarded )
			{
				/* preserve auto-forwarding */
				inOutOpts->fPOP3Login = eAcctDisabled;
				inOutOpts->fIMAPLogin = eAcctDisabled;
			}
			else
			{
				inOutOpts->fAcctState = eAcctEnabled;
				inOutOpts->fPOP3Login = eAcctProtocolEnabled;
				inOutOpts->fIMAPLogin = eAcctProtocolEnabled;
			}
		}
		else
		{
			/* we are not a member override any settings form user record */
			inOutOpts->fAcctState = eAcctNotMember;
			inOutOpts->fPOP3Login = eAcctDisabled;
			inOutOpts->fIMAPLogin = eAcctDisabled;
		}
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
	sDoesGroupExist ()
   ----------------------------------------------------------------- */

bool sDoesGroupExist ( const char *inGroup )
{
	struct group *passwd_group = getgrnam( inGroup );
	if ( passwd_group != NULL )
	{
		return( true );
	}

	return( false );

} /* sDoesGroupExist */


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
	char				   *p				= NULL;
	tDirStatus				dsStatus		= eMemoryAllocError;
	int						done			= FALSE;
	int						i				= 0;
	char				   *pAcctName		= NULL;
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
				pUserAttrType = dsBuildListFromStrings( inDirRef, kDS1AttrMailAttribute, kDS1AttrUniqueID, kDS1AttrGeneratedUID, kDSNAttrRecordName, NULL );
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
									/* Get the record name */
									(void)dsGetRecordNameFromEntry( pRecEntry, &pAcctName );

									if ( pAcctName != NULL )
									{
										if ( strlen( pAcctName ) < kONE_K_BUF )
										{
											strcpy( inOutOpts->fUserID, pAcctName );
											free( pAcctName );
											pAcctName = NULL;
										}
									}
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
													/* Get the individual mail attribute values */
													sGet_mail_values( (char *)pValueEntry->fAttributeValueData.fBufferData, inOutOpts );

													/* If we don't find duplicate users in the same node, we take the first one with
														a valid mail attribute */
													done = true;
												}
											}
											else if ( strcasecmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrUniqueID ) == 0 )
											{
												dsStatus = dsGetAttributeValue( inSearchNodeRef, pTDataBuff, 1, valueRef, &pValueEntry );
												if ( dsStatus == eDSNoErr )
												{
													/* Get uid */
													inOutOpts->fUID = strtol( pValueEntry->fAttributeValueData.fBufferData, &p, 10 );
												}
											}
											else if ( strcasecmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrGeneratedUID ) == 0 )
											{
												/* Only get the first attribute value */
												dsStatus = dsGetAttributeValue( inSearchNodeRef, pTDataBuff, 1, valueRef, &pValueEntry );
												if ( dsStatus == eDSNoErr )
												{
													/* Get the generated uid */
													if ( pValueEntry->fAttributeValueData.fBufferLength < kMAX_GUID_LEN )
													{
														strncpy( inOutOpts->fGUID, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );
													}
													else
													{
														strncpy( inOutOpts->fGUID, pValueEntry->fAttributeValueData.fBufferData, kMAX_GUID_LEN - 1 );
													}
												}
											}
											else if ( strcasecmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
											{
												/* Only get the first attribute value */
												dsStatus = dsGetAttributeValue( inSearchNodeRef, pTDataBuff, 1, valueRef, &pValueEntry );
												if ( dsStatus == eDSNoErr )
												{
													/* Get the generated uid */
													if ( pValueEntry->fAttributeValueData.fBufferLength < kONE_K_BUF )
													{
														strncpy( inOutOpts->fRecName, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );
													}
													else
													{
														strncpy( inOutOpts->fRecName, pValueEntry->fAttributeValueData.fBufferData, kONE_K_BUF - 1 );
													}
												}
											}

											if ( pValueEntry != NULL )
											{
												(void)dsDeallocAttributeValueEntry( inSearchNodeRef, pValueEntry );
												pValueEntry = NULL;
											}
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
							}
							else
							{
								done = true;
								if ( uiRecCount > 1 )
								{
									syslog( LOG_NOTICE, "Duplicate users %s found in directory.", inUserID );
								}
								inOutOpts->fUserID[ 0 ] = '\0';
								dsStatus = eDSUserUnknown;
							}
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
	sLook_up_user ()
   ----------------------------------------------------------------- */

tDirStatus sLook_up_user ( tDirReference inDirRef,
						  tDirNodeReference inSearchNodeRef,
						  const char *inUserID,
						  char **outUserLocation )
{
	tDirStatus				dsStatus		= eMemoryAllocError;
	int						done			= FALSE;
	char				   *pAcctName		= NULL;
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
				pUserAttrType = dsBuildListFromStrings( inDirRef, kDSNAttrMetaNodeLocation, NULL );
				if ( pUserAttrType != NULL )
				{
					do {
						/* Get the user record(s) that matches the name */
						dsStatus = dsGetRecordList( inSearchNodeRef, pTDataBuff, &tdlRecName, eDSiExact, pUserRecType,
													pUserAttrType, FALSE, &uiRecCount, &pContext );

						if ( dsStatus == eDSNoErr )
						{
							dsStatus = eDSInvalidName;
							if ( uiRecCount == 1 ) 
							{
								dsStatus = dsGetRecordEntry( inSearchNodeRef, pTDataBuff, 1, &attrListRef, &pRecEntry );
								if ( dsStatus == eDSNoErr )
								{
									/* Get the record name */
									(void)dsGetRecordNameFromEntry( pRecEntry, &pAcctName );
			
									dsStatus = dsGetAttributeEntry( inSearchNodeRef, pTDataBuff, attrListRef, 1, &valueRef, &pAttrEntry );
									if ( (dsStatus == eDSNoErr) && (pAttrEntry != NULL) )
									{
										dsStatus = dsGetAttributeValue( inSearchNodeRef, pTDataBuff, 1, valueRef, &pValueEntry );
										if ( (dsStatus == eDSNoErr) && (pValueEntry != NULL) )
										{
											if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
											{
												/* Get the user location */
												*outUserLocation = (char *)calloc( pValueEntry->fAttributeValueData.fBufferLength + 1, sizeof( char ) );
												memcpy( *outUserLocation, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );

												/* If we don't find duplicate users in the same node, we take the first one with
													a valid mail attribute */
												done = TRUE;
											}
											dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
											pValueEntry = NULL;
											(void)dsCloseAttributeValueList( valueRef );
										}

										(void)dsDeallocAttributeEntry( inSearchNodeRef, pAttrEntry );
										pAttrEntry = NULL;

										(void)dsCloseAttributeList( attrListRef );
									}

									if ( pRecEntry != NULL )
									{
										(void)dsDeallocRecordEntry( inSearchNodeRef, pRecEntry );
										pRecEntry = NULL;
									}
								}
							}
							else
							{
								done = true;
								if ( uiRecCount > 1 )
								{
									syslog( LOG_NOTICE, "Duplicate users %s found in directory.", inUserID );
								}
								dsStatus = eDSAuthInvalidUserName;
							}
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

	if ( pAcctName != NULL )
	{
		free( pAcctName );
		pAcctName = NULL;
	}

	return( dsStatus );

} /* sLook_up_user */


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
	tDirStatus				dsStatus		= eDSNoErr;
	int						iResult			= -1;
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
				/* User Name */
				len = nameLen;
				memcpy( &(pAuthBuff->fBufferData[ curr ]), &len, sizeof( unsigned long ) );
				curr += sizeof( unsigned long );
				memcpy( &(pAuthBuff->fBufferData[ curr ]), inUserID, len );
				curr += len;

				/* Password */
				len = passwdLen;
				memcpy( &(pAuthBuff->fBufferData[ curr ]), &len, sizeof( unsigned long ) );
				curr += sizeof( unsigned long );
				memcpy( &(pAuthBuff->fBufferData[ curr ]), inPasswd, len );
				curr += len;

				pAuthBuff->fBufferLength = curr;

				dsStatus = dsDoDirNodeAuth( inUserNodeRef, pAuthType, true, pAuthBuff, pStepBuff, NULL );
				if ( dsStatus == eDSNoErr )
				{
					iResult = 0;
				}
				else
				{
					syslog( LOG_ERR, "AOD: Authentication failed for user %s. (Open Directory error: %d)", inUserID, dsStatus );
					iResult = -7;
				}

				(void)dsDataNodeDeAllocate( inDirRef, pAuthType );
				pAuthType = NULL;
			}
			else
			{
				syslog( LOG_ERR, "AOD: Authentication failed for user %s.  Unable to allocate memory.", inUserID );
				iResult = -6;
			}
			(void)dsDataNodeDeAllocate( inDirRef, pStepBuff );
			pStepBuff = NULL;
		}
		else
		{
			syslog( LOG_ERR, "AOD: Authentication failed for user %s.  Unable to allocate memory.", inUserID );
			iResult = -5;
		}
		(void)dsDataNodeDeAllocate( inDirRef, pAuthBuff );
		pAuthBuff = NULL;
	}

	return( iResult );

} /* sDoCryptAuth */


/* -----------------------------------------------------------------
	sValidateResponse ()
   ----------------------------------------------------------------- */

int sValidateResponse ( const char *inUserID, const char *inChallenge, const char *inResponse, const char *inAuthType )
{
	int					iResult			= -1;
	tDirStatus			dsStatus		= eDSNoErr;
	tDirReference		dirRef			= 0;
	tDirNodeReference	searchNodeRef	= 0;
	tDirNodeReference	userNodeRef		= 0;
	tDataBuffer		   *pAuthBuff		= NULL;
	tDataBuffer		   *pStepBuff		= NULL;
	tDataNode		   *pAuthType		= NULL;
	char			   *userLoc			= NULL;
	unsigned long		uiNameLen		= 0;
	unsigned long		uiChalLen		= 0;
	unsigned long		uiRespLen		= 0;
	unsigned long		uiBuffSzie		= 0;
	unsigned long		uiCurr			= 0;
	unsigned long		uiLen			= 0;

	if ( (inUserID == NULL) || (inChallenge == NULL) || (inResponse == NULL) || (inAuthType == NULL) )
	{
		return( -1 );
	}

	uiNameLen = strlen( inUserID );
	uiChalLen = strlen( inChallenge );
	uiRespLen = strlen( inResponse );

	uiBuffSzie = uiNameLen + uiChalLen + uiRespLen + 32;

	dsStatus = sOpen_ds( &dirRef );
	if ( dsStatus == eDSNoErr )
	{
		dsStatus = sGet_search_node( dirRef, &searchNodeRef );
		if ( dsStatus == eDSNoErr )
		{
			dsStatus = sLook_up_user( dirRef, searchNodeRef, inUserID, &userLoc );
			if ( dsStatus == eDSNoErr )
			{
				dsStatus = sOpen_user_node( dirRef, userLoc, &userNodeRef );
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
								memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), inUserID, uiLen );
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
										sSetErrorText( eTypeAuthWarnNewPW, dsStatus, inUserID );
										iResult = eAODNoErr;
										break;
			
									case eDSAuthPasswordExpired:
										sSetErrorText( eTypeAuthWarnExpirePW, dsStatus, inUserID );
										iResult = eAODNoErr;
										break;

									default:
										sSetErrorText( eTypeAuthFailed, dsStatus, inUserID );
										iResult = eAODAuthFailed;
										break;
								}
								(void)dsDataNodeDeAllocate( dirRef, pAuthType );
								pAuthType = NULL;
							}
							else
							{
								sSetErrorText( eTypeUserNotFound, dsStatus, inUserID );
								syslog( LOG_ERR, "AOD: Authentication failed for user %s.  Unable to allocate memory.", inUserID );
								iResult = eAODAllocError;
							}
							(void)dsDataNodeDeAllocate( dirRef, pStepBuff );
							pStepBuff = NULL;
						}
						else
						{
							sSetErrorText( eTypeUserNotFound, dsStatus, inUserID );
							syslog( LOG_ERR, "AOD: Authentication failed for user %s.  Unable to allocate memory.", inUserID );
							iResult = eAODAllocError;
						}
						(void)dsDataNodeDeAllocate( dirRef, pAuthBuff );
						pAuthBuff = NULL;
					}
					else
					{
						sSetErrorText( eTypeAllocError, dsStatus, inUserID );
						syslog( LOG_ERR, "AOD: Authentication failed for user %s.  Unable to allocate memory.", inUserID );
						iResult = eAODAllocError;
					}
					(void)dsCloseDirNode( userNodeRef );
					userNodeRef = 0;
				}
				else
				{
					sSetErrorText( eTypeCantOpenUserNode, dsStatus, inUserID );
					syslog( LOG_ERR, "AOD: Unable to open user directory node for user %s. (Open Directory error: %d)", inUserID, dsStatus );
					iResult = eAODCantOpenUserNode;
				}
			}
			else
			{
				sSetErrorText( eTypeUserNotFound, dsStatus, inUserID );
				syslog( LOG_ERR, "AOD: Unable to find user %s. (Open Directory error: %d)", inUserID, dsStatus );
				iResult = eAODUserNotFound;
			}
			(void)dsCloseDirNode( searchNodeRef );
			searchNodeRef = 0;
		}
		else
		{
			sSetErrorText( eTypeOpenSearchFailed, dsStatus, inUserID );
			syslog( LOG_ERR, "AOD: Unable to open Directory search node. (Open Directory error: %d)", dsStatus );
			iResult = eAODOpenSearchFailed;
		}
		(void)dsCloseDirService( dirRef );
		dirRef = 0;
	}
	else
	{
		sSetErrorText( eTypeOpenDSFailed, dsStatus, inUserID );
		syslog( LOG_ERR, "AOD: Unable to open Directory. (Open Directory error: %d)", dsStatus );
		iResult = eAODOpenDSFailed;
	}

	if ( userLoc != nil )
	{
		free( userLoc );
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
	int				iResult 	= 0;
	bool			bFound		= FALSE;
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	bFound = CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAttrVersion ) );
	if ( bFound == true )
	{
		iResult = eInvalidDataType;

		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAttrVersion ) );
		if ( cfStringRef != NULL )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				iResult = eItemNotFound;

				pValue = (char *)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
				if ( pValue != NULL )
				{
					iResult = eWrongVersion;

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
	bool			bFound		= FALSE;
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	/* Default value */
	inOutOpts->fAcctState = eAcctDisabled;

	bFound = CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAcctState ) );
	if ( bFound == true )
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
						inOutOpts->fAcctState = eAcctEnabled;
					}
					else if ( strcasecmp( pValue, kXMLValueAcctDisabled ) == 0 )
					{
						inOutOpts->fAcctState = eAcctDisabled;
					}
					else if ( strcasecmp( pValue, kXMLValueAcctFwd ) == 0 )
					{
						sGet_auto_forward( inCFDictRef, inOutOpts );
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
	bool			bFound		= FALSE;
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	bFound = CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAutoFwd ) );
	if ( bFound == true )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAutoFwd ) );
		if ( cfStringRef != NULL )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				pValue = (char *)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
				if ( pValue != NULL )
				{
					if ( strlen( pValue ) < kONE_K_BUF )
					{
						inOutOpts->fAcctState = eAcctForwarded;
						inOutOpts->fPOP3Login = eAcctDisabled;
						inOutOpts->fIMAPLogin = eAcctDisabled;

						strcpy( inOutOpts->fAutoFwdAddr, pValue );
					}
				}
			}
		}
	}
} /* sGet_auto_forward */


/* -----------------------------------------------------------------
	sGet_IMAP_login ()
   ----------------------------------------------------------------- */

void sGet_IMAP_login ( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts )
{
	bool			bFound		= FALSE;
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	/* Default value */
	inOutOpts->fIMAPLogin = eAcctDisabled;

	bFound = CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeykIMAPLoginState ) );
	if ( bFound == true )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeykIMAPLoginState ) );
		if ( cfStringRef != NULL )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				pValue = (char *)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
				if ( pValue != NULL )
				{
					if ( strcasecmp( pValue, kXMLValueIMAPLoginOK ) == 0 )
					{
						inOutOpts->fIMAPLogin = eAcctProtocolEnabled;
					}
				}
			}
		}
	}
} /* sGet_IMAP_login */


/* -----------------------------------------------------------------
	sGet_POP3_login ()
   ----------------------------------------------------------------- */

void sGet_POP3_login ( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts )
{
	bool			bFound		= FALSE;
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	/* Default value */
	inOutOpts->fPOP3Login = eAcctDisabled;

	bFound = CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyPOP3LoginState ) );
	if ( bFound == true )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyPOP3LoginState ) );
		if ( cfStringRef != NULL )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				pValue = (char *)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
				if ( pValue != NULL )
				{
					if ( strcasecmp( pValue, kXMLValuePOP3LoginOK ) == 0 )
					{
						inOutOpts->fPOP3Login = eAcctProtocolEnabled;
					}
				}
			}
		}
	}
} /* sGet_POP3_login */


/* -----------------------------------------------------------------
	sGet_acct_loc ()
   ----------------------------------------------------------------- */

void sGet_acct_loc ( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts )
{
	bool			bFound		= FALSE;
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	/* Default value */
	inOutOpts->fAccountLoc[ 0 ] = '\0';

	bFound = CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAcctLoc ) );
	if ( bFound == true )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAcctLoc ) );
		if ( cfStringRef != NULL )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				pValue = (char*)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
				if ( (pValue != NULL) && (strlen( pValue ) < kONE_K_BUF) )
				{
					if ( inOutOpts->fAccountLoc != NULL )
					{
						strcpy( inOutOpts->fAccountLoc, pValue );
					}
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
	bool			bFound		= FALSE;
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	/* Default value */
	inOutOpts->fAltDataLoc[ 0 ] = '\0';

	bFound = CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAltDataStoreLoc ) );
	if ( bFound == true )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAltDataStoreLoc ) );
		if ( cfStringRef != NULL )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				pValue = (char*)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
				if ( (pValue != NULL) && (strlen( pValue ) < kONE_K_BUF) )
				{
					if ( inOutOpts->fAltDataLoc != NULL )
					{
						strcpy( inOutOpts->fAltDataLoc, pValue );
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
	bool			bFound		= FALSE;
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	/* Default value */
	inOutOpts->fDiskQuota = 0;

	bFound = CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyDiskQuota ) );
	if ( bFound == true )
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
	sSetErrorText ()
   ----------------------------------------------------------------- */

void sSetErrorText ( eErrorType inErrType, int inError, const char *inStr )
{
	const char *pErrText	= NULL;

	if ( inErrType <= eMaxErrors )
	{
		pErrText = odText[ inErrType ];

		if ( inStr != NULL )
		{
			sprintf( gErrStr, pErrText, inStr, inError );
		}
		else
		{
			sprintf( gErrStr, pErrText, inError );
		}
	}
	else
	{
		sprintf( gErrStr, "Unknown error type: %d", inErrType );
	}
} /* sSetErrorText */



/* -----------------------------------------------------------------
	sGSS_Init ()
   ----------------------------------------------------------------- */

int sGSS_Init ( const char *inProtocol )
{
	int					iResult		= GSS_S_COMPLETE;
	char			   *pService	= NULL;
	gss_buffer_desc		nameToken;
	gss_name_t			principalName;
	OM_uint32			majStatus	= 0;
	OM_uint32			minStatus	= 0;

	pService = sGetServerPrincipal( inProtocol );
	if ( pService == NULL )
	{
		syslog( LOG_ERR, "No service principal found for: %s", inProtocol );
		return( GSS_S_NO_CRED );
	}

	nameToken.value		= pService;
	nameToken.length	= strlen( pService );

	majStatus = gss_import_name( &minStatus, 
									&nameToken, 
									GSS_KRB5_NT_PRINCIPAL_NAME,	 //gss_nt_service_name
									&principalName );

	if ( majStatus != GSS_S_COMPLETE )
	{
		sLogErrors( "gss_import_name", majStatus, minStatus );
		iResult = kSGSSImportNameErr;
	}
	else
	{
		majStatus = gss_acquire_cred( &minStatus, 
										principalName, 
										GSS_C_INDEFINITE, 
										GSS_C_NO_OID_SET,
										GSS_C_ACCEPT,
									   &stCredentials,
										NULL, 
										NULL );

		if ( majStatus != GSS_S_COMPLETE )
		{
			sLogErrors( "gss_acquire_cred", majStatus, minStatus );
			iResult = kSGSSAquireCredErr;
		}
		(void)gss_release_name( &minStatus, &principalName );
	}

	free( pService );

	return( iResult );

} /* sGSS_Init */


/* -----------------------------------------------------------------
	get_realm_form_creds ()
   ----------------------------------------------------------------- */

int get_realm_form_creds ( char *inBuffer, int inSize )
{
	int			iResult		= GSS_S_COMPLETE;
	char		buffer[256] = {0};
	char	   *token1		= NULL;

	iResult = sGetPrincipalStr( buffer, 256 );
	if ( iResult == 0 )
	{
		token1 = strrchr( buffer, '@' );
		if ( token1 != NULL )
		{
			++token1;
			if ( strlen( token1 ) > inSize - 1 )
			{
				iResult = kSGSSBufferSizeErr;
			}
			else
			{
				strncpy( inBuffer, token1, inSize - 1 );
				inBuffer[ strlen( token1 ) ] = 0;
			}
		}
		else
		{
			iResult = kUnknownErr;
		}
	}

	return( iResult );

} /* get_realm_form_creds */


/* -----------------------------------------------------------------
	sGetPrincipalStr ()
   ----------------------------------------------------------------- */

int sGetPrincipalStr ( char *inOutBuf, int inSize )
{
	OM_uint32		minStatus	= 0;
	OM_uint32		majStatus	= 0;
	gss_name_t		principalName;
	gss_buffer_desc	token;
	gss_OID			id;

	majStatus = gss_inquire_cred(&minStatus, stCredentials, &principalName,  NULL, NULL, NULL);
	if ( majStatus != GSS_S_COMPLETE )
	{
		return( kSGSSInquireCredErr );
	}

	majStatus = gss_display_name( &minStatus, principalName, &token, &id );
	if ( majStatus != GSS_S_COMPLETE )
	{
		return( kSGSSInquireCredErr );
	}

	majStatus = gss_release_name( &minStatus, &principalName );
	if ( inSize - 1 < token.length )
	{
		return( kSGSSBufferSizeErr );
	}

	strncpy( inOutBuf, (char *)token.value, token.length );
	inOutBuf[ token.length ] = 0;

	(void)gss_release_buffer( &minStatus, &token );

	return( GSS_S_COMPLETE );

} /* sGetPrincipalStr */


/* -----------------------------------------------------------------
	sGetServerPrincipal ()
   ----------------------------------------------------------------- */

char * sGetServerPrincipal ( const char *inServerKey )
{
    FILE			   *pFile		= NULL;
	char			   *outStr		= NULL;
	char			   *buf			= NULL;
	ssize_t				bytes		= 0;
	struct stat			fileStat;
	bool				bFound		= FALSE;
	CFStringRef			cfStringRef	= NULL;
	char			   *pValue		= NULL;
	CFDataRef			cfDataRef	= NULL;
	CFPropertyListRef	cfPlistRef	= NULL;
	CFDictionaryRef		cfDictRef	= NULL;
	CFDictionaryRef		cfDictCyrus	= NULL;

    pFile = fopen( kPlistFilePath, "r" );
    if ( pFile == NULL )
	{
		syslog( LOG_ERR, "Cannot open principal file" );
		return( NULL );
	}

	if ( -1 == fstat( fileno( pFile ), &fileStat ) )
	{
		fclose( pFile );
		syslog( LOG_ERR, "Cannot get stat on principal file" );
		return( NULL );
	}

	buf = (char *)malloc( fileStat.st_size + 1 );
	if ( buf == NULL )
	{
		fclose( pFile );
		syslog( LOG_ERR, "Cannot alloc principal buffer" );
		return( NULL );
	}

	memset( buf, 0, fileStat.st_size + 1 );
	bytes = read( fileno( pFile ), buf, fileStat.st_size );
	if ( -1 == bytes )
	{
		fclose( pFile );
		free( buf );
		syslog( LOG_ERR, "Cannot read principal file" );
		return( NULL );
	}

	cfDataRef = CFDataCreate( NULL, (const UInt8 *)buf, fileStat.st_size );
	if ( cfDataRef != NULL )
	{
		cfPlistRef = CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfDataRef, kCFPropertyListImmutable, NULL );
		if ( cfPlistRef != NULL )
		{
			if ( CFDictionaryGetTypeID() == CFGetTypeID( cfPlistRef ) )
			{
				cfDictRef = (CFDictionaryRef)cfPlistRef;

				bFound = CFDictionaryContainsKey( cfDictRef, CFSTR( kXMLDictionary ) );
				if ( bFound == true )
				{
					cfDictCyrus = (CFDictionaryRef)CFDictionaryGetValue( cfDictRef, CFSTR( kXMLDictionary ) );
					if ( cfDictCyrus != NULL )
					{
						if ( strcmp( inServerKey, kXMLIMAP_Principal ) == 0 )
						{
							cfStringRef = (CFStringRef)CFDictionaryGetValue( cfDictCyrus, CFSTR( kXMLIMAP_Principal ) );
						}
						else
						{
							cfStringRef = (CFStringRef)CFDictionaryGetValue( cfDictCyrus, CFSTR( kXMLPOP3_Principal ) );
						}
						if ( cfStringRef != NULL )
						{
							if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
							{
								pValue = (char *)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
								if ( pValue != NULL )
								{
									outStr = malloc( strlen( pValue ) + 1 );
									if ( outStr != NULL )
									{
										strcpy( outStr, pValue );
									}
								}
							}
						}
					}
				}
			}
			CFRelease( cfPlistRef );
		}
		CFRelease( cfDataRef );
	}

	return( outStr );

} /* sGetServerPrincipal */


/* -----------------------------------------------------------------
	export_and_print ()
   ----------------------------------------------------------------- */

OM_uint32 export_and_print ( const gss_name_t principalName, char *outUserID )
{
	char		   *p			= NULL;
	OM_uint32		minStatus	= 0;
	OM_uint32		majStatus	= 0;
	gss_OID			mechid;
	gss_buffer_desc nameToken;

	majStatus = gss_display_name( &minStatus, principalName, &nameToken, &mechid );

	p = strstr( (char *)nameToken.value, "@" );
	if ( p != NULL )
	{
		strncpy( outUserID, (char *)nameToken.value, p - (char *)nameToken.value );
	}
	else
	{
		strncpy( outUserID, (char *)nameToken.value, nameToken.length );
	}

	(void)gss_release_buffer( &minStatus, &nameToken );

	return( majStatus );

} /* export_and_print */


/* -----------------------------------------------------------------
	sLogErrors ()
   ----------------------------------------------------------------- */

void sLogErrors ( char *inName, OM_uint32 inMajor, OM_uint32 inMinor )
{
	OM_uint32		msg_context = 0;
	OM_uint32		minStatus = 0;
	OM_uint32		majStatus = 0;
	gss_buffer_desc errBuf;
	int				count = 1;

	do {
		majStatus = gss_display_status( &minStatus, inMajor, GSS_C_GSS_CODE, GSS_C_NULL_OID, &msg_context, &errBuf );

		syslog( LOG_ERR, "  Major Error (%d): %s (%s)", count, (char *)errBuf.value, inName );

		majStatus = gss_release_buffer( &minStatus, &errBuf );
		++count;
	} while ( msg_context != 0 );

	count = 1;
	msg_context = 0;
	do {
		majStatus = gss_display_status( &minStatus, inMinor, GSS_C_MECH_CODE, GSS_C_NULL_OID, &msg_context, &errBuf );

		syslog( LOG_ERR, "  Minor Error (%d): %s (%s)", count, (char *)errBuf.value, inName );

		majStatus = gss_release_buffer( &minStatus, &errBuf );
		++count;

	} while ( msg_context != 0 );

} // sLogErrors


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


/* SSL    callback */
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
		syslog( LOG_ERR, "AOD Error: SecKeychainSetPreferenceDomain returned status: %d", status );
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
			syslog( LOG_ERR, "AOD Error: Invalid buffer size callback (size:%d, len:%d)", inSize, pwdLen );
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
		syslog( LOG_ERR, "AOD Error: SecKeychainSetPreferenceDomain: No keychain is available" );
	}
	else if ( status == errSecItemNotFound )
	{
		syslog( LOG_ERR, "AOD Error: SecKeychainSetPreferenceDomain: The requested key could not be found in the system keychain");
	}
	else if (status != noErr)
	{
		syslog( LOG_ERR, "AOD Error: SecKeychainFindGenericPassword returned status %d", status );
	}

	return( 0 );
}

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
		syslog( LOG_ERR, "Cannot open /dev/urandom" );

		/* try to open /dev/random */
		file = open( "/dev/random", O_RDONLY, 0 );
	}

	if ( file == -1 )
	{
		syslog( LOG_ERR, "Cannot open /dev/random" );

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
