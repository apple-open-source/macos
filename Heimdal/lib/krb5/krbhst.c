/*
 * Copyright (c) 2001 - 2003 Kungliga Tekniska Högskolan
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
#include <resolve.h>
#include "locate_plugin.h"
#if __APPLE__
#include <dns_sd.h>
#include <dns_sd_private.h>
#endif

static void append_host_hostinfo(krb5_context, struct krb5_krbhst_data *, struct krb5_krbhst_info *);

struct krb5_krbhst_data {
    struct heim_base_uniq base;
    HEIMDAL_MUTEX mutex;
    char *realm;
    unsigned int flags;
    int def_port;
    int port;			/* hardwired port number if != 0 */
#define KD_CONFIG		0x0001
#define KD_SRV_UDP		0x0002
#define KD_SRV_TCP		0x0004
#define KD_SITE_SRV_UDP		0x0008
#define KD_SITE_SRV_TCP		0x0010
#define KD_SRV_HTTP		0x0020
#define KD_SRV_KKDCP		0x0040
#define KD_FALLBACK		0x0080
#define KD_CONFIG_EXISTS	0x0100
#define KD_LARGE_MSG		0x0200
#define KD_PLUGIN		0x0400
#define KD_HOSTNAMES		0x0800
#define KD_DELEG_UUID		0x1000
    krb5_error_code (*get_next)(krb5_context, struct krb5_krbhst_data *,
				krb5_krbhst_info**);

    char *hostname;
    char *sitename;
    krb5_uuid delegate_uuid;
    unsigned int fallback_count;

    struct krb5_krbhst_info *hosts, **index, **end;

    krb5_context context;
    heim_queue_t srv_queue;
    heim_queue_t addrinfo_queue;
#if __APPLE__
    DNSServiceRef main_sd;
#endif
    void (*callback)(void *, krb5_krbhst_info *);
    void *userctx;
};

static int
string_to_proto(const char *string)
{
    if(strcasecmp(string, "udp") == 0)
	return KRB5_KRBHST_UDP;
    else if(strcasecmp(string, "tcp") == 0)
	return KRB5_KRBHST_TCP;
    else if(strcasecmp(string, "http") == 0)
	return KRB5_KRBHST_HTTP;
    else if(strcasecmp(string, "kkdcp") == 0)
	return KRB5_KRBHST_KKDCP;
    return -1;
}

/*
 * create a getaddrinfo `hints' based on `proto'
 */

static void
make_hints(struct addrinfo *hints, int proto)
{
    memset(hints, 0, sizeof(*hints));
    hints->ai_family = AF_UNSPEC;
    switch(proto) {
	case KRB5_KRBHST_UDP :
	    hints->ai_socktype = SOCK_DGRAM;
	    break;
	case KRB5_KRBHST_KKDCP :
	case KRB5_KRBHST_HTTP :
	case KRB5_KRBHST_TCP :
	    hints->ai_socktype = SOCK_STREAM;
	    break;
    }
}

static void
query_release(void *ctx)
{
    struct _krb5_srv_query_ctx *query = (struct _krb5_srv_query_ctx *)ctx;

    free(query->domain);
    if (query->sema)
	dispatch_release((dispatch_semaphore_t)query->sema);
#ifdef __APPLE__
    if (query->array) {
	size_t n;
	for (n = 0; n < query->len; n++) {

	    /*
	     * check if the call is still pending, if it is, cancel it and
	     * clean up the reference that the callback was supposed
	     * to clean
	     */
	    dispatch_sync((dispatch_queue_t)query->handle->addrinfo_queue, ^{
		if (query->array[n]->query_sd) {
		    DNSServiceRefDeallocate(query->array[n]->query_sd);
		    query->array[n]->query_sd = NULL;

		    if (!query->array[n]->flags.getAddrDone) {
			query->array[n]->flags.getAddrDone = true;
			heim_release(query->array[n]);
		    }
		}
	    });
	    heim_release(query->array[n]);
	}
	free(query->array);
    }
#endif
    heim_release(query->handle);
}

#ifdef __APPLE__

static int
compare_srv(const void *a, const void *b)
{
    struct srv_reply * const *aa = a, * const *bb = b;

    if((*aa)->priority == (*bb)->priority)
	return (*aa)->weight - (*bb)->weight;
    return (*aa)->priority - (*bb)->priority;
}

void
_krb5_state_srv_sort(struct _krb5_srv_query_ctx *query)
{
    size_t n, m, o, prio_marker;
    uint32_t rnd, sum = 0;

    /* don't sort [0,1] srv records, they come pre-sorted */
    if (query->len < 2)
	return;

    /* sort them by priority and weight */
    qsort(query->array, query->len, sizeof(query->array[0]), compare_srv);

    /*
     * Fixup weight sorting too, assign a negative weight to elements
     * that are picked. Negative since the protocol only defines
     * postive values (int16_t) 
     */
    prio_marker = 0;

    for (n = 1; n < query->len; n++) {
	if (query->array[prio_marker]->priority != query->array[n]->priority) {

	    for (m = prio_marker; m < n && sum != 0; m++) {
		int32_t count = -1 * (int32_t)(query->len + 1);

		rnd = rk_random() % sum;

		for (o = prio_marker; o < n; o++) {
		    if (query->array[o]->weight < 0)
			continue;
		    if (rnd <= (uint32_t)query->array[o]->weight) {
			sum -= query->array[o]->weight;
			query->array[o]->weight = count++;
			break;
		    }
		    rnd -= query->array[o]->weight;
		}
		if (o >= n)
		    _krb5_debugx(query->context, 2,
				 "o too large: sum %d", (int)sum);
	    }
	    sum = 0;
	    prio_marker = n;
	} else {
	    sum += query->array[n]->weight;
	}
    }
    
    qsort(query->array, query->len, sizeof(query->array[0]), compare_srv);
}

static const char *
srv_reply_get_hostname(struct srv_reply *reply)
{
    if (reply->hostinfo)
	return reply->hostinfo->hostname;
    if (reply->hostname)
	return reply->hostname;
    return "<no-hostname>";
}

static int
srv_reply_get_port(struct srv_reply *reply)
{
    if (reply->hostinfo == NULL)
	return 0;
    return reply->hostinfo->port;
}

static void
state_append_hosts(struct _krb5_srv_query_ctx *query)
{
    size_t n;

    _krb5_debugx(query->context, 10, "SRV order after sorting");

    for (n = 0; n < query->len; n++) {
	_krb5_debugx(query->context, 10, "  SRV%lu kdc: %s:%d prio: %d weight: %d",
		     (unsigned long)n,
		     srv_reply_get_hostname(query->array[n]),
		     srv_reply_get_port(query->array[n]),
		     query->array[n]->priority,
		     query->array[n]->weight);

	if (n < query->context->max_srv_entries) {
	    append_host_hostinfo(query->context, query->handle, query->array[n]->hostinfo);
	    query->array[n]->hostinfo = NULL;
	}
    }
}

/*
 *
 */

static heim_dict_t dns_negative_cache;
static dispatch_queue_t dns_cache_queue;

static void
host_timeout_add(krb5_context context, const char *hostname, time_t t)
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
	dns_negative_cache = heim_dict_create(0);
	dns_cache_queue = dispatch_queue_create("krbhst-dns-cache", NULL);
    });
    if (dns_cache_queue == NULL || dns_negative_cache == NULL) {
	return;
    }

    if (t > 3600) {
	t = 3600;
    }

    _krb5_debugx(context, 10, " DNS adding negative cache for %s for %ld seconds", hostname, (long)t);

    dispatch_sync(dns_cache_queue, ^{
	heim_number_t expire = heim_number_create((int)(time(NULL) + t));
	if (expire == NULL) {
	    return;
	}
	heim_string_t s = heim_string_create(hostname);
	if (s == NULL) {
	    heim_release(expire);
	    return;
	}

	heim_dict_set_value(dns_negative_cache, s, expire);
	heim_release(expire);
	heim_release(s);
    });
}

static time_t
host_timeout_check(krb5_context context, const char *hostname)
{
    __block time_t e = 0;
    if (dns_cache_queue == NULL || dns_negative_cache == NULL) {
	return 0;
    }

    dispatch_sync(dns_cache_queue, ^{
	heim_string_t s = heim_string_create(hostname);
	if (s == NULL) {
	    return;
	}
	heim_number_t expire = heim_dict_copy_value(dns_negative_cache, s);
	if (expire) {
	    time_t t = time(NULL);
	    e = heim_number_get_int(expire);
	    e -= t;
	    heim_release(expire);
	}
	if (e <= 0) {
	    heim_dict_delete_key(dns_negative_cache, s);
	    e = 0;
	} else {
	    _krb5_debugx(context, 10, " DNS negative cache for host %s for another %ld seconds", hostname, (long)e);
	}
	heim_release(s);
    });
    return e;
}



/*
 *
 */

static void
dns_query_done(struct _krb5_srv_query_ctx *query)
{
    time_t endtimes = time(NULL) + 10; /* wait for max 10 seconds to stranglers to catch up */
    size_t n;

    heim_assert(!query->state.srvQueryDone, "DNS-SD invariant not true, canceled but got error message");
    query->state.srvQueryDone = 1;

    /* wait up for up to MAX_SRV_ENTRIES children queries if we had any */
    for (n = 0; n < query->len; n++) {
	const char *hostname = srv_reply_get_hostname(query->array[n]);
	time_t maxWait = endtimes - time(NULL);

	if (query->array[n]->hostsema && maxWait > 0 && n < query->context->max_srv_entries) {
	    _krb5_debugx(query->context, 10, "SRV waiting for addrinfo: %s", hostname);

	    if (heim_sema_wait(query->array[n]->hostsema, maxWait)) {
		/*
		 * If we timed out, when mark as cancelled
		 */
		dispatch_sync((dispatch_queue_t)query->handle->addrinfo_queue, ^{
		    query->array[n]->flags.canceled = true;
		});
		host_timeout_add(query->context, hostname, 3600);
	    }
	} else if (query->array[n]->hostsema && query->handle->addrinfo_queue) {
	    _krb5_debugx(query->context, 10, "SRV timeout waiting for addrinfo: %s", hostname);
	    dispatch_sync((dispatch_queue_t)query->handle->addrinfo_queue, ^{
	        query->array[n]->flags.canceled = true;
	    });
	    host_timeout_add(query->context, hostname, 3600);
	}
    }

    _krb5_state_srv_sort(query);
    state_append_hosts(query);

    dispatch_semaphore_signal((dispatch_semaphore_t)query->sema);
    heim_release(query);
}

static int
host_get_dns_service_id(krb5_context context,
			const char *hostname,
			const char *port,
			struct krb5_krbhst_data *handle,
			DNSServiceFlags *flags)
{
    int dns_service_id = 0;
#if !TARGET_OS_SIMULATOR && !TARGET_OS_OSX
    nw_path_evaluator_t evaluator = NULL;
    nw_parameters_t parameters = NULL;
    nw_interface_t iface = NULL;
    nw_endpoint_t nwhost = NULL;
    nw_path_t path = NULL;
    char *lname = NULL;

    lname = strdup(hostname);
    strlwr(lname);

    hostname = lname;

    nwhost = nw_endpoint_create_host(lname, port);
    if (nwhost == NULL) {
	_krb5_debugx(context, 5, "host_create(%s): nw_endpoint_t host is NULL", hostname);
	goto out;
    }

    parameters = nw_parameters_create();
    if (parameters == NULL) {
	_krb5_debugx(context, 5, "host_create(%s): nw_parameters_t is NULL", hostname);
	goto out;
    }

    if (handle->flags & KD_DELEG_UUID) {
	_krb5_debugx(context, 5, "host_create(%s): have delegate uuid", hostname);
	nw_parameters_set_e_proc_uuid(parameters, handle->delegate_uuid);
    } else {
	nw_parameters_set_pid(parameters, getpid());
    }

    evaluator = nw_path_create_evaluator_for_endpoint(nwhost, parameters);
    if (evaluator == NULL) {
	_krb5_debugx(context, 5, "host_create(%s): nw_path_evaluator_t is NULL", hostname);
	goto out;
    }

    path = nw_path_evaluator_copy_path(evaluator);
    if (path == NULL) {
	_krb5_debugx(context, 5, "host_create(%s): path is NULL", hostname);
	goto out;
    }

    if (nw_path_is_flow_divert(path)) {
	uuid_t uuid;

	if (!nw_path_get_vpn_config_id(path, &uuid)) {
	    _krb5_debugx(context, 5, "host_create(%s): path have no config id", hostname);
	    goto out;
	}

	_krb5_debugx(context, 5, "host_create(%s): vpn config uuid: "
		     "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		     hostname,
		     uuid[0],uuid[1],uuid[2],uuid[3],uuid[4],uuid[5],uuid[6],uuid[7],
		     uuid[8],uuid[9],uuid[10],uuid[11],uuid[12],uuid[13],uuid[14],uuid[15]);

	dns_service_id = ne_session_service_get_dns_service_id(uuid, NESessionTypeAppVPN, hostname);

	_krb5_debugx(context, 5, "host_create(%s): AppVPN: %d", hostname, (int)dns_service_id);

	if (dns_service_id)
	    *flags |= kDNSServiceFlagsServiceIndex;

    } else {

	iface = nw_path_copy_scoped_interface(path);
	if (iface == NULL) {
	    _krb5_debugx(context, 5, "host_create(%s): no interface", hostname);
	    goto out;
	}

	dns_service_id = nw_interface_get_index(iface);

	_krb5_debugx(context, 5, "host_create(%s): use dns_service_id %d", hostname, dns_service_id);
    }
 out:
    if (path)
	nw_release(path);
    if (nwhost)
	nw_release(nwhost);
    if (evaluator)
	nw_release(evaluator);
    if (iface)
	nw_release(iface);
    if (parameters)
	nw_release(parameters);
    if (lname)
	free(lname);

#endif /* !TARGET_OS_SIMULATOR */

    return dns_service_id;
}

/*
 *
 */

static void
srv_release(void *ctx)
{
    struct srv_reply *reply = ctx;
    struct krb5_krbhst_info *hi;
    if (!reply->flags.getAddrDone)
	_krb5_debugx(NULL, 10, "srv_release w/o getAddrDone set");
    if (reply->hostsema)
	heim_release(reply->hostsema);

    if (reply->addrinfo_queue) {
	DNSServiceRef srv_sd = reply->srv_sd;
	DNSServiceRef query_sd = reply->query_sd;
	heim_queue_t queue = reply->addrinfo_queue;

	reply->srv_sd = NULL;
	reply->query_sd = NULL;

	heim_retain(queue);

	/* need to release on queue */
	dispatch_async((dispatch_queue_t)queue, ^{
	    if (query_sd) {
		DNSServiceRefDeallocate(query_sd);
	    }
	    if (srv_sd) {
		DNSServiceRefDeallocate(srv_sd);
	    }
	    heim_release(queue);
	});
    }
    if (reply->hostname)
	free(reply->hostname);

    hi = reply->hostinfo;
    while (hi) {
	struct krb5_krbhst_info *next = hi->next;
	_krb5_free_krbhst_info(hi);
	hi = next;
    }
}

static void
add_hostinfo(struct srv_reply *reply,
	     const char *hostname,
	     struct addrinfo *ai,
	     const char *source,
	     struct _krb5_srv_query_ctx *query)
{
    struct krb5_krbhst_info *hi;
    size_t hostname_len = strlen(hostname);

    hi = calloc(1, sizeof(*hi) + hostname_len);
    if (hi == NULL) {
	if (ai)
	    freeaddrinfo(ai);
	return;
    }
    memcpy(hi->hostname, hostname, strlen(hostname) + 1);

    hi->proto = query->proto_num;

    hi->def_port = 0;
    hi->port = reply->port;

    if (query->path)
	hi->path = strdup(query->path);

    hi->source = source;
    hi->ai = ai;

    hi->next = reply->hostinfo;
    reply->hostinfo = hi;
}

static void
dns_getaddrinfo_callback(DNSServiceRef sdRef,
			 DNSServiceFlags flags,
			 uint32_t interfaceIndex,
			 DNSServiceErrorType errorCode,
			 const char *hostname,
			 const struct sockaddr *address,
			 uint32_t ttl,
			 void *context)
{
    struct srv_reply *srv_reply = context;
    struct _krb5_srv_query_ctx *query = srv_reply->query;
    struct addrinfo *ai = NULL;
    char host[NI_MAXHOST];
    int failure = 0, error;

    if (srv_reply->flags.getAddrDone) {
	return;
    }
    if (srv_reply->flags.canceled) {
	_krb5_debugx(query->context, 10, "SRV getaddrinfo: canceled: %s", hostname);
	failure = 1;
	goto out;
    }

    /*
     * We get called at least twice, once for each address family, so
     * we need to keep track of successful lookups and no such record.
     *
     * The failure to find a name arrive in the form of a
     * kDNSServiceErr_NoSuchRecord, all other failure are treated as
     * hard failures and the connection is directly aborted.
     */

    switch (errorCode) {
    case kDNSServiceErr_NoSuchRecord:
	if (address->sa_family == AF_INET)
	    srv_reply->flags.noIPv4 = 1;
	else if (address->sa_family == AF_INET6)
	    srv_reply->flags.noIPv6 = 1;
	else
	    failure = 1; /* other values are undefined, this will end the processing */
	
	_krb5_debugx(query->context, 10, "SRV callback: getaddrinfo on %s: no such record for af = %d", hostname, (int)address->sa_family);
	break;

    case kDNSServiceErr_NoError:

	_krb5_debugx(query->context, 10, "DNS getaddrinfo callback on: %s for af = %d", hostname, (int)address->sa_family);

	if (address->sa_family == AF_INET) {
	    srv_reply->flags.recvIPv4 = 1;
	} else if (address->sa_family == AF_INET6) {
	    srv_reply->flags.recvIPv6 = 1;
	} else {
	    failure = 1;
	}
	
	
	/* Now this is silly, go get from a sockaddr to a addrinfo, bounce though getaddrinfo/getnameinfo */
	error = getnameinfo(address, (socklen_t)rk_socket_addr_size(address), host, sizeof(host), NULL, 0, NI_NUMERICHOST);
	if (error == 0) {
	    struct addrinfo hints;
	    char portname[NI_MAXSERV];

	    snprintf(portname, sizeof(portname), "%d", srv_reply->port);
	    make_hints(&hints, query->proto_num);
	    hints.ai_family = address->sa_family;
	    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

	    _krb5_debugx(query->context, 10, " SRV getaddrinfo: %s domain: proto: %d addr: %s:%d",
			 query->domain, (int)query->proto_num, host, srv_reply->port);

	    error = getaddrinfo(host, portname, &hints, &ai);
	    if (error)
		_krb5_debugx(query->context, 10, " SRV getaddrinfo: failed to parse host: [%s]:%s error: %d", host, portname, error);
	}
	
	if (ai) {
	    _krb5_debugx(query->context, 10, " Adding srv-addrinfo host: %s:%d", hostname, srv_reply->port);
	    add_hostinfo(srv_reply, hostname, ai, "srv-addrinfo", query);
	}
	break;

    default:
	/* all other failures are hard stop failure */
	_krb5_debugx(query->context, 10, "SRV getaddrinfo other error: %d", (int)errorCode);
	failure = 1; /* other values are undefined on failure */
	break;
    }

 out:
    _krb5_debugx(query->context, 10, " SRV getaddrinfo end: %s (%s) (IPv4:%s) (IPv6:%s)",
		 hostname, (flags & kDNSServiceFlagsMoreComing) ? "more coming" : "done",
		 srv_reply->flags.noIPv4 ? "no-rr" :  (srv_reply->flags.recvIPv4 ? "yes" : "waiting"),
		 srv_reply->flags.noIPv6 ? "no-rr" :  (srv_reply->flags.recvIPv6 ? "yes" : "waiting"));

    /*
     * Stop processing requests when we have:
     * - both IPv4 and IPv6 answers and no pending packets.
     * - hard stop in case of a failure
     */

    if (failure ||
	((srv_reply->flags.noIPv4 || srv_reply->flags.recvIPv4) &&
	 (srv_reply->flags.noIPv6 || srv_reply->flags.recvIPv6) &&
	 (flags & kDNSServiceFlagsMoreComing) == 0))
    {
	_krb5_debugx(query->context, 10, " DNS getaddrinfo done: %s %s", srv_reply->hostname, failure ? "failed" : "success");
	srv_reply->flags.getAddrDone = true;
	heim_sema_signal(srv_reply->hostsema);
	heim_release(srv_reply);
    }
}

static void
SRVQueryCallback(DNSServiceRef sdRef,
		 DNSServiceFlags flags,
		 uint32_t ifIndex,
		 DNSServiceErrorType errorCode,
		 const char *fullname,
		 uint16_t rrtype,
		 uint16_t rrclass,
		 uint16_t rdlen,
		 const void *rdata,
		 uint32_t ttl,
		 void *info)
{
    const uint8_t *end_rd = ((const uint8_t *)rdata) + rdlen, *rd = rdata;
    struct _krb5_srv_query_ctx *query = info;
    struct srv_reply *srv_reply = NULL;
    uint16_t priority, weight, port;
    struct srv_reply **tmp;
    char *hostname = NULL;
    int dns_service_id = 0;
    int status;
    DNSServiceErrorType error;

    if (query->state.srvQueryDone) {
	_krb5_debugx(query->context, 10, "Got SRV callback after MoreComing == 0 was already set!");
	return;
    }

    if (errorCode != kDNSServiceErr_NoError) {
	flags = 0; /* other values are undefined on failure */
	_krb5_debugx(query->context, 10, "SRV callback: error: %d", (int)errorCode);
	goto end;
    }

    if (rrtype != kDNSServiceType_SRV) {
	_krb5_debugx(query->context, 10, "SRV callback: rd_len no srv query");
	goto end;
    }

    if (rdlen < 7) {
	_krb5_debugx(query->context, 10, "SRV callback: rd_len too short: %d", (int)rdlen);
	goto end;
    }

    priority = (rd[0] << 8) | rd[1];
    weight = (rd[2] << 8) | rd[3];
    port = (rd[4] << 8) | rd[5];

    _krb5_debugx(query->context, 10, "Got SRV callback: %s:%d prio: %d weight: %d",
		 fullname, port, priority, weight);

    hostname = malloc(rdlen + 1);
    if (hostname == NULL) {
	_krb5_debugx(query->context, 10, "SRV callback: failed to allocate hostname");
	goto end;
    }

    status = dn_expand(rdata, end_rd, rd + 6, hostname, rdlen);
    if(status < 0 || (size_t)status + 6 > rdlen) {
	_krb5_debugx(query->context, 10, "SRV callback: dn_expand: %d", status);
	goto end;
    }
    
    /* Trim out any trailing . that the lovingly DNS layer have might have added for us */
    _krb5_remove_trailing_dot(hostname);

    if (host_timeout_check(query->context, hostname)) {
	goto end;
    }

    srv_reply = heim_uniq_alloc(sizeof(query->array[query->len][0]), "heim-srv-object", srv_release);
    if (srv_reply == NULL) {
	_krb5_debugx(query->context, 10, "SRV callback: alloc heim-srv-object");
	goto end;
    }

    srv_reply->port = port;
    srv_reply->priority = priority;
    srv_reply->weight = weight;
    srv_reply->query = query;
    srv_reply->hostname = hostname;
    hostname = NULL;

    /*
     * Only trigger the A/AAAA query if we are delegating
     */
    if (query->handle->flags & KD_DELEG_UUID) {
	krb5_af_flags kafs = _krb5_get_supported_af(query->context);
	struct krb5_krbhst_data *handle = query->handle;
	DNSServiceFlags dnsFlags =
	    kDNSServiceFlagsTimeout | kDNSServiceFlagsReturnIntermediates;
	DNSServiceProtocol protocol_flags = 0;
	char *fqdn_host = NULL;

	_krb5_debugx(query->context, 10, "Got delegated query on: %s", srv_reply->hostname);

	/*
	 * If we are dealing with KD_DELEG_UUID, we need one handle per request
	 * for each addrinfo calls, since mDNSResponder uses that to track the
	 * morecoming flag, and if we have these new addrinfo calls outstanding
	 * while the main query is still out there (it will be), it will never
	 * get the morecoming flag cleared.
	 */

	srv_reply->addrinfo_queue = heim_retain(handle->addrinfo_queue);

	error = DNSServiceCreateDelegateConnection(&srv_reply->srv_sd, 0, handle->delegate_uuid);
	if (error != kDNSServiceErr_NoError) {
	    _krb5_debugx(query->context, 2,
			 "Failed setting up search context for addrinfo resolving for %s failed: %d",
			 srv_reply->hostname, error);
	    goto end;
	}

	_krb5_debugx(query->context, 10, "setting queue");

	error = DNSServiceSetDispatchQueue(srv_reply->srv_sd, (dispatch_queue_t)srv_reply->addrinfo_queue);
	if (error) {
	    _krb5_debugx(query->context, 2,
			 "Failed setting run queue for SRV query: %d", error);
	    goto end;
	}

	/* always use the realm as the "hostname" so that we tunnel out though the same connection */
	dns_service_id = host_get_dns_service_id(query->context, query->handle->realm, "88", handle, &dnsFlags);

	srv_reply->hostsema = heim_sema_create(0);
	if (srv_reply->hostsema == NULL) {
	    goto end;
	}

	heim_retain(srv_reply); /* retain for callback */

	if (kafs & KRB5_AF_FLAG_INET) {
	    protocol_flags |= kDNSServiceProtocol_IPv4;
	} else {
	    srv_reply->flags.noIPv4 = true;
	}
	if (kafs & KRB5_AF_FLAG_INET6) {
	    protocol_flags |= kDNSServiceProtocol_IPv6;
	} else {
	    srv_reply->flags.noIPv6 = true;
	}

	if (protocol_flags == 0) {
	    _krb5_debugx(query->context, 10, "No support address families: %s", srv_reply->hostname);
	    heim_release(srv_reply); /* release for the callback that will never happen */
	    goto end;
	}

	_krb5_debugx(query->context, 10, "DNS SRV using address flags: %08x", protocol_flags);

	/*
	 * Since its a DNS query, add a . at the end to avoid having
	 * the client iterate over the search field since we know we
	 * have the FQDN in the first place since that's that the SRV
	 * RR give us.
	 */
	asprintf(&fqdn_host, "%s.", srv_reply->hostname);

	srv_reply->query_sd = srv_reply->srv_sd;

	error = DNSServiceGetAddrInfo(&srv_reply->query_sd,
				      dnsFlags | kDNSServiceFlagsShareConnection,
				      dns_service_id,
				      protocol_flags,
				      fqdn_host,
				      dns_getaddrinfo_callback,
				      srv_reply);
	free(fqdn_host);
	if (error) {
	    _krb5_debugx(query->context, 10, "Failed doing A/AAAA lookup: %s: %d", srv_reply->hostname, error);
	    heim_release(srv_reply); /* release for the callback that will never happen */
	    goto end;
	}
    } else {
	_krb5_debugx(query->context, 10, "Adding host: %s:%d", srv_reply->hostname, srv_reply->port);
	add_hostinfo(srv_reply, srv_reply->hostname, NULL, "srv", query);
    }

    tmp = realloc(query->array, (query->len + 1) * sizeof(query->array[0]));
    if (tmp == NULL) {
	_krb5_debugx(query->context, 10, "SRV callback: realloc failed");

	if (srv_reply->query_sd) {
	    /* we need to release the retain for the callback if it never happned */
	    dispatch_sync((dispatch_queue_t)srv_reply->addrinfo_queue, ^{
		if (srv_reply->query_sd) {
		    DNSServiceRefDeallocate(srv_reply->query_sd);
		    srv_reply->query_sd = NULL;
		    if (!srv_reply->flags.getAddrDone) {
			srv_reply->flags.getAddrDone = true;
			heim_release(srv_reply);
		    }
		}
	    });
	} else {
	    heim_release(srv_reply);
	}

	goto end;
    }
    query->array = tmp;
    query->array[query->len] = heim_retain(srv_reply);
    query->len++;

 end:
    _krb5_debugx(query->context, 10, "SRV callback end: %s", fullname);

    if (srv_reply)
	heim_release(srv_reply);

    if (hostname) {
	free(hostname);
    }
    if ((flags & kDNSServiceFlagsMoreComing) == 0) {
	_krb5_debugx(query->context, 10, "SRV callback no more coming");
	dns_query_done(query);
    }
}


/*
 *
 */

static krb5_error_code
srv_find_realm(krb5_context context, struct krb5_krbhst_data *handle,
	       struct _krb5_srv_query_ctx *query)
{
    DNSServiceRef client = NULL;
    DNSServiceErrorType error;
    krb5_error_code ret;
    int dns_service_id;
    DNSServiceFlags dnsFlags =
	kDNSServiceFlagsTimeout | kDNSServiceFlagsReturnIntermediates;

    if (handle->main_sd == NULL) {

	if (handle->flags & KD_DELEG_UUID) {
	    error = DNSServiceCreateDelegateConnection(&handle->main_sd, 0, handle->delegate_uuid);
	} else {
	    error = DNSServiceCreateConnection(&handle->main_sd);
	}
	if (error != kDNSServiceErr_NoError) {
	    _krb5_debugx(context, 2,
			 "Failed setting up search context for domain %s failed: %d",
			 query->domain, error);
	    return KRB5_KDC_UNREACH;
	}
	
	error = DNSServiceSetDispatchQueue(handle->main_sd, (dispatch_queue_t)handle->srv_queue);
	if (error) {
	    DNSServiceRefDeallocate(handle->main_sd);
	    _krb5_debugx(context, 2,
			 "Failed setting run queue for SRV query: %d", error);
	    return KRB5_KDC_UNREACH;
	}
    }

    dns_service_id = host_get_dns_service_id(context, query->handle->realm, "88", handle, &dnsFlags);

    client = handle->main_sd;

    heim_retain(query);

    error = DNSServiceQueryRecord(&client,
				  dnsFlags | kDNSServiceFlagsShareConnection,
				  dns_service_id,
				  query->domain,
				  kDNSServiceType_SRV,
				  kDNSServiceClass_IN,
				  SRVQueryCallback,
				  query);
    if (error == kDNSServiceErr_NoError) {

	if (dispatch_semaphore_wait((dispatch_semaphore_t)query->sema,  dispatch_time(DISPATCH_TIME_NOW, 10ull * NSEC_PER_SEC))) {
	    _krb5_debugx(context, 2,
			 "searching DNS %s for domain timed out",
			 query->domain);
	    ret = KRB5_KDC_UNREACH;
	} else
	    ret = 0;


	/* must run the DNSServiceRefDeallocate on the same queue as dns request are processed on */
	dispatch_sync((dispatch_queue_t)handle->srv_queue, ^{
	    DNSServiceRefDeallocate(client);

	    /*
	     * If we canceled the connection, and the callback didn't
	     * get a chance to any work, now its time to clean up
	     * since after DNSServiceRefDeallocate() completed, there
	     * will be no more callbacks.
	     */

	    if (!query->state.srvQueryDone) {
		query->state.srvQueryDone = 1;
		heim_release(query);
	    }
	});
    } else {
	_krb5_debugx(context, 2,
		     "searching DNS for domain %s failed: %d",
		     query->domain, error);
	ret = KRB5_KDC_UNREACH;
    }
   
    return ret;
}

#else

/*
 *
 */

static void
srv_query_domain(void *ctx)
{
    struct _krb5_srv_query_ctx *query = ctx;
    struct rk_dns_reply *r;
    struct rk_resource_record *rr;

    r = rk_dns_lookup(query->domain, "SRV");
    if(r == NULL)
	goto out;

    rk_dns_srv_order(r);

    for(rr = r->head; rr; rr = rr->next) {
	if(rr->type == rk_ns_t_srv) {
	    krb5_krbhst_info *hi;
	    size_t len = strlen(rr->u.srv->target);

	    hi = calloc(1, sizeof(*hi) + len);
	    if(hi == NULL)
		goto out;

	    hi->proto = query->proto_num;

	    hi->def_port = query->def_port;
	    if (handle->port != 0)
		hi->port = handle->port;
	    else
		hi->port = rr->u.srv->port;
	    hi->source = "srv";
	    if (query->path)
		hi->path = strdup(query->path);
	    strlcpy(hi->hostname, rr->u.srv->target, len + 1);

	    append_host_hostinfo(query->context, kd, hi);
	}
    }

 out:
    if (r)
	rk_dns_free_data(r);
    heim_sema_signal(query->sema);
    heim_release(query->handle);
}

static krb5_error_code
srv_find_realm(krb5_context context, struct krb5_krbhst_data *handle,
	       struct _krb5_srv_query_ctx *query)
{
    heim_async_f(handle->process_queue, query, srv_query_domain);
    heim_sema_wait(query->sema, 10);
}

#endif

/**
 * Squash any trailing .
 */

void
_krb5_remove_trailing_dot(char *hostname)
{
    size_t len;

    len = strlen(hostname);
    if (len > 0 && hostname[len - 1] == '.')
	hostname[len - 1] = '\0';
}


static krb5_boolean
krbhst_empty(struct krb5_krbhst_data *kd)
{
    krb5_boolean empty;

    HEIMDAL_MUTEX_lock(&kd->mutex);
    empty = (kd->index == &kd->hosts);
    HEIMDAL_MUTEX_unlock(&kd->mutex);

    return empty;
}

/*
 * Return the default protocol for the `kd' (either TCP or UDP)
 */

static int
krbhst_get_default_proto(struct krb5_krbhst_data *kd)
{
    if (kd->flags & KD_LARGE_MSG)
	return KRB5_KRBHST_TCP;
    return KRB5_KRBHST_UDP;
}

static int
krbhst_get_default_port(struct krb5_krbhst_data *kd)
{
    return kd->def_port;
}

/*
 *
 */

const char *
_krb5_krbhst_get_realm(krb5_krbhst_handle handle)
{
    return handle->realm;
}

/*
 * parse `spec' into a krb5_krbhst_info, defaulting the port to `def_port'
 * and forcing it to `port' if port != 0
 */

static struct krb5_krbhst_info*
parse_hostspec(krb5_context context,
	       struct krb5_krbhst_data *kd,
	       const char *source,
	       const char *spec,
	       int def_port,
	       int port)
{
    const char *p = spec, *q, *portstr = NULL;
    struct krb5_krbhst_info *hi;
    size_t end_hostname;

    hi = calloc(1, sizeof(*hi) + strlen(spec));
    if(hi == NULL)
	return NULL;

    hi->proto = krbhst_get_default_proto(kd);

    if(strncmp(p, "http://", 7) == 0){
	hi->proto = KRB5_KRBHST_HTTP;
	p += 7;
    } else if(strncmp(p, "http/", 5) == 0) {
	hi->proto = KRB5_KRBHST_HTTP;
	p += 5;
	def_port = ntohs(krb5_getportbyname (context, "http", "tcp", 80));
    } else if(strncmp(p, "kkdcp://", 8) == 0) {
	hi->proto = KRB5_KRBHST_KKDCP;
	p += 8;
	def_port = ntohs(krb5_getportbyname (context, "https", "tcp", 443));
    }else if(strncmp(p, "tcp/", 4) == 0){
	hi->proto = KRB5_KRBHST_TCP;
	p += 4;
    } else if(strncmp(p, "udp/", 4) == 0) {
	hi->proto = KRB5_KRBHST_UDP;
	p += 4;
    }

    hi->source = source;

    if (p[0] == '[' && (q = strchr(p, ']')) != NULL) {
	/* if address looks like [foo:bar] or [foo:bar]: its a ipv6
	   adress, strip of [] */
	memcpy(hi->hostname, &p[1], q - p - 1);
	hi->hostname[q - p - 1] = '\0';
	p = q + 1;
	/* get trailing : */
	if (p[0] == ':')
	    portstr = ++p;

	p = strchr(p, '/');

    } else if ((end_hostname = strcspn(p, ":/")) != 0) {
	memcpy(hi->hostname, p, end_hostname);
	hi->hostname[end_hostname] = '\0';
	if (p[end_hostname] == ':') {
	    portstr = p + end_hostname + 1;
	    p = strchr(p, '/');
	} else { 
	    p = p + end_hostname;
	}
    } else {
	memcpy(hi->hostname, p, strlen(p) + 1);
    }

    /* if we had a path, pick it up now */
    if (p && p[0] == '/')
	hi->path = strdup(&p[1]);

    strlwr(hi->hostname);

    hi->port = hi->def_port = def_port;
    if(portstr != NULL && portstr[0]) {
	char *end;
	hi->port = strtol(portstr, &end, 0);
	if(end == portstr) {
	    if (hi->path)
		free(hi->path);
	    free(hi);
	    return NULL;
	}
    }
    if (port)
	hi->port = port;
    return hi;
}

void
_krb5_free_krbhst_info(krb5_krbhst_info *hi)
{
    if (hi->ai != NULL)
	freeaddrinfo(hi->ai);
    if (hi->path)
	free(hi->path);
    free(hi);
}

krb5_error_code
_krb5_krbhost_info_move(krb5_context context,
			krb5_krbhst_info *from,
			krb5_krbhst_info **to)
{
    size_t hostnamelen = strlen(from->hostname);
    /* trailing NUL is included in structure */
    *to = calloc(1, sizeof(**to) + hostnamelen);
    if(*to == NULL) {
	krb5_set_error_message(context, ENOMEM,
			       N_("malloc: out of memory", ""));
	return ENOMEM;
    }

    (*to)->proto = from->proto;
    (*to)->port = from->port;
    (*to)->def_port = from->def_port;
    (*to)->ai = from->ai;
    from->ai = NULL;
    (*to)->next = NULL;
    (*to)->path = from->path;
    from->path = NULL;
    memcpy((*to)->hostname, from->hostname, hostnamelen + 1);
    return 0;
}


static void
append_host_hostinfo(krb5_context context, struct krb5_krbhst_data *kd, struct krb5_krbhst_info *host)
{
    struct krb5_krbhst_info *h, *next;

    HEIMDAL_MUTEX_lock(&kd->mutex);

    while (host) {
	next = host->next;
	host->next = NULL;

	/* 
	 * First filter out duplicates
	 */
	for(h = kd->hosts; h && host; h = h->next) {
	    if(h->proto == host->proto &&
	       h->port == host->port &&
	       strcasecmp(h->hostname, host->hostname) == 0)
	    {
		_krb5_debugx(context, 10, "dropping dup KDC host: %s:%d (proto %d)", h->hostname, h->port, h->proto);
		_krb5_free_krbhst_info(host);
		host = NULL;
	    }
	}

	if (host) {
	    *kd->end = host;
	    kd->end = &host->next;
	}

	host = next;
    }

    HEIMDAL_MUTEX_unlock(&kd->mutex);
}

static krb5_error_code
append_host_string(krb5_context context,
		   struct krb5_krbhst_data *kd,
		   const char *source,
		   const char *host,
		   int def_port,
		   int port)
{
    struct krb5_krbhst_info *hi;

    hi = parse_hostspec(context, kd, source, host, def_port, port);
    if(hi == NULL)
	return ENOMEM;

    append_host_hostinfo(context, kd, hi);
    return 0;
}

/*
 * return a readable representation of `host' in `hostname, hostlen'
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_krbhst_format_string(krb5_context context, const krb5_krbhst_info *host,
			  char *hostname, size_t hostlen)
{
    const char *proto = "";
    char portstr[7] = "";
    if(host->proto == KRB5_KRBHST_TCP)
	proto = "tcp/";
    else if(host->proto == KRB5_KRBHST_HTTP)
	proto = "http://";
    else if(host->proto == KRB5_KRBHST_KKDCP)
	proto = "kkdcp://";
    if(host->port != host->def_port)
	snprintf(portstr, sizeof(portstr), ":%d", host->port);
    snprintf(hostname, hostlen, "%s%s%s%s%s", proto, host->hostname, portstr,
	     host->proto == KRB5_KRBHST_KKDCP ? "/" : "",
	     host->proto == KRB5_KRBHST_KKDCP ? host->path : "");
    return 0;
}

/**
 * Return an `struct addrinfo *' for a KDC host.
 *
 * Returns an the struct addrinfo in in that corresponds to the
 * information in `host'.  free:ing is handled by krb5_krbhst_free, so
 * the returned ai must not be released.
 *
 * @ingroup krb5
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_krbhst_get_addrinfo(krb5_context context, krb5_krbhst_info *host,
			 struct addrinfo **ai)
{
    int ret = 0;

    if (host->ai == NULL) {
	struct addrinfo hints;
	char portstr[NI_MAXSERV];
	char *hostname = host->hostname;

	snprintf (portstr, sizeof(portstr), "%d", host->port);
	make_hints(&hints, host->proto);

	/**
	 * First try this as an IP address, this allows us to add a
	 * dot at the end to stop using the search domains.
	 */

	hints.ai_flags |= AI_NUMERICHOST | AI_NUMERICSERV;

	ret = getaddrinfo(host->hostname, portstr, &hints, &host->ai);
	if (ret == 0)
	    goto out;

	/**
	 * If the hostname contains a dot, assumes it's a FQDN and
	 * don't use search domains since that might be painfully slow
	 * when machine is disconnected from that network.
	 */

	hints.ai_flags &= ~(AI_NUMERICHOST);

	if (strchr(hostname, '.') && hostname[strlen(hostname) - 1] != '.') {
	    ret = asprintf(&hostname, "%s.", host->hostname);
	    if (ret < 0 || hostname == NULL)
		return ENOMEM;
	}

	ret = getaddrinfo(hostname, portstr, &hints, &host->ai);
	if (hostname != host->hostname)
	    free(hostname);
	if (ret) {
	    ret = krb5_eai_to_heim_errno(ret, errno);
	    goto out;
	}
    }
 out:
    *ai = host->ai;
    return ret;
}

static krb5_boolean
get_next(struct krb5_krbhst_data *kd, krb5_krbhst_info **host)
{
    struct krb5_krbhst_info *hi;

    HEIMDAL_MUTEX_lock(&kd->mutex);

    hi = *kd->index;
    if(hi != NULL) {
	*host = hi;
	kd->index = &(*kd->index)->next;
    }
    HEIMDAL_MUTEX_unlock(&kd->mutex);
    return hi ? TRUE : FALSE;
}

static krb5_error_code
srv_get_hosts(krb5_context context, struct krb5_krbhst_data *kd,
	      const char *sitename, const char *proto, const char *service)
{
    struct _krb5_srv_query_ctx *query = NULL;
    int proto_num, def_port;
    const char *path = NULL;
    char *domain = NULL;

    if (krb5_realm_is_lkdc(kd->realm))
	return 0;

    proto_num = string_to_proto(proto);
    if(proto_num < 0) {
	_krb5_debugx(context, 1, N_("unknown protocol `%s' to lookup", ""), proto);
	return 0;
    }

    if(proto_num == KRB5_KRBHST_HTTP) {
	def_port = ntohs(krb5_getportbyname (context, "http", "tcp", 80));
    } else if(proto_num == KRB5_KRBHST_KKDCP) {
	def_port = ntohs(krb5_getportbyname (context, "https", "tcp", 443));
	path = "kkdcp";
    } else if(kd->port) {
	def_port = kd->port;
    } else { 
	def_port = ntohs(krb5_getportbyname (context, service, proto, 88));
    }

    if (sitename) {
	asprintf(&domain, "_%s._%s.%s._sites.%s.", service, proto, sitename, kd->realm);
    } else {
	asprintf(&domain, "_%s._%s.%s.", service, proto, kd->realm);
    }
    if (domain == NULL)
	return krb5_enomem(context);

    query = heim_uniq_alloc(sizeof(*query), "heim-query-ctx", query_release);
    if (query == NULL) {
	free(domain);
	return krb5_enomem(context);
    }

    query->context = context;
    query->sema = heim_sema_create(0);
    query->domain = domain;
    if (query->sema == NULL) {
	heim_release(query);
	return krb5_enomem(context);
    }
    query->handle = heim_retain(kd);
    query->def_port = def_port;
    query->proto_num = proto_num;
    query->path = path;
#ifdef __APPLE__
    query->array = NULL;
    query->len = 0;
#endif

    srv_find_realm(context, kd, query);

    heim_release(query);

    return 0;
}

/*
 * read the configuration for `conf_string', defaulting to kd->def_port and
 * forcing it to `kd->port' if kd->port != 0
 */

static void
config_get_hosts(krb5_context context, struct krb5_krbhst_data *kd,
		 const char *conf_string)
{
    int i;
    char **hostlist;
    hostlist = krb5_config_get_strings(context, NULL,
				       "realms", kd->realm, conf_string, NULL);

    _krb5_debugx(context, 2, "configuration file for realm %s%s found",
		kd->realm, hostlist ? "" : " not");

    if(hostlist == NULL)
	return;
    kd->flags |= KD_CONFIG_EXISTS;
    for(i = 0; hostlist && hostlist[i] != NULL; i++)
	append_host_string(context, kd, "config", hostlist[i], kd->def_port, kd->port);

    krb5_config_free_strings(hostlist);
}

/*
 * as a fallback, look for `serv_string.kd->realm' (typically
 * kerberos.REALM, kerberos-1.REALM, ...
 * `port' is the default port for the service, and `proto' the
 * protocol
 */

static krb5_error_code
fallback_get_hosts(krb5_context context, struct krb5_krbhst_data *kd,
		   const char *serv_string, int port, int proto)
{
    char *host = NULL;
    int ret;
    struct addrinfo *ai;
    struct addrinfo hints;
    char portstr[NI_MAXSERV];

    ret = krb5_config_get_bool_default(context, NULL, KRB5_FALLBACK_DEFAULT,
				       "libdefaults", "use_fallback", NULL);
    if (!ret) {
	kd->flags |= KD_FALLBACK;
	return 0;
    }

    _krb5_debugx(context, 2, "fallback lookup %d for realm %s (service %s)",
		kd->fallback_count, kd->realm, serv_string);

    /*
     * Don't try forever in case the DNS server keep returning us
     * entries (like wildcard entries or the .nu TLD)
     *
     * Also don't try LKDC realms since fallback wont work on them at all.
     */
    if(kd->fallback_count >= 5 || krb5_realm_is_lkdc(kd->realm)) {
	kd->flags |= KD_FALLBACK;
	return 0;
    }

    if(kd->fallback_count == 0)
	ret = asprintf(&host, "%s.%s.", serv_string, kd->realm);
    else
	ret = asprintf(&host, "%s-%d.%s.",
		       serv_string, kd->fallback_count, kd->realm);

    if (ret < 0 || host == NULL)
	return ENOMEM;

    make_hints(&hints, proto);
    snprintf(portstr, sizeof(portstr), "%d", port);
    ret = getaddrinfo(host, portstr, &hints, &ai);
    if (ret) {
	/* no more hosts, so we're done here */
	free(host);
	kd->flags |= KD_FALLBACK;
    } else {
	struct krb5_krbhst_info *hi;
	size_t hostlen = strlen(host);

	hi = calloc(1, sizeof(*hi) + hostlen);
	if(hi == NULL) {
	    free(host);
	    return ENOMEM;
	}

	hi->proto = proto;
	hi->port  = hi->def_port = port;
	hi->ai    = ai;
	hi->source = "fallback";
	memmove(hi->hostname, host, hostlen);
	hi->hostname[hostlen] = '\0';
	free(host);
	append_host_hostinfo(context, kd, hi);
	kd->fallback_count++;
    }
    return 0;
}

/*
 * Fetch hosts from plugin
 */

static krb5_error_code
add_plugin_host(struct krb5_krbhst_data *kd,
		const char *host,
		const char *port,
		int portnum,
		int proto)
{
    struct krb5_krbhst_info *hi;
    struct addrinfo hints, *ai;
    size_t hostlen;
    int ret;

    make_hints(&hints, proto);
    ret = getaddrinfo(host, port, &hints, &ai);
    if (ret)
	return 0;

    hostlen = strlen(host);

    hi = calloc(1, sizeof(*hi) + hostlen);
    if(hi == NULL)
	return ENOMEM;

    hi->proto = proto;
    hi->port  = hi->def_port = portnum;
    hi->ai    = ai;
    hi->source = "plugin";
    memmove(hi->hostname, host, hostlen);
    hi->hostname[hostlen] = '\0';
    append_host_hostinfo(kd->context, kd, hi);

    return 0;
}

static krb5_error_code
add_locate(void *ctx, int type, struct sockaddr *addr)
{
    struct krb5_krbhst_data *kd = ctx;
    char host[NI_MAXHOST], port[NI_MAXSERV];
    socklen_t socklen;
    krb5_error_code ret;
    int proto, portnum;

    socklen = socket_sockaddr_size(addr);
    portnum = socket_get_port(addr);

    ret = getnameinfo(addr, socklen, host, sizeof(host), port, sizeof(port),
		      NI_NUMERICHOST|NI_NUMERICSERV);
    if (ret != 0)
	return 0;

    if (kd->port)
	snprintf(port, sizeof(port), "%d", kd->port);
    else if (atoi(port) == 0)
	snprintf(port, sizeof(port), "%d", krbhst_get_default_port(kd));

    proto = krbhst_get_default_proto(kd);

    ret = add_plugin_host(kd, host, port, portnum, proto);
    if (ret)
	return ret;

    /*
     * This is really kind of broken and should be solved a different
     * way, some sites block UDP, and we don't, in the general case,
     * fall back to TCP, that should also be done. But since that
     * should require us to invert the whole "find kdc" stack, let put
     * this in for now. 
     */

    if (proto == KRB5_KRBHST_UDP) {
	ret = add_plugin_host(kd, host, port, portnum, KRB5_KRBHST_TCP);
	if (ret)
	    return ret;
    }

    return 0;
}

struct plctx {
    enum locate_service_type type;
    struct krb5_krbhst_data *kd;
    unsigned long flags;
};

static krb5_error_code
plcallback(krb5_context context,
	   const void *plug, void *plugctx, void *userctx)
{
    const krb5plugin_service_locate_ftable *locate = plug;
    struct plctx *plctx = userctx;
    
    if (locate->minor_version >= KRB5_PLUGIN_LOCATE_VERSION_2)
	return locate->lookup(plugctx, plctx->flags, plctx->type, plctx->kd->realm, 0, 0, add_locate, plctx->kd);
    
    if (plctx->flags & KRB5_PLF_ALLOW_HOMEDIR)
	return locate->old_lookup(plugctx, plctx->type, plctx->kd->realm, 0, 0, add_locate, plctx->kd);
    
    return KRB5_PLUGIN_NO_HANDLE;
}

static void
plugin_get_hosts(krb5_context context,
		 struct krb5_krbhst_data *kd,
		 enum locate_service_type type)
{
    struct plctx ctx = { type, kd, 0 };

    if (krb5_homedir_access(context))
	ctx.flags |= KRB5_PLF_ALLOW_HOMEDIR;

    krb5_plugin_run_f(context, "krb5", KRB5_PLUGIN_LOCATE,
		      KRB5_PLUGIN_LOCATE_VERSION_0,
		      0, &ctx, plcallback);
}

/*
 *
 */

static void
hostnames_get_hosts(krb5_context context,
		    struct krb5_krbhst_data *kd,
		    const char *type)
{
    kd->flags |= KD_HOSTNAMES;
    if (kd->hostname) {
	_krb5_debugx(context, 5, "adding hostname: %s", kd->hostname);
	append_host_string(context, kd, "hostname", kd->hostname, kd->def_port, kd->port);
    }
}


/*
 *
 */

static krb5_error_code
kdc_get_next(krb5_context context,
	     struct krb5_krbhst_data *kd,
	     krb5_krbhst_info **host)
{
    krb5_error_code ret;

    if ((kd->flags & KD_HOSTNAMES) == 0) {
	hostnames_get_hosts(context, kd, "kdc");
	if(get_next(kd, host))
	    return 0;
    }

    if ((kd->flags & KD_PLUGIN) == 0) {
	plugin_get_hosts(context, kd, locate_service_kdc);
	kd->flags |= KD_PLUGIN;
	if(get_next(kd, host))
	    return 0;
    }

    if((kd->flags & KD_CONFIG) == 0) {
	config_get_hosts(context, kd, "kdc");
	kd->flags |= KD_CONFIG;
	if(get_next(kd, host))
	    return 0;
    }

    if (kd->flags & KD_CONFIG_EXISTS) {
	_krb5_debugx(context, 1,
		    "Configuration exists for realm %s, wont go to DNS",
		    kd->realm);
	return KRB5_KDC_UNREACH;
    }

    if(context->srv_lookup) {
	if(kd->sitename && (kd->flags & KD_SITE_SRV_TCP) == 0) {
	    srv_get_hosts(context, kd, kd->sitename, "tcp", "kerberos");
	    kd->flags |= KD_SITE_SRV_TCP;
	    if(get_next(kd, host))
		return 0;
	}
	if((kd->flags & KD_SRV_TCP) == 0) {
	    srv_get_hosts(context, kd, NULL, "tcp", "kerberos");
	    kd->flags |= KD_SRV_TCP;
	    if(get_next(kd, host))
		return 0;
	}
	if((kd->flags & KD_SRV_UDP) == 0 && (kd->flags & KD_LARGE_MSG) == 0) {
	    srv_get_hosts(context, kd, NULL, "udp", "kerberos");
	    kd->flags |= KD_SRV_UDP;
	    if(get_next(kd, host))
		return 0;
	}
	if((kd->flags & KD_SRV_KKDCP) == 0) {
	    srv_get_hosts(context, kd, NULL, "kkdcp", "kerberos");
	    kd->flags |= KD_SRV_KKDCP;
	    if(get_next(kd, host))
		return 0;
	}
	if((kd->flags & KD_SRV_HTTP) == 0) {
	    srv_get_hosts(context, kd, NULL, "http", "kerberos");
	    kd->flags |= KD_SRV_HTTP;
	    if(get_next(kd, host))
		return 0;
	}
    }

    while((kd->flags & KD_FALLBACK) == 0) {
	ret = fallback_get_hosts(context, kd, "kerberos",
				 kd->def_port,
				 krbhst_get_default_proto(kd));
	if(ret)
	    return ret;
	if(get_next(kd, host))
	    return 0;
    }

    _krb5_debugx(context, 0, "No KDC entries found for %s", kd->realm);

    return KRB5_KDC_UNREACH; /* XXX */
}

static krb5_error_code
admin_get_next(krb5_context context,
	       struct krb5_krbhst_data *kd,
	       krb5_krbhst_info **host)
{
    krb5_error_code ret;

    if ((kd->flags & KD_PLUGIN) == 0) {
	plugin_get_hosts(context, kd, locate_service_kadmin);
	kd->flags |= KD_PLUGIN;
	if(get_next(kd, host))
	    return 0;
    }

    if((kd->flags & KD_CONFIG) == 0) {
	config_get_hosts(context, kd, "admin_server");
	kd->flags |= KD_CONFIG;
	if(get_next(kd, host))
	    return 0;
    }

    if (kd->flags & KD_CONFIG_EXISTS) {
	_krb5_debugx(context, 1,
		    "Configuration exists for realm %s, wont go to DNS",
		    kd->realm);
	return KRB5_KDC_UNREACH;
    }

    if(context->srv_lookup) {
	if((kd->flags & KD_SRV_TCP) == 0) {
	    srv_get_hosts(context, kd, NULL, "tcp", "kerberos-adm");
	    kd->flags |= KD_SRV_TCP;
	    if(get_next(kd, host))
		return 0;
	}
    }

    if (krbhst_empty(kd)
	&& (kd->flags & KD_FALLBACK) == 0) {
	ret = fallback_get_hosts(context, kd, "kerberos",
				 kd->def_port,
				 krbhst_get_default_proto(kd));
	if(ret)
	    return ret;
	kd->flags |= KD_FALLBACK;
	if(get_next(kd, host))
	    return 0;
    }

    _krb5_debugx(context, 0, "No admin entries found for realm %s", kd->realm);

    return KRB5_KDC_UNREACH;	/* XXX */
}

static krb5_error_code
kpasswd_get_next(krb5_context context,
		 struct krb5_krbhst_data *kd,
		 krb5_krbhst_info **host)
{
    krb5_error_code ret;

    if ((kd->flags & KD_PLUGIN) == 0) {
	plugin_get_hosts(context, kd, locate_service_kpasswd);
	kd->flags |= KD_PLUGIN;
	if(get_next(kd, host))
	    return 0;
    }

    if((kd->flags & KD_CONFIG) == 0) {
	config_get_hosts(context, kd, "kpasswd_server");
	kd->flags |= KD_CONFIG;
	if(get_next(kd, host))
	    return 0;
    }

    if (kd->flags & KD_CONFIG_EXISTS) {
	_krb5_debugx(context, 1,
		    "Configuration exists for realm %s, wont go to DNS",
		    kd->realm);
	return KRB5_KDC_UNREACH;
    }

    if(context->srv_lookup) {
	if((kd->flags & KD_SRV_UDP) == 0) {
	    srv_get_hosts(context, kd, NULL, "udp", "kpasswd");
	    kd->flags |= KD_SRV_UDP;
	    if(get_next(kd, host))
		return 0;
	}
	if((kd->flags & KD_SRV_TCP) == 0) {
	    srv_get_hosts(context, kd, NULL, "tcp", "kpasswd");
	    kd->flags |= KD_SRV_TCP;
	    if(get_next(kd, host))
		return 0;
	}
    }

    /* no matches -> try admin */

    if (krbhst_empty(kd)) {
	kd->flags = 0;
	kd->port  = kd->def_port;
	kd->get_next = admin_get_next;
	ret = (*kd->get_next)(context, kd, host);
	if (ret == 0)
	    (*host)->proto = krbhst_get_default_proto(kd);
	return ret;
    }

    _krb5_debugx(context, 0, "No kpasswd entries found for realm %s", kd->realm);

    return KRB5_KDC_UNREACH;
}

static void
krbhost_dealloc(void *ptr)
{
    struct krb5_krbhst_data *handle = (struct krb5_krbhst_data *)ptr;
    krb5_krbhst_info *h, *next;

    for (h = handle->hosts; h != NULL; h = next) {
	next = h->next;
	_krb5_free_krbhst_info(h);
    }
    if (handle->hostname)
	free(handle->hostname);
    if (handle->sitename)
	free(handle->sitename);
#if __APPLE__
    if (handle->main_sd) {
	DNSServiceRef client = handle->main_sd;
	handle->main_sd = NULL;

	dispatch_async((dispatch_queue_t)handle->srv_queue, ^{
	    DNSServiceRefDeallocate(client);
	});
    }
#endif
    if (handle->srv_queue)
	heim_queue_release(handle->srv_queue);
    if (handle->addrinfo_queue)
	heim_queue_release(handle->addrinfo_queue);
    HEIMDAL_MUTEX_destroy(&handle->mutex);

    free(handle->realm);
}

static struct krb5_krbhst_data*
common_init(krb5_context context,
	    const char *service,
	    const char *realm,
	    int flags)
{
    struct krb5_krbhst_data *kd;

    if ((kd = heim_uniq_alloc(sizeof(*kd), "krbhst-context", krbhost_dealloc)) == NULL)
	return NULL;

    if((kd->realm = strdup(realm)) == NULL) {
	heim_release(kd);
	return NULL;
    }

    kd->srv_queue = heim_queue_create("krbhst-srv", NULL);
    if (kd->srv_queue == NULL) {
	heim_release(kd);
	return NULL;
    }
    kd->addrinfo_queue = heim_queue_create("krbhst-addrinfo", NULL);
    if (kd->addrinfo_queue == NULL) {
	heim_release(kd);
	return NULL;
    }

    _krb5_debugx(context, 2, "Trying to find service %s for realm %s flags %x",
		service, realm, flags);

    /* For 'realms' without a . do not even think of going to DNS */
    if (!strchr(realm, '.'))
	kd->flags |= KD_CONFIG_EXISTS;

    if (flags & KRB5_KRBHST_FLAGS_LARGE_MSG)
	kd->flags |= KD_LARGE_MSG;
    kd->end = kd->index = &kd->hosts;

    HEIMDAL_MUTEX_init(&kd->mutex);

    return kd;
}

/*
 * initialize `handle' to look for hosts of type `type' in realm `realm'
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_krbhst_init(krb5_context context,
		 const char *realm,
		 unsigned int type,
		 krb5_krbhst_handle *handle)
{
    return krb5_krbhst_init_flags(context, realm, type, 0, handle);
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_krbhst_init_flags(krb5_context context,
		       const char *realm,
		       unsigned int type,
		       int flags,
		       krb5_krbhst_handle *handle)
{
    struct krb5_krbhst_data *kd;
    krb5_error_code (*next)(krb5_context, struct krb5_krbhst_data *,
			    krb5_krbhst_info **);
    int def_port;
    const char *service;

    *handle = NULL;

    switch(type) {
    case KRB5_KRBHST_KDC:
	next = kdc_get_next;
	def_port = ntohs(krb5_getportbyname (context, "kerberos", "udp", 88));
	service = "kdc";
	break;
    case KRB5_KRBHST_ADMIN:
	next = admin_get_next;
	def_port = ntohs(krb5_getportbyname (context, "kerberos-adm",
					     "tcp", 749));
	service = "admin";
	break;
    case KRB5_KRBHST_CHANGEPW:
	next = kpasswd_get_next;
	def_port = ntohs(krb5_getportbyname (context, "kpasswd", "udp",
					     KPASSWD_PORT));
	service = "change_password";
	break;
    default:
	krb5_set_error_message(context, ENOTTY,
			       N_("unknown krbhst type (%u)", ""), type);
	return ENOTTY;
    }
    if((kd = common_init(context, service, realm, flags)) == NULL)
	return ENOMEM;
    kd->get_next = next;
    kd->def_port = def_port;
    *handle = kd;
    return 0;
}

/*
 * return the next host information from `handle' in `host'
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_krbhst_next(krb5_context context,
		 krb5_krbhst_handle handle,
		 krb5_krbhst_info **host)
{
    if(get_next(handle, host))
	return 0;

    return (*handle->get_next)(context, handle, host);
}

/*
 *
 */

static void
krbhst_callback(void *ctx)
{
    krb5_krbhst_info *host = ctx;
    krb5_krbhst_handle handle = host->__private;

    if (handle->callback) {
	handle->callback(handle->userctx, host);
	heim_release(handle);
    }
}

static void
krbhst_callback_done(void *ctx)
{
    krb5_krbhst_handle handle = ctx;
    void (*callback)(void *, krb5_krbhst_info *) = handle->callback;
    void *userctx = handle->userctx;

    handle->callback = NULL;
    handle->userctx = NULL;

    if (callback)
	callback(userctx, NULL);

    heim_release(handle);
}

static void
krbhst_callback_cancel(void *ctx)
{
    krb5_krbhst_handle handle = ctx;
    handle->callback(handle->userctx, NULL);
    handle->callback = NULL;
}

static void
process_loop(void *ctx)
{
    krb5_krbhst_handle handle = ctx;
    krb5_krbhst_info *hi;
    
    while(krb5_krbhst_next(handle->context, handle, &hi)) {
	hi->__private = heim_retain(handle);
	heim_async_f(handle->srv_queue, hi, krbhst_callback);
    }

    heim_async_f(handle->srv_queue, handle, krbhst_callback_done);
}

/*
 *
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_krbhst_async(krb5_context context,
		   krb5_krbhst_handle handle,
		   void *userctx,
		   void (*callback)(void *userctx, krb5_krbhst_info *host))
{
    handle->context = context;

    handle->userctx = userctx;
    handle->callback = callback;

    heim_async_f(handle->srv_queue, heim_retain(handle), process_loop);

    return 0;
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_krbhst_cancel(krb5_context context,
		    krb5_krbhst_handle handle)
{
    heim_assert(handle->srv_queue != NULL, "cancel non async krbhst");
    heim_assert(handle->callback != NULL, "cancel on already canceled handle");
    heim_async_f(handle->srv_queue, handle, krbhst_callback_cancel);
}

/*
 * return the next host information from `handle' as a host name
 * in `hostname' (or length `hostlen)
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_krbhst_next_as_string(krb5_context context,
			   krb5_krbhst_handle handle,
			   char *hostname,
			   size_t hostlen)
{
    krb5_error_code ret;
    krb5_krbhst_info *host;
    ret = krb5_krbhst_next(context, handle, &host);
    if(ret)
	return ret;
    return krb5_krbhst_format_string(context, host, hostname, hostlen);
}

/*
 *
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_krbhst_set_hostname(krb5_context context,
			 krb5_krbhst_handle handle,
			 const char *hostname)
{
    if (handle->hostname)
	free(handle->hostname);
    handle->hostname = strdup(hostname);
    if (handle->hostname == NULL)
	return krb5_enomem(context);
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_krbhst_set_sitename(krb5_context context,
			 krb5_krbhst_handle handle,
			 const char *sitename)
{
    if (handle->sitename)
	free(handle->sitename);
    handle->sitename = strdup(sitename);
    if (handle->sitename == NULL)
	return krb5_enomem(context);
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_krbhst_set_delgated_uuid(krb5_context context,
			      krb5_krbhst_handle handle,
			      krb5_uuid uuid)
{
    handle->flags |= KD_DELEG_UUID;
    memcpy(handle->delegate_uuid, uuid, sizeof(krb5_uuid));

    _krb5_debugx(context, 5, "krb5_krbhst_set_delegated_uuid: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		 uuid[0],uuid[1],uuid[2],uuid[3],uuid[4],uuid[5],uuid[6],uuid[7],
		 uuid[8],uuid[9],uuid[10],uuid[11],uuid[12],uuid[13],uuid[14],uuid[15]);

    return 0;
}


KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_krbhst_reset(krb5_context context, krb5_krbhst_handle handle)
{
    HEIMDAL_MUTEX_lock(&handle->mutex);
    handle->index = &handle->hosts;
    HEIMDAL_MUTEX_unlock(&handle->mutex);
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_krbhst_free(krb5_context context, krb5_krbhst_handle handle)
{
    heim_release(handle);
}

#ifndef HEIMDAL_SMALLER

/* backwards compatibility ahead */

static krb5_error_code
gethostlist(krb5_context context, const char *realm,
	    unsigned int type, char ***hostlist)
{
    krb5_error_code ret;
    int nhost = 0;
    krb5_krbhst_handle handle;
    char host[MAXHOSTNAMELEN];
    krb5_krbhst_info *hostinfo;

    ret = krb5_krbhst_init(context, realm, type, &handle);
    if (ret)
	return ret;

    while(krb5_krbhst_next(context, handle, &hostinfo) == 0)
	nhost++;
    if(nhost == 0) {
	krb5_set_error_message(context, KRB5_KDC_UNREACH,
			       N_("No KDC found for realm %s", ""), realm);
	return KRB5_KDC_UNREACH;
    }
    *hostlist = calloc(nhost + 1, sizeof(**hostlist));
    if(*hostlist == NULL) {
	krb5_krbhst_free(context, handle);
	return ENOMEM;
    }

    krb5_krbhst_reset(context, handle);
    nhost = 0;
    while(krb5_krbhst_next_as_string(context, handle,
				     host, sizeof(host)) == 0) {
	if(((*hostlist)[nhost++] = strdup(host)) == NULL) {
	    krb5_free_krbhst(context, *hostlist);
	    krb5_krbhst_free(context, handle);
	    return ENOMEM;
	}
    }
    (*hostlist)[nhost] = NULL;
    krb5_krbhst_free(context, handle);
    return 0;
}

/*
 * return an malloced list of kadmin-hosts for `realm' in `hostlist'
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_krb_admin_hst (krb5_context context,
			const krb5_realm *realm,
			char ***hostlist)
{
    return gethostlist(context, *realm, KRB5_KRBHST_ADMIN, hostlist);
}

/*
 * return an malloced list of changepw-hosts for `realm' in `hostlist'
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_krb_changepw_hst (krb5_context context,
			   const krb5_realm *realm,
			   char ***hostlist)
{
    return gethostlist(context, *realm, KRB5_KRBHST_CHANGEPW, hostlist);
}

/*
 * return an malloced list of 524-hosts for `realm' in `hostlist'
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_krb524hst (krb5_context context,
		    const krb5_realm *realm,
		    char ***hostlist)
{
    return gethostlist(context, *realm, KRB5_KRBHST_KRB524, hostlist);
}

/*
 * return an malloced list of KDC's for `realm' in `hostlist'
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_krbhst (krb5_context context,
		 const krb5_realm *realm,
		 char ***hostlist)
{
    return gethostlist(context, *realm, KRB5_KRBHST_KDC, hostlist);
}

/*
 * free all the memory allocated in `hostlist'
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_free_krbhst (krb5_context context,
		  char **hostlist)
{
    char **p;

    for (p = hostlist; *p; ++p)
	free (*p);
    free (hostlist);
    return 0;
}

#endif /* HEIMDAL_SMALLER */
