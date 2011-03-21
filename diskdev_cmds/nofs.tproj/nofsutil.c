/*
 * Copyright (c) 2009-2011 Apple Inc. All rights reserved.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/loadable_fs.h>
#include <fcntl.h>
#include <sys/time.h>
#include <spawn.h>
#include <syslog.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitrationPrivate.h>

#include "ui.h"

#define COOKIE_FILE_PATH "/var/run/nofsalertcookie"
/* Do not display another alert unless it has been at
 * least this many seconds since the last one. */
#define ALERT_INTERVAL 600

const char *progname;

static void
usage(void)
{
	fprintf(stderr, "usage: %s -p device\n", progname);
	exit(FSUR_INVAL);
}

/*
 * Tell Disk Arbitration not to try to mount the disk.
 */
static void
suppress_mount(char *devname)
{
	DASessionRef session = (DASessionRef)0;
	DADiskRef disk = (DADiskRef)0;

	session = DASessionCreate(kCFAllocatorDefault);
	if (session == (DASessionRef)0)
		goto exit;

	disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, devname);
	if (disk == (DADiskRef)0)
		goto exit;

	/* Tell DA not to auto-mount this disk (since we're lying about it being
	 * mountable in the first place). */
	DADiskSetOptions(disk, kDADiskOptionMountAutomatic, FALSE);

exit:
	if (disk)
		CFRelease(disk);
	if (session)
		CFRelease(session);
}

/*
 * Display alert, if enough time has elapsed since the last alert.
 * We keep track of this using the mtime on a cookie file.
 */
static void
display_alert_if_timeout_elapsed()
{
	int fd, ret, err;
	struct stat st;
	struct timeval tv;
	time_t last;
	time_t now;

top:
	fd = open(COOKIE_FILE_PATH, O_CREAT|O_EXCL|O_WRONLY, 0644);

	if (fd == -1) {
		err = errno;
		if (err != EEXIST) {
			goto out;
		}

		/* Cookie file already exists, check mtime */
		ret = lstat(COOKIE_FILE_PATH, &st);
		if (ret == -1) {
			goto out;
		}

		ret = gettimeofday(&tv, 0);
		if (ret == -1) {
			goto out;
		}
		now = tv.tv_sec;
		last = st.st_mtimespec.tv_sec;

		/* If the cookie file is not a regular file or has an mtime 
		 * in the future, remove it and recreate it. */
		if (((st.st_mode & S_IFMT) != S_IFREG) || last > now) {
			ret = remove(COOKIE_FILE_PATH);
			if (ret == -1) {
				goto out;
			} else {
				goto top;
			}
		}

		if ((now - last) < ALERT_INTERVAL) {
			goto out;
		}
	} else {
		close(fd);
	}

	/* Update cookie file timestamp */
	ret = utimes(COOKIE_FILE_PATH, NULL);

	/* Display the actual alert */
	AlertCoreStorageNotInstalled();

out:
	return;
}

/*
 * Write our (localized) fake filesystem name to stdout.
 */
static void
print_fsname()
{
	char cstr[256];
	CFStringRef str;
	str = CFCopyLocalizedString(CFSTR("Incompatible Format"), "Incompatible Format");
	CFStringGetCString(str, cstr, 256, kCFStringEncodingUTF8);
	(void) fprintf(stdout, "%s", cstr);
	fflush(stdout);

	if (str)
		CFRelease(str);
}

static void
idle_cb(void *ctx)
{
	CFRunLoopStop(CFRunLoopGetCurrent());
}

static void
disk_appeared_cb(DADiskRef disk, void *ctx)
{
	CFDictionaryRef description = NULL;
	CFBooleanRef mountable;
	CFStringRef content = NULL;

	if (!ctx || !disk) return;

	description = DADiskCopyDescription(disk);
	if (!description) return;

	mountable = CFDictionaryGetValue(description, kDADiskDescriptionVolumeMountableKey);
	if (!mountable) goto out;
	content = CFDictionaryGetValue(description, kDADiskDescriptionMediaContentKey);
	if (!content) goto out;

	if (CFBooleanGetValue(mountable) &&
	        CFStringCompare(content, CFSTR("53746F72-6167-11AA-AA11-00306543ECAC"),
	        kCFCompareCaseInsensitive) != kCFCompareEqualTo &&
	        CFStringCompare(content, CFSTR("426F6F74-0000-11AA-AA11-00306543ECAC"),
	        kCFCompareCaseInsensitive) != kCFCompareEqualTo) {
		/* The disk is marked mountable and isn't CoreStorage or Apple_Boot,
		 * which means that it actually is mountable (since we lie and mark
		 * CoreStorage disks as mountable). */
		*((bool *)ctx) = true;
	}

out:
	if (description)
		CFRelease(description);
}

/*
 * Check if there are any mountable partitions on the disk. If there are any
 * mountable partitions, then we don't want to display the dialog. CoreStorage
 * and Apple_Boot partitions don't count as mountable for our purposes.
 */
static bool
check_all_partitions(char *devname)
{
	bool found_mountable_fs = false;
	SInt32 rc;
	CFNumberRef num = NULL;
	CFDictionaryRef description = NULL, matchDict = NULL;

	DASessionRef session = (DASessionRef)0;
	DADiskRef disk = (DADiskRef)0;

	session = DASessionCreate(kCFAllocatorDefault);
	if (session == (DASessionRef)0)
		goto exit;

	disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, devname);
	if (disk == (DADiskRef)0)
		goto exit;

	description = DADiskCopyDescription(disk);
	num = CFDictionaryGetValue(description, kDADiskDescriptionMediaBSDUnitKey);
	matchDict = CFDictionaryCreate(kCFAllocatorDefault,
	    (const void **)&kDADiskDescriptionMediaBSDUnitKey,
	    (const void **)&num, 1, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);

	DASessionScheduleWithRunLoop(session, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	DARegisterIdleCallback(session, idle_cb, &found_mountable_fs);
	DARegisterDiskAppearedCallback(session, matchDict, disk_appeared_cb, &found_mountable_fs);
	rc = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 10, false);
	DAUnregisterCallback(session, idle_cb, &found_mountable_fs);
	DAUnregisterCallback(session, disk_appeared_cb, &found_mountable_fs);
	DASessionUnscheduleFromRunLoop(session, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

exit:
	if (disk)
		CFRelease(disk);
	if (session)
		CFRelease(session);
	if (description)
		CFRelease(description);
	if (matchDict)
		CFRelease(matchDict);

	return (!found_mountable_fs);
}

static void
console_user_changed_cb(SCDynamicStoreRef store, CFArrayRef changedKeys, void *context)
{
    CFStringRef                 user;
    uid_t                       uid;
    gid_t                       gid;

    user = SCDynamicStoreCopyConsoleUser(store, &uid, &gid);
    if (user != NULL) {
        CFRelease(user);
		CFRunLoopStop(CFRunLoopGetCurrent());
    }
}


/*
 * Wait until a console user logs in (or don't wait if one is already logged in).
 */
static bool
wait_for_console_user(char *devname)
{
	CFStringRef             key;
	CFMutableArrayRef       keys;
	Boolean                 ok;
	SCDynamicStoreRef       store = NULL;
	CFRunLoopSourceRef      rls;
	CFStringRef             user;
	uid_t                   uid;
	gid_t                   gid;
	bool                    ret = false;

	store = SCDynamicStoreCreate(NULL, CFSTR("com.apple.nofs"),
	    console_user_changed_cb, NULL);
	if (store == NULL) {
		return ret;
	}

	/* check if a console user is already logged in */
	user = SCDynamicStoreCopyConsoleUser(store, &uid, &gid);
	if (user != NULL) {
		CFRelease(user);
		ret = true;
		goto out;
	}

	/* wait for a notification that a console user logged in */
	keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	key = SCDynamicStoreKeyCreateConsoleUser(NULL);
	CFArrayAppendValue(keys, key);
	CFRelease(key);
	ok = SCDynamicStoreSetNotificationKeys(store, keys, NULL);
	CFRelease(keys);
	if (!ok) {
		syslog(LOG_ERR, "nofs: SCDynamicStoreSetNotificationKeys() failed");
		goto out;
	}
	rls = SCDynamicStoreCreateRunLoopSource(NULL, store, -1);
	if (rls == NULL) {
		syslog(LOG_ERR, "nofs: SCDynamicStoreCreateRunLoopSource() failed");
		goto out;
	}

	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
	CFRunLoopRun();
	CFRunLoopSourceInvalidate(rls);
	CFRelease(rls);

	ret = true;

out:
	if (store) {
		CFRelease(store);
	}
	return ret;
}

int
main(int argc, char **argv)
{
	char what;
	char *devname;
	int ret = FSUR_UNRECOGNIZED;
	int ii;

	/* save & strip off program name */
	progname = argv[0];
	argc--;
	argv++;

	if (argc < 2 || argv[0][0] != '-') {
		usage();
	}

	what = argv[0][1];
	devname = argv[1];

	switch (what) {
	case FSUC_PROBE:
		ret = FSUR_RECOGNIZED;
		print_fsname();
		suppress_mount(devname);

		/* make sure the pipe file descriptor(s) are closed to avoid
		 * deadlocking with diskarbitrationd! */
		for (ii = 0; ii < 20; ii++) {
			(void)close(ii);
		}

		/* spawn a child to hang out and wait for a console user to log in
		 * necessary), then display the dialog */
		const char* argv[] = { progname, "-d", devname, NULL };
		const char* env[] = { NULL };
		pid_t child = 0;
		int rc = posix_spawn(&child, progname, NULL, NULL, (char**)argv, (char**)env);
		if (rc) {
			syslog(LOG_ERR, "nofs: posix_spawn failed with %d\n", rc);
		}
		break;
	case 'd':
		if (wait_for_console_user(devname) && check_all_partitions(devname)) {
			display_alert_if_timeout_elapsed();
		}
		break;
	default:
		usage();
	}

	return ret;
}
