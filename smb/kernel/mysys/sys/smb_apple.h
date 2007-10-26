/*
 * Copyright (c) 2001 - 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifdef KERNEL
#define _KERNEL

#include <sys/buf.h>
#include <sys/kpi_mbuf.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/ubc.h>
#include <miscfs/specfs/specdev.h>
#include <miscfs/devfs/devfs.h>
#include <machine/spl.h>
#include <kern/thread.h>
#include <kern/thread_call.h>
#include <string.h>

#ifndef PRIVSYM
#define PRIVSYM __private_extern__
#endif

#undef KASSERT
#define KASSERT(exp,msg)	do { if (!(exp)) panic msg; } while (0)

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

struct smbnode;
struct smb_cred;
extern int	smb_smb_flush __P((struct smbnode *, struct smb_cred *));
extern int	smbfs_0extend __P((vnode_t, u_int16_t, u_quad_t, u_quad_t,
				 struct smb_cred *, int));

extern unsigned		splbio __P((void));
extern void		splx __P((unsigned));
extern char *		strchr __P((const char *, int));
#define index strchr
extern int		groupmember __P((gid_t, kauth_cred_t));
extern void		wakeup_one __P((caddr_t chan));
extern int selwait;
extern int		tvtohz __P((struct timeval *));

typedef int	 vnop_t __P((void *));

#define vn_todev(vp) (vnode_vtype(vp) == VBLK || vnode_vtype(vp) == VCHR ? \
		      vnode_specrdev(vp) : NODEV)
#define RFNOWAIT	(1<<6)
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

#endif /* KERNEL */
