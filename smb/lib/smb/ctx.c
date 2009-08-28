/*
 * Copyright (c) 2000, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2009 Apple Inc. All rights reserved.
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

/* Needed for Bonjour service name lookup */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <CFNetwork/CFNetServices.h>
#include <CFNetwork/CFNetServicesPriv.h>	/* Required for _CFNetServiceCreateFromURL */

#include <netsmb/smb_lib.h>
#include <netsmb/netbios.h>
#include <netsmb/nb_lib.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_netshareenum.h>
#include <fs/smbfs/smbfs.h>
#include <charsets.h>
#include <parse_url.h>
#include <cflib.h>
#include <com_err.h>

#ifdef SMB_ROSETTA
unsigned long rosetta_ioctl_cmds[SMBIOC_COMMAND_COUNT] = {0};
unsigned long rosetta_error_ioctl_cmds[SMBIOC_COMMAND_COUNT] = {0};
unsigned long rosetta_fsctl_cmds = 0;
#endif // SMB_ROSETTA

#ifdef SMB_DEBUG
void smb_ctx_hexdump(const char *func, const char *s, unsigned char *buf, size_t inlen)
{
    int32_t addr;
    int32_t i;
	int32_t len = (int32_t)inlen;
	
	printf("%s: hexdump: %s %p length %d inlen %ld\n", func, s, buf, len, inlen);
    addr = 0;
    while( addr < len )
    {
        printf("%6.6x - " , addr );
        for( i=0; i<16; i++ )
        {
            if( addr+i < len )
                printf("%2.2x ", buf[addr+i] );
            else
                printf("   " );
        }
        printf(" \"");
        for( i=0; i<16; i++ )
        {
            if( addr+i < len )
            {
                if(( buf[addr+i] > 0x19 ) && ( buf[addr+i] < 0x7e ) )
                    printf("%c", buf[addr+i] );
                else
                    printf(".");
            }
        }
        printf("\" \n");
        addr += 16;
    }
    printf("\" \n");
}
#endif // SMB_DEBUG

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
		}
#ifdef SMB_ROSETTA
		{
			u_int32_t rosetta_cmd = (u_int32_t)(IOCPARM_MASK & cmd) & ~(('n') << 8);
			
			if ((rosetta_cmd < MIN_SMBIOC_COMMAND) || (rosetta_cmd > MAX_SMBIOC_COMMAND)) {
				smb_log_info("unknown smb ioctl call %ld failed error", 0, ASL_LEVEL_ERR, rosetta_cmd, errno);				
			} else {
				rosetta_error_ioctl_cmds[rosetta_cmd - MIN_SMBIOC_COMMAND] = rosetta_cmd;
				smb_log_info("smb ioctl call %ld failed error", 0, ASL_LEVEL_ERR, rosetta_cmd, errno);				
			}
		}
#endif //* SMB_ROSETTA
		return -1;
	} else {
#ifdef SMB_ROSETTA
		{
			u_int32_t rosetta_cmd = (u_int32_t)(IOCPARM_MASK & cmd) & ~(('n') << 8);
			
			if ((rosetta_cmd < MIN_SMBIOC_COMMAND) || (rosetta_cmd > MAX_SMBIOC_COMMAND)) {
				smb_log_info("unknown smb ioctl call %ld", 0, ASL_LEVEL_ERR, rosetta_cmd);				
			} else {
				rosetta_ioctl_cmds[rosetta_cmd - MIN_SMBIOC_COMMAND] = rosetta_cmd;
			}
		}
#endif //* SMB_ROSETTA
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
		smb_log_info("The SMBIOC_GET_OS_LANMAN call failed!", errno, ASL_LEVEL_DEBUG);
	}
	/* Didn't get a Native OS, default to UNIX or Windows */
	if (OSLanman.NativeOS[0] == 0) {
		if (ctx->ct_vc_caps & SMB_CAP_UNIX)
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
	smb_log_info("Native OS = '%s' Native Lanman = '%s'", 0, ASL_LEVEL_DEBUG, OSLanman.NativeOS, OSLanman.NativeLANManager);
}

/* 
 * Create a unique_id, that can be used to find a matching mounted
 * volume, given the server address, port number, share name and path.
 */
static void create_unique_id(struct smb_ctx *ctx, unsigned char *id, int32_t *unique_id_len)
{
	struct sockaddr_in	saddr = GET_IP_ADDRESS_FROM_NB((ctx->ct_saddr));
	int total_len = (int)sizeof(saddr.sin_addr) + (int)sizeof(ctx->ct_port) + 
					(int)strlen(ctx->ct_sh.ioc_share) + MAXPATHLEN;
	
	memset(id, 0, SMB_MAX_UBIQUE_ID);
	if (total_len > SMB_MAX_UBIQUE_ID) {
		smb_log_info("create_unique_id '%s' too long", 0, ASL_LEVEL_ERR, ctx->ct_sh.ioc_share);
		return; /* program error should never happen, but just incase */
	}
	memcpy(id, &saddr.sin_addr, sizeof(saddr.sin_addr));
	id += sizeof(saddr.sin_addr);
	memcpy(id, &ctx->ct_port, sizeof(ctx->ct_port));
	id += sizeof(ctx->ct_port);
	memcpy(id, ctx->ct_sh.ioc_share, strlen(ctx->ct_sh.ioc_share));
	id += strlen(ctx->ct_sh.ioc_share);
	/* We have a path make it part of the unique id */
	if (ctx->mountPath)
		CFStringGetCString(ctx->mountPath, (char *)id, MAXPATHLEN, kCFStringEncodingUTF8);
	id += MAXPATHLEN;
	*unique_id_len = total_len;
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
#ifdef SMB_ROSETTA
	rosetta_fsctl_cmds = (IOCPARM_MASK & smbfsUniqueShareIDFSCTL) & ~(('z') << 8);
	errno = 0;
#endif //* SMB_ROSETTA
	if ((fsctl(mntonname, (unsigned int)smbfsUniqueShareIDFSCTL, req, 0 ) == 0) && (req->error == EEXIST)) {
		CFStringRef tmpString = NULL;
		
		tmpString = CFStringCreateWithCString (NULL, mntonname, kCFStringEncodingUTF8);
		if (tmpString) {
			CFDictionarySetValue (mdict, kNetFSMountPathKey, tmpString);
			CFRelease (tmpString);			
		}
		if ((req->connection_type == kConnectedByGuest) || (strcasecmp(req->user, kGuestAccountName) == 0))
			CFDictionarySetValue (mdict, kNetFSMountedByGuestKey, kCFBooleanTrue);
		else if (req->user[0] != 0) {
			tmpString = CFStringCreateWithCString (NULL, req->user, kCFStringEncodingUTF8);
			if (tmpString) {
				CFDictionarySetValue (mdict, kNetFSMountedByUserKey, tmpString);			
				CFRelease (tmpString);
			}
			/* We have a user name, but it's a Kerberos client principal name */
			if (req->connection_type == kConnectedByKerberos)
		    		CFDictionarySetValue (mdict, kNetFSMountedByKerberosKey, kCFBooleanTrue);	    
		} else 
			CFDictionarySetValue (mdict, kNetFSMountedByKerberosKey, kCFBooleanTrue);

		return EEXIST;
	}
#ifdef SMB_ROSETTA
	if (errno != ENOENT)
		rosetta_fsctl_cmds = errno;
#endif //* SMB_ROSETTA
	return 0;
}

static int already_mounted(struct smb_ctx *ctx, struct statfs *fs, int fs_cnt, CFMutableDictionaryRef mdict)
{
	struct UniqueSMBShareID req;
	int				ii;
	
	if ((fs == NULL) || (ctx->ct_saddr == NULL))
		return 0;
	bzero(&req, sizeof(req));
	/* now create the unique_id, using tcp address + port + uppercase share */
	create_unique_id(ctx, req.unique_id, &req.unique_id_len);
	for (ii = 0; ii < fs_cnt; ii++, fs++) {
		if (fs->f_owner != ctx->ct_ssn.ioc_owner)
			continue;
		if (strcmp(fs->f_fstypename, SMBFS_VFSNAME) != 0)
			continue;
		if (fs->f_flags & (MNT_DONTBROWSE | MNT_AUTOMOUNTED))
			continue;
		/* Now call the file system to see if this is the one we are looking for */
		if (get_share_mount_info(fs->f_mntonname, mdict, &req) == EEXIST)
			return EEXIST;
	}
	return 0;
}

/*
 * Given a dictionary see if the key has a boolean value to return.
 * If no dictionary or no value return the passed in default value
 * otherwise return the value
 */
static int SMBGetDictBooleanValue(CFDictionaryRef Dict, const void * KeyValue, int DefaultValue)
{
	CFBooleanRef booleanRef = NULL;
	
	if (Dict)
		booleanRef = (CFBooleanRef)CFDictionaryGetValue(Dict, KeyValue);
	if (booleanRef == NULL)
		return DefaultValue;

	return CFBooleanGetValue(booleanRef);
}

/* 
 * Create the SPN name using the server name we used to connect with.
 */
static void smb_get_spn_using_servername(struct smb_ctx *ctx, char *spn, size_t maxlen)
{
	/* We need to add "cifs/ instance part" */
	strlcpy(spn, "cifs/", maxlen);
	/* Now the host name without a realm */
	if ((ctx->ct_port != NBSS_TCP_PORT_139) && (ctx->serverName))
		strlcat(spn, ctx->serverName, maxlen);
	else {
		struct sockaddr_in	saddr = GET_IP_ADDRESS_FROM_NB((ctx->ct_saddr));
		/* 
		 * At this point all we really have is the ip address. The name could be 
		 * anything so we can't use that for the server address. So lets just get 
		 * the IP address in presentation form and leave it at that.
		 */
		strlcat(spn, inet_ntoa(saddr.sin_addr), maxlen);				
	}	
}

void smb_ctx_get_user_mount_info(const char *mntonname, CFMutableDictionaryRef mdict)
{
	struct UniqueSMBShareID req;
	
	bzero(&req, sizeof(req));
	req.flags = SMBFS_GET_ACCESS_INFO;
	if (get_share_mount_info(mntonname, mdict, &req) != EEXIST) {
		smb_log_info("Failed to get user access for mount %s", 0, ASL_LEVEL_ERR, mntonname);
	}		
}

/*
 * Copy the username in and make sure its not to long.
 */
int smb_ctx_setuser(struct smb_ctx *ctx, const char *name)
{
	if (strlen(name) >= SMB_MAXUSERNAMELEN) {
		smb_log_info("user name '%s' too long", 0, ASL_LEVEL_ERR, name);
		return ENAMETOOLONG;
	}
	strlcpy(ctx->ct_setup.ioc_user, name, SMB_MAXUSERNAMELEN);
	/* Used in NTLMSSP code for NTLMv2 */
	str_upper(ctx->ct_setup.ioc_uppercase_user, ctx->ct_setup.ioc_user);
	/* We need to tell the kernel if we are trying to do guest access */
	if (strcasecmp(ctx->ct_setup.ioc_user, kGuestAccountName) == 0)
		ctx->ct_ssn.ioc_opt |= SMBV_GUEST_ACCESS;
	else
		ctx->ct_ssn.ioc_opt &= ~SMBV_GUEST_ACCESS;

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
		smb_log_info("domain/workgroup name '%s' too long", 0, ASL_LEVEL_ERR, name);
		return ENAMETOOLONG;
	}
	strlcpy(ctx->ct_setup.ioc_domain,  name, SMB_MAXNetBIOSNAMELEN+1);
	return 0;
}

int smb_ctx_setpassword(struct smb_ctx *ctx, const char *passwd)
{
	if (passwd == NULL)
		return EINVAL;
	if (strlen(passwd) >= SMB_MAXPASSWORDLEN) {
		smb_log_info("password too long", 0, ASL_LEVEL_ERR);
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
	ctx->ct_flags |= SMBCF_EXPLICITPWD;
	return 0;
}

/*
 * Copy the share in and make sure its not to long.
 */
int smb_ctx_setshare(struct smb_ctx *ctx, const char *share, int stype)
{
	if (strlen(share) >= SMB_MAXSHARENAMELEN) {
		smb_log_info("share name '%s' too long", 0, ASL_LEVEL_ERR, share);
		return ENAMETOOLONG;
	}
	if (ctx->ct_origshare)
		free(ctx->ct_origshare);
	if ((ctx->ct_origshare = strdup(share)) == NULL)
		return ENOMEM;
	str_upper(ctx->ct_sh.ioc_share, share); 
	ctx->ct_sh.ioc_stype = stype;
	return 0;
}

/*
 * Set the DNS name or Dot IP Notation that should be used for this NetBIOS name. The
 * configuration file contian the DNS name to use when connecting. 
 */
static int set_netbios_dns_name(struct smb_ctx *ctx, const char *addr)
{
	if (addr == NULL || addr[0] == 0)
		return EINVAL;
	if (ctx->netbios_dns_name)
		free(ctx->netbios_dns_name);
	if ((ctx->netbios_dns_name = strdup(addr)) == NULL)
		return ENOMEM;
	return 0;
}

/*
 * level values:
 * 0 - default
 * 1 - server
 * 2 - server:user
 * 3 - server:user:share
 */
static int smb_ctx_readrcsection(struct rcfile *smb_rc, struct smb_ctx *ctx, const char *sname, int level)
{
	char *p;
	int error;
	
	if (level == 0) {
		rc_getstringptr(smb_rc, sname, "doscharset", &p);
		if (p)
			setcharset(p);
		rc_getstringptr(smb_rc, sname, "netbiosname", &p);
		if (p) {
			strlcpy(ctx->ct_ssn.ioc_localname, p, sizeof(ctx->ct_ssn.ioc_localname));
			str_upper(ctx->ct_ssn.ioc_localname, ctx->ct_ssn.ioc_localname);
			smb_log_info("Using NetBIOS Name  %s", 0, ASL_LEVEL_DEBUG, ctx->ct_ssn.ioc_localname);
		}
	}
	if (level <= 1) {
		int aflags;
		rc_getstringptr(smb_rc, sname, "port445", &p);
		/* 
		 * See if the configuration file wants us to use a specific port. Now if the
		 * URL has a port in it then ignore the configuration file. 
		 */
		if (p && (ctx->ct_port_behavior != USE_THIS_PORT_ONLY)) {
			if (strcmp(p, "netbios_only") == 0) {
				ctx->ct_port_behavior = USE_THIS_PORT_ONLY;
				ctx->ct_port = NBSS_TCP_PORT_139;
			}
			else if (strcmp(p, "no_netbios") == 0) {
				ctx->ct_port_behavior = USE_THIS_PORT_ONLY;
				ctx->ct_port = SMB_TCP_PORT_445;
			}
		}
		rc_getint(smb_rc, sname, "debug_level", &ctx->debug_level);
		
		/* Only get the value if it exist */
		if (rc_getbool(smb_rc, sname, "notify_off", &aflags) == 0)
		{			
			if (aflags)
				ctx->altflags |= SMBFS_MNT_NOTIFY_OFF;
			else
				ctx->altflags &= ~SMBFS_MNT_NOTIFY_OFF;			
		}
		
		/* Only get the value if it exist */
		if (rc_getbool(smb_rc, sname, "streams", &aflags) == 0)
		{
			if (aflags)
				ctx->altflags |= SMBFS_MNT_STREAMS_ON;
			else
				ctx->altflags &= ~SMBFS_MNT_STREAMS_ON;			
		}

		/* Only get the value if it exist */
		if ( rc_getbool(smb_rc, sname, "soft", &aflags) == 0) {
			if (aflags)
				ctx->altflags |= SMBFS_MNT_SOFT;
			else
				ctx->altflags &= ~SMBFS_MNT_SOFT;
		}

		/* 
		 * See Radar 6650825
		 *
		 * We are not putting this in the man pages, because I am not sure we want to keep
		 * this as a configuration option. If we have no reported issue with Radar 6650825
		 * then I would like to remove this in the future. 
		 *  
		 * So now if the user doesn't supply a domain we will use the one supplied by
		 * the server. This is only done in the NTLMSSP case. Not required in any other
		 * case. This option is being added so users can turn off the default behavior.
		 *
		 */
		if (rc_getbool(smb_rc, sname, "use_server_domain", &aflags) == 0)
		{
			if (aflags)
				ctx->ct_ssn.ioc_opt |= SMBV_SERVER_DOMAIN;
			else
				ctx->ct_ssn.ioc_opt &= ~SMBV_SERVER_DOMAIN;			
		}
		
		rc_getstringptr(smb_rc, sname, "minauth", &p);
		if (p) {
			/*
			 * "minauth" was set in this section; override
			 * the current minimum authentication setting.
			 */
			ctx->ct_ssn.ioc_opt &= ~SMBV_MINAUTH;
			if (strcmp(p, "kerberos") == 0) {
				/*
				 * Don't fall back to NTLMv2, NTLMv1, or
				 * a clear text password.
				 */
				ctx->ct_ssn.ioc_opt |= SMBV_MINAUTH_KERBEROS;
			} else if (strcmp(p, "ntlmv2") == 0) {
				/*
				 * Don't fall back to NTLMv1 or a clear
				 * text password.
				 */
				ctx->ct_ssn.ioc_opt |= SMBV_MINAUTH_NTLMV2;
			} else if (strcmp(p, "ntlm") == 0) {
				/*
				 * Don't send the LM response over the wire.
				 */
				ctx->ct_ssn.ioc_opt |= SMBV_MINAUTH_NTLM;
			} else if (strcmp(p, "lm") == 0) {
				/*
				 * Fail if the server doesn't do encrypted
				 * passwords.
				 */
				ctx->ct_ssn.ioc_opt |= SMBV_MINAUTH_LM;
			} else if (strcmp(p, "ntlmv2_off") == 0) {
				/*
				 * Allow anything, but never send ntlmv2.
				 */
				ctx->ct_ssn.ioc_opt |= SMBV_NTLMV2_OFF;
				
			} else if (strcmp(p, "none") == 0) {
				/*
				 * Anything goes.
				 * (The following statement should be
				 * optimized away.)
				 */
				ctx->ct_ssn.ioc_opt &= ~SMBV_MINAUTH;
			} else {
				/*
				 * Unknown minimum authentication level.
				 */
				smb_log_info("invalid minimum authentication level \"%s\" specified in the section %s", 
								EINVAL, ASL_LEVEL_ERR, p, sname);
				return EINVAL;
			}
		}
	}
	if (level == 1) {
		rc_getstringptr(smb_rc, sname, "addr", &p);
		if (p) {
			error = set_netbios_dns_name(ctx, p);
			if (error) {
				smb_log_info("invalid address specified in the section %s", ASL_LEVEL_ERR, error, sname);
				return error;
			}
		}
	}
	/* We no longer allow you to set this in Leopard, we we should remove this code in the future */
	if (level >= 2) {
		rc_getstringptr(smb_rc, sname, "password", &p);
		if (p)
			error = smb_ctx_setpassword(ctx, p);
	}
	
	rc_getstringptr(smb_rc, sname, "domain", &p);
	if (p) {
		str_upper(p,p);
		/* This is also user config, so mark it as such */
		error = smb_ctx_setdomain(ctx, p);
		if (error)
			smb_log_info("domain specification in the section '%s' ignored", 0, ASL_LEVEL_DEBUG, sname);
	}
	return 0;
}

/*
 * read rc file as follows:
 * 1. read [default] section
 * 2. override with [server] section
 * 3. override with [server:user] section
 * 4. override with [server:user:share] section
 * Since absence of rcfile is not fatal, silently ignore this fact.
 * smb_rc file should be closed by caller.
 */
static void smb_ctx_readrc(struct smb_ctx *ctx, int NoUserPreferences)
{
	struct rcfile *smb_rc = smb_open_rcfile(NoUserPreferences);
	char sname[SMB_MAX_DNS_SRVNAMELEN + SMB_MAXUSERNAMELEN + SMB_MAXSHARENAMELEN + 4];

	ctx->ct_flags |= SMBCF_READ_PREFS;	/* Save the fact that we tried to read the preference files */

	if (smb_rc == NULL)
		goto done;
	
	/*
	 * The /var/db/smb.conf uses the global section header and we use 
	 * default. They really mean the same thing. We should call the
	 * smb_ctx_readrcsection routine here, but that cause an extra search
	 * that we would like to avoid. So in the rc_addsect routine we check
	 * to see if the section header is "global" if so we change it to 
	 * "default". Not the best way to handle this and we may want to change
	 * it in the future, but it does improve the lookup performance.
	 *
	 * smb_ctx_readrcsection(ctx, "global", 0);
	 */
	/*
	 * default parameters
	 */
	smb_ctx_readrcsection(smb_rc, ctx, "default", 0);
	nb_ctx_readrcsection(smb_rc, &ctx->ct_nb, "default", 0);
	
	/*
	 * If we don't have a server name, we can't read any of the
	 * [server...] sections.
	 */
	if (! ctx->serverName)
		goto done;
	
	/*
	 * SERVER parameters.
	 *
	 * We use the server name passed in from the URL to do this lookup.
	 */
	smb_ctx_readrcsection(smb_rc, ctx, ctx->serverName, 1);
	nb_ctx_readrcsection(smb_rc, &ctx->ct_nb, ctx->serverName, 1);
	
	/*
	 * If we don't have a user name, we can't read any of the
	 * [server:user...] sections.
	 */
	if (ctx->ct_setup.ioc_user[0] == 0)
		goto done;
	
	/*
	 * SERVER:USER parameters
	 */
	snprintf(sname, sizeof(sname), "%s:%s", ctx->serverName, ctx->ct_setup.ioc_user);
	smb_ctx_readrcsection(smb_rc, ctx, sname, 2);
	
	/*
	 * If we don't have a share name, we can't read any of the
	 * [server:user:share] sections.
	 */
	if (ctx->ct_sh.ioc_share[0] != 0) {
		/*
		 * SERVER:USER:SHARE parameters
		 */
		snprintf(sname, sizeof(sname), "%s:%s:%s", ctx->serverName, ctx->ct_setup.ioc_user, ctx->ct_sh.ioc_share);
		smb_ctx_readrcsection(smb_rc, ctx, sname, 3);
	}
	
done:
	/* Done with it close the preference file */
	if (smb_rc)
		rc_close(smb_rc);
	smb_rc = NULL;
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
	error = nbns_getnodestatus(sap, &ctx->ct_nb, nbt_server, NULL);
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
	smb_log_info("%d failures to open smb device", errno, ASL_LEVEL_ERR, i+1);
	return ENOENT;
}

/*
 * Return the hflags2 word for an smb_ctx.
 * If we get an error we return zero flags, this should be ok, because the next
 * call will fail.
 */
u_int16_t smb_ctx_flags2(struct smb_ctx *ctx)
{
	u_int16_t flags2;
	
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_GET_VC_FLAGS2, &flags2) == -1) {
		smb_log_info("can't get flags2 for a session", errno, ASL_LEVEL_DEBUG);
		return 0;	/* Shouldn't happen, but if it does pretend no flags set */
	}
	return flags2;
}
/*
 * Cancel any outstanding connection
 */
void smb_ctx_cancel_connection(struct smb_ctx *ctx)
{
	u_int16_t dummy = 0;
	
	if ((ctx->ct_fd != -1) && (smb_ioctl_call(ctx->ct_fd, SMBIOC_CANCEL_SESSION, &dummy) == -1))
		smb_log_info("can't cancel the connection ", errno, ASL_LEVEL_DEBUG);
}

/*
 * Return the connection state of the session. Currently only returns ENOTCONN
 * or EISCONN. May want to expand this in the future.
 */
u_int16_t smb_ctx_connstate(struct smb_ctx *ctx)
{
	u_int16_t connstate = 0;
	
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_SESSSTATE, &connstate) == -1) {
		smb_log_info("can't get connection state for the session", errno, ASL_LEVEL_DEBUG);
		return ENOTCONN;
	}	
	return connstate;
}

/*
 * This routine actually does the whole connection. So if the ctx has a connection
 * this routine will break it and start the whole connection process over.
 */
static int smb_negotiate(struct smb_ctx *ctx)
{
	struct smbioc_negotiate	rq;
	int	error = 0;

	error = smb_ctx_gethandle(ctx);
	if (error)
		return (error);
	
	bzero(&rq, sizeof(rq));
	rq.ioc_version = SMB_IOC_STRUCT_VERSION;
	rq.ioc_extra_flags |= (ctx->ct_port_behavior & TRY_BOTH_PORTS);
	rq.ioc_saddr_len = ctx->ct_saddr_len;
	rq.ioc_laddr_len = ctx->ct_laddr_len;
	rq.ioc_saddr = ctx->ct_saddr;
	rq.ioc_laddr = ctx->ct_laddr;
	bcopy(&ctx->ct_ssn, &rq.ioc_ssn, sizeof(struct smbioc_ossn));
	/* ct_setup.ioc_user and rq.ioc_user must be the same size */
	bcopy(&ctx->ct_setup.ioc_user, &rq.ioc_user, sizeof(rq.ioc_user));
		
	/* Call the kernel to do make the negotiate call */
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_NEGOTIATE, &rq) == -1) {
		error = errno;
		smb_log_info("%s: negotiate ioctl failed:\n", error, ASL_LEVEL_DEBUG, __FUNCTION__);
		goto out;
	}

	/* Get the server's capablilities */
	ctx->ct_vc_caps = rq.ioc_ret_caps;
	ctx->ct_vc_shared = (rq.ioc_extra_flags & SMB_SHARING_VC);
	/* Get the virtual circuit flags */
	ctx->ct_vc_flags = rq.ioc_ret_vc_flags;
	/* If we have no username and the kernel does then use the name in the kernel */
	if ((ctx->ct_setup.ioc_user[0] == 0) && rq.ioc_user[0]) {
		strlcpy(ctx->ct_setup.ioc_user, rq.ioc_user, sizeof(ctx->ct_setup.ioc_user));
		smb_log_info("%s: ctx->ct_setup.ioc_user = %s\n", 0, ASL_LEVEL_DEBUG, 
					 __FUNCTION__, ctx->ct_setup.ioc_user);		
	}
	
	if (ctx->ct_vc_flags & SMBV_MECHTYPE_KRB5) {
		size_t max_hint_len = sizeof(rq.ioc_ssn.ioc_kspn_hint);

		/* We are sharing this connection get the Kerberos client principal name */
		if (ctx->ct_vc_shared && (rq.ioc_ssn.ioc_kuser[0]))
			strlcpy(ctx->ct_ssn.ioc_kuser, rq.ioc_ssn.ioc_kuser, sizeof(ctx->ct_ssn.ioc_kuser));
		/* 
		 * When Windows shipped, there were no other SPNEGO implementations to test against, and so Windows 
		 * really didn't match SPNEGO RFC 2478 100%. ÊEventually, Larry, Paul "Mr. CIFS" Leach, & company at 
		 * Microsoft made an effort to clean this mess up, and revisit the standard so that everyone could play 
		 * well together. The end result is RFC 4178, which supersedes 2478.
		 *
		 * As such, in early versions of Windows SPNEGO, there were some "extra" fields added to the negTokenInit 
		 * message which are being deprecated in Windows 2008 Server, and eventually service packs for older 
		 * platforms. The most significant of these fields is the principal name - there is really no place in 
		 * either standard which allows the return of a principal in negTokenInit messages. This is being corrected 
		 * in Windows 2008 server by continuing to add the field, but instead of a "real" principal, it now contains 
		 * "not_defined_in_RFC4178 at please_ignore".
		 *
		 * From a security standpoint, allowing the server to specify its service principal is a "bad idea" - So we 
		 * need to handle this case. If the SPN is "not_defined_in_RFC4178 at please_ignore" then we will replace it
		 * with the host name. In the furture we may want to check for an empty SPN also.
		 *
		 * Make sure we didn't get an empty SPN.
		 */
		if (((strncasecmp ((char *)rq.ioc_ssn.ioc_kspn_hint, "cifs/",  max_hint_len)) == 0) ||
			((strncasecmp ((char *)rq.ioc_ssn.ioc_kspn_hint, WIN2008_SPN_PLEASE_IGNORE_REALM,  max_hint_len)) == 0)) {
			smb_get_spn_using_servername(ctx, (char *)rq.ioc_ssn.ioc_kspn_hint, max_hint_len);
		}
		if (rq.ioc_ssn.ioc_kspn_hint[0]) {
			bcopy(rq.ioc_ssn.ioc_kspn_hint, ctx->ct_ssn.ioc_kspn_hint, max_hint_len);
			smb_log_info("%s: Kerberos spn hint = %s", 0, ASL_LEVEL_DEBUG, __FUNCTION__, ctx->ct_ssn.ioc_kspn_hint);
		}
	}
	
out:
	if (error) {
		/* 
		 * If we have an EINTR error then the user canceled the connection. Never log that as an
		 * error. We only log an error if the is the last connection attempt or we had a time out
		 * error.
		 */
		if (error == EINTR)
			error = ECANCELED;
		else if ((ctx->ct_port_behavior == USE_THIS_PORT_ONLY) || (error == ETIMEDOUT))
			smb_log_info("%s: negotiate phase failed %s:\n", error, ASL_LEVEL_DEBUG, __FUNCTION__, 
						 (ctx->serverName) ? ctx->serverName : "");
		close(ctx->ct_fd);
		ctx->ct_fd = -1;
	}
	return (error);
}

/*
 * Do a tree disconnect with the last tree we connected on.
 */
static int smb_share_disconnect(struct smb_ctx *ctx)
{
	int error = 0;
	
	if ((ctx->ct_fd < 0) || ((ctx->ct_flags & SMBCF_SHARE_CONN) != SMBCF_SHARE_CONN))
		return 0;	/* Nothing to do here */
	
	ctx->ct_sh.ioc_version = SMB_IOC_STRUCT_VERSION;
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_TDIS, &ctx->ct_sh) == -1) {
		error = errno;
		smb_log_info("%s: tree disconnect failed:\n", error, ASL_LEVEL_DEBUG, __FUNCTION__);
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
	
	ctx->ct_flags &= ~SMBCF_SHARE_CONN;
	if ((ctx->ct_flags & SMBCF_AUTHORIZED) == 0)
		return EAUTH;

	ctx->ct_sh.ioc_version = SMB_IOC_STRUCT_VERSION;
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_TCON, &ctx->ct_sh) == -1)
		error = errno;
	if (error == 0)
		ctx->ct_flags |= SMBCF_SHARE_CONN;
	
	return (error);
}

/* 
 * Update the vc_flags, calling routine should make sure we have a connection.
 */
static void smb_get_vc_flags(struct smb_ctx *ctx)
{
	u_int32_t	vc_flags = ctx->ct_vc_flags;

	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_GET_VC_FLAGS, &vc_flags) == -1)
		smb_log_info("%s: Getting the vc flags falied:\n", -1, ASL_LEVEL_ERR, __FUNCTION__);
	else
		ctx->ct_vc_flags = vc_flags;
}

int smb_session_security(struct smb_ctx *ctx, char *clientpn, char *servicepn)
{
	int error = 0;
	
	ctx->ct_flags &= ~SMBCF_AUTHORIZED;
	if ((ctx->ct_flags & SMBCF_CONNECTED) != SMBCF_CONNECTED) {
		return EPIPE;
	}

	/* They don't want to use Kerberos, but the min auth level requires it */
	if (((ctx->ct_ssn.ioc_opt & SMBV_KERBEROS_ACCESS) != SMBV_KERBEROS_ACCESS) && 
		(ctx->ct_ssn.ioc_opt & SMBV_MINAUTH_KERBEROS)) {
		smb_log_info("%s: Kerberos required!", 0, ASL_LEVEL_ERR, __FUNCTION__);		
		return EAUTH;
	}
	
	/* Server requires clear text password, but we are not doing clear text passwords. */
	if (((ctx->ct_vc_flags & SMBV_ENCRYPT_PASSWORD) != SMBV_ENCRYPT_PASSWORD) && 
		(ctx->ct_ssn.ioc_opt & SMBV_MINAUTH)) {
		smb_log_info("%s: Clear text passwords are not allowed!", 0, ASL_LEVEL_ERR, __FUNCTION__);		
		return EAUTH;
	}
	
	ctx->ct_setup.ioc_version = SMB_IOC_STRUCT_VERSION;
	ctx->ct_setup.ioc_vcflags = ctx->ct_ssn.ioc_opt;
	ctx->ct_setup.ioc_kclientpn[0] = 0;
	ctx->ct_setup.ioc_kservicepn[0] = 0;
	
	/* Used by Kerberos, should be NULL in all other cases */
	if (clientpn) {
		strlcpy(ctx->ct_setup.ioc_kclientpn, clientpn, SMB_MAX_KERB_PN+1);
		smb_log_info("Client principal name '%s'", 0, ASL_LEVEL_DEBUG, ctx->ct_setup.ioc_kservicepn);
	}
	

	if (servicepn) {
		strlcpy(ctx->ct_setup.ioc_kservicepn, servicepn, SMB_MAX_KERB_PN+1);
		smb_log_info("Service principal name '%s'", 0, ASL_LEVEL_DEBUG, ctx->ct_setup.ioc_kservicepn);
	} else if (ctx->ct_ssn.ioc_opt & SMBV_KERBEROS_ACCESS) {
		/* They didn't give us a SPN, but they want to use Kerberos use the server name */
		smb_get_spn_using_servername(ctx, ctx->ct_setup.ioc_kservicepn, sizeof(ctx->ct_setup.ioc_kservicepn));
		smb_log_info("Created service principal name '%s'", 0, ASL_LEVEL_DEBUG, ctx->ct_setup.ioc_kservicepn);
	}
		
	if (smb_ioctl_call(ctx->ct_fd, SMBIOC_SSNSETUP, &ctx->ct_setup) == -1)
		error = errno;
	
	if (error == 0) {
		ctx->ct_flags |= SMBCF_AUTHORIZED;
		smb_get_vc_flags(ctx);	/* Update the the vc flags in case they have changed */	
		/* Save off the client name, if we are using Kerberos */
		if (clientpn) {
			char *realm;
			
			realm = strchr(clientpn, KERBEROS_REALM_DELIMITER);
			if (realm)	/* We really only what the client name, skip the realm stuff */
				*realm = 0;
			strlcpy(ctx->ct_ssn.ioc_kuser, clientpn, SMB_MAXUSERNAMELEN + 1);
			if (realm)	/* Now put it back */
				*realm = KERBEROS_REALM_DELIMITER;
		}
	} else
		ctx->ct_ssn.ioc_opt &= ~(SMBV_GUEST_ACCESS | SMBV_KERBEROS_ACCESS | SMBV_ANONYMOUS_ACCESS);

	return (error);
}

/*
 * We should use Radar 3916980 and 3165159 to clean up this code. We only need ct_laddr
 * when connecting on port 139 and the way we pass it down is just stupid. We can't fix
 * this yet, because the kernel will error out if ct_laddr is null. 
 */
static void set_local_nb_sockaddr(struct smb_ctx *ctx) 
{
	struct sockaddr_nb *salocal = NULL;
	struct nb_name nn;
	int error = 0;
	
	strlcpy((char *)nn.nn_name, ctx->ct_ssn.ioc_localname, sizeof(nn.nn_name));
	nn.nn_type = NBT_WKSTA;
	nn.nn_scope = (u_char *)(ctx->ct_nb.nb_scope);
	error = nb_sockaddr(NULL, &nn, &salocal);
	/* We only need this for port 139 and the kernel will return an error if null */ 
	if (error) 
		smb_log_info("can't create local address", error, ASL_LEVEL_DEBUG);		
	if (ctx->ct_laddr)
		free(ctx->ct_laddr);		
	ctx->ct_laddr = (struct sockaddr*)salocal;
	ctx->ct_laddr_len = salocal->snb_len;
	
}

/*
 * We should use Radar 3916980 and 3165159 to clean up this code. We only need ct_laddr
 * when connecting on port 139 and the way we pass it down is just stupid. We can't fix
 * this yet, because the kernel will error out if ct_laddr is null. 
 */
static void set_server_nb_sockaddr(struct smb_ctx *ctx, const char *netbiosname, struct sockaddr *sap)
{
	struct sockaddr_nb *saserver = NULL;
	struct nb_name nn;
	int error = 0;

	/* See if they are using a scope, more code that is not the norm, but we still support */
	nn.nn_scope = (u_char *)(ctx->ct_nb.nb_scope);
	nn.nn_type = NBT_SERVER;
	strlcpy((char *)(nn.nn_name), netbiosname, sizeof(nn.nn_name));
	/*
	 * We no longer keep the server name in uppercase. When connecting on port 139 we need to 
	 * uppercases netbios name that is used for the connection. So after we copy it in and before
	 * we encode it we should make sure it gets uppercase.
	 */
	str_upper((char *)(nn.nn_name), (char *)(nn.nn_name));
	error = nb_sockaddr(sap, &nn, &saserver);						
	if (error) 
		smb_log_info("can't create remote address", error, ASL_LEVEL_DEBUG);		
	if (ctx->ct_saddr)
		free(ctx->ct_saddr);		
	ctx->ct_saddr = (struct sockaddr*)saserver;
	ctx->ct_saddr_len = saserver->snb_len;
}

/*
 * Resolve the name using NetBIOS. This should always return a IPv4 address.
 */
static int smb_resolve_netbios_name(struct smb_ctx *ctx, u_int16_t port)
{
	struct sockaddr *sap = NULL;
	char * netbios_name = NULL;
	int error = 0;
	int allow_local_conn = (ctx->ct_flags & SMBCF_MOUNTSMBFS);
	
	/*
	 * We convert the server name given in the URL to Windows Code Page? Seems
	 * broken but not sure how else to solve this issue.
	 */
	if (ctx->serverName)
		netbios_name = convert_utf8_to_wincs(ctx->serverName);
	
	/* Must have a NetBIOS name if we are going to resovle it using NetBIOS */
	if ((netbios_name == NULL) || (strlen(netbios_name) > SMB_MAXNetBIOSNAMELEN)) {
		error = EHOSTUNREACH;
		goto WeAreDone;
	}
	
	/*
	 * If we have a "netbios_dns_name" then the configuration file contained the DNS name or 
	 * DOT IP Notification address (192.0.0.1) that we should be using to connect to the server.
	 *
	 * When we add IPv6 address this will need to be changed. We will need a way to require the
	 * lookup to only return an IPv4 address (See Radar 3165159).
	 */
	if (ctx->netbios_dns_name)
		error = nb_resolvehost_in(ctx->netbios_dns_name, &sap, port, allow_local_conn);
	else {
		/* Get the address need to resolve the NetBIOS name could be wins or a broadcast address */
		error = nb_ctx_resolve(&ctx->ct_nb);
		if (error == 0)
			error = nbns_resolvename(netbios_name, &ctx->ct_nb, &sap, allow_local_conn, port);
	}
	
	if (error) {
		/* We have something that looked like a NetBIOS name, but we couldn't find it */
		smb_log_info("Couldn't resolve NetBIOS name %s", error, ASL_LEVEL_DEBUG, netbios_name);		
	} else {
		/* The name they gave us is NetBIOS name so use it in tree connects, should we upper case it? */
		strlcpy(ctx->ct_ssn.ioc_srvname, netbios_name, sizeof(ctx->ct_ssn.ioc_srvname));
		set_server_nb_sockaddr(ctx, netbios_name, sap);
	}

WeAreDone:
	/* If we get an ELOOP then we found the address, just happens to be the local address */
	if ((error == 0) || (error == ELOOP))
		ctx->ct_flags |= SMBCF_RESOLVED; /* We are done no more resolving needed */
	/* Done with the name, so free it */
	if (netbios_name)
		free(netbios_name);
	if (sap)
		free(sap);
	return error;
}

/*
 * Resolve the name using Bonjour. This currently always returns a IPv4 address
 * With Radar 3165159 "SMB should support IPv6" this routine will need some work.
 * We always use the port supplied by Bonjour. 
 */
static int smb_resolve_bonjour_name(struct smb_ctx *ctx)
{
	CFNetServiceRef theService = _CFNetServiceCreateFromURL(NULL, ctx->ct_url);
	CFStringRef serviceNameType;
	CFStringRef displayServiceName;
	CFStreamError debug_error = {(CFStreamErrorDomain)0, 0};
	CFArrayRef retAddresses = NULL;
	CFIndex numAddresses = 0;
	struct sockaddr *sockAddr = NULL, *sap = NULL;
	CFIndex ii;
	
	/* Not a Bonjour Service Name, need to resolve with some other method */
	if (theService == NULL)
		return EHOSTUNREACH;
		
	ctx->ct_flags |= SMBCF_RESOLVED; /* Error or no error we are done no more resolving needed */
	/* Bonjour service name lookup, never fallback. */
	ctx->ct_port_behavior = USE_THIS_PORT_ONLY;
	serviceNameType = CFNetServiceGetType(theService);
	displayServiceName = CFNetServiceGetName(theService);
	
	if (serviceNameType && (CFStringCompare(serviceNameType, CFSTR(SMB_BonjourServiceNameType), kCFCompareCaseInsensitive) != kCFCompareEqualTo)) {
		const char *serviceNameTypeCString = CFStringGetCStringPtr(serviceNameType, kCFStringEncodingUTF8);
		
		smb_log_info("Wrong service type for smb, should be %s got %s", 0, ASL_LEVEL_ERR, 
					 SMB_BonjourServiceNameType, (serviceNameTypeCString) ? serviceNameTypeCString : "NULL");
	} else if (CFNetServiceResolveWithTimeout(theService, 30, &debug_error) == TRUE) {
		retAddresses = CFNetServiceGetAddressing(theService);
		numAddresses = (retAddresses) ? CFArrayGetCount(retAddresses) : 0;
		smb_log_info("Bonjour lookup found %d address entries.", 0, ASL_LEVEL_DEBUG, numAddresses);			
	}
	else 
		smb_log_info("Looking up Bonjour service name timeouted? error %d:%d", 0, ASL_LEVEL_DEBUG, debug_error.domain, debug_error.error);
	
	if (retAddresses)	/* Shouldn't be needed, but just to be safe */
		for (ii=0; ii<numAddresses; ii++) {
			sockAddr = (struct sockaddr*) CFDataGetBytePtr ((CFDataRef) CFArrayGetValueAtIndex (retAddresses, ii));
			if (sockAddr == NULL) {
				smb_log_info("We have a NULL sock address pointer, shouldn't happed!", -1, ASL_LEVEL_ERR);
				break;
			} else if (sockAddr->sa_family == AF_INET6) {
				smb_log_info("The AF_INET6 family is not supported yet.", 0, ASL_LEVEL_DEBUG);
				continue;			
			} else if (sockAddr->sa_family == AF_INET) {
				smb_log_info("Resolve for Bonjour found IPv4 sin_port = %d sap sin_addr = 0x%x", 0, ASL_LEVEL_DEBUG, 
							 htons(((struct sockaddr_in *)sockAddr)->sin_port), htonl(((struct sockaddr_in *)sockAddr)->sin_addr.s_addr));	
				sap = malloc(sizeof(struct sockaddr_in));
				if (sap) {
					ctx->ct_port = htons(((struct sockaddr_in *)sockAddr)->sin_port);
					memcpy (sap, sockAddr, sizeof (struct sockaddr_in));
				}
				break;
			} else
				smb_log_info("Unknown sa_family = %d sa_len = %d", 0, ASL_LEVEL_DEBUG, sockAddr->sa_family, sockAddr->sa_len);
		}
	/*
	 * Once we add IPv6 support (See Radar 3165159) we will need to
	 * add some code here to find out which address to use. 
	 */
	if (sap && displayServiceName) {
		if (ctx->serverNameRef)
			CFRelease(ctx->serverNameRef);
		ctx->serverNameRef = CFStringCreateCopy(kCFAllocatorDefault, displayServiceName);
	}
	
	if (theService)
		CFRelease(theService);
	
	 /* Nothing founded, we are done here get out */
	if (sap == NULL)
		return EADDRNOTAVAIL;
	/* 
	 * Found an address, we need to find a name we can use in the tree
	 * connect server field. Since we found it using Bonjour, always set
	 * it to the IPv4 address in presentation form (xxx.xxx.xxx.xxx).
	 *
	 * See comments in smb_smb_treeconnect for more information on why this
	 * is done.
	 */
	if (sap->sa_family == AF_INET) {
		struct sockaddr_in *sinp = (struct sockaddr_in *)sap;
		strlcpy(ctx->ct_ssn.ioc_srvname, inet_ntoa(sinp->sin_addr), sizeof(ctx->ct_ssn.ioc_srvname));
	} else	/* IPv6 nothing else we can do at this point */
		strlcpy(ctx->ct_ssn.ioc_srvname, ctx->serverName, sizeof(ctx->ct_ssn.ioc_srvname));

	set_server_nb_sockaddr(ctx, NetBIOS_SMBSERVER, sap);
	if (sap)
		free(sap);
		
	return 0;
}

/*
 * Resolve the name using DNS. This currently always returns a IPv4 address
 * With Radar 3165159 "SMB should support IPv6" this routine will need some work.
 * We always use the port passed in. If the name is resolved in this case then
 * we should be using port 445. In some cases we will fallback to port 139, but in
 * those cases we should lookup the NetBIOS before we attempt the port 139 connection.
*/
static int smb_resolve_dns_name(struct smb_ctx *ctx, u_int16_t port)
{
	int error = 0;
	int allow_local_conn = (ctx->ct_flags & SMBCF_MOUNTSMBFS);
	struct sockaddr *sap = NULL;
	
	error = nb_resolvehost_in(ctx->serverName, &sap, port, allow_local_conn);
	if (error == 0) {
		if (sap && (sap->sa_family == AF_INET6)) {
			/* No port 139 support with IPv6 address, should this be handled in nb_resolvehost_in?  */
			if (ctx->ct_port == NBSS_TCP_PORT_139)
				return EADDRNOTAVAIL;
			ctx->ct_port_behavior = USE_THIS_PORT_ONLY;
		}
		ctx->ct_flags |= SMBCF_RESOLVED;
		/* 
		 * Found an address, we need to find a name we can use in the tree
		 * connect server field. First ask the server for its NetBIOS name, 
		 * if no response then use the IPv4 address in presentation form. The
		 * smb_ctx_getnbname routine fills in the ioc_srvname name if its
		 * finds one.
		 *
		 * See comments in smb_smb_treeconnect for more information on why this
		 * is done.
		 */
		if (smb_ctx_getnbname(ctx, sap) == 0)
			set_server_nb_sockaddr(ctx, ctx->ct_ssn.ioc_srvname, sap);
		else {	
			if (sap->sa_family == AF_INET) {
				struct sockaddr_in *sinp = (struct sockaddr_in *)sap;
				strlcpy(ctx->ct_ssn.ioc_srvname, inet_ntoa(sinp->sin_addr), sizeof(ctx->ct_ssn.ioc_srvname));
			} else	/* IPv6 nothing else we can do at this point */
				strlcpy(ctx->ct_ssn.ioc_srvname, ctx->serverName, sizeof(ctx->ct_ssn.ioc_srvname));
			
			set_server_nb_sockaddr(ctx, NetBIOS_SMBSERVER, sap);
		}
	}
	if (sap)
		free(sap);
	return error;
}

/* 
 * We want to attempt to resolve the name using any available method. 
 *	1.	Bonjour Lookup: We always check to see if the name is a Bonjour 
 *		service name first. If it's a Bonjour name than we either resolve it or 
 *		fail the whole connection. If we succeed then we always use the port give
 *		to us by Bonjour.
 *
 *	2.	NetBIOS Lookup: If not a Bonjour name then by default attempt to resolve it
 *		using NetBIOS. If we find it go ahead and set the NetBIOS name. Skip this lookup
 *		if we are not trying both ports and the port the have set is not 139.
 *
 *	3.	DNS Lookup: If all else fails attempt to look it up with DNS. In this case the 
 *		default is to try port 445 and if that fails attempt port 139. We will need to
 *		find and set the NetBIOS name if the connection is on port 139.
 *
 * NOTE:	All of this will need to be re-looked at when we implement 
 *			Radar 3165159 "SMB should support IPv6".
 *	
 */
static int smb_resolve(struct smb_ctx *ctx)
{
	int error;
	
	/* We already resolved it nothing else to do here */
	if (ctx->ct_flags & SMBCF_RESOLVED)
		return 0;
	
	/* We always try Bonjour first and use the port if gave us. */
	error = smb_resolve_bonjour_name(ctx);
	/* We are done if Bonjour resolved it otherwise try the other methods. */
	if (ctx->ct_flags & SMBCF_RESOLVED)
		goto WeAreDone;
	
	/* We default to trying NetBIOS next unless they request us to use some other port */
	if ((ctx->ct_port == NBSS_TCP_PORT_139) || (ctx->ct_port_behavior == TRY_BOTH_PORTS))
		error = smb_resolve_netbios_name(ctx, ctx->ct_port);
	
	/* We found it with NetBIOS we are done. */
	if (ctx->ct_flags & SMBCF_RESOLVED)
		goto WeAreDone;

	/* Last resort try DNS */
	error = smb_resolve_dns_name(ctx, ctx->ct_port);
	
WeAreDone:
	if (error == 0) {
		/*
		 * We should use Radar 3916980 and 3165159 to clean up this code. We only need ct_laddr
		 * when connecting on port 139 and the way we pass it down is just stupid. We can't fix
		 * this yet, because the kernel will error out if ct_laddr is null. We always have the
		 * ct_laddr information at this point so just fill it in.
		 */
		set_local_nb_sockaddr(ctx);
	}
	return error;
}
/*
 * First we reolsve the name, once we have it resolved then we are done.
 */
int smb_connect(struct smb_ctx *ctx)
{
	int error;
	
	ctx->ct_flags &= ~SMBCF_CONNECTED;
	error = smb_resolve(ctx);
	if (error)
		return error;
	error = smb_negotiate(ctx);
	if (error == ECANCELED)
		return error;
	
	/* 
	 * So here is the plan. If we are trying both ports and we ETIMEDOUT from the first port then don't try 
	 * the second port. This will cause problems with firewalls. If the first port is blocked then we will 
	 * never connect to the second port, without a configuration change. This really shouldn't be that bad 
	 * because we default to try port 445 first and port 139 second. So if they for some strange reason have 
	 * 445 blocked and not port 139 they will need to change their configuration file to force us to use 139 first.
	 *
	 * NOTE: We should never have TRY_BOTH_PORTS set if this is an IPV6 address. 
	 *		 Radar 3165159 "SMB should support IPv6" need to confirm and test for this fact.
	 */
	if (error && (error != ETIMEDOUT) && (ctx->ct_port_behavior == TRY_BOTH_PORTS)) {
		struct sockaddr_in * sap = &GET_IP_ADDRESS_FROM_NB((ctx->ct_saddr));
		
		sap->sin_port = htons(NBSS_TCP_PORT_139);
		ctx->ct_port = NBSS_TCP_PORT_139;
		ctx->ct_port_behavior = USE_THIS_PORT_ONLY;
		error = smb_negotiate(ctx);		
	}
	if (error == 0)
		ctx->ct_flags |= SMBCF_CONNECTED;
	return error;
}

/*
 * Common code used by both  smb_get_server_info and smb_open_session. 
 */
static void smb_get_sessioninfo(struct smb_ctx *ctx, CFMutableDictionaryRef mutableDict, const char * func)
{
	if (ctx->ct_vc_flags & SMBV_GUEST_ACCESS) {
		CFDictionarySetValue (mutableDict, kNetFSMountedByGuestKey, kCFBooleanTrue);
		smb_log_info("%s: Session shared as Guest", 0, ASL_LEVEL_DEBUG, func);	
	} else {
		CFStringRef userNameRef = NULL;
		
		if (ctx->ct_setup.ioc_user[0]) {
			userNameRef = CFStringCreateWithCString (NULL, ctx->ct_setup.ioc_user, kCFStringEncodingUTF8);
			smb_log_info("%s: User session shared as %s", 0, ASL_LEVEL_DEBUG, func, ctx->ct_setup.ioc_user);						
		}
		else if ((ctx->ct_vc_flags & SMBV_KERBEROS_ACCESS) && (ctx->ct_ssn.ioc_kuser[0])) {
			userNameRef = CFStringCreateWithCString (NULL, ctx->ct_ssn.ioc_kuser, kCFStringEncodingUTF8);
			smb_log_info("%s: Kerberos session shared as %s", 0, ASL_LEVEL_DEBUG, func, ctx->ct_ssn.ioc_kuser);						
		}
		if (userNameRef != NULL) {
			CFDictionarySetValue (mutableDict, kNetFSMountedByUserKey, userNameRef);
			CFRelease (userNameRef);
		}
		
		if (ctx->ct_vc_flags & SMBV_KERBEROS_ACCESS) {
			CFDictionarySetValue (mutableDict, kNetFSMountedByKerberosKey, kCFBooleanTrue);
			smb_log_info("%s: Session shared as Kerberos", 0, ASL_LEVEL_DEBUG, func);		
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
	int	 NoUserPreferences;
	CFMutableDictionaryRef mutableDict = NULL;

	*ServerParams = NULL;

	/* Now deal with the URL */
	if (ctx->ct_url)
		CFRelease(ctx->ct_url);
	ctx->ct_url =  CFURLCopyAbsoluteURL(url);
	error = ParseSMBURL(ctx, SMB_ST_ANY);
	if (error)
		return error;
	
	/* Should we read the users home directory preferences */
	NoUserPreferences = SMBGetDictBooleanValue(OpenOptions, kNetFSNoUserPreferences, FALSE);

	/* Only read the preference files once */
	if ((ctx->ct_flags & SMBCF_READ_PREFS) != SMBCF_READ_PREFS)
		smb_ctx_readrc(ctx, NoUserPreferences);
	error  = smb_connect(ctx);
	if (error)
		return error;
	/* Now return what we know about the server */
	mutableDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (mutableDict == NULL) {
		smb_log_info("%s: CFDictionaryCreateMutable failed!", -1, ASL_LEVEL_ERR, __FUNCTION__);
		return errno;
	}
	/* Handle the case we know about here for sure */
	/* All modern servers support change password, but the client doesn't so for now the answer is no! */
	CFDictionarySetValue (mutableDict, kNetFSSupportsChangePasswordKey, kCFBooleanFalse);
	/* Most servers support guest, but not sure how we can tell if it is turned on yet */
	CFDictionarySetValue (mutableDict, kNetFSSupportsGuestKey, kCFBooleanTrue);
	CFDictionarySetValue (mutableDict, kNetFSGuestOnlyKey, kCFBooleanFalse);
	/* Now for the ones the server does return */
	if (ctx->ct_vc_flags & SMBV_MECHTYPE_KRB5) {
		CFStringRef kerbServerAddressRef = NULL;
		CFStringRef kerbServicePrincipalRef = NULL;
		CFMutableDictionaryRef KerberosInfoDict = NULL;

		/* Server supports Kerberos */
		CFDictionarySetValue (mutableDict, kNetFSSupportsKerberosKey, kCFBooleanTrue);
		/* The server gave us a Kerberos service principal name so create a CFString */
		if (ctx->ct_ssn.ioc_kspn_hint[0])
			kerbServicePrincipalRef = CFStringCreateWithCString(NULL, ctx->ct_ssn.ioc_kspn_hint, kCFStringEncodingUTF8);
		
		/* We are not using port 139 so serverName contains either a Bonjour/DNS name or IP address string */
		if ((ctx->ct_port != NBSS_TCP_PORT_139) && (ctx->serverName))
			kerbServerAddressRef = CFStringCreateWithCString(NULL, ctx->serverName, kCFStringEncodingUTF8);
		else {
			struct sockaddr_in	saddr = GET_IP_ADDRESS_FROM_NB((ctx->ct_saddr));
			/* 
			 * At this point all we really have is the ip address. The name could be anything so we can't use
			 * that for the server address. Luckly we don't support IPv6 yet and even when we do it isn't
			 * support with port 139. So lets just get the IP address in presentation form and leave it at that.
			*/
			kerbServerAddressRef = CFStringCreateWithCString(NULL, inet_ntoa(saddr.sin_addr), kCFStringEncodingUTF8);
		}
		
		/* We have a service principal name, a server host name, or both create the Kerberos Info Dictionary */
		if ((kerbServicePrincipalRef) || (kerbServerAddressRef))
			KerberosInfoDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
														 &kCFTypeDictionaryValueCallBacks);
		/* Add the service principal name to the Kerberos Info Dictionary */
		if (kerbServicePrincipalRef) {
			if (KerberosInfoDict)
				CFDictionarySetValue (KerberosInfoDict, kNetFSServicePrincipalKey, kerbServicePrincipalRef);			
			CFRelease (kerbServicePrincipalRef);
		}
		/* Add the server host name to the Kerberos Info Dictionary */
		if (kerbServerAddressRef) {
			if (KerberosInfoDict)
				CFDictionarySetValue (KerberosInfoDict, kNetFSServerAddressKey, kerbServerAddressRef);						
			CFRelease (kerbServerAddressRef);
		}
		if (KerberosInfoDict) {
			CFDictionarySetValue (mutableDict, kNetFSKerberosInfoKey, KerberosInfoDict);			
			CFRelease (KerberosInfoDict);
		}
	}
	else
		CFDictionarySetValue (mutableDict, kNetFSSupportsKerberosKey, kCFBooleanFalse);
	
	/*
	 * Need to return the server display name. We always have serverNameRef,
	 * unless we ran out of memory.
	 *
	 * %%% In the future we should handle the case of not enough memory when 
	 * creating the serverNameRef. Until then just fallback to the server name that
	 * came from the URL.
	 */
	if (ctx->serverNameRef) {
		CFDictionarySetValue (mutableDict, kNetFSServerDisplayNameKey, ctx->serverNameRef);
	} else if (ctx->serverName != NULL) {
		CFStringRef Server = CFStringCreateWithCString(NULL, ctx->serverName, kCFStringEncodingUTF8);
		if (Server != NULL) { 
			CFDictionarySetValue (mutableDict, kNetFSServerDisplayNameKey, Server);
			CFRelease (Server);
		}
	}
	smb_get_os_lanman(ctx, mutableDict);

	if (ctx->ct_vc_shared) 
		smb_get_sessioninfo(ctx, mutableDict, __FUNCTION__);

	*ServerParams = mutableDict;
	return error;
}

int smb_open_session(struct smb_ctx *ctx, CFURLRef url, CFDictionaryRef OpenOptions, CFDictionaryRef *sessionInfo)
{
	int  error = 0;
	int	 NoUserPreferences = SMBGetDictBooleanValue(OpenOptions, kNetFSNoUserPreferences, FALSE);
	int	UseKerberos = SMBGetDictBooleanValue(OpenOptions, kNetFSUseKerberosKey, FALSE);
	int	UseGuest = SMBGetDictBooleanValue(OpenOptions, kNetFSUseGuestKey, FALSE);
	int	UseAnonymous = SMBGetDictBooleanValue(OpenOptions, kNetFSUseAnonymousKey, FALSE);
	int	ChangePassword = SMBGetDictBooleanValue(OpenOptions, kNetFSChangePasswordKey, FALSE);
	char kerbServicePrincipal[1024];
	char kerbClientPrincipal[1024];
	char *servicepn = NULL;
	char *clientpn = NULL;

	/* Now deal with the URL */
	if (ctx->ct_url)
		CFRelease(ctx->ct_url);
	ctx->ct_url =  CFURLCopyAbsoluteURL(url);
	if (ctx->ct_url)
		error = ParseSMBURL(ctx, SMB_ST_ANY);
	else
		error = ENOMEM;
	if (error)
		return error;

	/* Force a new session */
	if (SMBGetDictBooleanValue(OpenOptions, kForcePrivateSessionKey, FALSE)) {
		ctx->ct_ssn.ioc_opt |= SMBV_PRIVATE_VC;
		ctx->ct_flags &= ~SMBCF_CONNECTED;		
		smb_log_info("%s: Force a new session!", 0, ASL_LEVEL_DEBUG, __FUNCTION__);
	} else {
		ctx->ct_ssn.ioc_opt &= ~SMBV_PRIVATE_VC;
		if (SMBGetDictBooleanValue(OpenOptions, kNetFSForceNewSessionKey, FALSE))
			ctx->ct_flags &= ~SMBCF_CONNECTED;
	}

	/* We haven't connect yet or we need to start over in either case read the preference again and do the connect */
	if ((ctx->ct_flags & SMBCF_CONNECTED) != SMBCF_CONNECTED) {
	    	/* Only read the preference files once */
		if ((ctx->ct_flags & SMBCF_READ_PREFS) != SMBCF_READ_PREFS)
			smb_ctx_readrc(ctx, NoUserPreferences);
		error  = smb_connect(ctx);
		if (error)
			return error;
	}
	
	/* 
	 * We are sharing a session. Update the vc flags so we know the current
	 * state. We need to find out if this vc has already been authenticated.
	 */
	if (ctx->ct_vc_shared) {
		smb_get_vc_flags(ctx);
		/* Sharing a Kerberos Session turn on UseKerberos */
		if (ctx->ct_vc_flags & SMBV_KERBEROS_ACCESS)
			UseKerberos = TRUE;
	}
	
	/* They want to use Kerberos, but the server doesn't support it */
	if (UseKerberos && ((ctx->ct_vc_flags & SMBV_MECHTYPE_KRB5) != SMBV_MECHTYPE_KRB5)) {
		smb_log_info("%s: Server doesn't support Kerberos", EAUTH, ASL_LEVEL_DEBUG, __FUNCTION__);
		return EAUTH;
	}
	
	/* Trying to mix security options, not allowed */
	if ((UseKerberos && UseGuest) || (UseKerberos && UseAnonymous) || (UseGuest && UseAnonymous))
		return EINVAL;
	
	/* %%% We currently do not support change password maybe someday */
	if (ChangePassword)
		return ENOTSUP;
	
	/*
	 * Only set these if we are not sharing the session, if we are sharing the session
	 * then use the values of the shared session.
	 */
	if ((! ctx->ct_vc_shared) || (!(ctx->ct_vc_flags & SMBV_AUTH_DONE))) {
		/* Remember that Guest Access is just a Username of "Guest" and an empty password */
		if (UseGuest) {
			smb_ctx_setuser(ctx, kGuestAccountName);
			smb_ctx_setpassword(ctx, kGuestPassword);
		} else
			ctx->ct_ssn.ioc_opt &= ~SMBV_GUEST_ACCESS;
		
		/* Anonymous connection require no username, password or domain */
		if (UseAnonymous) {
			ctx->ct_ssn.ioc_opt |= SMBV_ANONYMOUS_ACCESS;
			ctx->ct_setup.ioc_domain[0] = '\0';
			ctx->ct_setup.ioc_user[0] = '\0';
			ctx->ct_setup.ioc_password[0] = '\0';			
		}
	}
	
	/*
	 * If we are given an empty username and password and they don't request Kerberos, 
	 * Guest, or Anonymous then we should return EAUTH. Having an empty password and username
	 * is Anonymous authentication. 
	 */
	if ((!ctx->ct_vc_shared) && (ctx->ct_setup.ioc_user[0] == 0) && !UseGuest && !UseKerberos && !UseAnonymous)
		return EAUTH;
	
	if (UseKerberos) {
		CFStringRef kerbClientPrincipalRef = NULL;
		CFStringRef kerbServicePrincipalRef = NULL;
		CFDictionaryRef KerberosInfoDict = CFDictionaryGetValue(OpenOptions, kNetFSKerberosInfoKey);
		
		if (KerberosInfoDict) {
			kerbClientPrincipalRef = CFDictionaryGetValue(KerberosInfoDict, kNetFSClientPrincipalKey);
			kerbServicePrincipalRef = CFDictionaryGetValue(KerberosInfoDict, kNetFSServicePrincipalKey);
		}
		if (kerbClientPrincipalRef) {
			CFStringGetCString(kerbClientPrincipalRef, kerbClientPrincipal, 
							   SMB_MAX_KERB_PN, kCFStringEncodingUTF8);
			clientpn = kerbClientPrincipal;
		}
		if (kerbServicePrincipalRef) {
			CFStringGetCString(kerbServicePrincipalRef, kerbServicePrincipal, 
							   SMB_MAX_KERB_PN, kCFStringEncodingUTF8);
			servicepn = kerbServicePrincipal;
		}
		smb_log_info("Kerberos Security: Client Principal Name %s Service Principal Name %s", 0, ASL_LEVEL_DEBUG, 
						 (clientpn) ? clientpn : "", (servicepn) ? servicepn : "");	
		ctx->ct_ssn.ioc_opt |= SMBV_KERBEROS_ACCESS;
	} else
		ctx->ct_ssn.ioc_opt &= ~SMBV_KERBEROS_ACCESS;
		
	error = smb_session_security(ctx, clientpn, servicepn);
	/* 
	 * We failed, so clear out any local security settings. We need to make
	 * sure we do not reuse these values in the next open session.
	 */
	if (error) {
		smb_ctx_setuser(ctx, "");
		smb_ctx_setpassword(ctx, "");
		smb_ctx_setdomain(ctx, "");
	}
	
	if ((error == 0) && sessionInfo) {
		/* create and return session info dictionary */
		CFMutableDictionaryRef mutableDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
												&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (mutableDict == NULL) {
			smb_log_info("%s: Creating sessionInfo failed!", -1, ASL_LEVEL_ERR, __FUNCTION__);	
			return error;
		} 
		smb_get_sessioninfo(ctx, mutableDict, __FUNCTION__);
		*sessionInfo = mutableDict;
	}
	return error;
}

/*
 * Force this mount to be on a new session.
 */
static int smb_force_new_session(struct smb_ctx *ctx, int ForcePrivateSession)
{
	CFMutableDictionaryRef OpenOptions = NULL;
	CFURLRef url = NULL;
	int error = 0;
	
	ctx->ct_flags &= ~SMBCF_CONNECT_STATE;
	
	OpenOptions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
											&kCFTypeDictionaryValueCallBacks);
	if (OpenOptions == NULL)
		return errno;
	
	/* We always force a new vc */
	ctx->ct_flags &= ~SMBCF_CONNECTED;
	
	if (ForcePrivateSession)
		CFDictionarySetValue (OpenOptions, kForcePrivateSessionKey, kCFBooleanTrue);		

	/* We were using kerberos before so use it now */
	if (ctx->ct_vc_flags & SMBV_KERBEROS_ACCESS) {
		CFDictionarySetValue (OpenOptions, kNetFSUseKerberosKey, kCFBooleanTrue);
	} else {
		CFDictionarySetValue (OpenOptions, kNetFSUseKerberosKey, kCFBooleanFalse);
	}
	if (ctx->ct_ssn.ioc_opt & SMBV_GUEST_ACCESS)
		CFDictionarySetValue (OpenOptions, kNetFSUseGuestKey, kCFBooleanTrue);
	/* 
	 * Get ready to do the open session call again. We have no way of telling if we should read
	 * the users home directory preference file. We should have already read the preference files, but just to
	 * be safe we will not allow this connection to read the home directory preference file. Second we need to
	 * save the url off until after the call. The smb_open_session will free ct_url if it exist.
	 */
	CFDictionarySetValue (OpenOptions, kNetFSNoUserPreferences, kCFBooleanTrue);
	url = ctx->ct_url;
	ctx->ct_url = NULL;
	error = smb_open_session(ctx, url, OpenOptions, NULL);
	/* if we have an error put the url back the way it was before we did the connect */
	if (error) {
		if (ctx->ct_url)
			CFRelease(ctx->ct_url);		
		ctx->ct_url = url;
	} else if (url)
		CFRelease(url);		
	CFRelease(OpenOptions);
	return error;
}

/*
 * It would be nice if the smb_netshareenum used a CF Dictionary, maybe some day
 */
int smb_enumerate_shares(struct smb_ctx *ctx, CFDictionaryRef *shares)
{
	struct share_info *share_info = NULL;
	struct share_info *ep;
	int ii, entries, total;
	int error = 0, len;
	CFMutableDictionaryRef	currDict = NULL;
	CFMutableDictionaryRef mutableDict = NULL;
	CFStringRef	ShareStr = NULL;
	struct statfs *fs = NULL;
	int fs_cnt = 0;
	
	/* If connection is down report EPIPE */
	if (((ctx->ct_flags & SMBCF_CONNECTED) != SMBCF_CONNECTED) || (smb_ctx_connstate(ctx) == ENOTCONN)) {
		ctx->ct_flags &= ~SMBCF_CONNECT_STATE;
		return EPIPE;		
	}

	smb_ctx_setshare(ctx, "IPC$", SMB_ST_ANY);
	error = smb_share_connect(ctx);
	if (error)
		goto WeAreDone;		

	*shares = NULL;
	error = smb_netshareenum(ctx, &entries, &total, &share_info);
	if (error) {
		smb_log_info("%s: smb_netshareenum!", error, ASL_LEVEL_DEBUG, __FUNCTION__);
		goto WeAreDone;		
	}
	
	error = -1;
	mutableDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
											&kCFTypeDictionaryValueCallBacks);
	if (mutableDict == NULL)
		goto WeAreDone;
	/* Get the list of mounted volumes */
	fs = smb_getfsstat(&fs_cnt);

	for (ep = share_info, ii = 0; ii < entries; ii++, ep++) {
		/* We only return Disk or Printer shares */
		if ((ep->type != SMB_ST_DISK) && (ep->type != SMB_ST_PRINTER))
			continue;
		currDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
											&kCFTypeDictionaryValueCallBacks);
		if (currDict == NULL)
			goto WeAreDone;				

		if (ep->type == SMB_ST_DISK) {
			if ((smb_ctx_setshare(ctx, ep->netname, SMB_ST_ANY) == 0) && (already_mounted(ctx, fs, fs_cnt, currDict)))
				CFDictionarySetValue (currDict, kNetFSAlreadyMountedKey, kCFBooleanTrue);
			else 
				CFDictionarySetValue (currDict, kNetFSAlreadyMountedKey, kCFBooleanFalse);
		} else {
			CFDictionarySetValue (currDict, kNetFSPrinterShareKey, kCFBooleanTrue);				
			CFDictionarySetValue (currDict, kNetFSAlreadyMountedKey, kCFBooleanFalse);
		}
			
		/* If in share mode we could require a volume password, need to test this out and figure out what we should  do. */
		CFDictionarySetValue (currDict, kNetFSHasPasswordKey, kCFBooleanFalse);
		
		len = (int)strlen(ep->netname);
		if (len && (ep->netname[len-1] == '$'))
			CFDictionarySetValue (currDict, kNetFSIsHiddenKey, kCFBooleanTrue);
		ShareStr = CFStringCreateWithCString(NULL, ep->netname, kCFStringEncodingUTF8);
		if (ShareStr == NULL)
			goto WeAreDone;				

		CFDictionarySetValue (mutableDict, ShareStr, currDict);
		CFRelease (ShareStr);
		CFRelease (currDict);
		ShareStr = NULL;
		currDict = NULL;		
	}
	*shares = mutableDict;
	mutableDict = NULL;
	error = 0;
	
WeAreDone:
		
	if (error == -1)
		error = errno;
	/* 
	 * We can do this one of two ways. I decided to just close the tree connect and clear the rpc after every
	 * enumerate shares call. This way every enumerate shares call will be a fresh tree connection. The other
	 * way to do this is keep track of the fact we have have a IPC$ tree connect and only close it at smb_ctx_done
	 * time. That method means we would have to hold on to the IPC$ tree connect for the life of the process.
	 */
	(void)smb_share_disconnect(ctx);
	smb_ctx_setshare(ctx, "", SMB_ST_ANY);
	if (fs)
		free(fs);
	smb_freeshareinfo(share_info, entries);
	if (mutableDict)
		CFRelease (mutableDict);
	if (ShareStr)
		CFRelease (ShareStr);
	if (currDict)
		CFRelease (currDict);

	if (error)
		smb_log_info("Enumerate shares failed!", error, ASL_LEVEL_ERR);
	return error;	
}

int smb_mount(struct smb_ctx *ctx, CFStringRef mpoint, CFDictionaryRef mOptions, CFDictionaryRef *mInfo)
{
	CFMutableDictionaryRef mdict = NULL;
	struct UniqueSMBShareID req;
	int	 ForcePrivateSession = SMBGetDictBooleanValue(mOptions, kForcePrivateSessionKey, FALSE);
	struct smb_mount_args mdata;
	int error = 0;
	char mount_point[MAXPATHLEN];
	struct stat st;
	int mntflags;
	CFNumberRef numRef;

	
	/*  Initialize the mount arguments  */
	mdata.version = SMB_IOC_STRUCT_VERSION;
	mdata.altflags = ctx->altflags; /* Contains flags that were read from preference */
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
	
	/* Get the mount flags, just in case there are no flags or something is wrong we start with them set to zero. */
	mntflags = 0;
	if (mOptions) {
		numRef = (CFNumberRef)CFDictionaryGetValue (mOptions, kNetFSMountFlagsKey);
		if (numRef)
			(void)CFNumberGetValue(numRef, kCFNumberSInt32Type, &mntflags);
		
	}
	/* If automounted or don't browse force a new private session. */
	if (ctx->ct_vc_shared && (mntflags & (MNT_DONTBROWSE | MNT_AUTOMOUNTED)))
		ForcePrivateSession = TRUE;
	
	/* Create the dictionary used to return mount information in. */
	mdict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (!mdict) {
		smb_log_info("%s: allocation of dictionary failed!", -1, ASL_LEVEL_DEBUG, __FUNCTION__);
		return ENOMEM;
	}
	
	/*
	 * Should we always require a new connection. The old code always mounted every volume on
	 * its own vc. Not sure how to handle that yet. For now we allow them to share the VC, if we start
	 * having problem all we need to do is cause smb_force_new_session to get called.
	 */
	if ((ForcePrivateSession) || ((ctx->ct_flags & SMBCF_CONNECTED) != SMBCF_CONNECTED))
		error = smb_force_new_session(ctx, ForcePrivateSession);
	
	/*
	 * If they have the MNT_DONTBROWSE or MOPT_AUTOMOUNTED then ignore the fact that its already mounted. AFP 
	 * also checks for the kForcePrivateSessionKey so we will to, when in doubt always do what AFP does. 
	 * If its is already mounted return EEXIST and the mount information.
	 */
	if ((error == 0) && (!ForcePrivateSession) && ((mntflags & (MNT_DONTBROWSE | MNT_AUTOMOUNTED)) == 0)) {
		int fs_cnt = 0;
		struct statfs *fs = smb_getfsstat(&fs_cnt);	/* Get the list of  mounted volumes */
		
		error = already_mounted(ctx, fs, fs_cnt, mdict);
		if (fs)	/* Done with free it */
			free(fs);
		/* It already exist return the mount point */
		if (error == EEXIST) /* Only error already_mounted returns */ {
			if (mInfo) {
				*mInfo = mdict;
				mdict = NULL;
			}
			goto WeAreDone;
		}
	}
		 /* Connect to the share, Dfs work should be done here */
	if (error == 0)		
		error = smb_share_connect(ctx);
	
	if (error) {
		/* If connection is down report EPIPE and set that the connection is down */
		if ((error == ENOTCONN) || (smb_ctx_connstate(ctx) == ENOTCONN)) {
			ctx->ct_flags &= ~SMBCF_CONNECT_STATE;
			error =  EPIPE;
		}
		smb_log_info("%s: smb_share_connect failed!", error, ASL_LEVEL_DEBUG, __FUNCTION__);
		goto WeAreDone;
	}
		 
	CFStringGetCString(mpoint, mount_point, sizeof(mount_point), kCFStringEncodingUTF8);
	if (stat(mount_point, &st) == -1)
		 error = errno;
	else if (!S_ISDIR(st.st_mode))
		 error = ENOTDIR;
		 
	 if (error) {
		 smb_log_info("%s: bad mount point!", error, ASL_LEVEL_DEBUG, __FUNCTION__);
		 goto WeAreDone;
	 }
	
	/* now create the unique_id, using tcp address + port + uppercase share */
	if (ctx->ct_saddr)
		create_unique_id(ctx, mdata.unique_id, &mdata.unique_id_len);
	else
		smb_log_info("%s: ioc_saddr is NULL how did that happen?", 0, ASL_LEVEL_DEBUG, __FUNCTION__);
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
	
	if (numRef && (CFNumberGetValue(numRef, kCFNumberSInt16Type, &mdata.dir_mode)))
		mdata.dir_mode &= (S_IRWXU | S_IRWXG | S_IRWXO); /* We were passed in the modes to use */
	else if (ctx->ct_ssn.ioc_opt & SMBV_GUEST_ACCESS)
		mdata.dir_mode = S_IRWXU | S_IRWXG | S_IRWXO;	/* Guest access open it up */
	else 
		mdata.dir_mode = S_IRWXU;						/* User access limit access to the user that mounted it */
	
	if (mOptions)
		numRef = (CFNumberRef)CFDictionaryGetValue(mOptions, kfileModeKey);
	else 
		numRef = NULL;

	/* See if we were passed a file mode */
	if (numRef && (CFNumberGetValue(numRef, kCFNumberSInt16Type, &mdata.file_mode)))
		mdata.file_mode &= (S_IRWXU | S_IRWXG | S_IRWXO); /* We were passed in the modes to use */
	else if (ctx->ct_ssn.ioc_opt & SMBV_GUEST_ACCESS)
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
	
	mdata.debug_level = ctx->debug_level;
	mdata.dev = ctx->ct_fd;
	
	CreateSMBFromName(ctx, mdata.url_fromname, MAXPATHLEN);
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

		CFStringGetCString(ctx->mountPath, mdata.path, MAXPATHLEN, kCFStringEncodingUTF8);
		mdata.path_len = (u_int32_t)strlen(mdata.path);	/* Path length does not include the null byte */
		
	} else if (ctx->ct_origshare) /* Just to be safe, should never happen */
		strlcpy(mdata.volume_name, ctx->ct_origshare, sizeof(mdata.volume_name));
		
	smb_log_info("%s: Volume name = %s", 0, ASL_LEVEL_DEBUG, __FUNCTION__, mdata.volume_name);
	
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
	} else 
		smb_log_info("%s: Getting mount information failed !", 0, ASL_LEVEL_ERR, __FUNCTION__);

WeAreDone:
	if (error) {
		char * log_server = (ctx->serverName) ? ctx->serverName : (char *)"";
		char * log_share = (ctx->ct_origshare) ? ctx->ct_origshare : (char *)"";
		smb_log_info("%s: mount failed to %s/%s ", error, ASL_LEVEL_ERR, 
					 __FUNCTION__, log_server, log_share);
		/* If we have an error send a tree disconnect */
		if (ctx->ct_flags & SMBCF_SHARE_CONN)
			(void)smb_share_disconnect(ctx);
	}
	if (mdict)
		 CFRelease(mdict);
	ctx->ct_flags &= ~SMBCF_SHARE_CONN;
	return error;
}

/*
 * Create the smb library context and fill in the default values.
 */
void *smb_create_ctx(void)
{
	struct smb_ctx *ctx = NULL;
	
	ctx = malloc(sizeof(struct smb_ctx));
	if (ctx == NULL)
		return NULL;
	
	/* Clear out everything out to start with */
	bzero(ctx, sizeof(*ctx));

	if (pthread_mutex_init(&ctx->ctx_mutex, NULL) == -1) {
		smb_log_info("%s: pthread_mutex_init failed!", -1, ASL_LEVEL_DEBUG, __FUNCTION__);
		free(ctx);
		return NULL;
	}
	
	ctx->ct_fd = -1;
	ctx->ct_level = SMBL_VC;	/* Deafult to VC */
	ctx->ct_port_behavior = TRY_BOTH_PORTS;
	ctx->ct_port = SMB_TCP_PORT_445;

	ctx->ct_ssn.ioc_opt = SMBV_MINAUTH_NTLM | SMBV_SERVER_DOMAIN;
	ctx->ct_ssn.ioc_owner = geteuid();
	ctx->ct_ssn.ioc_reconnect_wait_time = SMBM_RECONNECT_WAIT_TIME;
	/* We now default to using streams */
	ctx->altflags |= SMBFS_MNT_STREAMS_ON;

		
	/* Should this be setable? */
	strlcpy(ctx->ct_ssn.ioc_localcs, "default", sizeof(ctx->ct_ssn.ioc_localcs));	
	/* 
	 * Get the host name, but only copy what will fit in the buffer. We do not care if the name 
	 * did not fit. In most cases this name will get replaced, because we now try to get the local 
	 * NetBIOS name from the global config file.
	 *
	 * NOTE: It looks like NTLMSSP requires the local host name, currently it looks like 
	 * they always use a NetBIOS name.
	 */
	(void)nb_getlocalname(ctx->ct_ssn.ioc_localname, (size_t)sizeof(ctx->ct_ssn.ioc_localname));
	
	return ctx;
}

/*
 * This is used by the mount_smbfs and smbutil routine to create and initialize the
 * smb library context structure.
 */
int smb_ctx_init(struct smb_ctx **out_ctx, const char *url, u_int32_t level, 
				 int sharetype, int NoUserPreferences)
{
	int  error = 0;
	struct smb_ctx *ctx = NULL;
	
	/* Create the structure and fill in the default values */
	ctx = smb_create_ctx();
	if (ctx == NULL) {
		error = ENOMEM;
		goto failed;		
	}
	ctx->ct_level = level;
	ctx->ct_flags |= SMBCF_MOUNTSMBFS;	/* Called from a command line utility */
	/* Create the CFURL */
	ctx->ct_url = CreateSMBURL(url);
	if (ctx->ct_url == NULL) {
		error = EINVAL;
		goto failed;
	}
	
	error = ParseSMBURL(ctx, sharetype);
	/* read the preference files */
	if (! error )
		smb_ctx_readrc(ctx, NoUserPreferences);
	if (!error) {
		*out_ctx = ctx;
		return 0;
	}
	
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
		smb_log_info("%s: Canceling connection", 0, ASL_LEVEL_DEBUG, __FUNCTION__);
		smb_ctx_cancel_connection(ctx);
		pthread_mutex_lock(&ctx->ctx_mutex);
	}
	
	if (ctx->ct_fd != -1)
		close(ctx->ct_fd);
	nb_ctx_done(&ctx->ct_nb);
	if (ctx->ct_url)
		CFRelease(ctx->ct_url);
	if (ctx->serverName)
		free(ctx->serverName);
	if (ctx->serverNameRef)
		CFRelease(ctx->serverNameRef);
	if (ctx->ct_saddr)
		free(ctx->ct_saddr);
	if (ctx->ct_laddr)
		free(ctx->ct_laddr);
	if (ctx->netbios_dns_name)
		free(ctx->netbios_dns_name);
	if (ctx->ct_origshare)
		free(ctx->ct_origshare);
	if (ctx->mountPath)
		CFRelease(ctx->mountPath);
	pthread_mutex_unlock(&ctx->ctx_mutex);
	pthread_mutex_destroy(&ctx->ctx_mutex);
	free(ctx);
}
