/*
 * Copyright (c) 2000-2001 Boris Popov
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
#ifndef _NETSMB_DEV_H_
#define _NETSMB_DEV_H_

#ifndef _KERNEL
#include <sys/types.h>
#endif
#include <sys/ioccom.h>
#include <sys/unistd.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>

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

/* Negotiate Ioctl extra flags */
#define SMB_SHARING_SESSION      0x01	/* We are sharing this session */
#define SMB_FORCE_NEW_SESSION    0x02	/* Use a new session */
#define SMB_SMB1_ENABLED         0x04	/* SMB 1 Enabled */
#define SMB_SMB2_ENABLED         0x08	/* SMB 2 Enabled */
#define SMB_SIGNING_REQUIRED     0x10
#define SMB_SMB3_ENABLED         0x20	/* SMB 3 Enabled */
#define SMB_MATCH_DNS            0x40	/* Try to find mounted srvrs using dns */
#define SMB_SMB1_SIGNING_REQ     0x80	/* Signing required for SMB 1 */
#define SMB_SMB2_SIGNING_REQ    0x100	/* Signing required for SMB 2 */
#define SMB_SMB3_SIGNING_REQ    0x200   /* Signing required for SMB 3 */
#define SMB_HIFI_REQUESTED      0x400   /* HiFi mode is being requested */
#define SMB_MULTICHANNEL_ENABLE 0x800   /* Enable Multi-Channel SMB */
#define SMB_MC_PREFER_WIRED    0x1000   /* Prefer wired NICs in multichannel */
#define SMB_DISABLE_311        0x2000   /* Disable SMB v3.1.1 */

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
	uint32_t	ioc_ret_session_flags;
	int32_t		ioc_saddr_len;
	int32_t		ioc_laddr_len;
	uint32_t	ioc_ntstatus;
	uint32_t	ioc_errno;
    uuid_t      ioc_client_guid;    /* SMB 2/3 */
	SMB_IOC_POINTER(struct sockaddr *, saddr);
	SMB_IOC_POINTER(struct sockaddr *, laddr);
	uint32_t	ioc_userflags;		/* Authentication request flags */
	uint32_t	ioc_max_client_size; /* If share, then the size of client principal name */
	uint32_t	ioc_max_target_size; /* If share, then the size of server principal name */
	struct smbioc_ossn	ioc_ssn __attribute((aligned(8)));
	char ioc_user[SMB_MAXUSERNAMELEN + 1] __attribute((aligned(8)));
	uint32_t	ioc_negotiate_token_len __attribute((aligned(8)));   /* Server provided init token length */
	user_addr_t	ioc_negotiate_token __attribute((aligned(8))); /* Server provided init token */
	int32_t     ioc_max_resp_timeout;
    char        ioc_dns_name[255] __attribute((aligned(8)));
    struct sockaddr_storage ioc_shared_saddr;   /* Shared server's address */
	uint64_t	ioc_reserved __attribute((aligned(8))); /* Force correct size always */
    int32_t     ioc_mc_max_channel;
    int32_t     ioc_mc_max_rss_channel;
    uint32_t    ioc_mc_client_if_blacklist[kClientIfBlacklistMaxLen] __attribute((aligned(8)));
    uint32_t    ioc_mc_client_if_blacklist_len;

    void        *ioc_sessionp;
};

/*
 * Pushing the limit here on the structure size. We are about 1K under the
 * max size support for a ioctl call.
 */
struct smbioc_setup {
	uint32_t	ioc_version;
	uint32_t	ioc_userflags;	/* userable settable session_flags see SMBV_USER_LAND_MASK */
	uint32_t	ioc_gss_client_nt;   /* Name type of client principal */
	uint32_t	ioc_gss_client_size; /* Size of GSS principal name */
	user_addr_t	ioc_gss_client_name; /* principal name */
	uint32_t	ioc_gss_target_nt; /* Name type of target princial */
	uint32_t	ioc_gss_target_size;/* Size of GSS target name */
	user_addr_t	ioc_gss_target_name;/* Target name */
	char		ioc_user[SMB_MAXUSERNAMELEN + 1] __attribute((aligned(8)));
	char		ioc_password[SMB_MAXPASSWORDLEN + 1] __attribute((aligned(8)));
	char		ioc_domain[SMB_MAXNetBIOSNAMELEN + 1] __attribute((aligned(8)));
    char        ioc_dns_name[255] __attribute((aligned(8)));
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

struct smbioc_session_properties {
	uint32_t    ioc_version;
	uint32_t	ioc_reserved;
    uint32_t    uid;
    uint32_t    smb1_caps;
    uint32_t    smb2_caps;
	uint32_t	flags;
    uint64_t    misc_flags;
    uint32_t    hflags;
	uint32_t	hflags2;
	uint64_t	txmax;				
	uint64_t	rxmax;				
	uint64_t	wxmax;
    /* mc additions */
    struct timespec ioc_session_setup_time;
    uint64_t        ioc_total_rx_bytes;
    uint64_t        ioc_total_tx_bytes;
    uint32_t        ioc_session_reconnect_count;
    uint32_t        ioc_reserved0;
    struct timespec ioc_session_reconnect_time;

    char        model_info[SMB_MAXFNAMELEN * 2] __attribute((aligned(8)));

    char        snapshot_time[32] __attribute((aligned(8)));
};

struct adress_propreties {
    uint8_t  addr_family;
    union {
        char addr_ipv4[4];
        char addr_ipv6[16];
    };
};

#define MAX_NUM_OF_NICS 16
#define MAX_ADDRS_FOR_NIC 8
#define SERVER_NICS 0
#define CLIENT_NICS 1
struct smbioc_nic_info {
    uint32_t    ioc_version;
    uint32_t    ioc_reserved;
    uint8_t     flags;  // server or client
    uint32_t    num_of_nics;
    struct nic_properties {
        uint64_t if_index;
        uint32_t capabilities;
        uint32_t nic_type;
        uint64_t speed;
        uint8_t  ip_types;
        uint8_t  state;
        uint32_t num_of_addrs;
        struct adress_propreties addr_list[MAX_ADDRS_FOR_NIC];
    } nic_props[MAX_NUM_OF_NICS];
};

#define MAX_NUM_OF_IODS_IN_QUERY 64
struct smbioc_multichannel_properties {
    uint32_t    ioc_version;
    uint32_t    ioc_reserved;
    uint32_t    num_of_iod_properties;
    struct smbioc_iod_prop {
        uint32_t iod_prop_id;
        uint32_t iod_flags;
        uint64_t iod_prop_con_speed;
        uint64_t iod_prop_c_if;
        uint32_t iod_prop_c_if_type;
        uint64_t iod_prop_s_if;
        uint32_t iod_prop_s_if_caps;
        uint8_t  iod_prop_state;
        uint32_t iod_prop_con_port;
        uint64_t iod_prop_rx;
        uint64_t iod_prop_tx;
        struct timespec iod_prop_setup_time;
        struct adress_propreties  iod_prop_s_addr;
    } iod_properties[MAX_NUM_OF_IODS_IN_QUERY];
};

struct smbioc_list_snapshots {
    uint32_t    ioc_version;
    uint32_t    ioc_reserved;
    uint32_t    nmbr_snapshots;
    uint32_t    nmbr_snapshots_returned;
    uint32_t    snapshot_array_size;
    uint32_t    snapshot_buffer_len;
    SMB_IOC_POINTER(const char *, snapshot_buffer);
};

/*
 * Device IOCTLs
 */
#define	SMBIOC_AUTH_INFO		_IOWR('n', 101, struct smbioc_auth_info)
#define	SMBIOC_REQUEST			_IOWR('n', 102, struct smbioc_rq)
#define	SMBIOC_T2RQ				_IOWR('n', 103, struct smbioc_t2rq)
#define	SMBIOC_CONVERT_PATH		_IOWR('n', 104, struct smbioc_path_convert)
#define	SMBIOC_FIND_SESSION			_IOWR('n', 105, struct smbioc_negotiate)
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
#define SMBIOC_SESSION_PROPERTIES	_IOWR('n', 116, struct smbioc_session_properties)
#define	SMBIOC_GET_OS_LANMAN	_IOR('n', 117, struct smbioc_os_lanman)
#define SMBIOC_MULTICHANNEL_PROPERTIES    _IOWR('n', 131, struct smbioc_multichannel_properties)
#define SMBIOC_NIC_INFO         _IOWR('n', 132, struct smbioc_nic_info)
/* Rest of the IOCTLs are defined in smb_dev_2.h */

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
	lck_rw_t	sd_rwlock;		/* lock used to protect access to session and share */
	struct smb_session *sd_session;		/* reference to session */
	struct smb_share *sd_share;	/* reference to share if any */
	uint32_t	sd_flags;
	void		*sd_devfs;
};

/*
 * Compound user interface
 */
int smb_usr_set_network_identity(struct smb_session *sessionp, struct smbioc_ntwrk_identity *ntwrkID);
int smb_usr_negotiate(struct smbioc_negotiate *dp, vfs_context_t context, 
					  struct smb_dev *sdp, int searchOnly);
int smb_usr_simplerequest(struct smb_share *share, struct smbioc_rq *data, vfs_context_t context);
int smb_usr_t2request(struct smb_share *share, struct smbioc_t2rq *data, vfs_context_t context);
int smb_usr_convert_path_to_network(struct smb_session *session, struct smbioc_path_convert * dp);
int smb_usr_convert_network_to_path(struct smb_session *session, struct smbioc_path_convert * dp);
int smb_dev2share(int fd, struct smb_share **outShare);
int smb_usr_fsctl(struct smb_share *share, struct smbioc_fsctl * fsctl, vfs_context_t context);
int smb_cpdatain(struct mbchain *mbp, user_addr_t data, int len, 
                 vfs_context_t context);

#if 0
int smb_check_named_pipe(struct smb_share *share, char *pathp, size_t path_len);
#endif

#else /* _KERNEL */
/* Only used in user land */
int smb_ioctl_call(int /* ct_fd */, unsigned long /* cmd */, void */*info*/);

#endif /* _KERNEL */

#endif /* _NETSMB_DEV_H_ */
