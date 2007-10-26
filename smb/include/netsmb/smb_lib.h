/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2007 Apple Inc. All rights reserved.
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
#ifndef _NETSMB_SMB_LIB_H_
#define _NETSMB_SMB_LIB_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>

#include <netsmb/smb.h>
#include <netsmb/smb_dev.h>
#include <asl.h>

#define	SMB_CFG_FILE		"/etc/nsmb.conf"
#define SMB_CFG_LOCAL_FILE	"/Library/Preferences/nsmb.conf"
#define	SMB_GCFG_FILE		"/var/run/smb.conf"

/* Used by mount_smbfs to pass mount option into the smb_mount call */
#define kStreamstMountKey	CFSTR("SMBStreamsMount")
#define kdirModeKey			CFSTR("SMBDirModes")
#define kfileModeKey		CFSTR("SMBFileModes")

/* %%% This really needs to be in URLMount post Leopard */
#define kForcePrivateSessionKey		CFSTR("ForcePrivateSession")


#define SMB_PASSWORD_KEY "Password"

#ifndef min
#define	min(a,b)	(((a)<(b)) ? (a) : (b))
#endif

#define getb(buf,ofs) 		(((const u_int8_t *)(buf))[ofs])
#define setb(buf,ofs,val)	(((u_int8_t*)(buf))[ofs])=val
#define getbw(buf,ofs)		((u_int16_t)(getb(buf,ofs)))
#define getw(buf,ofs)		(*((u_int16_t*)(&((u_int8_t*)(buf))[ofs])))
#define getdw(buf,ofs)		(*((u_int32_t*)(&((u_int8_t*)(buf))[ofs])))

#if (BYTE_ORDER == LITTLE_ENDIAN)

#define getwle(buf,ofs)	(*((u_int16_t*)(&((u_int8_t*)(buf))[ofs])))
#define getdle(buf,ofs)	(*((u_int32_t*)(&((u_int8_t*)(buf))[ofs])))
#define getwbe(buf,ofs)	(ntohs(getwle(buf,ofs)))
#define getdbe(buf,ofs)	(ntohl(getdle(buf,ofs)))

#define setwle(buf,ofs,val) getwle(buf,ofs)=val
#define setwbe(buf,ofs,val) getwle(buf,ofs)=htons(val)
#define setdle(buf,ofs,val) getdle(buf,ofs)=val
#define setdbe(buf,ofs,val) getdle(buf,ofs)=htonl(val)

#else	/* (BYTE_ORDER == LITTLE_ENDIAN) */
#define getwbe(buf,ofs) (*((u_int16_t*)(&((u_int8_t*)(buf))[ofs])))
#define getdbe(buf,ofs) (*((u_int32_t*)(&((u_int8_t*)(buf))[ofs])))
#define getwle(buf,ofs) (NXSwapShort(getwbe(buf,ofs)))
#define getdle(buf,ofs) (NXSwapLong(getdbe(buf,ofs)))

#define setwbe(buf,ofs,val) getwbe(buf,ofs)=val
#define setwle(buf,ofs,val) getwbe(buf,ofs)=NXSwapShort(val)
#define setdbe(buf,ofs,val) getdbe(buf,ofs)=val
#define setdle(buf,ofs,val) getdbe(buf,ofs)=NXSwapLong(val)

#endif	/* (BYTE_ORDER == LITTLE_ENDIAN) */

/* Constants for smb port behavior  */
#define TRY_BOTH_PORTS		1	/* Try port 445 -- if unsuccessful try NetBIOS */
#define USE_THIS_PORT_ONLY	2	/* Try supplied port -- if unsuccessful quit ( Could be 139, 445, or other) */

/*
 * nb environment
 */
struct nb_ctx {
	int					nb_timo;
	char *				nb_scope;	/* NetBIOS scope */
	char *				nb_nsname;	/* name server */
	struct sockaddr_in	nb_ns;	/* ip addr of name server */
	struct sockaddr_in	nb_lastns;
};

/*
 * SMB work context. Used to store all values which are necessary
 * to establish connection to an SMB server.
 */
struct smb_ctx {
	CFStringRef scheme;		/* Must always be the first entry, required by the NetFS calls */
	pthread_mutex_t ctx_mutex;
	CFURLRef	ct_url;
	UInt32		ct_flags;	/* SMBCF_ */
	int			ct_fd;		/* handle of connection */
	int			ct_level;
	UInt32   	ct_port_behavior; 
	UInt16		ct_port;
	CFStringRef	serverDisplayName; /* Server name from URL or Bonjour Service Name*/
	char *		ct_fullserver; /* Server name from URL */
	char *		ct_srvaddr;	/* hostname or IP address of server taken from config file */
	struct nb_ctx ct_nb;
	struct smbioc_ossn	ct_ssn;
	struct smbioc_oshare	ct_sh;
	char *		ct_origshare;
	char *		ct_path;
	char *		ct_kerbPrincipalName;
	int32_t		ct_kerbPrincipalName_len;
	int			debug_level;
	int			altflags;
	u_int32_t	ct_vc_caps;		/* Obtained from the negotiate message */
	u_int32_t	ct_vc_flags;	/* Obtained from the negotiate message */
	u_int32_t	ct_vc_shared;	/* Obtained from the negotiate message, currently only tells if the vc is shared */
	char		LocalNetBIOSName[SMB_MAXUSERNAMELEN + 1];	/* Local NetBIOS Name or host name used for port 139 only */
};

#define	SMBCF_RESOLVED		0x00000001	/* We have reolved the address and name */
#define	SMBCF_CONNECTED		0x00000002	/* The negoticate message was succesful */
#define	SMBCF_AUTHORIZED	0x00000004	/* We have completed the security phase */
#define	SMBCF_SHARE_CONN	0x00000008	/* We have a tree connection */
#define	SMBCF_READ_PREFS	0x00000010	/* We already read the preference */
#define SMBCF_MOUNTSMBFS	0x00000020	/* Called from the mount_smbfs command */
#define SMBCF_EXPLICITPWD	0x00010000	/* The password set by the url */

#define SMBCF_CONNECT_STATE	SMBCF_CONNECTED | SMBCF_AUTHORIZED | SMBCF_SHARE_CONN
/*
 * request handling structures
 */
struct smb_lib_mbuf {
	int					m_len;
	int					m_maxlen;
	char				*m_data;
	struct smb_lib_mbuf *m_next;
};

struct mbdata {
	struct smb_lib_mbuf *mb_top;
	struct smb_lib_mbuf *mb_cur;
	char				*mb_pos;
	int					mb_count;
};

#define SMB_LIB_M_ALIGN(len)	(((len) + (sizeof(long)) - 1) & ~((sizeof(long)) - 1))
#define	SMB_LIB_M_BASESIZE	(sizeof(struct smb_lib_mbuf))
#define	SMB_LIB_M_MINSIZE	(256 - SMB_LIB_M_BASESIZE)
#define SMB_LIB_M_TOP(m)	((char*)(m) + SMB_LIB_M_BASESIZE)
#define SMB_LIB_MTODATA(m,t)	((t)(m)->m_data)
#define SMB_LIB_M_TRAILINGSPACE(m) ((m)->m_maxlen - (m)->m_len)

struct smb_rq {
	u_char		rq_cmd;
	struct mbdata	rq_rq;
	struct mbdata	rq_rp;
	struct smb_ctx *rq_ctx;
	int		rq_wcount;
	int		rq_bcount;
};

struct smb_bitname {
	u_int	bn_bit;
	char	*bn_name;
};

__BEGIN_DECLS

struct sockaddr;

int smb_load_library(char *codepage);
struct rcfile * smb_open_rcfile(int NoUserPreferences);
void smb_log_info(const char *, int, int,...);

/*
 * Context management
 */
void *smb_create_ctx();
int  smb_ctx_init(struct smb_ctx **out_ctx, const char *url, int level, int sharetype, int NoUserPreferences);
void smb_ctx_cancel_connection(struct smb_ctx *ctx);
void smb_ctx_done(void *);

int smb_get_server_info(struct smb_ctx *ctx, CFURLRef url, CFDictionaryRef OpenOptions, CFDictionaryRef *ServerParams);
int smb_open_session(struct smb_ctx *ctx, CFURLRef url, CFDictionaryRef OpenOptions, CFDictionaryRef *sessionInfo);
int smb_enumerate_shares(struct smb_ctx *ctx, CFDictionaryRef *shares);
int smb_mount(struct smb_ctx *in_ctx, CFStringRef mpoint, CFDictionaryRef mOptions, CFDictionaryRef *mInfo);

int smb_connect(struct smb_ctx *ctx);
int smb_session_security(struct smb_ctx *ctx, char *clientpn, char *servicepn);
int smb_share_connect(struct smb_ctx *ctx);

void smb_ctx_setserver(struct smb_ctx *ctx, const char *name);
int  smb_ctx_setuser(struct smb_ctx *, const char *);
int  smb_ctx_setshare(struct smb_ctx *, const char *, int);
int  smb_ctx_setdomain(struct smb_ctx *, const char *);
int  smb_ctx_setpassword(struct smb_ctx *, const char *);

u_int16_t smb_ctx_flags2(struct smb_ctx *);
u_int16_t smb_ctx_connstate(struct smb_ctx *ctx);
int  smb_smb_open_print_file(struct smb_ctx *, int, int, const char *, smbfh*);
int  smb_smb_close_print_file(struct smb_ctx *, smbfh);
int  smb_read(struct smb_ctx *, smbfh, off_t, size_t, char *);
int  smb_write(struct smb_ctx *, smbfh, off_t, size_t, const char *);

#define smb_rq_getrequest(rqp)	(&(rqp)->rq_rq)
#define smb_rq_getreply(rqp)	(&(rqp)->rq_rp)

int  smb_rq_init(struct smb_ctx *, u_char, size_t, struct smb_rq **);
void smb_rq_done(struct smb_rq *);
void smb_rq_wend(struct smb_rq *);
int  smb_rq_simple(struct smb_rq *);
int  smb_rq_dmem(struct mbdata *, const char *, size_t);
int  smb_rq_dstring(struct mbdata *, const char *);

int  smb_t2_request(struct smb_ctx *, int, u_int16_t *, const char *,
	int, void *, int, void *, int *, void *, int *, void *, int *);

void smb_simplecrypt(char *dst, const char *src);
int  smb_simpledecrypt(char *dst, const char *src);

int  smb_lib_mbuf_getm(struct smb_lib_mbuf *, size_t, struct smb_lib_mbuf **);
int  smb_lib_m_lineup(struct smb_lib_mbuf *, struct smb_lib_mbuf **);
int  mb_init(struct mbdata *, size_t);
int  mb_initm(struct mbdata *, struct smb_lib_mbuf *);
int  mb_done(struct mbdata *);
int  mb_fit(struct mbdata *mbp, size_t size, char **pp);
int  mb_put_uint8(struct mbdata *, u_int8_t);
int  mb_put_uint16be(struct mbdata *, u_int16_t);
int  mb_put_uint16le(struct mbdata *, u_int16_t);
int  mb_put_uint32be(struct mbdata *, u_int32_t);
int  mb_put_uint32le(struct mbdata *, u_int32_t);
int  mb_put_uint64be(struct mbdata *, u_int64_t);
int  mb_put_uint64le(struct mbdata *, u_int64_t);
int  mb_put_mem(struct mbdata *, const char *, size_t);
int  mb_put_pstring(struct mbdata *mbp, const char *s);
int  mb_put_mbuf(struct mbdata *, struct smb_lib_mbuf *);

int  mb_get_uint8(struct mbdata *, u_int8_t *);
int  mb_get_uint16(struct mbdata *, u_int16_t *);
int  mb_get_uint16le(struct mbdata *, u_int16_t *);
int  mb_get_uint16be(struct mbdata *, u_int16_t *);
int  mb_get_uint32(struct mbdata *, u_int32_t *);
int  mb_get_uint32be(struct mbdata *, u_int32_t *);
int  mb_get_uint32le(struct mbdata *, u_int32_t *);
int  mb_get_uint64(struct mbdata *, u_int64_t *);
int  mb_get_uint64be(struct mbdata *, u_int64_t *);
int  mb_get_uint64le(struct mbdata *, u_int64_t *);
int  mb_get_mem(struct mbdata *, char *, size_t);

__END_DECLS

#endif /* _NETSMB_SMB_LIB_H_ */
