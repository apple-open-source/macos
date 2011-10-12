/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2011 Apple Inc. All rights reserved.
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
#ifndef _NETSMB_SMB_LIB_H_
#define _NETSMB_SMB_LIB_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>
#include <sys/mount.h>

#include <netsmb/smb.h>
#include <netsmb/smb_dev.h>
#include <netsmb/netbios.h>
#include <asl.h>
#include "preference.h"

#define SMB_BonjourServiceNameType "_smb._tcp."
#define NetBIOS_SMBSERVER		"*SMBSERVER"

/* Used by mount_smbfs to pass mount option into the smb_mount call */
#define kNotifyOffMountKey	CFSTR("SMBNotifyOffMount")
#define kStreamstMountKey	CFSTR("SMBStreamsMount")
#define kdirModeKey			CFSTR("SMBDirModes")
#define kfileModeKey		CFSTR("SMBFileModes")

#define SMB_PASSWORD_KEY "Password"

struct connectAddress {
	int	so;
	union {
		struct sockaddr addr;
		struct sockaddr_in in4;
		struct sockaddr_in6 in6;
		struct sockaddr_nb nb;
		struct sockaddr_storage storage;	/* Make we always have enough room */
	};
};

/*
 * nb environment
 */
struct nb_ctx {
	struct sockaddr_in	nb_ns;			/* ip addr of name server */
	struct sockaddr_in	nb_sender;		/* address of the system that responded */
};

/*
 * SMB work context. Used to store all values which are necessary
 * to establish connection to an SMB server.
 */
struct smb_ctx {
	pthread_mutex_t ctx_mutex;
	CFURLRef		ct_url;
	uint32_t		ct_flags;	/* SMBCF_ */
	int				ct_fd;		/* handle of connection */
	uint16_t		ct_cancel;
	CFStringRef		serverNameRef; /* Display Server name obtain from URL or Bonjour Service Name */
	char *			serverName;		/* Server name obtain from the URL */
	struct nb_ctx		ct_nb;
	struct smbioc_ossn	ct_ssn;
	struct smbioc_setup ct_setup;
	struct smbioc_share	ct_sh;
	struct sockaddr	*ct_saddr;
	char *			ct_origshare;
	CFStringRef		mountPath;
	uint32_t		ct_vc_caps;		/* Obtained from the negotiate message */
	uint32_t		ct_vc_flags;	/* Obtained from the negotiate message */
	uint32_t		ct_vc_shared;	/* Obtained from the negotiate message, currently only tells if the vc is shared */
	uint64_t		ct_vc_txmax;				
	uint64_t		ct_vc_rxmax;				
	uint64_t		ct_vc_wxmax;				
	int				forceNewSession;
	int				inCallback;
	int				serverIsDomainController;
	CFDictionaryRef mechDict;
	struct smb_prefs prefs;
};

#define	SMBCF_RESOLVED			0x00000001	/* We have reolved the address and name */
#define	SMBCF_CONNECTED			0x00000002	/* The negoticate message was succesful */
#define	SMBCF_AUTHORIZED		0x00000004	/* We have completed the security phase */
#define	SMBCF_SHARE_CONN		0x00000008	/* We have a tree connection */
#define	SMBCF_READ_PREFS		0x00000010	/* We already read the preference */
#define SMBCF_RAW_NTLMSSP		0x00000040	/* Server only supports RAW NTLMSSP */
#define SMBCF_EXPLICITPWD		0x00010000	/* The password set by the url */

#define SMBCF_CONNECT_STATE	SMBCF_CONNECTED | SMBCF_AUTHORIZED | SMBCF_SHARE_CONN

__BEGIN_DECLS

struct sockaddr;

int smb_load_library(void);

#define SMB_BTMM_DOMAIN "com.apple.filesharing.smb.btmm-connection"

void LogToMessageTracer(const char *domain, const char *signature, 
						const char *optResult, const char *optValue,
						const char *fmt,...) __printflike(5, 6);
void smb_log_info(const char *, int,...) __printflike(1, 3);
void smb_ctx_hexdump(const char */* func */, const char */* comments */, unsigned char */* buf */, size_t /* inlen */);

/*
 * Context management
 */
CFMutableDictionaryRef CreateAuthDictionary(struct smb_ctx *ctx, uint32_t authFlags,
											const char * clientPrincipal, 
											uint32_t clientNameType);
int smb_ctx_clone(struct smb_ctx *new_ctx, struct smb_ctx *old_ctx,
				  CFMutableDictionaryRef openOptions);
int findMountPointVC(void *inRef, const char *mntPoint);
void *create_smb_ctx(void);
int  create_smb_ctx_with_url(struct smb_ctx **out_ctx, const char *url);
void smb_ctx_cancel_connection(struct smb_ctx *ctx);
void smb_ctx_done(void *);
int already_mounted(struct smb_ctx *ctx, char *uppercaseShareName, struct statfs *fs, 
					int fs_cnt, CFMutableDictionaryRef mdict, int requestMntFlags);

Boolean SMBGetDictBooleanValue(CFDictionaryRef Dict, const void * KeyValue, Boolean DefaultValue);

int smb_get_server_info(struct smb_ctx *ctx, CFURLRef url, CFDictionaryRef OpenOptions, CFDictionaryRef *ServerParams);
int smb_open_session(struct smb_ctx *ctx, CFURLRef url, CFDictionaryRef OpenOptions, CFDictionaryRef *sessionInfo);
int smb_mount(struct smb_ctx *in_ctx, CFStringRef mpoint, 
			  CFDictionaryRef mOptions, CFDictionaryRef *mInfo,
			  void (*)(void  *, void *), void *);

void smb_get_vc_properties(struct smb_ctx *ctx);
int smb_share_disconnect(struct smb_ctx *ctx);
int smb_share_connect(struct smb_ctx *ctx);
uint16_t smb_tree_conn_optional_support_flags(struct smb_ctx *ctx);
uint32_t smb_tree_conn_fstype(struct smb_ctx *ctx);
int  smb_ctx_setuser(struct smb_ctx *, const char *);
int  smb_ctx_setshare(struct smb_ctx *, const char *);
int  smb_ctx_setdomain(struct smb_ctx *, const char *);
int  smb_ctx_setpassword(struct smb_ctx *, const char *, int /*setFlags*/);

uint16_t smb_ctx_connstate(struct smb_ctx *ctx);
int  smb_smb_open_print_file(struct smb_ctx *, int, int, const char *, smbfh*);
int  smb_smb_close_print_file(struct smb_ctx *, smbfh);
int  smb_read(struct smb_ctx *, smbfh, off_t, uint32_t, char *);
int  smb_write(struct smb_ctx *, smbfh, off_t, uint32_t, const char *);
void smb_ctx_get_user_mount_info(const char * /*mntonname */, CFMutableDictionaryRef);

__END_DECLS

#endif /* _NETSMB_SMB_LIB_H_ */
