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
#ifndef _NETINET_IN_H_
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

/*
 * Two levels of connection hierarchy
 */
#define	SMBL_SM		0x0000
#define SMBL_VC		0x0001
#define SMBL_SHARE	0x0002
#define SMBL_VALID	0x0003

/*
 * Device flags
 */

#define NSMBFL_OPEN		0x0001
#define NSMBFL_CANCEL	0x0002
#define NSMBFL_SHAREVC	0x0004
#define NSMBFL_TRYBOTH	0x0008

/*
 * Common object flags
 *
 * Remember that vc_flags and ss_flags are defined to be co_flags,
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
 * VC flags
 */
#define SMBV_USER_SECURITY			SMB_SM_USER		/* 0x01, server in the user security mode */
#define SMBV_ENCRYPT_PASSWORD		SMB_SM_ENCRYPT	/* 0x02 use challenge/responce for password */
#define SMBV_SIGNING				SMB_SM_SIGS		/* 0x04 server does smb signing */
#define SMBV_SIGNING_REQUIRED		SMB_SM_SIGS_REQ	/* 0x08 serrver requires smb signing */
#define SMBV_SECURITY_MODE_MASK		0x000000ff		/* Lower byte reserved for the security modes */
#define	SMBV_NT4					0x00000100		/* Tells us the server is a NT4 */
#define	SMBV_WIN2K					0x00000200		/* Tells us the server is Windows 2000 */
#define	SMBV_WIN98					0x00000400		/* The server is a Windows 95, 98 or Me OS */
#define SMBV_SERVER_MODE_MASK		0x0000ff00		/* This nible is resvered for special server types */
#define SMBV_PRIVATE_VC				0x00010000		/* Negotiate will create a new VC, that is private */
#define SMBV_EXT_SEC				0x00020000		/* conn to use extended security */
#define SMBV_GUEST_ACCESS			0x00040000		/* user is using guess security */
#define	SMBV_KERBEROS_SUPPORT		0x00080000		/* Server supports Kerberos */
#define SMBV_MECHTYPE_KRB5			0x00200000		/* Server supports Kerberos Mech Type */
#define SMBV_MECHTYPE_NTLMSSP		0x00400000		/* Server supports NTLMSSP Mech Type */
#define	SMBV_AUTH_DONE				0x00800000		/* Security compeleted successfully */
#define SMBV_MINAUTH				0x0f000000		/* minimum auth level for conn */
#define SMBV_MINAUTH_LM				0x01000000		/* no plaintext passwords */
#define SMBV_MINAUTH_NTLM			0x02000000		/* don't send LM reply */
#define SMBV_MINAUTH_NTLMV2			0x04000000		/* don't fall back to NTLMv1 */
#define SMBV_MINAUTH_KERBEROS		0x08000000		/* don't do NTLMv1 or NTLMv2 */
#define SMBV_USER_LAND_MASK			0x0f070000		/* items that are changable by the user */
#define	SMBV_GONE					SMBO_GONE		/* 0x80000000 - Reserved see above for more details */

/*
 * smb_share flags
 */
#define SMBS_PERMANENT		0x0001
#define SMBS_RECONNECTING	0x0002
#define SMBS_CONNECTED		0x0004
#define SMBS_SFM_VOLUME		0x0020			/* This share was setup as a SFM Volume. */
#define	SMBS_GONE			SMBO_GONE		/* 0x80000000 - Reserved see above for more details */

/*
 * share types
 */
#define	SMB_ST_DISK		0x0	/* A: */
#define	SMB_ST_PRINTER		0x1	/* LPT: */
#define	SMB_ST_PIPE		0x2	/* IPC */
#define	SMB_ST_COMM		0x3	/* COMM */
#define	SMB_ST_ANY		0x4
#define	SMB_ST_MAX		0x4
#define SMB_ST_NONE		0xff	/* not a part of protocol */

/*
 * Negotiated protocol parameters
 */
struct smb_sopt {
	int		sv_proto;
	int16_t		sv_tz;		/* offset in min relative to UTC */
	u_int32_t	sv_maxtx;	/* maximum transmit buf size */
	u_int16_t	sv_maxmux;	/* max number of outstanding rq's */
	u_int16_t 	sv_maxvcs;	/* max number of VCs */
	u_int16_t	sv_rawmode;
	u_int32_t	sv_maxraw;	/* maximum raw-buffer size */
	u_int32_t	sv_skey;	/* session key */
	u_int32_t	sv_caps;	/* capabilites SMB_CAP_ */
	u_int64_t	sv_unix_caps;	/* unix capabilites  */
};

/*
 * network IO daemon states
 */
enum smbiod_state {
	SMBIOD_ST_NOTCONN,	/* no connect request was made */
	SMBIOD_ST_CONNECT,	/* a connect attempt is in progress */
	SMBIOD_ST_TRANACTIVE,	/* transport level is up */
	SMBIOD_ST_NEGOACTIVE,	/* completed negotiation */
	SMBIOD_ST_SSNSETUP,	/* started (a) session setup */
	SMBIOD_ST_VCACTIVE,	/* session established */
	SMBIOD_ST_DEAD,		/* connection broken, transport is down */
	SMBIOD_ST_RECONNECT	/* We need to attempt to reconnect again */
};


/*
 * Info structures
 */
#define	SMB_INFO_NONE		0
#define	SMB_INFO_VC		2
#define	SMB_INFO_SHARE		3

struct smb_vc_info {
	int		itype;
	int		usecount;
	uid_t		uid;		/* user id of connection */
	gid_t		gid;		/* group of connection */
	mode_t		mode;		/* access mode */
	int		flags;
	enum smbiod_state iodstate;
	struct smb_sopt	sopt;
	char		srvname[SMB_MAX_DNS_SRVNAMELEN];
	char		vcname[128];
};

#ifdef _KERNEL

#include <sys/lock.h>
#include <netsmb/smb_subr.h>

#define CONNADDREQ(a1,a2)	((a1)->sa_len == (a2)->sa_len && \
				 bcmp(a1, a2, (a1)->sa_len) == 0)

struct smb_vc;
struct smb_share;
struct smb_cred;
struct smb_rq;
struct mbdata;
struct smbioc_oshare;
struct smbioc_ossn;

TAILQ_HEAD(smb_rqhead, smb_rq);

#define SMB_NBTIMO	15
#define SMB_DEFRQTIMO	30	/* 30 for oplock revoke/writeback */
#define SMBWRTTIMO	60
#define SMBSSNSETUPTIMO	60
#define SMBNOREPLYWAIT (0)

#define SMB_DIALECT(vcp)	((vcp)->vc_sopt.sv_proto)

struct smb_tran_desc;
struct smb_dev;

/*
 * Connection object
 */
struct smb_connobj;

typedef void smb_co_gone_t (struct smb_connobj *cp, struct smb_cred *scred);
typedef void smb_co_free_t (struct smb_connobj *cp);

struct smb_connobj {
	int			co_level;	/* SMBL_ */
	u_int32_t	co_flags;
	lck_mtx_t   *co_lock;
	void		 *co_lockowner;
	int32_t		co_lockcount;
	uint32_t	co_lock_flags;
	lck_mtx_t	co_interlock;
	int			co_usecount;
	struct smb_connobj *	co_parent;
	SLIST_HEAD(,smb_connobj)co_children;
	SLIST_ENTRY(smb_connobj)co_next;
	smb_co_gone_t *		co_gone;
	smb_co_free_t *		co_free;
};

#define SMBFS_CO_LOCK_WAIT 1

#define SMBLIST_FOREACH(var, head, field) \
	for((var) = (typeof (var))((head)->slh_first);(var);  \
		(var) = (typeof(var))(((struct smb_connobj*)var)->field.sle_next))

#define	SMBCO_FOREACH(var, cp)	SMBLIST_FOREACH((var), &(cp)->co_children, co_next)

/*
 * Data structure to access gssd for the use of SPNEGO/Kerberos
 */
struct smb_gss {
	mach_port_t	gss_mp;					/* Mach port to gssd */
	char *		gss_spn;				/* Service's principal name */
	char *		gss_cpn;				/* Client's principal name */
	uint32_t	gss_spnlen;				/* Length of SPN */
	uint32_t	gss_tokenlen;			/* Gss token length */
	uint8_t *	gss_token;				/* Gss token */
	uint64_t	gss_verif;				/* Verifier for gssd instance */
	uint32_t	gss_ctx;				/* GSS opaque context handle */
	uint32_t	gss_cred;				/* GSS opaque cred handle */
	uint8_t*	gss_skey;				/* GSS session key */
	uint32_t	gss_skeylen;			/* Session key length */
	uint32_t	gss_major;				/* GSS major error code */
	uint32_t	gss_minor;				/* GSS minor (mech) error code */
};

/*
 * Virtual Circuit (session) to a server.
 * This is the most (over)complicated part of SMB protocol.
 * For the user security level (usl), each session with different remote
 * user name has its own VC.
 * It is unclear however, should share security level (ssl) allow additional
 * VCs, because user name is not used and can be the same. On other hand,
 * multiple VCs allows us to create separate sessions to server on a per
 * user basis.
 */

/*
 * This lock protects vc_flags
 */
#define	SMBC_ST_LOCK(vcp)	lck_mtx_lock(&(vcp)->vc_stlock)
#define	SMBC_ST_UNLOCK(vcp)	lck_mtx_unlock(&(vcp)->vc_stlock)


struct smb_vc {
	struct smb_connobj obj;
	char *		vc_srvname;
	struct sockaddr*vc_paddr;	/* server addr */
	struct sockaddr*vc_laddr;	/* local addr, if any */
	char *		vc_username;
	char *		vc_pass;	/* password for usl case */
	char *		vc_domain;	/* workgroup/primary domain */

	u_int		vc_timo;	/* default request timeout */
	int		vc_maxvcs;	/* maximum number of VC per connection */

	void *		vc_tolower;	/* local charset */
	void *		vc_toupper;	/* local charset */
	void *		vc_toserver;	/* local charset to server one */
	void *		vc_tolocal;	/* server charset to local one */
	int			vc_number;	/* number of this VC from the client side */
	uid_t		vc_uid;		/* user id of connection */
	gid_t		vc_grp;		/* group of connection */
	mode_t		vc_mode;	/* access mode */
	u_short		vc_smbuid;	/* unique vc id assigned by server */

	u_char		vc_hflags;	/* or'ed with flags in the smb header */
	u_short		vc_hflags2;	/* or'ed with flags in the smb header */
	void *		vc_tdata;	/* transport control block */
	struct smb_tran_desc *vc_tdesc;
	int		vc_chlen;	/* actual challenge length */
	u_char 		vc_ch[SMB_MAXCHALLENGELEN];
	u_short		vc_mid;		/* multiplex id */
	struct smb_sopt	vc_sopt;	/* server options */
	int			vc_txmax;	/* max tx/rx packet size */
	int			vc_rxmax;	/* max readx data size */
	int			vc_wxmax;	/* max writex data size */
	struct smbiod *	vc_iod;
	lck_mtx_t	vc_stlock;
	u_int32_t	vc_seqno;       /* my next sequence number */
	u_int8_t	*vc_mackey;     /* MAC key */
	int			vc_mackeylen;   /* length of MAC key */
	u_int32_t	reconnect_wait_time;	/* Amount of time to wait while reconnecting */
	u_int32_t	*connect_flag;
	struct smb_gss vc_gss;		/* Parameters for gssd */
};

#define vc_maxmux	vc_sopt.sv_maxmux
#define	vc_flags	obj.co_flags

#define SMB_UNICODE_STRINGS(vcp)	((vcp)->vc_hflags2 & SMB_FLAGS2_UNICODE)
/*
 * smb_share structure describes connection to the given SMB share (tree).
 * Connection to share is always built on top of the VC.
 */

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
	SMB_FS_MAC_OS_X = 6			/* Mac OS X Leopard SAMBA Server, Support streams. */
};

/*
 * This lock protects ss_flags
 */
#define	SMBS_ST_LOCK(ssp)		lck_mtx_lock(&(ssp)->ss_stlock)
#define	SMBS_ST_LOCKPTR(ssp)	(&(ssp)->ss_stlock)
#define	SMBS_ST_UNLOCK(ssp)		lck_mtx_unlock(&(ssp)->ss_stlock)

struct smb_share {
	struct smb_connobj obj;
	char *		ss_name;
	struct smbmount* ss_mount;	/* used for smb up/down */
	u_short		ss_tid;		/* TID */
	int			ss_type;	/* share type */
	enum smb_fs_types ss_fstype;	/* file system type of the  share */
	uid_t		ss_uid;		/* user id of connection */
	gid_t		ss_grp;		/* group of connection */
	mode_t		ss_mode;	/* access mode */
	char *		ss_pass;	/* password to a share, can be null */
	lck_mtx_t	ss_stlock;
	u_int32_t	ss_attributes;	/* File System Attributes */
	u_int32_t	ss_maxfilenamelen;
	char *		ss_fsname;
};

#define	ss_flags	obj.co_flags

#define CPTOVC(cp)	((struct smb_vc*)(cp))
#define VCTOCP(vcp)	(&(vcp)->obj)
#define CPTOSS(cp)	((struct smb_share*)(cp))
#define	SSTOVC(ssp)	CPTOVC(((ssp)->obj.co_parent))
#define SSTOCP(ssp)	(&(ssp)->obj)

struct smb_vcspec {
	char *		srvname;
	struct sockaddr*sap;
	struct sockaddr*lap;
	int		flags;
	char *		username;
	char *		pass;
	char *		domain;
	mode_t		mode;
	mode_t		rights;
	uid_t		owner;
	gid_t		group;
	char *		localcs;
	char *		servercs;
	struct smb_sharespec *shspec;
	struct smb_share *ssp;		/* returned */
	u_int32_t	reconnect_wait_time;	/* Amount of time to wait while reconnecting */
	/*
	 * The rest is an internal data
	 */
	struct smb_cred *scred;
};

struct smb_sharespec {
	char *		name;
	char *		pass;
	mode_t		mode;
	mode_t		rights;
	uid_t		owner;
	gid_t		group;
	int		stype;
	/*
	 * The rest is an internal data
	 */
	struct smb_cred *scred;
};

struct smbioc_ssnsetup;

/*
 * Session level functions
 */
int  smb_sm_init(void);
int  smb_sm_done(void);
int  smb_sm_negotiate(struct smb_vcspec *vcspec, struct smb_cred *scred, struct smb_vc **vcpp, struct smb_dev *sdp);
int  smb_sm_ssnsetup(struct smbioc_ssnsetup *dp, struct smb_vcspec *vcspec, struct smb_cred *scred, struct smb_vc *vcp);
int  smb_sm_tcon(struct smb_vcspec *vcspec,
	struct smb_sharespec *shspec, struct smb_cred *scred,
	struct smb_vc *vcp);
uint32_t smb_vc_caps(struct smb_vc *vcp);
int smb_check_for_win2k(void *refptr, int namelen);

/*
 * Connection object
 */
void smb_co_ref(struct smb_connobj *cp);
void smb_co_rele(struct smb_connobj *cp, struct smb_cred *scred);
void smb_co_put(struct smb_connobj *cp, struct smb_cred *scred);
int  smb_co_lock(struct smb_connobj *cp);
void smb_co_unlock(struct smb_connobj *cp);
void smb_co_drain(struct smb_connobj *cp);
void smb_co_lockdrain(struct smb_connobj *cp);

/*
 * session level functions
 */
int  smb_vc_create(struct smb_vcspec *vcspec,
	struct smb_cred *scred, struct smb_vc **vcpp);
int  smb_vc_negotiate(struct smb_vc *vcp, struct smb_cred *scred);
int  smb_vc_ssnsetup(struct smb_vc *vcp, struct smb_cred *scred);
int  smb_vc_access(struct smb_vc *vcp, vfs_context_t context);
int  smb_vc_get(struct smb_vc *vcp, int flags, struct smb_cred *scred);
void smb_vc_put(struct smb_vc *vcp, struct smb_cred *scred);
void smb_vc_ref(struct smb_vc *vcp);
void smb_vc_rele(struct smb_vc *vcp, struct smb_cred *scred);
int  smb_vc_lock(struct smb_vc *vcp);
void smb_vc_unlock(struct smb_vc *vcp);
int  smb_vc_lookupshare(struct smb_vc *vcp, struct smb_sharespec *shspec,
	struct smb_cred *scred, struct smb_share **sspp);
const char * smb_vc_getpass(struct smb_vc *vcp);
u_short smb_vc_nextmid(struct smb_vc *vcp);

/*
 * share level functions
 */
int  smb_share_create(struct smb_vc *vcp, struct smb_sharespec *shspec,
	struct smb_cred *scred, struct smb_share **sspp);
int  smb_share_access(struct smb_share *ssp, vfs_context_t context);
void smb_share_ref(struct smb_share *ssp);
void smb_share_rele(struct smb_share *ssp, struct smb_cred *scred);
int  smb_share_get(struct smb_share *ssp, int flags, struct smb_cred *scred);
void smb_share_put(struct smb_share *ssp, struct smb_cred *scred);
int  smb_share_lock(struct smb_share *ssp);
void smb_share_unlock(struct smb_share *ssp);
void smb_share_invalidate(struct smb_share *ssp);
const char * smb_share_getpass(struct smb_share *ssp);

/*
 * SMB protocol level functions
 */
int  smb_smb_negotiate(struct smb_vc *vcp, struct smb_cred *scred, struct smb_cred *user_scred, int inReconnect);
int  smb_smb_ssnsetup(struct smb_vc *vcp, struct smb_cred *scred);
int  smb_smb_ssnclose(struct smb_vc *vcp, struct smb_cred *scred);
int  smb_smb_treeconnect(struct smb_share *ssp, struct smb_cred *scred);
int  smb_smb_treedisconnect(struct smb_share *ssp, struct smb_cred *scred);
int  smb_read(struct smb_share *ssp, u_int16_t fid, uio_t uio,
	struct smb_cred *scred);
int  smb_write(struct smb_share *ssp, u_int16_t fid, uio_t uio,
	struct smb_cred *scred, int timo);
int  smb_smb_echo(struct smb_vc *vcp, struct smb_cred *scred, int timo);
int  smb_smb_checkdir(struct smb_share *ssp, struct smbnode *dnp, char *name, int nmlen, struct smb_cred *scred);

#define SMBIOD_INTR_TIMO		2       
#define SMBIOD_SLEEP_TIMO       2       
#define SMBIOD_PING_TIMO        60 * 2	/* 2 minutes for now */
#define SMB_SEND_WAIT_TIMO		60 * 2	/* How long should we wait for the server to response to a request. */
#define SMB_RESP_WAIT_TIMO		60		/* How long should we wait for the server to response to any request. */

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
 * Raised from 8 to 12, to improve PPP DSL connections.
 */
#define SMBUETIMEOUT 12				/* seconds */
#define SMB_MAX_SLEEP_CNT 5			/* Max seconds we wait between connections while doing a reconnect. */
#define NOTIFY_USER_TIMEOUT 5		/* The number of seconds before we will the user there is a problem. */
#define SOFTMOUNT_TIMEOUT 10		/* The number of seconds to wait before soft mount calls time out */


#define SMB_IOD_EVLOCKPTR(iod)  (&((iod)->iod_evlock))
#define SMB_IOD_EVLOCK(iod)     lck_mtx_lock(&((iod)->iod_evlock))
#define SMB_IOD_EVUNLOCK(iod)   lck_mtx_unlock(&((iod)->iod_evlock))

#define SMB_IOD_RQLOCKPTR(iod)  (&((iod)->iod_rqlock))
#define SMB_IOD_RQLOCK(iod)     lck_mtx_lock(&((iod)->iod_rqlock))
#define SMB_IOD_RQUNLOCK(iod)   lck_mtx_unlock(&((iod)->iod_rqlock))

#define SMB_IOD_FLAGSLOCKPTR(iod)       (&((iod)->iod_flagslock))
#define SMB_IOD_FLAGSLOCK(iod)          lck_mtx_lock(&((iod)->iod_flagslock))
#define SMB_IOD_FLAGSUNLOCK(iod)        lck_mtx_unlock(&((iod)->iod_flagslock))

#define smb_iod_wakeup(iod)     wakeup(&(iod)->iod_flags)

/*
 * smbiod thread
 */

#define	SMBIOD_EV_NEWRQ		0x0001
#define	SMBIOD_EV_SHUTDOWN	0x0002
#define	SMBIOD_EV_DISCONNECT	0x0004
#define	SMBIOD_EV_NEGOTIATE	0x0006
#define	SMBIOD_EV_SSNSETUP	0x0007
#define	SMBIOD_EV_MASK		0x00ff
#define	SMBIOD_EV_SYNC		0x0100
#define	SMBIOD_EV_PROCESSING	0x0200

struct smbiod_event {
	int	ev_type;
	int	ev_error;
	void *	ev_ident;
	STAILQ_ENTRY(smbiod_event)	ev_link;
};

#define	SMBIOD_SHUTDOWN			0x0001
#define	SMBIOD_RUNNING			0x0002
#define	SMBIOD_RECONNECT		0x0004
#define	SMBIOD_START_RECONNECT	0x0008
#define	SMBIOD_VC_NOTRESP		0x0010

struct smbiod {
	int					iod_id;
	int					iod_flags;
	enum smbiod_state	iod_state;
	lck_mtx_t			iod_flagslock;	/* iod_flags */
	int					iod_muxcnt;	/* number of active outstanding requests */
	struct timespec 	iod_sleeptimespec;
	struct smb_vc *		iod_vc;
	lck_mtx_t			iod_rqlock;	/* iod_rqlist, iod_muxwant */
	struct smb_rqhead	iod_rqlist;	/* list of outstanding requests */
	int					iod_muxwant;
	struct proc *		iod_p;
	struct smb_cred		iod_scred;
	lck_mtx_t			iod_evlock;	/* iod_evlist */
	STAILQ_HEAD(,smbiod_event) iod_evlist;
	struct timespec 	iod_lastrqsent;
	struct timespec 	iod_lastrecv;
	struct timespec 	iod_pingtimo;
	int					iod_workflag;	/* should be protected with lock */
	struct timespec		reconnectStartTime; /* Time when the reconnect was started */
};

int  smb_iod_nb_intr(struct smb_vc *vcp);
int  smb_iod_init(void);
int  smb_iod_done(void);
void smb_vc_reset(struct smb_vc *vcp);
int  smb_iod_create(struct smb_vc *vcp);
int  smb_iod_destroy(struct smbiod *iod);
int  smb_iod_request(struct smbiod *iod, int event, void *ident);
int  smb_iod_rq_enqueue(struct smb_rq *rqp);
int  smb_iod_waitrq(struct smb_rq *rqp);
int  smb_iod_removerq(struct smb_rq *rqp);
void smb_iod_shutdown_share(struct smb_share *ssp);

extern lck_grp_attr_t *co_grp_attr;
extern lck_grp_t *co_lck_group;
extern lck_attr_t *co_lck_attr;
extern lck_grp_attr_t *vcst_grp_attr;
extern lck_grp_t *vcst_lck_group;
extern lck_attr_t *vcst_lck_attr;
extern lck_grp_attr_t *ssst_grp_attr;
extern lck_grp_t *ssst_lck_group;
extern lck_attr_t *ssst_lck_attr;
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
