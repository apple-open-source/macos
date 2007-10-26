/*
 * Copyright (c) 2000-2001 Boris Popov
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
#ifndef _NETSMB_DEV_H_
#define _NETSMB_DEV_H_

#ifndef _KERNEL
#include <sys/types.h>
#endif
#include <sys/ioccom.h>

#include <netsmb/smb.h>

#define	NSMB_NAME		"nsmb"
#define	NSMB_MAJOR		144

#define NSMB_VERMAJ	1
#define NSMB_VERMIN	4000
#define NSMB_VERSION	(NSMB_VERMAJ * 100000 + NSMB_VERMIN)

#define KERBEROS_REALM_DELIMITER '@'
#define WIN2008_SPN_PLEASE_IGNORE_REALM "cifs/not_defined_in_RFC4178@please_ignore"

/*
 * Once these structure are passed into the kernel the pointer fields may no longer have the same  
 * value on returned. Any user land routine will need to reset them before calling the kernel again.
 *
 */
struct smbioc_ossn {
	union {
		user_addr_t	ioc_kern_server;
		struct sockaddr	*ioc_server;
	};
	union {
		user_addr_t	ioc_kern_local;
		struct sockaddr	*ioc_local;
	};
	u_int32_t	ioc_opt;
	int32_t		ioc_svlen;	/* size of ioc_server address */
	int32_t		ioc_lolen;	/* size of ioc_local address */
	u_int32_t	ioc_reconnect_wait_time;	/* Amount of time to wait while reconnecting */
	uid_t		ioc_owner;	/* proposed owner */
	gid_t		ioc_group;	/* proposed group */
	mode_t		ioc_mode;	/* desired access mode */
	mode_t		ioc_rights;	/* SMBM_* */
	char		ioc_localcs[16] __attribute((aligned(8)));/* local charset */
	char		ioc_servercs[16];/* server charset */
	char		ioc_srvname[SMB_MAX_DNS_SRVNAMELEN+1]; /* make room for null */
	char		ioc_user[SMB_MAXUSERNAMELEN + 1];
	char		ioc_kuser[SMB_MAXUSERNAMELEN + 1];
	char		ioc_domain[SMB_MAXNetBIOSNAMELEN + 1];
	char		ioc_password[SMB_MAXPASSWORDLEN + 1];
};

struct smbioc_oshare {
	int32_t		ioc_opt;
	int32_t		ioc_stype;	/* share type */
	uid_t		ioc_owner;	/* proposed owner of share */
	gid_t		ioc_group;	/* proposed group of share */
	mode_t		ioc_mode;	/* desired access mode to share */
	mode_t		ioc_rights;	/* SMBM_* */
	char		ioc_share[SMB_MAXSHARENAMELEN + 1];
	char		ioc_password[SMB_MAXPASSWORDLEN + 1];
};

struct smbioc_rq {
	union {
		user_addr_t	ioc_kern_twords;
		void *		ioc_twords;
	};
	union {
		user_addr_t	ioc_kern_tbytes;
		void *		ioc_tbytes;
	};
	union {
		user_addr_t	ioc_kern_rpbuf;
		char *		ioc_rpbuf;
	};
	u_int8_t	ioc_cmd;
	u_int8_t	ioc_twc;
	u_int16_t	ioc_tbc;
	int32_t		ioc_rpbufsz;
	u_int8_t	ioc_rwc;
	u_int16_t	ioc_rbc;
	u_int8_t	ioc_errclass;
	u_int32_t	ioc_error;
	u_int16_t	ioc_serror;
};

struct smbioc_t2rq {
	union {
		user_addr_t	ioc_kern_name;
		char *		ioc_name;
	};
	union {
		user_addr_t	ioc_kern_tparam;
		void *		ioc_tparam;
	};
	union {
		user_addr_t	ioc_kern_tdata;
		void *		ioc_tdata;
	};
	union {
		user_addr_t	ioc_kern_rparam;
		void *		ioc_rparam;
	};
	union {
		user_addr_t	ioc_kern_rdata;
		void *		ioc_rdata;
	};
	u_int32_t	ioc_name_len;
	u_int16_t	ioc_setup[SMB_MAXSETUPWORDS];
	u_int16_t	ioc_setupcnt;
	u_int16_t	ioc_tparamcnt;
	u_int16_t	ioc_tdatacnt;
	u_int16_t	ioc_rparamcnt;
	u_int16_t	ioc_rdatacnt;
	u_int16_t	ioc_rpflags2;
	u_int8_t	ioc_errclass;
	u_int16_t	ioc_serror;
	u_int32_t	ioc_error;
};

struct smbioc_negotiate {
	u_int32_t			flags;		/* This is where the vc flags are returned */
	u_int32_t			vc_caps;	/* This is where the vc capabilities are returned */
	u_int32_t			spn_len;	/* Servers principal name length */
	u_int32_t			vc_conn_state;	/* On input the ports to try, on output set if we found a vc that is shared. */
	struct smbioc_ossn	ioc_ssn __attribute((aligned(8)));
	u_int8_t			spn[1025] __attribute((aligned(8)));
	u_int8_t			pad __attribute((aligned(8)));
};


struct smbioc_ssnsetup {
	union {
		user_addr_t	kern_clientpn;
		char		*user_clientpn;
	};
	union {
		user_addr_t	kern_servicepn;
		char		*user_servicepn;
	};
	u_int32_t		clientpn_len;
	u_int32_t		servicepn_len;
	struct smbioc_ossn	ioc_ssn;
};

struct smbioc_treeconn {
	struct smbioc_ossn		ioc_ssn;
	struct smbioc_oshare	ioc_sh __attribute((aligned(8)));
};

/* Always keep the pointers on 64 bit boundries*/
struct smbioc_rw {
	union {
		user_addr_t	ioc_kern_base;
		void		*ioc_base;
	};
	off_t		ioc_offset;
	int32_t		ioc_cnt;
	smbfh		ioc_fh;
	u_int8_t	pad __attribute((aligned(8)));
};

/*
 * Device IOCTLs
 */
#define	SMBIOC_REQUEST		_IOWR('n', 102, struct smbioc_rq)
#define	SMBIOC_T2RQ			_IOWR('n', 103, struct smbioc_t2rq)
#define	SMBIOC_READ			_IOWR('n', 107, struct smbioc_rw)
#define	SMBIOC_WRITE		_IOWR('n', 108, struct smbioc_rw)
#define	SMBIOC_NEGOTIATE	_IOWR('n',  109, struct smbioc_negotiate)
#define	SMBIOC_SSNSETUP		_IOW('n',  110, struct smbioc_ssnsetup)
#define	SMBIOC_TCON			_IOW('n',  111, struct smbioc_treeconn)
#define	SMBIOC_TDIS			_IOW('n',  112, struct smbioc_treeconn)
#define	SMBIOC_FLAGS2		_IOR('n',  113, u_int16_t)
#define	SMBIOC_SESSSTATE	_IOR('n',  114, u_int16_t)
#define	SMBIOC_CANCEL_SESSION	_IOR('n',  115, u_int16_t)
#define SMBIOC_GET_VC_FLAGS	_IOR('n', 116, u_int32_t)

#ifdef _KERNEL

STAILQ_HEAD(smbrqh, smb_rq);

struct smb_dev {
	struct smb_vc * sd_vc;		/* reference to VC */
	struct smb_share *sd_share;	/* reference to share if any */
	u_int32_t	sd_flags;
	void		*	sd_devfs;
};

struct smb_cred;
/*
 * Compound user interface
 */
int  smb_usr_negotiate(struct smbioc_negotiate *dp, struct smb_cred *scred, struct smb_vc **vcpp, struct smb_dev *sdp);
int  smb_usr_ssnsetup(struct smbioc_ssnsetup *dp, struct smb_cred *scred, struct smb_vc *vcp);
int  smb_usr_tcon(struct smbioc_treeconn *dp, struct smb_cred *scred, struct smb_vc *vcp, struct smb_share **sspp);
int  smb_usr_simplerequest(struct smb_share *ssp, struct smbioc_rq *data, struct smb_cred *scred);
int  smb_usr_t2request(struct smb_share *ssp, struct smbioc_t2rq *data, struct smb_cred *scred);
int  smb_dev2share(int fd, struct smb_share **sspp);


#endif /* _KERNEL */

#endif /* _NETSMB_DEV_H_ */
