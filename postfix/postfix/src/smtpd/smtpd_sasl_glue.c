/*++
/* NAME
/*	smtpd_sasl_glue 3
/* SUMMARY
/*	Postfix SMTP server, SASL support interface
/* SYNOPSIS
/*	#include "smtpd_sasl_glue.h"
/*
/*	void    smtpd_sasl_initialize()
/*
/*	void	smtpd_sasl_connect(state, sasl_opts_name, sasl_opts_val)
/*	SMTPD_STATE *state;
/*	const char *sasl_opts_name;
/*	const char *sasl_opts_val;
/*
/*	char	*smtpd_sasl_authenticate(state, sasl_method, init_response)
/*	SMTPD_STATE *state;
/*	const char *sasl_method;
/*	const char *init_response;
/*
/*	void	smtpd_sasl_logout(state)
/*	SMTPD_STATE *state;
/*
/*	void	smtpd_sasl_disconnect(state)
/*	SMTPD_STATE *state;
/* DESCRIPTION
/*	This module encapsulates most of the detail specific to SASL
/*	authentication.
/*
/*	smtpd_sasl_initialize() initializes the SASL library. This
/*	routine should be called once at process start-up. It may
/*	need access to the file system for run-time loading of
/*	plug-in modules. There is no corresponding cleanup routine.
/*
/*	smtpd_sasl_connect() performs per-connection initialization.
/*	This routine should be called once at the start of every
/*	connection. The sasl_opts_name and sasl_opts_val parameters
/*	are the postfix configuration parameters setting the security
/*	policy of the SASL authentication.
/*
/*	smtpd_sasl_authenticate() implements the authentication
/*	dialog.  The result is zero in case of success, -1 in case
/*	of failure. smtpd_sasl_authenticate() updates the following
/*	state structure members:
/* .IP sasl_method
/*	The authentication method that was successfully applied.
/*	This member is a null pointer in the absence of successful
/*	authentication.
/* .IP sasl_username
/*	The username that was successfully authenticated.
/*	This member is a null pointer in the absence of successful
/*	authentication.
/* .PP
/*	smtpd_sasl_logout() cleans up after smtpd_sasl_authenticate().
/*	This routine exists for the sake of symmetry.
/*
/*	smtpd_sasl_disconnect() performs per-connection cleanup.
/*	This routine should be called at the end of every connection.
/*
/*	Arguments:
/* .IP state
/*	SMTP session context.
/* .IP sasl_opts_name
/*	Security options parameter name.
/* .IP sasl_opts_val
/*	Security options parameter value.
/* .IP sasl_method
/*	A SASL mechanism name
/* .IP init_reply
/*	An optional initial client response.
/* DIAGNOSTICS
/*	All errors are fatal.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Initial implementation by:
/*	Till Franke
/*	SuSE Rhein/Main AG
/*	65760 Eschborn, Germany
/*
/*	Adopted by:
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE_OS_X_SERVER__
#include <syslog.h>
#include <stdio.h>
#include <sys/stat.h>
#endif /* __APPLE_OS_X_SERVER__ */

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <stringops.h>

/* Global library. */

#include <mail_params.h>

/* XSASL library. */

#include <xsasl.h>

/* Application-specific. */

#include "smtpd.h"
#include "smtpd_sasl_glue.h"
#include "smtpd_chat.h"

#ifdef __APPLE_OS_X_SERVER__
/* Apple Open Directory */

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPropertyList.h>

/* kerberos */
//#include <Kerberos/Kerberos.h>
#include <Kerberos/gssapi_krb5.h>
//#include <Kerberos/gssapi.h>

/* mach */

#include <mach/boolean.h>
#endif /* __APPLE_OS_X_SERVER__ */

#ifdef USE_SASL_AUTH

/*
 * Silly little macros.
 */
#define STR(s)	vstring_str(s)

 /*
  * SASL server implementation handle.
  */
static XSASL_SERVER_IMPL *smtpd_sasl_impl;

#ifdef __APPLE_OS_X_SERVER__

void log_errors ( char *inName, OM_uint32 inMajor, OM_uint32 inMinor );

/* Apple's Password Server */

static NAME_MASK smtpd_pw_server_mask[] = {
    "none",     PW_SERVER_NONE,
    "login",    PW_SERVER_LOGIN,
    "plain",    PW_SERVER_PLAIN,
    "cram-md5", PW_SERVER_CRAM_MD5,
    "gssapi",   PW_SERVER_GSSAPI,
    0,
};

#endif /* __APPLE_OS_X_SERVER__ */

/* smtpd_sasl_initialize - per-process initialization */

#ifdef __APPLE_OS_X_SERVER__
void    smtpd_sasl_initialize( int in_use_pw_server )
{
	if ( in_use_pw_server )
	{
		smtpd_pw_server_sasl_opts = name_mask( VAR_SMTPD_PW_SERVER_OPTS, smtpd_pw_server_mask,
									 var_smtpd_pw_server_opts );
	}
#else /* __APPLE_OS_X_SERVER__ */
void    smtpd_sasl_initialize(void)
{
#endif /* __APPLE_OS_X_SERVER__ */

    /*
     * Sanity check.
     */
    if (smtpd_sasl_impl)
	msg_panic("smtpd_sasl_initialize: repeated call");

    /*
     * Initialize the SASL library.
     */
    if ((smtpd_sasl_impl = xsasl_server_init(var_smtpd_sasl_type,
					     var_smtpd_sasl_path)) == 0)
	msg_fatal("SASL per-process initialization failed");

}

/* smtpd_sasl_connect - per-connection initialization */

void    smtpd_sasl_connect(SMTPD_STATE *state, const char *sasl_opts_name,
			           const char *sasl_opts_val)
{
    const char *mechanism_list;

    /*
     * Initialize SASL-specific state variables. Use long-lived storage for
     * base 64 conversion results, rather than local variables, to avoid
     * memory leaks when a read or write routine returns abnormally after
     * timeout or I/O error.
     */
    state->sasl_reply = vstring_alloc(20);
    state->sasl_mechanism_list = 0;
    state->sasl_username = 0;
    state->sasl_method = 0;
    state->sasl_sender = 0;

    /*
     * Set up a new server context for this connection.
     */
#define SMTPD_SASL_SERVICE "smtp"

    if ((state->sasl_server =
	 xsasl_server_create(smtpd_sasl_impl, state->client,
			     SMTPD_SASL_SERVICE, *var_smtpd_sasl_realm ?
			     var_smtpd_sasl_realm : (char *) 0,
			     sasl_opts_val)) == 0)
	msg_fatal("SASL per-connection initialization failed");

    /*
     * Get the list of authentication mechanisms.
     */
    if ((mechanism_list =
	 xsasl_server_get_mechanism_list(state->sasl_server)) == 0)
	msg_fatal("no SASL authentication mechanisms");
    state->sasl_mechanism_list = mystrdup(mechanism_list);
}

/* smtpd_sasl_disconnect - per-connection cleanup */

void    smtpd_sasl_disconnect(SMTPD_STATE *state)
{
    if (state->sasl_reply) {
	vstring_free(state->sasl_reply);
	state->sasl_reply = 0;
    }
    if (state->sasl_mechanism_list) {
	myfree(state->sasl_mechanism_list);
	state->sasl_mechanism_list = 0;
    }
    if (state->sasl_username) {
	myfree(state->sasl_username);
	state->sasl_username = 0;
    }
    if (state->sasl_method) {
	myfree(state->sasl_method);
	state->sasl_method = 0;
    }
    if (state->sasl_sender) {
	myfree(state->sasl_sender);
	state->sasl_sender = 0;
    }
    if (state->sasl_server) {
	xsasl_server_free(state->sasl_server);
	state->sasl_server = 0;
    }
}

/* smtpd_sasl_authenticate - per-session authentication */

int     smtpd_sasl_authenticate(SMTPD_STATE *state,
				        const char *sasl_method,
				        const char *init_response)
{
    int     status;
    const char *sasl_username;

    /*
     * SASL authentication protocol start-up. Process any initial client
     * response that was sent along in the AUTH command.
     */
    for (status = xsasl_server_first(state->sasl_server, sasl_method,
				     init_response, state->sasl_reply);
	 status == XSASL_AUTH_MORE;
	 status = xsasl_server_next(state->sasl_server, STR(state->buffer),
				    state->sasl_reply)) {

	/*
	 * Send a server challenge.
	 */
	smtpd_chat_reply(state, "334 %s", STR(state->sasl_reply));

	/*
	 * Receive the client response. "*" means that the client gives up.
	 * XXX For now we ignore the fact that an excessively long response
	 * will be chopped into multiple reponses. To handle such responses,
	 * we need to change smtpd_chat_query() so that it returns an error
	 * indication.
	 */
	smtpd_chat_query(state);
	if (strcmp(STR(state->buffer), "*") == 0) {
	    msg_warn("%s[%s]: SASL %s authentication aborted",
		     state->name, state->addr, sasl_method);
	    smtpd_chat_reply(state, "501 5.7.0 Authentication aborted");
	    return (-1);
	}
    }
    if (status != XSASL_AUTH_DONE) {
	msg_warn("%s[%s]: SASL %s authentication failed: %s",
		 state->name, state->addr, sasl_method,
		 STR(state->sasl_reply));
	smtpd_chat_reply(state, "535 5.7.0 Error: authentication failed: %s",
			 STR(state->sasl_reply));
	return (-1);
    }
    smtpd_chat_reply(state, "235 2.0.0 Authentication successful");
    if ((sasl_username = xsasl_server_get_username(state->sasl_server)) == 0)
	msg_panic("cannot look up the authenticated SASL username");
    state->sasl_username = mystrdup(sasl_username);
    printable(state->sasl_username, '?');
    state->sasl_method = mystrdup(sasl_method);
    printable(state->sasl_method, '?');

    return (0);
}

/* smtpd_sasl_logout - clean up after smtpd_sasl_authenticate */

void    smtpd_sasl_logout(SMTPD_STATE *state)
{
    if (state->sasl_username) {
	myfree(state->sasl_username);
	state->sasl_username = 0;
    }
    if (state->sasl_method) {
	myfree(state->sasl_method);
	state->sasl_method = 0;
    }
}

#ifdef __APPLE_OS_X_SERVER__
/* -----------------------------------------------------------------
	- Password Server auth methods
   ----------------------------------------------------------------- */

static tDirStatus	sOpen_ds			( tDirReference *inOutDirRef );
static tDirStatus	sGet_search_node	( tDirReference inDirRef, tDirNodeReference *outSearchNodeRef );
static tDirStatus	sLook_up_user		( tDirReference inDirRef, tDirNodeReference inSearchNodeRef, const char *inUserID, char **outUserLocation );
static tDirStatus	sOpen_user_node		( tDirReference inDirRef, const char *inUserLoc, tDirNodeReference *outUserNodeRef );
static int			sValidateResponse	( const char *inUserID, const char *inChallenge, const char *inResponse, const char *inAuthType );
static int			sDoCryptAuth		( tDirReference inDirRef, tDirNodeReference inUserNodeRef, const char *inUserID, const char *inPasswd );
static int			sEncodeBase64		( const char *inStr, const int inLen, char *outStr, int outLen );
static int			sDecodeBase64		( const char *inStr, const int inLen, char *outStr, int outLen, int *destLen );
static int			sClearTextCrypt		( const char *inUserID, const char *inPasswd );
static int			gss_Init			( void );
static int			get_principal_str	( char *inOutBuf, int inSize );
static int			get_realm_form_creds( char *inBuffer, int inSize );
static char *		get_server_principal( void );
static OM_uint32	display_name		( const gss_name_t principalName );

#define	MAX_USER_BUF_SIZE		512
#define	MAX_CHAL_BUF_SIZE		2048
#define	MAX_IO_BUF_SIZE			21848

static	gss_cred_id_t	stCredentials;

#include <sys/param.h>

/* -----------------------------------------------------------------
	- smtpd apple auth methods
   ----------------------------------------------------------------- */

/* -----------------------------------------------------------------
	- do_login_auth
   ----------------------------------------------------------------- */

char * do_login_auth (	SMTPD_STATE *state,
						const char *sasl_method,
						const char *init_response )
{
    unsigned	len			= 0;
	int			respLen		= 0;
	char		userBuf[ MAX_USER_BUF_SIZE ];
	char		chalBuf[ MAX_CHAL_BUF_SIZE ];
	char		respBuf[ MAX_IO_BUF_SIZE ];
	char		pwdBuf[ MAX_USER_BUF_SIZE ];

	memset( userBuf, 0, MAX_USER_BUF_SIZE );
	memset( chalBuf, 0, MAX_CHAL_BUF_SIZE );
	memset( respBuf, 0, MAX_IO_BUF_SIZE );
	memset( pwdBuf, 0, MAX_USER_BUF_SIZE );

	/* is LOGIN auth enabled */
	if ( !(smtpd_pw_server_sasl_opts & PW_SERVER_LOGIN) )
	{
		return ( "504 Authentication method not enabled" );
	}

	/* encode the user name prompt and send it */
	strcpy( chalBuf, "Username:" );
	sEncodeBase64( chalBuf, strlen( chalBuf ), respBuf, MAX_IO_BUF_SIZE );
	smtpd_chat_reply( state, "334 %s", respBuf );

	/* reset the buffer */
	memset( respBuf, 0, MAX_IO_BUF_SIZE );

	/* get the user name and decode it */
	smtpd_chat_query( state );
	len = VSTRING_LEN( state->buffer );
	if ( sDecodeBase64( vstring_str( state->buffer ), len, userBuf, MAX_USER_BUF_SIZE, &respLen ) != 0 )
	{
		return ( "501 Authentication failed: malformed initial response" );
	}

	/* has the client given up */
	if ( strcmp(vstring_str( state->buffer ), "*") == 0 )
	{
		return ( "501 Authentication aborted" );
	}

	/* encode the password prompt and send it */
	strcpy( chalBuf, "Password:" );
	sEncodeBase64( chalBuf, strlen( chalBuf ), respBuf, MAX_IO_BUF_SIZE );
	smtpd_chat_reply( state, "334 %s", respBuf );

	/* get the password */
	smtpd_chat_query( state );
	len = VSTRING_LEN( state->buffer );
	if ( sDecodeBase64( vstring_str( state->buffer ), len, pwdBuf, MAX_USER_BUF_SIZE, &respLen ) != 0 )
	{
		return ( "501 Authentication failed: malformed response" );
	}

	/* do the auth */
	if ( sClearTextCrypt( userBuf, pwdBuf ) == eAODNoErr )
	{
		state->sasl_username = mystrdup( userBuf );
		state->sasl_method = mystrdup(sasl_method);

		return( 0 );
	}
	else
	{
		return ( "535 Error: authentication failed" );
	}

} /* do_login_auth */


/* -----------------------------------------------------------------
	- do_plain_auth
   ----------------------------------------------------------------- */

char *do_plain_auth (	SMTPD_STATE *state,
						const char *sasl_method,
						const char *init_response )

{
	char	   *ptr			= NULL;
    unsigned	len			= 0;
	int			respLen		= 0;
	char		userBuf[ MAX_USER_BUF_SIZE ];
	char		respBuf[ MAX_IO_BUF_SIZE ];
	char		pwdBuf[ MAX_USER_BUF_SIZE ];

	memset( userBuf, 0, MAX_USER_BUF_SIZE );
	memset( respBuf, 0, MAX_IO_BUF_SIZE );
	memset( pwdBuf, 0, MAX_USER_BUF_SIZE );

	/* is PLAIN auth enabled */
	if ( !(smtpd_pw_server_sasl_opts & PW_SERVER_PLAIN) )
	{
		return ( "504 Authentication method not enabled" );
	}

	/* decode the initial response */
	if ( init_response == NULL )
	{
		return ( "501 Authentication failed: malformed initial response" );
	}
	len = strlen( init_response );
	if ( sDecodeBase64( init_response, len, respBuf, MAX_USER_BUF_SIZE, &respLen ) != 0 )
	{
		return ( "501 Authentication failed: malformed initial response" );
	}

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
					if ( sClearTextCrypt( userBuf, pwdBuf ) == eAODNoErr )
					{
						state->sasl_username = mystrdup( userBuf );
						state->sasl_method = mystrdup(sasl_method);
	
						return( 0 );
					}
				}
			}
		}
	}

	return ( "535 Error: authentication failed" );

} /* do_plain_auth */


/* -----------------------------------------------------------------
	- get_random_chars
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


/* -----------------------------------------------------------------
	- do_cram_md5_auth
   ----------------------------------------------------------------- */

char *do_cram_md5_auth (	SMTPD_STATE *state,
							const char *sasl_method,
							const char *init_response )
{
	char	   *ptr			= NULL;
    unsigned	len			= 0;
	int			respLen		= 0;
	const char *host_name	= NULL;
	char		userBuf[ MAX_USER_BUF_SIZE ];
	char		chalBuf[ MAX_USER_BUF_SIZE ];
	char		respBuf[ MAX_IO_BUF_SIZE ];
	char		randbuf[ 17 ];

	memset( userBuf, 0, MAX_USER_BUF_SIZE );
	memset( chalBuf, 0, MAX_USER_BUF_SIZE );
	memset( respBuf, 0, MAX_IO_BUF_SIZE );

	/* is CRAM-MD5 auth enabled */
	if ( !(smtpd_pw_server_sasl_opts & PW_SERVER_CRAM_MD5) )
	{
		return ( "504 Authentication method not enabled" );
	}

	/* challenge host name */
	host_name = (const char *)get_hostname();

	/* get random data string */
	get_random_chars( randbuf, 17 );

	snprintf( chalBuf, sizeof( chalBuf ),
			"<%lu.%s.%lu@%s>",
			 (unsigned long) getpid(),
			 randbuf,
			 (unsigned long)time(0),
			 host_name );


	/* encode the challenge and send it */
	sEncodeBase64( chalBuf, strlen( chalBuf ), respBuf, MAX_IO_BUF_SIZE );

	smtpd_chat_reply( state, "334 %s", respBuf );

	/* get the client response */
	smtpd_chat_query( state );

	/* check if client cancelled */
	if ( strcmp( vstring_str( state->buffer ), "*" ) == 0 )
	{
		return ( "501 Authentication aborted" );
	}

	/* reset the buffer */
	memset( respBuf, 0, MAX_IO_BUF_SIZE );

	/* decode the response */
	len = VSTRING_LEN( state->buffer );
	if ( sDecodeBase64( vstring_str( state->buffer ), len, respBuf, MAX_IO_BUF_SIZE, &respLen ) != 0 )
	{
		return ( "501 Authentication failed: malformed initial response" );
	}

	/* get the user name */
	ptr = strchr( respBuf, ' ' );
	if ( ptr != NULL )
	{
		len = ptr - respBuf;
		if ( len < MAX_USER_BUF_SIZE )
		{
			/* copy user name */
			memset( userBuf, 0, MAX_USER_BUF_SIZE );
			strncpy( userBuf, respBuf, len );

			/* move past the space */
			ptr++;
			if ( ptr != NULL )
			{
				/* validate the response */
				if ( sValidateResponse( userBuf, chalBuf, ptr, kDSStdAuthCRAM_MD5 ) == eAODNoErr )
				{
					state->sasl_username = mystrdup( userBuf );
					state->sasl_method = mystrdup(sasl_method);

					return( 0 );
				}
			}
		}
	}

	return ( "535 Error: authentication failed" );

} /* do_cram_md5_auth */


/* -----------------------------------------------------------------
	- do_gssapi_auth
   ----------------------------------------------------------------- */

char *do_gssapi_auth (	SMTPD_STATE *state,
						const char *sasl_method,
						const char *init_response )
{
	int				r		= ODA_AUTH_FAILED;
    unsigned		len			= 0;
	int				respLen		= 0;
	gss_buffer_desc	in_token;
	gss_buffer_desc	out_token;
	OM_uint32		minStatus	= 0;
	OM_uint32		majStatus	= 0;
	OM_uint32		ret_flags	= 0;
	gss_ctx_id_t	context		= GSS_C_NO_CONTEXT;
	gss_OID			mechTypes;
	gss_name_t		clientName;
	unsigned long	maxsize		= htonl( MAX_IO_BUF_SIZE );


	char		userBuf[ MAX_USER_BUF_SIZE ];
	char		respBuf[ MAX_IO_BUF_SIZE ];
	char		pwdBuf[ MAX_USER_BUF_SIZE ];

	memset( userBuf, 0, MAX_USER_BUF_SIZE );
	memset( respBuf, 0, MAX_IO_BUF_SIZE );
	memset( pwdBuf, 0, MAX_USER_BUF_SIZE );

	/* is Kerberos V5 enabled */
	if ( !(smtpd_pw_server_sasl_opts & PW_SERVER_GSSAPI) )
	{
		return ( "504 Authentication method not enabled" );
	}

	if ( gss_Init() == GSS_S_COMPLETE )
	{
		if ( init_response == NULL )
		{
			smtpd_chat_reply( state, "334 " );
			smtpd_chat_query( state );

			/* check if client cancelled */
			if ( strcmp( vstring_str( state->buffer ), "*" ) == 0 )
			{
				return ( "501 Authentication aborted" );
			}

			/* clear response buffer */
			memset( respBuf, 0, MAX_IO_BUF_SIZE );

			/* decode the response */
			len = VSTRING_LEN( state->buffer );
			if ( sDecodeBase64( vstring_str( state->buffer ), len, respBuf, MAX_IO_BUF_SIZE, &respLen ) != 0 )
			{
				return ( "501 Authentication failed: malformed initial response" );
			}
		}
		else
		{
			/* clear response buffer */
			memset( respBuf, 0, MAX_IO_BUF_SIZE );

			len = strlen( init_response );
			if ( sDecodeBase64( init_response, len, respBuf, MAX_IO_BUF_SIZE, &respLen ) != 0 )
			{
				return ( "501 Authentication failed: malformed initial response" );
			}
		}

		in_token.value  = respBuf;
		in_token.length = respLen;

		do {
			/* negotiate authentication */
			majStatus = gss_accept_sec_context(	&minStatus,
												&context,
												stCredentials,
												&in_token,
												GSS_C_NO_CHANNEL_BINDINGS,
												&clientName,
												NULL, /* &mechTypes */
												&out_token,
												&ret_flags,
												NULL,	/* ignore time?*/
												NULL );
	
			switch ( majStatus )
			{
				case GSS_S_COMPLETE:			/* successful */
				case GSS_S_CONTINUE_NEEDED:		/* continue */
				{
					if ( out_token.value )
					{
						/* Encode the challenge and send it */
						memset( respBuf, 0, MAX_IO_BUF_SIZE );
						sEncodeBase64( (char *)out_token.value, out_token.length, respBuf, MAX_IO_BUF_SIZE );

						smtpd_chat_reply( state, "334 %s", respBuf );
						smtpd_chat_query( state );
		
						/* check if client cancelled */
						if ( strcmp( vstring_str( state->buffer ), "*" ) == 0 )
						{
							return ( "501 Authentication aborted" );
						}
	
						/* decode the response */
						memset( respBuf, 0, MAX_IO_BUF_SIZE );
						len = VSTRING_LEN( state->buffer );
						if ( len != 0 )
						{
							if ( sDecodeBase64( vstring_str( state->buffer ), len, respBuf, MAX_IO_BUF_SIZE, &respLen ) != 0 )
							{
								return ( "501 Authentication failed: malformed response" );
							}
						}
						in_token.value  = respBuf;
						in_token.length = respLen;
	
						gss_release_buffer( &minStatus, &out_token );
					}
					break;
				}
	
				default:
					log_errors( "gss_accept_sec_context", majStatus, minStatus );
					break;
			}
		} while ( in_token.value && in_token.length && (majStatus == GSS_S_CONTINUE_NEEDED) );

		if ( majStatus == GSS_S_COMPLETE )
		{
			gss_buffer_desc		inToken;
			gss_buffer_desc		outToken;

			memcpy( pwdBuf, (void *)&maxsize, 4 );
			inToken.value	= pwdBuf;
			inToken.length	= 4;

			pwdBuf[ 0 ] = 1;

			majStatus = gss_wrap( &minStatus, context, 0, GSS_C_QOP_DEFAULT, &inToken, NULL, &outToken );
			if ( majStatus == GSS_S_COMPLETE )
			{
				/* Encode the challenge and send it */
				sEncodeBase64( (char *)outToken.value, outToken.length, respBuf, MAX_IO_BUF_SIZE );

				smtpd_chat_reply( state, "334 %s", respBuf );
				smtpd_chat_query( state );

				/* check if client cancelled */
				if ( strcmp( vstring_str( state->buffer ), "*" ) == 0 )
				{
					return ( "501 Authentication aborted" );
				}

				/* Decode the response */
				memset( respBuf, 0, MAX_IO_BUF_SIZE );
				len = VSTRING_LEN( state->buffer );
				if ( sDecodeBase64( vstring_str( state->buffer ), len, respBuf, MAX_IO_BUF_SIZE, &respLen ) != 0 )
				{
					return ( "501 Authentication failed: malformed response" );
				}

				inToken.value  = respBuf;
				inToken.length = respLen;

				gss_release_buffer( &minStatus, &outToken );

				majStatus = gss_unwrap( &minStatus, context, &inToken, &outToken, NULL, NULL );
				if ( majStatus == GSS_S_COMPLETE )
				{
					if ( (outToken.value != NULL)		&&
						(outToken.length > 4)			&&
						(outToken.length < MAX_USER_BUF_SIZE) )
					{
						memcpy( userBuf, outToken.value, outToken.length );
						if ( userBuf[0] & 1 )
						{
							userBuf[ outToken.length ] = '\0';
							state->sasl_username = mystrdup( userBuf + 4 );
							state->sasl_method = mystrdup(sasl_method);

							return( 0 );
						}
					}
				}
				else
				{
					log_errors( "gss_unwrap", majStatus, minStatus );
				}

				gss_release_buffer( &minStatus, &outToken );
			}
			else
			{
				log_errors( "gss_wrap", majStatus, minStatus );
			}
		}
	}

	return ( "504 Authentication failed" );

} /* do_gssapi_auth */


/* -----------------------------------------------------------------
	- smtpd_pw_server_authenticate
   ----------------------------------------------------------------- */

char *smtpd_pw_server_authenticate (	SMTPD_STATE *state,
										const char *sasl_method,
										const char *init_response )

{
    char *myname = "smtpd_pw_server_authenticate";

	/*** Sanity check ***/
    if ( state->sasl_username || state->sasl_method )
	{
		msg_panic( "%s: already authenticated", myname );
	}

	if ( strcasecmp( sasl_method, "LOGIN" ) == 0 )
	{
		return( do_login_auth( state, sasl_method, init_response ) );
	}
	else if ( strcasecmp( sasl_method, "PLAIN" ) == 0 )
	{
		return( do_plain_auth( state, sasl_method, init_response ) );
	}
	else if ( strcasecmp( sasl_method, "CRAM-MD5" ) == 0 )
	{
		return( do_cram_md5_auth( state, sasl_method, init_response ) );
	}
	else if ( strcasecmp( sasl_method, "GSSAPI" ) == 0 )
	{
		return( do_gssapi_auth( state, sasl_method, init_response ) );
	}

	return ( "504 Unsupported authentication method" );

} /* smtpd_pw_server_authenticate */


/* -----------------------------------------------------------
 *	gss_Init ()
 * ----------------------------------------------------------- */

int gss_Init ( void )
{
	int					iResult		= GSS_S_COMPLETE;
	char			   *pService	= NULL;
	gss_buffer_desc		nameToken;
	gss_name_t			principalName;
	gss_OID				mechid;
	OM_uint32			majStatus	= 0;
	OM_uint32			minStatus	= 0;

	pService = get_server_principal();
	if ( pService == NULL )
	{
		syslog( LOG_ERR, "No service principal found" );
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
		log_errors( "gss_import_name", majStatus, minStatus );
		iResult = kSGSSImportNameErr;
	}
	else
	{
		// Send name to logs
		(void)gss_display_name( &minStatus, principalName, &nameToken, &mechid );

		(void)gss_release_buffer( &minStatus, &nameToken );

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
			log_errors( "gss_acquire_cred", majStatus, minStatus );
			iResult = kSGSSAquireCredErr;
		}
		else
		{
			gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
		}
		(void)gss_release_name( &minStatus, &principalName );
	}

	free( pService );

	return( iResult );

} /* gss_Init */


/* -----------------------------------------------------------
 *	get_realm_form_creds ()
 * ----------------------------------------------------------- */

int get_realm_form_creds ( char *inBuffer, int inSize )
{
	int			iResult		= GSS_S_COMPLETE;
	char		buffer[256] = {0};
	char	   *token1		= NULL;

	iResult = get_principal_str( buffer, 256 );
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


/* -----------------------------------------------------------
 *	get_principal_str ()
 * ----------------------------------------------------------- */

int get_principal_str ( char *inOutBuf, int inSize )
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

} /* get_principal_str */


/* -----------------------------------------------------------
 *	display_name ()
 * ----------------------------------------------------------- */

OM_uint32 display_name ( const gss_name_t principalName )
{
	OM_uint32		minStatus	= 0;
	OM_uint32		majStatus	= 0;
	gss_OID			mechid;
	gss_buffer_desc nameToken;

	majStatus = gss_display_name( &minStatus, principalName, &nameToken, &mechid );

	(void)gss_release_buffer( &minStatus, &nameToken );

	return( majStatus );

} /* display_name */


#define CHAR64(c)  (((c) < 0 || (c) > 127) ? -1 : index_64[(c)])

static char basis_64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????";

static char index_64[ 128 ] =
{
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-1,-1,-1,
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};


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
	sClearTextCrypt ()
   ----------------------------------------------------------------- */

int sClearTextCrypt ( const char *inUserID, const char *inPasswd )
{
	int					iResult			= eAODNoErr;
	tDirStatus			dsStatus		= eDSNoErr;
	tDirReference		dirRef			= 0;
	tDirNodeReference	searchNodeRef	= 0;
	tDirNodeReference	userNodeRef		= 0;
	char			   *userLoc			= NULL;

	if ( (inUserID == NULL) || (inPasswd == NULL) )
	{
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
						case eDSAuthNewPasswordRequired:
						case eDSAuthPasswordExpired:
							iResult = eAODNoErr;
							break;

						default:
							iResult = eAODAuthFailed;
							break;
					}
					(void)dsCloseDirNode( userNodeRef );
				}
				else
				{
					iResult = eAODCantOpenUserNode;
				}
			}
			else
			{
				iResult = eAODUserNotFound;
			}
			(void)dsCloseDirNode( searchNodeRef );
		}
		else
		{
			iResult = eAODOpenSearchFailed;
		}
		(void)dsCloseDirService( dirRef );
	}
	else
	{
		iResult = eAODOpenDSFailed;
	}

	return( iResult );

} /* sClearTextCrypt */

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
					(void)dsDataListDeAllocate( inDirRef, pDataList, TRUE );

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
				if ( pUserAttrType != NULL );
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
								done = TRUE;
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
					pUserAttrType = NULL;
				}
				(void)dsDataListDeallocate( inDirRef, pUserRecType );
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

static int sDoCryptAuth ( tDirReference inDirRef, tDirNodeReference inUserNodeRef, const char *inUserID, const char *inPasswd )
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

				dsStatus = dsDoDirNodeAuth( inUserNodeRef, pAuthType, TRUE, pAuthBuff, pStepBuff, NULL );
				if ( dsStatus == eDSNoErr )
				{
					iResult = 0;
				}
				else
				{
					msg_warn( "AOD: Authentication failed for user %s. (Open Directory error: %d)", inUserID, dsStatus );
					iResult = -7;
				}

				(void)dsDataNodeDeAllocate( inDirRef, pAuthType );
				pAuthType = NULL;
			}
			else
			{
				msg_warn( "AOD: Authentication failed for user %s.  Unable to allocate memory.", inUserID );
				iResult = -6;
			}
			(void)dsDataNodeDeAllocate( inDirRef, pStepBuff );
			pStepBuff = NULL;
		}
		else
		{
			msg_warn( "AOD: Authentication failed for user %s.  Unable to allocate memory.", inUserID );
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

								dsStatus = dsDoDirNodeAuth( userNodeRef, pAuthType, TRUE, pAuthBuff, pStepBuff, NULL );
								if ( dsStatus == eDSNoErr )
								{
									iResult = 0;
								}
								else
								{
									iResult = -7;
								}
								(void)dsDataNodeDeAllocate( dirRef, pAuthType );
								pAuthType = NULL;
							}
							else
							{
								msg_warn( "AOD: Authentication failed for user %s.  Unable to allocate memory.", inUserID );
								iResult = -6;
							}
							(void)dsDataNodeDeAllocate( dirRef, pStepBuff );
							pStepBuff = NULL;
						}
						else
						{
							msg_warn( "AOD: Authentication failed for user %s.  Unable to allocate memory.", inUserID );
							iResult = -5;
						}
						(void)dsDataNodeDeAllocate( dirRef, pAuthBuff );
						pAuthBuff = NULL;
					}
					else
					{
						msg_warn( "AOD: Authentication failed for user %s.  Unable to allocate memory.", inUserID );
						iResult = -4;
					}
					(void)dsCloseDirNode( userNodeRef );
					userNodeRef = 0;
				}
				else
				{
					msg_warn( "AOD: Unable to open user directory node for user %s. (Open Directory error: %d)", inUserID, dsStatus );
					iResult = -1;
				}
			}
			else
			{
				msg_warn( "AOD: Unable to find user %s. (Open Directory error: %d)", inUserID, dsStatus );
				iResult = -1;
			}
			(void)dsCloseDirNode( searchNodeRef );
			searchNodeRef = 0;
		}
		else
		{
			msg_warn( "AOD: Unable to open directroy search node. (Open Directory error: %d)", dsStatus );
			iResult = -1;
		}
		(void)dsCloseDirService( dirRef );
		dirRef = 0;
	}
	else
	{
		msg_warn( "AOD: Unable to open directroy. (Open Directory error: %d)", dsStatus );
		iResult = -1;
	}

	return( iResult );

} /* sValidateResponse */


#define	kPlistFilePath					"/etc/MailServicesOther.plist"
#define	kXMLDictionary					"postfix"
#define	kXMLSMTP_Principal				"smtp_principal"

/* -----------------------------------------------------------------
	aodGetServerPrincipal ()
   ----------------------------------------------------------------- */

char * get_server_principal ( void )
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
	CFDictionaryRef		cfDictPost	= NULL;

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
					cfDictPost = (CFDictionaryRef)CFDictionaryGetValue( cfDictRef, CFSTR( kXMLDictionary ) );
					if ( cfDictPost != NULL )
					{
						cfStringRef = (CFStringRef)CFDictionaryGetValue( cfDictPost, CFSTR( kXMLSMTP_Principal ) );
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

} /* get_server_principal */

void log_errors ( char *inName, OM_uint32 inMajor, OM_uint32 inMinor )
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

} // LogErrors
#endif /* __APPLE_OS_X_SERVER__ */

#endif
