/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All rights reserved.
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
 * April 16, 2002		Allan Nathanson <ajn@apple.com>
 * - updated to use _SCDPluginExecCommand()
 *
 * June 23, 2001		Allan Nathanson <ajn@apple.com>
 * - updated to public SystemConfiguration.framework APIs
 *
 * June 4, 2001			Allan Nathanson <ajn@apple.com>
 * - add changed keys as the arguments to the kicker script
 *
 * June 30, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <notify.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>	// for SCLog()
#include <SystemConfiguration/SCDPlugin.h>
#include <SystemConfiguration/SCValidation.h>

/*
 * Information maintained for each to-be-kicked registration.
 */
typedef struct {
	boolean_t		active;
	boolean_t		needsKick;

	/* dictionary associated with this target */
	CFDictionaryRef		dict;

	/* SCDynamicStore session information for this target */
	CFRunLoopRef		rl;
	CFRunLoopSourceRef	rls;
	SCDynamicStoreRef	store;

	/* changed keys */
	CFMutableArrayRef	changedKeys;
} kickee, *kickeeRef;

static CFURLRef	myBundleURL	= NULL;
static Boolean	_verbose	= FALSE;

static void booter(kickeeRef target);
static void booterExit(pid_t pid, int status, struct rusage *rusage, void *context);


static void
cleanupKicker(kickeeRef target)
{
	CFStringRef		name	= CFDictionaryGetValue(target->dict, CFSTR("name"));

	SCLog(TRUE, LOG_NOTICE,
	      CFSTR("  target=%@: disabled"),
	      name);
	CFRunLoopRemoveSource(target->rl, target->rls, kCFRunLoopDefaultMode);
	CFRelease(target->rls);
	CFRelease(target->store);
	if (target->dict)		CFRelease(target->dict);
	if (target->changedKeys)	CFRelease(target->changedKeys);
	CFAllocatorDeallocate(NULL, target);
}


static void
booter(kickeeRef target)
{
	char			**argv		= NULL;
	char			*cmd		= NULL;
	CFStringRef		execCommand	= CFDictionaryGetValue(target->dict, CFSTR("execCommand"));
	int			i;
	CFArrayRef		keys		= NULL;
	CFStringRef		name		= CFDictionaryGetValue(target->dict, CFSTR("name"));
	int			nKeys		= 0;
	Boolean			ok		= FALSE;
	CFStringRef		postName	= CFDictionaryGetValue(target->dict, CFSTR("postName"));

	if (target->active) {
		/* we need another kick! */
		target->needsKick = TRUE;

		SCLog(_verbose, LOG_DEBUG, CFSTR("Kicker callback, target=%@ request queued"), name);
		return;
	}

	SCLog(_verbose, LOG_DEBUG, CFSTR("Kicker callback, target=%@"), name);

	if (!isA_CFString(postName) && !isA_CFString(execCommand)) {
		goto error;	/* if no notifications to post nor commands to execute */
	}

	if (isA_CFString(postName)) {
		uint32_t	status;

		/*
		 * post a notification
		 */
		cmd = _SC_cfstring_to_cstring(postName, NULL, 0, kCFStringEncodingASCII);
		if (!cmd) {
			SCLog(TRUE, LOG_DEBUG, CFSTR("  could not convert post name to C string"));
			goto error;
		}

		SCLog(TRUE, LOG_NOTICE, CFSTR("posting notification %s"), cmd);
		status = notify_post(cmd);
		if (status != NOTIFY_STATUS_OK) {
			SCLog(TRUE, LOG_DEBUG, CFSTR("  notify_post() failed: error=%ld"), status);
			goto error;
		}

		CFAllocatorDeallocate(NULL, cmd);	/* clean up */
		cmd = NULL;
	}

	/*
	 * get the arguments for the kickee
	 */
	keys = target->changedKeys;
	target->changedKeys = NULL;

	if (isA_CFString(execCommand)) {
		CFRange			bpr;
		CFNumberRef		execGID		= CFDictionaryGetValue(target->dict, CFSTR("execGID"));
		CFNumberRef		execUID		= CFDictionaryGetValue(target->dict, CFSTR("execUID"));
		CFBooleanRef		passKeys	= CFDictionaryGetValue(target->dict, CFSTR("changedKeysAsArguments"));
		gid_t			reqGID		= 0;
		uid_t			reqUID		= 0;
		CFMutableStringRef	str;

		/*
		 * build the kickee command
		 */
		str = CFStringCreateMutableCopy(NULL, 0, execCommand);
		bpr = CFStringFind(str, CFSTR("$BUNDLE"), 0);
		if (bpr.location != kCFNotFound) {
			CFStringRef	bundlePath;

			bundlePath = CFURLCopyFileSystemPath(myBundleURL, kCFURLPOSIXPathStyle);
			CFStringReplace(str, bpr, bundlePath);
			CFRelease(bundlePath);
		}

		cmd = _SC_cfstring_to_cstring(str, NULL, 0, kCFStringEncodingASCII);
		CFRelease(str);
		if (!cmd) {
			SCLog(TRUE, LOG_DEBUG, CFSTR("  could not convert command to C string"));
			goto error;
		}

		/*
		 * get the UID/GID for the kickee
		 */
		if (isA_CFNumber(execUID)) {
			CFNumberGetValue(execUID, kCFNumberIntType, &reqUID);
		}

		if (isA_CFNumber(execGID)) {
			CFNumberGetValue(execGID, kCFNumberIntType, &reqGID);
		}

		nKeys = CFArrayGetCount(keys);
		argv  = CFAllocatorAllocate(NULL, (nKeys + 2) * sizeof(char *), 0);
		for (i = 0; i < (nKeys + 2); i++) {
			argv[i] = NULL;
		}

		/* create command name argument */
		if ((argv[0] = rindex(cmd, '/')) != NULL) {
			argv[0]++;
		} else {
			argv[0] = cmd;
		}

		/* create changed key arguments */
		if (isA_CFBoolean(passKeys) && CFBooleanGetValue(passKeys)) {
			for (i = 0; i < nKeys; i++) {
				CFStringRef	key = CFArrayGetValueAtIndex(keys, i);

				argv[i+1] = _SC_cfstring_to_cstring(key, NULL, 0, kCFStringEncodingASCII);
				if (!argv[i+1]) {
					SCLog(TRUE, LOG_DEBUG, CFSTR("  could not convert argument to C string"));
					goto error;
				}
			}
		}

		SCLog(TRUE,     LOG_NOTICE, CFSTR("executing %s"), cmd);
		SCLog(_verbose, LOG_DEBUG,  CFSTR("  current uid = %d, requested = %d"), geteuid(), reqUID);

		/* this kicker is now "running" */
		target->active = TRUE;

		(void)_SCDPluginExecCommand(booterExit,
			      target,
			      reqUID,
			      reqGID,
			      cmd,
			      argv);

//		CFAllocatorDeallocate(NULL, cmd);	/* clean up */
//		cmd = NULL;
	}
	ok = TRUE;

    error :

	if (keys)	CFRelease(keys);
	if (cmd)	CFAllocatorDeallocate(NULL, cmd);
	if (argv) {
		for (i = 0; i < nKeys; i++) {
			if (argv[i+1]) {
				CFAllocatorDeallocate(NULL, argv[i+1]);
			}
		}
		CFAllocatorDeallocate(NULL, argv);
	}

	if (!ok) {
		/*
		 * If the target action can't be performed this time then
		 * there's not much point in trying again. As such, I close
		 * the session and the kickee target released.
		 */
		cleanupKicker(target);
	}

	return;
}


static void
booterExit(pid_t pid, int status, struct rusage *rusage, void *context)
{
	CFStringRef	name;
	Boolean		ok	= TRUE;
	kickeeRef	target	= (kickeeRef)context;

	name = CFDictionaryGetValue(target->dict, CFSTR("name"));
	target->active = FALSE;
	if (WIFEXITED(status)) {
		SCLog(TRUE, LOG_DEBUG,
		      CFSTR("  target=%@: exit status = %d"),
		      name,
		      WEXITSTATUS(status));
		if (WEXITSTATUS(status) != 0) {
			ok = FALSE;
		}
	} else if (WIFSIGNALED(status)) {
		SCLog(TRUE, LOG_DEBUG,
		      CFSTR("  target=%@: terminated w/signal = %d"),
		      name,
		      WTERMSIG(status));
		ok = FALSE;
	} else {
		SCLog(TRUE, LOG_DEBUG,
		      CFSTR("  target=%@: exit status = %d"),
		      name,
		      status);
		ok = FALSE;
	}

	if (!ok) {
		if (CFDictionaryContainsKey(target->dict, CFSTR("postName"))) {
			CFDictionaryRef		oldDict	= target->dict;
			CFMutableDictionaryRef	newDict	= CFDictionaryCreateMutableCopy(NULL, 0, oldDict);

			/*
			 * if this target specifies both a BSD notification and
			 * a script to be executed then we want to continue to
			 * post the BSD notifications (and not execute the
			 * script).  As such, remove the script reference from
			 * the dictionary.
			 */
			CFDictionaryRemoveValue(newDict, CFSTR("execCommand"));
			CFDictionaryRemoveValue(newDict, CFSTR("execGID"));
			CFDictionaryRemoveValue(newDict, CFSTR("execUID"));
			CFDictionaryRemoveValue(newDict, CFSTR("changedKeysAsArguments"));
			target->dict = newDict;
			CFRelease(oldDict);
		} else {
			/*
			 * If the target action can't be performed this time then
			 * there's not much point in trying again. As such, I close
			 * the session and the kickee target released.
			 */
			cleanupKicker(target);
			target = NULL;
		}
	} 
	if (target != NULL && target->needsKick) {
		target->needsKick = FALSE;
		booter(target);
	}

	return;
}


static void
kicker(SCDynamicStoreRef store, CFArrayRef changedKeys, void *arg)
{
	CFIndex		i;
	CFIndex		n		= CFArrayGetCount(changedKeys);
	kickeeRef	target		= (kickeeRef)arg;

	/*
	 * Start a new kicker.  If a kicker was already active then flag
	 * the need for a second kick after the active one completes.
	 */

	/* create (or add to) the full list of keys that have changed */
	if (!target->changedKeys) {
		target->changedKeys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}
	for (i = 0; i < n; i++) {
		CFStringRef	key = CFArrayGetValueAtIndex(changedKeys, i);

		if (!CFArrayContainsValue(target->changedKeys,
					  CFRangeMake(0, CFArrayGetCount(target->changedKeys)),
					  key)) {
			CFArrayAppendValue(target->changedKeys, key);
		}
	}

	/*
	 * let 'er rip.
	 */
	booter(target);

	return;
}


/*
 * startKicker()
 *
 * The first argument is a dictionary representing the keys
 * which need to be monitored for a given "target" and what
 * action should be taken if a change in one of those keys
 * is detected.
 */
static void
startKicker(const void *value, void *context)
{
	CFMutableStringRef	name;
	CFArrayRef		keys;
	CFArrayRef		patterns;
	kickeeRef		target		= CFAllocatorAllocate(NULL, sizeof(kickee), 0);
	SCDynamicStoreContext	targetContext	= { 0, (void *)target, NULL, NULL, NULL };

	target->active		= FALSE;
	target->needsKick	= FALSE;
	target->dict		= CFRetain((CFDictionaryRef)value);
	target->store		= NULL;
	target->rl		= NULL;
	target->rls		= NULL;
	target->changedKeys	= NULL;

	name = CFStringCreateMutableCopy(NULL,
					 0,
					 CFDictionaryGetValue(target->dict, CFSTR("name")));
	SCLog(TRUE, LOG_DEBUG, CFSTR("Starting kicker for %@"), name);

	CFStringAppend(name, CFSTR(" \"Kicker\""));
	target->store = SCDynamicStoreCreate(NULL, name, kicker, &targetContext);
	CFRelease(name);
	if (!target->store) {
		SCLog(TRUE,
		      LOG_NOTICE,
		      CFSTR("SCDynamicStoreCreate() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}

	keys     = isA_CFArray(CFDictionaryGetValue(target->dict, CFSTR("keys")));
	patterns = isA_CFArray(CFDictionaryGetValue(target->dict, CFSTR("regexKeys")));
	if (!SCDynamicStoreSetNotificationKeys(target->store, keys, patterns)) {
		SCLog(TRUE,
		      LOG_NOTICE,
		      CFSTR("SCDynamicStoreSetNotifications() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}

	target->rl  = CFRunLoopGetCurrent();
	target->rls = SCDynamicStoreCreateRunLoopSource(NULL, target->store, 0);
	if (!target->rls) {
		SCLog(TRUE,
		      LOG_NOTICE,
		      CFSTR("SCDynamicStoreCreateRunLoopSource() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}

	CFRunLoopAddSource(target->rl, target->rls, kCFRunLoopDefaultMode);
	return;

    error :

	CFRelease(target->dict);
	if (target->store)	CFRelease(target->store);
	CFAllocatorDeallocate(NULL, target);
	return;
}


static CFArrayRef
getTargets(CFBundleRef bundle)
{
	Boolean			ok;
	CFArrayRef		targets;	/* The array of dictionaries
						   representing targets with
						   a "kick me" sign posted on
						   their backs. */
	CFURLRef		url;
	CFStringRef		xmlError;
	CFDataRef		xmlTargets	= NULL;

	/* locate the Kicker targets */
	url = CFBundleCopyResourceURL(bundle, CFSTR("Kicker"), CFSTR("xml"), NULL);
	if (url == NULL) {
		return NULL;
	}

	/* read the resource data */
	ok = CFURLCreateDataAndPropertiesFromResource(NULL, url, &xmlTargets, NULL, NULL, NULL);
	CFRelease(url);
	if (!ok || (xmlTargets == NULL)) {
		return NULL;
	}

	/* convert the XML data into a property list */
	targets = CFPropertyListCreateFromXMLData(NULL,
						  xmlTargets,
						  kCFPropertyListImmutable,
						  &xmlError);
	CFRelease(xmlTargets);
	if (targets == NULL) {
		if (xmlError != NULL) {
			SCLog(TRUE, LOG_DEBUG, CFSTR("getTargets(): %@"), xmlError);
			CFRelease(xmlError);
		}
		return NULL;
	}

	if (!isA_CFArray(targets)) {
		CFRelease(targets);
		targets = NULL;
	}

	return targets;
}


__private_extern__
void
load_Kicker(CFBundleRef bundle, Boolean bundleVerbose)
{
	CFArrayRef	targets;	/* The array of dictionaries representing targets
					 * with a "kick me" sign posted on their backs.*/

	if (bundleVerbose) {
		_verbose = TRUE;
	}

	SCLog(_verbose, LOG_DEBUG, CFSTR("load() called"));
	SCLog(_verbose, LOG_DEBUG, CFSTR("  bundle ID = %@"), CFBundleGetIdentifier(bundle));

	/* get the bundle's URL */
	myBundleURL = CFBundleCopyBundleURL(bundle);
	if (myBundleURL == NULL) {
		return;
	}

	/* get the targets */
	targets = getTargets(bundle);
	if (targets == NULL) {
		/* if nothing to do */
		CFRelease(myBundleURL);
		return;
	}

	/* start a kicker for each target */
	CFArrayApplyFunction(targets,
			     CFRangeMake(0, CFArrayGetCount(targets)),
			     startKicker,
			     NULL);
	CFRelease(targets);

	return;
}

#ifdef	MAIN
int
main(int argc, char * const argv[])
{
	_sc_log     = FALSE;
	_sc_verbose = (argc > 1) ? TRUE : FALSE;

	load_Kicker(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);
	CFRunLoopRun();
	/* not reached */
	exit(0);
	return 0;
}
#endif
