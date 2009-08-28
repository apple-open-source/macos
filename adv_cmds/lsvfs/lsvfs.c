/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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

#include <sys/mount.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define FMT "%-32.32s %5d %s\n"
#define HDRFMT "%-32.32s %5.5s %s\n"
#define DASHES "-------------------------------- ----- ---------------\n"

static const char *fmt_flags(int flags);

int
main(int argc, char *argv[])
{
	struct vfsconf vfc;
	int mib[4], max, x;
	size_t len;

	printf(HDRFMT, "Filesystem", "Refs", "Flags");
	fputs(DASHES, stdout);

	if (argc > 1) {
		for (x = 1; x < argc; x++)
			if (getvfsbyname(argv[x], &vfc) == 0)
				printf(FMT, vfc.vfc_name, vfc.vfc_refcount,
				    fmt_flags(vfc.vfc_flags));
			else
				warnx("VFS %s unknown or not loaded", argv[x]);
	} else {
		mib[0] = CTL_VFS;
		mib[1] = VFS_GENERIC;
		mib[2] = VFS_MAXTYPENUM;
		len  = sizeof(int);
		if (sysctl(mib, 3, &max, &len, NULL, 0) != 0)
			errx(1, "sysctl");
		mib[2] = VFS_CONF;

		len = sizeof(vfc);
		for (x = 0; x < max; x++) {
			mib[3] = x;
			if (sysctl(mib, 4, &vfc, &len, NULL, 0) != 0) {
				if (errno != ENOTSUP)
					errx(1, "sysctl");
			} else {
				printf(FMT, vfc.vfc_name, vfc.vfc_refcount,
				    fmt_flags(vfc.vfc_flags));
			}
		}
	}

	return 0;
}

static const char *
fmt_flags(int flags)
{
	static char buf[sizeof "local, dovolfs"];
	int comma = 0;

	buf[0] = '\0';

        if(flags & MNT_LOCAL) {
                if(comma++) strcat(buf, ", ");
                strcat(buf, "local");
        }

	if(flags & MNT_DOVOLFS) {
		if(comma++) strcat(buf, ", ");
		strcat(buf, "dovolfs");
	}

	return buf;
}
