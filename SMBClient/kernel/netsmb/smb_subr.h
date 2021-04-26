/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2013 Apple Inc. All rights reserved.
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

#if (!defined(_KERNEL) && !defined(MC_TESTER))
#error "This file shouldn't be included from userland programs"
#endif

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_SMBTEMP);
#endif


#define SMB_NO_LOG_LEVEL		   0x0000
#define SMB_LOW_LOG_LEVEL		   0x0001
#define SMB_ACL_LOG_LEVEL		   0x0002
#define SMB_IO_LOG_LEVEL		   0x0004
#define SMB_AUTH_LOG_LEVEL		   0x0008
#define SMB_KTRACE_LOG_LEVEL       0x0010
#define SMB_DIR_CACHE_LOG_LEVEL    0x0020
#define SMB_DIR_CACHE_LOG_LEVEL2   0x0040
#define SMB_MC_LOG_LEVEL           0x0080
#define SMB_MC_REF_LOG_LEVEL       0x0100
#define SMB_UNIT_TEST			   0x8000

extern int smbfs_loglevel;

#ifdef SMB_DEBUG
#define SMBDEBUG(format, args...) printf("%s: " format, __FUNCTION__ ,## args)

#define SMBDEBUG_LOCK(np, format, args...) do { \
    lck_rw_lock_shared(&(np)->n_name_rwlock); \
    printf("%s: " format, __FUNCTION__ ,## args); \
    lck_rw_unlock_shared(&(np)->n_name_rwlock); \
    } while(0)

#ifdef DEBUG_SYMBOLIC_LINKS
#define SMBSYMDEBUG SMBDEBUG
#define SMBSYMDEBUG_LOCK SMBDEBUG_LOCK
#else // DEBUG_SYMBOLIC_LINKS
#define SMBSYMDEBUG(format, args...)
#define SMBSYMDEBUG_LOCK(np, format, args...)
#endif // DEBUG_SYMBOLIC_LINKS
#else // SMB_DEBUG
#define SMBDEBUG(format, args...)
#define SMBDEBUG_LOCK(np, format, args...)
#define SMBSYMDEBUG(format, args...)
#define SMBSYMDEBUG_LOCK(np, format, args...)
#endif // SMB_DEBUG

#define SMBERROR(format, args...) printf("%s: " format, __FUNCTION__ ,## args)

#define SMBERROR_LOCK(np, format, args...) do { \
    lck_rw_lock_shared(&(np)->n_name_rwlock); \
    printf("%s: " format, __FUNCTION__ ,## args); \
    lck_rw_unlock_shared(&(np)->n_name_rwlock); \
    } while(0)

#define SMBWARNING_LOCK(np, format, args...) do { \
    if (smbfs_loglevel) { \
        lck_rw_lock_shared(&(np)->n_name_rwlock); \
        printf("%s: " format, __FUNCTION__ ,## args); \
        lck_rw_unlock_shared(&(np)->n_name_rwlock); \
    } \
    } while(0)

#define SMBWARNING(format, args...) do { \
    if (smbfs_loglevel) \
        printf("%s: " format, __FUNCTION__ ,## args); \
    } while(0)

#define SMB_LOG_AUTH_LOCK(np, format, args...) do { \
    if (smbfs_loglevel & SMB_AUTH_LOG_LEVEL) { \
        lck_rw_lock_shared(&(np)->n_name_rwlock); \
        printf("%s: " format, __FUNCTION__ ,## args); \
        lck_rw_unlock_shared(&(np)->n_name_rwlock); \
    } \
    } while(0)

#define SMB_LOG_AUTH(format, args...) do { \
    if (smbfs_loglevel & SMB_AUTH_LOG_LEVEL) \
        printf("%s: " format, __FUNCTION__ ,## args); \
    } while(0)

#define SMB_LOG_ACCESS_LOCK(np, format, args...) do { \
    if (smbfs_loglevel & SMB_ACL_LOG_LEVEL) { \
        lck_rw_lock_shared(&(np)->n_name_rwlock); \
        printf("%s: " format, __FUNCTION__ ,## args); \
        lck_rw_unlock_shared(&(np)->n_name_rwlock); \
    } \
    } while(0)

#define SMB_LOG_ACCESS(format, args...) do { \
    if (smbfs_loglevel & SMB_ACL_LOG_LEVEL) \
        printf("%s: " format, __FUNCTION__ ,## args); \
    } while(0)

#define SMB_LOG_IO_LOCK(np, format, args...) do { \
    if (smbfs_loglevel & SMB_IO_LOG_LEVEL) { \
        lck_rw_lock_shared(&(np)->n_name_rwlock); \
        printf("%s: " format, __FUNCTION__ ,## args); \
        lck_rw_unlock_shared(&(np)->n_name_rwlock); \
    } \
    } while(0)

#define SMB_LOG_IO(format, args...) do { \
    if (smbfs_loglevel & SMB_IO_LOG_LEVEL) \
        printf("%s: " format, __FUNCTION__ ,## args); \
    } while(0)

/*
 * Ariadne only shows 4 args, so only use 4 args
 * Using "args..." and passing "args" to KDBG_RELEASE() doesnt work
 */
#define SMB_LOG_KTRACE(code, arg1, arg2, arg3, arg4, arg5) do { \
    if (smbfs_loglevel & SMB_KTRACE_LOG_LEVEL) \
        KDBG_RELEASE(code, arg1, arg2, arg3, arg4); \
    } while(0)

#define SMB_LOG_DIR_CACHE(format, args...) do { \
    if (smbfs_loglevel & SMB_DIR_CACHE_LOG_LEVEL) \
        printf("%s: " format, __FUNCTION__ ,## args); \
    } while(0)

#define SMB_LOG_DIR_CACHE_LOCK(np, format, args...) do { \
    if (smbfs_loglevel & SMB_DIR_CACHE_LOG_LEVEL) { \
        lck_rw_lock_shared(&(np)->n_name_rwlock); \
        printf("%s: " format, __FUNCTION__ ,## args); \
        lck_rw_unlock_shared(&(np)->n_name_rwlock); \
    } \
    } while(0)

#define SMB_LOG_DIR_CACHE2(format, args...) do { \
    if (smbfs_loglevel & SMB_DIR_CACHE_LOG_LEVEL2) \
        printf("%s: " format, __FUNCTION__ ,## args); \
    } while(0)

#define SMB_LOG_DIR_CACHE2_LOCK(np, format, args...) do { \
    if (smbfs_loglevel & SMB_DIR_CACHE_LOG_LEVEL2) { \
        lck_rw_lock_shared(&(np)->n_name_rwlock); \
        printf("%s: " format, __FUNCTION__ ,## args); \
        lck_rw_unlock_shared(&(np)->n_name_rwlock); \
    } \
    } while(0)

#define SMB_LOG_MC(format, args...) do { \
    if (smbfs_loglevel & SMB_MC_LOG_LEVEL) \
        printf("%s: " format, __FUNCTION__ ,## args); \
    } while(0)

#define SMB_LOG_MC_REF(format, args...) do { \
    if (smbfs_loglevel & SMB_MC_REF_LOG_LEVEL) \
        printf("%s: " format, __FUNCTION__ ,## args); \
    } while(0)

#define SMB_LOG_UNIT_TEST(format, args...) do { \
	if (smbfs_loglevel & SMB_UNIT_TEST) \
		printf("%s: " format, __FUNCTION__ ,## args); \
	} while(0)

#define SMB_LOG_UNIT_TEST_LOCK(np, format, args...) do { \
	if (smbfs_loglevel & SMB_UNIT_TEST) { \
		lck_rw_lock_shared(&(np)->n_name_rwlock); \
		printf("%s: " format, __FUNCTION__ ,## args); \
		lck_rw_unlock_shared(&(np)->n_name_rwlock); \
	} \
	} while(0)

#define SMB_ASSERT(a) { \
	if (!(a)) { \
		panic("assertion '%s' failed", #a); \
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
#define SMBSDEBUG(format, args...) printf("%s: " format, __FUNCTION__ ,## args)
#else
#define SMBSDEBUG(format, args...)
#endif

#ifdef SMB_IOD_DEBUG
#define SMBIODEBUG(format, args...) printf("%s: " format, __FUNCTION__ ,## args)
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
struct mdchain;
struct smb_session;
struct smb_rq;
struct smbiod;

#ifdef SMB_DEBUG
void smb_hexdump(const char *func, const char *s, unsigned char *buf, size_t inlen);
#else // SMB_DEBUG
#define smb_hexdump(a,b,c,d)
#endif // SMB_DEBUG
char *smb_strndup(const char *s, size_t maxlen);
void *smb_memdup(const void *umem, int len);
void *smb_memdupin(user_addr_t umem, int len);

void smb_reset_sig(struct smb_session *sessionp);

int  smb_lmresponse(const u_char *apwd, u_char *C8, u_char *RN);
void  smb_ntlmresponse(const u_char *apwd, u_char *C8, u_char *RN);

int smb_ntlmv2hash(const u_char *apwd, const u_char *user,
                   const u_char *destination, u_char *v2hash);
int smb_ntlmv2response(u_char *v2hash, u_char *C8, const u_char *blob,
                       size_t bloblen, u_char **RN, size_t *RNlen);



void *make_target_info(struct smb_session *sessionp, uint16_t *target_len);
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
int  smb2_rq_sign(struct smb_rq *rqp);
int  smb2_rq_verify(struct smb_rq *rqp, struct mdchain *mdp, uint8_t *signature);
int  smb3_derive_channel_keys(struct smbiod *iod);
int  smb3_derive_keys(struct smbiod *iod);
int  smb3_rq_encrypt(struct smb_rq *rqp);
int  smb3_msg_decrypt(struct smb_session *sessionp, mbuf_t *m);
int smb3_verify_session_setup(struct smb_session *sessionp, struct smbiod *iod,
                              uint8_t *sess_setup_reply, size_t sess_setup_len);

int smb311_pre_auth_integrity_hash_init(struct smbiod *iod, uint16_t command, mbuf_t m0);
int smb311_pre_auth_integrity_hash_update(struct smbiod *iod, mbuf_t m0);
int smb311_pre_auth_integrity_hash_print(struct smbiod *iod);

#if 0
void smb_test_crypt_performance(struct smb_session *sessionp, size_t orig_packet_len,
								size_t orig_mb_len);
#endif

#endif /* !_NETSMB_SMB_SUBR_H_ */
