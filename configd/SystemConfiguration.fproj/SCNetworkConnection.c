/*
 * Copyright (c) 2003-2007 Apple Inc. All rights reserved.
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


#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>

#include <Security/Security.h>
#include "dy_framework.h"

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>

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

#include <ppp/ppp_msg.h>
#include <ppp/PPPControllerPriv.h>
#include "pppcontroller.h"
#include <ppp/pppcontroller_types.h>



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
	CFRunLoopSourceRef		rls;
	SCNetworkConnectionCallBack	rlsFunction;
	SCNetworkConnectionContext	rlsContext;
	CFMutableArrayRef		rlList;

	/* misc info */
	int				debug;

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

	if (connectionPrivate->debug) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionDeallocate (0x%x)"), connectionPrivate);
	}

	/* release resources */
	pthread_mutex_destroy(&connectionPrivate->lock);

	if (connectionPrivate->rlList != NULL) {
		CFRunLoopSourceInvalidate(connectionPrivate->rls);
		CFRelease(connectionPrivate->rls);
		CFRelease(connectionPrivate->rlList);
	}

	if (connectionPrivate->notify_port != NULL) {
		CFMachPortInvalidate(connectionPrivate->notify_port);
		CFRelease(connectionPrivate->notify_port);
	}

	if (connectionPrivate->session_port != MACH_PORT_NULL)
		mach_port_destroy(mach_task_self(), connectionPrivate->session_port);

	if (connectionPrivate->rlsContext.release != NULL)
		(*connectionPrivate->rlsContext.release)(connectionPrivate->rlsContext.info);

	CFRelease(connectionPrivate->service);

	return;
}


static pthread_once_t initialized		= PTHREAD_ONCE_INIT;

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
	__kSCNetworkConnectionTypeID = _CFRuntimeRegisterClass(&__SCNetworkConnectionClass);
	return;
}


static SCNetworkConnectionStatus
__SCNetworkConnectionConvertStatus(int state)
{
	SCNetworkConnectionStatus	status = kSCNetworkConnectionDisconnected;

	switch (state) {
		case PPP_INITIALIZE:
		case PPP_CONNECTLINK:
		case PPP_ESTABLISH:
		case PPP_AUTHENTICATE:
		case PPP_CALLBACK:
		case PPP_NETWORK:
		case PPP_WAITONBUSY:
			status = kSCNetworkConnectionConnecting;
			break;
		case PPP_TERMINATE:
		case PPP_DISCONNECTLINK:
			status = kSCNetworkConnectionDisconnecting;
			break;
		case PPP_RUNNING:
		case PPP_ONHOLD:
			status = kSCNetworkConnectionConnected;
			break;
		case PPP_IDLE:
		case PPP_DORMANT:
		case PPP_HOLDOFF:
		default:
			status = kSCNetworkConnectionDisconnected;
	}
	return status;
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
	int				phase			= PPP_IDLE;
	SCNetworkConnectionCallBack	rlsFunction;
	kern_return_t			status;
	SCNetworkConnectionStatus	scstatus;

	if (msgid == MACH_NOTIFY_NO_SENDERS) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("__SCNetworkConnectionCallBack: PPPController server died"));
	} else {
		status = pppcontroller_getstatus(connectionPrivate->session_port, &phase, &error);
	}

	if (connectionPrivate->rls == NULL) {
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

	scstatus = __SCNetworkConnectionConvertStatus(phase);

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
	char				*envdebug;
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

	/* get the debug environment variable */
	envdebug = getenv("PPPDebug");
	if (envdebug) {
		if (sscanf(envdebug, "%d", &connectionPrivate->debug) != 1)
			connectionPrivate->debug = 1; /* PPPDebug value is invalid, set debug to 1 */
	}

	connectionPrivate->rlsFunction = callout;

	if (context) {
		bcopy(context, &connectionPrivate->rlsContext, sizeof(SCNetworkConnectionContext));
		if (context->retain != NULL) {
			connectionPrivate->rlsContext.info = (void *)(*context->retain)(context->info);
		}
	}

	if (connectionPrivate->debug) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionCreate (0x%x) succeeded for service : %@"), connectionPrivate, service);
	}

	/* success, return the connection reference */
	return connectionPrivate;

    fail:

	if (connectionPrivate->debug)
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionCreate (0x%x) failed for service : %@"), connectionPrivate, service);

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
	mach_port_t	port_old		= MACH_PORT_NULL;
	mach_port_t	server			= MACH_PORT_NULL;
	kern_return_t	status;
	mach_port_t	unpriv_bootstrap_port	= MACH_PORT_NULL;

	if (connectionPrivate->session_port != MACH_PORT_NULL) {
		return connectionPrivate->session_port;
	}

	pthread_mutex_lock(&connectionPrivate->lock);

	if (bootstrap_look_up(bootstrap_port, PPPCONTROLLER_SERVER, &server) != BOOTSTRAP_SUCCESS) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("PPP Controller server not found"));
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
		CFMachPortContext	context	= { 0
			, (void *)connectionPrivate
			, NULL
			, NULL
			, pppMPCopyDescription
		};

		/* allocate port (for server response) */
		connectionPrivate->notify_port = CFMachPortCreate(NULL, __SCNetworkConnectionCallBack, &context, NULL);

		/* request a notification when/if the server dies */
		notify_port = CFMachPortGetPort(connectionPrivate->notify_port);
		status = mach_port_request_notification(mach_task_self(),
							notify_port,
							MACH_NOTIFY_NO_SENDERS,
							1,
							notify_port,
							MACH_MSG_TYPE_MAKE_SEND_ONCE,
							&port_old);
		if (status != KERN_SUCCESS) {
			goto done;
		}
	}

	status = pppcontroller_attach(server, data, dataLen, unpriv_bootstrap_port, notify_port,
				      &connectionPrivate->session_port, &error);
	if (status != KERN_SUCCESS) {
		error = kSCStatusFailed;
	}

    done :

	if (dataRef != NULL)	CFRelease(dataRef);

	if (unpriv_bootstrap_port != MACH_PORT_NULL) {
		mach_port_deallocate(mach_task_self(), unpriv_bootstrap_port);
	}

	if (error != kSCStatusOK) {
		if (connectionPrivate->session_port != MACH_PORT_NULL) {
			mach_port_destroy(mach_task_self(), connectionPrivate->session_port);
			connectionPrivate->session_port = MACH_PORT_NULL;
		}
		if (connectionPrivate->notify_port != NULL) {
			CFMachPortInvalidate(connectionPrivate->notify_port);
			CFRelease(connectionPrivate->notify_port);
			connectionPrivate->notify_port = NULL;
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
				if (CFEqual(interfaceType, kSCNetworkInterfaceTypePPP)) {
					CFArrayAppendValue(available, service);
				}
			}

			CFRelease(services);
		}
	}

	if (tempSet)	CFRelease(set);
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
	SCPreferencesRef	prefs;
	SCNetworkServiceRef	service;

	if (!isA_CFString(serviceID)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	prefs = SCPreferencesCreate(NULL, CFSTR("SCNetworkConnectionCreateWithServiceID"), NULL);
	if (prefs == NULL) {
		return NULL;
	}

	service = SCNetworkServiceCopy(prefs, serviceID);
	CFRelease(prefs);
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
	int				phase;
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

	status = pppcontroller_getstatus(session_port, &phase, &error);
	if ((status != KERN_SUCCESS) || (error != kSCStatusOK)) {
		return kSCNetworkConnectionDisconnected;
	}

	scstatus = __SCNetworkConnectionConvertStatus(phase);
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

	if (connectionPrivate->debug) {
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

	if (connectionPrivate->debug)
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionStart (0x%x), return: %d"), connectionPrivate, error);

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

	if (connectionPrivate->debug)
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionStop (0x%x)"), connectionPrivate);

	status = pppcontroller_stop(session_port, forceDisconnect, &error);
	if (status != KERN_SUCCESS) {
		goto fail;
	}

	if (connectionPrivate->debug)
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionStop (0x%x), return: %d"), connectionPrivate, error);

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

	if (connectionPrivate->debug)
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionSuspend (0x%x)"), connectionPrivate);

	status = pppcontroller_suspend(session_port, &error);
	if (status != KERN_SUCCESS) {
		goto fail;
	}

	if (connectionPrivate->debug)
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionSuspend (0x%x), return: %d"), connectionPrivate, error);

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

	if (connectionPrivate->debug)
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionResume (0x%x)"), connectionPrivate);

	status = pppcontroller_resume(session_port, &error);
	if (status != KERN_SUCCESS) {
		goto fail;
	}

	if (connectionPrivate->debug)
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCNetworkConnectionResume (0x%x), return: %d"), connectionPrivate, error);

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


Boolean
SCNetworkConnectionScheduleWithRunLoop(SCNetworkConnectionRef	connection,
				       CFRunLoopRef		runLoop,
				       CFStringRef		runLoopMode)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	int				error			= kSCStatusFailed;
	mach_port_t			session_port;
	kern_return_t			status;

	if (!isA_SCNetworkConnection(connection) || runLoop == NULL || runLoopMode == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (connectionPrivate->rlsFunction == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if ((connectionPrivate->rlList != NULL) &&
	    _SC_isScheduled(NULL, runLoop, runLoopMode, connectionPrivate->rlList)) {
		/* already scheduled */
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	session_port = __SCNetworkConnectionSessionPort(connectionPrivate);
	if (session_port == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (connectionPrivate->rlList == NULL) {
		status = pppcontroller_notification(session_port, 1, &error);
		if ((status != KERN_SUCCESS) || (error != kSCStatusOK)) {
			_SCErrorSet(error);
			return FALSE;
		}

		connectionPrivate->rls = CFMachPortCreateRunLoopSource(NULL, connectionPrivate->notify_port, 0);
		connectionPrivate->rlList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}

	CFRunLoopAddSource(runLoop, connectionPrivate->rls, runLoopMode);
	_SC_schedule(connectionPrivate, runLoop, runLoopMode, connectionPrivate->rlList);

	return TRUE;
}


Boolean
SCNetworkConnectionUnscheduleFromRunLoop(SCNetworkConnectionRef		connection,
					 CFRunLoopRef			runLoop,
					 CFStringRef			runLoopMode)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	int				error			= kSCStatusFailed;
	mach_port_t			session_port;
	kern_return_t			status;

	if (!isA_SCNetworkConnection(connection) || runLoop == NULL || runLoopMode == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if ((connectionPrivate->rlList == NULL) ||
	    !_SC_unschedule(connectionPrivate, runLoop, runLoopMode, connectionPrivate->rlList, FALSE)) {
		/* if not currently scheduled */
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	session_port = __SCNetworkConnectionSessionPort(connectionPrivate);
	if (session_port == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	CFRunLoopRemoveSource(runLoop, connectionPrivate->rls, runLoopMode);

	if (CFArrayGetCount(connectionPrivate->rlList) == 0) {
		CFRelease(connectionPrivate->rls);
		connectionPrivate->rls = NULL;
		CFRelease(connectionPrivate->rlList);
		connectionPrivate->rlList = NULL;

		status = pppcontroller_notification(session_port, 0, &error);
		if ((status != KERN_SUCCESS) || (error != kSCStatusOK)) {
			_SCErrorSet(error);
			return FALSE;
		}
	}

	return TRUE;
}


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
static CFArrayRef copyKeychainEnumerator				(CFStringRef uniqueIdentifier);

static int notify_userprefs_token	= -1;

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


Boolean
SCNetworkConnectionCopyUserPreferences(CFDictionaryRef	selectionOptions,
				       CFStringRef	*serviceID,
				       CFDictionaryRef	*userOptions)
{
	int			debug		= 0;
	char			*envdebug;
	int			prefsChanged;
	SCDynamicStoreRef	session;
	Boolean			success		= FALSE;
	int			status;


	envdebug = getenv("PPPDebug");
	if (envdebug) {
		if (sscanf(envdebug, "%d", &debug) != 1)
			debug = 1; /* PPPDebug value is invalid, set debug to 1 */
	}

	if (notify_userprefs_token == -1) {
		status = notify_register_check(k_NetworkConnect_Notification, &notify_userprefs_token);
		if (status != NOTIFY_STATUS_OK)
			notify_userprefs_token = -1;
		else
			// clear the flag
			notify_check(notify_userprefs_token, &prefsChanged);
	}

	prefsChanged = 1;
	if (notify_userprefs_token != -1)
		notify_check(notify_userprefs_token, &prefsChanged);


	*serviceID = NULL;
	*userOptions = NULL;

	session	= SCDynamicStoreCreate(NULL, CFSTR("SCNetworkConnection"), NULL, NULL);
	if (session == NULL) {
		fprintf(stderr, "Error,  SCNetworkConnectionCopyUserPreferences, SCDynamicStoreCreate() returned NULL!\n");
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

		hostName = CFDictionaryGetValue(selectionOptions, kSCPropNetPPPOnDemandHostName);
		if (!isA_CFString(hostName))
			hostName = NULL;

		// can't select anything
		if (hostName == NULL)
			goto done_selection;

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
					CFArrayRef	autoDomains;
					CFIndex		domainIndex;
					CFIndex		domainsCount;

					/* we found an conditional connection enabled configuration */

					/* check domain */
					autoDomains = CFDictionaryGetValue(pppConfig, kSCPropNetPPPOnDemandDomains);
					if (!isA_CFArray(autoDomains))
						continue;

					domainsCount = CFArrayGetCount(autoDomains);
					for (domainIndex = 0; domainIndex < domainsCount; domainIndex++) {
						CFStringRef	domain;

						domain = CFArrayGetValueAtIndex(autoDomains, domainIndex);
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
							CFDictionarySetValue((CFMutableDictionaryRef)*userOptions, kSCPropNetPPPOnDemandHostName, hostName);
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
			CFDictionarySetValue((CFMutableDictionaryRef)*userOptions, kSCPropNetPPPOnDemandHostName, hostName);
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
				fprintf(stderr, "Error, userServices are not of type CFArray!\n");
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
// (1) Start by looking at the last service used in Internet Connect
// (2) If Internet Connect has not been used, find the PPP service with the highest ordering
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

//********************************************************************************
// copyPasswordFromKeychain
// --------------------------------------
// Given a uniqueID, retrieve the password from the keychain
//********************************************************************************
static CFStringRef
copyPasswordFromKeychain(CFStringRef uniqueID)
{
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
}

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
