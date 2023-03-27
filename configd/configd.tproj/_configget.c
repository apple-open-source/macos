/*
 * Copyright (c) 2000-2022 Apple Inc. All rights reserved.
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

#include "configd.h"
#include "session.h"
#include "SCInternal.h"

__private_extern__
int
__SCDynamicStoreCopyValue(SCDynamicStoreRef	store,
			  CFStringRef		key,
			  CFDictionaryRef	*key_controls,
			  CFDataRef		*value,
			  Boolean		internal)
{
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;
	CFDictionaryRef			dict;

	if (key_controls == NULL) {
		// if we already know we have read access to the key
		SC_trace("%s : %5d : %@",
			 internal ? "*copy  " : "copy   ",
			 storePrivate->server,
			 key);
	}

	dict = CFDictionaryGetValue(storeData, key);
	if ((dict == NULL) ||
	    !CFDictionaryGetValueIfPresent(dict, kSCDData, (const void **)value)) {
		/* key doesn't exist (or data never defined) */
		return kSCStatusNoKey;
	}

	/* Return the data associated with the key */
	CFRetain(*value);

	if (key_controls != NULL) {
		// return per-key controls for future handling
		*key_controls = CFDictionaryGetValue(dict, kSCDAccessControls);
	}

	return kSCStatusOK;
}

__private_extern__
kern_return_t
_configget(mach_port_t			server,
	   xmlData_t			keyRef,		/* raw XML bytes */
	   mach_msg_type_number_t	keyLen,
	   xmlDataOut_t			*dataRef,	/* raw XML bytes */
	   mach_msg_type_number_t	*dataLen,
	   int				*newInstance,
	   int				*sc_status)
{
	CFStringRef		key		= NULL;		/* key  (un-serialized) */
	CFIndex			len;
	serverSessionRef	mySession;
	Boolean			ok;
	int			status;
	CFDataRef		value;

	*dataRef = NULL;
	*dataLen = 0;
	*newInstance = 0;

	/* un-serialize the key */
	if (!_SCUnserializeString(&key, NULL, keyRef, keyLen)) {
		*sc_status = kSCStatusFailed;
		goto done;
	}

	if (!isA_CFString(key)) {
		*sc_status = kSCStatusInvalidArgument;
		goto done;
	}

	mySession = getSession(server);
	if (mySession == NULL) {
		/* you must have an open session to play */
		*sc_status = kSCStatusNoStoreSession;
		goto done;
	}
	status = checkReadAccess(mySession, key, NULL);
	switch (status) {
	case kSCStatusOK:
	case kSCStatusOK_MissingReadEntitlement:
		break;
	default:
#ifdef DEBUG
		SC_trace("!copy   : %5d : %@",
			 mySession->store->server,
			 key);
#endif	// DEBUG
		*sc_status = status;
		goto done;
	}
	*sc_status = __SCDynamicStoreCopyValue(mySession->store, key, NULL, &value, FALSE);
	if (*sc_status != kSCStatusOK) {
		goto done;
	}

	/* serialize the data */
	ok = _SCSerializeData(value, dataRef, &len);
	*dataLen = (mach_msg_type_number_t)len;
	CFRelease(value);
	if (!ok) {
		*sc_status = kSCStatusFailed;
		goto done;
	}

	/* this may change the value to kSCStatusOK_MissingReadEntitlement */
	*sc_status = status;

 done :

	if (key != NULL)	CFRelease(key);
	return KERN_SUCCESS;
}

/*
 * "context" argument for addSpecificKey() and addSpecificPattern()
 */
typedef struct {
	CFMutableDictionaryRef	controls;	// key --> controls (for the key)
	SCDynamicStoreRef	store;
	CFMutableDictionaryRef	dict;
} addSpecific, *addSpecificRef;

static void
addSpecificKey(const void *value, void *context)
{
	addSpecificRef		addContext	= (addSpecificRef)context;
	CFDataRef		data;
	CFStringRef		key		= (CFStringRef)value;
	CFDictionaryRef		key_controls	= NULL;
	int			sc_status;

	if (!isA_CFString(key)) {
		return;
	}

	sc_status = __SCDynamicStoreCopyValue(addContext->store, key, &key_controls, &data, TRUE);
	if (sc_status == kSCStatusOK) {
		CFDictionaryAddValue(addContext->dict, key, data);
		CFRelease(data);

		if (key_controls == NULL) {
			// no access controls are associated with this key so we
			// simply include the key/value pair in the return results
			// that will be passed back to the caller.
			SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)addContext->store;

			SC_trace("*copy   : %5d : %@",
				 storePrivate->server,
				 key);
		} else {
			// this key requires some access control checks.  We have added
			// the key/value to the return results but they need further
			// scrutiny before being passed back to the caller.
			if (addContext->controls == NULL) {
				addContext->controls = CFDictionaryCreateMutable(NULL,
										 0,
										 &kCFTypeDictionaryKeyCallBacks,
										 &kCFTypeDictionaryValueCallBacks);
			}
			CFDictionarySetValue(addContext->controls, key, key_controls);
		}
	}

	return;
}

static void
addSpecificPattern(const void *value, void *context)
{
	addSpecificRef		addContext	= (addSpecificRef)context;
	CFArrayRef		keys;
	CFStringRef		pattern		= (CFStringRef)value;
	int			sc_status;

	if (!isA_CFString(pattern)) {
		return;
	}

	sc_status = __SCDynamicStoreCopyKeyList(addContext->store, pattern, TRUE, &keys);
	if (sc_status == kSCStatusOK) {
		CFArrayApplyFunction(keys,
				     CFRangeMake(0, CFArrayGetCount(keys)),
				     addSpecificKey,
				     context);
		CFRelease(keys);
	}

	return;
}

static CFDictionaryRef
__SCDynamicStoreCopyMultiple(SCDynamicStoreRef	store,
			     CFArrayRef		keys,
			     CFArrayRef		patterns,
			     CFDictionaryRef	*kv_controls)
{
	addSpecific			addContext;

	SC_trace("copy m  : %5d : %ld keys, %ld patterns",
		 store->server,
		 keys     ? CFArrayGetCount(keys)     : 0,
		 patterns ? CFArrayGetCount(patterns) : 0);

	addContext.controls = NULL;
	addContext.store    = store;
	addContext.dict     = CFDictionaryCreateMutable(NULL,
							0,
							&kCFTypeDictionaryKeyCallBacks,
							&kCFTypeDictionaryValueCallBacks);

	if (keys != NULL) {
		CFArrayApplyFunction(keys,
				     CFRangeMake(0, CFArrayGetCount(keys)),
				     addSpecificKey,
				     &addContext);
	}

	if (patterns != NULL) {
		CFArrayApplyFunction(patterns,
				     CFRangeMake(0, CFArrayGetCount(patterns)),
				     addSpecificPattern,
				     &addContext);
	}

	/* return any [key_]controls associated with the returned keys */
	if (kv_controls != NULL) {
		*kv_controls = addContext.controls;
	} else if (addContext.controls != NULL) {
		CFRelease(addContext.controls);
	}

	/* Return the keys/values associated with the key */
	return (addContext.dict);
}

/*
 * "context" argument for update_multiple()
 */
typedef struct {
	serverSessionRef        mySession;	// SCDynamicStore session
	CFMutableDictionaryRef  dict;		// matching keys/values (being updated per access controls)
} updateContext, *updateContextRef;

static void
update_multiple(const void *key, const void *value, void *context)
{
	int				status;
	CFDictionaryRef			controls	= (CFDictionaryRef)value;
	updateContextRef		update		= (updateContextRef)context;
#ifdef	DEBUG
	SCDynamicStorePrivateRef	storePrivate;
#endif	// DEBUG

	status = checkReadAccess(update->mySession, key, controls);
	switch (status) {
	case kSCStatusOK:
	case kSCStatusOK_MissingReadEntitlement:
		/* read is allowed */
		SC_trace("*copy   : %5d : %@",
			 update->mySession->store->server,
			 key);
		return;
	default:
		break;
	}

#ifdef	DEBUG
	/* entitlement protected key */
	storePrivate = (SCDynamicStorePrivateRef)update->mySession->store;
	SC_trace("!copy   : %5d : %@",
		 storePrivate->server,
		 key);
#endif	// DEBUG

	CFDictionaryRemoveValue(update->dict, key);
}

__private_extern__
kern_return_t
_configget_m(mach_port_t		server,
	     xmlData_t			keysRef,
	     mach_msg_type_number_t	keysLen,
	     xmlData_t			patternsRef,
	     mach_msg_type_number_t	patternsLen,
	     xmlDataOut_t		*dataRef,
	     mach_msg_type_number_t	*dataLen,
	     int			*sc_status)
{
	CFArrayRef		keys		= NULL;	/* keys (un-serialized) */
	CFDictionaryRef		kv		= NULL;	/* keys/values (un-serialized) */
	CFDictionaryRef		kv_controls	= NULL;
	CFIndex			len;
	serverSessionRef	mySession;
	Boolean			ok;
	CFArrayRef		patterns	= NULL;	/* patterns (un-serialized) */

	*dataRef = NULL;
	*dataLen = 0;

	*sc_status = kSCStatusOK;

	if (keysRef && (keysLen > 0)) {
		/* un-serialize the keys */
		if (!_SCUnserialize((CFPropertyListRef *)&keys, NULL, keysRef, keysLen)) {
			*sc_status = kSCStatusFailed;
		}
	}

	if (patternsRef && (patternsLen > 0)) {
		/* un-serialize the patterns */
		if (!_SCUnserialize((CFPropertyListRef *)&patterns, NULL, patternsRef, patternsLen)) {
			*sc_status = kSCStatusFailed;
		}
	}

	if (*sc_status != kSCStatusOK) {
		goto done;
	}

	if ((keys != NULL) && !isA_CFArray(keys)) {
		*sc_status = kSCStatusInvalidArgument;
		goto done;
	}

	if ((patterns != NULL) && !isA_CFArray(patterns)) {
		*sc_status = kSCStatusInvalidArgument;
		goto done;
	}

	mySession = getSession(server);
	if (mySession == NULL) {
		/* you must have an open session to play */
		*sc_status = kSCStatusNoStoreSession;
		goto done;
	}

	/* fetch the requested information */
	kv = __SCDynamicStoreCopyMultiple(mySession->store, keys, patterns,
					  &kv_controls);
	if (kv == NULL) {
		goto done;
	}

	if (kv_controls != NULL) {
		/*
		 * One (or more) of the requested keys/patterns is access
		 * controlled.  Check the session credentials to see if
		 * any results need to be excluded.
		 */
		if (CFDictionaryGetCount(kv) > 0) {
			updateContext   update;

			update.mySession = mySession;
			update.dict = CFDictionaryCreateMutableCopy(NULL, 0, kv);
			CFDictionaryApplyFunction(kv_controls, update_multiple, &update);

			CFRelease(kv);
			kv = update.dict;
		}
	}

	/* serialize the dictionary of matching keys/patterns */
	ok = _SCSerialize(kv, NULL, dataRef, &len);
	*dataLen = (mach_msg_type_number_t)len;
	CFRelease(kv);
	if (!ok) {
		*sc_status = kSCStatusFailed;
	}

    done :

	if (keys != NULL)		CFRelease(keys);
	if (patterns != NULL)		CFRelease(patterns);
	if (kv_controls != NULL)	CFRelease(kv_controls);
	return KERN_SUCCESS;
}
