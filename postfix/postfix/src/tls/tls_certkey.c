/*++
/* NAME
/*	tls_certkey 3
/* SUMMARY
/*	public key certificate and private key loader
/* SYNOPSIS
/*	#define TLS_INTERNAL
/*	#include <tls.h>
/*
/*	int	tls_set_ca_certificate_info(ctx, CAfile, CApath)
/*	SSL_CTX	*ctx;
/*	const char *CAfile;
/*	const char *CApath;
/*
/*	int	tls_set_my_certificate_key_info(ctx, cert_file, key_file,
/*						dcert_file, dkey_file,
/*						eccert_file, eckey_file)
/*	SSL_CTX	*ctx;
/*	const char *cert_file;
/*	const char *key_file;
/*	const char *dcert_file;
/*	const char *dkey_file;
/*	const char *eccert_file;
/*	const char *eckey_file;
/* DESCRIPTION
/*	OpenSSL supports two options to specify CA certificates:
/*	either one file CAfile that contains all CA certificates,
/*	or a directory CApath with separate files for each
/*	individual CA, with symbolic links named after the hash
/*	values of the certificates. The second option is not
/*	convenient with a chrooted process.
/*
/*	tls_set_ca_certificate_info() loads the CA certificate
/*	information for the specified TLS server or client context.
/*	The result is -1 on failure, 0 on success.
/*
/*	tls_set_my_certificate_key_info() loads the public key
/*	certificates and private keys for the specified TLS server
/*	or client context. Up to 3 pairs of key pairs (RSA, DSA and
/*	ECDSA) may be specified; each certificate and key pair must
/*	match.  The result is -1 on failure, 0 on success.
/* LICENSE
/* .ad
/* .fi
/*	This software is free. You can do with it whatever you want.
/*	The original author kindly requests that you acknowledge
/*	the use of his software.
/* AUTHOR(S)
/*	Originally written by:
/*	Lutz Jaenicke
/*	BTU Cottbus
/*	Allgemeine Elektrotechnik
/*	Universitaetsplatz 3-4
/*	D-03044 Cottbus, Germany
/*
/*	Updated by:
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>

#ifdef USE_TLS

/* Utility library. */

#include <msg.h>

/* Global library. */

#include <mail_params.h>

#ifdef __APPLE_OS_X_SERVER__
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>

typedef struct
{
        int             len;
        char    key[ FILENAME_MAX ];
        int             reserved;
} CallbackUserData;

int apple_password_callback ( char *in_buf, int in_size, int in_rwflag, void *in_user_data );

static CallbackUserData *s_user_data = NULL;
#endif

/* TLS library. */

#define TLS_INTERNAL
#include <tls.h>

#ifdef __APPLE_OS_X_SERVER__
/* -----------------------------------------------------------------
        apple_password_callback ()
   ----------------------------------------------------------------- */

int apple_password_callback ( char *in_buf, int in_size, int in_rwflag, void *in_user_data )
{
	int					status;
	char			   *buf		= NULL;
	ssize_t				len		= 0;
	int					fd[ 2 ];
	char			   *args[ 4 ];
	CallbackUserData   *cb_data	= (CallbackUserData *)in_user_data;

	if ( (cb_data == NULL) || strlen( cb_data->key ) == 0 ||
		 (cb_data->len >= FILENAME_MAX) || (cb_data->len == 0) || !in_buf )
	{
		msg_error("invalid arguments in callback" );
		return( 0 );
	}

	/* open a pipe */
	pipe( fd );

	/* fork the child */
	pid_t pid = fork();
	if ( pid == 0 )
	{
		/* child: exec certadmin tool */
		close(0);
		close(1);

		dup2( fd[1], 1 );

		/* set up the args list */
		args[ 0 ] = "/usr/sbin/certadmin";
		args[ 1 ] = "--get-private-key-passphrase";
		args[ 2 ] = cb_data->key;
		args[ 3 ] = NULL;

		/* get the passphrase */
		execv("/usr/sbin/certadmin", args);

		exit( 0 );
	}
	else if ( pid > 0 )
	{
		/* parent: wait for the child to terminate */
		wait( &status );
		if ( !NORMAL_EXIT_STATUS(status) ) {
			if ( WIFEXITED(status) )
				msg_warn("command /usr/sbin/certadmin exit status %d",
					WEXITSTATUS(status) );
			if ( WIFSIGNALED(status) )
				msg_warn("command /usr/sbin/certadmin killed by signal %d",
					WTERMSIG(status) );

			return( 0 );
		}

		/* parent: read passphrase */
		len = 0;
		buf = malloc( in_size );
		if ( buf == NULL )
		{
			msg_error( "memory allocation error" );
			return( 0 );
		}

		len = read( fd[0], buf, in_size);
		if ( len != 0 )
		{
			/* copy passphrase into buffer & strip off /n */
			strncpy( in_buf, buf, len - 1 );
			in_buf[ len - 1 ] = '\0';
		}
	}

	return( strlen(in_buf ) );

} /* apple_password_callback */

#endif


/* tls_set_ca_certificate_info - load certificate authority certificates */

int     tls_set_ca_certificate_info(SSL_CTX *ctx, const char *CAfile,
				            const char *CApath)
{
    if (*CAfile == 0)
	CAfile = 0;
    if (*CApath == 0)
	CApath = 0;
    if (CAfile || CApath) {
	if (!SSL_CTX_load_verify_locations(ctx, CAfile, CApath)) {
	    msg_info("cannot load Certificate Authority data: "
		     "disabling TLS support");
	    tls_print_errors();
	    return (-1);
	}
	if (var_tls_append_def_CA && !SSL_CTX_set_default_verify_paths(ctx)) {
	    msg_info("cannot set certificate verification paths: "
		     "disabling TLS support");
	    tls_print_errors();
	    return (-1);
	}
    }
    return (0);
}

/* set_cert_stuff - specify certificate and key information */

static int set_cert_stuff(SSL_CTX *ctx, const char *cert_type,
			          const char *cert_file,
			          const char *key_file)
{

    /*
     * We need both the private key (in key_file) and the public key
     * certificate (in cert_file). Both may specify the same file.
     * 
     * Code adapted from OpenSSL apps/s_cb.c.
     */
    ERR_clear_error();
    if (SSL_CTX_use_certificate_chain_file(ctx, cert_file) <= 0) {
	msg_warn("cannot get %s certificate from file %s: "
		 "disabling TLS support", cert_type, cert_file);
	tls_print_errors();
	return (0);
    }

#ifdef __APPLE_OS_X_SERVER__
	if ( strlen( key_file ) < FILENAME_MAX )
	{
		if ( s_user_data == NULL )
		{
			s_user_data = malloc( sizeof(CallbackUserData) );
			if ( s_user_data != NULL )
			{
				memset( s_user_data, 0, sizeof(CallbackUserData) );
			}
		}

		if ( s_user_data != NULL )
		{
			snprintf( s_user_data->key, FILENAME_MAX, "%s", key_file );
			s_user_data->len = strlen( s_user_data->key );

			SSL_CTX_set_default_passwd_cb_userdata( ctx, (void *)s_user_data );
			SSL_CTX_set_default_passwd_cb( ctx, &apple_password_callback );
		}
		else
		{
			msg_info( "Could not allocate memory for custom callback: %s", key_file );
		}
	}
	else
	{
		msg_info( "Key file path too big for custom callback: %s", key_file );
	}
#endif

    if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
	msg_warn("cannot get %s private key from file %s: "
		 "disabling TLS support", cert_type, key_file);
	tls_print_errors();
	return (0);
    }

    /*
     * Sanity check.
     */
    if (!SSL_CTX_check_private_key(ctx)) {
	msg_warn("%s private key in %s does not match public key in %s: "
		 "disabling TLS support", cert_type, key_file, cert_file);
	return (0);
    }
    return (1);
}

/* tls_set_my_certificate_key_info - load client or server certificates/keys */

int     tls_set_my_certificate_key_info(SSL_CTX *ctx,
					        const char *cert_file,
					        const char *key_file,
					        const char *dcert_file,
					        const char *dkey_file,
					        const char *eccert_file,
					        const char *eckey_file)
{

    /*
     * Lack of certificates is fine so long as we are prepared to use
     * anonymous ciphers.
     */
    if (*cert_file && !set_cert_stuff(ctx, "RSA", cert_file, key_file))
	return (-1);			/* logged */
    if (*dcert_file && !set_cert_stuff(ctx, "DSA", dcert_file, dkey_file))
	return (-1);				/* logged */
#if OPENSSL_VERSION_NUMBER >= 0x1000000fL && !defined(OPENSSL_NO_ECDH)
    if (*eccert_file && !set_cert_stuff(ctx, "ECDSA", eccert_file, eckey_file))
	return (-1);				/* logged */
#else
    if (*eccert_file)
	msg_warn("ECDSA not supported. Ignoring ECDSA certificate file \"%s\"",
		 eccert_file);
#endif
    return (0);
}

#endif
