/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

/*
 * CategoryManager.c
 * - IPC channel to PreferencesMonitor
 */

/*
 * Modification History
 *
 * November 6, 2022	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#include "CategoryManagerInternal.h"
#include "CategoryManager.h"
#include "CategoryManagerPrivate.h"
#include "symbol_scope.h"
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFXPCBridge.h>
#include <net/if.h>
#include <xpc/xpc.h>
#include <xpc/private.h>
#define __SC_CFRELEASE_NEEDED	1
#include <SystemConfiguration/SCPrivate.h>

/**
 ** CategoryManager support functions
 **/
static xpc_object_t
create_request(CategoryManagerRequestType type)
{
	xpc_object_t	request;

	request = xpc_dictionary_create(NULL, NULL, 0);
	xpc_dictionary_set_int64(request,
				 kCategoryManagerRequestKeyType,
				 type);
	return (request);
}

static errno_t
reply_get_error(xpc_object_t reply)
{
	int64_t	error;

	if (reply == XPC_ERROR_CONNECTION_INVALID) {
		return (ENOENT);
	}
	error = xpc_dictionary_get_int64(reply,
					 kCategoryManagerResponseKeyError);
	return ((errno_t)error);
}

static Boolean
CategoryManagerHandleResponse(xpc_object_t message, Boolean async,
			      CategoryManagerEvent * event_p)
{
	CategoryManagerEvent	event = kCategoryManagerEventNone;
	Boolean			success = FALSE;
	xpc_type_t		type;

	type = xpc_get_type(message);
	if (type == XPC_TYPE_DICTIONARY) {
		if (async) {
			if (xpc_dictionary_get_count(message) != 0) {
				/* not an empty dictionary */
				my_log(LOG_NOTICE,
				       "%s: unexpected message", __func__);
			}
			else {
				/* notification */
				event = kCategoryManagerEventValueAcknowledged;
			}
		}
		else {
			errno_t	error;

			error = reply_get_error(message);
			if (error != 0) {
				success = FALSE;
				my_log(LOG_NOTICE,
				       "%s: failure code %s (%d)",
				       __func__, strerror(error), error);
			}
			else {
				success = TRUE;
			}
		}
	}
	else if (type == XPC_TYPE_ERROR) {
		if (message == XPC_ERROR_CONNECTION_INTERRUPTED) {
			my_log(LOG_NOTICE,
			       "%s: connection interrupted",
			       __func__);
			event = kCategoryManagerEventConnectionInterrupted;
		}
		else if (message == XPC_ERROR_CONNECTION_INVALID) {
			my_log(LOG_NOTICE,
			       "%s: connection invalid %s",
			       __func__, async ? "[async]" : "");
			event = kCategoryManagerEventConnectionInvalid;
		}
		else {
			const char *	desc;

			desc = xpc_dictionary_get_string(message,
						 XPC_ERROR_KEY_DESCRIPTION);
			my_log(LOG_NOTICE, "%s: %s", __func__, desc);
		}
	}
	else {
		my_log(LOG_NOTICE, "%s: unknown event type : %p",
		       __func__, type);
	}
	if (event_p != NULL) {
		*event_p = event;
	}
	return (success);
}

static xpc_object_t
CategoryManagerSendRequest(xpc_connection_t connection,
			   xpc_object_t request)
{
	xpc_object_t	reply;

	while (TRUE) {
		CategoryManagerEvent	event;
		Boolean			success;

		reply = xpc_connection_send_message_with_reply_sync(connection,
								    request);
		if (reply == NULL) {
			my_log(LOG_NOTICE,
			       "%s: failed to send message",
			       __func__);
			break;
		}
		success = CategoryManagerHandleResponse(reply, FALSE, &event);
		if (success) {
			break;
		}
		if (event == kCategoryManagerEventConnectionInterrupted) {
			xpc_release(reply);
			continue;
		}
		break;
	}
	return (reply);
}

static xpc_object_t
register_request_create(CFStringRef category, CFStringRef ifname,
			SCNetworkCategoryManagerFlags flags)
{
	const char *	progname;
	xpc_object_t	request;

	request	= create_request(kCategoryManagerRequestTypeRegister);
	progname = getprogname();
	if (progname != NULL) {
		xpc_dictionary_set_string(request,
					  kCategoryManagerRequestKeyProcessName,
					  progname);
	}
	my_xpc_dictionary_set_cfstring(request,
				       kCategoryManagerRequestKeyCategory,
				       category);
	if (ifname != NULL) {
		my_xpc_dictionary_set_cfstring(request,
				       kCategoryManagerRequestKeyInterfaceName,
				       ifname);
	}
	xpc_dictionary_set_int64(request,
				 kCategoryManagerRequestKeyFlags,
				 flags);
	return (request);
}

static xpc_object_t
activate_request_create(CFStringRef value)
{
	xpc_object_t		request;

	request	= create_request(kCategoryManagerRequestTypeActivateValue);
	if (value != NULL) {
		my_xpc_dictionary_set_cfstring(request,
					       kCategoryManagerRequestKeyValue,
					       value);
	}
	return (request);
}

/**
 ** CategoryManager SPI
 **/
PRIVATE_EXTERN xpc_connection_t
CategoryManagerConnectionCreate(dispatch_queue_t queue,
				CategoryManagerEventHandler event_handler)
{
	xpc_connection_t	connection;
	uint64_t		flags = XPC_CONNECTION_MACH_SERVICE_PRIVILEGED;
	xpc_handler_t		handler;

#define _SERVER_NAME	kNetworkCategoryManagerServerName
	connection = xpc_connection_create_mach_service(_SERVER_NAME,
							queue, flags);
	handler = ^(xpc_object_t message) {
		CategoryManagerEvent	event;

		(void)CategoryManagerHandleResponse(message, TRUE, &event);
		if (event_handler == NULL) {
			return;
		}
		(event_handler)(connection, event);
	};
	xpc_connection_set_event_handler(connection, handler);
	xpc_connection_activate(connection);
	return (connection);
}

PRIVATE_EXTERN errno_t
CategoryManagerConnectionRegister(xpc_connection_t connection,
				  CFStringRef category,
				  CFStringRef ifname,
				  SCNetworkCategoryManagerFlags flags)
{
	errno_t		error = EINVAL;
	xpc_object_t	reply;
	xpc_object_t	request;

	request = register_request_create(category, ifname, flags);
	reply = CategoryManagerSendRequest(connection, request);
	xpc_release(request);
	if (reply != NULL) {
		error = reply_get_error(reply);
		if (error != 0) {
			my_log(LOG_NOTICE, "%s: failure code %s (%d)",
			       __func__, strerror(error), error);
		}
		xpc_release(reply);
	}
	else {
		my_log(LOG_NOTICE, "%s: no reply?", __func__);
	}
	return (error);
}

PRIVATE_EXTERN void
CategoryManagerConnectionSynchronize(xpc_connection_t connection,
				     CFStringRef category,
				     CFStringRef ifname,
				     SCNetworkCategoryManagerFlags flags,
				     CFStringRef value)
{
	xpc_object_t		request;

	/* register */
	request = register_request_create(category, ifname, flags);
	xpc_connection_send_message(connection, request);
	xpc_release(request);

	/* activate value */
	request = activate_request_create(value);
	xpc_connection_send_message(connection, request);
	xpc_release(request);
}

PRIVATE_EXTERN errno_t
CategoryManagerConnectionActivateValue(xpc_connection_t connection,
				       CFStringRef value)
{
	errno_t		error = EINVAL;
	xpc_object_t	reply;
	xpc_object_t	request;

	request = activate_request_create(value);
	reply = CategoryManagerSendRequest(connection, request);
	xpc_release(request);
	if (reply != NULL) {
		error = reply_get_error(reply);
		if (error != 0) {
			my_log(LOG_NOTICE, "%s: failure code %s (%d)",
			       __func__, strerror(error), error);
		}
		xpc_release(reply);
	}
	else {
		my_log(LOG_NOTICE, "%s: no reply?", __func__);
	}
	return (error);
}

PRIVATE_EXTERN CFStringRef
CategoryManagerConnectionCopyActiveValue(xpc_connection_t connection,
					 int * ret_error)
{
	errno_t		error = EINVAL;
	xpc_object_t	reply;
	xpc_object_t	request;
	CFStringRef	ret_active_value = NULL;

	request	= create_request(kCategoryManagerRequestTypeGetActiveValue);
	reply = CategoryManagerSendRequest(connection, request);
	xpc_release(request);
	if (reply != NULL) {
		const char *	active_value;

		error = reply_get_error(reply);
		if (error != 0) {
			my_log(LOG_NOTICE, "%s: failure code %s (%d)",
			       __func__, strerror(error), error);
		}
		active_value
			= xpc_dictionary_get_string(reply,
					kCategoryManagerResponseKeyActiveValue);
		if (active_value != NULL) {
			ret_active_value
				= cfstring_create_with_cstring(active_value);
		}
		xpc_release(reply);
	}
	else {
		my_log(LOG_NOTICE, "%s: no reply?", __func__);
	}

	*ret_error = error;
	return (ret_active_value);
}

PRIVATE_EXTERN CFStringRef
CategoryManagerConnectionCopyActiveValueNoSession(xpc_connection_t connection,
						  CFStringRef category,
						  CFStringRef ifname,
						  int * ret_error)
{
	errno_t		error = EINVAL;
	xpc_object_t	reply;
	xpc_object_t	request;
	CFStringRef	ret_active_value = NULL;

	request	= create_request(kCategoryManagerRequestTypeGetActiveValue);
	if (category != NULL) {
		my_xpc_dictionary_set_cfstring(request,
					kCategoryManagerRequestKeyCategory,
					category);
	}
	if (ifname != NULL) {
		my_xpc_dictionary_set_cfstring(request,
					kCategoryManagerRequestKeyInterfaceName,
					ifname);
	}
	reply = CategoryManagerSendRequest(connection, request);
	xpc_release(request);
	if (reply != NULL) {
		const char *	active_value;

		error = reply_get_error(reply);
		if (error != 0) {
			my_log(LOG_NOTICE, "%s: failure code %s (%d)",
			       __func__, strerror(error), error);
		}
		active_value
			= xpc_dictionary_get_string(reply,
					kCategoryManagerResponseKeyActiveValue);
		if (active_value != NULL) {
			ret_active_value
				= cfstring_create_with_cstring(active_value);
		}
		xpc_release(reply);
	}
	else {
		my_log(LOG_NOTICE, "%s: no reply?", __func__);
	}

	*ret_error = error;
	return (ret_active_value);
}

#ifdef TEST_CATEGORY_MANAGER

static Boolean G_get_value;

static void
get_value(CFStringRef category, CFStringRef ifname)
{
	static xpc_connection_t	connection;
	int			error;
	static dispatch_queue_t	queue;
	CFStringRef		value;

	if (queue == NULL) {
		queue = dispatch_queue_create("CategoryManagerGetValue", NULL);
	}
	if (connection == NULL) {
		connection = CategoryManagerConnectionCreate(queue, NULL);
	}
	value = CategoryManagerConnectionCopyActiveValueNoSession(connection,
								  category,
								  ifname,
								  &error);
	SCPrint(TRUE, stdout, CFSTR("%s: value %@\n"), __func__, value);
	__SC_CFRELEASE(value);
}

static void
change_value(xpc_connection_t connection, dispatch_queue_t queue,
	     CFStringRef value)
{
	errno_t		error;
	CFStringRef	current_value;
	CFStringRef	new_value;
	dispatch_time_t	when;

	current_value = CategoryManagerConnectionCopyActiveValue(connection,
								 &error);
	if (error != 0) {
		fprintf(stderr, "copy active value failed: %s (%d)\n",
			strerror(error), error);
	}
	if (current_value == NULL) {
		new_value = value;
	}
	else {
		new_value = NULL;
	}
	my_log(LOG_NOTICE, "Value is %@ current value is %@, activating %@",
	       value, current_value, new_value);
	__SC_CFRELEASE(current_value);
	SCPrint(TRUE, stdout, CFSTR("Activating: %@\n"), new_value);
	error = CategoryManagerConnectionActivateValue(connection,
						       new_value);
	if (error != 0) {
		fprintf(stderr, "activate failed: %s (%d)\n",
			strerror(error), error);
	}
#define _ACTIVATE_DELAY	(1000 * 1000 * 1000)
	when = dispatch_walltime(NULL, _ACTIVATE_DELAY);
	dispatch_after(when, queue, ^{
			change_value(connection, queue, value);
		});
}

static void
handle_acknowledge(xpc_connection_t connection)
{
	errno_t		error;
	CFStringRef	value;

	value = CategoryManagerConnectionCopyActiveValue(connection,
							 &error);
	if (error != 0) {
		fprintf(stderr, "copy active value failed: %s (%d)\n",
			strerror(error), error);
	}
	SCPrint(TRUE, stdout, CFSTR("Acknowledged: %@\n"),
		value);
	__SC_CFRELEASE(value);
}

static Boolean do_cycle;

static Boolean
register_manager(dispatch_queue_t queue,
		 CFStringRef category, CFStringRef ifname,
		 SCNetworkCategoryManagerFlags flags, CFStringRef value)
{
	xpc_connection_t		connection;
	CFStringRef			current_value;
	errno_t				error;
	CategoryManagerEventHandler	handler;
	Boolean				success = FALSE;
	dispatch_time_t			when;

	handler = ^(xpc_connection_t c, CategoryManagerEvent event) {
		switch (event) {
		case kCategoryManagerEventConnectionInvalid:
			printf("Invalid connection %p\n", c);
			break;
		case kCategoryManagerEventConnectionInterrupted:
			printf("Reconnected, re-registering %p\n", c);
			CategoryManagerConnectionSynchronize(c,
							     category,
							     ifname,
							     flags,
							     value);
			break;
		case kCategoryManagerEventValueAcknowledged:
			handle_acknowledge(c);
			get_value(category, ifname);
			break;
		default:
			break;
		}
	};
	connection = CategoryManagerConnectionCreate(queue, handler);
	if (connection == NULL) {
		fprintf(stderr, "CategoryManagerConnectionCreate failed\n");
		goto done;
	}
	printf("Connection %p\n", connection);
	error = CategoryManagerConnectionRegister(connection,
						  category,
						  ifname,
						  flags);
	if (error != 0) {
		fprintf(stderr, "register failed: %s (%d)\n",
			strerror(error), error);
		/* do it again */
		error = CategoryManagerConnectionRegister(connection,
							  category,
							  ifname,
							  flags);
		if (error != 0) {
			fprintf(stderr, "register failed(2): %s (%d)\n",
				strerror(error), error);
			goto done;
		}
	}
	error = CategoryManagerConnectionActivateValue(connection,
						       value);
	if (error != 0) {
		fprintf(stderr, "activate failed: %s (%d)\n",
			strerror(error), error);
		goto done;
	}
	current_value = CategoryManagerConnectionCopyActiveValue(connection,
								 &error);
	if (error != 0) {
		fprintf(stderr, "copy active value failed: %s (%d)\n",
			strerror(error), error);
		goto done;
	}
	SCPrint(TRUE, stdout, CFSTR("value is %@\n"), current_value);
	__SC_CFRELEASE(current_value);
	if (do_cycle) {
		when = dispatch_walltime(NULL, _ACTIVATE_DELAY);
		dispatch_after(when, queue, ^{
				change_value(connection, queue, value);
			});
	}
	success = TRUE;

 done:
	if (!success && connection != NULL) {
		xpc_release(connection);
	}
	return (success);
}

#include <getopt.h>

static void
usage(const char * progname)
{
	fprintf(stderr,
		"usage: %s  -c <category> [ -i <ifname>] "
		"[ -C -v <value> ] [ -f <flags> ]\n", progname);
	exit(1);
}

int
main(int argc, char * argv[])
{
	int 				ch;
	CFStringRef			category = NULL;
	SCNetworkCategoryManagerFlags	flags = 0;
	CFStringRef			ifname = NULL;
	const char *			progname = argv[0];
	dispatch_queue_t		queue;
	CFStringRef			value = NULL;

	while ((ch = getopt(argc, argv, "Cc:gF:i:v:")) != -1) {
		switch (ch) {
		case 'C':
			do_cycle = TRUE;
			break;
		case 'c':
			category = cfstring_create_with_cstring(optarg);
			break;
		case 'g':
			G_get_value = TRUE;
			break;
		case 'F':
			flags = strtoul(optarg, NULL, 0);
			break;
		case 'i':
			ifname = cfstring_create_with_cstring(optarg);
			break;
		case 'v':
			value = cfstring_create_with_cstring(optarg);
			break;
		default:
			break;
		}
	}
	if (category == NULL) {
		fprintf(stderr, "category must be specified\n");
		usage(progname);
	}
	if (do_cycle && value == NULL) {
		fprintf(stderr, "-C requires -v\n");
		usage(progname);
	}
	if (G_get_value) {
		get_value(category, ifname);
		/* do it again */
		get_value(category, ifname);
	}
	queue = dispatch_queue_create("CategoryManager", NULL);
	if (!register_manager(queue, category, ifname, flags, value)) {
		exit(1);
	}
	dispatch_main();
}

#endif /* TEST_CATEGORY_MANAGER */
