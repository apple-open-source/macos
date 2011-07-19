/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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

#include <sys/sysctl.h>
#include <NetFS/NetFS.h>
#include <KerberosHelper/KerberosHelper.h>
#include <KerberosHelper/NetworkAuthenticationHelper.h>
#include <CoreFoundation/CoreFoundation.h>

#include <netsmb/smb_lib.h>
#include <netsmb/smb_conn.h>
#include <smbfs/smbfs.h>
#include <parse_url.h>
#include "msdfs.h"
#include <smbclient/smbclient.h>
#include <smbclient/smbclient_internal.h>
#include "gss.h"
#include "remount.h"

/*
 * SysctlByFSID
 *
 * utility routine to call sysctl by fsid
 */
static int
SysctlByFSID(int op, fsid_t fsid, void *oldp, size_t *oldlenp, void *newp, 
			size_t newlen)
{
	int ctlname[CTL_MAXNAME+2];
	size_t ctllen;
	const char *sysstr = "vfs.generic.ctlbyfsid";
	struct vfsidctl vc;
	
	ctllen = CTL_MAXNAME+2;
	if (sysctlnametomib(sysstr, ctlname, &ctllen) == -1) {
		smb_log_info("%s: sysctlnametomib(%s)", ASL_LEVEL_ERR, __FUNCTION__, 
					 sysstr);
		return -1;
	};
	ctlname[ctllen] = op;
	
	bzero(&vc, sizeof(vc));
	vc.vc_vers = VFS_CTL_VERS1;
	vc.vc_fsid = fsid;
	vc.vc_ptr = newp;
	vc.vc_len = newlen;
	return sysctl(ctlname, (u_int)(ctllen + 1), oldp, oldlenp, &vc, sizeof(vc));
}


/*
 * SysctlRemountInfo
 *
 * Calls into sysctl with a fsid to retrieve the remount information, may be 
 * removed in the future. We could just have autofsd pass this info up with
 * the fsid. Need to decide if there any secuity concerns with doing it that
 * way. Until autofs work is complete we need this method to get the information.
 */
static int 
SysctlRemountInfo(fsid_t fsid, struct smb_remount_info *info)
{
	size_t size;
	
	size = sizeof(*info);	
	if (SysctlByFSID(SMBFS_SYSCTL_REMOUNT_INFO, fsid, info, &size, NULL, 0) == -1) {
		smb_log_info("%s: failed for 0x%x:0x%x - %s", ASL_LEVEL_ERR, 
					 __FUNCTION__, fsid.val[0], fsid.val[1], strerror(errno));
		return errno;
	}
#ifdef SMB_DEBUG
	smb_ctx_hexdump(__FUNCTION__, "Remount Info =", (u_char *)info, sizeof(info));
#endif // SMB_DEBUG
	return 0;
}

/*
 * SysctlRemountFS
 *
 * Calls into sysctl with a fsid to trigger the remount. The devId is just a 
 * file descriptor to the device that holds the share. The kernel will 
 * retrieve the share from the file descriptor and replace share on the mount
 * point with the new share.
 */
static int
SysctlRemountFS(fsid_t fsid, int devId)
{
	if (SysctlByFSID(SMBFS_SYSCTL_REMOUNT, fsid, NULL, NULL, &devId, 
					 sizeof(devId)) == -1) {
		smb_log_info("%s: failed for 0x%x:0x%x - %s", ASL_LEVEL_ERR, 
					 __FUNCTION__, fsid.val[0], fsid.val[1], strerror(errno));
		return errno;
	}
	return 0;	
}

static int
GetRootShareConnection(struct smb_ctx *ctx, const char *url, uint32_t authFlags,
					   const char * clientPrincipal, uint32_t clientNameType,
					   uint32_t	maxTimer)
{
	CFDictionaryRef serverParams = NULL;
	CFDictionaryRef sessionInfo = NULL;
	CFMutableDictionaryRef openOptions;
	int error = 0;
	time_t  start_time = time(NULL);
	
	openOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
											&kCFTypeDictionaryKeyCallBacks, 
											&kCFTypeDictionaryValueCallBacks);
	if (!openOptions) {
		smb_log_info("%s: Couldn't create open options for %s", ASL_LEVEL_ERR,
					 __FUNCTION__, url);
		error = ENOMEM;
		goto done;
	}

#ifdef SMBDEBUG_REMOUNT
	/* This is only needed for testing and should be remove once we have autofs hooked up */
	CFDictionarySetValue(openOptions, kNetFSForceNewSessionKey, kCFBooleanTrue);
#endif // SMBDEBUG_REMOUNT
	
	/* Never touch the user home directory */
	CFDictionarySetValue(openOptions, kNetFSNoUserPreferencesKey, kCFBooleanTrue);
	/* 
	 * If they have a loopback in the referral we always allow it, no way for
	 * us to decided what is correct at this point.
	 */
	CFDictionarySetValue(openOptions, kNetFSAllowLoopbackKey, kCFBooleanTrue);
	
	/* 
	 * Do a get server info call first to determine the if the server supports 
	 * the security we need. Also needed it to make sure we have the correct 
	 * server principal name.
	 */
	while (difftime(time(NULL), start_time) < maxTimer ) {
		error = smb_get_server_info(ctx, NULL, openOptions, &serverParams);
		if (!error) {
			break;
		}
		smb_log_info("%s: get server info failed %d, sleeping one second, have %d seconds left.", 
					 ASL_LEVEL_DEBUG,  __FUNCTION__, error, 
					 maxTimer - (int)difftime(time(NULL), start_time));
		sleep(1);	/* Wait one second before trying again */
	}
	if (error) {
		smb_log_info("%s: get server info failed from %s with %d", 
					 ASL_LEVEL_ERR,  __FUNCTION__, url, error);
		goto done;
	}
		
	/* 
	 * We should check the server params and make sure this server supports
	 * the same auth method as the old server. Doesn't really make any difference
	 * we should fail in the open if they don't support the correct auth.
	 */
	
	/*
	 * Set up the authorization using the same auth method that was use in the 
	 * original mount.
	 */
	if (authFlags & (SMBV_GUEST_ACCESS | SMBV_SFS_ACCESS | SMBV_PRIV_GUEST_ACCESS)) {
		CFDictionarySetValue( openOptions, kNetFSUseGuestKey, kCFBooleanTrue);
	} else {
		CFMutableDictionaryRef authInfoDict;
		
		authInfoDict = CreateAuthDictionary(ctx, authFlags, clientPrincipal, clientNameType);
		if (!authInfoDict) {
			smb_log_info("%s: Creating authorization dictionary failed for  %s", 
						 ASL_LEVEL_ERR, __FUNCTION__, url);
			error = ENOMEM;
			goto done;
		}
		CFDictionarySetValue( openOptions, kNetFSUseAuthenticationInfoKey, kCFBooleanTrue);
		CFDictionarySetValue(openOptions, kNetFSAuthenticationInfoKey, authInfoDict);
		CFRelease(authInfoDict);
	}
	
    error = smb_open_session(ctx, NULL, openOptions, &sessionInfo);
	if (error) {
		smb_log_info("%s: open session failed from url %s with %d", ASL_LEVEL_ERR,  
					 __FUNCTION__, url, error);
		goto done;
	}	
	error = smb_share_connect(ctx);
	if (error) {
		smb_log_info("%s: share connect failed from url %s with %d", ASL_LEVEL_ERR, 
					 __FUNCTION__, url, error);
		goto done;
	}
done:
	if (sessionInfo) {
		CFRelease(sessionInfo);
	}
	if (serverParams) {
		CFRelease(serverParams);
	}
	if (openOptions) {
		CFRelease(openOptions);
	}
	return error;
}

/*
 * Would prefer to do all of this as the user that mounted the volume, but it
 * seems the sysctlbyfsid requires you to be root. So do the sysctlbyfsid as
 * root and everything else as the user.
 */
int smb_remount_with_fsid(fsid_t fsid)
{
	int error;
	struct smb_ctx *ctx = NULL;
	struct smb_remount_info remountInfo;
	uid_t rootUID = geteuid();
	
	/* Get the remount information need for the remount */
	memset(&remountInfo, 0, sizeof(remountInfo));
	error = SysctlRemountInfo(fsid, &remountInfo);
	if (error) {
		goto done;
	}
	
	smb_log_info("%s: mount url smb:%s", ASL_LEVEL_DEBUG, __FUNCTION__, 
				 remountInfo.mntURL);
	smb_log_info("%s: client principal name %s", ASL_LEVEL_DEBUG, __FUNCTION__, 
				 remountInfo.mntClientPrincipalName);
	smb_log_info("%s: client principal name type %d", ASL_LEVEL_DEBUG, __FUNCTION__, 
				 remountInfo.mntClientPrincipalNameType);
	smb_log_info("%s: authorization flags 0x%x", ASL_LEVEL_DEBUG, __FUNCTION__, 
				 remountInfo.mntAuthFlags);
	smb_log_info("%s: owner of mount point %d", ASL_LEVEL_DEBUG, __FUNCTION__, 
				 remountInfo.mntOwner);
	
	/* Switch to the user that owns the mount */
	seteuid(remountInfo.mntOwner);
	error = create_smb_ctx_with_url(&ctx, remountInfo.mntURL);
	if (error) {
		smb_log_info("%s: Could create ctx from url smb:%s", ASL_LEVEL_ERR, 
					 __FUNCTION__, remountInfo.mntURL);
		seteuid(rootUID);
		goto done;
	}
	error = GetRootShareConnection(ctx, remountInfo.mntURL, 
								   remountInfo.mntAuthFlags, 
								   remountInfo.mntClientPrincipalName, 
								   remountInfo.mntClientPrincipalNameType,
								   remountInfo.mntDeadTimer);
	if (!error) {
		struct smb_ctx *dfs_ctx = NULL;
		
		/*
		 * XXX - Need to hanlde the DfsRoot remount case, we should check to 
		 * see if this is just a DfsRoot, if so then continue with the remount
		 * because we found a different domain control to access.
		 */
		error = checkForDfsReferral(ctx, &dfs_ctx, NULL);
		if (error || ctx == dfs_ctx) {
			seteuid(rootUID);
			goto done;
		}
		smb_ctx_done(ctx);
		ctx = dfs_ctx;
	}

	seteuid(rootUID);
	if (error) {
		goto done;
	}
	
	if (smb_tree_conn_fstype(ctx) == SMB_FS_FAT) {
		smb_log_info("%s: Could remount url smb:%s found a fat file system", 
					 ASL_LEVEL_ERR,  __FUNCTION__, remountInfo.mntURL);
		error = ENOTSUP;
		goto done;
	}
	if (!error) {
		error = SysctlRemountFS(fsid, ctx->ct_fd);
	}

done:
	if (ctx) {
		seteuid(remountInfo.mntOwner);
		smb_ctx_done(ctx);
		seteuid(rootUID);
	}
	if (error) {
		smb_log_info("%s: remount failed for url %s with error = %d", 
					 ASL_LEVEL_ERR, __FUNCTION__, remountInfo.mntURL, error);
	}
	return error;
}
