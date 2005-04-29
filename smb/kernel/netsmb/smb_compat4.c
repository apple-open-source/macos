/*
 * Compatibility shims with RELENG_4
 *
 * $Id: smb_compat4.c,v 1.18 2004/10/12 23:03:56 lindak Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/lock.h>

#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/unistd.h>

#include <stdarg.h>

#include <sys/smb_apple.h>
#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>

#include <netsmb/smb_compat4.h>

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
PRIVSYM struct mbuf *
m_getm(struct mbuf *m, int len, int how, int type)
{
	struct mbuf *top, *tail, *mp, *mtail = NULL;

	KASSERT(len >= 0, ("len is < 0 in m_getm"));

	MGET(mp, how, type);
	if (mp == NULL)
		return (NULL);
	else if (len > (int)MINCLSIZE) {
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
		if (len > (int)MINCLSIZE) {
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
