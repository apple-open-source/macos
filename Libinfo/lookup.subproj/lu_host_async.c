/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Portions Copyright (c) 2002 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <netdb.h>
#include <netdb_async.h>	/* async gethostbyXXX function prototypes */
#include <pthread.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <netinfo/_lu_types.h>
#include <netinfo/lookup.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>

#include "lu_host.h"
#include "lu_utils.h"

extern mach_port_t _lu_port;
extern int _lu_running(void);

extern int h_errno;


#define msgh_request_port       msgh_remote_port
#define msgh_reply_port         msgh_local_port


typedef union {
	gethostbyaddr_async_callback	hostAddr;
	gethostbyname_async_callback	hostName;
	getipnodebyaddr_async_callback	nodeAddr;
	getipnodebyname_async_callback	nodeName;
} a_request_callout_t;

typedef struct a_requests {
	struct a_requests			*next;
	int					retry;
	struct {
		int				proc;
		ooline_data			data;
		unsigned int			dataLen;
		int				want;
	} request;
	mach_port_t				replyPort;
	a_request_callout_t			callout;
	void					*context;
	struct hostent				*hent;		/* if reply known in XXX_start() */
} a_requests_t;

static a_requests_t		*a_requests	= NULL;
static pthread_mutex_t		a_requests_lock	= PTHREAD_MUTEX_INITIALIZER;

#define MAX_LOOKUP_ATTEMPTS 10


static kern_return_t
_lookup_all_tx
(
	mach_port_t		server,
	int			proc,
	ooline_data		indata,
	mach_msg_type_number_t	indataCnt,
	mach_port_t		*replyPort
)
{
	typedef struct {
		mach_msg_header_t	Head;
		NDR_record_t		NDR;
		int			proc;
		mach_msg_type_number_t	indataCnt;
		unit			indata[4096];
	} Request;

	Request			In;
	register Request	*InP = &In;
	mach_msg_return_t	mr;
	unsigned int		msgh_size;

	if (indataCnt > 4096) {
		return MIG_ARRAY_TOO_LARGE;
	}

	if (*replyPort == MACH_PORT_NULL) {
		mr = mach_port_allocate(mach_task_self(),
					MACH_PORT_RIGHT_RECEIVE,
					replyPort);
		if (mr != KERN_SUCCESS) {
			return mr;
		}
	}

	msgh_size = (sizeof(Request) - 16384) + ((4 * indataCnt));
	InP->Head.msgh_bits		= MACH_MSGH_BITS(19, MACH_MSG_TYPE_MAKE_SEND_ONCE);
/*	InP->Head.msgh_size		= msgh_size;	/* msgh_size passed as argument */
	InP->Head.msgh_request_port	= server;
	InP->Head.msgh_reply_port	= *replyPort;
	InP->Head.msgh_id		= 4241776;
	InP->NDR			= NDR_record;
	InP->proc			= proc;
	InP->indataCnt			= indataCnt;
	(void)memcpy((char *)InP->indata, (const char *)indata, 4 * indataCnt);

	mr = mach_msg(&InP->Head,		/* msg */
		      MACH_SEND_MSG,		/* options */
		      msgh_size,		/* send_size */
		      0,			/* rcv_size */
		      MACH_PORT_NULL,		/* rcv_name */
		      MACH_MSG_TIMEOUT_NONE,	/* timeout */
		      MACH_PORT_NULL);		/* notify */
	switch (mr) {
		case MACH_MSG_SUCCESS :
			mr = KERN_SUCCESS;
			break;
		case MACH_SEND_INVALID_REPLY :
			(void)mach_port_destroy(mach_task_self(), *replyPort);
			break;
		default:
			break;
	}

	return mr;
}


static kern_return_t
_lookup_all_rx
(
	void			*msg,
	ooline_data		*outdata,
	mach_msg_type_number_t	*outdataCnt,
	security_token_t	*token
)
{
	typedef struct {
		mach_msg_header_t		Head;
		mach_msg_body_t			msgh_body;
		mach_msg_ool_descriptor_t	outdata;
		NDR_record_t			NDR;
		mach_msg_type_number_t		outdataCnt;
		mach_msg_format_0_trailer_t	trailer;
	} Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t	Head;
	 * 	NDR_record_t		NDR;
	 * 	kern_return_t		RetCode;
	 * } mig_reply_error_t;
	 */

	register Reply			*OutP		= msg;
	mach_msg_format_0_trailer_t	*TrailerP;
	boolean_t			msgh_simple;

	if (OutP->Head.msgh_id != (4241776 + 100)) {
	    if (OutP->Head.msgh_id == MACH_NOTIFY_SEND_ONCE)
		return MIG_SERVER_DIED;
	    else
		return MIG_REPLY_MISMATCH;
	}

	msgh_simple = !(OutP->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX);

	TrailerP = (mach_msg_format_0_trailer_t *)((vm_offset_t)OutP +
		round_msg(OutP->Head.msgh_size));
	if (TrailerP->msgh_trailer_type != MACH_MSG_TRAILER_FORMAT_0)
		return MIG_TRAILER_ERROR;

	if (msgh_simple && ((mig_reply_error_t *)OutP)->RetCode != KERN_SUCCESS)
		return ((mig_reply_error_t *)OutP)->RetCode;

	*outdata    = (ooline_data)(OutP->outdata.address);
	*outdataCnt = OutP->outdataCnt;

	*token = TrailerP->msgh_sender;

	return KERN_SUCCESS;
}


static a_requests_t *
request_extract(mach_port_t port)
{
	a_requests_t	*request0, *request;

	pthread_mutex_lock(&a_requests_lock);
	request0 = NULL;
	request  = a_requests;
	while (request) {
	       if (port == request->replyPort) {
			/* request found, remove from list */
			if (request0) {
				request0->next = request->next;
			} else {
				a_requests = request->next;
			}
			break;
		} else {
			/* not this request, skip to next */
			request0 = request;
			request  = request->next;
		}
	}
	pthread_mutex_unlock(&a_requests_lock);

	return request;
}


static void
request_queue(a_requests_t *request)
{
	pthread_mutex_lock(&a_requests_lock);
	request->next = a_requests;
	a_requests = request;
	pthread_mutex_unlock(&a_requests_lock);

	return;
}


static boolean_t
sendCannedReply(a_requests_t *request, int *error)
{
	/*
	 * typedef struct {
	 * 	mach_msg_header_t	Head;
	 * 	NDR_record_t		NDR;
	 * 	kern_return_t		RetCode;
	 * } mig_reply_error_t;
	 */

	mig_reply_error_t		Out;
	register mig_reply_error_t	*OutP = &Out;

	kern_return_t			kr;
	mach_msg_return_t		mr;
	unsigned int			msgh_size;

	/*
	 * allocate reply port
	 */
	kr = mach_port_allocate(mach_task_self(),
				MACH_PORT_RIGHT_RECEIVE,
				&request->replyPort);
	if (kr != KERN_SUCCESS) {
		*error = NO_RECOVERY;
		return FALSE;
	}

	kr = mach_port_insert_right(mach_task_self(),
				    request->replyPort,
				    request->replyPort,
				    MACH_MSG_TYPE_MAKE_SEND);
	if (kr != KERN_SUCCESS) {
		(void) mach_port_destroy(mach_task_self(), request->replyPort);
		*error = NO_RECOVERY;
		return FALSE;
	}

	/*
	 * queue reply message
	 */
	msgh_size = sizeof(Out);
	OutP->Head.msgh_bits		= MACH_MSGH_BITS(19, 0);
/*	OutP->Head.msgh_size		= msgh_size;    /* msgh_size passed as argument */
	OutP->Head.msgh_request_port	= request->replyPort;
	OutP->Head.msgh_reply_port	= MACH_PORT_NULL;
	OutP->Head.msgh_id		= 4241776 + 100;
	OutP->RetCode			= MIG_REMOTE_ERROR;
	OutP->NDR			= NDR_record;

	mr = mach_msg(&OutP->Head,		/* msg */
		      MACH_SEND_MSG,		/* options */
		      msgh_size,		/* send_size */
		      0,			/* rcv_size */
		      MACH_PORT_NULL,		/* rcv_name */
		      MACH_MSG_TIMEOUT_NONE,	/* timeout */
		      MACH_PORT_NULL);		/* notify */
	if (mr != MACH_MSG_SUCCESS) {
		if (mr == MACH_SEND_INVALID_REPLY) {
			(void)mach_port_destroy(mach_task_self(), request->replyPort);
		}
		*error = NO_RECOVERY;
		return FALSE;
	}

	return TRUE;
}


mach_port_t
_gethostbyaddr_async_start(const char		*addr,
			   int			len,
			   int			type,
			   a_request_callout_t	callout,
			   void			*context,
			   int			*error)
{
	void		*address;
	int		proc;
	a_requests_t	*request;
	int		want;

	switch (type) {
	    case AF_INET :
		{
			static int	proc4	= -1;
			struct in_addr	*v4addr;

			if (proc4 < 0) {
				if (_lookup_link(_lu_port, "gethostbyaddr", &proc4) != KERN_SUCCESS) {
					*error = NO_RECOVERY;
					return MACH_PORT_NULL;
				}
			}

			if (len != sizeof(struct in_addr)) {
				*error = NO_RECOVERY;
				return NULL;
			}

			v4addr = malloc(len);
			memmove(v4addr, addr, len);
			v4addr->s_addr = htonl(v4addr->s_addr);

			address = (void *)v4addr;
			proc    = proc4;
			want    = WANT_A4_ONLY;
			break;
		}

	    case AF_INET6 :
		{
			static int	proc6	= -1;
			struct in6_addr	*v6addr;

			if (proc6 < 0) {
				if (_lookup_link(_lu_port, "getipv6nodebyaddr", &proc6) != KERN_SUCCESS) {
					*error = NO_RECOVERY;
					return MACH_PORT_NULL;
				}
			}

			if (len != sizeof(struct in6_addr)) {
				*error = NO_RECOVERY;
				return NULL;
			}

			v6addr = malloc(len);
			memmove(v6addr, addr, len);
			v6addr->__u6_addr.__u6_addr32[0] = htonl(v6addr->__u6_addr.__u6_addr32[0]);
			v6addr->__u6_addr.__u6_addr32[1] = htonl(v6addr->__u6_addr.__u6_addr32[1]);
			v6addr->__u6_addr.__u6_addr32[2] = htonl(v6addr->__u6_addr.__u6_addr32[2]);
			v6addr->__u6_addr.__u6_addr32[3] = htonl(v6addr->__u6_addr.__u6_addr32[3]);

			address = (void *)v6addr;
			proc    = proc6;
			want    = WANT_A6_ONLY;
			break;
		}

	    default:
		*error = NO_RECOVERY;
		return MACH_PORT_NULL;
	}

	request = malloc(sizeof(a_requests_t));
	request->next            = NULL;
	request->retry           = MAX_LOOKUP_ATTEMPTS;
	request->request.proc    = proc;
	request->request.data    = (ooline_data)address;
	request->request.dataLen = len / BYTES_PER_XDR_UNIT;
	request->request.want    = want;
	request->replyPort       = MACH_PORT_NULL;
	request->callout         = callout;
	request->context         = context;
	request->hent            = NULL;

	/*
	 * allocate reply port, send query to lookupd
	 */
	if (_lookup_all_tx(_lu_port,
			   request->request.proc,
			   request->request.data,
			   request->request.dataLen,
			   &request->replyPort) == KERN_SUCCESS) {
		request_queue(request);
	} else {
		if (request->request.data) free(request->request.data);
		free(request);
		*error = NO_RECOVERY;
		return MACH_PORT_NULL;
	}

	return request->replyPort;
}


boolean_t
_gethostbyaddr_async_handleReply(void		*replyMsg,
				 a_requests_t	**requestP,
				 struct hostent	**he,
				 int		*error)
{
	int			count;
	ooline_data		data;
	unsigned int		datalen;
	XDR			inxdr;
	mach_msg_header_t	*msg	= (mach_msg_header_t *)replyMsg;
	a_requests_t		*request;
	kern_return_t		status;
	security_token_t	token;

	request = request_extract(msg->msgh_local_port);
	if (!request) {
		/* excuse me, what happenned to the request info? */
		return FALSE;
	}

	*requestP = request;
	*he       = NULL;
	*error    = 0;

	/* unpack the reply */
	status = _lookup_all_rx(replyMsg, &data, &datalen, &token);
	switch (status) {
		case KERN_SUCCESS :
			break;

		case MIG_SERVER_DIED :
			if (--request->retry > 0) {
				/* retry the request */
				if (_lookup_all_tx(_lu_port,
						   request->request.proc,
						   request->request.data,
						   request->request.dataLen,
						   &request->replyPort) == KERN_SUCCESS) {
					request_queue(request);
					return FALSE;
				}
			}
			/* fall through */

		default :
			*error = HOST_NOT_FOUND;
			return TRUE;
	}

	datalen *= BYTES_PER_XDR_UNIT;

	if (token.val[0] != 0) {
		vm_deallocate(mach_task_self(), (vm_address_t)data, datalen);
		*error = NO_RECOVERY;
		return TRUE;
	}

	xdrmem_create(&inxdr, data, datalen, XDR_DECODE);

	count = 0;
	if (!xdr_int(&inxdr, &count)) {
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)data, datalen);
		*error = NO_RECOVERY;
		return TRUE;
	}

	if (count == 0) {
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)data, datalen);
		*error = HOST_NOT_FOUND;
		*he    = NULL;
		return TRUE;
	}

	*he = extract_host(&inxdr, request->request.want, error);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)data, datalen);
	return TRUE;
}


mach_port_t
gethostbyaddr_async_start(const char			*addr,
			  int				len,
			  int				type,
			  gethostbyaddr_async_callback	callout,
			  void				*context)
{
	a_request_callout_t	cb;
	mach_port_t		mp;

	if (!_lu_running()) {
		h_errno = NO_RECOVERY;
		return MACH_PORT_NULL;
	}

	cb.hostAddr = callout;
	mp = _gethostbyaddr_async_start(addr, len, type, cb, context, &h_errno);
	return mp;
}


void
gethostbyaddr_async_handleReply(void *replyMsg)
{
	int		error		= 0;
	struct hostent	*he		= NULL;
	a_requests_t	*request	= NULL;

	if (_gethostbyaddr_async_handleReply(replyMsg, &request, &he, &error)) {
		/* if we have an answer to provide */
		h_errno = error;
		(request->callout.hostAddr)(he, request->context);

		(void)mach_port_destroy(mach_task_self(), request->replyPort);
		if (request->request.data) free(request->request.data);
		free(request);
		if (he) freehostent(he);
	}

	return;
}


mach_port_t
getipnodebyaddr_async_start(const void				*addr,
			    size_t				len,
			    int					af,
			    int					*error,
			    getipnodebyaddr_async_callback	callout,
			    void				*context)
{
	a_request_callout_t	cb;
	mach_port_t		mp;

	if (!_lu_running()) {
		*error = NO_RECOVERY;
		return MACH_PORT_NULL;
	}

	if ((af == AF_INET6) &&
	    (len == sizeof(struct in6_addr)) &&
	    (is_a4_mapped((const char *)addr) || is_a4_compat((const char *)addr))) {
		/*
		 * this is really a v4 address
		 */
		addr += sizeof(struct in6_addr) - sizeof(struct in_addr);
		len  =  sizeof(struct in_addr);
		af   = AF_INET;
	}

	cb.nodeAddr = callout;
	mp = _gethostbyaddr_async_start(addr, len, af, cb, context, error);
	return mp;
}


void
getipnodebyaddr_async_handleReply(void *replyMsg)
{
	int		error		= 0;
	struct hostent	*he		= NULL;
	a_requests_t	*request	= NULL;

	if (_gethostbyaddr_async_handleReply(replyMsg, &request, &he, &error)) {
		/* if we have an answer to provide */
		(request->callout.nodeAddr)(he, error, request->context);

		(void)mach_port_destroy(mach_task_self(), request->replyPort);
		if (request->request.data) free(request->request.data);
		free(request);
		/*
		 * Note: it is up to the callback function to call
		 *       freehostent().
		 */
	}

	return;
}


mach_port_t
_gethostbyname_async_start(const char			*name,
			   int				want,
			   int				*error,
			   a_request_callout_t		callout,
			   void				*context)
{
	int		af;
	boolean_t	is_addr		= FALSE;
	mach_port_t	mp		= MACH_PORT_NULL;
	XDR		outxdr;
	static int	proc;
	a_requests_t	*request;

	if ((name == NULL) || (name[0] == '\0')) {
		*error = NO_DATA;
		return MACH_PORT_NULL;
	}

	af = (want == WANT_A4_ONLY) ? AF_INET : AF_INET6;

	if ((af == AF_INET) || (want == WANT_MAPPED_A4_ONLY)) {
		static int	proc4	= -1;

		if (proc4 < 0) {
			if (_lookup_link(_lu_port, "gethostbyname", &proc4) != KERN_SUCCESS) {
				*error = NO_RECOVERY;
				return MACH_PORT_NULL;
			}
		}
		proc = proc4;
	} else /* if (af == AF_INET6) */ {
		static int	proc6	= -1;

		if (proc6 < 0) {
			if (_lookup_link(_lu_port, "getipv6nodebyname", &proc6) != KERN_SUCCESS)
			{
				*error = NO_RECOVERY;
				return MACH_PORT_NULL;
			}
		}
		proc = proc6;
	}

	request = malloc(sizeof(a_requests_t));
	request->next            = NULL;
	request->retry           = MAX_LOOKUP_ATTEMPTS;
	request->request.proc    = proc;
	request->request.data    = NULL;
	request->request.dataLen = 0;
	request->request.want    = want;
	request->replyPort       = MACH_PORT_NULL;
	request->callout         = callout;
	request->context         = context;
	request->hent            = NULL;

	switch (af) {
	    case AF_INET :
		{
			struct in_addr	v4addr;

			memset(&v4addr, 0, sizeof(struct in_addr));
			if (inet_aton(name, &v4addr) == 1) {
				/* return a fake hostent */
				request->hent = fake_hostent(name, v4addr);
				is_addr = TRUE;
			}
			break;
		}

	    case AF_INET6 :
		{
			struct in_addr	v4addr;
			struct in6_addr	v6addr;

			memset(&v6addr, 0, sizeof(struct in6_addr));
			if (inet_pton(af, name, &v6addr) == 1) {
				/* return a fake hostent */
				request->hent = fake_hostent6(name, v6addr);
				is_addr = TRUE;
				break;
			} 

			memset(&v4addr, 0, sizeof(struct in_addr));
			if (inet_aton(name, &v4addr) == 1) {
				if (want == WANT_A4_ONLY) {
					free(request);
					*error = HOST_NOT_FOUND;
					return MACH_PORT_NULL;
				}

				v6addr.__u6_addr.__u6_addr32[0] = 0x00000000;
				v6addr.__u6_addr.__u6_addr32[1] = 0x00000000;
				v6addr.__u6_addr.__u6_addr32[2] = htonl(0x0000ffff);
				memmove(&(v6addr.__u6_addr.__u6_addr32[3]), &(v4addr.s_addr), sizeof(struct in_addr));

				/* return a fake hostent */
				request->hent = fake_hostent6(name, v6addr);
				is_addr = TRUE;
			}
			break;
		}

	    default:
		free(request);
		*error = NO_RECOVERY;
		return MACH_PORT_NULL;
	}

	if (is_addr) {
		/*
		 * queue reply message
		 */
		if (sendCannedReply(request, error)) {
			request_queue(request);
			return request->replyPort;
		} else {
			freehostent(request->hent);
			free(request);
			return MACH_PORT_NULL;
		}
	}

	request->request.dataLen = _LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT;
	request->request.data    = malloc(request->request.dataLen);

	xdrmem_create(&outxdr, request->request.data, request->request.dataLen, XDR_ENCODE);
	if (!xdr__lu_string(&outxdr, (_lu_string *)&name)) {
		xdr_destroy(&outxdr);
		free(request->request.data);
		free(request);
		*error = NO_RECOVERY;
		return MACH_PORT_NULL;
	}

	request->request.dataLen = xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT;

	/*
	 * allocate reply port, send query to lookupd
	 */
	if (_lookup_all_tx(_lu_port,
			   request->request.proc,
			   request->request.data,
			   request->request.dataLen,
			   &request->replyPort) == KERN_SUCCESS) {
		request_queue(request);
		mp = request->replyPort;
	} else {
		free(request->request.data);
		free(request);
		*error = NO_RECOVERY;
		mp = NULL;
	}

	xdr_destroy(&outxdr);
	return mp;
}


boolean_t
_gethostbyname_async_handleReply(void		*replyMsg,
				 a_requests_t   **requestP,
				 struct hostent **he,
				 int            *error)
{

	int			count;
	unsigned int		datalen;
	XDR			inxdr;
	ooline_data		data;
	mach_msg_header_t	*msg	= (mach_msg_header_t *)replyMsg;
	a_requests_t		*request;
	kern_return_t		status;
	security_token_t	token;
	int			want;

	request = request_extract(msg->msgh_local_port);
	if (!request) {
		/* excuse me, what happenned to the request info? */
		return FALSE;
	}

	*requestP = request;
	*he       = NULL;
	*error    = 0;

	if (request->hent) {
		/*
		 * if the reply was already available when the
		 * request was made
		 */
		*he = request->hent;
		return TRUE;
	}

	/* unpack the reply */
	status = _lookup_all_rx(replyMsg, &data, &datalen, &token);
	switch (status) {
		case KERN_SUCCESS :
			break;

		case MIG_SERVER_DIED :
			if (--request->retry > 0) {
				/*
				 * retry the request
				 */
				if (_lookup_all_tx(_lu_port,
						   request->request.proc,
						   request->request.data,
						   request->request.dataLen,
						   &request->replyPort) == KERN_SUCCESS) {
					request_queue(request);
					return FALSE;
				}
			}
			/* fall through */

		default :
			*error = HOST_NOT_FOUND;
			return TRUE;
	}

	datalen *= BYTES_PER_XDR_UNIT;

	if (token.val[0] != 0) {
		vm_deallocate(mach_task_self(), (vm_address_t)data, datalen);
		*error = NO_RECOVERY;
		return TRUE;
	}

	xdrmem_create(&inxdr, data, datalen, XDR_DECODE);

	count = 0;
	if (!xdr_int(&inxdr, &count)) {
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)data, datalen);
		*error = NO_RECOVERY;
		return TRUE;
	}

	if (count == 0) {
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)data, datalen);
		*error = HOST_NOT_FOUND;
		return TRUE;
	}

	want = request->request.want;
	if (want == WANT_A6_OR_MAPPED_A4_IF_NO_A6) want = WANT_A6_ONLY;

	*he = extract_host(&inxdr, want, error);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)data, datalen);
	return TRUE;
}


mach_port_t
gethostbyname_async_start(const char			*name,
			  gethostbyname_async_callback	callout,
			  void				*context)
{
	a_request_callout_t	cb;
	int			error;
	mach_port_t		mp	= MACH_PORT_NULL;

	if (!_lu_running()) {
		h_errno = NO_RECOVERY;
		return MACH_PORT_NULL;
	}

	cb.hostName = callout;
	mp = _gethostbyname_async_start(name, WANT_A4_ONLY, &error, cb, context);
	if (!mp) {
		h_errno = error;
	}
	return mp;
}


void
gethostbyname_async_handleReply(void *replyMsg)
{
	int             error;
	struct hostent  *he;
	a_requests_t    *request;

	if (_gethostbyname_async_handleReply(replyMsg, &request, &he, &error)) {
		/* if we have an answer to provide */
		h_errno = error;
		(request->callout.hostAddr)(he, request->context);

		(void)mach_port_destroy(mach_task_self(), request->replyPort);
		if (request->request.data) free(request->request.data);
		free(request);
		if (he) freehostent(he);
	}

	return;
}


mach_port_t
getipnodebyname_async_start(const char				*name,
			    int					af,
			    int					flags,
			    int					*error,
			    getipnodebyname_async_callback	callout,
			    void				*context)
{
	a_request_callout_t     cb;
	int			if4		= 0;
	int			if6		= 0;
	mach_port_t		mp		= MACH_PORT_NULL;
	int			want		= WANT_A4_ONLY;

	if (!_lu_running()) {
		h_errno = NO_RECOVERY;
		return MACH_PORT_NULL;
	}

	/*
	 * IF AI_ADDRCONFIG is set, we need to know what interface flavors we really have.
	 */
	if (flags & AI_ADDRCONFIG) {
		struct ifaddrs	*ifa, *ifap;

		if (getifaddrs(&ifa) < 0) {
			*error = NO_RECOVERY;
			return MACH_PORT_NULL;
		}

		for (ifap = ifa; ifap != NULL; ifap = ifap->ifa_next) {
			if (ifap->ifa_addr == NULL)
				continue;
			if ((ifap->ifa_flags & IFF_UP) == 0)
				continue;
			if (ifap->ifa_addr->sa_family == AF_INET) {
				if4++;
			} else if (ifap->ifa_addr->sa_family == AF_INET6) {
				if6++;
			}
		}

		freeifaddrs(ifa);

		/* Bail out if there are no interfaces */
		if ((if4 == 0) && (if6 == 0)) {
			*error = NO_ADDRESS;
			return MACH_PORT_NULL;
		}
	}

	/*
	 * Figure out what we want.
	 * If user asked for AF_INET, we only want V4 addresses.
	 */
	switch (af) {
	    case AF_INET :
		{
			want = WANT_A4_ONLY;
			if ((flags & AI_ADDRCONFIG) && (if4 == 0)) {
				*error = NO_ADDRESS;
				return MACH_PORT_NULL;
			}
		}
		break;
	    case AF_INET6 :
		{
			want = WANT_A6_ONLY;
			if (flags & (AI_V4MAPPED|AI_V4MAPPED_CFG)) {
				if (flags & AI_ALL) {
					want = WANT_A6_PLUS_MAPPED_A4;
				} else {
					want = WANT_A6_OR_MAPPED_A4_IF_NO_A6;
				}
			} else {
				if ((flags & AI_ADDRCONFIG) && (if6 == 0)) {
					*error = NO_ADDRESS;
					return MACH_PORT_NULL;
				}
			}
		}
		break;
	}

	cb.nodeName = callout;
	mp = _gethostbyname_async_start(name, want, &h_errno, cb, context);
	return mp;
}


void
getipnodebyname_async_handleReply(void *replyMsg)
{
	int		error		= 0;
	struct hostent	*he		= NULL;
	a_requests_t	*request	= NULL;

	if (_gethostbyname_async_handleReply(replyMsg, &request, &he, &error)) {
		/*
		 * we have an answer to provide
		 */
		if ((he == NULL) &&
		    (error == HOST_NOT_FOUND) &&
		    ((request->request.want == WANT_A6_PLUS_MAPPED_A4) ||
		     (request->request.want == WANT_A6_OR_MAPPED_A4_IF_NO_A6))) {
			/*
			 * no host found (yet), if requested we send a
			 * followup query to lookupd.
			 */
			static int	proc4		= -1;

			if (proc4 < 0) {
				if (_lookup_link(_lu_port, "gethostbyname", &proc4) != KERN_SUCCESS) {
					error = NO_RECOVERY;
					goto answer;
				}
			}

			request->request.proc = proc4;
			request->request.want = WANT_MAPPED_A4_ONLY;
			if (_lookup_all_tx(_lu_port,
					   request->request.proc,
					   request->request.data,
					   request->request.dataLen,
					   &request->replyPort) == KERN_SUCCESS) {
				request_queue(request);
				return;
			} else {
				error = NO_RECOVERY;
			}
		}

	    answer :
		(request->callout.nodeName)(he, error, request->context);
		(void)mach_port_destroy(mach_task_self(), request->replyPort);
		if (request->request.data) free(request->request.data);
		free(request);
		/*
		 * Note: it is up to the callback function to call
		 *       freehostent().
		 */
	}

	return;
}
