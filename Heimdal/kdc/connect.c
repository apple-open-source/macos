/*
 * Copyright (c) 1997-2005 Kungliga Tekniska Högskolan
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

#include "kdc_locl.h"

/* Should we enable the HTTP hack? */
int enable_http = -1;

/* Log over requests to the KDC */
const char *request_log;

/* A string describing on what ports to listen */
const char *port_str;

krb5_addresses explicit_addresses;

size_t max_request_udp;
size_t max_request_tcp;

/*
 * a tuple describing on what to listen
 */

struct port_desc{
    int family;
    int type;
    int port;
};

/* the current ones */

static struct port_desc *ports;
static size_t num_ports;

static void
kdc_service(void *ctx, const heim_idata *req,
	    const heim_icred cred,
	    heim_ipc_complete complete,
	    heim_sipc_call cctx);



/*
 * add `family, port, protocol' to the list with duplicate suppresion.
 */

static void
add_port(krb5_context context,
	 int family, int port, const char *protocol)
{
    int type;
    size_t i;

    if(strcmp(protocol, "udp") == 0)
	type = SOCK_DGRAM;
    else if(strcmp(protocol, "tcp") == 0)
	type = SOCK_STREAM;
    else
	return;
    for(i = 0; i < num_ports; i++){
	if(ports[i].type == type
	   && ports[i].port == port
	   && ports[i].family == family)
	    return;
    }
    ports = realloc(ports, (num_ports + 1) * sizeof(*ports));
    if (ports == NULL)
	krb5_err (context, 1, errno, "realloc");
    ports[num_ports].family = family;
    ports[num_ports].type   = type;
    ports[num_ports].port   = port;
    num_ports++;
}

/*
 * add a triple but with service -> port lookup
 * (this prints warnings for stuff that does not exist)
 */

static void
add_port_service(krb5_context context,
		 int family, const char *service, int port,
		 const char *protocol)
{
    port = krb5_getportbyname (context, service, protocol, port);
    add_port (context, family, port, protocol);
}

/*
 * add the port with service -> port lookup or string -> number
 * (no warning is printed)
 */

static void
add_port_string (krb5_context context,
		 int family, const char *str, const char *protocol)
{
    struct servent *sp;
    int port;

    sp = roken_getservbyname (str, protocol);
    if (sp != NULL) {
	port = sp->s_port;
    } else {
	char *end;

	port = htons(strtol(str, &end, 0));
	if (end == str)
	    return;
    }
    add_port (context, family, port, protocol);
}

/*
 * add the standard collection of ports for `family'
 */

static void
add_standard_ports (krb5_context context,
		    krb5_kdc_configuration *config,
		    int family)
{
    add_port_service(context, family, "kerberos", 88, "udp");
    add_port_service(context, family, "kerberos", 88, "tcp");
    add_port_service(context, family, "kerberos-sec", 88, "udp");
    add_port_service(context, family, "kerberos-sec", 88, "tcp");
    if(enable_http)
	add_port_service(context, family, "http", 80, "tcp");
    if(config->enable_kx509) {
	add_port_service(context, family, "kca_service", 9878, "udp");
	add_port_service(context, family, "kca_service", 9878, "tcp");
    }

}

/*
 * parse the set of space-delimited ports in `str' and add them.
 * "+" => all the standard ones
 * otherwise it's port|service[/protocol]
 */

static void
parse_ports(krb5_context context,
	    krb5_kdc_configuration *config,
	    const char *str)
{
    char *pos = NULL;
    char *p;
    char *str_copy = strdup (str);

    p = strtok_r(str_copy, " \t", &pos);
    while(p != NULL) {
	if(strcmp(p, "+") == 0) {
#ifdef HAVE_IPV6
	    add_standard_ports(context, config, AF_INET6);
#endif
	    add_standard_ports(context, config, AF_INET);
	} else {
	    char *q = strchr(p, '/');
	    if(q){
		*q++ = 0;
#ifdef HAVE_IPV6
		add_port_string(context, AF_INET6, p, q);
#endif
		add_port_string(context, AF_INET, p, q);
	    }else {
#ifdef HAVE_IPV6
		add_port_string(context, AF_INET6, p, "udp");
		add_port_string(context, AF_INET6, p, "tcp");
#endif
		add_port_string(context, AF_INET, p, "udp");
		add_port_string(context, AF_INET, p, "tcp");
	    }
	}

	p = strtok_r(NULL, " \t", &pos);
    }
    free (str_copy);
}

/*
 * every socket we listen on
 */

struct descr {
    krb5_socket_t s;
    int type;
    int port;
    unsigned char *buf;
    size_t size;
    size_t len;
    time_t timeout;
    struct sockaddr_storage __ss;
    struct sockaddr *sa;
    socklen_t sock_len;
    char addr_string[128];
    heim_sipc u;
};

static void
init_descr(struct descr *d)
{
    memset(d, 0, sizeof(*d));
    d->sa = (struct sockaddr *)&d->__ss;
    d->s = rk_INVALID_SOCKET;
}

/*
 * Create the socket (family, type, port) in `d'
 */

static void
init_socket(krb5_context context,
	    krb5_kdc_configuration *config,
	    struct descr *d, krb5_address *a, int family, int type, int port)
{
    krb5_error_code ret;
    struct sockaddr_storage __ss;
    struct sockaddr *sa = (struct sockaddr *)&__ss;
    krb5_socklen_t sa_size = sizeof(__ss);
    int http_flag = 0;
    
    if (enable_http == 1)
	http_flag = HEIM_SIPC_TYPE_HTTP;

    init_descr(d);

    ret = krb5_addr2sockaddr (context, a, sa, &sa_size, port);
    if (ret) {
	krb5_warn(context, ret, "krb5_addr2sockaddr");
	rk_closesocket(d->s);
	d->s = rk_INVALID_SOCKET;
	return;
    }

    if (sa->sa_family != family)
	return;

    d->s = socket(family, type, 0);
    if(rk_IS_BAD_SOCKET(d->s)){
	krb5_warn(context, errno, "socket(%d, %d, 0)", family, type);
	d->s = rk_INVALID_SOCKET;
	return;
    }
    socket_set_reuseaddr(d->s, 1);
    socket_set_nopipe(d->s, 1);
#ifdef HAVE_IPV6
    if (family == AF_INET6)
	socket_set_ipv6only(d->s, 1);
#endif
    d->type = type;
    d->port = port;

    if(rk_IS_SOCKET_ERROR(bind(d->s, sa, sa_size))){
	char a_str[256];
	size_t len;

	krb5_print_address (a, a_str, sizeof(a_str), &len);
	krb5_warn(context, errno, "bind %s/%d", a_str, ntohs(port));
	rk_closesocket(d->s);
	d->s = rk_INVALID_SOCKET;
	return;
    }

    if(type == SOCK_STREAM && listen(d->s, SOMAXCONN) < 0){
	char a_str[256];
	size_t len;

	krb5_print_address (a, a_str, sizeof(a_str), &len);
	krb5_warn(context, errno, "listen %s/%d", a_str, ntohs(port));
	rk_closesocket(d->s);
	d->s = rk_INVALID_SOCKET;
	return;
    }

    if (type == SOCK_STREAM) {
	ret = heim_sipc_stream_listener(d->s,
					HEIM_SIPC_TYPE_UINT32|http_flag|HEIM_SIPC_TYPE_ONE_REQUEST,
					kdc_service, d, &d->u);
	if (ret)
	    errx(1, "heim_sipc_stream_listener: %d", ret);
    } else {
	ret = heim_sipc_service_dgram(d->s, 0,
				      kdc_service, d, &d->u);
	if (ret)
	    errx(1, "heim_sipc_service_dgram: %d", ret);
    }
}

/*
 * Allocate descriptors for all the sockets that we should listen on
 * and return the number of them.
 */

static void
init_sockets(krb5_context context,
	     krb5_kdc_configuration *config)
{
    krb5_error_code ret;
    size_t i, j;
    struct descr *d;
    krb5_addresses addresses;
    int found = 0;

    if (explicit_addresses.len) {
	addresses = explicit_addresses;
    } else {
#if defined(IPV6_PKTINFO) && defined(IP_PKTINFO)
	ret = krb5_get_all_any_addrs(context, &addresses);
#else
	ret = krb5_get_all_server_addrs(context, &addresses);
#endif
	if (ret)
	    krb5_err (context, 1, ret, "krb5_get_all_{server,any}_addrs");
    }

    parse_ports(context, config, port_str);

    for (i = 0; i < num_ports; i++){
	for (j = 0; j < addresses.len; ++j) {
	    char a_str[80];
	    size_t len;

	    d = calloc(1, sizeof(*d));
	    heim_assert(d != NULL, "out of memory");

	    init_socket(context, config, d, &addresses.val[j],
			ports[i].family, ports[i].type, ports[i].port);
	    krb5_print_address (&addresses.val[j], a_str,
				sizeof(a_str), &len);

	    kdc_log(context, config, 5, "%slistening on %s port %u/%s",
		    d->s != rk_INVALID_SOCKET ? "" : "FAILED ",
		    a_str,
		    ntohs(ports[i].port),
		    (ports[i].type == SOCK_STREAM) ? "tcp" : "udp");

	    if(d->s == rk_INVALID_SOCKET)
		free(d);
	    else
		found = 1;
	}
    }

    if (explicit_addresses.len == 0)
	krb5_free_addresses (context, &addresses);

    if (!found)
	krb5_errx(context, 1, "No sockets!");
}

/*
 *
 */

static krb5_kdc_configuration *kdc_config;
	   
static HEIMDAL_MUTEX context_mutex = HEIMDAL_MUTEX_INITIALIZER;
static int created_key;
static HEIMDAL_thread_key context_key;


static void
destroy_context(void *ptr)
{
    krb5_context c = ptr;
    if (c)
	krb5_free_context(c);
}


static krb5_context
thread_context(void)
{
    krb5_context context = NULL;
    int ret = 0;

    HEIMDAL_MUTEX_lock(&context_mutex);

    if (!created_key) {
	HEIMDAL_key_create(&context_key, destroy_context, ret);
	heim_assert(ret == 0, "failed to create context key");
	created_key = 1;
    }
    HEIMDAL_MUTEX_unlock(&context_mutex);

    context = HEIMDAL_getspecific(context_key);
    if (context == NULL) {
	ret = krb5_init_context(&context);
	heim_assert(ret == 0 && context != NULL, "failed to create context");

	HEIMDAL_setspecific(context_key, context, ret);
	heim_assert(ret == 0, "failed to set context key");

    }
    return context;
}


/*
 *
 */

static void
kdc_service(void *ctx, const heim_idata *req,
	     const heim_icred cred,
	     heim_ipc_complete complete,
	     heim_sipc_call cctx)
{
    krb5_socklen_t sasize;
    struct sockaddr *sa;
    struct descr *d = ctx;
    int datagram_reply = (d->type == SOCK_DGRAM);
    krb5_data reply;
    krb5_error_code ret;
    char addr[NI_MAXHOST], port[NI_MAXSERV];
    krb5_context context;

    context = thread_context();

    krb5_kdc_update_time(NULL);
    krb5_data_zero(&reply);
 
    sa = heim_ipc_cred_get_client_address(cred, &sasize);

    if (sa == NULL || getnameinfo(sa, sasize, addr, sizeof(addr), port, sizeof(port), NI_NUMERICHOST|NI_NUMERICSERV) != 0)
	strlcpy(addr, "unknown network address", sizeof(addr));
    else {
	strlcat(addr, ":", sizeof(addr));
	strlcat(addr, port, sizeof(addr));
    }

    ret = krb5_kdc_process_request(context, kdc_config,
				   req->data, req->length,
				   &reply,
				   addr, sa,
				   datagram_reply);
    if(request_log)
	krb5_kdc_save_request(context, request_log,
			      req->data, req->length, &reply, d->sa);

    (*complete)(cctx, ret, &reply);
    krb5_data_free(&reply);
}

static void
kdc_local(void *ctx, const heim_idata *req,
	  const heim_icred cred,
	  heim_ipc_complete complete,
	  heim_sipc_call cctx)
{
    krb5_context context;
    krb5_error_code ret;
    krb5_data reply;

    krb5_kdc_update_time(NULL);
    krb5_data_zero(&reply);

    context = thread_context();

    ret = krb5_kdc_process_request(context, kdc_config,
				   req->data, req->length,
				   &reply,
				   "local-ipc", NULL, 0);
    (*complete)(cctx, ret, &reply);
    krb5_data_free(&reply);
}



void
setup_listeners(krb5_context context,
		krb5_kdc_configuration *config,
		int ipc, int network)
{
    kdc_config = config;

    if (network)
	init_sockets(context, config);

#ifdef __APPLE__
    if (ipc) {
	heim_sipc mach;
	heim_sipc_launchd_mach_init("org.h5l.kdc", kdc_local, NULL, &mach);
    }
#endif
    kdc_log(context, config, 0, "KDC started");
}
