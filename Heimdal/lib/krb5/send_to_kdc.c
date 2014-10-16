/*
 * Copyright (c) 1997 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 - 2013 Apple Inc. All rights reserved.
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

/**
 * @section send_to_kdc Locating and sending packets to the KDC
 *
 * The send to kdc code is responsible to request the list of KDC from
 * the locate-kdc subsystem and then send requests to each of them.
 *
 * - Each second a new hostname is tried.
 * - If the hostname have several addresses, the first will be tried
 *   directly then in turn the other will be tried every 3 seconds
 *   (host_timeout).
 * - UDP requests are tried 3 times (ntries), and it tried with a individual timeout of kdc_timeout / ntries.
 * - TCP and HTTP requests are tried 1 time.
 *
 *  Total wait time is (number of addresses * 3) + kdc_timeout seconds.
 *
 */

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

struct send_via_plugin_s {
    krb5_const_realm realm;
    krb5_krbhst_info *hi;
    time_t timeout;
    const krb5_data *send_data;
    krb5_data *receive;
};
    

static krb5_error_code KRB5_LIB_CALL
kdccallback(krb5_context context, const void *plug, void *plugctx, void *userctx)
{
    const krb5plugin_send_to_kdc_ftable *service = (const krb5plugin_send_to_kdc_ftable *)plug;
    struct send_via_plugin_s *ctx = userctx;

    if (service->send_to_kdc == NULL)
	return KRB5_PLUGIN_NO_HANDLE;
    return service->send_to_kdc(context, plugctx, ctx->hi, ctx->timeout,
				ctx->send_data, ctx->receive);
}

static krb5_error_code KRB5_LIB_CALL
realmcallback(krb5_context context, const void *plug, void *plugctx, void *userctx)
{
    const krb5plugin_send_to_kdc_ftable *service = (const krb5plugin_send_to_kdc_ftable *)plug;
    struct send_via_plugin_s *ctx = userctx;

    if (service->send_to_realm == NULL)
	return KRB5_PLUGIN_NO_HANDLE;
    return service->send_to_realm(context, plugctx, ctx->realm, ctx->timeout,
				  ctx->send_data, ctx->receive);
}

static krb5_error_code
kdc_via_plugin(krb5_context context,
	       krb5_krbhst_info *hi,
	       time_t timeout,
	       const krb5_data *send_data,
	       krb5_data *receive)
{
    struct send_via_plugin_s userctx;

    userctx.realm = NULL;
    userctx.hi = hi;
    userctx.timeout = timeout;
    userctx.send_data = send_data;
    userctx.receive = receive;

    return krb5_plugin_run_f(context, "krb5", KRB5_PLUGIN_SEND_TO_KDC,
			     KRB5_PLUGIN_SEND_TO_KDC_VERSION_0, 0,
			     &userctx, kdccallback);
}

static krb5_error_code
realm_via_plugin(krb5_context context,
		 krb5_const_realm realm,
		 time_t timeout,
		 const krb5_data *send_data,
		 krb5_data *receive)
{
    struct send_via_plugin_s userctx;

    userctx.realm = realm;
    userctx.hi = NULL;
    userctx.timeout = timeout;
    userctx.send_data = send_data;
    userctx.receive = receive;

    return krb5_plugin_run_f(context, "krb5", KRB5_PLUGIN_SEND_TO_KDC,
			     KRB5_PLUGIN_SEND_TO_KDC_VERSION_2, 0,
			     &userctx, realmcallback);
}

struct krb5_sendto_ctx_data {
    struct heim_base_uniq base;
    int flags;
    int type;
    krb5_sendto_ctx_func func;
    void *data;
    char *hostname;
    krb5_krbhst_handle krbhst;

    /* context2 */
    const krb5_data *send_data;
    krb5_data response;
    heim_array_t hosts;
    const char *realm;
    int stateflags;
#define KRBHST_COMPLETED	1

    /* prexmit */
    krb5_sendto_prexmit prexmit_func;
    void *prexmit_ctx;

    /* stats */
    struct {
	struct timeval start_time;
	struct timeval name_resolution;
	struct timeval krbhst;
	unsigned long sent_packets;
	unsigned long num_hosts;
    } stats;
    unsigned int stid;
};

static void
dealloc_sendto_ctx(void *ptr)
{
    krb5_sendto_ctx ctx = (krb5_sendto_ctx)ptr;
    if (ctx->hostname)
	free(ctx->hostname);
    heim_release(ctx->hosts);
    heim_release(ctx->krbhst);
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_sendto_ctx_alloc(krb5_context context, krb5_sendto_ctx *ctx)
{
    *ctx = heim_uniq_alloc(sizeof(**ctx), "sendto-context", dealloc_sendto_ctx);
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
_krb5_sendto_ctx_set_krb5hst(krb5_context context,
			     krb5_sendto_ctx ctx,
			     krb5_krbhst_handle handle)
{
    heim_release(ctx->krbhst);
    ctx->krbhst = heim_retain(handle);
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_sendto_ctx_free(krb5_context context, krb5_sendto_ctx ctx)
{
    heim_release(ctx);
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

struct host;

struct host_fun {
    krb5_error_code (*prepare)(krb5_context, struct host *, const krb5_data *);
    krb5_error_code (*send)(krb5_context, struct host *);
    krb5_error_code (*recv)(krb5_context, struct host *, krb5_data *);
    int ntries;
};

struct host {
    struct heim_base_uniq base;
    krb5_sendto_ctx ctx;
    enum host_state { CONNECT, CONNECTING, CONNECTED, WAITING_REPLY, DEAD } state;
    krb5_krbhst_info *hi;
    struct addrinfo *ai;
    rk_socket_t fd;
    rk_socket_t fd2;
    struct host_fun *fun;
    unsigned int tries;
    time_t timeout;
    krb5_data data;
    unsigned int tid;
};

static void
debug_host(krb5_context context, int level, struct host *host, const char *fmt, ...)
	__attribute__((__format__(__printf__, 4, 5)));

static void
debug_host(krb5_context context, int level, struct host *host, const char *fmt, ...)
{
    const char *proto = "unknown";
    char name[NI_MAXHOST], port[NI_MAXSERV];
    char *text = NULL;
    va_list ap;
    int ret;

    if (!_krb5_have_debug(context, 5))
	return;

    va_start(ap, fmt);
    ret = vasprintf(&text, fmt, ap);
    va_end(ap);
    if (ret == -1 || text == NULL)
	return;

    if (host->hi->proto == KRB5_KRBHST_HTTP)
	proto = "http";
    else if (host->hi->proto == KRB5_KRBHST_TCP)
	proto = "tcp";
    else if (host->hi->proto == KRB5_KRBHST_UDP)
	proto = "udp";
    else if (host->hi->proto == KRB5_KRBHST_KKDCP)
	proto = "kkdcp";

    if (host->ai == NULL ||
	getnameinfo(host->ai->ai_addr, host->ai->ai_addrlen,
		    name, sizeof(name), port, sizeof(port), NI_NUMERICHOST) != 0)
    {
	name[0] = '\0';
	port[0] = '\0';
    }

    _krb5_debugx(context, level, "%s: %s %s:%s (%s) tid: %08x",
		 text, proto, name, port, host->hi->hostname, host->tid);
    free(text);
}


static void
deallocate_host(void *ptr)
{
    struct host *host = ptr;
    if (!rk_IS_BAD_SOCKET(host->fd))
	rk_closesocket(host->fd);
    if (!rk_IS_BAD_SOCKET(host->fd2))
	rk_closesocket(host->fd2);
    krb5_data_free(&host->data);
    host->ai = NULL;
}

static void
host_dead(krb5_context context, struct host *host, const char *msg)
{
    debug_host(context, 5, host, "%s", msg);
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
    else if ((size_t)len < host->data.length) {
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

    if (rk_SOCK_IOCTL(host->fd, FIONREAD, &nbytes) != 0 || nbytes <= 0) {
	debug_host(context, 5, host, "failed to get nbytes from socket, no bytes there?");
	return HEIM_NET_CONN_REFUSED;
    }

    if (context->max_msg_size - host->data.length < (size_t)nbytes) {
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
	debug_host(context, 5, host, "failed to read bytes from stream: %d", ret);
	return ret;
    }
    host->data.length = oldlen + sret;
    /* zero terminate for http transport */
    ((uint8_t *)host->data.data)[host->data.length] = '\0';

    return 0;
}

/*
 *
 */

static krb5_error_code
send_kkdcp(krb5_context context, struct host *host)
{
#ifdef __APPLE__
    dispatch_queue_t q;
    char *url = NULL;
    char *path = host->hi->path;
    __block krb5_data data;

    q = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);

    heim_retain(host);
    heim_retain(host->ctx);
    
    if (path == NULL)
	path = "";
 
    if (host->hi->def_port != host->hi->port)
	asprintf(&url, "https://%s:%d/%s", host->hi->hostname, host->hi->port, path);
    else
	asprintf(&url, "https://%s/%s", host->hi->hostname, path);
    if (url == NULL)
	return ENOMEM;
    
    data = host->data;
    krb5_data_zero(&host->data);

    debug_host(context, 5, host, "sending request to: %s", url);

    heim_retain(context);

    dispatch_async(q, ^{
	    krb5_error_code ret;
	    krb5_data retdata;
 
 	    krb5_data_zero(&retdata);
 
	    ret = _krb5_kkdcp_request(context, host->ctx->realm, url, 
				      &data, &retdata);
	    krb5_data_free(&data);
 	    free(url);
 	    if (ret == 0) {
		uint8_t length[4];

		debug_host(context, 5, host, "kkdcp: got %d bytes, feeding them back", (int)retdata.length);

		_krb5_put_int(length, retdata.length, 4);
		krb5_net_write_block(context, &host->fd2, length, sizeof(length), 2);
		krb5_net_write_block(context, &host->fd2, retdata.data, retdata.length, 2);
	    }
 
 	    close(host->fd2);
	    host->fd2 = -1;
	    heim_release(host->ctx);
 	    heim_release(host);
	    heim_release(context);
 	});
     return 0;
#else
     close(host->fd2);
     host->fd2 = -1;
#endif
}

/*
 *
 */

static void
host_next_timeout(krb5_context context, struct host *host)
{
    host->timeout = context->kdc_timeout / host->fun->ntries;
    if (host->timeout == 0)
	host->timeout = 1;

    host->timeout += time(NULL);
}

/*
 * connected host
 */

static void
host_connected(krb5_context context, krb5_sendto_ctx ctx, struct host *host)
{
    krb5_error_code ret;

    host->state = CONNECTED; 
    /*
     * Now prepare data to send to host
     */
    if (ctx->prexmit_func) {
	krb5_data data;
	    
	krb5_data_zero(&data);

	ret = ctx->prexmit_func(context, host->hi->proto,
				ctx->prexmit_ctx, host->fd, &data);
	if (ret == 0) {
	    if (data.length == 0) {
		host_dead(context, host, "prexmit function didn't send data");
		return;
	    }
	    ret = host->fun->prepare(context, host, &data);
	    krb5_data_free(&data);
	}
	
    } else {
	ret = host->fun->prepare(context, host, ctx->send_data);
    }
    if (ret)
	debug_host(context, 5, host, "failed to prexmit/prepare");
}

/*
 * connect host
 */

static void
host_connect(krb5_context context, krb5_sendto_ctx ctx, struct host *host)
{
    krb5_krbhst_info *hi = host->hi;
    struct addrinfo *ai = host->ai;

    debug_host(context, 5, host, "connecting to host");

    if (connect(host->fd, ai->ai_addr, ai->ai_addrlen) < 0) {
	if (errno == EINPROGRESS && (hi->proto == KRB5_KRBHST_HTTP || hi->proto == KRB5_KRBHST_TCP)) {
	    debug_host(context, 5, host, "connecting to %d", host->fd);
	    host->state = CONNECTING;
	} else {
	    host_dead(context, host, "failed to connect");
	}
    } else {
	host_connected(context, ctx, host);
    }

    host_next_timeout(context, host);
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

    heim_assert(host->data.length == 0, "prepare_http called twice");

    len = base64_encode(data->data, (int)data->length, &str);
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

    heim_assert(host->data.length == 0, "prepare_tcp called twice");

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
    ssize_t sret;

    if (rk_SOCK_IOCTL(host->fd, FIONREAD, &nbytes) != 0 || nbytes <= 0) {
	debug_host(context, 5, host, "failed to get nbytes from socket, no bytes there?");
	return HEIM_NET_CONN_REFUSED;
    }

    if (nbytes > context->max_msg_size) {
	debug_host(context, 5, host, "server sent too large message %d (max is %d)",
		   (int)nbytes, (int)context->max_msg_size);
	krb5_set_error_message(context, KRB5KRB_ERR_FIELD_TOOLONG,
			       N_("UDP message from KDC too large %d", ""),
			       (int)nbytes);
	return KRB5KRB_ERR_FIELD_TOOLONG;
    }

    ret = krb5_data_alloc(data, nbytes);
    if (ret)
	return ret;

    sret = recv(host->fd, data->data, data->length, 0);
    if (sret < 0) {
	debug_host(context, 5, host, "read data from nbytes from host: %d", errno);
	ret = errno;
	krb5_data_free(data);
	return ret;
    }
    data->length = sret;

    return 0;
}

static struct host_fun http_fun = {
    prepare_http,
    send_stream,
    recv_http,
    1
};
static struct host_fun kkdcp_fun = {
    prepare_udp,
    send_kkdcp,
    recv_tcp,
    1
};
static struct host_fun tcp_fun = {
    prepare_tcp,
    send_stream,
    recv_tcp,
    1
};
static struct host_fun udp_fun = {
    prepare_udp,
    send_udp,
    recv_udp,
    3
};


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
	host_connected(context, ctx, host);

    if (readable) {

	debug_host(context, 5, host, "reading packet");

	ret = host->fun->recv(context, host, &ctx->response);
	if (ret == -1) {
	    /* not done yet */
	} else if (ret == 0) {
	    /* if recv_foo function returns 0, we have a complete reply */
	    debug_host(context, 5, host, "host completed");
	    return 1;
	} else {
	    host_dead(context, host, "host disconnected");
	}
    }

    /* check if there is anything to send, state might DEAD after read */
    if (writeable && host->state == CONNECTED) {

	ctx->stats.sent_packets++;

	debug_host(context, 5, host, "writing packet");

	ret = host->fun->send(context, host);
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

static struct host *
host_create(krb5_context context,
	    krb5_sendto_ctx ctx,
	    krb5_krbhst_info *hi,
	    struct addrinfo *ai,
	    int fd)
{
    struct host *host;

    host = heim_uniq_alloc(sizeof(*host), "sendto-host", deallocate_host);
    if (host == NULL)
	    return ENOMEM;

    host->hi = hi;
    host->fd = fd;
    host->fd2 = -1;
    host->ai = ai;
    host->ctx = ctx;
    /* next version of stid */
    host->tid = ctx->stid = (ctx->stid & 0xffff0000) | ((ctx->stid & 0xffff) + 1);

    host->state = CONNECT;

    switch (host->hi->proto) {
    case KRB5_KRBHST_HTTP :
	host->fun = &http_fun;
	break;
    case KRB5_KRBHST_KKDCP :
	host->fun = &kkdcp_fun;
	break;
    case KRB5_KRBHST_TCP :
	host->fun = &tcp_fun;
	break;
    case KRB5_KRBHST_UDP :
	host->fun = &udp_fun;
	break;
    }

    host->tries = host->fun->ntries;
    
    heim_array_append_value(ctx->hosts, host);
	
    return host;
}


/*
 *
 */

static krb5_error_code
submit_request(krb5_context context, krb5_sendto_ctx ctx, krb5_krbhst_info *hi)
{
    unsigned long submitted_host = 0;
    krb5_boolean freeai = FALSE;
    struct timeval nrstart, nrstop;
    krb5_error_code ret;
    struct addrinfo *ai = NULL, *a;
    struct host *host;

    ret = kdc_via_plugin(context, hi, context->kdc_timeout,
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

    gettimeofday(&nrstart, NULL);

    if (hi->proto == KRB5_KRBHST_HTTP && context->http_proxy) {
	char *proxy2 = strdup(context->http_proxy);
	char *el, *proxy  = proxy2;
	struct addrinfo hints;
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
    } else if (hi->proto == KRB5_KRBHST_KKDCP) {
	ai = NULL;
    } else {
	ret = krb5_krbhst_get_addrinfo(context, hi, &ai);
	if (ret)
	    return ret;
    }

    /* add up times */
    gettimeofday(&nrstop, NULL);
    timevalsub(&nrstop, &nrstart);
    timevaladd(&ctx->stats.name_resolution, &nrstop);

    ctx->stats.num_hosts++;

    for (a = ai; a != NULL; a = a->ai_next) {
	rk_socket_t fd;

	fd = socket(a->ai_family, a->ai_socktype | SOCK_CLOEXEC, a->ai_protocol);
	if (rk_IS_BAD_SOCKET(fd))
	    continue;
	rk_cloexec(fd);
	socket_set_nopipe(fd, 1);
	socket_set_nonblocking(fd, 1);

#ifndef NO_LIMIT_FD_SETSIZE
	if (fd >= FD_SETSIZE) {
	    _krb5_debugx(context, 0, "fd too large for select");
	    rk_closesocket(fd);
	    continue;
	}
#endif
	host = host_create(context, ctx, hi, a, fd);
	if (host == NULL) {
	    rk_closesocket(fd);
	    continue;
	}

	/*
	 * Connect directly next host, wait a host_timeout for each next address
	 */
	if (submitted_host == 0)
	    host_connect(context, ctx, host);
	else {
	    debug_host(context, 5, host,
		       "Queuing host in future (in %ds), "
		       "its the %lu address on the same name",
		       (int)(context->host_timeout * submitted_host),
		       submitted_host + 1);
	    host->timeout = time(NULL) + (submitted_host * context->host_timeout);
	}

	submitted_host++;
	heim_release(host);
    }

    if (hi->proto == KRB5_KRBHST_KKDCP) {
	int fds[2];

	heim_assert(ai == NULL, "kkdcp host with ai ?");

	if (socketpair(PF_LOCAL, SOCK_STREAM, 0, fds) < 0)
	    return KRB5_KDC_UNREACH;

	socket_set_nopipe(fds[0], 1);
	socket_set_nopipe(fds[1], 1);
	socket_set_nonblocking(fds[0], 1);
	socket_set_nonblocking(fds[1], 1);

	host = host_create(context, ctx, hi, NULL, fds[0]);
	if (host == NULL) {
	    close(fds[0]);
	    close(fds[1]);
	    return ENOMEM;
	}
	host->fd2 = fds[1];

	host_next_timeout(context, host);
	host_connected(context, ctx, host);

	submitted_host++;
	heim_release(host);
    }


    if (freeai)
	freeaddrinfo(ai);

    if (!submitted_host)
	return KRB5_KDC_UNREACH;

    return 0;
}

static void
set_fd_status(struct host *h, fd_set *rfds, fd_set *wfds, int *max_fd)
{
#ifndef NO_LIMIT_FD_SETSIZE
    heim_assert(h->fd < FD_SETSIZE, "fd too large");
#endif
    switch (h->state) {
    case WAITING_REPLY:
	FD_SET(h->fd, rfds);
	break;
    case CONNECTING:
    case CONNECTED:
	FD_SET(h->fd, rfds);
	FD_SET(h->fd, wfds);
	break;
    case DEAD:
    case CONNECT:
	break;
    }
    if (h->fd > *max_fd)
	*max_fd = h->fd + 1;
}



static krb5_error_code
wait_response(krb5_context context, int *action, krb5_sendto_ctx ctx)
{
    __block struct host *next_pending = NULL;
    __block fd_set rfds, wfds;
    __block int max_fd = 0;
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

	    /*
	     * process submitted by pending hosts here
	     */
	    if (h->state == CONNECT) {
		if (h->timeout < timenow) {
		    host_connect(context, ctx, h);
		} else if (next_pending == NULL || next_pending->timeout > h->timeout) {
		    next_pending = h;
		    return;
		} else {
		    return;
		}
	    }

	    /* if host timed out, dec tries and (retry or kill host) */
	    if (h->timeout < timenow) {
		heim_assert(h->tries != 0, "tries should not reach 0");
		h->tries--;
		if (h->tries == 0) {
		    host_dead(context, h, "host timed out");
		    return;
		} else {
		    debug_host(context, 5, h, "retrying sending to");
		    host_next_timeout(context, h);
		    host_connected(context, ctx, h);
		}
	    }

	    set_fd_status(h, &rfds, &wfds, &max_fd);
	});

    /*
     * We have no host to wait for, but there is one pending, lets
     * kick that one off.
     */
    if (max_fd == 0 && next_pending) {
	time_t forward = next_pending->timeout - timenow;

	host_connect(context, ctx, next_pending);
	set_fd_status(next_pending, &rfds, &wfds, &max_fd);

	/* 
	 * Move all waiting host forward in time too, only if the next
	 * host didn't happen the same about the same time as the last
	 * expiration
	*/
	if (forward > 0) {
	    heim_array_iterate(ctx->hosts, ^(heim_object_t obj, int *stop) {
		    struct host *h = (struct host *)obj;
		    
		    if (h->state != CONNECT)
			return;
		    h->timeout -= forward;
		    if (h->timeout < timenow)
			h->timeout = timenow;
		});
	}
    }

    heim_array_filter(ctx->hosts, ^(heim_object_t obj) {
	    struct host *h = (struct host *)obj;
	    return (int)((h->state == DEAD) ? true : false);
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
    if (ret < 0) {
	if (errno != EAGAIN || errno != EINTR)
	    return errno;
	ret = 0;
    }
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
		    krb5_const_realm realm,
		    krb5_data *receive)
{
    krb5_error_code ret = KRB5_KDC_UNREACH;
    krb5_krbhst_handle handle = NULL;
    struct timeval nrstart, nrstop, stop_time;
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

    memset(&ctx->stats, 0, sizeof(ctx->stats));
    gettimeofday(&ctx->stats.start_time, NULL);

    ctx->realm = realm;
    ctx->stid = (context->num_kdc_requests++) << 16;

    type = ctx->type;
    if (type == 0) {
	if ((ctx->flags & KRB5_KRBHST_FLAGS_MASTER) || context->use_admin_kdc)
	    type = KRB5_KRBHST_ADMIN;
	else
	    type = KRB5_KRBHST_KDC;
    }

    ctx->send_data = send_data;

    if ((int)send_data->length > context->large_msg_size)
	ctx->flags |= KRB5_KRBHST_FLAGS_LARGE_MSG;

    /* loop until we get back a appropriate response */

    action = KRB5_SENDTO_INITIAL;

    while (action != KRB5_SENDTO_DONE && action != KRB5_SENDTO_FAILED) {
	krb5_krbhst_info *hi;

	switch (action) {
	case KRB5_SENDTO_INITIAL:
	    ret = realm_via_plugin(context, realm, context->kdc_timeout,
				   send_data, receive);
	    if (ret == 0 || ret != KRB5_PLUGIN_NO_HANDLE) {
		action = KRB5_SENDTO_DONE;
		break;
	    }
	    action = KRB5_SENDTO_KRBHST;
	    /* FALLTHOUGH */
	case KRB5_SENDTO_KRBHST:
	    if (ctx->krbhst == NULL) {
		ret = krb5_krbhst_init_flags(context, realm, type,
					     ctx->flags, &handle);
		if (ret)
		    goto out;

		if (ctx->hostname) {
		    ret = krb5_krbhst_set_hostname(context, handle, ctx->hostname);
		    if (ret)
			goto out;
		}
	    } else {
		handle = heim_retain(ctx->krbhst);
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
	     *
	     * Collect time spent in krbhst (dns, plugin, etc)
	     */


	    gettimeofday(&nrstart, NULL);

	    ret = krb5_krbhst_next(context, handle, &hi);

	    gettimeofday(&nrstop, NULL);
	    timevalsub(&nrstop, &nrstart);
	    timevaladd(&ctx->stats.krbhst, &nrstop);

	    action = KRB5_SENDTO_CONTINUE;
	    if (ret == 0) {
		_krb5_debugx(context, 5, "submissing new requests to new host");
		if (submit_request(context, ctx, hi) != 0)
		    action = KRB5_SENDTO_TIMEOUT;
	    } else {
		_krb5_debugx(context, 5, "out of hosts, waiting for replies");
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
		action = KRB5_SENDTO_KRBHST;

	    ret = 0;
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
	    heim_abort("invalid krb5_sendto_context action: %d", (int)action);
	}
    }

 out:
    gettimeofday(&stop_time, NULL);
    timevalsub(&stop_time, &ctx->stats.start_time);
    
    if (ret == 0 && ctx->response.length) {
	*receive = ctx->response;
	krb5_data_zero(&ctx->response);
    } else {
	krb5_data_free(&ctx->response);
	krb5_clear_error_message (context);
	ret = KRB5_KDC_UNREACH;
	krb5_set_error_message(context, ret,
			       N_("unable to reach any KDC in realm %s, tried %lu %s", ""),
			       realm, ctx->stats.num_hosts,
			       ctx->stats.num_hosts == 1 ? "KDC" : "KDCs");
    }

    _krb5_debugx(context, 1,
		 "krb5_sendto_context %s done: %d hosts %lu packets %lu wc: %ld.%06d nr: %ld.%06d kh: %ld.%06d tid: %08x",
		 ctx->realm, ret,
		 ctx->stats.num_hosts, ctx->stats.sent_packets,
		 stop_time.tv_sec, stop_time.tv_usec,
		 ctx->stats.name_resolution.tv_sec, ctx->stats.name_resolution.tv_usec,
		 ctx->stats.krbhst.tv_sec, ctx->stats.krbhst.tv_usec, ctx->stid);


    if (freectx)
	krb5_sendto_ctx_free(context, ctx);
    else
	reset_context(context, ctx);

    if (handle)
	krb5_krbhst_free(context, handle);

    return ret;
}
