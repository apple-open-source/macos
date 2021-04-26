/*
 * Copyright (c) 2000, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2020 Apple Inc. All rights reserved.
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
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include <asl.h>
#include <NetFS/NetFS.h>
#include <NetFS/NetFSPrivate.h>

/* Needed for parsing the Negotiate Token  */
#include <KerberosHelper/KerberosHelper.h>
#include <KerberosHelper/NetworkAuthenticationHelper.h>

/* Needed for Bonjour service name lookup */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <CFNetwork/CFNetServices.h>
#include <CFNetwork/CFNetServicesPriv.h>	/* Required for _CFNetServiceCreateFromURL */

/* Needed for netfs MessageTracer logging */
#include <NetFS/NetFSUtilPrivate.h>

#include <Heimdal/krb5.h>

#include <smbclient/smbclient.h>
#include <smbclient/ntstatus.h>
#include <netsmb/smb_lib.h>
#include <netsmb/netbios.h>
#include <netsmb/nb_lib.h>
#include <netsmb/smb_conn.h>
#include <smbfs/smbfs.h>
#include <netsmb/smbio.h>
#include <netsmb/smbio_2.h>
#include <charsets.h>
#include <parse_url.h>
#include <com_err.h>
#include <netsmb/upi_mbuf.h>
#include <sys/mchain.h>
#include "msdfs.h"
#include <smbclient/smbclient.h>
#include <smbclient/smbclient_internal.h>
#include "gss.h"

#include <ifaddrs.h>
#include <spawn.h>
#include <net/if_media.h>
#include <net/if.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_dev_2.h>

#define DISCONNECT_ERROR(error) \
	((error == ENETUNREACH) || (error == ENOTCONN) || (error == ENETRESET) || \
	(error == ECONNABORTED) || (error == EPIPE))


#define SPN_PLEASE_IGNORE_REALM CFSTR("cifs/not_defined_in_RFC4178@please_ignore")

/*
 * Since ENETFSACCOUNTRESTRICTED, ENETFSPWDNEEDSCHANGE, and ENETFSPWDPOLICY are only
 * defined in NetFS.h and we can't include NetFS.h in the kernel so lets reset these
 * herre. In the future we should just pass up the NTSTATUS and do all the translation
 * in user land.
*/
int smb_ioctl_call(int ct_fd, unsigned long cmd, void *info)
{
	if (ioctl(ct_fd, cmd, info) == -1) {
		switch (errno) {
			case SMB_ENETFSACCOUNTRESTRICTED:
				errno = ENETFSACCOUNTRESTRICTED;
				break;
			case SMB_ENETFSPWDNEEDSCHANGE:
				errno = ENETFSPWDNEEDSCHANGE;
				break;
			case SMB_ENETFSPWDPOLICY:
				errno = ENETFSPWDPOLICY;
				break;
			case SMB_ENETFSNOAUTHMECHSUPP:
				errno = ENETFSNOAUTHMECHSUPP;
				break;
			case SMB_ENETFSNOPROTOVERSSUPP:
				errno = ENETFSNOPROTOVERSSUPP;
				break;
		}
		return -1;
	} else {
		return 0;
	}
}


/*
 * Return the OS and Lanman strings.
 */
static void smb_get_os_lanman(struct smb_ctx *ctx, CFMutableDictionaryRef mutableDict)
{
	struct smbioc_os_lanman OSLanman;
	CFStringRef NativeOSString;
	CFStringRef NativeLANManagerString;
	
	/* See if the kernel has the Native OS and Native Lanman Strings */
	memset(&OSLanman, 0, sizeof(OSLanman));
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_GET_OS_LANMAN, &OSLanman) == -1) {
		os_log_debug(OS_LOG_DEFAULT, "The SMBIOC_GET_OS_LANMAN call failed, syserr = %s",
					 strerror(errno));
	}
	/* Didn't get a Native OS, default to UNIX or Windows */
	if (OSLanman.NativeOS[0] == 0) {
		if (ctx->ct_session_caps & SMB_CAP_UNIX)
			strlcpy(OSLanman.NativeOS, "UNIX", sizeof(OSLanman.NativeOS));
		else
			strlcpy(OSLanman.NativeOS, "WINDOWS", sizeof(OSLanman.NativeOS));		
	}
	/* Get the Native OS and added it to the dictionary */
	NativeOSString = CFStringCreateWithCString(NULL, OSLanman.NativeOS, kCFStringEncodingUTF8);
	if (NativeOSString != NULL) {
		CFDictionarySetValue (mutableDict, kNetFSSMBNativeOSKey, NativeOSString);
		CFRelease (NativeOSString);
	}
	/* Get the Native Lanman and added it to the dictionary */
	if (OSLanman.NativeLANManager[0]) {
		NativeLANManagerString = CFStringCreateWithCString(NULL, OSLanman.NativeLANManager, kCFStringEncodingUTF8);
		if (NativeLANManagerString != NULL) {
			CFDictionarySetValue (mutableDict, kNetFSSMBNativeLANManagerKey, NativeLANManagerString);
			CFRelease (NativeLANManagerString);
		}
	}
	os_log_debug(OS_LOG_DEFAULT, "Native OS = '%s' Native Lanman = '%s'", OSLanman.NativeOS, OSLanman.NativeLANManager);
}

/* 
 * Create a unique_id, that can be used to find a matching mounted
 * volume, given the server address, port number, share name and path.
 */
static void create_unique_id(struct smb_ctx *ctx, char *UppercaseShareName, 
							 unsigned char *id, int32_t *unique_id_len)
{
    struct sockaddr	*ct_saddr;
    int32_t total_len, actual_len;
#ifdef SMB_DEBUG
    unsigned char *start = id;
#endif
    
    /* Always just use the real sockaddr when making a unique id */
    if (ctx->ct_saddr->sa_family == AF_NETBIOS) {
        ct_saddr = (struct sockaddr *)((void *)&((struct sockaddr_nb *)((void *)ctx->ct_saddr))->snb_addrin);
    } else {
        ct_saddr = ctx->ct_saddr;
    }
    
    total_len = ct_saddr->sa_len + (int32_t)strlen(UppercaseShareName) + MAXPATHLEN;
    
    memset(id, 0, SMB_MAX_UNIQUE_ID);
    
    if (total_len > SMB_MAX_UNIQUE_ID) {
        os_log_error(OS_LOG_DEFAULT, "create_unique_id '%s' too long", ctx->ct_sh.ioc_share);
        return; /* program error should never happen, but just in case */
    }
    
    memcpy(id, ct_saddr, ct_saddr->sa_len);
    id += ct_saddr->sa_len;
    
    actual_len = ct_saddr->sa_len;
    
    memcpy(id, UppercaseShareName, strlen(UppercaseShareName));
    id += strlen(UppercaseShareName);
    
    actual_len += strlen(UppercaseShareName);
    
    /* We have a path make it part of the unique id */
    if (ctx->mountPath) {
        CFStringGetCString(ctx->mountPath, (char *)id, MAXPATHLEN, kCFStringEncodingUTF8);
        actual_len += MAXPATHLEN;
    }
    
    /* id += MAXPATHLEN; */
    *unique_id_len = total_len;
    
#ifdef SMB_DEBUG
    smb_ctx_hexdump(__FUNCTION__, "id = ", start, actual_len);
#endif
}


/*
 * Get a list of all mount volumes. The calling routine will need to free the memory.
 */
static struct statfs *smb_getfsstat(int *fs_cnt)
{
	struct statfs *fs;
	int bufsize = 0;
	
	/* See what we need to allocate */
	*fs_cnt = getfsstat(NULL, bufsize, MNT_NOWAIT);
	if (*fs_cnt <=  0)
		return NULL;
	bufsize = *fs_cnt * (int)sizeof(*fs);
	fs = malloc(bufsize);
	if (fs == NULL)
		return NULL;
	
	*fs_cnt = getfsstat(fs, bufsize, MNT_NOWAIT);
	if (*fs_cnt < 0) {
		*fs_cnt = 0;
		free (fs);
		fs = NULL;
	}
	return fs;
}

/*
 * Call the kernel and get the mount information.
 */
static int get_share_mount_info(const char *mntonname, CFMutableDictionaryRef mdict, struct UniqueSMBShareID *req)
{
	req->error = 0;
	req->user[0] = 0;
	if ((fsctl(mntonname, (unsigned int)smbfsUniqueShareIDFSCTL, req, 0 ) == 0) && (req->error == EEXIST)) {
		CFStringRef tmpString = NULL;
		
		tmpString = CFStringCreateWithCString (NULL, mntonname, kCFStringEncodingUTF8);
		if (tmpString) {
			CFDictionarySetValue (mdict, kNetFSMountPathKey, tmpString);
			CFRelease (tmpString);			
		}
		
		if ((req->connection_type == kConnectedByGuest) || (strcasecmp(req->user, kGuestAccountName) == 0)) {
			CFDictionarySetValue (mdict, kNetFSMountedByGuestKey, kCFBooleanTrue);
			return EEXIST;
		} 
		/* Authenticated mount, set the key */
		CFDictionarySetValue (mdict, kNetFSMountedWithAuthenticationInfoKey, kCFBooleanTrue);	    
		if (req->user[0]) {
			tmpString = CFStringCreateWithCString (NULL, req->user, kCFStringEncodingUTF8);
			if (tmpString) {
				CFDictionarySetValue (mdict, kNetFSMountedByUserKey, tmpString);			
				CFRelease (tmpString);
			}
		}
		return EEXIST;
	}
	return 0;
}
 
int already_mounted(struct smb_ctx *ctx, char *UppercaseShareName, struct statfs *fs, 
					int fs_cnt, CFMutableDictionaryRef mdict, int requestMntFlags)
{
	struct UniqueSMBShareID req;
	int ii;
	
	if ((fs == NULL) || (ctx->ct_saddr == NULL))
		return 0;
	bzero(&req, sizeof(req));

#ifdef SMB_DEBUG
    os_log_error(OS_LOG_DEFAULT, "already_mounted: create id for <%s> ",
                 (UppercaseShareName == NULL ? "null" : UppercaseShareName));
#endif
    
    /* now create the unique_id, using tcp address + port + uppercase share */
	create_unique_id(ctx, UppercaseShareName, req.unique_id, &req.unique_id_len);
    
	for (ii = 0; ii < fs_cnt; ii++, fs++) {
        if (fs->f_owner != ctx->ct_ssn.ioc_owner) {
			continue;
        }
        
        if (strcmp(fs->f_fstypename, SMBFS_VFSNAME) != 0) {
			continue;
        }
        
        /* Automounts don't count as already mounted */
        if (fs->f_flags & MNT_AUTOMOUNTED) {
			continue;
        }
        
        /*
		 * See Rusty's comments in Radar 5337352 for more detail.
		 * If you get a MNT_DONTBROWSE mount request, and find a prior instance 
		 * of that as a MNT_DONTBROWSE. mounted by the same UID, you should 
		 * then return that its already mounted. 
		 */
		if (requestMntFlags & MNT_DONTBROWSE) {
			if ((fs->f_flags & MNT_DONTBROWSE) != MNT_DONTBROWSE) {
				continue;
			}
		} else if (fs->f_flags & MNT_DONTBROWSE) {
			continue;
		}
        
#ifdef SMB_DEBUG
        os_log_error(OS_LOG_DEFAULT, "already_mounted: Checking <%s> == <%s> ",
                     fs->f_mntonname,
                     (UppercaseShareName == NULL ? "null" : UppercaseShareName));
#endif
        
		/* Now call the file system to see if this is the one we are looking for */
		if (get_share_mount_info(fs->f_mntonname, mdict, &req) == EEXIST) {
            os_log_error(OS_LOG_DEFAULT, "already_mounted: share <%s> already mounted",
                         (UppercaseShareName == NULL ? "null" : UppercaseShareName));
			return EEXIST;
		}
	}
	return 0;
}

/*
 * Given a dictionary see if the key has a boolean value to return.
 * If no dictionary or no value return the passed in default value
 * otherwise return the value
 */
Boolean SMBGetDictBooleanValue(CFDictionaryRef Dict, const void * KeyValue, Boolean DefaultValue)
{
	CFBooleanRef booleanRef = NULL;
	
	if (Dict)
		booleanRef = (CFBooleanRef)CFDictionaryGetValue(Dict, KeyValue);
	if (booleanRef == NULL)
		return DefaultValue;

	return CFBooleanGetValue(booleanRef);
}

void smb_ctx_get_user_mount_info(const char *mntonname, CFMutableDictionaryRef mdict)
{
	struct UniqueSMBShareID req;
	
	bzero(&req, sizeof(req));
	req.flags = SMBFS_GET_ACCESS_INFO;
	if (get_share_mount_info(mntonname, mdict, &req) != EEXIST) {
		os_log_error(OS_LOG_DEFAULT, "Failed to get user access for mount %s", mntonname);
	}		
}

/*
 * Copy the username in and make sure its not to long.
 */
int smb_ctx_setuser(struct smb_ctx *ctx, const char *name)
{
	if (strlen(name) >= SMB_MAXUSERNAMELEN) {
		os_log_error(OS_LOG_DEFAULT, "user name '%s' too long", name);
		return ENAMETOOLONG;
	}
	strlcpy(ctx->ct_setup.ioc_user, name, SMB_MAXUSERNAMELEN);

	/* We need to tell the kernel if we are trying to do guest access */
	if (strcasecmp(ctx->ct_setup.ioc_user, kGuestAccountName) == 0)
		ctx->ct_setup.ioc_userflags |= SMBV_GUEST_ACCESS;
	else
		ctx->ct_setup.ioc_userflags &= ~(SMBV_GUEST_ACCESS | SMBV_PRIV_GUEST_ACCESS);

	return 0;
}

/* 
 * Never uppercase the Domain/Workgroup name here, because it might come from a Windows codepage encoding. 
 * We can get the domain in several different ways. Never override a domain name generated
 * by the user. Here is the priority for the domain
 *		1. Domain came from the URL. done in the parse routine
 *		2. Domain came from the configuration file.
 *		3. Domain came from the network.
 *
 * Once a user sets the domain then it is always set. The URL will always have first crack.
 */
int smb_ctx_setdomain(struct smb_ctx *ctx, const char *name)
{
	/* The user already set the domain so don't reset it */
	if (ctx->ct_setup.ioc_domain[0])
		return 0;
	
	if (strlen(name) > SMB_MAXNetBIOSNAMELEN) {
		os_log_error(OS_LOG_DEFAULT, "domain/workgroup name '%s' too long", name);
		return ENAMETOOLONG;
	}
	strlcpy(ctx->ct_setup.ioc_domain,  name, SMB_MAXNetBIOSNAMELEN+1);
	return 0;
}

int smb_ctx_setpassword(struct smb_ctx *ctx, const char *passwd, int setFlags)
{
	if (passwd == NULL)
		return EINVAL;
	if (strlen(passwd) >= SMB_MAXPASSWORDLEN) {
		os_log_error(OS_LOG_DEFAULT, "password too long", ASL_LEVEL_ERR);
		return ENAMETOOLONG;
	}
	/*
	 * They user wants to mount with an empty password. This is differnet then no password
	 * but should be treated the same.
	 */
	if (*passwd == 0)
		strlcpy(ctx->ct_setup.ioc_password, "", sizeof(ctx->ct_setup.ioc_password));
	else
		strlcpy(ctx->ct_setup.ioc_password, passwd, sizeof(ctx->ct_setup.ioc_password));
	if (setFlags) {
		ctx->ct_flags |= SMBCF_EXPLICITPWD;
	}
	return 0;
}

/*
 * Copy the share in and make sure its not to long.
 */
int smb_ctx_setshare(struct smb_ctx *ctx, const char *share)
{
	CFStringRef shareRef;
	
	if (strlen(share) >= SMB_MAXSHARENAMELEN) {
		os_log_error(OS_LOG_DEFAULT, "share name '%s' too long", share);
		return ENAMETOOLONG;
	}
	if (ctx->ct_origshare)
		free(ctx->ct_origshare);
	if ((ctx->ct_origshare = strdup(share)) == NULL) {
		return ENOMEM;
	}
	shareRef = CFStringCreateWithCString(kCFAllocatorDefault, share,  kCFStringEncodingUTF8);
	if (shareRef) {
		str_upper(ctx->ct_sh.ioc_share, sizeof(ctx->ct_sh.ioc_share), shareRef);
		CFRelease(shareRef);
	} else {
		/* Nothing else we can do here */
		strlcpy(ctx->ct_sh.ioc_share, share, sizeof(ctx->ct_sh.ioc_share));
	}

	return 0;
}

/*
 * If the call to nbns_getnodestatus(...) fails we can try one of two other
 * methods; use a name of "*SMBSERVER", which is supported by Samba (at least)
 * or, as a last resort, try the "truncate-at-dot" heuristic.
 * And the heuristic really should attempt truncation at
 * each dot in turn, left to right.
 *
 * These fallback heuristics should be triggered when the attempt to open the
 * session fails instead of in the code below.
 *
 * See http://ietf.org/internet-drafts/draft-crhertel-smb-url-07.txt
 */
static int smb_ctx_getnbname(struct smb_ctx *ctx, struct sockaddr *sap)
{
	char nbt_server[SMB_MAXNetBIOSNAMELEN + 1];
	int error;
	
	nbt_server[0] = '\0';
	/* Really need to see if we can find a match */
	error = nbns_getnodestatus(sap, &ctx->ct_nb, &ctx->prefs, NULL, nbt_server, NULL, NULL);
	/* No error and we found the servers NetBIOS name */
	if (!error && nbt_server[0])
		strlcpy(ctx->ct_ssn.ioc_srvname, nbt_server, sizeof(ctx->ct_ssn.ioc_srvname));
	else
		error = ENOENT;	/* Couldn't find the NetBIOS name */
	return error;
}

static int smb_ctx_gethandle(struct smb_ctx *ctx)
{
	int fd, i;
	char buf[20];

	if (ctx->ct_fd != -1) {
		close(ctx->ct_fd);
		ctx->ct_fd = -1;
		ctx->ct_flags &= ~SMBCF_CONNECT_STATE;	/* Remove all the connect state flags */
	}
	/*
	 * First try to open as clone
	 */
	fd = open("/dev/"NSMB_NAME, O_RDWR);
	if (fd >= 0) {
		ctx->ct_fd = fd;
		return 0;
	}
	/*
	 * well, no clone capabilities available - we have to scan
	 * all devices in order to get free one
	 */
	for (i = 0; i < 1024; i++) {
		snprintf(buf, sizeof(buf), "/dev/%s%x", NSMB_NAME, i);
		fd = open(buf, O_RDWR);
		if (fd >= 0) {
			ctx->ct_fd = fd;
			return 0;
		}	
	}
	os_log_error(OS_LOG_DEFAULT, "%d failures to open smb device, syserr = %s",
				 i+1, strerror(errno));
	return ENOENT;
}

/*
 * Cancel any outstanding connection
 */
void smb_ctx_cancel_connection(struct smb_ctx *ctx)
{	
	ctx->ct_cancel = TRUE;
	if ((ctx->ct_fd != -1) && (smb_ioctl_call(ctx->ct_fd, SMBIOC_CANCEL_SESSION, &ctx->ct_cancel) == -1))
		os_log_debug(OS_LOG_DEFAULT, "can't cancel the connection, syserr = %s",
					 strerror(errno));
}

/*
 * Return the connection state of the session. Currently only returns ENOTCONN
 * or EISCONN. May want to expand this in the future.
 */
uint16_t smb_ctx_connstate(struct smb_ctx *ctx)
{
	uint16_t connstate = 0;
	
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_SESSSTATE, &connstate) == -1) {
		os_log_debug(OS_LOG_DEFAULT, "can't get connection state for the session, syserr = %s",
					 strerror(errno));
		return ENOTCONN;
	}	
	return connstate;
}

/*
 * Should only be called if we are shaing a session, will atempt to obtain
 * the kernel's version of the client an server principal names.
 */
static void 
getAuthInfo(struct smb_ctx *ctx, uint32_t max_client_size, uint32_t	max_target_size)
{
	struct smbioc_auth_info info;
	
	if  (ctx->ct_session_flags &  (SMBV_ANONYMOUS_ACCESS | SMBV_GUEST_ACCESS | SMBV_SFS_ACCESS)) {
		/* Not an authenticated sesssion, get out nothing to do here */
		return;
	}
	if (ctx->ct_setup.ioc_gss_client_name && ctx->ct_setup.ioc_gss_target_name) {
		/* We already have what we need, get out nothing to do here */
		return;
	}
	memset(&info, 0, sizeof(info));
	info.ioc_version = SMB_IOC_STRUCT_VERSION;
	info.ioc_client_size = max_client_size;
	info.ioc_target_size = max_target_size;
	info.ioc_client_name = CAST_USER_ADDR_T(calloc(max_client_size, 1));
	info.ioc_target_name = CAST_USER_ADDR_T(calloc(max_target_size, 1));
	if (!info.ioc_client_name || !info.ioc_target_name) {
		goto done;
	}
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_AUTH_INFO, &info) == -1) {
		goto done;
	}
	if (!info.ioc_client_size || !info.ioc_target_size) {
		goto done;
	}
	if (ctx->ct_setup.ioc_gss_client_name) {
		free((void *)(uintptr_t)ctx->ct_setup.ioc_gss_client_name);
		ctx->ct_setup.ioc_gss_client_name = USER_ADDR_NULL;
	}
	ctx->ct_setup.ioc_gss_client_name = info.ioc_client_name;
	ctx->ct_setup.ioc_gss_client_size = info.ioc_client_size;
	ctx->ct_setup.ioc_gss_client_nt = info.ioc_client_nt;
	info.ioc_client_name = USER_ADDR_NULL;
	
	if (ctx->ct_setup.ioc_gss_target_name) {
		free((void *)(uintptr_t)ctx->ct_setup.ioc_gss_target_name);
		ctx->ct_setup.ioc_gss_target_name = USER_ADDR_NULL;
	}
	ctx->ct_setup.ioc_gss_target_name = info.ioc_target_name;
	ctx->ct_setup.ioc_gss_target_size = info.ioc_target_size;
	ctx->ct_setup.ioc_gss_target_nt = info.ioc_target_nt;	
	info.ioc_target_name = USER_ADDR_NULL;

done:
	if (ctx->ct_setup.ioc_gss_client_name && ctx->ct_setup.ioc_gss_target_name) {
		os_log_debug(OS_LOG_DEFAULT, "%s: Client principal name '%s' Server principal name '%s'",
					 __FUNCTION__,
					 (char *)(uintptr_t)ctx->ct_setup.ioc_gss_client_name,
					 (char *)(uintptr_t)ctx->ct_setup.ioc_gss_target_name);
	}
	if (info.ioc_client_name != USER_ADDR_NULL) {
		free((void *)(uintptr_t)info.ioc_client_name);
	}
	if (info.ioc_target_name != USER_ADDR_NULL) {
		free((void *)(uintptr_t)info.ioc_target_name);
	}
}

static bool
needToSpawnMCNotifier(int pid)
{
    bool ret_val = false;
    if ((pid == -1) || (kill(pid, 0)))
    {
        os_log_debug(OS_LOG_DEFAULT, "%s: Seems like MC notifier process is not running",
                     __FUNCTION__);
        ret_val = true;
    }

    return ret_val;
}

/*
 * This routine actually does the whole connection. So if the ctx has a connection
 * this routine will break it and start the whole connection process over.
 */
static int findMatchingSession(struct smb_ctx *ctx,
                               CFMutableArrayRef addressArray,
                               void *sessionp,
                               Boolean forceHiFi)
{
	struct smbioc_negotiate	rq;
	CFMutableDataRef addressData;
	struct connectAddress *conn;
	CFIndex ii;
	int	error = smb_ctx_gethandle(ctx);

    if (error) {
		return (error);
    }
	
	error = ENOTCONN;
	for (ii = 0; ii < CFArrayGetCount(addressArray); ii++) {
		addressData = (CFMutableDataRef)CFArrayGetValueAtIndex(addressArray, ii);
        if (!addressData) {
			continue;
        }
        
		conn = (struct connectAddress *)((void *)CFDataGetMutableBytePtr(addressData));
        if (!conn) {
			continue;
        }
			
		bzero(&rq, sizeof(rq));
		rq.ioc_version = SMB_IOC_STRUCT_VERSION;
		rq.ioc_saddr_len = conn->addr.sa_len;
		rq.ioc_saddr = &conn->addr;
		bcopy(&ctx->ct_ssn, &rq.ioc_ssn, sizeof(struct smbioc_ossn));
		/* ct_setup.ioc_user and rq.ioc_user must be the same size */
		bcopy(&ctx->ct_setup.ioc_user, &rq.ioc_user, sizeof(rq.ioc_user));
		rq.ioc_negotiate_token = CAST_USER_ADDR_T(calloc(SMB_IOC_SPI_INIT_SIZE, 1));
		if (rq.ioc_negotiate_token != USER_ADDR_NULL) {
			rq.ioc_negotiate_token_len = SMB_IOC_SPI_INIT_SIZE;
		}
		
        /* Should we also try to match on the dns name? */
        if ((ctx->ct_flags & SMBCF_MATCH_DNS) &&
            (ctx->serverName != NULL) &&
            (strnlen(ctx->serverName, 255) > 0)) {
            strlcpy(rq.ioc_dns_name, ctx->serverName, sizeof(rq.ioc_dns_name));
            rq.ioc_extra_flags |= SMB_MATCH_DNS;
#ifdef SMB_DEBUG
			os_log_error(OS_LOG_DEFAULT, "findMatchingSession: try to match DNS name of ctx->serverName <%s> ",
						 
						 ctx->serverName);
#endif
        }
        
        if (ctx->ct_flags & SMBCF_HIFI_REQUESTED) {
#ifdef SMB_DEBUG
            os_log_error(OS_LOG_DEFAULT, "%s: HiFi requested ", __FUNCTION__);
#endif
            rq.ioc_extra_flags |= SMB_HIFI_REQUESTED;
        }

        if (forceHiFi) {
            /*
             * <67014611> Must be SMBOpenServerWithMountPoint() calling
             * findMountPointSession() which calls here looking for the
             * mountpoint.
             */
            rq.ioc_extra_flags |= SMB_HIFI_REQUESTED;
        }

        if (sessionp != NULL) {
            rq.ioc_sessionp = sessionp;
        }

        /* Call the kernel to see if we already have a session */
        if (smb_ioctl_call(ctx->ct_fd, SMBIOC_FIND_SESSION, &rq) == -1) {
			error = errno;	/* Some internal error happen? */
        }
        else {
			error = rq.ioc_errno;	/* The real error */
        }
		if (error) {
			if (rq.ioc_negotiate_token) {
				free((void *)((uintptr_t)(rq.ioc_negotiate_token)));
			}
			continue;
		}
        
		if (rq.ioc_negotiate_token_len > SMB_IOC_SPI_INIT_SIZE)  {
			/* Just log it and then pretend that we didn't get any mech info */
			os_log_debug(OS_LOG_DEFAULT, "%s: %s mech info too large %d",
						 __FUNCTION__, ctx->serverName, rq.ioc_negotiate_token_len);
			rq.ioc_negotiate_token_len = 0;
			if (rq.ioc_negotiate_token) {
				free((void *)((uintptr_t)(rq.ioc_negotiate_token)));
                rq.ioc_negotiate_token = USER_ADDR_NULL;
			}
		}
		
		if (ctx->mechDict) {
			CFRelease(ctx->mechDict);
			ctx->mechDict = NULL;
		}
		
		/* Server return a negotiate token, get the mech dictionary */
		if (rq.ioc_negotiate_token_len) {
			CFDataRef NegotiateToken = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)(uintptr_t)rq.ioc_negotiate_token, rq.ioc_negotiate_token_len);
			if (NegotiateToken) {
				ctx->mechDict = KRBDecodeNegTokenInit(kCFAllocatorDefault, NegotiateToken);
				CFRelease(NegotiateToken);
			}
		}
		
		if ((ctx->mechDict == NULL) && (ctx->ct_session_caps & SMB_CAP_EXT_SECURITY)) {
			/* 
			 * The server does extended security, but they didn't return any mech 
			 * types. Then create a default RAW NTLMSSP mech dictionary.
			 */
			ctx->mechDict = KRBCreateNegTokenLegacyNTLM(kCFAllocatorDefault);		
			ctx->ct_flags |= SMBCF_RAW_NTLMSSP;
		}
	
		/* Free it if we have one */
        if (ctx->ct_saddr) {
			free(ctx->ct_saddr);
        }
        
		/* We have a good sockaddr make a copy */
		ctx->ct_saddr = malloc(conn->addr.sa_len);
        if (ctx->ct_saddr) {
			memcpy(ctx->ct_saddr, &conn->addr, conn->addr.sa_len);
        }
        
		/* Get the server's capablilities */
		ctx->ct_session_caps = rq.ioc_ret_caps;
#ifdef SMB_DEBUG
		os_log_error(OS_LOG_DEFAULT, "findMatchingSession: we found a session to share for ctx->serverName <%s> ",
					 ctx->serverName);
#endif
		/* Get the session flags */
		ctx->ct_session_flags = rq.ioc_ret_session_flags;
		if ((rq.ioc_extra_flags & SMB_SHARING_SESSION) &&
            (ctx->ct_session_flags & SMBV_AUTH_DONE)) {
			ctx->ct_flags |= SMBCF_AUTHORIZED | SMBCF_CONNECTED;
			getAuthInfo(ctx,  rq.ioc_max_client_size, rq.ioc_max_target_size);
			ctx->ct_session_shared = TRUE;
#ifdef SMB_DEBUG
			os_log_error(OS_LOG_DEFAULT, "findMatchingSession: ss_len %d for ctx->serverName <%s> ",
						 rq.ioc_shared_saddr.ss_len,
						 ctx->serverName);
#endif
			if (rq.ioc_shared_saddr.ss_len > 0) {
                /*
                 * Copy in the shared session's TCP address so that
                 * create_unique_id() will create the correct ID to match on
                 */
                
                /* Free it if we have one */
                if (ctx->ct_saddr) {
                    free(ctx->ct_saddr);
                }
                
                /* We have a good sockaddr make a copy */
                ctx->ct_saddr = malloc(rq.ioc_shared_saddr.ss_len);
                if (ctx->ct_saddr) {
                    memcpy(ctx->ct_saddr, &rq.ioc_shared_saddr, rq.ioc_shared_saddr.ss_len);
                }
            }
		}
        else {
			ctx->ct_session_shared = FALSE;
		}
        
		/* If we have no username and the kernel does then use the name in the kernel */
		if ((ctx->ct_setup.ioc_user[0] == 0) && rq.ioc_user[0]) {
			strlcpy(ctx->ct_setup.ioc_user, rq.ioc_user, sizeof(ctx->ct_setup.ioc_user));
		}
        
        if (rq.ioc_negotiate_token != USER_ADDR_NULL) {
            free((void *)((uintptr_t)(rq.ioc_negotiate_token)));
        }
	
		break; /* We found one so we are done */
	}
    
	if (error) {
		close(ctx->ct_fd);
		ctx->ct_fd = -1;
	}
	return (error);
}

/*
 * This routine actually does the whole connection. So if the ctx has a connection
 * this routine will break it and start the whole connection process over.
 */
static int smb_negotiate(struct smb_ctx *ctx, struct sockaddr *raddr,
						 struct sockaddr *laddr, int forceNewSession)
{
	struct smbioc_negotiate	rq;
	int	error = 0;
    struct timespec timeout;

	error = smb_ctx_gethandle(ctx);
	if (error)
		return (error);
	
	ctx->ct_flags &= ~SMBCF_RAW_NTLMSSP;
	bzero(&rq, sizeof(rq));
	rq.ioc_version = SMB_IOC_STRUCT_VERSION;
	if (raddr) {
		rq.ioc_saddr_len = raddr->sa_len;
		rq.ioc_saddr = raddr;
	}
	if (laddr) {
		rq.ioc_laddr = laddr;
		rq.ioc_laddr_len = laddr->sa_len;
	}
	bcopy(&ctx->ct_ssn, &rq.ioc_ssn, sizeof(struct smbioc_ossn));
	/* ct_setup.ioc_user and rq.ioc_user must be the same size */
	bcopy(&ctx->ct_setup.ioc_user, &rq.ioc_user, sizeof(rq.ioc_user));
	/* Pass the user request auth types and other user settable flags. */
	rq.ioc_userflags = ctx->ct_setup.ioc_userflags;
	rq.ioc_negotiate_token = CAST_USER_ADDR_T(calloc(SMB_IOC_SPI_INIT_SIZE, 1));
	if (rq.ioc_negotiate_token == USER_ADDR_NULL) {
		error = ENOMEM;
		goto out;
	}
	
	/* They want a new session to the server */
	if (forceNewSession) {
		rq.ioc_extra_flags |= SMB_FORCE_NEW_SESSION;
	}

	/* What versions of SMB are allowed? */
	if (ctx->prefs.protocol_version_map & 0x0001) {
		rq.ioc_extra_flags |= SMB_SMB1_ENABLED;
	}
	
	if (ctx->prefs.protocol_version_map & 0x0002) {
		rq.ioc_extra_flags |= SMB_SMB2_ENABLED;
	}
	
	if (ctx->prefs.protocol_version_map & 0x0004) {
		rq.ioc_extra_flags |= SMB_SMB3_ENABLED;
	}
    
    if (ctx->prefs.altflags & SMBFS_MNT_DISABLE_311) {
        /* obtained from nsmb.conf */
        rq.ioc_extra_flags |= SMB_DISABLE_311;
    }

    if (rq.ioc_extra_flags & (SMB_SMB2_ENABLED | SMB_SMB3_ENABLED)) {
        if (ctx->prefs.altflags & SMBFS_MNT_MULTI_CHANNEL_ON) {  // obtained from nsmb.conf
            rq.ioc_extra_flags |= SMB_MULTICHANNEL_ENABLE;
            rq.ioc_mc_max_channel = ctx->prefs.mc_max_channels;
            rq.ioc_mc_max_rss_channel = ctx->prefs.mc_max_rss_channels;
            bcopy(ctx->prefs.mc_client_if_blacklist,
                  rq.ioc_mc_client_if_blacklist,
                  sizeof(rq.ioc_mc_client_if_blacklist));
            rq.ioc_mc_client_if_blacklist_len = ctx->prefs.mc_client_if_blacklist_len;
            if (ctx->prefs.altflags & SMBFS_MNT_MC_PREFER_WIRED) {
                rq.ioc_extra_flags |= SMB_MC_PREFER_WIRED;
            }
        }
    }

	/* Check to see what versions of SMB require signing */
    if (ctx->prefs.signing_required) {
        rq.ioc_extra_flags |= SMB_SIGNING_REQUIRED;
		
		/* What versions of SMB required signing? */
		if (ctx->prefs.signing_req_versions & 0x01) {
			rq.ioc_extra_flags |= SMB_SMB1_SIGNING_REQ;
		}
		
		if (ctx->prefs.signing_req_versions & 0x02) {
			rq.ioc_extra_flags |= SMB_SMB2_SIGNING_REQ;
		}
		
		if (ctx->prefs.signing_req_versions & 0x04) {
			rq.ioc_extra_flags |= SMB_SMB3_SIGNING_REQ;
		}
    }
	
    /* 
     * See if "cifs://" was specified. 
	 * Specifying "cifs://" forces us to only try SMB 1
     */
	CFStringRef scheme = CFURLCopyScheme (ctx->ct_url);
	
	if (scheme != NULL) {
		if (kCFCompareEqualTo == CFStringCompare (scheme, CFSTR("cifs"),
												  kCFCompareCaseInsensitive)) {
			/* Is SMB 1 even enabled? */
			if (!(rq.ioc_extra_flags & SMB_SMB1_ENABLED)) {
				os_log_error(OS_LOG_DEFAULT, "%s: cifs specified but SMB 1 is not enabled",
							 __FUNCTION__);
				error = ENETFSNOPROTOVERSSUPP;
                CFRelease(scheme);
				goto out;
			}
			else {
				os_log_debug(OS_LOG_DEFAULT, "%s: cifs specified so force using SMB 1",
							 __FUNCTION__);
				
				/* Clear SMB 2 and 3 enabled flags in case they are set */
				rq.ioc_extra_flags |= SMB_SMB1_ENABLED;
				rq.ioc_extra_flags &= ~(SMB_SMB2_ENABLED | SMB_SMB3_ENABLED);
			}
		}

		CFRelease(scheme);
	}
	
    /* Pass in the max_resp_timeout */
    rq.ioc_max_resp_timeout = ctx->prefs.max_resp_timeout;
    
    rq.ioc_negotiate_token_len = SMB_IOC_SPI_INIT_SIZE;

    /* Need Client Guid for SMB 2/3 Neg request */
    timeout.tv_nsec = 0;
    timeout.tv_sec = 180;   /* 3 mins should be plenty of time to get uuid */
    
    /* prefill with 1's in case we fail to get the host uuid */
    memset(rq.ioc_client_guid, 1, sizeof(rq.ioc_client_guid));
    
    error = gethostuuid(rq.ioc_client_guid, &timeout);
    if (error) {
		os_log_error(OS_LOG_DEFAULT, "gethostuuid failed %d", errno);
    }
    
	/* Should we also try to match on the dns name? */
	if ((ctx->ct_flags & SMBCF_MATCH_DNS) &&
		(ctx->serverName != NULL) &&
		(strnlen(ctx->serverName, 255) > 0)) {
		strlcpy(rq.ioc_dns_name, ctx->serverName, sizeof(rq.ioc_dns_name));
		rq.ioc_extra_flags |= SMB_MATCH_DNS;
#ifdef SMB_DEBUG
		os_log_error(OS_LOG_DEFAULT, "smb_negotiate: try to match DNS name of ctx->serverName <%s> ",
					 ctx->serverName);
#endif
	}

    if (ctx->ct_flags & SMBCF_HIFI_REQUESTED) {
#ifdef SMB_DEBUG
        os_log_error(OS_LOG_DEFAULT, "%s: HiFi requested ", __FUNCTION__);
#endif
        rq.ioc_extra_flags |= SMB_HIFI_REQUESTED;
    }

    /* Call the kernel to make the negotiate call */
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_NEGOTIATE, &rq) == -1)
		error = errno;	/* Some internal error happen? */
	else
		error = rq.ioc_errno;	/* The real error */

	if (error) {
		os_log_debug(OS_LOG_DEFAULT, "%s: negotiate ioctl failed, syserr = %s",
					 __FUNCTION__, strerror(error));
		goto out;
	}

	if (rq.ioc_negotiate_token_len > SMB_IOC_SPI_INIT_SIZE)  {
		/* Just log it and then pretend that we didn't get any mech info */
		os_log_debug(OS_LOG_DEFAULT, "%s: %s mech info too large %d",
					 __FUNCTION__, ctx->serverName, rq.ioc_negotiate_token_len);
		rq.ioc_negotiate_token_len = 0;
	}
    
	/* Get the server's capablilities */
	ctx->ct_session_caps = rq.ioc_ret_caps;
	/* Get the session flags */
	ctx->ct_session_flags = rq.ioc_ret_session_flags;
	if ((rq.ioc_extra_flags & SMB_SHARING_SESSION) &&
        (ctx->ct_session_flags & SMBV_AUTH_DONE)) {
		ctx->ct_flags |= SMBCF_AUTHORIZED;
		getAuthInfo(ctx,  rq.ioc_max_client_size, rq.ioc_max_target_size);
		ctx->ct_session_shared = TRUE;
#ifdef SMB_DEBUG
		os_log_error(OS_LOG_DEFAULT, "smb_negotiate: ss_len %d for ctx->serverName <%s> ",
					 
					 rq.ioc_shared_saddr.ss_len,
					 ctx->serverName);
#endif		
		if (rq.ioc_shared_saddr.ss_len > 0) {
			/*
			 * Copy in the shared session's TCP address so that
			 * create_unique_id() will create the correct ID to match on
			 */
			
			/* Free it if we have one */
			if (ctx->ct_saddr) {
				free(ctx->ct_saddr);
			}
			
			/* We have a good sockaddr make a copy */
			ctx->ct_saddr = malloc(rq.ioc_shared_saddr.ss_len);
			if (ctx->ct_saddr) {
				memcpy(ctx->ct_saddr, &rq.ioc_shared_saddr, rq.ioc_shared_saddr.ss_len);
			}
		}
	}
	else {
		ctx->ct_session_shared = FALSE;
	}
    
	/* If we have no username and the kernel does then use the name in the kernel */
	if ((ctx->ct_setup.ioc_user[0] == 0) && rq.ioc_user[0]) {
		strlcpy(ctx->ct_setup.ioc_user, rq.ioc_user, sizeof(ctx->ct_setup.ioc_user));
		os_log_debug(OS_LOG_DEFAULT, "%s: ctx->ct_setup.ioc_user = %s",
					 __FUNCTION__, ctx->ct_setup.ioc_user);		
	}
	
	if (ctx->mechDict) {
		CFRelease(ctx->mechDict);
		ctx->mechDict = NULL;
	}
	
	/* Server return a negotiate token, get the mech dictionary */
	if (rq.ioc_negotiate_token_len) {
		CFDataRef NegotiateToken = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)(uintptr_t)rq.ioc_negotiate_token, rq.ioc_negotiate_token_len);
		if (NegotiateToken) {
			ctx->mechDict = KRBDecodeNegTokenInit(kCFAllocatorDefault, NegotiateToken);
			CFRelease(NegotiateToken);
		}
	}
    
    /* Remove this when <12991970> is fixed */
    if ((ctx->mechDict) && (ctx->prefs.altflags & SMBFS_MNT_KERBEROS_OFF)) {
        if (CFDictionaryGetValue(ctx->mechDict, KSPNEGOSupportsLKDC)) {
            os_log_debug(OS_LOG_DEFAULT, "%s: Leaving LKDC on", __FUNCTION__);
        }
        else {
            os_log_error(OS_LOG_DEFAULT, "%s: Kerberos turned off in preferences", __FUNCTION__);
            CFRelease(ctx->mechDict);
            ctx->mechDict = NULL;
        }
    }
    
    if ((ctx->mechDict == NULL) && (ctx->ct_session_caps & SMB_CAP_EXT_SECURITY)) {
		/* 
		 * The server does extended security, but they didn't return any mech 
		 * types. Then create a default RAW NTLMSSP mech dictionary.
		 */
		ctx->mechDict = KRBCreateNegTokenLegacyNTLM(kCFAllocatorDefault);		
		ctx->ct_flags |= SMBCF_RAW_NTLMSSP;
	}
	
out:
	if (error) {
		/* 
		 * If we have an EINTR error then the user canceled the connection. Never
		 * log that as an error.
		 */
		if (error == EINTR)
			error = ECANCELED;
		/* We got an error and its not a cancel error log it */
		if (error && (error != ECANCELED))
			os_log_debug(OS_LOG_DEFAULT, "%s: negotiate phase failed %s, syserr = %s", __FUNCTION__,
						 (ctx->serverName) ? ctx->serverName : "", strerror(error));
		close(ctx->ct_fd);
		ctx->ct_fd = -1;
	}
	if (rq.ioc_negotiate_token) {
		free((void *)((uintptr_t)(rq.ioc_negotiate_token)));
	}
	return (error);
}

/*
 * Do a tree disconnect with the last tree we connected on.
 */
int smb_share_disconnect(struct smb_ctx *ctx)
{
	int error = 0;
	
	if ((ctx->ct_fd < 0) || ((ctx->ct_flags & SMBCF_SHARE_CONN) != SMBCF_SHARE_CONN))
		return 0;	/* Nothing to do here */
	
	ctx->ct_sh.ioc_version = SMB_IOC_STRUCT_VERSION;
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_TDIS, &ctx->ct_sh) == -1) {
		error = errno;
		if (error != ENOTCONN) {
			os_log_debug(OS_LOG_DEFAULT, "%s: tree disconnect failed, syserr = %s",
						 __FUNCTION__, strerror(error));
		}
	}
	
	if (!error)
		ctx->ct_flags &= ~SMBCF_SHARE_CONN;
	
	return error;
}

/*
 * Do a tree connect.
 */
int smb_share_connect(struct smb_ctx *ctx)
{
	int error = 0;
	
	if ((ctx->ct_flags & SMBCF_AUTHORIZED) == 0)
		return EAUTH;

	/* Make a tree disconnect if we have a tree connect. */
	error = smb_share_disconnect(ctx);
	if (error == 0) {
		ctx->ct_sh.ioc_version = SMB_IOC_STRUCT_VERSION;
		ctx->ct_sh.ioc_optionalSupport = 0;
		ctx->ct_sh.ioc_fstype = 0;
		if (smb_ioctl_call(ctx->ct_fd, SMBIOC_TCON, &ctx->ct_sh) == -1)
			error = errno;
	}
	if (error == 0)
		ctx->ct_flags |= SMBCF_SHARE_CONN;

	return (error);
}

/*
 * Return the tree connect optional support flags
 */
uint16_t smb_tree_conn_optional_support_flags(struct smb_ctx *ctx)
{
	if (ctx->ct_flags & SMBCF_SHARE_CONN)
		return ctx->ct_sh.ioc_optionalSupport;
	return 0;
}

/*
 * Return the tree connect optional support flags
 */
uint32_t
smb_tree_conn_fstype(struct smb_ctx *ctx)
{
	if (ctx->ct_flags & SMBCF_SHARE_CONN)
		return ctx->ct_sh.ioc_fstype;
	return 0;
}

/* 
 * Update the session properties, calling routine should make sure we have a connection.
 */
void smb_get_session_properties(struct smb_ctx *ctx)
{
	struct smbioc_session_properties properties;

	memset(&properties, 0, sizeof(properties));
	properties.ioc_version = SMB_IOC_STRUCT_VERSION;
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_SESSION_PROPERTIES, &properties) == -1)
		os_log_error(OS_LOG_DEFAULT, "%s: Getting the session properties failed, syserr = %s",
					 __FUNCTION__, strerror(errno));
	else {
        size_t model_len = 0;
        
		ctx->ct_session_uid = properties.uid;
        ctx->ct_session_caps = properties.smb1_caps;
        ctx->ct_session_smb2_caps = properties.smb2_caps;
		ctx->ct_session_flags = properties.flags;
        ctx->ct_session_misc_flags = properties.misc_flags;
        ctx->ct_session_hflags = properties.hflags;
        ctx->ct_session_hflags2 = properties.hflags2;
		ctx->ct_session_txmax = properties.txmax;			
		ctx->ct_session_rxmax = properties.rxmax;
        ctx->ct_session_wxmax = properties.wxmax;
        /* if we are mac to mac and SMB 2/3 then we have model info, so update it */
        if (ctx->model_info) {
            free(ctx->model_info);
            ctx->model_info = NULL;
        }
        model_len = strnlen(properties.model_info, sizeof(properties.model_info));
        if ((properties.misc_flags & SMBV_OSX_SERVER) && (model_len > 0)) {
            /* SMBV_OSX_SERVER and we have the model information from session */
            ctx->model_info = malloc(model_len + 1); /* add space for null */
            if (ctx->model_info) {
                memset(ctx->model_info, 0, model_len);
                memcpy(ctx->model_info, properties.model_info, model_len);
            }
        }
	}
}

static void smb_session_reset_security(struct smb_ctx *ctx)
{
	ctx->ct_setup.ioc_version = SMB_IOC_STRUCT_VERSION;
	/* Remove any previous client name */
	if (ctx->ct_setup.ioc_gss_client_name) {
		free((void *)(uintptr_t)ctx->ct_setup.ioc_gss_client_name);
		ctx->ct_setup.ioc_gss_client_name = USER_ADDR_NULL;
	}
	ctx->ct_setup.ioc_gss_client_size = 0;
	ctx->ct_setup.ioc_gss_client_nt = GSSD_STRING_NAME;		
	
	/* Remove any previous target name */
	if (ctx->ct_setup.ioc_gss_target_name) {
		free((void *)(uintptr_t)ctx->ct_setup.ioc_gss_target_name);
		ctx->ct_setup.ioc_gss_target_name = USER_ADDR_NULL;
	}
	ctx->ct_setup.ioc_gss_target_size = 0;
	ctx->ct_setup.ioc_gss_target_nt = GSSD_STRING_NAME;		
}

static int MakeTarget(struct smb_ctx *ctx, const char *serverPrincipal, gssd_nametype serverNameType)
{	
	if (serverPrincipal) {
		ctx->ct_setup.ioc_gss_target_size = (uint32_t)strlen(serverPrincipal) + 1;
		ctx->ct_setup.ioc_gss_target_name = CAST_USER_ADDR_T(malloc(ctx->ct_setup.ioc_gss_target_size));
		if (ctx->ct_setup.ioc_gss_target_name) {
			memcpy((char *)(uintptr_t)ctx->ct_setup.ioc_gss_target_name, serverPrincipal, 
			       ctx->ct_setup.ioc_gss_target_size);
		}
	} else { 
		GetTargetNameUsingHostName(ctx);
	}
	/* Set the target name type */
	ctx->ct_setup.ioc_gss_target_nt = serverNameType;
	
	/* Couldn't get a target name, nothing more we can do here */
	if (ctx->ct_setup.ioc_gss_target_name == USER_ADDR_NULL) {
		os_log_error(OS_LOG_DEFAULT, "%s: Couldn't create server name!", __FUNCTION__);
		ctx->ct_setup.ioc_gss_target_size = 0;
		return ENOMEM;
	}
	return 0;
}
	
static int smb_session_send_auth(struct smb_ctx *ctx)
{
	int error = 0;

	/* Doing extended security, but they don't support SPEGNO NTLMSSP */
	if (ctx->ct_flags & SMBCF_RAW_NTLMSSP) { 
		/* We are doing RAW NTLMSSP */
		ctx->ct_setup.ioc_userflags |= SMBV_RAW_NTLMSSP; 
	}

	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_SSNSETUP, &ctx->ct_setup) == -1)
		error = errno;
	
	if (error == 0) {
		
		ctx->ct_flags |= SMBCF_AUTHORIZED;
		smb_get_session_properties(ctx);	/* Update the the session flags in case they have changed */	
	}
    
	return (error);
}


/*
 * It seems that the NTLM client principal name can have two different formats:
 *	1. domain\username - Traditional NTLM style
 *	2. username@domain - Kerberos style
 *
 * Seems Heimdal always displays and uses the Kerberos style so lets always 
 * create that style. If the domain contians a server name then it should begin
 * with a backslash.
 */
static void 
smb_session_set_user(struct smb_ctx *ctx, const char *clientpn)
{
	char *k_component;
	
	if (ctx->ct_setup.ioc_user[0]) {
		return; /* We already have a user name nothing to do here */
	}
	strlcpy(ctx->ct_setup.ioc_user, clientpn, SMB_MAXUSERNAMELEN + 1);
	k_component = strchr(ctx->ct_setup.ioc_user, KERBEROS_INSTANCE_DELIMITER);
	if (k_component == NULL) {
		k_component = strchr(ctx->ct_setup.ioc_user, KERBEROS_REALM_DELIMITER);
	}
	if (k_component) {
		*k_component = '\0';
	}
}

static gssd_nametype get_nt_from_oid(gss_OID mech)
{
	if (gss_oid_equal(GSS_KRB5_MECHANISM, mech))
		return GSSD_KRB5_PRINCIPAL;
	else if (gss_oid_equal(GSS_NTLM_MECHANISM, mech))
		return GSSD_NTLM_PRINCIPAL;
	else {
		return GSSD_NTLM_PRINCIPAL;	/* Not sure what we should return as a default */
	}
}

static int smb_session_set_client(struct smb_ctx *ctx, const char *clientpn, gssd_nametype nt)
{
	if (ctx->ct_setup.ioc_gss_client_name) {
		free((void *)(uintptr_t)ctx->ct_setup.ioc_gss_client_name);
		ctx->ct_setup.ioc_gss_client_name = USER_ADDR_NULL;
	}
	
	ctx->ct_setup.ioc_gss_client_size = (uint32_t)strlen(clientpn) + 1;
	ctx->ct_setup.ioc_gss_client_name = CAST_USER_ADDR_T(malloc(ctx->ct_setup.ioc_gss_client_size));
	if (ctx->ct_setup.ioc_gss_client_name) {
		memcpy((char *)(uintptr_t)ctx->ct_setup.ioc_gss_client_name, clientpn, 
		       ctx->ct_setup.ioc_gss_client_size);
		ctx->ct_setup.ioc_gss_client_nt = nt;
	} else {
		os_log_debug(OS_LOG_DEFAULT, "Creating client name '%s' failed!", clientpn);
		ctx->ct_setup.ioc_gss_client_size = 0;
		return (ENOMEM);
	}
	return (0);
}

/*
 * It seems that the NTLM client principal name can have two different formats:
 *	1. domain\username - Traditional NTLM style
 *	2. username@domain - Kerberos style
 *
 * Seems Heimdal always displays and uses the Kerberos style so lets always 
 * create that style. If the domain contians a server name then it should begin
 * with a backslash.
 */
static int 
smb_session_set_ntlm_name(struct smb_ctx *ctx, const char *name, const char *domain)
{
	char ntlm_name[SMB_MAX_NTLM_NAME + 1];
	
	strlcpy(ntlm_name, name, sizeof(ntlm_name));
	strlcat(ntlm_name, "@", sizeof(ntlm_name));
	strlcat(ntlm_name, domain, sizeof(ntlm_name));
	return (smb_session_set_client(ctx, ntlm_name, GSSD_NTLM_PRINCIPAL));
}

static int 
smb_session_cache(struct smb_ctx *ctx, gss_OID mech, const char *name, 
				  const char *domain)
{
	struct smb_gss_cred_list *cl;
	struct smb_gss_cred_list_entry *ep, *tmp;
	int error;

	error = smb_gss_get_cred_list(&cl, mech);
	if (error)
		return (error);
	
	if (TAILQ_EMPTY(cl)) { // No credentials nothing to try
		error = ENOENT;
		goto out;
	}
	
	if (name && *name) {
		TAILQ_FOREACH_SAFE(ep, cl, next, tmp) {
			if (smb_gss_match_cred_entry(ep, mech, name, domain)) {
				error = smb_session_set_client(ctx, ep->principal, get_nt_from_oid(mech));
				if (error)
					goto out;
				error = smb_session_send_auth(ctx);
				if (error == 0)
					goto out;
				TAILQ_REMOVE(cl, ep, next);
				smb_gss_free_cred_entry(&ep);
			}
		}
		/* They gave us a user name and we didn't find a match, return EAUTH */
		return (EAUTH);
		
	}
	
	/* Found no creds that match the user name, try the others now */
	TAILQ_FOREACH(ep, cl, next) {
		error = smb_session_set_client(ctx, ep->principal, get_nt_from_oid(mech));
		if (error)
			goto out;
		/* Set the user name so the kernel will have a copy */
		smb_session_set_user(ctx, ep->principal);
		error = smb_session_send_auth(ctx);
		if (error) {
			/* We failed clear out the user name */
			smb_ctx_setuser(ctx, "");
		} else {
			goto out;
		}
	}
out:
	
	smb_gss_free_cred_list(&cl);
	return (error);
}

/*
 * Given a authentication dictionary use the information provided to authenticate
 * the connection.
 */
static int AuthenticateWithDictionary(struct smb_ctx *ctx, CFDictionaryRef authInfoDict)
{
	int error = 0;
	CFStringRef clientPrincipalRef = CFDictionaryGetValue(authInfoDict, kNAHClientPrincipal);
	CFStringRef serverPrincipalRef = CFDictionaryGetValue(authInfoDict, kNAHServerPrincipal);
	CFNumberRef clientNameTypeRef = CFDictionaryGetValue (authInfoDict, kNAHClientNameTypeGSSD);
	CFNumberRef serverNameTypeRef = CFDictionaryGetValue (authInfoDict, kNAHServerNameTypeGSSD);
	CFStringRef inferedUserNameRef = CFDictionaryGetValue(authInfoDict, kNAHInferredLabel);

	char clientPrincipal[SMB_MAX_KERB_PN] = {0};
	char serverPrincipal[SMB_MAX_KERB_PN] = {0};
	char userName[SMB_MAX_KERB_PN] = {0};
	int32_t clientNameType = 0, serverNameType = 0;
	
	/* We require the dictionary to contain all the information needed */
	if (!clientPrincipalRef || !serverPrincipalRef || !clientNameTypeRef || !serverNameTypeRef) {
		os_log_error(OS_LOG_DEFAULT, "%s: authorization dictionary is incomplete, syserr = %s",
					 __FUNCTION__, strerror(EINVAL));
		return EINVAL;	/* Should we return EAUTH here? */
	}
	CFStringGetCString(clientPrincipalRef, clientPrincipal, SMB_MAX_KERB_PN, kCFStringEncodingUTF8);
    if (inferedUserNameRef) {
        CFStringGetCString(inferedUserNameRef, userName, SMB_MAX_KERB_PN, kCFStringEncodingUTF8);
        os_log_debug(OS_LOG_DEFAULT, "%s: infered User Name = %s", __FUNCTION__, userName);
    } else {
        CFStringGetCString(clientPrincipalRef, userName, SMB_MAX_KERB_PN, kCFStringEncodingUTF8);
    }
	CFStringGetCString(serverPrincipalRef, serverPrincipal, SMB_MAX_KERB_PN, kCFStringEncodingUTF8);
	CFNumberGetValue(clientNameTypeRef, kCFNumberSInt32Type, &clientNameType);
	CFNumberGetValue(serverNameTypeRef, kCFNumberSInt32Type, &serverNameType);
	
	smb_session_reset_security(ctx);
	
	/*
	 * LHA reqested we do it this way, may want to change this in the future. The
	 * serverIsDomainController flag means we were passed in an AD Domain name but
	 * we connected to one of domain controllers. We are being passed down the 
	 * AD domain name, but if we used that name kerberos may fail. If we put 
	 * the domain controller name into to targe name, then this should always
	 * work. In the future I think we should pass the domain controllers up to
	 * and let Helimdal use it.
	 */
	if (ctx->serverIsDomainController) {
		error = MakeTarget(ctx, NULL, GSSD_HOSTBASED);
		
	} else {
		error = MakeTarget(ctx, serverPrincipal, serverNameType);
		
	}
	
	/* We always create a target name if we are doing extended security. */
	if (error)
		return (error);
	
	/* Now see if we have a client name */
	smb_session_set_user(ctx, userName);
	error = smb_session_set_client(ctx, clientPrincipal, clientNameType);
	if (error)
		return (error);
		
	error = smb_session_send_auth(ctx);
	/* Will need to reset this if these names ever become binary */
	os_log_debug(OS_LOG_DEFAULT, "%s: Authentication %s, client principal %s - %d server principal %s - %d gss client principal %s - %d gss server principal %s - %d for %s",
				 __FUNCTION__,
				 (error) ? "FAILED" : "SUCCEED", 
				 clientPrincipal, clientNameType,  
				 serverPrincipal, serverNameType,
				 (char *)(uintptr_t)ctx->ct_setup.ioc_gss_client_name,  
				 ctx->ct_setup.ioc_gss_client_nt,
				 (char *)(uintptr_t)ctx->ct_setup.ioc_gss_target_name,  
				 ctx->ct_setup.ioc_gss_target_nt,
				 ctx->serverName);
	return (error);
}

/*
 * Use the information obtain from the URL to authenticate. This may include
 * looking into the cache.
 */
static int AuthenticateWithURL(struct smb_ctx *ctx, Boolean useNTLM)
{
	int foundInCache = FALSE;
	const char *name = ctx->ct_setup.ioc_user;
	const char *domain = NULL;
	gss_OID mech = (useNTLM) ? GSS_NTLM_MECHANISM : GSS_KRB5_MECHANISM;
	const char *passwd = NULL;
	int error = 0;
	char serverDomain[SMB_MAX_NTLM_NAME + 1];
	
	smb_session_reset_security(ctx);
	
	/* We always create a target name if we are doing extended security. */
	error = MakeTarget(ctx, NULL, GSSD_HOSTBASED);
	if (error) {
		goto done;
	}
	/* The NTLM creds are store in user@domain or user@server */
	if (useNTLM) {		
		if (ctx->ct_setup.ioc_domain[0]) {
			/* We have a domain use it */
			domain = ctx->ct_setup.ioc_domain;
		} else {
			/* 
			 * No domain use the server name as the domain, but make sure it 
			 * starts with a backslash so Hemidal can tell that its a server name
			 */
			strlcpy(serverDomain, "\\", sizeof(serverDomain));
			strlcat(serverDomain, ctx->serverName, sizeof(serverDomain));
			domain = serverDomain;
		}
	}
	/* 
	 * Try and acquire credentials if we were given a name, domain and/or password,
	 * then use those credentials. If we can't acquire credentials or send
	 * auth fails, return EAUTH, since we can't do what the user wanted and
	 * it is likely this name and password are really for this type of authentication.
	 */
	if (*name && (ctx->ct_flags & SMBCF_EXPLICITPWD)) {
		void *gssCreds = NULL;
		
		passwd = ctx->ct_setup.ioc_password;
		if (useNTLM) {
			error = smb_session_set_ntlm_name(ctx, name, domain);
			if (error) {
				goto done;
			}
			
			if (!smb_acquire_ntlm_cred(name, domain, passwd, &gssCreds)) {
				error = EAUTH;
				goto done;
			}
		} else {
			char *principal;
			
			if (!smb_acquire_krb5_cred(name, NULL, passwd, &gssCreds)) {
				error = EAUTH;
				goto done;
			}
			
			principal = smb_gss_principal_from_cred(gssCreds);
			if (principal) {
				error = smb_session_set_client(ctx, principal, GSSD_KRB5_PRINCIPAL);
				free(principal);
			} else {
				error = EAUTH;
			}
			
			if (error) {
				smb_release_gss_cred(gssCreds, error);
				goto done;
			}
		}
		error = smb_session_send_auth(ctx);
		smb_release_gss_cred(gssCreds, error);
	} else {
		/* Otherwise try the credentials in the cache */
		error = smb_session_cache(ctx, mech, name, domain);
		if (error == ENOENT) {
			error = EAUTH;
		}
		foundInCache = TRUE;
	}
done:
	/* Will need to reset this if these names ever become binary */
	os_log_debug(OS_LOG_DEFAULT, "%s: Authentication %s%s, user name %s %s '%s' gss client principal %s - %d gss server principal %s - %d %s for %s",
				 __FUNCTION__,
				 (error) ? "FAILED" : "SUCCEED", 
				 (foundInCache) ? " in cache" : "", 
				 ctx->ct_setup.ioc_user,  
				 (useNTLM) ? "domain" : "using", 
				 (useNTLM) ? ctx->ct_setup.ioc_domain : "KERBEROS",
				 (char *)(uintptr_t)ctx->ct_setup.ioc_gss_client_name, 
				 ctx->ct_setup.ioc_gss_client_nt,
				 (char *)(uintptr_t)ctx->ct_setup.ioc_gss_target_name, 
				 ctx->ct_setup.ioc_gss_target_nt,
				 (passwd) ? "" : "obtain from cache",
				 ctx->serverName);
	return (error);
}


/*
 * Guest authentication, always NTLMSSP.
 */
static int AuthenticateWithGuest(struct smb_ctx *ctx)
{
	int foundInCache = FALSE;
	int error = 0;
	void *gssCreds = NULL;
	
	smb_session_reset_security(ctx);
	
	/* We always create a target name if we are doing extended security. */
	error = MakeTarget(ctx, NULL, GSSD_HOSTBASED);
	if (error) {
		goto done;
	}
		
	/* We should search the cache first to see if we already have guest creds */
	error = smb_session_cache(ctx, GSS_NTLM_MECHANISM, ctx->ct_setup.ioc_user, NULL);
	if (! error) {
		foundInCache = TRUE;
		goto done;
	}
	/* We never have a domain name in the guest case */
	error = smb_session_set_ntlm_name(ctx, ctx->ct_setup.ioc_user, "");
	if (error) {
		goto done;
	}
	/* We never have a domain or password  in the guest case */
	if (!smb_acquire_ntlm_cred(ctx->ct_setup.ioc_user, "",  "", &gssCreds)) {
		error = EAUTH;
		goto done;
	}
	error = smb_session_send_auth(ctx);
	smb_release_gss_cred(gssCreds, error);
	
done:
	/* Will need to reset this if these names ever become binary */
	os_log_debug(OS_LOG_DEFAULT, "%s: Authentication %s%s, user name %s gss client principal %s - %d gss server principal %s - %d for %s",
				 __FUNCTION__,
				 (error) ? "FAILED" : "SUCCEED", 
				 (foundInCache) ? " in cache" : "",
				 ctx->ct_setup.ioc_user,  
				 (char *)(uintptr_t)ctx->ct_setup.ioc_gss_client_name, 
				 ctx->ct_setup.ioc_gss_client_nt,
				 (char *)(uintptr_t)ctx->ct_setup.ioc_gss_target_name, 
				 ctx->ct_setup.ioc_gss_target_nt,
				 ctx->serverName);
	return (error);
}

/*
 * Anonymous authentication.
 */
static int AuthenticateWithAnonymous(struct smb_ctx *ctx)
{
	int error;
	
	smb_session_reset_security(ctx);
	ctx->ct_setup.ioc_gss_client_nt = GSSD_ANONYMOUS;
	error = MakeTarget(ctx, NULL, GSSD_HOSTBASED);
	if (error)
		return (error);
	error = smb_session_send_auth(ctx);
	return (error);
}

/*
 * Non Extended security, need to depricated this authentication method. Since 
 * this is the only way to do clear text and share level security removing it
 * will break some NetApp sites.
 */
static int AuthenticateWithNonExtSecurity(struct smb_ctx *ctx)
{
	int error;
	
	smb_session_reset_security(ctx);
	error = smb_session_send_auth(ctx);
	os_log_debug(OS_LOG_DEFAULT, "%s: Authentication %s, user name %s domain %s for %s",
				 __FUNCTION__, (error) ? "FAILED" : "SUCCEED",
				 ctx->ct_setup.ioc_user, ctx->ct_setup.ioc_domain,
				 ctx->serverName);
	return error;
}

static int smb_session_security(struct smb_ctx *ctx, CFDictionaryRef authInfoDict)
{	
	Boolean serverSupportsKerb = serverSupportsKerberos(ctx->mechDict);
	
	ctx->ct_flags &= ~SMBCF_AUTHORIZED;
	if ((ctx->ct_flags & SMBCF_CONNECTED) != SMBCF_CONNECTED) {
		return EPIPE;
	}

	/* Allow anonymous and guest to skip any min auth requirements */
	if ((ctx->ct_setup.ioc_userflags & SMBV_ANONYMOUS_ACCESS) || 
		(ctx->ct_setup.ioc_userflags & SMBV_GUEST_ACCESS)) {
		goto skip_minauth_check;
	}

	if (!serverSupportsKerb && (ctx->prefs.minAuthAllowed == SMB_MINAUTH_KERBEROS)) {
		os_log_error(OS_LOG_DEFAULT, "%s: Server doesn't support Kerberos and its required!", __FUNCTION__);
		return ENETFSNOAUTHMECHSUPP;
	}
	
	/* Server requires clear text password, but we are not doing clear text passwords. */
	if (((ctx->ct_session_flags & SMBV_ENCRYPT_PASSWORD) != SMBV_ENCRYPT_PASSWORD) && 
		(ctx->prefs.minAuthAllowed != SMB_MINAUTH)) {
		os_log_error(OS_LOG_DEFAULT, "%s: Clear text passwords are not allowed!", __FUNCTION__);
		return ENETFSNOAUTHMECHSUPP;
	}
	
    /* 
     * If non extended security, we do NTLMv2 and if that fails, try NTLMv1.
     * For extended security, GSS only allows a min of NTLM v2.
     * 
     * The new default min auth is NTLMv2.
     */
	if (((ctx->ct_session_caps & SMB_CAP_EXT_SECURITY) != SMB_CAP_EXT_SECURITY) &&
		(ctx->prefs.minAuthAllowed >= SMB_MINAUTH_NTLMV2)) {
        ctx->ct_setup.ioc_userflags |= SMBV_NO_NTLMV1;
    }
    
	/* They want to use Kerberos, but the server doesn't support it */
	if ((ctx->ct_setup.ioc_userflags & SMBV_KERBEROS_ACCESS) && !serverSupportsKerb) {
		os_log_error(OS_LOG_DEFAULT, "%s: Server doesn't support Kerberos, syserr = %s",
					 __FUNCTION__, strerror(EAUTH));
		return ENETFSNOAUTHMECHSUPP;
	}

skip_minauth_check:
	
	/*
	 * Not doing extended security no more processing needed just send the
	 * authenication message.
	 */
	if ((ctx->ct_session_caps & SMB_CAP_EXT_SECURITY) != SMB_CAP_EXT_SECURITY) {
		return AuthenticateWithNonExtSecurity(ctx);
	}
	
	/*
	 * They gave us a authentication dictionary then use it.
	 */	
	if (authInfoDict) {
		return AuthenticateWithDictionary(ctx, authInfoDict);
	}
	
	/* They want us to use anonymous */
	if (ctx->ct_setup.ioc_userflags & SMBV_ANONYMOUS_ACCESS) {
		return AuthenticateWithAnonymous(ctx);
	} 
	
	/* They want us to use guest */
	if (ctx->ct_setup.ioc_userflags & SMBV_GUEST_ACCESS) {
		return AuthenticateWithGuest(ctx);
	} 
	
	/* 
	 * If the server support kerberos then we try it first and if that fails
	 * then try NTLM.
	 */
	if (serverSupportsKerb) {
		int error;
		
		ctx->ct_setup.ioc_userflags |= SMBV_KERBEROS_ACCESS;
		error = AuthenticateWithURL(ctx, FALSE);
		if (!error || (ctx->prefs.minAuthAllowed == SMB_MINAUTH_KERBEROS)) {
			if (!error)
			return error;
		}
		/* Only fall through if the min level allows it */
		/* Kerberos failed, clear the flag */
		ctx->ct_setup.ioc_userflags &= ~SMBV_KERBEROS_ACCESS;
	}
	/* Ok try NTLM */
	return AuthenticateWithURL(ctx, TRUE);
}

/*
 * Resolve the name using NetBIOS.
 */
static int 
smb_resolve_netbios_name(struct smb_ctx *ctx, CFMutableArrayRef *outAddressArray, 
						 Boolean loopBackAllowed)
{
	char * netbios_name = NULL;
	int error = 0;
	struct smb_prefs *prefs = &ctx->prefs;
	
	/*
	 * We uppercase and convert the server name given in the URL to Windows Code 
	 * Page. We assume the server name is a a UTF8 name, if not this could fail,
	 * but it only fails on port 139.
	 */
	if (ctx->serverName)
		netbios_name = convert_utf8_to_wincs(ctx->serverName, prefs->WinCodePage, TRUE);
	
	/* Must have a NetBIOS name if we are going to resovle it using NetBIOS */
	if ((netbios_name == NULL) || (strlen(netbios_name) > SMB_MAXNetBIOSNAMELEN)) {
		error = EHOSTUNREACH;
		goto done;
	}
	/*
	 * If we have a "NetBIOSDNSName" then the configuration file contained the 
	 * DNS name or DOT IP Notification address that we should be using to connect
	 * to the server.
	 */
	if (ctx->prefs.NetBIOSDNSName) {
		char NetBIOSDNSName[SMB_MAX_DNS_SRVNAMELEN+1];
		
		CFStringGetCString(ctx->prefs.NetBIOSDNSName, NetBIOSDNSName, 
						   sizeof(NetBIOSDNSName), kCFStringEncodingUTF8);
		error = resolvehost(NetBIOSDNSName, outAddressArray, netbios_name, 
							prefs->tcp_port, loopBackAllowed, prefs->tryBothPorts);
	} else {
		error = nbns_resolvename(&ctx->ct_nb, prefs, netbios_name, NBT_SERVER, 
								 outAddressArray, prefs->tcp_port,  loopBackAllowed, 
								 prefs->tryBothPorts, &ctx->ct_cancel);
	}
	
	if (error) {
		/* 
		 * We have something that looked like a NetBIOS name, but we couldn't
		 * resolve it try DNS.
		 */
		goto done;
	}
	/*
	 * We now have a list of address and we have a NetBIOS name. Use the NetBIOS
	 * name as the server name. This should always work for the tree connect.
	 */
	strlcpy(ctx->ct_ssn.ioc_srvname, netbios_name, sizeof(ctx->ct_ssn.ioc_srvname));	

done:
	/* If we get an ELOOP then we found the address, just happens to be the local address */
	if ((error == 0) || (error == ELOOP))
		ctx->ct_flags |= SMBCF_RESOLVED; /* We are done no more resolving needed */
	/* Done with the name, so free it */
	if (netbios_name)
		free(netbios_name);
	
	return error;
}

/*
 * Resolve the name using Bonjour.
 */
static int 
smb_resolve_bonjour_name(struct smb_ctx *ctx, CFMutableArrayRef *outAddressArray, 
						 Boolean loopBackAllowed)
{
	CFNetServiceRef theService = _CFNetServiceCreateFromURL(NULL, ctx->ct_url);
	CFStringRef serviceNameType;
	CFStringRef displayServiceName;
	CFStreamError debug_error = {(CFStreamErrorDomain)0, 0};
	CFArrayRef retAddresses = NULL;
	CFIndex numAddresses = 0;
	CFIndex ii;
	CFMutableArrayRef addressArray = NULL;
	CFMutableDataRef addressData;
	int error = 0;
	int localAddressFound = FALSE;
	
	/* Not a Bonjour Service Name, need to resolve with some other method */
	if (theService == NULL)
		return EHOSTUNREACH;

	 /* Error or no error we are done no more resolving needed */
	ctx->ct_flags |= SMBCF_RESOLVED;
	/* Bonjour service name lookup, never fallback. */
	ctx->prefs.tryBothPorts = FALSE;
	
	addressArray = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeArrayCallBacks);
	if (!addressArray) {
		error = ENOMEM;
		goto done;
	}

	serviceNameType = CFNetServiceGetType(theService);
	displayServiceName = CFNetServiceGetName(theService);
	
	/* Should we be doing CFNetServiceResolveWithTimeout async? */
	if (serviceNameType && (CFStringCompare(serviceNameType, CFSTR(SMB_BonjourServiceNameType), kCFCompareCaseInsensitive) != kCFCompareEqualTo)) {
		const char *serviceNameTypeCString = CFStringGetCStringPtr(serviceNameType, kCFStringEncodingUTF8);
		
		os_log_error(OS_LOG_DEFAULT, "Wrong service type for smb, should be %s got %s",
					 SMB_BonjourServiceNameType, (serviceNameTypeCString) ? serviceNameTypeCString : "NULL");
	} else if (CFNetServiceResolveWithTimeout(theService, 30, &debug_error) == TRUE) {
		retAddresses = CFNetServiceGetAddressing(theService);
		numAddresses = (retAddresses) ? CFArrayGetCount(retAddresses) : 0;
		os_log_debug(OS_LOG_DEFAULT, "Bonjour lookup found %ld address entries.",
					 numAddresses);
	}
	else 
		os_log_debug(OS_LOG_DEFAULT, "Looking up Bonjour service name timeouted? error %ld:%d",
					 debug_error.domain, (int)debug_error.error);
	
	for (ii=0; ii<numAddresses; ii++) {
		struct sockaddr *sockAddr = (struct sockaddr*) CFDataGetBytePtr ((CFDataRef) CFArrayGetValueAtIndex (retAddresses, ii));
		if (sockAddr == NULL) {
			os_log_error(OS_LOG_DEFAULT, "We have a NULL sock address pointer, shouldn't happed, syserr = %s",
						 strerror(errno));
			continue;
		} else if ((sockAddr->sa_family != AF_INET) && (sockAddr->sa_family != AF_INET6)) {
			os_log_debug(OS_LOG_DEFAULT, "Unknown sa_family = %d sa_len = %d",
						 sockAddr->sa_family, sockAddr->sa_len);
			continue;
		} else {
			struct connectAddress conn;
			in_port_t port;

			memset(&conn, 0, sizeof(conn));
			memcpy(&conn.addr, sockAddr, sockAddr->sa_len);
			conn.so = -1;	/* Default to socket create failed */
			port = (conn.addr.sa_family == AF_INET) ? conn.in4.sin_port : conn.in6.sin6_port;
			/* We don't support port 139 with Bonjour */
			if (port == htons(NBSS_TCP_PORT_139)) {
				os_log_debug(OS_LOG_DEFAULT, "Port 139 is not support with Bonjour skipping sa_family %d",
							 sockAddr->sa_family);
				continue;
			}
			/* Check to see if we have a loopback bonjour name */
			if (isLocalIPAddress(sockAddr, port, loopBackAllowed)) {
				os_log_debug(OS_LOG_DEFAULT, "The address for `%s' is a loopback address, not allowed!",
							 ctx->serverName);
				localAddressFound = TRUE;
				continue;
			}
			
			
			addressData = CFDataCreateMutable(NULL, 0);
			if (addressData) {
				CFDataAppendBytes(addressData, (const UInt8 *)&conn, (CFIndex)sizeof(conn));
				CFArrayAppendValue(addressArray, addressData);
				CFRelease(addressData);
			}
			os_log_debug(OS_LOG_DEFAULT, "Resolve for Bonjour[%ld] found family %d sin_port = %d",
						 ii, sockAddr->sa_family, port);
		}
	}

	if (CFArrayGetCount(addressArray) == 0) {
		/* 
		 * If the only address we found was a loopback address, then return the 
		 * ELOOP (AFP error) otherwise return not reachable. 
		 */
		if (localAddressFound) {
			error = ELOOP;
		} else {
			error = EHOSTUNREACH;
		}
		goto done;
	}
				
	if (displayServiceName) {
		if (ctx->serverNameRef)
			CFRelease(ctx->serverNameRef);
		ctx->serverNameRef = CFStringCreateCopy(kCFAllocatorDefault, displayServiceName);
	}
	
	/*
	 * We now have a list of address and we have a Bonjour name. Use the Bonjour
	 * name as the server name. This should always work for the tree connect.
	 */
	strlcpy(ctx->ct_ssn.ioc_srvname, ctx->serverName, sizeof(ctx->ct_ssn.ioc_srvname));
	
done:
	if (theService)
		CFRelease(theService);
	
	if (error) {
		if (addressArray)
			CFRelease(addressArray);
		addressArray = NULL;
	}
	*outAddressArray = addressArray;
	return error;
}

/*
 * Resolve the name using DNS.
 * By the time we get here, we have already tried resolving it as a Bonjour
 * name, then tried as a NetBios name and its not either of those.
 */
static int smb_resolve_dns_name(struct smb_ctx *ctx, CFMutableArrayRef *outAddressArray, 
								Boolean loopBackAllowed)
{
	int error;
    size_t len;
    char *temp_name = NULL;
    char *scope = NULL;
	
	error = resolvehost(ctx->serverName, outAddressArray, NULL, ctx->prefs.tcp_port, 
						loopBackAllowed, ctx->prefs.tryBothPorts);
	if (error == 0) {
        ctx->ct_flags |= SMBCF_RESOLVED;
        
        /*
         * Note: getaddrinfo() and inet_pton() both will give errors if its
         * an IPv6 address enclosed by brackets. I cant find a way to detect
         * if the address is IPv6 or not if the brackets are present. Thus, the
         * check for '[' at the start and ']' at the end of the string.
         */

        /* Check to see if its IPv6 and if it is IPv6 with brackets */
        len = strnlen(ctx->serverName, 1024);  /* assume hostname < 1024 */
        if ((len > 3) && (ctx->serverName[0] == '[') && (ctx->serverName[len - 1] == ']')) {
            /* Seems to be IPv6 with brackets */
            temp_name = malloc(len);
            
            if (temp_name != NULL) {
                /*
                 * Copy string and skip beginning '[' (&hostname[1]) and
                 * ending ']' (len - 1)
                 */
                strlcpy(temp_name, &ctx->serverName[1], len - 1);
                
                /* 
                 * Strip off the scope if one is found as our own server does
                 * not like the %en0 in the Tree Connect
                 */
                scope = strrchr(temp_name, '%');
                if (scope != NULL) {
                    /* Found a scope, so lop it off */
                    *scope = '\0';
                }
                
                strlcpy(ctx->ct_ssn.ioc_srvname, temp_name, sizeof(ctx->ct_ssn.ioc_srvname));
                
                free(temp_name);
            }
            else {
                error = ENOMEM;
            }
        }
        else {
            strlcpy(ctx->ct_ssn.ioc_srvname, ctx->serverName, sizeof(ctx->ct_ssn.ioc_srvname));
        }
	}

	return error;
}

/*
 * Returns 1 if the string appears to be an IPv6 or IPv4 address, 0 if it isn't,
 * -1 on error.  It checks whether it's an IPv6 or IPv4 address by checking
 * whether inet_pton(AF_INET6/AF_INET4) succeeds on it.
 *
 * For IPv6, the [], scope and port must already be stripped out of hostString
 */
static int
IsIPv4v6Address(const char *hostString)
{
    int retValue = 0;
    struct sockaddr_in sin4;
    struct sockaddr_in6 sin6;

    if (hostString == NULL) {
        return(retValue);
    }

    /* Is it IPv4? */
    if (inet_pton (AF_INET, hostString, &sin4.sin_addr) == 1) {
        os_log_debug(OS_LOG_DEFAULT, "%s: <%s> is IPv4 address", __FUNCTION__, hostString);
        retValue = 1;
    }
    else {
        /* Is it IPv6? */
        if (inet_pton (AF_INET6, hostString, &sin6.sin6_addr) == 1) {
            os_log_debug(OS_LOG_DEFAULT, "%s: <%s> is IPv6 address", __FUNCTION__, hostString);
            retValue = 1;
        }
    }

    return(retValue);
}

/*
 *  We want to attempt to resolve the name using any available method.
 *  1.  Bonjour Lookup: We always check to see if the name is a Bonjour
 *      service name first. If it's a Bonjour name than we either resolve it or
 *      fail the whole connection. If we succeed then we always use the port give
 *      to us by Bonjour.
 *
 *  2.  DNS Lookup: If Bonjour fails to resolve, try to resolve it with DNS.
 *
 *  3.  NetBIOS Lookup: If Bonjour and DNS fail to resolve, then try
 *      using NetBIOS. NetBIOS resolution is really slow and I dont think
 *      anyone uses it anymore, so move it to the last resolution attempt.
 */
static int 
smb_resolve(struct smb_ctx *ctx, CFMutableArrayRef *outAddressArray, 
			Boolean loopBackAllowed)
{
	int error;
	
	ctx->ct_flags &= ~(SMBCF_RESOLVED | SMBCF_MATCH_DNS);
    
	/* We always try Bonjour first and use the port if gave us. */
	error = smb_resolve_bonjour_name(ctx, outAddressArray, loopBackAllowed);
	/* We are done if Bonjour resolved it otherwise try the other methods. */
    if (ctx->ct_flags & SMBCF_RESOLVED) {
        if (ctx->prefs.no_DNS_match == 0) {
#ifdef SMB_DEBUG
			os_log_error(OS_LOG_DEFAULT, "Bonjour check dns match <%s>", ctx->serverName);
#endif
            ctx->ct_flags |= SMBCF_MATCH_DNS;
        }
		goto WeAreDone;
    }

    /*
     * Did they set option to check NetBIOS before DNS?
     * Default is to try NetBIOS after DNS because NetBIOS resolution can be
     * very slow.
     */
    if (ctx->prefs.try_netBIOS_before_DNS == 1) {
        /* Try NetBIOS unless they request us to use some other port */
        if ((ctx->prefs.tcp_port == NBSS_TCP_PORT_139) || ctx->prefs.tryBothPorts)
            error = smb_resolve_netbios_name(ctx, outAddressArray, loopBackAllowed);
        /* We found it with NetBIOS we are done. */
        if (ctx->ct_flags & SMBCF_RESOLVED) {
            goto WeAreDone;
        }
    }

    /* Next try DNS because NetBIOS resolution */
    error = smb_resolve_dns_name(ctx, outAddressArray, loopBackAllowed);
    if (ctx->ct_flags & SMBCF_RESOLVED) {
        if (ctx->prefs.no_DNS_match == 0) {
            /*
             * Only check for DNS matches if NOT IPv4 or IPv6 address
             * Use the name that has been stripped of [] and scope for IPv6 by
             * smb_resolve_dns_name()
             */
            if (IsIPv4v6Address(ctx->ct_ssn.ioc_srvname) == 0) {
#ifdef SMB_DEBUG
                os_log_error(OS_LOG_DEFAULT, "DNS check dns match <%s>", ctx->serverName);
#endif
                ctx->ct_flags |= SMBCF_MATCH_DNS;
            }
        }
        goto WeAreDone;
    }

    /* Default behavior, check NetBIOS after trying DNS */
    if (ctx->prefs.try_netBIOS_before_DNS == 0) {
        /* Last, we try NetBIOS unless they request us to use some other port */
        if ((ctx->prefs.tcp_port == NBSS_TCP_PORT_139) || ctx->prefs.tryBothPorts)
            error = smb_resolve_netbios_name(ctx, outAddressArray, loopBackAllowed);
        /* We found it with NetBIOS we are done. */
        if (ctx->ct_flags & SMBCF_RESOLVED) {
            goto WeAreDone;
        }
    }

WeAreDone:
	if (error) {
		ctx->ct_flags &= ~SMBCF_RESOLVED;
		os_log_debug(OS_LOG_DEFAULT, "Couldn't resolve %s", ctx->serverName);
	}
	return error;
}

/*
 * First we resolve the name, once we have it resolved then we are done.
 */
static int 
smb_connect_one(struct smb_ctx *ctx, int forceNewSession, Boolean loopBackAllowed)
{
	CFMutableArrayRef addressArray = NULL;
	struct connectAddress *conn  = NULL;
	struct sockaddr *laddr = NULL;
	int error = 0;

    /* We are already connected nothing to do here */
    if (ctx->ct_flags & SMBCF_CONNECTED) {
		return 0;
    }
	
	/* We already found an address use that one */
    if ((ctx->ct_flags & SMBCF_RESOLVED) && ctx->ct_saddr) {
		 goto do_negotiate;
    }
	
	error = smb_resolve(ctx, &addressArray, loopBackAllowed);
	if (error) {
		return error;
	}
	/* We have a list of address, if only one address then use it */
	 if (CFArrayGetCount(addressArray) == 1) {
		 CFMutableDataRef addressData = (CFMutableDataRef)CFArrayGetValueAtIndex(addressArray, 0);
         if (addressData) {
			 conn = (struct connectAddress *)((void *)CFDataGetMutableBytePtr(addressData));
         }
         
         if (!conn) {
			error = ENOMEM; /* Should never happen but just in case */
         }
	 } else {
		 /* Search all the address and see if we are already connected */
         if ((!forceNewSession) && findMatchingSession(ctx, addressArray, NULL, FALSE) == 0) {
			 goto done;
         }
         
		 /*
		  * So we have a list of address, try connecting to all of them async. The 
		  * first to come back is the one we will use
		  */
		 error = findReachableAddress(addressArray, &ctx->ct_cancel, &conn);
	 }
	if (error) {
		ctx->ct_flags &= ~SMBCF_RESOLVED;
		goto done;
	}
	
	/* Free the old sockaddr if we have one */
    if (ctx->ct_saddr) {
		free(ctx->ct_saddr);
    }
    
	ctx->ct_saddr = NULL;
		
	/* We found it with DNS, and we are using port 139, we need a NetBIOS socket_addr */
	if ((conn->addr.sa_family == AF_INET) &&
        (conn->in4.sin_port == htons(NBSS_TCP_PORT_139))) {
		char *netbios_name = (char *)NetBIOS_SMBSERVER;
		
		/* We need a NetBIOS name, if we don't find one use *SMBSERVER. */
        if (smb_ctx_getnbname(ctx, &conn->addr) == 0) {
			netbios_name = ctx->ct_ssn.ioc_srvname; /* Found the NetBIOS name */
        }
        
		nb_sockaddr(&conn->addr, netbios_name, NBT_SERVER, &ctx->ct_saddr);
	} else {
		/* We already have the sockaddr we will be using for the connection */
		ctx->ct_saddr = malloc(conn->addr.sa_len);
		/* If smb_negotiate will error out, so deal with it at that time */
        if (ctx->ct_saddr) {
			memcpy(ctx->ct_saddr, &conn->addr, conn->addr.sa_len);
        }
	}

do_negotiate:
	/* We need a local address if connecting with NetBIOS */
    if (ctx->ct_saddr && (ctx->ct_saddr->sa_family == AF_NETBIOS)) {
		nb_sockaddr(NULL, ctx->ct_ssn.ioc_localname, NBT_WKSTA, &laddr);
    }

	error = smb_negotiate(ctx, ctx->ct_saddr, laddr, forceNewSession);
	if (error) {
		goto done;
	}
	
done:
    if (error == 0) {
		ctx->ct_flags |= SMBCF_CONNECTED;
    }
		
    if (laddr) {
		free(laddr);
    }
    
    if (addressArray) {
		CFRelease(addressArray);
    }
    
	return error;
}

/*
 * In the future I would like to move the open directory code to its own file,
 * but since this is going into a software update lets leave it here for now.
 */

#include <OpenDirectory/OpenDirectory.h>
#include <CoreFoundation/CoreFoundation.h>
#include <opendirectory/adtypes.h>
#include <SystemConfiguration/SystemConfiguration.h>

/*
 * Make the call to the AD Plugin to get a list of the domain controllers, if any.
 */
CF_RETURNS_RETAINED
static CFArrayRef
smb_dc_lookup(ODNodeRef nodeRef, CFStringRef lookupName)
{
    CFDataRef name = CFStringCreateExternalRepresentation(kCFAllocatorDefault, lookupName, kCFStringEncodingUTF8, 0);
    CFArrayRef result = NULL;
    CFDataRef response;
	
	if (!name) {
		return NULL;
	}
    
	response = ODNodeCustomCall(nodeRef, eODCustomCallActiveDirectoryDCsForName, name, NULL);
    if (response) {
        CFPropertyListRef plist = CFPropertyListCreateWithData(kCFAllocatorDefault, response, kCFPropertyListImmutable, NULL, NULL);
        if (plist != NULL) {
            if (CFArrayGetTypeID() == CFGetTypeID(plist)) {
                result = CFRetain(plist);
            }
            CFRelease(plist);
        }
        CFRelease(response);
    }
    
	CFRelease(name);
    return result;
}

/*
 * The server name could be an AD Domain Name, in that case we really need to 
 * connect to one of domain controllers. The old code would just use DNS, but 
 * not all AD enviroments are configure to have the AD Domain Name resolve to
 * the domain controllers. So now lets ask the AD plugin for help. If this is 
 * an AD Domain Name that we are bound to then the AD plugin will return a list
 * of domain controllers for that domain. If not bound or if this name is not
 * part of the AD domain then the plugin will return NULL.
 */
CF_RETURNS_RETAINED
CFArrayRef
smb_resolve_domain(CFStringRef serverNameRef)
{
    CFArrayRef result = NULL;
	ODNodeRef nodeRef = NULL;
	SCDynamicStoreRef store = NULL;
	CFDictionaryRef dict = NULL;
	CFStringRef nodename = NULL;	
	
	/* Just to be safe should never happen */
	if (!serverNameRef) {
        os_log_debug(OS_LOG_DEFAULT, "%s: No server name", __FUNCTION__);
		return NULL;
	}

	store = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("smb client"), NULL, NULL);
	if (!store) {
        os_log_debug(OS_LOG_DEFAULT, "%s: SCDynamicStoreCreate failed",
                     __FUNCTION__);
		return NULL;
	}
	
	/* If SCDynamicStoreCopyValue returns null then we are not bound to AD */
	dict = SCDynamicStoreCopyValue(store,
                                   CFSTR("com.apple.opendirectoryd.ActiveDirectory"));
	if (dict != NULL) {
		nodename = CFDictionaryGetValue(dict, CFSTR("NodeName"));
		if (!nodename) {
			os_log_debug(OS_LOG_DEFAULT, "%s: Failed to obtain the AD node?",
                         __FUNCTION__);
		}
	}
    else {
		os_log_debug(OS_LOG_DEFAULT, "%s: We are not bound to AD",
                     __FUNCTION__);
	}
	
	if (nodename) {
		/* Open the the AD Plugin Node */
		nodeRef = ODNodeCreateWithName(kCFAllocatorDefault, kODSessionDefault,
                                       nodename, NULL);
		if (nodeRef) {
			/* attempt to get the list of domain controllers */
			result = smb_dc_lookup(nodeRef, serverNameRef);
            if (!result) {
                os_log_debug(OS_LOG_DEFAULT, "%s: smb_dc_lookup failed",
                             __FUNCTION__);
            }

		}
	}

	if (nodeRef) {
		CFRelease(nodeRef);
	}
    
	if (dict) {
		CFRelease(dict);
	}
    
	if (store) {
		CFRelease(store);
	}
    
	return result;
}

/* 
 * Helper routine that will set the server name field to the domain controller
 * name, but doesn't replace ctx->serverNameRef. The ctx->serverNameRef is the
 * name the user typed in and should never be replaced.
 */
static int
smb_set_server_name_to_dc(struct smb_ctx *ctx, CFStringRef dcNameRef)
{
	CFIndex maxlen;
	
	maxlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(dcNameRef), kCFStringEncodingUTF8) + 1;
	if (ctx->serverName)
		free(ctx->serverName);
	ctx->serverName = malloc(maxlen);
	if (!ctx->serverName) {
		return ENOMEM;
	}
	CFStringGetCString(dcNameRef, ctx->serverName, maxlen, kCFStringEncodingUTF8);
	os_log_debug(OS_LOG_DEFAULT, "%s: Setting serverName to %s", __FUNCTION__, ctx->serverName);
	return 0;
}

static int 
smb_connect(struct smb_ctx *ctx, int forceNewSession, Boolean loopBackAllowed)
{
	int error = EHOSTUNREACH;
	CFArrayRef dcArrayRef = smb_resolve_domain(ctx->serverNameRef);
	CFIndex numItems = (dcArrayRef) ? CFArrayGetCount(dcArrayRef) : 0;
	
	ctx->serverIsDomainController = FALSE;

    /*
	 * Did we get a list of domain controllers, then attempt to connect to one
	 * of them, otherwise just use the server name passed in.
	 */
	if (dcArrayRef && numItems) {
		CFIndex ii;
		char *hold_serverName = ctx->serverName;
		
		os_log_debug(OS_LOG_DEFAULT, "%s: AD return %ld domain controllers.", __FUNCTION__, numItems);
		ctx->serverName = NULL;
		
		for (ii = 0; ii < numItems && error; ii++) {
			CFStringRef dc = CFArrayGetValueAtIndex(dcArrayRef, ii);
			
			if (dc) {
				error = smb_set_server_name_to_dc(ctx, dc);
				if (!error) {
					ctx->ct_flags &= ~(SMBCF_CONNECTED | SMBCF_RESOLVED);
					error = smb_connect_one(ctx, forceNewSession, loopBackAllowed);
					os_log_error(OS_LOG_DEFAULT, "%s: Connecting to domain controller %s %s",
								 __FUNCTION__, ctx->serverName,
								 (error) ? "FAILED" : "SUCCEED");	
				}
			} else {
				os_log_error(OS_LOG_DEFAULT, "%s: We have a NULL domain controller entry?", __FUNCTION__);
				error = EHOSTUNREACH;
			}
		}

		if (error) {
            /* Could not connect to any of the domain controllers */
			if (ctx->serverName) {
				free(ctx->serverName);
			}
            
            /* Fall back to server name that was passed in */
			ctx->serverName = hold_serverName;
            
			ctx->ct_flags &= ~(SMBCF_CONNECTED | SMBCF_RESOLVED);
			error = smb_connect_one(ctx, forceNewSession, loopBackAllowed);
			os_log_debug(OS_LOG_DEFAULT, "%s: Connecting to all the domain controller failed! Connecting to %s %s",
						 __FUNCTION__, ctx->serverName,
						 (error) ? "FAILED" : "SUCCEED");	
		}
        else {
			ctx->serverIsDomainController = TRUE;			
			free(hold_serverName);
		}
	}
    else {
        /* 
         * No list of domain controllers, just try to connect using the name
         * passed in.
         */
		error = smb_connect_one(ctx, forceNewSession, loopBackAllowed);
	}

	if (dcArrayRef) {
		CFRelease(dcArrayRef);
	}

	return error;
}

/*
 * Common code used by both smb_get_server_info and smb_open_session.
 */
static void smb_get_sessioninfo(struct smb_ctx *ctx, CFMutableDictionaryRef mutableDict, const char * func)
{
	if (ctx->ct_session_flags & (SMBV_GUEST_ACCESS | SMBV_SFS_ACCESS)) {
		CFDictionarySetValue (mutableDict, kNetFSMountedByGuestKey, kCFBooleanTrue);
		os_log_debug(OS_LOG_DEFAULT, "%s: Session shared as Guest", func);
	} else {
		CFStringRef userNameRef = NULL;
		
		/* Authenticated mount, set the key */
		CFDictionarySetValue (mutableDict, kNetFSMountedWithAuthenticationInfoKey, kCFBooleanTrue);
		if (ctx->ct_setup.ioc_user[0]) {
			userNameRef = CFStringCreateWithCString (NULL, ctx->ct_setup.ioc_user, kCFStringEncodingUTF8);
			os_log_debug(OS_LOG_DEFAULT, "%s: Session shared as %s", func, ctx->ct_setup.ioc_user);
		}

		if (userNameRef) {
			CFDictionarySetValue (mutableDict, kNetFSMountedByUserKey, userNameRef);
			CFRelease (userNameRef);
		}
	}
	/* if the server is OSX server, get the model string if session has it */
	smb_get_session_properties(ctx);
	if (ctx->model_info) {
		CFStringRef modelInfoRef = NULL;
		modelInfoRef = CFStringCreateWithCString(NULL, ctx->model_info, kCFStringEncodingUTF8);
		if (modelInfoRef) {
			os_log_debug(OS_LOG_DEFAULT, "%s: kNetFSMachineTypeKey model_info = %s",
						 __FUNCTION__, ctx->model_info);
			CFDictionarySetValue(mutableDict, kNetFSMachineTypeKey, modelInfoRef);
			CFRelease(modelInfoRef);
		}
		else {
			os_log_debug(OS_LOG_DEFAULT, "%s: CFStringCreateWithCString() failed for model_info = %s",
						 __FUNCTION__, ctx->model_info);
		}
	}

}

/*
 * smb_get_server_info
 *
 * Every call to this routine will force a new connection to happen. So if this
 * session already has a connection that connection will be broken and a new
 * connection will be start.
 */
int smb_get_server_info(struct smb_ctx *ctx, CFURLRef url, CFDictionaryRef OpenOptions, CFDictionaryRef *ServerParams)
{
	int  error = 0;
	CFMutableDictionaryRef mutableDict = NULL;
	Boolean loopBackAllowed = SMBGetDictBooleanValue(OpenOptions, kNetFSAllowLoopbackKey, FALSE);
	Boolean noUserPrefs = SMBGetDictBooleanValue(OpenOptions, kNetFSNoUserPreferencesKey, FALSE);
    Boolean forceNewSession = SMBGetDictBooleanValue(OpenOptions, kNetFSForceNewSessionKey, FALSE);
    Boolean HiFiRequested = SMBGetDictBooleanValue(OpenOptions, kHighFidelityMountKey, FALSE);

    *ServerParams = NULL;

	/* 
	 * We are already connected, if this is the same URL then just use the previous
	 * connection. If its a different URL force a new lookup and connection.
	 * NOTE: NetAuthAgent will call us twice sometimes with the same URL. Save
	 * the performace hit and just reuse the connection. Also isUrlStringEqual
	 * deals with the case of null URLs
	 */
	if ((ctx->ct_flags & SMBCF_CONNECTED) && (! isUrlStringEqual(ctx->ct_url, url))) {
		ctx->ct_flags &= ~(SMBCF_CONNECTED | SMBCF_RESOLVED);
	}
	if (url) {
		/* Now deal with the URL */
		if (ctx->ct_url)
			CFRelease(ctx->ct_url);
		ctx->ct_url =  CFURLCopyAbsoluteURL(url);
	}
	if (ctx->ct_url) {
		error = ParseSMBURL(ctx);
	} else {
		error = ENOMEM;
	}
	if (error)
		return error;
	
	/* Tell the kernel that we shouldn't touch the home directory, ever on this session */
	if (noUserPrefs) {
		ctx->ct_setup.ioc_userflags &= ~SMBV_HOME_ACCESS_OK;
	}
	
	/* Only read the preference files once */
	if ((ctx->ct_flags & SMBCF_READ_PREFS) != SMBCF_READ_PREFS) {
		char *shareName = (ctx->ct_sh.ioc_share[0]) ? ctx->ct_sh.ioc_share : NULL;
		readPreferences(&ctx->prefs, ctx->serverName, shareName,  noUserPrefs, FALSE);
	}

    if (HiFiRequested) {
#ifdef SMB_DEBUG
        os_log_error(OS_LOG_DEFAULT, "%s: HiFi requested ", __FUNCTION__);
#endif
        ctx->ct_flags |= SMBCF_HIFI_REQUESTED;
    }

	error = smb_connect(ctx, forceNewSession, loopBackAllowed);
    if (error) {
		return error;
    }

	/* Now return what we know about the server */
	mutableDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (mutableDict == NULL) {
		os_log_error(OS_LOG_DEFAULT, "%s: CFDictionaryCreateMutable failed, syserr = %s",
					 __FUNCTION__, strerror(errno));
		return errno;
	}
	/* Handle the case we know about here for sure */
	/* All modern servers support change password, but the client doesn't so for now the answer is no! */
	CFDictionarySetValue(mutableDict, kNetFSSupportsChangePasswordKey, kCFBooleanFalse);
	/* Most servers support guest, but not sure how we can tell if it is turned on yet */
	CFDictionarySetValue(mutableDict, kNetFSSupportsGuestKey, kCFBooleanTrue);
	CFDictionarySetValue(mutableDict, kNetFSGuestOnlyKey, kCFBooleanFalse);
	/* We have a Mech Dictionary add it to the info */
	if (ctx->mechDict) {
		CFDictionarySetValue(mutableDict, kNetFSMechTypesSupportedKey, ctx->mechDict);			
	}
	
	/*
	 * Need to return the server display name. We always have serverNameRef,
	 * unless we ran out of memory.
	 *
	 * %%% In the future we should handle the case of not enough memory when 
	 * creating the serverNameRef. Until then just fallback to the server name that
	 * came from the URL.
	 */
	if (ctx->serverNameRef) {
		CFDictionarySetValue(mutableDict, kNetFSServerDisplayNameKey, ctx->serverNameRef);
	} else if (ctx->serverName != NULL) {
		CFStringRef Server = CFStringCreateWithCString(NULL, ctx->serverName, kCFStringEncodingUTF8);
		if (Server != NULL) { 
			CFDictionarySetValue(mutableDict, kNetFSServerDisplayNameKey, Server);
			CFRelease (Server);
		}
	}
	smb_get_os_lanman(ctx, mutableDict);

	if (ctx->ct_session_shared) 
		smb_get_sessioninfo(ctx, mutableDict, __FUNCTION__);

	*ServerParams = mutableDict;
	return error;
}

int smb_open_session(struct smb_ctx *ctx, CFURLRef url, CFDictionaryRef OpenOptions, CFDictionaryRef *sessionInfo)
{
	int  error = 0;
	Boolean loopBackAllowed = SMBGetDictBooleanValue(OpenOptions, kNetFSAllowLoopbackKey, FALSE);
	Boolean forceNewSession = SMBGetDictBooleanValue(OpenOptions, kNetFSForceNewSessionKey, FALSE);
	Boolean	UseAuthentication = SMBGetDictBooleanValue(OpenOptions, kNetFSUseAuthenticationInfoKey, FALSE);
	Boolean	UseGuest = SMBGetDictBooleanValue(OpenOptions, kNetFSUseGuestKey, FALSE);
	Boolean	UseAnonymous = SMBGetDictBooleanValue(OpenOptions, kNetFSUseAnonymousKey, FALSE);
	Boolean	ChangePassword = SMBGetDictBooleanValue(OpenOptions, kNetFSChangePasswordKey, FALSE);
	Boolean noUserPrefs = SMBGetDictBooleanValue(OpenOptions, kNetFSNoUserPreferencesKey, FALSE);
    Boolean HiFiRequested = SMBGetDictBooleanValue(OpenOptions, kHighFidelityMountKey, FALSE);
	CFDictionaryRef authInfoDict = NULL;

    /* Remove any previously auth request flags */
	ctx->ct_setup.ioc_userflags &= ~(SMBV_KERBEROS_ACCESS | SMBV_ANONYMOUS_ACCESS |
									 SMBV_PRIV_GUEST_ACCESS | SMBV_GUEST_ACCESS);

	/* They are trying to mix security options, not allowed */
	if ((UseAuthentication && UseGuest) || (UseAuthentication && UseAnonymous) || 
		(UseGuest && UseAnonymous)) {
		error = EINVAL;
		goto done;
	}
	/* We currently do not support change password maybe someday? */
	if (ChangePassword) {
		error = ENOTSUP;
		goto done;
	}
	/* Tell the kernel that we shouldn't touch the home directory, ever on this session */
	if (noUserPrefs) {
		ctx->ct_setup.ioc_userflags &= ~SMBV_HOME_ACCESS_OK;
        
        /* 
         * Kerberos is not allowed to access home dir either. autofs and 
         * home dir mounting *should* be setting the noUserPrefs flags. No
         * access to mount flags which would tell us if its automounter, so 
         * have to rely upon this flag to be set correctly.
         */
        krb5_set_home_dir_access(NULL, false);
	}

	/*  If they pass a URL then use it otherwise use the one we already have */
	if (url) {
		if (ctx->ct_url)
			CFRelease(ctx->ct_url);
		ctx->ct_url =  CFURLCopyAbsoluteURL(url);
	}
	/* Remember that parsing the url can set the SMBV_GUEST_ACCESS */
	if (ctx->ct_url) {
		error = ParseSMBURL(ctx);
	} else {
		error = ENOMEM;
		goto done;
	}
		
	/*
	 * Check to see what authentication method they wish for us to use in the
	 * connection process. Remove any other values that may have been set when
	 * we parsed the URL.
	 */
	if (UseAuthentication) {
		authInfoDict = CFDictionaryGetValue(OpenOptions, kNetFSAuthenticationInfoKey);
		
		if (authInfoDict) {
			CFStringRef mechanismRef = CFDictionaryGetValue(authInfoDict, kNAHMechanism);

			/* Parsing the URL could have set the guest flag remove it */
			ctx->ct_setup.ioc_userflags &= ~SMBV_GUEST_ACCESS;
			
			/* Used for debugging in the future want to remove */
			if (mechanismRef) {
				char mechanismStr[256];
				CFStringGetCString(mechanismRef, mechanismStr, sizeof(mechanismStr), kCFStringEncodingUTF8);
				os_log_debug(OS_LOG_DEFAULT, "%s: Connecting to %s Authentication Info kNAHMechanism %s",
							 __FUNCTION__, ctx->serverName,
							 mechanismStr);
			} else {
				os_log_debug(OS_LOG_DEFAULT, "%s: Connecting to %s using authentication Info, but has no kNAHMechanism",
							 __FUNCTION__, ctx->serverName);
			}

			/* Should we look for other Kerberos Mech here, do we need this anymore? */
			if (mechanismRef && (CFStringCompare(mechanismRef,  kGSSAPIMechKerberos, 0) == kCFCompareEqualTo)) {
				ctx->ct_setup.ioc_userflags |= SMBV_KERBEROS_ACCESS;
			} else if (ctx->prefs.minAuthAllowed == SMB_MINAUTH_KERBEROS) {
				/* They don't want to use Kerberos, but the min auth level requires it */
				os_log_error(OS_LOG_DEFAULT, "%s: Kerberos required!", __FUNCTION__);
				error = ENETFSNOAUTHMECHSUPP;
				goto done;
			}
		} else {
			os_log_debug(OS_LOG_DEFAULT, "%s: Connecting to %s using authentication, but has no Authentication Info",
						 __FUNCTION__, ctx->serverName);
		}
	} else if (UseAnonymous) {
		/* 
		 * If the URL contian the username guest then remove it because they 
		 * told us to use anonymous.
		 */
		ctx->ct_setup.ioc_userflags &= ~SMBV_GUEST_ACCESS;
		ctx->ct_setup.ioc_userflags |= SMBV_ANONYMOUS_ACCESS;
		smb_ctx_setuser(ctx, "");
		smb_ctx_setpassword(ctx, "", FALSE);
		smb_ctx_setdomain(ctx, "");
		os_log_debug(OS_LOG_DEFAULT, "%s: Connecting to %s using anonymous",
					 __FUNCTION__, ctx->serverName);
	} else if (UseGuest) {
		ctx->ct_setup.ioc_userflags |= SMBV_GUEST_ACCESS;
		smb_ctx_setuser(ctx, kGuestAccountName);
		smb_ctx_setpassword(ctx, kGuestPassword, FALSE);
		os_log_debug(OS_LOG_DEFAULT, "%s: Connecting to %s using guest",
					 __FUNCTION__, ctx->serverName);
	} else if (ctx->ct_setup.ioc_userflags & SMBV_GUEST_ACCESS) {
		/* Must have been set from the URL, mark it as private */
		ctx->ct_setup.ioc_userflags |= SMBV_PRIV_GUEST_ACCESS;
		smb_ctx_setpassword(ctx, kGuestPassword, TRUE);
		os_log_debug(OS_LOG_DEFAULT, "%s: Connecting to %s using URL guest",
					 __FUNCTION__, ctx->serverName);
	}

	/* 
	 * If we found a matching existing session and forceNewSession is set,
     * then clear the SMBCF_CONNECTED so that we create a new TCP connection to
     * use for the new session
	 */
	if ((ctx->ct_session_shared) && forceNewSession) {
		ctx->ct_flags &= ~SMBCF_CONNECTED;
	}

    if (HiFiRequested) {
#ifdef SMB_DEBUG
        os_log_error(OS_LOG_DEFAULT, "%s: HiFi requested ", __FUNCTION__);
#endif
        ctx->ct_flags |= SMBCF_HIFI_REQUESTED;
    }

	/* We haven't connect yet or we need to start over in either case read the preference again and do the connect */
	if ((ctx->ct_flags & SMBCF_CONNECTED) != SMBCF_CONNECTED) {
		/* Only read the preference files once */
		if ((ctx->ct_flags & SMBCF_READ_PREFS) != SMBCF_READ_PREFS) {
			char *shareName = (ctx->ct_sh.ioc_share[0]) ? ctx->ct_sh.ioc_share : NULL;
			readPreferences(&ctx->prefs, ctx->serverName, shareName, noUserPrefs, FALSE);
		}
		error = smb_connect(ctx, forceNewSession, loopBackAllowed);
		if (error) {
			goto done;
		}
	}
	
	/* If we are not sharing the session we need to authenticate */
	if (!ctx->ct_session_shared) {
		error = smb_session_security(ctx, authInfoDict);
	}
    
	/* If we are not sharing the session we need to authenticate */
	if (!ctx->ct_session_shared) {
		/*
		 * Only if the server supports Multichannel, we need to detect
		 * our available interfaces.
		 */
		if ((ctx->ct_session_smb2_caps & SMB2_GLOBAL_CAP_MULTI_CHANNEL) == SMB2_GLOBAL_CAP_MULTI_CHANNEL) {
			/*
			 * Look for client's interface server and update the NIC
			 * table.
			 */
			struct smbioc_client_interface client_interface_update;

            error = get_client_interfaces(&client_interface_update);

			if (!error) {
                if (smb_ioctl_call(ctx->ct_fd, SMBIOC_UPDATE_CLIENT_INTERFACES, &client_interface_update) == -1) {
                    /* Some internal error happened? */
                    error = errno;
                } else {
                    /* The real error */
                    error = client_interface_update.ioc_errno;
                }
                free(client_interface_update.ioc_info_array);
            }

            /* TODO: think on a better place to put this */
            /* Raise the mc_notifier if needed */
            struct smbioc_notifier_pid ioc_pid;
            if (ioctl(ctx->ct_fd, SMBIOC_GET_NOTIFIER_PID, &ioc_pid) == -1) {
                /* Some internal error happened? */
                error = errno;
            } else {
                pid_t child = -1;
                if (needToSpawnMCNotifier(ioc_pid.pid)) {
                    char **environ = NULL;
                    short psflags = POSIX_SPAWN_CLOEXEC_DEFAULT;
                    posix_spawnattr_t psattr = {0};
                    sigset_t sig_mask = {0};
                    pid_t process_group = 0;
                    uint32_t notifier_on_timeout = 0;

                    /*
                     * <71090647>
                     * Need to set POSIX_SPAWN_CLOEXEC_DEFAULT, otherwise
                     * mc_notifier will inherit any open fd which includes the
                     * current open /dev/nsmb# and that holds a session ref.
                     * If the session ref does not go to 0, then the SMB Log
                     * off will never get sent.
                     */
                    error = posix_spawnattr_init(&psattr);
                    if (error != 0) {
                        os_log_error(OS_LOG_DEFAULT, "%s: posix_spawnattr_init failed (%d)",
                                     __FUNCTION__, error);
                    }
                    
                    /*
                     * <72188692> Reset all signals to defaults since we use
                     * SIGTERM to tell mc_notifiier when to quit
                     */
                    if (error == 0) {
                        sigfillset(&sig_mask);
                        error = posix_spawnattr_setsigmask(&psattr,
                                                           &sig_mask);
                        if (error != 0) {
                            os_log_error(OS_LOG_DEFAULT, "%s: posix_spawnattr_setsigmask failed (%d)",
                                         __FUNCTION__, error);
                        }
                        else {
                            psflags |= POSIX_SPAWN_SETSIGDEF;
                        }
                    }
                    
                    /*
                     * <72188692> Change our process group so that when the
                     * parent process exits, it wont send mc_notifier a SIGTERM
                     * and cause it to exit early. Since the process_group is
                     * set to 0, then the child process group ID will be set to
                     * the same as its pid.
                     */
                    if (error == 0) {
                        error = posix_spawnattr_setpgroup(&psattr,
                                                          process_group);
                        if (error != 0) {
                            os_log_error(OS_LOG_DEFAULT, "%s: posix_spawnattr_setpgroup failed (%d)",
                                         __FUNCTION__, error);
                        }
                        else {
                            psflags |= POSIX_SPAWN_SETPGROUP;
                        }
                    }
                    
                    /* Set the posix_spawn flags */
                    if (error == 0) {
                        error = posix_spawnattr_setflags(&psattr, psflags);
                        if (error != 0) {
                            os_log_error(OS_LOG_DEFAULT, "%s: posix_spawnattr_setflags failed (%d)",
                                         __FUNCTION__, error);
                        }
                    }
 
                    /* Call posix_spawn to launch mc_notifier */
                    if (error == 0) {
                        error = posix_spawn(&child, "/usr/libexec/mc_notifier",
                                            NULL, &psattr, NULL, environ);
                        if (error) {
                            os_log_error(OS_LOG_DEFAULT, "%s: posix_spawn failed (%d)",
                                         __FUNCTION__, error);
                        }
                    }

                    if (error == 0) {
                        while (ioctl(ctx->ct_fd, SMBIOC_GET_NOTIFIER_PID, &ioc_pid) != -1 && ioc_pid.pid != child) {
                            sleep(1);
                            notifier_on_timeout++;
                            if (notifier_on_timeout > 60) {
                                os_log_error(OS_LOG_DEFAULT, "%s: MC notifier timeout (%d)",
                                             __FUNCTION__, error);
                                break;
                            }
                        }
                    } /* if (error == 0) */
                } /* if (needToSpawnMCNotifier) */
            } /* Couldnt find mc_notifier pid */
		} /* Server supports multichannel */
	} /* Not a shared session */
    
	/*
	 * The connection went down for some reason. Attempt to connect again
	 * and retry the authentication.
	 */
	if (DISCONNECT_ERROR(error)) {
		os_log_debug(OS_LOG_DEFAULT, "%s: The connection to %s went down retry authentication",
					 __FUNCTION__, ctx->serverName);

		ctx->ct_flags &= ~SMBCF_CONNECTED;
		error = smb_connect(ctx, TRUE, loopBackAllowed);
		if (!error) {
			error = smb_session_security(ctx, authInfoDict);
		}
		if (DISCONNECT_ERROR(error)) {
			/* Connection failed again, mark that we are not connect */
			ctx->ct_flags &= ~SMBCF_CONNECTED;
		}
	}
	/* 
	 * We failed, so clear out any local security settings. We need to make
	 * sure we do not reuse these values in the next open session. If these
	 * values were set by the URL then will get reset on the next open call.
	 */
	if (error) {
		smb_ctx_setuser(ctx, "");
		smb_ctx_setpassword(ctx, "", FALSE);
		smb_ctx_setdomain(ctx, "");
	}

    if (error == 0) {
        /* Do they want a sessionInfo returned? */
        if (sessionInfo != NULL) {
            /* create and return session info dictionary */
            CFMutableDictionaryRef mutableDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                                           &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            if (mutableDict) {
                smb_get_sessioninfo(ctx, mutableDict, __FUNCTION__);
            }
            else {
                os_log_debug(OS_LOG_DEFAULT, "%s: CFDictionaryCreateMutable() failed",  __FUNCTION__);
            }
            *sessionInfo = mutableDict;
        }

        /*
         * If no error and Force New Session was set, set a context flag so that
         * the mount call will know to Force a New Session.
         */
        if (forceNewSession) {
            ctx->ct_flags |= SMBCF_FORCE_NEW_SESSION;
        }
    }
    
done:
	return error;
}

int smb_mount(struct smb_ctx *ctx, CFStringRef mpoint,
              CFDictionaryRef mOptions, CFDictionaryRef *mInfo, 
			  void (*callout)(void  *, void *), void *args)
{
	CFMutableDictionaryRef mdict = NULL;
	struct UniqueSMBShareID req;
	Boolean ForceNewSession = SMBGetDictBooleanValue(mOptions, kNetFSForceNewSessionKey, FALSE);
    struct smb_mount_args mdata = {0};
	int error = 0;
	char mount_point[MAXPATHLEN];
	struct stat st;
	int mntflags;
	CFNumberRef numRef;
	struct smb_ctx *dfs_ctx = ctx;
    CFStringRef strRef = NULL;

	if (!ForceNewSession) {
		/* 
		 * If the Open Session call had Force New Session set, then the mount 
		 * call should also honor it too
		 */
		if (ctx->ct_flags & SMBCF_FORCE_NEW_SESSION) {
			ForceNewSession = true;
		}
	}
	
	if (ForceNewSession) {
		os_log_debug(OS_LOG_DEFAULT, "%s: ForceNewSession is set", __FUNCTION__);
	}

	/*
	 * Need to be connected before we can mount the volume.
	 */
	if ((ctx->ct_flags & SMBCF_CONNECTED) != SMBCF_CONNECTED) {
		error = ENOTCONN;
		os_log_debug(OS_LOG_DEFAULT, "%s: syserr = %s",  __FUNCTION__, strerror(error));
		goto done;
	}
	/*  Initialize the mount arguments  */
	mdata.version = SMB_IOC_STRUCT_VERSION;
	mdata.altflags = ctx->prefs.altflags; /* Contains flags that were read from preference */
	mdata.path_len = 0;
	mdata.path[0] = 0;
	mdata.volume_name[0] = 0;
	
	/* Get the alternative mount flags from the mount options */
	if (SMBGetDictBooleanValue(mOptions, kNetFSSoftMountKey, FALSE))
		mdata.altflags |= SMBFS_MNT_SOFT;
	if (SMBGetDictBooleanValue(mOptions, kNotifyOffMountKey, FALSE))
		mdata.altflags |= SMBFS_MNT_NOTIFY_OFF;
	/* Only want the value if it exist */
	if (SMBGetDictBooleanValue(mOptions, kStreamstMountKey, FALSE))
		mdata.altflags |= SMBFS_MNT_STREAMS_ON;
	else if (! SMBGetDictBooleanValue(mOptions, kStreamstMountKey, TRUE))
		mdata.altflags &= ~SMBFS_MNT_STREAMS_ON;
	
    if (SMBGetDictBooleanValue(mOptions, kTimeMachineMountKey, FALSE)) {
        mdata.altflags |= SMBFS_MNT_TIME_MACHINE;
    }

    if (SMBGetDictBooleanValue(mOptions, kHighFidelityMountKey, FALSE)) {
        mdata.altflags |= SMBFS_MNT_HIGH_FIDELITY;
    }

    if (SMBGetDictBooleanValue(mOptions, kDataCacheOffMountKey, FALSE)) {
        mdata.altflags |= SMBFS_MNT_DATACACHE_OFF;
    }

    if (SMBGetDictBooleanValue(mOptions, kMDataCacheOffMountKey, FALSE)) {
        mdata.altflags |= SMBFS_MNT_MDATACACHE_OFF;
    }

    /*
	 * Get the mount flags, just in case there are no flags or something is 
	 * wrong we start with them set to zero. 
	 */
	mntflags = 0;
	if (mOptions) {
		numRef = (CFNumberRef)CFDictionaryGetValue (mOptions, kNetFSMountFlagsKey);
		if (numRef) {
			(void)CFNumberGetValue(numRef, kCFNumberSInt32Type, &mntflags);
		}
	}
	
	if (mntflags & MNT_AUTOMOUNTED) {
        /* Kerberos is not allowed to access home dir for automounts */
        krb5_set_home_dir_access(NULL, false);
    }

    /* Is it a snapshot mount? */
    if (mOptions) {
        strRef = (CFStringRef)CFDictionaryGetValue (mOptions, kSnapshotTimeKey);
        if (strRef) {
            CFStringGetCString(strRef, mdata.snapshot_time,
                               sizeof(mdata.snapshot_time), kCFStringEncodingUTF8);
            if (strlen(mdata.snapshot_time) > 0) {
                /* Convert the @GMT token to local time */
                mdata.snapshot_local_time = SMBConvertGMT(mdata.snapshot_time);

                if (mdata.snapshot_local_time != 0) {
                    mdata.altflags |= SMBFS_MNT_SNAPSHOT;

                    /* Read only and Force New Session should already be set */
                    if (!(mntflags & MNT_RDONLY)) {
                        os_log_error(OS_LOG_DEFAULT, "smb_mount: Snapshot mounts should have read only set. Setting read only option");
                        mntflags |= MNT_RDONLY;
                    }

                    if (!ForceNewSession) {
                        os_log_error(OS_LOG_DEFAULT, "smb_mount: Snapshot mounts should have Force New Session set. Setting Force New Session option");
                        ForceNewSession = true;
                    }
               }
                else {
                    os_log_error(OS_LOG_DEFAULT, "smb_mount: SMBConvertGMT failed for <%s>",
                                 mdata.snapshot_time);
                }
            }
        }
    }

    /* Create the dictionary used to return mount information in. */
	mdict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (!mdict) {
		error = ENOMEM;
		os_log_debug(OS_LOG_DEFAULT, "%s: allocation of dictionary failed, syserr = %s",
					 __FUNCTION__, strerror(error));
		goto done;
	}
	
    /*
     * The old code would do a tree connect here, we now do that in checkForDfsReferral
     * since the tree connect can fail because we are going to a domain instead
     * of a server.
     */	
	error = checkForDfsReferral(ctx, &dfs_ctx, NULL);
    if (error) {
        /* Connection went down, make sure we return the correct error */
  		if ((error == ENOTCONN) || (smb_ctx_connstate(ctx) == ENOTCONN)) {
			ctx->ct_flags &= ~SMBCF_CONNECT_STATE;
			error =  EPIPE;
		}
        dfs_ctx = ctx;  /* Clean up code requires this to be set */
        goto WeAreDone;
    }

	if (dfs_ctx == NULL) {
        dfs_ctx = ctx;  /* Clean up code requires this to be set */
        /* 
		 * We have a mount path and we are not doing Dfs so see if we should be
		 * doing submounts. Remember if the submount key is not set then we 
		 * do submounts. They have to tell us not to do submounts.
		 */
		if ((ctx->mountPath) &&
			(!SMBGetDictBooleanValue(mOptions, kNetFSAllowSubMountsKey, TRUE) ||
			 (ctx->prefs.altflags & SMBFS_MNT_SUBMOUNTS_OFF))) {
			CFMutableDictionaryRef urlParmsRef = NULL;
			CFURLRef urlRef = NULL;
			int error2;
			
			/* 
			 * Not a submount, so remove the path from ctx->ct_url.
			 * First, convert URLRef to a dictionary
			 */
			error2 = smb_url_to_dictionary(ctx->ct_url, (CFDictionaryRef *) &urlParmsRef);
			if (error2 || (urlParmsRef == NULL)) {
				os_log_debug(OS_LOG_DEFAULT, "Failed parsing URL, syserr = %s", strerror(error2));
				goto WeAreDone;
			}
			
			/* Remove kNetFSPathKey from the dictionary */
			CFDictionaryRemoveValue(urlParmsRef, kNetFSPathKey);
			
			/* Convert dictionary back into URLRef */
			error2 = smb_dictionary_to_url(urlParmsRef, &urlRef);
			CFRelease(urlParmsRef);
			if (error || (urlRef == NULL)) {
				os_log_debug(OS_LOG_DEFAULT, "Failed creating URL, syserr = %s", strerror(error2));
				goto WeAreDone;
			}
			
			/* Replace the previous URLRef with the new one */
			CFRelease(ctx->ct_url);
			ctx->ct_url = urlRef;
			
			/* Turn off submounts */
			CFRelease(ctx->mountPath);
			ctx->mountPath = NULL;
		}
	}

	/*
	 * If they have MOPT_AUTOMOUNTED options set then ignore the fact that its
	 * already mounted. If the ForceNewSession is set then ignore those also, 
	 * this is the same as AFP. If its is already mounted return EEXIST and the 
	 * mount information.
	 */
	if (!ForceNewSession && ((mntflags & MNT_AUTOMOUNTED) == 0)) {
		int fs_cnt = 0;
		struct statfs *fs = smb_getfsstat(&fs_cnt);	/* Get the list of mounted volumes */
		
		/* 
		 * Use ctx instead of dfs_ctx to match later on when we call
		 * create_unique_id() below to pass into the mountpoint
		 */
#ifdef SMB_DEBUG
		os_log_error(OS_LOG_DEFAULT, "smb_mount: call already_mounted for <%s>", ctx->ct_sh.ioc_share);
#endif
		error = already_mounted(ctx, ctx->ct_sh.ioc_share, fs, fs_cnt, mdict, mntflags);
		
		if (fs)	{
			/* Done with free it */
			free(fs);
		}
		
		/* It already exist return the mount point */
		if (error == EEXIST) /* Only error already_mounted returns */ {
			if (mInfo) {
				*mInfo = mdict;
				mdict = NULL;
			}
			goto WeAreDone;
		}
	}
	
	/* 
	 * We have a mount path make sure it exist and that the last part of the 
	 * path is a directory.
	 */
	if (dfs_ctx->mountPath) {
		char *newpath = CStringCreateWithCFString(dfs_ctx->mountPath);
		
		if (newpath) {
			uint32_t ntError;
			
			error = smb2io_check_directory(dfs_ctx, newpath, 0, &ntError);
			if (error)
				os_log_debug(OS_LOG_DEFAULT, "%s Check path on %s return ntstatus = 0x%x, syserr = %s",
							 __FUNCTION__, newpath, ntError, strerror(error));
			
			/*
			 * NOTE: With SMB we only get the STATUS_NOT_A_DIRECTORY error, 
			 * when the last component is not a directory. We will get a bad
			 * path if there is some other issue with the path. If the last 
			 * component is not a directory then remove it from the path. We just
			 * return any other error to the calling process.
			 */
			if (error && (ntError == STATUS_NOT_A_DIRECTORY)) {
				CFArrayRef userArray = CFStringCreateArrayBySeparatingStrings(NULL, dfs_ctx->mountPath, CFSTR("/"));
				CFRelease(dfs_ctx->mountPath);
				dfs_ctx->mountPath = NULL;
				if (userArray) {
					CFIndex cnt = CFArrayGetCount(userArray);
					
					if (cnt > 1) {
						CFMutableArrayRef userArrayM = CFArrayCreateMutableCopy(NULL, cnt, userArray);
						
						if (userArrayM) {
							CFArrayRemoveValueAtIndex(userArrayM, cnt-1);			
							dfs_ctx->mountPath = CFStringCreateByCombiningStrings(NULL, userArrayM, CFSTR("/"));
							CFRelease(userArrayM);
						}
					}
					CFRelease(userArray);
				}
				error = 0;
			}
			free(newpath);
		} else {
			error = ENOMEM;
		}
		if (error) {
			goto WeAreDone;
		}
	}

	CFStringGetCString(mpoint, mount_point, sizeof(mount_point), kCFStringEncodingUTF8);
	if (stat(mount_point, &st) == -1)
		 error = errno;
	else if (!S_ISDIR(st.st_mode))
		 error = ENOTDIR;
		 
	 if (error) {
		 os_log_debug(OS_LOG_DEFAULT, "%s: bad mount point, syserr = %s",
					  __FUNCTION__, strerror(error));
		 goto WeAreDone;
	 }
	
	/* now create the unique_id, using tcp address + port + uppercase share */
    if (ctx->ct_saddr) {
#ifdef SMB_DEBUG
        os_log_error(OS_LOG_DEFAULT, "smb_mount: create id for <%s> ",
                     ctx->ct_sh.ioc_share);
#endif
        create_unique_id(ctx, ctx->ct_sh.ioc_share,
                         mdata.unique_id, &mdata.unique_id_len);
    }
    else {
		os_log_debug(OS_LOG_DEFAULT, "%s: ioc_saddr is NULL how did that happen?",
                     __FUNCTION__);
    }
    
	mdata.uid = geteuid();
	mdata.gid = getegid();;
	/* 
	 * Really would like a better way of doing this, but until we can get the real file/directory access
	 * use this method. The old code base the access on the mount point, we no longer do it that way. If
	 * the mount option dictionary has file or directory modes use them otherwise always set it to 0700
	 */
	if (mOptions)
		numRef = (CFNumberRef)CFDictionaryGetValue(mOptions, kdirModeKey);
	else 
		numRef = NULL;
	
	if (numRef && (CFNumberGetValue(numRef, kCFNumberSInt16Type, &mdata.dir_mode))) {
		/* We were passed in the modes to use */
		mdata.dir_mode &= (S_IRWXU | S_IRWXG | S_IRWXO);
	}
	else if ((ctx->ct_setup.ioc_userflags & SMBV_GUEST_ACCESS) &&
			 !(mdata.altflags & SMBFS_MNT_TIME_MACHINE)) {
		/* Guest access open it up and NOT a Time Machine mount <28555880> */
		mdata.dir_mode = S_IRWXU | S_IRWXG | S_IRWXO;
	}
	else {
		/* User access limit access to the user that mounted it */
		mdata.dir_mode = S_IRWXU;
	}
	
	if (mOptions)
		numRef = (CFNumberRef)CFDictionaryGetValue(mOptions, kfileModeKey);
	else 
		numRef = NULL;

	/* See if we were passed a file mode */
	if (numRef && (CFNumberGetValue(numRef, kCFNumberSInt16Type, &mdata.file_mode)))
		mdata.file_mode &= (S_IRWXU | S_IRWXG | S_IRWXO); /* We were passed in the modes to use */
	else if ((ctx->ct_setup.ioc_userflags & SMBV_GUEST_ACCESS) || (ctx->ct_session_flags & SMBV_SFS_ACCESS))
		mdata.file_mode = S_IRWXU | S_IRWXG | S_IRWXO;	/* Guest access open it up */
	else 
		mdata.file_mode = S_IRWXU;						/* User access limit access to the user that mounted it */
	
	/* Just make sure they didn't do something stupid */
	if (mdata.dir_mode & S_IRUSR)
		mdata.dir_mode |= S_IXUSR;
	if (mdata.dir_mode & S_IRGRP)
		mdata.dir_mode |= S_IXGRP;
	if (mdata.dir_mode & S_IROTH)
		mdata.dir_mode |= S_IXOTH;
	
	mdata.KernelLogLevel = ctx->prefs.KernelLogLevel;
    mdata.max_resp_timeout = ctx->prefs.max_resp_timeout;
    
    mdata.ip_QoS = ctx->prefs.ip_QoS;

    /* Dir caching values */
    mdata.dir_cache_async_cnt = ctx->prefs.dir_cache_async_cnt;
    mdata.dir_cache_max = ctx->prefs.dir_cache_max;
    mdata.dir_cache_min = ctx->prefs.dir_cache_min;
    mdata.max_dirs_cached = ctx->prefs.max_dirs_cached;
    mdata.max_dir_entries_cached = ctx->prefs.max_dir_entries_cached;

    /* User defined max quantum size */
    mdata.max_read_size = ctx->prefs.max_read_size;
    mdata.max_write_size = ctx->prefs.max_write_size;

    mdata.dev = dfs_ctx->ct_fd;
	
	CreateSMBFromName(ctx, mdata.url_fromname, MAXPATHLEN);

	if (dfs_ctx->mountPath) {
		CFStringGetCString(dfs_ctx->mountPath, mdata.path, MAXPATHLEN, kCFStringEncodingUTF8);
		mdata.path_len = (uint32_t)strlen(mdata.path);	/* Path length does not include the null byte */
	}
	/* The URL had a starting path send it to the kernel */
	if (ctx->mountPath) {
		CFStringRef	volname;
		CFArrayRef pathArray;
		
		/* Get the Volume Name from the path.
		 * 
		 * Remember we already removed any extra slashes in the path. So it will
		 * be in one of the following formats:
		 *		"/"
		 *		"path"
		 *		"path1/path2/path3"
		 *
		 * We can have a share that is only a slash, but we can never have a path
		 * that is only a slash. Just to be safe we do test for that case. A slash
		 * in the path will cause us to have two empty array elements.
		 */
		
		pathArray = CFStringCreateArrayBySeparatingStrings(NULL, ctx->mountPath, CFSTR("/"));
		if (pathArray) {
			CFIndex indexCnt = CFArrayGetCount(pathArray);
			
			/* Make sure we don't have a path with just a slash or only one element */
			if ((indexCnt < 1) ||
				((indexCnt == 2) && (CFStringGetLength((CFStringRef)CFArrayGetValueAtIndex(pathArray, 0)) == 0)))
				volname = ctx->mountPath;
			else
				volname = (CFStringRef)CFArrayGetValueAtIndex(pathArray, indexCnt -1);
		} else
			volname = ctx->mountPath;
		
		CFStringGetCString(volname, mdata.volume_name, MAXPATHLEN, kCFStringEncodingUTF8);
		if (pathArray)
			CFRelease(pathArray);
	} else if (ctx->ct_origshare) /* Just to be safe, should never happen */
		strlcpy(mdata.volume_name, ctx->ct_origshare, sizeof(mdata.volume_name));
	
	/* We have a callback and they aren't calling us from a callback */ 
	if (callout && !ctx->inCallback) {
		ctx->inCallback = TRUE;
		callout(dfs_ctx, args);
		ctx->inCallback = FALSE;
	}
	/* 
	 * Either we found it with Dfs or its a Dfs share, either case tell the 
	 * kernel this is a Dfs mount 
	 */
	if ((dfs_ctx != ctx) || 
		(smb_tree_conn_optional_support_flags(ctx) & SMB_SHARE_IS_IN_DFS)) {
	    mdata.altflags |= SMBFS_MNT_DFS_SHARE;
	}
	os_log_debug(OS_LOG_DEFAULT, "%s: Volume name = %s mntflags = 0x%x altflags = 0x%x",
				 __FUNCTION__, mdata.volume_name,
				 mntflags, mdata.altflags);
	error = mount(SMBFS_VFSNAME, mount_point, mntflags, (void*)&mdata);
	if (error || (mInfo == NULL)) {
		if (error == -1)
			error = errno; /* Make sure we return a real error number */
		goto WeAreDone;
	}
	/* Now  get the mount information. */
	bzero(&req, sizeof(req));
	bcopy(mdata.unique_id, req.unique_id, sizeof(req.unique_id));
	req.unique_id_len = mdata.unique_id_len;
	if (get_share_mount_info(mount_point, mdict, &req) == EEXIST) {
		*mInfo = mdict;
		mdict = NULL;		
	} else {
		os_log_error(OS_LOG_DEFAULT, "%s: Getting mount information failed !", __FUNCTION__);
	}

WeAreDone:
	if (error) {
		char * log_server = (ctx->serverName) ? ctx->serverName : (char *)"";
		char * log_share = (ctx->ct_origshare) ? ctx->ct_origshare : (char *)"";
		os_log_error(OS_LOG_DEFAULT, "%s: mount failed to %s/%s, syserr = %s",
					 __FUNCTION__, log_server, log_share, strerror(error));
	}
	if (mdict)
		 CFRelease(mdict);
	ctx->ct_flags &= ~SMBCF_SHARE_CONN;
	if (dfs_ctx != ctx)
		smb_ctx_done(dfs_ctx);
		
done:
	return error;
}

CFMutableDictionaryRef
CreateAuthDictionary(struct smb_ctx *ctx, uint32_t authFlags,
					 const char * clientPrincipal, uint32_t clientNameType)
{
	uint32_t serverNameType = GSSD_HOSTBASED;
	CFStringRef serverPrincipalRef = TargetNameCreateWithHostName(ctx);
	CFNumberRef serverNameTypeRef;
	CFStringRef clientPrincipalRef;
	CFNumberRef clientNameTypeRef;
	CFMutableDictionaryRef authInfoDict;
	
	serverNameTypeRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, 
									   &serverNameType);
	clientPrincipalRef = CFStringCreateWithCString(kCFAllocatorDefault, 
												   clientPrincipal, 
												   kCFStringEncodingUTF8);
	clientNameTypeRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, 
									   &clientNameType);
	authInfoDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
											 &kCFTypeDictionaryKeyCallBacks, 
											 &kCFTypeDictionaryValueCallBacks);
	
	/* Something went wrong bail out */
	if (!authInfoDict || !clientPrincipalRef || !serverPrincipalRef || 
		!clientNameTypeRef || !serverNameTypeRef) {
		if (authInfoDict) {
			CFRelease(authInfoDict);
		}
		authInfoDict = NULL;	/* Make sure we return null */
		goto done;
	}
	/* Set the mechanism type, currently we only know about two */
	if (authFlags & SMBV_KERBEROS_ACCESS) {
		CFDictionarySetValue(authInfoDict, kNAHMechanism, kGSSAPIMechKerberos);
	} else {
		CFDictionarySetValue(authInfoDict, kNAHMechanism, kGSSAPIMechNTLM);
	}
	/* Add the client principal */ 
	CFDictionarySetValue(authInfoDict, kNAHClientPrincipal, clientPrincipalRef);
	CFDictionarySetValue (authInfoDict, kNAHClientNameTypeGSSD, clientNameTypeRef);
	/* Add the server principal */ 
	CFDictionarySetValue(authInfoDict, kNAHServerPrincipal, serverPrincipalRef);
	CFDictionarySetValue (authInfoDict, kNAHServerNameTypeGSSD, serverNameTypeRef);
done:
	if (clientPrincipalRef) {
		CFRelease(clientPrincipalRef);
	}
	if (serverPrincipalRef) {
		CFRelease(serverPrincipalRef);
	}
	if (clientNameTypeRef) {
		CFRelease(clientNameTypeRef);
	}
	if (serverNameTypeRef) {
		CFRelease(serverNameTypeRef);
	}
	return authInfoDict;
}

/*
 * smb_ctx_clone
 *
 * Clone any security, and local info we can fron the old ctx
 * into the new ctx.
 */
int smb_ctx_clone(struct smb_ctx *new_ctx, struct smb_ctx *old_ctx,
						   CFMutableDictionaryRef openOptions)
{
	CFMutableDictionaryRef authInfoDict;
	const char *clientName, *oldName;
	char *newName;
	size_t	size;
	
	if (openOptions == NULL) {
		return ENOTSUP;
	}
	
	new_ctx->ct_setup = old_ctx->ct_setup;
	
	/* Make a copy of any pointer values in case we are sharing this session. */
	new_ctx->ct_setup.ioc_gss_client_name = USER_ADDR_NULL;
	new_ctx->ct_setup.ioc_gss_client_size = 0;
	new_ctx->ct_setup.ioc_gss_client_nt = GSSD_STRING_NAME;		
	if (old_ctx->ct_setup.ioc_gss_client_name != USER_ADDR_NULL) {
		size = old_ctx->ct_setup.ioc_gss_client_size;
		newName = calloc(size, 1);
		if (newName) {
			oldName = (char *)(uintptr_t)old_ctx->ct_setup.ioc_gss_client_name;
			memcpy(newName,  oldName,  size);
			new_ctx->ct_setup.ioc_gss_client_name = CAST_USER_ADDR_T(newName);
			new_ctx->ct_setup.ioc_gss_client_size = (uint32_t)size;
			new_ctx->ct_setup.ioc_gss_client_nt = old_ctx->ct_setup.ioc_gss_client_nt;
		}
	}

	/* Make a copy of any pointer values in case we are sharing this session. */
	new_ctx->ct_setup.ioc_gss_target_name = USER_ADDR_NULL;
	new_ctx->ct_setup.ioc_gss_target_size = 0;
	new_ctx->ct_setup.ioc_gss_target_nt = GSSD_STRING_NAME;	
	if (old_ctx->ct_setup.ioc_gss_target_name != USER_ADDR_NULL) {
		size = old_ctx->ct_setup.ioc_gss_target_size;
		newName = calloc(size, 1);
		if (newName) {
			oldName = (char *)(uintptr_t)old_ctx->ct_setup.ioc_gss_target_name;
			memcpy(newName,  oldName,  size);
			new_ctx->ct_setup.ioc_gss_target_name = CAST_USER_ADDR_T(newName);
			new_ctx->ct_setup.ioc_gss_target_size = (uint32_t)size;
			new_ctx->ct_setup.ioc_gss_target_nt = old_ctx->ct_setup.ioc_gss_target_nt;
		}
	}
	
	/* 
	 * The previous server was connected using kerberos and this server doesn't 
	 * support Kerberos then nothing we can do try the cache and if that fails 
	 * give up. Some day we should call Kerberous Helper?
	 */
	if ((old_ctx->ct_setup.ioc_userflags & SMBV_KERBEROS_ACCESS) &&
		(!serverSupportsKerberos(new_ctx->mechDict))) {
		goto done;
	}
	clientName = (char *)(uintptr_t)old_ctx->ct_setup.ioc_gss_client_name;
	/* We have no client name then just try the cache and get out */
	if (!clientName) {
		goto done;
	}
		
	authInfoDict = CreateAuthDictionary(new_ctx, old_ctx->ct_session_flags, clientName, 
										old_ctx->ct_setup.ioc_gss_client_nt);
	if (!authInfoDict) {
		goto done;
	}
	
	CFDictionarySetValue(openOptions, kNetFSUseAuthenticationInfoKey, kCFBooleanTrue);
	CFDictionarySetValue(openOptions, kNetFSAuthenticationInfoKey, authInfoDict);
	CFRelease(authInfoDict);
	
done:
	return 0;
}


int findMountPointSession(void *inRef, const char *mntPoint)
{
	struct connectAddress conn;
	CFMutableArrayRef addressArray = NULL;
	CFMutableDataRef addressData = NULL;
	int error;
    struct smbSockAddrPB pb = {0};

	memset(&conn, 0, sizeof(conn));
    if (fsctl(mntPoint, (unsigned int)smbfsGetSessionSockaddrFSCTL2, &pb, 0 ) != 0) {
        return errno;
    }
    memcpy(&conn.storage, &pb.addr, pb.addr.ss_len);

    addressArray = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeArrayCallBacks);
	if (!addressArray) {
		return ENOMEM;
	}
	addressData = CFDataCreateMutable(NULL, 0);
	if (addressData) {
		CFDataAppendBytes(addressData, (const UInt8 *)&conn, (CFIndex)sizeof(conn));
		CFArrayAppendValue(addressArray, addressData);
		CFRelease(addressData);
		error = findMatchingSession(inRef, addressArray, pb.sessionp, FALSE);
        if (error == ENOENT) {
            /*
             * <67014611> Maybe it was mounted with HiFi option? Try searching
             * again with HiFi option set.
             */
            error = findMatchingSession(inRef, addressArray, pb.sessionp, TRUE);
        }
	} else {
		error = ENOMEM;
	}

	CFRelease(addressArray);
	return error;
	
}

/*
 * Create the smb library context and fill in the default values.
 */
void *create_smb_ctx(void)
{
	struct smb_ctx *ctx = NULL;
	
	ctx = malloc(sizeof(struct smb_ctx));
	if (ctx == NULL)
		return NULL;
	
	/* Clear out everything out to start with */
	bzero(ctx, sizeof(*ctx));

	if (pthread_mutex_init(&ctx->ctx_mutex, NULL) == -1) {
		os_log_debug(OS_LOG_DEFAULT, "%s: pthread_mutex_init failed, syserr = %s",
					 __FUNCTION__, strerror(errno));
		free(ctx);
		return NULL;
	}
	
	ctx->ct_fd = -1;
	
	/* We default to allowing them to touch the home directory */
	ctx->ct_setup.ioc_userflags = SMBV_HOME_ACCESS_OK;
	ctx->ct_ssn.ioc_owner = geteuid();
	ctx->ct_ssn.ioc_reconnect_wait_time = SMBM_RECONNECT_WAIT_TIME;
	getDefaultPreferences(&ctx->prefs);
	/*
	 * We need a local NetBIOS name when connecting on port 139 or when doing
	 * in kernel NTLMSSP. Once we remove the NTLMSSP kernel code we should 
	 * relook at this code. See <rdar://problem/7016849>
	 */
	if (ctx->prefs.LocalNetBIOSName) {
		CFStringGetCString(ctx->prefs.LocalNetBIOSName, ctx->ct_ssn.ioc_localname, 
					   sizeof(ctx->ct_ssn.ioc_localname), kCFStringEncodingUTF8);
	} else {
		os_log_debug(OS_LOG_DEFAULT, "%s: Couldn't obtain the Local NetBIOS Name", __FUNCTION__);
	}

	return ctx;
}

/*
 * Create the smb library context and fill in the default values. Use the url
 * string to create a CFURL that should be used.
 */
int create_smb_ctx_with_url(struct smb_ctx **out_ctx, const char *url)
{
	int  error = EINVAL;
	struct smb_ctx *ctx = NULL;
	
	if (!url) {
		error = EINVAL;
		goto failed;		
	}
	/* Create the structure and fill in the default values */
	ctx = create_smb_ctx();
	if (ctx == NULL) {
		error = ENOMEM;
		goto failed;		
	}
	/* Create the CFURL, from the c-style url string */
	ctx->ct_url = CreateSMBURL(url);
	if (ctx->ct_url)
		error = ParseSMBURL(ctx); /* Now verify the URL */ 
	if (error)
		goto failed;		

	*out_ctx = ctx;
	return 0;
	
failed:
	smb_ctx_done(ctx);
	return error;
}

/*
 * We are done with the smb library context remove it and all its pointers.
 */
void smb_ctx_done(void *inRef)
{
	struct smb_ctx *ctx = (struct smb_ctx *)inRef;
	
	if (ctx == NULL)
		return; /* Nothing to do here */

	if (pthread_mutex_trylock(&ctx->ctx_mutex) != 0) {
		os_log_debug(OS_LOG_DEFAULT, "%s: Canceling connection", __FUNCTION__);
		smb_ctx_cancel_connection(ctx);
		pthread_mutex_lock(&ctx->ctx_mutex);
	}
	/* If we have a tree connect, disconnect */
	smb_share_disconnect(ctx);
	releasePreferenceInfo(&ctx->prefs);
	if (ctx->ct_fd != -1)
		close(ctx->ct_fd);
	if (ctx->ct_url)
		CFRelease(ctx->ct_url);
	if (ctx->serverName)
		free(ctx->serverName);
	if (ctx->serverNameRef)
		CFRelease(ctx->serverNameRef);
	if (ctx->ct_saddr)
		free(ctx->ct_saddr);
	if (ctx->ct_origshare)
		free(ctx->ct_origshare);
	if (ctx->mountPath)
		CFRelease(ctx->mountPath);
	if (ctx->ct_setup.ioc_gss_client_name)
		free((void *)((uintptr_t)(ctx->ct_setup.ioc_gss_client_name)));
	if (ctx->ct_setup.ioc_gss_target_name)
		free((void *)((uintptr_t)(ctx->ct_setup.ioc_gss_target_name)));
	if (ctx->mechDict) {
		CFRelease(ctx->mechDict);
		ctx->mechDict = NULL;
	}
	pthread_mutex_unlock(&ctx->ctx_mutex);
	pthread_mutex_destroy(&ctx->ctx_mutex);
	free(ctx);
}

