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
 * $Id: ctx.c,v 1.32.70.8 2005/11/15 01:45:30 lindak Exp $
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
#include <charsets.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <sys/smb_iconv.h>

#include <sys/types.h>
extern uid_t real_uid, eff_uid;

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
#include <sys/mchain.h>

#include <charsets.h>

#include <rpc_cleanup.h>

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
	if (sharetype == SMB_ST_DISK)
		ctx->ct_flags |= SMBCF_BROWSEOK;
	error = nb_ctx_create(&ctx->ct_nb);
	if (error)
		return error;
	ctx->ct_fd = -1;
	ctx->ct_parsedlevel = SMBL_NONE;
	ctx->ct_minlevel = minlevel;
	ctx->ct_maxlevel = maxlevel;

	ctx->ct_ssn.ioc_opt = SMBVOPT_CREATE | SMBVOPT_MINAUTH_NTLM;
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
	rpc_cleanup_smbctx(ctx);
	if (ctx->ct_ssn.ioc_server)
		nb_snbfree(ctx->ct_ssn.ioc_server);
	if (ctx->ct_ssn.ioc_local)
		nb_snbfree(ctx->ct_ssn.ioc_local);
	if (ctx->ct_srvaddr)
		free(ctx->ct_srvaddr);
	if (ctx->ct_utf8_servname)
		free(ctx->ct_utf8_servname);
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

/*
 * Here we expect something like
 *   "//[workgroup;][user[:password]@]host[/share[/path]]"
 * See http://ietf.org/internet-drafts/draft-crhertel-smb-url-07.txt
 */
int
smb_ctx_parseunc(struct smb_ctx *ctx, const char *unc, int sharetype,
	const char **next)
{
	const char *p = unc;
	char *p1, *colon, *servername, *tmp_utf8_servername;
	char tmp[1024];
	char tmp2[1024];
	int error ;

	ctx->ct_parsedlevel = SMBL_NONE;
	if (*p++ != '/' || *p++ != '/') {
		smb_error("UNC should start with '//'", 0);
		return EINVAL;
	}
	p1 = tmp;
	error = getsubstring(p, ';', p1, sizeof(tmp), &p);
	if (!error) {
		if (*p1 == 0) {
			smb_error("empty workgroup name", 0);
			return EINVAL;
		}
		nls_str_upper(tmp, tmp);
		error = smb_ctx_setworkgroup(ctx, unpercent(tmp), SETWG_FROMUSER);
		if (error)
			return error;
	}
	colon = (char *)p;
	error = getsubstring(p, '@', p1, sizeof(tmp), &p);
	if (!error) {
		if (ctx->ct_maxlevel < SMBL_VC) {
			smb_error("no user name required", 0);
			return EINVAL;
		}
		p1 = strchr(tmp, ':');
		if (p1) {
			colon += p1 - tmp;
			*p1++ = (char)0;
			error = smb_ctx_setpassword(ctx, unpercent(p1));
			if (error)
				return error;
			ctx->ct_flags |= SMBCF_EXPLICITPWD;
			if (p - colon > 2)
				memset(colon+1, '*', p - colon - 2);
		}
		p1 = tmp;
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

	/* 
	 * It's safe to uppercase this string, which
	 * consists of ascii characters that should
	 * be uppercased, %s, and ascii characters representing
	 * hex digits 0-9 and A-F (already uppercased, and
	 * if not uppercased they need to be). However,
	 * it is NOT safe to uppercase after it has been 
	 * converted, below! 
	 */
	
	nls_str_upper(tmp2,tmp);
	
	/*
	 * scan for % in the string.
	 * If we find one, convert 
	 * to the assumed codepage.
	 */

	if (strchr(tmp2,'%')) {
		/* use the 1st buffer, we don't need the old string */
		tmp_utf8_servername = unpercent(tmp2);
		/* Save original server component */
		ctx->ct_utf8_servname = strdup(tmp_utf8_servername);
		
		/* 
		 * Converts utf8 to win equivalent of 
		 * what is configured on this machine. 
		 * Note that we are assuming this is the 
		 * encoding used on the server, and that
		 * assumption might be incorrect. This is
		 * the best we can do now, and we should 
		 * move to use port 445 to avoid having
		 * to worry about server codepages.
		 */
		if (!(servername = convert_utf8_to_wincs(tmp_utf8_servername))) {
			smb_error("bad server name", 0);
			return EINVAL;
		}
	}
	else { /* no conversion needed */
		servername = tmp2;
		/* Save original server component */
		ctx->ct_utf8_servname = strdup(servername);
	}

	smb_ctx_setserver(ctx, servername);
	error = smb_ctx_setfullserver(ctx, servername);

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
	if (*p1 == 0 && ctx->ct_minlevel >= SMBL_SHARE &&
	    !(ctx->ct_flags & SMBCF_BROWSEOK)) {
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

int
smb_ctx_setfullserver(struct smb_ctx *ctx, const char *name)
{
	ctx->ct_fullserver = strdup(name);
	if (ctx->ct_fullserver == NULL)
		return ENOMEM;
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
 * See http://ietf.org/internet-drafts/draft-crhertel-smb-url-07.txt
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
			smb_ctx_setworkgroup(ctx, workgroup, SETWG_NOT_FROMUSER);
		if (server[0])
			smb_ctx_setserver(ctx, server);
	} else {
		if (ctx->ct_ssn.ioc_srvname[0] == (char)0)
			smb_ctx_setserver(ctx, "*SMBSERVER");
	}
#if 0
	if (server[0] == (char)0) {
		dot = strchr(ctx->ct_fullserver, '.');
		if (dot)
			*dot = '\0';
		if (strlen(ctx->ct_fullserver) <= SMB_MAXSRVNAMELEN) {
			/* don't uppercase the server name. it comes from
			* NBNS and uppercasing can clobber the characters */
			strcpy(ctx->ct_ssn.ioc_srvname, ctx->ct_fullserver);
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

/* this routine does not uppercase the server name */
void
smb_ctx_setserver(struct smb_ctx *ctx, const char *name)
{
	/* don't uppercase the server name */
	if (strlen(name) > SMB_MAXSRVNAMELEN) { /* NB limit is 15 */
		ctx->ct_ssn.ioc_srvname[0] = '\0';
	} else
		strcpy(ctx->ct_ssn.ioc_srvname, name);
	return;
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

/* Never uppercase the workgroup	
 * name here, because it might come
 * from a Windows codepage encoding.
 * "wgfromuser" means, if nonzero, the WG
 * name comes from mount_smbfs args or 
 * from config file.
 */
int
smb_ctx_setworkgroup(struct smb_ctx *ctx, const char *name,
			u_int32_t wgfromuser)
{
	/* 
	 * Don't overwrite WG name generated by user 
	 * with a name not generated by user. For
	 * example, if the user defined a workgroup
	 * via the "-W" argument on the command line,
	 * don't overwrite it with a name we got from 
	 * NBNS.  
	 */
	if (wgfromuser || (!(ctx->ct_flags & SMBCF_WGFROMUSR))) {
		if (strlen(name) >= SMB_MAXUSERNAMELEN) {
			smb_error("workgroup name '%s' too long", 0, name);
			return ENAMETOOLONG;
		}
		/* Remember if the WG name was defined by user */
		if (wgfromuser)
			ctx->ct_flags |= SMBCF_WGFROMUSR;
		strcpy(ctx->ct_ssn.ioc_workgroup, name);
	}
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
	char tmp[1024];

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
		nls_str_upper(tmp, arg);
		error = smb_ctx_setworkgroup(ctx, tmp, SETWG_FROMUSER);
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
	char password[SMB_MAXPASSWORDLEN + 1];
	int browseok = ctx->ct_flags & SMBCF_BROWSEOK;
	int renego = 0;
	
	password[0] = '\0';
	ctx->ct_flags &= ~SMBCF_RESOLVED;
	if (isatty(STDIN_FILENO))
		browseok = 0;
	if (ctx->ct_fullserver == NULL || ctx->ct_fullserver[0] == 0) {
		smb_error("no server name specified", 0);
		return EINVAL;
	}
	if (ctx->ct_minlevel >= SMBL_SHARE && sh->ioc_share[0] == 0 &&
	    !browseok) {
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
	    if (ssn->ioc_srvname[0]) 
		error = nbns_resolvename(ssn->ioc_srvname, 
			ctx->ct_nb, ctx, &sap);
	    else
	    	error = -1;
	    if (error && ctx->ct_fullserver) {
		    error = nb_resolvehost_in(ctx->ct_fullserver, &sap);
		    if (error == 0)
		    	smb_ctx_getnbname(ctx, sap);
	    }
	}
	if (error) {
		smb_error("can't get server address", error);
		return error;
	}
	nn.nn_scope = (u_char *)(ctx->ct_nb->nb_scope);
	nn.nn_type = NBT_SERVER;
	strcpy((char *)(nn.nn_name), ssn->ioc_srvname);
	error = nb_sockaddr(sap, &nn, &saserver);
	memcpy(&ctx->ct_srvinaddr, sap, sizeof (struct sockaddr_in));
	nb_snbfree(sap);
	if (error) {
		smb_error("can't allocate server address", error);
		return error;
	}
	ssn->ioc_server = (struct sockaddr*)saserver;
	if (ctx->ct_locname[0] == 0) {
		/* 
		 * Get the host name, but only copy what will fit in the buffer.
		 */ 
		error = nb_getlocalname(ctx->ct_locname, 
					(size_t)sizeof(ctx->ct_locname));
		if (error) {
			smb_error("can't get local name", error);
			return error;
		}
		nls_str_upper(ctx->ct_locname, ctx->ct_locname);
	}
	/* 
	 * Get the host name, but only copy what will fit in the buffer. This 
	 * corrects Radar 4321020. We were overriding the buffer. On Intel boxes
	 * this cause the saserver pointer to get overwritten. Which would 
	 * cause ssn->ioc_svlen to get set to zero. In the smb kernel we would
	 * malloc zero bytes. Accessing this pointer would cause a freeze.
	 * See the kernel code for those changes. 
	 */ 
	strlcpy((char *)nn.nn_name, (ctx->ct_locname), sizeof(nn.nn_name));
	nn.nn_type = NBT_WKSTA;
	nn.nn_scope = (u_char *)(ctx->ct_nb->nb_scope);
	error = nb_sockaddr(NULL, &nn, &salocal);
	if (error) {
		nb_snbfree((struct sockaddr*)saserver);
		smb_error("can't allocate local address", error);
		return error;
	}
	ssn->ioc_local = (struct sockaddr*)salocal;
	ssn->ioc_lolen = salocal->snb_len;
	ssn->ioc_svlen = saserver->snb_len;

	error = smb_ctx_negotiate(ctx, SMBL_SHARE, SMBLK_CREATE,ssn->ioc_workgroup);
	if (error)
		return (error);
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
		error = smb_ctx_setpassword(ctx, cp);
		if (error)
			return error;
		ctx->ct_flags |= SMBCF_EXPLICITPWD;
	}
	/*
	 * if we have a session it is either anonymous
	 * or from a stale authentication.  re-negotiating
	 * gets us ready for a fresh session
	 */
	if (ctx->ct_flags & SMBCF_SSNACTIVE || renego) {
		renego = 0;
		/* don't clobber workgroup name, pass null arg */
		error = smb_ctx_negotiate(ctx, SMBL_SHARE, SMBLK_CREATE, NULL);
		if (error)
			return (error);
	}
	if (browseok && !sh->ioc_share[0]) {
		ctx->ct_flags &= ~SMBCF_AUTHREQ;
		error = smb_browse(ctx, 0);
		if (ctx->ct_flags & SMBCF_KCFOUND && smb_autherr(error)) {
			smb_error("smb_ctx_resolve: bad keychain entry", 0);
			ctx->ct_flags |= SMBCF_KCBAD;
			renego = 1;
			goto reauth;
		}
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
	ctx->ct_flags |= SMBCF_RESOLVED;

	return 0;
}

static int
smb_ctx_gethandle(struct smb_ctx *ctx)
{
	int fd, i;
	char buf[20];

	if (ctx->ct_fd != -1) {
		rpc_cleanup_smbctx(ctx);
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
	seteuid(eff_uid); /* restore setuid root briefly */
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
	seteuid(real_uid); /* and back to real user */
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
 * Samba support lagged (not due to Samba itself, but due to OS'
 * Kerberos implementations.)
 *
 * Only session enc type should matter, not ticket enc type,
 * per Sam Hartman on krbdev.
 *
 * Preauthentication failure topics in krb-protocol may help here...
 * try "John Brezak" and/or "Clifford Neuman" too.
 */
static krb5_enctype kenctypes[] = {
	ENCTYPE_ARCFOUR_HMAC,	/* defined in Tiger krb5.h */
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
	u_char *	tkt;

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
		/*
		 * Avoid logging the typical "No credentials cache found"
		 */
		if (kerr != KRB5_FCC_NOFILE ||
		    strcmp(failure, "krb5_cc_get_principal"))
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
smb_ctx_principal2blob(size_t **blobp, char *prin)
{
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
smb_ctx_blob2principal(size_t *blob, u_char **prinp)
{
	/* Why does gcc4.0 require this cast ??? */
	size_t		len = (size_t)(*blob - SMB_GUIDLEN);
	u_char *	start = (u_char *)blob + sizeof(size_t) + SMB_GUIDLEN;
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
	if (!(prin = malloc(plen + 1)))
		goto out;
	failure = "spnegoGetMechListMIC";
	if ((rc = spnegoGetMechListMIC(stok, prin, &plen))) {
		free(prin);
		goto out;
	}
	prin[plen] = '\0';
	*prinp = prin;
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
smb_ctx_negotiate(struct smb_ctx *ctx, int level, int flags, char *workgroup)
{
	struct smbioc_lookup	rq;
	int	error = 0;
	char *	failure = NULL;
	u_char	*principal = NULL;
	u_char  *bytes;

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
						    &principal)) &&
		 !(failure = smb_ctx_principal2blob(&rq.ioc_ssn.ioc_intok,
						    (char *)principal))) {
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
	seteuid(eff_uid); /* restore setuid root briefly */
	/* calling "plain" ioctl instead of smb_ctx_ioctl doesn't get ioc_outtok */
	error = smb_ctx_ioctl(ctx, SMBIOC_NEGOTIATE, &rq);
	seteuid(real_uid); /* and back to real user */
	if (error) {
		smb_error("negotiate phase failed", error);
		rpc_cleanup_smbctx(ctx);
		close(ctx->ct_fd);
		ctx->ct_fd = -1;
	}

	/* make sure we're supposed to update the workgroup,
	* also check for no outtok */
	if (workgroup && rq.ioc_ssn.ioc_outtok && 
	    !(ctx->ct_flags & SMBCF_NONEGDOM) &&
	    !(ctx->ct_flags & SMBCF_WGFROMUSR)) {
		size_t outtoklen = *(rq.ioc_ssn.ioc_outtok);

		/*
		 * We might not get a domain name (Windows 98, and
		 * possibly other non-NT/W2K/etc. versions of Windows,
		 * don't seem to supply one).
		 *
		 * XXX - if the domain name is in Unicode, it might
		 * or might not be padded to be 2-byte aligned; I've
		 * seen captures where it is and captures where it
		 * isn't.  The captures where it's aligned might have
		 * come from buggy servers - but that suggests that
		 * Windows clients might not look at the domain name
		 * field.
		 *
		 * In at least some captures from Win2K's server,
		 * there's also a server name after the domain name.
		 */
		if (outtoklen > 4) {
			u_int32_t *outtokp = (u_int32_t *)(rq.ioc_ssn.ioc_outtok + 1);

			/*
			 * Copy the domain name from outtok because we are
			 * fixin' to free outtok.
			 *
			 * The Caps flag from the server in the Negotiate
			 * Protocol response is supplied to us by the kernel
			 * in host byte order, because it had already
			 * converted them for its own use.  We check it here
			 * to see whether the domain name string is in
			 * Unicode or not.
			 */
			outtoklen -= 4;	/* don't count Caps */
			if ((*outtokp) & SMB_CAP_UNICODE) {
				/* Unicode - convert to UTF-8 */
				unsigned short *workgroup_p = (unsigned short *)(outtokp + 1);
				size_t workgroup_len;
				unsigned short *unicode_workgroup;
				char *utf8_workgroup;

				/*
				 * Find the length of the workgroup string,
				 * in characters (not bytes), not counting
				 * any terminating null character.
				 */
				for (workgroup_len = 0;
				    2*workgroup_len < outtoklen &&
				      workgroup_p[workgroup_len] != 0;
				    workgroup_len++)
					;

				/*
				 * Make a copy of it with a null terminator;
				 * the null terminator might have been
				 * missing from the packet.
				 */
				unicode_workgroup = malloc(2*(workgroup_len + 1));
				memcpy(unicode_workgroup, workgroup_p,
				    2*workgroup_len);
				unicode_workgroup[workgroup_len] = 0;
				/* 
				 * Windows bug: If the domain name is 15 or more they might 
				 * put garbage in the 15th unicode character. Undo this 
				 * mischief by terminating with a null. The following code
				 * checks if the first 2 utf-16 characters in the string 
				 * match 8-bit representations of those unicode characters
				 * in position [15] in the utf-16 array, which would be 
				 * positions [30] and [31] in an 8-bit array. 
				 */
				bytes = (u_int8_t *)unicode_workgroup;
				if ((workgroup_len > 15) && 
				   (workgroup_len*2 == outtoklen-2) &&
				   (bytes[0] == bytes[30]) && (bytes[1] == 0) &&
				   (bytes[2] == bytes[31]) && (bytes[3] == 0))
					unicode_workgroup[15] = 0;

				/*
				 * Convert it to UTF-8; if it's Unicode, the
				 * kernel did *not* convert its characters
				 * to host byte order - they're little-endian -
				 * as it doesn't use it, it just supplies it
				 * to us.
				 */
				utf8_workgroup = convert_leunicode_to_utf8(unicode_workgroup);
				if (utf8_workgroup != NULL) {
					strncpy(workgroup, utf8_workgroup,
					    SMB_MAXUSERNAMELEN);
					workgroup[SMB_MAXUSERNAMELEN] = '\0';
					free(utf8_workgroup);
				} else {
					/*
					 * Conversion failed.
					 */
					workgroup[0] = '\0';
				}
				free(unicode_workgroup);
			} else {
				/* ASCII - just copy it */
				strncpy(workgroup, ((char *)rq.ioc_ssn.ioc_outtok+8),
				    SMB_MAXUSERNAMELEN);
				workgroup[SMB_MAXUSERNAMELEN] = '\0';
			}
		}
		free(rq.ioc_ssn.ioc_outtok);
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
		return (EINVAL);
	}
	if (ctx->ct_fd < 0) {
		smb_error("handle from smb_ctx_nego() gone?!", 0);
		return (EINVAL);
	}
	if (!(flags & SMBLK_CREATE))
		return (0);
	bzero(&rq, sizeof(rq));
	bcopy(&ctx->ct_ssn, &rq.ioc_ssn, sizeof(struct smbioc_ossn));
	bcopy(&ctx->ct_sh, &rq.ioc_sh, sizeof(struct smbioc_oshare));
	rq.ioc_flags = flags;
	rq.ioc_level = level;
	if (ctx->ct_secblob) { /* kerberos path... */
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
	/*
	 * The point here is to avoid trying defaulted passwords against
	 * people's accounts, as that can result in account lockout.
	 * We make an exception for null users as that's anonymous login
	 * and is expected to have a null/defaulted password.
	 */
	if (!(ctx->ct_flags & SMBCF_EXPLICITPWD) &&
	    ctx->ct_ssn.ioc_user[0] != '\0') {
		error = EINVAL;
		smb_error("no password specified for user %s", error,
			  ctx->ct_ssn.ioc_user);
		return (error);
	}
	seteuid(eff_uid); /* restore setuid root briefly */
	if (!(ctx->ct_flags & SMBCF_SSNACTIVE) &&
	    ioctl(ctx->ct_fd, SMBIOC_SSNSETUP, &rq) == -1)
		failure = "session setup";
	else {
		ctx->ct_flags |= SMBCF_SSNACTIVE;
		if (ioctl(ctx->ct_fd, SMBIOC_TCON, &rq) == -1)
			failure = "tree connect";
	}
	seteuid(real_uid); /* and back to real user */
	if (failure) {
		error = errno;
		smb_error("%s phase failed", error, failure);
	}
	return (error);
}

/*
 * Return the hflags2 word for an smb_ctx.
 */
u_int16_t
smb_ctx_flags2(struct smb_ctx *ctx)
{
	u_int16_t flags2;

	if (ioctl(ctx->ct_fd, SMBIOC_FLAGS2, &flags2) == -1) {
		smb_error("can't get flags2 for a session", errno);
		return -1;
	}
	return flags2;
}

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
		rc_getstringptr(smb_rc, sname, "use_negprot_domain", &p);
		if (p && !strcmp(p,"NO"))
			ctx->ct_flags |= SMBCF_NONEGDOM;
		rc_getstringptr(smb_rc, sname, "minauth", &p);
		if (p) {
			/*
			 * "minauth" was set in this section; override
			 * the current minimum authentication setting.
			 */
			ctx->ct_ssn.ioc_opt &= ~SMBVOPT_MINAUTH;
			if (strcmp(p, "kerberos") == 0) {
				/*
				 * Don't fall back to NTLMv2, NTLMv1, or
				 * a clear text password.
				 */
				ctx->ct_ssn.ioc_opt |= SMBVOPT_MINAUTH_KERBEROS;
			} else if (strcmp(p, "ntlmv2") == 0) {
				/*
				 * Don't fall back to NTLMv1 or a clear
				 * text password.
				 */
				ctx->ct_ssn.ioc_opt |= SMBVOPT_MINAUTH_NTLMV2;
			} else if (strcmp(p, "ntlm") == 0) {
				/*
				 * Don't send the LM response over the wire.
				 */
				ctx->ct_ssn.ioc_opt |= SMBVOPT_MINAUTH_NTLM;
			} else if (strcmp(p, "lm") == 0) {
				/*
				 * Fail if the server doesn't do encrypted
				 * passwords.
				 */
				ctx->ct_ssn.ioc_opt |= SMBVOPT_MINAUTH_LM;
			} else if (strcmp(p, "none") == 0) {
				/*
				 * Anything goes.
				 * (The following statement should be
				 * optimized away.)
				 */
				ctx->ct_ssn.ioc_opt |= SMBVOPT_MINAUTH_NONE;
			} else {
				/*
				 * Unknown minimum authentication level.
				 */
				smb_error("invalid minimum authentication level \"%s\" specified in the section %s", 0, p, sname);
				return EINVAL;
			}
		}
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
			else
				ctx->ct_flags |= SMBCF_EXPLICITPWD;
		}
	}
	rc_getstringptr(smb_rc, sname, "workgroup", &p);
	if (p) {
		nls_str_upper(p,p);
		/* This is also user config, so mark it as such */
		error = smb_ctx_setworkgroup(ctx, p, SETWG_FROMUSER);
		if (error)
			smb_error("workgroup specification in the section '%s' ignored", error, sname);
	}
	return 0;
}

/*
 * read rc file as follows:
 * 1. read [default] section
 * 2. override with [server] section
 * 3. override with [server:user] section
 * 4. override with [server:user:share] section
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

	/*
	 * default parameters
	 */
	smb_ctx_readrcsection(ctx, "default", 0);
	nb_ctx_readrcsection(smb_rc, ctx->ct_nb, "default", 0);

	/*
	 * If we don't have a server name, we can't read any of the
	 * [server...] sections.
	 */
	if (ctx->ct_ssn.ioc_srvname[0] == 0)
		return 0;

	/*
	 * SERVER parameters.
	 */
	smb_ctx_readrcsection(ctx, ctx->ct_ssn.ioc_srvname, 1);
	nb_ctx_readrcsection(smb_rc, ctx->ct_nb, ctx->ct_ssn.ioc_srvname, 1);

	/*
	 * If we don't have a user name, we can't read any of the
	 * [server:user...] sections.
	 */
	if (ctx->ct_ssn.ioc_user[0] == 0)
		return 0;

	/*
	 * SERVER:USER parameters
	 */
	snprintf(sname, sizeof(sname), "%s:%s", ctx->ct_ssn.ioc_srvname,
	    ctx->ct_ssn.ioc_user);
	smb_ctx_readrcsection(ctx, sname, 2);

	/*
	 * If we don't have a share name, we can't read any of the
	 * [server:user:share] sections.
	 */
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
