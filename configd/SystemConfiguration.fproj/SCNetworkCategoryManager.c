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
 * SCNetworkCategoryManager.c
 * - object to interact with category manager server
 *   to change per-category service configurations
 */

/*
 * Modification History
 *
 * November 4, 2022		Dieter Siegmund <dieter@apple.com>
 * - initial revision
 */


#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFBundlePriv.h>
#include <dispatch/dispatch.h>
#define __SC_CFRELEASE_NEEDED	1
#include "SCPrivate.h"
#include "SCNetworkCategoryManager.h"
#include "SCNetworkConfigurationPrivate.h"
#include "CategoryManager.h"

typedef struct __SCNetworkCategoryManager {
	CFRuntimeBase			cfBase;

	CFStringRef			category;
	CFStringRef			ifname;
	SCNetworkCategoryManagerFlags	flags;
	CFStringRef			value;
	xpc_connection_t		connection;
	dispatch_queue_t		queue;

	SCNetworkCategoryManagerNotify	notify;
	dispatch_queue_t		notify_queue;
} SCNetworkCategoryManager;

static CFStringRef 	__SCNetworkCategoryManagerCopyDescription(CFTypeRef cf);
static void	   	__SCNetworkCategoryManagerDeallocate(CFTypeRef cf);
static Boolean		__SCNetworkCategoryManagerEqual(CFTypeRef cf1, CFTypeRef cf2);
static CFHashCode	__SCNetworkCategoryManagerHash(CFTypeRef cf);

static CFTypeID __kSCNetworkCategoryManagerTypeID;

static const CFRuntimeClass SCNetworkCategoryManagerClass = {
	.version = 0,
	.className = "SCNetworkCategoryManager",
	.init = NULL,
	.copy = NULL,
	.finalize = __SCNetworkCategoryManagerDeallocate,
	.equal = __SCNetworkCategoryManagerEqual,
	.hash = __SCNetworkCategoryManagerHash,
	.copyFormattingDesc = NULL,
	.copyDebugDesc = __SCNetworkCategoryManagerCopyDescription,
#ifdef CF_RECLAIM_AVAILABLE
	NULL,
#endif
#ifdef CF_REFCOUNT_AVAILABLE
	NULL
#endif
};

static CFStringRef
__SCNetworkCategoryManagerCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef		allocator = CFGetAllocator(cf);
	CFMutableStringRef      result;
	SCNetworkCategoryManagerRef	category = (SCNetworkCategoryManagerRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL,
			     CFSTR("<%s %p [%p]> {"
				   " ID = %@, ifname = %@, flags = 0x%x }"),
			     SCNetworkCategoryManagerClass.className,
			     category, allocator, category->category,
			     category->ifname, category->flags);
	return result;
}

static void
__SCNetworkCategoryManagerDeallocate(CFTypeRef cf)
{
	SCNetworkCategoryManagerRef manager;

	manager = (SCNetworkCategoryManagerRef)cf;
	__SC_CFRELEASE(manager->category);
	__SC_CFRELEASE(manager->ifname);
	__SC_CFRELEASE(manager->value);
	if (manager->connection != NULL) {
		xpc_release(manager->connection);
		manager->connection = NULL;
	}
	if (manager->queue != NULL) {
		dispatch_release(manager->queue);
		manager->queue = NULL;
	}
	if (manager->notify_queue != NULL) {
		dispatch_release(manager->notify_queue);
		manager->notify_queue = NULL;
	}
	if (manager->notify != NULL) {
		Block_release(manager->notify);
		manager->notify = NULL;
	}
	return;
}

static Boolean
__SCNetworkCategoryManagerEqual(CFTypeRef cf1, CFTypeRef cf2)
{
	SCNetworkCategoryManagerRef	m1 = (SCNetworkCategoryManagerRef)cf1;
	SCNetworkCategoryManagerRef	m2 = (SCNetworkCategoryManagerRef)cf2;

	if (m1 == m2) {
		return (TRUE);
	}
	if (m1->flags != m2->flags) {
		return (FALSE);
	}
	if (!_SC_CFEqual(m1->ifname, m2->ifname)) {
		return (FALSE);
	}
	return (CFEqual(m1->category, m2->category));
}

static CFHashCode
__SCNetworkCategoryManagerHash(CFTypeRef cf)
{
	SCNetworkCategoryManagerRef	manager;
	CFHashCode			hash;

	manager = (SCNetworkCategoryManagerRef)cf;
	hash = CFHash(manager->category);
	if (manager->ifname != NULL) {
		hash ^= CFHash(manager->ifname);
	}
	return (hash);
}

static void
__SCNetworkCategoryManagerInitialize(void)
{
	static dispatch_once_t  initialized;

	dispatch_once(&initialized, ^{
		__kSCNetworkCategoryManagerTypeID
			= _CFRuntimeRegisterClass(&SCNetworkCategoryManagerClass);
	});
	return;
}

#define __kSCNetworkCategoryManagerSize					\
	(sizeof(SCNetworkCategoryManager) - sizeof(CFRuntimeBase))

static SCNetworkCategoryManagerRef
__SCNetworkCategoryManagerCreate(CFStringRef category, CFStringRef ifname,
				 SCNetworkCategoryManagerFlags flags)
				 
{
	SCNetworkCategoryManagerRef  	manager;

	__SCNetworkCategoryManagerInitialize();
	manager = (SCNetworkCategoryManagerRef)
		_CFRuntimeCreateInstance(NULL,
					 __kSCNetworkCategoryManagerTypeID,
					 __kSCNetworkCategoryManagerSize,
					 NULL);
	if (manager == NULL) {
		return NULL;
	}
	manager->category = CFStringCreateCopy(NULL, category);
	manager->flags = flags;
	if (ifname != NULL) {
		manager->ifname = CFStringCreateCopy(NULL, ifname);
	}
	return (manager);
}

static int
errno_to_scerror(errno_t error)
{
	int	status = kSCStatusFailed;

	switch (error) {
	case 0:
		status = kSCStatusOK;
		break;
	case EINVAL:
		status = kSCStatusInvalidArgument;
		break;
	case EPERM:
		status = kSCStatusAccessError;
		break;
	case ENOENT:
		status = kSCStatusNoStoreServer;
		break;
	default:
		break;
	}
	return (status);
}

static void
SCNetworkCategoryManagerSynchronize(SCNetworkCategoryManagerRef manager)
{
	CategoryManagerConnectionSynchronize(manager->connection,
					     manager->category,
					     manager->ifname,
					     manager->flags,
					     manager->value);
}

static dispatch_queue_t
get_global_queue(void)
{
	return dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
}

static void
SCNetworkCategoryManagerDeliverNotification(SCNetworkCategoryManagerRef manager)
{
	errno_t				error;
	SCNetworkCategoryManagerNotify	notify;
	dispatch_queue_t		queue;
	CFStringRef			value;

	value = CategoryManagerConnectionCopyActiveValue(manager->connection,
							 &error);
	SC_log(LOG_NOTICE,  "%s: value %@ error %d",
	       __func__, value, error);

	notify = manager->notify;
	if (notify == NULL) {
		SC_log(LOG_NOTICE,  "%s: no handler installed",
		       __func__);
		__SC_CFRELEASE(value);
		return;
	}
	queue = manager->notify_queue;	
	if (queue == NULL) {
		queue = get_global_queue();
	}
	dispatch_async(queue, ^{
			CFStringRef 	the_value = value;

			(notify)(the_value);
			__SC_CFRELEASE(the_value);
		});
	return;
}

/*
 * API
 */

CFTypeID
SCNetworkCategoryManagerGetTypeID(void)
{
	__SCNetworkCategoryManagerInitialize();
	return __kSCNetworkCategoryManagerTypeID;
}

SCNetworkCategoryManagerRef __nullable
SCNetworkCategoryManagerCreateWithInterface(CFStringRef category,
					    SCNetworkInterfaceRef netif,
					    SCNetworkCategoryManagerFlags flags,
					    CFDictionaryRef  options)
{
	errno_t				error;
	CategoryManagerEventHandler	handler;	
	CFStringRef			ifname;
	SCNetworkCategoryManagerRef	manager = NULL;
	int				status = kSCStatusInvalidArgument;

	if (options != NULL) {
		goto done;
	}
	if (flags != 0
	    && flags !=	kSCNetworkCategoryManagerFlagsKeepConfigured) {
		/* unsupported flags */
		goto done;
	}
	if (category == NULL || netif == NULL) {
		goto done;
	}
	ifname = SCNetworkInterfaceGetBSDName(netif);
	if (ifname == NULL) {
		goto done;
	}
	manager = __SCNetworkCategoryManagerCreate(category, ifname, flags);
	manager->queue
		= dispatch_queue_create("SCNetworkCategoryManager", NULL);
	handler = ^(xpc_connection_t connection, CategoryManagerEvent event) {
		if (manager->connection != connection) {
			SC_log(LOG_NOTICE,
			       "%s: connection %p != %p",
			       __func__,
			       manager->connection, connection);
			return;
		}
		switch (event) {
		case kCategoryManagerEventConnectionInvalid:
			SC_log(LOG_NOTICE,
			       "%s: invalid connection %p",
			       __func__, connection);
			break;
		case kCategoryManagerEventConnectionInterrupted:
			SC_log(LOG_NOTICE,
			       "%s: re-registering %p\n",
			       __func__,
			       connection);
			/* synchronize state */
			SCNetworkCategoryManagerSynchronize(manager);
			break;
		case kCategoryManagerEventValueAcknowledged:
			SCNetworkCategoryManagerDeliverNotification(manager);
			break;
		default:
			break;
		}
	};
	manager->connection
		= CategoryManagerConnectionCreate(manager->queue, handler);
	if (manager->connection == NULL) {
		CFRelease(manager);
		manager = NULL;
		goto done;
	}
	error = CategoryManagerConnectionRegister(manager->connection,
						  manager->category,
						  manager->ifname,
						  manager->flags);
	if (error != 0) {
		CFRelease(manager);
		manager = NULL;
	}
	status = errno_to_scerror(error);

 done:
	_SCErrorSet(status);
	return (manager);
}

void
SCNetworkCategoryManagerSetNotifyHandler(SCNetworkCategoryManagerRef manager,
					 dispatch_queue_t queue,
					 SCNetworkCategoryManagerNotify notify)
{
	dispatch_block_t	b;

	b = ^{
		/* set notify_queue */
		if (queue != NULL) {
			dispatch_retain(queue);
		}
		if (manager->notify_queue != NULL) {
			dispatch_release(manager->notify_queue);
		}
		manager->notify_queue = queue;
		
		/* set notify handler */
		if (manager->notify != NULL) {
			Block_release(manager->notify);
		}
		if (notify != NULL) {
			manager->notify = Block_copy(notify);
		}
	};
	dispatch_sync(manager->queue, b);
	return;
}

Boolean
SCNetworkCategoryManagerActivateValue(SCNetworkCategoryManagerRef manager,
				      CFStringRef  value)
{
	dispatch_block_t	b;
	__block errno_t		error;

	b = ^{
		xpc_connection_t	connection = manager->connection;

		error = CategoryManagerConnectionActivateValue(connection,
							       value);
	};
	dispatch_sync(manager->queue, b);
	_SCErrorSet(errno_to_scerror(error));
	return (error == 0);
}

__private_extern__ CFStringRef
__SCNetworkCategoryManagerCopyActiveValueNoSession(CFStringRef category,
						   SCNetworkInterfaceRef netif)
{
	dispatch_block_t		b;
	static xpc_connection_t		connection;
	int				error = 0;
	CFStringRef			ifname;
	static dispatch_once_t		initialized;
	CFStringRef			value = NULL;

	b = ^{
		dispatch_queue_t	queue;

		queue = dispatch_queue_create(__func__, NULL);
		connection = CategoryManagerConnectionCreate(queue, NULL);
		dispatch_release(queue);
	};
	dispatch_once(&initialized, b);
	if (connection == NULL) {
		goto done;
	}
	if (netif != NULL) {
		ifname = SCNetworkInterfaceGetBSDName(netif);
		if (ifname == NULL) {
			goto done;
		}
	}
	value = CategoryManagerConnectionCopyActiveValueNoSession(connection,
								  category,
								  ifname,
								  &error);
	if (value == NULL) {
		_SCErrorSet(errno_to_scerror(error));
	}
 done:
	return (value);
}

#ifdef TEST_SCNETWORK_CATEGORY_MANAGER

static CFStringRef
SCNetworkCategoryManagerGetCategory(SCNetworkCategoryManagerRef manager)
{
	return (manager->category);
}

static CFStringRef
SCNetworkCategoryManagerGetInterfaceName(SCNetworkCategoryManagerRef manager)
{
	return (manager->ifname);
}

static CFStringRef
SCNetworkCategoryManagerCopyActiveValue(SCNetworkCategoryManagerRef manager)
{
	dispatch_block_t	b;
	__block errno_t		error;
	__block CFStringRef	value;

	b = ^{
		xpc_connection_t	connection = manager->connection;

		value = CategoryManagerConnectionCopyActiveValue(connection,
								 &error);
	};
	dispatch_sync(manager->queue, b);
	_SCErrorSet(errno_to_scerror(error));
	return (value);
}

static Boolean G_get_value;

static void
get_value(CFStringRef category, CFStringRef ifname)
{
	SCNetworkInterfaceRef	netif = NULL;
	CFStringRef		value;

	if (ifname != NULL) {
		netif = _SCNetworkInterfaceCreateWithBSDName(NULL, ifname,
					     kIncludeAllVirtualInterfaces);
		if (netif == NULL) {
			fprintf(stderr, "Can't create SCNetworkInterface, %s\n",
				SCErrorString(SCError()));
			goto done;
		}
	}
	value = __SCNetworkCategoryManagerCopyActiveValueNoSession(category,
								   netif);
	SCPrint(TRUE, stdout, CFSTR("Value = %@\n"), value);
 done:
	__SC_CFRELEASE(netif);
}

static void
change_value(dispatch_queue_t queue, SCNetworkCategoryManagerRef manager,
	     CFStringRef value)
{
	CFStringRef	current_value;
	CFStringRef	new_value;
	dispatch_time_t	when;
	int		status;

	current_value = SCNetworkCategoryManagerCopyActiveValue(manager);
	if (current_value == NULL) {
		status = SCError();
		if (status != kSCStatusOK) {
			fprintf(stderr, "copy active value failed: %s (%d)\n",
				SCErrorString(status), status);
		}
		new_value = value;
	}
	else {
		new_value = NULL;
	}
	SC_log(LOG_NOTICE, "Current value is %@, activating %@",
	       current_value, new_value);
	__SC_CFRELEASE(current_value);
	SCPrint(TRUE, stdout, CFSTR("Activating: %@\n"), new_value);
	if (!SCNetworkCategoryManagerActivateValue(manager, new_value)) {
		status = SCError();
		fprintf(stderr, "activate failed: %s (%d)\n",
			SCErrorString(status), status);
	}
	if (G_get_value) {
		get_value(SCNetworkCategoryManagerGetCategory(manager),
			  SCNetworkCategoryManagerGetInterfaceName(manager));
	}

#define _ACTIVATE_DELAY	(1000 * 1000 * 1000)
	when = dispatch_walltime(NULL, _ACTIVATE_DELAY);
	dispatch_after(when, queue, ^{
			change_value(queue, manager, value);
		});
}

static Boolean do_cycle;

static Boolean
register_manager(CFStringRef category, CFStringRef ifname,
		 SCNetworkCategoryManagerFlags flags, CFStringRef value)
{
	SCNetworkCategoryManagerNotify	handler;
	SCNetworkCategoryManagerRef	manager;
	SCNetworkInterfaceRef		netif = NULL;
	dispatch_queue_t		queue;
	Boolean				success = FALSE;
	dispatch_time_t			when;

	if (ifname != NULL) {
		netif = _SCNetworkInterfaceCreateWithBSDName(NULL, ifname,
					     kIncludeAllVirtualInterfaces);
		if (netif == NULL) {
			fprintf(stderr, "Can't create SCNetworkInterface, %s\n",
				SCErrorString(SCError()));
			goto done;
		}
	}
	manager =
		SCNetworkCategoryManagerCreateWithInterface(category,
							    netif,
							    flags,
							    NULL);
	__SC_CFRELEASE(netif);
	if (manager == NULL) {
		fprintf(stderr,
			"Can't create SCNetworkCategoryManager, %s\n",
			SCErrorString(SCError()));
		goto done;
	}
	handler = ^(CFStringRef value) {
		SCPrint(TRUE, stdout, CFSTR("Confirmed %@\n"), value);
	};
	queue = dispatch_queue_create("test-SCNetworkCategoryManager", NULL);
	SCNetworkCategoryManagerSetNotifyHandler(manager,
						 queue,
						 handler);
	if (!SCNetworkCategoryManagerActivateValue(manager, value)) {
		fprintf(stderr, "Failed to activate value, %s\n",
			SCErrorString(SCError()));
		goto done;
	}
	if (do_cycle) {
		when = dispatch_walltime(NULL, _ACTIVATE_DELAY);
		dispatch_after(when, queue, ^{
				change_value(queue, manager, value);
			});
	}
	success = TRUE;
 done:
 	if (!success) {
		if (queue != NULL) {
			dispatch_release(queue);
		}
		__SC_CFRELEASE(manager);
	}
	return (success);
}

static inline CFStringRef
cfstring_create_with_cstring(const char * str)
{
	if (str == NULL) {
		return (NULL);
	}
	return (CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8));
}

__private_extern__ os_log_t
__log_SCNetworkConfiguration(void)
{
	static os_log_t	log	= NULL;

	if (log == NULL) {
		log = os_log_create("com.apple.SystemConfiguration", "SCNetworkConfiguration");
	}

	return log;
}

#include <getopt.h>

static void
usage(const char * progname)
{
	fprintf(stderr,
		"usage: %s  -c <category> [ -i <ifname>] "
		"[ [ -C ] -v <value> ] [ -f <flags> ]\n", progname);
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
	CFStringRef			value = NULL;

	while ((ch = getopt(argc, argv, "Cc:F:gi:v:")) != -1) {
		switch (ch) {
		case 'C':
			do_cycle = TRUE;
			break;
		case 'c':
			category = cfstring_create_with_cstring(optarg);
			break;
		case 'F':
			flags = strtoul(optarg, NULL, 0);
			break;
		case 'g':
			G_get_value = TRUE;
			break;
		case 'i':
			ifname = cfstring_create_with_cstring(optarg);
			break;
		case 'v':
			value = cfstring_create_with_cstring(optarg);
			break;
		default:
			usage(progname);
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
	if (!register_manager(category, ifname, flags, value)) {
		exit(1);
	}
	dispatch_main();
}

#endif /* TEST_SCNETWORK_CATEGORY_MANAGER */
