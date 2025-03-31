/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#ifndef _NFS_CLNT_H_
#define _NFS_CLNT_H_

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#define CONFIG_NFS4           1
#define CONFIG_TRIGGERS       1
#define NAMEDSTREAMS          1
#endif /* TARGET_OS_OSX */

#define CONFIG_NFS_GSS        1

/*
 * -------------
 */
#include <sys/kdebug.h>
#include <sys/kernel_types.h>

#include <kern/lock_group.h>
#include <kern/locks.h>
#include <kern/thread_call.h>

#include <sys/vnode.h>
#include <nfs/nfs.h>

/* NFS client lock groups */
typedef enum {
	NLG_GLOBAL = 0,
	NLG_MOUNT,
	NLG_REQUEST,
	NLG_OPEN,
	NLG_NFSIOD,
	NLG_NODE_HASH,
	NLG_NODE,
	NLG_DATA,
	NLG_LOCK,
	NLG_BUF,
	NLG_GSS_KRB5_MECH,
	NLG_GSS_CLNT,
	NLG_XID,
	NLG_ASYNC_WRITE,
	NLG_OPEN_OWNERS,
	NLG_DELEGATIONS,
	NLG_SEND_STATE,
	NLG_COMMITD,
	NLG_NUM_GROUPS
} nfs_lck_group_kind_t;

/* NFS client lock mutexes */
typedef enum {
	NLM_GLOBAL = 0,
	NLM_REQUEST,
	NLM_NFSIOD,
	NLM_NODE_HASH,
	NLM_LOCK,
	NLM_BUF,
	NLM_XID,
	NLM_COMMITD,
	NLM_NUM_MUTEXES
} nfs_lck_mtx_kind_t;

lck_grp_t* get_lck_group(nfs_lck_group_kind_t group);
lck_mtx_t* get_lck_mtx(nfs_lck_mtx_kind_t mtx);

int nfs_locks_init(void);
void nfs_locks_free(void);

/* NFS client memory zones */
typedef enum {
	NFS_MOUNT_ZONE = 0,
	NFS_FILE_HANDLE_ZONE,
	NFS_REQUEST_ZONE,
	NFS_NODE_ZONE,
	NFS_BIO_ZONE,
	NFS_DIROFF,
	NFS_NAMEI,
	NFS_VATTR,
	NFS_NUM_ZONES
} nfs_zone_kind_t;

zone_t get_zone(nfs_zone_kind_t zone);

void nfs_zone_init(void);
void nfs_zone_destroy(void);

void nfs_sysctl_register(void);
void nfs_sysctl_unregister(void);

extern struct nfs_hooks_out hooks_out;
void * nfs_bsdthreadtask_info(thread_t th);

int nfsclnt_device_add(void);
void nfsclnt_device_remove(void);

int install_nfs_vfs_fs(void);
int uninstall_nfs_vfs_fs(void);

nfstype vtonfs_type(enum vtype, int);
enum vtype nfstov_type(nfstype, int);
int     vtonfsv2_mode(enum vtype, mode_t);

struct vnode_attr;
struct sockaddr_in; /* XXX */
struct dqblk;
struct direntry;
struct nfsbuf;
struct nfs_vattr;
struct nfs_fsattr;
struct nfsnode;
typedef struct nfsnode * nfsnode_t;
struct nfs_open_owner;
struct nfs_open_file;
struct nfs_lock_owner;
struct nfs_file_lock;
struct nfsreq;
struct nfs_rpc_record_state;
struct nfs_fs_locations;
struct nfs_location_index;
struct nfs_socket;
struct nfs_socket_search;

/*
 * The set of signals the interrupt an I/O in progress for NFSMNT_INT mounts.
 * What should be in this set is open to debate, but I believe that since
 * I/O system calls on ufs are never interrupted by signals the set should
 * be minimal. My reasoning is that many current programs that use signals
 * such as SIGALRM will not expect file I/O system calls to be interrupted
 * by them and break.
 */
#define NFSINT_SIGMASK  (sigmask(SIGINT)|sigmask(SIGTERM)|sigmask(SIGKILL)| \
	                                                                 sigmask(SIGHUP)|sigmask(SIGQUIT))

/**
 * nfsreq callback args
 */
struct nfsreq_cbargs {
	off_t offset;
	size_t length;
	uint32_t stategenid;
};

/*
 * async NFS request callback info
 */
struct nfsreq_cbinfo {
	void                    (*rcb_func)(struct nfsreq *);   /* async request callback function */
	struct nfsbuf           *rcb_bp;                        /* buffer I/O RPC is for */
	struct nfsreq_cbargs    rcb_args;                       /* nfsreq callback args */
};

/*
 * Arguments to use if a request needs to call SECINFO to handle a WRONGSEC error
 *
 * If only node is set, use the parent file handle and this node's name; otherwise,
 * use any file handle and name provided.
 */
struct nfsreq_secinfo_args {
	nfsnode_t               rsia_np;                /* the node */
	const char              *rsia_name;             /* alternate name string */
	u_char                  *rsia_fh;               /* alternate file handle */
	uint32_t                rsia_namelen;           /* length of string */
	uint32_t                rsia_fhsize;            /* length of fh */
};
#define NFSREQ_SECINFO_SET(SI, NP, FH, FHSIZE, NAME, NAMELEN) \
	do { \
	                                (SI)->rsia_np = (NP); \
	                                (SI)->rsia_fh = (FH); \
	                                (SI)->rsia_fhsize = (FHSIZE); \
	                                (SI)->rsia_name = (NAME); \
	                                (SI)->rsia_namelen = (NAMELEN); \
	} while (0)

/*
 * NFS outstanding request list element
 */
struct nfsreq {
	lck_mtx_t               r_mtx;          /* NFS request mutex */
	TAILQ_ENTRY(nfsreq)     r_chain;        /* request queue chain */
	TAILQ_ENTRY(nfsreq)     r_achain;       /* mount's async I/O request queue chain */
	TAILQ_ENTRY(nfsreq)     r_rchain;       /* mount's async I/O resend queue chain */
	TAILQ_ENTRY(nfsreq)     r_cchain;       /* mount's cwnd queue chain */
	mbuf_t                  r_mrest;        /* request body mbufs */
	mbuf_t                  r_mhead;        /* request header mbufs */
	struct nfsm_chain       r_nmrep;        /* reply mbufs */
	nfsnode_t               r_np;           /* NFS node */
	struct nfsmount         *r_nmp;         /* NFS mount point */
	uint64_t                r_xid;          /* RPC transaction ID */
	uint32_t                r_procnum;      /* NFS procedure number */
	int                     r_error;        /* request error */
	size_t                  r_mreqlen;      /* request length */
	int                     r_flags;        /* flags on request, see below */
	int                     r_lflags;       /* flags protected by list mutex, see below */
	int                     r_refs;         /* # outstanding references */
	uint8_t                 r_delay;        /* delay to use for jukebox error */
	uint8_t                 r_rexmit;       /* current retrans count */
	uint32_t                r_retry;        /* max retransmission count */
	int                     r_rtt;          /* RTT for rpc */
	thread_t                r_thread;       /* thread that did I/O system call */
	kauth_cred_t            r_cred;         /* credential used for request */
	time_t                  r_start;        /* request start time */
	time_t                  r_lastmsg;      /* time of last tprintf */
	time_t                  r_resendtime;   /* time of next jukebox error resend */
	struct nfs_gss_clnt_ctx *r_gss_ctx;     /* RPCSEC_GSS context */
	SLIST_HEAD(, gss_seq)   r_gss_seqlist;  /* RPCSEC_GSS sequence numbers */
	size_t                  r_gss_argoff;   /* RPCSEC_GSS offset to args */
	uint32_t                r_gss_arglen;   /* RPCSEC_GSS arg length */
	uint32_t                r_auth;         /* security flavor request sent with */
	uint32_t                *r_wrongsec;    /* wrongsec: other flavors to try */
	struct nfsreq_cbinfo    r_callback;     /* callback info */
	struct nfsreq_secinfo_args r_secinfo;   /* secinfo args */
};

/*
 * Queue head for nfsreq's
 */
TAILQ_HEAD(nfs_reqqhead, nfsreq);
extern struct nfs_reqqhead nfs_reqq;
extern lck_grp_t nfs_request_grp;

#define R_XID32(x)      ((x) & 0xffffffff)

#define NFSNOLIST       ((void *)0x0badcafe)    /* sentinel value for nfs lists */
#define NFSREQNOLIST    NFSNOLIST               /* sentinel value for nfsreq lists */

/* Flag values for r_flags */
#define R_TIMING        0x00000001      /* timing request (in mntp) */
#define R_CWND          0x00000002      /* request accounted for in congestion window */
#define R_SOFTTERM      0x00000004      /* request terminated (e.g. soft mnt) */
#define R_RESTART       0x00000008      /* RPC should be restarted. */
#define R_INITTED       0x00000010      /* request has been initialized */
#define R_TPRINTFMSG    0x00000020      /* Did a tprintf msg. */
#define R_MUSTRESEND    0x00000040      /* Must resend request */
#define R_ALLOCATED     0x00000080      /* request was allocated */
#define R_SENT          0x00000100      /* request has been sent */
#define R_WAITSENT      0x00000200      /* someone is waiting for request to be sent */
#define R_RESENDERR     0x00000400      /* resend failed */
#define R_JBTPRINTFMSG  0x00000800      /* Did a tprintf msg for jukebox error */
#define R_ASYNC         0x00001000      /* async request */
#define R_ASYNCWAIT     0x00002000      /* async request now being waited on */
#define R_RESENDQ       0x00004000      /* async request currently on resendq */
#define R_SENDING       0x00008000      /* request currently being sent */
#define R_SOFT          0x00010000      /* request is soft - don't retry or reconnect */
#define R_IOD           0x00020000      /* request is being managed by an IOD */

#define R_NOUMOUNTINTR  0x10000000      /* request should not be interrupted by a signal during unmount */
#define R_NOINTR        0x20000000      /* request should not be interrupted by a signal */
#define R_RECOVER       0x40000000      /* a state recovery RPC - during NFSSTA_RECOVER */
#define R_SETUP         0x80000000      /* a setup RPC - during (re)connection */
#define R_OPTMASK       0xf0000000      /* mask of all RPC option flags */

/* Flag values for r_lflags */
#define RL_BUSY         0x0001          /* Locked. */
#define RL_WAITING      0x0002          /* Someone waiting for lock. */
#define RL_QUEUED       0x0004          /* request is on the queue */

extern u_int64_t nfs_xid, nfs_xidwrap;
extern int nfs_iosize, nfs_allow_async, nfs_statfs_rate_limit;
extern int nfs_access_cache_timeout, nfs_access_delete, nfs_access_dotzfs, nfs_access_for_getattr;
extern int nfs_lockd_mounts, nfs_lockd_request_sent;
extern int nfs_tprintf_initial_delay, nfs_tprintf_delay;
extern int nfsiod_thread_count, nfsiod_thread_max, nfs_max_async_writes;
extern int nfs_idmap_ctrl, nfs_callback_port, nfs_split_open_owner;
extern int nfs_is_mobile, nfs_readlink_nocache, nfs_root_steals_ctx;
extern int nfs_mount_timeout, nfs_mount_quick_timeout;
extern uint32_t unload_in_progress;
extern uint32_t nfs_tcp_sockbuf;
extern uint32_t nfs_squishy_flags;
extern uint32_t nfsclnt_debug_ctl;
extern struct nfsclntstats nfsclntstats;
extern int nfsclnt_nointr_pagein;

extern uint32_t nfs_fs_attr_bitmap[NFS_ATTR_BITMAP_LEN];
extern uint32_t nfs_object_attr_bitmap[NFS_ATTR_BITMAP_LEN];
extern uint32_t nfs_getattr_bitmap[NFS_ATTR_BITMAP_LEN];
extern uint32_t nfs4_getattr_write_bitmap[NFS_ATTR_BITMAP_LEN];

/* bits for nfs_idmap_ctrl: */
#define NFS_IDMAP_CTRL_USE_IDMAP_SERVICE                0x00000001 /* use the ID mapping service */
#define NFS_IDMAP_CTRL_FALLBACK_NO_COMMON_IDS           0x00000002 /* fallback should NOT handle common IDs like "root" and "nobody" */
#define NFS_IDMAP_CTRL_LOG_FAILED_MAPPINGS              0x00000020 /* log failed ID mapping attempts */
#define NFS_IDMAP_CTRL_LOG_SUCCESSFUL_MAPPINGS          0x00000040 /* log successful ID mapping attempts */

#define NFSIOD_MAX      (MIN(nfsiod_thread_max, NFS_MAXASYNCTHREAD))

struct nfs_dulookup {
	int du_flags;                   /* state of ._ lookup */
#define NFS_DULOOKUP_DOIT       0x1
#define NFS_DULOOKUP_INPROG     0x2
	struct componentname du_cn;     /* ._ name being looked up */
	struct nfsreq du_req;           /* NFS request for lookup */
	char du_smallname[48];          /* buffer for small names */
};

/* request list mutex */
extern lck_mtx_t nfs_request_mutex;
extern int nfs_request_timer_on;

/* mutex for nfs client globals */
extern lck_mtx_t nfs_global_mutex;

#if CONFIG_NFS4
/* NFSv4 callback globals */
extern int nfs4_callback_timer_on;
extern in_port_t nfs4_cb_port, nfs4_cb_port6;

/* nfs 4 default domain for user mapping */
extern char nfs4_default_domain[MAXPATHLEN];
/* nfs 4 timer call structure */
extern thread_call_t    nfs4_callback_timer_call;
#endif

/* nfs timer call structures */
extern thread_call_t    nfs_request_timer_call;
extern thread_call_t    nfs_buf_timer_call;

void    nfs_nodehash_init(void);
void    nfs_nodehash_destroy(void);
u_long  nfs_hash(u_char *, int);

void    nfs_mbuf_init(void);

#if CONFIG_NFS4
int     nfs4_init_clientid(struct nfsmount *);
int     nfs4_setclientid(struct nfsmount *, int);
void    nfs4_remove_clientid(struct nfsmount *);
int     nfs4_renew(struct nfsmount *, int);
void    nfs4_renew_timer(void *, void *);
void    nfs4_mount_callback_setup(struct nfsmount *);
void    nfs4_mount_callback_shutdown(struct nfsmount *);
void    nfs4_cb_accept(socket_t, void *, int);
void    nfs4_cb_rcv(socket_t, void *, int);
void    nfs4_callback_timer(void *, void *);
int     nfs4_secinfo_rpc(struct nfsmount *, struct nfsreq_secinfo_args *, kauth_cred_t, uint32_t *, int *);
int     nfs4_get_fs_locations(struct nfsmount *, nfsnode_t, u_char *, int, const char *, vfs_context_t, struct nfs_fs_locations *);
void    nfs4_default_attrs_for_referral_trigger(nfsnode_t, char *, int, struct nfs_vattr *, fhandle_t *);
#endif

void    nfs_interval_timer_start(thread_call_t, time_t);
void    nfs_fs_locations_cleanup(struct nfs_fs_locations *);
int     nfs_sockaddr_cmp(struct sockaddr *, struct sockaddr *);

int     nfs_connect(struct nfsmount *, int, int);
void    nfs_disconnect(struct nfsmount *);
void    nfs_need_reconnect(struct nfsmount *);
void    nfs_mount_sock_thread_wake(struct nfsmount *);
int     nfs_mount_check_dead_timeout(struct nfsmount *);
int     nfs_mount_gone(struct nfsmount *);
void    nfs_mount_rele(struct nfsmount *);
void    nfs_mount_zombie(struct nfsmount *, int);
void    nfs_mount_make_zombie(struct nfsmount *);
int     nfs_mountopts(struct nfsmount *, char *, int);

void    nfs_rpc_record_state_init(struct nfs_rpc_record_state *);
void    nfs_rpc_record_state_cleanup(struct nfs_rpc_record_state *);
int     nfs_rpc_record_read(socket_t, struct nfs_rpc_record_state *, int, int *, mbuf_t *);

int     nfs_getattr(nfsnode_t, struct nfs_vattr *, vfs_context_t, int);
int     nfs_getattrcache(nfsnode_t, struct nfs_vattr *, int);
int     nfs_loadattrcache(nfsnode_t, struct nfs_vattr *, u_int64_t *, int);
long    nfs_attrcachetimeout(nfsnode_t);

int     nfs_buf_page_inval_internal(vnode_t vp, off_t offset);
int     nfs_vinvalbuf1(vnode_t, int, vfs_context_t, int);
int     nfs_vinvalbuf2(vnode_t, int, thread_t, kauth_cred_t, int);
int     nfs_vinvalbuf_internal(nfsnode_t, int, thread_t, kauth_cred_t, int, int);
void    nfs_wait_bufs(nfsnode_t);

int     nfs_request_create(nfsnode_t, mount_t, struct nfsm_chain *, int, thread_t, kauth_cred_t, struct nfsreq **);
void    nfs_request_destroy(struct nfsreq *);
void    nfs_request_ref(struct nfsreq *, int);
void    nfs_request_rele(struct nfsreq *);
int     nfs_request_add_header(struct nfsreq *);
int     nfs_request_send(struct nfsreq *, int);
void    nfs_request_wait(struct nfsreq *);
int     nfs_request_finish(struct nfsreq *, struct nfsm_chain *, int *);
int     nfs_request(nfsnode_t, mount_t, struct nfsm_chain *, int, vfs_context_t, struct nfsreq_secinfo_args *, struct nfsm_chain *, u_int64_t *, int *);
int     nfs_request2(nfsnode_t, mount_t, struct nfsm_chain *, int, thread_t, kauth_cred_t, struct nfsreq_secinfo_args *, int, struct nfsm_chain *, u_int64_t *, int *);
int     nfs_request_gss(mount_t, struct nfsm_chain *, thread_t, kauth_cred_t, int, struct nfs_gss_clnt_ctx *, struct nfsm_chain *, int *);
int     nfs_request_async(nfsnode_t, mount_t, struct nfsm_chain *, int, thread_t, kauth_cred_t, struct nfsreq_secinfo_args *, int, struct nfsreq_cbinfo *, struct nfsreq **);
int     nfs_request_async_finish(struct nfsreq *, struct nfsm_chain *, u_int64_t *, int *);
void    nfs_request_async_cancel(struct nfsreq *);
void    nfs_request_timer(void *, void *);
int     nfs_request_using_gss(struct nfsreq *);
void    nfs_get_xid(uint64_t *);
int     nfs_sigintr(struct nfsmount *, struct nfsreq *, thread_t, int);
int     nfs_noremotehang(thread_t);

int     nfs_send(struct nfsreq *, int);
int     nfs_sndlock(struct nfsreq *);
void    nfs_sndunlock(struct nfsreq *);

int     nfs_uaddr2sockaddr(const char *, struct sockaddr *);

int     nfs_aux_request(struct nfsmount *, thread_t, struct sockaddr *, socket_t, int, mbuf_t, uint32_t, int, int, struct nfsm_chain *);
int     nfs_portmap_lookup(struct nfsmount *, vfs_context_t, struct sockaddr *, socket_t, uint32_t, uint32_t, uint32_t, int);

void    nfs_location_next(struct nfs_fs_locations *, struct nfs_location_index *);
int     nfs_location_index_cmp(struct nfs_location_index *, struct nfs_location_index *);
void    nfs_location_mntfromname(struct nfs_fs_locations *, struct nfs_location_index, char *, size_t, int);
int     nfs_socket_create(struct nfsmount *, struct sockaddr *, uint8_t, in_port_t, uint32_t, uint32_t, int, struct nfs_socket **);
void    nfs_socket_destroy(struct nfs_socket *);
void    nfs_socket_options(struct nfsmount *, struct nfs_socket *);
void    nfs_connect_upcall(socket_t, void *, int);
int     nfs_connect_error_class(int);
int     nfs_connect_search_loop(struct nfsmount *, struct nfs_socket_search *);
void    nfs_socket_search_update_error(struct nfs_socket_search *, int);
void    nfs_socket_search_cleanup(struct nfs_socket_search *);
void    nfs_mount_connect_thread(void *, __unused wait_result_t);

int     nfs_lookitup(nfsnode_t, char *, int, vfs_context_t, nfsnode_t *);
void    nfs_dulookup_init(struct nfs_dulookup *, nfsnode_t, const char *, int, vfs_context_t);
void    nfs_dulookup_start(struct nfs_dulookup *, nfsnode_t, vfs_context_t);
void    nfs_dulookup_finish(struct nfs_dulookup *, nfsnode_t, vfs_context_t);
int     nfs_dir_buf_cache_lookup(nfsnode_t, nfsnode_t *, struct componentname *, vfs_context_t, int, int *);
int     nfs_dir_buf_search(struct nfsbuf *, struct componentname *, fhandle_t *, struct nfs_vattr *, uint64_t *, time_t *, daddr64_t *, int);
void    nfs_name_cache_purge(nfsnode_t, nfsnode_t, struct componentname *, vfs_context_t);
void    nfs_negative_cache_purge(nfsnode_t);

#if CONFIG_NFS4
uint32_t nfs4_ace_nfstype_to_vfstype(uint32_t, int *);
uint32_t nfs4_ace_vfstype_to_nfstype(uint32_t, int *);
uint32_t nfs4_ace_nfsflags_to_vfsflags(uint32_t);
uint32_t nfs4_ace_vfsflags_to_nfsflags(uint32_t);
uint32_t nfs4_ace_nfsmask_to_vfsrights(uint32_t);
uint32_t nfs4_ace_vfsrights_to_nfsmask(uint32_t);
int nfs4_id2guid(char *, guid_t *, int);
int nfs4_guid2id(guid_t *, char *, size_t *, int);

int     nfs4_parsefattr(struct nfsm_chain *, struct nfs_fsattr *, struct nfs_vattr *, fhandle_t *, struct dqblk *, struct nfs_fs_locations *);
#endif

int     nfs_parsefattr(struct nfsmount *nmp, struct nfsm_chain *, int,
    struct nfs_vattr *);
void    nfs_vattr_set_supported(uint32_t *, struct vnode_attr *);
void    nfs_vattr_set_bitmap(struct nfsmount *, uint32_t *, struct vnode_attr *);
void    nfs3_pathconf_cache(struct nfsmount *, struct nfs_fsattr *);
int     nfs3_check_lockmode(struct nfsmount *, struct sockaddr *, int, int);
int     nfs3_mount_rpc(struct nfsmount *, struct sockaddr *, int, int, char *, vfs_context_t, int, fhandle_t *, struct nfs_sec *);
void    nfs3_umount_rpc(struct nfsmount *, vfs_context_t, int);
void    nfs_rdirplus_update_node_attrs(nfsnode_t, struct direntry *, fhandle_t *, struct nfs_vattr *, uint64_t *);
int     nfs_node_access_slot(nfsnode_t, uid_t, int);
void    nfs_vnode_notify(nfsnode_t, uint32_t);

void    nfs_avoid_needless_id_setting_on_create(nfsnode_t, struct vnode_attr *, vfs_context_t);
int     nfs_open_state_set_busy(nfsnode_t, thread_t);
void    nfs_open_state_clear_busy(nfsnode_t);
struct nfs_open_owner *nfs_open_owner_find(struct nfsmount *, kauth_cred_t, proc_t, int);
void    nfs_open_owner_destroy(struct nfs_open_owner *);
void    nfs_open_owner_ref(struct nfs_open_owner *);
void    nfs_open_owner_rele(struct nfs_open_owner *);
int     nfs_open_owner_set_busy(struct nfs_open_owner *, thread_t);
void    nfs_open_owner_clear_busy(struct nfs_open_owner *);
void    nfs_owner_seqid_increment(struct nfs_open_owner *, struct nfs_lock_owner *, int);
int     nfs_open_file_find(nfsnode_t, struct nfs_open_owner *, struct nfs_open_file **, uint32_t, uint32_t, int);
int     nfs_open_file_find_internal(nfsnode_t, struct nfs_open_owner *, struct nfs_open_file **, uint32_t, uint32_t, int);
void    nfs_open_file_destroy(struct nfs_open_file *);
int     nfs_open_file_set_busy(struct nfs_open_file *, thread_t);
void    nfs_open_file_clear_busy(struct nfs_open_file *);
void    nfs_open_file_add_open(struct nfs_open_file *, uint32_t, uint32_t, int);
void    nfs_open_file_remove_open_find(struct nfs_open_file *, uint32_t, uint32_t, uint8_t *, uint8_t *, int *);
void    nfs_open_file_remove_open(struct nfs_open_file *, uint32_t, uint32_t);
int     nfs_open_file_merge(struct nfs_open_file *, struct nfs_open_file *, uint32_t, uint32_t);
void    nfs_get_stateid(nfsnode_t, thread_t, kauth_cred_t, nfs_stateid *, int);
int     nfs_check_for_locks(struct nfs_open_owner *, struct nfs_open_file *);
int     nfs_close(nfsnode_t, struct nfs_open_file *, uint32_t, uint32_t, vfs_context_t);

void    nfs_release_open_state_for_node(nfsnode_t, int);
void    nfs_revoke_open_state_for_node(nfsnode_t);
struct nfs_lock_owner *nfs_lock_owner_find(nfsnode_t, proc_t, caddr_t, int);
void    nfs_lock_owner_destroy(struct nfs_lock_owner *);
void    nfs_lock_owner_ref(struct nfs_lock_owner *);
void    nfs_lock_owner_rele(struct nfs_lock_owner *);
int     nfs_lock_owner_set_busy(struct nfs_lock_owner *, thread_t);
void    nfs_lock_owner_clear_busy(struct nfs_lock_owner *);
void    nfs_lock_owner_insert_held_lock(struct nfs_lock_owner *, struct nfs_file_lock *);
struct nfs_file_lock *nfs_file_lock_alloc(struct nfs_lock_owner *);
void    nfs_file_lock_destroy(nfsnode_t, struct nfs_file_lock *, thread_t, kauth_cred_t);
int     nfs_file_lock_conflict(struct nfs_file_lock *, struct nfs_file_lock *, int *);
int     nfs_advlock_getlock(nfsnode_t, struct nfs_lock_owner *, struct flock *, uint64_t, uint64_t, vfs_context_t);
int     nfs_advlock_setlock(nfsnode_t, struct nfs_open_file *, struct nfs_lock_owner *, int, uint64_t, uint64_t, int, short, vfs_context_t);
int     nfs_advlock_unlock(nfsnode_t, struct nfs_open_file *, struct nfs_lock_owner *, uint64_t, uint64_t, int, vfs_context_t);

#if CONFIG_NFS4
int     nfs4_release_lockowner_rpc(nfsnode_t, struct nfs_lock_owner *, thread_t, kauth_cred_t);
int     nfs4_create_rpc(vfs_context_t, nfsnode_t, struct componentname *, struct vnode_attr *, int, char *, nfsnode_t *);
int     nfs4_open(nfsnode_t, struct nfs_open_file *, uint32_t, uint32_t, vfs_context_t);
int     nfs4_open_delegated(nfsnode_t, struct nfs_open_file *, uint32_t, uint32_t, vfs_context_t);
int     nfs4_reopen(struct nfs_open_file *, thread_t);
int     nfs4_open_rpc(struct nfs_open_file *, vfs_context_t, struct componentname *, struct vnode_attr *, vnode_t, vnode_t *, int, int, int);
int     nfs4_open_rpc_internal(struct nfs_open_file *, vfs_context_t, thread_t, kauth_cred_t, struct componentname *, struct vnode_attr *, vnode_t, vnode_t *, int, int, int);
int     nfs4_open_confirm_rpc(struct nfsmount *, nfsnode_t, u_char *, int, struct nfs_open_owner *, nfs_stateid *, thread_t, kauth_cred_t, struct nfs_vattr *, uint64_t *);
int     nfs4_open_reopen_rpc(struct nfs_open_file *, thread_t, kauth_cred_t, struct componentname *, vnode_t, vnode_t *, int, int);
int     nfs4_open_reclaim_rpc(struct nfs_open_file *, int, int);
int     nfs4_open_handle_fh_mismatch(nfsnode_t, struct nfs_open_file *, char *, int, fhandle_t *, struct nfs_vattr *, u_int64_t *, uint32_t, thread_t, kauth_cred_t);
int     nfs4_claim_delegated_open_rpc(struct nfs_open_file *, int, int, int);
int     nfs4_claim_delegated_state_for_open_file(struct nfs_open_file *, int);
int     nfs4_claim_delegated_state_for_node(nfsnode_t, int);
int     nfs4_open_downgrade_rpc(nfsnode_t, struct nfs_open_file *, vfs_context_t);
int     nfs4_close_rpc(nfsnode_t, struct nfs_open_file *, thread_t, kauth_cred_t, int, int);
void    nfs4_delegation_return_enqueue(nfsnode_t);
void    nfs4_delegation_return_read(nfsnode_t, int, thread_t, kauth_cred_t);
int     nfs4_delegation_return(nfsnode_t, int, thread_t, kauth_cred_t);
int     nfs4_delegreturn_rpc(struct nfsmount *, u_char *, int, struct nfs_stateid *, int, thread_t, kauth_cred_t);

nfsnode_t nfs4_named_attr_dir_get(nfsnode_t, int, vfs_context_t);
int     nfs4_named_attr_get(nfsnode_t, struct componentname *, uint32_t, int, vfs_context_t, nfsnode_t *, struct nfs_open_file **);
int     nfs4_named_attr_remove(nfsnode_t, nfsnode_t, const char *, vfs_context_t);
#endif

int     nfs_mount_state_in_use_start(struct nfsmount *, thread_t);
int     nfs_mount_state_in_use_end(struct nfsmount *, int);
int     nfs_mount_state_error_should_restart(int);
int     nfs_mount_state_error_delegation_lost(int);
uint    nfs_mount_state_max_restarts(struct nfsmount *);
int     nfs_mount_state_wait_for_recovery(struct nfsmount *);
void    nfs_need_recover(struct nfsmount *nmp, int error);
void    nfs_recover(struct nfsmount *);

int     nfs_vnop_access(struct vnop_access_args *);
int     nfs_vnop_remove(struct vnop_remove_args *);
int     nfs_vnop_read(struct vnop_read_args *);
int     nfs_vnop_write(struct vnop_write_args *);
int     nfs_vnop_open(struct vnop_open_args *);
int     nfs_vnop_close(struct vnop_close_args *);
int     nfs_vnop_advlock(struct vnop_advlock_args *);
int     nfs_vnop_mmap(struct vnop_mmap_args *);
int     nfs_vnop_mmap_check(struct vnop_mmap_check_args *ap);
int     nfs_vnop_mnomap(struct vnop_mnomap_args *);

#if CONFIG_NFS4
int     nfs4_vnop_create(struct vnop_create_args *);
int     nfs4_vnop_mknod(struct vnop_mknod_args *);
int     nfs4_vnop_getattr(struct vnop_getattr_args *);
int     nfs4_vnop_link(struct vnop_link_args *);
int     nfs4_vnop_mkdir(struct vnop_mkdir_args *);
int     nfs4_vnop_rmdir(struct vnop_rmdir_args *);
int     nfs4_vnop_symlink(struct vnop_symlink_args *);
int     nfs4_vnop_getxattr(struct vnop_getxattr_args *);
int     nfs4_vnop_setxattr(struct vnop_setxattr_args *);
int     nfs4_vnop_removexattr(struct vnop_removexattr_args *);
int     nfs4_vnop_listxattr(struct vnop_listxattr_args *);
#if NAMEDSTREAMS
int     nfs4_vnop_getnamedstream(struct vnop_getnamedstream_args *);
int     nfs4_vnop_makenamedstream(struct vnop_makenamedstream_args *);
int     nfs4_vnop_removenamedstream(struct vnop_removenamedstream_args *);
#endif

int     nfs4_access_rpc(nfsnode_t, u_int32_t *, int, vfs_context_t);
int     nfs4_getattr_rpc(nfsnode_t, mount_t, u_char *, size_t, int, vfs_context_t, struct nfs_vattr *, u_int64_t *);
int     nfs4_setattr_rpc(nfsnode_t, struct vnode_attr *, vfs_context_t);
int     nfs4_read_rpc_async(nfsnode_t, off_t, size_t, thread_t, kauth_cred_t, struct nfsreq_cbinfo *, struct nfsreq **);
int     nfs4_read_rpc_async_finish(nfsnode_t, struct nfsreq *, uio_t, size_t *, int *);
int     nfs4_write_rpc_async(nfsnode_t, uio_t, size_t, thread_t, kauth_cred_t, int, struct nfsreq_cbinfo *, struct nfsreq **);
int     nfs4_write_rpc_async_finish(nfsnode_t, struct nfsreq *, int *, size_t *, uint64_t *);
int     nfs4_readdir_rpc(nfsnode_t, struct nfsbuf *, vfs_context_t);
int     nfs4_readlink_rpc(nfsnode_t, char *, size_t *, vfs_context_t);
int     nfs4_commit_rpc(nfsnode_t, uint64_t, uint64_t, kauth_cred_t, uint64_t);
int     nfs4_lookup_rpc_async(nfsnode_t, char *, int, vfs_context_t, struct nfsreq **);
int     nfs4_lookup_rpc_async_finish(nfsnode_t, char *, int, vfs_context_t, struct nfsreq *, u_int64_t *, fhandle_t *, struct nfs_vattr *);
int     nfs4_remove_rpc(nfsnode_t, char *, int, thread_t, kauth_cred_t);
int     nfs4_rename_rpc(nfsnode_t, char *, int, nfsnode_t, char *, int, vfs_context_t);
int     nfs4_pathconf_rpc(nfsnode_t, struct nfs_fsattr *, vfs_context_t);
int     nfs4_setlock_rpc(nfsnode_t, struct nfs_open_file *, struct nfs_file_lock *, int, int, thread_t, kauth_cred_t);
int     nfs4_unlock_rpc(nfsnode_t, struct nfs_lock_owner *, int, uint64_t, uint64_t, int, thread_t, kauth_cred_t);
int     nfs4_getlock_rpc(nfsnode_t, struct nfs_lock_owner *, struct flock *, uint64_t, uint64_t, vfs_context_t);
#endif

int     nfs_read_rpc(nfsnode_t, uio_t, vfs_context_t);
int     nfs_write_rpc(nfsnode_t, uio_t, thread_t, kauth_cred_t, int *, uint64_t *);

int     nfs3_access_rpc(nfsnode_t, u_int32_t *, int, vfs_context_t);
int     nfs3_getattr_rpc(nfsnode_t, mount_t, u_char *, size_t, int, vfs_context_t, struct nfs_vattr *, u_int64_t *);
int     nfs3_setattr_rpc(nfsnode_t, struct vnode_attr *, vfs_context_t);
int     nfs3_read_rpc_async(nfsnode_t, off_t, size_t, thread_t, kauth_cred_t, struct nfsreq_cbinfo *, struct nfsreq **);
int     nfs3_read_rpc_async_finish(nfsnode_t, struct nfsreq *, uio_t, size_t *, int *);
int     nfs3_write_rpc_async(nfsnode_t, uio_t, size_t, thread_t, kauth_cred_t, int, struct nfsreq_cbinfo *, struct nfsreq **);
int     nfs3_write_rpc_async_finish(nfsnode_t, struct nfsreq *, int *, size_t *, uint64_t *);
int     nfs3_readdir_rpc(nfsnode_t, struct nfsbuf *, vfs_context_t);
int     nfs3_readlink_rpc(nfsnode_t, char *, size_t *, vfs_context_t);
int     nfs3_commit_rpc(nfsnode_t, uint64_t, uint64_t, kauth_cred_t, uint64_t);
int     nfs3_lookup_rpc_async(nfsnode_t, char *, int, vfs_context_t, struct nfsreq **);
int     nfs3_lookup_rpc_async_finish(nfsnode_t, char *, int, vfs_context_t, struct nfsreq *, u_int64_t *, fhandle_t *, struct nfs_vattr *);
int     nfs3_remove_rpc(nfsnode_t, char *, int, thread_t, kauth_cred_t);
int     nfs3_rename_rpc(nfsnode_t, char *, int, nfsnode_t, char *, int, vfs_context_t);
int     nfs3_pathconf_rpc(nfsnode_t, struct nfs_fsattr *, vfs_context_t);
int     nfs3_setlock_rpc(nfsnode_t, struct nfs_open_file *, struct nfs_file_lock *, int, int, thread_t, kauth_cred_t);
int     nfs3_unlock_rpc(nfsnode_t, struct nfs_lock_owner *, int, uint64_t, uint64_t, int, thread_t, kauth_cred_t);
int     nfs3_getlock_rpc(nfsnode_t, struct nfs_lock_owner *, struct flock *, uint64_t, uint64_t, vfs_context_t);

/* Client unload support */
int     nfs_isbusy(void);
void    nfs_hashes_free(void);
void    nfs_threads_terminate(void);

int     nfs_use_cache(struct nfsmount *);
void    nfs_up(struct nfsmount *, thread_t, int, const char *);
void    nfs_down(struct nfsmount *, thread_t, int, int, const char *, int);
void    nfs_msg(thread_t, const char *, const char *, int);

int     nfs_maperr(const char *, int);
#define NFS_MAPERR(ERR) nfs_maperr(__FUNCTION__, (ERR))
vm_offset_t nfs_kernel_hideaddr(void *);

#if CONFIG_TRIGGERS
resolver_result_t nfs_mirror_mount_trigger_resolve(vnode_t, const struct componentname *, enum path_operation, int, void *, vfs_context_t);
resolver_result_t nfs_mirror_mount_trigger_unresolve(vnode_t, int, void *, vfs_context_t);
resolver_result_t nfs_mirror_mount_trigger_rearm(vnode_t, int, void *, vfs_context_t);
int     nfs_mirror_mount_domount(vnode_t, vnode_t, vfs_context_t);
void    nfs_ephemeral_mount_harvester_start(void);
void    nfs_ephemeral_mount_harvester(__unused void *arg, __unused wait_result_t wr);
#endif

extern uint32_t nfsclnt_debug_ctl;

/* Client debug support */
#define NFSCLNT_FAC_SOCK                 0x001
#define NFSCLNT_FAC_STATE                0x002
#define NFSCLNT_FAC_NODE                 0x004
#define NFSCLNT_FAC_VNOP                 0x008
#define NFSCLNT_FAC_BIO                  0x010
#define NFSCLNT_FAC_GSS                  0x020
#define NFSCLNT_FAC_VFS                  0x040
#define NFSCLNT_FAC_SRV                  0x080
#define NFSCLNT_FAC_KDBG                 0x100
#define NFSCLNT_DEBUG_LEVEL              __NFS_DEBUG_LEVEL(nfsclnt_debug_ctl)
#define NFSCLNT_DEBUG_FACILITY           __NFS_DEBUG_FACILITY(nfsclnt_debug_ctl)
#define NFSCLNT_DEBUG_FLAGS              __NFS_DEBUG_FLAGS(nfsclnt_debug_ctl)
#define NFSCLNT_DEBUG_VALUE              __NFS_DEBUG_VALUE(nfsclnt_debug_ctl)
#define NFSCLNT_IS_DBG(fac, lev)         __NFS_IS_DBG(nfsclnt_debug_ctl, fac, lev)
#define NFSCLNT_DBG(fac, lev, fmt, ...)  __NFS_DBG(nfsclnt_debug_ctl, fac, lev, fmt, ## __VA_ARGS__)

#endif /* _NFS_CLNT_H_ */
