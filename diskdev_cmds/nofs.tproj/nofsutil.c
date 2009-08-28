/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/loadable_fs.h>
#include <fcntl.h>
#include <sys/time.h>

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitrationPrivate.h>

#include "ui.h"

/* We will be run as root, so we use /var/run rather than /tmp */
#define COOKIE_FILE "/var/run/nofsalertcookie"
/* Do not display another alert unless it has been at 
 * least this many seconds since the last one. */
#define ALERT_INTERVAL 3600

const char *progname;

static void
usage(void)
{
	fprintf(stderr, "usage: %s -p device\n", progname);
	exit(FSUR_INVAL);
}

/*
 * Tell Disk Arbitration not to try to automatically mount the disk for us.
 */
static void
suppress_mount(char *devname)
{
	DASessionRef session =(DASessionRef) 0;
	DADiskRef disk = (DADiskRef)0;

	session = DASessionCreate(kCFAllocatorDefault);
	if (session == (DASessionRef)0)
		goto exit;

	disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, devname);
	if (disk == (DADiskRef)0)
		goto exit;

	/* Tell DA not to auto-mount from this disk. */
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
display_alert()
{
	int fd, ret, err;
	struct stat st;
	struct timeval tv;
	time_t last;
	time_t now;

top:
	fd = open(COOKIE_FILE, O_CREAT|O_EXCL|O_WRONLY, 0644);

	if (fd == -1) {
		err = errno;
		if (err != EEXIST) {
			goto out;
		}

		/* Cookie file already exists, check mtime */	
		ret = lstat(COOKIE_FILE, &st);
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
			ret = remove(COOKIE_FILE);
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

	AlertZFSNotInstalled();
	/* Update cookie file timestamp */
	ret = utimes(COOKIE_FILE, NULL);

out:
	return;
}

/*
 * Write our (localized) fake filesystem name to stdout.
 */
static void
print_fsname()
{
	char cstr[1024];
	CFStringRef str;
	str = CFCopyLocalizedString(CFSTR("ZFS File System"), "ZFS File System");
	CFStringGetCString(str, cstr, 1024, kCFStringEncodingUTF8);
	(void) fprintf(stdout, "%s", cstr);
	fflush(stdout);

	if (str)
		CFRelease(str);
}

/*
 * Returns the version number of the currently installed zfs.fs, or 0 if zfs is
 * not installed.
 */
int
get_zfs_version()
{
	int ret = 0;
	CFURLRef zfs_url = NULL;
	CFBundleRef zfs_bundle = NULL;
	CFStringRef vers_str = NULL;
	char vers_cstr[256];
	unsigned long major_vers;
	
	zfs_url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
	               CFSTR("/System/Library/Filesystems/zfs.fs"),
	               kCFURLPOSIXPathStyle,
	               true);
	if (zfs_url == NULL) {
		ret = 0;
		goto out;
	}

	zfs_bundle = CFBundleCreate(NULL, zfs_url);	
	if (zfs_bundle == NULL) {
		ret = 0;
		goto out;
	}

	/* Get the major version */
	vers_str = CFBundleGetValueForInfoDictionaryKey(zfs_bundle, kCFBundleVersionKey);
	CFStringGetCString(vers_str, vers_cstr, sizeof(vers_cstr), kCFStringEncodingUTF8);
	major_vers = strtol(vers_cstr, NULL, 10);

	ret = major_vers;
	
out:
	if (zfs_url)
		CFRelease(zfs_url);
	if (zfs_bundle)
		CFRelease(zfs_bundle);

	return ret;
}

int
main(int argc, char **argv)
{
	char what;
	char *devname;
	int ret = FSUR_UNRECOGNIZED;

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
		/*
		 * Leopard ZFS is version 8 -- nofs will claim all ZFS disks 
		 * if no ZFS is installed, or if Leopard ZFS is installed.
		 */
		if (get_zfs_version() < 9) {
			ret = FSUR_RECOGNIZED;
			print_fsname();
			suppress_mount(devname);
			display_alert();
		}
		break;
	default:
		usage();
	}

	return ret;
}
