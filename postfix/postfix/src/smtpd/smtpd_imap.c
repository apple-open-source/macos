/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without  
 * modification, are permitted provided that the following conditions  
 * are met:
 * 
 * 1.  Redistributions of source code must retain the above copyright  
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above  
 * copyright notice, this list of conditions and the following  
 * disclaimer in the documentation and/or other materials provided  
 * with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its  
 * contributors may be used to endorse or promote products derived  
 * from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND  
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,  
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS  
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,  
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT  
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF  
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND  
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF  
 * SUCH DAMAGE.
 */

/* Implements RFC 4468 - Submission BURL */

#if defined(USE_SASL_AUTH) && defined(USE_TLS)

#include <sys_defs.h>
#include <msg.h>
#include <mymalloc.h>
#include <vstring.h>
#include <vstream.h>
#include <vstring_vstream.h>
#include <mail_params.h>
#include <iostuff.h>
#include <imap-url.h>
#include <smtpd.h>
#include <smtpd_chat.h>
#include <smtpd_sasl_glue.h>
#include <smtp_stream.h>
#include <base64_code.h>
#include <connect.h>
#include <tls.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

char *var_imap_submit_cred_file;

struct imap_server {
    struct imap_server *next;
    char *hostport;
    char *username;
    char *password;
};
static struct imap_server *imap_servers = NULL;

static bool imap_validate(const struct imap_url_parts *parts,
			  const char **error)
{
	// user: mandatory; RFC 3501 "userid"
	if (parts->user == NULL ||
	    !imap_url_astring_validate(parts->user)) {
		*error = "missing or invalid user ID";
		return FALSE;
	}

	// auth_type: optional; RFC 3501 "auth-type"
	if (parts->auth_type != NULL &&
	    !imap_url_atom_validate(parts->auth_type)) {
		*error = "invalid auth type";
		return FALSE;
	}
		    
	// hostport: mandatory; RFC 1738 "hostport"
	if (parts->hostport == NULL ||
	    !imap_url_hostport_validate(parts->hostport)) {
		*error = "missing or invalid server";
		return FALSE;
	}

	// mailbox: mandatory; RFC 3501 "mailbox"
	if (parts->mailbox == NULL ||
	    !imap_url_mailbox_validate(parts->mailbox)) {
		*error = "missing or invalid mailbox";
		return FALSE;
	}

	// uidvalidity: optional; RFC 3501 "nz-number"
	if (parts->uidvalidity != NULL &&
	    !imap_url_nz_number_validate(parts->uidvalidity)) {
		*error = "invalid uidvalidity";
		return FALSE;
	}

	// uid: mandatory; RFC 3501 "nz-number"
	if (parts->uid == NULL ||
	    !imap_url_nz_number_validate(parts->uid)) {
		*error = "missing or invalid uid";
		return FALSE;
	}

	// section: optional; RFC 2192 "section"
	if (parts->section != NULL &&
	    !imap_url_section_validate(parts->section)) {
		*error = "invalid section";
		return FALSE;
	}

	// expiration: optional; RFC 3339 "date-time"
	if (parts->expiration != NULL &&
	    !imap_url_datetime_validate(parts->expiration)) {
		*error = "invalid expiration";
		return FALSE;
	}

	// access: mandatory; RFC 4467 "access"
	if (parts->access == NULL ||
	    !imap_url_access_validate(parts->access)) {
		*error = "missing or invalid access ID";
		return FALSE;
	}

	// mechanism: mandatory; RFC 4467 "mechanism"
	if (parts->mechanism == NULL ||
	    !imap_url_mechanism_validate(parts->mechanism)) {
		*error = "missing or invalid mechanism";
		return FALSE;
	}

	// urlauth: mandatory; RFC 4467 "urlauth"
	if (parts->urlauth == NULL ||
	    !imap_url_urlauth_validate(parts->urlauth)) {
		*error = "missing or invalid access token";
		return FALSE;
	}

	return TRUE;
}

static const struct imap_server *imap_check_policy(SMTPD_STATE *state,
	const struct imap_url_parts *parts)
{
    const struct imap_server *is;

    if (strncasecmp(parts->access, "user+", 5) == 0) {
	smtpd_chat_reply(state, "554 5.7.0 Invalid URL: unsupported access method");
	return NULL;
    }

    if (strcmp(parts->user, state->sasl_username) != 0 ||
	(strncasecmp(parts->access, "submit+", 7) == 0 &&
	 strcmp(&parts->access[7], state->sasl_username) != 0)) {
	smtpd_chat_reply(state, "554 5.7.0 Invalid URL: user mismatch");
	return NULL;
    }

    for (is = imap_servers; is != NULL; is = is->next)
	if (strcasecmp(parts->hostport, is->hostport) == 0)
	    return is;

    smtpd_chat_reply(state, "554 5.7.14 No trust relationship with IMAP server");
    return NULL;
}

void imap_read_config(void)
{
    VSTREAM *fp;
    struct stat stbuf;
    VSTRING *line;
    struct imap_server *list;
    int lineno;

    if (*var_imap_submit_cred_file == 0)
	return;

    fp = vstream_fopen(var_imap_submit_cred_file, O_RDONLY, 0600);
    if (fp == NULL)
	msg_fatal("open %s: %m", var_imap_submit_cred_file);

    if (fstat(vstream_fileno(fp), &stbuf) < 0)
	msg_fatal("fstat %s: %m", var_imap_submit_cred_file);

    if (stbuf.st_uid != 0 || stbuf.st_gid != 0 ||
	(stbuf.st_mode & (S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) != 0)
	msg_fatal("unsafe ownership or permissions on %s: "
		  "uid/gid/mode are %d/%d/%0o should be 0/0/0600",
		  var_imap_submit_cred_file, stbuf.st_uid, stbuf.st_gid,
		  stbuf.st_mode & ~S_IFMT);

    line = vstring_alloc(100);
    list = NULL;
    lineno = 0;
    while (vstring_get_nonl(line, fp) != VSTREAM_EOF) {
	const char *str = vstring_str(line);
	const char *username, *password;
	struct imap_server *is;
	const char *invalid;

	++lineno;

	if (*str == '#')
	    continue;

	/* future-proofing */
	if (strcmp(str, "submitcred version 1") == 0)
	    continue;

	/* hostport|username|password, all nonempty */
	username = strchr(str, '|');
	if (username == NULL || username == str || *++username == 0) {
	    msg_warn("syntax error on line %d of %s", lineno,
		     var_imap_submit_cred_file);
	    continue;
	}
	password = strchr(username, '|');
	if (password == NULL || password == username || *++password == 0) {
	    msg_warn("syntax error on line %d of %s", lineno,
		     var_imap_submit_cred_file);
	    continue;
	}

	is = (struct imap_server *) mymalloc(sizeof *is);
	is->hostport = mystrndup(str, username - str - 1);
	is->username = mystrndup(username, password - username - 1);
	is->password = mystrdup(password);

	invalid = NULL;
	if (!imap_url_hostport_validate(is->hostport))
	    invalid = "hostport";
	else if (!imap_url_astring_validate(is->username))
	    invalid = "username";
	else if (!imap_url_astring_validate(is->password))
	    invalid = "password";
	if (invalid != NULL) {
	    msg_warn("invalid %s on line %d of %s", invalid, lineno,
		     var_imap_submit_cred_file);
	    myfree((char *) is);
	    continue;
	}

	is->next = list;
	list = is;
    }

    vstring_free(line);
    vstream_fclose(fp);

    if (list == NULL) {
	msg_warn("no valid hostport|username|password entries in %s%s",
		 var_imap_submit_cred_file, imap_servers != NULL ?
		 "; keeping old list" : "");
	return;
    }

    /* free old list */
    while (imap_servers != NULL) {
	struct imap_server *next = imap_servers->next;
	myfree(imap_servers->hostport);
	myfree(imap_servers->username);
	myfree(imap_servers->password);
	myfree((char *) imap_servers);
	imap_servers = next;
    }

    /* reverse list to preserve order of entries in cred file */
    while (list != NULL) {
	struct imap_server *next = list->next;
	list->next = imap_servers;
	imap_servers = list;
	list = next;
    }
}

bool imap_allowed(SMTPD_STATE *state)
{
    return smtpd_sasl_is_active(state) && imap_servers != NULL &&
	(strcasecmp(state->service, "submission") == 0 ||
	 atoi(state->service) == 587);
}

static TLS_APPL_STATE *tls_ctx = NULL;
static TLS_SESS_STATE *imap_starttls(VSTREAM *stream,
				     const struct imap_server *is)
{
    TLS_CLIENT_START_PROPS start_props;
    TLS_SESS_STATE *sess_ctx;

    /* XXX all these hard-coded values should be configurable */

    if (tls_ctx == NULL) {
	TLS_CLIENT_INIT_PROPS init_props;

	tls_ctx = TLS_CLIENT_INIT(&init_props,
				  log_param = VAR_SMTPD_TLS_LOGLEVEL,
				  log_level = var_smtpd_tls_loglevel,
				  verifydepth = DEF_SMTP_TLS_SCERT_VD,
						/* XXX TLS_MGR_SCACHE_IMAP? */
				  cache_type = TLS_MGR_SCACHE_SMTPD,
				  cert_file = "",
				  key_file = "",
				  dcert_file = "",
				  dkey_file = "",
				  eccert_file = "",
				  eckey_file = "",
				  CAfile = "",
				  CApath = "",
				  fpt_dgst = DEF_SMTP_TLS_FPT_DGST);
	if (tls_ctx == NULL) {
	    msg_fatal("unable to initialize client TLS");
	    return NULL;
	}
    }

    sess_ctx = TLS_CLIENT_START(&start_props,
				ctx = tls_ctx,
				stream = stream,
				timeout = 30,
				tls_level = TLS_LEV_ENCRYPT,
				nexthop = "",
				host = is->hostport,
				namaddr = is->hostport,
				serverid = is->hostport,
				protocols = DEF_SMTP_TLS_MAND_PROTO,
				cipher_grade = DEF_SMTP_TLS_MAND_CIPH,
				cipher_exclusions = "SSLv2, aNULL, ADH, eNULL",
				matchargv = NULL,
				fpt_dgst = DEF_SMTP_TLS_FPT_DGST);
    if (sess_ctx == NULL)
	    msg_warn("unable to start client TLS for IMAP server %s",
		     is->hostport);
    return sess_ctx;
}

static bool imap_capable_of(VSTREAM *stream, const struct imap_server *is,
			    VSTRING *request, VSTRING *response,
			    const char *action, bool verbose)
{
    const char *cp, *capabilities;

    /* get capabilities if not already present in response */
    cp = strcasestr(vstring_str(response), "[CAPABILITY ");
    if (cp != NULL) {
	const char *eb;

	cp += 12;
	eb = strchr(cp, ']');
	if (eb)
	    capabilities = mystrndup(cp, eb - cp);
	else
	    capabilities = mystrdup(cp);
    } else {
	VSTRING_RESET(request);
	vstring_sprintf(request, "C CAPABILITY");
	smtp_fputs(vstring_str(request), VSTRING_LEN(request), stream);

	while (smtp_get(response, stream, 0, SMTP_GET_FLAG_NONE) == '\n') {
	    if (VSTRING_LEN(response) >= 13 &&
		strncasecmp(vstring_str(response), "* CAPABILITY ", 13) == 0)
		capabilities = mystrdup(vstring_str(response) + 13);
	    if (VSTRING_LEN(response) >= 2 &&
		strncasecmp(vstring_str(response), "C ", 2) == 0)
		break;
	}
	if (VSTRING_LEN(response) < 4 ||
	    strncasecmp(vstring_str(response), "C OK", 4) != 0) {
	    msg_warn("querying capabilities of IMAP server %s failed.  "
		     "request=\"%s\" response=\"%s\"",
		     is->hostport, vstring_str(request), vstring_str(response));
	    return FALSE;
	}
    }
    if (capabilities == NULL) {
	msg_warn("cannot determine capabilities of IMAP server %s",
		 is->hostport);
	return FALSE;
    }
    if (strcasestr(capabilities, action) == 0) {
	if (verbose)
	    msg_warn("IMAP server %s does not support %s.  "
		     "detected capabilities \"%s\"",
		     is->hostport, action, capabilities);
	myfree((char *) capabilities);
	return FALSE;
    }
    myfree((char *) capabilities);

    return TRUE;
}

VSTREAM *imap_open(SMTPD_STATE *state, const char *url)
{
    int port;
    VSTREAM *stream;
    struct imap_url_parts enc_parts, dec_parts;
    const char *error, *cp;
    const struct imap_server *is;
    int jv;
    TLS_SESS_STATE *sess_ctx;
    VSTRING *request, *response;
    unsigned int length;
    bool plain;

    port = 143;
    stream = NULL;
    memset(&enc_parts, 0, sizeof enc_parts);
    memset(&dec_parts, 0, sizeof dec_parts);
    error = NULL;

    /* first parse the url */
    imap_url_parse(url, &enc_parts);
    if (imap_url_decode(&enc_parts, &dec_parts, &error) &&
	imap_validate(&dec_parts, &error)) {
	is = imap_check_policy(state, &dec_parts);
	if (is != NULL) {
	    int fd;
	    char *hostport;

	    if ((cp = strchr(is->hostport, ':')) != NULL) {
		hostport = is->hostport;
		if (strcasecmp(cp + 1, "imaps") == 0)
		    port = 993;
		else
		    port = atoi(cp + 1);
	    } else {
		VSTRING *str = vstring_alloc(strlen(is->hostport) + 6);
		vstring_sprintf(str, "%s:imap", is->hostport);
		hostport = mystrdup(vstring_str(str));
		vstring_free(str);
		port = 143;
	    }

	    fd = inet_connect(hostport, BLOCKING, 30);
	    if (fd >= 0) {
		stream = vstream_fdopen(fd, O_RDWR);
		vstream_control(stream, VSTREAM_CTL_PATH, hostport,
				VSTREAM_CTL_END);
		smtp_timeout_setup(stream, 30);
	    } else {
		msg_warn("imap_open: connect to %s: %m", hostport);
		smtpd_chat_reply(state, "451 4.4.1 IMAP server unavailable");
	    }

	    if (hostport != is->hostport)
		myfree(hostport);
	}
    } else {
	if (error)
	    smtpd_chat_reply(state, "554 5.7.0 Invalid URL: %s", error);
	else
	    smtpd_chat_reply(state, "554 5.7.0 Invalid URL");
    }

    imap_url_parts_free(&dec_parts);
    imap_url_parts_free(&enc_parts);

    if (stream == NULL)
	return NULL;

    sess_ctx = NULL;
    request = vstring_alloc(128);
    response = vstring_alloc(128);

    jv = vstream_setjmp(stream);
    if (jv != 0) {
	if (jv == -2)
	    smtpd_chat_reply(state, "554 5.6.6 IMAP URL resolution failed");
	else
	    smtpd_chat_reply(state, "451 4.4.1 IMAP server unavailable");
	if (sess_ctx != NULL)
	    tls_client_stop(tls_ctx, stream, 5, TRUE, sess_ctx);
	vstream_fclose(stream);
	vstring_free(request);
	vstring_free(response);
	return NULL;
    }

    /* negotiate SSL now if applicable (IMAPS) */
    if (port == 993) {
	sess_ctx = imap_starttls(stream, is);
	if (sess_ctx == NULL) {
	    vstream_fpurge(stream, VSTREAM_PURGE_BOTH);
	    vstream_longjmp(stream, -1);
	}
    }

    /* read server greeting */
    if (smtp_get(response, stream, 0, SMTP_GET_FLAG_NONE) != '\n' || VSTRING_LEN(response) < 4 ||
	strncasecmp(vstring_str(response), "* OK", 4) != 0) {
	msg_warn("bad greeting from IMAP server %s: %s", is->hostport,
		 vstring_str(response));
	vstream_longjmp(stream, -1);
    }

    /* send STARTTLS if applicable (IMAP) */
    if (sess_ctx == NULL) {
	/* make sure the server supports STARTTLS */
	if (!imap_capable_of(stream, is, request, response, "STARTTLS", TRUE))
	    vstream_longjmp(stream, -1);

	VSTRING_RESET(request);
	vstring_sprintf(request, "S STARTTLS");
	smtp_fputs(vstring_str(request), VSTRING_LEN(request), stream);

	while (smtp_get(response, stream, 0, SMTP_GET_FLAG_NONE) == '\n') {
	    if (VSTRING_LEN(response) >= 2 &&
		strncasecmp(vstring_str(response), "S ", 2) == 0)
		break;
	}
	if (VSTRING_LEN(response) < 4 ||
	    strncasecmp(vstring_str(response), "S OK", 4) != 0) {
	    msg_warn("starttls to IMAP server %s failed.  "
		     "request=\"%s\" response=\"%s\"",
		     is->hostport, vstring_str(request), vstring_str(response));
	    vstream_fpurge(stream, VSTREAM_PURGE_BOTH);
	    vstream_longjmp(stream, -1);
	}

	sess_ctx = imap_starttls(stream, is);
	if (sess_ctx == NULL) {
	    vstream_fpurge(stream, VSTREAM_PURGE_BOTH);
	    vstream_longjmp(stream, -1);
	}

	/* can't use old capabilities, must request */
	VSTRING_RESET(response);
	VSTRING_TERMINATE(response);
    }

    /* determine which authentication mechanism to use; prefer PLAIN */
    plain = FALSE;
    if (imap_capable_of(stream, is, request, response, "AUTH=PLAIN", FALSE))
	plain = TRUE;
    else if (!imap_capable_of(stream, is, request, response,
			      "AUTH=X-PLAIN-SUBMIT", FALSE)) {
	msg_warn("IMAP server %s supports neither "
		 "AUTH=PLAIN nor AUTH=X-PLAIN-SUBMIT.  can't log in.",
		 is->hostport);
	vstream_longjmp(stream, -1);
    }

    /* log in as the submit user */
    VSTRING_RESET(request);
    vstring_sprintf(request, "A AUTHENTICATE %s",
		    plain ? "PLAIN" : "X-PLAIN-SUBMIT");
    smtp_fputs(vstring_str(request), VSTRING_LEN(request), stream);

    while (smtp_get(response, stream, 0, SMTP_GET_FLAG_NONE) == '\n') {
	if (VSTRING_LEN(response) >= 1 &&
	    strncmp(vstring_str(response), "+", 1) == 0)
	    break;
	if (VSTRING_LEN(response) >= 2 &&
	    strncasecmp(vstring_str(response), "A ", 2) == 0)
	    break;
    }
    if (VSTRING_LEN(response) < 1 ||
	strncmp(vstring_str(response), "+", 1) != 0) {
	msg_warn("logging in to IMAP server %s failed.  "
		 "request=\"%s\" response=\"%s\"",
		 is->hostport, vstring_str(request), vstring_str(response));
	vstream_longjmp(stream, -1);
    }

    /* authorization ID \0 authentication ID \0 password */
    VSTRING_RESET(response);
    vstring_strcat(response, state->sasl_username);
    VSTRING_ADDCH(response, 0);
    vstring_strcat(response, is->username);
    VSTRING_ADDCH(response, 0);
    vstring_strcat(response, is->password);

    VSTRING_RESET(request);
    base64_encode(request, vstring_str(response), VSTRING_LEN(response));
    smtp_fputs(vstring_str(request), VSTRING_LEN(request), stream);

    while (smtp_get(response, stream, 0, SMTP_GET_FLAG_NONE) == '\n') {
	if (VSTRING_LEN(response) >= 2 &&
	    strncasecmp(vstring_str(response), "A ", 2) == 0)
	    break;
    }
    if (VSTRING_LEN(response) < 4 ||
	strncasecmp(vstring_str(response), "A OK", 4) != 0) {
	msg_warn("logging in to IMAP server %s failed.  "
		 "mechanism=%s response=\"%s\"",
		 is->hostport, plain ? "PLAIN" : "X-PLAIN-SUBMIT",
		 vstring_str(response));
	vstream_longjmp(stream, -1);
    }

    /* make sure the server supports URLAUTH */
    if (!imap_capable_of(stream, is, request, response, "URLAUTH", TRUE))
	vstream_longjmp(stream, -1);

    /* finally, begin the fetch */
    VSTRING_RESET(request);
    vstring_sprintf(request, "U URLFETCH \"%s\"", url);
    smtp_fputs(vstring_str(request), VSTRING_LEN(request), stream);

    while (smtp_get(response, stream, 0, SMTP_GET_FLAG_NONE) == '\n') {
	if (VSTRING_LEN(response) >= 11 &&
	    strncasecmp(vstring_str(response), "* URLFETCH ", 11) == 0)
	    break;
	if (VSTRING_LEN(response) >= 2 &&
	    strncasecmp(vstring_str(response), "U ", 2) == 0)
	    break;
    }
    if (VSTRING_LEN(response) < 11 ||
	strncasecmp(vstring_str(response), "* URLFETCH ", 11) != 0) {
	msg_warn("URLFETCH from IMAP server %s returned no data.  "
		 "request=\"%s\" response=\"%s\"",
		 is->hostport, vstring_str(request), vstring_str(response));
	vstream_longjmp(stream, -1);
    }

    cp = strchr(vstring_str(response) + 11, ' ');
    if (cp == NULL || cp[1] != '{' || cp[2] < '0' || cp[2] > '9') {
	msg_warn("URLFETCH from IMAP server %s returned no data.  "
		 "request=\"%s\" response=\"%s\"",
		 is->hostport, vstring_str(request), vstring_str(response));
	vstream_longjmp(stream, -2);
    }
    length = strtoul(cp + 2, NULL, 10);

    vstream_control(stream, VSTREAM_CTL_CONTEXT, sess_ctx, VSTREAM_CTL_END);

    /* read only the literal, no more */
    vstream_limit_init(stream, length);

    vstring_free(request);
    vstring_free(response);

    return stream;
}

bool imap_isdone(VSTREAM *stream)
{
    return vstream_limit_reached(stream);
}

void imap_close(VSTREAM *stream)
{
    vstream_limit_deinit(stream);
    vstream_fputs("Z LOGOUT", stream);
    vstream_fflush(stream);
    tls_client_stop(tls_ctx, stream, 5, FALSE, vstream_context(stream));
    vstream_fclose(stream);
}

#endif
