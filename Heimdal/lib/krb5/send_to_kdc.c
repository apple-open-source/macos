/*
 * Copyright (c) 1997 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
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

#include "krb5_locl.h"
#include "send_to_kdc_plugin.h"

struct send_to_kdc {
    krb5_send_to_kdc_func func;
    void *data;
};

/*
 * send the data in `req' on the socket `fd' (which is datagram iff udp)
 * waiting `tmout' for a reply and returning the reply in `rep'.
 * iff limit read up to this many bytes
 * returns 0 and data in `rep' if succesful, otherwise -1
 */

static int
recv_loop (krb5_socket_t fd,
	   time_t tmout,
	   int udp,
	   size_t limit,
	   krb5_data *rep)
{
     fd_set fdset;
     struct timeval timeout;
     int ret;
     int nbytes;

#ifndef NO_LIMIT_FD_SETSIZE
     if (fd >= FD_SETSIZE) {
	 return -1;
     }
#endif

     krb5_data_zero(rep);
     do {
	 FD_ZERO(&fdset);
	 FD_SET(fd, &fdset);
	 timeout.tv_sec  = tmout;
	 timeout.tv_usec = 0;
	 ret = select (fd + 1, &fdset, NULL, NULL, &timeout);
	 if (ret < 0) {
	     if (errno == EINTR)
		 continue;
	     return -1;
	 } else if (ret == 0) {
	     return 0;
	 } else {
	     void *tmp;

	     if (rk_SOCK_IOCTL (fd, FIONREAD, &nbytes) < 0) {
		 krb5_data_free (rep);
		 return -1;
	     }
	     if(nbytes <= 0)
		 return 0;

	     if (limit)
		 nbytes = min(nbytes, limit - rep->length);

	     tmp = realloc (rep->data, rep->length + nbytes);
	     if (tmp == NULL) {
		 krb5_data_free (rep);
		 return -1;
	     }
	     rep->data = tmp;
	     ret = recv (fd, (char*)tmp + rep->length, nbytes, 0);
	     if (ret < 0) {
		 krb5_data_free (rep);
		 return -1;
	     }
	     rep->length += ret;
	 }
     } while(!udp && (limit == 0 || rep->length < limit));
     return 0;
}

/*
 * Send kerberos requests and receive a reply on a udp or any other kind
 * of a datagram socket.  See `recv_loop'.
 */

static int
send_and_recv_udp(krb5_socket_t fd,
		  time_t tmout,
		  const krb5_data *req,
		  krb5_data *rep)
{
    if (send (fd, req->data, req->length, 0) < 0)
	return -1;

    return recv_loop(fd, tmout, 1, 0, rep);
}

/*
 * `send_and_recv' for a TCP (or any other stream) socket.
 * Since there are no record limits on a stream socket the protocol here
 * is to prepend the request with 4 bytes of its length and the reply
 * is similarly encoded.
 */

static int
send_and_recv_tcp(krb5_socket_t fd,
		  time_t tmout,
		  const krb5_data *req,
		  krb5_data *rep)
{
    unsigned char len[4];
    unsigned long rep_len;
    krb5_data len_data;

    _krb5_put_int(len, req->length, 4);
    if(net_write (fd, len, sizeof(len)) < 0)
	return -1;
    if(net_write (fd, req->data, req->length) < 0)
	return -1;
    if (recv_loop (fd, tmout, 0, 4, &len_data) < 0)
	return -1;
    if (len_data.length != 4) {
	krb5_data_free (&len_data);
	return -1;
    }
    _krb5_get_int(len_data.data, &rep_len, 4);
    krb5_data_free (&len_data);
    if (recv_loop (fd, tmout, 0, rep_len, rep) < 0)
	return -1;
    if(rep->length != rep_len) {
	krb5_data_free (rep);
	return -1;
    }
    return 0;
}

int
_krb5_send_and_recv_tcp(krb5_socket_t fd,
			time_t tmout,
			const krb5_data *req,
			krb5_data *rep)
{
    return send_and_recv_tcp(fd, tmout, req, rep);
}

/*
 * `send_and_recv' tailored for the HTTP protocol.
 */

static int
send_and_recv_http(krb5_socket_t fd,
		   time_t tmout,
		   const char *prefix,
		   const krb5_data *req,
		   krb5_data *rep)
{
    char *request = NULL;
    char *str;
    int ret;
    int len = base64_encode(req->data, req->length, &str);

    if(len < 0)
	return -1;
    ret = asprintf(&request, "GET %s%s HTTP/1.0\r\n\r\n", prefix, str);
    free(str);
    if (ret < 0 || request == NULL)
	return -1;
    ret = net_write (fd, request, strlen(request));
    free (request);
    if (ret < 0)
	return ret;
    ret = recv_loop(fd, tmout, 0, 0, rep);
    if(ret)
	return ret;
    {
	unsigned long rep_len;
	char *s, *p;

	s = realloc(rep->data, rep->length + 1);
	if (s == NULL) {
	    krb5_data_free (rep);
	    return -1;
	}
	s[rep->length] = 0;
	p = strstr(s, "\r\n\r\n");
	if(p == NULL) {
	    krb5_data_zero(rep);
	    free(s);
	    return -1;
	}
	p += 4;
	rep->data = s;
	rep->length -= p - s;
	if(rep->length < 4) { /* remove length */
	    krb5_data_zero(rep);
	    free(s);
	    return -1;
	}
	rep->length -= 4;
	_krb5_get_int(p, &rep_len, 4);
	if (rep_len != rep->length) {
	    krb5_data_zero(rep);
	    free(s);
	    return -1;
	}
	memmove(rep->data, p + 4, rep->length);
    }
    return 0;
}

static int
init_port(const char *s, int fallback)
{
    if (s) {
	int tmp;

	sscanf (s, "%d", &tmp);
	return htons(tmp);
    } else
	return fallback;
}

/*
 * Return 0 if succesful, otherwise 1
 */

static int
send_via_proxy (krb5_context context,
		const krb5_krbhst_info *hi,
		const krb5_data *send_data,
		krb5_data *receive)
{
    char *proxy2 = strdup(context->http_proxy);
    char *proxy  = proxy2;
    char *prefix = NULL;
    char *colon;
    struct addrinfo hints;
    struct addrinfo *ai, *a;
    int ret;
    krb5_socket_t s = rk_INVALID_SOCKET;
    char portstr[NI_MAXSERV];
		
    if (proxy == NULL)
	return ENOMEM;
    if (strncmp (proxy, "http://", 7) == 0)
	proxy += 7;

    colon = strchr(proxy, ':');
    if(colon != NULL)
	*colon++ = '\0';
    memset (&hints, 0, sizeof(hints));
    hints.ai_family   = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf (portstr, sizeof(portstr), "%d",
	      ntohs(init_port (colon, htons(80))));
    ret = getaddrinfo (proxy, portstr, &hints, &ai);
    free (proxy2);
    if (ret)
	return krb5_eai_to_heim_errno(ret, errno);

    for (a = ai; a != NULL; a = a->ai_next) {
	s = socket (a->ai_family, a->ai_socktype | SOCK_CLOEXEC, a->ai_protocol);
	if (s < 0)
	    continue;
	rk_cloexec(s);
	if (connect (s, a->ai_addr, a->ai_addrlen) < 0) {
	    rk_closesocket (s);
	    continue;
	}
	break;
    }
    if (a == NULL) {
	freeaddrinfo (ai);
	return 1;
    }
    freeaddrinfo (ai);

    ret = asprintf(&prefix, "http://%s/", hi->hostname);
    if(ret < 0 || prefix == NULL) {
	close(s);
	return 1;
    }
    ret = send_and_recv_http(s, context->kdc_timeout,
			     prefix, send_data, receive);
    rk_closesocket (s);
    free(prefix);
    if(ret == 0 && receive->length != 0)
	return 0;
    return 1;
}

static krb5_error_code
send_via_plugin(krb5_context context,
		krb5_krbhst_info *hi,
		time_t timeout,
		const krb5_data *send_data,
		krb5_data *receive)
{
    struct krb5_plugin *list = NULL, *e;
    krb5_error_code ret;

    ret = _krb5_plugin_find(context, PLUGIN_TYPE_DATA, KRB5_PLUGIN_SEND_TO_KDC, &list);
    if(ret != 0 || list == NULL)
	return KRB5_PLUGIN_NO_HANDLE;

    for (e = list; e != NULL; e = _krb5_plugin_get_next(e)) {
	krb5plugin_send_to_kdc_ftable *service;
	void *ctx;

	service = _krb5_plugin_get_symbol(e);
	if (service->minor_version != 0)
	    continue;
	
	(*service->init)(context, &ctx);
	ret = (*service->send_to_kdc)(context, ctx, hi,
				      timeout, send_data, receive);
	(*service->fini)(ctx);
	if (ret == 0)
	    break;
	if (ret != KRB5_PLUGIN_NO_HANDLE) {
	    krb5_set_error_message(context, ret,
				   N_("Plugin send_to_kdc failed to "
				      "lookup with error: %d", ""), ret);
	    break;
	}
    }
    _krb5_plugin_free(list);
    return KRB5_PLUGIN_NO_HANDLE;
}


/*
 * Send the data `send' to one host from `handle` and get back the reply
 * in `receive'.
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_sendto (krb5_context context,
	     const krb5_data *send_data,
	     krb5_krbhst_handle handle,	
	     krb5_data *receive)
{
     krb5_error_code ret;
     krb5_socket_t fd;
     int i;

     krb5_data_zero(receive);

     for (i = 0; i < context->max_retries; ++i) {
	 krb5_krbhst_info *hi;

	 while (krb5_krbhst_next(context, handle, &hi) == 0) {
	     struct addrinfo *ai, *a;

	     _krb5_debugx(context, 2,
			 "trying to communicate with host %s in realm %s",
			 hi->hostname, _krb5_krbhst_get_realm(handle));

	     if (context->send_to_kdc) {
		 struct send_to_kdc *s = context->send_to_kdc;

		 ret = (*s->func)(context, s->data, hi,
				  context->kdc_timeout, send_data, receive);
		 if (ret == 0 && receive->length != 0)
		     goto out;
		 continue;
	     }

	     ret = send_via_plugin(context, hi, context->kdc_timeout,
				   send_data, receive);
	     if (ret == 0 && receive->length != 0)
		 goto out;
	     else if (ret != KRB5_PLUGIN_NO_HANDLE)
		 continue;

	     if(hi->proto == KRB5_KRBHST_HTTP && context->http_proxy) {
		 if (send_via_proxy (context, hi, send_data, receive) == 0) {
		     ret = 0;
		     goto out;
		 }
		 continue;
	     }

	     ret = krb5_krbhst_get_addrinfo(context, hi, &ai);
	     if (ret)
		 continue;

	     for (a = ai; a != NULL; a = a->ai_next) {
		 fd = socket (a->ai_family, a->ai_socktype | SOCK_CLOEXEC, a->ai_protocol);
		 if (rk_IS_BAD_SOCKET(fd))
		     continue;
		 rk_cloexec(fd);
		 if (connect (fd, a->ai_addr, a->ai_addrlen) < 0) {
		     rk_closesocket (fd);
		     continue;
		 }
		 switch (hi->proto) {
		 case KRB5_KRBHST_HTTP :
		     ret = send_and_recv_http(fd, context->kdc_timeout,
					      "", send_data, receive);
		     break;
		 case KRB5_KRBHST_TCP :
		     ret = send_and_recv_tcp (fd, context->kdc_timeout,
					      send_data, receive);
		     break;
		 case KRB5_KRBHST_UDP :
		     ret = send_and_recv_udp (fd, context->kdc_timeout,
					      send_data, receive);
		     break;
		 }
		 rk_closesocket (fd);
		 if(ret == 0 && receive->length != 0)
		     goto out;
	     }
	 }
	 krb5_krbhst_reset(context, handle);
     }
     krb5_clear_error_message (context);
     ret = KRB5_KDC_UNREACH;
out:
     _krb5_debugx(context, 2,
		 "result of trying to talk to realm %s = %d",
		 _krb5_krbhst_get_realm(handle), ret);
     return ret;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_sendto_kdc(krb5_context context,
		const krb5_data *send_data,
		const krb5_realm *realm,
		krb5_data *receive)
{
    return krb5_sendto_kdc_flags(context, send_data, realm, receive, 0);
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_sendto_kdc_flags(krb5_context context,
		      const krb5_data *send_data,
		      const krb5_realm *realm,
		      krb5_data *receive,
		      int flags)
{
    krb5_error_code ret;
    krb5_sendto_ctx ctx;

    ret = krb5_sendto_ctx_alloc(context, &ctx);
    if (ret)
	return ret;
    krb5_sendto_ctx_add_flags(ctx, flags);
    krb5_sendto_ctx_set_func(ctx, _krb5_kdc_retry, NULL);

    ret = krb5_sendto_context(context, ctx, send_data, *realm, receive);
    krb5_sendto_ctx_free(context, ctx);
    return ret;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_set_send_to_kdc_func(krb5_context context,
			  krb5_send_to_kdc_func func,
			  void *data)
{
    free(context->send_to_kdc);
    if (func == NULL) {
	context->send_to_kdc = NULL;
	return 0;
    }

    context->send_to_kdc = malloc(sizeof(*context->send_to_kdc));
    if (context->send_to_kdc == NULL) {
	krb5_set_error_message(context, ENOMEM,
			       N_("malloc: out of memory", ""));
	return ENOMEM;
    }

    context->send_to_kdc->func = func;
    context->send_to_kdc->data = data;
    return 0;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_copy_send_to_kdc_func(krb5_context context, krb5_context to)
{
    if (context->send_to_kdc)
	return krb5_set_send_to_kdc_func(to,
					 context->send_to_kdc->func, 
					 context->send_to_kdc->data);
    else
	return krb5_set_send_to_kdc_func(to, NULL, NULL);
}

struct krb5_sendto_ctx_data {
    int flags;
    int type;
    krb5_sendto_ctx_func func;
    void *data;
    char *hostname;

    /* context2 */
    const krb5_data *send_data;
    krb5_data response;
    heim_array_t hosts;
    int stateflags;
#define KRBHST_COMPLETED	1

    /* prexmit */
    krb5_sendto_prexmit prexmit_func;
    void *prexmit_ctx;
};

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_sendto_ctx_alloc(krb5_context context, krb5_sendto_ctx *ctx)
{
    *ctx = calloc(1, sizeof(**ctx));
    if (*ctx == NULL) {
	krb5_set_error_message(context, ENOMEM,
			       N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    (*ctx)->hosts = heim_array_create();

    return 0;
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_sendto_ctx_add_flags(krb5_sendto_ctx ctx, int flags)
{
    ctx->flags |= flags;
}

KRB5_LIB_FUNCTION int KRB5_LIB_CALL
krb5_sendto_ctx_get_flags(krb5_sendto_ctx ctx)
{
    return ctx->flags;
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_sendto_ctx_set_type(krb5_sendto_ctx ctx, int type)
{
    ctx->type = type;
}


KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_sendto_ctx_set_func(krb5_sendto_ctx ctx,
			 krb5_sendto_ctx_func func,
			 void *data)
{
    ctx->func = func;
    ctx->data = data;
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_sendto_ctx_set_prexmit(krb5_sendto_ctx ctx,
			     krb5_sendto_prexmit prexmit,
			     void *data)
{
    ctx->prexmit_func = prexmit;
    ctx->prexmit_ctx = data;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_sendto_set_hostname(krb5_context context,
			 krb5_sendto_ctx ctx,
			 const char *hostname)
{
    if (ctx->hostname == NULL)
	free(ctx->hostname);
    ctx->hostname = strdup(hostname);
    if (ctx->hostname == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    return 0;
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_sendto_ctx_free(krb5_context context, krb5_sendto_ctx ctx)
{
    if (ctx->hostname)
	free(ctx->hostname);
    heim_release(ctx->hosts);
    memset(ctx, 0, sizeof(*ctx));
    free(ctx);
}

krb5_error_code
_krb5_kdc_retry(krb5_context context, krb5_sendto_ctx ctx, void *data,
		const krb5_data *reply, int *action)
{
    krb5_error_code ret;
    KRB_ERROR error;

    if(krb5_rd_error(context, reply, &error))
	return 0;

    ret = krb5_error_from_rd_error(context, &error, NULL);
    krb5_free_error_contents(context, &error);

    switch(ret) {
    case KRB5KRB_ERR_RESPONSE_TOO_BIG: {
	if (krb5_sendto_ctx_get_flags(ctx) & KRB5_KRBHST_FLAGS_LARGE_MSG)
	    break;
	krb5_sendto_ctx_add_flags(ctx, KRB5_KRBHST_FLAGS_LARGE_MSG);
	*action = KRB5_SENDTO_RESET;
	break;
    }
    case KRB5KDC_ERR_SVC_UNAVAILABLE:
	*action = KRB5_SENDTO_CONTINUE;
	break;
    }
    return 0;
}

/*
 *
 */

struct host {
    enum host_state { CONNECTING, CONNECTED, WAITING_REPLY, DEAD } state;
    krb5_krbhst_info *hi;
    struct addrinfo *ai;
    rk_socket_t fd;
    krb5_error_code (*prepare)(krb5_context, struct host *, const krb5_data *);
    krb5_error_code (*send)(krb5_context, struct host *);
    krb5_error_code (*recv)(krb5_context, struct host *, krb5_data *);
    unsigned int tries;
    time_t timeout;
    krb5_data data;
};

static void
debug_host(krb5_context context, int level, struct host *host, const char *msg)
{
    const char *proto = "unknown";
    char name[NI_MAXHOST], port[NI_MAXSERV];

    if (!_krb5_have_debug(context, 5))
	return;

    if (host->hi->proto == KRB5_KRBHST_HTTP)
	proto = "http";
    else if (host->hi->proto == KRB5_KRBHST_TCP)
	proto = "tcp";
    else if (host->hi->proto == KRB5_KRBHST_UDP)
	proto = "udp";

    if (getnameinfo(host->ai->ai_addr, host->ai->ai_addrlen,
		    name, sizeof(name), port, sizeof(port), NI_NUMERICHOST) != 0)
	name[0] = '\0';

    _krb5_debugx(context, level, "%s: %s %s:%s (%s)", msg, proto, name, port, host->hi->hostname);
}


static void
deallocate_host(void *ptr)
{
    struct host *host = ptr;
    if (!rk_IS_BAD_SOCKET(host->fd))
	rk_closesocket(host->fd);
    krb5_data_free(&host->data);
    host->ai = NULL;
}

static void
host_dead(krb5_context context, struct host *host, const char *msg)
{
    debug_host(context, 5, host, msg);
    rk_closesocket(host->fd);
    host->fd = rk_INVALID_SOCKET;
    host->state = DEAD;
}

static krb5_error_code
send_stream(krb5_context context, struct host *host)
{
    ssize_t len;

    len = write(host->fd, host->data.data, host->data.length);

    if (len < 0)
	return errno;
    else if (len < host->data.length) {
	host->data.length -= len;
	memmove(host->data.data, ((uint8_t *)host->data.data) + len, host->data.length - len);
	return -1;
    } else {
	krb5_data_free(&host->data);
	return 0;
    }
}

static krb5_error_code
recv_stream(krb5_context context, struct host *host)
{
    krb5_error_code ret;
    size_t oldlen;
    ssize_t sret;
    int nbytes;

    if (rk_SOCK_IOCTL(host->fd, FIONREAD, &nbytes) != 0 || nbytes <= 0)
	return HEIM_NET_CONN_REFUSED;

    if (context->max_msg_size - host->data.length < nbytes) {
	krb5_set_error_message(context, KRB5KRB_ERR_FIELD_TOOLONG,
			       N_("TCP message from KDC too large %d", ""),
			       (int)(host->data.length + nbytes));
	return KRB5KRB_ERR_FIELD_TOOLONG;
    }

    oldlen = host->data.length;

    ret = krb5_data_realloc(&host->data, oldlen + nbytes + 1 /* NUL */);
    if (ret)
	return ret;

    sret = read(host->fd, ((uint8_t *)host->data.data) + oldlen, nbytes);
    if (sret <= 0) {
	ret = errno;
	return ret;
    }
    host->data.length = oldlen + sret;
    /* zero terminate for http transport */
    ((uint8_t *)host->data.data)[host->data.length] = '\0';

    return 0;
}


/*
 * HTTP transport
 */

static krb5_error_code
prepare_http(krb5_context context, struct host *host, const krb5_data *data)
{
    char *str = NULL, *request = NULL;
    krb5_error_code ret;
    int len;

    len = base64_encode(data->data, data->length, &str);
    if(len < 0)
	return ENOMEM;

    if (context->http_proxy)
	ret = asprintf(&request, "GET http://%s/%s HTTP/1.0\r\n\r\n", host->hi->hostname, str);
    else
	ret = asprintf(&request, "GET /%s HTTP/1.0\r\n\r\n", str);
    free(str);
    if(ret < 0 || request == NULL)
	return ENOMEM;
    
    host->data.data = request;
    host->data.length = strlen(request);

    return 0;
}

static krb5_error_code
recv_http(krb5_context context, struct host *host, krb5_data *data)
{
    krb5_error_code ret;
    unsigned long rep_len;
    size_t len;
    char *p;

    /*
     * recv_stream returns a NUL terminated stream
     */

    ret = recv_stream(context, host);
    if (ret)
	return ret;

    p = strstr(host->data.data, "\r\n\r\n");
    if (p == NULL)
	return -1;
    p += 4;

    len = host->data.length - (p - (char *)host->data.data);
    if (len < 4)
	return KRB5KRB_ERR_FIELD_TOOLONG;

    _krb5_get_int(p, &rep_len, 4);
    if (len < rep_len)
	return -1;

    p += 4;

    memmove(host->data.data, p, rep_len);
    host->data.length = rep_len;

    *data = host->data;
    krb5_data_zero(&host->data);

    return 0;
}

/*
 * TCP transport
 */

static krb5_error_code
prepare_tcp(krb5_context context, struct host *host, const krb5_data *data)
{
    krb5_error_code ret;
    krb5_storage *sp;

    sp = krb5_storage_emem();
    if (sp == NULL)
	return ENOMEM;
    
    ret = krb5_store_data(sp, *data);
    if (ret) {
	krb5_storage_free(sp);
	return ret;
    }
    ret = krb5_storage_to_data(sp, &host->data);
    krb5_storage_free(sp);

    return ret;
}

static krb5_error_code
recv_tcp(krb5_context context, struct host *host, krb5_data *data)
{
    krb5_error_code ret;
    unsigned long pktlen;

    ret = recv_stream(context, host);
    if (ret)
	return ret;

    if (host->data.length < 4)
	return -1;

    _krb5_get_int(host->data.data, &pktlen, 4);
    
    if (pktlen > host->data.length - 4)
	return -1;

    memmove(host->data.data, ((uint8_t *)host->data.data) + 4, host->data.length - 4);
    host->data.length -= 4;

    *data = host->data;
    krb5_data_zero(&host->data);
    
    return 0;
}

/*
 * UDP transport
 */

static krb5_error_code
prepare_udp(krb5_context context, struct host *host, const krb5_data *data)
{
    return krb5_data_copy(&host->data, data->data, data->length);
}

static krb5_error_code
send_udp(krb5_context context, struct host *host)
{
    if (send(host->fd, host->data.data, host->data.length, 0) < 0)
	return errno;
    return 0;
}

static krb5_error_code
recv_udp(krb5_context context, struct host *host, krb5_data *data)
{
    krb5_error_code ret;
    int nbytes;


    if (rk_SOCK_IOCTL(host->fd, FIONREAD, &nbytes) != 0 || nbytes <= 0)
	return HEIM_NET_CONN_REFUSED;

    if (context->max_msg_size < nbytes) {
	krb5_set_error_message(context, KRB5KRB_ERR_FIELD_TOOLONG,
			       N_("UDP message from KDC too large %d", ""),
			       (int)nbytes);
	return KRB5KRB_ERR_FIELD_TOOLONG;
    }

    ret = krb5_data_alloc(data, nbytes);
    if (ret)
	return ret;

    ret = recv(host->fd, data->data, data->length, 0);
    if (ret < 0) {
	ret = errno;
	krb5_data_free(data);
	return ret;
    }
    data->length = ret;

    return 0;
}

/*
 * Host state machine
 */

static int
eval_host_state(krb5_context context, 
		krb5_sendto_ctx ctx,
		struct host *host,
		int readable, int writeable)
{
    krb5_error_code ret;

    if (host->state == CONNECTING && writeable)
	host->state = CONNECTED;

    if (readable) {
	ret = host->recv(context, host, &ctx->response);
	if (ret == -1) {
	    /* not done yet */
	} else if (ret == 0) {
	    /* if recv_foo function returns 0, we have a complete reply */
	    return 1;
	} else {
	    host_dead(context, host, "host disconnected");
	}
    }

    /* check if there is anything to send, state might DEAD after read */
    if (writeable && host->state == CONNECTED) {

	ret = host->send(context, host);
	if (ret == -1) {
	    /* not done yet */
	} else if (ret) {
	    host_dead(context, host, "host dead, write failed");
	} else
	    host->state = WAITING_REPLY;
    }

    return 0;
}

/*
 *
 */

static krb5_error_code
submit_request(krb5_context context, krb5_sendto_ctx ctx, krb5_krbhst_info *hi)
{
    krb5_boolean submitted_host = FALSE, freeai = FALSE;
    krb5_error_code ret;
    struct addrinfo *ai = NULL, *a;
    struct host *host;

    ret = send_via_plugin(context, hi, context->kdc_timeout,
			  ctx->send_data, &ctx->response);
    if (ret == 0) {
	return 0;
    } else if (ret != KRB5_PLUGIN_NO_HANDLE) {
	_krb5_debugx(context, 5, "send via plugin failed %s: %d",
		    hi->hostname, ret);
	return ret;
    }

    /*
     * If we have a proxy, let use the address of the proxy instead of
     * the KDC and let the proxy deal with the resolving of the KDC.
     */

    if (hi->proto == KRB5_KRBHST_HTTP && context->http_proxy) {
	char *proxy2 = strdup(context->http_proxy);
	char *el, *proxy  = proxy2;
	struct addrinfo hints;
	struct addrinfo *ai;
	char portstr[NI_MAXSERV];
	
	if (proxy == NULL)
	    return ENOMEM;
	if (strncmp(proxy, "http://", 7) == 0)
	    proxy += 7;
	
	/* check for url terminating slash */
	el = strchr(proxy, '/');
	if (el != NULL)
	    *el = '\0';

	/* check for port in hostname, used below as port */
	el = strchr(proxy, ':');
	if(el != NULL)
	    *el++ = '\0';

	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(portstr, sizeof(portstr), "%d",
		 ntohs(init_port(el, htons(80))));

	ret = getaddrinfo(proxy, portstr, &hints, &ai);
	free(proxy2);
	if (ret)
	    return krb5_eai_to_heim_errno(ret, errno);
	
	freeai = TRUE;

    } else {
	ret = krb5_krbhst_get_addrinfo(context, hi, &ai);
	if (ret)
	    return ret;
    }

    for (a = ai; a != NULL; a = a->ai_next) {
	rk_socket_t fd;

	fd = socket(a->ai_family, a->ai_socktype | SOCK_CLOEXEC, a->ai_protocol);
	if (rk_IS_BAD_SOCKET(fd))
	    continue;
	rk_cloexec(fd);

#ifndef NO_LIMIT_FD_SETSIZE
	if (fd >= FD_SETSIZE) {
	    _krb5_debugx(context, 0, "fd too large for select");
	    rk_closesocket(fd);
	    continue;
	}
#endif
	socket_set_nonblocking(fd, 1);

	host = heim_alloc(sizeof(*host), "sendto-host", deallocate_host);
	if (host == NULL) {
	    rk_closesocket(fd);
	    return ENOMEM;
	}
	host->hi = hi;
	host->fd = fd;
	host->ai = a;

	if (connect(fd, a->ai_addr, a->ai_addrlen) < 0) {
	    if (errno == EINPROGRESS && (hi->proto == KRB5_KRBHST_HTTP || hi->proto == KRB5_KRBHST_TCP)) {
		_krb5_debugx(context, 5, "connecting to %d", fd);
		host->state = CONNECTING;
	    } else {
		debug_host(context, 5, host, "failed to connect");
		heim_release(host);
		continue;
	    }
	} else {
	    host->state = CONNECTED;
	}
	
	switch (host->hi->proto) {
	case KRB5_KRBHST_HTTP :
	    host->prepare = prepare_http;
	    host->send = send_stream;
	    host->recv = recv_http;
	    host->tries = 1;
	    break;
	case KRB5_KRBHST_TCP :
	    host->prepare = prepare_tcp;
	    host->send = send_stream;
	    host->recv = recv_tcp;
	    host->tries = 1;
	    break;
	case KRB5_KRBHST_UDP :
	    host->prepare = prepare_udp;
	    host->send = send_udp;
	    host->recv = recv_udp;
	    host->tries = 3;
	    break;
	default:
	    heim_abort("undefined http transport protocol: %d", (int)host->hi->proto);
	}

	debug_host(context, 5, host, "connecting to host");

	/*
	 * Now prepare data to send to host
	 */
	if (ctx->prexmit_func) {
	    krb5_data data;
	    
	    krb5_data_zero(&data);

	    ret = ctx->prexmit_func(context, host->hi->proto,
				    ctx->prexmit_ctx, fd, &data);
	    if (ret == 0) {
		if (data.length == 0) {
		    debug_host(context, 5, host, "prexmit hook didn't want to send a packet, "
			       "but also didn't want to abort the tries, just skip this host");
		    heim_release(host);
		    continue;
		}
		ret = host->prepare(context, host, &data);
		krb5_data_free(&data);
	    }

	} else {
	    ret = host->prepare(context, host, ctx->send_data);
	}
	if (ret) {
	    debug_host(context, 5, host, "failed to prexmit/prepare");
	    heim_release(host);
	    /*
	     * Prepare/prexmit hooks are fatal
	     */
	    return ret;
	}

	host->timeout = time(NULL) + context->kdc_timeout;

	heim_array_append_value(ctx->hosts, host);

	if (host->state == CONNECTED)
	    eval_host_state(context, ctx, host, 0, 1);

	heim_release(host);

	submitted_host = TRUE;
    }

    if (freeai)
	freeaddrinfo(ai);

    if (!submitted_host)
	return KRB5_KDC_UNREACH;

    return 0;
}

static krb5_error_code
wait_response(krb5_context context, int *action, krb5_sendto_ctx ctx)
{
    __block fd_set rfds, wfds;
    __block unsigned max_fd = 0;
    __block int ret;
    struct timeval tv;
    time_t timenow;

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

    /* oh, we have a reply, it must be a plugin that got it for us */
    if (ctx->response.length) {
	*action = KRB5_SENDTO_FILTER;
	return 0;
    }

    timenow = time(NULL);

    heim_array_iterate(ctx->hosts, ^(heim_object_t obj, int *stop) {
	    struct host *h = (struct host *)obj;

	    /* skip dead hosts */
	    if (h->state == DEAD)
		return;

	    /* if host timed out, dec tries and (retry or kill host) */
	    if (h->timeout < timenow) {
		heim_assert(h->tries != 0, "tries should not reach 0");
		h->tries--;
		if (h->tries == 0) {
		    host_dead(context, h, "host timed out");
		    return;
		} else {
		    debug_host(context, 5, h, "retrying sending to");
		    h->state = CONNECTED;
		}
	    }

#ifndef NO_LIMIT_FD_SETSIZE
	    heim_assert(h->fd < FD_SETSIZE, "fd too large");
#endif
	    switch (h->state) {
	    case WAITING_REPLY:
		FD_SET(h->fd, &rfds);
		break;
	    case CONNECTING:
	    case CONNECTED:
		FD_SET(h->fd, &rfds);
		FD_SET(h->fd, &wfds);
		break;
	    default:
		heim_abort("invalid sendto host state");
	    }
	    if (h->fd > max_fd)
		max_fd = h->fd;
	});

    heim_array_filter(ctx->hosts, ^(heim_object_t obj) {
	    struct host *h = (struct host *)obj;
	    return (bool)((h->state == DEAD) ? true : false);
	});

    if (heim_array_get_length(ctx->hosts) == 0) {
	if (ctx->stateflags & KRBHST_COMPLETED) {
	    _krb5_debugx(context, 5, "no more hosts to send/recv packets to/from "
			 "trying to pulling more hosts");
	    *action = KRB5_SENDTO_FAILED;
	} else {
	    _krb5_debugx(context, 5, "no more hosts to send/recv packets to/from "
			 "and no more hosts -> failure");
	    *action = KRB5_SENDTO_TIMEOUT;
	}
	return 0;
    }

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    ret = select(max_fd + 1, &rfds, &wfds, NULL, &tv);
    if (ret < 0)
	return errno;
    if (ret == 0) {
	*action = KRB5_SENDTO_TIMEOUT;
	return 0;
    }

    ret = 0;
    heim_array_iterate(ctx->hosts, ^(heim_object_t obj, int *stop) {
	    struct host *h = (struct host *)obj;
	    int readable, writeable;
	    heim_assert(h->state != DEAD, "dead host resurected");

#ifndef NO_LIMIT_FD_SETSIZE
	    heim_assert(h->fd < FD_SETSIZE, "fd too large");
#endif
	    readable = FD_ISSET(h->fd, &rfds);
	    writeable = FD_ISSET(h->fd, &wfds);

	    if (readable || writeable)
		ret |= eval_host_state(context, ctx, h, readable, writeable);

	    /* if there is already a reply, just fall though the array */
	    if (ret)
		*stop = 1;
	});
    if (ret)
	*action = KRB5_SENDTO_FILTER;
    else
	*action = KRB5_SENDTO_CONTINUE;

    return 0;
}

static void
reset_context(krb5_context context, krb5_sendto_ctx ctx)
{
    krb5_data_free(&ctx->response);
    heim_release(ctx->hosts);
    ctx->hosts = heim_array_create();
    ctx->stateflags = 0;
}


/*
 *
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_sendto_context(krb5_context context,
		    krb5_sendto_ctx ctx,
		    const krb5_data *send_data,
		    const krb5_realm realm,
		    krb5_data *receive)
{
    krb5_error_code ret;
    krb5_krbhst_handle handle = NULL;
    int type, freectx = 0;
    int action;
    int numreset = 0;

    krb5_data_zero(receive);
    
    HEIM_WARN_BLOCKING("krb5_sendto_context", warn_once);

    if (ctx == NULL) {
	ret = krb5_sendto_ctx_alloc(context, &ctx);
	if (ret)
	    goto out;
	freectx = 1;
    }

    type = ctx->type;
    if (type == 0) {
	if ((ctx->flags & KRB5_KRBHST_FLAGS_MASTER) || context->use_admin_kdc)
	    type = KRB5_KRBHST_ADMIN;
	else
	    type = KRB5_KRBHST_KDC;
    }

    ctx->send_data = send_data;

    if (send_data->length > context->large_msg_size)
	ctx->flags |= KRB5_KRBHST_FLAGS_LARGE_MSG;

    /* loop until we get back a appropriate response */

    action = KRB5_SENDTO_INITIAL;

    while (action != KRB5_SENDTO_DONE && action != KRB5_SENDTO_FAILED) {
	krb5_krbhst_info *hi;

	switch (action) {
	case KRB5_SENDTO_INITIAL:
	    ret = krb5_krbhst_init_flags(context, realm, type,
					 ctx->flags, &handle);
	    if (ret)
		goto out;

	    if (ctx->hostname) {
		ret = krb5_krbhst_set_hostname(context, handle, ctx->hostname);
		if (ret)
		    goto out;
	    }
	    action = KRB5_SENDTO_TIMEOUT;
	    /* FALLTHOUGH */
	case KRB5_SENDTO_TIMEOUT:

	    /*
	     * If we completed, just got to next step
	     */

	    if (ctx->stateflags & KRBHST_COMPLETED) {
		action = KRB5_SENDTO_CONTINUE;
		break;
	    }

	    /*
	     * Pull out next host, if there is no more, close the
	     * handle and mark as completed.
	     */

	    ret = krb5_krbhst_next(context, handle, &hi);
	    action = KRB5_SENDTO_CONTINUE;
	    if (ret == 0) {
		if (submit_request(context, ctx, hi) != 0)
		    action = KRB5_SENDTO_TIMEOUT;
	    } else {
		ctx->stateflags |= KRBHST_COMPLETED;
	    }

	    break;
	case KRB5_SENDTO_CONTINUE:

	    ret = wait_response(context, &action, ctx);
	    if (ret)
		goto out;

	    break;
	case KRB5_SENDTO_RESET:
	    /* start over */
	    _krb5_debugx(context, 5,
			"krb5_sendto trying over again (reset): %d",
			numreset);
	    reset_context(context, ctx);
	    if (handle) {
		krb5_krbhst_free(context, handle);
		handle = NULL;
	    }
	    numreset++;
	    if (numreset >= 3)
		action = KRB5_SENDTO_FAILED;
	    else
		action = KRB5_SENDTO_INITIAL;

	    break;
	case KRB5_SENDTO_FILTER:
	    /* default to next state, the filter function might modify this */
	    action = KRB5_SENDTO_DONE;

	    if (ctx->func) {
		ret = (*ctx->func)(context, ctx, ctx->data,
				   &ctx->response, &action);
		if (ret)
		    goto out;
	    }
	    break;
	case KRB5_SENDTO_FAILED:
	    ret = KRB5_KDC_UNREACH;
	    break;
	case KRB5_SENDTO_DONE:
	    ret = 0;
	    break;
	default:
	    heim_abort("invalid krb5_sendto_context state");
	}
    }

 out:
    if (ret == 0 && ctx->response.length) {
	*receive = ctx->response;
	krb5_data_zero(&ctx->response);
    } else {
	krb5_data_free(&ctx->response);
	krb5_clear_error_message (context);
	ret = KRB5_KDC_UNREACH;
	krb5_set_error_message(context, ret,
			       N_("unable to reach any KDC in realm %s", ""),
			       realm);
    }

    _krb5_debugx(context, 5, "krb5_sendto_context done: %d", ret);

    if (freectx)
	krb5_sendto_ctx_free(context, ctx);
    else
	reset_context(context, ctx);

    if (handle)
	krb5_krbhst_free(context, handle);

    return ret;
}
