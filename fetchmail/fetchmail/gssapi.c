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
#include "md5.h"

#include <sys/types.h>
#include <netinet/in.h>  /* for htonl/ntohl */

#ifdef GSSAPI
#  ifdef HAVE_GSSAPI_H
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

#define GSSAUTH_P_NONE      1
#define GSSAUTH_P_INTEGRITY 2
#define GSSAUTH_P_PRIVACY   4

int do_gssauth(int sock, char *command, char *hostname, char *username)
{
    gss_buffer_desc request_buf, send_token;
    gss_buffer_t sec_token;
    gss_name_t target_name;
    gss_ctx_id_t context;
    gss_OID mech_name;
    gss_qop_t quality;
    int cflags;
    OM_uint32 maj_stat, min_stat;
    char buf1[8192], buf2[8192], server_conf_flags;
    unsigned long buf_size;
    int result;

    /* first things first: get an imap ticket for host */
    sprintf(buf1, "imap@%s", hostname);
    request_buf.value = buf1;
    request_buf.length = strlen(buf1) + 1;
    maj_stat = gss_import_name(&min_stat, &request_buf, GSS_C_NT_HOSTBASED_SERVICE,
        &target_name);
    if (maj_stat != GSS_S_COMPLETE) {
        report(stderr, GT_("Couldn't get service name for [%s]\n"), buf1);
        return PS_AUTHFAIL;
    }
    else if (outlevel >= O_DEBUG) {
        maj_stat = gss_display_name(&min_stat, target_name, &request_buf,
            &mech_name);
        report(stderr, GT_("Using service name [%s]\n"),request_buf.value);
        maj_stat = gss_release_buffer(&min_stat, &request_buf);
    }

    gen_send(sock, "%s GSSAPI", command);

    /* upon receipt of the GSSAPI authentication request, server returns
     * null data ready response. */
    if (result = gen_recv(sock, buf1, sizeof buf1)) {
        return result;
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
            report(stderr, GT_("Error exchanging credentials\n"));
            gss_release_name(&min_stat, &target_name);
            /* wake up server and await NO response */
            SockWrite(sock, "\r\n", 2);
            if (result = gen_recv(sock, buf1, sizeof buf1))
                return result;
            return PS_AUTHFAIL;
        }
        to64frombits(buf1, send_token.value, send_token.length);
        gss_release_buffer(&min_stat, &send_token);

	suppress_tags = TRUE;
	gen_send(sock, buf1, strlen(buf1));
	suppress_tags = FALSE;

        if (maj_stat == GSS_S_CONTINUE_NEEDED) {
	    if (result = gen_recv(sock, buf1, sizeof buf1)) {
	        gss_release_name(&min_stat, &target_name);
	        return result;
	    }
	    request_buf.length = from64tobits(buf2, buf1 + 2, sizeof(buf2));
	    if (request_buf.length == -1)	/* in case of bad data */
		request_buf.length = 0;
	    request_buf.value = buf2;
	    sec_token = &request_buf;
        }
    } while
	(maj_stat == GSS_S_CONTINUE_NEEDED);
    gss_release_name(&min_stat, &target_name);

    /* get security flags and buffer size */
    if (result = gen_recv(sock, buf1, sizeof buf1))
        return result;

    request_buf.length = from64tobits(buf2, buf1 + 2, sizeof(buf2));
    if (request_buf.length == -1)	/* in case of bad data */
	request_buf.length = 0;
    request_buf.value = buf2;

    maj_stat = gss_unwrap(&min_stat, context, 
			  &request_buf, &send_token, &cflags, &quality);
    if (maj_stat != GSS_S_COMPLETE) {
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
    strcpy(buf1+4, username); /* server decides if princ is user */
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
    result = gen_transact(sock, buf1, strlen(buf1));
    suppress_tags = FALSE;

    /* flush security context */
    if (outlevel >= O_DEBUG)
	report(stdout, GT_("Releasing GSS credentials\n"));
    maj_stat = gss_delete_sec_context(&min_stat, &context, &send_token);
    if (maj_stat != GSS_S_COMPLETE) {
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
