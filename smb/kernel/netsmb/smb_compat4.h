/*
 * Compatibility shims with RELENG_4
 *
 * $Id: smb_compat4.h,v 1.3 2001/07/07 00:16:58 conrad Exp $
 */
#ifndef SMB_COMPAT4_H
#define SMB_COMPAT4_H

#include <sys/param.h>

#if defined(APPLE) || __FreeBSD_version < 500000
#define	MODULE_VERSION(name, ver)

#define	M_TRYWAIT	0	/* M_WAITOK */
#define	PDROP	0x200	/* OR'd with pri to stop re-entry of interlock mutex */

struct mbuf;
struct proc;
struct simplelock;

struct mbuf *m_getm(struct mbuf *m, int len, int how, int type);
int kthread_create2(void (*func)(void *), void *arg,
    struct proc **newpp, int flags, const char *fmt, ...);
int msleep(void *chan, struct simplelock *mtx, int pri, const char *wmesg, int timo);

#endif	/* __FreeBSD_version < 500000 */
#endif	/* !SMB_COMPAT4_H */
