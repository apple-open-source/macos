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

#ifdef WITH_LDAP

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <syslog.h>
#include <crypt.h>

#include "lak.h"

LAK_CONN *conn = NULL;

static int lak_get_conn(LAK_CONN **conn) 
{
    	char  *myname = "lak_get_conn";

	LAK_CONN *c;

	c = (LAK_CONN *)malloc(sizeof(LAK_CONN));
	if (c == NULL) {
		syslog(LOG_ERR|LOG_AUTH, "%s: Cannot allocate memory", myname);
		return LAK_FAIL;
	}

	c->bound=0;
	c->ld=NULL;
	*conn=c;

	return LAK_OK;
}

/*
 * If any characters in the supplied address should be escaped per RFC
 * 2254, do so. Thanks to Keith Stevenson and Wietse. And thanks to
 * Samuel Tardieu for spotting that wildcard searches were being done in
 * the first place, which prompted the ill-conceived lookup_wildcards
 * parameter and then this more comprehensive mechanism.
 *
 * Note: calling function must free memory.
 */
static char *lak_escape(LAK *conf, char *s) 
{
    	char  *myname = "lak_escape";

	char *buf;
	char *end, *ptr, *temp;

	if (s == NULL) {
		syslog(LOG_WARNING|LOG_AUTH, "%s: Nothing to escape.", myname);
		return NULL;
	}

	buf = malloc(sizeof(s) * 2 + 1);
	if (buf == NULL) {
		syslog(LOG_ERR|LOG_AUTH, "%s: Cannot allocate memory", myname);
		return NULL;
	}

	ptr = s;
	end = (char *) ptr + strlen((char *) ptr);

	while (((temp = (char *)strpbrk((char *)ptr, "*()\\\0"))!=NULL) && (temp < end)) {

		if ((temp-ptr) > 0)
			strncat(buf, ptr, temp-ptr);

		switch (*temp) {
			case '*':
				strcat(buf, "\\2a");
				break;
			case '(':
				strcat(buf, "\\28");
				break;
			case ')':
				strcat(buf, "\\29");
				break;
			case '\\':
				strcat(buf, "\\5c");
				break;
			case '\0':
				strcat(buf, "\\00");
				break;
		}
		ptr=temp+1;
	}
	if (temp<end)
		strcat(buf, (char *) ptr);

	if (conf->verbose)
		syslog(LOG_INFO|LOG_AUTH,"%s: After escaping, it's %s", myname, buf);

	return(buf);
}

/*
 * lak_filter
 * Parts with the strings provided.
 *   %% = %
 *   %u = user
 *   %r = realm
 * Note: calling function must free memory.
 */
static char *lak_filter(LAK *conf, char *user, char *realm) 
{
    	char *myname = "lak_filter";

	char *buf; 
	char *end, *ptr, *temp;
	char *ebuf;
	
	if (conf->filter == NULL) {
		syslog(LOG_WARNING|LOG_AUTH, "%s: filter not setup", myname);
		return NULL;
	}

	if (user == NULL) {
		syslog(LOG_WARNING|LOG_AUTH, "%s: user is null", myname);
		return NULL;
	}

	if ((buf=(char *)malloc(sizeof(conf->filter)+sizeof(user)+sizeof(realm)+1))==NULL) {
		syslog(LOG_ERR|LOG_AUTH, "%s: Cannot allocate memory", myname);
		return NULL;
	}
	
	ptr=conf->filter;
	end = (char *) ptr + strlen((char *) ptr);

	while ((temp=(char *)strchr(ptr,'%'))!=NULL ) {

		if ((temp-ptr) > 0)
			strncat(buf, ptr, temp-ptr);

		if ((temp+1) >= end) {
			syslog(LOG_WARNING|LOG_AUTH, "%s: Incomplete lookup substitution format", myname);
			break;
		}

		switch (*(temp+1)) {
			case '%':
				strncat(buf,temp+1,1);
				break;
			case 'u':
				ebuf=lak_escape(conf, user);
				if (ebuf!=NULL) {
					strcat(buf,ebuf);
					free(ebuf);
				}
				break;
			case 'r':
				if (realm!=NULL) {
					ebuf = lak_escape(conf, realm);
					if (ebuf != NULL) {
						strcat(buf,ebuf);
						free(ebuf);
					}
				}
				break;
			default:
				syslog(LOG_WARNING|LOG_AUTH, "%s: Invalid lookup substitution format '%%%c'!", myname, *(temp+1));
				break;
		}
		ptr=temp+2;
	}
	if (temp<end)
		strcat(buf, ptr);

	if (conf->verbose)
		syslog(LOG_INFO|LOG_AUTH,"%s: After filter substitution, it's %s", myname, buf);

	return(buf);
}

/* 
 * Establish a connection to the LDAP server. 
 */
static int lak_connect(LAK *conf, LDAP **res)
{
	char   *myname = "lak_connect";

	int     rc = 0;
	LDAP   *ld;

	if (conf->verbose)
		syslog(LOG_INFO|LOG_AUTH, "%s: Connecting to server %s", myname, conf->servers);

	rc = ldap_initialize(&ld, conf->servers);
	if (rc != LDAP_SUCCESS) {
		syslog(LOG_ERR|LOG_AUTH, "%s: ldap_initialize failed", myname, conf->servers);
		return LAK_FAIL;
	}

	if (conf->debug) {
		ldap_set_option(NULL, LDAP_OPT_DEBUG_LEVEL, &(conf->debug));
		if (rc != LDAP_OPT_SUCCESS)
			syslog(LOG_WARNING|LOG_AUTH, "%s: Unable to set LDAP_OPT_DEBUG_LEVEL %x.", myname, conf->debug);
	}

	rc = ldap_set_option(ld, LDAP_OPT_NETWORK_TIMEOUT, &(conf->timeout));
	if (rc != LDAP_OPT_SUCCESS) {
		syslog(LOG_WARNING|LOG_AUTH, "%s: Unable to set LDAP_OPT_NETWORK_TIMEOUT %d.%d.", myname, conf->timeout.tv_sec, conf->timeout.tv_usec);
	}

	if (conf->timelimit) {
		ldap_set_option(ld, LDAP_OPT_TIMELIMIT, &(conf->timelimit));
		if (rc != LDAP_OPT_SUCCESS)
			syslog(LOG_WARNING|LOG_AUTH, "%s: Unable to set LDAP_OPT_TIMELIMIT %d.", myname, conf->timelimit);
	}

	if (conf->deref) {
		rc = ldap_set_option(ld, LDAP_OPT_DEREF, &(conf->deref));
		if (rc != LDAP_OPT_SUCCESS)
			syslog(LOG_WARNING|LOG_AUTH, "%s: Unable to set LDAP_OPT_DEREF %d.", myname, conf->deref);
	}

	if (conf->referrals) {
		rc = ldap_set_option(ld, LDAP_OPT_REFERRALS, LDAP_OPT_ON);
		if (rc != LDAP_OPT_SUCCESS)
			syslog(LOG_WARNING|LOG_AUTH, "%s: Unable to set LDAP_OPT_REFERRALS.", myname);
	}

	if (conf->sizelimit) {
		rc = ldap_set_option(ld, LDAP_OPT_SIZELIMIT, &(conf->sizelimit));
		if (rc != LDAP_OPT_SUCCESS)
			syslog(LOG_WARNING|LOG_AUTH, "%s: Unable to set LDAP_OPT_SIZELIMIT %d.", myname, conf->sizelimit);
	}

	/*
	* Bind to the server
	*/
	rc = ldap_bind_s(ld, conf->bind_dn, conf->bind_pw, LDAP_AUTH_SIMPLE);
	if (rc != LDAP_SUCCESS) {
		syslog(LOG_WARNING|LOG_AUTH, "%s: Unable to bind to server %s as %s: %d (%s)", myname,
		conf->servers, conf->bind_dn, rc, ldap_err2string(rc));
		ldap_unbind_s(ld);
		return LAK_FAIL;
	}

	/*
	 * Set up client-side caching
	 */
	if (conf->cache_expiry) {
		if (conf->verbose)
			syslog(LOG_INFO|LOG_AUTH, "%s: Enabling %ld-byte cache with %ld-second expiry", myname, conf->cache_size, conf->cache_expiry);

		rc = ldap_enable_cache(ld, conf->cache_expiry, conf->cache_size);
		if (rc != LDAP_SUCCESS) {
			syslog(LOG_WARNING|LOG_AUTH, "%s: Unable to configure cache: %d (%s) -- continuing", myname, rc, ldap_err2string(rc));
		} else {
		if (conf->verbose)
			syslog(LOG_WARNING|LOG_AUTH, "%s: Caching enabled", myname);
		}
	}

	*res = ld;
	return LAK_OK;
}

/* 
 * lak_search - 
 */
static int lak_search(LAK *conf, LAK_CONN *conn, char *filter, char **attrs, LDAPMessage **res)
{
	char   *myname = "lak_search";
	int rc = 0;

	*res = NULL;

	/*
	* Connect to the LDAP server, if necessary.
	*/
	if (!conn->bound) {
		syslog(LOG_WARNING|LOG_AUTH, "%s: No existing connection reopening", myname);

		if (conn->ld) {
			if (conf->verbose)
				syslog(LOG_INFO|LOG_AUTH, "%s: Closing existing connection", myname);
			if (conf->cache_expiry)
				ldap_destroy_cache(conn->ld);

			ldap_unbind_s(conn->ld);
			conn->ld = NULL;
		}

		rc = lak_connect(conf, &(conn->ld));
		if (rc) {
			syslog(LOG_WARNING|LOG_AUTH, "%s: (re)connect attempt failed", myname);
			return LAK_FAIL;
		}

		conn->bound = 1;
	} 

	/*
	* On to the search.
	*/
	rc = ldap_search_st(conn->ld, conf->search_base, conf->scope, filter,
	                    (char **) attrs, 0, &(conf->timeout), res);
	switch (rc) {
		case LDAP_SUCCESS:
			break;

		default:
			syslog(LOG_WARNING|LOG_AUTH, "%s: ldap_search_st() failed: %s", myname, ldap_err2string(rc));
			ldap_msgfree(*res);
			conn->bound = 0;
			return LAK_FAIL;
	}

	if ((ldap_count_entries(conn->ld, *res)) != 1) {
		syslog(LOG_WARNING|LOG_AUTH, "%s: object not found or got ambiguous search result.", myname);
		ldap_msgfree(*res);
		return LAK_FAIL;
	}

	return LAK_OK;
}

/* 
 * lak_lookup - retrieve user@realm values specified by 'attrs'
 */
int lak_lookup(LAK *conf, char *user, char *realm, char **attrs, LAK_RESULT **res)
{
	char   *myname = "lak_lookup";

	int rc = 0;
	char *filter = NULL;
	LAK_RESULT *ptr = NULL, *temp = NULL;
	LDAPMessage *msg;
	LDAPMessage *entry;
	BerElement *ber;
	char *attr, **vals;
    
    	*res = NULL;

	if (conn == NULL) {
		rc = lak_get_conn(&conn);
		if (rc != LAK_OK) {
			syslog(LOG_WARNING|LOG_AUTH, "%s: lak_get_conn failed.", myname);
			return LAK_FAIL;
		}
	}

	filter = lak_filter(conf, user, realm);
	if (filter == NULL) {
		syslog(LOG_WARNING|LOG_AUTH, "%s: lak_filter failed.", myname);
		return LAK_FAIL;
	}

	rc = lak_search(conf, conn, filter, attrs, &msg);
	if (rc != LAK_OK) {
		syslog(LOG_WARNING|LOG_AUTH, "%s: lak_search failed.", myname);
		free(filter);
		return LAK_FAIL;
	}

	entry = ldap_first_entry(conn->ld, msg); 
	if (entry == NULL) {
		syslog(LOG_WARNING|LOG_AUTH, "%s: ldap_first_entry() failed.", myname);
		free(filter);
		ldap_msgfree(msg);
		return LAK_FAIL;
	}

	attr = ldap_first_attribute(conn->ld, entry, &ber);
	if (attr == NULL) {
		syslog(LOG_WARNING|LOG_AUTH, "%s: no attributes found", myname);
		free(filter);
		ldap_msgfree(msg);
		return LAK_FAIL;
	}

	while (attr != NULL) {

		vals = ldap_get_values(conn->ld, entry, attr);
		if (vals == NULL) {
			syslog(LOG_WARNING|LOG_AUTH, "%s: Entry doesn't have any values for %s", myname, attr);
			continue;
		}

		temp = (LAK_RESULT *) malloc(sizeof(LAK_RESULT));
		if (*res == NULL) {
			*res = temp;
			ptr = *res;
		}
		else {
			ptr->next = temp;
			ptr = temp;
		}
		ptr->attribute = (char *)strdup(attr);
		ptr->value = (char *)strdup(vals[0]); /* Get only first attribute */
		ptr->len = strlen(ptr->value);
		ptr->next = NULL;

		if (conf->verbose)
			syslog(LOG_INFO|LOG_AUTH, "%s: Attribute %s, Value %s", myname, ptr->attribute, ptr->value);

		ldap_value_free(vals);
		ldap_memfree(attr);

		attr = ldap_next_attribute(conn->ld, entry, ber);
	}

	if (ber != NULL)
		ber_free(ber, 0);
	ldap_msgfree(msg);
	free(filter);

	if (*res == NULL)
		return LAK_NOENT;
	else
		return LAK_OK;
		
}

int lak_authenticate(LAK *conf, char *user, char* realm, char *password) 
{
	char   *myname = "lak_authenticate";

	LAK_RESULT *res;
	int rc;
	int ret;
	char *end, *temp, *ptr;
	char *attrs[] = {"userPassword", NULL};

	rc = lak_lookup(conf, user, realm, attrs, &res);
	if (rc != LAK_OK) {
		if (conf->verbose) {
			syslog(LOG_WARNING|LOG_AUTH, "%s: User not found %s", myname, user);
		}
		lak_free(res);
		return LAK_FAIL;
	}

	ret = LAK_FAIL;

	ptr = res->value;
	end = (char *) ptr + res->len;

	temp = (char *) strchr(ptr, '}');

	if ((temp != NULL) && (temp < end)) {
		if (!strncasecmp(ptr, "{crypt}", temp - ptr + 1)) {
			if (!strcmp(ptr+7, (char *)crypt(password, ptr+7)))
				ret = LAK_OK;
		}
		else if (!strncasecmp(ptr, "{clear}", temp - ptr + 1)) {
			if (!strcmp(ptr+7, password))
				ret = LAK_OK;
		}
		/* Add MD5, SHA and others */
		else {
			syslog(LOG_WARNING|LOG_AUTH, "%s: Unknown password encryption for %s", myname, user);
		}
	}

	lak_free(res);

	return(ret);
}

/* 
 * lak_free - free memory buffers
 */
void lak_free(LAK_RESULT *result) 
{
	/* char   *myname = "lak_free"; */

	if (result == NULL)
		return;

	if (result->next != NULL) {
		lak_free(result->next);
	}

	free(result->attribute);	
	free(result->value);	
	free(result);
}

/* 
 * lak_close - free memory buffers
 */
void lak_close(LAK *conf, LAK_CONN *conn) 
{
	/* char   *myname = "lak_close"; */

	if (conn->ld) {
		if (conf->cache_expiry)
			ldap_destroy_cache(conn->ld);
			
		ldap_unbind_s(conn->ld);
	}

	if (conn) {
		free(conn);
	}
}

#endif /* WITH_LDAP */
