/*
 * Copyright (c) 2000-2001 Boris Popov
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
#ifndef _NETSMB_DEV_H_
#define _NETSMB_DEV_H_

#ifndef _KERNEL
#include <sys/types.h>
#endif
#include <sys/ioccom.h>

#include <netsmb/smb.h>

#define	NSMB_NAME		"nsmb"


#define KERBEROS_REALM_DELIMITER '@'
#define WIN2008_SPN_PLEASE_IGNORE_REALM "cifs/not_defined_in_RFC4178@please_ignore"

/*
 * Used to verify the userland and kernel are using the
 * correct structure. Only needs to be changed when the
 * structure in this routine are changed.
 */
#define SMB_IOC_STRUCT_VERSION		160

/*
 * The structure passed into the kernel must be less than or equal to 4K. If the
 * structure needs to be larger then we need to pass in a user_addr_t and do
 * a copyin on it.
 *
 */
#define SMB_MAX_IOC_SIZE 4 * 1024


#define TRY_BOTH_PORTS		1	/* Try port 445 -- if unsuccessful try 139 */
#define USE_THIS_PORT_ONLY	2	/* Try supplied port -- only */
#define SMB_SHARING_VC		4	/* We are sharing this Virtual Circuit */


struct smbioc_ossn {
	u_int32_t	ioc_opt;	/* Authentication request flags */
	u_int32_t	ioc_reconnect_wait_time;
	uid_t		ioc_owner;
	char		ioc_localcs[16] __attribute((aligned(8)));/* local charset */
	char		ioc_srvname[SMB_MAX_DNS_SRVNAMELEN+1] __attribute((aligned(8)));
	char		ioc_localname[SMB_MAXNetBIOSNAMELEN+1] __attribute((aligned(8)));
	char		ioc_kuser[SMB_MAXUSERNAMELEN + 1] __attribute((aligned(8)));
	char		ioc_kspn_hint[SMB_MAX_KERB_PN+1] __attribute((aligned(8)));
};

/* 
 * The SMBIOC_NEGOTIATE ioctl is a read/write ioctl. So we use smbioc_negotiate
 * structure to pass information from the user land to the kernel and vis-versa  
 */
struct smbioc_negotiate {
	u_int32_t	ioc_version;
	u_int32_t	ioc_extra_flags;
	u_int32_t	ioc_ret_caps;
	u_int32_t	ioc_ret_vc_flags;
	int32_t		ioc_saddr_len;
	int32_t		ioc_laddr_len;
	union {	/* Rosetta notes: User to Kernel (WRITE) */
		user_addr_t	ioc_kern_saddr __attribute((aligned(8)));
		struct sockaddr	*ioc_saddr __attribute((aligned(8)));
	};
	union {	/* Rosetta notes: User to Kernel (WRITE) */
		user_addr_t	ioc_kern_laddr __attribute((aligned(8)));
		struct sockaddr	*ioc_laddr __attribute((aligned(8)));
	};
	struct smbioc_ossn	ioc_ssn __attribute((aligned(8)));
	char ioc_user[SMB_MAXUSERNAMELEN + 1] __attribute((aligned(8)));
	u_int64_t	ioc_reserved __attribute((aligned(8))); /* Force correct size always */
};

/*
 * Pushing the limit here on the structure size. We are about 1K under the
 * max size support for a ioctl call.
 */
struct smbioc_setup {
	u_int32_t	ioc_version;
	int32_t		ioc_vcflags;	/* vc_flags used for security */
	char		ioc_user[SMB_MAXUSERNAMELEN + 1] __attribute((aligned(8)));
	char		ioc_uppercase_user[SMB_MAXUSERNAMELEN + 1] __attribute((aligned(8)));
	char		ioc_password[SMB_MAXPASSWORDLEN + 1] __attribute((aligned(8)));
	char		ioc_domain[SMB_MAXNetBIOSNAMELEN + 1] __attribute((aligned(8)));
	char		ioc_kclientpn[SMB_MAX_KERB_PN+1] __attribute((aligned(8)));
	char		ioc_kservicepn[SMB_MAX_KERB_PN+1] __attribute((aligned(8)));
	u_int64_t	ioc_reserved __attribute((aligned(8))); /* Force correct size always */
};

struct smbioc_share {
	u_int32_t	ioc_version;
	int32_t		ioc_stype;	/* share type */
	char		ioc_share[SMB_MAXSHARENAMELEN + 1] __attribute((aligned(8)));
	u_int64_t	ioc_reserved __attribute((aligned(8))); /* Force correct size always */
};

struct dos_error {
	u_int8_t	errclass;
	u_int8_t	err_reserved;
	u_int16_t	error;	
};

struct smbioc_rq {
	u_int32_t	ioc_version;
	u_int8_t	ioc_cmd;
	u_int8_t	ioc_twc;
	u_int16_t	ioc_tbc;	
	int32_t		ioc_rpbufsz;
	u_int16_t	ioc_rbc;
	u_int16_t	ioc_srflags2;
	u_int8_t	ioc_rwc;
	u_int8_t	ioc_pad[3];
	struct dos_error ioc_dos_error;
	u_int32_t	ioc_nt_error;
	union {	/* Rosetta notes: User to Kernel (WRITE) */
		user_addr_t	ioc_kern_twords __attribute((aligned(8)));
		void *		ioc_twords __attribute((aligned(8)));
	};
	union { /* Rosetta notes: User to Kernel (WRITE) */
		user_addr_t	ioc_kern_tbytes __attribute((aligned(8)));
		void *		ioc_tbytes __attribute((aligned(8)));
	};
	union {	 /* Rosetta notes: Kernel to User (READ) */
		user_addr_t	ioc_kern_rpbuf __attribute((aligned(8)));
		char *		ioc_rpbuf __attribute((aligned(8)));
	};
	u_int64_t	ioc_reserved __attribute((aligned(8))); /* Force correct size always */
};

struct smbioc_t2rq {
	u_int32_t	ioc_version;
	u_int32_t	ioc_name_len;
	u_int16_t	ioc_setupcnt;
	u_int16_t	ioc_tparamcnt;
	u_int16_t	ioc_tdatacnt;
	u_int16_t	ioc_rparamcnt;
	u_int16_t	ioc_rdatacnt;
	u_int16_t	ioc_srflags2;
	u_int16_t	ioc_setup[SMB_MAXSETUPWORDS];
	struct dos_error ioc_dos_error __attribute((aligned(8)));
	u_int32_t	ioc_nt_error __attribute((aligned(8)));
	union {	/* Rosetta notes: User to Kernel (WRITE) */
		user_addr_t	ioc_kern_name __attribute((aligned(8)));
		const char *ioc_name __attribute((aligned(8)));
	};
	union {	/* Rosetta notes: User to Kernel (WRITE) */
		user_addr_t	ioc_kern_tparam __attribute((aligned(8)));
		void *		ioc_tparam __attribute((aligned(8)));
	};
	union {	/* Rosetta notes: User to Kernel (WRITE) */
		user_addr_t	ioc_kern_tdata __attribute((aligned(8)));
		void *		ioc_tdata __attribute((aligned(8)));
	};
	union { /* Rosetta notes: Kernel to User (READ) */
		user_addr_t	ioc_kern_rparam __attribute((aligned(8)));
		void *		ioc_rparam __attribute((aligned(8)));
	};
	union { /* Rosetta notes: Kernel to User (READ) */
		user_addr_t	ioc_kern_rdata __attribute((aligned(8)));
		void *		ioc_rdata __attribute((aligned(8)));
	};
	u_int64_t	ioc_reserved __attribute((aligned(8))); /* Force correct size always */
};

/* Always keep the pointers on 64 bit boundries*/
struct smbioc_rw {
	u_int32_t	ioc_version;
	u_int32_t	ioc_cnt;
	off_t		ioc_offset;
	smbfh		ioc_fh;
	union {
		/* Rosetta notes: Kernel to User (READ) SMBIOC_READ, */
		/* Rosetta notes: User to Kernel (WRITE) SMBIOC_WRITE*/
		user_addr_t	ioc_kern_base __attribute((aligned(8)));
		void		*ioc_base __attribute((aligned(8)));
	};
	u_int64_t	ioc_reserved __attribute((aligned(8))); /* Force correct size always */
};

struct smbioc_os_lanman {
	char	NativeOS[SMB_MAX_NATIVE_OS_STRING];
	char	NativeLANManager[SMB_MAX_NATIVE_LANMAN_STRING];
};

/*
 * Device IOCTLs
 */
#ifdef SMB_ROSETTA
#define MIN_SMBIOC_COMMAND		102
#define MAX_SMBIOC_COMMAND		117
#define SMBIOC_COMMAND_COUNT	MAX_SMBIOC_COMMAND - MIN_SMBIOC_COMMAND + 1

#define SMBIOC_UNUSED_104		104 - MIN_SMBIOC_COMMAND
#define SMBIOC_UNUSED_105		105 - MIN_SMBIOC_COMMAND
#define SMBIOC_UNUSED_106		106 - MIN_SMBIOC_COMMAND
#define SMBIOC_UNUSED_108		108 - MIN_SMBIOC_COMMAND	/* NO TEST FOR SMBIOC_WRITE */
#endif // SMB_ROSETTA

#define	SMBIOC_REQUEST			_IOWR('n', 102, struct smbioc_rq)
#define	SMBIOC_T2RQ				_IOWR('n', 103, struct smbioc_t2rq)
#define	SMBIOC_READ				_IOWR('n', 107, struct smbioc_rw)
#define	SMBIOC_WRITE			_IOWR('n', 108, struct smbioc_rw)
#define	SMBIOC_NEGOTIATE		_IOWR('n', 109, struct smbioc_negotiate)
#define	SMBIOC_SSNSETUP			_IOW('n', 110, struct smbioc_setup)
#define	SMBIOC_TCON				_IOW('n', 111, struct smbioc_share)
#define	SMBIOC_TDIS				_IOW('n', 112, struct smbioc_share)
#define	SMBIOC_GET_VC_FLAGS2	_IOR('n', 113, u_int16_t)
#define	SMBIOC_SESSSTATE		_IOR('n', 114, u_int16_t)
#define	SMBIOC_CANCEL_SESSION	_IOR('n', 115, u_int16_t)
#define SMBIOC_GET_VC_FLAGS		_IOR('n', 116, u_int32_t)
#define	SMBIOC_GET_OS_LANMAN	_IOR('n', 117, struct smbioc_os_lanman)


/*
* Additional non-errno values that can be returned to NetFS, these get translate
* to the NetFS errors in user land.
*/
#define SMB_ENETFSACCOUNTRESTRICTED -5042
#define SMB_ENETFSPWDNEEDSCHANGE -5045
#define SMB_ENETFSPWDPOLICY -5046

#ifdef _KERNEL

STAILQ_HEAD(smbrqh, smb_rq);

struct smb_dev {
	struct smb_vc * sd_vc;		/* reference to VC */
	struct smb_share *sd_share;	/* reference to share if any */
	u_int32_t	sd_flags;
	void		*	sd_devfs;
};

/*
 * Compound user interface
 */
int  smb_usr_negotiate(struct smbioc_negotiate *dp, vfs_context_t context, struct smb_dev *sdp);
int  smb_usr_simplerequest(struct smb_share *ssp, struct smbioc_rq *data, vfs_context_t context);
int  smb_usr_t2request(struct smb_share *ssp, struct smbioc_t2rq *data, vfs_context_t context);
int  smb_dev2share(int fd, struct smb_share **sspp);


#else /* _KERNEL */
/* Only used in user land */
int smb_ioctl_call(int /* ct_fd */, unsigned long /* cmd */, void */*info*/);

#endif /* _KERNEL */

#endif /* _NETSMB_DEV_H_ */
