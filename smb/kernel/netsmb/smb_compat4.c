/*
 * Compatibility shims with RELENG_4
 *
 * $Id: smb_compat4.c,v 1.9 2002/03/12 22:06:10 lindak Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/lock.h>

#ifndef APPLE
#include <sys/kthread.h>
#endif
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/wait.h>
#include <sys/unistd.h>

#ifdef APPLE
#include <stdarg.h>
#else
#include <machine/stdarg.h>
#endif

#ifdef APPLE
#include <sys/smb_apple.h>
#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <IOKit/IOLib.h>
#endif

#include <netsmb/smb_compat4.h>

#if defined(APPLE) || __FreeBSD_version < 500000
/*
 * This will allocate len-worth of mbufs and/or mbuf clusters (whatever fits
 * best) and return a pointer to the top of the allocated chain. If m is
 * non-null, then we assume that it is a single mbuf or an mbuf chain to
 * which we want len bytes worth of mbufs and/or clusters attached, and so
 * if we succeed in allocating it, we will just return a pointer to m.
 *
 * If we happen to fail at any point during the allocation, we will free
 * up everything we have already allocated and return NULL.
 *
 */
struct mbuf *
m_getm(struct mbuf *m, int len, int how, int type)
{
	struct mbuf *top, *tail, *mp, *mtail = NULL;

	KASSERT(len >= 0, ("len is < 0 in m_getm"));

	MGET(mp, how, type);
	if (mp == NULL)
		return (NULL);
	else if (len > MINCLSIZE) {
		MCLGET(mp, how);
		if ((mp->m_flags & M_EXT) == 0) {
			m_free(mp);
			return (NULL);
		}
	}
	mp->m_len = 0;
	len -= M_TRAILINGSPACE(mp);

	if (m != NULL)
		for (mtail = m; mtail->m_next != NULL; mtail = mtail->m_next);
	else
		m = mp;

	top = tail = mp;
	while (len > 0) {
		MGET(mp, how, type);
		if (mp == NULL)
			goto failed;

		tail->m_next = mp;
		tail = mp;
		if (len > MINCLSIZE) {
			MCLGET(mp, how);
			if ((mp->m_flags & M_EXT) == 0)
				goto failed;
		}

		mp->m_len = 0;
		len -= M_TRAILINGSPACE(mp);
	}

	if (mtail != NULL)
		mtail->m_next = top;
	return (m);

failed:
	m_freem(top);
	return (NULL);
}

/*
 * Create a kernel process/thread/whatever.  It shares it's address space
 * with proc0 - ie: kernel only.
 */
int
kthread_create2(void (*func)(void *), void *arg,
    struct proc **newpp, int flags, const char *fmt, ...)
{
#ifdef APPLE
	struct proc *p2;
#if APPLE_USE_CALLOUT_THREAD	/* XXX bad idea */
	struct smbiod *iod = (struct smbiod *)arg;

	iod->iod_tc = thread_call_allocate((thread_call_func_t)func,
					   (thread_call_param_t)arg);
	if (!iod->iod_tc)
		return (-1);
	(void) thread_call_enter(iod->iod_tc);
#else
	IOCreateThread(func, arg);
#endif
	p2 = kernproc;
#else
	int error;
	va_list ap;
	struct proc *p2;

	if (!proc0.p_stats || proc0.p_stats->p_start.tv_sec == 0) {
		panic("kthread_create called too soon");
	}

	error = fork1(&proc0, RFMEM | RFFDG | RFPROC | flags, &p2);
	if (error)
		return error;

#endif
	/* save a global descriptor, if desired */
	if (newpp != NULL)
		*newpp = p2;

#ifndef APPLE
	/* this is a non-swapped system process */
	p2->p_flag |= P_INMEM | P_SYSTEM;
	p2->p_procsig->ps_flag |= PS_NOCLDWAIT;
	PHOLD(p2);

	/* set up arg0 for 'ps', et al */
	va_start(ap, fmt);
	vsnprintf(p2->p_comm, sizeof(p2->p_comm), fmt, ap);
	va_end(ap);

	/* call the processes' main()... */
	cpu_set_fork_handler(p2, func, arg);

#endif
	return 0;
}

int
msleep(void *chan, struct simplelock *mtx, int pri, const char *wmesg, int timo)
{
	int error;

	if (mtx)
		simple_unlock(mtx);
#ifdef APPLE
	/* no PDROP */
	error = tsleep(chan, pri & ~PDROP, (char *)wmesg, timo);
#else
	error = tsleep(chan, pri, wmesg, timo);
#endif
	if ((pri & PDROP) == 0 && mtx)
		simple_lock(mtx);
	return error;
}

#endif
