/*	$KAME: localconf.c,v 1.33 2001/08/09 07:32:19 sakane Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <err.h>

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "debug.h"

#include "localconf.h"
#include "algorithm.h"
#include "admin.h"
#include "privsep.h"
#include "isakmp_var.h"
#include "isakmp.h"
#include "ipsec_doi.h"
#include "grabmyaddr.h"
#include "vendorid.h"
#include "str2val.h"
#include "safefile.h"
#include "admin.h"
#include "gcmalloc.h"
#include "session.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#if HAVE_SECURITY_FRAMEWORK
#include <Security/Security.h>
#else
typedef void * SecKeychainRef;
#endif
#endif

struct localconf *lcconf;

static void setdefault __P((void));

void
initlcconf()
{
	lcconf = racoon_calloc(1, sizeof(*lcconf));
	if (lcconf == NULL)
		errx(1, "failed to allocate local conf.");

	setdefault();
	lcconf->sock_vpncontrol = -1;	/* not to be done during flush */
	lcconf->racoon_conf = LC_DEFAULT_CF;
	TAILQ_INIT(&lcconf->saved_msg_queue);
}

void
flushlcconf()
{
	int i;

	setdefault();
	clear_myaddr();
	for (i = 0; i < LC_PATHTYPE_MAX; i++) {
		if (lcconf->pathinfo[i]) {
			racoon_free(lcconf->pathinfo[i]);
			lcconf->pathinfo[i] = NULL;
		}
	}
	for (i = 0; i < LC_IDENTTYPE_MAX; i++) {
		if (lcconf->ident[i])
			vfree(lcconf->ident[i]);
		lcconf->ident[i] = NULL;
	}
	if (lcconf->ext_nat_id) {
		vfree(lcconf->ext_nat_id);
		lcconf->ext_nat_id = NULL;
	}
}

static void
setdefault()
{
	lcconf->uid = 0;
	lcconf->gid = 0;
	lcconf->chroot = NULL;
	lcconf->autograbaddr = 1;
	lcconf->port_isakmp = PORT_ISAKMP;
	lcconf->port_isakmp_natt = PORT_ISAKMP_NATT;
	lcconf->default_af = AF_INET;
	lcconf->pad_random = LC_DEFAULT_PAD_RANDOM;
	lcconf->pad_randomlen = LC_DEFAULT_PAD_RANDOMLEN;
	lcconf->pad_maxsize = LC_DEFAULT_PAD_MAXSIZE;
	lcconf->pad_strict = LC_DEFAULT_PAD_STRICT;
	lcconf->pad_excltail = LC_DEFAULT_PAD_EXCLTAIL;
	lcconf->retry_counter = LC_DEFAULT_RETRY_COUNTER;
	lcconf->retry_interval = LC_DEFAULT_RETRY_INTERVAL;
	lcconf->count_persend = LC_DEFAULT_COUNT_PERSEND;
	lcconf->secret_size = LC_DEFAULT_SECRETSIZE;
	lcconf->retry_checkph1 = LC_DEFAULT_RETRY_CHECKPH1;
	lcconf->wait_ph2complete = LC_DEFAULT_WAIT_PH2COMPLETE;
	lcconf->strict_address = FALSE;
	lcconf->complex_bundle = TRUE; /*XXX FALSE;*/
	lcconf->gss_id_enc = LC_GSSENC_UTF16LE; /* Windows compatibility */
	lcconf->natt_ka_interval = LC_DEFAULT_NATT_KA_INTERVAL;
	lcconf->auto_exit_delay = 0;
	lcconf->auto_exit_state &= ~LC_AUTOEXITSTATE_SET;
	lcconf->auto_exit_state |= LC_AUTOEXITSTATE_CLIENT;				/* always auto exit as default */
}

/*
 * get PSK by string.
 */
vchar_t *
getpskbyname(id0)
	vchar_t *id0;
{
	char *id;
	vchar_t *key = NULL;

	id = racoon_calloc(1, 1 + id0->l - sizeof(struct ipsecdoi_id_b));
	if (id == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to get psk buffer.\n");
		goto end;
	}
	memcpy(id, id0->v + sizeof(struct ipsecdoi_id_b),
		id0->l - sizeof(struct ipsecdoi_id_b));
	id[id0->l - sizeof(struct ipsecdoi_id_b)] = '\0';

	key = privsep_getpsk(id, id0->l - sizeof(struct ipsecdoi_id_b));

end:
	if (id)
		racoon_free(id);
	return key;
}

#if defined(__APPLE__) && HAVE_KEYCHAIN
/*
 * get PSK from keyChain.
 */
vchar_t *
getpskfromkeychain(const char *name, u_int8_t etype, int secrettype, vchar_t *id_p)
{
	SecKeychainRef keychain = NULL;
	vchar_t *key = NULL;
	void *cur_password = NULL;
	UInt32 cur_password_len	= 0;
	OSStatus status;
	char serviceName[] = "com.apple.net.racoon";

	status = SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
	if (status != noErr) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to set system keychain domain.\n");
		goto end;
	}

	status = SecKeychainCopyDomainDefault(kSecPreferencesDomainSystem,
					      &keychain);
	if (status != noErr) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to get system keychain domain.\n");
		goto end;
	}

	if (secrettype == SECRETTYPE_KEYCHAIN_BY_ID && etype == ISAKMP_ETYPE_AGG) {
		/* try looking up based on peers id */
		
		char* peer_id;
		int idlen = id_p->l - sizeof(struct ipsecdoi_id_b);
		u_int8_t id_type = ((struct ipsecdoi_id_b *)(id_p->v))->type;

		switch (id_type) {
			case IPSECDOI_ID_IPV4_ADDR:
			case IPSECDOI_ID_IPV6_ADDR:
			case IPSECDOI_ID_IPV4_ADDR_SUBNET:
			case IPSECDOI_ID_IPV4_ADDR_RANGE:
			case IPSECDOI_ID_IPV6_ADDR_SUBNET:
			case IPSECDOI_ID_IPV6_ADDR_RANGE:
			case IPSECDOI_ID_DER_ASN1_DN:
			case IPSECDOI_ID_DER_ASN1_GN:
				goto no_id;
				break;
				
			case IPSECDOI_ID_FQDN:
			case IPSECDOI_ID_USER_FQDN:
			case IPSECDOI_ID_KEY_ID:
				peer_id = racoon_malloc(idlen);
				if (peer_id == NULL)
					goto end;
				memcpy(peer_id, id_p->v + sizeof(struct ipsecdoi_id_b), idlen);
				*(peer_id + idlen) = '\0';
				plog(LLV_ERROR, LOCATION, NULL,
					"getting shared secret from keychain using %s.\n", peer_id);

				break;
			default:
				goto end;
				break;
		}
						
		status = SecKeychainFindGenericPassword(keychain,
								strlen(serviceName),
								serviceName,
								idlen,
								peer_id,
								&cur_password_len,
								&cur_password,
								NULL);

		/* try find it using using only the peer id. */
		if (status == errSecItemNotFound) 
			status = SecKeychainFindGenericPassword(keychain,
								idlen,
								peer_id,
								0,
								0,
								&cur_password_len,
								&cur_password,
								NULL);
	
		if (status == noErr)
			goto end;
		/* otherwise fall through to use the default value */
	}
	
no_id:
	/*	use the value in remote config sharedsecret field
		this is either the value specified for lookup or the 
		default when lookup by id fails.
	*/
	status = SecKeychainFindGenericPassword(keychain,
							strlen(serviceName),
							serviceName,
							strlen(name),
							name,
							&cur_password_len,
							&cur_password,
							NULL);
								
	/* try find it using using only name. */
	if (status == errSecItemNotFound) 
		status = SecKeychainFindGenericPassword(keychain,
							strlen(name),
							name,
							0,
							0,
							&cur_password_len,
							&cur_password,
							NULL);

	switch (status) {

		case noErr :
			goto end;
			break;

		case errSecItemNotFound :
			break;

		default :
			plog(LLV_ERROR, LOCATION, NULL,
				"failed to get preshared key from system keychain (error %d).\n", status);
	}

end:

        if (cur_password) {
                key = vmalloc(cur_password_len + 1);
                if (key == NULL) {
                        plog(LLV_ERROR, LOCATION, NULL,
                                "failed to allocate key buffer.\n");
                } else {
					memcpy(key->v, cur_password, key->l);
					key->v[cur_password_len] = 0;
                }
			free(cur_password);
        }
        
        if (keychain)
            CFRelease(keychain);

	return key;
}
#endif

/*
 * get PSK by address.
 */
vchar_t *
getpskbyaddr(remote)
	struct sockaddr *remote;
{
	vchar_t *key = NULL;
	char addr[NI_MAXHOST], port[NI_MAXSERV];

	GETNAMEINFO(remote, addr, port);

	key = privsep_getpsk(addr, strlen(addr));

	return key;
}

vchar_t *
getpsk(str, len)
	const char *str;
	const int len;
{
	FILE *fp;
	char buf[1024];	/* XXX how is variable length ? */
	vchar_t *key = NULL;
	char *p, *q;
	size_t keylen;
	char *k = NULL;

	if (safefile(lcconf->pathinfo[LC_PATHTYPE_PSK], 1) == 0)
		fp = fopen(lcconf->pathinfo[LC_PATHTYPE_PSK], "r");
	else
		fp = NULL;
	if (fp == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to open pre_share_key file %s\n",
			lcconf->pathinfo[LC_PATHTYPE_PSK]);
		return NULL;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		/* comment line */
		if (buf[0] == '#')
			continue;

		/* search the end of 1st string. */
		for (p = buf; *p != '\0' && !isspace((int)*p); p++)
			;
		if (*p == '\0')
			continue;	/* no 2nd parameter */
		*p = '\0';
		/* search the 1st of 2nd string. */
		while (isspace((int)*++p))
			;
		if (*p == '\0')
			continue;	/* no 2nd parameter */
		p--;
		if (strncmp(buf, str, len) == 0 && buf[len] == '\0') {
			p++;
			keylen = 0;
			for (q = p; *q != '\0' && *q != '\n'; q++)
				keylen++;
			*q = '\0';

			/* fix key if hex string */
			if (strncmp(p, "0x", 2) == 0) {
				k = str2val(p + 2, 16, &keylen);
				if (k == NULL) {
					plog(LLV_ERROR, LOCATION, NULL,
						"failed to get psk buffer.\n");
					goto end;
				}
				p = k;
			}

			key = vmalloc(keylen);
			if (key == NULL) {
				plog(LLV_ERROR, LOCATION, NULL,
					"failed to allocate key buffer.\n");
				goto end;
			}
			memcpy(key->v, p, key->l);
			if (k)
				racoon_free(k);
			goto end;
		}
	}

end:
	fclose(fp);
	return key;
}

/*
 * get a file name of a type specified.
 */
void
getpathname(path, len, type, name)
	char *path;
	int len, type;
	const char *name;
{
	snprintf(path, len, "%s%s%s", 
		name[0] == '/' ? "" : lcconf->pathinfo[type],
		name[0] == '/' ? "" : "/",
		name);

	plog(LLV_DEBUG, LOCATION, NULL, "filename: %s\n", path);
}

#if 0 /* DELETEIT */
static int lc_doi2idtype[] = {
	-1,
	-1,
	LC_IDENTTYPE_FQDN,
	LC_IDENTTYPE_USERFQDN,
	-1,
	-1,
	-1,
	-1,
	-1,
	LC_IDENTTYPE_CERTNAME,
	-1,
	LC_IDENTTYPE_KEYID,
};

/*
 * convert DOI value to idtype
 * OUT	-1   : NG
 *	other: converted.
 */
int
doi2idtype(idtype)
	int idtype;
{
	if (ARRAYLEN(lc_doi2idtype) > idtype)
		return lc_doi2idtype[idtype];
	return -1;
}
#endif

static int lc_sittype2doi[] = {
	IPSECDOI_SIT_IDENTITY_ONLY,
	IPSECDOI_SIT_SECRECY,
	IPSECDOI_SIT_INTEGRITY,
};

/*
 * convert sittype to DOI value.
 * OUT	-1   : NG
 *	other: converted.
 */
int
sittype2doi(sittype)
	int sittype;
{
	if (ARRAYLEN(lc_sittype2doi) > sittype)
		return lc_sittype2doi[sittype];
	return -1;
}

static int lc_doitype2doi[] = {
	IPSEC_DOI,
};

/*
 * convert doitype to DOI value.
 * OUT	-1   : NG
 *	other: converted.
 */
int
doitype2doi(doitype)
	int doitype;
{
	if (ARRAYLEN(lc_doitype2doi) > doitype)
		return lc_doitype2doi[doitype];
	return -1;
}



