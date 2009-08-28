/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2008 Apple Inc. All rights reserved.
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
#ifndef _NETSMB_SMB_SUBR_H_
#define _NETSMB_SMB_SUBR_H_

#ifndef _KERNEL
#error "This file shouldn't be included from userland programs"
#endif

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_SMBTEMP);
#endif


#define SMB_NO_LOG_LEVEL		0
#define SMB_LOW_LOG_LEVEL		1
#define SMB_ACL_LOG_LEVEL		2

extern int smbfs_loglevel;

#define SMB_PRNT_LOW(offset) (int32_t)(offset & 0x00000000ffffffff)
#define SMB_PRNT_HIGH(offset) (int32_t)((offset >> 32) & 0x00000000ffffffff)

#define SMB_PRETEND_TO_BE_AFP(mp) \
{	\
	struct vfsstatfs * st = vfs_statfs(mp);	\
	strlcpy(st->f_fstypename, "afpfs", MFSNAMELEN);	\
}

#ifdef SMB_DEBUG
#define SMBDEBUG(format, args...) printf("%s: "format, __FUNCTION__ ,## args)

#ifdef DEBUG_SYMBOLIC_LINKS
#define SMBSYMDEBUG	SMBDEBUG
#else // DEBUG_SYMBOLIC_LINKS
#define SMBSYMDEBUG(format, args...)
#endif // DEBUG_SYMBOLIC_LINKS
#else SMB_DEBUG
#define SMBDEBUG(format, args...)
#define SMBSYMDEBUG(format, args...)
#endif SMB_DEBUG

#define SMBERROR(format, args...) printf("%s: "format, __FUNCTION__ ,## args)
#define SMBWARNING(format, args...) do { \
	if (smbfs_loglevel) \
		printf("%s: "format, __FUNCTION__ ,## args); \
}while(0)

#ifdef SMB_DEBUG
#define DBG_ASSERT(a) { if (!(a)) { panic("File "__FILE__", line %d: assertion '%s' failed.\n", __LINE__, #a); } }
#define DBG_LOCKLIST_ASSERT(lock_cnt, lock_order) { \
	int jj, kk; \
	for (jj=0; jj < lock_cnt; jj++) \
		for (kk=0; kk < lock_cnt; kk++)	\
			if ((jj != kk) && (lock_order[jj] == lock_order[kk]))	\
				panic("lock_order[%d] == lock_order[%d]", jj, kk); \
}

#else // SMB_DEBUG
#define DBG_ASSERT(a)
#define DBG_LOCKLIST_ASSERT(lock_cnt, lock_order)
#endif // SMB_DEBUG

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
void m_dumpm(mbuf_t m);
#else
#define m_dumpm(m)
#endif

#define	SMB_SIGMASK	(sigmask(SIGINT)|sigmask(SIGTERM)|sigmask(SIGKILL)| \
			 sigmask(SIGHUP)|sigmask(SIGQUIT))

#define UNIX_SERVER(a) ((a)->vc_sopt.sv_caps & SMB_CAP_UNIX)
#define UNIX_CAPS(a) (a)->vc_sopt.sv_unix_caps

/*
 * Compatibility wrappers for simple locks
 */

#include <sys/lock.h>

#define SMB_STRFREE(p)	do { if (p) free(p, M_SMBSTR);  p = NULL; } while(0)

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

struct mbchain;
struct smb_vc;
struct smb_rq;

#define EMOREDATA (0x7fff)

#ifdef SMB_DEBUG
void smb_hexdump(const char *func, const char *s, unsigned char *buf, size_t inlen);
#else // SMB_DEBUG
#define smb_hexdump(a,b,c,d)
#endif // SMB_DEBUG
int  smb_sigintr(vfs_context_t);
char *smb_strdup(const char *s, size_t maxlen);
void *smb_memdup(const void *umem, int len);
void *smb_memdupin(user_addr_t umem, int len);

void smb_memfree(void *s);
void *smb_zmalloc(size_t size, int type, int flags);

void smb_reset_sig(struct smb_vc *vcp);
void smb_calcmackey(struct smb_vc *vcp, void *ntlm, size_t resplen);
void smb_calcv2mackey(struct smb_vc *vcp, void *v2hash, void *ntlmv2, void *resp, size_t resplen);
int  smb_lmresponse(const u_char *apwd, u_char *C8, u_char *RN);
int  smb_ntlmresponse(const u_char *apwd, u_char *C8, u_char *RN);
void smb_ntlmv2hash(uint8_t *ntlmv2Hash, const void * domain, const void * user, const void * password);
void *smb_lmv2_response(void *ntlmv2Hash, u_int64_t server_nonce, u_int64_t client_nonce, size_t *lmv2_len);
void smb_ntlmv2_response(void *ntlmv2Hash, void *ntlmv2, size_t ntlmv2_len, u_int64_t server_nonce);
void *make_target_info(struct smb_vc *vcp, u_int16_t *target_len);
uint8_t *make_ntlmv2_blob(u_int64_t client_nonce, void *target_info, u_int16_t target_len, size_t *blob_len);
int  smb_maperror(int eclass, int eno);
u_int32_t  smb_maperr32(u_int32_t eno);
int  smb_put_dmem(struct mbchain *mbp, struct smb_vc *vcp, const char *src, size_t len, int flags, size_t *lenp);
int  smb_put_dstring(struct mbchain *mbp, struct smb_vc *vcp, const char *src, size_t maxlen, int flags);
int  smb_put_string(struct smb_rq *rqp, const char *src);
int  smb_put_asunistring(struct smb_rq *rqp, const char *src);
struct sockaddr *smb_dup_sockaddr(struct sockaddr *sa, int canwait);
int  smb_rq_sign(struct smb_rq *rqp);
int  smb_rq_verify(struct smb_rq *rqp);
void smb_get_username_from_kcpn(struct smb_vc *vcp, char *kuser, size_t maxlen);
#endif /* !_NETSMB_SMB_SUBR_H_ */
