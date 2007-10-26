/*
 * Copyright (c) 2000, Boris Popov
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
#include <URLMount/URLMount.h>

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
#include <rpc_cleanup.h>


/* 
 * Create a unique_id, that can be used to find a matching mounted
 * volume, given the server address, port number and share name.
 */
static void create_unique_id(struct smb_ctx *ctx, unsigned char *id, int32_t *unique_id_len)
{
	struct sockaddr_in	saddr = GET_IP_ADDRESS_FROM_NB((ctx->ct_ssn.ioc_server));
	int total_len = sizeof(saddr.sin_addr) + sizeof(ctx->ct_port) + strlen(ctx->ct_sh.ioc_share);
	
	if (total_len > SMB_MAX_UBIQUE_ID) {
		smb_log_info("create_unique_id '%s' too long", 0, ASL_LEVEL_ERR, ctx->ct_sh.ioc_share);
		memset(id, 0, SMB_MAX_UBIQUE_ID);
		return; /* program error should never happen, but just incase */
	}
	memcpy(id, &saddr.sin_addr, sizeof(saddr.sin_addr));
	id += sizeof(saddr.sin_addr);
	memcpy(id, &ctx->ct_port, sizeof(ctx->ct_port));
	id += sizeof(ctx->ct_port);
	memcpy(id, ctx->ct_sh.ioc_share, strlen(ctx->ct_sh.ioc_share));
	id += strlen(ctx->ct_sh.ioc_share);
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
	bufsize = *fs_cnt * sizeof(*fs);
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
	if ((fsctl(mntonname, smbfsUniqueShareIDFSCTL, req, 0 ) == 0) && (req->error == EEXIST)) {
		CFStringRef tmpString = NULL;
		
		tmpString = CFStringCreateWithCString (NULL, mntonname, kCFStringEncodingUTF8);
		if (tmpString) {
			CFDictionarySetValue (mdict, kMountPathKey, tmpString);
			CFRelease (tmpString);			
		}
		if ((req->connection_type == kConnectedByGuest) || (strcasecmp(req->user, kGuestAccountName) == 0))
			CFDictionarySetValue (mdict, kMountedByGuestKey, kCFBooleanTrue);
		else if (req->user[0] != 0) {
			tmpString = CFStringCreateWithCString (NULL, req->user, kCFStringEncodingUTF8);
			if (tmpString) {
				CFDictionarySetValue (mdict, kMountedByUserKey, tmpString);			
				CFRelease (tmpString);
			}
			/* We have a user name, but it's a Kerberos client principal name */
			if (req->connection_type == kConnectedByKerberos)
		    		CFDictionarySetValue (mdict, kMountedByKerberosKey, kCFBooleanTrue);	    
		} else 
			CFDictionarySetValue (mdict, kMountedByKerberosKey, kCFBooleanTrue);

		return EEXIST;
	}
	return 0;
}

static int already_mounted(struct smb_ctx *ctx, struct statfs *fs, int fs_cnt, CFMutableDictionaryRef mdict)
{
	struct UniqueSMBShareID req;
	int				ii;
	
	if ((fs == NULL) || (ctx->ct_ssn.ioc_server == NULL))
		return 0;
	/* now create the unique_id, using tcp address + port + uppercase share */
	create_unique_id(ctx, req.unique_id, &req.unique_id_len);
	req.error = 0;
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
 * Given a dictionary see if it has a boolean.
 * If no dictionary or no value return false otherwise return the value
 */
static int SMBGetDictBooleanValue(CFDictionaryRef Dict, const void * KeyValue)
{
	CFBooleanRef booleanRef;
	
	if (Dict) {
		booleanRef = (CFBooleanRef)CFDictionaryGetValue(Dict, KeyValue);
		if ((booleanRef != NULL) && (CFBooleanGetValue(booleanRef)))
			return TRUE;
	}
	return FALSE;
}

/* this routine does not uppercase the server name */
void smb_ctx_setserver(struct smb_ctx *ctx, const char *name)
{
	/* don't uppercase the server name */
	if (strlen(name) > SMB_MAX_DNS_SRVNAMELEN) { 
		ctx->ct_ssn.ioc_srvname[0] = '\0';
	} else
		strcpy(ctx->ct_ssn.ioc_srvname, name);
	return;
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
	strlcpy(ctx->ct_ssn.ioc_user, name, SMB_MAXUSERNAMELEN);
	/* We need to tell the kernel if we are trying to do guest access */
	if (strcasecmp(ctx->ct_ssn.ioc_user, kGuestAccountName) == 0)
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
	if (ctx->ct_ssn.ioc_domain[0])
		return 0;
	
	if (strlen(name) >= SMB_MAXNetBIOSNAMELEN) {
		smb_log_info("domain/workgroup name '%s' too long", 0, ASL_LEVEL_ERR, name);
		return ENAMETOOLONG;
	}
	strlcpy(ctx->ct_ssn.ioc_domain,  name, SMB_MAXNetBIOSNAMELEN+1);
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
		strcpy(ctx->ct_ssn.ioc_password, "");
	else
		strcpy(ctx->ct_ssn.ioc_password, passwd);
	/* Fill in the share level password */
	strcpy(ctx->ct_sh.ioc_password, ctx->ct_ssn.ioc_password);
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
 * Copy in the servers address
 */
static int smb_ctx_setsrvaddr(struct smb_ctx *ctx, const char *addr)
{
	if (addr == NULL || addr[0] == 0)
		return EINVAL;
	if (ctx->ct_srvaddr)
		free(ctx->ct_srvaddr);
	if ((ctx->ct_srvaddr = strdup(addr)) == NULL)
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
			strlcpy(ctx->LocalNetBIOSName, p, sizeof(ctx->LocalNetBIOSName));
			str_upper(ctx->LocalNetBIOSName, ctx->LocalNetBIOSName);
			smb_log_info("Using NetBIOS Name  %s", 0, ASL_LEVEL_DEBUG, ctx->LocalNetBIOSName);
		}
	}
	if (level <= 1) {
		int aflags;
		rc_getstringptr(smb_rc, sname, "port445", &p);
		if (p) { /* Default is to try both */
			if (strcmp(p, "netbios_only") == 0) {
				ctx->ct_port_behavior = USE_THIS_PORT_ONLY;
				ctx->ct_port = NBSS_TCP_PORT_139;
			}
			else if (strcmp(p, "no_netbios") == 0) 
				ctx->ct_port_behavior = USE_THIS_PORT_ONLY;
		}
		rc_getint(smb_rc, sname, "debug_level", &ctx->debug_level);
		aflags = FALSE;
		rc_getbool(smb_rc, sname, "streams", &aflags);
		if (aflags)
			ctx->altflags |= SMBFS_MNT_STREAMS_ON;
		aflags = FALSE;
		rc_getbool(smb_rc, sname, "soft", &aflags);
		if (aflags)
			ctx->altflags |= SMBFS_MNT_SOFT;
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
			error = smb_ctx_setsrvaddr(ctx, p);
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
	 * The /var/run/smb.conf uses the global section header and we use 
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
	if (ctx->ct_ssn.ioc_srvname[0] == 0)
		goto done;
	
	/*
	 * SERVER parameters.
	 */
	smb_ctx_readrcsection(smb_rc, ctx, ctx->ct_ssn.ioc_srvname, 1);
	nb_ctx_readrcsection(smb_rc, &ctx->ct_nb, ctx->ct_ssn.ioc_srvname, 1);
	
	/*
	 * If we don't have a user name, we can't read any of the
	 * [server:user...] sections.
	 */
	if (ctx->ct_ssn.ioc_user[0] == 0)
		goto done;
	
	/*
	 * SERVER:USER parameters
	 */
	snprintf(sname, sizeof(sname), "%s:%s", ctx->ct_ssn.ioc_srvname,
	ctx->ct_ssn.ioc_user);
	smb_ctx_readrcsection(smb_rc, ctx, sname, 2);
	
	/*
	 * If we don't have a share name, we can't read any of the
	 * [server:user:share] sections.
	 */
	if (ctx->ct_sh.ioc_share[0] != 0) {
		/*
		 * SERVER:USER:SHARE parameters
		 */
		snprintf(sname, sizeof(sname), "%s:%s:%s", ctx->ct_ssn.ioc_srvname,
		ctx->ct_ssn.ioc_user, ctx->ct_sh.ioc_share);
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
	char server[SMB_MAXNetBIOSNAMELEN + 1];
	char workgroup[SMB_MAXNetBIOSNAMELEN + 1];
	int error;
	
	server[0] = workgroup[0] = '\0';
	error = nbns_getnodestatus(sap, &ctx->ct_nb, server, workgroup);
	if (error == 0) {
#ifdef OVERRIDE_USER_SETTING_DOMAIN
		if (workgroup[0])
			smb_ctx_setdomain(ctx, workgroup);
#endif // OVERRIDE_USER_SETTING_DOMAIN
		if (server[0])
			smb_ctx_setserver(ctx, server);
	} else {
		if (ctx->ct_ssn.ioc_srvname[0] == (char)0)
			smb_ctx_setserver(ctx, "*SMBSERVER");
	}
	return error;
}

static int smb_ctx_gethandle(struct smb_ctx *ctx)
{
	int fd, i;
	char buf[20];

	if (ctx->ct_fd != -1) {
		rpc_cleanup_smbctx(ctx);
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
	
	if (ioctl(ctx->ct_fd, SMBIOC_FLAGS2, &flags2) == -1) {
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
	
	if ((ctx->ct_fd != -1) && (ioctl(ctx->ct_fd, SMBIOC_CANCEL_SESSION, &dummy) == -1))
		smb_log_info("can't cancel the connection ", errno, ASL_LEVEL_DEBUG);
}

/*
 * Return the connection state of the session. Currently only returns ENOTCONN
 * or EISCONN. May want to expand this in the future.
 */
u_int16_t smb_ctx_connstate(struct smb_ctx *ctx)
{
	u_int16_t connstate = 0;
	
	if (ioctl(ctx->ct_fd, SMBIOC_SESSSTATE, &connstate) == -1) {
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
	bcopy(&ctx->ct_ssn, &rq.ioc_ssn, sizeof(struct smbioc_ossn));
	
	if (ctx->ct_port_behavior == TRY_BOTH_PORTS)
		rq.vc_conn_state = NSMBFL_TRYBOTH;
		
	/* Call the kernel to do make the negotiate call */
	if (ioctl(ctx->ct_fd, SMBIOC_NEGOTIATE, &rq) == -1) {
		error = errno;
		smb_log_info("%s: negotiate ioctl failed:\n", error, ASL_LEVEL_DEBUG, __FUNCTION__);
		goto out;
	}

	/* Get the server's capablilities */
	ctx->ct_vc_caps = rq.vc_caps;
	ctx->ct_vc_shared = (rq.vc_conn_state & NSMBFL_SHAREVC);
	/* Get the virtual circuit flags */
	ctx->ct_vc_flags = rq.flags;
	/* If we have no username and the kernel does then use the name in the kernel */
	if ((ctx->ct_ssn.ioc_user[0] == 0) && rq.ioc_ssn.ioc_user[0]) {
		strlcpy(ctx->ct_ssn.ioc_user, rq.ioc_ssn.ioc_user, SMB_MAXUSERNAMELEN + 1);
		smb_log_info("%s: ctx->ct_ssn.ioc_user = %s\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, ctx->ct_ssn.ioc_user);		
	}
	
	/* This server doesn't support extended security never try extended security again */
	if ((ctx->ct_vc_flags & SMBV_EXT_SEC) != SMBV_EXT_SEC) {
		ctx->ct_ssn.ioc_opt &= ~SMBV_EXT_SEC;
	} else if ((ctx->ct_vc_flags & SMBV_KERBEROS_SUPPORT) == SMBV_KERBEROS_SUPPORT) {
		/* We are sharing this connection get the Kerberos client principal name */
		if (ctx->ct_vc_shared && (rq.ioc_ssn.ioc_kuser[0]))
			strlcpy(ctx->ct_ssn.ioc_kuser, rq.ioc_ssn.ioc_kuser, SMB_MAXUSERNAMELEN + 1);
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
		if (((strncasecmp ((char *)rq.spn, "cifs/", sizeof(rq.spn))) == 0) ||
			((strncasecmp ((char *)rq.spn, WIN2008_SPN_PLEASE_IGNORE_REALM, sizeof(rq.spn))) == 0)) {
			/* We need to add "cifs/ instance part" */
			strlcpy((char *)rq.spn, "cifs/", sizeof(rq.spn));
			/* Now the host name without a realm */
			if ((ctx->ct_port != NBSS_TCP_PORT_139) && (ctx->ct_fullserver))
				strlcat((char *)rq.spn, ctx->ct_fullserver, sizeof(rq.spn));
			else {
				struct sockaddr_in	saddr = GET_IP_ADDRESS_FROM_NB((ctx->ct_ssn.ioc_server));
				/* 
				 * At this point all we really have is the ip address. The name could be anything so we can't use
				 * that for the server address. Luckly we don't support IPv6 yet and even when we do it isn't
				 * support with port 139. So lets just get the IP address in presentation form and leave it at that.
				 */
				strlcat((char *)rq.spn, inet_ntoa(saddr.sin_addr), sizeof(rq.spn));				
			}
			rq.spn_len = strlen((char *)rq.spn)+1;
		}
		
		ctx->ct_kerbPrincipalName_len = 0;
		if (ctx->ct_kerbPrincipalName)
			free(ctx->ct_kerbPrincipalName);
		ctx->ct_kerbPrincipalName = malloc(rq.spn_len);
		if (ctx->ct_kerbPrincipalName) {
			ctx->ct_kerbPrincipalName_len = rq.spn_len;
			bcopy(rq.spn, ctx->ct_kerbPrincipalName, ctx->ct_kerbPrincipalName_len);
			smb_log_info("%s: ctx->ct_kerbPrincipalName = %s\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, ctx->ct_kerbPrincipalName);
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
						 (ctx->ct_fullserver) ? ctx->ct_fullserver : "");
		rpc_cleanup_smbctx(ctx);
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
	struct smbioc_treeconn rq;
	int error = 0;
	
	if ((ctx->ct_fd < 0) || ((ctx->ct_flags & SMBCF_SHARE_CONN) != SMBCF_SHARE_CONN))
		return 0;	/* Nothing to do here */
	
	bzero(&rq, sizeof(rq));
	bcopy(&ctx->ct_ssn, &rq.ioc_ssn, sizeof(struct smbioc_ossn));
	bcopy(&ctx->ct_sh, &rq.ioc_sh, sizeof(struct smbioc_oshare));
	if (ioctl(ctx->ct_fd, SMBIOC_TDIS, &rq) == -1) {
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
	struct smbioc_treeconn rq;
	int error = 0;
	
	ctx->ct_flags &= ~SMBCF_SHARE_CONN;
	if ((ctx->ct_flags & SMBCF_AUTHORIZED) == 0)
		return EAUTH;

	bzero(&rq, sizeof(rq));
	bcopy(&ctx->ct_ssn, &rq.ioc_ssn, sizeof(struct smbioc_ossn));
	bcopy(&ctx->ct_sh, &rq.ioc_sh, sizeof(struct smbioc_oshare));
	if (ioctl(ctx->ct_fd, SMBIOC_TCON, &rq) == -1)
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

	if (ioctl(ctx->ct_fd, SMBIOC_GET_VC_FLAGS, &vc_flags) == -1)
		smb_log_info("%s: Getting the vc flags falied:\n", -1, ASL_LEVEL_ERR, __FUNCTION__);
	else
		ctx->ct_vc_flags = vc_flags;
}

int smb_session_security(struct smb_ctx *ctx, char *clientpn, char *servicepn)
{
	struct smbioc_ssnsetup rq;
	int error = 0;
	
	ctx->ct_flags &= ~SMBCF_AUTHORIZED;
	if ((ctx->ct_flags & SMBCF_CONNECTED) != SMBCF_CONNECTED) {
		return EPIPE;
	}

	bzero(&rq, sizeof(rq));
	bcopy(&ctx->ct_ssn, &rq.ioc_ssn, sizeof(struct smbioc_ossn));
	/* Used by Kerberos, should be NULL in all other cases */
	if (clientpn) {
		rq.user_clientpn = clientpn;
		rq.clientpn_len = strlen(clientpn) + 1;
	}
	if (servicepn) {
		rq.user_servicepn = servicepn;
		rq.servicepn_len = strlen(servicepn) + 1;
	}
	if (ioctl(ctx->ct_fd, SMBIOC_SSNSETUP, &rq) == -1)
		error = errno;
	if (error == 0) {
		ctx->ct_flags |= SMBCF_AUTHORIZED;
		smb_get_vc_flags(ctx);	/* Update the the vc flags in case they have changed */	
		/* Save off the client name */
		if (clientpn) {
			char *realm;
			
			strlcpy(ctx->ct_ssn.ioc_kuser, clientpn, SMB_MAXUSERNAMELEN + 1);
			realm = strchr(ctx->ct_ssn.ioc_kuser, KERBEROS_REALM_DELIMITER);
			if (realm)	/* We really only what the client name, skip the realm stuff */
				*realm = 0;
		}
		
	}
	
	return (error);
}

/*
 * Resolve the server NetBIOS name and address
 */
static int smb_resolve_port_139(struct smb_ctx *ctx)
{
	struct sockaddr *sap = NULL;
	struct sockaddr_nb *saserver = NULL;
	struct sockaddr_nb *salocal = NULL;
	struct nb_name nn;
	int error = 0;
	int allow_local_conn = (ctx->ct_flags & SMBCF_MOUNTSMBFS);
	
	ctx->ct_flags &= ~SMBCF_RESOLVED;
	
	/* Get the inet addr need to resolve the NetBIOS name could be a broadcast address */
	error = nb_ctx_resolve(&ctx->ct_nb);
	if (error)
		goto WeAreDone;
	
	/*
	 * This is port 139, we need the server's address and its NetBIOS name. If this is Samba
	 * or NT4 (are greater) then we could use "*SMBSERVER" if we cannot find the server's
	 * NetBIOS name.
	 *
	 * If we have a "ct_srvaddr" then we have the servers address in the dot IP string form
	 * of 192.0.0.1 and we should have been given there NetBIOS name. Look at the config file
	 * for how this works.
	 *
	 * If we have the "ioc_srvname" assume its a NetBIOS name and try to resolve it using 
	 * port 137. If this fails or we don't have a "ioc_srvname" and we do have a "ct_fullserver"
	 * then try to resolve it using DNS and just use "*SMBSERVER" as the NetBIOS name.
	 *
	 * With support for 445 this code will be used less and less.
	 */
	if (ctx->ct_srvaddr)
		error = nb_resolvehost_in(ctx->ct_srvaddr, &sap, NBSS_TCP_PORT_139, allow_local_conn);
	else {
		if (ctx->ct_ssn.ioc_srvname[0]) {
			char * netbios_name = convert_utf8_to_wincs(ctx->ct_ssn.ioc_srvname);
			
			if (netbios_name == NULL)
				netbios_name = ctx->ct_ssn.ioc_srvname;
			error = nbns_resolvename(netbios_name, &ctx->ct_nb, ctx, &sap);
			/* We have the convert name, if we resolved it then keep that name */
			if (netbios_name != ctx->ct_ssn.ioc_srvname) {
				if (!error)
					smb_ctx_setserver(ctx, netbios_name);
				free(netbios_name);
			}
		}
		else
			error = ENOTSUP;

		if (error && ctx->ct_fullserver) {
			error = nb_resolvehost_in(ctx->ct_fullserver, &sap, NBSS_TCP_PORT_139, allow_local_conn);
			if (error == 0)
				smb_ctx_getnbname(ctx, sap);
		}
	}
	if (error) 
		goto WeAreDone;

	/* See if they are using a scope, more code that is not the norm, but we still support */
	nn.nn_scope = (u_char *)(ctx->ct_nb.nb_scope);
	nn.nn_type = NBT_SERVER;
	
	/* We use strlcpy here now, because ioc_srvname can be up to 60 bytes while a NetBIOS name can only be 16 bytes. */ 
	strlcpy((char *)(nn.nn_name), ctx->ct_ssn.ioc_srvname, sizeof(nn.nn_name));
	/*
	 * We no longer keep the server name in uppercase. When connecting on port 139 we need to 
	 * uppercases netbios name that is used for the connection. So after we copy it in and before
	 * we encode it we should make sure it gets uppercase.
	 */
	str_upper((char *)(nn.nn_name), (char *)(nn.nn_name));
	error = nb_sockaddr(sap, &nn, &saserver);
	free(sap);
	sap = NULL;
	if (error) 
		goto WeAreDone;

	/* Get the host name, but only copy what will fit in the buffer.  */ 
	strlcpy((char *)nn.nn_name, ctx->LocalNetBIOSName, sizeof(nn.nn_name));
	nn.nn_type = NBT_WKSTA;
	nn.nn_scope = (u_char *)(ctx->ct_nb.nb_scope);
	error = nb_sockaddr(NULL, &nn, &salocal);
	if (error) 
		goto WeAreDone;

	if (ctx->ct_ssn.ioc_server)
		free(ctx->ct_ssn.ioc_server);		
	if (ctx->ct_ssn.ioc_local)
		free(ctx->ct_ssn.ioc_local);		
	ctx->ct_ssn.ioc_server = (struct sockaddr*)saserver;
	ctx->ct_ssn.ioc_local = (struct sockaddr*)salocal;
	ctx->ct_ssn.ioc_lolen = salocal->snb_len;
	ctx->ct_ssn.ioc_svlen = saserver->snb_len;
	ctx->ct_flags |= SMBCF_RESOLVED;

WeAreDone:
	if (error) {
		if (sap)
			free(sap);		
		if (saserver)
			free(saserver);		
		if (salocal)
			free(salocal);		
		smb_log_info("can't resolve server address", error, ASL_LEVEL_DEBUG);		
	}
	return error;
}

#define SMB_BonjourServiceNameType "_smb._tcp."
/*
 * Given a DNS or Bonjour name find the address of the server and assign the correct port
 * to use.
 *
 * %%%
 * Currently we only support one address per name. In the future we should return a list
 * of address to try. This list should include AF_INET6 and AF_INET address. Since we only
 * support AF_INET in Leopard this shouldn't be much of an issue.
 */
static int smb_resolve_port(struct smb_ctx *ctx, u_int16_t port)
{
	struct sockaddr *sap = NULL;
	struct sockaddr_nb *saserver = NULL;
	struct sockaddr_nb *salocal = NULL;
	int error = 0;
	int allow_local_conn = (ctx->ct_flags & SMBCF_MOUNTSMBFS);
	CFNetServiceRef theService = _CFNetServiceCreateFromURL(NULL, ctx->ct_url);

	ctx->ct_flags &= ~SMBCF_RESOLVED;
	
	/* Must be doing a Bonjour service name lookup */
	if (theService) {
		CFStringRef serviceNameType = CFNetServiceGetType(theService);
		CFStringRef displayServiceName = CFNetServiceGetName(theService);
		CFStreamError debug_error = {(CFStreamErrorDomain)0, 0};
		CFArrayRef retAddresses = NULL;
		int32_t numAddresses = 0;
		struct sockaddr *sockAddr = NULL;
		int ii;
		
		/* Bonjour service name lookup, never fallback to NetBIOS name lookups. */
		ctx->ct_port_behavior = USE_THIS_PORT_ONLY;
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
		
		if (sap == NULL) /* Nothing founded */
			error = EADDRNOTAVAIL;
		if ((error == 0) && displayServiceName) {
			if (ctx->serverDisplayName)
				CFRelease(ctx->serverDisplayName);
			ctx->serverDisplayName = CFStringCreateCopy(kCFAllocatorDefault, displayServiceName);
		}
		CFRelease(theService);
	} else
		error = nb_resolvehost_in(ctx->ct_fullserver, &sap, port, allow_local_conn);
	
	if (error) 
		goto WeAreDone;

	/* If you don't allocate it big enough there are problems */
	error = nb_snballoc(NB_ENCNAMELEN+2, &salocal);
	if (error) 
		goto WeAreDone;
	
	/* If you don't allocate it big enough there are problems */
	error = nb_snballoc(NB_ENCNAMELEN+2, &saserver);
	if (error) 
		goto WeAreDone;

	memcpy(&saserver->snb_addrin, sap, ((struct sockaddr_in *)sap)->sin_len);
	if (ctx->ct_ssn.ioc_server)
		free(ctx->ct_ssn.ioc_server);		
	if (ctx->ct_ssn.ioc_local)
		free(ctx->ct_ssn.ioc_local);		
	ctx->ct_ssn.ioc_server = (struct sockaddr*)saserver; /* server sockaddr */
	ctx->ct_ssn.ioc_local = (struct sockaddr*)salocal; /* and local socakaddr */
	ctx->ct_ssn.ioc_lolen = salocal->snb_len;
	ctx->ct_ssn.ioc_svlen = saserver->snb_len;
	ctx->ct_flags |= SMBCF_RESOLVED;
	
WeAreDone:
	if (sap)
		free(sap);
	
	if (error) {
		if (saserver)
			free(saserver);		
		if (salocal)
			free(salocal);		
		smb_log_info("can't resolve server address", error, ASL_LEVEL_DEBUG);		
	}
	return error;
}

static int smb_resolve(struct smb_ctx *ctx)
{
	/* We already resolved it nothing else to do here */
	if (ctx->ct_flags & SMBCF_RESOLVED)
		return 0;
	if (ctx->ct_port == NBSS_TCP_PORT_139)
		return smb_resolve_port_139(ctx);
	else 
		return smb_resolve_port(ctx, ctx->ct_port);
}

int smb_connect(struct smb_ctx *ctx)
{
	int error;
	
	ctx->ct_flags &= ~SMBCF_CONNECTED;
	error = smb_resolve(ctx);
	if (!error)
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
	 * If we get an ELOOP then we found it on a local addres, so don't look for it on the other port.
	 */
	if (error && (error != ETIMEDOUT) && (error != ELOOP) && (ctx->ct_port_behavior == TRY_BOTH_PORTS)) {
		ctx->ct_flags &= ~SMBCF_RESOLVED;
		ctx->ct_port = NBSS_TCP_PORT_139;
		/* Port 445 failed never try it agian */
		ctx->ct_port_behavior = USE_THIS_PORT_ONLY;
		error = smb_resolve(ctx);
		if (!error)
			error = smb_negotiate(ctx);		
	} else	/* From now on use the same port */
		ctx->ct_port_behavior = USE_THIS_PORT_ONLY;
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
		CFDictionarySetValue (mutableDict, kMountedByGuestKey, kCFBooleanTrue);
		smb_log_info("%s: Session shared as Guest", 0, ASL_LEVEL_DEBUG, func);	
	} else {
		CFStringRef userNameRef = NULL;
		
		if (ctx->ct_ssn.ioc_user[0]) {
			userNameRef = CFStringCreateWithCString (NULL, ctx->ct_ssn.ioc_user, kCFStringEncodingUTF8);
			smb_log_info("%s: User session shared as %s", 0, ASL_LEVEL_DEBUG, func, ctx->ct_ssn.ioc_user);						
		}
		else if ((ctx->ct_vc_flags & SMBV_EXT_SEC) && (ctx->ct_ssn.ioc_kuser[0])) {
			userNameRef = CFStringCreateWithCString (NULL, ctx->ct_ssn.ioc_kuser, kCFStringEncodingUTF8);
			smb_log_info("%s: Kerberos session shared as %s", 0, ASL_LEVEL_DEBUG, func, ctx->ct_ssn.ioc_kuser);						
		}
		if (userNameRef != NULL) {
			CFDictionarySetValue (mutableDict, kMountedByUserKey, userNameRef);
			CFRelease (userNameRef);
		}
		
		/* %%% We should have a Kerberos flag to handle this, but thats another radar. */
		if (ctx->ct_vc_flags & SMBV_EXT_SEC) {
			CFDictionarySetValue (mutableDict, kMountedByKerberosKey, kCFBooleanTrue);
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
	NoUserPreferences = SMBGetDictBooleanValue(OpenOptions, kNoUserPreferences);

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
	CFDictionarySetValue (mutableDict, kSupportsChangePasswordKey, kCFBooleanFalse);
	/* We only support TCP/IP */
	CFDictionarySetValue (mutableDict, kSupportsTCPKey, kCFBooleanTrue);
	/* No support in SMB */
	CFDictionarySetValue (mutableDict, kSupportsSSHKey, kCFBooleanFalse);
	/* Most servers support guest, but not sure how we can tell if it is turned on yet */
	CFDictionarySetValue (mutableDict, kSupportsGuestKey, kCFBooleanTrue);
	CFDictionarySetValue (mutableDict, kGuestOnlyKey, kCFBooleanFalse);
	/* Now for the ones the server does return */
	if (ctx->ct_vc_caps & SMB_CAP_UNICODE)
		CFDictionarySetValue (mutableDict, kNeedsEncodingKey, kCFBooleanFalse);
	else 
		CFDictionarySetValue (mutableDict, kNeedsEncodingKey, kCFBooleanTrue);

	if (ctx->ct_vc_flags & SMBV_KERBEROS_SUPPORT) {
		CFStringRef kerbServerAddressRef = NULL;
		CFStringRef kerbServicePrincipalRef = NULL;
		CFMutableDictionaryRef KerberosInfoDict = NULL;

		/* Server supports Kerberos */
		CFDictionarySetValue (mutableDict, kSupportsKerberosKey, kCFBooleanTrue);
		/* The server gave us a Kerberos service principal name so create a CFString */
		if (ctx->ct_kerbPrincipalName != NULL)
			kerbServicePrincipalRef = CFStringCreateWithCString(NULL, ctx->ct_kerbPrincipalName, kCFStringEncodingUTF8);
		
		/* We are not using port 139 so ct_fullserver contains either a Bonjour/DNS name or IP address string */
		if ((ctx->ct_port != NBSS_TCP_PORT_139) && (ctx->ct_fullserver))
			kerbServerAddressRef = CFStringCreateWithCString(NULL, ctx->ct_fullserver, kCFStringEncodingUTF8);
		else {
			struct sockaddr_in	saddr = GET_IP_ADDRESS_FROM_NB((ctx->ct_ssn.ioc_server));
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
				CFDictionarySetValue (KerberosInfoDict, kServicePrincipalKey, kerbServicePrincipalRef);			
			CFRelease (kerbServicePrincipalRef);
		}
		/* Add the server host name to the Kerberos Info Dictionary */
		if (kerbServerAddressRef) {
			if (KerberosInfoDict)
				CFDictionarySetValue (KerberosInfoDict, kServerAddressKey, kerbServerAddressRef);						
			CFRelease (kerbServerAddressRef);
		}
		if (KerberosInfoDict) {
			CFDictionarySetValue (mutableDict, kKerberosInfoKey, KerberosInfoDict);			
			CFRelease (KerberosInfoDict);
		}
	}
	else
		CFDictionarySetValue (mutableDict, kSupportsKerberosKey, kCFBooleanFalse);
	
	/*
	 * In the furture we should only return what is in serverDisplayName, but because of time 
	 * constraints we will support both methods of returning the server's display name.
	 */
	if (ctx->serverDisplayName) {
		CFDictionarySetValue (mutableDict, kServerDisplayNameKey, ctx->serverDisplayName);
	} else if (ctx->ct_fullserver != NULL) {
		CFStringRef Server = CFStringCreateWithCString(NULL, ctx->ct_fullserver, kCFStringEncodingUTF8);
		if (Server != NULL) { 
			CFDictionarySetValue (mutableDict, kServerDisplayNameKey, Server);
			CFRelease (Server);
		}
	}
	if (ctx->ct_vc_shared) 
		smb_get_sessioninfo(ctx, mutableDict, __FUNCTION__);

	*ServerParams = mutableDict;
	return error;
}

int smb_open_session(struct smb_ctx *ctx, CFURLRef url, CFDictionaryRef OpenOptions, CFDictionaryRef *sessionInfo)
{
	int  error = 0;
	int	 NoUserPreferences = SMBGetDictBooleanValue(OpenOptions, kNoUserPreferences);
	int	UseKerberos = SMBGetDictBooleanValue(OpenOptions, kUseKerberosKey);
	int	UseGuest = SMBGetDictBooleanValue(OpenOptions, kUseGuestKey);
	int	UseAnonymous = SMBGetDictBooleanValue(OpenOptions, kUseAnonymousKey);
	int	ChangePassword = SMBGetDictBooleanValue(OpenOptions, kChangePasswordKey);
	char kerbServicePrincipal[1024];
	char kerbClientPrincipal[1024];
	char *servicepn = NULL;
	char *clientpn = NULL;

#ifdef DEBUG_FORCE_KERBEROS
	if (ctx->ct_vc_flags & SMBV_KERBEROS_SUPPORT) {
		smb_log_info("%s: Forcing Kerberos On!", 0, ASL_LEVEL_ERR, __FUNCTION__);		
		UseKerberos = TRUE;
	}
#endif // DEBUG_FORCE_KERBEROS
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
	if (SMBGetDictBooleanValue(OpenOptions, kForcePrivateSessionKey)) {
		ctx->ct_ssn.ioc_opt |= SMBV_PRIVATE_VC;
		ctx->ct_flags &= ~SMBCF_CONNECTED;		
		smb_log_info("%s: Force a new session!", 0, ASL_LEVEL_DEBUG, __FUNCTION__);
	} else {
		ctx->ct_ssn.ioc_opt &= ~SMBV_PRIVATE_VC;
		if (SMBGetDictBooleanValue(OpenOptions, kForceNewSessionKey))
			ctx->ct_flags &= ~SMBCF_CONNECTED;
	}

	/*
	 * %%% Need to be rewritten once we add NTLMSSP support!
	 * If we are not ask to do kerberos, then we need to turn off extended security. If we are already
	 * connected with extended security then we need to break the connection.
	 */
	if (!UseKerberos) {
		ctx->ct_ssn.ioc_opt &= ~SMBV_EXT_SEC;	/* Not doing kerberos */
		if (ctx->ct_vc_flags & SMBV_EXT_SEC)	/* %%% In the future we need a flag telling us we are sharing the vc */
			ctx->ct_flags &= ~SMBCF_CONNECTED;	/* Force a new connection */
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
	
	/* They want to use Kerberos, but the server doesn't support it */
	if (UseKerberos && ((ctx->ct_vc_flags & SMBV_KERBEROS_SUPPORT) != SMBV_KERBEROS_SUPPORT)) {
		smb_log_info("%s: Server doesn't support Kerberos, but you are requesting kerberos!", 0, ASL_LEVEL_ERR, __FUNCTION__);
		return EAUTH;
	}
	
	/* They don't want to use Kerberos, but the min auth level requires it */
	if (!UseKerberos && (ctx->ct_ssn.ioc_opt & SMBV_MINAUTH_KERBEROS)) {
		smb_log_info("%s:Kerberos required!", 0, ASL_LEVEL_ERR, __FUNCTION__);		
		return EAUTH;
	}
	
	/* Trying to mix security options, not allowed */
	if ((UseKerberos && UseGuest) || (UseKerberos && UseAnonymous) || (UseGuest && UseAnonymous))
		return EINVAL;
	
	/* %%% We currently do not support change password maybe someday */
	if (ChangePassword)
		return ENOTSUP;
	
	/* 
	 * We are sharing a session. Update the vc flags so we know the current
	 * state. We need to find out if this vc has already been authenticated.
	 */
	if (ctx->ct_vc_shared)
		smb_get_vc_flags(ctx);
	
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
		
		/* Force an anon connection why I am not sure */
		if (UseAnonymous)
			ctx->ct_ssn.ioc_user[0] = '\0';		
	}

	if (UseKerberos) {
		CFStringRef kerbClientPrincipalRef = NULL;
		CFStringRef kerbServicePrincipalRef = NULL;
		CFDictionaryRef KerberosInfoDict = CFDictionaryGetValue(OpenOptions, kKerberosInfoKey);
		
		if (KerberosInfoDict) {
			kerbClientPrincipalRef = CFDictionaryGetValue(KerberosInfoDict, kClientPrincipalKey);
			kerbServicePrincipalRef = CFDictionaryGetValue(KerberosInfoDict, kServicePrincipalKey);
		}
		if (kerbClientPrincipalRef) {
			CFStringGetCString(kerbClientPrincipalRef, kerbClientPrincipal, 1024, kCFStringEncodingUTF8);
			clientpn = kerbClientPrincipal;
		}
		if (kerbServicePrincipalRef) {
			CFStringGetCString(kerbServicePrincipalRef, kerbServicePrincipal, 1024, kCFStringEncodingUTF8);
			servicepn = kerbServicePrincipal;
		}
		smb_log_info("Kerberos Security: Client Principal Name %s Service Principal Name %s", 0, ASL_LEVEL_DEBUG, 
						 (clientpn) ? clientpn : "", (servicepn) ? servicepn : "");	
	} 
		
	error = smb_session_security(ctx, clientpn, servicepn);
	/* 
	 * We failed, so clear out any local security settings. We need to make
	 * sure we do not reuse these values in the next open session.
	 */
	if (error) {
		smb_ctx_setuser(ctx, "");
		smb_ctx_setpassword(ctx, "");
		smb_ctx_setdomain(ctx, "");
		ctx->ct_ssn.ioc_opt &= ~SMBV_GUEST_ACCESS;
	}
	
	/*
	 * %%%
	 * We currently use the flag SMBV_EXT_SEC to mean we are using Kerberos, this will break when we add NTLMSSP
	 * support, we need a flag that says we are doing Kerberos.
	 *
	 * We need to do something about the uppercasing of the username.
	 */
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
	if (ctx->ct_ssn.ioc_opt & SMBV_KERBEROS_SUPPORT) {
		CFDictionarySetValue (OpenOptions, kUseKerberosKey, kCFBooleanTrue);
	} else {
		CFDictionarySetValue (OpenOptions, kUseKerberosKey, kCFBooleanFalse);
		ctx->ct_ssn.ioc_opt &= ~SMBV_EXT_SEC;	
	}
	if (ctx->ct_ssn.ioc_opt & SMBV_GUEST_ACCESS)
		CFDictionarySetValue (OpenOptions, kUseGuestKey, kCFBooleanTrue);
	/* 
	 * Get ready to do the open session call again. We have no way of telling if we should read
	 * the users home directory preference file. We should have already read the preference files, but just to
	 * be safe we will not allow this connection to read the home directory preference file. Second we need to
	 * save the url off until after the call. The smb_open_session will free ct_url if it exist.
	 */
	CFDictionarySetValue (OpenOptions, kNoUserPreferences, kCFBooleanTrue);
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
				CFDictionarySetValue (currDict, kAlreadyMountedKey, kCFBooleanTrue);
			else 
				CFDictionarySetValue (currDict, kAlreadyMountedKey, kCFBooleanFalse);
		} else {
			CFDictionarySetValue (currDict, kPrinterShareKey, kCFBooleanTrue);				
			CFDictionarySetValue (currDict, kAlreadyMountedKey, kCFBooleanFalse);
		}
			
		/* If in share mode we could require a volume password, need to test this out and figure out what we should  do. */
		CFDictionarySetValue (currDict, kHasPasswordKey, kCFBooleanFalse);
		
		len = strlen(ep->netname);
		if (len && (ep->netname[len-1] == '$'))
			CFDictionarySetValue (currDict, kIsHiddenKey, kCFBooleanTrue);
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
	rpc_cleanup_smbctx(ctx);
	(void)smb_share_disconnect(ctx);
	smb_ctx_setshare(ctx, "", SMB_ST_ANY);
	if (fs)
		free(fs);
	if (share_info) {
		for (ep = share_info, ii = 0; ii < entries; ii++, ep++) {
			if (ep->netname)
				free(ep->netname);
			if (ep->remark)
				free(ep->remark);
		}
		free(share_info);
	}
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
	int	 ForcePrivateSession = SMBGetDictBooleanValue(mOptions, kForcePrivateSessionKey);
	struct smb_mount_args mdata;
	int error = 0;
	char mount_point[MAXPATHLEN];
	struct stat st;
	int mntflags;
	CFNumberRef numRef;

	
	/*  Initialize the mount arguments  */
	mdata.version = SMBFS_VERSION;
	mdata.altflags = ctx->altflags; /* Contains flags that were read from preference */
	mdata.path_len = 0;
	mdata.path[0] = 0;
	
	/* Get the alternative mount flags from the mount options */
	if (SMBGetDictBooleanValue(mOptions, kSoftMountKey))
		mdata.altflags |= SMBFS_MNT_SOFT;
	if (SMBGetDictBooleanValue(mOptions, kStreamstMountKey))
		mdata.altflags |= SMBFS_MNT_STREAMS_ON;
	
	/* Get the mount flags, just in case there are no flags or something is wrong we start with them set to zero. */
	mntflags = 0;
	if (mOptions) {
		numRef = (CFNumberRef)CFDictionaryGetValue (mOptions, kMountFlagsKey);
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
	/* Check for volume password, not sure its need any more, may want to remove in the future */
	if (mOptions) {
		CFDataRef volPwdRef = (CFDataRef)CFDictionaryGetValue (mOptions, kHasPasswordKey);
		if ((volPwdRef != NULL) && ((CFIndex)sizeof(ctx->ct_sh.ioc_password) > CFDataGetLength(volPwdRef)))
			CFDataGetBytes(volPwdRef, CFRangeMake(0, CFDataGetLength(volPwdRef)), (UInt8 *)ctx->ct_sh.ioc_password);
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
		if ((error = ENOTCONN) || (smb_ctx_connstate(ctx) == ENOTCONN)) {
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
	if (ctx->ct_ssn.ioc_server)
		create_unique_id(ctx, mdata.unique_id, &mdata.unique_id_len);
	else
		smb_log_info("%s: ioc_server is NULL how did that happen?", 0, ASL_LEVEL_DEBUG, __FUNCTION__);
	mdata.uid = ctx->ct_ssn.ioc_owner;
	mdata.gid = ctx->ct_ssn.ioc_group;
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
	if (ctx->ct_path) {
		mdata.path_len = strlen(ctx->ct_path);	/* Path length does not include the null byte */
		strlcpy(mdata.path, ctx->ct_path, MAXPATHLEN);
	}

	error = mount(SMBFS_VFSNAME, mount_point, mntflags, (void*)&mdata);
	if (error || (mInfo == NULL))
		goto WeAreDone;
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
		smb_log_info("%s: open session failed!", error, ASL_LEVEL_ERR, __FUNCTION__);
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
void *smb_create_ctx()
{
	struct smb_ctx *ctx = NULL;
	
	ctx = malloc(sizeof(struct smb_ctx));
	if (ctx == NULL)
		return NULL;
	if (pthread_mutex_init(&ctx->ctx_mutex, NULL) == -1) {
		smb_log_info("%s: pthread_mutex_init failed!", -1, ASL_LEVEL_DEBUG, __FUNCTION__);
		free(ctx);
		return NULL;
	}
	ctx->scheme = CFStringCreateWithCString (NULL, SMB_SCHEMA_STRING, kCFStringEncodingUTF8);
	if (ctx->scheme == NULL) {
		smb_log_info("%s: creating shema string failed!", -1, ASL_LEVEL_DEBUG, __FUNCTION__);
		pthread_mutex_destroy(&ctx->ctx_mutex);
		free(ctx);
		return NULL;
	}
	ctx->ct_url = NULL;
	ctx->ct_flags = 0;		/* We need to limit these flags */
	ctx->ct_fd = -1;
	ctx->ct_level = SMBL_VC;	/* Deafult to VC */
	ctx->ct_port_behavior = TRY_BOTH_PORTS;
	ctx->ct_port = SMB_TCP_PORT_445;
	ctx->ct_fullserver = NULL;
	ctx->serverDisplayName = NULL;
	ctx->ct_srvaddr = NULL;
	bzero(&ctx->ct_nb, sizeof(ctx->ct_nb));
	bzero(&ctx->ct_ssn, sizeof(ctx->ct_ssn));
	bzero(&ctx->ct_sh, sizeof(ctx->ct_sh));

	/* Default to use Extended security */
	ctx->ct_ssn.ioc_opt = SMBV_MINAUTH_NTLM | SMBV_EXT_SEC;
	ctx->ct_ssn.ioc_owner = geteuid();
	ctx->ct_ssn.ioc_group = getegid();
	ctx->ct_ssn.ioc_mode = SMBM_EXEC;
	ctx->ct_ssn.ioc_rights = SMBM_DEFAULT;
	ctx->ct_ssn.ioc_reconnect_wait_time = SMBM_RECONNECT_WAIT_TIME;
	
	ctx->ct_sh.ioc_owner = ctx->ct_ssn.ioc_owner;
	ctx->ct_sh.ioc_group = ctx->ct_ssn.ioc_group;
	ctx->ct_sh.ioc_mode = SMBM_EXEC;
	ctx->ct_sh.ioc_rights = SMBM_DEFAULT;
	
	/* Should this be setable? */
	strcpy(ctx->ct_ssn.ioc_localcs, "default");	
	
	ctx->ct_origshare = NULL;
	ctx->ct_path = NULL;
	ctx->ct_kerbPrincipalName = NULL;
	ctx->ct_kerbPrincipalName_len = 0;
	ctx->debug_level = 0;
	ctx->altflags = 0;
	ctx->ct_vc_caps = 0;
	/* 
	 * Not sure this is the place for this code yet. We should be getting the local NetBIOS
	 * name from the global config file. Should we use this as our default and replace it
	 * if there is one in the config file or should we check the config file first and only
	 * do this if it does not exist in the config file? 
	 *
	 * Get the host name, but only copy what will fit in the buffer. We do not care if the name 
	 * did not fit.  This will become less of a problem because of port
	 * 445 support which does not require a NetBIOS name.
	 */
	(void)nb_getlocalname(ctx->LocalNetBIOSName, (size_t)sizeof(ctx->LocalNetBIOSName));
	
	return ctx;
}

/*
 * This is used by the mount_smbfs and smbutil routine to create and initialize the
 * smb library context structure.
 */
int smb_ctx_init(struct smb_ctx **out_ctx, const char *url, int level, int sharetype, int NoUserPreferences)
{
	int  error = 0;
	struct smb_ctx *ctx = NULL;
	
	/* Create the structure and fill in the default values */
	ctx = smb_create_ctx();
	if (ctx == NULL) {
		error = errno;
		goto failed;		
	}
	ctx->ct_level = level;
	ctx->ct_flags |= SMBCF_MOUNTSMBFS;	/* Called from a command line utility */
	/* Create the CFURL */
	error = CreateSMBURL(ctx, url);
	if (error)
		goto failed;
	
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
	
	rpc_cleanup_smbctx(ctx);
	if (ctx->ct_fd != -1)
		close(ctx->ct_fd);
	nb_ctx_done(&ctx->ct_nb);
	if (ctx->scheme)
		CFRelease(ctx->scheme);
	if (ctx->ct_url)
		CFRelease(ctx->ct_url);
	if (ctx->ct_fullserver)
		free(ctx->ct_fullserver);
	if (ctx->serverDisplayName)
		CFRelease(ctx->serverDisplayName);
	if (ctx->ct_ssn.ioc_server)
		free(ctx->ct_ssn.ioc_server);
	if (ctx->ct_ssn.ioc_local)
		free(ctx->ct_ssn.ioc_local);
	if (ctx->ct_srvaddr)
		free(ctx->ct_srvaddr);
	if (ctx->ct_kerbPrincipalName)
		free(ctx->ct_kerbPrincipalName);
	if (ctx->ct_origshare)
		free(ctx->ct_origshare);
	if (ctx->ct_path)
		free(ctx->ct_path);
	pthread_mutex_unlock(&ctx->ctx_mutex);
	pthread_mutex_destroy(&ctx->ctx_mutex);
	free(ctx);
}
