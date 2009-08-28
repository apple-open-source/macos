/*
 * Copyright (c) 2003-2009 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * April 14, 2004		Christophe Allie <callie@apple.com>
 * - use mach messages

 * December 20, 2002		Christophe Allie <callie@apple.com>
 * - initial revision
 */


#include <Availability.h>
#include <TargetConditionals.h>
#include <sys/cdefs.h>
#if	!TARGET_OS_IPHONE
#include <dispatch/dispatch.h>
#endif	// !TARGET_OS_IPHONE
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>

#if	!TARGET_OS_IPHONE
#include <Security/Security.h>
#include "dy_framework.h"
#endif	// !TARGET_OS_IPHONE

#include <servers/bootstrap.h>

#include <pthread.h>
#include <notify.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <mach/mach.h>

#include <ppp/ppp_msg.h>
#include "pppcontroller.h"
#include <ppp/pppcontroller_types.h>



static int		debug			= 0;
static pthread_once_t	initialized		= PTHREAD_ONCE_INIT;


typedef struct {

	/* base CFType information */
	CFRuntimeBase			cfBase;

	/* lock */
	pthread_mutex_t			lock;

	/* service */
	SCNetworkServiceRef		service;

	/* ref to PPP controller for control messages */
	mach_port_t			session_port;

	/* ref to PPP controller for notification messages */
	CFMachPortRef			notify_port;

	/* run loop source, callout, context, rl scheduling info */
	Boolean				scheduled;
	CFRunLoopSourceRef		rls;
	SCNetworkConnectionCallBack	rlsFunction;
	SCNetworkConnectionContext	rlsContext;
	CFMutableArrayRef		rlList;

#if	!TARGET_OS_IPHONE
	dispatch_queue_t		dispatchQueue;		// SCNetworkConnectionSetDispatchQueue
	dispatch_queue_t		callbackQueue;
	dispatch_source_t		callbackSource;
#endif	// !TARGET_OS_IPHONE

} SCNetworkConnectionPrivate, *SCNetworkConnectionPrivateRef;


static __inline__ CFTypeRef
isA_SCNetworkConnection(CFTypeRef obj)
{
	return (isA_CFType(obj, SCNetworkConnectionGetTypeID()));
}


static CFStringRef
__SCNetworkConnectionCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef			allocator		= CFGetAllocator(cf);
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)cf;
	CFMutableStringRef		result;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCNetworkConnection, %p [%p]> {"), cf, allocator);
	CFStringAppendFormat(result, NULL, CFSTR("service = %p"), connectionPrivate->service);
	if (connectionPrivate->session_port != MACH_PORT_NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", server port = %p"), connectionPrivate->session_port);
	}
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}


static void
__SCNetworkConnectionDeallocate(CFTypeRef cf)
{
	SCNetworkConnectionPrivateRef	connectionPrivate = (SCNetworkConnectionPrivateRef)cf;

	/* release resources */
	pthread_mutex_destroy(&connectionPrivate->lock);

	if (connectionPrivate->rls != NULL) {
		CFRunLoopSourceInvalidate(connectionPrivate->rls);
		CFRelease(connectionPrivate->rls);
	}

	if (connectionPrivate->rlList != NULL) {
		CFRelease(connectionPrivate->rlList);
	}

	if (connectionPrivate->notify_port != NULL) {
		mach_port_t	mp	= CFMachPortGetPort(connectionPrivate->notify_port);

		__MACH_PORT_DEBUG(TRUE, "*** __SCNetworkConnectionDeallocate notify_port", mp);
		CFMachPortInvalidate(connectionPrivate->notify_port);
		CFRelease(connectionPrivate->notify_port);
		mach_port_mod_refs(mach_task_self(), mp, MACH_PORT_RIGHT_RECEIVE, -1);
	}

	if (connectionPrivate->session_port != MACH_PORT_NULL) {
		__MACH_PORT_DEBUG(TRUE, "*** __SCNetworkConnectionDeallocate session_port", connectionPrivate->session_port);
		(void) mach_port_deallocate(mach_task_self(), connectionPrivate->session_port);
	}

	if (connectionPrivate->rlsContext.release != NULL)
		(*connectionPrivate->rlsContext.release)(connectionPrivate->rlsContext.info);

	CFRelease(connectionPrivate->service);

	return;
}


static CFTypeID __kSCNetworkConnectionTypeID	= _kCFRuntimeNotATypeID;

static const CFRuntimeClass __SCNetworkConnectionClass = {
	0,					// version
	"SCNetworkConnection",			// className
	NULL,					// init
	NULL,					// copy
	__SCNetworkConnectionDeallocate,	// dealloc
	NULL,					// equal
	NULL,					// hash
	NULL,					// copyFormattingDesc
	__SCNetworkConnectionCopyDescription	// copyDebugDesc
};


static void
__SCNetworkConnectionInitialize(void)
{
	char	*env;

	/* get the debug environment variable */
	env = getenv("PPPDebug");
	if (env != NULL) {
		if (sscanf(env, "%d", &debug) != 1) {
			/* PPPDebug value is not valid (or non-numeric), set debug to 1 */
			debug = 1;
		}
	}

	__kSCNetworkConnectionTypeID = _CFRuntimeRegisterClass(&__SCNetworkConnectionClass);
	return;
}


static void
__SCNetworkConnectionCallBack(CFMachPortRef port, void * msg, CFIndex size, void * info)
{
	mach_msg_empty_rcv_t *		buf			= msg;
	SCNetworkConnectionRef		connection		= (SCNetworkConnectionRef)info;
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	void				*context_info;
	void				(*context_release)(const void *);
	int				error			= kSCStatusFailed;
	mach_msg_id_t			msgid			= buf->header.msgh_id;
	SCNetworkConnectionCallBack	rlsFunction;
	SCNetworkConnectionStatus	scstatus = kSCNetworkConnectionInvalid;

	if (msgid == MACH_NOTIFY_NO_SENDERS) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("__SCNetworkConnectionCallBack: PPPController server died"));
	} else {
		(void) pppcontroller_getstatus(connectionPrivate->session_port, &scstatus, &error);
	}

	if (!connectionPrivate->scheduled) {
		// if not currently scheduled
		return;
	}

	rlsFunction = connectionPrivate->rlsFunction;
	if (rlsFunction == NULL) {
		return;
	}

	if ((connectionPrivate->rlsContext.retain != NULL) && (connectionPrivate->rlsContext.info != NULL)) {
		context_info	= (void *)(*connectionPrivate->rlsContext.retain)(connectionPrivate->rlsContext.info);
		context_release	= connectionPrivate->rlsContext.release;
	} else {
		context_info	= connectionPrivate->rlsContext.info;
		context_release	= NULL;
	}

	(*rlsFunction)(connection, scstatus, context_info);
	if ((context_release != NULL) && (context_info != NULL)) {
		(*context_release)(context_info);
	}

	return;
}


#pragma mark -
#pragma mark SCNetworkConnection APIs


static CFStringRef
pppMPCopyDescription(const void *info)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)info;

	return CFStringCreateWithFormat(NULL,
					NULL,
					CFSTR("<SCNetworkConnection MP %p> {service = %@, callout = %p}"),
					connectionPrivate,
					connectionPrivate->service,
					connectionPrivate->rlsFunction);
}


static SCNetworkConnectionPrivateRef
__SCNetworkConnectionCreatePrivate(CFAllocatorRef		allocator,
				   SCNetworkServiceRef		service,
				   SCNetworkConnectionCallBack	callout,
				   SCNetworkConnectionContext	*context)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= NULL;
	uint32_t			size;


	/* initialize runtime */
	pthread_once(&initialized, __SCNetworkConnectionInitialize);

	/* allocate NetworkConnection */
	size = sizeof(SCNetworkConnectionPrivate) - sizeof(CFRuntimeBase);
	connectionPrivate = (SCNetworkConnectionPrivateRef)_CFRuntimeCreateInstance(allocator, __kSCNetworkConnectionTypeID, size, NULL);
	if (connectionPrivate == NULL) {
		goto fail;
	}

	/* zero the data structure */
	bzero(((u_char*)connectionPrivate)+sizeof(CFRuntimeBase), size);

	pthread_mutex_init(&connectionPrivate->lock, NULL);

	/* save the service */
	connectionPrivate->service = CFRetain(service);

	connectionPrivate->rlsFunction = callout;

	if (context) {
		bcopy(context, &connectionPrivate->rlsContext, sizeof(SCNetworkConnectionContext));
		if (context->retain != NULL) {
			connectionPrivate->rlsContext.info = (void *)(*context->retain)(context->info);
		}
	}

	/* success, return the connection reference */
	return connectionPrivate;

    fail:

	/* failure, clean up and leave */
	if (connectionPrivate != NULL) {
		CFRelease(connectionPrivate);
	}

	_SCErrorSet(kSCStatusFailed);
	return NULL;
}


static mach_port_t
__SCNetworkConnectionSessionPort(SCNetworkConnectionPrivateRef connectionPrivate)
{
	void		*data;
	CFIndex		dataLen;
	CFDataRef	dataRef			= NULL;
	int		error			= kSCStatusFailed;
	mach_port_t	notify_port		= MACH_PORT_NULL;
	mach_port_t	oldNotify		= MACH_PORT_NULL;
	mach_port_t	server			= MACH_PORT_NULL;
	kern_return_t	status;
	mach_port_t	unpriv_bootstrap_port	= MACH_PORT_NULL;

	if (connectionPrivate->session_port != MACH_PORT_NULL) {
		return connectionPrivate->session_port;
	}

	pthread_mutex_lock(&connectionPrivate->lock);

	if (bootstrap_look_up(bootstrap_port, PPPCONTROLLER_SERVER, &server) != BOOTSTRAP_SUCCESS) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPP Controller not found"));
		goto done;
	}

	if (!_SCSerializeString(SCNetworkServiceGetServiceID(connectionPrivate->service), &dataRef, &data, &dataLen)) {
		goto done;
	}

	status = bootstrap_unprivileged(bootstrap_port, &unpriv_bootstrap_port);
	if (status != BOOTSTRAP_SUCCESS) {
		goto done;
	}

	if (connectionPrivate->rlsFunction != NULL) {
		/* allocate port (for server response) */
		status = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &notify_port);
		if (status != KERN_SUCCESS) {
			goto done;
		}

		status = mach_port_insert_right(mach_task_self(),
						notify_port,
						notify_port,
						MACH_MSG_TYPE_MAKE_SEND);
		if (status != KERN_SUCCESS) {
			/*
			 * We can't insert a send right into our own port!  This should
			 * only happen if someone stomped on OUR port (so let's leave
			 * the port alone).
			 */
			SCLog(TRUE, LOG_ERR, CFSTR("__SCNetworkConnectionSessionPort mach_port_insert_right(): %s"), mach_error_string(status));
			goto done;
		}

		/* request a notification when/if the server dies */
		status = mach_port_request_notification(mach_task_self(),
							notify_port,
							MACH_NOTIFY_NO_SENDERS,
							1,
							notify_port,
							MACH_MSG_TYPE_MAKE_SEND_ONCE,
							&oldNotify);
		if (status != KERN_SUCCESS) {
			goto done;
		}

		if (oldNotify != MACH_PORT_NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("__SCNetworkConnectionSessionPort(): oldNotify != MACH_PORT_NULL"));
		}

		__MACH_PORT_DEBUG(TRUE, "*** __SCNetworkConnectionSessionPort notify_port (before)", notify_port);
	}

	status = pppcontroller_attach(server, data, dataLen, unpriv_bootstrap_port, notify_port,
				      &connectionPrivate->session_port, &error);
	if (status != KERN_SUCCESS) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("__SCNetworkConnectionSessionPort : pppcontroller_attach failed (status=0x%x)"), status);
		if ((notify_port != MACH_PORT_NULL) && (status == MACH_SEND_INVALID_DEST)) {
			(void) mach_port_destroy(mach_task_self(), notify_port);
			notify_port = MACH_PORT_NULL;
		}
		error = kSCStatusFailed;
		goto done;
	}

	__MACH_PORT_DEBUG(connectionPrivate->session_port != MACH_PORT_NULL,
			  "*** __SCNetworkConnectionSessionPort session_port",
			  connectionPrivate->session_port);
	__MACH_PORT_DEBUG(notify_port != MACH_PORT_NULL,
			  "*** __SCNetworkConnectionSessionPort notify_port",
			  notify_port);

	if (notify_port != MACH_PORT_NULL) {
		CFMachPortContext	context	= { 0
						  , (void *)connectionPrivate
						  , NULL
						  , NULL
						  , pppMPCopyDescription
		};

		connectionPrivate->notify_port = CFMachPortCreateWithPort(NULL, notify_port, __SCNetworkConnectionCallBack, &context, NULL);
	}

    done :

	if (dataRef != NULL)	CFRelease(dataRef);

	if (unpriv_bootstrap_port != MACH_PORT_NULL) {
		mach_port_deallocate(mach_task_self(), unpriv_bootstrap_port);
	}

	if (error != kSCStatusOK) {
		if (connectionPrivate->session_port != MACH_PORT_NULL) {
			__MACH_PORT_DEBUG(TRUE,
					  "*** __SCNetworkConnectionSessionPort attach failed: session_port",
					  connectionPrivate->session_port);
			mach_port_deallocate(mach_task_self(), connectionPrivate->session_port);
			connectionPrivate->session_port = MACH_PORT_NULL;
		}
		if (connectionPrivate->notify_port != NULL) {
			mach_port_t	port = CFMachPortGetPort(connectionPrivate->notify_port);

			CFMachPortInvalidate(connectionPrivate->notify_port);
			CFRelease(connectionPrivate->notify_port);
			connectionPrivate->notify_port = NULL;
			__MACH_PORT_DEBUG(TRUE, "*** __SCNetworkConnectionSessionPort attach failed: notify_port", port);
			mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
		}
		_SCErrorSet(error);
	}

	pthread_mutex_unlock(&connectionPrivate->lock);

	return connectionPrivate->session_port;
}


CFTypeID
SCNetworkConnectionGetTypeID(void) {
	pthread_once(&initialized, __SCNetworkConnectionInitialize);	/* initialize runtime */
	return __kSCNetworkConnectionTypeID;
}


CFArrayRef /* of SCNetworkServiceRef's */
SCNetworkConnectionCopyAvailableServices(SCNetworkSetRef set)
{
	CFMutableArrayRef	available;
	Boolean			tempSet	= FALSE;

	if (set == NULL) {
		SCPreferencesRef	prefs;

		prefs = SCPreferencesCreate(NULL, CFSTR("SCNetworkConnectionCopyAvailableServices"), NULL);
		set   = SCNetworkSetCopyCurrent(prefs);
		CFRelease(prefs);
		tempSet = TRUE;
	}

	available = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	if (set != NULL) {
		CFArrayRef	services;

		services = SCNetworkSetCopyServices(set);
		if (services != NULL) {
			CFIndex		i;
			CFIndex		n;

			n = CFArrayGetCount(services);
			for (i = 0; i < n; i++) {
				SCNetworkInterfaceRef	interface;
				CFStringRef		interfaceType;
				SCNetworkServiceRef	service;

				service       = CFArrayGetValueAtIndex(services, i);
				interface     = SCNetworkServiceGetInterface(service);
				interfaceType = SCNetworkInterfaceGetInterfaceType(interface);
				if (CFEqual(interfaceType, kSCNetworkInterfaceTypePPP) ||
				    CFEqual(interfaceType, kSCNetworkInterfaceTypeIPSec)) {
					CFArrayAppendValue(available, service);
				}
			}

			CFRelease(services);
		}
	}

	if (tempSet && (set != NULL)) {
		CFRelease(set);
	}
	return available;
}


SCNetworkConnectionRef
SCNetworkConnectionCreateWithService(CFAllocatorRef			allocator,
				     SCNetworkServiceRef		service,
				     SCNetworkConnectionCallBack	callout,
				     SCNetworkConnectionContext		*context)
{
	SCNetworkConnectionPrivateRef	connectionPrivate;

	if (!isA_SCNetworkService(service)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	connectionPrivate = __SCNetworkConnectionCreatePrivate(allocator, service, callout, context);
	return (SCNetworkConnectionRef)connectionPrivate;
}


SCNetworkConnectionRef
SCNetworkConnectionCreateWithServiceID(CFAllocatorRef			allocator,
				       CFStringRef			serviceID,
				       SCNetworkConnectionCallBack	callout,
				       SCNetworkConnectionContext	*context)
{
	SCNetworkConnectionRef	connection;
	SCNetworkServiceRef	service;

	if (!isA_CFString(serviceID)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	service = _SCNetworkServiceCopyActive(NULL, serviceID);
	if (service == NULL) {
		return NULL;
	}

	connection = SCNetworkConnectionCreateWithService(allocator, service, callout, context);
	CFRelease(service);

	return connection;
}


CFStringRef
SCNetworkConnectionCopyServiceID(SCNetworkConnectionRef connection)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	CFStringRef			serviceID;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	serviceID = SCNetworkServiceGetServiceID(connectionPrivate->service);
	return CFRetain(serviceID);
}


CFDictionaryRef
SCNetworkConnectionCopyStatistics(SCNetworkConnectionRef connection)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	xmlDataOut_t			data			= NULL;
	mach_msg_type_number_t		datalen;
	int				error			= kSCStatusFailed;
	mach_port_t			session_port;
	CFPropertyListRef		statistics		= NULL;
	kern_return_t			status;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	session_port = __SCNetworkConnectionSessionPort(connectionPrivate);
	if (session_port == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	status = pppcontroller_copystatistics(session_port, &data, &datalen, &error);
	if (status != KERN_SUCCESS) {
		goto fail;
	}

	if (error != kSCStatusOK) {
		goto fail;
	}

	if ((data == NULL) ||
	    !_SCUnserialize(&statistics, NULL, data, datalen) ||
	    !isA_CFDictionary(statistics)) {
		goto fail;
	}

	return statistics;

    fail:

	if (statistics)	CFRelease(statistics);
	_SCErrorSet(error);
	return NULL;
}


SCNetworkServiceRef
SCNetworkConnectionGetService(SCNetworkConnectionRef connection)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	return connectionPrivate->service;
}


SCNetworkConnectionStatus
SCNetworkConnectionGetStatus(SCNetworkConnectionRef connection)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	int				error			= kSCStatusFailed;
	SCNetworkConnectionStatus	scstatus;
	mach_port_t			session_port;
	kern_return_t			status;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return kSCNetworkConnectionInvalid;
	}

	session_port = __SCNetworkConnectionSessionPort(connectionPrivate);
	if (session_port == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return kSCNetworkConnectionInvalid;
	}

	status = pppcontroller_getstatus(session_port, &scstatus, &error);
	if (status != KERN_SUCCESS) {
		_SCErrorSet(kSCStatusFailed);
		return kSCNetworkConnectionInvalid;
	}

	if (error != kSCStatusOK) {
		_SCErrorSet(error);
		return kSCNetworkConnectionInvalid;
	}

	return scstatus;
}


CFDictionaryRef
SCNetworkConnectionCopyExtendedStatus(SCNetworkConnectionRef connection)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	xmlDataOut_t			data			= NULL;
	mach_msg_type_number_t		datalen;
	int				error			= kSCStatusFailed;
	CFPropertyListRef		extstatus		= NULL;
	mach_port_t			session_port;
	kern_return_t			status;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	session_port = __SCNetworkConnectionSessionPort(connectionPrivate);
	if (session_port == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	status = pppcontroller_copyextendedstatus(session_port, &data, &datalen, &error);
	if (status != KERN_SUCCESS) {
		goto fail;
	}

	if (error != kSCStatusOK) {
		goto fail;
	}

	if ((data == NULL) ||
	    !_SCUnserialize(&extstatus, NULL, data, datalen) ||
	    !isA_CFDictionary(extstatus)) {
		goto fail;
	}

	return extstatus;

    fail:

	if (extstatus)	CFRelease(extstatus);
	_SCErrorSet(error);
	return NULL;
}


Boolean
SCNetworkConnectionStart(SCNetworkConnectionRef	connection,
			 CFDictionaryRef	userOptions,
			 Boolean		linger)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	CFDataRef			dataref			= NULL;
	void				*data			= NULL;
	CFIndex				datalen			= 0;
	int				error			= kSCStatusFailed;
	mach_port_t			session_port;
	kern_return_t			status;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if ((userOptions != NULL) && !isA_CFDictionary(userOptions)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	session_port = __SCNetworkConnectionSessionPort(connectionPrivate);
	if (session_port == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (debug > 0) {
		CFMutableDictionaryRef	mdict = NULL;

		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionStart (0x%x)"), connectionPrivate);

		if (userOptions != NULL) {
			CFDictionaryRef		dict;
			CFStringRef		encryption;
			CFMutableDictionaryRef	new_dict;

			/* special code to remove secret information */
			mdict = CFDictionaryCreateMutableCopy(NULL, 0, userOptions);

			dict = CFDictionaryGetValue(mdict, kSCEntNetPPP);
			if (isA_CFDictionary(dict)) {
				encryption = CFDictionaryGetValue(dict, kSCPropNetPPPAuthPasswordEncryption);
				if (!isA_CFString(encryption) ||
				    !CFEqual(encryption, kSCValNetPPPAuthPasswordEncryptionKeychain)) {
					new_dict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
					CFDictionaryReplaceValue(new_dict, kSCPropNetPPPAuthPassword, CFSTR("******"));
					CFDictionarySetValue(mdict, kSCEntNetPPP, new_dict);
					CFRelease(new_dict);
				}
			}

			dict = CFDictionaryGetValue(mdict, kSCEntNetL2TP);
			if (isA_CFDictionary(dict)) {
				encryption = CFDictionaryGetValue(dict, kSCPropNetL2TPIPSecSharedSecretEncryption);
				if (!isA_CFString(encryption) ||
				    !CFEqual(encryption, kSCValNetL2TPIPSecSharedSecretEncryptionKeychain)) {
					new_dict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
					CFDictionaryReplaceValue(new_dict, kSCPropNetL2TPIPSecSharedSecret, CFSTR("******"));
					CFDictionarySetValue(mdict, kSCEntNetL2TP, new_dict);
					CFRelease(new_dict);
				}
			}

			dict = CFDictionaryGetValue(mdict, kSCEntNetIPSec);
			if (isA_CFDictionary(dict)) {
				encryption = CFDictionaryGetValue(dict, kSCPropNetIPSecSharedSecretEncryption);
				if (!isA_CFString(encryption) ||
				    !CFEqual(encryption, kSCValNetIPSecSharedSecretEncryptionKeychain)) {
					new_dict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
					CFDictionaryReplaceValue(new_dict, kSCPropNetIPSecSharedSecret, CFSTR("******"));
					CFDictionarySetValue(mdict, kSCEntNetIPSec, new_dict);
					CFRelease(new_dict);
				}
			}
		}

		SCLog(TRUE, LOG_DEBUG, CFSTR("User options: %@"), mdict);
		if (mdict != NULL) CFRelease(mdict);
	}

	if (userOptions && !_SCSerialize(userOptions, &dataref, &data, &datalen)) {
		goto fail;
	}

	status = pppcontroller_start(session_port, data, datalen, linger, &error);
	if (status != KERN_SUCCESS) {
		goto fail;
	}

	if (dataref) {
		CFRelease(dataref);
		dataref = NULL;
	}

	if (debug > 0) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionStart (0x%x), return: %d"), connectionPrivate, error);
	}

	if (error != kSCStatusOK) {
		goto fail;
	}

	/* connection is now started */
	return TRUE;

    fail:

	if (dataref)	CFRelease(dataref);
	_SCErrorSet(error);
	return FALSE;
}


Boolean
SCNetworkConnectionStop(SCNetworkConnectionRef	connection,
			Boolean			forceDisconnect)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	int				error			= kSCStatusFailed;
	mach_port_t			session_port;
	kern_return_t			status;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	session_port = __SCNetworkConnectionSessionPort(connectionPrivate);
	if (session_port == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (debug > 0) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionStop (0x%x)"), connectionPrivate);
	}

	status = pppcontroller_stop(session_port, forceDisconnect, &error);
	if (status != KERN_SUCCESS) {
		goto fail;
	}

	if (debug > 0) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionStop (0x%x), return: %d"), connectionPrivate, error);
	}

	if (error != kSCStatusOK) {
		goto fail;
	}

	/* connection is now disconnecting */
	return TRUE;

    fail:

	_SCErrorSet(error);
	return FALSE;
}


Boolean
SCNetworkConnectionSuspend(SCNetworkConnectionRef connection)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	int				error			= kSCStatusFailed;
	mach_port_t			session_port;
	kern_return_t			status;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	session_port = __SCNetworkConnectionSessionPort(connectionPrivate);
	if (session_port == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (debug > 0) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionSuspend (0x%x)"), connectionPrivate);
	}

	status = pppcontroller_suspend(session_port, &error);
	if (status != KERN_SUCCESS) {
		goto fail;
	}

	if (debug > 0) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionSuspend (0x%x), return: %d"), connectionPrivate, error);
	}

	if (error != kSCStatusOK) {
		goto fail;
	}

	/* connection is now suspended */
	return TRUE;

    fail:

	_SCErrorSet(error);
	return FALSE;
}


Boolean
SCNetworkConnectionResume(SCNetworkConnectionRef connection)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	int				error			= kSCStatusFailed;
	mach_port_t			session_port;
	kern_return_t			status;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	session_port = __SCNetworkConnectionSessionPort(connectionPrivate);
	if (session_port == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (debug > 0) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionResume (0x%x)"), connectionPrivate);
	}

	status = pppcontroller_resume(session_port, &error);
	if (status != KERN_SUCCESS) {
		goto fail;
	}

	if (debug > 0) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionResume (0x%x), return: %d"), connectionPrivate, error);
	}

	if (error != kSCStatusOK) {
		goto fail;
	}

	/* connection is now resume */
	return TRUE;

    fail:

	_SCErrorSet(error);
	return FALSE;
}


CFDictionaryRef
SCNetworkConnectionCopyUserOptions(SCNetworkConnectionRef connection)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	xmlDataOut_t			data			= NULL;
	mach_msg_type_number_t		datalen;
	int				error			= kSCStatusFailed;
	mach_port_t			session_port;
	kern_return_t			status;
	CFPropertyListRef 		userOptions		= NULL;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	session_port = __SCNetworkConnectionSessionPort(connectionPrivate);
	if (session_port == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	status = pppcontroller_copyuseroptions(session_port, &data, &datalen, &error);
	if (status != KERN_SUCCESS) {
		goto fail;
	}

	if (error != kSCStatusOK) {
		goto fail;
	}

	// no data were used, return an empty dictionary
	if (data == NULL) {
		CFDictionaryRef dict;

		dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (dict == NULL) {
			_SCErrorSet(kSCStatusFailed); // XXX
		}
		return dict;
	}

	if (!_SCUnserialize(&userOptions, NULL, data, datalen) ||
	    !isA_CFDictionary(userOptions)) {
		goto fail;
	}

	return userOptions;

    fail:

	if (userOptions)	CFRelease(userOptions);
	_SCErrorSet(error);
	return NULL;
}


#if	!TARGET_OS_IPHONE
static boolean_t
SCNetworkConnectionNotifyMIGCallback(mach_msg_header_t *message, mach_msg_header_t *reply)
{
	SCNetworkConnectionPrivateRef	connectionPrivate = dispatch_get_context(dispatch_get_current_queue());

	if (connectionPrivate != NULL) {
		CFRetain(connectionPrivate);
		dispatch_async(connectionPrivate->dispatchQueue, ^{
			__SCNetworkConnectionCallBack(connectionPrivate->notify_port, message, 4096, connectionPrivate);
			CFRelease(connectionPrivate);
		});
	}
	reply->msgh_remote_port = MACH_PORT_NULL;
	return false;
}
#endif	// !TARGET_OS_IPHONE


static Boolean
__SCNetworkConnectionScheduleWithRunLoop(SCNetworkConnectionRef	connection,
					 CFRunLoopRef		runLoop,
					 CFStringRef		runLoopMode,
#if	!TARGET_OS_IPHONE
					 dispatch_queue_t	queue
#else	// !TARGET_OS_IPHONE
					 void			*queue
#endif	// !TARGET_OS_IPHONE
					 )
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	int				error			= kSCStatusFailed;
	mach_port_t			session_port;
	kern_return_t			status;

	if (connectionPrivate->rlsFunction == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

#if	!TARGET_OS_IPHONE
	if ((connectionPrivate->dispatchQueue != NULL) ||		// if we are already scheduled on a dispatch queue
	    ((queue != NULL) && connectionPrivate->scheduled)) {	// if we are already scheduled on a CFRunLoop
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}
#endif	// !TARGET_OS_IPHONE

	session_port = __SCNetworkConnectionSessionPort(connectionPrivate);
	if (session_port == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!connectionPrivate->scheduled) {
		status = pppcontroller_notification(session_port, 1, &error);
		if ((status != KERN_SUCCESS) || (error != kSCStatusOK)) {
			_SCErrorSet(error);
			return FALSE;
		}

		if (runLoop != NULL) {
			connectionPrivate->rls = CFMachPortCreateRunLoopSource(NULL, connectionPrivate->notify_port, 0);
			connectionPrivate->rlList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		}

		connectionPrivate->scheduled = TRUE;
	}

#if	!TARGET_OS_IPHONE
	if (queue != NULL) {
		dispatch_queue_attr_t	attr;
		mach_port_t		mp;
		long			res;

		connectionPrivate->dispatchQueue = queue;
		dispatch_retain(connectionPrivate->dispatchQueue);

		attr = dispatch_queue_attr_create();
		res = dispatch_queue_attr_set_finalizer(attr,
							^(dispatch_queue_t dq) {
								SCNetworkConnectionRef	connection;

								connection = (SCNetworkConnectionRef)dispatch_get_context(dq);
								CFRelease(connection);
							});
		if (res != 0) {
			SCLog(TRUE, LOG_ERR, CFSTR("SCNetworkConnection dispatch_queue_attr_set_finalizer() failed"));
			dispatch_release(attr);
			goto fail;
		}
		connectionPrivate->callbackQueue = dispatch_queue_create("com.apple.SCNetworkConnection.notifications", attr);
		dispatch_release(attr);
		if (connectionPrivate->callbackQueue == NULL){
			SCLog(TRUE, LOG_ERR, CFSTR("SCNetworkConnection dispatch_queue_create() failed"));
			goto fail;
		}
		CFRetain(connection);	// Note: will be released when the dispatch queue is released
		dispatch_set_context(connectionPrivate->callbackQueue, connectionPrivate);

		mp = CFMachPortGetPort(connectionPrivate->notify_port);
		connectionPrivate->callbackSource = dispatch_source_mig_create(mp, sizeof(mach_msg_header_t), NULL, connectionPrivate->callbackQueue, SCNetworkConnectionNotifyMIGCallback);
		if (connectionPrivate->callbackSource == NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("SCNetworkConnection dispatch_source_mig_create() failed"));
			goto fail;
		}
	} else
#endif	// !TARGET_OS_IPHONE
	{
		if (!_SC_isScheduled(NULL, runLoop, runLoopMode, connectionPrivate->rlList)) {
			/*
			 * if we do not already have notifications scheduled with
			 * this runLoop / runLoopMode
			 */
			CFRunLoopAddSource(runLoop, connectionPrivate->rls, runLoopMode);
		}

		_SC_schedule(connection, runLoop, runLoopMode, connectionPrivate->rlList);
	}

	return TRUE;

#if	!TARGET_OS_IPHONE
    fail :

	if (connectionPrivate->callbackSource != NULL) {
		dispatch_cancel(connectionPrivate->callbackSource);
		dispatch_release(connectionPrivate->callbackSource);
		connectionPrivate->callbackSource = NULL;
	}
	if (connectionPrivate->callbackQueue != NULL) {
		dispatch_release(connectionPrivate->callbackQueue);
		connectionPrivate->callbackQueue = NULL;
	}
	if (connectionPrivate->dispatchQueue != NULL) {
		dispatch_release(connectionPrivate->dispatchQueue);
		connectionPrivate->dispatchQueue = NULL;
	}
	_SCErrorSet(kSCStatusFailed);
	return FALSE;
#endif	// !TARGET_OS_IPHONE
}


static Boolean
__SCNetworkConnectionUnscheduleFromRunLoop(SCNetworkConnectionRef	connection,
					   CFRunLoopRef			runLoop,
					   CFStringRef			runLoopMode,
#if	!TARGET_OS_IPHONE
					   dispatch_queue_t		queue
#else	// !TARGET_OS_IPHONE
					   void				*queue
#endif	// !TARGET_OS_IPHONE
					   )
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	int				error			= kSCStatusFailed;
	CFIndex				n			= 0;
	mach_port_t			session_port;
	kern_return_t			status;

	if ((runLoop != NULL) && !connectionPrivate->scheduled) {			// if we should be scheduled (but are not)
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

#if	!TARGET_OS_IPHONE
	if (((runLoop == NULL) && (connectionPrivate->dispatchQueue == NULL)) ||	// if we should be scheduled on a dispatch queue (but are not)
	    ((runLoop != NULL) && (connectionPrivate->dispatchQueue != NULL))) {	// if we should be scheduled on a CFRunLoop (but are scheduled on a dispatch queue)
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}
#endif	// !TARGET_OS_IPHONE

	session_port = __SCNetworkConnectionSessionPort(connectionPrivate);
	if (session_port == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

#if	!TARGET_OS_IPHONE
	if (runLoop == NULL) {
		dispatch_cancel(connectionPrivate->callbackSource);
		dispatch_release(connectionPrivate->callbackSource);
		connectionPrivate->callbackSource = NULL;
		dispatch_release(connectionPrivate->callbackQueue);
		connectionPrivate->callbackQueue = NULL;
		dispatch_release(connectionPrivate->dispatchQueue);
		connectionPrivate->dispatchQueue = NULL;
	} else
#endif	// !TARGET_OS_IPHONE
	{
		if (!_SC_unschedule(connection, runLoop, runLoopMode, connectionPrivate->rlList, FALSE)) {
			// if not currently scheduled on this runLoop / runLoopMode
			_SCErrorSet(kSCStatusFailed);
			return FALSE;
		}

		n = CFArrayGetCount(connectionPrivate->rlList);
		if (n == 0 || !_SC_isScheduled(NULL, runLoop, runLoopMode, connectionPrivate->rlList)) {
			/*
			 * if we are no longer scheduled to receive notifications for
			 * this runLoop / runLoopMode
			 */
			CFRunLoopRemoveSource(runLoop, connectionPrivate->rls, runLoopMode);

			if (n == 0) {
				// if *all* notifications have been unscheduled
				CFRelease(connectionPrivate->rlList);
				connectionPrivate->rlList = NULL;
				CFRunLoopSourceInvalidate(connectionPrivate->rls);
				CFRelease(connectionPrivate->rls);
				connectionPrivate->rls = NULL;
			}
		}
	}

	if (n == 0) {
		// if *all* notifications have been unscheduled
		connectionPrivate->scheduled = FALSE;

		status = pppcontroller_notification(session_port, 0, &error);
		if ((status != KERN_SUCCESS) || (error != kSCStatusOK)) {
			_SCErrorSet(error);
			return FALSE;
		}
	}

	return TRUE;
}


Boolean
SCNetworkConnectionScheduleWithRunLoop(SCNetworkConnectionRef	connection,
				       CFRunLoopRef		runLoop,
				       CFStringRef		runLoopMode)
{
	if (!isA_SCNetworkConnection(connection) || (runLoop == NULL) || (runLoopMode == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	return __SCNetworkConnectionScheduleWithRunLoop(connection, runLoop, runLoopMode, NULL);
}


Boolean
SCNetworkConnectionUnscheduleFromRunLoop(SCNetworkConnectionRef		connection,
					 CFRunLoopRef			runLoop,
					 CFStringRef			runLoopMode)
{
	if (!isA_SCNetworkConnection(connection) || (runLoop == NULL) || (runLoopMode == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	return __SCNetworkConnectionUnscheduleFromRunLoop(connection, runLoop, runLoopMode, NULL);
}


#if	!TARGET_OS_IPHONE
Boolean
SCNetworkConnectionSetDispatchQueue(SCNetworkConnectionRef	connection,
				    dispatch_queue_t		queue)
{
	Boolean	ok	= FALSE;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (queue != NULL) {
		ok = __SCNetworkConnectionScheduleWithRunLoop(connection, NULL, NULL, queue);
	} else {
		ok = __SCNetworkConnectionUnscheduleFromRunLoop(connection, NULL, NULL, NULL);
	}

	return ok;
}
#endif	// !TARGET_OS_IPHONE


#pragma mark -
#pragma mark User level "dial" API


#define k_NetworkConnect_Notification	"com.apple.networkConnect"
#define k_NetworkConnect_Pref_File	CFSTR("com.apple.networkConnect")
#define k_InterentConnect_Pref_File	CFSTR("com.apple.internetconnect")

#define k_Dial_Default_Key		CFSTR("ConnectByDefault") // needs to go into SC
#define k_Last_Service_Id_Key		CFSTR("ServiceID")
#define k_Unique_Id_Key	 		CFSTR("UniqueIdentifier")


/* Private Prototypes */
static Boolean SCNetworkConnectionPrivateCopyDefaultServiceIDForDial	(SCDynamicStoreRef session, CFStringRef *serviceID);
static Boolean SCNetworkConnectionPrivateGetPPPServiceFromDynamicStore	(SCDynamicStoreRef session, CFStringRef *serviceID);
static Boolean SCNetworkConnectionPrivateCopyDefaultUserOptionsFromArray(CFArrayRef userOptionsArray, CFDictionaryRef *userOptions);
static Boolean SCNetworkConnectionPrivateIsPPPService			(SCDynamicStoreRef session, CFStringRef serviceID, CFStringRef subType1, CFStringRef subType2);
static void addPasswordFromKeychain					(SCDynamicStoreRef session, CFStringRef serviceID, CFDictionaryRef *userOptions);
static CFStringRef copyPasswordFromKeychain				(CFStringRef uniqueID);

static int		notify_userprefs_token	= -1;

static CFDictionaryRef	onDemand_configuration	= NULL;
static pthread_mutex_t	onDemand_notify_lock	= PTHREAD_MUTEX_INITIALIZER;
static int		onDemand_notify_token	= -1;

/*
 *	return TRUE if domain1 ends with domain2, and will check for trailing "."
 */
static Boolean
domainEndsWithDomain(CFStringRef domain1, CFStringRef domain2)
{
	CFRange		range;
	Boolean		ret		= FALSE;
	CFStringRef	s1		= NULL;
	Boolean		s1_created	= FALSE;
	CFStringRef	s2		= NULL;
	Boolean		s2_created	= FALSE;

	if (CFStringHasSuffix(domain1, CFSTR("."))) {
		range.location = 0;
		range.length = CFStringGetLength(domain1) - 1;
		s1 = CFStringCreateWithSubstring(NULL, domain1, range);
		if (s1 == NULL) {
			goto done;
		}
		s1_created = TRUE;
	} else {
		s1 = domain1;
	}

	if (CFStringHasSuffix(domain2, CFSTR("."))) {
		range.location = 0;
		range.length = CFStringGetLength(domain2) - 1;
		s2 = CFStringCreateWithSubstring(NULL, domain2, range);
		if (s2 == NULL) {
			goto done;
		}
		s2_created = TRUE;
	} else {
		s2 = domain2;
	}

	ret = CFStringHasSuffix(s1, s2);

    done :

	if (s1_created)	CFRelease(s1);
	if (s2_created)	CFRelease(s2);
	return ret;
}

/* VPN On Demand */

Boolean
__SCNetworkConnectionCopyOnDemandInfoWithName(SCDynamicStoreRef		*storeP,
					      CFStringRef		hostName,
					      Boolean			onDemandRetry,
					      CFStringRef		*connectionServiceID,
					      SCNetworkConnectionStatus	*connectionStatus,
					      CFStringRef		*vpnRemoteAddress)	/*  CFDictionaryRef *info */
{
	int			changed		= 1;
	CFDictionaryRef		configuration;
	Boolean			ok		= FALSE;
	int			status;
	SCDynamicStoreRef	store		= *storeP;
	CFArrayRef		triggers;
	uint64_t		triggersCount	= 0;
	int			triggersIndex;

	pthread_mutex_lock(&onDemand_notify_lock);
	if (onDemand_notify_token == -1) {
		status = notify_register_check(kSCNETWORKCONNECTION_ONDEMAND_NOTIFY_KEY, &onDemand_notify_token);
		if (status != NOTIFY_STATUS_OK) {
			SCLog(TRUE, LOG_ERR, CFSTR("notify_register_check() failed, status=%lu"), status);
			onDemand_notify_token = -1;
		}
	}
	if (onDemand_notify_token != -1) {
		status = notify_check(onDemand_notify_token, &changed);
		if (status != NOTIFY_STATUS_OK) {
			SCLog(TRUE, LOG_ERR, CFSTR("notify_check() failed, status=%lu"), status);
			(void)notify_cancel(onDemand_notify_token);
			onDemand_notify_token = -1;
		}
	}

	if (changed && (onDemand_notify_token != -1)) {
		status = notify_get_state(onDemand_notify_token, &triggersCount);
		if (status != NOTIFY_STATUS_OK) {
			SCLog(TRUE, LOG_ERR, CFSTR("notify_get_state() failed, status=%lu"), status);
			(void)notify_cancel(onDemand_notify_token);
			onDemand_notify_token = -1;
		}
	}

	if (changed) {
		CFStringRef	key;

		if (_sc_debug || (debug > 0)) {
			SCLog(TRUE, LOG_INFO,
			      CFSTR("OnDemand information %s"),
			      (onDemand_configuration == NULL) ? "fetched" : "updated");
		}

		if (onDemand_configuration != NULL) {
			CFRelease(onDemand_configuration);
			onDemand_configuration = NULL;
		}

		if (triggersCount > 0) {
			if (store == NULL) {
				store = SCDynamicStoreCreate(NULL, CFSTR("__SCNetworkConnectionCopyOnDemandInfoWithName"), NULL, NULL);
				if (store == NULL) {
					SCLog(TRUE, LOG_ERR, CFSTR("__SCNetworkConnectionCopyOnDemandInfoWithName SCDynamicStoreCreate() failed"));

					// force retry on next check
					if (onDemand_notify_token != -1) {
						(void)notify_cancel(onDemand_notify_token);
						onDemand_notify_token = -1;
					}
					pthread_mutex_unlock(&onDemand_notify_lock);
					return FALSE;
				}
				*storeP = store;
			}

			key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetOnDemand);
			onDemand_configuration = SCDynamicStoreCopyValue(store, key);
			CFRelease(key);
			if ((onDemand_configuration != NULL) && !isA_CFDictionary(onDemand_configuration)) {
				CFRelease(onDemand_configuration);
				onDemand_configuration = NULL;
			}
		}
	}

	configuration = (onDemand_configuration != NULL) ? CFRetain(onDemand_configuration) : NULL;
	pthread_mutex_unlock(&onDemand_notify_lock);

	if (configuration == NULL) {
		// if no "OnDemand" configurations
		return FALSE;
	}

	triggers = CFDictionaryGetValue(configuration, kSCNetworkConnectionOnDemandTriggers);
	triggersCount = isA_CFArray(triggers) ? CFArrayGetCount(triggers) : 0;
	for (triggersIndex = 0; triggersIndex < triggersCount; triggersIndex++) {
		CFArrayRef	domains;
		int		domainsCount;
		int		domainsIndex;
		CFStringRef	key;
		CFDictionaryRef	trigger;

		trigger = CFArrayGetValueAtIndex(triggers, triggersIndex);
		if (!isA_CFDictionary(trigger)) {
			// if not a valid "OnDemand" configuration
			continue;
		}

		/*
		 * If we haven't tried a resulution yet, we only want to check for a name
		 * match for domains that require to always connect.
		 */
		key = onDemandRetry ? kSCNetworkConnectionOnDemandMatchDomainsOnRetry
				    : kSCNetworkConnectionOnDemandMatchDomainsAlways;
		domains = CFDictionaryGetValue(trigger, key);
		domainsCount = isA_CFArray(domains) ? CFArrayGetCount(domains) : 0;
		for (domainsIndex = 0; domainsIndex < domainsCount; domainsIndex++) {
			CFStringRef	domain;

			domain = CFArrayGetValueAtIndex(domains, domainsIndex);
			if (!isA_CFString(domain)) {
				// if not a valid match domain
				continue;
			}

			if (domainEndsWithDomain(hostName, domain)) {
				CFArrayRef			exceptions;
				int				exceptionsCount;
				int				exceptionsIndex;
				CFNumberRef			num;
				SCNetworkConnectionStatus	onDemandStatus	= kSCNetworkConnectionDisconnected;

				// we have a matching domain, check against exception list
				exceptions = CFDictionaryGetValue(trigger, kSCNetworkConnectionOnDemandMatchDomainsNever);
				exceptionsCount = isA_CFArray(exceptions) ? CFArrayGetCount(exceptions) : 0;
				for (exceptionsIndex = 0; exceptionsIndex < exceptionsCount; exceptionsIndex++) {
					CFStringRef	exception;

					exception = CFArrayGetValueAtIndex(exceptions, exceptionsIndex);
					if (!isA_CFString(exception)) {
						// if not a valid match exception
						continue;
					}

					if (domainEndsWithDomain(hostName, exception)) {
						// found matching exception
						if (_sc_debug || (debug > 0)) {
							SCLog(TRUE, LOG_INFO, CFSTR("OnDemand match exception"));
						}
						goto done;
					}
				}

				// if we have a matching domain and there were no exceptions
				// then we pass back the OnDemand info

				if (!CFDictionaryGetValueIfPresent(trigger,
								   kSCNetworkConnectionOnDemandStatus,
								   (const void **)&num) ||
				    !isA_CFNumber(num) ||
				    !CFNumberGetValue(num, kCFNumberSInt32Type, &onDemandStatus)) {
					onDemandStatus = kSCNetworkConnectionDisconnected;
				}
				if (connectionStatus != NULL) {
					*connectionStatus = onDemandStatus;
				}

				if (connectionServiceID != NULL) {
					*connectionServiceID = CFDictionaryGetValue(trigger, kSCNetworkConnectionOnDemandServiceID);
					*connectionServiceID = isA_CFString(*connectionServiceID);
					if (*connectionServiceID != NULL) {
						CFRetain(*connectionServiceID);
					}

				}

				if (vpnRemoteAddress != NULL) {
					*vpnRemoteAddress = CFDictionaryGetValue(trigger, kSCNetworkConnectionOnDemandRemoteAddress);
					*vpnRemoteAddress = isA_CFString(*vpnRemoteAddress);
					if (*vpnRemoteAddress != NULL) {
						CFRetain(*vpnRemoteAddress);
					}
				}

				if (_sc_debug || (debug > 0)) {
					SCLog(TRUE, LOG_INFO,
					      CFSTR("OnDemand%s match, connection status = %d"),
					      onDemandRetry ? " (on retry)" : "",
					      onDemandStatus);
				}

				ok = TRUE;
				goto done;
			}
		}
	}

//	if (_sc_debug || (debug > 0)) {
//		SCLog(TRUE, LOG_INFO, CFSTR("OnDemand domain name(s) not matched"));
//	}

    done :

	if (configuration != NULL) CFRelease(configuration);
	return ok;
}


Boolean
SCNetworkConnectionCopyUserPreferences(CFDictionaryRef	selectionOptions,
				       CFStringRef	*serviceID,
				       CFDictionaryRef	*userOptions)
{
	int			prefsChanged	= 1;
	SCDynamicStoreRef	session		= NULL;
	Boolean			success		= FALSE;
	int			status;


	/* initialize runtime */
	pthread_once(&initialized, __SCNetworkConnectionInitialize);

	/* first check for new VPN OnDemand style */
	if (selectionOptions != NULL) {
		CFStringRef	hostName;

		hostName = CFDictionaryGetValue(selectionOptions, kSCNetworkConnectionSelectionOptionOnDemandHostName);
		if (isA_CFString(hostName)) {
			CFStringRef			connectionServiceID	= NULL;
			SCNetworkConnectionStatus	connectionStatus;
			Boolean				onDemandRetry;
			CFTypeRef			val;

			val = CFDictionaryGetValue(selectionOptions, kSCNetworkConnectionSelectionOptionOnDemandRetry);
			onDemandRetry = isA_CFBoolean(val) ? CFBooleanGetValue(val) : TRUE;

			success = __SCNetworkConnectionCopyOnDemandInfoWithName(&session,
										hostName,
										onDemandRetry,
										&connectionServiceID,
										&connectionStatus,
										NULL);
			if (debug > 1) {
				SCLog(TRUE, LOG_DEBUG,
				      CFSTR("SCNetworkConnectionCopyUserPreferences __SCNetworkConnectionCopyOnDemandInfoWithName returns %d w/status %d"),
				      success,
				      connectionStatus);
			}

			if (success) {
				// if the hostname matches an OnDemand domain
				if (session != NULL) {
					CFRelease(session);
				}
				if (connectionStatus == kSCNetworkConnectionConnected) {
					// if we are already connected
					if (connectionServiceID != NULL) {
						CFRelease(connectionServiceID);
					}
					return FALSE;
				}

				*serviceID   = connectionServiceID;
				*userOptions = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
				CFDictionarySetValue((CFMutableDictionaryRef)*userOptions, kSCNetworkConnectionSelectionOptionOnDemandHostName, hostName);
				return TRUE;
			} else if (!onDemandRetry) {
				// if the hostname does not match an OnDemand domain and we have
				// not yet issued an initial DNS query (i.e. it's not a query
				// being retried after the VPN has been established) than we're
				// done
				if (session != NULL) {
					CFRelease(session);
				}
				return FALSE;
			}
		}
	}

	if (notify_userprefs_token == -1) {
		status = notify_register_check(k_NetworkConnect_Notification, &notify_userprefs_token);
		if (status != NOTIFY_STATUS_OK) {
			SCLog(TRUE, LOG_ERR, CFSTR("notify_register_check() failed, status=%lu"), status);
			(void)notify_cancel(notify_userprefs_token);
			notify_userprefs_token = -1;
		} else {
			// clear the "something has changed" state
			(void) notify_check(notify_userprefs_token, &prefsChanged);
			prefsChanged = 1;
		}
	}
	if (notify_userprefs_token != -1) {
		status = notify_check(notify_userprefs_token, &prefsChanged);
		if (status != NOTIFY_STATUS_OK) {
			SCLog(TRUE, LOG_ERR, CFSTR("notify_check() failed, status=%lu"), status);
			(void)notify_cancel(notify_userprefs_token);
			notify_userprefs_token = -1;
		}
	}


	*serviceID = NULL;
	*userOptions = NULL;

	if (session == NULL) {
		session	= SCDynamicStoreCreate(NULL, CFSTR("SCNetworkConnection"), NULL, NULL);
	}
	if (session == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("Error,  SCNetworkConnectionCopyUserPreferences, SCDynamicStoreCreate() returned NULL!"));
		return FALSE;
	}

	if (selectionOptions != NULL) {
		Boolean		catchAllFound	= FALSE;
		CFIndex		catchAllService	= 0;
		CFIndex		catchAllConfig	= 0;
		CFStringRef	hostName	= NULL;
		CFStringRef	priority	= NULL;
		CFArrayRef	serviceNames	= NULL;
		CFDictionaryRef	services	= NULL;
		CFIndex		serviceIndex;
		CFIndex		servicesCount;

		hostName = CFDictionaryGetValue(selectionOptions, kSCNetworkConnectionSelectionOptionOnDemandHostName);
		if (hostName == NULL) {
			hostName = CFDictionaryGetValue(selectionOptions, kSCPropNetPPPOnDemandHostName);
		}
		hostName = isA_CFString(hostName);
		if (hostName == NULL)
			goto done_selection;	// if no hostname for matching

		priority = CFDictionaryGetValue(selectionOptions, kSCPropNetPPPOnDemandPriority);
		if (!isA_CFString(priority))
			priority = kSCValNetPPPOnDemandPriorityDefault;


		if (!isA_CFArray(serviceNames))
			goto done_selection;


		if (!isA_CFDictionary(services))
			goto done_selection;

		servicesCount = CFArrayGetCount(serviceNames);
		for (serviceIndex = 0; serviceIndex < servicesCount; serviceIndex++) {
			CFIndex		configIndex;
			CFIndex		configsCount;
			CFArrayRef	serviceConfigs;
			CFStringRef	serviceName;
			int		val;

			serviceName = CFArrayGetValueAtIndex(serviceNames, serviceIndex);
			if (!isA_CFString(serviceName))
				continue;

			serviceConfigs = CFDictionaryGetValue(services, serviceName);
			if (!isA_CFArray(serviceConfigs))
				continue;

			configsCount = CFArrayGetCount(serviceConfigs);
			for (configIndex = 0; configIndex < configsCount; configIndex++) {
				CFNumberRef	autodial;
				CFDictionaryRef config;
				CFDictionaryRef pppConfig;

				config = CFArrayGetValueAtIndex(serviceConfigs, configIndex);
				if (!isA_CFDictionary(config))
					continue;

				pppConfig = CFDictionaryGetValue(config, kSCEntNetPPP);
				if (!isA_CFDictionary(pppConfig))
					continue;

				autodial = CFDictionaryGetValue(pppConfig, kSCPropNetPPPOnDemandEnabled);
				if (!isA_CFNumber(autodial))
					continue;

				CFNumberGetValue(autodial, kCFNumberIntType, &val);
				if (val) {
					CFArrayRef	domains;
					CFIndex		domainsCount;
					CFIndex		domainsIndex;

					/* we found an conditional connection enabled configuration */

					/* check domain */
					domains = CFDictionaryGetValue(pppConfig, kSCPropNetPPPOnDemandDomains);
					if (!isA_CFArray(domains))
						continue;

					domainsCount = CFArrayGetCount(domains);
					for (domainsIndex = 0; domainsIndex < domainsCount; domainsIndex++) {
						CFStringRef	domain;

						domain = CFArrayGetValueAtIndex(domains, domainsIndex);
						if (!isA_CFString(domain))
							continue;

						if (!catchAllFound &&
						    (CFStringCompare(domain, CFSTR(""), 0) == kCFCompareEqualTo
							|| CFStringCompare(domain, CFSTR("."), 0) == kCFCompareEqualTo)) {
							// found a catch all
							catchAllFound = TRUE;
							catchAllService = serviceIndex;
							catchAllConfig = configIndex;
						}

						if (domainEndsWithDomain(hostName, domain)) {
							// found matching configuration
							*serviceID = serviceName;
							CFRetain(*serviceID);
							*userOptions = CFDictionaryCreateMutableCopy(NULL, 0, config);
							CFDictionarySetValue((CFMutableDictionaryRef)*userOptions, kSCNetworkConnectionSelectionOptionOnDemandHostName, hostName);
							CFDictionarySetValue((CFMutableDictionaryRef)*userOptions, kSCPropNetPPPOnDemandPriority, priority);
							addPasswordFromKeychain(session, *serviceID, userOptions);
							success = TRUE;
							goto done_selection;
						}
					}
				}
			}
		}

		// config not found, do we have a catchall ?
		if (catchAllFound) {
			CFDictionaryRef config;
			CFArrayRef	serviceConfigs;
			CFStringRef	serviceName;

			serviceName = CFArrayGetValueAtIndex(serviceNames, catchAllService);
			serviceConfigs = CFDictionaryGetValue(services, serviceName);
			config = CFArrayGetValueAtIndex(serviceConfigs, catchAllConfig);

			*serviceID = serviceName;
			CFRetain(*serviceID);
			*userOptions = CFDictionaryCreateMutableCopy(NULL, 0, config);
			CFDictionarySetValue((CFMutableDictionaryRef)*userOptions, kSCNetworkConnectionSelectionOptionOnDemandHostName, hostName);
			CFDictionarySetValue((CFMutableDictionaryRef)*userOptions, kSCPropNetPPPOnDemandPriority, priority);
			addPasswordFromKeychain(session, *serviceID, userOptions);
			success = TRUE;
			goto done_selection;
		}

	    done_selection:

		if (serviceNames)
			CFRelease(serviceNames);
		if (services)
			CFRelease(services);
		CFRelease(session);

		if (debug > 1) {
			SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionCopyUserPreferences %@"), success ? CFSTR("succeeded") : CFSTR("failed"));
			SCLog(TRUE, LOG_DEBUG, CFSTR("Selection options: %@"), selectionOptions);
		}

		return success;
	}

	/* we don't have selection options */

	// (1) Figure out which service ID we care about, allocate it into passed "serviceID"
	success = SCNetworkConnectionPrivateCopyDefaultServiceIDForDial(session, serviceID);

	if (success && (*serviceID != NULL)) {
		// (2) Get the list of user data for this service ID
		CFPropertyListRef	userServices	= NULL;


		// (3) We are expecting an array if the user has defined records for this service ID or NULL if the user hasn't
		if (userServices != NULL) {
			if (isA_CFArray(userServices)) {
				// (4) Get the default set of user options for this service
				success = SCNetworkConnectionPrivateCopyDefaultUserOptionsFromArray((CFArrayRef)userServices,
												    userOptions);
				if(success && (userOptions != NULL)) {
					addPasswordFromKeychain(session, *serviceID, userOptions);
				}
			} else {
				SCLog(TRUE, LOG_DEBUG, CFSTR("Error, userServices are not of type CFArray!"));
			}

			CFRelease(userServices); // this is OK because SCNetworkConnectionPrivateISExpectedCFType() checks for NULL
		}
	}

	if (debug > 1) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionCopyUserPreferences %@, no selection options"), success ? CFSTR("succeeded") : CFSTR("failed"));
	}

	CFRelease(session);
	return success;
}


//*******************************************************************************************
// SCNetworkConnectionPrivateCopyDefaultServiceIDForDial
// ----------------------------------------------------
// Try to find the service id to connect
// (1) Start by looking at the last service used in Network Pref / Network menu extra
// (2) If Network Pref / Network menu extra has not been used, find the PPP service
//     with the highest ordering
//********************************************************************************************
static Boolean
SCNetworkConnectionPrivateCopyDefaultServiceIDForDial(SCDynamicStoreRef session, CFStringRef *serviceID)
{
	Boolean			foundService		= FALSE;
	CFPropertyListRef	lastServiceSelectedInIC = NULL;



	// we found the service the user last had open in IC
	if (lastServiceSelectedInIC != NULL) {
		// make sure its a PPP service
		if (SCNetworkConnectionPrivateIsPPPService(session, lastServiceSelectedInIC, kSCValNetInterfaceSubTypePPPSerial, kSCValNetInterfaceSubTypePPPoE)) {
			// make sure the service that we found is valid
			CFDictionaryRef	dict;
			CFStringRef	key;

			key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
									  kSCDynamicStoreDomainSetup,
									  lastServiceSelectedInIC,
									  kSCEntNetInterface);
			dict = SCDynamicStoreCopyValue(session, key);
			CFRelease(key);
			if (dict != NULL) {
				CFRelease(dict);
				*serviceID = CFRetain(lastServiceSelectedInIC);
				foundService = TRUE;
			}
		}
		CFRelease(lastServiceSelectedInIC);
	}

	if (!foundService) {
		foundService = SCNetworkConnectionPrivateGetPPPServiceFromDynamicStore(session, serviceID);
	}

	return foundService;
}

//********************************************************************************
// SCNetworkConnectionPrivateGetPPPServiceFromDynamicStore
// -------------------------------------------------------
// Find the highest ordered PPP service in the dynamic store
//********************************************************************************
static Boolean
SCNetworkConnectionPrivateGetPPPServiceFromDynamicStore(SCDynamicStoreRef session, CFStringRef *serviceID)
{
	CFDictionaryRef	dict		= NULL;
	CFStringRef	key		= NULL;
	CFArrayRef	serviceIDs	= NULL;
	Boolean		success		= FALSE;

	*serviceID = NULL;

	do {
		CFIndex count;
		CFIndex i;

		key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainSetup, kSCEntNetIPv4);
		if (key == NULL) {
			fprintf(stderr, "Error, Setup Key == NULL!\n");
			break;
		}

		dict = SCDynamicStoreCopyValue(session, key);
		if (!isA_CFDictionary(dict)) {
			fprintf(stderr, "no global IPv4 entity\n");
			break;
		}

		serviceIDs = CFDictionaryGetValue(dict, kSCPropNetServiceOrder); // array of service id's
		if (!isA_CFArray(serviceIDs)) {
			fprintf(stderr, "service order not specified\n");
			break;
		}

		count = CFArrayGetCount(serviceIDs);
		for (i = 0; i < count; i++) {
			CFStringRef service = CFArrayGetValueAtIndex(serviceIDs, i);

			if (SCNetworkConnectionPrivateIsPPPService(session, service, kSCValNetInterfaceSubTypePPPSerial, kSCValNetInterfaceSubTypePPPoE)) {
				*serviceID = CFRetain(service);
				success = TRUE;
				break;
			}
		}
	} while (FALSE);

	if (key != NULL)	CFRelease(key);
	if (dict != NULL)	CFRelease(dict);

	return success;
}

//********************************************************************************
// SCNetworkConnectionPrivateCopyDefaultUserOptionsFromArray
// ---------------------------------------------------------
// Copy over user preferences for a particular service if they exist
//********************************************************************************
static Boolean
SCNetworkConnectionPrivateCopyDefaultUserOptionsFromArray(CFArrayRef userOptionsArray, CFDictionaryRef *userOptions)
{
	CFIndex	count	= CFArrayGetCount(userOptionsArray);
	int	i;

	for (i = 0; i < count; i++) {
		// (1) Find the dictionary
		CFPropertyListRef propertyList = CFArrayGetValueAtIndex(userOptionsArray, i);

		if (isA_CFDictionary(propertyList) != NULL) {
			// See if there's a value for dial on demand
			CFPropertyListRef value;

			value = CFDictionaryGetValue((CFDictionaryRef)propertyList, k_Dial_Default_Key);
			if (isA_CFBoolean(value) != NULL) {
				if (CFBooleanGetValue(value)) {
					// we found the default user options
					*userOptions = CFDictionaryCreateCopy(NULL,
									      (CFDictionaryRef)propertyList);
					break;
				}
			}
		}
	}

	return TRUE;
}

//********************************************************************************
// SCNetworkConnectionPrivateIsServiceType
// --------------------------------------
// Check and see if the service is a PPP service of the given types
//********************************************************************************
static Boolean
SCNetworkConnectionPrivateIsPPPService(SCDynamicStoreRef session, CFStringRef serviceID, CFStringRef subType1, CFStringRef subType2)
{
	CFStringRef	entityKey;
	Boolean		isPPPService		= FALSE;
	Boolean		isMatchingSubType	= FALSE;
	CFDictionaryRef	serviceDict;

	entityKey = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								kSCDynamicStoreDomainSetup,
								serviceID,
								kSCEntNetInterface);
	if (entityKey == NULL) {
		return FALSE;
	}

	serviceDict = SCDynamicStoreCopyValue(session, entityKey);
	if (serviceDict != NULL) {
		if (isA_CFDictionary(serviceDict)) {
			CFStringRef	type;
			CFStringRef	subtype;

			type = CFDictionaryGetValue(serviceDict, kSCPropNetInterfaceType);
			if (isA_CFString(type)) {
				isPPPService = CFEqual(type, kSCValNetInterfaceTypePPP);
			}

			subtype = CFDictionaryGetValue(serviceDict, kSCPropNetInterfaceSubType);
			if (isA_CFString(subtype)) {
				isMatchingSubType = CFEqual(subtype, subType1);
				if (!isMatchingSubType && subType2)
					isMatchingSubType = CFEqual(subtype, subType2);
			}
		}
		CFRelease(serviceDict);
	}
	CFRelease(entityKey);

	return (isPPPService && isMatchingSubType);
}

//********************************************************************************
// addPasswordFromKeychain
// --------------------------------------
// Get the password and shared secret out of the keychain and add
// them to the PPP and IPSec dictionaries
//********************************************************************************
static void
addPasswordFromKeychain(SCDynamicStoreRef session, CFStringRef serviceID, CFDictionaryRef *userOptions)
{
	CFPropertyListRef	uniqueID;
	CFStringRef		password;
	CFStringRef		sharedsecret	= NULL;

	/* user options must exist */
	if (*userOptions == NULL)
		return;

	/* first, get the unique identifier used to store passwords in the keychain */
	uniqueID = CFDictionaryGetValue(*userOptions, k_Unique_Id_Key);
	if (!isA_CFString(uniqueID))
		return;

	/* first, get the PPP password */
	password = copyPasswordFromKeychain(uniqueID);

	/* then, if necessary, get the IPSec Shared Secret */
	if (SCNetworkConnectionPrivateIsPPPService(session, serviceID, kSCValNetInterfaceSubTypeL2TP, 0)) {
		CFMutableStringRef	uniqueIDSS;

		uniqueIDSS = CFStringCreateMutableCopy(NULL, 0, uniqueID);
		CFStringAppend(uniqueIDSS, CFSTR(".SS"));
		sharedsecret = copyPasswordFromKeychain(uniqueIDSS);
		CFRelease(uniqueIDSS);
	}

	/* did we find our information in the key chain ? */
	if ((password != NULL) || (sharedsecret != NULL)) {
		CFMutableDictionaryRef	newOptions;

		newOptions = CFDictionaryCreateMutableCopy(NULL, 0, *userOptions);

		/* PPP password */
		if (password != NULL) {
			CFDictionaryRef		entity;
			CFMutableDictionaryRef	newEntity;

			entity = CFDictionaryGetValue(*userOptions, kSCEntNetPPP);
			if (isA_CFDictionary(entity))
				newEntity = CFDictionaryCreateMutableCopy(NULL, 0, entity);
			else
				newEntity = CFDictionaryCreateMutable(NULL,
								      0,
								      &kCFTypeDictionaryKeyCallBacks,
								      &kCFTypeDictionaryValueCallBacks);


			/* set the PPP password */
			CFDictionarySetValue(newEntity, kSCPropNetPPPAuthPassword, uniqueID);
			CFDictionarySetValue(newEntity, kSCPropNetPPPAuthPasswordEncryption, kSCValNetPPPAuthPasswordEncryptionKeychain);
			CFRelease(password);

			/* update the PPP entity */
			CFDictionarySetValue(newOptions, kSCEntNetPPP, newEntity);
			CFRelease(newEntity);
		}

		/* IPSec Shared Secret */
		if (sharedsecret != NULL) {
			CFDictionaryRef		entity;
			CFMutableDictionaryRef	newEntity;

			entity = CFDictionaryGetValue(*userOptions, kSCEntNetIPSec);
			if (isA_CFDictionary(entity))
				newEntity = CFDictionaryCreateMutableCopy(NULL, 0, entity);
			else
				newEntity = CFDictionaryCreateMutable(NULL,
								      0,
								      &kCFTypeDictionaryKeyCallBacks,
								      &kCFTypeDictionaryValueCallBacks);

			/* set the IPSec Shared Secret */
			CFDictionarySetValue(newEntity, kSCPropNetIPSecSharedSecret, sharedsecret);
			CFRelease(sharedsecret);

			/* update the IPSec entity */
			CFDictionarySetValue(newOptions, kSCEntNetIPSec, newEntity);
			CFRelease(newEntity);
		}

		/* update the userOptions dictionary */
		CFRelease(*userOptions);
		*userOptions = CFDictionaryCreateCopy(NULL, newOptions);
		CFRelease(newOptions);
	}

}

#if	!TARGET_OS_IPHONE
//********************************************************************************
// copyKeychainEnumerator
// --------------------------------------
// Gather Keychain Enumerator
//********************************************************************************
static CFArrayRef
copyKeychainEnumerator(CFStringRef uniqueIdentifier)
{
	char			*buf;
	CFMutableArrayRef	itemArray	= NULL;
	OSStatus		result;
	SecKeychainSearchRef	search		= NULL;

	buf = _SC_cfstring_to_cstring(uniqueIdentifier, NULL, 0, kCFStringEncodingUTF8);
	if (buf != NULL) {
		// search for unique identifier in "svce" attribute
		SecKeychainAttribute		attributes[]	= {{ kSecServiceItemAttr,
								     CFStringGetLength(uniqueIdentifier),
								     (void *)buf
								   }};

		SecKeychainAttributeList	attrList	= { sizeof(attributes) / sizeof(*attributes),
								    attributes };

		result = SecKeychainSearchCreateFromAttributes(NULL, kSecGenericPasswordItemClass, &attrList, &search);
		if (result == noErr) {
			itemArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

			while (result == noErr) {
				SecKeychainItemRef	itemFound	= NULL;

				result = SecKeychainSearchCopyNext(search, &itemFound);
				if (result != noErr) {
					break;
				}

				if (itemFound) {
					CFArrayAppendValue(itemArray, itemFound);
					CFRelease(itemFound);
				}
			}
		}
	}

	if (search)	CFRelease(search);
	if (buf)	CFAllocatorDeallocate(NULL, buf);

	return itemArray;
}
#endif	// !TARGET_OS_IPHONE

//********************************************************************************
// copyPasswordFromKeychain
// --------------------------------------
// Given a uniqueID, retrieve the password from the keychain
//********************************************************************************
static CFStringRef
copyPasswordFromKeychain(CFStringRef uniqueID)
{
#if	!TARGET_OS_IPHONE
	CFArrayRef	enumerator;
	CFIndex		n;
	CFStringRef	password = NULL;

	enumerator = copyKeychainEnumerator(uniqueID);
	if (enumerator == NULL) {
		return NULL;		// if no keychain enumerator
	}

	n = CFArrayGetCount(enumerator);
	if (n > 0) {
		void			*data	= NULL;
		UInt32			dataLen	= 0;
		SecKeychainItemRef	itemRef;
		OSStatus		result;

		itemRef = (SecKeychainItemRef)CFArrayGetValueAtIndex(enumerator, 0);
		result = SecKeychainItemCopyContent(itemRef,		// itemRef
						    NULL,		// itemClass
						    NULL,		// attrList
						    &dataLen,		// length
						    (void *)&data);	// outData
		if ((result == noErr) && (data != NULL) && (dataLen > 0)) {
			password = CFStringCreateWithBytes(NULL, data, dataLen, kCFStringEncodingUTF8, TRUE);
			(void) SecKeychainItemFreeContent(NULL, data);
		}

	}

	CFRelease(enumerator);

	return password;
#else	// !TARGET_OS_IPHONE
	return NULL;
#endif	// !TARGET_OS_IPHONE
}
