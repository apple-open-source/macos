/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2010 Apple Inc. All rights reserved.
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


#define SMB_NO_LOG_LEVEL		0x00
#define SMB_LOW_LOG_LEVEL		0x01
#define SMB_ACL_LOG_LEVEL		0x02
#define SMB_IO_LOG_LEVEL		0x04
#define SMB_AUTH_LOG_LEVEL		0x08

extern int smbfs_loglevel;

#ifdef SMB_DEBUG
#define SMBDEBUG(format, args...) printf("%s: "format, __FUNCTION__ ,## args)

#ifdef DEBUG_SYMBOLIC_LINKS
#define SMBSYMDEBUG	SMBDEBUG
#else // DEBUG_SYMBOLIC_LINKS
#define SMBSYMDEBUG(format, args...)
#endif // DEBUG_SYMBOLIC_LINKS
#else // SMB_DEBUG
#define SMBDEBUG(format, args...)
#define SMBSYMDEBUG(format, args...)
#endif // SMB_DEBUG

#define SMBERROR(format, args...) printf("%s: "format, __FUNCTION__ ,## args)
#define SMBWARNING(format, args...) do { \
	if (smbfs_loglevel) \
		printf("%s: "format, __FUNCTION__ ,## args); \
}while(0)


#define SMB_LOG_AUTH(format, args...) do { \
if (smbfs_loglevel & SMB_AUTH_LOG_LEVEL) \
printf("%s: "format, __FUNCTION__ ,## args); \
}while(0)

#define SMB_LOG_ACCESS(format, args...) do { \
	if (smbfs_loglevel & SMB_ACL_LOG_LEVEL) \
	printf("%s: "format, __FUNCTION__ ,## args); \
}while(0)

#define SMB_LOG_IO(format, args...) do { \
if (smbfs_loglevel & SMB_IO_LOG_LEVEL) \
printf("%s: "format, __FUNCTION__ ,## args); \
}while(0)

#define SMB_ASSERT(a) { \
	if (!(a)) { \
		panic("File "__FILE__", line %d: assertion '%s' failed.\n", \
		__LINE__, #a); \
	} \
}

#ifdef SMB_DEBUG
#define DBG_ASSERT SMB_ASSERT
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

/*
 * Compatibility wrappers for simple locks
 */

#include <sys/lock.h>

typedef uint16_t	smb_unichar;
typedef	smb_unichar	*smb_uniptr;

struct mbchain;
struct smb_vc;
struct smb_rq;

#ifdef SMB_DEBUG
void smb_hexdump(const char *func, const char *s, unsigned char *buf, size_t inlen);
#else // SMB_DEBUG
#define smb_hexdump(a,b,c,d)
#endif // SMB_DEBUG
char *smb_strndup(const char *s, size_t maxlen);
void *smb_memdup(const void *umem, int len);
void *smb_memdupin(user_addr_t umem, int len);

void smb_reset_sig(struct smb_vc *vcp);
int  smb_lmresponse(const u_char *apwd, u_char *C8, u_char *RN);
void  smb_ntlmresponse(const u_char *apwd, u_char *C8, u_char *RN);
void *make_target_info(struct smb_vc *vcp, uint16_t *target_len);
uint32_t smb_errClassCodes_to_ntstatus(uint8_t errClass, uint16_t errCode);
uint32_t smb_ntstatus_to_errno(uint32_t ntstatus);
int smb_put_dmem(struct mbchain *mbp, const char *src, size_t srcSize, 
				 int flags, int usingUnicode, size_t *lenp);
int smb_put_dstring(struct mbchain *mbp, int usingUnicode, const char *src, 
					size_t maxlen, int flags);
int  smb_put_string(struct smb_rq *rqp, const char *src);
int  smb_put_asunistring(struct smb_rq *rqp, const char *src);
struct sockaddr *smb_dup_sockaddr(struct sockaddr *sa, int canwait);
int  smb_rq_sign(struct smb_rq *rqp);
int  smb_rq_verify(struct smb_rq *rqp);
#endif /* !_NETSMB_SMB_SUBR_H_ */
