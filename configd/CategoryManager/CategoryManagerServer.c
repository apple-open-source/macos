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
 * CategoryManagerServer.c
 */

/*
 * Modification History
 *
 * November 7, 2022	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#include <CoreFoundation/CoreFoundation.h>
#include <xpc/xpc.h>
#include <xpc/private.h>
#include <sys/queue.h>
#include <SystemConfiguration/SCNetworkConfigurationPrivate.h>
#define __SC_CFRELEASE_NEEDED	1
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCNetworkCategoryManager.h>
#include <os/state_private.h>
#include "CategoryManagerServer.h"
#include "symbol_scope.h"
#include "CategoryManagerPrivate.h"
#include "CategoryManagerInternal.h"

#define MANAGER_ENTITLEMENT					\
	"com.apple.private.SCNetworkCategoryManager.manager"
#define MANAGER_OBSERVER_ENTITLEMENT					\
	"com.apple.private.SCNetworkCategoryManager.observer"

typedef struct CategorySession CategorySession, * CategorySessionRef;

#define LIST_HEAD_CategorySession LIST_HEAD(CategorySessionHead, CategorySession)
#define LIST_ENTRY_CategorySession LIST_ENTRY(CategorySession)
LIST_HEAD_CategorySession	S_CategorySessions;

struct CategorySession {
	LIST_ENTRY_CategorySession	link;
	xpc_connection_t		connection;

	Boolean				in_list;
	pid_t				pid;
	char *				process_name;

	char *				category;
	char *				ifname;
	SCNetworkCategoryManagerFlags 	flags;	
    
	char *				value;
	char *				active_value;
};

/**
 ** Support Functions
 **/

static CFRunLoopRef		S_runloop;
static CFRunLoopSourceRef	S_signal_source;

static void
SetNotificationInfo(CFRunLoopRef runloop, CFRunLoopSourceRef rls)
{
    S_runloop = runloop;
    S_signal_source = rls;
    return;
}

static void
SendNotification(void)
{
	my_log(LOG_NOTICE, "%s\n", __func__);
	if (S_signal_source != NULL) {
		CFRunLoopSourceSignal(S_signal_source);
		if (S_runloop != NULL) {
			CFRunLoopWakeUp(S_runloop);
		}
	}
	return;
}

static dispatch_queue_t
ServerQueue(void)
{
	static dispatch_queue_t 	S_queue;

	if (S_queue == NULL) {
		S_queue = dispatch_queue_create("CategoryManagerServer", NULL);
	}
	return (S_queue);
}

static Boolean
connectionIsRoot(xpc_connection_t connection)
{
	uid_t		uid;

	uid = xpc_connection_get_euid(connection);
	return (uid == 0);
}

static Boolean
connectionHasEntitlement(xpc_connection_t connection, const char * entitlement)
{
	Boolean 		entitled = FALSE;
	xpc_object_t 	val;

	val = xpc_connection_copy_entitlement_value(connection, entitlement);
	if (val != NULL) {
		if (xpc_get_type(val) == XPC_TYPE_BOOL) {
			entitled = xpc_bool_get_value(val);
		}
		xpc_release(val);
	}
	return (entitled);
}

static Boolean
connectionAllowAccess(xpc_connection_t connection,
		      const char * entitlement)
{
	return (connectionIsRoot(connection)
		|| connectionHasEntitlement(connection, entitlement));
}

static Boolean
connectionAllowManagerAccess(xpc_connection_t connection)
{
	return (connectionAllowAccess(connection, MANAGER_ENTITLEMENT));
}

static Boolean
connectionAllowManagerObserverAccess(xpc_connection_t connection)
{
	Boolean		allowed = FALSE;
	void *		context;

	context = xpc_connection_get_context(connection);
	if (context == NULL) {
		allowed = connectionAllowAccess(connection,
						MANAGER_OBSERVER_ENTITLEMENT);
		if (!allowed) {
			my_log(LOG_NOTICE,
			       "%s: connection %p pid %d missing entitlement, "
			       "permission denied",
			       __func__, 
			       connection,
			       xpc_connection_get_pid(connection));
			xpc_connection_set_context(connection,
						   (void *)kCFBooleanFalse);
			goto done;
		}
		my_log(LOG_DEBUG,
		       "%s: connection %p pid %d connection allowed access",
		       __func__, 
		       connection,
		       xpc_connection_get_pid(connection));
		xpc_connection_set_context(connection, (void *)kCFBooleanTrue);
	}
	else if (context == kCFBooleanFalse) {
		my_log(LOG_NOTICE,
		       "%s: connection %p pid %d permission denied",
		       __func__, 
		       connection,
		       xpc_connection_get_pid(connection));
	}
	else if (context == kCFBooleanTrue) {
		my_log(LOG_DEBUG,
		       "%s: connection %p pid %d access is allowed",
		       __func__, 
		       connection,
		       xpc_connection_get_pid(connection));
		allowed = TRUE;
	}

 done:
	return (allowed);
}

/**
 ** CategorySession
 **/
static const char *
CategorySessionGetCategory(CategorySessionRef session)
{
	return (session->category);
}

static pid_t
CategorySessionGetPID(CategorySessionRef session)
{
	return (session->pid);
}

static const char *
CategorySessionGetProcessName(CategorySessionRef session)
{
	return (session->process_name);
}

static const char *
CategorySessionGetInterfaceName(CategorySessionRef session)
{
	return (session->ifname);
}

static void
CategorySessionSetValue(CategorySessionRef session, const char * value)
{
	char *	new_value = NULL;

	if (value != NULL) {
		new_value = strdup(value);
	}
	if (session->value != NULL) {
		free(session->value);
	}
	session->value = new_value;
}

static void
CategorySessionSetActiveValue(CategorySessionRef session, const char * value)
{
	char *	new_value = NULL;

	if (value != NULL) {
		new_value = strdup(value);
	}
	if (session->active_value != NULL) {
		free(session->active_value);
	}
	session->active_value = new_value;
}

static void
CategorySessionInvalidate(CategorySessionRef session)
{
	my_log(LOG_DEBUG, "%s: invalidating %p", __func__, session);
	if (session->in_list) {
		LIST_REMOVE(session, link);
		session->in_list = false;
	}
	SendNotification();
	return;
}

static void
CategorySessionRelease(void * p)
{
	CategorySessionRef	session = (CategorySessionRef)p;

	my_log(LOG_DEBUG,
	       "%s: releasing %s [%d] %s session %p",
	       __func__,
	       CategorySessionGetProcessName(session),
	       CategorySessionGetPID(session),
	       CategorySessionGetCategory(session), p);
	free(session->process_name);
	free(session->category);
	if (session->ifname != NULL) {
		free(session->ifname);
	}
	CategorySessionSetValue(session, NULL);
	CategorySessionSetActiveValue(session, NULL);
	assert(session->in_list == false);
	free(p);
	return;
}

static CategorySessionRef
CategorySessionCreate(xpc_connection_t connection, const char * process_name,
		      const char * category, const char * ifname,
		      SCNetworkCategoryManagerFlags flags)
{
	CategorySessionRef	session;

	session = (CategorySessionRef)malloc(sizeof(*session));
	memset(session, 0, sizeof(*session));
	session->connection = connection;
	session->pid = xpc_connection_get_pid(connection);
	if (process_name == NULL) {
		process_name = "<unknown>";
	}
	session->process_name = strdup(process_name);
	session->category = strdup(category);
	session->flags = flags;
	if (ifname != NULL) {
		session->ifname = strdup(ifname);
	}
	xpc_connection_set_finalizer_f(connection, CategorySessionRelease);
	xpc_connection_set_context(connection, session);
	LIST_INSERT_HEAD(&S_CategorySessions, session, link);
	session->in_list = true;

	my_log(LOG_DEBUG,
	       "%s: created %s [%d] category %s ifname %s"
	       " session %p (connection %p)",
	       __func__,
	       CategorySessionGetProcessName(session),
	       CategorySessionGetPID(session),
	       CategorySessionGetCategory(session),
	       (ifname != NULL) ? ifname : "<any>",
	       session, connection);
	return (session);
}

static CategorySessionRef
CategorySessionLookup(const char * category, const char * ifname)
{
	CategorySessionRef		ret_session = NULL;
	CategorySessionRef		session;

	LIST_FOREACH(session, &S_CategorySessions, link) {
		if (strcmp(session->category, category) != 0) {
			/* not the same category */
			continue;
		}
		if (ifname == NULL || session->ifname == NULL) {
			/* interface name is not relevant */
			ret_session = session;
			break;
		}
		if (strcmp(ifname, session->ifname) == 0) {
			/* same category and ifname */
			ret_session = session;
			break;
		}
	}
	return (ret_session);
}

static CategorySessionRef
connectionGetCategorySession(xpc_connection_t connection)
{
	void *			context;
	CategorySessionRef	session = NULL;

	context = xpc_connection_get_context(connection);
	if (context != NULL
	    && context != kCFBooleanTrue
	    && context != kCFBooleanFalse) {
		session = (CategorySessionRef)context;
	}
	return (session);
}

static CFDictionaryRef
CategorySessionCopyInfo(CategorySessionRef session)
{
	CFStringRef		active_value = NULL;
	CFStringRef		category;
	CFIndex			count;
	CFDictionaryRef		dict;
	CFNumberRef		flags = NULL;
	CFStringRef		ifname = NULL;
#define N_SESSION_KEYS		7
	const void *		keys[N_SESSION_KEYS];
	CFNumberRef		pid_cf;
	CFStringRef		process_name;
	CFStringRef		value = NULL;
	const void *		values[N_SESSION_KEYS];

	pid_cf = CFNumberCreate(NULL, kCFNumberSInt32Type, &session->pid);
	process_name = cfstring_create_with_cstring(session->process_name);
	category = cfstring_create_with_cstring(session->category);
	if (session->value != NULL) {
		value = cfstring_create_with_cstring(session->value);
	}
	if (session->active_value != NULL) {
		active_value
			= cfstring_create_with_cstring(session->active_value);
	}
	if (session->ifname != NULL) {
		ifname = cfstring_create_with_cstring(session->ifname);
	}
	flags = CFNumberCreate(NULL, kCFNumberSInt32Type, &session->flags);
	
	count = 0;
	keys[count] = kCategoryInformationKeyProcessID; 	/* 0 */
	values[count] = pid_cf;
	count++;

	keys[count] = kCategoryInformationKeyProcessName;	/* 1 */
	values[count] = process_name;
	count++;

	keys[count] = kCategoryInformationKeyCategory;		/* 2 */
	values[count] = category;
	count++;

	if (ifname != NULL) {
		keys[count] = kCategoryInformationKeyInterfaceName; /* 3 */
		values[count] = ifname;
		count++;
	}
	if (value != NULL) {
		keys[count] = kCategoryInformationKeyValue;	/* 4 */
		values[count] = value;
		count++;
	}
	if (active_value != NULL) {
		keys[count] = kCategoryInformationKeyActiveValue;/* 5 */
		values[count] = active_value;
		count++;
	}
	if (flags != NULL) {
		keys[count] = kCategoryInformationKeyFlags; /* 6 */
		values[count] = flags;
		count++;
	}

	assert(count <= N_SESSION_KEYS);

	dict = CFDictionaryCreate(NULL,
				  (const void * *)keys,
				  (const void * *)values,
				  count,
				  &kCFTypeDictionaryKeyCallBacks,
				  &kCFTypeDictionaryValueCallBacks);
	__SC_CFRELEASE(pid_cf);
	__SC_CFRELEASE(process_name);
	__SC_CFRELEASE(category);
	__SC_CFRELEASE(ifname);
	__SC_CFRELEASE(value);
	__SC_CFRELEASE(active_value);
	__SC_CFRELEASE(flags);
	return (dict);
}

static CFMutableArrayRef
mutable_array_create(CFIndex capacity)
{
    return CFArrayCreateMutable(NULL, capacity, &kCFTypeArrayCallBacks);
}

static CFArrayRef
CategorySessionCopyInfoAll(void)
{
	CFMutableArrayRef	list = NULL;
	CategorySessionRef	session;

	LIST_FOREACH(session, &S_CategorySessions, link) {
		CFDictionaryRef		info;

		info = CategorySessionCopyInfo(session);
		if (list == NULL) {
			list = mutable_array_create(0);
		}
		CFArrayAppendValue(list, info);
		CFRelease(info);
	}
	return (list);
}

static void
CategorySessionNotify(CategorySessionRef session)
{
	xpc_object_t 	message;

	/* send an empty message to notify the client */
	message = xpc_dictionary_create(NULL, NULL, 0);
	xpc_connection_send_message(session->connection, message);
	xpc_release(message);
}

/**
 ** os_state handler
 **/
static CFArrayRef
CategoryManagerCopySessionInfo(void)
{
	CFMutableArrayRef	list;
	CategorySessionRef	session;

	list = mutable_array_create(0);
	LIST_FOREACH(session, &S_CategorySessions, link) {
		CFDictionaryRef		info;

		info = CategorySessionCopyInfo(session);
		if (info != NULL) {
			CFArrayAppendValue(list, info);
			CFRelease(info);
		}
	}
	return (list);
}

static os_state_data_t
CategoryManagerCopyOSStateData(os_state_hints_t hints)
{
#pragma unused(hints)
	CFDataRef		data;
	CFArrayRef		list;
	os_state_data_t		state_data;
	size_t			state_data_size;
	CFIndex			state_len;

	/* copy session information list */
	list = CategoryManagerCopySessionInfo();

	/* serialize the array into XML plist form */
	data = CFPropertyListCreateData(NULL,
					list,
					kCFPropertyListBinaryFormat_v1_0,
					0,
					NULL);
	CFRelease(list);
	state_len = CFDataGetLength(data);
	state_data_size = OS_STATE_DATA_SIZE_NEEDED(state_len);
	if (state_data_size > MAX_STATEDUMP_SIZE) {
		state_data = NULL;
		my_log(LOG_NOTICE, "%s: state data too large (%zu > %d)",
		       __func__, state_data_size, MAX_STATEDUMP_SIZE);
	}
	else {
		state_data = calloc(1, state_data_size);
		state_data->osd_type = OS_STATE_DATA_SERIALIZED_NSCF_OBJECT;
		state_data->osd_data_size = (uint32_t)state_len;
		strlcpy(state_data->osd_title,
			"CategoryManager Sessions",
			sizeof(state_data->osd_title));
		memcpy(state_data->osd_data, CFDataGetBytePtr(data), state_len);
	}
	CFRelease(data);
	return (state_data);
}

static void
CategoryManagerServerAddStateHandler(void)
{
	os_state_block_t	dump_state;

	dump_state = ^os_state_data_t(os_state_hints_t hints)
		{
			return (CategoryManagerCopyOSStateData(hints));
		};

	(void) os_state_add_handler(ServerQueue(), dump_state);
}

/**
 ** CategoryManagerServer
 **/

static int
HandleRegister(xpc_connection_t connection,
	       xpc_object_t request)
{
	const char *			category;
	void *				context;
	const char *			ifname;
	SCNetworkCategoryManagerFlags	flags;
	const char 	*		process_name;
	CategorySessionRef		session;


	context = xpc_connection_get_context(connection);
	if (context != NULL) {
		my_log(LOG_NOTICE,
		       "%s: connection %p pid %d trying to register again",
		       __func__,
		       connection, xpc_connection_get_pid(connection));
		return (EINVAL);
	}
	if (!connectionAllowManagerAccess(connection)) {
		my_log(LOG_NOTICE,
		       "%s: connection %p pid %d permission denied",
		       __func__,
		       connection, xpc_connection_get_pid(connection));
		xpc_connection_set_context(connection, (void *)kCFBooleanFalse);
		return (EPERM);
	}
	category
		= xpc_dictionary_get_string(request,
					    kCategoryManagerRequestKeyCategory);
	if (category == NULL) {
		return (EINVAL);
	}
	ifname = xpc_dictionary_get_string(request,
				   kCategoryManagerRequestKeyInterfaceName);

	flags = (SCNetworkCategoryManagerFlags)
		xpc_dictionary_get_int64(request,
					 kCategoryManagerRequestKeyFlags);
	if (flags != 0) {
		if (flags !=
		    kSCNetworkCategoryManagerFlagsKeepConfigured) {
			/* unsupported flags */
			return (EINVAL);
		}
		if (ifname == NULL) {
			/* unsupported combination */
			return (EINVAL);
		}
	}
	session = CategorySessionLookup(category, ifname);
	if (session != NULL) {
		my_log(LOG_NOTICE,
		       "connection %p pid %d category %s already exists",
		       connection,
		       xpc_connection_get_pid(connection),
		       category);
		return (EEXIST);
	}
	process_name
		= xpc_dictionary_get_string(request,
				    kCategoryManagerRequestKeyProcessName);
	session = CategorySessionCreate(connection, process_name,
					category, ifname, flags);
	my_log(LOG_NOTICE,
	       "CategoryManager[Register]: %s[%d] category %s interface %s%s",
	       CategorySessionGetProcessName(session),
	       CategorySessionGetPID(session),
	       CategorySessionGetCategory(session),
	       (ifname != NULL) ? ifname : "<any>",
	       (flags != 0) ? " [no_default]" : "");
	SendNotification();
	return (0);
}

static Boolean
my_cstring_equal(const char * value, const char * old_value)
{
	Boolean		equal;

	if (value != NULL && old_value != NULL) {
		equal = strcmp(old_value, value) == 0;
	}
	else if (value != NULL || old_value != NULL) {
		equal = FALSE;
	}
	else {
		equal = TRUE;
	}
	return (equal);
}

static int
HandleActivateValue(xpc_connection_t connection,
		    xpc_object_t request)
{
	bool			changed = false;
	const char *		ifname;
	const char *		old_value;
	const char *		value;
	CategorySessionRef	session;

	session = connectionGetCategorySession(connection);
	if (session == NULL) {
		/* reject the request */
		my_log(LOG_NOTICE,
		       "CategoryManager[ActivateValue]: no session %p",
		       connection);
		return (EINVAL);
	}
	value = xpc_dictionary_get_string(request,
					  kCategoryManagerRequestKeyValue);
	old_value = session->value;
	changed = !my_cstring_equal(value, old_value);
	CategorySessionSetValue(session, value);
	ifname = CategorySessionGetInterfaceName(session);
	if (changed) {
		my_log(LOG_NOTICE,
		       "CategoryManager[ActivateValue]: %s[%d] "
		       "category %s interface %s value %s",
		       CategorySessionGetProcessName(session),
		       CategorySessionGetPID(session),
		       CategorySessionGetCategory(session),
		       (ifname != NULL) ? ifname : "<any>",
		       (value != NULL) ? value : "<none>");
		SendNotification();
	}
	return (0);
}

static int
HandleGetActiveValueNoSession(xpc_connection_t connection,
			      xpc_object_t request,
			      xpc_object_t reply)
{
	const char *		category;
	int			error = 0;
	const char *		ifname;
	CategorySessionRef	session;

	if (!connectionAllowManagerObserverAccess(connection)) {
		error = EPERM;
		goto done;
	}
	category = xpc_dictionary_get_string(request,
					     kCategoryManagerRequestKeyCategory);
	if (category == NULL) {
		error = EINVAL;
		goto done;
	}
	ifname = xpc_dictionary_get_string(request,
				   kCategoryManagerRequestKeyInterfaceName);
	session = CategorySessionLookup(category, ifname);
	if (session == NULL) {
		error = ENOENT;
		goto done;
	}
	if (session->active_value != NULL) {
		xpc_dictionary_set_string(reply,
					  kCategoryManagerResponseKeyActiveValue,
					  session->active_value);
	}
	
 done:
	return (error);
}

static int
HandleGetActiveValue(xpc_connection_t connection,
		     xpc_object_t request,
		     xpc_object_t reply)
{
	int			error = 0;
	CategorySessionRef	session;

	session = connectionGetCategorySession(connection);
	if (session == NULL) {
		error = HandleGetActiveValueNoSession(connection,
						      request, reply);
	}
	else if (session->active_value != NULL) {
		xpc_dictionary_set_string(reply,
					  kCategoryManagerResponseKeyActiveValue,
					  session->active_value);
	}
	return (error);
}

static void
CategoryManagerServerHandleDisconnect(xpc_connection_t connection)
{
	CategorySessionRef	session;

	my_log(LOG_DEBUG, "CategoryManagerServer: client %p went away",
	       connection);
	session = connectionGetCategorySession(connection);
	if (session == NULL) {
		/* nothing to clean up */
		return;
	}
	CategorySessionInvalidate(session);
	return;
}

static void
ServerHandleRequest(xpc_connection_t connection,
		    xpc_object_t request)
{
	xpc_type_t	type;

	type = xpc_get_type(request);
	if (type == XPC_TYPE_DICTIONARY) {
		int			error = 0;
		CategoryManagerRequestType request_type;
		xpc_connection_t	remote;
		xpc_object_t		reply;

		request_type = (CategoryManagerRequestType)
			xpc_dictionary_get_int64(request,
					 kCategoryManagerRequestKeyType);
		reply = xpc_dictionary_create_reply(request);
		switch (request_type) {
		case kCategoryManagerRequestTypeRegister:
			error = HandleRegister(connection, request);
			break;
		case kCategoryManagerRequestTypeActivateValue:
			error = HandleActivateValue(connection, request);
			break;
		case kCategoryManagerRequestTypeGetActiveValue:
			if (reply != NULL) {
				error = HandleGetActiveValue(connection,
							     request,
							     reply);
			}
			break;
		default:
			error = EINVAL;
			break;
		}
		if (reply == NULL) {
			/* client didn't want a reply */
			return;
		}
		xpc_dictionary_set_int64(reply,
					 kCategoryManagerResponseKeyError,
					 error);
		remote = xpc_dictionary_get_remote_connection(request);
		xpc_connection_send_message(remote, reply);
		xpc_release(reply);
	}
	else if (type == XPC_TYPE_ERROR) {
		if (request == XPC_ERROR_CONNECTION_INVALID) {
			CategoryManagerServerHandleDisconnect(connection);
		}
		else if (request == XPC_ERROR_CONNECTION_INTERRUPTED) {
			my_log(LOG_NOTICE, "connection interrupted");
		}
	}
	else {
		my_log(LOG_NOTICE, "unexpected event");
	}
	return;
}

static void
ServerHandleNewConnection(xpc_connection_t connection)
{
	xpc_handler_t	handler;

	handler = ^(xpc_object_t event) {
		ServerHandleRequest(connection, event);
	};
	xpc_connection_set_event_handler(connection, handler);
	xpc_connection_set_target_queue(connection, ServerQueue());
	xpc_connection_activate(connection);
	return;
}

static xpc_connection_t
CategoryManagerServerCreate(const char * name)
{
	uint64_t		flags = XPC_CONNECTION_MACH_SERVICE_LISTENER;
	xpc_connection_t	connection;
	xpc_handler_t		handler;
	dispatch_queue_t	queue;

	queue = ServerQueue();
	connection = xpc_connection_create_mach_service(name, queue, flags);
	if (connection == NULL) {
		return (NULL);
	}
	handler = ^(xpc_object_t event) {
		xpc_type_t	type;

		type = xpc_get_type(event);
		if (type == XPC_TYPE_CONNECTION) {
			ServerHandleNewConnection(event);
		}
		else if (type == XPC_TYPE_ERROR) {
			const char	*	desc;

			desc = xpc_dictionary_get_string(event,
						 XPC_ERROR_KEY_DESCRIPTION);
			if (event == XPC_ERROR_CONNECTION_INVALID) {
				my_log(LOG_NOTICE, "%s", desc);
				xpc_release(connection);
			}
			else {
				my_log(LOG_NOTICE, "%s", desc);
			}
		}
		else {
			my_log(LOG_NOTICE, "unknown event %p", type);
		}
	};
	xpc_connection_set_event_handler(connection, handler);
	xpc_connection_activate(connection);
	return (connection);
}

static Boolean
CategoryManagerServerAckOne(CFDictionaryRef info)
{
	CFStringRef		category;
	char *			category_c;
	Boolean			changed = FALSE;
	CFStringRef		ifname;
	char *			ifname_c;
	CategorySessionRef	session;
	CFStringRef		value;
	char *			value_c;

	category = CFDictionaryGetValue(info, kCategoryInformationKeyCategory);
	if (category == NULL) {
		my_log(LOG_NOTICE, "%s: %@ missing %@",
		       __func__, info,
		       kCategoryInformationKeyCategory);
		goto done;
	}
	category_c = cstring_create_with_cfstring(category);
	ifname = CFDictionaryGetValue(info,
				      kCategoryInformationKeyInterfaceName);
	ifname_c = cstring_create_with_cfstring(ifname);
	value = CFDictionaryGetValue(info,
				     kCategoryInformationKeyValue);
	value_c = cstring_create_with_cfstring(value);
	session = CategorySessionLookup(category_c, ifname_c);
	if (session == NULL) {
		my_log(LOG_NOTICE, "%s: no session for %s/%s",
		       __func__, category_c,
		       (ifname_c != NULL) ? ifname_c : "<any>");
		goto done;
	}
	if (!my_cstring_equal(value_c, session->active_value)) {
		CategorySessionSetActiveValue(session, value_c);
		CategorySessionNotify(session);
		changed = TRUE;
	}
	cstring_deallocate(category_c);
	cstring_deallocate(ifname_c);
	cstring_deallocate(value_c);
 done:
	return (changed);
}

static void
CategoryManagerServerNotifyStore(void)
{
	CFStringRef	key = kCategoryManagerNotificationKey;

	SC_log(LOG_NOTICE, "%s: %@", __func__, key);
	SCDynamicStoreNotifyValue(NULL, key);
}

static void
CategoryManagerServerInformationAckSync(CFArrayRef info)
{
	Boolean		changed = FALSE;
	CFIndex		count = CFArrayGetCount(info);

	for (CFIndex i = 0; i < count; i++) {
		CFDictionaryRef		dict;

		dict = (CFDictionaryRef)CFArrayGetValueAtIndex(info, i);
		if (CategoryManagerServerAckOne(dict)) {
			changed = TRUE;
		}
	}
	if (changed) {
		CategoryManagerServerNotifyStore();
	}
	return;
}

/**
 ** API
 **/
PRIVATE_EXTERN Boolean
CategoryManagerServerStart(CFRunLoopRef runloop, CFRunLoopSourceRef rls)
{
	xpc_connection_t	connection;

#define _SERVER_NAME	kNetworkCategoryManagerServerName
	SetNotificationInfo(runloop, rls);
	connection = CategoryManagerServerCreate(_SERVER_NAME);
	if (connection == NULL) {
		SetNotificationInfo(NULL, NULL);
		my_log(LOG_ERR,
		       "CategoryManagerServer: failed to create server");
		return (FALSE);
	}
	CategoryManagerServerAddStateHandler();
	return (TRUE);
}

PRIVATE_EXTERN CFArrayRef
CategoryManagerServerInformationCopy(void)
{
	dispatch_block_t	b;
	__block CFArrayRef	info;

	b = ^{
		info = CategorySessionCopyInfoAll();
	};
	dispatch_sync(ServerQueue(), b);
	return (info);
}

PRIVATE_EXTERN void
CategoryManagerServerInformationAck(CFArrayRef info)
{
	dispatch_block_t	b;

	if (info == NULL) {
		return;
	}
	b = ^{
		CategoryManagerServerInformationAckSync(info);
	};
	dispatch_sync(ServerQueue(), b);
}

#ifdef TEST_CATEGORY_MANAGER

static Boolean log_more;

static void
log_session_info(void)
{
	if (!log_more) {
		return;
	}
	dispatch_sync(ServerQueue(), ^{
			CFArrayRef	list;

			list = CategoryManagerCopySessionInfo();
			my_log(LOG_NOTICE, "%s: %@", __func__, list);
			CFRelease(list);
		});
}

static void
CategoryInformationChanged(void * _not_used)
{
#pragma unused(_not_used)
	CFArrayRef	info;

	log_session_info();
	info = CategoryManagerServerInformationCopy();
	my_log(LOG_NOTICE, "%s: info %@", __func__, info);
	CategoryManagerServerInformationAck(info);
	if (info != NULL) {
		CFRelease(info);
	}
	log_session_info();
	return;
}

static Boolean
ServerStart(void)
{
	CFRunLoopSourceContext 		context;
	CFRunLoopSourceRef		rls;
	Boolean				started;

	memset(&context, 0, sizeof(context));
	context.perform = CategoryInformationChanged;
	rls = CFRunLoopSourceCreate(NULL, 0, &context);
	started = CategoryManagerServerStart(CFRunLoopGetCurrent(), rls);
	if (!started) {
		my_log(LOG_ERR, "CategoryManagerServerStart failed");
	}
	else {
		CFRunLoopAddSource(CFRunLoopGetCurrent(), rls,
				   kCFRunLoopDefaultMode);
	}
	CFRelease(rls);
	return (started);
}

int
main(int argc, char * argv[])
{
	if (!ServerStart()) {
		exit(1);
	}
	printf("Server started\n");
	CFRunLoopRun();
}

#endif /* TEST_CATEGORY_MANAGER */
