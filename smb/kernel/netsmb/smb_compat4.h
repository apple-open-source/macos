/*
 * Compatibility shims with RELENG_4
 *
 * $Id: smb_compat4.h,v 1.7 2004/08/03 23:50:00 lindak Exp $
 */
#ifndef SMB_COMPAT4_H
#define SMB_COMPAT4_H

#include <sys/param.h>

#define	MODULE_VERSION(name, ver)

#define	M_TRYWAIT	0	/* M_WAITOK */

struct mbuf;
struct proc;
struct simplelock;

struct mbuf *m_getm(struct mbuf *m, int len, int how, int type);
int kthread_create2(void (*func)(void *), void *arg,
    struct proc **newpp, int flags, const char *fmt, ...);
#endif	/* !SMB_COMPAT4_H */
