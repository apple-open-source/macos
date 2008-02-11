/*
 * Copyright (c) 2000, 2001 Boris Popov
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
 * $Id: nb.c,v 1.2 2005/10/07 03:51:09 lindak Exp $
 */
#include <sys/param.h>
#include <sys/socket.h>

#include <ctype.h>
#include <netdb.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <cflib.h>

#include <netsmb/netbios.h>
#include <netsmb/smb_lib.h>
#include <netsmb/nb_lib.h>
#include "charsets.h"


void
nb_ctx_done(struct nb_ctx *ctx)
{
	if (ctx->nb_scope)
		free(ctx->nb_scope);
	if (ctx->nb_wins_name)
		free(ctx->nb_wins_name);
}

int
nb_ctx_set_wins_name(struct nb_ctx *ctx, const char *addr)
{
	char * wspace;
	
	if ((addr == NULL) || (addr[0] == 0) || (addr[0] == 0x20))
		return EINVAL;
	if (ctx->nb_wins_name)
		free(ctx->nb_wins_name);
	/* 
	 * %%%
	 * Currently we only support one WINS server, someday maybe more
	 * but thats a different radar. We could end up with a list of WINS
	 * servers that are separated by a space.
	 */
	wspace = strchr(addr, 0x20);
	if (wspace)
		*wspace = 0;
	
	if ((ctx->nb_wins_name = strdup(addr)) == NULL)
		return ENOMEM;
	return 0;
}

int
nb_ctx_setscope(struct nb_ctx *ctx, const char *scope)
{
	size_t slen = strlen(scope);

	if (slen >= 128) {
		smb_log_info("scope '%s' is too long", 0, ASL_LEVEL_ERR, scope);
		return ENAMETOOLONG;
	}
	if (ctx->nb_scope)
		free(ctx->nb_scope);
	ctx->nb_scope = malloc(slen + 1);
	if (ctx->nb_scope == NULL)
		return ENOMEM;
	str_upper(ctx->nb_scope, scope);
	return 0;
}

/*
 * Used for resolving NetBIOS names
 */
int nb_ctx_resolve(struct nb_ctx *ctx)
{
	struct sockaddr *sap;
	int error;

	if (ctx->nb_wins_name == NULL) {
		ctx->nb_ns.sin_addr.s_addr = htonl(INADDR_BROADCAST);
		ctx->nb_ns.sin_port = htons(NBNS_UDP_PORT_137);
		ctx->nb_ns.sin_family = AF_INET;
		ctx->nb_ns.sin_len = sizeof(ctx->nb_ns);
	} else {
		error = nb_resolvehost_in(ctx->nb_wins_name, &sap, NBNS_UDP_PORT_137, TRUE);
		if (error) {
			smb_log_info("can't resolve %s", error, ASL_LEVEL_DEBUG, ctx->nb_wins_name);
			return error;
		}
		bcopy(sap, &ctx->nb_ns, sizeof(ctx->nb_ns));
		free(sap);
	}
	return 0;
}

void nb_ctx_readcodepage(struct rcfile *rcfile, const char *sname)
{
	char *p;
	
	rc_getstringptr(rcfile, sname, "doscharset", &p);
	if (p)
		setcharset(p);	
}

/*
 * used level values:
 * 0 - default
 * 1 - server
 */
int
nb_ctx_readrcsection(struct rcfile *rcfile, struct nb_ctx *ctx,
	const char *sname, int level)
{
	char *p;
	int error;

	if (level > 1)
		return EINVAL;
	rc_getint(rcfile, sname, "nbtimeout", &ctx->nb_timo);
	rc_getstringptr(rcfile, sname, "nbns", &p);
	if (!p)
	    rc_getstringptr(rcfile, sname, "winsserver", &p);
	if (p) {
		smb_log_info("wins address %s specified in the section %s", 0, ASL_LEVEL_DEBUG, p, sname);
		error = nb_ctx_set_wins_name(ctx, p);
		if (error) {
			smb_log_info("invalid wins address specified in the section %s", 0, ASL_LEVEL_DEBUG, sname);
			return error;
		}
	}
	rc_getstringptr(rcfile, sname, "nbscope", &p);
	if (p)
		nb_ctx_setscope(ctx, p);
	return 0;
}

#define NBNS_FMT_ERR	0x01	/* Request was invalidly formatted */
#define NBNS_SRV_ERR	0x02	/* Problem with NBNS, connot process name */
#define NBNS_NME_ERR	0x03	/* No such name */
#define NBNS_IMP_ERR	0x04	/* Unsupport request */
#define NBNS_RFS_ERR	0x05	/* For policy reasons server will not register this name fron this host */
#define NBNS_ACT_ERR	0x06	/* Name is owned by another host */
#define NBNS_CFT_ERR	0x07	/* Name conflict error  */

/*
 * Convert NetBIOS name lookup errors to UNIX errors
 */
int nb_error_to_errno(int error)
{
	switch (error) {
	case NBNS_FMT_ERR:
		error = EINVAL;
		break;
	case NBNS_SRV_ERR: 
		error = EBUSY;
		break;
	case NBNS_NME_ERR: 
		error = ENOENT;
		break;
	case NBNS_IMP_ERR: 
		error = ENOTSUP;
		break;
	case NBNS_RFS_ERR: 
		error = EACCES;
		break;
	case NBNS_ACT_ERR: 
		error = EADDRINUSE;
		break;
	case NBNS_CFT_ERR: 
		error = EADDRINUSE;
		break;
	default:
		error = ETIMEDOUT;
		break;
	};
	return error;
}
