/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
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
 * $Id: smb_subr.h,v 1.13 2004/09/14 22:59:08 lindak Exp $
 */
#ifndef _NETSMB_SMB_SUBR_H_
#define _NETSMB_SMB_SUBR_H_

#ifndef _KERNEL
#error "This file shouldn't be included from userland programs"
#endif

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_SMBTEMP);
#endif

#define SMBERROR(format, args...) printf("%s: "format, __FUNCTION__ ,## args)
#define SMBPANIC(format, args...) printf("%s: "format, __FUNCTION__ ,## args)
#define SMBASSERT(expr) \
	((expr) ? 1 \
		: (printf("%s, line %d, assert failure: %s\n", __FILE__, \
			  __LINE__, #expr), \
		   0))

#ifdef SMB_SOCKET_DEBUG
#define SMBSDEBUG(format, args...) printf("%s: "format, __FUNCTION__ ,## args)
#else
#define SMBSDEBUG(format, args...)
#endif

#ifdef SMB_IOD_DEBUG
#define SMBIODEBUG(format, args...) printf("%s: "format, __FUNCTION__ ,## args)
#else
#define SMBIODEBUG(format, args...)
#endif

#ifdef SMB_SOCKETDATA_DEBUG
void m_dumpm(struct mbuf *m);
#else
#define m_dumpm(m)
#endif

#define	SMB_SIGMASK	(sigmask(SIGINT)|sigmask(SIGTERM)|sigmask(SIGKILL)| \
			 sigmask(SIGHUP)|sigmask(SIGQUIT))

/*
 * Compatibility wrappers for simple locks
 */

#include <sys/lock.h>

#define	lockdestroy(lock)
#define	smb_slock			lck_mtx_t
#define	smb_sl_init(mtx, grp, attr)	lck_mtx_init((mtx), (grp), (attr))
#define	smb_sl_destroy(mtx, grp)	lck_mtx_destroy((mtx), (grp))
#define	smb_sl_lock(mtx)		lck_mtx_lock(mtx)
#define	smb_sl_unlock(mtx)		lck_mtx_unlock(mtx)

#define SMB_STRFREE(p)	do { if (p) smb_strfree(p); } while(0)

/*
 * The simple try/catch/finally interface.
 * With GCC it is possible to allow more than one try/finally block per
 * function, but we'll avoid it to maintain portability.
 */
#define itry		{						\
				__label__ _finlab, _catchlab;		\
				int _tval;				\

#define icatch(var)							\
				goto _finlab;				\
				(void)&&_catchlab;			\
				_catchlab:				\
				var = _tval;

#define ifinally		(void)&&_finlab;			\
				_finlab:				
#define iendtry		}

#define inocatch							\
				goto _finlab;				\
				(void)&&_catchlab;			\
				_catchlab:				\

#define ithrow(t)	do {						\
				if ((_tval = (int)(t)) != 0)		\
					goto _catchlab;			\
			} while (0)

#define ierror(t,e)	do {						\
				if (t) {				\
					_tval = e;			\
					goto _catchlab;			\
				}					\
			} while (0)

typedef u_int16_t	smb_unichar;
typedef	smb_unichar	*smb_uniptr;

/*
 * Crediantials of user/process being processing in the connection procedures
 */
struct smb_cred {
	vfs_context_t	scr_vfsctx;
};

extern smb_unichar smb_unieol;

struct mbchain;
struct smb_vc;
struct smb_rq;

#define EMOREDATA (0x7fff)

void smb_scred_init(struct smb_cred *scred, vfs_context_t vfsctx);
int  smb_sigintr(vfs_context_t);
char *smb_strdup(const char *s);
void *smb_memdup(const void *umem, int len);
char *smb_strdupin(char *s, int maxlen);
void *smb_memdupin(void *umem, int len);
size_t smb_strtouni(u_int16_t *dst, const char *src, size_t inlen, int flags);
void smb_strfree(char *s);
void smb_memfree(void *s);
void *smb_zmalloc(unsigned long size, int type, int flags);

int  smb_lmresponse(const u_char *apwd, u_char *C8, u_char *RN);
int  smb_ntlmresponse(const u_char *apwd, u_char *C8, u_char *RN);
int  smb_ntlmv2response(const u_char *apwd, const u_char *user,
	const u_char *destination, u_char *C8, const u_char *blob,
	size_t bloblen, u_char **RN, size_t *RNlen);
int  smb_maperror(int eclass, int eno);
u_int32_t  smb_maperr32(u_int32_t eno);
int  smb_put_dmem(struct mbchain *mbp, struct smb_vc *vcp,
	const char *src, int len, int caseopt, int *lenp);
int  smb_put_dstring(struct mbchain *mbp, struct smb_vc *vcp,
	const char *src, int caseopt);
int  smb_put_string(struct smb_rq *rqp, const char *src);
int  smb_put_asunistring(struct smb_rq *rqp, const char *src);
int  smb_checksmp(void);
struct sockaddr *smb_dup_sockaddr(struct sockaddr *sa, int canwait);

#endif /* !_NETSMB_SMB_SUBR_H_ */
