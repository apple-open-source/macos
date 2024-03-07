/*
 * Copyright (c) 2000-2023 Apple Inc. All rights reserved.
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

#include <SystemConfiguration/SystemConfiguration.h>
#include "SCInternal.h"
#include "configd.h"
#include "configd_server.h"
#include "pattern.h"
#include "session.h"

#include <unistd.h>
#include <bsm/libbsm.h>
#include <os/state_private.h>
#include <sandbox.h>
#include <System/kern/cs_blobs.h>
#include <libproc.h>
#include <mach/mach_init.h>
#include <mach/task.h>

/* information maintained for the main listener */
static serverSessionRef		server_session		= NULL;

/*
 * information maintained for each active session
 * Note: sync w/sessionQueue()
 */
static CFMutableDictionaryRef	client_sessions		= NULL;
static CFIndex			client_sessions_advise	= 250;		// when snapshot handler should detail sessions


static dispatch_queue_t
sessionQueue(void)
{
	static dispatch_once_t	once;
	static dispatch_queue_t	q;

	dispatch_once(&once, ^{
		// allocate mapping between [client] session mach port and session info
		client_sessions = CFDictionaryCreateMutable(NULL,
							    0,
							    NULL,	// use the actual mach_port_t as the key
							    &kCFTypeDictionaryValueCallBacks);

		// and a queue to synchronize access to the mapping
		q = dispatch_queue_create("SCDynamicStore/sessions", NULL);
	});

	return q;
}


#pragma mark -
#pragma mark __serverSession object

static CFStringRef		__serverSessionCopyDescription	(CFTypeRef cf);
static void			__serverSessionDeallocate	(CFTypeRef cf);

static const CFRuntimeClass	__serverSessionClass = {
	0,					// version
	"serverSession",			// className
	NULL,					// init
	NULL,					// copy
	__serverSessionDeallocate,		// dealloc
	NULL,					// equal
	NULL,					// hash
	NULL,					// copyFormattingDesc
	__serverSessionCopyDescription	// copyDebugDesc
};

static CFTypeID	__serverSessionTypeID;


static CFStringRef
__serverSessionCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef		allocator	= CFGetAllocator(cf);
	CFMutableStringRef	result;
	serverSessionRef	session		= (serverSessionRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<serverSession %p [%p]> {"), cf, allocator);

	// add client port
	CFStringAppendFormat(result, NULL, CFSTR("port = 0x%x (%d)"), session->key, session->key);

	// add session info
	if (session->name != NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", name = %@"), session->name);
	}

	CFStringAppendFormat(result, NULL, CFSTR("}"));
	return result;
}


static void
__serverSessionDeallocate(CFTypeRef cf)
{
#pragma unused(cf)
	serverSessionRef	session		= (serverSessionRef)cf;

	if (session->changedKeys != NULL)	CFRelease(session->changedKeys);
	if (session->name != NULL)		CFRelease(session->name);
	if (session->sessionKeys != NULL)	CFRelease(session->sessionKeys);

	return;
}


static serverSessionRef
__serverSessionCreate(CFAllocatorRef allocator, mach_port_t server)
{
	static dispatch_once_t	once;
	serverSessionRef	session;
	uint32_t		size;

	// initialize runtime
	dispatch_once(&once, ^{
		__serverSessionTypeID = _CFRuntimeRegisterClass(&__serverSessionClass);
	});

	// allocate session
	size    = sizeof(serverSession) - sizeof(CFRuntimeBase);
	session = (serverSessionRef)_CFRuntimeCreateInstance(allocator,
							     __serverSessionTypeID,
							     size,
							     NULL);
	if (session == NULL) {
		return NULL;
	}

	// if needed, allocate a mach port for SCDynamicStore client
	if (server == MACH_PORT_NULL) {
		kern_return_t		kr;
		mach_port_t		mp	= MACH_PORT_NULL;
		mach_port_options_t	opts;

	    retry_allocate :

		memset(&opts, 0, sizeof(opts));
		opts.flags = MPO_CONTEXT_AS_GUARD;

		kr = mach_port_construct(mach_task_self(), &opts, (mach_port_context_t)session, &mp);
		if (kr != KERN_SUCCESS) {
			char	*err	= NULL;

			SC_log(LOG_NOTICE, "could not allocate mach port: %s", mach_error_string(kr));
			if ((kr == KERN_NO_SPACE) || (kr == KERN_RESOURCE_SHORTAGE)) {
				sleep(1);
				goto retry_allocate;
			}

			(void) asprintf(&err, "Could not allocate mach port: %s", mach_error_string(kr));
			_SC_crash(err != NULL ? err : "Could not allocate new session (mach) port",
				  NULL,
				  NULL);
			if (err != NULL) free(err);
			CFRelease(session);
			return NULL;
		}

		// insert send right that will be moved to the client
		kr = mach_port_insert_right(mach_task_self(),
					    mp,
					    mp,
					    MACH_MSG_TYPE_MAKE_SEND);
		if (kr != KERN_SUCCESS) {
			/*
			 * We can't insert a send right into our own port!  This should
			 * only happen if someone stomped on OUR port (so let's leave
			 * the port alone).
			 */
			SC_log(LOG_ERR, "mach_port_insert_right() failed: %s", mach_error_string(kr));
			CFRelease(session);
			return NULL;
		}

		server = mp;
	}

	session->callerEUID		= 1;		/* not "root" */
	session->entitlements		= NULL;		/* all entitlements are UNKNOWN */
	session->key			= server;
//	session->store			= NULL;

	return session;
}


#pragma mark -
#pragma mark SCDynamicStore state handler


static void
addSessionReference(const void *key, const void *value, void *context)
{
#pragma unused(key)
	CFMutableDictionaryRef	dict		= (CFMutableDictionaryRef)context;
	serverSessionRef	session		= (serverSessionRef)value;

	if (session->name != NULL) {
		int		cnt;
		CFNumberRef	num;

		if (!CFDictionaryGetValueIfPresent(dict,
						   session->name,
						   (const void **)&num) ||
		    !CFNumberGetValue(num, kCFNumberIntType, &cnt)) {
			// if first session
			cnt = 0;
		}
		cnt++;
		num = CFNumberCreate(NULL, kCFNumberIntType, &cnt);
		CFDictionarySetValue(dict, session->name, num);
		CFRelease(num);
	}

	return;
}


static void
add_state_handler(void)
{
	os_state_block_t	state_block;

	state_block = ^os_state_data_t(os_state_hints_t hints) {
#pragma unused(hints)
		CFDataRef		data	= NULL;
		CFIndex			n;
		Boolean			ok;
		os_state_data_t		state_data;
		size_t			state_data_size;
		CFIndex			state_len;

		n = CFDictionaryGetCount(client_sessions);
		if (n < client_sessions_advise) {
			CFStringRef	str;

			str = CFStringCreateWithFormat(NULL, NULL, CFSTR("n = %ld"), n);
			ok = _SCSerialize(str, &data, NULL, NULL);
			CFRelease(str);
		} else {
			CFMutableDictionaryRef	dict;

			dict = CFDictionaryCreateMutable(NULL,
							 0,
							 &kCFTypeDictionaryKeyCallBacks,
							 &kCFTypeDictionaryValueCallBacks);
			CFDictionaryApplyFunction(client_sessions, addSessionReference, dict);
			ok = _SCSerialize(dict, &data, NULL, NULL);
			CFRelease(dict);
		}

		state_len = (ok && (data != NULL)) ? CFDataGetLength(data) : 0;
		state_data_size = OS_STATE_DATA_SIZE_NEEDED(state_len);
		if (state_data_size > MAX_STATEDUMP_SIZE) {
			SC_log(LOG_ERR, "SCDynamicStore/sessions : state data too large (%zd > %zd)",
			       state_data_size,
			       (size_t)MAX_STATEDUMP_SIZE);
			if (data != NULL) CFRelease(data);
			return NULL;
		}

		state_data = calloc(1, state_data_size);
		if (state_data == NULL) {
			SC_log(LOG_ERR, "SCDynamicStore/sessions: could not allocate state data");
			if (data != NULL) CFRelease(data);
			return NULL;
		}

		state_data->osd_type = OS_STATE_DATA_SERIALIZED_NSCF_OBJECT;
		state_data->osd_data_size = (uint32_t)state_len;
		strlcpy(state_data->osd_title, "SCDynamicStore/sessions", sizeof(state_data->osd_title));
		if (state_len > 0) {
			memcpy(state_data->osd_data, CFDataGetBytePtr(data), state_len);
		}
		if (data != NULL) CFRelease(data);

		return state_data;
	};

	(void) os_state_add_handler(sessionQueue(), state_block);
	return;
}


#pragma mark -
#pragma mark SCDynamicStore session management


__private_extern__
serverSessionRef
getSession(mach_port_t server)
{
	__block serverSessionRef	session;

	assert(server != MACH_PORT_NULL);
	dispatch_sync(sessionQueue(), ^{
		session = (serverSessionRef)CFDictionaryGetValue(client_sessions,
								 (const void *)(uintptr_t)server);
	});

	return session;
}


__private_extern__
serverSessionRef
getSessionNum(CFNumberRef serverNum)
{
	union {
		mach_port_t	mp;
		uint64_t	val;
	} server;
	serverSessionRef	session;

	(void) CFNumberGetValue(serverNum, kCFNumberSInt64Type, &server.val);
	session = getSession(server.mp);

	return session;
}


__private_extern__
serverSessionRef
getSessionStr(CFStringRef serverKey)
{
	mach_port_t		server;
	serverSessionRef	session;
	char			str[16];

	(void) _SC_cfstring_to_cstring(serverKey, str, sizeof(str), kCFStringEncodingASCII);
	server = atoi(str);
	session = getSession(server);

	return session;
}


__private_extern__
void
addSession(serverSessionRef session, Boolean isMain)
{
	session->serverChannel = dispatch_mach_create_f("configd/SCDynamicStore",
							server_queue(),
							(void *)session,
							server_mach_channel_handler);
	if (!isMain) {
		// if not main SCDynamicStore port, watch for exit
		dispatch_mach_notify_no_senders(session->serverChannel, FALSE);
	}
#if	TARGET_OS_SIMULATOR
	// simulators don't support MiG QoS propagation yet
	dispatch_set_qos_class_fallback(session->serverChannel, QOS_CLASS_USER_INITIATED);
#else
	dispatch_set_qos_class_fallback(session->serverChannel, QOS_CLASS_BACKGROUND);
#endif
	dispatch_mach_connect(session->serverChannel, session->key, MACH_PORT_NULL, NULL);
	return;
}


__private_extern__
serverSessionRef
addClient(mach_port_t server, audit_token_t audit_token)
{

	__block serverSessionRef	newSession	= NULL;

	dispatch_sync(sessionQueue(), ^{
		Boolean		ok;

		// check to see if we already have an open session
		ok = CFDictionaryContainsKey(client_sessions,
					     (const void *)(uintptr_t)server);
		if (ok) {
			// if we've already added a session for this port
			return;
		}

		// allocate a new session for "the" server
		newSession = __serverSessionCreate(NULL, MACH_PORT_NULL);
		if (newSession != NULL) {
			// and add a port --> session mapping
			CFDictionarySetValue(client_sessions,
					     (const void *)(uintptr_t)newSession->key,
					     newSession);

			// save the audit_token in case we need to check the callers credentials
			newSession->auditToken = audit_token;
			newSession->callerEUID = audit_token_to_euid(newSession->auditToken);

			CFRelease(newSession);	// reference held by dictionary
		}
	});

	if (newSession != NULL) {
		addSession(newSession, FALSE);
	}

	return newSession;
}


__private_extern__
serverSessionRef
addServer(mach_port_t server)
{
	// allocate a session for "the" server
	server_session = __serverSessionCreate(NULL, server);
	addSession(server_session, TRUE);

	// add a state dump handler
	add_state_handler();

	return server_session;
}


__private_extern__
void
cleanupSession(serverSessionRef session)
{
	mach_port_t	server		= session->key;

	SC_trace("cleanup : %5d", server);

	/*
	 * Close any open connections including cancelling any outstanding
	 * notification requests and releasing any locks.
	 */
	__MACH_PORT_DEBUG(TRUE, "*** cleanupSession", server);
	(void) __SCDynamicStoreClose(&session->store);
	__MACH_PORT_DEBUG(TRUE, "*** cleanupSession (after __SCDynamicStoreClose)", server);

#ifdef	DEBUG_MACH_PORT_ALLOCATIONS
	Boolean	ok;

	ok = _SC_checkMachPortReceive("*** cleanupSession (check send right)", server);
	if (!ok) {
		_SC_logMachPortReferences("*** cleanupSession w/unexpected rights", server);
	}
#endif	// DEBUG_MACH_PORT_ALLOCATIONS

	/*
	 * Our send right has already been removed. Remove our receive right.
	 */
	(void) mach_port_destruct(mach_task_self(), server, 0, (mach_port_context_t)session);

	/*
	 * release any entitlement info
	 */
	if (session->entitlements != NULL) {
		CFRelease(session->entitlements);
		session->entitlements = NULL;
	}

	/*
	 * get rid of the per-session structure.
	 */
	dispatch_sync(sessionQueue(), ^{
		CFDictionaryRemoveValue(client_sessions,
					(const void *)(uintptr_t)server);
	});

	return;
}


__private_extern__
void
closeSession(serverSessionRef session)
{
	/*
	 * cancel and release the mach channel
	 */
	if (session->serverChannel != NULL) {
		dispatch_mach_cancel(session->serverChannel);
		dispatch_release(session->serverChannel);
		session->serverChannel = NULL;
	}

	return;
}


typedef struct ReportSessionInfo {
	FILE	*f;
	int	n;
} ReportSessionInfo, *ReportSessionInfoRef;

static void
printOne(const void *key, const void *value, void *context)
{
#pragma unused(key)
	ReportSessionInfoRef	reportInfo	= (ReportSessionInfoRef)context;
	serverSessionRef	session		= (serverSessionRef)value;

	SCPrint(TRUE, reportInfo->f, CFSTR("  %d : port = 0x%x"), ++reportInfo->n, session->key);
	SCPrint(TRUE, reportInfo->f, CFSTR(", name = %@"), session->name);
	if (session->changedKeys != NULL) {
		SCPrint(TRUE, reportInfo->f, CFSTR("\n    changedKeys = %@"), session->changedKeys);
	}
	if (session->sessionKeys != NULL) {
		SCPrint(TRUE, reportInfo->f, CFSTR("\n    sessionKeys = %@"), session->sessionKeys);
	}
	SCPrint(TRUE, reportInfo->f, CFSTR("\n"));
	return;
}


__private_extern__
void
listSessions(FILE *f)
{
	dispatch_sync(sessionQueue(), ^{
		ReportSessionInfo	reportInfo	= { .f = f, .n = 0 };

		SCPrint(TRUE, f, CFSTR("Current sessions :\n"));
		CFDictionaryApplyFunction(client_sessions,
					  printOne,
					  (void *)&reportInfo);
		SCPrint(TRUE, f, CFSTR("\n"));
	});
	return;
}


#include <Security/Security.h>
#include <Security/SecTask.h>

static CFTypeRef
getEntitlement(serverSessionRef session, CFStringRef entitlement)
{
	SecTaskRef	task;
	CFTypeRef	value	= NULL;

	if (session->entitlements != NULL) {
		value = CFDictionaryGetValue(session->entitlements, entitlement);
		if (value != NULL) {
			if (value == kCFNull) {
				// if we've already looked for this entitlement
				value = NULL;
			}
			return value;
		}
	}

	// Create the security task from the audit token
	task = SecTaskCreateWithAuditToken(NULL, session->auditToken);
	if (task != NULL) {
		CFErrorRef	error	= NULL;

		// Get the value for the entitlement
		value = SecTaskCopyValueForEntitlement(task, entitlement, &error);
		if ((value == NULL) && (error != NULL)) {
			CFIndex		code	= CFErrorGetCode(error);
			CFStringRef	domain	= CFErrorGetDomain(error);

			if (!CFEqual(domain, kCFErrorDomainMach) ||
			    ((code != kIOReturnInvalid) && (code != kIOReturnNotFound))) {
				// if unexpected error
				SC_log(LOG_NOTICE, "SecTaskCopyValueForEntitlement(,\"%@\",) failed, error = %@ : %@",
				       entitlement,
				       error,
				       session->name);
			}
			CFRelease(error);
		}

		if (session->entitlements == NULL) {
			session->entitlements = CFDictionaryCreateMutable(NULL, 0,
									  &kCFTypeDictionaryKeyCallBacks,
									  &kCFTypeDictionaryValueCallBacks);
		}
		if (value != NULL) {
			CFDictionarySetValue(session->entitlements, entitlement, value);
			CFRelease(value);
		} else {
			CFDictionarySetValue(session->entitlements, entitlement, kCFNull);
		}

		CFRelease(task);
	} else {
		SC_log(LOG_NOTICE, "SecTaskCreateWithAuditToken() failed: %@",
		       session->name);
	}

	return value;
}

#if _HAVE_PRIVACY_ACCOUNTING
static CFDictionaryRef
copyEntitlementsDictionary(serverSessionRef session, CFArrayRef entitlements)
{
	CFDictionaryRef	dict = NULL;
	SecTaskRef	task;

	task = SecTaskCreateWithAuditToken(NULL, session->auditToken);
	if (task != NULL) {
		CFErrorRef	error	= NULL;

		dict = SecTaskCopyValuesForEntitlements(task, entitlements,
							&error);
		if (dict == NULL && error != NULL) {
			CFIndex		code	= CFErrorGetCode(error);
			CFStringRef	domain	= CFErrorGetDomain(error);

			if (!CFEqual(domain, kCFErrorDomainMach)
			    || (code != kIOReturnInvalid
				&& code != kIOReturnNotFound)) {
				SC_log(LOG_NOTICE,
				       "SecTaskCopyValuesForEntitlements(%@) "
				       "failed, error = %@ : %@",
				       entitlements,  error, session->name);
			}
			CFRelease(error);
		}
		CFRelease(task);
	}
	return (dict);
}

static const CFStringRef kImplicitlyAssumedIdentityEntitlement =
	CFSTR("com.apple.private.attribution.implicitly-assumed-identity");

static const CFStringRef kUsageOnlyImplicitlyAssumedIdentityEntitlement =
	CFSTR("com.apple.private.attribution.usage-reporting-only.implicitly-assumed-identity");
static const CFStringRef kExplicitlyAssumedIdentitiesEntitlement =
	CFSTR("com.apple.private.attribution.explicitly-assumed-identities");

static CFArrayRef
getSystemProcessEntitlementsArray(void)
{
	static CFArrayRef	list;
	const void * 		keys[] = {
		 kImplicitlyAssumedIdentityEntitlement,
		 kUsageOnlyImplicitlyAssumedIdentityEntitlement,
		 kExplicitlyAssumedIdentitiesEntitlement
	};
	if (list != NULL) {
		return (list);
	}
	list = CFArrayCreate(NULL,
			     keys,
			     sizeof(keys)/ sizeof(keys[0]),
			     &kCFTypeArrayCallBacks);
	return (list);
}

static CFDictionaryRef
copySystemProcessEntitlementsDictionary(serverSessionRef session)
{
	return (copyEntitlementsDictionary(session,
					   getSystemProcessEntitlementsArray()));
}
#endif /* _HAVE_PRIVACY_ACCOUNTING */

static pid_t
sessionPid(serverSessionRef session)
{
	pid_t	pid;

	pid = audit_token_to_pid(session->auditToken);
	return pid;
}

static Boolean
hasBooleanEntitlement(serverSessionRef session, CFStringRef entitlementName)
{
	Boolean		allow = FALSE;
	CFBooleanRef	entitlement;

	entitlement = (CFBooleanRef)getEntitlement(session, entitlementName);
	if (isA_CFBoolean(entitlement) != NULL) {
		allow = CFBooleanGetValue(entitlement);
	}
	return (allow);
}

static Boolean
hasEntitlementForKey(serverSessionRef session, CFStringRef entitlementName,
		     CFStringRef key)
{
	CFTypeRef	entitlement;
	CFArrayRef	keys;
	CFArrayRef	patterns;

	entitlement = getEntitlement(session, entitlementName);
	if (isA_CFDictionary(entitlement) == NULL) {
		return (FALSE);
	}
	/* check for a specific entitlement matching the key */
	keys = CFDictionaryGetValue(entitlement, CFSTR("keys"));
	if (isA_CFArray(keys)) {
		if (CFArrayContainsValue(keys,
					 CFRangeMake(0, CFArrayGetCount(keys)),
					 key)) {
			/* exact match */
			return TRUE;
		}
	}
	/* check for an entitlement pattern matching the key */
	patterns = CFDictionaryGetValue(entitlement, CFSTR("patterns"));
	if (isA_CFArray(patterns)) {
		CFIndex		i;
		CFIndex		n	= CFArrayGetCount(patterns);

		for (i = 0; i < n; i++) {
			CFStringRef	pattern;

			pattern = CFArrayGetValueAtIndex(patterns, i);
			if (isA_CFString(pattern)) {
				if (patternKeyMatches(pattern, key)) {
					/* pattern match */
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}


static Boolean
isConfigd(serverSessionRef session)
{
	return (sessionPid(session) == getpid());
}


__private_extern__
Boolean
hasRootAccess(serverSessionRef session)
{
#if	!TARGET_OS_SIMULATOR

	return (session->callerEUID == 0) ? TRUE : FALSE;

#else	// !TARGET_OS_SIMULATOR
#pragma unused(session)

	/*
	 * assume that all processes interacting with
	 * the iOS Simulator "configd" are OK.
	 */
	return TRUE;

#endif	// !TARGET_OS_SIMULATOR
}

static Boolean
sessionHasEntitlement(serverSessionRef session, CFArrayRef list)
{
	for (CFIndex i = 0, count = CFArrayGetCount(list); i < count; i++) {
		CFStringRef	entitlement;

		entitlement = CFArrayGetValueAtIndex(list, i);
		if (getEntitlement(session, entitlement) != NULL) {
			return (TRUE);
		}
	}
	return (FALSE);
}

static Boolean
myCFBooleanGetValueIfSet(CFBooleanRef b, Boolean * ret_value)
{
	if (b == NULL) {
		*ret_value = FALSE;
		return (FALSE);
	}
	*ret_value = CFBooleanGetValue(b);
	return (TRUE);
}

#if _HAVE_BASUPPORT
#include <BASupport/BASupport.h>

/* "weak_import" to ensure that the compiler doesn't optimize out NULL check */
extern bool
ba_is_process_extension(audit_token_t *token)
	__attribute__((weak_import));

static Boolean
haveBASupport(void)
{
	return (ba_is_process_extension != NULL);
}

static Boolean
sessionIsBackgroundAssetExtension(serverSessionRef session)
{
	Boolean		isBAE = FALSE;

	if (haveBASupport()
	    && !myCFBooleanGetValueIfSet(session->isBackgroundAssetExtension,
				      &isBAE)) {
		isBAE = ba_is_process_extension(&session->auditToken);
		session->isBackgroundAssetExtension
			= isBAE ? kCFBooleanTrue : kCFBooleanFalse;
	}
	return (isBAE);
}

#else /* _HAVE_BASUPPORT */

static Boolean
sessionIsBackgroundAssetExtension(serverSessionRef session)
{
#pragma unused(session)
	return (FALSE);
}

#endif /* _HAVE_BASUPPORT */


Boolean _should_log_path;

static void
_log_path(audit_token_t * token, const char * msg)
{
	if (_should_log_path) {
		char 		proc_path[PROC_PIDPATHINFO_MAXSIZE] = {0};

		if (proc_pidpath_audittoken(token, proc_path, sizeof(proc_path))
		    <= 0) {
			return;
		}
		SC_log(LOG_NOTICE, "%s:%s", msg, proc_path);
	}
	return;
}

static Boolean
sessionIsPlatformBinary(serverSessionRef session)
{
	Boolean		isPB = FALSE;

	if (!myCFBooleanGetValueIfSet(session->isPlatformBinary, &isPB)) {
		SecTaskRef	task;

		task = SecTaskCreateWithAuditToken(NULL, session->auditToken);
		if (task != NULL) {
			uint32_t 	csr_status = 0;

			csr_status = SecTaskGetCodeSignStatus(task);
			CFRelease(task);
			isPB = ((csr_status & CS_PLATFORM_BINARY) != 0);
		}
		/* remember that we checked */
		session->isPlatformBinary
			= isPB ? kCFBooleanTrue : kCFBooleanFalse;
		SC_log(LOG_DEBUG, "%s: %@ is%s a platform binary",
		       __func__, session, isPB ? "" : " NOT");
		if (isPB) {
			_log_path(&session->auditToken,
				  "SC_PLATFORM_BINARY_PATH");
		}
	}
	return (isPB);
}

static Boolean
sessionIsSystemProcess(serverSessionRef session)
{
	Boolean		isSystem = FALSE;

#if _HAVE_PRIVACY_ACCOUNTING
	if (havePrivacyAccounting()
	    && !myCFBooleanGetValueIfSet(session->isSystemProcess, &isSystem)) {
		CFDictionaryRef	dict;

		dict = copySystemProcessEntitlementsDictionary(session);
		if (dict != NULL) {
			isSystem = isSystemProcess(dict);
			CFRelease(dict);
		}
		/* remember that we checked */
		session->isSystemProcess
			= isSystem ? kCFBooleanTrue : kCFBooleanFalse;
		SC_log(LOG_DEBUG, "%@ is%s a system process",
		       session, isSystem ? "" : " NOT");
		if (isSystem) {
			_log_path(&session->auditToken,
				  "SC_SYSTEM_PROCESS_PATH");
		}
	}
#else /* _HAVE_PRIVACY_ACCOUNTING */
#pragma unused(session)
#endif /* _HAVE_PRIVACY_ACCOUNTING */

	return (isSystem);
}

__private_extern__
int
checkReadAccess(serverSessionRef session, CFStringRef key,
		CFDictionaryRef controls)
{
	CFArrayRef	read_allow;
	CFArrayRef	read_deny;
	int		status = kSCStatusOK;

	if (isConfigd(session)) {
		/* configd can read any key */
		goto done;
	}

	/* check whether key has access restrictions */
	if (controls == NULL) {
		controls = _storeKeyGetAccessControls(key);
	}
	if (controls == NULL) {
		/* key is unrestricted */
		goto done;
	}
	/*
	 * Check "deny" restrictions
	 */

	/* read-deny */
	read_deny = CFDictionaryGetValue(controls,
					 kSCDAccessControls_readDeny);
	if (read_deny != NULL
	    && sessionHasEntitlement(session, read_deny)) {
		/* process has a deny entitlement */
		status = kSCStatusAccessError;
		SC_log(LOG_INFO,
		       "%s(%@): %@ has deny entitlement",
		       __func__, key, session);
		goto done;
	}

	/* read-deny-background */
	if (CFDictionaryContainsKey(controls,
				    kSCDAccessControls_readDenyBackground)
	    && sessionIsBackgroundAssetExtension(session)) {
		/* background asset extension denied access */
		status = kSCStatusAccessError;
		SC_log(LOG_INFO,
		       "%s(%@): %@ deny background asset extension",
		       __func__, key, session);
		goto done;
	}

	/*
	 * Check "allow" restrictions
	 */

	/* read-allow */
	read_allow = CFDictionaryGetValue(controls,
					  kSCDAccessControls_readAllow);
	if (read_allow != NULL) {
		Boolean		no_fault;

		if (sessionHasEntitlement(session, read_allow)) {
			/* process has allow entitlement */
			SC_log(LOG_INFO,
			       "%s(%@): %@ has allow entitlement",
			       __func__, key, session);
			goto done;
		}
		/* read-allow-system */
		if (CFDictionaryContainsKey(controls,
					    kSCDAccessControls_readAllowSystem)
		    && sessionIsSystemProcess(session)) {
			/* allow system process */
			goto done;
		}
		no_fault = hasBooleanEntitlement(session,
						 kSCReadNoFaultEntitlementName);
		/* read-allow-platform */
		if (CFDictionaryContainsKey(controls,
					    kSCDAccessControls_readAllowPlatform)
		    && sessionIsPlatformBinary(session)) {
			/* allow platform binary */
			if (!no_fault) {
				/* but generate fault on client-side */
				status = kSCStatusOK_MissingReadEntitlement;
			}
			goto done;
		}
		/* read not allowed */
		if (no_fault) {
			status = kSCStatusAccessError;
		}
		else {
			/* generate fault on client-side */
			status = kSCStatusAccessError_MissingReadEntitlement;
		}
		goto done;
	}

 done:
	return (status);
}


__private_extern__
int
checkWriteAccess(serverSessionRef session, CFStringRef key)
{
	CFDictionaryRef controls;
	int		status = kSCStatusOK;

	/* check whether key has write protect set */
	controls = _storeKeyGetAccessControls(key);
	if (controls != NULL &&
	    CFDictionaryContainsKey(controls,
				    kSCDAccessControls_writeProtect)) {
		/* process must have the key-specific entitlement */
		if (hasEntitlementForKey(session,
					 kSCWriteEntitlementName,
					 key)) {
			goto done;
		}
		/* entitlement is not present */
		if (!hasBooleanEntitlement(session,
					   kSCWriteNoFaultEntitlementName)) {
			/* return internal code to force crash on client-side */
			status = kSCStatusAccessError_MissingWriteEntitlement;
		}
		else {
			status = kSCStatusAccessError;
		}
		goto done;
	}
	/* allow configd */
	if (isConfigd(session)) {
		goto done;
	}

	/* allow root process, but log if Setup: key */
	if (hasRootAccess(session)) {
		if (CFStringHasPrefix(key, kSCDynamicStoreDomainSetup)) {
			SC_log(LOG_NOTICE,
			       "*** Non-configd pid %d modifying \"%@\" ***",
			       sessionPid(session),
			       key);
		}
		goto done;
	}
	/* check for boolean grant-all or key-specific entitlement */
	if (!hasBooleanEntitlement(session, kSCWriteEntitlementName)
	    && !hasEntitlementForKey(session, kSCWriteEntitlementName, key)) {
		status = kSCStatusAccessError;
		goto done;
	}

 done:
	return (status);
}
