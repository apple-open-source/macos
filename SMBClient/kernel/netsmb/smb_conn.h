/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2015 Apple Inc. All rights reserved.
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
#ifndef _SMB_CONN_H_
#define _SMB_CONN_H_
#ifndef _NETINET_IN_H_
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#endif
#include <sys/kauth.h>

#ifdef _KERNEL
#include <gssd/gssd_mach_types.h>
#else
#include <Kernel/gssd/gssd_mach_types.h>
#endif

#include <libkern/OSTypes.h>

/*
 * Two levels of connection hierarchy
 */
#define	SMBL_SESSION_LIST	0x0000
#define SMBL_SESSION		0x0001
#define SMBL_SHARE	0x0002
#define SMBL_VALID	0x0003

/*
 * Device flags
 */

#define NSMBFL_OPEN		0x0001
#define NSMBFL_CANCEL	0x0002
#define NSMBFL_SHARE_SESSION	0x0004

/*
 * Common object flags
 *
 * Remember that session_flags and ss_flags are defined to be co_flags,
 * so those flags can never reuse this value. 
 */
#define SMBO_GONE		0x80000000

/*
 * access modes
 */
#define	SMBM_READ		0400	/* read conn attrs.(like list shares) */
#define	SMBM_WRITE		0200	/* modify conn attrs */
#define	SMBM_EXEC		0100	/* can send SMB requests */
#define	SMBM_READGRP		0040
#define	SMBM_WRITEGRP		0020
#define	SMBM_EXECGRP		0010
#define	SMBM_READOTH		0004
#define	SMBM_WRITEOTH		0002
#define	SMBM_EXECOTH		0001
#define	SMBM_MASK		0777
#define	SMBM_ALL		(SMBM_READ | SMBM_WRITE | SMBM_EXEC)
#define	SMBM_DEFAULT		(SMBM_READ | SMBM_WRITE | SMBM_EXEC)

#define SMBM_RECONNECT_WAIT_TIME	60 * 10	/* Wait 10 minutes for reconnect? */

/*
 * OS Strings - Currently we only check for Windows 2000, XP and Dawin because
 * we want to treat them special.
 */
#define WIN2K_XP_UTF8_NAME "Windows 5."
#define DARWIN_UTF8_NAME "Darwin"

/*
 * Session flags
 */
#define SMBV_USER_SECURITY          SMB_SM_USER		/* 0x01, server in the user security mode */
#define SMBV_ENCRYPT_PASSWORD       SMB_SM_ENCRYPT	/* 0x02 use challenge/response for password */
#define SMBV_SIGNING                SMB_SM_SIGS		/* 0x04 server does SMB Signing */
#define SMBV_SIGNING_REQUIRED       SMB_SM_SIGS_REQ	/* 0x08 server requires SMB Signing */
#define SMBV_SECURITY_MODE_MASK     0x000000ff		/* Lower byte reserved for the security modes */

#define	SMBV_NT4                    0x00000100		/* Tells us the server is a NT4 */
#define	SMBV_WIN2K_XP               0x00000200		/* Tells us the server is Windows 2000 or XP */
#define SMBV_DARWIN                 0x00000400		/* Mac OS X Server */
#define SMBV_SMB30                  0x00000800		/* Using SMB 3.0 */
#define SMBV_SMB2                   0x00001000		/* Using some version of SMB 2 or 3 */
#define SMBV_SMB2002                0x00002000		/* Using SMB 2.002 */
#define SMBV_SMB21                  0x00004000		/* Using SMB 2.1 */
#define SMBV_SMB302                 0x00008000		/* Using SMB 3.02 */
#define SMBV_SERVER_MODE_MASK       0x0000ff00		/* This nible is reserved for special server types */

#define SMBV_NETWORK_SID            0x00010000		/* The user's sid has been set on the session */
#define	SMBV_AUTH_DONE              0x00080000		/* Security compeleted successfully */
#define SMBV_PRIV_GUEST_ACCESS      0x00100000		/* Guest access is private */
#define SMBV_KERBEROS_ACCESS        0x00200000		/* This session is using Kerberos */
#define SMBV_GUEST_ACCESS           0x00400000		/* user is using guess security */
#define SMBV_ANONYMOUS_ACCESS       0x00800000		/* user is using anonymous security */
#define SMBV_HOME_ACCESS_OK         0x01000000		/* <Currently unused> can touch the user home directory */
#define SMBV_RAW_NTLMSSP            0x02000000		/* server only supports RAW NTLM, no NTLMSSP */
#define SMBV_NO_NTLMV1              0x04000000		/* NTLMv1 not allowed in non Extended Security case */
#define SMBV_USER_LAND_MASK         0x07f00000		/* items that are changable by the user */
#define SMBV_SFS_ACCESS             0x08000000		/* Server is using simple file sharing. All access is forced to guest This is a kernel only flag */
#define SMBV_MULTICHANNEL_ON        0x10000000      /* Multichannel is enabled for this session */
#define	SMBV_GONE                   SMBO_GONE		/* 0x80000000 - Reserved see above for more details */

/*
 * session_misc_flags - another flags field since session_flags is almost full.
 */
#define	SMBV_NEG_SMB1_ENABLED			0x00000001	/* Allow SMB 1 */
#define	SMBV_NEG_SMB2_ENABLED			0x00000002	/* Allow SMB 2 */
#define	SMBV_64K_QUERY_DIR				0x00000004	/* Use 64Kb OutputBufLen in Query_Dir */
#define	SMBV_HAS_FILEIDS				0x00000010	/* Has File IDs that we can use for hash values and inode number */
#define	SMBV_NO_QUERYINFO				0x00000020	/* Server does not like Query Info for FileAllInformation */
#define	SMBV_OSX_SERVER					0x00000040	/* Server is OS X based */
#define	SMBV_OTHER_SERVER				0x00000080	/* Server is not OS X based */
#define SMBV_CLIENT_SIGNING_REQUIRED	0x00000100
#define SMBV_NON_COMPOUND_REPLIES       0x00000200	/* Server does not send compound replies */
#define SMBV_63K_IOCTL					0x00000400	/* Use 63K MaxOutputResponse */
#define SMBV_HAS_COPYCHUNK				0x00000800	/* Server supports FSCTL_SRV_COPY_CHUNK IOCTL */
#define	SMBV_NEG_SMB3_ENABLED			0x00001000	/* Allow SMB 3 */
#define	SMBV_NO_WRITE_THRU				0x00002000	/* Server does not like Write Through */
#define SMBV_SMB1_SIGNING_REQUIRED		0x00004000
#define SMBV_SMB2_SIGNING_REQUIRED		0x00008000
#define SMBV_SMB3_SIGNING_REQUIRED		0x00010000
#define SMBV_MNT_TIME_MACHINE			0x00020000	/* Time Machine session */
#define SMBV_HAS_DUR_HNDL_V2			0x00040000	/* Server supports Durable Handle V2 */
#define SMBV_NO_DUR_HNDL_V2				0x00080000	/* Server does not support Durable Handle V2 */
#define SMBV_MNT_HIGH_FIDELITY          0x00100000  /* High Fidelity session */
#define SMBV_MNT_DATACACHE_OFF          0x00200000  /* Disable data caching */
#define SMBV_MNT_MDATACACHE_OFF         0x00400000  /* Disable meta data caching */

#define SMBV_HAS_GUEST_ACCESS(sessionp)		(((sessionp)->session_flags & (SMBV_GUEST_ACCESS | SMBV_SFS_ACCESS)) != 0)
#define SMBV_HAS_ANONYMOUS_ACCESS(sessionp)	(((sessionp)->session_flags & (SMBV_ANONYMOUS_ACCESS | SMBV_SFS_ACCESS)) != 0)

/*
 * True if dialect is SMB 2.1 or later (i.e., SMB 2.1, SMB 3.0, SMB 3.1, SMB 3.02, ...)
 * Important: Remember to update this when adding new dialects.
 */
#define SMBV_SMB21_OR_LATER(sessionp) (((sessionp)->session_flags & (SMBV_SMB21 | SMBV_SMB30 | SMBV_SMB302)) != 0)

#define SMBV_SMB3_OR_LATER(sessionp) (((sessionp)->session_flags & (SMBV_SMB30 | SMBV_SMB302)) != 0)

#define kSMB_64K 65536      /* For the QueryDir and QueryInfo limits */
#define kSMB_63K 65534      /* <14281932> Max Net App can handle in IOCTL */
#define kSMB_MAX_TX 1048576 /* 1 MB max transaction size to match Win Clients */

/*
 * smb_share flags
 */
#define SMBS_PERMANENT		0x0001
#define SMBS_RECONNECTING	0x0002
#define SMBS_CONNECTED		0x0004
#define SMBS_GOING_AWAY		0x0008
#define	SMBS_GONE			SMBO_GONE		/* 0x80000000 - Reserved see above for more details */

/*
 * Negotiated protocol parameters
 */
struct smb_sopt {
    uint32_t    sv_maxtx;           /* maximum transmit buf size */
    uint16_t    sv_maxmux;          /* SMB 1 - max number of outstanding rq's */
    uint16_t    sv_maxsessions;     /* SMB 1 - max number of sessions */
    uint32_t    sv_skey;            /* session key */
    uint32_t    sv_caps;            /* SMB 1 - capabilities, preset for SMB 2/3 */
    uint32_t    sv_sessflags;       /* SMB 2/3 - final session setup reply flags */
    uint16_t    sv_dialect;         /* SMB 2 - dialect (non zero for SMB 2/3 */
    uint32_t    sv_capabilities;    /* SMB 2 - capabilities */
    uint32_t    sv_maxtransact;     /* SMB 2 - max transact size */
    uint32_t    sv_maxread;         /* SMB 2 - max read size */
    uint32_t    sv_maxwrite;        /* SMB 2 - max write size */
    uint8_t     sv_guid[16];        /* SMB 2 - GUID */
    uint16_t    sv_security_mode;   /* SMB 2 - security mode */
};

/*
 * network IO daemon states
 */
enum smbiod_state {
	SMBIOD_ST_NOTCONN,        /* no connect request was made */
	SMBIOD_ST_CONNECT,        /* a connect attempt is in progress */
	SMBIOD_ST_TRANACTIVE,     /* TCP transport level is connected */
	SMBIOD_ST_NEGOACTIVE,     /* completed negotiation */
	SMBIOD_ST_SSNSETUP,       /* started (a) session setup */
	SMBIOD_ST_SESSION_ACTIVE, /* SMB session established */
	SMBIOD_ST_DEAD,           /* connection broken, transport is down */
    SMBIOD_ST_RECONNECT_AGAIN,/* We need to attempt to reconnect again */
    SMBIOD_ST_RECONNECT       /* Currently in reconnect */
};

/*
 * File system types ss_fstype
 */
enum smb_fs_types { 
	SMB_FS_FAT = 0,				/* Fat file system */
	SMB_FS_CDFS = 1,			/* CD file system */
	SMB_FS_UDF = 2,				/* DVD file system */
	SMB_FS_NTFS_UNKNOWN = 3,	/* NTFS file system, sometimes faked by server no streams support */
	SMB_FS_NTFS = 4,			/* Real NTFS or fully pretending, NTFS share that also supports STREAMS. */
	SMB_FS_NTFS_UNIX = 5,		/* Pretending to be NTFS file system, no streams, but it is a UNIX system */
	SMB_FS_MAC_OS_X = 6			/* Mac OS X Server, SMB 2/3 or greater */
};

#ifdef _KERNEL

#include <sys/lock.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_fid.h>
#include <netsmb/smb2_mc.h>

struct smbioc_negotiate;
struct smbioc_setup;
struct smbioc_share;

TAILQ_HEAD(smb_rqhead, smb_rq);
TAILQ_HEAD(smb_lease_head, lease_rq);

#define SMB_NBTIMO	15
#define SMB_DEFRQTIMO	30	/* 30 for oplock revoke/writeback */
#define SMBWRTTIMO	60
#define SMBSSNSETUPTIMO	60
#define SMBNOREPLYWAIT (0)

struct smb_tran_desc;
struct smb_dev;

/*
 * Connection object
 */
struct smb_connobj;

typedef void smb_co_gone_t (struct smb_connobj *cp, vfs_context_t context);
typedef void smb_co_free_t (struct smb_connobj *cp);

struct smb_connobj {
	int         co_level;	/* SMBL_ */
	uint32_t    co_flags;
	lck_mtx_t   *co_lock;
	void        *co_lockowner;
	int32_t     co_lockcount;
	uint32_t    co_lock_flags;
	lck_mtx_t   co_interlock;
	int         co_usecount;
	struct smb_connobj *co_parent;
	SLIST_HEAD(,smb_connobj)co_children;
	SLIST_ENTRY(smb_connobj)co_next;
	smb_co_gone_t *co_gone;
	smb_co_free_t *co_free;
};

#define SMBFS_CO_LOCK_WAIT 1

#define SMBLIST_NEXT(var, field)    (typeof(var))(((struct smb_connobj*)var)->field.sle_next)
#define SMBLIST_FIRST(head, var)    (typeof (var))(SLIST_FIRST(head))

#define SMBLIST_FOREACH_SAFE(var, head, field, tvar)   \
for((var) = SMBLIST_FIRST(head, var);                  \
(var) &&  ((tvar) = SMBLIST_NEXT((var), field), 1);    \
(var) = (tvar))

#define SMBCO_FOREACH_SAFE(var, cp, tvar)    SMBLIST_FOREACH_SAFE((var), &(cp)->co_children, co_next, tvar)

/*
 * Data structure to access gssd for the use of SPNEGO/Kerberos
 */
struct smb_gss {
	mach_port_t	gss_mp;			/* Mach port to gssd */
	au_asid_t	gss_asid;		/* Audit session id to find gss_mp */
	gssd_nametype	gss_target_nt;		/* Service's principal's name type */
	uint32_t	gss_spn_len;		/* Service's principal's length */
	uint8_t *	gss_spn;		/* Service's principal name */
	gssd_nametype	gss_client_nt;		/* Client's principal's name type */
	uint32_t	gss_cpn_len;		/* Client's principal's length */
	uint8_t *	gss_cpn;		/* Client's principal name */
	char *		gss_cpn_display;	/* String representation of client principal */
	uint32_t	gss_tokenlen;		/* Gss token length */
	uint8_t *	gss_token;		/* Gss token */
	uint64_t	gss_ctx;		/* GSS opaque context handle */
	uint64_t	gss_cred;		/* GSS opaque cred handle */
	uint32_t	gss_rflags;		/* Flags returned from gssd */
	uint32_t	gss_major;		/* GSS major error code */
	uint32_t	gss_minor;		/* GSS minor (mech) error code */
	uint32_t	gss_smb_error;		/* Last error returned by smb SetUpAndX */
};

struct session_network_interface_info {
    lck_mtx_t interface_table_lck;

    uint32_t client_nic_count;
    struct interface_info_list client_nic_info_list; /* list of the client's available NICs */
    uint32_t server_nic_count;
    struct interface_info_list server_nic_info_list; /* list of the server's available NICs */

    /*
     * Table of all possible connections represent the state of every
     * couple of NICs.
     * The size of the table is (client_nic_count * server_nic_count).
     * In case of a successful connection will note the functionality of the connection.
     */
    uint64_t current_max_con_speed;  /* the max existing connection speed */
    struct connection_info_list session_con_list;    /* list of all possible connection - use next */
    uint8_t active_on_trial_connections;             /* record for the amount of open trial connections */
    struct connection_info_list successful_con_list; /* list of all successful connection - use success_next */
};

/*
 * Session to a server.
 * This is the most (over)complicated part of SMB protocol.
 * For the user security level (usl), each session with different remote
 * user name has its own session.
 * It is unclear however, should share security level (ssl) allow additional
 * sessions, because user name is not used and can be the same. On other hand,
 * multiple sessions allows us to create separate sessions to server on a per
 * user basis.
 */

/*
 * This lock protects session_flags
 */
#define	SMBC_ST_LOCK(sessionp)	lck_mtx_lock(&(sessionp)->session_stlock)
#define	SMBC_ST_UNLOCK(sessionp)	lck_mtx_unlock(&(sessionp)->session_stlock)

/*
 * This lock protects session_credits_ fields
 */
#define SMBC_CREDIT_LOCKPTR(sessionp)  (&(sessionp)->session_credits_lock)
#define	SMBC_CREDIT_LOCK(sessionp)	lck_mtx_lock(&(sessionp)->session_credits_lock)
#define	SMBC_CREDIT_UNLOCK(sessionp)	lck_mtx_unlock(&(sessionp)->session_credits_lock)

/* SMB3 Signing/Encrypt Key Length */
#define SMB3_KEY_LEN 16

/* Max number of SMB Dialects we currently support */
#define SMB3_MAX_DIALECTS 4

struct smb_session {
	struct smb_connobj	obj;
	char				*session_srvname;		/* The server name used for tree connect, also used for logging */
	char				*session_localname;
	char				ipv4v6DotName[45+1];    /* max IPv6 presentation len */
	struct sockaddr		*session_saddr;			/* server addr */
	struct sockaddr		*session_laddr;			/* local addr, if any, only used for port 139 */
	char				*session_username;
	char				*session_pass;			/* password for usl case */
	char				*session_domain;		/* workgroup/primary domain */
	int32_t				session_volume_cnt;
	unsigned			session_timo;			/* default request timeout */
	int32_t				session_number;			/* number of this session from the client side */
	uid_t				session_uid;			/* user id of connection */
	u_short				session_smbuid;			/* unique session id assigned by server */
	u_char				session_hflags;			/* or'ed with flags in the smb header */
	u_short				session_hflags2;		/* or'ed with flags in the smb header */
	void				*session_tdata;			/* transport control block */
	struct smb_tran_desc *session_tdesc;
	int					session_chlen;			/* actual challenge length */
	u_char				session_ch[SMB_MAXCHALLENGELEN];
	uint16_t			session_mid;			/* multiplex id */
	uint16_t			session_low_pid;		/* used for async requests only */
#if 0
    /* Message ID and credit checking debugging code */
    lck_mtx_t           session_mid_lock;
    uint64_t            session_expected_mid;
    uint64_t            session_last_sent_mid;
    uint16_t            session_last_credit_charge;
    uint16_t            session_last_command;
#endif
	uint64_t            session_message_id;      /* SMB 2/3 request message id */
	uint32_t            session_credits_granted; /* SMB 2/3 credits granted */
	uint32_t            session_credits_ss_granted; /* SMB 2/3 credits granted from session setup replies */
	uint32_t            session_credits_max;     /* SMB 2/3 max amount of credits server has granted us */
	int32_t             session_credits_wait;    /* SMB 2/3 credit wait */
    uint32_t            session_req_pending;     /* SMB 2/3 set if there is a pending request */
    uint64_t            session_oldest_message_id; /* SMB 2/3 oldest pending request message id */
	lck_mtx_t			session_credits_lock;
	uint64_t            session_session_id;      /* SMB 2/3 session id */
	uint64_t            session_prev_session_id; /* SMB 2/3 prev sessID for reconnect */
	uint64_t            session_misc_flags;      /* SMB 2/3 misc flags */
	struct smb_sopt		session_sopt;            /* server options */
	uint32_t			session_txmax;           /* max tx/rx packet size */
	uint32_t			session_rxmax;           /* max readx data size */
	uint32_t			session_wxmax;           /* max writex data size */
	struct smbiod		*session_iod;
	lck_mtx_t			session_stlock;
	uint32_t			session_seqno;           /* my next sequence number */
	uint8_t				*session_mackey;         /* MAC key */
	uint32_t			session_mackeylen;       /* length of MAC key */

    /* Adaptive Read/Write values */
    lck_mtx_t           iod_quantum_lock;
    uint32_t            iod_readSizes[3];           /* [0] = min, [1] = med, [2] = max */
    uint32_t            iod_writeSizes[3];          /* [0] = min, [1] = med, [2] = max */
    uint64_t            iod_readBytePerSec[3];      /* [0] = min, [1] = med, [2] = max */
    uint64_t            iod_writeBytePerSec[3];     /* [0] = min, [1] = med, [2] = max */
    struct timeval      iod_last_recheck_time;      /* Last time we checked speeds */

    uint32_t            iod_readQuantumSize;        /* current read quantum size */
    uint32_t            iod_readQuantumNumber;      /* current read quantum number */
    uint32_t            iod_writeQuantumSize;       /* current write quantum size */
    uint32_t            iod_writeQuantumNumber;     /* current write quantum number */

    /* SMB 3 signing key (Session.SessionKey) */
    uint8_t             session_smb3_signing_key[SMB3_KEY_LEN];
    uint32_t            session_smb3_signing_key_len;
    
    /* SMB 3 encryption key (Session.EncryptionKey) */
    /* A 128-bit key used for encrypting messages sent by the client */
    uint8_t             session_smb3_encrypt_key[SMB3_KEY_LEN];
    uint32_t            session_smb3_encrypt_key_len;
    
    /* SMB 3 decryption key (Session.DecryptionKey) */
    /* A 128-bit key used for decrypting messages received from the server. */
    uint8_t             session_smb3_decrypt_key[SMB3_KEY_LEN];
    uint32_t            session_smb3_decrypt_key_len;
    
    /* SMB 3 Nonce used for encryption */
    uint64_t            session_smb3_nonce_high;
    uint64_t            session_smb3_nonce_low;
    
	uint32_t			reconnect_wait_time;	/* Amount of time to wait while reconnecting */
	uint32_t			*connect_flag;
	char				*NativeOS;
	char				*NativeLANManager;
    
    /* Save the negotiate parameter for Validate Negotiate */
    uint32_t            neg_capabilities;
    uuid_t              session_client_guid;    /* SMB 2/3 client Guid for Neg req */
    uint16_t            neg_security_mode;
    uint16_t            neg_dialect_count;
    uint16_t            neg_dialects[8];        /* Space for 8 dialects */
    
	uint32_t			negotiate_tokenlen;	/* negotiate token length */
	uint8_t				*negotiate_token;	/* negotiate token */
	struct smb_gss		session_gss;				/* Parameters for gssd */
	ntsid_t				session_ntwrk_sid;
	void				*throttle_info;
	uint64_t            session_server_caps;     /* SMB 2/3 server capabilities */
	uint64_t            session_volume_caps;     /* SMB 2/3 volume capabilities*/
    lck_mtx_t           session_model_info_lock;
	char                session_model_info[80];  /* SMB 2/3 server model string (80 chars should be plenty of space */
    int32_t             session_lease_key;       /* SMB 2/3 lease key incrementer to keep it unique */
    uint32_t            session_resp_wait_timeout; /* max time to wait for any response to arrive */
	uint32_t			session_dur_hndl_v2_default_timeout;
	uint32_t			session_dur_hndl_v2_desired_timeout;
	u_int32_t			session_TCP_QoS;

    /* For SMB 3 sealing */
    char                *decrypt_bufferp;
    u_int32_t           decrypt_buf_len;

    /* For MC support */
    struct session_network_interface_info session_interface_table;
};

#define session_maxmux	session_sopt.sv_maxmux
#define	session_flags	obj.co_flags

#define SMB_UNICODE_STRINGS(sessionp)	((sessionp)->session_hflags2 & SMB_FLAGS2_UNICODE)
#define SESSION_CAPS(a) ((a)->session_sopt.sv_caps)
#define UNIX_SERVER(a) (SESSION_CAPS(a) & SMB_CAP_UNIX)

/*
 * smb_share structure describes connection to the given SMB share (tree).
 * Connection to share is always built on top of the session.
 */

struct smb_share;

typedef int ss_going_away_t (struct smb_share *share);
typedef void ss_dead_t (struct smb_share *share);
typedef int ss_up_t (struct smb_share *share, int reconnect);
typedef int ss_down_t (struct smb_share *share, int timeToNotify);

struct smb_share {
	struct smb_connobj obj;
	lck_mtx_t		ss_stlock;	/* Used to lock the flags field only */
	char			*ss_name;
	struct smbmount	*ss_mount;	/* used for smb up/down */
	ss_going_away_t	*ss_going_away;
	ss_dead_t		*ss_dead;
	ss_down_t		*ss_down;
	ss_up_t			*ss_up;
	lck_mtx_t		ss_shlock;	/* used to protect ss_mount */ 
	uint32_t		ss_dead_timer;	/* Time to wait before this share should be marked dead, zero means never */
	uint32_t		ss_soft_timer;	/* Time to wait before this share should return time out errors, zero means never */
	u_short			ss_tid;         /* Tree ID for SMB 1 */
	uint32_t		ss_tree_id;		/* Tree ID for SMB 2/3 */
	uint32_t		ss_share_type;	/* Tree share type for SMB 2/3 */
	uint32_t		ss_share_flags;	/* Tree share flags for SMB 2/3 */
	uint32_t		ss_share_caps;	/* Tree share capabilities for SMB 2/3 */
	uint64_t		ss_unix_caps;	/* Unix capabilites are per share not session */
	enum smb_fs_types ss_fstype;	/* File system type of the share */
	uint32_t		ss_attributes;	/* File System Attributes */
	uint32_t		ss_maxfilenamelen;
	uint16_t		optionalSupport;
	uint32_t		maxAccessRights;    /* SMB 1 and SMB 2/3 */
	uint32_t		maxGuestAccessRights;
	
	/* SMB 2/3 FID mapping support */
	lck_mtx_t		ss_fid_lock;
	uint64_t		ss_fid_collisions;
	uint64_t		ss_fid_inserted;
	uint64_t		ss_fid_max_iter;
	FID_HASH_TABLE_SLOT	ss_fid_table[SMB_FID_TABLE_SIZE];
	
	/* SMB 2/3 Count of currently deferred closes */
	int32_t         ss_max_def_close_cnt;	/* max allowed deferred closes */
	int32_t         ss_curr_def_close_cnt;	/* current count */
	int64_t         ss_total_def_close_cnt;	/* cumulative count */
};

#define	ss_flags	obj.co_flags

#define SESSION_TO_CP(sessionp)		(&(sessionp)->obj)
#define	SS_TO_SESSION(ssp)		((struct smb_session*)((ssp)->obj.co_parent))
#define SSTOCP(ssp)		(&(ssp)->obj)
#define UNIX_CAPS(ssp)	(ssp)->ss_unix_caps

/*
 * Session level functions
 */
int smb_sm_init(void);
int smb_sm_done(void);
int smb_sm_negotiate(struct smbioc_negotiate *session_spec,
                     vfs_context_t context, struct smb_session **sessionpp,
                     struct smb_dev *sdp, int searchOnly,
                     uint32_t *matched_dns);
int smb_sm_ssnsetup(struct smb_session *sessionp, struct smbioc_setup * sspec,
					vfs_context_t context);
int smb_sm_tcon(struct smb_session *sessionp, struct smbioc_share *sspec, 
				struct smb_share **shpp, vfs_context_t context);
uint32_t smb_session_caps(struct smb_session *sessionp);
void parse_server_os_lanman_strings(struct smb_session *sessionp, void *refptr, 
									uint16_t bc);

/*
 * session level functions
 */
int  smb_session_negotiate(struct smb_session *sessionp, vfs_context_t context);
int  smb_session_ssnsetup(struct smb_session *sessionp);
int  smb_session_query_net_if(struct smb_session *sessionp);
int  smb_session_access(struct smb_session *sessionp, vfs_context_t context);
void smb_session_ref(struct smb_session *sessionp);
void smb_session_rele(struct smb_session *sessionp, vfs_context_t context);
int  smb_session_lock(struct smb_session *sessionp);
void smb_session_unlock(struct smb_session *sessionp);
int smb_session_reconnect_ref(struct smb_session *sessionp, vfs_context_t context);
void smb_session_reconnect_rel(struct smb_session *sessionp);
const char * smb_session_getpass(struct smb_session *sessionp);

/*
 * share level functions
 */
void smb_share_ref(struct smb_share *share);
void smb_share_rele(struct smb_share *share, vfs_context_t context);
const char * smb_share_getpass(struct smb_share *share);

/*
 * SMB protocol level functions
 */
int  smb1_smb_negotiate(struct smb_session *sessionp, vfs_context_t user_context,
                        int inReconnect, int onlySMB1, vfs_context_t context);
int  smb_smb_ssnsetup(struct smb_session *sessionp, int inReconnect, vfs_context_t context);
int  smb_smb_ssnclose(struct smb_session *sessionp, vfs_context_t context);
int  smb_smb_treeconnect(struct smb_share *share, vfs_context_t context);
int  smb_smb_treedisconnect(struct smb_share *share, vfs_context_t context);
int  smb1_read(struct smb_share *share, SMBFID fid, uio_t uio, 
              vfs_context_t context);
int  smb1_write(struct smb_share *share, SMBFID fid, uio_t uio, int ioflag, 
               vfs_context_t context);
int  smb1_echo(struct smb_session *sessionp, int timo, uint32_t EchoCount, 
              vfs_context_t context);
int  smb_checkdir(struct smb_share *share, struct smbnode *dnp, 
                  const char *name, size_t nmlen, vfs_context_t context);

#define SMBIOD_INTR_TIMO		2       
#define SMBIOD_SLEEP_TIMO       2       
#define SMB_SEND_WAIT_TIMO		60 * 2	/* How long should we wait for the server to response to a request. */
#define SMB_FAST_SEND_WAIT_TIMO  1      /* How long should we wait for the server to response to a request during a shutdown or forced unmount. */
#define SMB_RESP_WAIT_TIMO		35		/* How long should we wait for the server to response to any request. */

/*
 * After this many seconds we want an unresponded-to request to trigger 
 * some sort of UE (dialogue).  If the connection hasn't responded at all
 * in this many seconds then the dialogue is of the "connection isn't
 * responding would you like to force unmount" variety.  If the connection
 * has been responding (to other requests that is) then we need a dialogue
 * of the "operation is still pending do you want to cancel it" variety.
 * At present this latter dialogue does not exist so we have no UE and
 * just keep waiting for the slow operation.
 *
 * Raised from 8 to 12, to improve PPP DSL connections. Set to 10s to better
 * match SMB_RESP_WAIT_TIMO second timeout of 30s.
 */
#define SMBUETIMEOUT			10		/* Seconds until we send an Echo request */
#define SMB_MAX_SLEEP_CNT		5		/* Max seconds we wait between connections while doing a reconnect. */
#define NOTIFY_USER_TIMEOUT		5		/* Seconds before we will notify user there is a problem. */
#define SOFTMOUNT_TIMEOUT		12		/* Seconds to wait before soft mount calls time out */
#define DEAD_TIMEOUT			60		/* Seconds until we force unmount a soft mount (ie its dead) */
#define HARD_DEAD_TIMER			10 * 60	/* Seconds until we force unmount a hard mount (ie its dead) */
#define TRIGGER_DEAD_TIMEOUT	30		/* Seconds until we force unmount a trigger mount (ie its dead) */
#define TIME_MACHINE_DEAD_TIMER	30		/* Seconds until we force unmount a Time Machine mount (ie its dead) */

#define SMB_IOD_EVLOCKPTR(iod)  (&((iod)->iod_evlock))
#define SMB_IOD_EVLOCK(iod)     lck_mtx_lock(&((iod)->iod_evlock))
#define SMB_IOD_EVUNLOCK(iod)   lck_mtx_unlock(&((iod)->iod_evlock))

#define SMB_IOD_RQLOCKPTR(iod)  (&((iod)->iod_rqlock))
#define SMB_IOD_RQLOCK(iod)     lck_mtx_lock(&((iod)->iod_rqlock))
#define SMB_IOD_RQUNLOCK(iod)   lck_mtx_unlock(&((iod)->iod_rqlock))

#define SMB_IOD_FLAGSLOCKPTR(iod)       (&((iod)->iod_flagslock))
#define SMB_IOD_FLAGSLOCK(iod)          lck_mtx_lock(&((iod)->iod_flagslock))
#define SMB_IOD_FLAGSUNLOCK(iod)        lck_mtx_unlock(&((iod)->iod_flagslock))

#define SMB_IOD_LEASELOCKPTR(iod)  (&((iod)->iod_lease_lock))
#define SMB_IOD_LEASELOCK(iod)     lck_mtx_lock(&((iod)->iod_lease_lock))
#define SMB_IOD_LEASEUNLOCK(iod)   lck_mtx_unlock(&((iod)->iod_lease_lock))

#define smb_iod_wakeup(iod)     wakeup(&(iod)->iod_flags)

/*
 * smbiod thread
 */

/* 
 * Event type (ev_type) must be less than 0xff 
 * Upper bits are used to setting Sync and Processing
 */
#define SMBIOD_EV_NEWRQ            0x0001
#define SMBIOD_EV_SHUTDOWN         0x0002
#define SMBIOD_EV_FORCE_RECONNECT  0x0003
#define SMBIOD_EV_DISCONNECT       0x0004
/* 0x0005 available for use */
#define SMBIOD_EV_NEGOTIATE        0x0006
#define SMBIOD_EV_SSNSETUP         0x0007
#define SMBIOD_EV_QUERY_IF_INFO    0x0008 // Query server for its available interfaces
#define SMBIOD_EV_ANALYZE_CON      0x0009 // Examime local & remote IF tables and determine if additional connections are required
#define SMBIOD_EV_ESTABLISH_ALT_CH 0x000a // Connect, Negotiate and Setup an alternate connection

#define SMBIOD_EV_MASK            0x00ff
#define SMBIOD_EV_SYNC            0x0100
#define SMBIOD_EV_PROCESSING      0x0200

struct smbiod_event {
	int	ev_type;
	int	ev_error;
	void *	ev_ident;
	STAILQ_ENTRY(smbiod_event)	ev_link;
};

#define SMBIOD_SHUTDOWN         0x0001
#define SMBIOD_RUNNING          0x0002
#define SMBIOD_RECONNECT        0x0004
#define SMBIOD_START_RECONNECT  0x0008
#define SMBIOD_SESSION_NOTRESP  0x0010
#define SMBIOD_LEASE_THREAD_RUNNING 0x0020
#define SMBIOD_READ_THREAD_RUNNING 0x0040
#define SMBIOD_READ_THREAD_STOP 0x0080

struct lease_rq {
	TAILQ_ENTRY(lease_rq) link;
	uint16_t server_epoch;
	uint32_t flags;
	uint64_t lease_key_hi;
	uint64_t lease_key_low;
	uint32_t curr_lease_state;
	uint32_t new_lease_state;
};

struct smbiod {
    int                 iod_id;
    int                 iod_flags;
    enum smbiod_state   iod_state;
    lck_mtx_t           iod_flagslock;  /* iod_flags */
    /* number of active outstanding requests (keep it signed!) */
    int64_t             iod_muxcnt;
    /* number of active outstanding async requests (keep it signed!) */
    int32_t             iod_asynccnt;	
    struct timespec     iod_sleeptimespec;
    struct smb_session *iod_session;
    lck_mtx_t           iod_rqlock;     /* iod_rqlist, iod_muxwant */
    struct smb_rqhead   iod_rqlist;     /* list of outstanding requests */
    int                 iod_muxwant;
    vfs_context_t       iod_context;
    lck_mtx_t           iod_evlock;     /* iod_evlist */
    STAILQ_HEAD(,smbiod_event) iod_evlist;
    struct timespec     iod_lastrqsent;
    struct timespec     iod_lastrecv;
    int                 iod_workflag;   /* should be protected with lock */
    struct timespec     reconnectStartTime; /* Time when the reconnect was started */
	lck_mtx_t           iod_lease_lock;     /* iod_rqlist, iod_muxwant */
	struct smb_lease_head iod_lease_list;     /* list of lease breaks to be handled */
	int					iod_lease_work_flag;
};

int  smb_iod_nb_intr(struct smb_session *sessionp);
int  smb_iod_init(void);
int  smb_iod_done(void);
int smb_session_force_reconnect(struct smb_session *sessionp);
void smb_session_reset(struct smb_session *sessionp);
int  smb_iod_create(struct smb_session *sessionp);
int  smb_iod_destroy(struct smbiod *iod);
void smb_iod_lease_enqueue(struct smbiod *iod,
						   uint16_t server_epoch, uint32_t flags,
						   uint64_t lease_key_hi, uint64_t lease_key_low,
						   uint32_t curr_lease_state, uint32_t new_lease_state);
int  smb_iod_request(struct smbiod *iod, int event, void *ident);
int  smb_iod_rq_enqueue(struct smb_rq *rqp);
int  smb_iod_waitrq(struct smb_rq *rqp);
int  smb_iod_removerq(struct smb_rq *rqp);
void smb_iod_errorout_share_request(struct smb_share *share, int error);
int smb_iod_get_qos(struct smb_session *sessionp, void *data);
int smb_iod_set_qos(struct smb_session *sessionp, void *data);

extern lck_grp_attr_t *co_grp_attr;
extern lck_grp_t *co_lck_group;
extern lck_attr_t *co_lck_attr;

extern lck_grp_attr_t *session_credits_grp_attr;
extern lck_grp_t *session_credits_lck_group;
extern lck_attr_t *session_credits_lck_attr;

extern lck_grp_attr_t *session_st_grp_attr;
extern lck_grp_t *session_st_lck_group;
extern lck_attr_t *session_st_lck_attr;

extern lck_grp_attr_t *ssst_grp_attr;
extern lck_grp_t *ssst_lck_group;
extern lck_attr_t *ssst_lck_attr;

extern lck_grp_attr_t *fid_lck_grp_attr;
extern lck_grp_t *fid_lck_grp;
extern lck_attr_t *fid_lck_attr;

extern lck_grp_attr_t *iodflags_grp_attr;
extern lck_grp_t *iodflags_lck_group;
extern lck_attr_t *iodflags_lck_attr;

extern lck_grp_attr_t *iodrq_grp_attr;
extern lck_grp_t *iodrq_lck_group;
extern lck_attr_t *iodrq_lck_attr;

extern lck_grp_attr_t *iodev_grp_attr;
extern lck_grp_t *iodev_lck_group;
extern lck_attr_t *iodev_lck_attr;

extern lck_grp_attr_t *srs_grp_attr;
extern lck_grp_t *srs_lck_group;
extern lck_attr_t *srs_lck_attr;

#endif /* _KERNEL */
#endif // _SMB_CONN_H_
