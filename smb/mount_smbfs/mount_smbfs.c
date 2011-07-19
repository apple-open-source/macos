/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2010 Apple Inc. All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/mount.h>

#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <err.h>
#include <sysexits.h>
#include <smbclient/smbclient.h>
#include <smbclient/smbclient_internal.h>
#include <smbclient/ntstatus.h>

#include "SetNetworkAccountSID.h"

#include <mntopts.h>

static void usage(void);

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	{ "streams",	0, SMBFS_MNT_STREAMS_ON, 1 },
	{ "notification",	1, SMBFS_MNT_NOTIFY_OFF, 1 },
	{ "soft",	0, SMBFS_MNT_SOFT, 1 },
	{ NULL, 0, 0, 0 }
};


static unsigned xtoi(unsigned u)
{
	if (isdigit(u))
		return (u - '0'); 
	else if (islower(u))
		return (10 + u - 'a'); 
	else if (isupper(u))
		return (10 + u - 'A'); 
	return (16);
}

/* Removes the "%" escape sequences from a URL component.
 * See IETF RFC 2396.
 *
 * Someday we should convert this to use CFURLCreateStringByReplacingPercentEscapesUsingEncoding
 */
static char * unpercent(char * component)
{
	unsigned char c, *s;
	unsigned hi, lo; 
	
	if (component)
		for (s = (unsigned char *)component; (c = (unsigned char)*s); s++) {
			if (c != '%') 
				continue;
			if ((hi = xtoi(s[1])) > 15 || (lo = xtoi(s[2])) > 15)
				continue; /* ignore invalid escapes */
			s[0] = hi*16 + lo;
			/*      
			 * This was strcpy(s + 1, s + 3);
			 * But nowadays leftward overlapping copies are
			 * officially undefined in C.  Ours seems to
			 * work or not depending upon alignment.
			 */      
			memmove(s+1, s+3, (strlen((char *)(s+3))) + 1);
		}       
	return (component);
}

int main(int argc, char *argv[])
{
	SMBHANDLE serverConnection = NULL;
	uint64_t options = kSMBOptionSessionOnly;
	uint64_t mntOptions = 0;
	int altflags = SMBFS_MNT_STREAMS_ON;
	mode_t fileMode = 0, dirMode = 0;
	int mntflags = 0;
	NTSTATUS	status;
	char mountPoint[MAXPATHLEN];
	struct stat st;
	char *next;
	int opt;
	const char * url = NULL;
	int version = SMBFrameworkVersion();

	while ((opt = getopt(argc, argv, "Nvhd:f:o:")) != -1) {
		switch (opt) {
		    case 'd':
				errno = 0;
				dirMode = strtol(optarg, &next, 8);
				if (errno || *next != 0)
					errx(EX_DATAERR, "invalid value for directory mode");
				break;
		    case 'f':
				errno = 0;
				fileMode = strtol(optarg, &next, 8);
				if (errno || *next != 0)
					errx(EX_DATAERR, "invalid value for file mode");
				break;
			case 'N':
				options |= kSMBOptionNoPrompt;
				break;
			case 'o': {
				mntoptparse_t mp = getmntopts(optarg, mopts, &mntflags, &altflags);
				if (mp == NULL)
					err(1, NULL);
				freemntopts(mp);
				break;
			}
			case 'v':
				errx(EX_OK, "version %d.%d.%d", 
					version / 100000, (version % 10000) / 1000, (version % 1000) / 100);
				break;
			case '?':
			case 'h':
		    default:
				usage();
				break;
		}
	}
	if (optind >= argc)
		usage();
	
	argc -= optind;
	/* At this point we should only have a url and a mount point */
	if (argc != 2)
		usage();
	url = argv[optind];
	optind++;
	realpath(unpercent(argv[optind]), mountPoint);
	
	if (stat(mountPoint, &st) == -1)
		err(EX_OSERR, "could not find mount point %s", mountPoint);
	
	if (!S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		err(EX_OSERR, "can't mount on %s", mountPoint);
	}
	
	if (mntflags & MNT_AUTOMOUNTED) {
		/* Automount volume, don't look in the user home directory */
		options |= kSMBOptionNoUserPreferences;
	}

	if ((altflags & SMBFS_MNT_STREAMS_ON) != SMBFS_MNT_STREAMS_ON) {
		/* They told us to turn of named streams */
		mntOptions |= kSMBMntOptionNoStreams;
	}
	if ((altflags & SMBFS_MNT_NOTIFY_OFF) == SMBFS_MNT_NOTIFY_OFF) {
		/* They told us to turn off remote notifications */
		mntOptions |= kSMBMntOptionNoNotifcations;
	}
	if ((altflags & SMBFS_MNT_SOFT) == SMBFS_MNT_SOFT) {
		/* Make this a soft mount */
		mntOptions |= kSMBMntOptionSoftMount;
	}
	
	status = SMBOpenServerEx(url, &serverConnection, options);
	if (NT_SUCCESS(status)) {
		status = SMBMountShareEx(serverConnection, NULL, mountPoint, mntflags, 
								 mntOptions, fileMode, dirMode, setNetworkAccountSID, NULL);
	}
	/* 
	 * SMBOpenServerEx now sets errno, so err will work correctly. We change 
	 * the string based on the NTSTATUS Error.
	 */
	if (!NT_SUCCESS(status)) {
		switch (status) {
			case STATUS_NO_SUCH_DEVICE:
				err(EX_UNAVAILABLE, "failed to intitialize the smb library");
				break;
			case STATUS_LOGON_FAILURE:
				err(EX_NOPERM, "server rejected the connection");
				break;
			case STATUS_CONNECTION_REFUSED:
				err(EX_NOHOST, "server connection failed");
			break;
			case STATUS_INVALID_HANDLE:
			case STATUS_NO_MEMORY:
				err(EX_UNAVAILABLE, "internal error");
				break;
			case STATUS_UNSUCCESSFUL:
				err(EX_USAGE, "mount error: %s", mountPoint);
				break;
			case STATUS_INVALID_PARAMETER:
				err(EX_USAGE, "URL parsing failed, please correct the URL and try again");
				break;
			case STATUS_BAD_NETWORK_NAME:
				err(EX_NOHOST, "share connection failed");
				break;
			default:
				err(EX_OSERR, "unknown status %d", status);
				break;
		}
	}

	/* We are done clean up anything left around */
	if (serverConnection)
		SMBReleaseServer(serverConnection);
	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "%s\n",
	"usage: mount_smbfs [-Nh] [-d mode] [-f mode]\n"
	"                   //"
	"[domain;][user[:password]@]server[/share]"
	" path");

	exit (EX_USAGE);
}
