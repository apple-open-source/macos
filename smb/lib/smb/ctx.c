/*
 * Copyright (c) 2000, Boris Popov
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
 * $Id: ctx.c,v 1.21 2003/09/08 23:45:26 lindak Exp $
 */
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <sys/iconv.h>

#ifdef APPLE
#include <sys/types.h>
extern uid_t real_uid, eff_uid;
#endif

#define NB_NEEDRESOLVER

#include <netsmb/smb_lib.h>
#include <netsmb/netbios.h>
#include <netsmb/nb_lib.h>
#include <netsmb/smb_conn.h>

#include <cflib.h>

#include <spnego.h>
#include "derparse.h"
extern MECH_OID g_stcMechOIDList [];

#include <com_err.h>
#include <krb5.h>

extern char *__progname;

#define POWEROF2(x) (((x) & ((x)-1)) == 0)

/*
 * Prescan command line for [-U user] argument
 * and fill context with defaults
 */
int
smb_ctx_init(struct smb_ctx *ctx, int argc, char *argv[],
	int minlevel, int maxlevel, int sharetype)
{
	int  opt, error = 0;
	const char *arg, *cp;
	struct passwd *pw;

	bzero(ctx, sizeof(*ctx));
#ifdef APPLE
	if (sharetype == SMB_ST_DISK)
		ctx->ct_flags |= SMBCF_BROWSEOK;
#endif
	error = nb_ctx_create(&ctx->ct_nb);
	if (error)
		return error;
	ctx->ct_fd = -1;
	ctx->ct_parsedlevel = SMBL_NONE;
	ctx->ct_minlevel = minlevel;
	ctx->ct_maxlevel = maxlevel;

	ctx->ct_ssn.ioc_opt = SMBVOPT_CREATE;
	ctx->ct_ssn.ioc_timeout = 15;
	ctx->ct_ssn.ioc_retrycount = 4;
	ctx->ct_ssn.ioc_owner = SMBM_ANY_OWNER;
	ctx->ct_ssn.ioc_group = SMBM_ANY_GROUP;
	ctx->ct_ssn.ioc_mode = SMBM_EXEC;
	ctx->ct_ssn.ioc_rights = SMBM_DEFAULT;

	ctx->ct_sh.ioc_opt = SMBVOPT_CREATE;
	ctx->ct_sh.ioc_owner = SMBM_ANY_OWNER;
	ctx->ct_sh.ioc_group = SMBM_ANY_GROUP;
	ctx->ct_sh.ioc_mode = SMBM_EXEC;
	ctx->ct_sh.ioc_rights = SMBM_DEFAULT;
	ctx->ct_sh.ioc_owner = SMBM_ANY_OWNER;
	ctx->ct_sh.ioc_group = SMBM_ANY_GROUP;

	nb_ctx_setscope(ctx->ct_nb, "");
	pw = getpwuid(geteuid());
	/* if cannot get current user name, don't crash */
	if (pw != NULL)
		/* if the user name is not specified some other way, use the current user name */
		smb_ctx_setuser(ctx, pw->pw_name);
	endpwent();
	if (argv == NULL)
		return 0;
	for (opt = 1; opt < argc; opt++) {
		cp = argv[opt];
		if (strncmp(cp, "//", 2) != 0)
			continue;
		error = smb_ctx_parseunc(ctx, cp, sharetype, (const char**)&cp);
		if (error)
			return error;
		ctx->ct_uncnext = cp;
		break;
	}
	while (error == 0 && (opt = cf_getopt(argc, argv, ":E:L:U:")) != -1) {
		arg = cf_optarg;
		switch (opt) {
		    case 'E':
			error = smb_ctx_setcharset(ctx, arg);
			if (error)
				return error;
			break;
		    case 'L':
			error = nls_setlocale(optarg);
			if (error)
				break;
			break;
		    case 'U':
			error = smb_ctx_setuser(ctx, arg);
			break;
		}
	}
	cf_optind = cf_optreset = 1;
	return error;
}

void
smb_ctx_done(struct smb_ctx *ctx)
{
	if (ctx->ct_ssn.ioc_server)
		nb_snbfree(ctx->ct_ssn.ioc_server);
	if (ctx->ct_ssn.ioc_local)
		nb_snbfree(ctx->ct_ssn.ioc_local);
	if (ctx->ct_srvaddr)
		free(ctx->ct_srvaddr);
	if (ctx->ct_nb)
		nb_ctx_done(ctx->ct_nb);
	if (ctx->ct_secblob)
		free(ctx->ct_secblob);
        if (ctx->ct_origshare)
		free(ctx->ct_origshare);
}

static int
getsubstring(const char *p, u_char sep, char *dest, int maxlen, const char **next)
{
	int len;

	maxlen--;
	for (len = 0; len < maxlen && *p != sep; p++, len++, dest++) {
		if (*p == 0)
			return EINVAL;
		*dest = *p;
	}
	*dest = 0;
	*next = *p ? p + 1 : p;
	return 0;
}


unsigned
xtoi(unsigned u)
{
	if (isdigit(u))
		return (u - '0');
	else if (islower(u))
		return (10 + u - 'a');
	else if (isupper(u))
		return (10 + u - 'A');
	return (16);
}


/*
 * Removes the "%" escape sequences from a URL component.
 * See IETF RFC 2396.
 */
char *
unpercent(char * component)
{
	unsigned char c, *s;
	unsigned hi, lo;

	if (component)
		for (s = component; (c = *s); s++) {
			if (c != '%')
				continue;
			if ((hi = xtoi(s[1])) > 15 || (lo = xtoi(s[2])) > 15)
				continue; /* ignore invalid escapes */
			s[0] = hi*16 + lo;
			/*
			 * This was strcpy(s + 1, s + 3);
			 * But nowadays leftward overlapping copies are
			 * officially undefined in C.  Ours seems to
			 * work or not depending upon alignment.
			 */
			memmove(s+1, s+3, strlen(s+3) + 1);
		}
	return (component);
}


#ifdef APPLE
/*
 * Here we expect something like
 *   "//[workgroup;][user[:password]@]host[/share[/path]]"
 * See http://ietf.org/internet-drafts/draft-crhertel-smb-url-01.txt
 */
#else
/*
 * Here we expect something like "[proto:]//[user@]host[/share][/path]"
 */
#endif /* APPLE */
int
smb_ctx_parseunc(struct smb_ctx *ctx, const char *unc, int sharetype,
	const char **next)
{
	const char *p = unc;
	char *p1;
	char tmp[1024];
	int error ;

	ctx->ct_parsedlevel = SMBL_NONE;
	if (*p++ != '/' || *p++ != '/') {
		smb_error("UNC should start with '//'", 0);
		return EINVAL;
	}
	p1 = tmp;
#ifdef APPLE
	error = getsubstring(p, ';', p1, sizeof(tmp), &p);
	if (!error) {
		if (*p1 == 0) {
			smb_error("empty workgroup name", 0);
			return EINVAL;
		}
		error = smb_ctx_setworkgroup(ctx, unpercent(tmp));
		if (error)
			return error;
	}
#endif /* APPLE */
	error = getsubstring(p, '@', p1, sizeof(tmp), &p);
	if (!error) {
		if (ctx->ct_maxlevel < SMBL_VC) {
			smb_error("no user name required", 0);
			return EINVAL;
		}
#ifdef APPLE
		p1 = strchr(tmp, ':');
		if (p1) {
			*p1++ = (char)0;
			error = smb_ctx_setpassword(ctx, unpercent(p1));
			if (error)
				return error;
		}
		p1 = tmp;
#endif /* APPLE */
		if (*p1 == 0) {
			smb_error("empty user name", 0);
			return EINVAL;
		}
		error = smb_ctx_setuser(ctx, unpercent(tmp));
		if (error)
			return error;
		ctx->ct_parsedlevel = SMBL_VC;
	}
	error = getsubstring(p, '/', p1, sizeof(tmp), &p);
	if (error) {
		error = getsubstring(p, '\0', p1, sizeof(tmp), &p);
		if (error) {
			smb_error("no server name found", 0);
			return error;
		}
	}
	if (*p1 == 0) {
		smb_error("empty server name", 0);
		return EINVAL;
	}
	error = smb_ctx_setserver(ctx, unpercent(tmp));
	if (error)
		return error;
	if (sharetype == SMB_ST_NONE) {
		*next = p;
		return 0;
	}
	if (*p != 0 && ctx->ct_maxlevel < SMBL_SHARE) {
		smb_error("no share name required", 0);
		return EINVAL;
	}
	error = getsubstring(p, '/', p1, sizeof(tmp), &p);
	if (error) {
		error = getsubstring(p, '\0', p1, sizeof(tmp), &p);
		if (error) {
			smb_error("unexpected end of line", 0);
			return error;
		}
	}
#ifdef APPLE
	if (*p1 == 0 && ctx->ct_minlevel >= SMBL_SHARE &&
	    !(ctx->ct_flags & SMBCF_BROWSEOK)) {
#else
	if (*p1 == 0 && ctx->ct_minlevel >= SMBL_SHARE) {
#endif
		smb_error("empty share name", 0);
		return EINVAL;
	}
	*next = p;
	if (*p1 == 0)
		return 0;
	error = smb_ctx_setshare(ctx, unpercent(p1), sharetype);
	return error;
}

int
smb_ctx_setcharset(struct smb_ctx *ctx, const char *arg)
{
	char *cp, *servercs, *localcs;
	int cslen = sizeof(ctx->ct_ssn.ioc_localcs);
	int scslen, lcslen, error;

	cp = strchr(arg, ':');
	lcslen = cp ? (cp - arg) : 0;
	if (lcslen == 0 || lcslen >= cslen) {
		smb_error("invalid local charset specification (%s)", 0, arg);
		return EINVAL;
	}
	scslen = (size_t)strlen(++cp);
	if (scslen == 0 || scslen >= cslen) {
		smb_error("invalid server charset specification (%s)", 0, arg);
		return EINVAL;
	}
	localcs = memcpy(ctx->ct_ssn.ioc_localcs, arg, lcslen);
	localcs[lcslen] = 0;
	servercs = strcpy(ctx->ct_ssn.ioc_servercs, cp);
	error = nls_setrecode(localcs, servercs);
	if (error == 0)
		return 0;
	smb_error("can't initialize iconv support (%s:%s)",
	    error, localcs, servercs);
	localcs[0] = 0;
	servercs[0] = 0;
	return error;
}

#ifdef APPLE
int
smb_ctx_setfullsrvraddr(struct smb_ctx *ctx, const char *name)
{
	ctx->ct_fullsrvaddr = strdup(name);
	if (ctx->ct_fullsrvaddr == NULL)
		return ENOMEM;
	/*
	 * XXX Still need to resolve possible DNS name into
	 * NetBIOS server name ctx->ct_ssn.ioc_srvname somehow...
	 */
	ctx->ct_ssn.ioc_srvname[0] = '\0';
	return 0;
}

/*
 * XXX TODO FIXME etc etc
 * If the call to nbns_getnodestatus(...) fails we can try one of two other
 * methods; use a name of "*SMBSERVER", which is supported by Samba (at least)
 * or, as a last resort, try the "truncate-at-dot" heuristic.
 * And the heuristic really should attempt truncation at
 * each dot in turn, left to right.
 *
 * These fallback heuristics should be triggered when the attempt to open the
 * session fails instead of in the code below.
 *
 * See http://ietf.org/internet-drafts/draft-crhertel-smb-url-01.txt
 */
int
smb_ctx_getnbname(struct smb_ctx *ctx, struct sockaddr *sap)
{
	char server[SMB_MAXSRVNAMELEN + 1];
	char workgroup[SMB_MAXUSERNAMELEN + 1];
	int error;
#if 0
	char *dot;
#endif
	
	server[0] = workgroup[0] = '\0';
	error = nbns_getnodestatus(sap, ctx->ct_nb, server, workgroup);
	if (error == 0) {
		if (workgroup[0] && !ctx->ct_ssn.ioc_workgroup[0])
			smb_ctx_setworkgroup(ctx, workgroup);
		if (server[0])
			smb_ctx_setserver(ctx, server);
	} else {
		if (ctx->ct_ssn.ioc_srvname[0] == (char)0)
			smb_ctx_setserver(ctx, "*SMBSERVER");
	}
#if 0
	if (server[0] == (char)0) {
		dot = strchr(ctx->ct_fullsrvaddr, '.');
		if (dot)
			*dot = '\0';
		if (strlen(ctx->ct_fullsrvaddr) <= SMB_MAXSRVNAMELEN) {
			nls_str_upper(ctx->ct_ssn.ioc_srvname, ctx->ct_fullsrvaddr);
			error = 0;
		} else {
			error = -1;
		}
		if (dot)
			*dot = '.';
	}
#endif
	return error;
}
#endif /* APPLE */

int
smb_ctx_setserver(struct smb_ctx *ctx, const char *name)
{
#ifdef APPLE
	int error = smb_ctx_setfullsrvraddr(ctx, name);

	if (error)
		return error;
	if (strlen(name) > SMB_MAXSRVNAMELEN)
		return 0;
#else
	if (strlen(name) > SMB_MAXSRVNAMELEN) {
		smb_error("server name '%s' too long", 0, name);
		return ENAMETOOLONG;
	}
#endif
	nls_str_upper(ctx->ct_ssn.ioc_srvname, name);
	return 0;
}

int
smb_ctx_setuser(struct smb_ctx *ctx, const char *name)
{
	if (strlen(name) >= SMB_MAXUSERNAMELEN) {
		smb_error("user name '%s' too long", 0, name);
		return ENAMETOOLONG;
	}
	nls_str_upper(ctx->ct_ssn.ioc_user, name);
	return 0;
}

int
smb_ctx_setworkgroup(struct smb_ctx *ctx, const char *name)
{
	if (strlen(name) >= SMB_MAXUSERNAMELEN) {
		smb_error("workgroup name '%s' too long", 0, name);
		return ENAMETOOLONG;
	}
	nls_str_upper(ctx->ct_ssn.ioc_workgroup, name);
	return 0;
}

int
smb_ctx_setpassword(struct smb_ctx *ctx, const char *passwd)
{
	if (passwd == NULL)
		return EINVAL;
	if (strlen(passwd) >= SMB_MAXPASSWORDLEN) {
		smb_error("password too long", 0);
		return ENAMETOOLONG;
	}
	if (strncmp(passwd, "$$1", 3) == 0)
		smb_simpledecrypt(ctx->ct_ssn.ioc_password, passwd);
	else
		strcpy(ctx->ct_ssn.ioc_password, passwd);
	strcpy(ctx->ct_sh.ioc_password, ctx->ct_ssn.ioc_password);
	return 0;
}

int
smb_ctx_setshare(struct smb_ctx *ctx, const char *share, int stype)
{
	if (strlen(share) >= SMB_MAXSHARENAMELEN) {
		smb_error("share name '%s' too long", 0, share);
		return ENAMETOOLONG;
	}
        if (ctx->ct_origshare)
                free(ctx->ct_origshare);
        if ((ctx->ct_origshare = strdup(share)) == NULL)
		return ENOMEM;
	nls_str_upper(ctx->ct_sh.ioc_share, share); 
	if (share[0] != 0)
		ctx->ct_parsedlevel = SMBL_SHARE;
	ctx->ct_sh.ioc_stype = stype;
	return 0;
}

int
smb_ctx_setsrvaddr(struct smb_ctx *ctx, const char *addr)
{
	if (addr == NULL || addr[0] == 0)
		return EINVAL;
	if (ctx->ct_srvaddr)
		free(ctx->ct_srvaddr);
	if ((ctx->ct_srvaddr = strdup(addr)) == NULL)
		return ENOMEM;
	return 0;
}

static int
smb_parse_owner(char *pair, uid_t *uid, gid_t *gid)
{
	struct group *gr;
	struct passwd *pw;
	char *cp;

	cp = strchr(pair, ':');
	if (cp) {
		*cp++ = '\0';
		if (*cp) {
			gr = getgrnam(cp);
			if (gr) {
				*gid = gr->gr_gid;
			} else
				smb_error("Invalid group name %s, ignored",
				    0, cp);
		}
	}
	if (*pair) {
		pw = getpwnam(pair);
		if (pw) {
			*uid = pw->pw_uid;
		} else
			smb_error("Invalid user name %s, ignored", 0, pair);
	}
	endpwent();
	return 0;
}

int
smb_ctx_opt(struct smb_ctx *ctx, int opt, const char *arg)
{
	int error = 0;
	char *p, *cp;

	switch(opt) {
	    case 'U':
		break;
	    case 'I':
		error = smb_ctx_setsrvaddr(ctx, arg);
		break;
	    case 'M':
		ctx->ct_ssn.ioc_rights = strtol(arg, &cp, 8);
		if (*cp == '/') {
			ctx->ct_sh.ioc_rights = strtol(cp + 1, &cp, 8);
			ctx->ct_flags |= SMBCF_SRIGHTS;
		}
		break;
	    case 'N':
		ctx->ct_flags |= SMBCF_NOPWD;
		break;
	    case 'O':
		p = strdup(arg);
		cp = strchr(p, '/');
		if (cp) {
			*cp++ = '\0';
			error = smb_parse_owner(cp, &ctx->ct_sh.ioc_owner,
			    &ctx->ct_sh.ioc_group);
		}
		if (*p && error == 0) {
			error = smb_parse_owner(cp, &ctx->ct_ssn.ioc_owner,
			    &ctx->ct_ssn.ioc_group);
		}
		free(p);
		break;
	    case 'P':
/*		ctx->ct_ssn.ioc_opt |= SMBCOPT_PERMANENT;*/
		break;
	    case 'R':
		ctx->ct_ssn.ioc_retrycount = atoi(arg);
		break;
	    case 'T':
		ctx->ct_ssn.ioc_timeout = atoi(arg);
		break;
	    case 'W':
		error = smb_ctx_setworkgroup(ctx, arg);
		break;
	}
	return error;
}

#if 0
static void
smb_hexdump(const u_char *buf, int len) {
	int ofs = 0;

	while (len--) {
		if (ofs % 16 == 0)
			printf("\n%02X: ", ofs);
		printf("%02x ", *buf++);
		ofs++;
	}
	printf("\n");
}
#endif


static int
smb_addiconvtbl(const char *to, const char *from, const u_char *tbl)
{
	int error;

	error = kiconv_add_xlat_table(to, from, tbl);
	if (error && error != EEXIST) {
		smb_error("can not setup kernel iconv table (%s:%s)", error,
		    from, to);
		return error;
	}
	return 0;
}

/*
 * Verify context before connect operation(s),
 * lookup specified server and try to fill all forgotten fields.
 */
int
smb_ctx_resolve(struct smb_ctx *ctx)
{
	struct smbioc_ossn *ssn = &ctx->ct_ssn;
	struct smbioc_oshare *sh = &ctx->ct_sh;
	struct nb_name nn;
	struct sockaddr *sap;
	struct sockaddr_nb *salocal, *saserver;
	char *cp;
	u_char cstbl[256];
	u_int i;
	int error = 0;
#ifdef APPLE
	char password[SMB_MAXPASSWORDLEN + 1];
	int browseok = ctx->ct_flags & SMBCF_BROWSEOK;
#endif
	
	password[0] = '\0';
	ctx->ct_flags &= ~SMBCF_RESOLVED;
#ifdef APPLE
	if (isatty(STDIN_FILENO))
		browseok = 0;
	if (ctx->ct_fullsrvaddr == NULL || ctx->ct_fullsrvaddr[0] == 0) {
#else
	if (ssn->ioc_srvname[0] == 0) {
#endif
		smb_error("no server name specified", 0);
		return EINVAL;
	}
#ifndef APPLE
	if (ssn->ioc_user[0] == 0) {
		smb_error("no user name specified for server %s",
		    0, ssn->ioc_srvname);
		return EINVAL;
	}
#endif /* !APPLE */
#ifdef APPLE
	if (ctx->ct_minlevel >= SMBL_SHARE && sh->ioc_share[0] == 0 &&
	    !browseok) {
#else
	if (ctx->ct_minlevel >= SMBL_SHARE && sh->ioc_share[0] == 0) {
#endif
		smb_error("no share name specified for %s@%s",
		    0, ssn->ioc_user, ssn->ioc_srvname);
		return EINVAL;
	}
	error = nb_ctx_resolve(ctx->ct_nb);
	if (error)
		return error;
	if (ssn->ioc_localcs[0] == 0)
		strcpy(ssn->ioc_localcs, "default");	/* XXX: locale name ? */
	error = smb_addiconvtbl("tolower", ssn->ioc_localcs, nls_lower);
	if (error)
		return error;
	error = smb_addiconvtbl("toupper", ssn->ioc_localcs, nls_upper);
	if (error)
		return error;
	if (ssn->ioc_servercs[0] != 0) {
		for(i = 0; i < sizeof(cstbl); i++)
			cstbl[i] = i;
		nls_mem_toext(cstbl, cstbl, sizeof(cstbl));
		error = smb_addiconvtbl(ssn->ioc_servercs, ssn->ioc_localcs, cstbl);
		if (error)
			return error;
		for(i = 0; i < sizeof(cstbl); i++)
			cstbl[i] = i;
		nls_mem_toloc(cstbl, cstbl, sizeof(cstbl));
		error = smb_addiconvtbl(ssn->ioc_localcs, ssn->ioc_servercs, cstbl);
		if (error)
			return error;
	}
	if (ctx->ct_srvaddr) {
		error = nb_resolvehost_in(ctx->ct_srvaddr, &sap);
	} else {
#ifdef APPLE
	    if (ssn->ioc_srvname[0]) {
#endif
		error = nbns_resolvename(ssn->ioc_srvname, ctx->ct_nb, &sap);
#ifdef APPLE
		if (!error && !ctx->ct_ssn.ioc_workgroup[0]) {
			char wkgrp[SMB_MAXUSERNAMELEN + 1];

			wkgrp[0] = '\0';
			if (!nbns_getnodestatus(sap, ctx->ct_nb, NULL, wkgrp) &&
			    wkgrp[0])
				smb_ctx_setworkgroup(ctx, wkgrp);
		}
	    } else
	    	error = -1;
	    if (error && ctx->ct_fullsrvaddr) {
		    error = nb_resolvehost_in(ctx->ct_fullsrvaddr, &sap);
		    if (error == 0)
		    	smb_ctx_getnbname(ctx, sap);
	    }
#endif /* APPLE */
	}
	if (error) {
		smb_error("can't get server address", error);
		return error;
	}
	nn.nn_scope = ctx->ct_nb->nb_scope;
	nn.nn_type = NBT_SERVER;
	strcpy(nn.nn_name, ssn->ioc_srvname);
	error = nb_sockaddr(sap, &nn, &saserver);
	nb_snbfree(sap);
	if (error) {
		smb_error("can't allocate server address", error);
		return error;
	}
	ssn->ioc_server = (struct sockaddr*)saserver;
	if (ctx->ct_locname[0] == 0) {
		error = nb_getlocalname(ctx->ct_locname);
		if (error) {
			smb_error("can't get local name", error);
			return error;
		}
		nls_str_upper(ctx->ct_locname, ctx->ct_locname);
	}
	strcpy(nn.nn_name, ctx->ct_locname);
	nn.nn_type = NBT_WKSTA;
	nn.nn_scope = ctx->ct_nb->nb_scope;
	error = nb_sockaddr(NULL, &nn, &salocal);
	if (error) {
		nb_snbfree((struct sockaddr*)saserver);
		smb_error("can't allocate local address", error);
		return error;
	}
	ssn->ioc_local = (struct sockaddr*)salocal;
	ssn->ioc_lolen = salocal->snb_len;
	ssn->ioc_svlen = saserver->snb_len;

	error = smb_ctx_negotiate(ctx, SMBL_SHARE, SMBLK_CREATE);
	if (error)
		return (error);
#ifdef APPLE
	ctx->ct_flags &= ~SMBCF_AUTHREQ;
	if (!ctx->ct_secblob && browseok && !sh->ioc_share[0] &&
	    !(ctx->ct_flags & SMBCF_XXX)) {
		/* assert: anon share list is subset of overall server shares */
		error = smb_browse(ctx, 1);
		if (error) /* user cancel or other error? */
			return (error);
		/*
		 * A share was selected, authenticate button was pressed,
		 * or anon-authentication failed getting browse list.
		 */
	}
	if (!ctx->ct_secblob && (ctx->ct_flags & SMBCF_AUTHREQ ||
				 (ssn->ioc_password[0] == 0 &&
				  !(ctx->ct_flags & SMBCF_NOPWD)))) {
reauth:
		cp = password;
		error = smb_get_authentication(ssn->ioc_workgroup,
					       sizeof(ssn->ioc_workgroup) - 1,
					       ssn->ioc_user,
					       sizeof(ssn->ioc_user) - 1,
					       cp, sizeof(password) - 1,
					       ssn->ioc_srvname, ctx);
		if (error)
			return error;
#else
	if (ssn->ioc_password[0] == 0 && (ctx->ct_flags & SMBCF_NOPWD) == 0) {
		cp = getpass("Password:");
#endif /* APPLE */
		error = smb_ctx_setpassword(ctx, cp);
		if (error)
			return error;
	}
	/*
	 * if we have a session it is either anonymous
	 * or from a stale authentication.  re-negotiating
	 * gets us ready for a fresh session
	 */
	if (ctx->ct_flags & SMBCF_SSNACTIVE) {
		error = smb_ctx_negotiate(ctx, SMBL_SHARE, SMBLK_CREATE);
		if (error)
			return (error);
	}
#ifdef APPLE
	if (browseok && !sh->ioc_share[0]) {
		ctx->ct_flags &= ~SMBCF_AUTHREQ;
		error = smb_browse(ctx, 0);
		if (ctx->ct_flags & SMBCF_KCFOUND && smb_autherr(error))
			goto reauth;
		if (error) /* auth, user cancel, or other error */
			return (error);
		/*
		 * Re-authenticate button was pressed?
		 */
		if (ctx->ct_flags & SMBCF_AUTHREQ)
			goto reauth;
		if (!sh->ioc_share[0] && !(ctx->ct_flags & SMBCF_XXX)) {
			smb_error("no share specified for %s@%s",
				  0, ssn->ioc_user, ssn->ioc_srvname);
			return (EINVAL);
		}
	}
#endif /* !APPLE */
	ctx->ct_flags |= SMBCF_RESOLVED;

	return 0;
}

static int
smb_ctx_gethandle(struct smb_ctx *ctx)
{
	int fd, i;
	char buf[20];

	if (ctx->ct_fd != -1) {
		close(ctx->ct_fd);
		ctx->ct_fd = -1;
		ctx->ct_flags &= ~SMBCF_SSNACTIVE;
	}
	/*
	 * First try to open as clone
	 */
	fd = open("/dev/"NSMB_NAME, O_RDWR);
	if (fd >= 0) {
		ctx->ct_fd = fd;
		return 0;
	}
	/*
	 * well, no clone capabilities available - we have to scan
	 * all devices in order to get free one
	 */
	for (i = 0; i < 1024; i++) {
		snprintf(buf, sizeof(buf), "/dev/%s%d", NSMB_NAME, i);
		fd = open(buf, O_RDWR);
		if (fd >= 0) {
			ctx->ct_fd = fd;
			return 0;
		}
		if (i && POWEROF2(i+1))
			smb_error("%d failures to open smb device", errno, i+1);
	 }
#ifndef APPLE
	 /*
	  * This is a compatibility with old /dev/net/nsmb device
	  */
	 for (i = 0; i < 1024; i++) {
		 snprintf(buf, sizeof(buf), "/dev/net/%s%d", NSMB_NAME, i);
		 fd = open(buf, O_RDWR);
		 if (fd >= 0) {
			ctx->ct_fd = fd;
			return 0;
		 }
		 if (errno == ENOENT)
			return ENOENT;
	 }
#endif /* APPLE */
	 return ENOENT;
}

int
smb_ctx_ioctl(struct smb_ctx *ctx, int inum, struct smbioc_lookup *rqp)
{
	size_t	siz = DEF_SEC_TOKEN_LEN;
	int	rc = 0;

	if (rqp->ioc_ssn.ioc_outtok)
		free(rqp->ioc_ssn.ioc_outtok);
	rqp->ioc_ssn.ioc_outtok = (size_t *)malloc(sizeof(size_t) + siz);
	if (!rqp->ioc_ssn.ioc_outtok) {
		smb_error("smb_ctx_ioctl malloc failed", 0);
		return (ENOMEM);
	}
	*rqp->ioc_ssn.ioc_outtok = siz;
#ifdef APPLE
	seteuid(eff_uid); /* restore setuid root briefly */
#endif
	if (ioctl(ctx->ct_fd, inum, rqp) == -1) {
		rc = errno;
		goto out;
	}
	if (*rqp->ioc_ssn.ioc_outtok <= siz)
		goto out;
	/*
	 * Operation completed, but our output token wasn't large enough.
	 * The re-call below only pulls the token from the kernel.
	 */
	siz = *rqp->ioc_ssn.ioc_outtok;
	free(rqp->ioc_ssn.ioc_outtok);
	rqp->ioc_ssn.ioc_outtok = (size_t *)malloc(sizeof(size_t) + siz);
	*rqp->ioc_ssn.ioc_outtok = siz;
	if (ioctl(ctx->ct_fd, inum, rqp) == -1)
		rc = errno;
out:;
#ifdef APPLE
	seteuid(real_uid); /* and back to real user */
#endif
	return (rc);
}


/*
 * adds a GSSAPI wrapper
 */
char *
smb_ctx_tkt2gtok(u_char *tkt, u_long tktlen, u_char **gtokp, u_long *gtoklenp)
{
	u_long		bloblen = tktlen;
	u_long		len;
	u_char		krbapreq[2] = "\x01\x00"; /* see RFC 1964 */
	char *		failure;
	u_char *	blob = NULL;		/* result */
	u_char *	b;

	bloblen += sizeof krbapreq;
	bloblen += g_stcMechOIDList[spnego_mech_oid_Kerberos_V5].iLen;
	len = bloblen;
	bloblen = ASNDerCalcTokenLength(bloblen, bloblen);
	failure = "smb_ctx_tkt2gtok malloc";
	if (!(blob = malloc(bloblen)))
		goto out;
	b = blob;
	b += ASNDerWriteToken(b, SPNEGO_NEGINIT_APP_CONSTRUCT, NULL, len);
	b += ASNDerWriteOID(b, spnego_mech_oid_Kerberos_V5);
	memcpy(b, krbapreq, sizeof krbapreq);
	b += sizeof krbapreq;
	failure = "smb_ctx_tkt2gtok insanity check";
	if (b + tktlen != blob + bloblen)
		goto out;
	memcpy(b, tkt, tktlen);
	*gtoklenp = bloblen;
	*gtokp = blob;
	failure = NULL;
out:;
	if (blob && failure)
		free(blob);
	return (failure);
}


/*
 * See "Windows 2000 Kerberos Interoperability" paper by
 * Christopher Nebergall.  RC4 HMAC is the W2K default but
 * I don't think Samba is supporting it yet.
 *
 * Only session enc type should matter, not ticket enc type,
 * per Sam Hartman on krbdev.
 *
 * Preauthentication failure topics in krb-protocol may help here...
 * try "John Brezak" and/or "Clifford Neuman" too.
 */
static krb5_enctype kenctypes[] = {
#ifdef XXX
	ENCTYPE_ARCFOUR_HMAC,
#endif
#if 0
	ENCTYPE_ARCFOUR_HMAC_EXP, /* exportable */
#endif
	ENCTYPE_DES_CBC_MD5,
	ENCTYPE_DES_CBC_CRC,
	ENCTYPE_NULL
};

/*
 * Obtain a kerberos ticket...
 * (if TLD != "gov" then pray first)
 */
char *
smb_ctx_principal2tkt(char *prin, u_char **tktp, u_long *tktlenp)
{
	char *		failure;
	krb5_context	kctx = NULL;
	krb5_error_code	kerr;
	krb5_ccache	kcc = NULL;
	krb5_principal	kprin = NULL;
	krb5_creds	kcreds, *kcredsp = NULL;
	krb5_auth_context	kauth = NULL;
	krb5_data	kdata, kdata0;
	char *		tkt;

	memset((char *)&kcreds, 0, sizeof(kcreds));
	kdata0.length = 0;

	failure = "krb5_init_context";
	if ((kerr = krb5_init_context(&kctx)))
		 goto out;
	/* non-default would instead use krb5_cc_resolve */
	failure = "krb5_cc_default";
	if ((kerr = krb5_cc_default(kctx, &kcc)))
		 goto out;
	failure = "krb5_set_default_tgs_enctypes";
	if ((kerr = krb5_set_default_tgs_enctypes(kctx, kenctypes)))
		goto out;
	/* 
	 * The following is an unrolling of krb5_mk_req.  Something like:
	 * krb5_mk_req(kctx, &kauth, 0, service(prin), hostname(prin), &kdata0
	 *	       kcc, &kdata);)
	 * ...except we needed krb5_parse_name not krb5_sname_to_principal.
	 */
	failure = "krb5_parse_name";
	if ((kerr = krb5_parse_name(kctx, prin, &kprin)))
		 goto out;
	failure = "krb5_copy_principal";
	if ((kerr = krb5_copy_principal(kctx, kprin, &kcreds.server)))
		 goto out;
	failure = "krb5_cc_get_principal";
	if ((kerr = krb5_cc_get_principal(kctx, kcc, &kcreds.client)))
		 goto out;
	failure = "krb5_get_credentials";
	if ((kerr = krb5_get_credentials(kctx, 0, kcc, &kcreds, &kcredsp)))
		 goto out;
	failure = "krb5_mk_req_extended";
	if ((kerr = krb5_mk_req_extended(kctx, &kauth, 0, &kdata0, kcredsp,
					 &kdata)))
		 goto out;
	failure = "malloc";
	if (!(tkt = malloc(kdata.length))) {
		krb5_free_data_contents(kctx, &kdata);
		goto out;
	}
	*tktlenp = kdata.length;
	memcpy(tkt, kdata.data, kdata.length);
	krb5_free_data_contents(kctx, &kdata);
	*tktp = tkt;
	failure = NULL;
out:;
	if (kerr) {
		if (!failure)
			failure = "smb_ctx_principal2tkt";
		com_err(__progname, kerr, failure);
	}
	if (kauth)
		krb5_auth_con_free(kctx, kauth);
	if (kcredsp)
		krb5_free_creds(kctx, kcredsp);
	if (kcreds.server || kcreds.client)
		krb5_free_cred_contents(kctx, &kcreds);
	if (kprin)
		krb5_free_principal(kctx, kprin);
	if (kctx)
		krb5_free_context(kctx);
	return (failure);
}

char *
smb_ctx_principal2blob(size_t **blobp, char *prin, size_t plen)
{
#pragma unused(plen)
	int		rc = 0;
	char *		failure;
	u_char *	tkt = NULL;
	u_long		tktlen;
	u_char *	gtok = NULL;		/* gssapi token */
	u_long		gtoklen;		/* gssapi token length*/
	SPNEGO_TOKEN_HANDLE  stok = NULL;	/* spnego token */
	size_t *	blob = NULL;		/* result */
	u_long		bloblen;		/* result length */

	if ((failure = smb_ctx_principal2tkt(prin, &tkt, &tktlen)))
		goto out;
	if ((failure = smb_ctx_tkt2gtok(tkt, tktlen, &gtok, &gtoklen)))
		goto out;
	/*
	 * RFC says to send NegTokenTarg now.  So does MS docs.  But
	 * win2k gives ERRbaduid if we do...  we must send
	 * another NegTokenInit now!
	 */ 
	failure = "spnegoCreateNegTokenInit";
	if ((rc = spnegoCreateNegTokenInit(spnego_mech_oid_Kerberos_V5_Legacy,
					   0, gtok, gtoklen, NULL, 0, &stok)))
		goto out;
	failure = "spnegoTokenGetBinary(NULL)";
	rc = spnegoTokenGetBinary(stok, NULL, &bloblen);
	if (rc != SPNEGO_E_BUFFER_TOO_SMALL)
		goto out;
	failure = "malloc";
	if (!(blob = (size_t *)malloc(sizeof(size_t) + bloblen)))
		goto out;
	*blob = bloblen;
	failure = "spnegoTokenGetBinary";
	if ((rc = spnegoTokenGetBinary(stok, (u_char *)(blob+1), &bloblen)))
		goto out;
	*blobp = blob;
	failure = NULL;
out:;
	if (rc) {
		/* XXX better is to embed rc in failure */
		smb_error("spnego principal2blob error %d", 0, -rc);
		if (!failure)
			failure = "spnego";
	}
	if (blob && failure)
		free(blob);
	if (stok)
		spnegoFreeData(stok);
	if (gtok)
		free(gtok);
	if (tkt)
		free(tkt);
	return (failure);
}


#if 0
void
prblob(u_char *b, size_t len)
{
	while (len--)
		fprintf(stderr, "%02x", *b++);
	fprintf(stderr, "\n");
}
#endif


/*
 * We navigate the SPNEGO & ASN1 encoding to find a kerberos principal
 */
char *
smb_ctx_blob2principal(size_t *blob, u_char **prinp, size_t *plenp)
{
	size_t		len = *blob - SMB_GUIDLEN;
	u_char *	start = (char *)blob + sizeof(size_t) + SMB_GUIDLEN;
	int		rc = 0;
	SPNEGO_TOKEN_HANDLE	stok = NULL;
	int		indx = 0;
	char *		failure;
	u_char		flags = 0;
	unsigned long	plen = 0;
	u_char *	prin;

#if 0
	fprintf(stderr, "blob from negotiate:\n");
	prblob(start, len);
#endif
	failure = "spnegoInitFromBinary";
	if ((rc = spnegoInitFromBinary(start, len, &stok)))
		goto out;
	/*
	 * Needn't use new Kerberos OID - the Legacy one is fine.
	 */
	failure = "spnegoIsMechTypeAvailable";
	if (spnegoIsMechTypeAvailable(stok, spnego_mech_oid_Kerberos_V5_Legacy,
				      &indx))
		goto out;
	/*
	 * Ignoring optional context flags for now.  May want to pass
	 * them to krb5 layer.  XXX
	 */
	if (!spnegoGetContextFlags(stok, &flags))
		fprintf(stderr, "spnego context flags 0x%x\n", flags);
	failure = "spnegoGetMechListMIC(NULL)";
	rc = spnegoGetMechListMIC(stok, NULL, &plen);
	if (rc != SPNEGO_E_BUFFER_TOO_SMALL)
		goto out;
	failure = "malloc";
	if (!(prin = malloc(plen)))
		goto out;
	failure = "spnegoGetMechListMIC";
	if ((rc = spnegoGetMechListMIC(stok, prin, &plen))) {
		free(prin);
		goto out;
	}
	*prinp = prin;
	*plenp = plen;
	failure = NULL;
out:;
	if (stok)
		spnegoFreeData(stok);
	if (rc) {
		/* XXX better is to embed rc in failure */
		smb_error("spnego blob2principal error %d", 0, -rc);
		if (!failure)
			failure = "spnego";
	}
	return (failure);
}


int
smb_ctx_negotiate(struct smb_ctx *ctx, int level, int flags)
{
	struct smbioc_lookup	rq;
	int	error = 0;
	char *	failure = NULL;
	u_char	*principal = NULL;
	size_t	prinlen;

	/*
	 * We leave ct_secblob set iff extended security
	 * negotiation succeeds.
	 */
	if (ctx->ct_secblob) {
		free(ctx->ct_secblob);
		ctx->ct_secblob = NULL;
	}
#ifdef XXX
	if ((ctx->ct_flags & SMBCF_RESOLVED) == 0) {
		smb_error("smb_ctx_lookup() data is not resolved", 0);
		return EINVAL;
	}
#endif
	if ((error = smb_ctx_gethandle(ctx)))
		return (error);
	bzero(&rq, sizeof(rq));
	bcopy(&ctx->ct_ssn, &rq.ioc_ssn, sizeof(struct smbioc_ossn));
	bcopy(&ctx->ct_sh, &rq.ioc_sh, sizeof(struct smbioc_oshare));
	rq.ioc_flags = flags;
	rq.ioc_level = level;
	rq.ioc_ssn.ioc_opt |= SMBVOPT_EXT_SEC;
	if ((error = smb_ctx_ioctl(ctx, SMBIOC_NEGOTIATE, &rq)))
		failure = "negotiate failed";
	else if (*rq.ioc_ssn.ioc_outtok < SMB_GUIDLEN)
		failure = "small blob"; /* XXX */
	else if (*rq.ioc_ssn.ioc_outtok == SMB_GUIDLEN)
		failure = "NTLMSSP unsupported";	/* XXX */
	else if (!(failure = smb_ctx_blob2principal(rq.ioc_ssn.ioc_outtok,
						    &principal, &prinlen)) &&
		 !(failure = smb_ctx_principal2blob(&rq.ioc_ssn.ioc_intok,
						    principal, prinlen))) {
		ctx->ct_secblob = rq.ioc_ssn.ioc_intok;
		rq.ioc_ssn.ioc_intok = NULL;
	}
	if (principal)
		free(principal);
	if (rq.ioc_ssn.ioc_intok)
		free(rq.ioc_ssn.ioc_intok);
	if (rq.ioc_ssn.ioc_outtok)
		free(rq.ioc_ssn.ioc_outtok);
	if (!failure)
		return (0);
	/*
	 * Avoid spew for anticipated failure modes
	 * but (XXX) enable this with command line debug flag
	 */
#if 0
	smb_error("%s (extended security negotiate)", error, failure);
#endif
	if ((error = smb_ctx_gethandle(ctx)))
		return (error);
	bzero(&rq, sizeof(rq));
	bcopy(&ctx->ct_ssn, &rq.ioc_ssn, sizeof(struct smbioc_ossn));
	bcopy(&ctx->ct_sh, &rq.ioc_sh, sizeof(struct smbioc_oshare));
	rq.ioc_flags = flags;
	rq.ioc_level = level;
#ifdef APPLE
	seteuid(eff_uid); /* restore setuid root briefly */
#endif
	if (ioctl(ctx->ct_fd, SMBIOC_NEGOTIATE, &rq) == -1)
		error = errno;
#ifdef APPLE
	seteuid(real_uid); /* and back to real user */
#endif
	if (error) {
		smb_error("negotiate phase failed", error);
		close(ctx->ct_fd);
		ctx->ct_fd = -1;
	}
	return (error);
}


int
smb_ctx_tdis(struct smb_ctx *ctx)
{
	struct smbioc_lookup rq; /* XXX may be used, someday */
	int error = 0;

	if (ctx->ct_fd < 0) {
		smb_error("tdis w/o handle?!", 0);
		return EINVAL;
	}
	if (!(ctx->ct_flags & SMBCF_SSNACTIVE)) {
		smb_error("tdis w/o session?!", 0);
		return EINVAL;
	}
	bzero(&rq, sizeof(rq));
	bcopy(&ctx->ct_ssn, &rq.ioc_ssn, sizeof(struct smbioc_ossn));
	bcopy(&ctx->ct_sh, &rq.ioc_sh, sizeof(struct smbioc_oshare));
	if (ioctl(ctx->ct_fd, SMBIOC_TDIS, &rq) == -1) {
		error = errno;
		smb_error("tree disconnect failed", error);
	}
	return (error);
}


int
smb_ctx_lookup(struct smb_ctx *ctx, int level, int flags)
{
	struct smbioc_lookup rq;
	int error = 0;
	char *	failure = NULL;

	if ((ctx->ct_flags & SMBCF_RESOLVED) == 0) {
		smb_error("smb_ctx_lookup() data is not resolved", 0);
		return EINVAL;
	}
	if (ctx->ct_fd < 0) {
		smb_error("handle from smb_ctx_nego() gone?!", 0);
		return EINVAL;
	}
	if (!(flags & SMBLK_CREATE))
		return (0);
	bzero(&rq, sizeof(rq));
	bcopy(&ctx->ct_ssn, &rq.ioc_ssn, sizeof(struct smbioc_ossn));
	bcopy(&ctx->ct_sh, &rq.ioc_sh, sizeof(struct smbioc_oshare));
	rq.ioc_flags = flags;
	rq.ioc_level = level;
	if (ctx->ct_secblob) {
		rq.ioc_ssn.ioc_opt |= SMBVOPT_EXT_SEC;
		if (!(ctx->ct_flags & SMBCF_SSNACTIVE)) {
			rq.ioc_ssn.ioc_intok = ctx->ct_secblob;
			error = smb_ctx_ioctl(ctx, SMBIOC_SSNSETUP, &rq);
		}
		rq.ioc_ssn.ioc_intok = NULL;
		if (error) {
			failure = "session setup failed";
		} else {
			ctx->ct_flags |= SMBCF_SSNACTIVE;
			if ((error = smb_ctx_ioctl(ctx, SMBIOC_TCON, &rq)))
				failure = "tree connect failed";
		}
		if (rq.ioc_ssn.ioc_intok)
			free(rq.ioc_ssn.ioc_intok);
		if (rq.ioc_ssn.ioc_outtok)
			free(rq.ioc_ssn.ioc_outtok);
		if (!failure)
			return (0);
		smb_error("%s (extended security lookup2)", error, failure);
		/* unwise to failback to NTLM now */
		return (error);
	}
#ifdef APPLE
	seteuid(eff_uid); /* restore setuid root briefly */
#endif
	if (!(ctx->ct_flags & SMBCF_SSNACTIVE) &&
	    ioctl(ctx->ct_fd, SMBIOC_SSNSETUP, &rq) == -1)
		failure = "session setup";
	else {
		ctx->ct_flags |= SMBCF_SSNACTIVE;
		if (ioctl(ctx->ct_fd, SMBIOC_TCON, &rq) == -1)
			failure = "tree connect";
	}
#ifdef APPLE
	seteuid(real_uid); /* and back to real user */
#endif
	if (failure) {
		error = errno;
		smb_error("%s phase failed", error, failure);
	}
	return error;
}

#ifndef APPLE
int
smb_ctx_login(struct smb_ctx *ctx)
{
	struct smbioc_ossn *ssn = &ctx->ct_ssn;
	struct smbioc_oshare *sh = &ctx->ct_sh;
	int error;

	if ((ctx->ct_flags & SMBCF_RESOLVED) == 0) {
		smb_error("smb_ctx_resolve() should be called first", 0);
		return EINVAL;
	}
	error = smb_ctx_gethandle(ctx);
	if (error)
		return EINVAL;
	if (ioctl(ctx->ct_fd, SMBIOC_OPENSESSION, ssn) == -1) {
		error = errno;
		smb_error("can't open session to server %s", error, ssn->ioc_srvname);
		return error;
	}
	if (sh->ioc_share[0] == 0)
		return 0;
	if (ioctl(ctx->ct_fd, SMBIOC_OPENSHARE, sh) == -1) {
		error = errno;
		smb_error("can't connect to share //%s/%s", error,
		    ssn->ioc_srvname, sh->ioc_share);
		return error;
	}
	return 0;
}

int
smb_ctx_setflags(struct smb_ctx *ctx, int level, int mask, int flags)
{
	struct smbioc_flags fl;

	if (ctx->ct_fd == -1)
		return EINVAL;
	fl.ioc_level = level;
	fl.ioc_mask = mask;
	fl.ioc_flags = flags;
	if (ioctl(ctx->ct_fd, SMBIOC_SETFLAGS, &fl) == -1)
		return errno;
	return 0;
}
#endif /* !APPLE */

/*
 * level values:
 * 0 - default
 * 1 - server
 * 2 - server:user
 * 3 - server:user:share
 */
static int
smb_ctx_readrcsection(struct smb_ctx *ctx, const char *sname, int level)
{
	char *p;
	int error;

	if (level > 0) {
		rc_getstringptr(smb_rc, sname, "charsets", &p);
		if (p) {
			error = smb_ctx_setcharset(ctx, p);
			if (error)
				smb_error("charset specification in the section '%s' ignored", error, sname);
		}
	}
	if (level <= 1) {
		rc_getint(smb_rc, sname, "timeout", &ctx->ct_ssn.ioc_timeout);
		rc_getint(smb_rc, sname, "retry_count", &ctx->ct_ssn.ioc_retrycount);
	}
	if (level == 1) {
		rc_getstringptr(smb_rc, sname, "addr", &p);
		if (p) {
			error = smb_ctx_setsrvaddr(ctx, p);
			if (error) {
				smb_error("invalid address specified in the section %s", 0, sname);
				return error;
			}
		}
	}
	if (level >= 2) {
		rc_getstringptr(smb_rc, sname, "password", &p);
		if (p) {
			error = smb_ctx_setpassword(ctx, p);
			if (error)
				smb_error("password specification in the section '%s' ignored", error, sname);
		}
	}
	rc_getstringptr(smb_rc, sname, "workgroup", &p);
	if (p) {
		error = smb_ctx_setworkgroup(ctx, p);
		if (error)
			smb_error("workgroup specification in the section '%s' ignored", error, sname);
	}
	return 0;
}

/*
 * read rc file as follows:
 * 1. read [default] section
 * 2. override with [server] section
 * 3. override with [server:user:share] section
 * Since absence of rcfile is not fatal, silently ignore this fact.
 * smb_rc file should be closed by caller.
 */
int
smb_ctx_readrc(struct smb_ctx *ctx)
{
	char sname[SMB_MAXSRVNAMELEN + SMB_MAXUSERNAMELEN + SMB_MAXSHARENAMELEN + 4];
/*	char *p;*/

	if (smb_open_rcfile() != 0)
		return 0;

	if (ctx->ct_ssn.ioc_user[0] == 0 || ctx->ct_ssn.ioc_srvname[0] == 0)
		return 0;

	/*
	 * default parameters
	 */
	smb_ctx_readrcsection(ctx, "default", 0);
	nb_ctx_readrcsection(smb_rc, ctx->ct_nb, "default", 0);
	/*
	 * SERVER parameters
	 */
	smb_ctx_readrcsection(ctx, ctx->ct_ssn.ioc_srvname, 1);
	nb_ctx_readrcsection(smb_rc, ctx->ct_nb, ctx->ct_ssn.ioc_srvname, 1);
	/*
	 * SERVER:USER parameters
	 */
	snprintf(sname, sizeof(sname), "%s:%s", ctx->ct_ssn.ioc_srvname,
	    ctx->ct_ssn.ioc_user);
	smb_ctx_readrcsection(ctx, sname, 2);

	if (ctx->ct_sh.ioc_share[0] != 0) {
		/*
		 * SERVER:USER:SHARE parameters
		 */
		snprintf(sname, sizeof(sname), "%s:%s:%s", ctx->ct_ssn.ioc_srvname,
		    ctx->ct_ssn.ioc_user, ctx->ct_sh.ioc_share);
		smb_ctx_readrcsection(ctx, sname, 3);
	}
	return 0;
}

