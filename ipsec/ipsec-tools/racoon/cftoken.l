/*	$NetBSD: cftoken.l,v 1.11.4.1 2007/08/01 11:52:20 vanhu Exp $	*/

/* Id: cftoken.l,v 1.53 2006/08/22 18:17:17 manubsd Exp */
%option noyywrap
%{
/*
 * Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002 and 2003 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#ifdef HAVE_NETINET6_IPSEC 
#  include <netinet6/ipsec.h>
#else 
#  include <netinet/ipsec.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <glob.h>
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#else
#include <varargs.h>
#endif

//%%% BUG FIX - 2 missing include files when not using
// the bison files
#ifdef HAVE_OPENSSL
#include <openssl/bn.h>
#include <openssl/rsa.h>
#endif
	
#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "debug.h"

#include "algorithm.h"
#include "cfparse_proto.h"
#include "cftoken_proto.h"
#include "localconf.h"
#include "oakley.h"
#include "isakmp_var.h"
#include "isakmp.h"
#include "ipsec_doi.h"
#include "policy.h"
#include "proposal.h"
#include "remoteconf.h"
#include "nattraversal.h"
#ifdef GC
#include "gcmalloc.h"
#endif

#include "y.tab.h"

int yyerrorcount = 0;

#if defined(YIPS_DEBUG)
#  define YYDB plog(ASL_LEVEL_DEBUG,                                \
		"begin <%d>%s\n", yy_start, yytext);
#  define YYD {                                                                \
	plog(ASL_LEVEL_DEBUG, "<%d>%s",                             \
	    yy_start, loglevel >= ASL_LEVEL_DEBUG ? "\n" : "");                     \
}
#else
#  define YYDB
#  define YYD
#endif /* defined(YIPS_DEBUG) */

#define MAX_INCLUDE_DEPTH 10

static struct include_stack {
	char *path;
	FILE *fp;
	YY_BUFFER_STATE prevstate;
	int lineno;
	glob_t matches;
	int matchon;
} incstack[MAX_INCLUDE_DEPTH];
static int incstackp = 0;

static int yy_first_time = 1;
%}

/* common section */
nl		\n
ws		[ \t]+
digit		[0-9]
letter		[A-Za-z]
hexdigit	[0-9A-Fa-f]
/*octet		(([01]?{digit}?{digit})|((2([0-4]{digit}))|(25[0-5]))) */
special		[()+\|\?\*]
comma		\,
dot		\.
slash		\/
bcl		\{
ecl		\}
blcl		\[
elcl		\]
hyphen          \-
percent		\%
semi		\;
comment		\#.*
ccomment	"/*"
bracketstring	\<[^>]*\>
quotedstring	\"[^"]*\"
addrstring	[a-fA-F0-9:]([a-fA-F0-9:\.]*|[a-fA-F0-9:\.]*%[a-zA-Z0-9]*)
decstring	{digit}+
hexstring	0x{hexdigit}+

%s S_INI S_PRIV S_PTH S_INF S_LOG S_PAD S_LST S_RTRY S_CFG
%s S_ALGST S_ALGCL
%s S_SAINF S_SAINFS
%s S_RMT S_RMTS S_RMTP
%s S_SA
%s S_GSSENC

%%
%{
	if (yy_first_time) {
		BEGIN S_INI;
		yy_first_time = 0;
	}
%}


	/* path */
<S_INI>path		{ BEGIN S_PTH; YYDB; return(PATH); }
<S_PTH>include		{ YYD; yylval.num = LC_PATHTYPE_INCLUDE;
				return(PATHTYPE); }
<S_PTH>pre_shared_key	{ YYD; yylval.num = LC_PATHTYPE_PSK;
				return(PATHTYPE); }
<S_PTH>certificate	{ YYD; yylval.num = LC_PATHTYPE_CERT;
				return(PATHTYPE); }
<S_PTH>pidfile		{ YYD; yylval.num = LC_PATHTYPE_PIDFILE;
				return(PATHTYPE); }
<S_PTH>logfile		{ YYD; yylval.num = LC_PATHTYPE_LOGFILE;
				return(PATHTYPE); }
<S_PTH>{semi}		{ BEGIN S_INI; YYDB; return(EOS); }

	/* include */
<S_INI>include		{ YYDB; return(INCLUDE); }

	/* self information */
<S_INI>identifier	{ BEGIN S_INF; YYDB; racoon_yywarn("it is obsoleted.  use \"my_identifier\" in each remote directives."); return(IDENTIFIER); }
<S_INF>{semi}		{ BEGIN S_INI; return(EOS); }

	/* special */
<S_INI>complex_bundle	{ YYDB; return(COMPLEX_BUNDLE); }

	/* logging */
<S_INI>log		{ BEGIN S_LOG; YYDB; return(LOGGING); }
<S_LOG>error		{ YYD; yylval.num = ASL_LEVEL_ERR; return(LOGLEV); }
<S_LOG>warning		{ YYD; yylval.num = ASL_LEVEL_WARNING; return(LOGLEV); }
<S_LOG>notify		{ YYD; yylval.num = ASL_LEVEL_NOTICE; return(LOGLEV); }
<S_LOG>info		{ YYD; yylval.num = ASL_LEVEL_INFO; return(LOGLEV); }
<S_LOG>debug		{ YYD; yylval.num = ASL_LEVEL_DEBUG; return(LOGLEV); }
<S_LOG>debug2		{ YYD; yylval.num = ASL_LEVEL_DEBUG; return(LOGLEV); }
<S_LOG>debug3		{ YYD; racoon_yywarn("it is obsoleted.  use \"debug2\""); yylval.num = ASL_LEVEL_DEBUG; return(LOGLEV); }
<S_LOG>debug4		{ YYD; racoon_yywarn("it is obsoleted.  use \"debug2\""); yylval.num = ASL_LEVEL_DEBUG; return(LOGLEV); }
<S_LOG>{semi}		{ BEGIN S_INI; return(EOS); }

	/* padding */
<S_INI>padding		{ BEGIN S_PAD; YYDB; return(PADDING); }
<S_PAD>{bcl}		{ return(BOC); }
<S_PAD>randomize	{ YYD; return(PAD_RANDOMIZE); }
<S_PAD>randomize_length	{ YYD; return(PAD_RANDOMIZELEN); }
<S_PAD>maximum_length	{ YYD; return(PAD_MAXLEN); }
<S_PAD>strict_check	{ YYD; return(PAD_STRICT); }
<S_PAD>exclusive_tail	{ YYD; return(PAD_EXCLTAIL); }
<S_PAD>{ecl}		{ BEGIN S_INI; return(EOC); }

	/* listen */
<S_INI>listen		{ BEGIN S_LST; YYDB; return(LISTEN); }
<S_LST>{bcl}		{ return(BOC); }
<S_LST>isakmp		{ YYD; return(X_ISAKMP); }
<S_LST>isakmp_natt	{ YYD; return(X_ISAKMP_NATT); }
<S_LST>admin		{ YYD; return(X_ADMIN); }
<S_LST>adminsock	{ YYD; return(ADMINSOCK); }
<S_LST>disabled		{ YYD; return(DISABLED); }
<S_LST>strict_address	{ YYD; return(STRICT_ADDRESS); }
<S_LST>{ecl}		{ BEGIN S_INI; return(EOC); }

	/* mode_cfg */
<S_INI>mode_cfg		{ BEGIN S_CFG; YYDB; return(MODECFG); }
<S_CFG>{bcl}		{ return(BOC); }
<S_CFG>network4		{ YYD; return(CFG_NET4); }
<S_CFG>netmask4		{ YYD; return(CFG_MASK4); }
<S_CFG>dns4		{ YYD; return(CFG_DNS4); }
<S_CFG>wins4		{ YYD; return(CFG_NBNS4); }
<S_CFG>default_domain	{ YYD; return(CFG_DEFAULT_DOMAIN); }
<S_CFG>auth_source	{ YYD; return(CFG_AUTH_SOURCE); }
<S_CFG>auth_groups	{ YYD; return(CFG_AUTH_GROUPS); }
<S_CFG>group_source	{ YYD; return(CFG_GROUP_SOURCE); }
<S_CFG>conf_source	{ YYD; return(CFG_CONF_SOURCE); }
<S_CFG>accounting	{ YYD; return(CFG_ACCOUNTING); }
<S_CFG>system		{ YYD; return(CFG_SYSTEM); }
<S_CFG>local		{ YYD; return(CFG_LOCAL); }
<S_CFG>none		{ YYD; return(CFG_NONE); }
<S_CFG>radius		{ YYD; return(CFG_RADIUS); }
<S_CFG>pam		{ YYD; return(CFG_PAM); }
<S_CFG>pool_size	{ YYD; return(CFG_POOL_SIZE); }
<S_CFG>banner		{ YYD; return(CFG_MOTD); }
<S_CFG>auth_throttle	{ YYD; return(CFG_AUTH_THROTTLE); }
<S_CFG>split_network	{ YYD; return(CFG_SPLIT_NETWORK); }
<S_CFG>local_lan	{ YYD; return(CFG_SPLIT_LOCAL); }
<S_CFG>include		{ YYD; return(CFG_SPLIT_INCLUDE); }
<S_CFG>split_dns	{ YYD; return(CFG_SPLIT_DNS); }
<S_CFG>pfs_group	{ YYD; return(CFG_PFS_GROUP); }
<S_CFG>save_passwd	{ YYD; return(CFG_SAVE_PASSWD); }
<S_CFG>{comma}		{ YYD; return(COMMA); }
<S_CFG>{ecl}		{ BEGIN S_INI; return(EOC); }

	/* timer */
<S_INI>timer		{ BEGIN S_RTRY; YYDB; return(RETRY); }
<S_RTRY>{bcl}		{ return(BOC); }
<S_RTRY>counter		{ YYD; return(RETRY_COUNTER); }
<S_RTRY>interval	{ YYD; return(RETRY_INTERVAL); }
<S_RTRY>persend		{ YYD; return(RETRY_PERSEND); }
<S_RTRY>phase1		{ YYD; return(RETRY_PHASE1); }
<S_RTRY>phase2		{ YYD; return(RETRY_PHASE2); }
<S_RTRY>natt_keepalive	{ YYD; return(NATT_KA); }
<S_RTRY>auto_exit_delay	{ YYD; return(AUTO_EXIT_DELAY); } 
<S_RTRY>{ecl}		{ BEGIN S_INI; return(EOC); }

	/* sainfo */
<S_INI>sainfo		{ BEGIN S_SAINF; YYDB; return(SAINFO); }
<S_SAINF>anonymous	{ YYD; return(ANONYMOUS); }
<S_SAINF>{blcl}any{elcl}	{ YYD; return(PORTANY); }
<S_SAINF>any		{ YYD; return(ANY); }
<S_SAINF>from		{ YYD; return(FROM); }
<S_SAINF>group		{ YYD; return(GROUP); }
	/* sainfo spec */
<S_SAINF>{bcl}		{ BEGIN S_SAINFS; return(BOC); }
<S_SAINF>{semi}		{ BEGIN S_INI; return(EOS); }
<S_SAINFS>{ecl}		{ BEGIN S_INI; return(EOC); }
<S_SAINFS>pfs_group	{ YYD; return(PFS_GROUP); }
<S_SAINFS>remoteid	{ YYD; return(REMOTEID); }
<S_SAINFS>identifier	{ YYD; racoon_yywarn("it is obsoleted.  use \"my_identifier\"."); return(IDENTIFIER); }
<S_SAINFS>my_identifier	{ YYD; return(MY_IDENTIFIER); }
<S_SAINFS>lifetime	{ YYD; return(LIFETIME); }
<S_SAINFS>time		{ YYD; return(LIFETYPE_TIME); }
<S_SAINFS>byte		{ YYD; return(LIFETYPE_BYTE); }
<S_SAINFS>encryption_algorithm { YYD; yylval.num = algclass_ipsec_enc; return(ALGORITHM_CLASS); }
<S_SAINFS>authentication_algorithm { YYD; yylval.num = algclass_ipsec_auth; return(ALGORITHM_CLASS); }
<S_SAINFS>compression_algorithm	{ YYD; yylval.num = algclass_ipsec_comp; return(ALGORITHM_CLASS); }
<S_SAINFS>{comma}	{ YYD; return(COMMA); }

	/* remote */
<S_INI>remote		{ BEGIN S_RMT; YYDB; return(REMOTE); }
<S_RMT>anonymous	{ YYD; return(ANONYMOUS); }
<S_RMT>inherit		{ YYD; return(INHERIT); }
	/* remote spec */
<S_RMT>{bcl}		{ BEGIN S_RMTS; return(BOC); }
<S_RMTS>{ecl}		{ BEGIN S_INI; return(EOC); }
<S_RMTS>exchange_mode	{ YYD; return(EXCHANGE_MODE); }
<S_RMTS>{comma}		{ YYD; /* XXX ignored, but to be handled. */ ; }
<S_RMTS>main		{ YYD; yylval.num = ISAKMP_ETYPE_IDENT; return(EXCHANGETYPE); }
<S_RMTS>aggressive	{ YYD; yylval.num = ISAKMP_ETYPE_AGG; return(EXCHANGETYPE); }
<S_RMTS>doi		{ YYD; return(DOI); }
<S_RMTS>ipsec_doi	{ YYD; yylval.num = IPSEC_DOI; return(DOITYPE); }
<S_RMTS>situation	{ YYD; return(SITUATION); }
<S_RMTS>ike_version	{ YYD; return(IKE_VERSION); }
<S_RMTS>identity_only	{ YYD; yylval.num = IPSECDOI_SIT_IDENTITY_ONLY; return(SITUATIONTYPE); }
<S_RMTS>secrecy		{ YYD; yylval.num = IPSECDOI_SIT_SECRECY; return(SITUATIONTYPE); }
<S_RMTS>integrity	{ YYD; yylval.num = IPSECDOI_SIT_INTEGRITY; return(SITUATIONTYPE); }
<S_RMTS>identifier	{ YYD; racoon_yywarn("it is obsoleted.  use \"my_identifier\"."); return(IDENTIFIER); }
<S_RMTS>my_identifier	{ YYD; return(MY_IDENTIFIER); }
<S_RMTS>xauth_login	{ YYD; return(XAUTH_LOGIN); /* formerly identifier type login */ }
<S_RMTS>peers_identifier	{ YYD; return(PEERS_IDENTIFIER); }
<S_RMTS>verify_identifier	{ YYD; return(VERIFY_IDENTIFIER); }
<S_RMTS>local_address	{ YYD; return(LOCAL_ADDRESS); }
<S_RMTS>certificate_type	{ YYD; return(CERTIFICATE_TYPE); }
<S_RMTS>ca_type		{ YYD; return(CA_TYPE); }
<S_RMTS>x509		{ YYD; yylval.num = ISAKMP_CERT_X509SIGN; return(CERT_X509); }
<S_RMTS>plain_rsa	{
	racoon_yyerror("plainrsa not supported.");
}
<S_RMTS>open_dir_auth_group	{ 
#if HAVE_OPENDIR
	YYD; 
	return(OPEN_DIR_AUTH_GROUP); 
#else
	racoon_yyerror("Apple specific features not compiled in.");
#endif
}
<S_RMTS>shared_secret { 
	YYD; 
	return(SHARED_SECRET); 
}
<S_RMTS>in_keychain { 
	YYD; 
	return(IN_KEYCHAIN); 
}
<S_RMTS>certificate_verification { 
	YYD; 
	return(CERTIFICATE_VERIFICATION); 
}
<S_RMTS>peers_certfile	{ YYD; return(PEERS_CERTFILE); }
<S_RMTS>dnssec		{ YYD; return(DNSSEC); }
<S_RMTS>verify_cert	{ YYD; return(VERIFY_CERT); }
<S_RMTS>send_cert	{ YYD; return(SEND_CERT); }
<S_RMTS>send_cr		{ YYD; return(SEND_CR); }
<S_RMTS>dh_group	{ YYD; return(DH_GROUP); }
<S_RMTS>nonce_size	{ YYD; return(NONCE_SIZE); }
<S_RMTS>generate_policy	{ YYD; return(GENERATE_POLICY); }
<S_RMTS>support_mip6	{ YYD; racoon_yywarn("it is obsoleted.  use \"support_proxy\"."); return(SUPPORT_PROXY); }
<S_RMTS>support_proxy	{ YYD; return(SUPPORT_PROXY); }
<S_RMTS>initial_contact	{ YYD; return(INITIAL_CONTACT); }
<S_RMTS>nat_traversal	{ YYD; return(NAT_TRAVERSAL); }
<S_RMTS>force		{ YYD; yylval.num = NATT_FORCE; return(NAT_TRAVERSAL_LEVEL); }
<S_RMTS>nat_traversal_multi_user {
	YYD; 
	return(NAT_TRAVERSAL_MULTI_USER);
}
<S_RMTS>nat_traversal_keepalive {
	YYD; 
	return(NAT_TRAVERSAL_KEEPALIVE);
}
<S_RMTS>proposal_check	{ YYD; return(PROPOSAL_CHECK); }
<S_RMTS>obey		{ YYD; yylval.num = PROP_CHECK_OBEY; return(PROPOSAL_CHECK_LEVEL); }
<S_RMTS>strict		{ YYD; yylval.num = PROP_CHECK_STRICT; return(PROPOSAL_CHECK_LEVEL); }
<S_RMTS>exact		{ YYD; yylval.num = PROP_CHECK_EXACT; return(PROPOSAL_CHECK_LEVEL); }
<S_RMTS>claim		{ YYD; yylval.num = PROP_CHECK_CLAIM; return(PROPOSAL_CHECK_LEVEL); }
<S_RMTS>keepalive	{ YYD; return(KEEPALIVE); }
<S_RMTS>passive		{ YYD; return(PASSIVE); }
<S_RMTS>lifetime	{ YYD; return(LIFETIME); }
<S_RMTS>time		{ YYD; return(LIFETYPE_TIME); }
<S_RMTS>byte		{ YYD; return(LIFETYPE_BYTE); }
<S_RMTS>dpd			{ YYD; return(DPD); }
<S_RMTS>dpd_delay	{ YYD; return(DPD_DELAY); }
<S_RMTS>dpd_retry	{ YYD; return(DPD_RETRY); }
<S_RMTS>dpd_maxfail	{ YYD; return(DPD_MAXFAIL); }
<S_RMTS>dpd_algorithm	{ YYD; return(DPD_ALGORITHM); }
<S_RMTS>disconnect_on_idle { YYD; return(DISCONNECT_ON_IDLE); }
<S_RMTS>idle_timeout { YYD; return(IDLE_TIMEOUT); }
<S_RMTS>idle_direction { YYD; return(IDLE_DIRECTION); }
<S_RMTS>ike_frag	{ YYD; return(IKE_FRAG); }
<S_RMTS>esp_frag	{ YYD; return(ESP_FRAG); }
<S_RMTS>mode_cfg	{ YYD; return(MODE_CFG); }
<S_RMTS>weak_phase1_check { YYD; return(WEAK_PHASE1_CHECK); }
	/* remote proposal */
<S_RMTS>proposal	{ BEGIN S_RMTP; YYDB; return(PROPOSAL); }
<S_RMTP>{bcl}		{ return(BOC); }
<S_RMTP>{ecl}		{ BEGIN S_RMTS; return(EOC); }
<S_RMTP>lifetime	{ YYD; return(LIFETIME); }
<S_RMTP>time		{ YYD; return(LIFETYPE_TIME); }
<S_RMTP>byte		{ YYD; return(LIFETYPE_BYTE); }
<S_RMTP>encryption_algorithm { YYD; yylval.num = algclass_isakmp_enc; return(ALGORITHM_CLASS); }
<S_RMTP>authentication_method { YYD; yylval.num = algclass_isakmp_ameth; return(ALGORITHM_CLASS); }
<S_RMTP>hash_algorithm	{ YYD; yylval.num = algclass_isakmp_hash; return(ALGORITHM_CLASS); }
<S_RMTP>dh_group	{ YYD; return(DH_GROUP); }
<S_RMTP>gss_id		{ YYD; return(GSS_ID); }
<S_RMTP>gssapi_id	{ YYD; return(GSS_ID); } /* for back compatibility */

	/* GSS ID encoding type (global) */
<S_INI>gss_id_enc	{ BEGIN S_GSSENC; YYDB; return(GSS_ID_ENC); }
<S_GSSENC>latin1	{ YYD; yylval.num = LC_GSSENC_LATIN1;
				return(GSS_ID_ENCTYPE); }
<S_GSSENC>utf-16le	{ YYD; yylval.num = LC_GSSENC_UTF16LE;
				return(GSS_ID_ENCTYPE); }
<S_GSSENC>{semi}	{ BEGIN S_INI; YYDB; return(EOS); }

	/* parameter */
on		{ YYD; yylval.num = TRUE; return(SWITCH); }
off		{ YYD; yylval.num = FALSE; return(SWITCH); }

	/* prefix */
{slash}({digit}{1,3}) {
			YYD;
			yytext++;
			yylval.num = atoi(yytext);
			return(PREFIX);
		}

	/* port number */
{blcl}{decstring}{elcl}	{
			char *p = yytext;
			YYD;
			while (*++p != ']') ;
			*p = 0;
			yytext++;
			yylval.num = atoi(yytext);
			return(PORT);
		}

	/* address range */
{hyphen}{addrstring} {
                        YYD;
                        yytext++;
			yylval.val = vmalloc(yyleng + 1);
			if (yylval.val == NULL) {
				racoon_yyerror("vmalloc failed");
				return -1;
			}
			memcpy(yylval.val->v, yytext, yylval.val->l);
                        return(ADDRRANGE);
                } 

	/* upper protocol */
esp		{ YYD; yylval.num = IPPROTO_ESP; return(UL_PROTO); }
ah		{ YYD; yylval.num = IPPROTO_AH; return(UL_PROTO); }
ipcomp		{ YYD; yylval.num = IPPROTO_IPCOMP; return(UL_PROTO); }
icmp		{ YYD; yylval.num = IPPROTO_ICMP; return(UL_PROTO); }
icmp6		{ YYD; yylval.num = IPPROTO_ICMPV6; return(UL_PROTO); }
tcp		{ YYD; yylval.num = IPPROTO_TCP; return(UL_PROTO); }
udp		{ YYD; yylval.num = IPPROTO_UDP; return(UL_PROTO); }

	/* algorithm type */
des_iv64	{ YYD; yylval.num = algtype_des_iv64;	return(ALGORITHMTYPE); }
des		{ YYD; yylval.num = algtype_des;	return(ALGORITHMTYPE); }
3des		{ YYD; yylval.num = algtype_3des;	return(ALGORITHMTYPE); }
rc5		{ YYD; yylval.num = algtype_rc5;	return(ALGORITHMTYPE); }
idea 		{ YYD; yylval.num = algtype_idea;	return(ALGORITHMTYPE); }
cast128		{ YYD; yylval.num = algtype_cast128;	return(ALGORITHMTYPE); }
blowfish	{ YYD; yylval.num = algtype_blowfish;	return(ALGORITHMTYPE); }
3idea		{ YYD; yylval.num = algtype_3idea;	return(ALGORITHMTYPE); }
des_iv32	{ YYD; yylval.num = algtype_des_iv32;	return(ALGORITHMTYPE); }
rc4 		{ YYD; yylval.num = algtype_rc4;	return(ALGORITHMTYPE); }
null_enc	{ YYD; yylval.num = algtype_null_enc;	return(ALGORITHMTYPE); }
null		{ YYD; yylval.num = algtype_null_enc;	return(ALGORITHMTYPE); }
aes		{ YYD; yylval.num = algtype_aes;	return(ALGORITHMTYPE); }
rijndael	{ YYD; yylval.num = algtype_aes;	return(ALGORITHMTYPE); }
twofish		{ YYD; yylval.num = algtype_twofish;	return(ALGORITHMTYPE); }
non_auth	{ YYD; yylval.num = algtype_non_auth;	return(ALGORITHMTYPE); }
hmac_md5	{ YYD; yylval.num = algtype_hmac_md5_128;	return(ALGORITHMTYPE); }
hmac_sha1	{ YYD; yylval.num = algtype_hmac_sha1_160;	return(ALGORITHMTYPE); }
hmac_sha2_256	{ YYD; yylval.num = algtype_hmac_sha2_256;	return(ALGORITHMTYPE); }
hmac_sha256	{ YYD; yylval.num = algtype_hmac_sha2_256;	return(ALGORITHMTYPE); }
hmac_sha2_384	{ YYD; yylval.num = algtype_hmac_sha2_384;	return(ALGORITHMTYPE); }
hmac_sha384	{ YYD; yylval.num = algtype_hmac_sha2_384;	return(ALGORITHMTYPE); }
hmac_sha2_512	{ YYD; yylval.num = algtype_hmac_sha2_512;	return(ALGORITHMTYPE); }
hmac_sha512	{ YYD; yylval.num = algtype_hmac_sha2_512;	return(ALGORITHMTYPE); }
hmac_md5_96	{ YYD; yylval.num = algtype_hmac_md5_96;	return(ALGORITHMTYPE); }
hmac_sha1_96	{ YYD; yylval.num = algtype_hmac_sha1_96;	return(ALGORITHMTYPE); }
des_mac		{ YYD; yylval.num = algtype_des_mac;	return(ALGORITHMTYPE); }
kpdk		{ YYD; yylval.num = algtype_kpdk;	return(ALGORITHMTYPE); }
md5		{ YYD; yylval.num = algtype_md5;	return(ALGORITHMTYPE); }
sha1		{ YYD; yylval.num = algtype_sha1;	return(ALGORITHMTYPE); }
tiger		{ YYD; yylval.num = algtype_tiger;	return(ALGORITHMTYPE); }
sha2_256	{ YYD; yylval.num = algtype_sha2_256;	return(ALGORITHMTYPE); }
sha256		{ YYD; yylval.num = algtype_sha2_256;	return(ALGORITHMTYPE); }
sha2_384	{ YYD; yylval.num = algtype_sha2_384;	return(ALGORITHMTYPE); }
sha384		{ YYD; yylval.num = algtype_sha2_384;	return(ALGORITHMTYPE); }
sha2_512	{ YYD; yylval.num = algtype_sha2_512;	return(ALGORITHMTYPE); }
sha512		{ YYD; yylval.num = algtype_sha2_512;	return(ALGORITHMTYPE); }
oui		{ YYD; yylval.num = algtype_oui;	return(ALGORITHMTYPE); }
deflate		{ YYD; yylval.num = algtype_deflate;	return(ALGORITHMTYPE); }
lzs		{ YYD; yylval.num = algtype_lzs;	return(ALGORITHMTYPE); }
modp768		{ YYD; yylval.num = algtype_modp768;	return(ALGORITHMTYPE); }
modp1024	{ YYD; yylval.num = algtype_modp1024;	return(ALGORITHMTYPE); }
modp1536	{ YYD; yylval.num = algtype_modp1536;	return(ALGORITHMTYPE); }
ec2n155		{ YYD; yylval.num = algtype_ec2n155;	return(ALGORITHMTYPE); }
ec2n185		{ YYD; yylval.num = algtype_ec2n185;	return(ALGORITHMTYPE); }
modp2048	{ YYD; yylval.num = algtype_modp2048;	return(ALGORITHMTYPE); }
modp3072	{ YYD; yylval.num = algtype_modp3072;	return(ALGORITHMTYPE); }
modp4096	{ YYD; yylval.num = algtype_modp4096;	return(ALGORITHMTYPE); }
modp6144	{ YYD; yylval.num = algtype_modp6144;	return(ALGORITHMTYPE); }
modp8192	{ YYD; yylval.num = algtype_modp8192;	return(ALGORITHMTYPE); }
pre_shared_key	{ YYD; yylval.num = algtype_psk;	return(ALGORITHMTYPE); }
rsasig		{ YYD; yylval.num = algtype_rsasig;	return(ALGORITHMTYPE); }
dsssig		{ YYD; yylval.num = algtype_dsssig;	return(ALGORITHMTYPE); }
rsaenc		{ YYD; yylval.num = algtype_rsaenc;	return(ALGORITHMTYPE); }
rsarev		{ YYD; yylval.num = algtype_rsarev;	return(ALGORITHMTYPE); }
gssapi_krb	{ YYD; yylval.num = algtype_gssapikrb;	return(ALGORITHMTYPE); }
hybrid_rsa_server {
#ifdef ENABLE_HYBRID
	YYD; yylval.num = algtype_hybrid_rsa_s; return(ALGORITHMTYPE);
#else
	racoon_yyerror("racoon not configured with --enable-hybrid");
#endif
}
hybrid_dss_server {
#ifdef ENABLE_HYBRID
	YYD; yylval.num = algtype_hybrid_dss_s; return(ALGORITHMTYPE);
#else
	racoon_yyerror("racoon not configured with --enable-hybrid");
#endif
}
hybrid_rsa_client {
#ifdef ENABLE_HYBRID
	YYD; yylval.num = algtype_hybrid_rsa_c; return(ALGORITHMTYPE);
#else
	racoon_yyerror("racoon not configured with --enable-hybrid");
#endif
}
hybrid_dss_client {
#ifdef ENABLE_HYBRID
	YYD; yylval.num = algtype_hybrid_dss_c; return(ALGORITHMTYPE);
#else
	racoon_yyerror("racoon not configured with --enable-hybrid");
#endif
}

xauth_psk_server {
#ifdef ENABLE_HYBRID
	YYD; yylval.num = algtype_xauth_psk_s; return(ALGORITHMTYPE);
#else
	racoon_yyerror("racoon not configured with --enable-hybrid");
#endif
}
xauth_psk_client {
#ifdef ENABLE_HYBRID
	YYD; yylval.num = algtype_xauth_psk_c; return(ALGORITHMTYPE);
#else
	racoon_yyerror("racoon not configured with --enable-hybrid");
#endif
}
xauth_rsa_server {
#ifdef ENABLE_HYBRID
	YYD; yylval.num = algtype_xauth_rsa_s; return(ALGORITHMTYPE);
#else
	racoon_yyerror("racoon not configured with --enable-hybrid");
#endif
}
xauth_rsa_client {
#ifdef ENABLE_HYBRID
	YYD; yylval.num = algtype_xauth_rsa_c; return(ALGORITHMTYPE);
#else
	racoon_yyerror("racoon not configured with --enable-hybrid");
#endif
}
eap_psk_client {
    #ifdef ENABLE_HYBRID
	YYD; yylval.num = algtype_eap_psk_c; return(ALGORITHMTYPE);
    #else
	racoon_yyerror("racoon not configured with --enable-hybrid");
    #endif
}
eap_rsa_client {
    #ifdef ENABLE_HYBRID
	YYD; yylval.num = algtype_eap_rsa_c; return(ALGORITHMTYPE);
    #else
	racoon_yyerror("racoon not configured with --enable-hybrid");
    #endif
}



	/* identifier type */
vendor_id	{ YYD; racoon_yywarn("it is obsoleted."); return(VENDORID); }
user_fqdn	{ YYD; yylval.num = IDTYPE_USERFQDN; return(IDENTIFIERTYPE); }
fqdn		{ YYD; yylval.num = IDTYPE_FQDN; return(IDENTIFIERTYPE); }
keyid		{ YYD; yylval.num = IDTYPE_KEYID; return(IDENTIFIERTYPE); }
keyid_use	{ 
	YYD; 
	yylval.num = IDTYPE_KEYIDUSE; 
	return(IDENTIFIERTYPE); 
}
address		{ YYD; yylval.num = IDTYPE_ADDRESS; return(IDENTIFIERTYPE); }
subnet		{ YYD; yylval.num = IDTYPE_SUBNET; return(IDENTIFIERTYPE); }
asn1dn		{ YYD; yylval.num = IDTYPE_ASN1DN; return(IDENTIFIERTYPE); }
certname	{ YYD; racoon_yywarn("certname will be obsoleted in near future."); yylval.num = IDTYPE_ASN1DN; return(IDENTIFIERTYPE); }

	/* shared secret type */
use		{ 
	YYD; 
	yylval.num = SECRETTYPE_USE; 
	return(SECRETTYPE); 
}
key		{ 
	YYD; 
	yylval.num = SECRETTYPE_KEY; 
	return(SECRETTYPE); 
}
keychain	{ 
#if HAVE_KEYCHAIN
	YYD; 
	yylval.num = SECRETTYPE_KEYCHAIN; 
	return(SECRETTYPE); 
#else
	racoon_yyerror("Apple specific features not compiled in.");
#endif
}
keychain_by_id	{ 
	YYD; 
	yylval.num = SECRETTYPE_KEYCHAIN_BY_ID; 
	return(SECRETTYPE); 
}

	/* certificate verification */
openssl			{ 
	YYD; 
	yylval.num = VERIFICATION_MODULE_OPENSSL; 
	return(VERIFICATION_MODULE); 
}
sec_framework	{ 
	YYD; 
	yylval.num = VERIFICATION_MODULE_SEC_FRAMEWORK; 
	return(VERIFICATION_MODULE); 
}
use_open_dir	{ 
	YYD; 
	yylval.num = VERIFICATION_OPTION_OPEN_DIR; 
	return(VERIFICATION_OPTION); 
}
use_peers_identifier	{ 
	YYD; 
	yylval.num = VERIFICATION_OPTION_PEERS_IDENTIFIER; 
	return(VERIFICATION_OPTION); 
}

	/* identifier qualifier */
tag		{ YYD; yylval.num = IDQUAL_TAG;  return(IDENTIFIERQUAL); }
file		{ YYD; yylval.num = IDQUAL_FILE; return(IDENTIFIERQUAL); }

	/* units */
B|byte|bytes		{ YYD; return(UNITTYPE_BYTE); }
KB			{ YYD; return(UNITTYPE_KBYTES); }
MB			{ YYD; return(UNITTYPE_MBYTES); }
TB			{ YYD; return(UNITTYPE_TBYTES); }
sec|secs|second|seconds	{ YYD; return(UNITTYPE_SEC); }
min|mins|minute|minutes	{ YYD; return(UNITTYPE_MIN); }
hour|hours		{ YYD; return(UNITTYPE_HOUR); }

dpd_default             { YYD; return(DPD_ALGO_TYPE_DEFAULT); }
dpd_inbound_detect      { YYD; return(DPD_ALGO_TYPE_INBOUND); }
dpd_blackhole_detect    { YYD; return(DPD_ALGO_TYPE_BLACKHOLE); }

idle_any                { YYD; return(IDLE_DIRECTION_ANY); }
idle_inbound            { YYD; return(IDLE_DIRECTION_IN); }
idle_outbound           { YYD; return(IDLE_DIRECTION_OUT); }

	/* boolean */
yes		{ YYD; yylval.num = TRUE; return(BOOLEAN); }
no		{ YYD; yylval.num = FALSE; return(BOOLEAN); }

{decstring}	{
			char *bp;

			YYD;
			yylval.num = strtol(yytext, &bp, 10);
			return(NUMBER);
		}

{hexstring}	{
			char *p;

			YYD; 
			yylval.val = vmalloc(yyleng + (yyleng & 1) + 1);
			if (yylval.val == NULL) {
				racoon_yyerror("vmalloc failed");
				return -1;
			}

			p = yylval.val->v;
			*p++ = '0';
			*p++ = 'x';

			/* fixed string if length is odd. */
			if (yyleng & 1)
				*p++ = '0';
			memcpy(p, &yytext[2], yyleng - 1);

			return(HEXSTRING);
		}

{quotedstring}	{
			char *p = yytext;

			YYD;
			while (*++p != '"') ;
			*p = '\0';

			yylval.val = vmalloc(yyleng - 1);
			if (yylval.val == NULL) {
				racoon_yyerror("vmalloc failed");
				return -1;
			}
			memcpy(yylval.val->v, &yytext[1], yylval.val->l);

			return(QUOTEDSTRING);
		}

{addrstring}	{
			YYD;

			yylval.val = vmalloc(yyleng + 1);
			if (yylval.val == NULL) {
				racoon_yyerror("vmalloc failed");
				return -1;
			}
			memcpy(yylval.val->v, yytext, yylval.val->l);

			return(ADDRSTRING);
		}

<<EOF>>		{
			yy_delete_buffer(YY_CURRENT_BUFFER);
			incstackp--;
    nextfile:
			if (incstack[incstackp].matches.gl_pathc != 0 &&
				incstack[incstackp].matches.gl_pathv &&
				incstack[incstackp].matchon <
			    incstack[incstackp].matches.gl_pathc) {
				char* filepath = incstack[incstackp].matches.gl_pathv[incstack[incstackp].matchon];
				incstack[incstackp].matchon++;
				incstackp++;
				if (yycf_set_buffer(filepath) != 0) {
					incstackp--;
					goto nextfile;
				}
				yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
				BEGIN(S_INI);
			} else {
				globfree(&incstack[incstackp].matches);
				if (incstackp == 0)
					yyterminate();
				else
					yy_switch_to_buffer(incstack[incstackp].prevstate);
			}
		}

	/* ... */
{ws}		{ ; }
{nl}		{ incstack[incstackp].lineno++; }
{comment}	{ YYD; }
{semi}		{ return(EOS); }
.		{ yymore(); }

%%

void
yyerror(const char *msg)
{
    plog(ASL_LEVEL_ERR, "%s:%d: %s\n", incstack[incstackp].path, incstack[incstackp].lineno, msg);
	yyerrorcount++;
}

void
yywarn(const char *msg)
{
	 plog(ASL_LEVEL_ERR, "%s:%d: %s\n", incstack[incstackp].path, incstack[incstackp].lineno, msg);
}

void
racoon_yyerror(const char *fmt, ...)
{
    va_list ap;
    char msg[512];
	
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    
    yyerror(msg);
}

void
racoon_yywarn(const char *fmt, ...)
{
    va_list ap;
	char msg[512];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    
    yywarn(msg);
}

int
yycf_switch_buffer(path)
	char *path;
{
	char *filepath = NULL;

	/* got the include file name */
	if (incstackp >= MAX_INCLUDE_DEPTH) {
		plog(ASL_LEVEL_ERR, 
			"Includes nested too deeply");
		return -1;
	}

	if (glob(path, GLOB_TILDE, NULL, &incstack[incstackp].matches) != 0 ||
	    incstack[incstackp].matches.gl_pathc == 0) {
		plog(ASL_LEVEL_WARNING, 
			"glob found no matches for path \"%s\"\n", path);
		return 0;
	}
	incstack[incstackp].matchon = 0;
	incstack[incstackp].prevstate = YY_CURRENT_BUFFER;

    nextmatch:
	if (incstack[incstackp].matchon >= incstack[incstackp].matches.gl_pathc)
		return -1;
	filepath =
	    incstack[incstackp].matches.gl_pathv[incstack[incstackp].matchon];
	incstack[incstackp].matchon++;
	incstackp++;

	if (yycf_set_buffer(filepath) != 0) {
	      incstackp--;
	      goto nextmatch;
	}

	yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));

	BEGIN(S_INI);

	return 0;
}

int
yycf_set_buffer(path)
	char *path;
{
	yyin = fopen(path, "r");
	if (yyin == NULL) {
		fprintf(stderr, "failed to open file %s (%s)\n",
			path, strerror(errno));
		plog(ASL_LEVEL_ERR, 
			"failed to open file %s (%s)\n",
			path, strerror(errno));
		return -1;
	}

	/* initialize */
	if (incstack[incstackp].path != NULL) {
		fclose(incstack[incstackp].fp);
		racoon_free(incstack[incstackp].path);
	}
	incstack[incstackp].fp = yyin;
	incstack[incstackp].path = racoon_strdup(path);
	STRDUP_FATAL(incstack[incstackp].path);
	incstack[incstackp].lineno = 1;
	plog(ASL_LEVEL_DEBUG, 
		"reading configuration file %s\n", path);

	return 0;
}

void
yycf_init_buffer()
{
	int i;

	for (i = 0; i < MAX_INCLUDE_DEPTH; i++)
		memset(&incstack[i], 0, sizeof(incstack[i]));
	incstackp = 0;
}

void
yycf_clean_buffer()
{
	int i;

	for (i = 0; i < MAX_INCLUDE_DEPTH; i++) {
		if (incstack[i].path != NULL) {
			fclose(incstack[i].fp);
			racoon_free(incstack[i].path);
			incstack[i].path = NULL;
		}
	}
}

