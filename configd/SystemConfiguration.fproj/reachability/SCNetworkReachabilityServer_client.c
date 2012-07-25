/*
 * Copyright (c) 2011, 2012 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCNetworkReachabilityInternal.h"

#ifdef	HAVE_REACHABILITY_SERVER

#include <xpc/xpc.h>
#include <xpc/private.h>

#include "rb.h"


#pragma mark -
#pragma mark Globals


static const struct addrinfo	hints0	= {
#ifdef  AI_PARALLEL
	.ai_flags	= AI_PARALLEL | AI_ADDRCONFIG
#else  // AI_PARALLEL
	.ai_flags	= AI_ADDRCONFIG
#endif  // AI_PARALLEL
};


static Boolean			serverAvailable	= TRUE;


#pragma mark -
#pragma mark Support functions


static void
log_xpc_object(const char *msg, xpc_object_t obj)
{
	char		*desc;

	desc = xpc_copy_description(obj);
	SCLog(TRUE, LOG_DEBUG, CFSTR("%s = %s"), msg, desc);
	free(desc);
}


#pragma mark -
#pragma mark Reachability [RBT] client support


typedef struct {
	struct rb_node			rbn;
	SCNetworkReachabilityRef	target;
} reach_request_t;


#define RBNODE_TO_REACH_REQUEST(node) \
	((reach_request_t *)((uintptr_t)node - offsetof(reach_request_t, rbn)))


static int
_rbt_compare_transaction_nodes(const struct rb_node *n1, const struct rb_node *n2)
{
	uint64_t	a = (uintptr_t)(RBNODE_TO_REACH_REQUEST(n1)->target);
	uint64_t	b = (uintptr_t)(RBNODE_TO_REACH_REQUEST(n2)->target);

	return (a - b);
}


static int
_rbt_compare_transaction_key(const struct rb_node *n1, const void *key)
{
	uint64_t	a = (uintptr_t)(RBNODE_TO_REACH_REQUEST(n1)->target);
	uint64_t	b = *(uint64_t *)key;

	return (a - b);
}


static struct rb_tree *
_reach_requests_rbt()
{
	static dispatch_once_t		once;
	static const struct rb_tree_ops	ops = {
		.rbto_compare_nodes	= _rbt_compare_transaction_nodes,
		.rbto_compare_key	= _rbt_compare_transaction_key,
	};
	static struct rb_tree		rbtree;

	dispatch_once(&once, ^{
		rb_tree_init(&rbtree, &ops);
	});

	return &rbtree;
}


static dispatch_queue_t
_reach_requests_rbt_queue()
{
	static dispatch_once_t	once;
	static dispatch_queue_t	q;

	dispatch_once(&once, ^{
		q = dispatch_queue_create(REACH_SERVICE_NAME ".rbt", NULL);
	});

	return q;
}


static reach_request_t *
_reach_request_create(SCNetworkReachabilityRef target)
{
	reach_request_t	*request;

	request = calloc(1, sizeof(*request));
	request->target = CFRetain(target);

	return request;
}


static void
_reach_request_release(reach_request_t *request)
{
	SCNetworkReachabilityRef	target	= request->target;

	CFRelease(target);
	free(request);

	return;
}


static void
_reach_request_add(SCNetworkReachabilityRef target)
{
	uint64_t	target_id	= (uintptr_t)target;

	dispatch_sync(_reach_requests_rbt_queue(), ^{
		struct rb_node		*rbn;

		rbn = rb_tree_find_node(_reach_requests_rbt(), &target_id);
		if (rbn == NULL) {
			reach_request_t	*request;

			request = _reach_request_create(target);
			if (request == NULL || !rb_tree_insert_node(_reach_requests_rbt(), &request->rbn)) {
				__builtin_trap();
			}
		}
	});

	return;
}


static void
_reach_request_remove(SCNetworkReachabilityRef target)
{
	uint64_t	target_id	= (uintptr_t)target;

	dispatch_sync(_reach_requests_rbt_queue(), ^{		// FIXME ?? use dispatch_async?
		struct rb_node		*rbn;
		struct rb_tree		*rbtree = _reach_requests_rbt();

		rbn = rb_tree_find_node(rbtree, &target_id);
		if (rbn != NULL) {
			reach_request_t	*request	= RBNODE_TO_REACH_REQUEST(rbn);

			rb_tree_remove_node(rbtree, rbn);
			_reach_request_release(request);
		}
	});
}


static SCNetworkReachabilityRef
_reach_request_copy_target(uint64_t target_id)
{
	__block SCNetworkReachabilityRef	target	= NULL;

	dispatch_sync(_reach_requests_rbt_queue(), ^{
		struct rb_node	*rbn;

		rbn = rb_tree_find_node(_reach_requests_rbt(), &target_id);
		if (rbn != NULL) {
			// handle the [async] reply
			target = (SCNetworkReachabilityRef)(uintptr_t)target_id;
			CFRetain(target);
		}
	});

	return target;
}


#pragma mark -
#pragma mark Reachability [XPC] client support


static void
handle_reachability_status(SCNetworkReachabilityRef target, xpc_object_t dict)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	if (_sc_debug) {
		SCLog(TRUE, LOG_INFO, CFSTR("%sgot [async] notification"),
		      targetPrivate->log_prefix);
//		log_xpc_object("  status", dict);
	}

	__SCNetworkReachabilityPerformNoLock(target);

	return;
}


static void
handle_async_notification(SCNetworkReachabilityRef target, xpc_object_t dict)
{
	int64_t				op;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	op = xpc_dictionary_get_int64(dict, MESSAGE_NOTIFY);
	switch (op) {
		case MESSAGE_REACHABILITY_STATUS :
			handle_reachability_status(target, dict);
			break;
		default :
			SCLog(TRUE, LOG_ERR, CFSTR("%sgot [async] unknown reply : %d"),
			      targetPrivate->log_prefix,
			      op);
			log_xpc_object("  reply", dict);
			break;
	}

	return;
}


static dispatch_queue_t
_reach_xpc_queue()
{
	static dispatch_once_t	once;
	static dispatch_queue_t	q;

	dispatch_once(&once, ^{
		q = dispatch_queue_create(REACH_SERVICE_NAME ".xpc", NULL);
	});

	return q;
}


static void
_reach_connection_reconnect(xpc_connection_t connection);


static xpc_connection_t
_reach_connection_create()
{
	xpc_connection_t		c;
	const char			*name;
	dispatch_queue_t		q	= _reach_xpc_queue();

	// create XPC connection
	name = getenv("REACH_SERVER");
	if ((name == NULL) || (issetugid() != 0)) {
		name = REACH_SERVICE_NAME;
	}

	c = xpc_connection_create_mach_service(name,
					       q,
					       XPC_CONNECTION_MACH_SERVICE_PRIVILEGED);

	xpc_connection_set_event_handler(c, ^(xpc_object_t xobj) {
		xpc_type_t	type;

		type = xpc_get_type(xobj);
		if (type == XPC_TYPE_DICTIONARY) {
			SCNetworkReachabilityRef	target;
			uint64_t			target_id;

			target_id = xpc_dictionary_get_uint64(xobj, REACH_CLIENT_TARGET_ID);
			if (target_id == 0) {
				SCLog(TRUE, LOG_ERR,
				      CFSTR("reach client %p: async reply with no target [ID]"),
				      c);
				log_xpc_object("  reply", xobj);
				return;
			}

			target = _reach_request_copy_target(target_id);
			if (target == NULL) {
				SCLog(TRUE, LOG_ERR,
				      CFSTR("received unexpected target [ID] from SCNetworkReachability server"));
				log_xpc_object("  reply", xobj);
				return;
			}

			handle_async_notification(target, xobj);
			CFRelease(target);

		} else if (type == XPC_TYPE_ERROR) {
			if (xobj == XPC_ERROR_CONNECTION_INVALID) {
				SCLog(TRUE, LOG_ERR,
				      CFSTR("SCNetworkReachability server not available"));
				serverAvailable = FALSE;
			} else if (xobj == XPC_ERROR_CONNECTION_INTERRUPTED) {
				SCLog(TRUE, LOG_DEBUG,
				      CFSTR("SCNetworkReachability server failure, reconnecting"));
				_reach_connection_reconnect(c);
			} else {
				const char	*desc;

				desc = xpc_dictionary_get_string(xobj, XPC_ERROR_KEY_DESCRIPTION);
				SCLog(TRUE, LOG_ERR,
				      CFSTR("reach client %p: Connection error: %s"),
				      c,
				      desc);
			}

		} else {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("reach client %p: unknown event type : %x"),
			      c,
			      type);
		}
	});
	xpc_connection_resume(c);

	return c;
}


static xpc_connection_t
_reach_connection()
{
	static xpc_connection_t		c;
	static dispatch_once_t		once;
	static dispatch_queue_t		q;

	if (!serverAvailable) {
		// if SCNetworkReachabilty [XPC] server not available
		return NULL;
	}

	dispatch_once(&once, ^{
		q = dispatch_queue_create(REACH_SERVICE_NAME ".connection", NULL);
	});

	dispatch_sync(q, ^{
		if (c == NULL) {
			c = _reach_connection_create();
		}
	});

	return c;
}


typedef void (^reach_server_reply_handler_t)(xpc_object_t reply);


static void
add_proc_name(xpc_object_t reqdict)
{
	static const char	*name	= NULL;
	static dispatch_once_t	once;

	// add the process name
	dispatch_once(&once, ^{
		name = getprogname();
	});
	xpc_dictionary_set_string(reqdict, REACH_CLIENT_PROC_NAME, name);

	return;
}


static void
_reach_server_target_reconnect(xpc_connection_t connection, SCNetworkReachabilityRef target);


static Boolean
_reach_server_target_add(xpc_connection_t connection, SCNetworkReachabilityRef target)
{
	Boolean				ok		= FALSE;
	xpc_object_t			reply;
	xpc_object_t			reqdict;
	Boolean				retry		= FALSE;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	// create message
	reqdict = xpc_dictionary_create(NULL, NULL, 0);

	// set request
	xpc_dictionary_set_int64(reqdict, REACH_REQUEST, REACH_REQUEST_CREATE);

	// add reachability target info
	if (targetPrivate->name != NULL) {
		xpc_dictionary_set_string(reqdict,
					  REACH_TARGET_NAME,
					  targetPrivate->name);
	}
	if (targetPrivate->serv != NULL) {
		xpc_dictionary_set_string(reqdict,
					  REACH_TARGET_SERV,
					  targetPrivate->serv);
	}
	if (targetPrivate->localAddress != NULL) {
		xpc_dictionary_set_data(reqdict,
					REACH_TARGET_LOCAL_ADDR,
					targetPrivate->localAddress,
					targetPrivate->localAddress->sa_len);
	}
	if (targetPrivate->remoteAddress != NULL) {
		xpc_dictionary_set_data(reqdict,
					REACH_TARGET_REMOTE_ADDR,
					targetPrivate->remoteAddress,
					targetPrivate->remoteAddress->sa_len);
	}
	if (bcmp(&targetPrivate->hints, &hints0, sizeof(struct addrinfo)) != 0) {
		xpc_dictionary_set_data(reqdict,
					REACH_TARGET_HINTS,
					&targetPrivate->hints,
					sizeof(targetPrivate->hints));
	}
	if (targetPrivate->if_index != 0) {
		xpc_dictionary_set_int64(reqdict,
					 REACH_TARGET_IF_INDEX,
					 targetPrivate->if_index);
		xpc_dictionary_set_string(reqdict,
					  REACH_TARGET_IF_NAME,
					  targetPrivate->if_name);
	}
	if (targetPrivate->onDemandBypass) {
		xpc_dictionary_set_bool(reqdict,
					REACH_TARGET_ONDEMAND_BYPASS,
					TRUE);
	}
	if (targetPrivate->resolverBypass) {
		xpc_dictionary_set_bool(reqdict,
					REACH_TARGET_RESOLVER_BYPASS,
					TRUE);
	}


	// add the target [ID]
	xpc_dictionary_set_uint64(reqdict, REACH_CLIENT_TARGET_ID, (uintptr_t)target);

	// add the process name (for debugging)
	add_proc_name(reqdict);

    retry :

	// send request to the SCNetworkReachability server
	reply = xpc_connection_send_message_with_reply_sync(connection, reqdict);
	if (reply != NULL) {
		xpc_type_t	type;

		type = xpc_get_type(reply);
		if (type == XPC_TYPE_DICTIONARY) {
			int64_t		status;

			status = xpc_dictionary_get_int64(reply, REACH_REQUEST_REPLY);
			ok = (status == REACH_REQUEST_REPLY_OK);
		} else if ((type == XPC_TYPE_ERROR) && (reply == XPC_ERROR_CONNECTION_INVALID)) {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("SCNetworkReachability server not available"));
			serverAvailable = FALSE;
		} else if ((type == XPC_TYPE_ERROR) && (reply == XPC_ERROR_CONNECTION_INTERRUPTED)) {
			SCLog(TRUE, LOG_DEBUG,
			      CFSTR("reach target %p: SCNetworkReachability server failure, retrying"),
			      target);
			retry = TRUE;
		} else {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("reach target %p: _targetAdd with unexpected reply"),
			      target);
			log_xpc_object("  reply", reply);
		}

		xpc_release(reply);
	}

	if (retry) {
		retry = FALSE;
		goto retry;
	}

	xpc_release(reqdict);
	return ok;
}


static Boolean
_reach_server_target_remove(xpc_connection_t connection, SCNetworkReachabilityRef target)
{
	Boolean				ok		= FALSE;
	xpc_object_t			reply;
	xpc_object_t			reqdict;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	// create message
	reqdict = xpc_dictionary_create(NULL, NULL, 0);

	// set request
	xpc_dictionary_set_int64(reqdict, REACH_REQUEST, REACH_REQUEST_REMOVE);

	// add the target [ID]
	xpc_dictionary_set_uint64(reqdict, REACH_CLIENT_TARGET_ID, (uintptr_t)target);

	reply = xpc_connection_send_message_with_reply_sync(connection, reqdict);
	if (reply != NULL) {
		xpc_type_t	type;

		type = xpc_get_type(reply);
		if (type == XPC_TYPE_DICTIONARY) {
			int64_t		status;

			status = xpc_dictionary_get_int64(reply, REACH_REQUEST_REPLY);
			switch (status) {
				case REACH_REQUEST_REPLY_OK :
					ok = TRUE;
					break;
				case REACH_REQUEST_REPLY_UNKNOWN :
					SCLog(TRUE, LOG_DEBUG,
					      CFSTR("reach target %p: SCNetworkReachability server failure, no need to remove"),
					      target);
					ok = TRUE;
					break;
				default : {
					SCLog(TRUE, LOG_ERR, CFSTR("%s  target remove failed"),
					      targetPrivate->log_prefix);
					log_xpc_object("  reply", reply);
				}
			}
		} else if ((type == XPC_TYPE_ERROR) && (reply == XPC_ERROR_CONNECTION_INVALID)) {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("SCNetworkReachability server not available"));
			serverAvailable = FALSE;
			ok = TRUE;
		} else if ((type == XPC_TYPE_ERROR) && (reply == XPC_ERROR_CONNECTION_INTERRUPTED)) {
			SCLog(TRUE, LOG_DEBUG,
			      CFSTR("reach target %p: SCNetworkReachability server failure, no need to remove"),
			      target);
			ok = TRUE;
		} else {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("reach target %p: _targetRemove with unexpected reply"),
			      target);
			log_xpc_object("  reply", reply);
		}

		xpc_release(reply);
	}

	xpc_release(reqdict);
	return ok;
}


static Boolean
_reach_server_target_schedule(xpc_connection_t connection, SCNetworkReachabilityRef target)
{
	Boolean				ok		= FALSE;
	xpc_object_t			reply;
	xpc_object_t			reqdict;
	Boolean				retry		= FALSE;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	// create message
	reqdict = xpc_dictionary_create(NULL, NULL, 0);

	// set request
	xpc_dictionary_set_int64(reqdict, REACH_REQUEST, REACH_REQUEST_SCHEDULE);

	// add the target [ID]
	xpc_dictionary_set_uint64(reqdict, REACH_CLIENT_TARGET_ID, (uintptr_t)target);

    retry :

	reply = xpc_connection_send_message_with_reply_sync(connection, reqdict);
	if (reply != NULL) {
		xpc_type_t	type;

		type = xpc_get_type(reply);
		if (type == XPC_TYPE_DICTIONARY) {
			int64_t		status;

			status = xpc_dictionary_get_int64(reply, REACH_REQUEST_REPLY);
			switch (status) {
				case REACH_REQUEST_REPLY_OK :
					ok = TRUE;
					break;
				case REACH_REQUEST_REPLY_UNKNOWN :
					SCLog(TRUE, LOG_DEBUG,
					      CFSTR("reach target %p: SCNetworkReachability server failure, retry schedule"),
					      target);
					retry = TRUE;
					break;
				default : {
					SCLog(TRUE, LOG_ERR, CFSTR("%s  target schedule failed"),
					      targetPrivate->log_prefix);
					log_xpc_object("  reply", reply);
				}
			}

			if (ok) {
				CFRetain(target);
			}
		} else if ((type == XPC_TYPE_ERROR) && (reply == XPC_ERROR_CONNECTION_INVALID)) {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("SCNetworkReachability server not available"));
			serverAvailable = FALSE;
		} else if ((type == XPC_TYPE_ERROR) && (reply == XPC_ERROR_CONNECTION_INTERRUPTED)) {
			SCLog(TRUE, LOG_DEBUG,
			      CFSTR("reach target %p: SCNetworkReachability server failure, retry schedule"),
			      target);
			retry = TRUE;
		} else {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("reach target %p: _targetSchedule with unexpected reply"),
			      target);
			log_xpc_object("  reply", reply);
		}

		xpc_release(reply);
	}

	if (retry) {
		// reconnect
		_reach_server_target_reconnect(connection, target);

		// and retry
		retry = FALSE;
		goto retry;
	}

	xpc_release(reqdict);
	return ok;
}


static void
_reach_reply_set_reachability(SCNetworkReachabilityRef	target,
			      xpc_object_t		reply)
{
	char				*if_name;
	size_t				len		= 0;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	targetPrivate->serverInfo.cycle = xpc_dictionary_get_uint64(reply,
								    REACH_STATUS_CYCLE);

	targetPrivate->serverInfo.flags = xpc_dictionary_get_uint64(reply,
								    REACH_STATUS_FLAGS);

	targetPrivate->serverInfo.if_index = xpc_dictionary_get_uint64(reply,
								       REACH_STATUS_IF_INDEX);

	bzero(&targetPrivate->serverInfo.if_name, sizeof(targetPrivate->serverInfo.if_name));
	if_name = (void *)xpc_dictionary_get_data(reply,
						  REACH_STATUS_IF_NAME,
						  &len);
	if ((if_name != NULL) && (len > 0)) {
		if (len > sizeof(targetPrivate->serverInfo.if_name)) {
			len = sizeof(targetPrivate->serverInfo.if_name);
		}

		bcopy(if_name, targetPrivate->serverInfo.if_name, len);
	}

	targetPrivate->serverInfo.sleeping = xpc_dictionary_get_bool(reply,
								     REACH_STATUS_SLEEPING);

	if (targetPrivate->type == reachabilityTypeName) {
		xpc_object_t		addresses;

		if (targetPrivate->resolvedAddress != NULL) {
			CFRelease(targetPrivate->resolvedAddress);
			targetPrivate->resolvedAddress = NULL;
		}

		targetPrivate->resolvedAddressError = xpc_dictionary_get_int64(reply,
									       REACH_STATUS_RESOLVED_ADDRESS_ERROR);

		addresses = xpc_dictionary_get_value(reply, REACH_STATUS_RESOLVED_ADDRESS);
		if ((addresses != NULL) && (xpc_get_type(addresses) != XPC_TYPE_ARRAY)) {
			addresses = NULL;
		}

		if ((targetPrivate->resolvedAddressError == 0) && (addresses != NULL)) {
			int			i;
			int			n;
			CFMutableArrayRef	newAddresses;

			newAddresses = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

			n = xpc_array_get_count(addresses);
			for (i = 0; i < n; i++) {
				struct addrinfo	*sa;
				size_t		len;
				CFDataRef	newAddress;

				sa = (struct addrinfo *)xpc_array_get_data(addresses, i, &len);
				newAddress = CFDataCreate(NULL, (const UInt8 *)sa, len);
				CFArrayAppendValue(newAddresses, newAddress);
				CFRelease(newAddress);
			}

			targetPrivate->resolvedAddress = newAddresses;
		} else {
			/* save the error associated with the attempt to resolve the name */
			targetPrivate->resolvedAddress = CFRetain(kCFNull);
		}
		targetPrivate->needResolve = FALSE;
	}

	return;
}


__private_extern__
Boolean
_reach_server_target_status(xpc_connection_t connection, SCNetworkReachabilityRef target)
{
	Boolean				ok		= FALSE;
	xpc_object_t			reply;
	xpc_object_t			reqdict;
	Boolean				retry		= FALSE;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	if (_sc_debug) {
		CFStringRef	str;

		str = _SCNetworkReachabilityCopyTargetDescription(target);
		SCLog(TRUE, LOG_INFO, CFSTR("%scheckReachability(%@)"),
		      targetPrivate->log_prefix,
		      str);
		CFRelease(str);
	}

	// create message
	reqdict = xpc_dictionary_create(NULL, NULL, 0);

	// set request
	xpc_dictionary_set_int64(reqdict, REACH_REQUEST, REACH_REQUEST_STATUS);

	// add the target [ID]
	xpc_dictionary_set_uint64(reqdict, REACH_CLIENT_TARGET_ID, (uintptr_t)target);

    retry :

	reply = xpc_connection_send_message_with_reply_sync(connection, reqdict);
	if (reply != NULL) {
		xpc_type_t	type;

		type = xpc_get_type(reply);
		if (type == XPC_TYPE_DICTIONARY) {
			int64_t		status;

			status = xpc_dictionary_get_int64(reply, REACH_REQUEST_REPLY);
			switch (status) {
				case REACH_REQUEST_REPLY_OK :
					ok = TRUE;
					break;
				case REACH_REQUEST_REPLY_UNKNOWN :
					SCLog(TRUE, LOG_DEBUG,
					      CFSTR("reach target %p: SCNetworkReachability server failure, retry status"),
					      target);
					retry = TRUE;
					break;
				default :
					SCLog(TRUE, LOG_INFO, CFSTR("%s  target status failed"),
					      targetPrivate->log_prefix);
					log_xpc_object("  reply", reply);
			}

			if (ok) {
				_reach_reply_set_reachability(target, reply);

				if (_sc_debug) {
					SCLog(TRUE, LOG_INFO, CFSTR("%s  flags     = 0x%08x"),
					      targetPrivate->log_prefix,
					      targetPrivate->serverInfo.flags);
					if (targetPrivate->serverInfo.if_index != 0) {
						SCLog(TRUE, LOG_INFO, CFSTR("%s  device    = %s (%hu%s)"),
						      targetPrivate->log_prefix,
						      targetPrivate->serverInfo.if_name,
						      targetPrivate->serverInfo.if_index,
						      targetPrivate->serverInfo.sleeping ? ", z" : "");
					}
					if (targetPrivate->serverInfo.cycle != targetPrivate->cycle) {
						SCLog(TRUE, LOG_INFO, CFSTR("%s  forced"),
						      targetPrivate->log_prefix);
					}
				}
			}
		} else if ((type == XPC_TYPE_ERROR) && (reply == XPC_ERROR_CONNECTION_INVALID)) {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("SCNetworkReachability server not available"));
			serverAvailable = FALSE;
			ok = TRUE;
		} else if ((type == XPC_TYPE_ERROR) && (reply == XPC_ERROR_CONNECTION_INTERRUPTED)) {
			SCLog(TRUE, LOG_DEBUG,
			      CFSTR("reach target %p: SCNetworkReachability server failure, retry status"),
			      target);
			retry = TRUE;
		} else {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("reach target %p: _targetStatus with unexpected reply"),
			      target);
			log_xpc_object("  reply", reply);
		}

		xpc_release(reply);
	}

	if (retry) {
		// reconnect
		_reach_server_target_reconnect(connection, target);

		// and retry
		retry = FALSE;
		goto retry;
	}

	xpc_release(reqdict);
	return ok;
}


static Boolean
_reach_server_target_unschedule(xpc_connection_t connection, SCNetworkReachabilityRef target)
{
	Boolean				ok		= FALSE;
	xpc_object_t			reply;
	xpc_object_t			reqdict;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	// create message
	reqdict = xpc_dictionary_create(NULL, NULL, 0);

	// set request
	xpc_dictionary_set_int64(reqdict, REACH_REQUEST, REACH_REQUEST_UNSCHEDULE);

	// add the target [ID]
	xpc_dictionary_set_uint64(reqdict, REACH_CLIENT_TARGET_ID, (uintptr_t)target);

	reply = xpc_connection_send_message_with_reply_sync(connection, reqdict);
	if (reply != NULL) {
		xpc_type_t	type;

		type = xpc_get_type(reply);
		if (type == XPC_TYPE_DICTIONARY) {
			int64_t		status;

			status = xpc_dictionary_get_int64(reply, REACH_REQUEST_REPLY);
			switch (status) {
				case REACH_REQUEST_REPLY_OK :
					ok = TRUE;
					break;
				case REACH_REQUEST_REPLY_UNKNOWN :
					SCLog(TRUE, LOG_DEBUG,
					      CFSTR("reach target %p: SCNetworkReachability server failure, no need to unschedule"),
					      target);
					break;
				default :
					SCLog(TRUE, LOG_INFO, CFSTR("%s  target unschedule failed"),
					      targetPrivate->log_prefix);
					log_xpc_object("  reply", reply);
			}

			if (ok) {
				CFRelease(target);
			}
		} else if ((type == XPC_TYPE_ERROR) && (reply == XPC_ERROR_CONNECTION_INVALID)) {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("SCNetworkReachability server not available"));
			serverAvailable = FALSE;
			ok = TRUE;
		} else if ((type == XPC_TYPE_ERROR) && (reply == XPC_ERROR_CONNECTION_INTERRUPTED)) {
			SCLog(TRUE, LOG_DEBUG,
			      CFSTR("reach target %p: SCNetworkReachability server failure, no need to unschedule"),
			      target);
			ok = TRUE;
		} else {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("reach target %p: _targetUnschedule with unexpected reply"),
			      target);
			log_xpc_object("  reply", reply);
		}

		xpc_release(reply);
	}

	xpc_release(reqdict);
	return ok;
}


#pragma mark -
#pragma mark Reconnect


static void
_reach_server_target_reconnect(xpc_connection_t connection, SCNetworkReachabilityRef target)
{
	Boolean				ok;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	if (!targetPrivate->serverActive) {
		// if target already removed
		return;
	}

	// server has been restarted
	targetPrivate->cycle = 0;

	// re-associate with server
	ok = _reach_server_target_add(connection, target);
	if (!ok) {
		// if we could not add the target
		return;
	}

	if (!targetPrivate->serverScheduled) {
		// if not scheduled
		return;
	}

	// ... and re-schedule with server
	ok = _reach_server_target_schedule(connection, target);
	if (!ok) {
		// if we could not reschedule the target
		return;
	}

	// .. and update our status
	__SCNetworkReachabilityPerformNoLock(target);

	return;
}


static void
_reach_connection_reconnect(xpc_connection_t connection)
{
	dispatch_queue_t	q;

	q = _reach_requests_rbt_queue();
	dispatch_sync(q, ^{
		struct rb_node		*rbn;
		struct rb_tree		*rbt;

		rbt = _reach_requests_rbt();
		rbn = rb_tree_iterate(rbt, NULL, RB_DIR_RIGHT);
		for ( ; rbn != NULL ; rbn = rb_tree_iterate(rbt, rbn, RB_DIR_LEFT)) {
			reach_request_t			*rbt_request;
			SCNetworkReachabilityRef	target;

			rbt_request = RBNODE_TO_REACH_REQUEST(rbn);

			target = rbt_request->target;
			CFRetain(target);
			dispatch_async(__SCNetworkReachability_concurrent_queue(), ^{
				_reach_server_target_reconnect(connection, target);
				CFRelease(target);
			});
		}
	});

	return;
}


#pragma mark -
#pragma mark SPI (exposed)


Boolean
_SCNetworkReachabilityServer_snapshot(void)
{
	xpc_connection_t	c;
	Boolean			ok	= FALSE;
	xpc_object_t		reply;
	xpc_object_t		reqdict;

	// initialize connection with SCNetworkReachability server
	c = _reach_connection();
	if (c == NULL) {
		return FALSE;
	}

	// create message
	reqdict = xpc_dictionary_create(NULL, NULL, 0);

	// set request
	xpc_dictionary_set_int64(reqdict, REACH_REQUEST, REACH_REQUEST_SNAPSHOT);

	// add the process name (for debugging)
	add_proc_name(reqdict);

    retry :

	// send request
	reply = xpc_connection_send_message_with_reply_sync(c, reqdict);
	if (reply != NULL) {
		xpc_type_t	type;

		type = xpc_get_type(reply);
		if (type == XPC_TYPE_DICTIONARY) {
			int64_t		status;

			status = xpc_dictionary_get_int64(reply, REACH_REQUEST_REPLY);
			ok = (status == REACH_REQUEST_REPLY_OK);
		} else if ((type == XPC_TYPE_ERROR) && (reply == XPC_ERROR_CONNECTION_INVALID)) {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("SCNetworkReachability server not available"));
			serverAvailable = FALSE;
		} else if ((type == XPC_TYPE_ERROR) && (reply == XPC_ERROR_CONNECTION_INTERRUPTED)) {
			SCLog(TRUE, LOG_DEBUG,
			      CFSTR("SCNetworkReachability server failure, retrying"));
			xpc_release(reply);
			goto retry;
		} else {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("_snapshot with unexpected reply"));
			log_xpc_object("  reply", reply);
		}

		xpc_release(reply);
	}

	xpc_release(reqdict);
	return ok;
}


__private_extern__
Boolean
__SCNetworkReachabilityServer_targetAdd(SCNetworkReachabilityRef target)
{
	xpc_connection_t		c;
	Boolean				ok;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	c = _reach_connection();
	if (c == NULL) {
		return FALSE;
	}

	ok = _reach_server_target_add(c, target);
	if (ok) {
		_SC_ATOMIC_CMPXCHG(&targetPrivate->serverActive, FALSE, TRUE);
	}

	return ok;
}


__private_extern__
void
__SCNetworkReachabilityServer_targetRemove(SCNetworkReachabilityRef target)
{
	xpc_connection_t		c;
	Boolean				ok;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	if (!targetPrivate->serverActive) {
		// if not active
		return;
	}

	c = _reach_connection();
	if (c == NULL) {
		return;
	}

	ok = _reach_server_target_remove(c, target);
	if (ok) {
		_SC_ATOMIC_CMPXCHG(&targetPrivate->serverActive, TRUE, FALSE);
	}

	return;
}


__private_extern__
Boolean
__SCNetworkReachabilityServer_targetSchedule(SCNetworkReachabilityRef target)
{
	xpc_connection_t		c;
	Boolean				ok;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	c = _reach_connection();
	if (c == NULL) {
		return FALSE;
	}

	_reach_request_add(target);
	ok = _reach_server_target_schedule(c, target);
	if (ok) {
		_SC_ATOMIC_CMPXCHG(&targetPrivate->serverScheduled, FALSE, TRUE);
	} else {
		_reach_request_remove(target);
	}

	return ok;
}


__private_extern__
Boolean
__SCNetworkReachabilityServer_targetStatus(SCNetworkReachabilityRef target)
{
	xpc_connection_t		c;
	Boolean				ok;

	c = _reach_connection();
	if (c == NULL) {
		return FALSE;
	}

	ok = _reach_server_target_status(c, target);
	return ok;
}


__private_extern__
Boolean
__SCNetworkReachabilityServer_targetUnschedule(SCNetworkReachabilityRef target)
{
	xpc_connection_t		c;
	Boolean				ok;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	if (!targetPrivate->serverScheduled) {
		// if not scheduled
		return TRUE;
	}

	c = _reach_connection();
	if (c == NULL) {
		return FALSE;
	}

	ok = _reach_server_target_unschedule(c, target);
	if (ok) {
		_SC_ATOMIC_CMPXCHG(&targetPrivate->serverScheduled, TRUE, FALSE);
		_reach_request_remove(target);
	} else {
		// if unschedule failed
	}

	return ok;
}

#endif	// HAVE_REACHABILITY_SERVER
