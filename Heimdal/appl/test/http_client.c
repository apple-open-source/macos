/*
 * Copyright (c) 2003 - 2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "test_locl.h"
#include <gssapi.h>
#include <gssapi_krb5.h>
#include <gssapi_spnego.h>
#include <gssapi_ntlm.h>
#include "gss_common.h"
#include <base64.h>

static void
storage_printf(krb5_storage *sp, const char *fmt, ...)
    __attribute__((format (printf, 2, 3)));

/*
 * A simplistic client implementing draft-brezak-spnego-http-04.txt
 */

static int
do_connect (const char *hostname, const char *port)
{
    struct addrinfo *ai, *a;
    struct addrinfo hints;
    int error;
    int s = -1;

    memset (&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;

    error = getaddrinfo (hostname, port, &hints, &ai);
    if (error)
	errx (1, "getaddrinfo(%s): %s", hostname, gai_strerror(error));

    for (a = ai; a != NULL; a = a->ai_next) {
	s = socket (a->ai_family, a->ai_socktype, a->ai_protocol);
	if (s < 0)
	    continue;
	socket_set_nopipe(s, 1);
	if (connect (s, a->ai_addr, a->ai_addrlen) < 0) {
	    warn ("connect(%s)", hostname);
 	    close (s);
 	    continue;
	}
	break;
    }
    freeaddrinfo (ai);
    if (a == NULL)
	errx (1, "failed to contact %s", hostname);

    return s;
}

static void
storage_printf(krb5_storage *sp, const char *fmt, ...)
{
    size_t len;
    ssize_t ret;
    va_list ap;
    char *str;

    va_start(ap, fmt);
    vasprintf(&str, fmt, ap);
    va_end(ap);

    if (str == NULL)
	errx(1, "vasprintf");

    len = strlen(str);

    ret = krb5_storage_write(sp, str, len);
    if (ret < 0 || (size_t)ret != len)
	errx(1, "failed to write to server");

    free(str);
}

static int help_flag;
static int version_flag;
static int verbose_flag;
static int mutual_flag = 1;
static int delegate_flag;
static int policy_flag;
static char *port_str = "http";
static char *gss_service = "HTTP";
static char *client_str = NULL;
static char *cred_mech_str = NULL;

static struct getargs http_args[] = {
    { "verbose", 'v', arg_flag, &verbose_flag, "verbose logging", },
    { "port", 'p', arg_string, &port_str, "port to connect to", "port" },
    { "delegate", 0, arg_flag, &delegate_flag, "gssapi delegate credential" },
    { "policy", 0, arg_flag, &policy_flag, "gssapi delegate policy credential" },
    { "gss-service", 's', arg_string, &gss_service, "gssapi service to use",
      "service" },
    { "mech", 'm', arg_string, &mech, "gssapi mech to use", "mech" },
    { "cred-mech", 'c', arg_string, &cred_mech_str, "gssapi mech to use for the cred", "mech" },
    { "mutual", 0, arg_negative_flag, &mutual_flag, "no gssapi mutual auth" },
    { "client", 0, arg_string, &client_str, "client_name" },
    { "help", 'h', arg_flag, &help_flag },
    { "version", 0, arg_flag, &version_flag }
};

static int num_http_args = sizeof(http_args) / sizeof(http_args[0]);

static void
usage(int code)
{
    arg_printusage(http_args, num_http_args, NULL, "host [page]");
    exit(code);
}

/*
 *
 */

struct http_req {
    char *response;
    char **headers;
    unsigned num_headers;
    void *body;
    size_t body_size;
};


static void
http_req_zero(struct http_req *req)
{
    req->response = NULL;
    req->headers = NULL;
    req->num_headers = 0;
    req->body = NULL;
    req->body_size = 0;
}

static void
http_req_free(struct http_req *req)
{
    unsigned i;

    free(req->response);
    for (i = 0; i < req->num_headers; i++)
	free(req->headers[i]);
    free(req->headers);
    free(req->body);
    http_req_zero(req);
}

static const char *
http_find_header(struct http_req *req, const char *header)
{
    size_t len = strlen(header);
    unsigned i;

    for (i = 0; i < req->num_headers; i++) {
	if (strncasecmp(header, req->headers[i], len) == 0) {
	    return req->headers[i] + len + 1;
	}
    }
    return NULL;
}


static int
http_query(krb5_storage *sp,
	   const char *host, const char *page,
	   char **headers, unsigned num_headers, struct http_req *req)
{
    enum { RESPONSE, HEADER, BODY } state;
    ssize_t ret;
    char in_buf[1024];
    size_t in_len = 0, content_length;
    unsigned i;

    http_req_zero(req);
    
    if (verbose_flag) {
	for (i = 0; i < num_headers; i++)
	    printf("outheader[%d]: %s\n", i, headers[0]);
    }

    storage_printf(sp, "GET %s HTTP/1.1\r\n", page);
    for (i = 0; i < num_headers; i++)
	storage_printf(sp, "%s\r\n", headers[i]);
    storage_printf(sp, "Host: %s\r\n\r\n", host);

    state = RESPONSE;

    while (1) {
	char *p;

	ret = krb5_storage_read(sp, in_buf + in_len, 1);
	if (ret != 1)
	    errx(1, "storage foo");
	
	in_len += 1;

	in_buf[in_len] = '\0';

	p = strstr(in_buf, "\r\n");

	if (p == NULL)
	    continue;
	
	if (p == in_buf) {
	    memmove(in_buf, in_buf + 2, sizeof(in_buf) - 2);
	    state = BODY;
	    break;
	} else if (state == RESPONSE) {
	    req->response = strndup(in_buf, p - in_buf);
	    state = HEADER;
	} else {
	    req->headers = realloc(req->headers,
				   (req->num_headers + 1) * sizeof(req->headers[0]));
	    req->headers[req->num_headers] = strndup(in_buf, p - in_buf);
	    if (req->headers[req->num_headers] == NULL)
		errx(1, "strdup");
	    req->num_headers++;
	}
	in_len = 0;
    }

    if (state != BODY)
	abort();

    const char *h = http_find_header(req, "Content-Length:");
    if (h == NULL)
	errx(1, "Missing `Content-Length'");

    content_length = atoi(h);

    req->body_size = content_length;
    req->body = erealloc(req->body, content_length + 1);

    ret = krb5_storage_read(sp, req->body, req->body_size);
    if (ret < 0 || (size_t)ret != req->body_size)
	errx(1, "failed to read body");

    ((char *)req->body)[req->body_size] = '\0';
	
    if (verbose_flag) {
	printf("response: %s\n", req->response);
	for (i = 0; i < req->num_headers; i++)
	    printf("response-header[%d] %s\n", i, req->headers[i]);
	printf("body: %.*s\n", (int)req->body_size, (char *)req->body);
    }

    return 0;
}


int
main(int argc, char **argv)
{
    int i, s, done, print_body, gssapi_done, gssapi_started, optidx = 0;
    const char *host, *page;
    struct http_req req;
    char *headers[99]; /* XXX */
    int num_headers;
    krb5_storage *sp;

    gss_cred_id_t client_cred = GSS_C_NO_CREDENTIAL;
    gss_ctx_id_t context_hdl = GSS_C_NO_CONTEXT;
    gss_name_t server = GSS_C_NO_NAME;
    gss_OID mech_oid, cred_mech_oid;
    OM_uint32 flags;
    OM_uint32 maj_stat, min_stat;

    setprogname(argv[0]);

    if(getarg(http_args, num_http_args, argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if(version_flag) {
	print_version(NULL);
	exit(0);
    }

    argc -= optidx;
    argv += optidx;

    mech_oid = select_mech(mech);

    if (cred_mech_str)
	cred_mech_oid = select_mech(cred_mech_str);
    else
	cred_mech_oid = mech_oid;

    if (argc != 1 && argc != 2)
	errx(1, "usage: %s host [page]", getprogname());
    host = argv[0];
    if (argc == 2)
	page = argv[1];
    else
	page = "/";

    flags = 0;
    if (delegate_flag)
	flags |= GSS_C_DELEG_FLAG;
    if (policy_flag)
	flags |= GSS_C_DELEG_POLICY_FLAG;
    if (mutual_flag)
	flags |= GSS_C_MUTUAL_FLAG;

    done = 0;
    num_headers = 0;
    gssapi_done = 0;
    gssapi_started = 0;

    if (client_str) {
	gss_buffer_desc name_buffer;
	gss_name_t name;
	gss_OID_set mechset = GSS_C_NO_OID_SET;

	name_buffer.value = client_str;
	name_buffer.length = strlen(client_str);

	maj_stat = gss_import_name(&min_stat, &name_buffer, GSS_C_NT_USER_NAME, &name);
	if (maj_stat)
	    errx(1, "failed to import name");

	if (cred_mech_oid) {
	    gss_create_empty_oid_set(&min_stat, &mechset);
	    gss_add_oid_set_member(&min_stat, cred_mech_oid, &mechset);
	}
	
	maj_stat = gss_acquire_cred(&min_stat, name, GSS_C_INDEFINITE,
				    mechset, GSS_C_INITIATE,
				    &client_cred, NULL, NULL);
	gss_release_name(&min_stat, &name);
	gss_release_oid_set(&min_stat, &mechset);
	if (maj_stat)
	    errx(1, "failed to find cred of name %s", client_str);
    }

    {
	gss_buffer_desc name_token;
	char *name;
	asprintf(&name, "%s@%s", gss_service, host);
	name_token.length = strlen(name);
	name_token.value = name;
	
	maj_stat = gss_import_name(&min_stat,
				   &name_token,
				   GSS_C_NT_HOSTBASED_SERVICE,
				   &server);
	if (GSS_ERROR(maj_stat))
	    gss_err (1, min_stat, "gss_inport_name: %s", name);
	free(name);
    }

    s = do_connect(host, port_str);
    if (s < 0)
	errx(1, "connection failed");

    sp = krb5_storage_from_fd(s);
    if (sp == NULL)
	errx(1, "krb5_storage_from_fd");

    do {
	print_body = 0;

	http_query(sp, host, page, headers, num_headers, &req);
	for (i = 0 ; i < num_headers; i++)
	    free(headers[i]);
	num_headers = 0;

	if (strstr(req.response, " 200 ") != NULL) {
	    print_body = 1;
	    done = 1;
	} else if (strstr(req.response, " 401 ") != NULL) {
	    if (http_find_header(&req, "WWW-Authenticate:") == NULL)
		errx(1, "Got %s but missed `WWW-Authenticate'", req.response);
	}

	if (!gssapi_done) {
	    const char *h = http_find_header(&req, "WWW-Authenticate:");
	    if (h == NULL)
		errx(1, "Got %s but missed `WWW-Authenticate'", req.response);

	    if (strncasecmp(h, "Negotiate", 9) == 0) {
		gss_buffer_desc input_token, output_token;

		if (verbose_flag)
		    printf("Negotiate found\n");

		i = 9;
		while(h[i] && isspace((unsigned char)h[i]))
		    i++;
		if (h[i] != '\0') {
		    size_t len = strlen(&h[i]);
		    int slen;
		    if (len == 0)
			errx(1, "invalid Negotiate token");
		    input_token.value = emalloc(len);
		    slen = base64_decode(&h[i], input_token.value);
		    if (slen < 0)
			errx(1, "invalid base64 Negotiate token %s", &h[i]);
		    input_token.length = slen;
		} else {
		    if (gssapi_started)
			errx(1, "Negotiate already started");
		    gssapi_started = 1;

		    input_token.length = 0;
		    input_token.value = NULL;
		}

		if (strstr(req.response, " 200 ") != NULL)
		    sleep(1);

		maj_stat =
		    gss_init_sec_context(&min_stat,
					 client_cred,
					 &context_hdl,
					 server,
					 mech_oid,
					 flags,
					 0,
					 GSS_C_NO_CHANNEL_BINDINGS,
					 &input_token,
					 NULL,
					 &output_token,
					 NULL,
					 NULL);
		if (maj_stat == GSS_S_CONTINUE_NEEDED) {

		} else if (maj_stat == GSS_S_COMPLETE) {
		    gss_name_t targ_name, src_name;
		    gss_buffer_desc name_buffer;
		    gss_OID mech_type;

		    gssapi_done = 1;

		    maj_stat = gss_inquire_context(&min_stat,
						   context_hdl,
						   &src_name,
						   &targ_name,
						   NULL,
						   &mech_type,
						   NULL,
						   NULL,
						   NULL);
		    if (GSS_ERROR(maj_stat))
			gss_err (1, min_stat, "gss_inquire_context");

		    printf("Negotiate done: %s\n", mech);

		    maj_stat = gss_display_name(&min_stat,
						src_name,
						&name_buffer,
						NULL);
		    if (GSS_ERROR(maj_stat))
			gss_print_errors(min_stat);
		    else
			printf("Source: %.*s\n",
			       (int)name_buffer.length,
			       (char *)name_buffer.value);

		    gss_release_buffer(&min_stat, &name_buffer);

		    maj_stat = gss_display_name(&min_stat,
						targ_name,
						&name_buffer,
						NULL);
		    if (GSS_ERROR(maj_stat))
			gss_print_errors(min_stat);
		    else
			printf("Target: %.*s\n",
			       (int)name_buffer.length,
			       (char *)name_buffer.value);

		    gss_release_name(&min_stat, &targ_name);
		    gss_release_buffer(&min_stat, &name_buffer);
		} else {
		    gss_err (1, min_stat, "gss_init_sec_context");
		}


		if (output_token.length) {
		    char *neg_token;

		    base64_encode(output_token.value,
				  (int)output_token.length,
				  &neg_token);

		    asprintf(&headers[0], "Authorization: Negotiate %s",
			     neg_token);

		    num_headers = 1;
		    free(neg_token);
		    gss_release_buffer(&min_stat, &output_token);
		}
		if (input_token.length)
		    free(input_token.value);

	    } else
		done = 1;
	} else
	    done = 1;

	if (print_body || verbose_flag)
	    printf("%.*s\n", (int)req.body_size, (char *)req.body);

	http_req_free(&req);
    } while (!done);

    if (gssapi_done == 0)
	errx(1, "gssapi not done but http dance done");

    krb5_storage_free(sp);
    close(s);

    return 0;
}
