/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2009 Apple Inc. All rights reserved.
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
#define	SMB_GCFG_FILE		"/var/db/smb.conf"
#define SMB_BonjourServiceNameType "_smb._tcp."
#define NetBIOS_SMBSERVER		"*SMBSERVER"

/* Used by mount_smbfs to pass mount option into the smb_mount call */
#define kNotifyOffMountKey	CFSTR("SMBNotifyOffMount")
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
#define getwbe(buf,ofs)	(OSSwapInt16(getwle(buf,ofs)))
#define getdbe(buf,ofs)	(OSSwapInt32(getdle(buf,ofs)))

#define setwle(buf,ofs,val) getwle(buf,ofs)=val
#define setwbe(buf,ofs,val) getwle(buf,ofs)=htons(val)
#define setdle(buf,ofs,val) getdle(buf,ofs)=val
#define setdbe(buf,ofs,val) getdle(buf,ofs)=htonl(val)

#else	/* (BYTE_ORDER == LITTLE_ENDIAN) */
#define getwbe(buf,ofs) (*((u_int16_t*)(&((u_int8_t*)(buf))[ofs])))
#define getdbe(buf,ofs) (*((u_int32_t*)(&((u_int8_t*)(buf))[ofs])))
#define getwle(buf,ofs) (OSSwapInt16(getwbe(buf,ofs)))
#define getdle(buf,ofs) (OSSwapInt32(getdbe(buf,ofs)))

#define setwbe(buf,ofs,val) getwbe(buf,ofs)=val
#define setwle(buf,ofs,val) getwbe(buf,ofs)=OSSwapInt16(val)
#define setdbe(buf,ofs,val) getdbe(buf,ofs)=val
#define setdle(buf,ofs,val) getdbe(buf,ofs)=OSSwapInt32(val)

#endif	/* (BYTE_ORDER == LITTLE_ENDIAN) */


/*
 * nb environment
 */
struct nb_ctx {
	int					nb_timo;
	char *				nb_scope;	/* NetBIOS scope */
	char *				nb_wins_name;	/* WINS name server, DNS name or IP Dot Notation */
	struct sockaddr_in	nb_ns;	/* ip addr of name server */
	struct sockaddr_in	nb_lastns;
};

/*
 * SMB work context. Used to store all values which are necessary
 * to establish connection to an SMB server.
 */
struct smb_ctx {
	pthread_mutex_t ctx_mutex;
	CFURLRef		ct_url;
	u_int32_t		ct_flags;	/* SMBCF_ */
	int				ct_fd;		/* handle of connection */
	u_int32_t		ct_level;
	u_int32_t   	ct_port_behavior; 
	u_int16_t		ct_port;
	CFStringRef		serverNameRef; /* Display Server name obtain from URL or Bonjour Service Name */
	char *			serverName;		/* Server name obtain from the URL */
	char *			netbios_dns_name;	/* Given a NetBIOS name this is the DNS name (or IP Dot Notation) found in the configuration file */
	struct nb_ctx		ct_nb;
	struct smbioc_ossn	ct_ssn;
	struct smbioc_setup ct_setup;
	struct smbioc_share	ct_sh;
	int32_t			ct_saddr_len;
	struct sockaddr	*ct_saddr;
	int32_t			ct_laddr_len;
	struct sockaddr	*ct_laddr;
	char *			ct_origshare;
	CFStringRef		mountPath;
	int32_t			debug_level;
	int				altflags;
	u_int32_t		ct_vc_caps;		/* Obtained from the negotiate message */
	u_int32_t		ct_vc_flags;	/* Obtained from the negotiate message */
	u_int32_t		ct_vc_shared;	/* Obtained from the negotiate message, currently only tells if the vc is shared */
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
	size_t				m_len;
	size_t				m_maxlen;
	char				*m_data;
	struct smb_lib_mbuf *m_next;
};

struct mbdata {
	struct smb_lib_mbuf *mb_top;
	struct smb_lib_mbuf *mb_cur;
	char				*mb_pos;
	size_t				mb_count;
};

#define SMB_LIB_M_ALIGN(len)	(((len) + (sizeof(u_int32_t)) - 1) & ~((sizeof(u_int32_t)) - 1))
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

int smb_load_library(void);
struct rcfile * smb_open_rcfile(int NoUserPreferences);
void smb_log_info(const char *, int, int,...);

#ifdef SMB_DEBUG
void smb_ctx_hexdump(const char */* func */, const char */* comments */, unsigned char */* buf */, size_t /* inlen */);
#endif // SMB_DEBUG
/*
 * Context management
 */
void *smb_create_ctx(void);
int  smb_ctx_init(struct smb_ctx **out_ctx, const char *url, u_int32_t level, int sharetype, int NoUserPreferences);
void smb_ctx_cancel_connection(struct smb_ctx *ctx);
void smb_ctx_done(void *);

int smb_get_server_info(struct smb_ctx *ctx, CFURLRef url, CFDictionaryRef OpenOptions, CFDictionaryRef *ServerParams);
int smb_open_session(struct smb_ctx *ctx, CFURLRef url, CFDictionaryRef OpenOptions, CFDictionaryRef *sessionInfo);
int smb_enumerate_shares(struct smb_ctx *ctx, CFDictionaryRef *shares);
int smb_mount(struct smb_ctx *in_ctx, CFStringRef mpoint, CFDictionaryRef mOptions, CFDictionaryRef *mInfo);

int smb_connect(struct smb_ctx *ctx);
int smb_session_security(struct smb_ctx *ctx, char *clientpn, char *servicepn);
int smb_share_connect(struct smb_ctx *ctx);

int  smb_ctx_setuser(struct smb_ctx *, const char *);
int  smb_ctx_setshare(struct smb_ctx *, const char *, int);
int  smb_ctx_setdomain(struct smb_ctx *, const char *);
int  smb_ctx_setpassword(struct smb_ctx *, const char *);

u_int16_t smb_ctx_flags2(struct smb_ctx *);
u_int16_t smb_ctx_connstate(struct smb_ctx *ctx);
int  smb_smb_open_print_file(struct smb_ctx *, int, int, const char *, smbfh*);
int  smb_smb_close_print_file(struct smb_ctx *, smbfh);
int  smb_read(struct smb_ctx *, smbfh, off_t, u_int32_t, char *);
int  smb_write(struct smb_ctx *, smbfh, off_t, u_int32_t, const char *);
void smb_ctx_get_user_mount_info(const char * /*mntonname */, CFMutableDictionaryRef);

#define smb_rq_getrequest(rqp)	(&(rqp)->rq_rq)
#define smb_rq_getreply(rqp)	(&(rqp)->rq_rp)

int  smb_rq_init(struct smb_ctx *, u_char, size_t, struct smb_rq **);
void smb_rq_done(struct smb_rq *);
void smb_rq_wend(struct smb_rq *);
int  smb_rq_simple(struct smb_rq *);
int  smb_rq_dstring(struct mbdata *, const char *);

int  smb_t2_request(struct smb_ctx *, int, u_int16_t *, const char *,
	int, void *, int, void *, int *, void *, int *, void *, int *);

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
int  mb_put_mbuf(struct mbdata *, struct smb_lib_mbuf *);

int  mb_get_uint8(struct mbdata *, u_int8_t *);
int  mb_get_uint16(struct mbdata *, u_int16_t *);
int  mb_get_uint16le(struct mbdata *, u_int16_t *);
int  mb_get_uint16be(struct mbdata *, u_int16_t *);
int  mb_get_uint32be(struct mbdata *, u_int32_t *);
int  mb_get_uint32le(struct mbdata *, u_int32_t *);
int  mb_get_uint64le(struct mbdata *, u_int64_t *);
int  mb_get_uint64be(struct mbdata *mbp, u_int64_t *x);

int  mb_get_mem(struct mbdata *, char *, u_int32_t);

__END_DECLS

#endif /* _NETSMB_SMB_LIB_H_ */
