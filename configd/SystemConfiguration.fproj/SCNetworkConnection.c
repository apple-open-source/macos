/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
 * December 20, 2002		Christophe Allie <callie@apple.com>
 * - initial revision
 */

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>

#include <Security/Security.h>
#include "dy_framework.h"

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>

#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>

#include "ppp.h"


/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

typedef struct {

	/* base CFType information */
	CFRuntimeBase	cfBase;

	/* service ID */
	CFStringRef	serviceID;		/* serviceID */

	int		eventRef;		/* ref to PPP controller for event messages */
	CFSocketRef	eventRefCF;		/* ref to PPP controller for event messages */
	int		controlRef;		/* ref to PPP controller for control messages */
	//u_int32_t	status;			/* current status of the connection */
	//char		ifname[IFNAMSIZ];	/* ppp interface used for this connection */

	/* run loop source, callout, context, rl scheduling info */
	CFRunLoopSourceRef		rls;
	SCNetworkConnectionCallBack	rlsFunction;
	SCNetworkConnectionContext	rlsContext;
	CFMutableArrayRef		rlList;

} SCNetworkConnectionPrivate, *SCNetworkConnectionPrivateRef;

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

static __inline__ CFTypeRef
isA_SCNetworkConnection(CFTypeRef obj)
{
	return (isA_CFType(obj, SCNetworkConnectionGetTypeID()));
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

static CFStringRef
__SCNetworkConnectionCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef			allocator = CFGetAllocator(cf);
	CFMutableStringRef		result;
	SCNetworkConnectionPrivateRef	connectionPrivate = (SCNetworkConnectionPrivateRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCNetworkConnection, %p [%p]> {\n"), cf, allocator);
	CFStringAppendFormat(result, NULL, CFSTR("   serviceID = %@ \n"), connectionPrivate->serviceID);
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */
static void
__SCNetworkConnectionDeallocate(CFTypeRef cf)
{
	SCNetworkConnectionPrivateRef	connectionPrivate = (SCNetworkConnectionPrivateRef)cf;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("__SCNetworkConnectionDeallocate:"));

	/* release resources */
	if (connectionPrivate->eventRef != -1) {
		while (CFArrayGetCount(connectionPrivate->rlList)) {
			CFRunLoopRef	runLoop;
			CFStringRef		runLoopMode;

			runLoop = (CFRunLoopRef)CFArrayGetValueAtIndex(connectionPrivate->rlList, 1);
			runLoopMode = CFArrayGetValueAtIndex(connectionPrivate->rlList, 2);
			CFRunLoopRemoveSource(runLoop, connectionPrivate->rls, runLoopMode);

			CFArrayRemoveValueAtIndex(connectionPrivate->rlList, 2);
			CFArrayRemoveValueAtIndex(connectionPrivate->rlList, 1);
			CFArrayRemoveValueAtIndex(connectionPrivate->rlList, 0);
		}
		CFRelease(connectionPrivate->rls);
		CFRelease(connectionPrivate->rlList);
		//PPPDispose(connectionPrivate->eventRef);
		CFSocketInvalidate(connectionPrivate->eventRefCF);
		CFRelease(connectionPrivate->eventRefCF);
	}
	if (connectionPrivate->controlRef != -1)
		PPPDispose(connectionPrivate->controlRef);
	if (connectionPrivate->rlsContext.release)
		connectionPrivate->rlsContext.release(connectionPrivate->rlsContext.info);
	if (connectionPrivate->serviceID)
		CFRelease(connectionPrivate->serviceID);

	return;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

static void
__SCNetworkConnectionInitialize(void)
{
	__kSCNetworkConnectionTypeID = _CFRuntimeRegisterClass(&__SCNetworkConnectionClass);
	return;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */
static SCNetworkConnectionPrivateRef
__SCNetworkConnectionCreatePrivate(CFAllocatorRef allocator, CFStringRef serviceID)
{
	SCNetworkConnectionPrivateRef	connectionPrivate = 0;
	uint32_t			size;
	struct ppp_status		*stats = 0;
	int				error = kSCStatusFailed;

	/* initialize runtime */
	pthread_once(&initialized, __SCNetworkConnectionInitialize);

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("__SCNetworkConnectionCreatePrivate:"));

	/* allocate NetworkConnection */
	size = sizeof(SCNetworkConnectionPrivate) - sizeof(CFRuntimeBase);
	connectionPrivate = (SCNetworkConnectionPrivateRef)_CFRuntimeCreateInstance(allocator, __kSCNetworkConnectionTypeID,size, NULL);
	if (connectionPrivate == 0)
		goto fail;

	/* zero the data structure */
	bzero(((u_char*)connectionPrivate)+sizeof(CFRuntimeBase), size);

	/* save the serviceID */
	connectionPrivate->serviceID = CFStringCreateCopy(NULL, serviceID);

	connectionPrivate->controlRef = -1;
	connectionPrivate->eventRef = -1;

	if (PPPInit(&connectionPrivate->controlRef))
		goto fail;

	if (PPPStatus(connectionPrivate->controlRef, serviceID, 0, &stats)) {
		error = kSCStatusInvalidArgument;	// XXX can't get status, invalid service id
		goto fail;
	}

	CFAllocatorDeallocate(NULL, stats);
	stats = 0;

	/* success, return the connection reference */
	return connectionPrivate;

    fail:

	/* failure, clean up and leave */
	if (connectionPrivate)
		CFRelease(connectionPrivate);
	if (stats)
		CFAllocatorDeallocate(NULL, stats);
	_SCErrorSet(error);
	return NULL;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

CFTypeID
SCNetworkConnectionGetTypeID (void) {
	pthread_once(&initialized, __SCNetworkConnectionInitialize);	/* initialize runtime */
	return __kSCNetworkConnectionTypeID;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */
static SCNetworkConnectionStatus
__SCNetworkConnectionConvertStatus (int state)
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
		case PPP_HOLDOFF:
			status = kSCNetworkConnectionDisconnecting;
			break;
		case PPP_RUNNING:
		case PPP_ONHOLD:
			status = kSCNetworkConnectionConnected;
			break;
		case PPP_IDLE:
		case PPP_STATERESERVED:
		default:
			status = kSCNetworkConnectionDisconnected;
	}
	return status;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

SCNetworkConnectionRef
SCNetworkConnectionCreateWithServiceID (CFAllocatorRef			allocator,
					CFStringRef			serviceID,
					SCNetworkConnectionCallBack	callout,
					SCNetworkConnectionContext	*context)
{
	SCNetworkConnectionPrivateRef	connectionPrivate;

	if (!isA_CFString(serviceID)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	connectionPrivate = __SCNetworkConnectionCreatePrivate(allocator, serviceID);

	if (connectionPrivate) {
		connectionPrivate->rlsFunction = callout;
		if (context) {
			bcopy(context, &connectionPrivate->rlsContext, sizeof(SCNetworkConnectionContext));
			if (context->retain) {
				connectionPrivate->rlsContext.info = (void *)context->retain(context->info);
			}
		}
	}

	return (SCNetworkConnectionRef)connectionPrivate;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

CFStringRef
SCNetworkConnectionCopyServiceID (SCNetworkConnectionRef connection)
{
	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	return CFRetain(((SCNetworkConnectionPrivateRef)connection)->serviceID);
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

CFDictionaryRef
SCNetworkConnectionCopyStatistics (SCNetworkConnectionRef connection)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	int				error			= kSCStatusFailed;
	struct ppp_status		*stats			= 0;
	CFMutableDictionaryRef		dict			= 0;
	CFMutableDictionaryRef		statsdict		= 0;

#define ADDNUMBER(d, k, n)					\
{								\
	CFNumberRef num;					\
	num = CFNumberCreate(NULL, kCFNumberSInt32Type, n);	\
	if (num) {						\
		CFDictionaryAddValue(d, k, num);		\
		CFRelease(num);					\
	}							\
}

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	/* get status and check connected state */
	if (PPPStatus(connectionPrivate->controlRef, connectionPrivate->serviceID, 0, &stats))
		goto fail;

	if (__SCNetworkConnectionConvertStatus(stats->status) != kSCNetworkConnectionConnected)
		goto fail;

	/* create dictionaries */
	if ((statsdict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
		goto fail;

	if ((dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
		goto fail;

	/* add statistics */
	ADDNUMBER(dict, kSCNetworkConnectionBytesIn, &stats->s.run.inBytes);
	ADDNUMBER(dict, kSCNetworkConnectionBytesOut, &stats->s.run.outBytes);
	ADDNUMBER(dict, kSCNetworkConnectionPacketsIn, &stats->s.run.inPackets);
	ADDNUMBER(dict, kSCNetworkConnectionPacketsOut, &stats->s.run.outPackets);
	ADDNUMBER(dict, kSCNetworkConnectionErrorsIn, &stats->s.run.inErrors);
	ADDNUMBER(dict, kSCNetworkConnectionErrorsOut, &stats->s.run.outErrors);

	/* add the PPP dictionary to the statistics dictionary */
	CFDictionaryAddValue(statsdict, kSCEntNetPPP, dict);
	CFRelease(dict);

	/* done */
	CFAllocatorDeallocate(NULL, stats);
	return statsdict;

    fail:

	if (stats)
		CFAllocatorDeallocate(NULL, stats);
	if (dict)
		CFRelease(dict);
	if (statsdict)
		CFRelease(statsdict);
	_SCErrorSet(error);
	return NULL;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

SCNetworkConnectionStatus
SCNetworkConnectionGetStatus (SCNetworkConnectionRef connection)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	struct ppp_status		*stats			= 0;
	SCNetworkConnectionStatus	status;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (PPPStatus(connectionPrivate->controlRef, connectionPrivate->serviceID, 0, &stats))
		return kSCNetworkConnectionDisconnected; // XXX

	status = __SCNetworkConnectionConvertStatus(stats->status);

	CFAllocatorDeallocate(NULL, stats);
	return status;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */
CFDictionaryRef
SCNetworkConnectionCopyExtendedStatus (SCNetworkConnectionRef connection)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	CFPropertyListRef		status			= 0;
	void				*data			= 0;
	u_int32_t			datalen;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (PPPExtendedStatus(connectionPrivate->controlRef, connectionPrivate->serviceID, 0, &data, &datalen))
		goto fail;

	if (!data
	    || !(status = PPPUnserialize(data, datalen))
	    || !isA_CFDictionary(status))
		goto fail;

	CFAllocatorDeallocate(NULL, data);
	return status;

    fail:

	_SCErrorSet(kSCStatusFailed);
	if (status)
		CFRelease(status);
	if (data)
		CFAllocatorDeallocate(NULL, data);
	return NULL;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

Boolean
SCNetworkConnectionStart (SCNetworkConnectionRef	connection,
			  CFDictionaryRef		userOptions,
			  Boolean			linger)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	CFDataRef			dataref			= 0;
	void				*data			= 0;
	u_int32_t			datalen			= 0;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (userOptions && !(dataref = PPPSerialize(userOptions, &data, &datalen)))
		goto fail;

	if (PPPConnect(connectionPrivate->controlRef, connectionPrivate->serviceID, 0, data, datalen, linger))
		goto fail;

	if (dataref)
		CFRelease(dataref);

	/* connection is now started */
	return TRUE;

    fail:

	if (dataref)
		CFRelease(dataref);
	_SCErrorSet(kSCStatusFailed);	// XXX
	return FALSE;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

Boolean
SCNetworkConnectionStop (SCNetworkConnectionRef	connection,
			 Boolean		forceDisconnect)
{
	SCNetworkConnectionPrivateRef	connectionPrivate = (SCNetworkConnectionPrivateRef)connection;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (PPPDisconnect(connectionPrivate->controlRef, connectionPrivate->serviceID, 0, forceDisconnect)) {
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	/* connection is now disconnecting */
	return TRUE;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

Boolean
SCNetworkConnectionSuspend (SCNetworkConnectionRef connection)
{
	SCNetworkConnectionPrivateRef	connectionPrivate = (SCNetworkConnectionPrivateRef)connection;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (PPPSuspend(connectionPrivate->controlRef, connectionPrivate->serviceID, 0)) {
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	/* connection is now suspended */
	return TRUE;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

Boolean
SCNetworkConnectionResume (SCNetworkConnectionRef connection)
{
	SCNetworkConnectionPrivateRef	connectionPrivate = (SCNetworkConnectionPrivateRef)connection;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (PPPResume(connectionPrivate->controlRef, connectionPrivate->serviceID, 0)) {
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	/* connection is now resume */
	return TRUE;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

CFDictionaryRef
SCNetworkConnectionCopyUserOptions (SCNetworkConnectionRef connection)
{
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	void				*data			= 0;
	u_int32_t			datalen;
	CFPropertyListRef		userOptions		= 0;

	if (!isA_SCNetworkConnection(connection)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (PPPGetConnectData(connectionPrivate->controlRef, connectionPrivate->serviceID, 0, &data, &datalen))
		goto fail;

	// no data were used, return an empty dictionary
	if (data == 0) {
		CFDictionaryRef dict;

		dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (dict == 0)
			_SCErrorSet(kSCStatusFailed); // XXX
		return dict;
	}

	userOptions = PPPUnserialize(data, datalen);
	if (!isA_CFDictionary(userOptions))
		goto fail;

	CFAllocatorDeallocate(NULL, data);
	return userOptions;

    fail:

	_SCErrorSet(kSCStatusFailed);
	if (userOptions)
		CFRelease(userOptions);
	if (data)
		CFAllocatorDeallocate(NULL, data);
	return NULL;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

static Boolean
__isScheduled(CFTypeRef obj, CFRunLoopRef runLoop, CFStringRef runLoopMode, CFMutableArrayRef rlList)
{
	CFIndex	i;
	CFIndex	n	= CFArrayGetCount(rlList);

	for (i = 0; i < n; i += 3) {
		if (obj         && !CFEqual(obj,         CFArrayGetValueAtIndex(rlList, i))) {
			continue;
		}
		if (runLoop     && !CFEqual(runLoop,     CFArrayGetValueAtIndex(rlList, i+1))) {
			continue;
		}
		if (runLoopMode && !CFEqual(runLoopMode, CFArrayGetValueAtIndex(rlList, i+2))) {
			continue;
		}
		return TRUE;
	}

	return FALSE;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */
static void
__schedule(CFTypeRef obj, CFRunLoopRef runLoop, CFStringRef runLoopMode, CFMutableArrayRef rlList)
{
	CFArrayAppendValue(rlList, obj);
	CFArrayAppendValue(rlList, runLoop);
	CFArrayAppendValue(rlList, runLoopMode);

	return;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */
static Boolean
__unschedule(CFTypeRef obj, CFRunLoopRef runLoop, CFStringRef runLoopMode, CFMutableArrayRef rlList, Boolean all)
{
	CFIndex	i	= 0;
	Boolean	found	= FALSE;
	CFIndex	n	= CFArrayGetCount(rlList);

	while (i < n) {
		if (obj         && !CFEqual(obj,         CFArrayGetValueAtIndex(rlList, i))) {
			i += 3;
			continue;
		}
		if (runLoop     && !CFEqual(runLoop,     CFArrayGetValueAtIndex(rlList, i+1))) {
			i += 3;
			continue;
		}
		if (runLoopMode && !CFEqual(runLoopMode, CFArrayGetValueAtIndex(rlList, i+2))) {
			i += 3;
			continue;
		}

		found = TRUE;

		CFArrayRemoveValueAtIndex(rlList, i + 2);
		CFArrayRemoveValueAtIndex(rlList, i + 1);
		CFArrayRemoveValueAtIndex(rlList, i);

		if (!all) {
			return found;
		}

		n -= 3;
	}

	return found;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

static void
__SCNetworkConnectionCallBack(CFSocketRef		inref,
			      CFSocketCallBackType	type,
			      CFDataRef			address,
			      const void		*data,
			      void			*info)
{
	void				*context_info;
	void				(*context_release)(const void *);
	SCNetworkConnectionRef		connection		= (SCNetworkConnectionRef)info;
	SCNetworkConnectionPrivateRef	connectionPrivate	= (SCNetworkConnectionPrivateRef)connection;
	SCNetworkConnectionCallBack	rlsFunction;
	SCNetworkConnectionStatus	status;
	int				pppstatus;
	int				err;

	err = PPPReadEvent(connectionPrivate->eventRef, &pppstatus);
	if (err)
		return;

	rlsFunction = connectionPrivate->rlsFunction;
	if (connectionPrivate->rlsContext.retain && connectionPrivate->rlsContext.info) {
		context_info	= (void *)connectionPrivate->rlsContext.retain(connectionPrivate->rlsContext.info);
		context_release	= connectionPrivate->rlsContext.release;
	}
	else {
		context_info	= connectionPrivate->rlsContext.info;
		context_release	= NULL;
	}

	status = __SCNetworkConnectionConvertStatus(pppstatus);

	(*rlsFunction)(connection, status, context_info);
	if (context_release && context_info) {
		context_release(context_info);
	}
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

Boolean
SCNetworkConnectionScheduleWithRunLoop(SCNetworkConnectionRef	connection,
				       CFRunLoopRef		runLoop,
				       CFStringRef		runLoopMode)
{
	SCNetworkConnectionPrivateRef	connectionPrivate = (SCNetworkConnectionPrivateRef)connection;
	//CFSocketRef			ref;

	if (!isA_SCNetworkConnection(connection) || runLoop == NULL || runLoopMode == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (connectionPrivate->rlsFunction == 0) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (connectionPrivate->rlList
	    && __isScheduled(NULL, runLoop, runLoopMode, connectionPrivate->rlList)) {
		/* already scheduled */
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	if (connectionPrivate->eventRef == -1) {
		CFSocketContext	context = { 0, (void*)connection, CFRetain, CFRelease, CFCopyDescription };

		if (PPPInit(&connectionPrivate->eventRef)) {
			_SCErrorSet(kSCStatusFailed);
			return FALSE;
		}

		PPPEnableEvents(connectionPrivate->eventRef, connectionPrivate->serviceID, 0, 1);

		connectionPrivate->eventRefCF = CFSocketCreateWithNative(NULL, connectionPrivate->eventRef,
									 kCFSocketReadCallBack, __SCNetworkConnectionCallBack, &context);
		connectionPrivate->rls = CFSocketCreateRunLoopSource(NULL, connectionPrivate->eventRefCF, 0);
		connectionPrivate->rlList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

		//CFRelease(ref);
	}

	CFRunLoopAddSource(runLoop, connectionPrivate->rls, runLoopMode);
	__schedule(connectionPrivate, runLoop, runLoopMode, connectionPrivate->rlList);

	return TRUE;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */

Boolean
SCNetworkConnectionUnscheduleFromRunLoop(SCNetworkConnectionRef		connection,
					 CFRunLoopRef			runLoop,
					 CFStringRef			runLoopMode)
{
	SCNetworkConnectionPrivateRef	connectionPrivate = (SCNetworkConnectionPrivateRef)connection;

	if (!isA_SCNetworkConnection(connection) || runLoop == NULL || runLoopMode == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (connectionPrivate->rlList == NULL
	    || !__unschedule(connectionPrivate, runLoop, runLoopMode, connectionPrivate->rlList, FALSE)) {
		/* if not currently scheduled */
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	CFRunLoopRemoveSource(runLoop, connectionPrivate->rls, runLoopMode);

	if (CFArrayGetCount(connectionPrivate->rlList) == 0) {
		CFRelease(connectionPrivate->rls);
		connectionPrivate->rls = NULL;
		CFRelease(connectionPrivate->rlList);
		connectionPrivate->rlList = NULL;
		//PPPDispose(connectionPrivate->eventRef);
		CFSocketInvalidate(connectionPrivate->eventRefCF);
		CFRelease(connectionPrivate->eventRefCF);
		connectionPrivate->eventRefCF = 0;
		connectionPrivate->eventRef = -1;
	}

	return TRUE;
}


//************************* USER LEVEL DIAL API **********************************


#define k_NetworkConnect_Pref_File	CFSTR("com.apple.networkConnect")
#define k_InterentConnect_Pref_File	CFSTR("com.apple.internetconnect")

#define k_Dial_Default_Key		CFSTR("ConnectByDefault") // needs to go into SC
#define k_Last_Service_Id_Key		CFSTR("ServiceID")
#define k_Unique_Id_Key	 		CFSTR("UniqueIdentifier")


/* Private Prototypes */
static Boolean SCNetworkConnectionPrivateCopyDefaultServiceIDForDial	(SCDynamicStoreRef session, CFStringRef *serviceID);
static Boolean SCNetworkConnectionPrivateGetPPPServiceFromDynamicStore	(SCDynamicStoreRef session, CFStringRef *serviceID);
static Boolean SCNetworkConnectionPrivateCopyDefaultUserOptionsFromArray(CFArrayRef userOptionsArray, CFDictionaryRef *userOptions);
static Boolean SCNetworkConnectionPrivateIsPPPService			(SCDynamicStoreRef session, CFStringRef serviceID);
static void addPasswordFromKeychain(CFDictionaryRef *userOptions);
static CFArrayRef copyKeychainEnumerator(CFStringRef uniqueIdentifier);

Boolean
SCNetworkConnectionCopyUserPreferences (CFDictionaryRef	selectionOptions,
					CFStringRef	*serviceID,
					CFDictionaryRef	*userOptions)
{
	SCDynamicStoreRef	session	= SCDynamicStoreCreate(NULL, CFSTR("SCNetworkConnection"), NULL, NULL);
	Boolean			success	= FALSE;

	// NOTE:  we are currently ignoring selectionOptions

	if (session != NULL) {
		// (1) Figure out which service ID we care about, allocate it into passed "serviceID"
		success = SCNetworkConnectionPrivateCopyDefaultServiceIDForDial(session, serviceID);

		if (success && (*serviceID != NULL)) {
			// (2) Get the list of user data for this service ID
			CFPropertyListRef userServices = CFPreferencesCopyValue(*serviceID,
										k_NetworkConnect_Pref_File,
										kCFPreferencesCurrentUser,
										kCFPreferencesCurrentHost);

			// (3) We are expecting an array if the user has defined records for this service ID or NULL if the user hasn't
			if (userServices != NULL) {
				if (isA_CFArray(userServices)) {
					// (4) Get the default set of user options for this service
					success =  SCNetworkConnectionPrivateCopyDefaultUserOptionsFromArray(
									  (CFArrayRef)userServices,
									userOptions);
					if(success && userOptions != NULL)
					{
					    addPasswordFromKeychain(userOptions);
					}
				} else {
					fprintf(stderr, "Error, userServices are not of type CFArray!\n");
				}

				CFRelease(userServices); // this is OK because SCNetworkConnectionPrivateISExpectedCFType() checks for NULL
			}
		}

		CFRelease(session);
	} else {
		fprintf(stderr, "Error,  SCNetworkConnectionCopyUserPreferences, SCDynamicStoreCreate() returned NULL!\n");
	}

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

	// NULL out the pointer
	*serviceID = NULL;

	// read out the last service from the Internet Connect preference file
	lastServiceSelectedInIC = CFPreferencesCopyValue(k_Last_Service_Id_Key,
							 k_InterentConnect_Pref_File,
							 kCFPreferencesCurrentUser,
							 kCFPreferencesAnyHost);

	// we found the service the user last had open in IC
	if (lastServiceSelectedInIC != NULL) {
		// make sure its a PPP service
		if (SCNetworkConnectionPrivateIsPPPService(session, lastServiceSelectedInIC)) {
			// make sure the service that we found is valid
			CFDictionaryRef	dict;
			CFStringRef	key;

			key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
									  kSCDynamicStoreDomainSetup,
									  lastServiceSelectedInIC,
									  kSCEntNetInterface);
			dict = SCDynamicStoreCopyValue(session, key);
			CFRelease(key);
			if (dict) {
				*serviceID = CFRetain(lastServiceSelectedInIC);
				foundService = TRUE;
				CFRelease(dict);
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
	Boolean		success		= FALSE;
	CFStringRef	key		= NULL;
	CFDictionaryRef	dict		= NULL;
	CFArrayRef	serviceIDs	= NULL;

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
		if (dict == NULL) {
			fprintf(stderr, "Error, Dictionary for setup key == NULL!\n");
			break;
		}

		serviceIDs = CFDictionaryGetValue(dict, kSCPropNetServiceOrder); // array of service id's
		if (isA_CFArray(serviceIDs) == NULL) {
			if (serviceIDs == NULL)
				fprintf(stderr, "Error, Array of service IDs == NULL!\n");
			else
				fprintf(stderr, "Error, serviceIds are not of type CFArray!\n");
			break;
		}

		count = CFArrayGetCount(serviceIDs);
		for (i = 0; i < count; i++) {
			CFStringRef service = CFArrayGetValueAtIndex(serviceIDs, i);

			if (SCNetworkConnectionPrivateIsPPPService(session, service)) {
				*serviceID = CFRetain(service);
				success = TRUE;
				break;
			}
		}
	} while (FALSE);

	if (key != NULL)
		CFRelease(key);

	if (dict != NULL)
		CFRelease(dict);

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

	*userOptions = NULL;

	for (i = 0; i < count; i++) {
		// (1) Find the dictionary
		CFPropertyListRef propertyList = CFArrayGetValueAtIndex(userOptionsArray, i);

		if (isA_CFDictionary(propertyList) != NULL) {
			// See if there's a value for dial on demand
			CFPropertyListRef value = CFDictionaryGetValue((CFDictionaryRef)propertyList,
								       k_Dial_Default_Key);
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
// SCNetworkConnectionPrivateIsPPPService
// --------------------------------------
// Check and see if the service is a PPP service
//********************************************************************************
static Boolean
SCNetworkConnectionPrivateIsPPPService(SCDynamicStoreRef session, CFStringRef serviceID)
{
	CFStringRef	entityKey;
	Boolean		isPPPService	= FALSE;
	Boolean		isModemOrPPPoE	= FALSE;

	entityKey = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								kSCDynamicStoreDomainSetup,
								serviceID,
								kSCEntNetInterface);
	if (entityKey != NULL) {
		CFDictionaryRef	serviceDict;

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
					isModemOrPPPoE = (CFEqual(subtype, kSCValNetInterfaceSubTypePPPSerial) ||
							  CFEqual(subtype, kSCValNetInterfaceSubTypePPPoE));
				}
			}
			CFRelease(serviceDict);
		}
		CFRelease(entityKey);
	}

	return (isPPPService && isModemOrPPPoE);
}

//********************************************************************************
// addPasswordFromKeychain
// --------------------------------------
// Get the password out of the keychain and add it to the PPP dictionary
//********************************************************************************
static void
addPasswordFromKeychain(CFDictionaryRef *userOptions)
{
	CFArrayRef		enumerator;
	CFIndex			n;
	CFDictionaryRef		oldDict;
	CFPropertyListRef	uniqueID	= NULL;

	oldDict = *userOptions;
	if(oldDict == NULL) {
		return;		// if no userOptions
	}

	uniqueID = CFDictionaryGetValue(oldDict, k_Unique_Id_Key);
	if(!isA_CFString(uniqueID)) {
		return;		// if no unique ID
	}

	enumerator = copyKeychainEnumerator(uniqueID);
	if(enumerator == NULL) {
		return;		// if no keychain enumerator
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
		if(result == noErr && data != NULL && dataLen > 0) {
			CFStringRef		pass;

			pass = CFStringCreateWithBytes(NULL, data, dataLen, kCFStringEncodingUTF8, TRUE);
			if (pass) {
				CFMutableDictionaryRef	newDict;
				CFMutableDictionaryRef	newPPP;
				CFDictionaryRef		pppDict;

				newDict = CFDictionaryCreateMutableCopy(NULL, 0, oldDict);
				pppDict = CFDictionaryGetValue(newDict, kSCEntNetPPP);
				if (isA_CFDictionary(pppDict)) {
					newPPP = CFDictionaryCreateMutableCopy(NULL, 0, pppDict);
				} else {
					newPPP = CFDictionaryCreateMutable(NULL,
									   0,
									   &kCFTypeDictionaryKeyCallBacks,
									   &kCFTypeDictionaryValueCallBacks);
				}

				// set the PPP password
				CFDictionarySetValue(newPPP, kSCPropNetPPPAuthPassword, pass);
				CFRelease(pass);

				// update the PPP entity
				CFDictionarySetValue(newDict, kSCEntNetPPP, newPPP);
				CFRelease(newPPP);

				// update the userOptions dictionary
				CFRelease(oldDict);
				*userOptions = CFDictionaryCreateCopy(NULL, newDict);
				CFRelease(newDict);
			}
		}
	}

	CFRelease(enumerator);
	return;
}

//********************************************************************************
// copyKeychainEnumerator
// --------------------------------------
// Gather Keychain Enumerator
//********************************************************************************
static CFArrayRef
copyKeychainEnumerator(CFStringRef uniqueIdentifier)
{
	char			*buf		= NULL;
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
