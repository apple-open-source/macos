/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2023 Apple Inc. All rights reserved.
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
#include <netinet/in.h>
#endif
#include <sys/kauth.h>

#ifdef _KERNEL
#include <gssd/gssd_mach_types.h>
#include <libkern/crypto/sha2.h>
#else
#include <Kernel/gssd/gssd_mach_types.h>
#endif

#include <libkern/OSTypes.h>
#include <smbclient/smbclient.h>

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
#define SMBO_GONE       0x80000000

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
#define SMBV_SMB30                  kSMBAttributes_SessionFlags_SMB30   /* Using SMB 3.0 */
#define SMBV_SMB2                   kSMBAttributes_SessionFlags_SMB2    /* Using some version of SMB 2 or 3 */
#define SMBV_SMB2002                kSMBAttributes_SessionFlags_SMB2002 /* Using SMB 2.002 */
#define SMBV_SMB21                  kSMBAttributes_SessionFlags_SMB21   /* Using SMB 2.1 */
#define SMBV_SMB302                 kSMBAttributes_SessionFlags_SMB302  /* Using SMB 3.0.2 */
#define SMBV_SERVER_MODE_MASK       0x0000ff00		/* This nible is reserved for special server types */

#define SMBV_NETWORK_SID            0x00010000		/* The user's sid has been set on the session */
#define SMBV_SMB311                 kSMBAttributes_SessionFlags_SMB311 /* Using SMB 3.1.1 */
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
#define SMBV_MC_PREFER_WIRED        0x20000000      /* Prefer wired NICs in multichannel */
#define SMBV_DISABLE_311            0x40000000      /* Disable SMB v3.1.1 mainly pre auth integrity checking */
#define	SMBV_GONE                   SMBO_GONE		/* 0x80000000 - Reserved see above for more details */

/*
 * session_misc_flags - another flags field since session_flags is almost full.
 */
#define	SMBV_NEG_SMB1_ENABLED       0x00000001  /* Allow SMB 1 */
#define	SMBV_NEG_SMB2_ENABLED       0x00000002  /* Allow SMB 2 */
#define	SMBV_64K_QUERY_DIR          0x00000004  /* Use 64Kb OutputBufLen in Query_Dir */
#define SMBV_NO_CMPD_FLUSH_CLOSE    0x00000008  /* Server does not like cmpd Flush/Close */
#define	SMBV_HAS_FILEIDS            0x00000010  /* Has File IDs that we can use for hash values and inode number */
#define	SMBV_NO_QUERYINFO           0x00000020  /* Server does not like Query Info for FileAllInformation */
#define	SMBV_OSX_SERVER             0x00000040  /* Server is OS X based */
#define	SMBV_OTHER_SERVER           0x00000080  /* Server is not OS X based */
#define SMBV_CLIENT_SIGNING_REQUIRED 0x00000100
#define SMBV_NON_COMPOUND_REPLIES   0x00000200  /* Server does not send compound replies */
#define SMBV_63K_IOCTL              0x00000400  /* Use 63K MaxOutputResponse */
#define SMBV_HAS_COPYCHUNK          0x00000800  /* Server supports FSCTL_SRV_COPY_CHUNK IOCTL */
#define	SMBV_NEG_SMB3_ENABLED       0x00001000  /* Allow SMB 3 */
#define	SMBV_NO_WRITE_THRU          0x00002000  /* Server does not like Write Through */
#define SMBV_SMB1_SIGNING_REQUIRED  0x00004000
#define SMBV_SMB2_SIGNING_REQUIRED  0x00008000
#define SMBV_SMB3_SIGNING_REQUIRED  0x00010000
#define SMBV_MNT_TIME_MACHINE       0x00020000  /* Time Machine session */
#define SMBV_HAS_DUR_HNDL_V2        0x00040000  /* Server supports Durable Handle V2 */
#define SMBV_NO_DUR_HNDL_V2         0x00080000  /* Server does not support Durable Handle V2 */
#define SMBV_MNT_HIGH_FIDELITY      kSMBAttributes_SessionMiscFlags_HighFidelity  /* High Fidelity session */
#define SMBV_MNT_DATACACHE_OFF      0x00200000  /* Disable data caching */
#define SMBV_MNT_MDATACACHE_OFF     0x00400000  /* Disable meta data caching */
#define SMBV_MNT_SNAPSHOT           0x00800000  /* Snapshot mount */
#define SMBV_ENABLE_AES_128_CCM     0x01000000  /* Enable SMB v3.1.1 AES_128_CCM encryption */
#define SMBV_ENABLE_AES_128_GCM     0x02000000  /* Enable SMB v3.1.1 AES_128_GCM encryption */
#define SMBV_ENABLE_AES_256_CCM     0x04000000  /* Enable SMB v3.1.1 AES_256_CCM encryption */
#define SMBV_ENABLE_AES_256_GCM     0x08000000  /* Enable SMB v3.1.1 AES_256_GCM encryption */
#define SMBV_FORCE_SESSION_ENCRYPT  0x10000000  /* Force session level encryption */
#define SMBV_FORCE_SHARE_ENCRYPT    0x20000000  /* Force share level encryption */
#define SMBV_FORCE_IPC_ENCRYPT      0x40000000  /* Force share level encryption on IPC$ */
#define SMBV_FORCE_SHARE_ENCRYPT    0x20000000  /* Force share level encryption */
#define SMBV_FORCE_IPC_ENCRYPT      0x40000000  /* Force share level encryption on IPC$ */
#define SMBV_ENABLE_AES_128_CMAC        0x080000000  /* Enable SMB v3.1.1 AES_128_CMAC signing */
#define SMBV_ENABLE_AES_128_GMAC        0x100000000  /* Enable SMB v3.1.1 AES_128_GMAC signing */
#define SMBV_COMPRESSION_CHAINING_OFF   0x200000000  /* Chained compression is enabled */
#define SMBV_MC_CLIENT_RSS_FORCE_ON     0x400000000  /* MultiChannel force client RSS on */

#define SMBV_HAS_GUEST_ACCESS(sessionp)		(((sessionp)->session_flags & (SMBV_GUEST_ACCESS | SMBV_SFS_ACCESS)) != 0)
#define SMBV_HAS_ANONYMOUS_ACCESS(sessionp)	(((sessionp)->session_flags & (SMBV_ANONYMOUS_ACCESS | SMBV_SFS_ACCESS)) != 0)

/*
 * True if dialect is SMB 2.1 or later (i.e., SMB 2.1, SMB 3.0, SMB 3.1, SMB 3.02, ...)
 * Important: Remember to update this and smbclient.h when adding new dialects.
 */
#define SMBV_SMB21_OR_LATER(sessionp) (((sessionp)->session_flags & (SMBV_SMB21 | SMBV_SMB30 | SMBV_SMB302 | SMBV_SMB311)) != 0)

#define SMBV_SMB3_OR_LATER(sessionp) (((sessionp)->session_flags & (SMBV_SMB30 | SMBV_SMB302 | SMBV_SMB311)) != 0)

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

/* Multichannel interfaces ignore list */
#define kClientIfIgnorelistMaxLen 32 /* max len of client interface ignorelist */

/* Compression */
#define kClientCompressMaxEntries 64 /* max exclude/include for compression extensions */
#define kClientCompressMaxExtLen 16  /* max extension string length */

/*
 * Negotiated protocol parameters
 */
struct smb_sopt {
    uint32_t    sv_maxtx;               /* maximum transmit buf size */
    uint16_t    sv_maxmux;              /* SMB 1 - max number of outstanding rq's */
    uint16_t    sv_maxsessions;         /* SMB 1 - max number of sessions */
    uint32_t    sv_skey;                /* session key */
    uint32_t    sv_caps;                /* SMB 1 - capabilities, preset for SMB 2/3 */
    uint32_t    sv_sessflags;           /* SMB 2/3 - final session setup reply flags */
    uint16_t    sv_dialect;             /* SMB 2 - dialect (non zero for SMB 2/3 */
    uint32_t    sv_saved_capabilities;  /* SMB 2 - capabilities to check validate_negotiate */
    uint32_t    sv_active_capabilities; /* SMB 2 - active capabilities to check during runtime */
    uint32_t    sv_maxtransact;         /* SMB 2 - max transact size */
    uint32_t    sv_maxread;             /* SMB 2 - max read size */
    uint32_t    sv_maxwrite;            /* SMB 2 - max write size */
    uint8_t     sv_guid[16];            /* SMB 2 - GUID */
    uint16_t    sv_security_mode;       /* SMB 2 - security mode */
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


#define SMBIOD_SHUTDOWN             0x0001
#define SMBIOD_RUNNING              0x0002
#define SMBIOD_RECONNECT            0x0004
#define SMBIOD_START_RECONNECT      0x0008
#define SMBIOD_SESSION_NOTRESP      0x0010
#define SMBIOD_READ_THREAD_RUNNING  0x0020
#define SMBIOD_READ_THREAD_STOP     0x0040
#define SMBIOD_READ_THREAD_ERROR    0x0080  // The read-thread can not read from socket. Reconnect is required.
#define SMBIOD_ALTERNATE_CHANNEL    0x0100
#define SMBIOD_USE_CHANNEL_KEYS     0x0200  // Alternate channel messages are signed with the main-channel
                                            // until a successful session-setup is obtained, the the alt-ch
                                            // keys there after.
#define SMBIOD_INACTIVE_CHANNEL     0x0800 // The channel is connected but does not pass data.
#define SMBIOD_ABORT_CONNECT        0x1000 // Stop iod from trying to establish connection

#if defined(_KERNEL) || defined(MC_TESTER)

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
	au_asid_t     gss_asid;                     /* Audit session id to find gss_mp */
	gssd_nametype gss_target_nt;                /* Service's principal's name type */
	uint32_t      gss_spn_len;                  /* Service's principal's length */
	uint8_t *     gss_spn;                      /* Service's principal name */
    size_t        gss_spn_allocsize;            /* Service's principal name alloc size, required when deallocating gss_spn */
	gssd_nametype gss_client_nt;                /* Client's principal's name type */
	uint32_t      gss_cpn_len;                  /* Client's principal's length */
	uint8_t *     gss_cpn;                      /* Client's principal name */
    size_t        gss_cpn_allocsize;            /* Client's principal name alloc size, required when deallocating gss_cpn */
	char *        gss_cpn_display;              /* String representation of client principal */
    size_t        gss_cpn_display_allocsize;    /* gss_cpn_display alloc size, required when deallocating gss_cpn_display */
    uint32_t      gss_tokenlen;                 /* Gss token length */
	uint8_t *     gss_token;                    /* Gss token */
    size_t        gss_token_allocsize;          /* gss_token alloc size, required when deallocating gss_token */
	uint64_t      gss_ctx;                      /* GSS opaque context handle */
	uint64_t      gss_cred;                     /* GSS opaque cred handle */
	uint32_t      gss_rflags;                   /* Flags returned from gssd */
	uint32_t      gss_major;                    /* GSS major error code */
	uint32_t      gss_minor;                    /* GSS minor (mech) error code */
	uint32_t      gss_smb_error;                /* Last error returned by smb SetUpAndX */
};

struct session_network_interface_info {
    lck_mtx_t interface_table_lck;
    uint32_t pause_trials; 
    uint32_t client_nic_count;
    struct interface_info_list client_nic_info_list; /* list of the client's available NICs */
    uint32_t server_nic_count;
    struct interface_info_list server_nic_info_list; /* list of the server's available NICs */

    uint32_t max_channels;
    uint32_t srvr_rss_channels;
    uint32_t clnt_rss_channels;
    uint32_t *client_if_ignorelist;
    uint32_t client_if_ignorelist_len;
    size_t client_if_ignorelist_allocsize; /* client_if_ignorelist alloc size, required when deallocating client_if_ignorelist */
    uint32_t prefer_wired;

    /*
     * Table of all possible connections represent the state of every
     * couple of NICs.
     * The size of the table is (client_nic_count * server_nic_count).
     * In case of a successful connection will note the functionality of the connection.
     */
    struct connection_info_list session_con_list;    /* list of all possible connection - use next */
    uint32_t active_on_trial_connections;            /* record for the amount of open trial connections */
    struct connection_info_list successful_con_list; /* list of all successful connection - use success_next */
};

/* Maintain a list of iod threads per session */
TAILQ_HEAD(iod_tailq_head, smbiod);
// iod_tailq_flags:
#define SMB_IOD_TAILQ_SESSION_IS_SHUTTING_DOWN 1

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
 * SMB3 Signing/Encrypt Key Length
 * SMB3_256BIT_KEY_LEN is used for the 256 bit long keys which is currently
 * only being used by newer encryption algorithms
 */
#define SMB3_KEY_LEN 16
#define SMB3_256BIT_KEY_LEN 32  /* Used for 256 bit long keys */

/* Max number of SMB Dialects we currently support */
#define SMB3_MAX_DIALECTS 5

struct smb_session {
	struct smb_connobj	obj;
	char				*session_srvname;		/* The server name used for tree connect, also used for logging */
	char				*session_localname;
    size_t              session_srvname_allocsize;   /* session_srvname allocsize, required for freeing session_srvname */
    size_t              session_localname_allocsize; /* session_localname_allocsize allocsize, required for freeing session_localname_allocsize */
	char				ipv4v6DotName[45+1];    /* max IPv6 presentation len */
    
    /*
     * <72239144> session_saddr is the server addr used on initial connection
     * (ie the first main channel). Its used for checking to see if we have an
     * existing session to the server. For multichannel, all channels will
     * report this address for checking for existing sessions.
     *
     * Note: Someday, reconnect should use a non bound socket and attempt the
     * reconnect to this address.
     */
    struct sockaddr     *session_saddr;
    
	char				*session_username;
	char				*session_pass;			/* password for usl case */
	char				*session_domain;		/* workgroup/primary domain */
    size_t              session_username_allocsize; /* session_username allocsize, required for freeing session_username */
    size_t              session_pass_allocsize;     /* session_pass allocsize, required for freeing session_pass */
    size_t              session_domain_allocsize;   /* session_domain allocsize, required for freeing session_domain */
	int32_t				session_volume_cnt;
	unsigned			session_timo;			/* default request timeout */
	int32_t				session_number;			/* number of this session from the client side */
	uid_t				session_uid;			/* user id of connection */
	u_short				session_smbuid;			/* unique session id assigned by server */
	u_char				session_hflags;			/* or'ed with flags in the smb header */
	u_short				session_hflags2;		/* or'ed with flags in the smb header */
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
	uint64_t            session_session_id;      /* SMB 2/3 session id */
	uint64_t            session_prev_session_id; /* SMB 2/3 prev sessID for reconnect */
	uint64_t            session_misc_flags;      /* SMB 2/3 misc flags */
	struct smb_sopt		session_sopt;            /* server options */
	uint32_t			session_txmax;           /* max tx/rx packet size */
	uint32_t			session_rxmax;           /* max readx data size */
	uint32_t			session_wxmax;           /* max writex data size */
	struct smbiod		*session_iod;            /* main iod */
    struct smbiod       *round_robin_iod;        /* last iod used to transfer io */
	lck_mtx_t			session_stlock;
	uint32_t			session_seqno;           /* my next sequence number */
    uint8_t             *session_mackey;         /* MAC key */
    uint32_t            session_mackeylen;       /* length of MAC key */
    uint8_t             *full_session_mackey;    /* Session.FullSessionKey */
    uint32_t            full_session_mackeylen;  /* length of Session.FullSessionKey */

    /* Adaptive Read/Write values */
    lck_mtx_t           iod_quantum_lock;
    
    uint32_t            iod_readSizes[3];           /* [0] = min, [1] = med, [2] = max */
    uint32_t            iod_readCounts[3];          /* [0] = max, [1] = med, [2] = min */
    uint32_t            iod_readTotalTime[3];       /* [0] = min, [1] = med, [2] = max */
    uint64_t            iod_readTotalBytes[3];      /* [0] = min, [1] = med, [2] = max */

    uint32_t            iod_writeSizes[3];          /* [0] = min, [1] = med, [2] = max */
    uint32_t            iod_writeCounts[3];         /* [0] = max, [1] = med, [2] = min */
    uint64_t            iod_readBytePerSec[3];      /* [0] = min, [1] = med, [2] = max */
    uint64_t            iod_writeBytePerSec[3];     /* [0] = min, [1] = med, [2] = max */
    uint32_t            iod_writeTotalTime[3];      /* [0] = min, [1] = med, [2] = max */
    uint64_t            iod_writeTotalBytes[3];     /* [0] = min, [1] = med, [2] = max */

    struct timeval      iod_last_recheck_time;      /* Last time we checked speeds */

    uint32_t            iod_readQuantumSize;        /* current read quantum size */
    uint32_t            iod_readQuantumNumber;      /* current read quantum number */
    uint32_t            iod_writeQuantumSize;       /* current write quantum size */
    uint32_t            iod_writeQuantumNumber;     /* current write quantum number */

    uint64_t            active_channel_speed;
    uint32_t            active_channel_count;
    uint32_t            rw_gb_threshold;

    uint32_t            rw_max_check_time;

    /* SMB 3 signing key (Session.SessionKey) */
    uint8_t             session_smb3_signing_key[SMB3_KEY_LEN];
    uint32_t            session_smb3_signing_key_len;
    uint16_t            session_smb3_signing_algorithm;
    
    /* SMB 3 encryption key (Session.EncryptionKey) */
    /* A 128-bit key used for encrypting messages sent by the client */
    uint8_t             session_smb3_encrypt_key[SMB3_256BIT_KEY_LEN];
    uint32_t            session_smb3_encrypt_key_len;
    uint16_t            session_smb3_encrypt_ciper;

    /* SMB 3 decryption key (Session.DecryptionKey) */
    /* A 128-bit key used for decrypting messages received from the server. */
    uint8_t             session_smb3_decrypt_key[SMB3_256BIT_KEY_LEN];
    uint32_t            session_smb3_decrypt_key_len;
    
    /* SMB 3 Nonce used for encryption */
    uint64_t            session_smb3_nonce_high;
    uint64_t            session_smb3_nonce_low;
    
	uint32_t			reconnect_wait_time;     /* Amount of time to wait while reconnecting */
	uint32_t			*connect_flag;
	char				*NativeOS;
	char				*NativeLANManager;
    size_t              NativeOSAllocSize;          /* NativeOS allocsize, required for freeing NativeOS */
    size_t              NativeLANManagerAllocSize;  /* NativeLANManager allocsize, required for freeing NativeLANManager */
    
    /* Save the negotiate parameter for Validate Negotiate */
    uint32_t            neg_capabilities;
    uuid_t              session_client_guid;     /* SMB 2/3 client Guid for Neg req */
    uint16_t            neg_security_mode;
    uint16_t            neg_dialect_count;
    uint16_t            neg_dialects[8];         /* Space for 8 dialects */
    
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

    /* For SMB MultiChannel */
    lck_mtx_t           iod_tailq_lock;
    uint32_t            iod_tailq_flags;
    struct iod_tailq_head iod_tailq_head;         // A TailQ of all iod threads (connections) associated to the seession
    mach_port_t         gss_mp;                     // Mach port to gssd
    /* total number of bytes transmitted by this session on gone channels */
    uint64_t            session_gone_iod_total_tx_bytes;
    /* total number of bytes received by this session on gone channels */
    uint64_t            session_gone_iod_total_rx_bytes;
    /* total number of packets transmitted by this session on gone channels */
    uint64_t            session_gone_iod_total_tx_packets;
    /* total number of packets received by this session on gone channels */
    uint64_t            session_gone_iod_total_rx_packets;
    uint32_t            session_reconnect_count;    // The number of times this session has gone through reconnect
    struct timespec     session_setup_time;         // The local time at which the session has been established
    struct timespec     session_reconnect_time;     // The last time a reconnect took place
    lck_mtx_t           failover_lock;              // sync failovers
    uint32_t            max_channels;
    uint32_t            srvr_rss_channels;
    uint32_t            clnt_rss_channels;
    struct session_network_interface_info session_interface_table;

    /* Lease break */
    lck_mtx_t           session_lease_lock;         // session_lease_list
    struct smb_lease_head session_lease_list;       // list of lease breaks to be handle
    int                 session_lease_work_flag;
    int                 session_lease_flags;
    lck_mtx_t           session_lease_flagslock;

    /* For querying for server network interface changes */
    struct timeval      session_query_net_recheck_time;

    /* Snapshot info */
    char                snapshot_time[32] __attribute((aligned(8)));
    time_t              snapshot_local_time;
    
    /* For DFS, if tree connect returns STATUS_SMB_BAD_CLUSTER_DIALECT */
    uint32_t            session_max_dialect;

    uint8_t             uuid[16];                   // session random ID to return to userspace

    /*
     * SMB Compression support
     */
    uint32_t            client_compression_algorithms_map;  /* What client supports */
    uint32_t            server_compression_algorithms_map;  /* What server picked to use */
    uint32_t            compression_io_threshold;           /* Min IO size to compress */
    uint32_t            compression_chunk_len;
    uint32_t            compression_max_fail_cnt;
    /* Compression Counters */
    uint64_t            write_compress_cnt;
    uint64_t            write_cnt_LZ77Huff;
    uint64_t            write_cnt_LZ77;
    uint64_t            write_cnt_LZNT1;
    uint64_t            write_cnt_fwd_pattern;
    uint64_t            write_cnt_bwd_pattern;
    
    uint64_t            read_compress_cnt;
    uint64_t            read_cnt_LZ77Huff;
    uint64_t            read_cnt_LZ77;
    uint64_t            read_cnt_LZNT1;
    uint64_t            read_cnt_fwd_pattern;
    uint64_t            read_cnt_bwd_pattern;
};

#define session_maxmux	session_sopt.sv_maxmux
#define	session_flags	obj.co_flags

#define SMB_UNICODE_STRINGS(sessionp)	((sessionp)->session_hflags2 & SMB_FLAGS2_UNICODE)
#define SESSION_CAPS(a) ((a)->session_sopt.sv_caps)
#define UNIX_SERVER(a) (SESSION_CAPS(a) & SMB_CAP_UNIX)

#define SMB_SESSION_LEASE_THREAD_RUNNING 0x001
#define SMB_SESSION_LEASE_THREAD_STOP    0x002

/* time to wait between query network interface in multichannel mode  */
#define SMB_SESSION_QUERY_NET_IF_TIMEOUT_SEC 600

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
    size_t          ss_name_allocsize; /* ss_name allocsize, required for freeing ss_name */
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
                     uint32_t *matched_dns, struct sockaddr *saddr,
                     struct sockaddr *laddr);
int smb_sm_ssnsetup(struct smb_session *sessionp, struct smbioc_setup * sspec,
					vfs_context_t context);
int smb_sm_tcon(struct smb_session *sessionp, struct smbioc_share *sspec, 
				struct smb_share **shpp, vfs_context_t context);
uint32_t smb_session_caps(struct smb_session *sessionp);
void parse_server_os_lanman_strings(struct smb_session *sessionp, void *refptr, 
									uint16_t bc);
void smb_sm_lock_session_list(void);
void smb_sm_unlock_session_list(void);

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
int  smb_session_establish_alternate_connection(struct smbiod *parent_iod, struct session_con_entry *con_entry_p);

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
int  smb_smb_ssnsetup(struct smbiod *iod, int inReconnect, vfs_context_t context);
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

#define SMBIOD_INTR_TIMO         2
#define SMBIOD_SLEEP_TIMO        2
#define SMB_SEND_WAIT_TIMO      60 * 2 /* How long should we wait for the server to response to a request. */
#define SMB_FAST_SEND_WAIT_TIMO  1     /* How long should we wait for the server to response to a request during a shutdown or forced unmount. */
#define SMB_RESP_WAIT_TIMO      35     /* How long should we wait for the server to response to any request. */
#define SMB_RESP_TIMO_W_LEASING 45     /* How long should we wait for the server to response to any request when leasing is enabled. */

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

#define SMB_SESSION_LEASE_FLAGSLOCKPTR(sessionp)    \
    (&((sessionp)->session_lease_flagslock))
#define SMB_SESSION_LEASE_FLAGSLOCK(sessionp)       \
    lck_mtx_lock(&((sessionp)->session_lease_flagslock))
#define SMB_SESSION_LEASE_FLAGSUNLOCK(sessionp)     \
    lck_mtx_unlock(&((sessionp)->session_lease_flagslock))

#define SMB_SESSION_LEASELOCK(sessionp)     \
    lck_mtx_lock(&((sessionp)->session_lease_lock))
#define SMB_SESSION_LEASEUNLOCK(sessionp)   \
    lck_mtx_unlock(&((sessionp)->session_lease_lock))

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
#define SMBIOD_EV_FAILOVER         0x0005
#define SMBIOD_EV_NEGOTIATE        0x0006
#define SMBIOD_EV_SSNSETUP         0x0007
#define SMBIOD_EV_QUERY_IF_INFO    0x0008 // Query server for its available interfaces
#define SMBIOD_EV_ANALYZE_CON      0x0009 // Examime local & remote IF tables and determine if additional connections are required
#define SMBIOD_EV_ESTABLISH_ALT_CH 0x000a // Connect, Negotiate and Setup an alternate connection
#define SMBIOD_EV_PROCLAIM_MAIN    0x000b // Force iod to become the main iod
#define SMBIOD_EV_ECHO             0x000c

#define SMBIOD_EV_MASK            0x00ff
#define SMBIOD_EV_SYNC            0x0100
#define SMBIOD_EV_PROCESSING      0x0200
#define SMBIOD_EV_PROCESSED       0x0400


struct smbiod_event {
	int	ev_type;
	int	ev_error;
	void *	ev_ident;
	STAILQ_ENTRY(smbiod_event)	ev_link;
};

struct lease_rq {
	TAILQ_ENTRY(lease_rq) link;
	uint16_t server_epoch;
	uint32_t flags;
	uint64_t lease_key_hi;
	uint64_t lease_key_low;
	uint32_t curr_lease_state;
	uint32_t new_lease_state;
    struct smbiod *received_iod;
    int need_close_dir;
};

struct conn_params {
    struct session_con_entry  *con_entry;
    uint64_t client_if_idx;                     // IF index of the client NIC
    struct sock_addr_entry    *selected_saddr;  // The currently selected server IP address
};


// This lock protects session_credits_ fields
#define SMBC_CREDIT_LOCKPTR(iod)  (&(iod)->iod_credits_lock)
#define SMBC_CREDIT_LOCK(iod)     lck_mtx_lock(&(iod)->iod_credits_lock)
#define SMBC_CREDIT_UNLOCK(iod)   lck_mtx_unlock(&(iod)->iod_credits_lock)

struct smbiod {
    int                 iod_id;
    int                 iod_flags;
    enum smbiod_state   iod_state;
    lck_mtx_t           iod_flagslock;        /* iod_flags */
    /* number of active outstanding requests (keep it signed!) */
    int32_t             iod_muxcnt;
    /* number of active outstanding async requests (keep it signed!) */
    int32_t             iod_asynccnt;	
    struct timespec     iod_sleeptimespec;
    struct smb_session *iod_session;
    lck_mtx_t           iod_rqlock;           /* iod_rqlist, iod_muxwant */
    struct smb_rqhead   iod_rqlist;           /* list of outstanding requests */
    int                 iod_muxwant;
    vfs_context_t       iod_context;
    lck_mtx_t           iod_evlock;           /* iod_evlist */
    STAILQ_HEAD(,smbiod_event) iod_evlist;
    struct timespec     iod_lastrqsent;
    struct timespec     iod_lastrecv;
    int                 iod_workflag;         /* protected with iod_flags mutex */
    struct timespec     reconnectStartTime;   /* Time when the reconnect was started */
    
    /* MultiChannel Support */
    TAILQ_ENTRY(smbiod) tailq;                  /* list of iods per session */
    struct sockaddr     *iod_saddr;             /* server addr */
    struct sockaddr     *iod_laddr;             /* local addr, if any, only used for port 139 */
    lck_mtx_t           iod_tdata_lock;         /* transport control block lock */
    void                *iod_tdata;             /* transport control block */
    uint32_t            iod_ref_cnt;            /* counts references to this iod, protected by iod_tailq_lock */
    struct smb_tran_desc *iod_tdesc;            /* transport functions */
    struct smb_gss      iod_gss;                /* Parameters for gssd */
    uint64_t            iod_message_id;         /* SMB 2/3 request message id */
    uint32_t            negotiate_tokenlen;     /* negotiate token length */
    uint8_t             *negotiate_token;       /* negotiate token */
    uint32_t            iod_credits_granted;    /* SMB 2/3 credits granted */
    uint32_t            iod_credits_ss_granted; /* SMB 2/3 credits granted from session setup replies */
    uint32_t            iod_credits_max;        /* SMB 2/3 max amount of credits server has granted us */
    int32_t             iod_credits_wait;       /* SMB 2/3 credit wait */
    uint32_t            iod_req_pending;        /* SMB 2/3 set if there is a pending request */
    uint64_t            iod_oldest_message_id;  /* SMB 2/3 oldest pending request message id */
    lck_mtx_t           iod_credits_lock;

    /* Alternate channels have their own signing key */
    uint8_t             *iod_mackey;            /* MAC key */
    uint32_t            iod_mackeylen;          /* length of MAC key */
    uint8_t             *iod_full_mackey;       /* Session.FullSessionKey */
    uint32_t            iod_full_mackeylen;     /* length of Session.FullSessionKey */
    uint8_t             iod_smb3_signing_key[SMB3_KEY_LEN]; /* Channel.SigningKey */
    uint32_t            iod_smb3_signing_key_len;

    /* SMB 3.1.1 PreAuthIntegrity fields */
    uint8_t             iod_pre_auth_int_salt[32]; /* Random number */
    uint8_t             iod_pre_auth_int_hash_neg[SHA512_DIGEST_LENGTH] __attribute__((aligned(4)));
    uint8_t             iod_pre_auth_int_hash[SHA512_DIGEST_LENGTH] __attribute__((aligned(4)));

    struct conn_params  iod_conn_entry;
    uint64_t            iod_total_tx_bytes;     /* The total number of bytes transmitted by this iod */
    uint64_t            iod_total_rx_bytes;     /* The total number of bytes received by this iod */
    uint64_t            iod_total_tx_packets;     /* The total number of packets transmitted by this iod */
    uint64_t            iod_total_rx_packets;     /* The total number of packets received by this iod */
    struct timespec     iod_session_setup_time;
    
    /* Saved last session setup reply */
    uint8_t             *iod_sess_setup_reply;  /* Used for pre auth and alt channels */
    size_t              iod_sess_setup_reply_len;
    uint64_t            iod_sess_setup_message_id; /* Used for AES-GMAC signing */

    struct timeval      iod_connection_to; /* tcp connection timeout */
};

int  smb_iod_nb_intr(struct smbiod *iod);
int  smb_iod_init(void);
int  smb_iod_done(void);
int smb_session_force_reconnect(struct smbiod *iod);
void smb_session_reset(struct smb_session *sessionp);
int  smb_iod_create(struct smb_session *sessionp, struct smbiod **iodpp);
int  smb_iod_destroy(struct smbiod *iod, bool selfclean);
void smb_iod_gss_destroy(struct smbiod *iod);
void smb_iod_lease_enqueue(struct smbiod *iod,
						   uint16_t server_epoch, uint32_t flags,
						   uint64_t lease_key_hi, uint64_t lease_key_low,
						   uint32_t curr_lease_state, uint32_t new_lease_state);
int  smb_iod_request(struct smbiod *iod, int event, void *ident);
int  smb_iod_rq_enqueue(struct smb_rq *rqp);
int  smb_iod_waitrq(struct smb_rq *rqp);
int  smb_iod_removerq(struct smb_rq *rqp);
void smb_iod_errorout_share_request(struct smb_share *share, int error);
int smb_iod_get_qos(struct smbiod *iod, void *data);
int smb_iod_set_qos(struct smbiod *iod, void *data);
int smb_iod_get_main_iod(struct smb_session *sessionp, struct smbiod **iodpp,
                         const char *function);
int smb_iod_get_any_iod(struct smb_session *sessionp, struct smbiod **iodpp,
                        const char *function);
int smb_iod_get_non_main_iod(struct smb_session *sessionp,
                             struct smbiod **iodpp, const char *function, int force_inactive);
int smb_iod_ref(struct smbiod *iod, const char *function);
int smb_iod_rel(struct smbiod *iod, struct smb_rq *rqp, const char *function);
bool smb2_extract_ip_and_len(struct sockaddr *addr, void **addrp, uint32_t *addr_lenp);
int  smb2_sockaddr_to_str(struct sockaddr *sockaddrp, char *str, uint32_t max_str_len);
void smb2_spd_to_txt(uint64_t spd, char *str, uint32_t str_len);
uint32_t smb2_get_port_from_sockaddr(struct sockaddr *);
int  smb2_set_port_in_sockaddr(struct sockaddr *addr, uint32_t port);
int  smb2_create_server_addr(struct smbiod *iod);
int  smb2_find_valid_ip_family(struct smbiod *iod);
int smb_request_iod_disconnect_and_shutdown(struct smbiod *iod);
int smb_iod_inactive(struct smbiod *iod);
int smb_iod_active(struct smbiod *iod);
int smb_iod_establish_alt_ch(struct smbiod *iod);

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

extern lck_grp_attr_t *iodtdata_grp_attr;
extern lck_grp_t *iodtdata_lck_group;
extern lck_attr_t *iodtdata_lck_attr;

extern lck_grp_attr_t *iodev_grp_attr;
extern lck_grp_t *iodev_lck_group;
extern lck_attr_t *iodev_lck_attr;

extern lck_grp_attr_t *srs_grp_attr;
extern lck_grp_t *srs_lck_group;
extern lck_attr_t *srs_lck_attr;

#endif /* _KERNEL */
#endif // _SMB_CONN_H_
