/* MODULE: auth_ldap */
/* COPYRIGHT
 * Copyright (c) 2002-2002 Igor Brezac
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
 *
 * THIS SOFTWARE IS PROVIDED BY IGOR BREZAC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL IGOR BREZAC OR
 * ITS EMPLOYEES OR AGENTS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * END COPYRIGHT */

/* SYNOPSIS
 * Authenticate against LDAP.
 * END SYNOPSIS */

#ifdef __GNUC__
#ident "$Id: auth_ldap.c,v 1.1 2002/05/23 18:58:38 snsimon Exp $"
#endif

/* PUBLIC DEPENDENCIES */
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "mechanisms.h"

/* END PUBLIC DEPENDENCIES */

# define RETURN(x) {return strdup(x);}

/* FUNCTION: auth_ldap */

#ifdef AUTH_LDAP

#include "lak.h"

#define CONFIGLISTGROWSIZE 100
#define SASLAUTHD_CONF_FILE "/usr/local/etc/saslauthd.conf"

#define AUTH_LDAP_OK 1
#define AUTH_LDAP_FAIL 0

struct configlist {
    char *key;
    char *value;
};

static struct configlist *configlist;
static int nconfiglist;

static LAK *lak = NULL;

static int auth_ldap_config_init(const char *filename)
{
	FILE *infile;
	int lineno = 0;
	int alloced = 0;
	char buf[4096];
	char *p, *key;
	char *result;

	nconfiglist=0;

	infile = fopen(filename, "r");
	if (!infile) {
		return AUTH_LDAP_FAIL;
	}
    
	while (fgets(buf, sizeof(buf), infile)) {
		lineno++;

		if (buf[strlen(buf)-1] == '\n') 
			buf[strlen(buf)-1] = '\0';
		for (p = buf; *p && isspace((int) *p); p++);
			if (!*p || *p == '#') 
				continue;

		key = p;
		while (*p && (isalnum((int) *p) || *p == '-' || *p == '_')) {
			if (isupper((int) *p)) 
				*p = tolower(*p);
			p++;
		}
		if (*p != ':') {
			return AUTH_LDAP_FAIL;
		}
		*p++ = '\0';

		while (*p && isspace((int) *p)) 
			p++;

		if (!*p) {
			return AUTH_LDAP_FAIL;
		}

		if (nconfiglist == alloced) {
			alloced += CONFIGLISTGROWSIZE;
			configlist=realloc((char *)configlist, alloced * sizeof(struct configlist));
			if (configlist==NULL) 
				return AUTH_LDAP_FAIL;
		}

		result = strdup(key);
		if (result==NULL) 
			return AUTH_LDAP_FAIL;
			configlist[nconfiglist].key = result;

		result = strdup(p);
		if (result==NULL) 
			return AUTH_LDAP_FAIL;
		configlist[nconfiglist].value = result;

		nconfiglist++;
	}

	fclose(infile);

	return AUTH_LDAP_OK;
}

static const char *auth_ldap_config_getstring(const char *key, const char *def)
{
    int opt;

    for (opt = 0; opt < nconfiglist; opt++) {
	if (*key == configlist[opt].key[0] &&
	    !strcmp(key, configlist[opt].key))
	  return configlist[opt].value;
    }
    return def;
}

static int auth_ldap_config_getint(const char *key, int def)
{
    const char *val = auth_ldap_config_getstring(key, (char *)0);

    if (!val) return def;
    if (!isdigit((int) *val) && (*val != '-' || !isdigit((int) val[1]))) return def;
    return atoi(val);
}

static int auth_ldap_config_getswitch(const char *key, int def)
{
    const char *val = auth_ldap_config_getstring(key, (char *)0);

    if (!val) return def;

    if (*val == '0' || *val == 'n' ||
	(*val == 'o' && val[1] == 'f') || *val == 'f') {
	return 0;
    }
    else if (*val == '1' || *val == 'y' ||
	     (*val == 'o' && val[1] == 'n') || *val == 't') {
	return 1;
    }
    return def;
}

static int auth_ldap_configure()
{
    	char  *myname = "auth_ldap_configure";

	int rc = 0;
	char *s;

	rc = auth_ldap_config_init(SASLAUTHD_CONF_FILE);
	if (rc != AUTH_LDAP_OK) {
		syslog(LOG_INFO, "Cannot open configuration file", myname);
		return AUTH_LDAP_FAIL;
	}

	lak = malloc( sizeof(LAK) );
	if (lak == NULL) {
		syslog(LOG_INFO, "Cannot allocate memory", myname);
		return AUTH_LDAP_FAIL;
	}

	lak->servers = (char *) auth_ldap_config_getstring("ldap_servers", "ldap://localhost/");
	lak->bind_dn = (char *) auth_ldap_config_getstring("ldap_bind_dn", "");
	lak->bind_pw = (char *) auth_ldap_config_getstring("ldap_bind_pw", "");
	lak->search_base = (char *) auth_ldap_config_getstring("ldap_search_base", "");
	lak->filter = (char *) auth_ldap_config_getstring("ldap_filter", "uid=%u");
	lak->timeout.tv_sec = auth_ldap_config_getint("ldap_timeout", 5);
	lak->timeout.tv_usec = 0;
	lak->sizelimit = auth_ldap_config_getint("ldap_sizelimit", 0);
	lak->timelimit = auth_ldap_config_getint("ldap_timelimit", 5);
	s = (char *) auth_ldap_config_getstring("ldap_deref", NULL);
	if (s) {
		if (strcasecmp(s, "search")) {
			lak->deref = LDAP_DEREF_SEARCHING;
		} else if (strcasecmp(s, "find")) {
			lak->deref = LDAP_DEREF_FINDING;
		} else if (strcasecmp(s, "always")) {
			lak->deref = LDAP_DEREF_ALWAYS;
		} else if (strcasecmp(s, "never")) {
			lak->deref = LDAP_DEREF_NEVER;
		} else {
			lak->deref = NULL;
		}
	} else {
		lak->deref = NULL;
	}
	lak->referrals = auth_ldap_config_getswitch("ldap_referrals", 0);
	lak->cache_expiry = auth_ldap_config_getint("ldap_cache_expiry", 0);
	lak->cache_size = auth_ldap_config_getint("ldap_cache_size", 0);
	s = (char *) auth_ldap_config_getstring("ldap_scope", NULL);
	if (s) {
		if (strcasecmp(s, "one")) {
			lak->scope = LDAP_SCOPE_ONELEVEL;
		} else if (strcasecmp(s, "base")) {
			lak->scope = LDAP_SCOPE_BASE;
		} else {
			lak->scope = LDAP_SCOPE_SUBTREE;
		}
	} else {
		lak->scope = LDAP_SCOPE_SUBTREE;
	}
	lak->debug = auth_ldap_config_getint("ldap_debug", 0);
	lak->verbose = auth_ldap_config_getswitch("ldap_verbose", 0);

	if (lak->verbose) {
		syslog(LOG_INFO, "ldap_servers: %s", lak->servers);
		syslog(LOG_INFO, "ldap_bind_dn: %s", lak->bind_dn);
		syslog(LOG_INFO, "ldap_bind_pw: %s", crypt(lak->bind_pw,lak->bind_pw));
		syslog(LOG_INFO, "ldap_timeout: %d", lak->timeout.tv_sec);
		syslog(LOG_INFO, "ldap_sizelimit: %d", lak->sizelimit);
		syslog(LOG_INFO, "ldap_timelimit: %d", lak->timelimit);
		syslog(LOG_INFO, "ldap_deref: %d", lak->deref);
		syslog(LOG_INFO, "ldap_referrals: %d", lak->referrals);
		syslog(LOG_INFO, "ldap_cache_expiry: %d", lak->cache_expiry);
		syslog(LOG_INFO, "ldap_cache_size: %d", lak->cache_size);
		syslog(LOG_INFO, "ldap_scope: %d", lak->scope);
		syslog(LOG_INFO, "ldap_search_base: %s", lak->search_base);
		syslog(LOG_INFO, "ldap_filter: %s", lak->filter);
		syslog(LOG_INFO, "ldap_debug: %d", lak->debug);
	}

	return AUTH_LDAP_OK;
}


char *					/* R: allocated response string */
auth_ldap(
  /* PARAMETERS */
  const char *login,			/* I: plaintext authenticator */
  const char *password,			/* I: plaintext password */
  const char *service __attribute__((unused)),
  const char *realm
  /* END PARAMETERS */
  )
{
	char  *myname = "auth_ldap";

	int rc = 0;

	if (lak == NULL) {
		rc = auth_ldap_configure();
		if (rc != AUTH_LDAP_OK) {
			RETURN("NO");
		}
	}

	rc = lak_authenticate(lak,(char *)login,(char *)realm,(char *)password);
    	if (rc == LAK_OK) {
		RETURN("OK");
	} else {
		RETURN("NO");
	}
}

#else /* !AUTH_LDAP */

char *
auth_ldap(
  const char *login __attribute__((unused)),
  const char *password __attribute__((unused)),
  const char *service __attribute__((unused)),
  const char *realm __attribute__((unused))
  )
{
     return NULL;
}

#endif /* !AUTH_LDAP */

/* END FUNCTION: auth_ldap */

/* END MODULE: auth_ldap */
