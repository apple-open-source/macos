#ifdef KERNEL
#define _KERNEL

#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/socketvar.h>
#include <sys/ubc.h>
#include <miscfs/specfs/specdev.h>
#include <miscfs/devfs/devfs.h>
#include <machine/spl.h>
#include <kern/thread_act.h>
#include <kern/thread_call.h>
#include <kern/wait_queue.h>
#include <string.h>

#undef KASSERT
#define KASSERT(exp,msg)	do { if (!(exp)) panic msg; } while (0)

#define curproc current_proc()
#define vnode_pager_setsize ubc_setsize /* works since n_size is quad */

#define lf_advlock(a, b, c) (0)

#define VFS_SET(a, b, c)
#define vop_defaultop vn_default_error
#define VNODEOP_SET(a)

#define _kern_iconv	_net_smb_fs_iconv

/* restore iconv.h stuff which vanished with 1.4.1 */
typedef u_int32_t       ucs_t;
#ifdef KOBJ_CLASS_FIELDS
struct iconv_ces_class {
	KOBJ_CLASS_FIELDS;
	TAILQ_ENTRY(iconv_ces_class)    cd_link;
};
#else
struct iconv_ces_class;
#endif
#ifdef KOBJ_FIELDS
struct iconv_ces {
	KOBJ_FIELDS;
};
#else
struct iconv_ces;
#endif
extern int  iconv_ces_initstub __P((struct iconv_ces_class *));
extern int  iconv_ces_donestub __P((struct iconv_ces_class *));
extern void iconv_ces_noreset __P((struct iconv_ces *));

/*
 * BSD malloc of 0 bytes returns at least 1 byte area.
 * NeXT malloc returns zero (ala ENOMEM).  There is no standard.
 * Beware side effects in 1st argument.
 */
#define malloc(s, t, f) (_MALLOC((s) ? (s) : (1), (t), (f)))

#define free(a, b) (FREE((a), (b)))

#define M_USE_RESERVE M_WAITOK
#define M_SMBFSMNT M_TEMP /* HACK XXX CSM */
#define M_SMBNODE M_TEMP /* HACK XXX CSM */
#define M_SMBNODENAME M_TEMP /* HACK XXX CSM */
#define M_SMBFSDATA M_TEMP /* HACK XXX CSM */
#define M_SMBFSHASH M_TEMP /* HACK XXX CSM */
#define M_ICONV M_TEMP /* XXX HACK CSM */
#define M_ICONVDATA M_TEMP /* XXX HACK CSM */
#define M_SMBCONN M_TEMP /* HACK CSM XXX */
#define M_NSMBDEV M_TEMP /* HACK CSM XXX */
#define M_SMBIOD M_TEMP /* HACK CSM XXX */
#define M_SMBRQ M_TEMP /* HACK CSM XXX */
#define M_SMBDATA M_TEMP /* HACK CSM XXX */
#define M_SMBSTR M_TEMP /* HACK CSM XXX */
#define M_SMBTEMP M_TEMP /* HACK CSM XXX */
#define M_KOBJ M_TEMP /* XXX HACK CSM */

#undef FB_CURRENT

typedef enum modeventtype {
	MOD_LOAD,
	MOD_UNLOAD,
	MOD_SHUTDOWN
} modeventtype_t;
typedef struct kmod_info *module_t;
typedef int (*modeventhand_t)(module_t mod, int what, void *arg);
typedef struct moduledata {
	char		*name;  /* module name */
	modeventhand_t  evhand; /* event handler */
	void		*priv;  /* extra data */
} moduledata_t;
#define DECLARE_MODULE(name, data, sub, order)				\
	moduledata_t * _smb_md_##name = &data;
#define SEND_EVENT(name, event)						\
	{								\
		extern moduledata_t * _smb_md_##name;			\
		_smb_md_##name->evhand(smbfs_kmod_infop,		\
					 event,				\
					 _smb_md_##name->priv);		\
	}
#define DEV_MODULE(name, evh, arg) \
	static moduledata_t name##_mod = {	\
		#name,				\
		evh,				\
		arg				\
	};					\
	DECLARE_MODULE(name, name##_mod, SI_SUB_DRIVERS, SI_ORDER_ANY);
#define CDEV_MODULE(a, b, c, d, e) DEV_MODULE(a, d, e)
#define MODULE_DEPEND(a, b, c, d, e)

struct smbnode;
struct smb_cred;
extern int	smbfs_smb_flush __P((struct smbnode *, struct smb_cred *));
extern int	smb_smb_flush __P((struct smbnode *, struct smb_cred *));
extern int	smbfs_0extend __P((struct vnode *, u_quad_t, u_quad_t,
				 struct smb_cred *, struct proc *));

extern unsigned		splbio __P((void));
extern void		splx __P((unsigned));
extern char *		strchr __P((const char *, int));
#define index strchr
extern int		groupmember __P((gid_t, struct ucred *));
extern void		wakeup_one __P((caddr_t chan));
extern int selwait;
extern void		m_cat __P((struct mbuf *, struct mbuf *));
extern int		tvtohz __P((struct timeval *));

extern void wait_queue_sub_init __P((wait_queue_sub_t, int));
extern kern_return_t wait_subqueue_unlink_all __P((wait_queue_sub_t));


extern void		smb_vhashrem __P((struct vnode *, struct proc *));

typedef int	 vop_t __P((void *));

#define vn_todev(vp) ((vp)->v_type == VBLK || (vp)->v_type == VCHR ? \
		      (vp)->v_rdev : NODEV)
#define RFNOWAIT	(1<<6)
#define kthread_exit(a)
#define simplelock	slock

typedef __const char *  c_caddr_t;

void timevaladd(struct timeval *, struct timeval *);
void timevalsub(struct timeval *, struct timeval *);
#define timevalcmp(l, r, cmp)	timercmp(l, r, cmp)
#define timespeccmp(tvp, uvp, cmp)				  \
	(((tvp)->tv_sec == (uvp)->tv_sec) ?			 \
	    ((tvp)->tv_nsec cmp (uvp)->tv_nsec) :		   \
	    ((tvp)->tv_sec cmp (uvp)->tv_sec))			  
#define timespecadd(vvp, uvp)					   \
	do {							\
		(vvp)->tv_sec += (uvp)->tv_sec;			 \
		(vvp)->tv_nsec += (uvp)->tv_nsec;		   \
		if ((vvp)->tv_nsec >= 1000000000) {		 \
			(vvp)->tv_sec++;				\
			(vvp)->tv_nsec -= 1000000000;		   \
		}						   \
	} while (0)
#define timespecsub(vvp, uvp)					   \
	do {							\
		(vvp)->tv_sec -= (uvp)->tv_sec;			 \
		(vvp)->tv_nsec -= (uvp)->tv_nsec;		   \
		if ((vvp)->tv_nsec < 0) {			   \
			(vvp)->tv_sec--;				\
			(vvp)->tv_nsec += 1000000000;		   \
		}						   \
	} while (0)

extern int smbtraceindx;
#define SMBTBUFSIZ 8912 
struct smbtracerec { uint i1, i2, i3, i4; };
extern struct smbtracerec smbtracebuf[SMBTBUFSIZ];
extern uint smbtracemask; /* 32 bits - trace points over 31 are unconditional */

#define SMBTRC_LKP_LOOKUP	0	/* smbfs_lookup */
#define SMBTRC_CRT_ENTER	1	/* smbfs_create */
#define SMBTRC_LKP_ENTER	2	/* smbfs_lookup */
#define SMBTRC_LKP_PURGE	17	/* smbfs_lookup */
#define SMBTRC_RCLM_PURGE	18	/* smbfs_reclaim */
#define SMBTRC_RMV_PURGE	19	/* smbfs_remove */
#define SMBTRC_RNM_TD_PURGE	20	/* smbfs_rename */
#define SMBTRC_RNM_FD_PURGE	21	/* smbfs_rename */
#define SMBTRC_RMD_PURGE	22	/* smbfs_rmdir */
#define SMBTRC_SIZE_WV		24	/* smb_writevnode */
#define SMBTRC_SIZE_ACE		25	/* smb_attr cache enter */
#define SMBTRC_SIZE_ACL		26	/* smb_attr cache lookup */
#define SMBTRC_SIZE_GA		27	/* smbfs_getattr */
#define SMBTRC_SIZE_SA		28	/* smbfs_setattr */
#define SMBTRC_SIZE_SAE		29	/* smbfs_setattr error */
#define SMBTRC_CONTINUE		0xff
#define SMBTRACEX(a1, a2, a3, a4) \
( \
	smbtracebuf[smbtraceindx].i1 = (uint)(a1), \
	smbtracebuf[smbtraceindx].i2 = (uint)(a2), \
	smbtracebuf[smbtraceindx].i3 = (uint)(a3), \
	smbtracebuf[smbtraceindx].i4 = (uint)(a4), \
	smbtraceindx = (smbtraceindx + 1) % SMBTBUFSIZ, \
	1 \
)
#define SMBTRACE(cnst, a1, a2, a3, a4) \
( \
	(smbtracemask && ((cnst) > 31 || smbtracemask & 1<<(cnst))) ? \
		(SMBTRACEX((cnst), current_thread(), \
			   clock_get_system_value().tv_nsec, (a1)), \
		 SMBTRACEX(SMBTRC_CONTINUE, (a2), (a3), (a4))) : \
		0 \
)
#endif /* KERNEL */
