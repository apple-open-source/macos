/*
 * Copyright (c) 2000-2001 Boris Popov
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
#ifndef _NETSMB_DEV_H_
#define _NETSMB_DEV_H_

#ifndef _KERNEL
#include <sys/types.h>
#endif
#include <sys/ioccom.h>

#include <netsmb/smb.h>

#include <sys/kauth.h>

#define	NSMB_NAME		"nsmb"


#define KERBEROS_REALM_DELIMITER '@'
#define KERBEROS_INSTANCE_DELIMITER '/'
#define WINDOWS_DOMAIN_DELIMITER '\\'

/*
 * Used to verify the userland and kernel are using the
 * correct structure. Only needs to be changed when the
 * structure in this routine are changed.
 */
#define SMB_IOC_STRUCT_VERSION		170

/*
 * The structure passed into the kernel must be less than or equal to 4K. If the
 * structure needs to be larger then we need to pass in a user_addr_t and do
 * a copyin on it.
 *
 */
#define SMB_MAX_IOC_SIZE 4 * 1024

#define SMB_KERN_NTLMSSP		1	/* The kernel is doing NTLMSSP */
#define SMB_SHARING_VC			2	/* We are sharing this Virtual Circuit */
#define SMB_FORCE_NEW_SESSION	4	/* Use a new Virtual Circuit */

#define SMB_IOC_SPI_INIT_SIZE	8 * 1024 /* Inital buffer size for server provided init token */

/* Declare a pointer member of a ioctl structure. */
#define SMB_IOC_POINTER(TYPE, NAME) \
    union { \
	user_addr_t	ioc_kern_ ## NAME __attribute((aligned(8))); \
	TYPE		ioc_ ## NAME __attribute((aligned(8))); \
    }

struct smbioc_ossn {
	uint32_t	ioc_reconnect_wait_time;
	uid_t		ioc_owner;
	char		ioc_srvname[SMB_MAX_DNS_SRVNAMELEN+1] __attribute((aligned(8)));
	char		ioc_localname[SMB_MAXNetBIOSNAMELEN+1] __attribute((aligned(8)));
};

struct smbioc_auth_info {
	uint32_t	ioc_version;
	uint32_t	ioc_client_nt;   /* Name type of client principal */
	uint32_t	ioc_client_size; /* Size of GSS principal name */
	user_addr_t	ioc_client_name __attribute((aligned(8))); /* principal name */
	uint32_t	ioc_target_nt; /* Name type of target princial */
	uint32_t	ioc_target_size;/* Size of GSS target name */
	user_addr_t	ioc_target_name __attribute((aligned(8)));/* Target name */
	uint64_t	ioc_reserved __attribute((aligned(8))); /* Force correct size always */
};

/* 
 * The SMBIOC_NEGOTIATE ioctl is a read/write ioctl. So we use smbioc_negotiate
 * structure to pass information from the user land to the kernel and vis-versa  
 */
struct smbioc_negotiate {
	uint32_t	ioc_version;
	uint32_t	ioc_extra_flags;
	uint32_t	ioc_ret_caps;
	uint32_t	ioc_ret_vc_flags;
	int32_t		ioc_saddr_len;
	int32_t		ioc_laddr_len;
	uint32_t	ioc_ntstatus;
	uint32_t	ioc_errno;
	SMB_IOC_POINTER(struct sockaddr *, saddr);
	SMB_IOC_POINTER(struct sockaddr *, laddr);
	uint32_t	ioc_userflags;		/* Authentication request flags */
	uint32_t	ioc_max_client_size; /* If share, then the size of client principal name */
	uint32_t	ioc_max_target_size; /* If share, then the size of server principal name */
	struct smbioc_ossn	ioc_ssn __attribute((aligned(8)));
	char ioc_user[SMB_MAXUSERNAMELEN + 1] __attribute((aligned(8)));
	uint32_t	ioc_negotiate_token_len __attribute((aligned(8)));   /* Server provided init token length */
	user_addr_t	ioc_negotiate_token __attribute((aligned(8))); /* Server provided init token */
	uint64_t	ioc_reserved __attribute((aligned(8))); /* Force correct size always */
};

/*
 * Pushing the limit here on the structure size. We are about 1K under the
 * max size support for a ioctl call.
 */
struct smbioc_setup {
	uint32_t	ioc_version;
	uint32_t	ioc_userflags;	/* userable settable vc_flags see SMBV_USER_LAND_MASK */
	uint32_t	ioc_gss_client_nt;   /* Name type of client principal */
	uint32_t	ioc_gss_client_size; /* Size of GSS principal name */
	user_addr_t	ioc_gss_client_name; /* principal name */
	uint32_t	ioc_gss_target_nt; /* Name type of target princial */
	uint32_t	ioc_gss_target_size;/* Size of GSS target name */
	user_addr_t	ioc_gss_target_name;/* Target name */
	char		ioc_user[SMB_MAXUSERNAMELEN + 1] __attribute((aligned(8)));
	char		ioc_uppercase_user[SMB_MAXUSERNAMELEN + 1] __attribute((aligned(8)));
	char		ioc_password[SMB_MAXPASSWORDLEN + 1] __attribute((aligned(8)));
	char		ioc_domain[SMB_MAXNetBIOSNAMELEN + 1] __attribute((aligned(8)));
	uint64_t	ioc_reserved __attribute((aligned(8))); /* Force correct size always */
};

struct smbioc_share {
	uint32_t	ioc_version;
	uint32_t	ioc_optionalSupport;
	uint16_t	ioc_fstype;
	char		ioc_share[SMB_MAXSHARENAMELEN + 1] __attribute((aligned(8)));
	uint64_t	ioc_reserved __attribute((aligned(8))); /* Force correct size always */
};

struct smbioc_rq {
	uint32_t	ioc_version;
	uint8_t		ioc_cmd;
	uint8_t		ioc_twc;
	uint16_t	ioc_tbc;	
	int32_t		ioc_rpbufsz;
	uint32_t	ioc_ntstatus;
	uint32_t	ioc_errno;
	uint16_t	ioc_flags2;
	uint8_t     ioc_flags;
	uint8_t		ioc_padd;
	SMB_IOC_POINTER(void *, twords);
	SMB_IOC_POINTER(void *, tbytes);
	SMB_IOC_POINTER(char *, rpbuf);
	uint64_t	ioc_reserved __attribute((aligned(8))); /* Force correct size always */
};

struct smbioc_t2rq {
	uint32_t	ioc_version;
	uint32_t	ioc_name_len;
	uint16_t	ioc_setupcnt;
	uint16_t	ioc_tparamcnt;
	uint16_t	ioc_tdatacnt;
	uint16_t	ioc_rparamcnt;
	uint16_t	ioc_rdatacnt;
	uint16_t	ioc_flags2;
	uint16_t	ioc_setup[SMB_MAXSETUPWORDS];
	uint32_t	ioc_ntstatus __attribute((aligned(8)));
	SMB_IOC_POINTER(const char *, name);
	SMB_IOC_POINTER(void *, tparam);
	SMB_IOC_POINTER(void *, tdata);
	SMB_IOC_POINTER(void *, rparam);
	SMB_IOC_POINTER(void *, rdata);
	uint64_t	ioc_reserved __attribute((aligned(8))); /* Force correct size always */
};

struct smbioc_fsctl {
	uint32_t	ioc_version;			/* IOC version */
	smbfh		ioc_fh;					/* file handle */
	uint32_t	ioc_fsctl;				/* FSCTL code */
	uint32_t	ioc_tdatacnt;			/* transmit buffer size */
	uint32_t	ioc_rdatacnt;			/* receive buffer size */
	SMB_IOC_POINTER(void *, tdata);		/* transmit buffer */
	SMB_IOC_POINTER(void *, rdata);		/* receive buffer */

	uint32_t	ioc_errno;
	uint32_t	ioc_ntstatus;
};

/* Always keep the pointers on 64 bit boundries*/
struct smbioc_rw {
	uint32_t	ioc_version;
	uint32_t	ioc_cnt;
	off_t		ioc_offset;
	uint16_t	ioc_writeMode;
	smbfh		ioc_fh;
	SMB_IOC_POINTER(void *, base);
	uint64_t	ioc_reserved __attribute((aligned(8))); /* Force correct size always */
};

struct smbioc_os_lanman {
	char	NativeOS[SMB_MAX_NATIVE_OS_STRING];
	char	NativeLANManager[SMB_MAX_NATIVE_LANMAN_STRING];
};

#define NETWORK_TO_LOCAL	1
#define LOCAL_TO_NETWORK	2

struct smbioc_path_convert {
	uint32_t	ioc_version;
	uint32_t	ioc_direction;
	uint32_t	ioc_flags;
	uint32_t	ioc_src_len;
	uint64_t	ioc_dest_len;
	SMB_IOC_POINTER(const char *, src);
	SMB_IOC_POINTER(const char *, dest);
};

/*
 * Currently the network account and network domain are ignore, they are present
 * only for future considerations. 
 */ 
struct smbioc_ntwrk_identity {
	uint32_t	ioc_version;
	uint32_t	ioc_reserved;
	char		ioc_ntwrk_account[SMB_MAXUSERNAMELEN + 1] __attribute((aligned(8)));
	char		ioc_ntwrk_domain[SMB_MAX_DNS_SRVNAMELEN + 1] __attribute((aligned(8)));
	uint64_t	ioc_ntsid_len;	
	ntsid_t		ioc_ntsid;
};

struct smbioc_vc_properties {
	uint32_t	ioc_version;
	uint32_t	ioc_reserved;
	uint32_t	flags;				
	uint32_t	hflags2;				
	uint64_t	txmax;				
	uint64_t	rxmax;				
	uint64_t	wxmax;				
};

/*
 * Device IOCTLs
 */
#define	SMBIOC_AUTH_INFO		_IOWR('n', 101, struct smbioc_auth_info)
#define	SMBIOC_REQUEST			_IOWR('n', 102, struct smbioc_rq)
#define	SMBIOC_T2RQ				_IOWR('n', 103, struct smbioc_t2rq)
#define	SMBIOC_CONVERT_PATH		_IOWR('n', 104, struct smbioc_path_convert)
#define	SMBIOC_FIND_VC			_IOWR('n', 105, struct smbioc_negotiate)
#define SMBIOC_NTWRK_IDENTITY	_IOW('n', 106, struct smbioc_ntwrk_identity)
#define	SMBIOC_READ				_IOWR('n', 107, struct smbioc_rw)
#define	SMBIOC_WRITE			_IOWR('n', 108, struct smbioc_rw)
#define	SMBIOC_NEGOTIATE		_IOWR('n', 109, struct smbioc_negotiate)
#define	SMBIOC_SSNSETUP			_IOW('n', 110, struct smbioc_setup)
#define	SMBIOC_TCON				_IOWR('n', 111, struct smbioc_share)
#define	SMBIOC_TDIS				_IOW('n', 112, struct smbioc_share)
#define SMBIOC_FSCTL			_IOWR('n', 113, struct smbioc_fsctl)
#define	SMBIOC_SESSSTATE		_IOR('n', 114, uint16_t)
#define	SMBIOC_CANCEL_SESSION	_IOR('n', 115, uint16_t)
#define SMBIOC_VC_PROPERTIES	_IOWR('n', 116, struct smbioc_vc_properties)
#define	SMBIOC_GET_OS_LANMAN	_IOR('n', 117, struct smbioc_os_lanman)

/*
* Additional non-errno values that can be returned to NetFS, these get translate
* to the NetFS errors in user land.
*/
#define SMB_ENETFSACCOUNTRESTRICTED -5042
#define SMB_ENETFSPWDNEEDSCHANGE -5045
#define SMB_ENETFSPWDPOLICY -5046
#define SMB_ENETFSNOAUTHMECHSUPP -5997
#define SMB_ENETFSNOPROTOVERSSUPP -5996

#ifdef _KERNEL

STAILQ_HEAD(smbrqh, smb_rq);

struct smb_dev {
	lck_rw_t	sd_rwlock;		/* lock used to protect access to vc and share */
	struct smb_vc * sd_vc;		/* reference to VC */
	struct smb_share *sd_share;	/* reference to share if any */
	uint32_t	sd_flags;
	void		*	sd_devfs;
};

/*
 * Compound user interface
 */
int smb_usr_set_network_identity(struct smb_vc *vcp, struct smbioc_ntwrk_identity *ntwrkID);
int smb_usr_negotiate(struct smbioc_negotiate *dp, vfs_context_t context, 
					  struct smb_dev *sdp, int searchOnly);
int smb_usr_simplerequest(struct smb_share *share, struct smbioc_rq *data, vfs_context_t context);
int smb_usr_t2request(struct smb_share *share, struct smbioc_t2rq *data, vfs_context_t context);
int smb_usr_convert_path_to_network(struct smb_vc *vc, struct smbioc_path_convert * dp);
int smb_usr_convert_network_to_path(struct smb_vc *vc, struct smbioc_path_convert * dp);
int smb_dev2share(int fd, struct smb_share **outShare);
int smb_usr_fsctl(struct smb_share *share, struct smbioc_fsctl * fsctl, vfs_context_t context);


#else /* _KERNEL */
/* Only used in user land */
int smb_ioctl_call(int /* ct_fd */, unsigned long /* cmd */, void */*info*/);

#endif /* _KERNEL */

#endif /* _NETSMB_DEV_H_ */
