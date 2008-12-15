#include <TargetConditionals.h>
#include <libc.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include  "IOSystemConfiguration.h"

__private_extern__ CFStringRef kSCCompAnyRegex = CFSTR("[^/]+");
__private_extern__ CFStringRef kSCDynamicStoreDomainState = CFSTR("State:");


#define SYSTEM_FRAMEWORK_DIR "/System/Library/Frameworks"
#define SYSTEM_CONFIGURATION "SystemConfiguration.framework/SystemConfiguration"
#define SC_FRAMEWORK	     SYSTEM_FRAMEWORK_DIR "/" SYSTEM_CONFIGURATION

static void * symAddrInSC(const char *name)
{
    static void* handle = NULL;
    
    if (!handle) {
	void *locHandle;
	const char  *framework = SC_FRAMEWORK;
	struct stat  statbuf;
	const char  *suffix   = getenv("DYLD_IMAGE_SUFFIX");
	char	     path[MAXPATHLEN];

	strlcpy(path, framework, sizeof(path));
	if (suffix)
	    strlcat(path, suffix, sizeof(path));
	if (0 <= stat(path, &statbuf))
	    locHandle = dlopen(path,      RTLD_LAZY);
	else
	    locHandle = dlopen(framework, RTLD_LAZY);

	if (locHandle) {
	    const CFStringRef *refP;
	    refP = dlsym(locHandle, "kSCCompAnyRegex");
	    kSCCompAnyRegex = *refP;
	    refP = dlsym(locHandle, "kSCDynamicStoreDomainState");
	    kSCDynamicStoreDomainState = *refP;
	    handle = locHandle;
	}
    }

    if (handle)
	return dlsym(handle, name);
    else
	return NULL;
}


__private_extern__ Boolean
SCDynamicStoreAddWatchedKey	(SCDynamicStoreRef		store,
				 CFStringRef			key,
				 Boolean			isRegex)
{
    static typeof (SCDynamicStoreAddWatchedKey) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCDynamicStoreAddWatchedKey");

    if (dyfunc)
	return (*dyfunc)(store, key, isRegex);
    else
	return false;
}

__private_extern__ int
SCError()
{
    static typeof (SCError) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCError");
    if (dyfunc)
	return (*dyfunc)();
    else
	return kSCStatusFailed;
}

__private_extern__ CFDictionaryRef
SCDynamicStoreCopyMultiple	(
				    SCDynamicStoreRef		store,
				    CFArrayRef			keys,
				    CFArrayRef			patterns
				)
{
    static typeof (SCDynamicStoreCopyMultiple) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCDynamicStoreCopyMultiple");
    if (dyfunc)
	return (*dyfunc)(store, keys, patterns);
    else
	return NULL;
}

__private_extern__ CFPropertyListRef
SCDynamicStoreCopyValue		(
				    SCDynamicStoreRef		store,
				    CFStringRef			key
				)
{
    static typeof (SCDynamicStoreCopyValue) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCDynamicStoreCopyValue");
    if (dyfunc)
	return (*dyfunc)(store, key);
    else
	return NULL;
}

__private_extern__ SCDynamicStoreRef
SCDynamicStoreCreate		(
				    CFAllocatorRef		allocator,
				    CFStringRef			name,
				    SCDynamicStoreCallBack	callout,
				    SCDynamicStoreContext	*context
				)
{
    static typeof (SCDynamicStoreCreate) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCDynamicStoreCreate");
    if (dyfunc)
	return (*dyfunc)(allocator, name, callout, context);
    else
	return NULL;
}

__private_extern__ CFRunLoopSourceRef
SCDynamicStoreCreateRunLoopSource(
				    CFAllocatorRef		allocator,
				    SCDynamicStoreRef		store,
				    CFIndex			order
				)
{
    static typeof (SCDynamicStoreCreateRunLoopSource) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCDynamicStoreCreateRunLoopSource");
    if (dyfunc)
	return (*dyfunc)(allocator, store, order);
    else
	return NULL;
}

__private_extern__ CFStringRef
SCDynamicStoreKeyCreate		(
				    CFAllocatorRef		allocator,
				    CFStringRef			fmt,
				    ...
				)
{
    // Local implementation of a SCDynamicStore wrapper function
    va_list val;

    va_start(val, fmt);
    CFStringRef key =
	CFStringCreateWithFormatAndArguments(allocator, NULL, fmt, val);
    va_end(val);

    return key;
}

__private_extern__ CFStringRef
SCDynamicStoreKeyCreatePreferences(
				    CFAllocatorRef		allocator,
				    CFStringRef			prefsID,
				    SCPreferencesKeyType	keyType
				)
{
    static typeof (SCDynamicStoreKeyCreatePreferences) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCDynamicStoreKeyCreatePreferences");
    if (dyfunc)
	return (*dyfunc)(allocator, prefsID, keyType);
    else
	return NULL;
}

__private_extern__ Boolean
SCDynamicStoreSetNotificationKeys(
				    SCDynamicStoreRef		store,
				    CFArrayRef			keys,
				    CFArrayRef			patterns
				)
{
    static typeof (SCDynamicStoreSetNotificationKeys) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCDynamicStoreSetNotificationKeys");
    if (dyfunc)
	return (*dyfunc)(store, keys, patterns);
    else
	return false;
}

__private_extern__ Boolean
SCDynamicStoreSetValue		(
				    SCDynamicStoreRef		store,
				    CFStringRef			key,
				    CFPropertyListRef		value
				)
{
    static typeof (SCDynamicStoreSetValue) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCDynamicStoreSetValue");
    if (dyfunc)
	return (*dyfunc)(store, key, value);
    else
	return false;
}

__private_extern__ Boolean
SCDynamicStoreNotifyValue		(
					SCDynamicStoreRef		store,
					CFStringRef			key
					)
{
    static typeof (SCDynamicStoreNotifyValue) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCDynamicStoreNotifyValue");
    if (dyfunc)
	return (*dyfunc)(store, key);
    else
	return false;
}

__private_extern__ Boolean
SCPreferencesApplyChanges	(
				    SCPreferencesRef		prefs
				)
{
    static typeof (SCPreferencesApplyChanges) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCPreferencesApplyChanges");
    if (dyfunc)
	return (*dyfunc)(prefs);
    else
	return false;
}

__private_extern__ Boolean
SCPreferencesCommitChanges	(
				    SCPreferencesRef		prefs
				)
{
    static typeof (SCPreferencesCommitChanges) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCPreferencesCommitChanges");
    if (dyfunc)
	return (*dyfunc)(prefs);
    else
	return false;
}

__private_extern__ SCPreferencesRef
SCPreferencesCreate		(
				    CFAllocatorRef		allocator,
				    CFStringRef			name,
				    CFStringRef			prefsID
				)
{
    static typeof (SCPreferencesCreate) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCPreferencesCreate");
    if (dyfunc)
	return (*dyfunc)(allocator, name, prefsID);
    else
	return NULL;
}

#if TARGET_OS_EMBEDDED
__private_extern__ SCPreferencesRef
SCPreferencesCreateWithAuthorization	(
					CFAllocatorRef		allocator,
					CFStringRef		name,
					CFStringRef		prefsID,
					AuthorizationRef	authorization
					)
{
    static typeof (SCPreferencesCreateWithAuthorization) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCPreferencesCreateWithAuthorization");
    if (dyfunc)
	return (*dyfunc)(allocator, name, prefsID, authorization);
    else
	return NULL;
}
#endif /* TARGET_OS_EMBEDDED */

__private_extern__ CFPropertyListRef
SCPreferencesGetValue		(
				    SCPreferencesRef		prefs,
				    CFStringRef			key
				)
{
    static typeof (SCPreferencesGetValue) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCPreferencesGetValue");
    if (dyfunc)
	return (*dyfunc)(prefs, key);
    else
	return NULL;
}

__private_extern__ Boolean
SCPreferencesLock		(
				    SCPreferencesRef		prefs,
				    Boolean			wait
				)
{
    static typeof (SCPreferencesLock) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCPreferencesLock");
    if (dyfunc)
	return (*dyfunc)(prefs, wait);
    else
	return false;
}

__private_extern__ Boolean
SCPreferencesRemoveValue	(
				    SCPreferencesRef		prefs,
				    CFStringRef			key
				)
{
    static typeof (SCPreferencesRemoveValue) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCPreferencesRemoveValue");
    if (dyfunc)
	return (*dyfunc)(prefs, key);
    else
	return false;
}

__private_extern__ Boolean
SCPreferencesSetValue		(
				    SCPreferencesRef		prefs,
				    CFStringRef			key,
				    CFPropertyListRef		value
				)
{
    static typeof (SCPreferencesSetValue) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCPreferencesSetValue");
    if (dyfunc)
	return (*dyfunc)(prefs, key, value);
    else
	return false;
}

__private_extern__ Boolean
SCPreferencesUnlock		(
				    SCPreferencesRef		prefs
				)
{
    static typeof (SCPreferencesUnlock) *dyfunc;
    if (!dyfunc) 
	dyfunc = symAddrInSC("SCPreferencesUnlock");
    if (dyfunc)
	return (*dyfunc)(prefs);
    else
	return false;
}
