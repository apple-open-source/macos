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

#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define	SYSTEMCONFIGURATION_NEW_API
#include <SystemConfiguration/SystemConfiguration.h>


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

	/* helper thread (when using the CFRunLoop) */
	pthread_t		helper;

	/* dictionary associated with this target */
	CFDictionaryRef		dict;

	/* SCD session information for this target */
	SCDSessionRef		session;
} kickee, *kickeeRef;


static CFStringRef	myBundleDir;


int
execCommandWithUID(const char *command, uid_t reqUID)
{
	uid_t		curUID = geteuid();
	pid_t		pid;
	const char	*arg0;
	int		status;
	int		i;

	SCDLog(LOG_NOTICE, CFSTR("executing %s"), command);
	SCDLog(LOG_DEBUG,  CFSTR("  current uid = %d, requested = %d"), curUID, reqUID);

	pid = vfork();
	switch (pid) {
		case -1 :
			/* if error */
			status = W_EXITCODE(errno, 0);
			SCDLog(LOG_DEBUG,  CFSTR("vfork() failed: %s"), strerror(errno));
			return status;

		case 0 :
			/* if child */

			if ((arg0 = rindex(command, '/')) != NULL) {
				arg0++;
			} else {
				arg0 = command;
			}

			if ((curUID != reqUID) && (curUID == 0)) {
				(void) setuid(reqUID);
			}

			/* close any open FDs */
			for (i = getdtablesize()-1; i>=0; i--) close(i);
			open("/dev/null", O_RDWR, 0);
			dup(0);
			dup(0);

			/* execute requested command */
			execl(command, arg0, (char *)NULL);
			status = W_EXITCODE(errno, 0);
			SCDLog(LOG_DEBUG,  CFSTR("execl() failed: %s"), strerror(errno));
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
	SCDLog(LOG_DEBUG, CFSTR("SCDNotifierInformViaCallback() thread cancelled, cleaning up \"target\""));

	pthread_mutex_destroy(&target->lock);
	CFRelease(target->dict);
	CFAllocatorDeallocate(NULL, target);
	return;
}


void
cleanupCFObject(void *ptr)
{
	SCDLog(LOG_DEBUG, CFSTR("SCDNotifierInformViaCallback() thread cancelled, cleaning up \"theCommand\" string"));
	if (ptr != NULL)
		CFRelease(ptr);
	return;
}


void
cleanupCFAllocator(void *ptr)
{
	SCDLog(LOG_DEBUG, CFSTR("SCDNotifierInformViaCallback() thread cancelled, cleaning up \"cmd\" buffer"));
	if (ptr != NULL)
		CFAllocatorDeallocate(NULL, ptr);
	return;
}


void *
booter(void *arg)
{
	kickeeRef	target      = (kickeeRef)arg;
	CFStringRef	name        = CFDictionaryGetValue(target->dict, CFSTR("name"));
	CFStringRef	execCommand = CFDictionaryGetValue(target->dict, CFSTR("execCommand"));
	CFNumberRef	execUID     = CFDictionaryGetValue(target->dict, CFSTR("execUID"));

	SCDLog(LOG_DEBUG, CFSTR("Kicker callback, target=%@"), name);

	pthread_cleanup_push(cleanupTarget, (void *)target);

	if (execCommand != NULL) {
		CFMutableStringRef	theCommand;
		CFRange			bpr;
		int			cmdLen = CFStringGetLength(execCommand) + 1;
		char			*cmd = NULL;
		Boolean			ok;
		uid_t			reqUID = 0;
		int			status;

		theCommand = CFStringCreateMutableCopy(NULL, 0, execCommand);
		pthread_cleanup_push(cleanupCFObject, (void *)theCommand);

		bpr = CFStringFind(theCommand, CFSTR("$BUNDLE"), 0);
		if (bpr.location != kCFNotFound) {
			CFStringReplace(theCommand, bpr, myBundleDir);
		}

		cmdLen = CFStringGetLength(theCommand) + 1;
		cmd    = CFAllocatorAllocate(NULL, cmdLen, 0);
		pthread_cleanup_push(cleanupCFAllocator, (void *)cmd);

		ok = CFStringGetCString(theCommand,
					cmd,
					cmdLen,
					kCFStringEncodingMacRoman);
		if (ok) {
			if (execUID) {
				CFNumberGetValue(execUID, kCFNumberIntType, &reqUID);
			}

			pthread_mutex_lock(&target->lock);
			while (ok && target->needsKick) {
				target->needsKick = FALSE;
				pthread_mutex_unlock(&target->lock);

				status = execCommandWithUID(cmd, reqUID);
				if (WIFEXITED(status)) {
					SCDLog(LOG_DEBUG, CFSTR("  exit status = %d"), WEXITSTATUS(status));
					if (WEXITSTATUS(status) != 0) {
						ok = FALSE;
					}
				} else if (WIFSIGNALED(status)) {
					SCDLog(LOG_DEBUG, CFSTR("  terminated w/signal = %d"), WTERMSIG(status));
					ok = FALSE;
				} else {
					SCDLog(LOG_DEBUG, CFSTR("  exit status = %d"), status);
					ok = FALSE;
				}

				pthread_mutex_lock(&target->lock);
			}
			target->active = FALSE;
			pthread_mutex_unlock(&target->lock);
		} else {
			SCDLog(LOG_DEBUG, CFSTR("  could not convert command to C string"));
		}

		/*
		 * pop the cleanup routine for the "cmd" string. We end up deallocating
		 * the block of memory in the process.
		 */
		pthread_cleanup_pop(1);

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
			SCDLog(LOG_NOTICE, CFSTR("Kicker: retries disabled (command failed), target=%@"), name);
			SCDClose(&target->session);
		}
	}

	/*
	 * pop the cleanup routine for the "target" structure
	 */
	pthread_cleanup_pop(0);

	pthread_exit (NULL);
	return NULL;
}


boolean_t
kicker(SCDSessionRef session, void *arg)
{
	kickeeRef	target      = (kickeeRef)arg;
	CFStringRef	name        = CFDictionaryGetValue(target->dict, CFSTR("name"));
	SCDStatus	scd_status;
	CFArrayRef	changedKeys;

	/*
	 * Fetched the changed keys
	 */
	scd_status = SCDNotifierGetChanges(session, &changedKeys);
	if (scd_status == SCD_OK) {
		CFRelease(changedKeys);
	} else {
		SCDLog(LOG_ERR, CFSTR("SCDNotifierGetChanges() failed: %s"), SCDError(scd_status));
		/* XXX need to do something more with this FATAL error XXXX */
	}

	/*
	 * Start a new kicker.  If a kicker was already active then flag
	 * the need for a second kick after the active one completes.
	 */
	pthread_mutex_lock(&target->lock);
	if (target->active) {
		target->needsKick = TRUE;
		pthread_mutex_unlock(&target->lock);
		SCDLog(LOG_DEBUG, CFSTR("Kicker callback, target=%@ request queued"), name);
		return TRUE;
	}
	target->active    = TRUE;
	target->needsKick = TRUE;
	pthread_mutex_unlock(&target->lock);

	if (SCDOptionGet(session, kSCDOptionUseCFRunLoop)) {
		/*
		 * we are executing in the CFRunLoop. As such, this
		 * callback IS NOT running in a per-notification
		 * thread and since we don't want to block other
		 * processes while we are waiting for our command
		 * execution to complete we need to spawn ourselves.
		 */
		pthread_attr_t	tattr;

		pthread_attr_init(&tattr);
		pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
		pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
		pthread_attr_setstacksize(&tattr, 96 * 1024); // each thread gets a 96K stack
		pthread_create(&target->helper, &tattr, booter, (void *)target);
		pthread_attr_destroy(&tattr);
	} else {
		/*
		 * we are not executing in the CFRunLoop. As such,
		 * this callback IS running in a per-notification
		 * thread.
		 */
		booter(target);
	}

	return TRUE;
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
	kickeeRef		target = CFAllocatorAllocate(NULL, sizeof(kickee), 0);
	CFMutableStringRef	name;
	SCDStatus		scd_status;
	CFArrayRef		keys;
	CFIndex			i;

	pthread_mutex_init(&target->lock, NULL);
	target->active		= FALSE;
	target->needsKick	= FALSE;
	target->dict		= (CFDictionaryRef)value;

	name = CFStringCreateMutableCopy(NULL,
					 0,
					 CFDictionaryGetValue(target->dict, CFSTR("name")));
	SCDLog(LOG_DEBUG, CFSTR("Starting kicker for %@"), name);

	CFStringAppend(name, CFSTR(" \"Kicker\""));
	scd_status = SCDOpen(&target->session, name);
	CFRelease(name);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_NOTICE, CFSTR("SCDOpen() failed: %s"), SCDError(scd_status));
		CFAllocatorDeallocate(NULL, target);
		return;
	}

	keys = CFDictionaryGetValue(target->dict, CFSTR("keys"));
	if (keys != NULL) {
		for (i=0; i<CFArrayGetCount(keys); i++) {
			scd_status = SCDNotifierAdd(target->session,
						    CFArrayGetValueAtIndex(keys, i),
						    0);
			if (scd_status != SCD_OK) {
				SCDLog(LOG_NOTICE, CFSTR("SCDNotifierAdd() failed: %s"), SCDError(scd_status));
				(void) SCDClose(&target->session);
				CFAllocatorDeallocate(NULL, target);
				return;
			}
		}
	}

	keys = CFDictionaryGetValue(target->dict, CFSTR("regexKeys"));
	if (keys != NULL) {
		for (i=0; i<CFArrayGetCount(keys); i++) {
			scd_status = SCDNotifierAdd(target->session,
						    CFArrayGetValueAtIndex(keys, i),
						    kSCDRegexKey);
			if (scd_status != SCD_OK) {
				SCDLog(LOG_NOTICE, CFSTR("SCDNotifierAdd() failed: %s"), SCDError(scd_status));
				(void) SCDClose(&target->session);
				CFAllocatorDeallocate(NULL, target);
				return;
			}
		}
	}

	CFRetain(target->dict);

	scd_status = SCDNotifierInformViaCallback(target->session, kicker, target);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_NOTICE, CFSTR("SCDNotifierInformViaCallback() failed: %s"), SCDError(scd_status));
		(void) SCDClose(&target->session);
		CFRelease(target->dict);
		CFAllocatorDeallocate(NULL, target);
		return;
	}

	return;
}


CFArrayRef
getTargets(const char *bundleName, const char *bundleDir)
{
	char			*targetPath;
	int			fd;
	struct stat		statBuf;
	CFMutableDataRef	xmlTargets;
	CFArrayRef		targets;	/* The array of dictionaries
						   representing targets with
						   a "kick me" sign posted on
						   their backs.*/
	CFStringRef		xmlError;

	/*
	 * Given a bundle name (e.g. "foo") and the associated bundle directory
	 * path (e.g. /System/Library/SystemConfiguration/foo.bundle) we build
	 * a path to the bundle's "Resources" directory and the XML property
	 * list which contains information on the processes to be kicked. The
	 * resulting path would look like the following:
	 *
	 *   /System/Library/SystemConfiguration/foo.bundle/Resources/foo[_debug]
	 */
	 targetPath = malloc(strlen(bundleDir) + 11 + strlen(bundleName) + 5);
	 (void) sprintf(targetPath, "%s/Resources/%s.xml", bundleDir, bundleName);
	 if ((fd = open(targetPath, O_RDONLY, 0)) == -1) {
		 SCDLog(LOG_NOTICE, CFSTR("%s start(): %s not found"), bundleName, targetPath);
		 free(targetPath);
		 return NULL;
	 }
	 free(targetPath);

	 /* Get the size of the XML data */
	 if (fstat(fd, &statBuf) < 0) {
		 (void) close(fd);
		 return NULL;
	 }
	 if ((statBuf.st_mode & S_IFMT) != S_IFREG) {
		 (void) close(fd);
		 return NULL;
	 }

	 xmlTargets = CFDataCreateMutable(NULL, statBuf.st_size);
	 CFDataSetLength(xmlTargets, statBuf.st_size);
	 if (read(fd, (void *)CFDataGetBytePtr(xmlTargets), statBuf.st_size) != statBuf.st_size) {
		 CFRelease(xmlTargets);
		 (void) close(fd);
		 return NULL;
	 }
	 (void) close(fd);

	 targets = CFPropertyListCreateFromXMLData(NULL,
						   xmlTargets,
						   kCFPropertyListImmutable,
						   &xmlError);
	 CFRelease(xmlTargets);
	 if (xmlError) {
		 SCDLog(LOG_DEBUG, CFSTR("CFPropertyListCreateFromXMLData() start: %s"), xmlError);
		 return NULL;
	 }

	 return targets ;
}


void
start(const char *bundleName, const char *bundleDir)
{
	CFArrayRef	targets;	/* The array of dictionaries representing targets
					 * with a "kick me" sign posted on their backs.*/

	SCDLog(LOG_DEBUG, CFSTR("start() called"));
	SCDLog(LOG_DEBUG, CFSTR("  bundle name      = %s"), bundleName);
	SCDLog(LOG_DEBUG, CFSTR("  bundle directory = %s"), bundleDir);

	/* save the path to the bundle directory */
	myBundleDir = CFStringCreateWithCString(NULL, bundleDir, kCFStringEncodingMacRoman);

	/* get the targets */
	targets = getTargets(bundleName, bundleDir);
	if (targets == NULL) {
		/* if nothing to do */
		CFRelease(myBundleDir);
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
