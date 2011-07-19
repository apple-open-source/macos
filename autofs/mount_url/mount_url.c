/*
 * Copyright (c) 2007-2011 Apple Inc.  All Rights Reserved.
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

#include <unistd.h>
#include <mntopts.h>
#include <syslog.h>

#include <sys/mount.h>

#include <CoreFoundation/CoreFoundation.h>
#include <NetFS/NetFS.h>
#include <NetFS/NetFSPrivate.h>
/*
 * XXX - <NetAuth/NetAuth.h> include <NetAuth/NAKeys.h>, which redefines
 * some of our #defines if _NETFS_H_ isn't defined.
 *
 * To prevent that from happening, we include it *after* including NetFS.h,
 * which defines _NETFS_H_.
 */
#include <NetAuth/NetAuth.h>

#define ALT_SOFT	0x00000001

static const struct mntopt mopts_std[] = {
	MOPT_STDOPTS,
	MOPT_UPDATE,
	MOPT_RELOAD,
	{ "soft",	0, ALT_SOFT, 1 },
	{ NULL,		0, 0, 0 }
};

static void usage(void);

static int do_mount_direct(CFURLRef server_URL, CFStringRef mountdir,
    CFDictionaryRef open_options, CFDictionaryRef mount_options,
    CFDictionaryRef *mount_infop);

int
main(int argc, char **argv)
{
	int c;
	int usenetauth = 0;
	mntoptparse_t mp;
	int flags, altflags;
	CFURLRef URL;
	CFStringRef mountdir_CFString;
	CFMutableDictionaryRef open_options, mount_options;
	CFDictionaryRef mount_info;
	int res;

	flags = altflags = 0;
	getmnt_silent = 1;
	while ((c = getopt(argc, argv, "no:rw")) != -1) {
		switch (c) {

		case 'n':
			usenetauth = 1;
			break;

		case 'o':
			/*
			 * OK, parse these options, and update the flags.
			 */
			mp = getmntopts(optarg, mopts_std, &flags, &altflags);
			freemntopts(mp);
			break;

		case 'r':
			flags |= MNT_RDONLY;
			break;

		case 'w':
			flags &= ~MNT_RDONLY;
			break;

		case '?':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	/*
	 * Nothing can stop the Duke of...
	 */
	URL = CFURLCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)argv[0],
	    strlen(argv[0]), kCFStringEncodingUTF8, NULL);
	if (URL == NULL)
		exit(ENOMEM);

	mountdir_CFString = CFStringCreateWithCString(kCFAllocatorDefault,
	    argv[1], kCFStringEncodingUTF8);
	if (mountdir_CFString == NULL)
		exit(ENOMEM);

	open_options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (open_options == NULL)
		exit(ENOMEM);
	/*
	 * It's OK to use an existing session.
	 */
	CFDictionaryAddValue(open_options, kNetFSForceNewSessionKey,
	    kCFBooleanFalse);
	/*
	 * And it's OK to mount something from ourselves.
	 */
	CFDictionaryAddValue(open_options, kNetFSAllowLoopbackKey,
	    kCFBooleanTrue);
	/*
	 * This could be mounting a home directory, so we don't want
	 * the mount to look at user preferences in the home directory.
	 */
	CFDictionaryAddValue(open_options, kNetFSNoUserPreferencesKey,
	    kCFBooleanTrue);
	/*
	 * We don't want any UI popped up for the mount.
	 */
	CFDictionaryAddValue(open_options, kUIOptionKey, kUIOptionNoUI);

	mount_options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (mount_options == NULL)
		exit(ENOMEM);
	/*
	 * It's OK to use an existing session.
	 */
	CFDictionaryAddValue(mount_options, kNetFSForceNewSessionKey,
	    kCFBooleanFalse);
	/*
	 * We want the URL mounted exactly where we specify.
	 */
	CFDictionaryAddValue(mount_options, kNetFSMountAtMountDirKey,
	    kCFBooleanTrue);
	/*
	 * This could be mounting a home directory, so we don't want
	 * the mount to look at user preferences in the home directory.
	 */
	CFDictionaryAddValue(mount_options, kNetFSNoUserPreferencesKey,
	    kCFBooleanTrue);
	/*
	 * We want to allow the URL to specify a directory underneath
	 * a share point for file systems that support the notion of
	 * shares.
	 */
	CFDictionaryAddValue(mount_options, kNetFSAllowSubMountsKey,
	    kCFBooleanTrue);
	/*
	 * Add the mount flags.
	 */
	CFDictionaryAddValue(mount_options, kNetFSMountFlagsKey,
	    CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
	      &flags));
	/*
	 * Add the soft mount flag.
	 */
	CFDictionaryAddValue(mount_options, kNetFSSoftMountKey,
	    (altflags & ALT_SOFT) ? kCFBooleanTrue : kCFBooleanFalse);
	/*
	 * We don't want any UI popped up for the mount.
	 */
	CFDictionaryAddValue(mount_options, kUIOptionKey, kUIOptionNoUI);

	if (usenetauth)
		res = NAConnectToServerSync(URL, mountdir_CFString,
		    open_options, mount_options, &mount_info);
	else
		res = do_mount_direct(URL, mountdir_CFString, open_options,
		    mount_options, &mount_info);
	/*
	 * 0 means "no error", EEXIST means "that's already mounted, and
	 * mountinfo says where it's mounted".  In those cases, a
	 * directory of mount information was returned; release it.
	 */
	if (res == 0 || res == EEXIST)
		CFRelease(mount_info);
	CFRelease(mount_options);
	CFRelease(open_options);
	CFRelease(mountdir_CFString);
	CFRelease(URL);
	if (res != 0) {
		/*
		 * Report any failure status that doesn't fit in the
		 * 8 bits of a UN*X exit status, and map it to EIO
		 * by default and EAUTH for ENETFS errors.
		 */
		if ((res & 0xFFFFFF00) != 0) {
			syslog(LOG_ERR,
			    "mount_url: Mount of %s on %s gives status %d",
			    argv[0], argv[1], res);
			switch (res) {

			case ENETFSACCOUNTRESTRICTED:
			case ENETFSPWDNEEDSCHANGE:
			case ENETFSPWDPOLICY:
				res = EAUTH;
				break;

			default:
				res = EIO;
				break;
			}
		}
	}

	return res;
}

static void
usage(void)
{
	fprintf(stderr, "Usage: mount_url [-n] [-rw] [-o options] url node\n");
	exit(1);
}

static int
do_mount_direct(CFURLRef server_URL, CFStringRef mountdir,
    CFDictionaryRef open_options, CFDictionaryRef mount_options,
    CFDictionaryRef *mount_infop)
{
	int ret;
	void *session_ref;

	*mount_infop = NULL;
	ret = netfs_CreateSessionRef(server_URL, &session_ref);
	if (ret != 0)
		return ret;
	ret = netfs_OpenSession(server_URL, session_ref, open_options, NULL);
	if (ret != 0) {
		netfs_CloseSession(session_ref);
		return ret;
	}
	ret = netfs_Mount(session_ref, server_URL, mountdir, mount_options,
	    mount_infop);
	netfs_CloseSession(session_ref);
	return ret;
}
