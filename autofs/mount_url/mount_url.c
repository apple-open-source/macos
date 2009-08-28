/*
 * Copyright (c) 2007-2009 Apple Inc.  All Rights Reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <NetFS/URLMount.h>

#define ALT_SOFT	0x00000001

static const struct mntopt mopts_std[] = {
	MOPT_STDOPTS,
	{ "soft",	0, ALT_SOFT, 1 },
	{ NULL,		0, 0, 0 }
};

static void usage(void);

int
main(int argc, char **argv)
{
	int c;
	uint32_t urlmount_flags;
	mntoptparse_t mp;
	int flags, altflags;
	CFURLRef URL;
	CFStringRef mountdir_CFString;
	CFArrayRef mountpoints;
	int res;

	/*
	 * XXX - do we really want to suppress WebDAV's "server not
	 * responding" UI?  That's what kSuppressAllUI does.
	 */
	urlmount_flags = kMountAtMountdir | kSuppressAllUI;
	while ((c = getopt(argc, argv, "o:rw")) != -1) {
		switch (c) {

		case 'o':
			/*
			 * OK, parse these options, and set the
			 * appropriate NetFS flags.
			 */
			flags = altflags = 0;
			getmnt_silent = 1;
			mp = getmntopts(optarg, mopts_std, &flags, &altflags);
			freemntopts(mp);
			if (flags & MNT_AUTOMOUNTED)
				urlmount_flags |= kMarkAutomounted;
			if (flags & MNT_DONTBROWSE)
				urlmount_flags |= kMarkDontBrowse;
			if (flags & MNT_NOSUID)
				urlmount_flags |= kNoSetUID;
			if (flags & MNT_NODEV)
				urlmount_flags |= kNoDevices;
			if (flags & MNT_RDONLY)
				urlmount_flags |= kReadOnlyMount;
			if (flags & MNT_QUARANTINE)
				urlmount_flags |= kQuarantine;
			if (altflags & ALT_SOFT)
				urlmount_flags |= kSoftMount;
			break;

		case 'r':
			urlmount_flags |= kReadOnlyMount;
			break;

		case 'w':
			urlmount_flags &= ~kReadOnlyMount;
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
	res = netfs_MountURLDirectSync(URL, NULL, NULL, mountdir_CFString,
	    urlmount_flags, &mountpoints);
	CFRelease(mountdir_CFString);
	CFRelease(URL);
	if (res == 0)
		CFRelease(mountpoints);
	else {
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
	exit(res);
}

static void
usage(void)
{
	fprintf(stderr, "Usage: mount_url [-rw] [-o options] url node\n");
	exit(1);
}
