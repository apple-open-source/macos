/*
 * Copyright (c) 2006-2008 Apple Inc. All rights reserved.
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
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <asl.h>
#include <stdio.h>

#include <netsmb/smb.h>
#include <fs/smbfs/smbfs.h>
#include "load_smbfs.h"

#include "smbfs_load_kext.h"
#include "smbfs_load_kextServer.h"

#define CFENVFORMATSTRING "__CF_USER_TEXT_ENCODING=0x%X:0:0"

union MaxMsgSize {
	union __RequestUnion__smbfs_load_kext_subsystem req;
	union __ReplyUnion__smbfs_load_kext_subsystem rep;
};

static int LoadKext(const char *inKextPath)
{
	pid_t	childPID;
	int	status = 0;
	
	if ((childPID = vfork()) < 0)
		return errno;

	if (childPID == 0) {
		char CFUserTextEncodingEnvSetting[sizeof(CFENVFORMATSTRING) + 20]; /* Extra  bytes for expansion of %X uid field */
		const char *env[] = {CFUserTextEncodingEnvSetting, "", NULL };
	     /* 
	      * Create a new environment with a definition of __CF_USER_TEXT_ENCODING to work 
	      * around CF's interest in the user's home directory (which could be networked, 
	      * causing recursive references through automount). Make sure we include the uid
	      * since CF will check for this when deciding if to look in the home directory.
	      */ 
	    snprintf(CFUserTextEncodingEnvSetting, sizeof(CFUserTextEncodingEnvSetting), CFENVFORMATSTRING, getuid()); 
	    execle(KEXT_LOAD_PATH, KEXT_LOAD_PATH, "-q", inKextPath, NULL, env); 
	    _exit(errno);
	} 
	else
		waitpid(childPID, &status, 0);
    
    if(WIFEXITED(status))	/* normal exit */
        return WEXITSTATUS(status);
    else if(WIFSIGNALED(status))
        return WTERMSIG(status);
	else
		return EIO;
}

kern_return_t do_load_kext(mach_port_t test_port __attribute__((unused)), string_t kextname)
{
	int error = KERN_SUCCESS;
	struct vfsconf vfc;

    if(geteuid() != 0)
    {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "Need to be root to load kext euid = %d!\n", geteuid());		
        return EACCES;
    }
	
	if (strcmp(kextname, SMBFS_VFSNAME) == 0)  {
		if (getvfsbyname(SMBFS_VFSNAME, &vfc) != 0)
			error = LoadKext(SMB_KEXT_PATH);
	} else
	    error = ENOENT;
	
	return error;
	
}

static mach_port_t checkin_or_register(const char *bname)
{
	kern_return_t kr;
	mach_port_t mp;
	
	/* If we're started by launchd or the old mach_init */
	kr = bootstrap_check_in(bootstrap_port, (char *)bname, &mp);
	if (kr == KERN_SUCCESS)
		return mp;
	/* If not then get out */
	exit(EXIT_FAILURE);
	
}

int main(void)
{
	mach_msg_size_t mxmsgsz = (mach_msg_size_t)(sizeof(union MaxMsgSize) + MAX_TRAILER_SIZE);
	mach_port_t mp = checkin_or_register(SMBFS_LOAD_KEXT_BOOTSTRAP_NAME);
	kern_return_t kr;
	
	kr = mach_msg_server_once(smbfs_load_kext_server, mxmsgsz, mp, 0);
	if (kr != KERN_SUCCESS) {
		asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "mach_msg_server(mp): %s\n", mach_error_string(kr));
		exit(EXIT_FAILURE);
	}
	
	exit(EXIT_SUCCESS);
}
