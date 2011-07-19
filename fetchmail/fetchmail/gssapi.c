/*
 * gssapi.c -- GSSAPI authentication (see RFC 1508)
 *
 * For license terms, see the file COPYING in this directory.
 */

#include  "config.h"
#include  <stdio.h>
#include  <string.h>
#include  <ctype.h>
#if defined(STDC_HEADERS)
#include  <stdlib.h>
#endif
#include  "fetchmail.h"
#include  "socket.h"

#include  "i18n.h"
#include "fm_md5.h"

#include <sys/types.h>
#include <netinet/in.h>  /* for htonl/ntohl */

#ifdef GSSAPI
#  ifdef HAVE_GSS_H
#    include <gss.h>
#  else
#  if defined(HAVE_GSSAPI_H) && !defined(HAVE_GSSAPI_GSSAPI_H)
#    include <gssapi.h>
#  endif
#  ifdef HAVE_GSSAPI_GSSAPI_H
#    include <gssapi/gssapi.h>
#  endif
#  ifdef HAVE_GSSAPI_GSSAPI_GENERIC_H
#    include <gssapi/gssapi_generic.h>
#  endif
#  ifndef HAVE_GSS_C_NT_HOSTBASED_SERVICE
#    define GSS_C_NT_HOSTBASED_SERVICE gss_nt_service_name
#  endif
#  endif

static void decode_subr(const char *m, uint32_t code, int type)
{
    uint32_t maj, min, context;
    gss_buffer_desc msg = GSS_C_EMPTY_BUFFER;

    context = 0;
    do {
	maj = gss_display_status(&min, code, type, GSS_C_NO_OID,
		&context, &msg);

	if (maj != GSS_S_COMPLETE) {
	    report(stderr, GT_("GSSAPI error in gss_display_status called from <%s>\n"), m);
	    break;
	}
	report(stderr, GT_("GSSAPI error %s: %.*s\n"), m,
		(int)msg.length, (char *)msg.value);
	(void)gss_release_buffer(&min, &msg);
    } while(context);
}

static void decode_status(const char *m, uint32_t major, uint32_t minor)
{
    decode_subr(m, major, GSS_C_GSS_CODE);
    decode_subr(m, minor, GSS_C_MECH_CODE);
}

#define GSSAUTH_P_NONE      1
#define GSSAUTH_P_INTEGRITY 2
#define GSSAUTH_P_PRIVACY   4

static int import_name(const char *service, const char *hostname,
	gss_name_t *target_name, flag verbose)
{
    char *buf1;
    size_t buf1siz;
    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc request_buf;

    /* first things first: get an imap ticket for host */
    buf1siz = strlen(service) + 1 + strlen(hostname) + 1;
    buf1 = (char *)xmalloc(buf1siz);
    snprintf(buf1, buf1siz, "%s@%s", service, hostname);
    request_buf.value = buf1;
    request_buf.length = strlen(buf1) + 1;
    maj_stat = gss_import_name(&min_stat, &request_buf,
	    GSS_C_NT_HOSTBASED_SERVICE, target_name);
    if (maj_stat != GSS_S_COMPLETE) {
	decode_status("gss_import_name", maj_stat, min_stat);
        report(stderr, GT_("Couldn't get service name for [%s]\n"), buf1);
        return PS_AUTHFAIL;
    }
    else if (outlevel >= O_DEBUG && verbose) {
        (void)gss_display_name(&min_stat, *target_name, &request_buf, NULL);
        report(stderr, GT_("Using service name [%s]\n"),
	       (char *)request_buf.value);
    }
    (void)gss_release_buffer(&min_stat, &request_buf);

    return PS_SUCCESS;
}

/* If we don't have suitable credentials, don't bother trying GSSAPI, but
 * fail right away. This is to avoid that a server - such as Microsoft
 * Exchange 2007 - gets wedged and refuses different authentication
 * mechanisms afterwards. */
int check_gss_creds(const char *service, const char *hostname)
{
    OM_uint32 maj_stat, min_stat;
    gss_cred_usage_t cu;
    gss_name_t target_name;

    (void)import_name(service, hostname, &target_name, FALSE);
    (void)gss_release_name(&min_stat, &target_name);

    maj_stat = gss_inquire_cred(&min_stat, GSS_C_NO_CREDENTIAL,
	    NULL, NULL, &cu, NULL);
    if (maj_stat != GSS_S_COMPLETE
	    || (cu != GSS_C_INITIATE && cu != GSS_C_BOTH)) {
	if (outlevel >= O_DEBUG) {
	    decode_status("gss_inquire_cred", maj_stat, min_stat);
	    report(stderr, GT_("No suitable GSSAPI credentials found. Skipping GSSAPI authentication.\n"));
	    report(stderr, GT_("If you want to use GSSAPI, you need credentials first, possibly from kinit.\n"));
	}
	return PS_AUTHFAIL;
    }

    return PS_SUCCESS;
}

int do_gssauth(int sock, const char *command, const char *service,
		const char *hostname, const char *username)
{
    gss_buffer_desc request_buf, send_token;
    gss_buffer_t sec_token;
    gss_name_t target_name;
    gss_ctx_id_t context;
    gss_qop_t quality;
    int cflags;
    OM_uint32 maj_stat, min_stat;
    char buf1[8192], buf2[8192], server_conf_flags;
    unsigned long buf_size;
    int result;

    result = import_name(service, hostname, &target_name, TRUE);
    if (result)
	return result;

    gen_send(sock, "%s GSSAPI", command);

    /* upon receipt of the GSSAPI authentication request, server returns
     * a null data ready challenge to us. */
    result = gen_recv(sock, buf1, sizeof buf1);
    if (result)
	return result;

    if (buf1[0] != '+' || strspn(buf1 + 1, " \t") < strlen(buf1 + 1)) {
	if (outlevel >= O_VERBOSE) {
	    report(stdout, GT_("Received malformed challenge to \"%s GSSAPI\"!\n"), command);
	}
	goto cancelfail;
    }


    /* now start the security context initialisation loop... */
    sec_token = GSS_C_NO_BUFFER;
    context = GSS_C_NO_CONTEXT;
    if (outlevel >= O_VERBOSE)
        report(stdout, GT_("Sending credentials\n"));
    do {
	send_token.length = 0;
	send_token.value = NULL;
	maj_stat = gss_init_sec_context(&min_stat,
					GSS_C_NO_CREDENTIAL,
					&context,
					target_name,
					GSS_C_NO_OID,
					GSS_C_MUTUAL_FLAG | GSS_C_SEQUENCE_FLAG,
					0,
					GSS_C_NO_CHANNEL_BINDINGS,
					sec_token,
					NULL,
					&send_token,
					NULL,
					NULL);

	if (maj_stat!=GSS_S_COMPLETE && maj_stat!=GSS_S_CONTINUE_NEEDED) {
	    if (outlevel >= O_VERBOSE)
		decode_status("gss_init_sec_context", maj_stat, min_stat);
	    (void)gss_release_name(&min_stat, &target_name);

cancelfail:
	    /* wake up server and cancel authentication */
	    suppress_tags = TRUE;
	    gen_send(sock, "*");
	    suppress_tags = FALSE;

	    result = gen_recv(sock, buf1, sizeof buf1);
	    if (outlevel >= O_VERBOSE)
		report(stderr, GT_("Error exchanging credentials\n"));
	    if (result)
		return result;
	    return PS_AUTHFAIL;
	}
	to64frombits(buf1, send_token.value, send_token.length);
	gss_release_buffer(&min_stat, &send_token);

	suppress_tags = TRUE;
	gen_send(sock, "%s", buf1);
	suppress_tags = FALSE;

        if (maj_stat == GSS_S_CONTINUE_NEEDED) {
	    result = gen_recv(sock, buf1, sizeof buf1);
	    if (result) {
	        gss_release_name(&min_stat, &target_name);
	        return result;
	    }
	    request_buf.length = from64tobits(buf2, buf1 + 2, sizeof(buf2));
	    if ((int)request_buf.length == -1)	/* in case of bad data */
		request_buf.length = 0;
	    request_buf.value = buf2;
	    sec_token = &request_buf;
        }
    } while
	(maj_stat == GSS_S_CONTINUE_NEEDED);
    gss_release_name(&min_stat, &target_name);

    /* get security flags and buffer size */
    result = gen_recv(sock, buf1, sizeof buf1);
    if (result)
        return result;

    request_buf.length = from64tobits(buf2, buf1 + 2, sizeof(buf2));
    if ((int)request_buf.length == -1)	/* in case of bad data */
	request_buf.length = 0;
    request_buf.value = buf2;

    maj_stat = gss_unwrap(&min_stat, context,
			  &request_buf, &send_token, &cflags, &quality);
    if (maj_stat != GSS_S_COMPLETE) {
	decode_status("gss_unwrap", maj_stat, min_stat);
        report(stderr, GT_("Couldn't unwrap security level data\n"));
        gss_release_buffer(&min_stat, &send_token);
        return PS_AUTHFAIL;
    }
    if (outlevel >= O_DEBUG)
        report(stdout, GT_("Credential exchange complete\n"));
    /* first octet is security levels supported. We want none, for now */
    server_conf_flags = ((char *)send_token.value)[0];
    if ( !(((char *)send_token.value)[0] & GSSAUTH_P_NONE) ) {
        report(stderr, GT_("Server requires integrity and/or privacy\n"));
        gss_release_buffer(&min_stat, &send_token);
        return PS_AUTHFAIL;
    }
    ((char *)send_token.value)[0] = 0;
    buf_size = ntohl(*((long *)send_token.value));
    /* we don't care about buffer size if we don't wrap data */
    gss_release_buffer(&min_stat, &send_token);
    if (outlevel >= O_DEBUG) {
        report(stdout, GT_("Unwrapped security level flags: %s%s%s\n"),
            server_conf_flags & GSSAUTH_P_NONE ? "N" : "-",
            server_conf_flags & GSSAUTH_P_INTEGRITY ? "I" : "-",
            server_conf_flags & GSSAUTH_P_PRIVACY ? "C" : "-");
        report(stdout, GT_("Maximum GSS token size is %ld\n"),buf_size);
    }

    /* now respond in kind (hack!!!) */
    buf_size = htonl(buf_size); /* do as they do... only matters if we do enc */
    memcpy(buf1, &buf_size, 4);
    buf1[0] = GSSAUTH_P_NONE;
    strlcpy(buf1+4, username, sizeof(buf1) - 4); /* server decides if princ is user */
    request_buf.length = 4 + strlen(username) + 1;
    request_buf.value = buf1;
    maj_stat = gss_wrap(&min_stat, context, 0, GSS_C_QOP_DEFAULT, &request_buf,
        &cflags, &send_token);
    if (maj_stat != GSS_S_COMPLETE) {
        report(stderr, GT_("Error creating security level request\n"));
        return PS_AUTHFAIL;
    }
    to64frombits(buf1, send_token.value, send_token.length);

    suppress_tags = TRUE;
    result = gen_transact(sock, "%s", buf1);
    suppress_tags = FALSE;

    /* flush security context */
    if (outlevel >= O_DEBUG)
	report(stdout, GT_("Releasing GSS credentials\n"));
    maj_stat = gss_delete_sec_context(&min_stat, &context, &send_token);
    if (maj_stat != GSS_S_COMPLETE) {
	decode_status("gss_delete_sec_context", maj_stat, min_stat);
	report(stderr, GT_("Error releasing credentials\n"));
	return PS_AUTHFAIL;
    }
    /* send_token may contain a notification to the server to flush
     * credentials. RFC 1731 doesn't specify what to do, and since this
     * support is only for authentication, we'll assume the server
     * knows enough to flush its own credentials */
    gss_release_buffer(&min_stat, &send_token);

    if (result)
	return(result);
    else
	return(PS_SUCCESS);
}	
#endif /* GSSAPI */

/* gssapi.c ends here */
