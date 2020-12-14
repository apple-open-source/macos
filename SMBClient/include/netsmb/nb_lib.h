/*
 * Copyright (c) 2000, Boris Popov
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
#ifndef _NETSMB_NB_LIB_H_
#define	_NETSMB_NB_LIB_H_


/*
 * resource record
 */
struct nbns_rr {
	u_char *	rr_name;	/* compressed NETBIOS name */
	uint16_t	rr_type;
	uint16_t	rr_class;
	uint32_t	rr_ttl;
	uint16_t	rr_rdlength;
	u_char *	rr_data;
};

/*
 * NetBIOS name return
 */
struct nbns_nr {
	char		nr_name[NB_NAMELEN];
	uint16_t	nr_beflags; /* Big endian, from network */
};

#define NBRQF_BROADCAST		0x0001

#define NBNS_GROUPFLG 0x8000


struct nb_ifdesc {
	int		id_flags;
	struct in_addr	id_addr;
	struct in_addr	id_mask;
	char		id_name[16];	/* actually IFNAMSIZ */
	struct nb_ifdesc * id_next;
};

struct sockaddr;

__BEGIN_DECLS

/* new flag UCflag. 1=uppercase,0=don't */
void nb_name_encode(struct nb_name *, u_char *);
int nb_encname_len(const char *);

int nb_sockaddr(struct sockaddr *peer, const char *name, unsigned type, 
				struct sockaddr **dst);
void convertToNetBIOSaddr(struct sockaddr_storage *storage, const char *name);

int resolvehost(const char *name, CFMutableArrayRef *outAddressArray, char *netbios_name, 
				uint16_t port,  int allowLocalConn, int tryBothPorts);
int findReachableAddress(CFMutableArrayRef addressArray, uint16_t *cancel, struct connectAddress **dest);
int nbns_resolvename(struct nb_ctx *ctx, struct smb_prefs *prefs, const char *name, 
					 uint8_t nodeType, CFMutableArrayRef *outAddressArray, uint16_t port, 
					 int allowLocalConn, int tryBothPorts, uint16_t *cancel);
int nbns_getnodestatus(struct sockaddr *targethost, struct nb_ctx *ctx,
					   struct smb_prefs *prefs, uint16_t *cancel, char *nbt_server, 
					   char *workgroup, CFMutableArrayRef nbrrArray);
int isLocalIPAddress(struct sockaddr *, uint16_t port, int allowLocalConn);
int isIPv6NumericName(const char *name);
int nb_enum_if(struct nb_ifdesc **, int);
int nb_error_to_errno(int error);

int nb_ctx_resolve(struct nb_ctx *ctx, CFArrayRef WINSAddresses);
__END_DECLS

#endif /* !_NETSMB_NB_LIB_H_ */
