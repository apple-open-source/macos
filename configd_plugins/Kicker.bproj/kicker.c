/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
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
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

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

void	booter(kickeeRef target);
void	booterExit(pid_t pid, int status, struct rusage *rusage, void *context);


void
cleanupKicker(kickeeRef target)
{
	CFStringRef		name	= CFDictionaryGetValue(target->dict, CFSTR("name"));

	SCLog(TRUE, LOG_NOTICE,
	      CFSTR("  target=%@: disabled"),
	      name);
	CFRunLoopRemoveSource(target->rl, target->rls, kCFRunLoopDefaultMode);
	CFRelease(target->store);
	if (target->dict)		CFRelease(target->dict);
	if (target->changedKeys)	CFRelease(target->changedKeys);
	CFAllocatorDeallocate(NULL, target);
}


void
booter(kickeeRef target)
{
	char			**argv		= NULL;
	CFRange			bpr;
	char			*cmd		= NULL;
	CFStringRef		execCommand	= CFDictionaryGetValue(target->dict, CFSTR("execCommand"));
	CFNumberRef		execGID		= CFDictionaryGetValue(target->dict, CFSTR("execGID"));
	CFNumberRef		execUID		= CFDictionaryGetValue(target->dict, CFSTR("execUID"));
	int			i;
	CFArrayRef		keys		= NULL;
	int			len;
	CFStringRef		name		= CFDictionaryGetValue(target->dict, CFSTR("name"));
	int			nKeys		= 0;
	Boolean			ok		= FALSE;
	CFBooleanRef		passKeys	= CFDictionaryGetValue(target->dict, CFSTR("changedKeysAsArguments"));
	gid_t			reqGID		= 0;
	uid_t			reqUID		= 0;
	CFMutableStringRef	str;

	SCLog(_verbose, LOG_DEBUG, CFSTR("Kicker callback, target=%@"), name);

	if (!isA_CFString(execCommand)) {
		goto error;	/* if no command */
	}

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

	len = CFStringGetLength(str) + 1;
	cmd = CFAllocatorAllocate(NULL, len, 0);
	ok = CFStringGetCString(str,
				cmd,
				len,
				kCFStringEncodingMacRoman);
	CFRelease(str);
	if (!ok) {
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

	/*
	 * get the arguments for the kickee
	 */
	keys = target->changedKeys;
	target->changedKeys = NULL;
	target->active      = TRUE;	/* this kicker is now "running" */
	target->needsKick   = FALSE;	/* allow additional requests to be queued */

	nKeys = CFArrayGetCount(keys);
	argv  = CFAllocatorAllocate(NULL, (nKeys + 2) * sizeof(char *), 0);
	for (i=0; i<(nKeys + 2); i++) {
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
		for (i=0; i<nKeys; i++) {
			CFStringRef	key = CFArrayGetValueAtIndex(keys, i);

			len = CFStringGetLength(key) + 1;
			argv[i+1] = CFAllocatorAllocate(NULL, len, 0);
			ok = CFStringGetCString(key,
						argv[i+1],
						len,
						kCFStringEncodingMacRoman);
			if (!ok) {
				SCLog(TRUE, LOG_DEBUG,
				      CFSTR("  could not convert argument to C string"));
				goto error;
			}
		}
	}

	SCLog(TRUE,     LOG_NOTICE, CFSTR("executing %s"), cmd);
	SCLog(_verbose, LOG_DEBUG,  CFSTR("  current uid = %d, requested = %d"), geteuid(), reqUID);

	(void)_SCDPluginExecCommand(booterExit,
				   target,
				   reqUID,
				   reqGID,
				   cmd,
				   argv);

    error :

	if (keys)	CFRelease(keys);
	if (cmd)	CFAllocatorDeallocate(NULL, cmd);
	if (argv) {
		for (i=0; i<nKeys; i++) {
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


void
booterExit(pid_t pid, int status, struct rusage *rusage, void *context)
{
	Boolean		again	= FALSE;
	CFStringRef	name;
	Boolean		ok	= TRUE;
	kickeeRef	target	= (kickeeRef)context;

	name = CFDictionaryGetValue(target->dict, CFSTR("name"));

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

	if (ok) {
		if (target->needsKick) {
			again = TRUE;		/* one more time */
		} else {
			target->active = FALSE;	/* normal exit, no more requests */
		}
	}

	if (!ok) {
		/*
		 * If the target action can't be performed this time then
		 * there's not much point in trying again. As such, I close
		 * the session and the kickee target released.
		 */
		cleanupKicker(target);
	} else if (again) {
		booter(target);
	}

	return;
}


void
kicker(SCDynamicStoreRef store, CFArrayRef changedKeys, void *arg)
{
	CFIndex		i;
	kickeeRef	target		= (kickeeRef)arg;

	/*
	 * Start a new kicker.  If a kicker was already active then flag
	 * the need for a second kick after the active one completes.
	 */

	/* create (or add to) the full list of keys that have changed */
	if (!target->changedKeys) {
		target->changedKeys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}
	for (i=0; i<CFArrayGetCount(changedKeys); i++) {
		CFStringRef	key = CFArrayGetValueAtIndex(changedKeys, i);

		if (!CFArrayContainsValue(target->changedKeys,
					  CFRangeMake(0, CFArrayGetCount(target->changedKeys)),
					  key)) {
			CFArrayAppendValue(target->changedKeys, key);
		}
	}

	if (target->active) {
		CFStringRef	name = CFDictionaryGetValue(target->dict, CFSTR("name"));

		/* we need another kick! */
		target->needsKick = TRUE;

		SCLog(_verbose, LOG_DEBUG, CFSTR("Kicker callback, target=%@ request queued"), name);
		return;
	}

	/* start a new kicker */
	target->active = TRUE;

	/*
	 * let 'er rip.
	 */
	booter(target);

	return;
}


/*
 * startkicker()
 *
 * The first argument is a dictionary representing the keys
 * which need to be monitored for a given "target" and what
 * action should be taken if a change in one of those keys
 * is detected.
 */
void
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


CFArrayRef
getTargets(CFBundleRef bundle)
{
	int			fd;
	Boolean			ok;
	struct stat		statBuf;
	CFArrayRef		targets;	/* The array of dictionaries
						   representing targets with
						   a "kick me" sign posted on
						   their backs. */
	char			targetPath[MAXPATHLEN];
	CFURLRef		url;
	CFStringRef		xmlError;
	CFMutableDataRef	xmlTargets;

	/* locate the Kicker targets */
	url = CFBundleCopyResourceURL(bundle, CFSTR("Kicker"), CFSTR("xml"), NULL);
	if (!url) {
		return NULL;
	}

	ok = CFURLGetFileSystemRepresentation(url,
					      TRUE,
					      (UInt8 *)&targetPath,
					      sizeof(targetPath));
	CFRelease(url);
	if (!ok) {
		return NULL;
	}

	/* open the file */
	if ((fd = open(targetPath, O_RDONLY, 0)) == -1) {
		SCLog(TRUE, LOG_NOTICE,
		      CFSTR("%@ load(): %s not found"),
		      CFBundleGetIdentifier(bundle),
		      targetPath);
		return NULL;
	}

	/* get the type and size of the XML data file */
	if (fstat(fd, &statBuf) < 0) {
		(void) close(fd);
		return NULL;
	}

	/* check that its a regular file */
	if ((statBuf.st_mode & S_IFMT) != S_IFREG) {
		(void) close(fd);
		return NULL;
	}

	/* load the file contents */
	xmlTargets = CFDataCreateMutable(NULL, statBuf.st_size);
	CFDataSetLength(xmlTargets, statBuf.st_size);
	if (read(fd, (void *)CFDataGetMutableBytePtr(xmlTargets), statBuf.st_size) != statBuf.st_size) {
		CFRelease(xmlTargets);
		(void) close(fd);
		return NULL;
	}
	(void) close(fd);

	/* convert the XML data into a property list */
	targets = CFPropertyListCreateFromXMLData(NULL,
						  xmlTargets,
						  kCFPropertyListImmutable,
						  &xmlError);
	CFRelease(xmlTargets);
	if (!targets) {
		if (xmlError) {
			SCLog(TRUE, LOG_DEBUG,
			      CFSTR("CFPropertyListCreateFromXMLData() start: %@"),
			      xmlError);
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


void
load(CFBundleRef bundle, Boolean bundleVerbose)
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
	if (!myBundleURL) {
		return;
	}

	/* get the targets */
	targets = getTargets(bundle);
	if (!targets) {
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

	load(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);
	CFRunLoopRun();
	/* not reached */
	exit(0);
	return 0;
}
#endif
