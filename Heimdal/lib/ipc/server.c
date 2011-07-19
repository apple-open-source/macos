/*
 * Copyright (c) 2009 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
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

#include "hi_locl.h"
#include "heimbase.h"
#include <assert.h>

struct heim_sipc {
    int (*release)(heim_sipc ctx);
    heim_ipc_callback callback;
    void *userctx;
    void *mech;
};


#if defined(__APPLE__) && defined(HAVE_GCD)

#include "heim_ipcServer.h"
#include "heim_ipc_reply.h"
#include "heim_ipc_async.h"

static dispatch_source_t timer;
static dispatch_queue_t timerq;
static uint64_t timeoutvalue;

static dispatch_queue_t eventq;

static dispatch_queue_t workq;

static heim_array_t dispatch_signals;

static void
default_timer_ev(void)
{
    exit(0);
}

static void (*timer_ev)(void) = default_timer_ev;

static void
set_timer(void)
{
    dispatch_source_set_timer(timer,
			      dispatch_time(DISPATCH_TIME_NOW,
					    timeoutvalue * NSEC_PER_SEC),
			      timeoutvalue * NSEC_PER_SEC, 1000000); 
}

static void
init_globals(void)
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{ 
	timerq = dispatch_queue_create("hiem-sipc-timer-q", NULL);
        timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, timerq);
	dispatch_source_set_event_handler(timer, ^{ timer_ev(); } );

	workq = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
	eventq = dispatch_queue_create("heim-ipc.event-queue", NULL);
	dispatch_signals = heim_array_create();
    });
}

void
_heim_ipc_suspend_timer(void)
{
    dispatch_suspend(timer);
}

void
_heim_ipc_restart_timer(void)
{
    dispatch_sync(timerq, ^{ set_timer(); });
    dispatch_resume(timer);
}

struct mach_service {
    mach_port_t sport;
    dispatch_source_t source;
    dispatch_queue_t queue;
};

struct mach_call_ctx {
    mach_port_t reply_port;
    heim_icred cred;
    heim_idata req;
};


static void
mach_complete_sync(heim_sipc_call ctx, int returnvalue, heim_idata *reply)
{
    struct mach_call_ctx *s = (struct mach_call_ctx *)ctx;
    heim_ipc_message_inband_t replyin;
    mach_msg_type_number_t replyinCnt;
    heim_ipc_message_outband_t replyout;
    mach_msg_type_number_t replyoutCnt;

    if (returnvalue) {
	/* on error, no reply */
	replyinCnt = 0;
	replyout = 0; replyoutCnt = 0;
    } else if (reply->length < 2048) {
	replyinCnt = reply->length;
	memcpy(replyin, reply->data, replyinCnt);
	replyout = 0; replyoutCnt = 0;
    } else {
	replyinCnt = 0;
	vm_read(mach_task_self(), 
		     (vm_address_t)reply->data, reply->length,
		     (vm_address_t *)&replyout, &replyoutCnt);
    }

    mheim_ripc_call_reply(s->reply_port, returnvalue,
			  replyin, replyinCnt,
			  replyout, replyoutCnt);

    heim_ipc_free_cred(s->cred);
    free(s->req.data);
    free(s);
    _heim_ipc_restart_timer();
}

static void
mach_complete_async(heim_sipc_call ctx, int returnvalue, heim_idata *reply)
{
    struct mach_call_ctx *s = (struct mach_call_ctx *)ctx;
    heim_ipc_message_inband_t replyin;
    mach_msg_type_number_t replyinCnt;
    heim_ipc_message_outband_t replyout;
    mach_msg_type_number_t replyoutCnt;
    kern_return_t kr;

    if (returnvalue) {
	/* on error, no reply */
	replyinCnt = 0;
	replyout = 0; replyoutCnt = 0;
	kr = KERN_SUCCESS;
    } else if (reply->length < 2048) {
	replyinCnt = reply->length;
	memcpy(replyin, reply->data, replyinCnt);
	replyout = 0; replyoutCnt = 0;
	kr = KERN_SUCCESS;
    } else {
	replyinCnt = 0;
	kr = vm_read(mach_task_self(), 
		     (vm_address_t)reply->data, reply->length,
		     (vm_address_t *)&replyout, &replyoutCnt);
    }

    kr = mheim_aipc_acall_reply(s->reply_port, returnvalue,
				replyin, replyinCnt,
				replyout, replyoutCnt);
    heim_ipc_free_cred(s->cred);
    free(s->req.data);
    memset(s, 0, sizeof(*s));
    free(s);
    _heim_ipc_restart_timer();
}


kern_return_t
mheim_do_call(mach_port_t server_port,
	      audit_token_t client_creds,
	      mach_port_t reply_port,
	      heim_ipc_message_inband_t requestin,
	      mach_msg_type_number_t requestinCnt,
	      heim_ipc_message_outband_t requestout,
	      mach_msg_type_number_t requestoutCnt,
	      int *returnvalue,
	      heim_ipc_message_inband_t replyin,
	      mach_msg_type_number_t *replyinCnt,
	      heim_ipc_message_outband_t *replyout,
	      mach_msg_type_number_t *replyoutCnt)
{
    heim_sipc ctx = dispatch_get_context(dispatch_get_current_queue());
    struct mach_call_ctx *s;
    kern_return_t kr;
    uid_t uid;
    gid_t gid;
    pid_t pid;
    au_asid_t session;

    *replyout = NULL;
    *replyoutCnt = 0;
    *replyinCnt = 0;

    s = malloc(sizeof(*s));
    if (s == NULL)
	return KERN_MEMORY_FAILURE; /* XXX */
    
    s->reply_port = reply_port;
    
    audit_token_to_au32(client_creds, NULL, &uid, &gid, NULL, NULL, &pid, &session, NULL);
    
    kr = _heim_ipc_create_cred(uid, gid, pid, session, &s->cred);
    if (kr) {
	free(s);
	return kr;
    }
    
    _heim_ipc_suspend_timer();
    
    if (requestinCnt) {
	s->req.data = malloc(requestinCnt);
	memcpy(s->req.data, requestin, requestinCnt);
	s->req.length = requestinCnt;
    } else {
	s->req.data = malloc(requestoutCnt);
	memcpy(s->req.data, requestout, requestoutCnt);
	s->req.length = requestoutCnt;
    }
    
    dispatch_async(workq, ^{
	(ctx->callback)(ctx->userctx, &s->req, s->cred,
			mach_complete_sync, (heim_sipc_call)s);
    });

    return MIG_NO_REPLY;
}

kern_return_t
mheim_do_call_request(mach_port_t server_port,
		      audit_token_t client_creds,
		      mach_port_t reply_port,
		      heim_ipc_message_inband_t requestin,
		      mach_msg_type_number_t requestinCnt,
		      heim_ipc_message_outband_t requestout,
		      mach_msg_type_number_t requestoutCnt)
{
    heim_sipc ctx = dispatch_get_context(dispatch_get_current_queue());
    struct mach_call_ctx *s;
    kern_return_t kr;
    uid_t uid;
    gid_t gid;
    pid_t pid;
    au_asid_t session;
    
    s = malloc(sizeof(*s));
    if (s == NULL)
	return KERN_MEMORY_FAILURE; /* XXX */
    
    s->reply_port = reply_port;
    
    audit_token_to_au32(client_creds, NULL, &uid, &gid, NULL, NULL, &pid, &session, NULL);
    
    kr = _heim_ipc_create_cred(uid, gid, pid, session, &s->cred);
    if (kr) {
	free(s);
	return kr;
    }
    
    _heim_ipc_suspend_timer();
    
    if (requestinCnt) {
	s->req.data = malloc(requestinCnt);
	memcpy(s->req.data, requestin, requestinCnt);
	s->req.length = requestinCnt;
    } else {
	s->req.data = malloc(requestoutCnt);
	memcpy(s->req.data, requestout, requestoutCnt);
	s->req.length = requestoutCnt;
    }
    
    dispatch_async(workq, ^{
	(ctx->callback)(ctx->userctx, &s->req, s->cred,
			mach_complete_async, (heim_sipc_call)s);
    });
    
    return KERN_SUCCESS;
}

static int
mach_init(const char *service, mach_port_t sport, heim_sipc ctx)
{
    struct mach_service *s;
    char *name;

    init_globals();

    s = calloc(1, sizeof(*s));
    if (s == NULL)
	return ENOMEM;

    asprintf(&name, "heim-ipc-mach-%s", service);

    s->queue = dispatch_queue_create(name, NULL);
    free(name);
    s->sport = sport;

    s->source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, 
				       s->sport, 0, s->queue);
    if (s->source == NULL) {
	dispatch_release(s->queue);
	free(s);
	return ENOMEM;
    }
    ctx->mech = s;

    dispatch_set_context(s->queue, ctx);
    dispatch_set_context(s->source, s);

    dispatch_source_set_event_handler(s->source, ^{
	    dispatch_mig_server(s->source, sizeof(union __RequestUnion__mheim_do_mheim_ipc_subsystem), mheim_ipc_server);
	});

    dispatch_source_set_cancel_handler(s->source, ^{
	    heim_sipc ctx = dispatch_get_context(dispatch_get_current_queue());
	    struct mach_service *st = ctx->mech;
	    mach_port_mod_refs(mach_task_self(), st->sport, 
			       MACH_PORT_RIGHT_RECEIVE, -1);
	    dispatch_release(st->queue);
	    dispatch_release(st->source);
	    free(st);
	    free(ctx);
	});

    dispatch_resume(s->source);

    return 0;
}

static int
mach_release(heim_sipc ctx)
{
    struct mach_service *s = ctx->mech;
    dispatch_source_cancel(s->source);
    dispatch_release(s->source);
    return 0;
}

static mach_port_t
mach_checkin_or_register(const char *service)
{
    mach_port_t mp;
    kern_return_t kr;

    kr = bootstrap_check_in(bootstrap_port, service, &mp);
    if (kr == KERN_SUCCESS)
	return mp;

#if 0
    /* Pre SnowLeopard version */
    kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &mp);
    if (kr != KERN_SUCCESS)
	return MACH_PORT_NULL;

    kr = mach_port_insert_right(mach_task_self(), mp, mp, 
				MACH_MSG_TYPE_MAKE_SEND);
    if (kr != KERN_SUCCESS) {
	mach_port_destroy(mach_task_self(), mp);
	return MACH_PORT_NULL;
    }

    kr = bootstrap_register(bootstrap_port, rk_UNCONST(service), mp);
    if (kr != KERN_SUCCESS) {
	mach_port_destroy(mach_task_self(), mp);
	return MACH_PORT_NULL;
    }
    return mp;
#else
    return MACH_PORT_NULL;
#endif
}


#endif /* __APPLE__ && HAVE_GCD */


#ifndef HAVE_GCD

/*
 * Signal handler logic for the non GCD case
 */

struct ipc_signal {
    int signo;
    sig_atomic_t signal_set;
    void (*handler)(void *);
    void *ctx;
};

static struct ipc_signal *ipc_signals = NULL;
static size_t num_signals = 0;

static RETSIGTYPE
signal_handler(int signo)
{
    size_t n;
    for (n = 0; n < num_signals; n++)
	if (ipc_signals[n].signo == signo)
	    ipc_signals[n].signal_set = 1;

    SIGRETURN(0);
}

#endif

int
heim_sipc_launchd_mach_init(const char *service,
			    heim_ipc_callback callback,
			    void *user, heim_sipc *ctx)
{
#if defined(__APPLE__) && defined(HAVE_GCD)
    mach_port_t sport = MACH_PORT_NULL;
    heim_sipc c = NULL;
    int ret;

    *ctx = NULL;

    sport = mach_checkin_or_register(service);
    if (sport == MACH_PORT_NULL) {
	ret = ENOENT;
	goto error;
    }

    c = calloc(1, sizeof(*c));
    if (c == NULL) {
	ret = ENOMEM;
	goto error;
    }
    c->release = mach_release;
    c->userctx = user;
    c->callback = callback;
		 
    ret = mach_init(service, sport, c);
    if (ret)
	goto error;

    *ctx = c;
    return 0;
 error:
    if (c)
	free(c);
    if (sport != MACH_PORT_NULL)
	mach_port_mod_refs(mach_task_self(), sport, 
			   MACH_PORT_RIGHT_RECEIVE, -1);
    return ret;
#else /* !(__APPLE__ && HAVE_GCD) */
    *ctx = NULL;
    return EINVAL;
#endif /* __APPLE__ && HAVE_GCD */
}

struct client {
    int fd;
    heim_ipc_callback callback;
    void *userctx;
    int flags;
#define LISTEN_SOCKET	1
#define WAITING_READ	2
#define WAITING_WRITE	4
#define WRITE_RUN	8

#define WAITING_CLOSE	16

#define CLOSE_ON_REPLY	32

#define DGRAM_SOCKET	64
#define HAVE_PKTINFO4	128
#define HAVE_PKTINFO6	256

#define INHERIT_MASK	0xffff0000
#define INCLUDE_ERROR_CODE (1 << 16)
#define ALLOW_HTTP	(1<<18)
    unsigned calls;
    size_t ptr, len;
    uint8_t *inmsg;
    size_t olen;
    uint8_t *outmsg;
#ifdef HAVE_GCD
    dispatch_source_t in;
    dispatch_source_t out;
#endif
    struct sockaddr_storage sa;
    krb5_socklen_t sock_len;

    struct sockaddr_storage localsa;
    krb5_socklen_t localsa_len;
};

#ifndef HAVE_GCD
static unsigned num_clients = 0;
static struct client **clients = NULL;
#endif

static void handle_read(struct client *);
static void handle_write(struct client *);
static int maybe_close(struct client *);

static struct client *
add_new_socket(int fd,
	       int flags,
	       heim_ipc_callback callback,
	       void *userctx)
{
    struct client *c;
    int fileflags;

    c = calloc(1, sizeof(*c));
    if (c == NULL)
	return NULL;
	
    if (flags & (LISTEN_SOCKET|DGRAM_SOCKET)) {
	c->fd = fd;
    } else {
	c->fd = accept(fd, NULL, NULL);
	if (c->fd < 0) {
	    free(c);
	    return NULL;
	}
    }

    c->flags = flags;
    c->callback = callback;
    c->userctx = userctx;

    fileflags = fcntl(c->fd, F_GETFL, 0);
    fcntl(c->fd, F_SETFL, fileflags | O_NONBLOCK);

    /* if its a dgram socket, allocate maximum size upfront */
    if (c->flags & DGRAM_SOCKET) {
	int on = 1;
	c->len = MAX_PACKET_SIZE;
	c->inmsg = emalloc(c->len);
	c->ptr = 0;

#ifdef IPV6_RECVPKTINFO
	setsockopt(c->fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
#endif
#ifdef IP_RECVPKTINFO
	setsockopt(c->fd, IPPROTO_IP, IP_RECVPKTINFO, &on, sizeof(on));
#endif
    }


#ifdef HAVE_GCD
    init_globals();
	
    c->in = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, 
				   c->fd, 0, eventq);
    c->out = dispatch_source_create(DISPATCH_SOURCE_TYPE_WRITE,
				    c->fd, 0, eventq);
    
    dispatch_source_set_event_handler(c->in, ^{ 
	    handle_read(c);
	    if ((c->flags & (WAITING_WRITE|WRITE_RUN)) == WAITING_WRITE) {
		c->flags |= WRITE_RUN;
		dispatch_resume(c->out);
	    }
	    if ((c->flags & WAITING_READ) == 0)
		dispatch_suspend(c->in);
	    maybe_close(c);
	});
    dispatch_source_set_event_handler(c->out, ^{
	    assert((c->flags & WRITE_RUN) != 0);
	    handle_write(c);
	    if ((c->flags & WAITING_WRITE) == 0) {
		c->flags &= ~WRITE_RUN;
		dispatch_suspend(c->out);
	    }
	    maybe_close(c);
	});
    
    dispatch_resume(c->in);
#else
    clients = erealloc(clients, sizeof(clients[0]) * (num_clients + 1));
    clients[num_clients] = c;
    num_clients++;
#endif

    return c;
}

static int
maybe_close(struct client *c)
{
    if (c->calls != 0)
	return 0;
    if (c->flags & (WAITING_READ|WAITING_WRITE))
	return 0;

#ifdef HAVE_GCD
    dispatch_source_cancel(c->in);
    if ((c->flags & WAITING_READ) == 0)
	dispatch_resume(c->in);
    dispatch_release(c->in);

    dispatch_source_cancel(c->out);
    if ((c->flags & WRITE_RUN) == 0) {
	c->flags |= WRITE_RUN;
	dispatch_resume(c->out);
    }
    dispatch_release(c->out);
#endif
    close(c->fd); /* ref count fd close */
    free(c->inmsg);
    free(c);
    return 1;
}


struct socket_call {
    heim_idata in;
    struct client *c;
    heim_icred cred;
};

static void
output_data(struct client *c, const void *data, size_t len)
{
    if (c->olen + len < c->olen)
	abort();
    if (len) {
	c->outmsg = erealloc(c->outmsg, c->olen + len);
	memcpy(&c->outmsg[c->olen], data, len);
	c->olen += len;
	c->flags |= WAITING_WRITE;
    }
}

static void
socket_complete(heim_sipc_call ctx, int returnvalue, heim_idata *reply)
{
    struct socket_call *sc = (struct socket_call *)ctx;
    struct client *c = sc->c;

    /* double complete ? */
    if (c == NULL)
	abort();

    if ((c->flags & WAITING_CLOSE) == 0) {
	uint32_t u32;

	/* length */
	if ((c->flags & DGRAM_SOCKET) == 0) {
	    u32 = htonl(reply->length);
	    output_data(c, &u32, sizeof(u32));
	}

	/* return value */
	if (c->flags & INCLUDE_ERROR_CODE) {
	    u32 = htonl(returnvalue);
	    output_data(c, &u32, sizeof(u32));
	}

	/* data */
	output_data(c, reply->data, reply->length);

	if ((c->flags & (WRITE_RUN|WAITING_WRITE)) == WAITING_WRITE) {
	    c->flags |= WRITE_RUN;
	    dispatch_resume(c->out);
	}

	/* if HTTP, close connection */
	if (c->flags & CLOSE_ON_REPLY) {
	    c->flags |= WAITING_CLOSE;
	    c->flags &= ~WAITING_READ;
	}
    }

    c->calls--;

    free(sc->in.data);
    sc->c = NULL; /* so we can catch double complete */
    heim_ipc_free_cred(sc->cred);
    sc->cred = NULL;
    free(sc);

    maybe_close(c);
}

/* remove HTTP %-quoting from buf */
static int
de_http(char *buf)
{
    unsigned char *p, *q;
    for(p = q = (unsigned char *)buf; *p; p++, q++) {
	if (*p == '%' && isxdigit(p[1]) && isxdigit(p[2])) {
	    unsigned int x;
	    if (sscanf((char *)p + 1, "%2x", &x) != 1)
		return -1;
	    *q = x;
	    p += 2;
	} else
	    *q = *p;
    }
    *q = '\0';
    return 0;
}

static struct socket_call *
handle_http_tcp(struct client *c, heim_icred cred)
{
    struct socket_call *cs;
    char *s, *p, *t;
    void *data;
    char *proto;
    int len;

    s = (char *)c->inmsg;

    /* If its a multi line query, truncate off the first line */
    p = strstr(s, "\r\n");
    if (p)
	*p = 0;

    p = NULL;
    t = strtok_r(s, " \t", &p);
    if (t == NULL)
	return NULL;

    t = strtok_r(NULL, " \t", &p);
    if (t == NULL)
	return NULL;

    data = malloc(strlen(t));
    if (data == NULL)
	return NULL;

    if (*t == '/')
	t++;
    if (de_http(t) != 0) {
	free(data);
	return NULL;
    }
    proto = strtok_r(NULL, " \t", &p);
    if (proto == NULL) {
	free(data);
	return NULL;
    }
    len = base64_decode(t, data);

    if (len <= 0) {
	const char *msg =
	    "HTTP/1.0 404 Not found\r\n"
	    "Server: Heimdal/" VERSION "\r\n"
	    "Cache-Control: no-cache\r\n"
	    "Pragma: no-cache\r\n"
	    "Content-type: text/html\r\n"
	    "Content-transfer-encoding: 8bit\r\n\r\n"
	    "<TITLE>404 Not found</TITLE>\r\n"
	    "<H1>404 Not found</H1>\r\n"
	    "That page doesn't exist, maybe you are looking for "
	    "<A HREF=\"http://www.h5l.org/\">Heimdal</A>?\r\n";
	free(data);
	output_data(c, msg, strlen(msg));
	return NULL;
    }

    cs = emalloc(sizeof(*cs));
    cs->c = c;
    cs->in.data = data;
    cs->in.length = len;
    cs->cred = cred;
    c->ptr = 0;

    {
	const char *msg =
	    " 200 OK\r\n"
	    "Server: Heimdal/" VERSION "\r\n"
	    "Cache-Control: no-cache\r\n"
	    "Pragma: no-cache\r\n"
	    "Content-type: application/octet-stream\r\n"
	    "Content-transfer-encoding: binary\r\n\r\n";
	output_data(c, proto, strlen(proto));
	output_data(c, msg, strlen(msg));
    }

    return cs;
}

static void
capture_localaddr(struct client *c, struct msghdr *hdr)
{
    struct cmsghdr *cm;

    c->flags &= ~(HAVE_PKTINFO4|HAVE_PKTINFO6);

    for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(hdr); cm;
	 cm = (struct cmsghdr *)CMSG_NXTHDR(hdr, cm)) {
#ifdef IPV6_RECVPKTINFO
	if (cm->cmsg_level == IPPROTO_IPV6 && cm->cmsg_type == IPV6_PKTINFO &&
	    cm->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo))) {
	    struct in6_pktinfo *pi6;

	    pi6 = (struct in6_pktinfo *)(CMSG_DATA(cm));

	    memcpy(((struct sockaddr *)&c->localsa)->sa_data, &pi6->ipi6_addr,
		   sizeof(pi6->ipi6_addr));
	    c->localsa.ss_family = AF_INET6;
	    c->localsa.ss_len = sizeof(struct sockaddr_in6);
	    c->localsa_len = sizeof(struct sockaddr_in6);
	    c->flags |= HAVE_PKTINFO6;
	    return;
	}
#endif
#ifdef IP_RECVPKTINFO
	if (cm->cmsg_level == IPPROTO_IP && cm->cmsg_type == IP_PKTINFO &&
	    cm->cmsg_len == CMSG_LEN(sizeof(struct in_pktinfo))) {
	    struct in_pktinfo *pi4;
	    pi4 = (struct in_pktinfo *)(CMSG_DATA(cm));

	    memcpy(((struct sockaddr *)&c->localsa)->sa_data, &pi4->ipi_addr,
		   sizeof(pi4->ipi_addr));
	    c->localsa.ss_family = AF_INET;
	    c->localsa.ss_len = sizeof(struct sockaddr_in);
	    c->localsa_len = sizeof(struct sockaddr_in);
	    c->flags |= HAVE_PKTINFO4;
	    return;
	}
#endif
    }
}

static void
handle_read(struct client *c)
{
    heim_icred cred = NULL;
    ssize_t len;
    uint32_t dlen;
    int ret;

    if (c->flags & LISTEN_SOCKET) {
	(void)add_new_socket(c->fd,
			     WAITING_READ | (c->flags & INHERIT_MASK),
			     c->callback,
			     c->userctx);
	return;
    }

    if (c->len - c->ptr < 1024) {
	c->inmsg = erealloc(c->inmsg,
			    c->len + 1024);
	c->len += 1024;
    }
    
    struct msghdr hdr;
    struct iovec iov[1];

    size_t cmsglen = 0;
#ifdef IPV6_RECVPKTINFO
    cmsglen += CMSG_SPACE(sizeof(struct in6_pktinfo));
#endif
#ifdef IP_RECVPKTINFO
    cmsglen += CMSG_SPACE(sizeof(struct in_pktinfo));
#endif
    void *cmsg = malloc(cmsglen);

    iov[0].iov_base = c->inmsg + c->ptr;
    iov[0].iov_len = c->len - c->ptr;
    hdr.msg_name = (void *)&c->sa;
    hdr.msg_namelen = sizeof(c->sa);
    hdr.msg_iov = iov;
    hdr.msg_iovlen = 1;
    hdr.msg_control = cmsg;
    hdr.msg_controllen = cmsglen;

    len = recvmsg(c->fd, &hdr, 0);
    if (len < 0 || ((c->flags & DGRAM_SOCKET) == 0 && len == 0)) {
	free(cmsg);
	c->flags |= WAITING_CLOSE;
	c->flags &= ~WAITING_READ;
	return;
    }
    c->ptr += len;
    if (c->ptr > c->len)
	abort();
    
    c->sock_len = hdr.msg_namelen;

    if (c->flags & DGRAM_SOCKET)
	capture_localaddr(c, &hdr);

    free(cmsg);

    /* handle dgram sockets */
    if (c->flags & DGRAM_SOCKET) {
	struct socket_call *cs;

	ret = _heim_ipc_create_network_cred(hdr.msg_name, hdr.msg_namelen, &cred);
	if (ret)
	    abort();

	cs = emalloc(sizeof(*cs));
	cs->c = c;
	cs->in.data = emalloc(c->ptr);
	memcpy(cs->in.data, c->inmsg, c->ptr);
	cs->in.length = c->ptr;
	cs->cred = cred;
	
	c->ptr = 0;
	
	c->calls++;
	c->callback(c->userctx, &cs->in,
		    cred, socket_complete,
		    (heim_sipc_call)cs);
	return;
    }

    while (c->ptr >= sizeof(dlen)) {
	struct socket_call *cs;
	
	if ((c->flags & ALLOW_HTTP) && strncmp((char *)c->inmsg, "GET ", 4) == 0) {

	    if (strncmp((char *)c->inmsg + c->ptr - 4, "\r\n\r\n", 4) != 0)
		break;

	    c->sock_len = sizeof(c->sa);
	    ret = getpeername(c->fd, (struct sockaddr *)&c->sa, &c->sock_len);
	    if (ret)
		abort();

	    ret = _heim_ipc_create_network_cred((struct sockaddr *)&c->sa, c->sock_len, &cred);
	    if (ret)
		abort();

	    /* remove the trailing \r\n\r\n so the string is NUL terminated */
	    c->inmsg[c->ptr - 4] = '\0';

	    c->flags |= CLOSE_ON_REPLY;

	    cs = handle_http_tcp(c, cred);
	    if (cs == NULL) {
		heim_ipc_free_cred(cred);
		c->flags |= WAITING_CLOSE;
		c->flags &= ~WAITING_READ;
		break;
	    }
	} else {
	    memcpy(&dlen, c->inmsg, sizeof(dlen));
	    dlen = ntohl(dlen);

	    if (dlen > MAX_PACKET_SIZE) {
		c->flags |= WAITING_CLOSE;
		c->flags &= ~WAITING_READ;
		return;
	    }
	    if (dlen > c->ptr - sizeof(dlen)) {
		break;
	    }
	
	    c->sock_len = sizeof(c->sa);
	    ret = getpeername(c->fd, (struct sockaddr *)&c->sa, &c->sock_len);
	    if (ret)
		abort();

	    ret = _heim_ipc_create_network_cred((struct sockaddr *)&c->sa, c->sock_len, &cred);
	    if (ret)
		abort();

	    cs = emalloc(sizeof(*cs));
	    cs->c = c;
	    cs->in.data = emalloc(dlen);
	    memcpy(cs->in.data, c->inmsg + sizeof(dlen), dlen);
	    cs->in.length = dlen;
	    cs->cred = cred;
	
	    c->ptr -= sizeof(dlen) + dlen;
	    memmove(c->inmsg,
		    c->inmsg + sizeof(dlen) + dlen,
		    c->ptr);
	}
	
	c->calls++;
	c->callback(c->userctx, &cs->in,
		    cred, socket_complete,
		    (heim_sipc_call)cs);
    }
}

static void
handle_write(struct client *c)
{
    ssize_t len;

    if (c->olen == 0)
	abort();

    if (c->flags & DGRAM_SOCKET) {
	struct msghdr hdr;
	struct iovec iov[1];
	struct cmsghdr *cm;
	void *cmsg;
	size_t cmsglen = 0;

#ifdef IPV6_RECVPKTINFO
	cmsglen += CMSG_SPACE(sizeof(struct in6_pktinfo));
#endif
#ifdef IP_RECVPKTINFO
	cmsglen += CMSG_SPACE(sizeof(struct in_pktinfo));
#endif
	cmsg = calloc(1, cmsglen);
	if (cmsg == NULL)
	    abort();

	iov[0].iov_base = c->outmsg;
	iov[0].iov_len = c->olen;

	hdr.msg_name = (void *)&c->sa;
	hdr.msg_namelen = c->sock_len;
	hdr.msg_iov = iov;
	hdr.msg_iovlen = 1;
	hdr.msg_control = cmsg;
	hdr.msg_controllen = cmsglen;

	if (c->flags & HAVE_PKTINFO6) {
#ifdef IPV6_RECVPKTINFO
	    struct in6_pktinfo *pi6;

	    cm = CMSG_FIRSTHDR(&hdr);
	    cm->cmsg_level = IPPROTO_IPV6;
	    cm->cmsg_type = IPV6_PKTINFO;
	    cm->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	    pi6 = (struct in6_pktinfo *)CMSG_DATA(cm);

	    memcpy(&pi6->ipi6_addr, ((struct sockaddr *)&c->localsa)->sa_data,
		   sizeof(pi6->ipi6_addr));
	    pi6->ipi6_ifindex = 0;

	    hdr.msg_controllen = CMSG_SPACE(sizeof(struct in6_pktinfo));
#endif
	}else if (c->flags & HAVE_PKTINFO4) {
#ifdef IP_RECVPKTINFO
	    struct in_pktinfo *pi4;

	    cm = CMSG_FIRSTHDR(&hdr);
	    cm->cmsg_level = IPPROTO_IP;
	    cm->cmsg_type = IP_PKTINFO;
	    cm->cmsg_len = CMSG_LEN(sizeof(struct in_pktinfo));
	    pi4 = (struct in_pktinfo *)CMSG_DATA(cm);

	    memcpy(&pi4->ipi_spec_dst, ((struct sockaddr *)&c->localsa)->sa_data,
		   sizeof(pi4->ipi_spec_dst));
	    pi4->ipi_ifindex = 0;

	    hdr.msg_controllen = CMSG_SPACE(sizeof(struct in_pktinfo));
#endif
	}

	len = sendmsg(c->fd, &hdr, 0);
	if (cmsg)
	    free(cmsg);
    } else {
	len = sendto(c->fd, c->outmsg, c->olen, 0, NULL, 0);
    }
    if (len < 0) {
	/* don't close DGRAM sockets on write failure since we don't
	   know that the failure came from, maybe we lost the address
	   and PKTINFO is all upset now, let the read path handle
	   failure for real */
	if ((c->flags & DGRAM_SOCKET) == 0) {
	    c->flags |= WAITING_CLOSE;
	    c->flags &= ~(WAITING_WRITE);
	} else {
	    c->olen = 0;
	    c->flags &= ~(WAITING_WRITE);
	    free(c->outmsg);
	    c->outmsg = NULL;
	}
    } else if (c->olen != len) {
	memmove(&c->outmsg[0], &c->outmsg[len], c->olen - len);
	c->olen -= len;
    } else {
	c->olen = 0;
	free(c->outmsg);
	c->outmsg = NULL;
	c->flags &= ~(WAITING_WRITE);
    }
}


#ifndef HAVE_GCD

static void
process_loop(void)
{
    struct pollfd *fds;
    unsigned n;
    unsigned num_fds;
    int ret;

    while (num_clients > 0) {

	fds = malloc(num_clients * sizeof(fds[0]));
	if (fds == NULL)
	    abort();

	num_fds = num_clients;

	for (n = 0 ; n < num_fds; n++) {
	    fds[n].fd = clients[n]->fd;
	    fds[n].events = 0;
	    if (clients[n]->flags & WAITING_READ)
		fds[n].events |= POLLIN;
	    if (clients[n]->flags & WAITING_WRITE)
		fds[n].events |= POLLOUT;
	    
	    fds[n].revents = 0;
	}

	poll(fds, num_fds, -1);

	for (n = 0 ; n < num_fds; n++) {
	    if (clients[n] == NULL)
		continue;
	    if (fds[n].revents & POLLERR) {
		clients[n]->flags |= WAITING_CLOSE;
		continue;
	    }

	    if (fds[n].revents & POLLIN)
		handle_read(clients[n]);
	    if (fds[n].revents & POLLOUT)
		handle_write(clients[n]);
	}

	n = 0;
	while (n < num_clients) {
	    struct client *c = clients[n];
	    if (maybe_close(c)) {
		if (n < num_clients - 1)
		    clients[n] = clients[num_clients - 1];
		num_clients--;
	    } else
		n++;
	}

	free(fds);
	
	for (n = 0; n < num_signals; n++) {
	    if (ipc_signals[n].signal_set) {
		ipc_signals[n].signal_set = 0;
		ipc_signals[n].handler(ipc_signals[n].ctx);
	    }
	}
    }
}

#endif

static int
socket_release(heim_sipc ctx)
{
    struct client *c = ctx->mech;
    c->flags |= WAITING_CLOSE;
    return 0;
}

int
heim_sipc_stream_listener(int fd, int type,
			  heim_ipc_callback callback,
			  void *user, heim_sipc *ctx)
{
    heim_sipc ct = calloc(1, sizeof(*ct));
    struct client *c;
    int flags = LISTEN_SOCKET|WAITING_READ;

    if ((type & HEIM_SIPC_TYPE_IPC) && (type & (HEIM_SIPC_TYPE_UINT32|HEIM_SIPC_TYPE_HTTP))) {
	free(ct);
	return EINVAL;
    }

    if (type & HEIM_SIPC_TYPE_ONE_REQUEST) {
	flags |= CLOSE_ON_REPLY;
	type &= ~HEIM_SIPC_TYPE_ONE_REQUEST;
    }

    switch (type) {
    case HEIM_SIPC_TYPE_IPC:
	c = add_new_socket(fd, flags|INCLUDE_ERROR_CODE, callback, user);
	break;
    case HEIM_SIPC_TYPE_UINT32:
	c = add_new_socket(fd, flags, callback, user);
	break;
    case HEIM_SIPC_TYPE_HTTP:
    case HEIM_SIPC_TYPE_UINT32|HEIM_SIPC_TYPE_HTTP:
	c = add_new_socket(fd, flags|ALLOW_HTTP, callback, user);
	break;
    default:
	free(ct);
	return EINVAL;
    }

    ct->mech = c;
    ct->release = socket_release;
    *ctx = ct;
    return 0;
}

int
heim_sipc_service_dgram(int fd, int type,
			heim_ipc_callback callback,
			void *user, heim_sipc *ctx)
{
    heim_sipc ct = calloc(1, sizeof(*ct));

    if (type != 0) {
	free(ct);
	return EINVAL;
    }

    ct->mech = add_new_socket(fd, WAITING_READ|DGRAM_SOCKET, callback, user);
    if (ct->mech == NULL) {
	free(ct);
	return ENOMEM;
    }

    ct->release = socket_release;
    *ctx = ct;
    return 0;
}

int
heim_sipc_service_unix(const char *service,
		       heim_ipc_callback callback,
		       void *user, heim_sipc *ctx)
{
    struct sockaddr_un un;
    int fd, ret;

    un.sun_family = AF_UNIX;

    snprintf(un.sun_path, sizeof(un.sun_path),
	     "/var/run/.heim_%s-socket", service);
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
	return errno;

    socket_set_reuseaddr(fd, 1);
#ifdef LOCAL_CREDS
    {
	int one = 1;
	setsockopt(fd, 0, LOCAL_CREDS, (void *)&one, sizeof(one));
    }
#endif

    unlink(un.sun_path);

    if (bind(fd, (struct sockaddr *)&un, sizeof(un)) < 0) {
	close(fd);
	return errno;
    }

    if (listen(fd, SOMAXCONN) < 0) {
	close(fd);
	return errno;
    }

    chmod(un.sun_path, 0666);

    ret = heim_sipc_stream_listener(fd, HEIM_SIPC_TYPE_IPC,
				    callback, user, ctx);
    if (ret)
	close(fd);

    return ret;
}

/**
 * Set the idle timeout value

 * The timeout event handler is triggered recurrently every idle
 * period `t'. The default action is rather draconian and just calls
 * exit(0), so you might want to change this to something more
 * graceful using heim_sipc_set_timeout_handler().
 */

void
heim_sipc_timeout(time_t t)
{
#ifdef HAVE_GCD
    static dispatch_once_t timeoutonce;
    init_globals();
    dispatch_sync(timerq, ^{
	    timeoutvalue = t;
	    set_timer();
	});
    dispatch_once(&timeoutonce, ^{  dispatch_resume(timer); });
#else
    abort();
#endif
}

/**
 * Set the timeout event handler
 *
 * Replaces the default idle timeout action.
 */

void
heim_sipc_set_timeout_handler(void (*func)(void))
{
#ifdef HAVE_GCD
    init_globals();
    dispatch_sync(timerq, ^{ timer_ev = func; });
#else
    abort();
#endif
}


void
heim_sipc_free_context(heim_sipc ctx)
{
    (ctx->release)(ctx);
}

void
heim_ipc_main(void)
{
#ifdef HAVE_GCD
    dispatch_main();
#else
    process_loop();
#endif
}

void
heim_sipc_signal_handler(int signo, void (*handler)(void *), void *ctx)
{
#ifdef HAVE_GCD
    init_globals();

    dispatch_sync(timerq, ^{
	dispatch_source_t *s;
	
	s = heim_alloc(sizeof(*s), "dispatch-signal-source", NULL);
	if (s == NULL)
	    abort();

	*s = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, signo, 0, workq);
	if (*s == NULL)
	    abort();
	
	dispatch_source_set_event_handler(*s, ^{
	    handler(ctx);
	});
	dispatch_resume(*s);
	heim_array_append_value(dispatch_signals, s);
    });
#else /* !HAVE_GCD */

    ipc_signals = realloc(ipc_signals, sizeof(ipc_signals[0]) * (num_signals + 1));
    if (ipc_signals == NULL)
	abort();

    ipc_signals[num_signals].signo = signo;
    ipc_signals[num_signals].handler = handler;
    ipc_signals[num_signals].ctx = ctx;
    num_signals++;

#ifdef HAVE_SIGACTION
    {
	struct sigaction sa;

	sa.sa_flags = 0;
#ifdef SA_RESTART
	sa.sa_flags |= SA_RESTART;
#endif
	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);

	sigaction(signo, &sa, NULL);
    }
#else /* !HAVE_SIGACTION */
    signal(SIGINT, signal_hander);
#endif /* !HAVE_SIGACTION */

#endif /* !HAVE_GCD */
}
