/*
 * Copyright (c) 2001 - 2012 Apple Inc. All rights reserved.
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

#define M_SMBFSMNT M_TEMP /* HACK XXX CSM */
#define M_SMBNODE M_TEMP /* HACK XXX CSM */
#define M_SMBNODENAME M_TEMP /* HACK XXX CSM */
#define M_SMBFSDATA M_TEMP /* HACK XXX CSM */
#define M_SMBFSHASH M_TEMP /* HACK XXX CSM */
#define M_SMBFSFID M_TEMP /* HACK XXX CSM */
#define M_SMBCONN M_TEMP /* HACK CSM XXX */
#define M_NSMBDEV M_TEMP /* HACK CSM XXX */
#define M_SMBIOD M_TEMP /* HACK CSM XXX */
#define M_SMBRQ M_TEMP /* HACK CSM XXX */
#define M_SMBDATA M_TEMP /* HACK CSM XXX */
#define M_SMBSTR M_TEMP /* HACK CSM XXX */
#define M_SMBTEMP M_TEMP /* HACK CSM XXX */

#define SMB_MALLOC(addr, cast, size, type, flags) do { MALLOC((addr), cast, (size), (type), (flags)); } while(0)

#ifndef SMB_DEBUG
#define SMB_FREE(addr, type)	do { if (addr) FREE(addr, type); addr = NULL; } while(0)
#else   // SMB_DEBUG
#define SMB_FREE(addr, type) do { \
    if (addr) {\
        FREE(addr, type); \
    } \
    else { \
        SMBERROR("%s: attempt to free NULL pointer, line %d\n", __FUNCTION__, __LINE__); \
    } \
    addr = NULL; \
} while(0)
#endif  // SMB_DEBUG

#undef FB_CURRENT

/* Max number of times we will attempt to open in a reconnect */
#define SMB_MAX_REOPEN_CNT	25

typedef enum modeventtype {
	MOD_LOAD,
	MOD_UNLOAD,
	MOD_SHUTDOWN
} modeventtype_t;

typedef struct kmod_info *module_t;

typedef int (*modeventhand_t)(module_t mod, int what, void *arg);

typedef struct moduledata {
	const char		*name;  /* module name */
	modeventhand_t  evhand; /* event handler */
	void		*priv;  /* extra data */
} moduledata_t;

#define DECLARE_MODULE(name, data, sub, order)				\
	moduledata_t * _smb_md_##name = &data;
#define SEND_EVENT(name, event)						\
	{								\
		extern moduledata_t * _smb_md_##name; 		\
		if (_smb_md_##name) \
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
extern int	smb_smb_flush __P((struct smbnode *, vfs_context_t));

typedef int	 vnop_t __P((void *));

#define vn_todev(vp) (vnode_vtype(vp) == VBLK || vnode_vtype(vp) == VCHR ? \
		      vnode_specrdev(vp) : NODEV)

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
