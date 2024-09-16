/*
 * Copyright (c) 2000-2024 Apple Inc. All rights reserved.
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
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <TargetConditionals.h>
#include <stdlib.h>
#include <unistd.h>
#include <os/state_private.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <bootstrap_priv.h>

#define SCD_HAS_USER_LOOKUP (!TARGET_OS_OSX && (!TARGET_OS_SIMULATOR || TARGET_OS_MACCATALYST))
#if SCD_HAS_USER_LOOKUP
#include <xpc/private.h>
#endif // SCD_HAS_USER_LOOKUP

#include "SCDynamicStoreInternal.h"
#include "SCInternal.h"
#include "config.h"		/* MiG generated file */
#include "SCD.h"

#define	N_SESSIONS_WARN_DEFAULT	50	// complain if SCDynamicStore session count exceeds this [soft-]limit
#define	N_SESSIONS_WARN_MAX	5000	// stop complaining when # sessions exceeds this [hard-]limit

static mach_port_t	_sc_server;						// sync w/storeQueue()
static CFIndex		_sc_store_advise;					// when snapshots should log
static CFIndex		_sc_store_max;
static CFMutableSetRef	_sc_store_sessions;					// sync w/storeQueue()

static dispatch_queue_t
storeQueue(void)
{
	static dispatch_once_t	once;
	static dispatch_queue_t	q;

	dispatch_once(&once, ^{
		// allocate mapping between [client] sessions and session info
		_sc_store_sessions = CFSetCreateMutable(NULL, 0, NULL);

		// and a queue to synchronize access to the mapping
		q = dispatch_queue_create("SCDynamicStore/client sessions", NULL);
	});

	return q;
}

static const char	*notifyType[] = {
	"",
	"wait",
	"inform w/callback",
	"inform w/mach port",
	"inform w/fd",
	"inform w/runLoop",
	"inform w/dispatch"
};


static SCDynamicStorePrivateRef
__SCDynamicStoreCreateInternal(CFAllocatorRef		allocator,
			       const CFStringRef	name,
			       SCDynamicStoreCallBack	callout,
			       SCDynamicStoreContext	*context,
			       Boolean			is_client);

#pragma mark -
#pragma mark SCDynamicStore state handler


static void
addSessionReference(const void *value, void *context)
{
	CFMutableDictionaryRef		dict		= (CFMutableDictionaryRef)context;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)value;

	if (storePrivate->name != NULL) {
		int			cnt;
		CFMutableStringRef	key;
		CFIndex			n;
		CFNumberRef		num;

		// create [key] signature
		key = CFStringCreateMutableCopy(NULL, 0, storePrivate->name);
		n = (storePrivate->keys != NULL) ? CFArrayGetCount(storePrivate->keys) : 0;
		if (n > 0) {
			CFStringAppendFormat(key, NULL, CFSTR(":k[0/%ld]=%@"),
					     n,
					     CFArrayGetValueAtIndex(storePrivate->keys, 0));
		}
		n = (storePrivate->patterns != NULL) ? CFArrayGetCount(storePrivate->patterns) : 0;
		if (n > 0) {
			CFStringAppendFormat(key, NULL, CFSTR(":p[0/%ld]=%@"),
					     n,
					     CFArrayGetValueAtIndex(storePrivate->patterns, 0));
		}

		// bump [key] count
		if (!CFDictionaryGetValueIfPresent(dict,
						   key,
						   (const void **)&num) ||
		    !CFNumberGetValue(num, kCFNumberIntType, &cnt)) {
			// if first session
			cnt = 0;
		}
		cnt++;
		num = CFNumberCreate(NULL, kCFNumberIntType, &cnt);
		CFDictionarySetValue(dict, key, num);	// do we want to include name+keys[0]/patterns[0] ?
		CFRelease(num);

		CFRelease(key);
	}

	return;
}

static inline CFIndex
get_sc_store_advise(void)
{
	return (_sc_store_advise != 0) ? _sc_store_advise : (N_SESSIONS_WARN_DEFAULT / 2);
}

static inline CFIndex
get_sc_store_max(void)
{
	return (_sc_store_max != 0) ? _sc_store_max : N_SESSIONS_WARN_DEFAULT;
}

static void
add_state_handler(void)
{
	os_state_block_t	state_block;

	state_block = ^os_state_data_t(os_state_hints_t hints) {
#pragma unused(hints)
		CFDataRef		data	= NULL;
		CFMutableDictionaryRef	dict;
		CFIndex			n;
		Boolean			ok;
		os_state_data_t		state_data;
		size_t			state_data_size;
		CFIndex			state_len;
		CFIndex 		store_max 	= get_sc_store_max();
		CFIndex 		store_advise 	= get_sc_store_advise();

		n = CFSetGetCount(_sc_store_sessions);
		if ((store_max < 0) || (n == 0) || (n < store_advise)) {
			return NULL;
		}

		dict = CFDictionaryCreateMutable(NULL,
						 0,
						 &kCFTypeDictionaryKeyCallBacks,
						 &kCFTypeDictionaryValueCallBacks);
		CFSetApplyFunction(_sc_store_sessions, addSessionReference, dict);
		if (CFDictionaryGetCount(dict) == 0) {
			CFRelease(dict);
			return NULL;
		}
		ok = _SCSerialize(dict, &data, NULL, NULL);
		CFRelease(dict);

		state_len = (ok && (data != NULL)) ? CFDataGetLength(data) : 0;
		state_data_size = OS_STATE_DATA_SIZE_NEEDED(state_len);
		if (state_data_size > MAX_STATEDUMP_SIZE) {
			SC_log(LOG_ERR, "SCDynamicStore/client sessions : state data too large (%zu > %zu)",
			       state_data_size,
			       (size_t)MAX_STATEDUMP_SIZE);
			if (data != NULL) CFRelease(data);
			return NULL;
		}

		state_data = calloc(1, state_data_size);
		if (state_data == NULL) {
			SC_log(LOG_ERR, "SCDynamicStore/client sessions: could not allocate state data");
			if (data != NULL) CFRelease(data);
			return NULL;
		}

		state_data->osd_type = OS_STATE_DATA_SERIALIZED_NSCF_OBJECT;
		state_data->osd_data_size = (uint32_t)state_len;
		strlcpy(state_data->osd_title, "SCDynamicStore/client sessions", sizeof(state_data->osd_title));
		if (state_len > 0) {
			memcpy(state_data->osd_data, CFDataGetBytePtr(data), state_len);
		}
		if (data != NULL) CFRelease(data);

		return state_data;
	};

	(void) os_state_add_handler(storeQueue(), state_block);
	return;
}


#pragma mark -
#pragma mark SCDynamicStore APIs


void
_SCDynamicStoreSetSessionWatchLimit(unsigned int limit)
{
	_sc_store_max = limit;
	_sc_store_advise = limit;
	return;
}


__private_extern__ os_log_t
__log_SCDynamicStore(void)
{
	static os_log_t	log	= NULL;

	if (log == NULL) {
		log = os_log_create("com.apple.SystemConfiguration", "SCDynamicStore");
	}

	return log;
}


static CFStringRef
__SCDynamicStoreCopyDescription(CFTypeRef cf) {
	CFAllocatorRef			allocator	= CFGetAllocator(cf);
	CFMutableStringRef		result;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCDynamicStore %p [%p]> {"), cf, allocator);
	if (storePrivate->server != MACH_PORT_NULL) {
		CFStringAppendFormat(result, NULL, CFSTR("server port = 0x%x"), storePrivate->server);
	} else {
		CFStringAppendFormat(result, NULL, CFSTR("server not (no longer) available"));
	}
	if (storePrivate->disconnectFunction != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", disconnect = %p"), storePrivate->disconnectFunction);
	}
	switch (storePrivate->notifyStatus) {
		case Using_NotifierWait :
			CFStringAppendFormat(result, NULL, CFSTR(", waiting for a notification"));
			break;
		case Using_NotifierInformViaMachPort :
			CFStringAppendFormat(result, NULL, CFSTR(", mach port notifications"));
			break;
		case Using_NotifierInformViaFD :
			CFStringAppendFormat(result, NULL, CFSTR(", FD notifications"));
			break;
		case Using_NotifierInformViaRunLoop :
			CFStringAppendFormat(result, NULL, CFSTR(", runloop notifications"));
			CFStringAppendFormat(result, NULL, CFSTR(" {callout = %p"), storePrivate->rlsFunction);
			CFStringAppendFormat(result, NULL, CFSTR(", info = %p"), storePrivate->rlsContext.info);
			CFStringAppendFormat(result, NULL, CFSTR(", rls = %p"), storePrivate->rls);
			CFStringAppendFormat(result, NULL, CFSTR(", notify rls = %@" ), storePrivate->rlsNotifyRLS);
			CFStringAppendFormat(result, NULL, CFSTR("}"));
			break;
		case Using_NotifierInformViaDispatch :
			CFStringAppendFormat(result, NULL, CFSTR(", dispatch notifications"));
			CFStringAppendFormat(result, NULL, CFSTR(" {callout = %p"), storePrivate->rlsFunction);
			CFStringAppendFormat(result, NULL, CFSTR(", info = %p"), storePrivate->rlsContext.info);
			CFStringAppendFormat(result, NULL, CFSTR(", queue = %p"), storePrivate->dispatchQueue);
			CFStringAppendFormat(result, NULL, CFSTR(", source = %p"), storePrivate->dispatchSource);
			CFStringAppendFormat(result, NULL, CFSTR("}"));
			break;
		default :
			CFStringAppendFormat(result, NULL, CFSTR(", notification delivery not requested%s"),
					     storePrivate->rlsFunction ? " (yet)" : "");
			break;
	}
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}


static void
__SCDynamicStoreDeallocate(CFTypeRef cf)
{
	int				oldThreadState;
	SCDynamicStoreRef		store		= (SCDynamicStoreRef)cf;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	(void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldThreadState);

	/*
	 * When using SCDynamicStoreSetDispatchQueue(), the SCDynamicStore object will
	 * ALWAYS have a reference while notifications are active.  This reference is
	 * held by storePrivate->dispatchSource.  If we get here with an active
	 * notification then we have been over-released.
	 */
	if (storePrivate->dispatchSource != NULL) {
		_SC_crash("SCDynamicStore OVER-RELEASED (notification still active)", NULL, NULL);
	}
	if (storePrivate->queue != NULL) {
		/* remove client session from session tracking set */
		dispatch_sync(storeQueue(), ^{
				CFSetRemoveValue(_sc_store_sessions,
						 storePrivate);
			});
	}

	/* Remove/cancel any outstanding notification requests. */
	(void) SCDynamicStoreNotifyCancel(store);

	if (storePrivate->server != MACH_PORT_NULL) {
		/*
		 * Remove our send right to the SCDynamicStore server.
		 *
		 * In the case of a "real" session this will result in our
		 * session being closed.
		 *
		 * In the case of a "NULL" session, we just remove the
		 * the send right reference we are holding.
		 */
		__MACH_PORT_DEBUG(TRUE, "*** __SCDynamicStoreDeallocate", storePrivate->server);
		(void) mach_port_deallocate(mach_task_self(), storePrivate->server);
		storePrivate->server = MACH_PORT_NULL;
	}

	(void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldThreadState);
	pthread_testcancel();

	/* release any callback context info */
	if (storePrivate->rlsContext.release != NULL) {
		(*storePrivate->rlsContext.release)(storePrivate->rlsContext.info);
	}

	/* release any keys being watched */
	if (storePrivate->keys != NULL) CFRelease(storePrivate->keys);
	if (storePrivate->patterns != NULL) CFRelease(storePrivate->patterns);

	/* release any client info */
	if (storePrivate->name != NULL) CFRelease(storePrivate->name);
	if (storePrivate->options != NULL) CFRelease(storePrivate->options);

	/* release any cached content */
	if (storePrivate->cache_active) {
		_SCDynamicStoreCacheClose(store);
	}

	if (storePrivate->queue != NULL) {
		dispatch_release(storePrivate->queue);
	}

	return;
}


static CFTypeID __kSCDynamicStoreTypeID;


static const CFRuntimeClass __SCDynamicStoreClass = {
	0,				// version
	"SCDynamicStore",		// className
	NULL,				// init
	NULL,				// copy
	__SCDynamicStoreDeallocate,	// dealloc
	NULL,				// equal
	NULL,				// hash
	NULL,				// copyFormattingDesc
	__SCDynamicStoreCopyDescription,// copyDebugDesc
#ifdef CF_RECLAIM_AVAILABLE
	NULL,
#endif
#ifdef CF_REFCOUNT_AVAILABLE
	NULL
#endif
};


static void
childForkHandler(void)
{
	/* the process has forked (and we are the child process) */

	_sc_server = MACH_PORT_NULL;
	return;
}


static void
__SCDynamicStoreInitialize(void)
{
	static dispatch_once_t  initialized;

	dispatch_once(&initialized, ^{
		/* register with CoreFoundation */
		__kSCDynamicStoreTypeID = _CFRuntimeRegisterClass(&__SCDynamicStoreClass);

		/* add handler to cleanup after fork() */
		(void) pthread_atfork(NULL, NULL, childForkHandler);

		add_state_handler();	
	});
	
	return;
}


static Boolean
get_sysctl_val(const char * name, int * val_p)
{
        int     val;
        size_t  val_size = sizeof(val);

        if (sysctlbyname(name, &val, &val_size, NULL, 0) != 0) {
                SC_log(LOG_NOTICE,
                       "sysctl %s failed, %s",
                       name, strerror(errno));
                return (FALSE);
        }
        *val_p = val;
        SC_log(LOG_INFO, "sysctl %s %d", name, val);
        return (TRUE);
}


#define KERN_WILLSHUTDOWN               "kern.willshutdown"
#define KERN_WILLUSERSPACEREBOOT        "kern.willuserspacereboot"


static Boolean
should_try_to_reconnect(void)
{
        int     val = 0;

        if (get_sysctl_val(KERN_WILLSHUTDOWN, &val) && val != 0) {
                return (FALSE);
        }
        if (get_sysctl_val(KERN_WILLUSERSPACEREBOOT, &val) && val != 0) {
                return (FALSE);
        }
        return (TRUE);
}

#define	MAX_UNKNOWN_SERVICE_RETRY	3

// Note: call when [dispatch] sync'd to storeQueue()
static mach_port_t
__SCDynamicStoreServerPort(SCDynamicStorePrivateRef storePrivate, kern_return_t *status, Boolean retry)
{
#pragma unused(storePrivate)
	mach_port_t	server	= MACH_PORT_NULL;
	char		*server_name;
	int		try	= 0;

	server_name = getenv("SCD_SERVER");

#ifndef	DEBUG
	/*
	 * only allow the SCDynamicStore server bootstrap name to be changed with
	 * DEBUG builds.  For RELEASE builds, assume that no server is available.
	 */
	if (server_name != NULL) {
		*status = BOOTSTRAP_UNKNOWN_SERVICE;
		return MACH_PORT_NULL;
	}
#endif	/* DEBUG */


	if (server_name == NULL) {
		server_name = SCD_SERVER;
	}

    again :

#if	SCD_HAS_USER_LOOKUP
	if (xpc_user_sessions_enabled()) {
		errno_t error = 0;
		uid_t target_user = xpc_user_sessions_get_foreground_uid(&error);
		if (target_user == KAUTH_UID_NONE) {
			SC_log(LOG_ERR, "xpc_user_sessions_get_foreground_uid() failed with error '%d'", error);
			*status = BOOTSTRAP_UNKNOWN_SERVICE;
		} else {
			*status = bootstrap_look_up_per_user(bootstrap_port, server_name, target_user, &server);
		}
		goto status_switch;
	}
#endif	// SCD_HAS_USER_LOOKUP

#if	defined(BOOTSTRAP_PRIVILEGED_SERVER) && !TARGET_OS_SIMULATOR
	*status = bootstrap_look_up2(bootstrap_port,
				     server_name,
				     &server,
				     0,
				     BOOTSTRAP_PRIVILEGED_SERVER);
#ifdef	DEBUG
	if ((*status != BOOTSTRAP_SUCCESS)
	    && (strcmp(server_name, SCD_SERVER) != 0)
	    ) {
		*status = bootstrap_look_up(bootstrap_port, server_name, &server);
	}
#endif	/* DEBUG */
#else	// defined(BOOTSTRAP_PRIVILEGED_SERVER) && !TARGET_OS_SIMULATOR
	*status = bootstrap_look_up(bootstrap_port, server_name, &server);
#endif	// defined(BOOTSTRAP_PRIVILEGED_SERVER) && !TARGET_OS_SIMULATOR

#if	SCD_HAS_USER_LOOKUP
status_switch:
#endif	// SCD_HAS_USER_LOOKUP

	switch (*status) {
		case BOOTSTRAP_SUCCESS :
			/* service currently registered, "a good thing" (tm) */
			if (try > 0) {
				SC_log(LOG_INFO,
				       "SCDynamicStore service found after %d retr%s",
				       try,
				       (try == 1) ? "y" : "ies");
			}
			return server;
		case BOOTSTRAP_NOT_PRIVILEGED :
			/* the service is not privileged */
			break;
		case BOOTSTRAP_UNKNOWN_SERVICE :
			/* service not currently registered, try again later */
			if (!should_try_to_reconnect()) {
				SC_log(LOG_INFO, "SCDynamicStore no longer available, skipping reconnect");
				break;
			}
			if (retry) {
				try++;
				SC_log((try <= MAX_UNKNOWN_SERVICE_RETRY) ? LOG_INFO : LOG_ERR,
				       "No SCDynamicStore service() (try %d): %s",
				       try,
				       bootstrap_strerror(*status));
				if (try <= MAX_UNKNOWN_SERVICE_RETRY) {
					usleep(50 * 1000);	// sleep 50ms between attempts
					goto again;
				}
			}
			break;
		default :
#ifdef	DEBUG
			SC_log(LOG_INFO, "bootstrap_look_up() failed: status=%s (%d)",
			       bootstrap_strerror(*status),
			       *status);
#endif	/* DEBUG */
			break;
	}

	return MACH_PORT_NULL;
}


static void
logSessionReference(const void *key, const void *value, void *context)
{
#pragma unused(context)
	CFNumberRef	cnt	= (CFNumberRef)value;
	CFStringRef	name	= (CFStringRef)key;

	SC_log(LOG_ERR, "  %@ sessions w/name = \"%@\"", cnt, name);
	return;
}


SCDynamicStorePrivateRef
__SCDynamicStoreCreatePrivate(CFAllocatorRef		allocator,
			      const CFStringRef		name,
			      SCDynamicStoreCallBack	callout,
			      SCDynamicStoreContext	*context)
{

	return __SCDynamicStoreCreateInternal(allocator, name, callout,
					      context, FALSE);
}

static SCDynamicStorePrivateRef
__SCDynamicStoreCreateInternal(CFAllocatorRef		allocator,
			       const CFStringRef	name,
			       SCDynamicStoreCallBack	callout,
			       SCDynamicStoreContext	*context,
			       Boolean			is_client)
{
	uint32_t			size;
	SCDynamicStorePrivateRef	storePrivate;

	/* initialize runtime */
	__SCDynamicStoreInitialize();

	/* allocate session */
	size  = sizeof(SCDynamicStorePrivate) - sizeof(CFRuntimeBase);
	storePrivate = (SCDynamicStorePrivateRef)_CFRuntimeCreateInstance(allocator,
									  __kSCDynamicStoreTypeID,
									  size,
									  NULL);
	if (storePrivate == NULL) {
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	/* initialize non-zero/NULL members */

	/* dispatch queue protecting ->server, ... */
	if (is_client) {
		storePrivate->queue = dispatch_queue_create("SCDynamicStore object", NULL);
	}

	/* client side of the "configd" session */
	storePrivate->name				= (name != NULL) ? CFRetain(name) : NULL;

	/* Notification status */
	storePrivate->notifyStatus			= NotifierNotRegistered;

	/* "client" information associated with SCDynamicStoreCreateRunLoopSource() */
	storePrivate->rlsFunction			= callout;
	if (context != NULL) {
		memcpy(&storePrivate->rlsContext, context, sizeof(SCDynamicStoreContext));
		if (context->retain != NULL) {
			storePrivate->rlsContext.info = (void *)(*context->retain)(context->info);
		}
	}

	/* "server" information associated with SCDynamicStoreNotifyFileDescriptor(); */
	storePrivate->notifyFile			= -1;

	if (is_client) {
		__block Boolean		tooManySessions	= FALSE;

		/* watch for excessive SCDynamicStore usage */
		dispatch_sync(storeQueue(), ^{
			CFIndex		n;

			// track the session
			CFSetAddValue(_sc_store_sessions, storePrivate);
			n = CFSetGetCount(_sc_store_sessions);
			if (n > _sc_store_max) {
				if (_sc_store_max > 0) {
					CFMutableDictionaryRef	dict;

					dict = CFDictionaryCreateMutable(NULL,
									 0,
									 &kCFTypeDictionaryKeyCallBacks,
									 &kCFTypeDictionaryValueCallBacks);
					CFSetApplyFunction(_sc_store_sessions, addSessionReference, dict);
					SC_log(LOG_ERR,
					       "SCDynamicStoreCreate(): number of SCDynamicStore sessions %sexceeds %ld",
					       (n != N_SESSIONS_WARN_DEFAULT) ? "now " : "",
					       _sc_store_max);
					CFDictionaryApplyFunction(dict, logSessionReference, NULL);
					CFRelease(dict);

					// yes, we should complain
					tooManySessions = TRUE;

					// bump the threshold before we complain again
					_sc_store_max = (_sc_store_max < N_SESSIONS_WARN_MAX) ? (_sc_store_max * 2) : 0;
				}
			}
		});

		if (tooManySessions) {
			_SC_crash_once("Excessive number of SCDynamicStore sessions", NULL, NULL);
		}
	}

	return storePrivate;
}

static void
updateServerPort(SCDynamicStorePrivateRef storePrivate, mach_port_t *server, int *sc_status_p)
{
	__block mach_port_t	old_port;

	dispatch_sync(storeQueue(), ^{
		old_port = _sc_server;
		if (_sc_server != MACH_PORT_NULL) {
			if (*server == _sc_server) {
				// if the server we tried returned the error, save the old port,
				// [re-]lookup the name to the server, and deallocate the original
				// send [or dead name] right
				_sc_server = __SCDynamicStoreServerPort(storePrivate, sc_status_p, TRUE);
				(void)mach_port_deallocate(mach_task_self(), old_port);
			} else {
				// another thread has refreshed the [main] SCDynamicStore server port
			}
		} else {
			_sc_server = __SCDynamicStoreServerPort(storePrivate, sc_status_p, FALSE);
		}

		*server = _sc_server;
	});

#ifdef	DEBUG
	SC_log(LOG_DEBUG, "updateServerPort (%@): 0x%x (%u) --> 0x%x (%u)",
	       (storePrivate->name != NULL) ? storePrivate->name : CFSTR("?"),
	       old_port, old_port,
	       *server, *server);
#endif	// DEBUG

	return;
}


// Note: call when [dispatch] sync'd to storePrivate->queue (or otherwise safe)
static Boolean
__SCDynamicStoreAddSession(SCDynamicStorePrivateRef storePrivate)
{
	kern_return_t		kr		= KERN_SUCCESS;
	CFDataRef		myName;			/* serialized name */
	xmlData_t		myNameRef;
	CFIndex			myNameLen;
	CFDataRef		myOptions	= NULL;	/* serialized options */
	xmlData_t		myOptionsRef	= NULL;
	CFIndex			myOptionsLen	= 0;
	__block int		sc_status	= kSCStatusFailed;
	__block mach_port_t	server;

	if (!_SCSerializeString(storePrivate->name, &myName, &myNameRef, &myNameLen)) {
		goto done;
	}

	/* serialize the options */
	if (storePrivate->options != NULL) {
		if (!_SCSerialize(storePrivate->options, &myOptions, &myOptionsRef, &myOptionsLen)) {
			CFRelease(myName);
			goto done;
		}
	}

	/* open a new session with the server */
	server = MACH_PORT_NULL;


	updateServerPort(storePrivate, &server, &sc_status);


	while (server != MACH_PORT_NULL) {
		// if SCDynamicStore server available
		kr = configopen(server,
				myNameRef,
				(mach_msg_type_number_t)myNameLen,
				myOptionsRef,
				(mach_msg_type_number_t)myOptionsLen,
				&storePrivate->server,
				(int *)&sc_status);
		if (kr == KERN_SUCCESS) {
			break;
		}

		// our [cached] server port is not valid
		if ((kr != MACH_SEND_INVALID_DEST) && (kr != MIG_SERVER_DIED)) {
			// if we got an unexpected error, don't retry
			sc_status = kr;
			break;
		}


		updateServerPort(storePrivate, &server, &sc_status);
	}
	__MACH_PORT_DEBUG((storePrivate->server != MACH_PORT_NULL), "*** SCDynamicStoreAddSession", storePrivate->server);

	// clean up
	CFRelease(myName);
	if (myOptions != NULL)	CFRelease(myOptions);

    done :

	switch (sc_status) {
		case kSCStatusOK :
			return TRUE;
		case BOOTSTRAP_UNKNOWN_SERVICE :
			SC_log((kr == KERN_SUCCESS) ? LOG_INFO : LOG_ERR, "SCDynamicStore server not available");
			sc_status = kSCStatusNoStoreServer;
			break;
		default :
			SC_log((kr == KERN_SUCCESS) ? LOG_INFO : LOG_ERR, "configopen() failed: %d: %s",
			       sc_status,
			       SCErrorString(sc_status));
			break;
	}

	_SCErrorSet(sc_status);
	return FALSE;
}

static SCDynamicStorePrivateRef	S_null_session;

static SCDynamicStoreRef
__SCDynamicStoreNullSession(void)
{
	dispatch_block_t	once_block;
	static dispatch_once_t  initialized;

	once_block = ^{
		S_null_session = (SCDynamicStorePrivateRef)
			SCDynamicStoreCreate(NULL,
					     CFSTR("null"),
					     NULL,
					     NULL);
	};
	dispatch_once(&initialized, once_block);
	return (S_null_session);
}


__private_extern__
Boolean
__SCDynamicStoreNormalize(SCDynamicStoreRef *store, Boolean allowNullSession)
{
	SCDynamicStorePrivateRef	storePrivate;

	assert(store != NULL);

	if ((*store == NULL) && allowNullSession) {
		*store = __SCDynamicStoreNullSession();
		if (*store == NULL) {
			return FALSE;
		}
	}

	if (!isA_SCDynamicStore(*store)) {
		_SCErrorSet(kSCStatusNoStoreSession);
		return FALSE;
	}

	storePrivate = (SCDynamicStorePrivateRef)*store;
	if (storePrivate->server == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusNoStoreServer);
		return FALSE;
	}

	return TRUE;
}


static Boolean
__SCDynamicStoreReconnect(SCDynamicStoreRef store)
{
	__block Boolean			ok;
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	dispatch_sync(storePrivate->queue, ^{
		ok = __SCDynamicStoreAddSession(storePrivate);
	});
	return ok;
}


__private_extern__
Boolean
__SCDynamicStoreCheckRetryAndHandleError(SCDynamicStoreRef	store,
					 kern_return_t		status,
					 int			*sc_status,
					 const char		*log_str)
{
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	if (status == KERN_SUCCESS) {
		/* no error */
		return FALSE;
	}

	switch (status) {
		case MACH_SEND_INVALID_DEST :
		case MIG_SERVER_DIED :
			/*
			 * the server's gone, remove the session's send (or dead name) right
			 */
			dispatch_sync(storePrivate->queue, ^{
#ifdef	DEBUG
				SC_log(LOG_DEBUG, "__SCDynamicStoreCheckRetryAndHandleError(%s): %@: 0x%x (%u) --> 0x%x (%d)",
				       log_str,
				       (storePrivate->name != NULL) ? storePrivate->name : CFSTR("?"),
				       storePrivate->server, storePrivate->server,
				       (unsigned int)MACH_PORT_NULL, MACH_PORT_NULL);
#endif	// DEBUG
				if (storePrivate->server != MACH_PORT_NULL) {
					(void) mach_port_deallocate(mach_task_self(), storePrivate->server);
					storePrivate->server = MACH_PORT_NULL;
				}
			});

			/* reconnect */
			if (__SCDynamicStoreReconnect(store)) {
				/* retry needed */
				return TRUE;
			} else {
				status = SCError();
			}

			break;

		default :
			break;
	}

	if (status == kSCStatusNoStoreServer) {
		/*
		 * the server is not (or no longer) available and, at this
		 * point, there is no need to retry nor complain
		 */
		goto done;
	}

	/*
	 * an unexpected error, leave the [session] port alone
	 *
	 * Note: we are abandoning our usage of this port
	 */

#ifdef	DEBUG
	SC_log(LOG_DEBUG, "__SCDynamicStoreCheckRetryAndHandleError(%s): %@: unexpected status=%s (%d)",
	       log_str,
	       (storePrivate->name != NULL) ? storePrivate->name : CFSTR("?"),
	       SCErrorString(status),
	       status);
#endif	// DEBUG

	SC_log(LOG_NOTICE, "%s: %s (%d)", log_str, SCErrorString(status), status);
	storePrivate->server = MACH_PORT_NULL;

	{
		char		*crash_info;
		CFStringRef	err;

		err = CFStringCreateWithFormat(NULL,
					       NULL,
					       CFSTR("CheckRetryAndHandleError \"%s\" failed: %s (%d)"),
					       log_str,
					       SCErrorString(status),
					       status);
		crash_info = _SC_cfstring_to_cstring(err, NULL, 0, kCFStringEncodingASCII);
		CFRelease(err);

		err = CFStringCreateWithFormat(NULL,
					       NULL,
					       CFSTR("A SCDynamicStore error has been detected by \"%s\"."),
					       getprogname());
		_SC_crash(crash_info, CFSTR("CheckRetryAndHandleError"), err);
		CFAllocatorDeallocate(NULL, crash_info);
		CFRelease(err);
	}

    done :

	*sc_status = status;
	return FALSE;
}


static void
pushDisconnect(SCDynamicStoreRef store)
{
	void					*context_info;
	void					(*context_release)(const void *);
	SCDynamicStoreDisconnectCallBack	disconnectFunction;
	SCDynamicStorePrivateRef		storePrivate	= (SCDynamicStorePrivateRef)store;

	disconnectFunction = storePrivate->disconnectFunction;
	if (disconnectFunction == NULL) {
		// if no reconnect callout, push empty notification
		storePrivate->disconnectForceCallBack = TRUE;
		return;
	}

	if (storePrivate->rlsContext.retain != NULL) {
		context_info	= (void *)storePrivate->rlsContext.retain(storePrivate->rlsContext.info);
		context_release	= storePrivate->rlsContext.release;
	} else {
		context_info	= storePrivate->rlsContext.info;
		context_release	= NULL;
	}
	SC_log(LOG_DEBUG, "exec SCDynamicStore disconnect callout");
	(*disconnectFunction)(store, context_info);
	if (context_release) {
		context_release(context_info);
	}

	return;
}


__private_extern__
Boolean
__SCDynamicStoreReconnectNotifications(SCDynamicStoreRef store)
{
	dispatch_queue_t			dispatchQueue	= NULL;
	__SCDynamicStoreNotificationStatus	notifyStatus;
	Boolean					ok		= TRUE;
	CFArrayRef				rlList		= NULL;
	int					sc_status;
	SCDynamicStorePrivateRef		storePrivate	= (SCDynamicStorePrivateRef)store;

#ifdef	DEBUG
	SC_log(LOG_DEBUG, "SCDynamicStore: reconnect notifications (%@)",
	       (storePrivate->name != NULL) ? storePrivate->name : CFSTR("?"));
#endif	// DEBUG

	// save old SCDynamicStore [notification] state
	notifyStatus = storePrivate->notifyStatus;

	// before tearing down our [old] notifications, make sure we've
	// retained any information that will be lost when we cancel the
	// current no-longer-valid handler
	switch (notifyStatus) {
		case Using_NotifierInformViaRunLoop :
			if (storePrivate->rlList != NULL) {
				rlList = CFArrayCreateCopy(NULL, storePrivate->rlList);
			}
			break;
		case Using_NotifierInformViaDispatch :
			dispatchQueue = storePrivate->dispatchQueue;
			if (dispatchQueue != NULL) dispatch_retain(dispatchQueue);
			break;
		default :
			break;
	}

	// cancel [old] notifications
	if (!SCDynamicStoreNotifyCancel(store)) {
		// if we could not cancel / reconnect
		SC_log(LOG_NOTICE, "SCDynamicStoreNotifyCancel() failed: %s", SCErrorString(SCError()));
	}

	// set notification keys & patterns
	if ((storePrivate->keys != NULL) || (storePrivate->patterns)) {
		ok = SCDynamicStoreSetNotificationKeys(store,
						       storePrivate->keys,
						       storePrivate->patterns);
		if (!ok) {
			sc_status = SCError();
			if (sc_status != BOOTSTRAP_UNKNOWN_SERVICE) {
				SC_log(LOG_NOTICE,
				       "SCDynamicStoreSetNotificationKeys() failed: %s",
				       SCErrorString(sc_status));
			}
			goto done;
		}
	}

	switch (notifyStatus) {
		case Using_NotifierInformViaRunLoop : {
			CFIndex			i;
			CFIndex			n;
			CFRunLoopSourceRef	rls;

#ifdef	DEBUG
			SC_log(LOG_DEBUG, "SCDynamicStore: reconnecting w/CFRunLoop (%@)",
			       (storePrivate->name != NULL) ? storePrivate->name : CFSTR("?"));
#endif	// DEBUG

			rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
			if (rls == NULL) {
				sc_status = SCError();
				if (sc_status != BOOTSTRAP_UNKNOWN_SERVICE) {
					SC_log(LOG_NOTICE,
					       "SCDynamicStoreCreateRunLoopSource() failed: %s",
					       SCErrorString(sc_status));
				}
				ok = FALSE;
				break;
			}

			n = (rlList != NULL) ? CFArrayGetCount(rlList) : 0;
			for (i = 0; i < n; i += 3) {
				CFRunLoopRef	rl	= (CFRunLoopRef)CFArrayGetValueAtIndex(rlList, i+1);
				CFStringRef	rlMode	= (CFStringRef) CFArrayGetValueAtIndex(rlList, i+2);

				CFRunLoopAddSource(rl, rls, rlMode);
			}

			CFRelease(rls);
			break;
		}
		case Using_NotifierInformViaDispatch :

#ifdef	DEBUG
			SC_log(LOG_DEBUG, "SCDynamicStore: reconnecting w/dispatch queue (%@)",
			       (storePrivate->name != NULL) ? storePrivate->name : CFSTR("?"));
#endif	// DEBUG

			ok = SCDynamicStoreSetDispatchQueue(store, dispatchQueue);
			if (!ok) {
				sc_status = SCError();
				if (sc_status != BOOTSTRAP_UNKNOWN_SERVICE) {
					SC_log(LOG_NOTICE, "SCDynamicStoreSetDispatchQueue() failed: %s",
					       SCErrorString(sc_status));
				}
				goto done;
			}
			break;

		default :
			_SCErrorSet(kSCStatusFailed);
			ok = FALSE;
			break;
	}

    done :

	// cleanup
	switch (notifyStatus) {
		case Using_NotifierInformViaRunLoop :
			if (rlList != NULL) CFRelease(rlList);
			break;
		case Using_NotifierInformViaDispatch :
			if (dispatchQueue != NULL) dispatch_release(dispatchQueue);
			break;
		default :
			break;
	}

	if (!ok) {
		sc_status = SCError();
		SC_log(LOG_NOTICE, "SCDynamicStore server %s, notification (%s) not restored",
		       (sc_status == BOOTSTRAP_UNKNOWN_SERVICE) ? "shutdown" : "failed",
		       notifyType[notifyStatus]);
	}

	// inform the client
	pushDisconnect(store);

	return ok;
}


const CFStringRef	kSCDynamicStoreUseSessionKeys	= CFSTR("UseSessionKeys");	/* CFBoolean */



SCDynamicStoreRef
SCDynamicStoreCreateWithOptions(CFAllocatorRef		allocator,
				CFStringRef		name,
				CFDictionaryRef		storeOptions,
				SCDynamicStoreCallBack	callout,
				SCDynamicStoreContext	*context)
{
	Boolean				ok;
	SCDynamicStorePrivateRef	storePrivate;

	// allocate and initialize a new session
	storePrivate = __SCDynamicStoreCreateInternal(allocator, NULL, callout, context, TRUE);
	if (storePrivate == NULL) {
		return NULL;
	}

	// set "name"
	storePrivate->name = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@:%@"), _SC_getApplicationBundleID(), name);

	// set "options"

	if (storeOptions != NULL) {
		storePrivate->options = CFRetain(storeOptions);
	}

	// establish SCDynamicStore session
	ok = __SCDynamicStoreAddSession(storePrivate);
	if (!ok) {
		CFRelease(storePrivate);
		storePrivate = NULL;
	}

	return (SCDynamicStoreRef)storePrivate;
}


SCDynamicStoreRef
SCDynamicStoreCreate(CFAllocatorRef		allocator,
		     CFStringRef		name,
		     SCDynamicStoreCallBack	callout,
		     SCDynamicStoreContext	*context)
{
	return SCDynamicStoreCreateWithOptions(allocator, name, NULL, callout, context);
}


CFTypeID
SCDynamicStoreGetTypeID(void) {
	__SCDynamicStoreInitialize();	/* initialize runtime */
	return __kSCDynamicStoreTypeID;
}


Boolean
SCDynamicStoreSetDisconnectCallBack(SCDynamicStoreRef			store,
				    SCDynamicStoreDisconnectCallBack	callout)
{
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;

	if (!isA_SCDynamicStore(store)) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoStoreSession);
		return FALSE;
	}

	storePrivate->disconnectFunction = callout;
	return TRUE;
}

__private_extern__
int
__SCDynamicStoreMapInternalStatus(int sc_status, Boolean generate_fault)
{
	switch (sc_status) {
	case kSCStatusAccessError_MissingReadEntitlement:
		sc_status = kSCStatusAccessError;
		if (!generate_fault) {
			break;
		}
		_SC_crash_once("SCDynamicStore read denied: "
			       "missing entitlement",
			       NULL, NULL);
		break;
	case kSCStatusAccessError_MissingWriteEntitlement:
		sc_status = kSCStatusAccessError;
		if (!generate_fault) {
			break;
		}
		_SC_crash_once("SCDynamicStore write denied : "
			       "missing entitlement",
			       NULL, NULL);
		break;
	case kSCStatusOK_MissingReadEntitlement:
		sc_status = kSCStatusOK;
		if (!generate_fault) {
			break;
		}
		_SC_crash_once("SCDynamicStore read allowed: "
			       "missing entitlement",
			       NULL, NULL);
		break;
	default:
		break;
	}
	return (sc_status);
}
