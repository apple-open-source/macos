/*
 * Copyright (c) 2006-2009 Apple Inc. All rights reserved.
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
#include <URLMount/URLMount.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <err.h>
#include <netsmb/smb_dev.h>
#include <asl.h>
#include <sysexits.h>
#include "smb_netfs.h"
#include "rosetta.h"

extern unsigned long rosetta_ioctl_cmds[SMBIOC_COMMAND_COUNT];
extern unsigned long rosetta_error_ioctl_cmds[SMBIOC_COMMAND_COUNT];
extern unsigned long rosetta_fsctl_cmds;

int verbose_on = FALSE;
extern char *__progname;

static void usage()
{
    const char * usage_message =
    "[-v] smb://[domain;][user[:password]@]server/share\n";
	
    printf("\n%s %s", __progname, usage_message);
    printf("\nExample:\n         smb://local1:local@smb-win2003/EmptyShare\n");
	
    exit(EX_USAGE);
}

static void unmount_volume(CFStringRef mPoint)
{
	struct statfs buf;
	char mountpointpath[MAXPATHLEN];
	
	CFStringGetCString(mPoint, mountpointpath, MAXPATHLEN, kCFStringEncodingUTF8);
	if ((statfs(mountpointpath, &buf) == 0) && (unmount(mountpointpath, MNT_FORCE) == -1))
		printf("Failed to unmount %s: %s '%d'\n", mountpointpath, strerror(errno), errno);

	if (rmdir(mountpointpath) == -1)
		printf("Failed to remove %s: %s '%d'\n", mountpointpath, strerror(errno), errno);
}

static void create_mount_point(CFURLRef url, CFStringRef *mPoint)
{
	char tmpstr[MAXPATHLEN];
	CFStringRef share = CFURLCopyLastPathComponent(url);
	CFMutableStringRef mountPoint = CFStringCreateMutableCopy(NULL, 0, CFSTR("/tmp/"));
	
	*mPoint = NULL;
	if (!mountPoint)
		err(ENOMEM, "Couldn't allocate mount point %s", tmpstr);
	
	if (share) {
		if (verbose_on) {
			printf("Mount point = ");
			CFShow(share);
		}
		CFStringAppend(mountPoint, share);
		CFRelease(share);		
	} else
		CFStringAppend(mountPoint, CFSTR("mp1"));
	CFStringGetCString(mountPoint, tmpstr, MAXPATHLEN, kCFStringEncodingUTF8);
	if (mkdir(tmpstr, S_IRWXU) == -1) {
		if (errno != EEXIST)
			err(errno, "Couldn't create mount point %s", tmpstr);
	}
	*mPoint = mountPoint;
}

int main(int argc, char *argv[])
{
	CFStringRef mPoint = NULL;
	void *sessionRef = NULL;
	CFURLRef url;
	int error, close_error;
	int ii;
	int opt;

    if (argc < 2) {
        usage();
    }

	while ((opt = getopt(argc, argv, "v")) != -1) {
		switch (opt) {
		    case 'v':
				verbose_on = TRUE;
				printf("versbose is on\n");
				break;
		    default:
				usage();
				break;
		}
	}
	if (optind >= argc)
		usage();
	
	url = CreateSMBURL((const char *)argv[optind]);
	if (!url) {
		printf("CreateSMBURL: FAILED error = %d\n", error);		
        usage();
	}
	create_mount_point(url, &mPoint);	
	
	error = SMB_CreateSessionRef(&sessionRef);
	if (error)
		printf("SMB_CreateSessionRef: error = %d\n", error);
	
	/* 
	 * We need to test a complete mount, getting info about a mount, 
	 * getting a list of shares using dce/rpc and getting a list of
	 * shares using rap. Do we have others not sure yet.
	 */
	if (!error && sessionRef) {
		CFDictionaryRef openOptions = NULL;	/* may want to change this later, but for now leave it empty */
		CFDictionaryRef serverParms = NULL;
		
		error = SMB_GetServerInfo(url, sessionRef, openOptions, &serverParms);
		if (error)	
			printf("SMB_GetServerInfo: error = %d\n", error);
		else if (verbose_on && serverParms) {
				CFShow(CFSTR("serverParms:"));
				CFShow(serverParms);
		}
		if (serverParms)
			CFRelease(serverParms);
	}
	if (!error && sessionRef) {
		CFDictionaryRef openOptions = NULL;	/* may want to change this later, but for now leave it empty */
		CFDictionaryRef sessionInfo = NULL;
		
		error = SMB_OpenSession(url, sessionRef, openOptions, &sessionInfo);
		if (error)	
			printf("SMB_OpenSession: error = %d\n", error);
		else if (verbose_on && sessionInfo) {
			CFShow(CFSTR("sessionInfo:"));
			CFShow(sessionInfo);
		}
		if (sessionInfo)
			CFRelease(sessionInfo);
	}
	
	if (!error && sessionRef && mPoint) {
		CFDictionaryRef mInfo;
		
		error = SMB_Mount(sessionRef, url, mPoint, NULL, &mInfo);
	}
	if (!error && sessionRef) {
		CFDictionaryRef sharePoints = NULL;
		
		error = SMB_EnumerateShares(sessionRef, NULL, &sharePoints);
		if (error)	
			printf("SMB_EnumerateShares: error = %d\n", error);
		else if (verbose_on && sharePoints) {
			CFShow(CFSTR("sharePoints:"));
			CFShow(sharePoints);
		}
		if (sharePoints)
			CFRelease(sharePoints);
	}
	if (sessionRef) {
		close_error = SMB_Cancel(sessionRef);
		if (close_error)
			printf("SMB_Cancel: error = %d\n", close_error);
		close_error = SMB_CloseSession(sessionRef);		
		if (close_error)
			printf("SMB_CloseSession: error = %d\n", close_error);
	}
	if (url)
		CFRelease(url);
	if (mPoint) {
		unmount_volume(mPoint);
		CFRelease(mPoint);
	}
	/* Now display what success, failed, or just never ran */
	for (ii=0; ii< SMBIOC_COMMAND_COUNT; ii++) {
		if (rosetta_ioctl_cmds[ii])
			printf("SMB IOCTL command %ld completed successfully.\n", rosetta_ioctl_cmds[ii]);
		else if (rosetta_error_ioctl_cmds[ii])
			printf("SMB IOCTL command %ld failed to complete %d.\n", rosetta_error_ioctl_cmds[ii], ii);
		else if ((ii != SMBIOC_UNUSED_104) && (ii != SMBIOC_UNUSED_105) && 
				 (ii != SMBIOC_UNUSED_106) && (ii != SMBIOC_UNUSED_108))
			printf("SMB IOCTL command %d didn't get excuted.\n", MIN_SMBIOC_COMMAND+ii);			
	}
	if (rosetta_fsctl_cmds == 19)
		printf("SMB FSCTL command %ld completed successfully.\n", rosetta_fsctl_cmds);
	else if (rosetta_fsctl_cmds == EEXIST)
		printf("SMB FSCTL command 19 completed successfully.\n");
	else if (rosetta_fsctl_cmds)
		printf("SMB FSCTL command 19 failed with error %ld.\n", rosetta_fsctl_cmds);
	else
		printf("SMB FSCTL command 19 didn't get excuted\n");

    return 0;
}
