/*
 * Copyright (c) 2007 Apple Computer, Inc. All rights reserved.
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
 * October 24, 2007		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <ApplicationServices/ApplicationServices.h>
#include "UserEventAgentInterface.h"

#define MY_BUNDLE_ID    CFSTR("com.apple.SystemConfiguration.SCMonitor")
#define	MY_ICON_PATH	"/System/Library/PreferencePanes/Network.prefPane/Contents/Resources/Network.icns"

#define	NETWORK_PREF_APP	"/System/Library/PreferencePanes/Network.prefPane"
#define	NETWORK_PREF_CMD	"New Interface"

typedef struct {
	UserEventAgentInterfaceStruct	*_UserEventAgentInterface;
	CFUUIDRef			_factoryID;
	UInt32				_refCount;

	Boolean				no_user_intervention;

	CFRunLoopSourceRef		monitorRls;

	CFMutableSetRef			knownInterfaces;

	CFMutableArrayRef		userInterfaces;
	CFUserNotificationRef		userNotification;
	CFRunLoopSourceRef		userRls;
} MyType;

static CFMutableDictionaryRef	notify_to_instance	= NULL;


#pragma mark -
#pragma mark Watch for new [network] interfaces


static void
open_NetworkPrefPane(void)
{
	AEDesc		aeDesc	= { typeNull, NULL };
	CFArrayRef	prefArray;
	CFURLRef	prefURL;
	LSLaunchURLSpec	prefSpec;
	OSStatus	status;

	prefURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
						CFSTR(NETWORK_PREF_APP),
						kCFURLPOSIXPathStyle,
						FALSE);
	prefArray = CFArrayCreate(NULL, (const void **)&prefURL, 1, &kCFTypeArrayCallBacks);
	CFRelease(prefURL);

	status = AECreateDesc('ptru',
			      (const void *)NETWORK_PREF_CMD,
			      strlen(NETWORK_PREF_CMD),
			      &aeDesc);
	if (status != noErr) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCMonitor: AECreateDesc() failed: %d"), status);
	}

	prefSpec.appURL		= NULL;
	prefSpec.itemURLs	= prefArray;
	prefSpec.passThruParams	= &aeDesc;
	prefSpec.launchFlags	= kLSLaunchAsync | kLSLaunchDontAddToRecents;
	prefSpec.asyncRefCon	= NULL;

	status = LSOpenFromURLSpec(&prefSpec, NULL);
	if (status != noErr) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCMonitor: LSOpenFromURLSpec() failed: %d"), status);
	}

	CFRelease(prefArray);
	if (aeDesc.descriptorType != typeNull) AEDisposeDesc(&aeDesc);
	return;
}


static void
notify_remove(MyType *myInstance, Boolean cancel)
{
	if (myInstance->userInterfaces != NULL) {
		CFRelease(myInstance->userInterfaces);
		myInstance->userInterfaces = NULL;
	}

	if (myInstance->userRls != NULL) {
		CFRunLoopSourceInvalidate(myInstance->userRls);
		CFRelease(myInstance->userRls);
		myInstance->userRls = NULL;
	}

	if (myInstance->userNotification != NULL) {
		if (cancel) {
			SInt32	status;

			status = CFUserNotificationCancel(myInstance->userNotification);
			if (status != 0) {
				SCLog(TRUE, LOG_ERR,
				      CFSTR("SCMonitor: CFUserNotificationCancel() failed, status=%d"),
				      status);
			}
		}
		CFRelease(myInstance->userNotification);
		myInstance->userNotification = NULL;
	}

	return;
}


static void
notify_reply(CFUserNotificationRef userNotification, CFOptionFlags response_flags)
{
	MyType	*myInstance	= NULL;

	// get instance for notification
	if (notify_to_instance != NULL) {
		myInstance = (MyType *)CFDictionaryGetValue(notify_to_instance, userNotification);
		if (myInstance != NULL) {
			CFDictionaryRemoveValue(notify_to_instance, userNotification);
			if (CFDictionaryGetCount(notify_to_instance) == 0) {
				CFRelease(notify_to_instance);
				notify_to_instance = NULL;
			}
		}
	}
	if (myInstance == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCMonitor: can't find user notification"));
		return;
	}

	// process response
	switch (response_flags & 0x3) {
		case kCFUserNotificationDefaultResponse:
			// user asked to configure interface
			open_NetworkPrefPane();
			break;
		default:
			// user cancelled
			break;
	}

	notify_remove(myInstance, FALSE);
	return;
}


static void
notify_add(MyType *myInstance)
{
	CFBundleRef		bundle;
	CFMutableDictionaryRef	dict	= NULL;
	SInt32			error	= 0;
	CFIndex			i;
	CFMutableArrayRef	message;
	CFIndex			n	= CFArrayGetCount(myInstance->userInterfaces);
	CFURLRef		url	= NULL;

	if (myInstance->userNotification != NULL) {
		CFMutableArrayRef	save	= NULL;

		if (n > 0) {
			CFRetain(myInstance->userInterfaces);
			save = myInstance->userInterfaces;
		}
		notify_remove(myInstance, TRUE);
		myInstance->userInterfaces = save;
		if (n == 0) {
			return;
		}
	}

	dict = CFDictionaryCreateMutable(NULL,
					 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);

	// set localization URL
	bundle = CFBundleGetBundleWithIdentifier(MY_BUNDLE_ID);
	if (bundle != NULL) {
		url = CFBundleCopyBundleURL(bundle);
	}
	if (url != NULL) {
		// set URL
		CFDictionarySetValue(dict, kCFUserNotificationLocalizationURLKey, url);
		CFRelease(url);
	} else {
		SCLog(TRUE, LOG_NOTICE, CFSTR("SCMonitor: can't find bundle"));
		goto done;
	}

	// set icon URL
	url = CFURLCreateFromFileSystemRepresentation(NULL,
						      (const UInt8 *)MY_ICON_PATH,
						      strlen(MY_ICON_PATH),
						      FALSE);
	if (url != NULL) {
		CFDictionarySetValue(dict, kCFUserNotificationIconURLKey, url);
		CFRelease(url);
	}

	// header
	CFDictionarySetValue(dict,
			     kCFUserNotificationAlertHeaderKey,
			     (n == 1) ? CFSTR("HEADER_1") : CFSTR("HEADER_N"));

	// message
	message = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(message,
			   (n == 1) ? CFSTR("MESSAGE_S1") : CFSTR("MESSAGE_SN"));
	for (i = 0; i < n; i++) {
		SCNetworkInterfaceRef	interface;
		CFStringRef		name;

		interface = CFArrayGetValueAtIndex(myInstance->userInterfaces, i);
		name = SCNetworkInterfaceGetLocalizedDisplayName(interface);
		if (n == 1) {
			CFArrayAppendValue(message, name);
		} else {
			CFStringRef	str;

			str = CFStringCreateWithFormat(NULL, NULL, CFSTR("\r\t%@"), name);
			CFArrayAppendValue(message, str);
			CFRelease(str);
		}
	}
	CFArrayAppendValue(message,
			   (n == 1) ? CFSTR("MESSAGE_E1") : CFSTR("MESSAGE_EN"));
	CFDictionarySetValue(dict, kCFUserNotificationAlertMessageKey, message);
	CFRelease(message);

	// button titles
	CFDictionaryAddValue(dict, kCFUserNotificationDefaultButtonTitleKey,   CFSTR("OPEN_NP"));
	CFDictionaryAddValue(dict, kCFUserNotificationAlternateButtonTitleKey, CFSTR("CANCEL"));

	// create and post notification
	myInstance->userNotification = CFUserNotificationCreate(NULL,
								0,
								kCFUserNotificationNoteAlertLevel,
								&error,
								dict);
	if (myInstance->userNotification == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCMonitor: CFUserNotificationCreate() failed, %d"), error);
		goto done;
	}

	// establish callback
	myInstance->userRls = CFUserNotificationCreateRunLoopSource(NULL,
								    myInstance->userNotification,
								    notify_reply,
								    0);
	if (myInstance->userRls == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCMonitor: CFUserNotificationCreateRunLoopSource() failed"));
		CFRelease(myInstance->userNotification);
		myInstance->userNotification = NULL;
		goto done;
	}
	CFRunLoopAddSource(CFRunLoopGetCurrent(), myInstance->userRls,  kCFRunLoopDefaultMode);

	// add instance for notification
	if (notify_to_instance == NULL) {
		notify_to_instance = CFDictionaryCreateMutable(NULL,
							       0,
							       &kCFTypeDictionaryKeyCallBacks,
							       NULL);	// no retain/release/... for values
	}
	CFDictionarySetValue(notify_to_instance, myInstance->userNotification, myInstance);

    done :

	if (dict != NULL) CFRelease(dict);
	return;
}


static void
notify_configure(MyType *myInstance)
{
	AuthorizationRef	authorization	= NULL;
	CFIndex			i;
	CFIndex			n;
	Boolean			ok;
	SCPreferencesRef	prefs		= NULL;
	SCNetworkSetRef		set		= NULL;

	if (geteuid() == 0) {
		prefs = SCPreferencesCreate(NULL, CFSTR("SCMonitor"), NULL);
	} else {
		AuthorizationFlags	flags		= kAuthorizationFlagDefaults;
		OSStatus		status;

		status = AuthorizationCreate(NULL,
					     kAuthorizationEmptyEnvironment,
					     flags,
					     &authorization);
		if (status != errAuthorizationSuccess) {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("AuthorizationCreate() failed: status = %d\n"),
			      status);
			return;
		}

		prefs = SCPreferencesCreateWithAuthorization(NULL, CFSTR("SCMonitor"), NULL, authorization);
	}

	set = SCNetworkSetCopyCurrent(prefs);
	if (set == NULL) {
		set = SCNetworkSetCreate(prefs);
		if (set == NULL) {
			goto done;
		}
	}

	n = CFArrayGetCount(myInstance->userInterfaces);
	for (i = 0; i < n; i++) {
		SCNetworkInterfaceRef	interface;
		
		interface = CFArrayGetValueAtIndex(myInstance->userInterfaces, i);
		ok = SCNetworkSetEstablishDefaultInterfaceConfiguration(set, interface);
		if (ok) {
			CFStringRef	name;

			name = SCNetworkInterfaceGetLocalizedDisplayName(interface);
			SCLog(TRUE, LOG_NOTICE, CFSTR("add service for %@"), name);
		}
	}
	
	ok = SCPreferencesCommitChanges(prefs);
	if (!ok) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCPreferencesCommitChanges() failed: %s\n"),
		      SCErrorString(SCError()));
                goto done;
        }
		
        ok = SCPreferencesApplyChanges(prefs);
	if (!ok) {
                SCLog(TRUE, LOG_ERR,
		      CFSTR("SCPreferencesApplyChanges() failed: %s\n"),
		      SCErrorString(SCError()));
                goto done;
        }

    done :
	
	if (set != NULL) {
		CFRelease(set);
		set = NULL;
	}

	if (prefs != NULL) {
		CFRelease(prefs);
		prefs = NULL;
	}
	
        if (authorization != NULL) {
                AuthorizationFree(authorization, kAuthorizationFlagDefaults);
		//              AuthorizationFree(authorization, kAuthorizationFlagDestroyRights);
                authorization = NULL;
        }
	
	CFRelease(myInstance->userInterfaces);
	myInstance->userInterfaces = NULL;
	
	return;
}


static void
updateInterfaceList(SCDynamicStoreRef store, CFArrayRef changes, void * arg)
{
	CFIndex			i;
	CFArrayRef		interfaces;
	MyType			*myInstance	= (MyType *)arg;
	CFIndex			n;
	SCPreferencesRef	prefs;
	CFMutableSetRef		previouslyKnown	= NULL;
	SCNetworkSetRef		set		= NULL;

	prefs = SCPreferencesCreate(NULL, CFSTR("SCMonitor"), NULL);
	if (prefs == NULL) {
		return;
	}

	set = SCNetworkSetCopyCurrent(prefs);
	if (set == NULL) {
		set = SCNetworkSetCreate(prefs);
		if (set == NULL) {
			goto done;
		}
	}

	previouslyKnown = CFSetCreateMutableCopy(NULL, 0, myInstance->knownInterfaces);

	interfaces = SCNetworkInterfaceCopyAll();
	if (interfaces != NULL) {

		n = CFArrayGetCount(interfaces);
		for (i = 0; i < n; i++) {
			CFStringRef		bsdName;
			SCNetworkInterfaceRef	interface;
			Boolean			ok;

			interface = CFArrayGetValueAtIndex(interfaces, i);
			bsdName = SCNetworkInterfaceGetBSDName(interface);
			if (bsdName == NULL) {
				// if no BSD name
				continue;
			}

			CFSetRemoveValue(previouslyKnown, bsdName);

			if (CFSetContainsValue(myInstance->knownInterfaces, bsdName)) {
				// if known interface
				continue;
			}

			CFSetAddValue(myInstance->knownInterfaces, bsdName);

			ok = SCNetworkSetEstablishDefaultInterfaceConfiguration(set, interface);
			if (ok) {
				// this is a *new* interface
				if (myInstance->userInterfaces == NULL) {
					myInstance->userInterfaces = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				}
				CFArrayAppendValue(myInstance->userInterfaces, interface);
			}
		}

		CFRelease(interfaces);
	}

	n = CFSetGetCount(previouslyKnown);
	if (n > 0) {
		const void *    names_q[32];
		const void **   names                 = names_q;

		if (n > (CFIndex)(sizeof(names_q) / sizeof(CFTypeRef)))
			names = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
		CFSetGetValues(previouslyKnown, names);
		for (i = 0; i < n; i++) {
			if (myInstance->userInterfaces != NULL) {
				CFIndex	j;

				j = CFArrayGetCount(myInstance->userInterfaces);
				while (--j >= 0) {
					CFStringRef		bsdName;
					SCNetworkInterfaceRef	interface;

					interface = CFArrayGetValueAtIndex(myInstance->userInterfaces, j);
					bsdName = SCNetworkInterfaceGetBSDName(interface);
					if (CFEqual(bsdName, names[i])) {
						// if we have previously posted a notification
						// for this no-longer-present interface
						CFArrayRemoveValueAtIndex(myInstance->userInterfaces, j);
					}
				}
			}

			CFSetRemoveValue(myInstance->knownInterfaces, names[i]);
		}
		if (names != names_q)       CFAllocatorDeallocate(NULL, names);
	}

    done :

	if (myInstance->userInterfaces != NULL) {
		if (myInstance->no_user_intervention) {
			// add network services for new interfaces
			notify_configure(myInstance);
		} else {
			// post notification
			notify_add(myInstance);
		}
	}

	if (set != NULL) CFRelease(set);
	CFRelease(prefs);
	return;
}


static void
watcher_remove(MyType *myInstance)
{
	if (myInstance->monitorRls != NULL) {
		CFRunLoopSourceInvalidate(myInstance->monitorRls);
		CFRelease(myInstance->monitorRls);
		myInstance->monitorRls = NULL;
	}

	if (myInstance->knownInterfaces != NULL) {
		CFRelease(myInstance->knownInterfaces);
		myInstance->knownInterfaces = NULL;
	}

	return;
}


static void
watcher_add(MyType *myInstance)
{
	CFBundleRef		bundle;
	SCDynamicStoreContext	context	= { 0, (void *)myInstance, NULL, NULL, NULL };
	CFDictionaryRef		dict;
	CFStringRef		key;
	CFArrayRef		keys;
	SCDynamicStoreRef	store;

	bundle = CFBundleGetBundleWithIdentifier(MY_BUNDLE_ID);
	if (bundle != NULL) {
		CFDictionaryRef	info;
		CFBooleanRef	user_intervention;
		
		info = CFBundleGetInfoDictionary(bundle);
		user_intervention = CFDictionaryGetValue(info, CFSTR("User Intervention"));
		if (isA_CFBoolean(user_intervention)) {
			myInstance->no_user_intervention = !CFBooleanGetValue(user_intervention);
		}
	}

	store = SCDynamicStoreCreate(NULL, CFSTR("SCMonitor"), updateInterfaceList, &context);
	if (store == NULL) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCMonitor: SCDynamicStoreCreate() failed: %s"),
		      SCErrorString(SCError()));
		return;
	}

	key = SCDynamicStoreKeyCreateNetworkInterface(NULL, kSCDynamicStoreDomainState);

	// watch for changes to the list of network interfaces
	keys = CFArrayCreate(NULL, (const void **)&key, 1, &kCFTypeArrayCallBacks);
	SCDynamicStoreSetNotificationKeys(store, NULL, keys);
	CFRelease(keys);
	myInstance->monitorRls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(),
			   myInstance->monitorRls,
			   kCFRunLoopDefaultMode);

	// initialize the list of known interfaces
	myInstance->knownInterfaces = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	dict = SCDynamicStoreCopyValue(store, key);
	if (dict != NULL) {
		if (isA_CFDictionary(dict)) {
			CFIndex		i;
			CFArrayRef	interfaces;
			CFIndex		n;

			interfaces = CFDictionaryGetValue(dict, kSCPropNetInterfaces);
			n = isA_CFArray(interfaces) ? CFArrayGetCount(interfaces) : 0;
			for (i = 0; i < n; i++) {
				CFStringRef	bsdName;

				bsdName = CFArrayGetValueAtIndex(interfaces, i);
				if (isA_CFString(bsdName)) {
					CFSetAddValue(myInstance->knownInterfaces, bsdName);
				}
			}
		}

		CFRelease(dict);
	}

	CFRelease(key);
	CFRelease(store);
	return;
}


#pragma mark -
#pragma mark UserEventAgent stubs


static HRESULT
myQueryInterface(void *myInstance, REFIID iid, LPVOID *ppv)
{
	CFUUIDRef	interfaceID	= CFUUIDCreateFromUUIDBytes(NULL, iid);

	// Test the requested ID against the valid interfaces.
	if (CFEqual(interfaceID, kUserEventAgentInterfaceID)) {
		((MyType *) myInstance)->_UserEventAgentInterface->AddRef(myInstance);
		*ppv = myInstance;
		CFRelease(interfaceID);
		return S_OK;
	}

	if (CFEqual(interfaceID, IUnknownUUID)) {
		((MyType *) myInstance)->_UserEventAgentInterface->AddRef(myInstance);
		*ppv = myInstance;
		CFRelease(interfaceID);
		return S_OK;
	}

	// Requested interface unknown, bail with error.
	*ppv = NULL;
	CFRelease(interfaceID);
	return E_NOINTERFACE;
}


static ULONG
myAddRef(void *myInstance)
{
	((MyType *) myInstance)->_refCount++;
	return ((MyType *) myInstance)->_refCount;
}


static ULONG
myRelease(void *myInstance)
{
	((MyType *) myInstance)->_refCount--;
	if (((MyType *) myInstance)->_refCount == 0) {
		CFUUIDRef	factoryID	= ((MyType *) myInstance)->_factoryID;

		if (factoryID != NULL) {
			CFPlugInRemoveInstanceForFactory(factoryID);
			CFRelease(factoryID);

			watcher_remove((MyType *)myInstance);
			notify_remove((MyType *)myInstance, TRUE);
		}
		free(myInstance);
		return 0;
	}

	return ((MyType *) myInstance)->_refCount;
}


static void
myInstall(void *myInstance)
{
	watcher_add((MyType *)myInstance);
	return;
}


static UserEventAgentInterfaceStruct UserEventAgentInterfaceFtbl = {
	NULL,			// Required padding for COM
	myQueryInterface,	// These three are the required COM functions
	myAddRef,
	myRelease,
	myInstall		// Interface implementation
};


void *
UserEventAgentFactory(CFAllocatorRef allocator, CFUUIDRef typeID)
{
	MyType	*newOne	= NULL;

	if (CFEqual(typeID, kUserEventAgentTypeID)) {
		newOne	= (MyType *)malloc(sizeof(MyType));
		bzero(newOne, sizeof(*newOne));
		newOne->_UserEventAgentInterface = &UserEventAgentInterfaceFtbl;
		newOne->_factoryID = (CFUUIDRef)CFRetain(kUserEventAgentFactoryID);
		CFPlugInAddInstanceForFactory(kUserEventAgentFactoryID);
		newOne->_refCount = 1;
	}

	return newOne;
}


#ifdef	MAIN
int
main(int argc, char **argv)
{
	MyType *newOne = (MyType *)malloc(sizeof(MyType));

	_sc_log     = FALSE;
	_sc_verbose = (argc > 1) ? TRUE : FALSE;

	bzero(newOne, sizeof(*newOne));
	myInstall(newOne);
	CFRunLoopRun();
	exit(0);
	return (0);
}
#endif	// MAIN
