/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * June 23, 2001		Allan Nathanson <ajn@apple.com>
 * - update to public SystemConfiguration.framework APIs
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
#include <SystemConfiguration/SCValidation.h>


/*
 * Information maintained for each to-be-kicked registration.
 */
typedef struct {
	/*
	 * flags to ensure we don't kick while a kicker is
	 * already active and to ensure that if we received a
	 * kick request while the kicking that we do it again.
	 */
	pthread_mutex_t		lock;
	boolean_t		active;
	boolean_t		needsKick;

	/* helper thread */
	pthread_t		helper;

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


int
execCommandWithUID(const char *command, const char *argv[], uid_t reqUID)
{
	uid_t		curUID = geteuid();
	pid_t		pid;
	int		status;
	int		i;

	SCLog(TRUE,     LOG_NOTICE, CFSTR("executing %s"), command);
	SCLog(_verbose, LOG_DEBUG,  CFSTR("  current uid = %d, requested = %d"), curUID, reqUID);

	pid = /*v*/fork();
	switch (pid) {
		case -1 :
			/* if error */
			status = W_EXITCODE(errno, 0);
			SCLog(TRUE, LOG_DEBUG,  CFSTR("vfork() failed: %s"), strerror(errno));
			return status;

		case 0 :
			/* if child */

			if ((curUID != reqUID) && (curUID == 0)) {
				(void) setuid(reqUID);
			}

			/* close any open FDs */
			for (i = getdtablesize()-1; i>=0; i--) close(i);
			open("/dev/null", O_RDWR, 0);
			dup(0);
			dup(0);

			/* execute requested command */
			execv(command, argv);
			status = W_EXITCODE(errno, 0);
			SCLog(TRUE, LOG_DEBUG,  CFSTR("execl() failed: %s"), strerror(errno));
			_exit (WEXITSTATUS(status));
	}

	/* if parent */
	pid = wait4(pid, &status, 0, 0);
	return (pid == -1) ? -1 : status;
}


void
cleanupTarget(void *ptr)
{
	kickeeRef	target = (kickeeRef)ptr;

	/*
	 * This function should only be called when the notifier thread has been
	 * cancelled. As such, anything related to the session has already been
	 * cleaned up so we can simply release the target dictionary and then
	 * remove the target itself.
	 */
	SCLog(_verbose, LOG_DEBUG, CFSTR("SCDynamicStore callback thread cancelled, cleaning up \"target\""));
	pthread_mutex_destroy(&target->lock);
	if (target->dict)		CFRelease(target->dict);
	if (target->changedKeys)	CFRelease(target->changedKeys);
	CFAllocatorDeallocate(NULL, target);
	return;
}


void
cleanupCFObject(void *ptr)
{
	SCLog(_verbose, LOG_DEBUG, CFSTR("SCDynamicStore callback thread cancelled, cleaning up \"theCommand\" string"));
	if (ptr != NULL)
		CFRelease(ptr);
	return;
}


typedef struct {
	char	*cmd;
	char	**argv;
} execInfo, *execInfoRef;


void
cleanupArgInfo(void *ptr)
{
	execInfoRef	info	= (execInfoRef)ptr;

	SCLog(_verbose, LOG_DEBUG, CFSTR("SCDynamicStore callback thread cancelled, cleaning up \"cmd\" buffer"));
	if (info == NULL) {
		return;
	}

	/* clean up the command */
	if (info->cmd) {
		CFAllocatorDeallocate(NULL, info->cmd);
	}

	/* clean up the arguments */
	if (info->argv) {
		int	argc = 1;	/* skip argv[0], command name */

		while (info->argv[argc] != NULL) {
			CFAllocatorDeallocate(NULL, info->argv[argc]);
			argc++;
		}
		CFAllocatorDeallocate(NULL, info->argv);
	}

	return;
}


void *
booter(void *arg)
{
	kickeeRef	target		= (kickeeRef)arg;
	CFStringRef	name		= CFDictionaryGetValue(target->dict, CFSTR("name"));
	CFStringRef	execCommand	= CFDictionaryGetValue(target->dict, CFSTR("execCommand"));
	CFNumberRef	execUID		= CFDictionaryGetValue(target->dict, CFSTR("execUID"));
	CFBooleanRef	passKeys	= CFDictionaryGetValue(target->dict, CFSTR("changedKeysAsArguments"));

	SCLog(_verbose, LOG_DEBUG, CFSTR("Kicker callback, target=%@"), name);

	pthread_cleanup_push(cleanupTarget, (void *)target);

	if (isA_CFString(execCommand)) {
		CFMutableStringRef	theCommand;
		CFRange			bpr;
		Boolean			ok	= TRUE;
		uid_t			reqUID	= 0;
		int			status;

		theCommand = CFStringCreateMutableCopy(NULL, 0, execCommand);
		pthread_cleanup_push(cleanupCFObject, (void *)theCommand);

		bpr = CFStringFind(theCommand, CFSTR("$BUNDLE"), 0);
		if (bpr.location != kCFNotFound) {
			CFStringRef	bundlePath;

			bundlePath = CFURLCopyFileSystemPath(myBundleURL, kCFURLPOSIXPathStyle);
			CFStringReplace(theCommand, bpr, bundlePath);
			CFRelease(bundlePath);
		}

		if (isA_CFNumber(execUID)) {
			CFNumberGetValue(execUID, kCFNumberIntType, &reqUID);
		}

		pthread_mutex_lock(&target->lock);
		while (ok && target->needsKick) {
			int		i;
			execInfo	info;
			int		len;
			int		nKeys	= CFArrayGetCount(target->changedKeys);

			info.cmd = NULL;
			info.argv = CFAllocatorAllocate(NULL,
							(nKeys + 2) * sizeof(char *),
							0);
			for (i=0; i<(nKeys + 2); i++) {
				info.argv[i] = NULL;
			}
			pthread_cleanup_push(cleanupArgInfo, &info);

			/* convert command */
			len = CFStringGetLength(theCommand) + 1;
			info.cmd = CFAllocatorAllocate(NULL, len, 0);
			ok = CFStringGetCString(theCommand,
						info.cmd,
						len,
						kCFStringEncodingMacRoman);
			if (!ok) {
				SCLog(TRUE, LOG_DEBUG, CFSTR("  could not convert command to C string"));
				goto error;
			}

			/* create command name argument */
			if ((info.argv[0] = rindex(info.cmd, '/')) != NULL) {
				info.argv[0]++;
			} else {
				info.argv[0] = info.cmd;
			}

			/* create changed key arguments */
			if (isA_CFBoolean(passKeys) && CFBooleanGetValue(passKeys)) {
				for (i=0; i<nKeys; i++) {
					CFStringRef	key = CFArrayGetValueAtIndex(target->changedKeys, i);

					len = CFStringGetLength(key) + 1;
					info.argv[i+1] = CFAllocatorAllocate(NULL, len, 0);
					ok = CFStringGetCString(key,
								info.argv[i+1],
								len,
								kCFStringEncodingMacRoman);
					if (!ok) {
						SCLog(TRUE, LOG_DEBUG,
						      CFSTR("  could not convert argument to C string"));
						goto error;
					}
				}
			}

			CFRelease(target->changedKeys);
			target->changedKeys = NULL;

			/* allow additional requests to be queued */
			target->needsKick = FALSE;

			pthread_mutex_unlock(&target->lock);

			status = execCommandWithUID(info.cmd, info.argv, reqUID);
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

		    error :

			/*
			 * pop the cleanup routine for the "info" structure. We end
			 * up deallocating the associated memory in the process.
			 */
			pthread_cleanup_pop(1);

			pthread_mutex_lock(&target->lock);
		}
		target->active = FALSE;
		pthread_mutex_unlock(&target->lock);

		/*
		 * pop the cleanup routine for the "theCommand" CFString. We end up calling
		 * CFRelease() in the process.
		 */
		pthread_cleanup_pop(1);

		if (!ok) {
			/*
			 * We are executing within the context of a callback thread. My gut
			 * tells me that if the target action can't be performed this time
			 * then there's not much point in trying again. As such, I close the
			 * session (which will result in this thread being cancelled) and
			 * the kickee target released.
			 */
			SCLog(TRUE, LOG_NOTICE,
			      CFSTR("Kicker: retries disabled (command failed), target=%@"),
			      name);
			CFRunLoopRemoveSource(target->rl, target->rls, kCFRunLoopDefaultMode);
			CFRelease(target->store);
		}
	}

	/*
	 * pop the cleanup routine for the "target" structure
	 */
	pthread_cleanup_pop(0);

	pthread_exit (NULL);
	return NULL;
}


void
kicker(SCDynamicStoreRef store, CFArrayRef changedKeys, void *arg)
{
	CFIndex		i;
	kickeeRef	target		= (kickeeRef)arg;
	pthread_attr_t	tattr;

	/*
	 * Start a new kicker.  If a kicker was already active then flag
	 * the need for a second kick after the active one completes.
	 */
	pthread_mutex_lock(&target->lock);

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

		pthread_mutex_unlock(&target->lock);
		SCLog(_verbose, LOG_DEBUG, CFSTR("Kicker callback, target=%@ request queued"), name);
		return;
	}

	/* we're starting a new kicker */
	target->active    = TRUE;
	target->needsKick = TRUE;
	pthread_mutex_unlock(&target->lock);

	/*
	 * since we are executing in the CFRunLoop and
	 * since we don't want to block other processes
	 * while we are waiting for our command execution
	 * to complete we need to spawn ourselves.
	 */
	pthread_attr_init(&tattr);
	pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setstacksize(&tattr, 96 * 1024); // each thread gets a 96K stack
	pthread_create(&target->helper, &tattr, booter, (void *)target);
	pthread_attr_destroy(&tattr);

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

	pthread_mutex_init(&target->lock, NULL);
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
	if (read(fd, (void *)CFDataGetBytePtr(xmlTargets), statBuf.st_size) != statBuf.st_size) {
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
main(int argc, char **argv)
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
