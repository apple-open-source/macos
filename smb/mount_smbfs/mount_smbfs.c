/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2007 Apple Inc. All rights reserved. 
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
#include <URLMount/URLMount.h>

#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <err.h>
#include <sysexits.h>
#include <cflib.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_lib.h>
#include <charsets.h>

#include <fs/smbfs/smbfs.h>

#include <mntopts.h>

static void usage(void);

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	{ "streams",	0, SMBFS_MNT_STREAMS_ON, 1 },
	{ "soft",	0, SMBFS_MNT_SOFT, 1 },
	{ NULL, 0, 0, 0 }
};

static char *readstring(int fd)
{
	uint32_t stringlen;
	char *string;
	ssize_t bytes_read;
	
	bytes_read = read(fd, &stringlen, sizeof stringlen);
	if (bytes_read != sizeof stringlen) {
		if (bytes_read < 0) {
			err(EX_IOERR, "error reading from authentication pipe: ");
		} else
			errx(EX_USAGE, "error reading from authentication pipe: expected %lu bytes, got %ld",
				 (unsigned long)sizeof stringlen, (long)bytes_read);
	}
	
	if (stringlen == 0xFFFFFFFF)
		return (NULL);	/* This string isn't present. */
	
	stringlen = ntohl(stringlen);
	string = malloc(stringlen + 1);
	if (string == NULL)
		err(EX_UNAVAILABLE, "can't allocate memory for string: ");
	
	bytes_read = read(fd, string, stringlen);
	if (bytes_read != stringlen) {
		if (bytes_read < 0)
			err(EX_IOERR, "error reading from authentication pipe: ");
		else
			errx(EX_USAGE, "error reading from authentication pipe: expected %u bytes, got %ld",
				 stringlen, (long)bytes_read);
	}
	string[stringlen] = '\0';
	return (string);
}

int
main(int argc, char *argv[])
{
	CFMutableDictionaryRef mOptions;
	CFNumberRef numRef;
	struct smb_ctx *ctx = NULL;
	char mount_point[MAXPATHLEN];
	CFStringRef	mountRef = NULL;
	struct stat st;
	char *next;
	const char *cp;
	int opt;
	int authpipe = -1;
	char * url = NULL;
	int error = 0;
	int mntflags = 0;
	int altflags = 0;
	mode_t	mode;
	mntoptparse_t mp;
	int prompt_user = (isatty(STDIN_FILENO)) ? TRUE : FALSE;
	
	mOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
										&kCFTypeDictionaryValueCallBacks);
	while ((opt = getopt(argc, argv, "Nvhp:d:f:o:")) != -1) {
		switch (opt) {
		    case 'p':
				errno = 0;
				authpipe = strtol(optarg, &next, 0);
				if (errno || (next == optarg) || (*next != 0))
					errx(EX_NOUSER, "invalid value for authentication pipe FD");
				break;
		    case 'd':	/* XXX Not sure we should even support this anymore */
				errno = 0;
				mode = strtol(optarg, &next, 8);
				if (errno || *next != 0)
					errx(EX_DATAERR, "invalid value for directory mode");
				numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &mode);
				if ((mOptions == NULL) || (numRef == NULL))
					errx(EX_NOUSER, "option failed '%s'", optarg);
				CFDictionarySetValue (mOptions, kdirModeKey, numRef);
				CFRelease(numRef);
				break;
		    case 'f':	/* XXX Not sure we should even support this anymore */
				errno = 0;
				mode = strtol(optarg, &next, 8);
				if (errno || *next != 0)
					errx(EX_DATAERR, "invalid value for file mode");
				numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &mode);
				if ((mOptions == NULL) || (numRef == NULL))
					errx(EX_NOUSER, "option failed '%s'", optarg);
				CFDictionarySetValue (mOptions, kfileModeKey, numRef);
				CFRelease(numRef);
				break;
			case 'N':
				prompt_user = FALSE;
				break;
			case 'o':
				mp = getmntopts(optarg, mopts, &mntflags, &altflags);
				if (mp == NULL)
					err(1, NULL);
				freemntopts(mp);
			break;
			case 'v':
				errx(EX_OK, "version %d.%d.%d", 
					SMBFS_VERSION / 100000, (SMBFS_VERSION % 10000) / 1000, (SMBFS_VERSION % 1000) / 100);
				break;
			case '?':
			case 'h':
		    default:
				usage();
				break;
		}
	}
	/* We now have the mount flags and alternative mount flags add them to the mount options */
	if (mOptions) {
		CFNumberRef numRef = CFNumberCreate (NULL, kCFNumberSInt32Type, &mntflags);

		if (numRef) {
			CFDictionarySetValue (mOptions, kMountFlagsKey, numRef);
			CFRelease(numRef);
		}
		if (altflags & SMBFS_MNT_STREAMS_ON)
			CFDictionarySetValue (mOptions, kStreamstMountKey, kCFBooleanTrue);
		if (altflags & SMBFS_MNT_SOFT)
			CFDictionarySetValue (mOptions, kSoftMountKey, kCFBooleanTrue);
	}

	/* Read in all the arguments, now get the URL */
	for (opt = 1; opt < argc; opt++) {
		cp = argv[opt];
		if (strncmp(cp, "//", 2) != 0)
			continue;
		url = (char *)cp;
		optind++;
		break;
	}
	
	/* At this point we need a URL and a Mount Point  */
	if ((!url) || (optind != (argc - 1)))
		usage();
	
	/* Make sure everything is correct */
	realpath(unpercent(argv[optind]), mount_point);
	if (stat(mount_point, &st) == -1)
		err(EX_OSERR, "could not find mount point %s", mount_point);
	
	if (!S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		err(EX_OSERR, "can't mount on %s", mount_point);
	}
	
	mountRef = CFStringCreateWithCString(NULL, mount_point, kCFStringEncodingUTF8);
	if (mountRef == NULL)
		err(EX_NOPERM, "couldn't create mount point reference");
	
	/* We should have all our arguments by now load the kernel and initialize the library */
	if ((errno = smb_load_library(NULL)) != 0)
		err(EX_UNAVAILABLE, "failed to load the smb library");
	
	/* Initialize the context structure */
	if ((errno = smb_ctx_init(&ctx, url, SMBL_SHARE, SMB_ST_DISK, (mntflags & MNT_AUTOMOUNTED))) != 0)
		err(EX_UNAVAILABLE, "failed to intitialize the smb library");
	
	/* Force a private session, if being automounted */
	if (mntflags & (MNT_DONTBROWSE | MNT_AUTOMOUNTED))
		ctx->ct_ssn.ioc_opt |= SMBV_PRIVATE_VC;

 	/*
	 * If "-p" was specified, read the user name and password from the pipe; If 
	 * present, they override any user nameand password supplied in the "UNC name" 
	 * (which is really a URL with "smb:" or "cifs:" stripped off).
	 */
	if (authpipe != -1) {
		char *string = readstring(authpipe);
		
		if (string != NULL) {
			error = smb_ctx_setuser(ctx, string);
			if (error)
				exit(error);
			free(string);
		}
		string = readstring(authpipe);
		if (string != NULL) {
			error = smb_ctx_setpassword(ctx, string);
			if (error)
				exit(error);
			free(string);
		}
		close(authpipe);
	}
	
	error  = smb_connect(ctx);
	if (error) {
		errno = error;
		err(EX_NOHOST, "server connection failed");
	}
	
	/* The server supports Kerberos then see we can connect */
	if (ctx->ct_vc_flags & SMBV_KERBEROS_SUPPORT)
		error = smb_session_security(ctx, NULL, NULL);
	else if (ctx->ct_ssn.ioc_opt & SMBV_EXT_SEC)
		error = ENOTSUP;
	else 
		error = smb_session_security(ctx, NULL, NULL);
		
	
	/* Either Kerberos failed or they do extended security, but not Kerberos */ 
	if (error) {
	    	if (error < 0) /* Could be an expired password error */
		    error = EAUTH;
		ctx->ct_ssn.ioc_opt &= ~SMBV_EXT_SEC;	
		ctx->ct_flags &= ~SMBCF_CONNECT_STATE;		
		error  = smb_connect(ctx);
		if (error) {
			errno = error;
			err(EX_NOHOST, "server connection failed");
		}
		/* need to command-line prompting for the password */
		if (prompt_user && ((ctx->ct_flags & SMBCF_EXPLICITPWD) != SMBCF_EXPLICITPWD)) {
			char passwd[SMB_MAXPASSWORDLEN + 1];

			strncpy(passwd, getpass(SMB_PASSWORD_KEY ":"), SMB_MAXPASSWORDLEN);
			smb_ctx_setpassword(ctx, passwd);
		}
		error = smb_session_security(ctx, NULL, NULL);
		if (error) {
	    		if (error < 0) /* Could be an expired password error */
		    		error = EAUTH;
			errno = error;
			err(EX_NOPERM, "server rejected the connection");
		}
	}
		
	error = smb_mount(ctx, mountRef, mOptions, NULL);
	if (error) {
		errno = error;
		err(error, "mount error: %s", mount_point);
	}
	/* We are done clean up anything left around */
	if (mountRef)
		CFRelease(mountRef);
	if (mOptions)
		CFRelease(mOptions);
	smb_ctx_done(ctx);
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
