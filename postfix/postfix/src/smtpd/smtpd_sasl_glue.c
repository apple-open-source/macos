/*++
/* NAME
/*	smtpd_sasl_glue 3
/* SUMMARY
/*	Postfix SMTP server, SASL support interface
/* SYNOPSIS
/*	#include "smtpd_sasl_glue.h"
/*
/*	void	smtpd_sasl_initialize()
/*
/*	void	smtpd_sasl_connect(state)
/*	SMTPD_STATE *state;
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
/*	connection.
/*
/*	smtpd_sasl_authenticate() implements the authentication dialog.
/*	The result is a null pointer in case of success, an SMTP reply
/*	in case of failure. smtpd_sasl_authenticate() updates the
/*	following state structure members:
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
#include <syslog.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>   // for gettimeofday()

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <namadr_list.h>
#include <name_mask.h>

/* Global library. */

#include <mail_params.h>
#include <smtp_stream.h>

/* Application-specific. */

#include "smtpd.h"
#include "smtpd_sasl_glue.h"
#include "smtpd_chat.h"

/* Apple Open Directory */

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPropertyList.h>

/* kerberos */

#include <Kerberos/Kerberos.h>
#include <Kerberos/gssapi_krb5.h>
#include <Kerberos/gssapi.h>

/* mach */

#include <mach/boolean.h>

#ifdef USE_SASL_AUTH

/*
 * Silly little macros.
 */
#define STR(s)	vstring_str(s)

 /*
  * Macros to handle API differences between SASLv1 and SASLv2. Specifics:
  * 
  * The SASL_LOG_* constants were renamed in SASLv2.
  * 
  * SASLv2's sasl_server_new takes two new parameters to specify local and
  * remote IP addresses for auth mechs that use them.
  * 
  * SASLv2's sasl_server_start and sasl_server_step no longer have the errstr
  * parameter.
  * 
  * SASLv2's sasl_decode64 function takes an extra parameter for the length of
  * the output buffer.
  * 
  * The other major change is that SASLv2 now takes more responsibility for
  * deallocating memory that it allocates internally.  Thus, some of the
  * function parameters are now 'const', to make sure we don't try to free
  * them too.  This is dealt with in the code later on.
  */

#if SASL_VERSION_MAJOR < 2
/* SASL version 1.x */
#define SASL_LOG_WARN SASL_LOG_WARNING
#define SASL_LOG_NOTE SASL_LOG_INFO
#define SASL_SERVER_NEW(srv, fqdn, rlm, lport, rport, cb, secflags, pconn) \
	sasl_server_new(srv, fqdn, rlm, cb, secflags, pconn)
#define SASL_SERVER_START(conn, mech, clin, clinlen, srvout, srvoutlen, err) \
	sasl_server_start(conn, mech, clin, clinlen, srvout, srvoutlen, err)
#define SASL_SERVER_STEP(conn, clin, clinlen, srvout, srvoutlen, err) \
	sasl_server_step(conn, clin, clinlen, srvout, srvoutlen, err)
#define SASL_DECODE64(in, inlen, out, outmaxlen, outlen) \
	sasl_decode64(in, inlen, out, outlen)
#endif

#if SASL_VERSION_MAJOR >= 2
/* SASL version > 2.x */
#define SASL_SERVER_NEW(srv, fqdn, rlm, lport, rport, cb, secflags, pconn) \
	sasl_server_new(srv, fqdn, rlm, lport, rport, cb, secflags, pconn)
#define SASL_SERVER_START(conn, mech, clin, clinlen, srvout, srvoutlen, err) \
	sasl_server_start(conn, mech, clin, clinlen, srvout, srvoutlen)
#define SASL_SERVER_STEP(conn, clin, clinlen, srvout, srvoutlen, err) \
	sasl_server_step(conn, clin, clinlen, srvout, srvoutlen)
#define SASL_DECODE64(in, inlen, out, outmaxlen, outlen) \
	sasl_decode64(in, inlen, out, outmaxlen, outlen)
#endif

/* smtpd_sasl_log - SASL logging callback */

static int smtpd_sasl_log(void *unused_context, int priority,
			          const char *message)
{
    switch (priority) {
	case SASL_LOG_ERR:
	case SASL_LOG_WARN:
	msg_warn("SASL authentication problem: %s", message);
	break;
    case SASL_LOG_NOTE:
	if (msg_verbose)
	    msg_info("SASL authentication info: %s", message);
	break;
#if SASL_VERSION_MAJOR >= 2
    case SASL_LOG_FAIL:
	msg_warn("SASL authentication failure: %s", message);
	break;
#endif
    }
    return SASL_OK;
}

 /*
  * SASL callback interface structure. These call-backs have no per-session
  * context.
  */
#define NO_CALLBACK_CONTEXT	0

static sasl_callback_t callbacks[] = {
    {SASL_CB_LOG, &smtpd_sasl_log, NO_CALLBACK_CONTEXT},
    {SASL_CB_LIST_END, 0, 0}
};

static NAME_MASK smtpd_sasl_mask[] = {
    "noplaintext", SASL_SEC_NOPLAINTEXT,
    "noactive", SASL_SEC_NOACTIVE,
    "nodictionary", SASL_SEC_NODICTIONARY,
    "noanonymous", SASL_SEC_NOANONYMOUS,
#if SASL_VERSION_MAJOR >= 2
    "mutual_auth", SASL_SEC_MUTUAL_AUTH,
#endif
    0,
};

void log_errors ( char *inName, OM_uint32 inMajor, OM_uint32 inMinor );

static int smtpd_sasl_opts;

/* Apple's Password Server */

static int smtpd_pw_server_sasl_opts;

#define PW_SERVER_NONE			0x0000
#define PW_SERVER_LOGIN			0x0001
#define PW_SERVER_PLAIN			0x0002
#define PW_SERVER_CRAM_MD5		0x0004
#define PW_SERVER_GSSAPI		0x0008

static NAME_MASK smtpd_pw_server_mask[] = {
    "none",     PW_SERVER_NONE,
    "login",    PW_SERVER_LOGIN,
    "plain",    PW_SERVER_PLAIN,
    "cram-md5", PW_SERVER_CRAM_MD5,
    "gssapi",   PW_SERVER_GSSAPI,
    0,
};

/* smtpd_sasl_initialize - per-process initialization */

void    smtpd_sasl_initialize( int use_pw_server )
{
	if ( var_smtpd_use_pw_server )
	{
		smtpd_pw_server_sasl_opts = name_mask( VAR_SMTPD_PW_SERVER_OPTS, smtpd_pw_server_mask,
									 var_smtpd_pw_server_opts );
	}
	else
	{
		/*
		* Initialize the library: load SASL plug-in routines, etc.
		*/
		if (sasl_server_init(callbacks, "smtpd") != SASL_OK)
		msg_fatal("SASL per-process initialization failed");
	
		/*
		* Configuration parameters.
		*/
		smtpd_sasl_opts = name_mask(VAR_SMTPD_SASL_OPTS, smtpd_sasl_mask,
					var_smtpd_sasl_opts);
	}
}

/* smtpd_sasl_connect - per-connection initialization */

void    smtpd_sasl_connect(SMTPD_STATE *state)
{
#if SASL_VERSION_MAJOR < 2
    unsigned sasl_mechanism_count;

#else
    int     sasl_mechanism_count;

#endif
    sasl_security_properties_t sec_props;
    char   *server_address;
    char   *client_address;

    /*
     * Initialize SASL-specific state variables. Use long-lived storage for
     * base 64 conversion results, rather than local variables, to avoid
     * memory leaks when a read or write routine returns abnormally after
     * timeout or I/O error.
     */
    state->sasl_mechanism_list = 0;
    state->sasl_username = 0;
    state->sasl_method = 0;
    state->sasl_sender = 0;
    state->sasl_conn = 0;
    state->sasl_decoded = vstring_alloc(10);
    state->sasl_encoded = vstring_alloc(10);

    state->pw_server_enabled		= var_smtpd_use_pw_server;
    state->pw_server_mechanism_list	= 0;
	state->pw_server_opts			= 0;

	if ( smtpd_pw_server_sasl_opts )
	{
		state->pw_server_mechanism_list = malloc( 64 );
		if ( state->pw_server_mechanism_list != NULL )
		{
			memset( state->pw_server_mechanism_list, 0, 64 );
			if ( smtpd_pw_server_sasl_opts & PW_SERVER_LOGIN )
			{
				strcpy( state->pw_server_mechanism_list, " LOGIN" );
			}
			if ( smtpd_pw_server_sasl_opts & PW_SERVER_PLAIN )
			{
				strcat( state->pw_server_mechanism_list, " PLAIN" );
			}
			if ( smtpd_pw_server_sasl_opts & PW_SERVER_CRAM_MD5 )
			{
				strcat( state->pw_server_mechanism_list, " CRAM-MD5" );
			}
			if ( smtpd_pw_server_sasl_opts & PW_SERVER_GSSAPI )
			{
				strcat( state->pw_server_mechanism_list, " GSSAPI" );
			}
			state->pw_server_opts = smtpd_pw_server_sasl_opts;
		}
	}

	if ( !state->pw_server_enabled )
	{
		/*
		* Set up a new server context for this connection.
		*/
#define NO_SECURITY_LAYERS	(0)
#define NO_SESSION_CALLBACKS	((sasl_callback_t *) 0)
#define NO_AUTH_REALM		((char *) 0)

#if SASL_VERSION_MAJOR >= 2 && defined(USE_SASL_IP_AUTH)

    /*
     * Get IP addresses of local and remote endpoints for SASL.
     */
#error "USE_SASL_IP_AUTH is not implemented"

#else
	
		/*
		* Don't give any IP address information to SASL.  SASLv1 doesn't use it,
		* and in SASLv2 this will disable any mechaniams that do.
		*/
		server_address = 0;
		client_address = 0;
#endif
	
		if (SASL_SERVER_NEW("smtp", var_myhostname, *var_smtpd_sasl_realm ?
				var_smtpd_sasl_realm : NO_AUTH_REALM,
				server_address, client_address,
				NO_SESSION_CALLBACKS, NO_SECURITY_LAYERS,
				&state->sasl_conn) != SASL_OK)
		msg_fatal("SASL per-connection server initialization");
	
		/*
		* Security options. Some information can be found in the sasl.h include
		* file. Disallow anonymous authentication; this is because the
		* permit_sasl_authenticated feature is restricted to authenticated
		* clients only.
		*/
		memset(&sec_props, 0, sizeof(sec_props));
		sec_props.min_ssf = 0;
		sec_props.max_ssf = 1;			/* don't allow real SASL
							* security layer */
		sec_props.security_flags = smtpd_sasl_opts;
		sec_props.maxbufsize = 0;
		sec_props.property_names = 0;
		sec_props.property_values = 0;
	
		if (sasl_setprop(state->sasl_conn, SASL_SEC_PROPS,
				&sec_props) != SASL_OK)
		msg_fatal("SASL per-connection security setup");
	
		/*
		* Get the list of authentication mechanisms.
		*/
#define UNSUPPORTED_USER	((char *) 0)
#define IGNORE_MECHANISM_LEN	((unsigned *) 0)
	
		if (sasl_listmech(state->sasl_conn, UNSUPPORTED_USER,
				"", " ", "",
				&state->sasl_mechanism_list,
				IGNORE_MECHANISM_LEN,
				&sasl_mechanism_count) != SASL_OK)
		msg_fatal("cannot lookup SASL authentication mechanisms");
		if (sasl_mechanism_count <= 0)
		msg_fatal("no SASL authentication mechanisms");
	}
}

/* smtpd_sasl_disconnect - per-connection cleanup */

void    smtpd_sasl_disconnect(SMTPD_STATE *state)
{
	if ( state->pw_server_mechanism_list )
	{
		free( state->pw_server_mechanism_list );
		state->pw_server_mechanism_list = 0;
	}

    if (state->sasl_mechanism_list) {
#if SASL_VERSION_MAJOR < 2
	/* SASL version 1 doesn't free memory that it allocates. */
	free(state->sasl_mechanism_list);
#endif
	state->sasl_mechanism_list = 0;
    }
    if (state->sasl_conn) {
	sasl_dispose(&state->sasl_conn);
	state->sasl_conn = 0;
    }
    vstring_free(state->sasl_decoded);
    vstring_free(state->sasl_encoded);
}

/* smtpd_sasl_authenticate - per-session authentication */

char   *smtpd_sasl_authenticate(SMTPD_STATE *state,
				        const char *sasl_method,
				        const char *init_response)
{
    char   *myname = "smtpd_sasl_authenticate";
    char   *dec_buffer;
    unsigned dec_length;
    unsigned enc_length;
    unsigned enc_length_out;
    unsigned reply_len;
    unsigned serveroutlen;
    int     result;

#if SASL_VERSION_MAJOR < 2
    char   *serverout = 0;

#else
    const char *serverout = 0;

#endif

#if SASL_VERSION_MAJOR < 2
    const char *errstr = 0;

#endif

#define IFELSE(e1,e2,e3) ((e1) ? (e2) : (e3))

    if (msg_verbose)
	msg_info("%s: sasl_method %s%s%s", myname, sasl_method,
		 IFELSE(init_response, ", init_response ", ""),
		 IFELSE(init_response, init_response, ""));

    /*
     * Sanity check.
     */
    if (state->sasl_username || state->sasl_method)
	msg_panic("%s: already authenticated", myname);

    /*
     * SASL authentication protocol start-up. Process any initial client
     * response that was sent along in the AUTH command.
     */
    if (init_response) {
	reply_len = strlen(init_response);
	VSTRING_SPACE(state->sasl_decoded, reply_len);
	dec_buffer = STR(state->sasl_decoded);
	if (SASL_DECODE64(init_response, reply_len,
			  dec_buffer, reply_len, &dec_length) != SASL_OK)
	    return ("501 Authentication failed: malformed initial response");
	if (msg_verbose)
	    msg_info("%s: decoded initial response %s", myname, dec_buffer);
    } else {
	dec_buffer = 0;
	dec_length = 0;
    }
    result = SASL_SERVER_START(state->sasl_conn, sasl_method, dec_buffer,
			    dec_length, &serverout, &serveroutlen, &errstr);

    /*
     * Repeat until done or until the client gives up.
     */
    while (result == SASL_CONTINUE) {

	/*
	 * Send a server challenge. Avoid storing the challenge in a local
	 * variable, because we would leak memory when smtpd_chat_reply()
	 * does not return due to timeout or I/O error. sasl_encode64()
	 * null-terminates the result if the result buffer is large enough.
	 * 
	 * Regarding the hairy expression below: output from sasl_encode64()
	 * comes in multiples of four bytes for each triple of input bytes,
	 * plus four bytes for any incomplete last triple, plus one byte for
	 * the null terminator.
	 * 
	 * XXX Replace the klunky sasl_encode64() interface by something that
	 * uses VSTRING buffers.
	 */
	if (msg_verbose)
	    msg_info("%s: uncoded challenge: %.*s",
		     myname, (int) serveroutlen, serverout);
	enc_length = ((serveroutlen + 2) / 3) * 4 + 1;
	VSTRING_SPACE(state->sasl_encoded, enc_length);
	if (sasl_encode64(serverout, serveroutlen, STR(state->sasl_encoded),
			  enc_length, &enc_length_out) != SASL_OK)
	    msg_panic("%s: sasl_encode64 botch", myname);
#if SASL_VERSION_MAJOR < 2
	/* SASL version 1 doesn't free memory that it allocates. */
	free(serverout);
#endif
	serverout = 0;
	smtpd_chat_reply(state, "334 %s", STR(state->sasl_encoded));

	/*
	 * Receive the client response. "*" means that the client gives up.
	 * XXX For now we ignore the fact that excessively long responses
	 * will be truncated. To handle such responses, we need to change
	 * smtpd_chat_query() so that it returns an error indication.
	 */
	smtpd_chat_query(state);
	if (strcmp(vstring_str(state->buffer), "*") == 0)
	    return ("501 Authentication aborted");	/* XXX */
	reply_len = VSTRING_LEN(state->buffer);
	VSTRING_SPACE(state->sasl_decoded, reply_len);
	if (SASL_DECODE64(vstring_str(state->buffer), reply_len,
			  STR(state->sasl_decoded), reply_len,
			  &dec_length) != SASL_OK)
	    return ("501 Error: malformed authentication response");
	if (msg_verbose)
	    msg_info("%s: decoded response: %.*s",
		     myname, (int) dec_length, STR(state->sasl_decoded));
	result = SASL_SERVER_STEP(state->sasl_conn, STR(state->sasl_decoded),
			    dec_length, &serverout, &serveroutlen, &errstr);
    }

    /*
     * Cleanup. What an awful interface.
     */
#if SASL_VERSION_MAJOR < 2
    if (serverout)
	free(serverout);
#endif

    /*
     * The authentication protocol was completed.
     */
    if (result != SASL_OK)
	return ("535 Error: authentication failed");

    /*
     * Authentication succeeded. Find out the login name for logging and for
     * accounting purposes. For the sake of completeness we also record the
     * authentication method that was used. XXX Do not free(serverout).
     */
#if SASL_VERSION_MAJOR >= 2
    result = sasl_getprop(state->sasl_conn, SASL_USERNAME,
			  (const void **) &serverout);
#else
    result = sasl_getprop(state->sasl_conn, SASL_USERNAME,
			  (void **) &serverout);
#endif
    if (result != SASL_OK || serverout == 0)
	msg_panic("%s: sasl_getprop SASL_USERNAME botch", myname);
    state->sasl_username = mystrdup(serverout);
    state->sasl_method = mystrdup(sasl_method);

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
	- smtpd_pw_server_authenticate
   ----------------------------------------------------------------- */

char *smtpd_pw_server_authenticate (	SMTPD_STATE *state,
											const char *sasl_method,
											const char *init_response )

{
	const char *host_name	= NULL;
	char	   *ptr			= NULL;
	int			respLen		= 0;
    char	   *myname		= "smtpd_pw_server_authenticate";
    unsigned	len			= 0;
	char		user[ MAX_USER_BUF_SIZE ];
	char		passwd[ MAX_USER_BUF_SIZE ];
	char		chal[ MAX_CHAL_BUF_SIZE ];
	char		resp[ MAX_IO_BUF_SIZE ];
	char		randbuf[ 17 ];

	/*** Sanity check ***/
    if ( state->sasl_username || state->sasl_method )
	{
		msg_panic( "%s: already authenticated", myname );
	}

	if ( strcasecmp( sasl_method, "LOGIN" ) == 0 )
	{
		/* is LOGIN auth enabled */
		if ( !(state->pw_server_opts & PW_SERVER_LOGIN) )
		{
			return ( "504 Authentication method not enabled" );
		}

		/* encode the user name prompt and send it */
		strcpy( chal, "Username:" );
		sEncodeBase64( chal, strlen( chal ), resp, MAX_IO_BUF_SIZE );
		smtpd_chat_reply( state, "334 %s", resp );

		/* reset the buffer */
		memset( resp, 0, MAX_IO_BUF_SIZE );

		/* get the user name and decode it */
		smtpd_chat_query( state );
		len = VSTRING_LEN( state->buffer );
		if ( sDecodeBase64( vstring_str( state->buffer ), len, user, MAX_USER_BUF_SIZE, &respLen ) != 0 )
		{
			return ( "501 Authentication failed: malformed initial response" );
		}

		/* has the client given up */
		if ( strcmp(vstring_str( state->buffer ), "*") == 0 )
		{
			return ( "501 Authentication aborted" );
		}

		/* encode the password prompt and send it */
		strcpy( chal, "Password:" );
		sEncodeBase64( chal, strlen( chal ), resp, MAX_IO_BUF_SIZE );
		smtpd_chat_reply( state, "334 %s", resp );

		/* get the password */
		smtpd_chat_query( state );
		len = VSTRING_LEN( state->buffer );
		if ( sDecodeBase64( vstring_str( state->buffer ), len, passwd, MAX_USER_BUF_SIZE, &respLen ) != 0 )
		{
			return ( "501 Authentication failed: malformed response" );
		}

		/* do the auth */
		if ( sClearTextCrypt( user, passwd ) == eAODNoErr )
		{
			state->sasl_username = mystrdup( user );
			state->sasl_method = mystrdup(sasl_method);

			return( 0 );
		}
		else
		{
			return ( "535 Error: authentication failed" );
		}
	}
	else if ( strcasecmp( sasl_method, "PLAIN" ) == 0 )
	{
		/* is PLAIN auth enabled */
		if ( !(state->pw_server_opts & PW_SERVER_PLAIN) )
		{
			return ( "504 Authentication method not enabled" );
		}

		/* decode the initial response */
		if ( init_response == NULL )
		{
			return ( "501 Authentication failed: malformed initial response" );
		}
		len = strlen( init_response );
		if ( sDecodeBase64( init_response, len, resp, MAX_USER_BUF_SIZE, &respLen ) != 0 )
		{
			return ( "501 Authentication failed: malformed initial response" );
		}

		ptr = resp;
		if ( *ptr == NULL )
		{
			ptr++;
		}

		if ( ptr != NULL )
		{
			if ( strlen( ptr ) < MAX_USER_BUF_SIZE )
			{
				strcpy( user, ptr );
		
				ptr = ptr + (strlen( user ) + 1 );
		
				if ( ptr != NULL )
				{
					if ( strlen( ptr ) < MAX_USER_BUF_SIZE )
					{
						strcpy( passwd, ptr );
		
						/* do the auth */
						if ( sClearTextCrypt( user, passwd ) == eAODNoErr )
						{
							state->sasl_username = mystrdup( user );
							state->sasl_method = mystrdup(sasl_method);
		
							return( 0 );
						}
					}
				}
			}
		}

		return ( "535 Error: authentication failed" );
	}
	else if ( strcasecmp( sasl_method, "CRAM-MD5" ) == 0 )
	{
		/* is CRAM-MD5 auth enabled */
		if ( !(state->pw_server_opts & PW_SERVER_CRAM_MD5) )
		{
			return ( "504 Authentication method not enabled" );
		}

		/* create the challenge */
		host_name = (const char *)get_hostname();

		/* get random data string */
		get_random_chars( randbuf, 17 );

		sprintf( chal,"<%lu.%s.%lu@%s>",
				 (unsigned long) getpid(),
				 randbuf,
				 (unsigned long)time(0),
				 host_name );

		/* encode the challenge and send it */
		sEncodeBase64( chal, strlen( chal ), resp, MAX_IO_BUF_SIZE );

		smtpd_chat_reply( state, "334 %s", resp );

		/* get the client response */
		smtpd_chat_query( state );

		/* check if client cancelled */
		if ( strcmp( vstring_str( state->buffer ), "*" ) == 0 )
		{
			return ( "501 Authentication aborted" );
		}

		/* reset the buffer */
		memset( resp, 0, MAX_IO_BUF_SIZE );

		/* decode the response */
		len = VSTRING_LEN( state->buffer );
		if ( sDecodeBase64( vstring_str( state->buffer ), len, resp, MAX_IO_BUF_SIZE, &respLen ) != 0 )
		{
			return ( "501 Authentication failed: malformed initial response" );
		}

		/* get the user name */
		ptr = strchr( resp, ' ' );
		if ( ptr != NULL )
		{
			len = ptr - resp;
			if ( len < MAX_USER_BUF_SIZE )
			{
				/* copy user name */
				memset( user, 0, MAX_USER_BUF_SIZE );
				strncpy( user, resp, len );

				/* move past the space */
				ptr++;
				if ( ptr != NULL )
				{
					/* validate the response */
					if ( sValidateResponse( user, chal, ptr, kDSStdAuthCRAM_MD5 ) == eAODNoErr )
					{
						state->sasl_username = mystrdup( user );
						state->sasl_method = mystrdup(sasl_method);
	
						return( 0 );
					}
				}
			}
		}

		return ( "535 Error: authentication failed" );
	}
	else if ( strcasecmp( sasl_method, "GSSAPI" ) == 0 )
	{
		int				r		= ODA_AUTH_FAILED;
		char			principalBuf[ 256 ];
		gss_buffer_desc	in_token;
		gss_buffer_desc	out_token;
		OM_uint32		minStatus	= 0;
		OM_uint32		majStatus	= 0;
		OM_uint32		ret_flags	= 0;
		gss_ctx_id_t	context		= GSS_C_NO_CONTEXT;
		gss_OID			mechTypes;
		gss_name_t		clientName;
		unsigned long	maxsize		= htonl( MAX_IO_BUF_SIZE );

		/* is Kerberos V5 enabled */
		if ( !(state->pw_server_opts & PW_SERVER_GSSAPI) )
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
				memset( resp, 0, MAX_IO_BUF_SIZE );
	
				/* decode the response */
				len = VSTRING_LEN( state->buffer );
				if ( sDecodeBase64( vstring_str( state->buffer ), len, resp, MAX_IO_BUF_SIZE, &respLen ) != 0 )
				{
					return ( "501 Authentication failed: malformed initial response" );
				}
			}
			else
			{
				/* clear response buffer */
				memset( resp, 0, MAX_IO_BUF_SIZE );

				len = strlen( init_response );
				if ( sDecodeBase64( init_response, len, resp, MAX_IO_BUF_SIZE, &respLen ) != 0 )
				{
					return ( "501 Authentication failed: malformed initial response" );
				}
			}

			in_token.value  = resp;
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
							memset( resp, 0, MAX_IO_BUF_SIZE );
							sEncodeBase64( (char *)out_token.value, out_token.length, resp, MAX_IO_BUF_SIZE );

							smtpd_chat_reply( state, "334 %s", resp );
							smtpd_chat_query( state );
			
							/* check if client cancelled */
							if ( strcmp( vstring_str( state->buffer ), "*" ) == 0 )
							{
								return ( "501 Authentication aborted" );
							}
		
							/* decode the response */
							memset( resp, 0, MAX_IO_BUF_SIZE );
							len = VSTRING_LEN( state->buffer );
							if ( len != 0 )
							{
								if ( sDecodeBase64( vstring_str( state->buffer ), len, resp, MAX_IO_BUF_SIZE, &respLen ) != 0 )
								{
									return ( "501 Authentication failed: malformed response" );
								}
							}
							in_token.value  = resp;
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

				memcpy( passwd, (void *)&maxsize, 4 );
				inToken.value	= passwd;
				inToken.length	= 4;

				passwd[ 0 ] = 1;
	
				majStatus = gss_wrap( &minStatus, context, 0, GSS_C_QOP_DEFAULT, &inToken, NULL, &outToken );
				if ( majStatus == GSS_S_COMPLETE )
				{
					/* Encode the challenge and send it */
					sEncodeBase64( (char *)outToken.value, outToken.length, resp, MAX_IO_BUF_SIZE );

					smtpd_chat_reply( state, "334 %s", resp );
					smtpd_chat_query( state );
	
					/* check if client cancelled */
					if ( strcmp( vstring_str( state->buffer ), "*" ) == 0 )
					{
						return ( "501 Authentication aborted" );
					}

					/* Decode the response */
					memset( resp, 0, MAX_IO_BUF_SIZE );
					len = VSTRING_LEN( state->buffer );
					if ( sDecodeBase64( vstring_str( state->buffer ), len, resp, MAX_IO_BUF_SIZE, &respLen ) != 0 )
					{
						return ( "501 Authentication failed: malformed response" );
					}

					inToken.value  = resp;
					inToken.length = respLen;
	
					gss_release_buffer( &minStatus, &outToken );
	
					majStatus = gss_unwrap( &minStatus, context, &inToken, &outToken, NULL, NULL );
					if ( majStatus == GSS_S_COMPLETE )
					{
						if ( (outToken.value != NULL)		&&
							(outToken.length > 4)			&&
							(outToken.length < MAX_USER_BUF_SIZE) )
						{
							memcpy( user, outToken.value, outToken.length );
							if ( user[0] & 1 )
							{
								user[ outToken.length ] = '\0';
								state->sasl_username = mystrdup( user + 4 );
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
						(void)dsReleaseContinueData( inDirRef, pContext );
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
					msg_warn( "AOD: Authentication failed for user %s. (Open Directroy error: %d)", inUserID, dsStatus );
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
					msg_warn( "AOD: Unable to open user directory node for user %s. (Open Directroy error: %d)", inUserID, dsStatus );
					iResult = -1;
				}
			}
			else
			{
				msg_warn( "AOD: Unable to find user %s. (Open Directroy error: %d)", inUserID, dsStatus );
				iResult = -1;
			}
			(void)dsCloseDirNode( searchNodeRef );
			searchNodeRef = 0;
		}
		else
		{
			msg_warn( "AOD: Unable to open directroy search node. (Open Directroy error: %d)", dsStatus );
			iResult = -1;
		}
		(void)dsCloseDirService( dirRef );
		dirRef = 0;
	}
	else
	{
		msg_warn( "AOD: Unable to open directroy. (Open Directroy error: %d)", dsStatus );
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

#endif
